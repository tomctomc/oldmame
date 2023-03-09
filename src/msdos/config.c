/*
 * Configuration routines.
 *
 * 971219 support for mame.cfg by Valerio Verrando
 * 980402 moved out of msdos.c (N.S.), generalized routines (BW)
 */

#include "driver.h"
#include <allegro.h>
#include <ctype.h>

/* from main() */
extern int ignorecfg;

/* from video.c */
extern int scanlines, use_double, video_sync, antialias, use_synced, ntsc;
extern int vgafreq, color_depth, skiplines, skipcolumns;
extern int beam, flicker, vesa;
extern float gamma_correction;
extern int gfx_mode, gfx_width, gfx_height;

/* from sound.c */
extern int usefm, soundcard;

/* from input.c */
extern int joy_type, use_mouse;

/* from fileio.c */
void decompose_rom_sample_path (char *rompath, char *samplepath);
extern char *hidir, *cfgdir, *inpdir, *pcxdir, *alternate_name;


static int mame_argc;
static char **mame_argv;
static int game;
char *rompath, *samplepath;

/*
 * gets some boolean config value.
 * 0 = false, >0 = true, <0 = auto
 * the shortcut can only be used on the commandline
 */
static int get_bool (char *section, char *option, char *shortcut, int def)
{
	char *yesnoauto;
	int res, i;

	res = def;

	if (ignorecfg) goto cmdline;

	/* look into mame.cfg, [section] */
	if (def == 0)
		yesnoauto = get_config_string(section, option, "no");
	else if (def > 0)
		yesnoauto = get_config_string(section, option, "yes");
	else /* def < 0 */
		yesnoauto = get_config_string(section, option, "auto");

	/* if the option doesn't exist in the cfgfile, create it */
	if (get_config_string(section, option, "#") == "#")
		set_config_string(section, option, yesnoauto);

	/* look into mame.cfg, [gamename] */
	yesnoauto = get_config_string((char *)drivers[game]->name, option, yesnoauto);

	/* also take numerical values instead of "yes", "no" and "auto" */
	if      (stricmp(yesnoauto, "no"  ) == 0) res = 0;
	else if (stricmp(yesnoauto, "yes" ) == 0) res = 1;
	else if (stricmp(yesnoauto, "auto") == 0) res = -1;
	else    res = atoi (yesnoauto);

cmdline:
	/* check the commandline */
	for (i = 1; i < mame_argc; i++)
	{
		if (mame_argv[i][0] != '-') continue;
		/* look for "-option" */
		if (stricmp(&mame_argv[i][1], option) == 0)
			res = 1;
		/* look for "-shortcut" */
		if (shortcut && (stricmp(&mame_argv[i][1], shortcut) == 0))
			res = 1;
		/* look for "-nooption" */
		if (strnicmp(&mame_argv[i][1], "no", 2) == 0)
		{
			if (stricmp(&mame_argv[i][3], option) == 0)
				res = 0;
			if (shortcut && (stricmp(&mame_argv[i][3], shortcut) == 0))
				res = 0;
		}
		/* look for "-autooption" */
		if (strnicmp(&mame_argv[i][1], "auto", 4) == 0)
		{
			if (stricmp(&mame_argv[i][5], option) == 0)
				res = -1;
			if (shortcut && (stricmp(&mame_argv[i][5], shortcut) == 0))
				res = -1;
		}
	}
	return res;
}

static int get_int (char *section, char *option, char *shortcut, int def)
{
	int res,i;

	res = def;

	if (!ignorecfg)
	{
		/* if the option does not exist, create it */
		if (get_config_int (section, option, -777) == -777)
			set_config_int (section, option, def);

		/* look into mame.cfg, [section] */
		res = get_config_int (section, option, def);

		/* look into mame.cfg, [gamename] */
		res = get_config_int ((char *)drivers[game]->name, option, res);
	}

	/* get it from the commandline */
	for (i = 1; i < mame_argc; i++)
	{
		if (mame_argv[i][0] != '-')
			continue;
		if ((stricmp(&mame_argv[i][1], option) == 0) ||
			(shortcut && (stricmp(&mame_argv[i][1], shortcut ) == 0)))
		{
			i++;
			if (i < mame_argc) res = atoi (mame_argv[i]);
		}
	}
	return res;
}

static int get_float (char *section, char *option, char *shortcut, float def)
{
	int i;
	float res;

	res = def;

	if (!ignorecfg)
	{
		/* if the option does not exist, create it */
		if (get_config_float (section, option, 9999.0) == 9999.0)
			set_config_float (section, option, def);

		/* look into mame.cfg, [section] */
		res = get_config_float (section, option, def);

		/* look into mame.cfg, [gamename] */
		res = get_config_float ((char *)drivers[game]->name, option, res);
	}

	/* get it from the commandline */
	for (i = 1; i < mame_argc; i++)
	{
		if (mame_argv[i][0] != '-')
			continue;
		if ((stricmp(&mame_argv[i][1], option) == 0) ||
			(shortcut && (stricmp(&mame_argv[i][1], shortcut ) == 0)))
		{
			i++;
			if (i < mame_argc) res = atof (mame_argv[i]);
		}
	}
	return res;
}

static char *get_string (char *section, char *option, char *shortcut, char *def)
{
	char *res;
	int i;

	res = def;

	if (!ignorecfg)
	{
		/* if the option does not exist, create it */
		if (get_config_string (section, option, "#") == "#" )
			set_config_string (section, option, def);

		/* look into mame.cfg, [section] */
		res = get_config_string(section, option, def);

		/* look into mame.cfg, [gamename] */
		res = get_config_string((char*)drivers[game]->name, option, res);
	}

	/* get it from the commandline */
	for (i = 1; i < mame_argc; i++)
	{
		if (mame_argv[i][0] != '-')
			continue;

		if ((stricmp(&mame_argv[i][1], option) == 0) ||
			(shortcut && (stricmp(&mame_argv[i][1], shortcut)  == 0)))
		{
			i++;
			if (i < mame_argc) res = mame_argv[i];
		}
	}
	return res;
}

void get_rom_sample_path (int argc, char **argv, int game_index)
{
	int i;

	alternate_name = 0;
	mame_argc = argc;
	mame_argv = argv;
	game = game_index;

	rompath    = get_string ("directory", "rompath",    NULL, ".;ROMS");
	samplepath = get_string ("directory", "samplepath", NULL, ".;SAMPLES");

	/* handle '-romdir' hack. We should get rid of this BW */
	alternate_name = 0;
	for (i = 1; i < argc; i++)
	{
		if (stricmp (argv[i], "-romdir") == 0)
		{
			i++;
			if (i < argc) alternate_name = argv[i];
		}
	}

	/* decompose paths into components (handled by fileio.c) */
	decompose_rom_sample_path (rompath, samplepath);
}

void parse_cmdline (int argc, char **argv, struct GameOptions *options, int game_index)
{
	static float f_beam, f_flicker;
	static int vesa;
	static char *resolution;
	char tmpres[10];
	int i;

	mame_argc = argc;
	mame_argv = argv;
	game = game_index;

	/* read graphic configuration */
	scanlines   = get_bool   ("config", "scanlines",    NULL,  1);
	use_double  = get_bool   ("config", "double",       NULL, -1);
	video_sync  = get_bool   ("config", "vsync",        NULL,  0);
	antialias   = get_bool   ("config", "antialias",    NULL,  1);
	use_synced  = get_bool   ("config", "syncedtweak",  NULL,  1);
	vesa        = get_bool   ("config", "vesa",         NULL,  0);
	ntsc        = get_bool   ("config", "ntsc",         NULL,  0);
	vgafreq     = get_int    ("config", "vgafreq",      NULL,  0);
	color_depth = get_int    ("config", "depth",        NULL, 16);
	skiplines   = get_int    ("config", "skiplines",    NULL, 0);
	skipcolumns = get_int    ("config", "skipcolumns",  NULL, 0);
	f_beam      = get_float  ("config", "beam",         NULL, 1.0);
	f_flicker   = get_float  ("config", "flicker",      NULL, 0.0);
	gamma_correction = get_float ("config", "gamma",   NULL, 1.0);

	options->frameskip = get_int  ("config", "frameskip", NULL, 0);
	options->norotate  = get_bool ("config", "norotate",  NULL, 0);
	options->ror       = get_bool ("config", "ror",       NULL, 0);
	options->rol       = get_bool ("config", "rol",       NULL, 0);
	options->flipx     = get_bool ("config", "flipx",     NULL, 0);
	options->flipy     = get_bool ("config", "flipy",     NULL, 0);

	/* read sound configuration */
	soundcard           = get_int  ("config", "soundcard",  NULL, -1);
	usefm               = get_bool ("config", "oplfm",      "fm",  0);
	options->samplerate = get_int  ("config", "samplerate", "sr", 22050);
	options->samplebits = get_int  ("config", "samplebits", "sb", 8);

	/* read input configuration */
	use_mouse = get_bool ("config", "mouse",   NULL,  1);
	joy_type  = get_int  ("config", "joytype", "joy", -1);

	/* misc configuration */
	options->cheat      = get_bool ("config", "cheat", NULL, 0);
	options->mame_debug = get_bool ("config", "debug", NULL, 0);

	/* get resolution */
	resolution  = get_string ("config", "resolution", NULL, "auto");

	/* set default subdirectories */
	hidir      = get_string ("directory", "hi",      NULL, "HI");
	cfgdir     = get_string ("directory", "cfg",     NULL, "CFG");
	pcxdir     = get_string ("directory", "pcx",     NULL, "PCX");
	inpdir     = get_string ("directory", "inp",     NULL, "INP");

	/* this is handled externally cause the audit stuff needs it, too */
	get_rom_sample_path (argc, argv, game_index);

	/* process some parameters */
	beam = (int)(f_beam * 0x00010000);
	if (beam < 0x00010000)
		beam = 0x00010000;
	if (beam > 0x00100000)
		beam = 0x00100000;

	flicker = (int)(f_flicker * 2.55);
	if (flicker < 0)
		flicker = 0;
	if (flicker > 255)
		flicker = 255;

	if (vesa == 1)
		gfx_mode = GFX_VESA2L;
//	else
//		gfx_mode = GFX_VGA;

	/* any option that starts with a digit is taken as a resolution option */
	/* this is to handle the old "-wxh" commandline option. */
	for (i = 1; i < argc; i++)
	{
		if ((argv[i][0] == '-') && (isdigit (argv[i][1])))
			resolution = &argv[i][1];
	}

	/* break up resolution into gfx_width and gfx_height */
	gfx_height = gfx_width = 0;
	if (stricmp (resolution, "auto") != 0)
	{
		char *tmp;
		strncpy (tmpres, resolution, 10);
		tmp = strtok (tmpres, "xX");
		gfx_width = atoi (tmp);
		tmp = strtok (0, "xX");
		if (tmp)
			gfx_height = atoi (tmp);
	}
}

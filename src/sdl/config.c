
#include "osd_sdl.h"

void osd_set_config(int def_samplerate, int def_samplebits)
{
    //printf( "TOMCXXX: osd_set_config(%d,%d)\n", def_samplerate, def_samplebits);
}

void osd_save_config(int frameskip, int samplerate, int samplebits)
{
    //printf( "TOMCXXX: osd_save_config(%d,%d,%d)\n", frameskip, samplerate, samplebits);
}

int osd_get_config_samplerate(int def_samplerate)
{
    //printf( "TOMCXXX: osd_get_config_samplerate(%d)\n", def_samplerate);
    return def_samplerate;
}

int osd_get_config_samplebits(int def_samplebits)
{
    //printf( "TOMCXXX: osd_get_config_samplebits(%d)\n", def_samplebits);
    return def_samplebits;
}

int osd_get_config_frameskip(int def_frameskip)
{
    //printf( "TOMCXXX: osd_get_config_frameskip(%d)\n", def_frameskip);
    return def_frameskip;
}

char *get_config_string(const char *section, const char *name, const char *def)
{
    //printf( "TOMCXXX: get_config_string(%s,%s,%s)\n", section, name, def);
    return (char *) def;
}

int get_config_int(const char *section, const char *name, const int def)
{
    //printf( "TOMCXXX: get_config_int(%s,%s,%d)\n", section, name, def);
    return def;
}

float get_config_float(const char *section, const char *name, const float def)
{
    //printf( "TOMCXXX: get_config_int(%s,%s,%f)\n", section, name, def);
    return def;
}

void set_config_string(const char *section, const char *name, const char *val)
{
    //printf( "TOMCXXX: set_config_string(%s,%s,%s)\n", section, name, val);
}

void set_config_int(const char *section, const char *name, const int val)
{
    //printf( "TOMCXXX: set_config_int(%s,%s,%d)\n", section, name, val);
}

void set_config_float(const char *section, const char *name, const float val)
{
    //printf( "TOMCXXX: set_config_float(%s,%s,%f)\n", section, name, val);
}

/*
 * gets some boolean config value.
 * 0 = false, >0 = true, <0 = auto
 * the shortcut can only be used on the commandline
 */
static int get_bool (char *section, char *option, char *shortcut, int def)
{
    char *yesnoauto;
    int res;

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
    for( int i = 1; i < mame_argc; i++)
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
    int res;

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
    for( int i = 1; i < mame_argc; i++)
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
    for( int i = 1; i < mame_argc; i++)
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
    for( int i = 1; i < mame_argc; i++)
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
    alternate_name = 0;
    mame_argc = argc;
    mame_argv = argv;
    game = game_index;

    rompath    = get_string ("directory", "rompath",    NULL, ".;roms");
    samplepath = get_string ("directory", "samplepath", NULL, ".;samples");

    /* handle '-romdir' hack. We should get rid of this BW */
    alternate_name = 0;
    for( int i = 1; i < argc; i++)
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

void parse_cmdline (int argc, char **argv, int game_index)
{
	static float f_beam, f_flicker;
	char *resolution;
	char *vesamode;
	char *joyname;
	char tmpres[10];
	int i;
	char *tmpstr;
	char *monitorname;

	mame_argc = argc;
	mame_argv = argv;
	game = game_index;


	/* force third mouse button emulation to "no" otherwise Allegro will default to "yes" */
	set_config_string(0,"emulate_three","no");

	/* read graphic configuration */
	scanlines   = get_bool   ("config", "scanlines",    NULL,  1);
	stretch     = get_bool   ("config", "stretch",		NULL,  1);
	video_sync  = get_bool   ("config", "vsync",        NULL,  0);
	use_dirty	= get_bool	 ("config", "dirty",	NULL,	-1);
	options.antialias   = get_bool   ("config", "antialias",    NULL,  1);
	options.translucency = get_bool    ("config", "translucency", NULL, 1);

	tmpstr             = get_string ("config", "depth", NULL, "auto");
	options.color_depth = atoi(tmpstr);
	if (options.color_depth != 8 && options.color_depth != 16) options.color_depth = 0;	/* auto */

	f_beam      = get_float  ("config", "beam",         NULL, 1.0);
	if (f_beam < 1.0) f_beam = 1.0;
	if (f_beam > 16.0) f_beam = 16.0;
	f_flicker   = get_float  ("config", "flicker",      NULL, 0.0);
	if (f_flicker < 0.0) f_flicker = 0.0;
	if (f_flicker > 100.0) f_flicker = 100.0;
	osd_gamma_correction = get_float ("config", "gamma",   NULL, 1.0);
	if (osd_gamma_correction < 0.5) osd_gamma_correction = 0.5;
	if (osd_gamma_correction > 2.0) osd_gamma_correction = 2.0;

	tmpstr             = get_string ("config", "frameskip", "fs", "auto");
	if (!stricmp(tmpstr,"auto"))
	{
		frameskip = 0;
		autoframeskip = 1;
	}
	else
	{
		frameskip = atoi(tmpstr);
		autoframeskip = 0;
	}
	options.norotate  = get_bool ("config", "norotate",  NULL, 0);
	options.ror       = get_bool ("config", "ror",       NULL, 0);
	options.rol       = get_bool ("config", "rol",       NULL, 0);
	options.flipx     = get_bool ("config", "flipx",     NULL, 0);
	options.flipy     = get_bool ("config", "flipy",     NULL, 0);

	/* read sound configuration */
	options.use_emulated_ym3812 = !get_bool ("config", "ym3812opl",  NULL,  0);
	options.samplerate = get_int  ("config", "samplerate", "sr", 22050);
	if (options.samplerate < 5000) options.samplerate = 5000;
	if (options.samplerate > 50000) options.samplerate = 50000;
	attenuation         = get_int  ("config", "volume",  NULL,  0);
	if (attenuation < -32) attenuation = -32;
	if (attenuation > 0) attenuation = 0;

	/* read input configuration */
	use_mouse = get_bool   ("config", "mouse",   NULL,  1);
	joyname   = get_string ("config", "joystick", "joy", "none");

	/* misc configuration */
	options.cheat      = get_bool ("config", "cheat", NULL, 0);
	options.mame_debug = get_bool ("config", "debug", NULL, 0);

	#ifndef MESS
	cheatfile  = get_string ("config", "cheatfile", "cf", "CHEAT.DAT");
	#else
	tmpstr  = get_string ("config", "cheatfile", "cf", "CHEAT.CDB");
	/* I assume that CHEAT.DAT (in old MESS.CFG files) and CHEAT.CDB are default filenames */
	if ((!stricmp(tmpstr,"cheat.dat")) || (!stricmp(tmpstr,"cheat.cdb")))
		sprintf(cheatfile,"%s.cdb",drivers[game_index]->name);
	else
		sprintf(cheatfile,"%s",tmpstr);
	#endif


 	#ifndef MESS
 	history_filename  = get_string ("config", "historyfile", NULL, "HISTORY.DAT");    /* JCK 980917 */
 	#else
 	history_filename  = get_string ("config", "historyfile", NULL, "SYSINFO.DAT");
 	#endif

	mameinfo_filename  = get_string ("config", "mameinfofile", NULL, "MAMEINFO.DAT");    /* JCK 980917 */

	/* get resolution */
	resolution  = get_string ("config", "resolution", NULL, "auto");

	/* set default subdirectories */
	nvdir      = get_string ("directory", "nvram",   NULL, "NVRAM");
	hidir      = get_string ("directory", "hi",      NULL, "HI");
	cfgdir     = get_string ("directory", "cfg",     NULL, "CFG");
	screenshotdir = get_string ("directory", "snap",     NULL, "SNAP");
	memcarddir = get_string ("directory", "memcard", NULL, "MEMCARD");
	stadir     = get_string ("directory", "sta",     NULL, "STA");
	artworkdir = get_string ("directory", "artwork", NULL, "ARTWORK");

 	#ifndef MESS
		cheatdir = get_string ("directory", "cheat", NULL, ".");
 	#else
		crcdir = get_string ("directory", "crc", NULL, "CRC");
		cheatdir = get_string ("directory", "cheat", NULL, "CHEAT");
 	#endif

	logerror("cheatfile = %s - cheatdir = %s\n",cheatfile,cheatdir);

	tmpstr = get_string ("config", "language", NULL, "english");
	options.language_file = osd_fopen(0,tmpstr,OSD_FILETYPE_LANGUAGE,0);

	/* this is handled externally cause the audit stuff needs it, too */
	get_rom_sample_path (argc, argv, game_index);

	/* process some parameters */
	options.beam = (int)(f_beam * 0x00010000);
	if (options.beam < 0x00010000)
		options.beam = 0x00010000;
	if (options.beam > 0x00100000)
		options.beam = 0x00100000;

	options.flicker = (int)(f_flicker * 2.55);
	if (options.flicker < 0)
		options.flicker = 0;
	if (options.flicker > 255)
		options.flicker = 255;

	/* any option that starts with a digit is taken as a resolution option */
	/* this is to handle the old "-wxh" commandline option. */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-' && isdigit(argv[i][1]) &&
				(strstr(argv[i],"x") || strstr(argv[i],"X")))
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

		options.vector_width = gfx_width;
		options.vector_height = gfx_height;
	}
}

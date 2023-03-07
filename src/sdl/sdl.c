
#define OSD_SDL_MAIN 1
#include "osd_sdl.h"

// whether sound is wanted or not (command line argument "-nosound")

unsigned char No_FM = 1;
static unsigned char current_background_color;
extern void decompose_rom_sample_path (char *rompath, char *samplepath);
static unsigned char current_background_color;

/* from video.c */
static int antialias, use_synced, ntsc;
static int vgafreq, skiplines, skipcolumns;
static int beam, flicker, vesa;
static float gamma_correction;

/* from sound.c */
static int usefm, soundcard;


static int viswidth;
static int visheight;
static int skiplinesmax;
static int skipcolumnsmax;
static int skiplinesmin;
static int skipcolumnsmin;

static int doubling = 0;

// joystick state (not implemented)
int osd_joy_up, osd_joy_down, osd_joy_left, osd_joy_right;
int osd_joy_b1, osd_joy_b2,   osd_joy_b3,   osd_joy_b4;

void fatal(char *s)
{
    fprintf(stderr,"fatal: %s\n",s);
    exit(1);
}

void *debug_malloc(ssize_t size)
{
    void *p = malloc(size);
    if( !p ) {
        fprintf(stderr,"fatal: malloc(%ld) failed\n", (long) size);
        exit(1);
    }
    return p;
}

void debug_free(void *p)
{
    free(p);
}

int osd_init(void)
{
    SDLCHK(20101,!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));

    displayWindow     = (SDL_Window *) 0;
    displayRenderer   = (SDL_Renderer *) 0;
    displayTexture    = (SDL_Texture  *) 0;
    pixelsMappedToARGB8888 = (uint32_t *) 0;
    first_free_pen    = 0;
    frame_count       = 0;
    play_sound        = 1;
    displayScale      = 3; // default fairly big
	attenuation = 0;
	throttle = 1; /* toggled by F10 */
	osd_gamma_correction = 1;
	dirtycolor = NULL;
	current_palette = NULL;
	palette_16bit_lookup = NULL;

    init_keymap();

	init_sound();

    return 0;
}

void osd_exit(void)
{
    stop_all_sound();
    SDL_Quit();
}

uint64_t now()
{
    struct timespec ts;
    uint64_t now;
    clock_gettime(CLOCK_REALTIME, &ts);
    now = ts.tv_sec * NS_PER_SECOND + ts.tv_nsec;
    return now;
}

void osd_profiler(int type)
{
}

void CLIB_DECL logerror(const char *text,...)
{
	va_list arg;
	va_start(arg,text);
	if (errorlog)
		vfprintf(errorlog,text,arg);
	va_end(arg);
}

unsigned int osd_cycles(void)
{
    return 0;
}

int main( int argc, char **argv )
{
    int res, j, game_index;
    char *playbackname = NULL;

	memset(&options,0,sizeof(options));

    /* these two are not available in mame.cfg */
    ignorecfg = 0;
    errorlog = 0;

	game_index = -1;

    for( int i = 1; i < argc; i++) /* V.V_121997 */
    {
        if (stricmp(argv[i],"-ignorecfg") == 0) ignorecfg = 1;
        else if (stricmp(argv[i],"-log") == 0) {
            errorlog = fopen("error.log","wa");
        }
        else if( strcmp(argv[i],"-scale") == 0 ) {
            i++;
            displayScale = atoi(argv[i]);
            if( displayScale < 1 ) {
                displayScale = 1;
            }
            if( displayScale > 7 ) {
                displayScale = 7;
            }
        }
        else if( strcmp(argv[i],"-nosound") == 0 ) {
            play_sound = 0;
        }
    }

    //TOMCXXX set_config_file ("mame.cfg");

    /* check for frontend options */
    res = frontend_help (argc, argv);

    /* if frontend options were used, return to DOS with the error code */
    if (res != 1234)
        exit (res);

    /* take the first commandline argument without "-" as the game name */
    for (j = 1; j < argc; j++)
        if (argv[j][0] != '-') break;

    /* do we have a drivers for this? */
    for( int i = 0; drivers[i]; i++) {
        if (stricmp(argv[j],drivers[i]->name) == 0) {
            game_index = i;
            break;
        }
    }

    if( game_index < 0 ) {
        printf("Game \"%s\" not supported\n", argv[j]);
        return 1;
    }

    /* parse generic (os-independent) options */
    parse_cmdline (argc, argv, game_index);

    /* handle record and playback. These are not available in mame.cfg */
    for( int i = 1; i < argc; i++)
    {
        if (stricmp(argv[i],"-record") == 0)
        {
            i++;
            if (i < argc)
                options.record = osd_fopen(argv[i],0,OSD_FILETYPE_INPUTLOG,1);
        }
        if (stricmp(argv[i],"-playback") == 0)
        {
            i++;
            if (i < argc)
                options.playback = osd_fopen(argv[i],0,OSD_FILETYPE_INPUTLOG,0);
        }
    }

    /* go for it */
    res = run_game (game_index);

    /* close open files */
    if (errorlog) fclose (errorlog);
    if (options.playback) osd_fclose (options.playback);
    if (options.record)   osd_fclose (options.record);

    exit (res);
}

/***************************************************************************

  osdepend.c

  OS dependant stuff (display handling, keyboard scan...)
  This is the only file which should me modified in order to port the
  emulator to a different system.

***************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "stricmp.h"
#include "driver.h"
#include "osdepend.h"

#include "SDL.h"

// whether sound is wanted or not (command line argument "-nosound")
int play_sound;

struct GameOptions options;
int  ignorecfg;
unsigned char No_FM = 1;
static unsigned char current_background_color;
extern void decompose_rom_sample_path (char *rompath, char *samplepath);
static unsigned char current_palette[256][3];
static unsigned char current_background_color;
int throttle = 1;       /* toggled by F10 */

/* from video.c */
static int scanlines, use_double, antialias, use_synced, ntsc;
int video_sync;
static int vgafreq, color_depth, skiplines, skipcolumns;
static int beam, flicker, vesa;
static float gamma_correction;
static int gfx_width, gfx_height;

static int game_width;
static int game_height;
static int game_attributes;

/* from sound.c */
static int usefm, soundcard;

/* from input.c */
static int joy_type, use_mouse;

/* from fileio.c */
static char *hidir, *cfgdir, *inpdir, *pcxdir, *alternate_name;


static int mame_argc;
static char **mame_argv;
static int game;
static char *rompath, *samplepath;

static int viswidth;
static int visheight;
static int skiplinesmax;
static int skipcolumnsmax;
static int skiplinesmin;
static int skipcolumnsmin;

static int vector_game;
static int use_dirty;

static int doubling = 0;

// joystick state (not implemented)
int osd_joy_up, osd_joy_down, osd_joy_left, osd_joy_right;
int osd_joy_b1, osd_joy_b2,   osd_joy_b3,   osd_joy_b4;

#define MAX_PENS 256
#define NKEYS    OSD_KEY_F12+1

static int displayScale;

#define NS_PER_SECOND 1000000000
#define KEEP_ALIVE_DISPLAY_TIME ((uint64_t) (0.5*NS_PER_SECOND)) // refresh every half a second
static uint64_t last_display_update_time;

static SDL_Window        *displayWindow;
static SDL_Renderer      *displayRenderer;
static SDL_Texture       *displayTexture;

static struct osd_bitmap *displayBitmap;

static int first_free_pen;

static uint32_t *pixelsMappedToARGB8888;
static uint32_t paletteMapping[MAX_PENS];

static int keys[NKEYS];

static int map_sdl_scancode_to_osd_key[SDL_NUM_SCANCODES+1];
static int last_key_pressed;

static int frame_count;

#define SDLCHK(C) { long long result = (long long) (C); if( !result ) { fprintf(stderr,"fatal: SDLCHK ##C returned %ld.  SDL_Error: %s\n", (long) result, SDL_GetError()); exit(1); } }
#define ASSERT(C) { if( !(C) ) { fatal("assertion failed"); } }

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

// audio
#define AUDIO_OUTPUT_FREQ 44100
#define NUM_AUDIO_CHANNELS 4
#define SAMPLES_PER_CALLBACK 128
#define STREAM_QUEUE_LENGTH 8 
#define STREAM_MAX_BUFSIZE  32768
typedef struct {
    int                channel;
    SDL_AudioSpec      desired;
    SDL_AudioSpec      obtained;
    SDL_AudioDeviceID  audioDeviceID;
    SDL_AudioStream   *audioStream;
    signed char       *data;
    int                len;
    int                freq;
    int                volume;
    int                loop;
    float              time;
} AudioChannel;
static AudioChannel audioChannels[NUM_AUDIO_CHANNELS];

void free_audio_channel_stream(AudioChannel *audioChannel)
{
    void *tmp = (void *) audioChannel->audioStream;
    if( tmp ) {
        audioChannel->audioStream = NULL;
        SDL_FreeAudioStream(tmp);
    }
}

void free_audio_channel_data(AudioChannel *audioChannel)
{
    // if not just continuing a stream, free the current data
    void *tmp = (void *) audioChannel->data;
    if( tmp ) {
        audioChannel->data = NULL;
        debug_free(tmp);
    }
}

void audioCallback(void *userdata, unsigned char *stream, int len)
{
    AudioChannel *audioChannel = (AudioChannel *) userdata;

    if( audioChannel->audioStream ) {
        int avail = SDL_AudioStreamAvailable(audioChannel->audioStream);
        if( avail >= len ) {
            // we have enough data to fill the rest
            SDLCHK(SDL_AudioStreamGet(audioChannel->audioStream, stream, len) == len);
        }
        // else do nothing?
    }
    else {
        char *s = (char *) stream; // change to signed
        for( int i=0; i<len; i++) {
            s[i] = audioChannel->data[((int)audioChannel->time) % audioChannel->len];
            s[i] *= (float) audioChannel->volume / (float) 255;
            audioChannel->time += (double) audioChannel->freq / (double) AUDIO_OUTPUT_FREQ;
            while( audioChannel->time >= audioChannel->len ) {
                audioChannel->time -= audioChannel->len;
                if( audioChannel->loop >= 0 ) {
                    audioChannel->loop--;
                    if( audioChannel->loop < 0 ) {
                        osd_stop_sample(audioChannel->channel);
                        audioChannel->time = 0;
                    }
                }
            }
        }
    }
}

void init_keymap()
{
    // we just use a giant array to map SDL_SCANCODE_XXX to OSD_KEY_XXX

    map_sdl_scancode_to_osd_key[SDL_SCANCODE_A           ] = OSD_KEY_A;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_B           ] = OSD_KEY_B;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_C           ] = OSD_KEY_C;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_D           ] = OSD_KEY_D;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_E           ] = OSD_KEY_E;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F           ] = OSD_KEY_F;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_G           ] = OSD_KEY_G;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_H           ] = OSD_KEY_H;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_I           ] = OSD_KEY_I;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_J           ] = OSD_KEY_J;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_K           ] = OSD_KEY_K;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_L           ] = OSD_KEY_L;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_M           ] = OSD_KEY_M;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_N           ] = OSD_KEY_N;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_O           ] = OSD_KEY_O;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_P           ] = OSD_KEY_P;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_Q           ] = OSD_KEY_Q;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_R           ] = OSD_KEY_R;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_S           ] = OSD_KEY_S;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_T           ] = OSD_KEY_T;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_U           ] = OSD_KEY_U;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_V           ] = OSD_KEY_V;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_W           ] = OSD_KEY_W;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_X           ] = OSD_KEY_X;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_Y           ] = OSD_KEY_Y;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_Z           ] = OSD_KEY_Z;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_1           ] = OSD_KEY_1;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_2           ] = OSD_KEY_2;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_3           ] = OSD_KEY_3;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_4           ] = OSD_KEY_4;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_5           ] = OSD_KEY_3; //TOMCXXX KLUDGE to map coin in from modern mame (5 instead of 3)
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_6           ] = OSD_KEY_6;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_7           ] = OSD_KEY_7;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_8           ] = OSD_KEY_8;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_9           ] = OSD_KEY_9;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_0           ] = OSD_KEY_0;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_ESCAPE      ] = OSD_KEY_ESC;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_RETURN      ] = OSD_KEY_ENTER;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_BACKSPACE   ] = OSD_KEY_BACKSPACE;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_TAB         ] = OSD_KEY_TAB;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_SPACE       ] = OSD_KEY_SPACE;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_MINUS       ] = OSD_KEY_MINUS;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_EQUALS      ] = OSD_KEY_EQUALS;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_LEFTBRACKET ] = OSD_KEY_OPENBRACE;  // not quite
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_RIGHTBRACKET] = OSD_KEY_CLOSEBRACE; // not quite
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_SEMICOLON   ] = OSD_KEY_COLON;      // not quite
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_APOSTROPHE  ] = OSD_KEY_QUOTE;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_GRAVE       ] = OSD_KEY_TILDE;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_COMMA       ] = OSD_KEY_COMMA;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_SLASH       ] = OSD_KEY_SLASH;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_CAPSLOCK    ] = OSD_KEY_CAPSLOCK;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F1          ] = OSD_KEY_F1;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F2          ] = OSD_KEY_F2;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F3          ] = OSD_KEY_F3;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F4          ] = OSD_KEY_F4;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F5          ] = OSD_KEY_F5;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F6          ] = OSD_KEY_F6;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F7          ] = OSD_KEY_F7;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F8          ] = OSD_KEY_F8;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F9          ] = OSD_KEY_F9;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F10         ] = OSD_KEY_F10;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F11         ] = OSD_KEY_F11;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_F12         ] = OSD_KEY_F12;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_SCROLLLOCK  ] = OSD_KEY_SCRLOCK;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_INSERT      ] = OSD_KEY_INSERT;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_HOME        ] = OSD_KEY_HOME;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_PAGEUP      ] = OSD_KEY_PGUP;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_DELETE      ] = OSD_KEY_DEL;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_END         ] = OSD_KEY_END;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_PAGEDOWN    ] = OSD_KEY_PGDN;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_RIGHT       ] = OSD_KEY_RIGHT;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_LEFT        ] = OSD_KEY_LEFT;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_DOWN        ] = OSD_KEY_DOWN;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_UP          ] = OSD_KEY_UP;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_NUMLOCKCLEAR] = OSD_KEY_NUMLOCK;
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_LCTRL       ] = OSD_KEY_LCONTROL;
}

int osd_init(void)
{
    SDLCHK(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));

    displayWindow     = (SDL_Window *) 0;
    displayRenderer   = (SDL_Renderer *) 0;
    displayBitmap     = (struct osd_bitmap *) 0;
    pixelsMappedToARGB8888 = (uint32_t *) 0;
    first_free_pen    = 0;
    frame_count       = 0;
    play_sound        = 1;
    displayScale      = 3; // default fairly big

    init_keymap();

    if( play_sound ) {
        // init sound
        for( int channel=0; channel < NUM_AUDIO_CHANNELS; channel++ ) {
            AudioChannel *audioChannel = &audioChannels[channel];
            audioChannel->channel          = channel;
            audioChannel->desired.freq     = AUDIO_OUTPUT_FREQ;
            audioChannel->desired.format   = AUDIO_S8;
            audioChannel->desired.channels = 1;
            audioChannel->desired.samples  = SAMPLES_PER_CALLBACK;
            audioChannel->desired.callback = audioCallback;
            audioChannel->desired.userdata = audioChannel;
            SDLCHK(audioChannel->audioDeviceID = SDL_OpenAudioDevice(NULL, 0, &audioChannel->desired, &audioChannel->obtained, 0));
        }
    }

    return 0;
}

void stop_all_sound()
{
    if( play_sound ) {
        for( int channel=0; channel<NUM_AUDIO_CHANNELS; channel++ ) {
            osd_stop_sample(channel);
        }
    }
}

void osd_exit(void)
{
    stop_all_sound();
    SDL_Quit();
}

struct osd_bitmap *osd_new_bitmap(int width,int height,int depth)
{
    struct osd_bitmap *bitmap;
    //printf( "TOMCXXX: osd_new_bitmap(%d,%d,%d)  Machine->orientation=%d\n", width, height, depth, Machine->orientation);

    if (Machine->orientation & ORIENTATION_SWAP_XY)
    {
        int temp;

        temp = width;
        width = height;
        height = temp;
    }

    if ((bitmap = debug_malloc(sizeof(struct osd_bitmap))) != 0)
    {
        int i,rowlen,rdwidth;
        unsigned char *bm;
        int safety;


        if (width > 32) safety = 8;
        else safety = 0;        /* don't create the safety area for GfxElement bitmaps */

        if (depth != 8 && depth != 16) depth = 8;

        bitmap->depth = depth;
        bitmap->width = width;
        bitmap->height = height;

        rdwidth = (width + 7) & ~7;     /* round width to a quadword */
        if (depth == 16)
            rowlen = 2 * (rdwidth + 2 * safety) * sizeof(unsigned char);
        else
            rowlen =     (rdwidth + 2 * safety) * sizeof(unsigned char);

        if ((bm = malloc((height + 2 * safety) * rowlen)) == 0)
        {
            free(bitmap);
            return 0;
        }

        if ((bitmap->line = malloc(height * sizeof(unsigned char *))) == 0)
        {
            free(bm);
            free(bitmap);
            return 0;
        }

        for (i = 0;i < height;i++)
            bitmap->line[i] = &bm[(i + safety) * rowlen + safety];

        bitmap->_private = bm;

        osd_clearbitmap(bitmap);
    }

    return bitmap;
}

void osd_clearbitmap(struct osd_bitmap *bitmap)
{
    for( int i = 0; i < bitmap->height; i++ ) {
        if (bitmap->depth == 16)
            memset(bitmap->line[i],0,2*bitmap->width);
        else
            memset(bitmap->line[i],current_background_color,bitmap->width);
    }


    if (bitmap == Machine->scrbitmap) {
        osd_mark_dirty (0,0,bitmap->width-1,bitmap->height-1,1);

        /* signal the layer system that the screenneeds a complete refresh */
        layer_mark_full_screen_dirty();
    }
}

void osd_free_bitmap(struct osd_bitmap *bitmap)
{
    if (bitmap) {
        debug_free(bitmap->_private);
        debug_free(bitmap);
    }
}

struct osd_bitmap *osd_create_display(int width,int height,unsigned int totalcolors,
        const unsigned char *palette,unsigned short *pens,int attributes)
{
    //printf( "TOMCXXX: osd_create_display(%d,%d,%d,%d) Machine->orientation=%d\n", width, height, totalcolors, attributes, Machine->orientation);
    if (errorlog)
        fprintf (errorlog, "width %d, height %d\n", width,height);

    if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
        vector_game = 1;
    else
        vector_game = 0;

    if ((Machine->drv->video_attributes & VIDEO_SUPPORTS_DIRTY) || vector_game)
        use_dirty = 1;
    else
        use_dirty = 0;

    game_width  = width;
    game_height = height;
    game_attributes = attributes;

    if( Machine->orientation & ORIENTATION_SWAP_XY ) {
        int temp;

        temp = game_width;
        game_width = height;
        game_height = temp;
    }

    if (vector_game)
    {
        /* for vector games, use_double == 0 means miniaturized. */
        /* TOMCXXX
        if (use_double == 0)
            scale_vectorgames(gfx_width/2,gfx_height/2,&width, &height);
        else
            scale_vectorgames(gfx_width,gfx_height,&width, &height);
        TOMCXXX */
        /* vector games are always non-doubling */
        use_double = 0;
        /* center display */
        //TOMCXXXXX adjust_display(0, 0, width-1, height-1);
    }
    else /* center display based on visible area */
    {
        struct rectangle vis = Machine->drv->visible_area;
        //TOMCXXXXX adjust_display (vis.min_x, vis.min_y, vis.max_x, vis.max_y);
    }

    SDLCHK(displayWindow   = SDL_CreateWindow( "origmame", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, game_width*displayScale, game_height*displayScale, SDL_WINDOW_RESIZABLE ));
    SDLCHK(displayRenderer = SDL_CreateRenderer( displayWindow, -1, 0 ));
    SDLCHK(displayTexture  = SDL_CreateTexture(displayRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, game_width, game_height));

    if( color_depth == 16 && (Machine->drv->video_attributes & VIDEO_SUPPORTS_16BIT) ) {
        displayBitmap = osd_new_bitmap(width,height,16);
    }
    else {
        displayBitmap = osd_new_bitmap(width,height,8);
    }

    if( !displayBitmap ) {
        return 0;
    }

    if( !osd_set_display(width, height, attributes) ) {
        return 0;
    }

    if( displayBitmap->depth == 16 ) {
#ifdef TOMCXXX_16BIT
        int r,g,b;


        for (r = 0; r < 32; r++)
            for (g = 0; g < 32; g++)
                for (b = 0; b < 32; b++)
                    *pens++ = makecol(8*r,8*g,8*b);
#endif /* TOMCXXX_16BIT */
    }
    else {
        /* initialize the palette */
        for( int i = 0; i < 256; i++) {
            current_palette[i][0] = current_palette[i][1] = current_palette[i][2] = 0;
        }

        /* fill the palette starting from the end, so we mess up badly written */
        /* drivers which don't go through Machine->pens[] */
        for( int i = 0; i < totalcolors; i++) {
            pens[i] = 255-i;
        }

        for( int i = 0; i < totalcolors; i++) {
            current_palette[pens[i]][0] = palette[3*i];
            current_palette[pens[i]][1] = palette[3*i+1];
            current_palette[pens[i]][2] = palette[3*i+2];

            paletteMapping[pens[i]] =                 (0xff  << 24) |
                                (current_palette[pens[i]][0] << 16) |
                                (current_palette[pens[i]][1] <<  8) |
                                (current_palette[pens[i]][2]      );
        }
    }

    return displayBitmap;
}

int osd_set_display(int width,int height,int attributes)
{
    //printf( "TOMCXXX: osd_set_display(%d, %d, %d)\n", width,height,attributes);
    pixelsMappedToARGB8888 = (uint32_t *) debug_malloc(width * height * sizeof(uint32_t));
    return 1;
}

void osd_close_display(void)
{
    if( pixelsMappedToARGB8888 ) {
        debug_free(pixelsMappedToARGB8888);
        pixelsMappedToARGB8888 = (uint32_t *) 0;
    }
    if( displayBitmap ) {
        osd_free_bitmap(displayBitmap);
        displayBitmap = (struct osd_bitmap *) 0;
    }
    if( displayTexture ) {
        SDL_DestroyTexture( displayTexture );
        displayTexture = NULL;
    }
    if( displayRenderer ) {
        SDL_DestroyRenderer( displayRenderer );
        displayRenderer = NULL;
    }
    if( displayWindow ) {
        SDL_DestroyWindow( displayWindow );
        displayWindow = NULL;
    }
}

void poll_events()
{
    SDL_Event e;
    while( SDL_PollEvent(&e) ) {
        if( e.type == SDL_KEYDOWN ) {
            int osd_key = map_sdl_scancode_to_osd_key[e.key.keysym.scancode];

            // hack - turn sound off when paused or dipswitch
            if( osd_key == OSD_KEY_P || osd_key == OSD_KEY_TAB ) {
                stop_all_sound();
            }

            keys[osd_key] = 1;
            last_key_pressed = osd_key;
        }
        else if( e.type == SDL_KEYUP ) {
            int osd_key = map_sdl_scancode_to_osd_key[e.key.keysym.scancode];
            keys[osd_key] = 0;
        }
        if( e.type == SDL_QUIT ) {
            keys[OSD_KEY_ESC] = 1;
        }
    }
}

uint64_t now()
{
    struct timespec ts;
    uint64_t now;
    clock_gettime(CLOCK_REALTIME, &ts);
    now = ts.tv_sec * NS_PER_SECOND + ts.tv_nsec;
    return now;
}

void do_update_display()
{
    int npixels = displayBitmap->width * displayBitmap->height;
    uint32_t *dstptr = pixelsMappedToARGB8888;
    for( int y=0; y<game_height; y++ ) {
        unsigned char *srcptr = displayBitmap->line[y];
        for( int x=0; x<game_width; x++ ) {
            *dstptr++ = paletteMapping[*srcptr++];
        }
    }

    SDL_UpdateTexture(displayTexture, NULL, pixelsMappedToARGB8888, game_width*sizeof(uint32_t));
    SDL_RenderCopy(displayRenderer, displayTexture, NULL, NULL);
    SDL_RenderPresent(displayRenderer);

    /* now wait until it's time to update the screen */
    uint64_t curr;
    extern int frameskip;
    if (throttle) {
        //printf( "TOMCXXX: frameskip=%d  Machine->drv->frames_per_second=%d\n", frameskip, Machine->drv->frames_per_second);
        while(1) {
            curr = now();
            int64_t delta  = (int64_t) curr - (int64_t) last_display_update_time;
            int64_t target = (frameskip+1) * NS_PER_SECOND/Machine->drv->frames_per_second;
            if( delta >= target ) {
                // time's up
                break;
            }

            poll_events(); // keep alive

            // recompute delta
            curr = now();
            delta  = (int64_t) curr - (int64_t) last_display_update_time;
            int sleep_usecs = (target - delta) / 1000000;
            if( sleep_usecs > 2000 ) { // leave some room
                usleep(1000);
            }
            //printf("TOMCXXX: curr=%lld   last_display_update_time=%lld   delta = %lld   target=%lld\n", (long long) curr, (long long) last_display_update_time, (long long) delta, (long long) target);
        }
    }

    last_display_update_time = now();
}

void osd_modify_pen(int pen,unsigned char red, unsigned char green, unsigned char blue)
{
    // TODO
    //printf( "TOMCXXX: osd_modify_pen(%d,%d,%d,%d)\n", pen, red, green, blue);
}

void osd_get_pen(int pen,unsigned char *red, unsigned char *green, unsigned char *blue)
{
    // TODO
    if( displayBitmap->depth == 16 ) {
#ifdef TOMCXXX_16BIT
        *red   = getr(pen);
        *green = getg(pen);
        *blue  = getb(pen);
#endif /* TOMCXXX_16BIT */
    }
    else {
        *red   = current_palette[pen][0];
        *green = current_palette[pen][1];
        *blue  = current_palette[pen][2];
    }
    //printf( "TOMCXXX: osd_get_pen(depth=%d,%d,(%d,%d,%d))\n", displayBitmap->depth, pen, (int) *red, (int) *green, (int) *blue);
}

void osd_mark_dirty(int xmin, int ymin, int xmax, int ymax, int ui)
{
    //printf( "TOMCXXX: osd_mark_dirty(%d,%d,%d,%d,%d)\n", xmin, ymin, xmax, ymax, ui);
}

void osd_update_display(void)
{
    frame_count++;

    do_update_display();

    poll_events();
}

void osd_update_audio(void)
{
    // not sure what needs to happen here
    //printf("TOMCXXX: osd_update_audio()\n");
}

void new_audio_channel_data(int channel,signed char *data,int len,int freq,int volume,int isstream)
{
    void *tmp;

    AudioChannel *audioChannel = &audioChannels[channel];

    if( audioChannel->audioStream && !isstream  ) {
        // going from stream to non-stream
        free_audio_channel_stream(audioChannel);
    }
    else if( !audioChannel->audioStream && isstream ) {
        // going from non-stream to stream
        free_audio_channel_stream(audioChannel);
        int base = Machine->sample_rate;
        int src_rate = base;
        int dst_rate = base * AUDIO_OUTPUT_FREQ / freq;
        audioChannel->audioStream = SDL_NewAudioStream( AUDIO_U8, 1, src_rate,
                                                        AUDIO_S8, 1, dst_rate );
    }

    if( audioChannel->audioStream ) {
        //printf( "TOMCXXX ********** PUTTING AUDIOSTREAM %p  d=%p l=%d\n", audioChannel->audioStream, data, len);
        SDLCHK(!SDL_AudioStreamPut(audioChannel->audioStream, data, len));
    }
    else {
        free_audio_channel_data(audioChannel);

        // make a copy of incoming data
        signed char *newdata = debug_malloc(len);
        memcpy(newdata,data,len);

        audioChannel->data = newdata;
        audioChannel->len  = len;
    }

    audioChannel->channel     = channel;
    audioChannel->freq        = freq;
    audioChannel->volume      = volume;
    audioChannel->time        = 0;
    /*
    static int TOMCXXX;
    if( TOMCXXX++ > 10 ) { exit(1); }
    */
}

void osd_play_sample(int channel,signed char *data,int len,int freq,int volume,int loop)
{
    //printf("TOMCXXX: osd_play_sample(%d, %p, %d, %d, %d, %d)\n", channel, data, len, freq, volume, loop);
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; }
    AudioChannel *audioChannel = &audioChannels[channel];

    new_audio_channel_data(channel, data, len, freq, volume, 0);

    audioChannel->loop = loop;

    osd_adjust_sample(channel,freq,volume);
}

void osd_play_sample_16(int channel,signed short *data,int len,int freq,int volume,int loop)
{
    //printf("TOMCXXX: osd_play_sample_16(%d, %p, %d, %d, %d, %d) not implemented!\n", channel, data, len, freq, volume, loop);
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; }
}

void osd_play_streamed_sample(int channel,signed char *data,int len,int freq,int volume)
{
    //printf("TOMCXXX: osd_play_streamed_sample(%d, %p, %d, %d, %d) [%4d %4d %4d %4d %4d %4d]\n", channel, data, len, freq, volume, data[0]&0xff, data[1]&0xff, data[2]&0xff, data[3]&0xff, data[4]&0xff, data[5]&0xff);
    /*
    for( int i=0; i<len; i += 16 ) {
        printf( "%02x ", (int)data[i   ] & 0xff);
        printf( "%02x ", (int)data[i+ 1] & 0xff);
        printf( "%02x ", (int)data[i+ 2] & 0xff);
        printf( "%02x ", (int)data[i+ 3] & 0xff);
        printf( "%02x ", (int)data[i+ 4] & 0xff);
        printf( "%02x ", (int)data[i+ 5] & 0xff);
        printf( "%02x ", (int)data[i+ 6] & 0xff);
        printf( "%02x ", (int)data[i+ 7] & 0xff);
        printf( "%02x ", (int)data[i+ 8] & 0xff);
        printf( "%02x ", (int)data[i+ 9] & 0xff);
        printf( "%02x ", (int)data[i+10] & 0xff);
        printf( "%02x ", (int)data[i+11] & 0xff);
        printf( "%02x ", (int)data[i+12] & 0xff);
        printf( "%02x ", (int)data[i+13] & 0xff);
        printf( "%02x ", (int)data[i+14] & 0xff);
        printf( "%02x ", (int)data[i+15] & 0xff);
    }
    printf( "\n");
    */
    AudioChannel *audioChannel = &audioChannels[channel];
    new_audio_channel_data(channel, data, len, freq, volume, 1);
    SDL_PauseAudioDevice(audioChannel->audioDeviceID, 0);
}

void osd_play_streamed_sample_16(int channel,signed short *data,int len,int freq,int volume)
{
    //printf("TOMCXXX: osd_play_streamed_sample_16(%d, %p, %d, %d, %d) not implemented!\n", channel, data, len, freq, volume);
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; }
}

void osd_adjust_sample(int channel,int freq,int volume)
{
    //printf("TOMCXXX: osd_adjust_sample(%d, %d, %d)\n", channel, freq, volume);
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; };

    AudioChannel *audioChannel = &audioChannels[channel];
    audioChannel->freq   = freq;
    audioChannel->volume = volume;
    SDL_PauseAudioDevice(audioChannel->audioDeviceID, 0);
}

void osd_stop_sample(int channel)
{
    //printf("TOMCXXX: osd_stop_sample(%d)\n", channel);
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; };

    AudioChannel *audioChannel = &audioChannels[channel];
    audioChannel->volume = 0;
    SDL_PauseAudioDevice(audioChannel->audioDeviceID, 1);
}

void osd_restart_sample(int channel)
{
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; };
    //printf("TOMCXXX: osd_restart_sample(%d)\n", channel);
}

int osd_get_sample_status(int channel)
{
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return 0; };
    //printf("TOMCXXX: osd_get_sample_status(%d)\n", channel);
    return 0;
}

void osd_ym2203_write(int n, int r, int v)
{
    //printf("TOMCXXX: osd_ym2203_write(%d,%d,%d)\n", n,r,v);
}

void osd_ym2203_update(void)
{
    //printf("TOMCXXX: osd_ym2203_update()\n");
}

int osd_ym3812_status(void)
{
    //printf("TOMCXXX: osd_ym3812_status()\n");
    return 0;
}

int osd_ym3812_read(void)
{
    //printf("TOMCXXX: osd_ym3812_read()\n");
    return 0;
}

void osd_ym3812_control(int reg)
{
    //printf("TOMCXXX: osd_ym3812_control(%d)\n",reg);
}

void osd_ym3812_write(int data)
{
    //printf("TOMCXXX: osd_ym3812_write(%d)\n",data);
}

void keep_alive(void)
{
    // to be called in tight loops.
    // ensures that SDL events continue to be polled
    poll_events();

    // also, periodically refresh the display
    if( now() - last_display_update_time > KEEP_ALIVE_DISPLAY_TIME ) {
        do_update_display();
    }
}

void osd_set_mastervolume(int volume)
{
    //printf("TOMCXXX: osd_set_mastervolume(%d)\n", volume);
}

int osd_key_pressed(int keycode)
{
    int pressed = 0;
    keep_alive(); // this is important.  several places in code: while(osd_keypressed(key));
    if( keycode >= 0 && keycode <= OSD_KEY_F12 ) {
        pressed = keys[keycode];
    }

    return pressed;
}

int osd_read_key(void)
{
    last_key_pressed = -1;
    while(1) {
        keep_alive(); // important
        if( last_key_pressed >= 0 ) {
            break;
        }
        usleep(100000);
    }
    return last_key_pressed;
}

int osd_read_keyrepeat(void)
{
    //printf("TOMCXXX: osd_read_keyrepeat()\n");
}

const char *osd_joy_name(int joycode)
{
    //printf("TOMCXXX: osd_joy_name(%d)\n", joycode);
}

const char *osd_key_name(int keycode)
{
    //printf("TOMCXXX: osd_key_name(%d)\n", keycode);
}

void osd_poll_joystick(void)
{
    // leaving joystick unimplemented for now
}

int osd_joy_pressed(int joycode)
{
    return 0;
}

void osd_trak_read(int *deltax,int *deltay)
{
    //printf("TOMCXXX: osd_trak_read(%p,%p)\n", deltax, deltay);
}

int osd_analogjoy_read(int axis)
{
    //printf("TOMCXXX: osd_analogjoy_read(%d)\n", axis);
}

void osd_led_w(int led,int on)
{
    //printf( "TOMCXXX: osd_led_w(%d,%d)\n", led, on);
}

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

void osd_mark_vector_dirty(int x, int y)
{
    //printf( "TOMCXXX: osd_mark_vector_dirty(%d,%d)\n", x, y);
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

void parse_cmdline (int argc, char **argv, struct GameOptions *options, int game_index)
{
    static float f_beam, f_flicker;
    static int vesa;
    static char *resolution;
    char tmpres[10];

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

    /* any option that starts with a digit is taken as a resolution option */
    /* this is to handle the old "-wxh" commandline option. */
    for( int i = 1; i < argc; i++)
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

int main( int argc, char **argv )
{
    int res, j, game_index;

    /* these two are not available in mame.cfg */
    ignorecfg = 0;
    errorlog = options.errorlog = 0;

    for( int i = 1; i < argc; i++) /* V.V_121997 */
    {
        if (stricmp(argv[i],"-ignorecfg") == 0) ignorecfg = 1;
        else if (stricmp(argv[i],"-log") == 0) {
            errorlog = options.errorlog = fopen("error.log","wa");
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

#ifdef TOMCXXX
    set_config_file ("mame.cfg");

    /* check for frontend options */
    res = frontend_help (argc, argv);

    /* if frontend options were used, return to DOS with the error code */
    if (res != 1234)
        exit (res);
#endif /* TOMCXXX */

    /* take the first commandline argument without "-" as the game name */
    for (j = 1; j < argc; j++)
        if (argv[j][0] != '-') break;

    /* do we have a drivers for this? */
    int found_game = -1;
    for( int i = 0; drivers[i]; i++) {
        if (stricmp(argv[j],drivers[i]->name) == 0) {
            found_game = i;
            break;
        }
    }

    if( found_game < 0 ) {
        printf("Game \"%s\" not supported\n", argv[j]);
        return 1;
    }
    else {
        game_index = found_game;
    }

    /* parse generic (os-independent) options */
    parse_cmdline (argc, argv, &options, game_index);

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
    res = run_game (game_index , &options);

    /* close open files */
    if (options.errorlog) fclose (options.errorlog);
    if (options.playback) osd_fclose (options.playback);
    if (options.record)   osd_fclose (options.record);

    exit (res);
}

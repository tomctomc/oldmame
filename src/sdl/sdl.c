/***************************************************************************

  osdepend.c

  OS dependant stuff (display handling, keyboard scan...)
  This is the only file which should me modified in order to port the
  emulator to a different system.

***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "osdepend.h"

#include "SDL.h"

#define MAX_PENS 256
#define NKEYS    OSD_KEY_F12+1

static int displayScale;

int play_sound;

// joystick state (not implemented)
int osd_joy_up, osd_joy_down, osd_joy_left, osd_joy_right;
int osd_joy_b1, osd_joy_b2,   osd_joy_b3,   osd_joy_b4;

// audio
#define NUM_AUDIO_CHANNELS 4
#define SAMPLE_RATE 44100
#define SAMPLES_PER_CALLBACK 128
typedef struct {
    int                channel;
    SDL_AudioSpec      desired;
    SDL_AudioSpec      obtained;
    SDL_AudioDeviceID  audioDeviceID;
    char              *data;
    int                len;
    int                freq;
    int                volume;
    float              time;
} AudioChannel;
static AudioChannel audioChannels[NUM_AUDIO_CHANNELS];

void audioCallback(void *userdata, Uint8 *stream, int len)
{
    AudioChannel *audioChannel = (AudioChannel *) userdata;
    char *s = (char *) stream; // convert to signed!
    for( int i=0; i<len; i++) {
        s[i] = audioChannel->data[((int)audioChannel->time) % audioChannel->len];
        s[i] *= (float) audioChannel->volume / (float) 512;
        audioChannel->time += (double) audioChannel->freq / (double) SAMPLE_RATE;
    }
}

static SDL_Window        *displayWindow;
static SDL_Renderer      *displayRenderer;
static SDL_Texture       *displayTexture;

static struct osd_bitmap *displayBitmap;

static int first_free_pen;

static unsigned char *pixelsMappedTo332;
static unsigned char paletteMapping[MAX_PENS];

static int keys[NKEYS];

static int map_sdl_scancode_to_osd_key[SDL_NUM_SCANCODES+1];
static int last_key_pressed;

static int frame_count;

void fatal(char *s)
{
    fprintf(stderr,"fatal: %s\n",s);
    exit(1);
}

#define SDLCHK(C) { long long result = (long long) (C); if( !result ) { fprintf(stderr,"fatal: SDLCHK ##C returned %ld.  SDL_Error: %s\n", (long) result, SDL_GetError()); exit(1); } }
#define ASSERT(C) { if( !(C) ) { fatal("assertion failed"); } }

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
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_5           ] = OSD_KEY_5;
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
    map_sdl_scancode_to_osd_key[SDL_SCANCODE_LCTRL       ] = OSD_KEY_CONTROL;
}

int osd_init(int argc,char **argv)
{
    SDLCHK(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));

    displayWindow     = (SDL_Window *) 0;
    displayRenderer   = (SDL_Renderer *) 0;
    displayBitmap     = (struct osd_bitmap *) 0;
    pixelsMappedTo332 = (unsigned char *) 0;
    first_free_pen    = 0;
    frame_count       = 0;
    play_sound        = 1;
    displayScale      = 3; // default fairly big

    for( int i = 1; i < argc; i++ ) {
        if( strcmp(argv[i],"-scale") == 0 ) {
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

    init_keymap();

    if( play_sound ) {
        // init sound
        for( int channel=0; channel < NUM_AUDIO_CHANNELS; channel++ ) {
            AudioChannel *audioChannel = &audioChannels[channel];
            audioChannel->channel          = channel;
            audioChannel->desired.freq     = SAMPLE_RATE;
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

void osd_exit(void)
{
    if( play_sound ) {
        for( int channel=0; channel<NUM_AUDIO_CHANNELS; channel++ ) {
            osd_stop_sample(channel);
        }
    }
    SDL_Quit();
}

struct osd_bitmap *osd_create_display(int width,int height)
{
    SDLCHK(displayWindow   = SDL_CreateWindow( "origmame", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width*displayScale, height*displayScale, SDL_WINDOW_RESIZABLE ));
    SDLCHK(displayRenderer = SDL_CreateRenderer( displayWindow, -1, 0 ));
    SDLCHK(displayTexture  = SDL_CreateTexture(displayRenderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, width, height));

    displayBitmap = osd_create_bitmap(width,height);
    pixelsMappedTo332 = malloc(width * height);

    return displayBitmap;
}

void osd_close_display(void)
{
    if( pixelsMappedTo332 ) {
        free(pixelsMappedTo332);
        pixelsMappedTo332 = (unsigned char *) 0;
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
                for( int i=0; i<NUM_AUDIO_CHANNELS; i++ ) {
                    osd_stop_sample(i);
                }
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

void osd_update_display(void)
{
    frame_count++;

    // don't know how to do palette stuff in SDL for textures.
    // so we just do our own crude mapping here (per frame).
    int npixels = displayBitmap->width * displayBitmap->height;
    unsigned char *pixels = (unsigned char *) displayBitmap->private;
    for( int i=0; i<npixels; i++ ) {
        pixelsMappedTo332[i] = paletteMapping[pixels[i]];
    }

    SDL_UpdateTexture(displayTexture, NULL, pixelsMappedTo332, displayBitmap->width);
    SDL_RenderCopy(displayRenderer, displayTexture, NULL, NULL);
    SDL_RenderPresent(displayRenderer);

    poll_events();
}

int osd_obtain_pen(unsigned char red, unsigned char green, unsigned char blue)
{
    int pen = first_free_pen;
	// we use SDL's built-in 332 palette
    paletteMapping[pen] = (red & 0xe0) | ((green >> 3) & 0x1c) | (blue & 0x3);
    first_free_pen++;
    if(first_free_pen > MAX_PENS) {
        first_free_pen = 0;
    }

    return pen;
}

struct osd_bitmap *osd_create_bitmap(int width,int height)
{
    struct osd_bitmap *bitmap;

    bitmap = debug_malloc(              sizeof(struct osd_bitmap) +
                           (height  ) * sizeof(void *) ); // pointers to each line

    if( bitmap ) {
        unsigned char *bm; // actual bitap pixel array

        bitmap->width  = width;
        bitmap->height = height;
        bm = debug_malloc(width * height * sizeof(unsigned char));
        if( !bm ) {
            debug_free(bitmap);
            return (struct osd_bitmap *) 0;
        }

        // set each line pointer in structure
        for( int y = 0; y < height; y++ ) {
            bitmap->line[y] = &bm[y * width];
        }

        bitmap->private = bm; // tuck away array pointer
    }

    return bitmap;
}

void osd_free_bitmap(struct osd_bitmap *bitmap)
{
    if (bitmap) {
        debug_free(bitmap->private);
        debug_free(bitmap);
    }
}

void osd_update_audio(void)
{
	// not sure what needs to happen here
}

void osd_play_sample(int channel,unsigned char *data,int len,int freq,int volume,int loop)
{
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; }
    AudioChannel *audioChannel = &audioChannels[channel];
    if( audioChannel->data ) {
        debug_free(audioChannel->data);
    }
    audioChannel->data   = debug_malloc(len);
    audioChannel->len    = len;
    audioChannel->time   = 0;
    memcpy(audioChannel->data,data,len);
    osd_adjust_sample(channel,freq,volume);
 }

void osd_play_streamed_sample(int channel,unsigned char *data,int len,int freq,int volume)
{
    printf("TOMCXXX: osd_play_streamed_sample(%d, %p, %d, %d, %d) not implemented!\n", channel, data, len, freq, volume);
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; }
}

void osd_adjust_sample(int channel,int freq,int volume)
{
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; };

    AudioChannel *audioChannel = &audioChannels[channel];
    audioChannel->freq   = freq;
    audioChannel->volume = volume;
    SDL_PauseAudioDevice(audioChannel->audioDeviceID, 0);
}

void osd_stop_sample(int channel)
{
    if( play_sound == 0 || channel >= NUM_AUDIO_CHANNELS ) { return; };

    AudioChannel *audioChannel = &audioChannels[channel];
    audioChannel->volume = 0;
    SDL_PauseAudioDevice(audioChannel->audioDeviceID, 1);
}

int osd_key_pressed(int keycode)
{
    int pressed = 0;

    poll_events(); // this is important.  several places in code: while(osd_keypressed(key));

    if( keycode >= 0 && keycode <= OSD_KEY_F12 ) {
        pressed = keys[keycode];
    }

    return pressed;
}

int osd_read_key(void)
{
    last_key_pressed = -1;
    while(1) {
        poll_events(); // important
        if( last_key_pressed >= 0 ) {
            break;
        }
        usleep(100000);
    }
    return last_key_pressed;
}

void osd_poll_joystick(void)
{
	// leaving joystick unimplemented for now
}

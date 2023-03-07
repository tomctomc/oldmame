#ifndef _OSD_SDL_H
#define _OSD_SDL_H 1

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "stricmp.h"
#include "memory.h"
#include "driver.h"
#include "osdepend.h"

#include "SDL.h"

#ifdef OSD_SDL_MAIN
#define EXTERN
#else /* OSD_SDL_MAIN */
#define EXTERN extern
#endif /* OSD_SDL_MAIN */

#define SDLCHK(N,C) { long long result = (long long) (C); if( !result ) { fprintf(stderr,"fatal: SDLCHK ##C (N=%d) returned %ld.  SDL_Error: %s\n", N, (long) result, SDL_GetError()); exit(1); } }
#define ASSERT(C) { if( !(C) ) { fatal("assertion failed"); } }

#define BACKGROUND 0

#define FRAMESKIP_LEVELS 12

#define NUM_BUFFERS 3	/* raising this number should improve performance with frameskip, */
						/* but also increases the latency. */

extern void keep_alive(void);
extern void poll_events(void);
extern uint64_t now(void);
extern void do_update_display(struct osd_bitmap *displayBitmap);
extern void stop_all_sound(void);
extern void init_keymap(void);
extern void init_sound(void);

#define NS_PER_SECOND 1000000000
#define KEEP_ALIVE_DISPLAY_TIME ((uint64_t) (0.5*NS_PER_SECOND)) // refresh every half a second
EXTERN uint64_t last_display_update_time;
EXTERN int last_key_pressed;
EXTERN int joy_type, use_mouse;
EXTERN int stretch;
EXTERN FILE *errorlog;
EXTERN char *nvdir, *hidir, *cfgdir, *inpdir, *stadir, *memcarddir;
EXTERN char *artworkdir, *screenshotdir;
EXTERN char *alternate_name;
EXTERN char *cheatdir;
extern char *cheatfile, *history_filename,*mameinfo_filename; // defined elsewhere in mame

EXTERN int vector_game;
EXTERN int use_dirty;

EXTERN int game_width;
EXTERN int game_height;
EXTERN int game_attributes;

EXTERN int displayScale;

EXTERN SDL_Window        *displayWindow;
EXTERN SDL_Renderer      *displayRenderer;
EXTERN SDL_Texture       *displayTexture;

EXTERN int first_free_pen;

EXTERN uint32_t *pixelsMappedToARGB8888;
EXTERN uint32_t *paletteMapping;

EXTERN int use_double;

EXTERN int color_depth;
EXTERN unsigned char *current_palette;
EXTERN unsigned int *dirtycolor;
EXTERN int dirtypalette;
EXTERN UINT32 *palette_16bit_lookup;

EXTERN int frame_count;
EXTERN int throttle;

EXTERN int frameskip,autoframeskip;
EXTERN int frameskip_counter;

EXTERN int attenuation;
EXTERN int play_sound;
EXTERN char *rompath, *samplepath;

EXTERN int mame_argc;
EXTERN char **mame_argv;
EXTERN int game;
EXTERN int  ignorecfg;
EXTERN int  scanlines;
EXTERN int video_sync;

EXTERN float osd_gamma_correction;
EXTERN int gfx_width, gfx_height;

// audio
typedef struct {
    SDL_AudioSpec      desired;
    SDL_AudioSpec      obtained;
    SDL_AudioDeviceID  audioDeviceID;
    SDL_AudioStream   *audioStream;
} AudioChannel;
EXTERN AudioChannel audioChannel[1]; // just one "channel" now

extern void audioCallback(void *userdata, unsigned char *stream, int len);

extern void *debug_malloc(ssize_t size);
extern void debug_free(void *p);

EXTERN int video_depth, video_fps, video_attributes, video_orientation;
EXTERN int brightness;
EXTERN float brightness_paused_adjust;
EXTERN int dirty_bright;
EXTERN int modifiable_palette;

EXTERN double samples_per_frame;
EXTERN double samples_left_over;
EXTERN UINT32 samples_this_frame;

EXTERN int stream_playing;
EXTERN INT16 *stream_cache_data;
EXTERN int stream_cache_len;
EXTERN int stream_cache_stereo;
EXTERN int audio_buffer_length;
EXTERN int voice_pos;

EXTERN int visible_min_x;
EXTERN int visible_min_y;
EXTERN int visible_max_x;
EXTERN int visible_max_y;
EXTERN int screen_colors;

#endif /* _OSD_SDL_H */



#include "osd_cpu.h"
#include "osd_sdl.h"
#include "input.h"


static struct KeyboardInfo keymap_by_scancode[SDL_NUM_SCANCODES+1]; // map sdl scancodes to mame keycodes
static struct KeyboardInfo keymap_by_keycode[ __code_max       +1]; // map mame keycodes to sdl scancodes
static struct KeyboardInfo keylist[           __code_max       +1]; // list for mame (order predetermined)
static int keysdown[__code_max+1];               // which keys are currently pressed (by keycode)

void set_keymap( int keylistindex, int scancode, const char *name, int keycode )
{
    struct KeyboardInfo  info;

    info.code         = scancode;
    info.standardcode = keycode;
    info.name         = name;


    keymap_by_keycode[ keycode     ] =  info;
    keymap_by_scancode[scancode    ] =  info;

    // mame keylist order (and code values) *must* stay fixed
    info.code = keylistindex;
    keylist[keylistindex] = info;
}

void init_keymap()
{
    // we just use a giant array to map SDL_SCANCODE_XXX to KEY_XXX

    int index = 0;

    // order here is critical
    set_keymap( index++, SDL_SCANCODE_A,              "A",         KEYCODE_A          );
    set_keymap( index++, SDL_SCANCODE_B,              "B",         KEYCODE_B          );
    set_keymap( index++, SDL_SCANCODE_C,              "C",         KEYCODE_C          );
    set_keymap( index++, SDL_SCANCODE_D,              "D",         KEYCODE_D          );
    set_keymap( index++, SDL_SCANCODE_E,              "E",         KEYCODE_E          );
    set_keymap( index++, SDL_SCANCODE_F,              "F",         KEYCODE_F          );
    set_keymap( index++, SDL_SCANCODE_G,              "G",         KEYCODE_G          );
    set_keymap( index++, SDL_SCANCODE_H,              "H",         KEYCODE_H          );
    set_keymap( index++, SDL_SCANCODE_I,              "I",         KEYCODE_I          );
    set_keymap( index++, SDL_SCANCODE_J,              "J",         KEYCODE_J          );
    set_keymap( index++, SDL_SCANCODE_K,              "K",         KEYCODE_K          );
    set_keymap( index++, SDL_SCANCODE_L,              "L",         KEYCODE_L          );
    set_keymap( index++, SDL_SCANCODE_M,              "M",         KEYCODE_M          );
    set_keymap( index++, SDL_SCANCODE_N,              "N",         KEYCODE_N          );
    set_keymap( index++, SDL_SCANCODE_O,              "O",         KEYCODE_O          );
    set_keymap( index++, SDL_SCANCODE_P,              "P",         KEYCODE_P          );
    set_keymap( index++, SDL_SCANCODE_Q,              "Q",         KEYCODE_Q          );
    set_keymap( index++, SDL_SCANCODE_R,              "R",         KEYCODE_R          );
    set_keymap( index++, SDL_SCANCODE_S,              "S",         KEYCODE_S          );
    set_keymap( index++, SDL_SCANCODE_T,              "T",         KEYCODE_T          );
    set_keymap( index++, SDL_SCANCODE_U,              "U",         KEYCODE_U          );
    set_keymap( index++, SDL_SCANCODE_V,              "V",         KEYCODE_V          );
    set_keymap( index++, SDL_SCANCODE_W,              "W",         KEYCODE_W          );
    set_keymap( index++, SDL_SCANCODE_X,              "X",         KEYCODE_X          );
    set_keymap( index++, SDL_SCANCODE_Y,              "Y",         KEYCODE_Y          );
    set_keymap( index++, SDL_SCANCODE_Z,              "Z",         KEYCODE_Z          );
    set_keymap( index++, SDL_SCANCODE_0,              "0",         KEYCODE_0          );
    set_keymap( index++, SDL_SCANCODE_1,              "1",         KEYCODE_1          );
    set_keymap( index++, SDL_SCANCODE_2,              "2",         KEYCODE_2          );
    set_keymap( index++, SDL_SCANCODE_3,              "3",         KEYCODE_3          );
    set_keymap( index++, SDL_SCANCODE_4,              "4",         KEYCODE_4          );
    set_keymap( index++, SDL_SCANCODE_5,              "5",         KEYCODE_5          );
    set_keymap( index++, SDL_SCANCODE_6,              "6",         KEYCODE_6          );
    set_keymap( index++, SDL_SCANCODE_7,              "7",         KEYCODE_7          );
    set_keymap( index++, SDL_SCANCODE_8,              "8",         KEYCODE_8          );
    set_keymap( index++, SDL_SCANCODE_9,              "9",         KEYCODE_9          );
    set_keymap( index++, SDL_SCANCODE_KP_0,           "0 PAD",     KEYCODE_0_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_1,           "1 PAD",     KEYCODE_1_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_2,           "2 PAD",     KEYCODE_2_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_3,           "3 PAD",     KEYCODE_3_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_4,           "4 PAD",     KEYCODE_4_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_5,           "5 PAD",     KEYCODE_5_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_6,           "6 PAD",     KEYCODE_6_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_7,           "7 PAD",     KEYCODE_7_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_8,           "8 PAD",     KEYCODE_8_PAD      );
    set_keymap( index++, SDL_SCANCODE_KP_9,           "9 PAD",     KEYCODE_9_PAD      );
    set_keymap( index++, SDL_SCANCODE_F1,             "F1",        KEYCODE_F1         );
    set_keymap( index++, SDL_SCANCODE_F2,             "F2",        KEYCODE_F2         );
    set_keymap( index++, SDL_SCANCODE_F3,             "F3",        KEYCODE_F3         );
    set_keymap( index++, SDL_SCANCODE_F4,             "F4",        KEYCODE_F4         );
    set_keymap( index++, SDL_SCANCODE_F5,             "F5",        KEYCODE_F5         );
    set_keymap( index++, SDL_SCANCODE_F6,             "F6",        KEYCODE_F6         );
    set_keymap( index++, SDL_SCANCODE_F7,             "F7",        KEYCODE_F7         );
    set_keymap( index++, SDL_SCANCODE_F8,             "F8",        KEYCODE_F8         );
    set_keymap( index++, SDL_SCANCODE_F9,             "F9",        KEYCODE_F9         );
    set_keymap( index++, SDL_SCANCODE_F10,            "F10",       KEYCODE_F10        );
    set_keymap( index++, SDL_SCANCODE_F11,            "F11",       KEYCODE_F11        );
    set_keymap( index++, SDL_SCANCODE_F12,            "F12",       KEYCODE_F12        );
    set_keymap( index++, SDL_SCANCODE_ESCAPE,         "ESC",       KEYCODE_ESC        );
    set_keymap( index++, SDL_SCANCODE_GRAVE,          "~",         KEYCODE_TILDE      );
    set_keymap( index++, SDL_SCANCODE_MINUS,          "-",         KEYCODE_MINUS      );
    set_keymap( index++, SDL_SCANCODE_EQUALS,         "=",         KEYCODE_EQUALS     );
    set_keymap( index++, SDL_SCANCODE_BACKSPACE,      "BKSPACE",   KEYCODE_BACKSPACE  );
    set_keymap( index++, SDL_SCANCODE_TAB,            "TAB",       KEYCODE_TAB        );
    set_keymap( index++, SDL_SCANCODE_LEFTBRACKET,    "[",         KEYCODE_OPENBRACE  );
    set_keymap( index++, SDL_SCANCODE_RIGHTBRACKET,   "]",         KEYCODE_CLOSEBRACE );
    set_keymap( index++, SDL_SCANCODE_RETURN,         "ENTER",     KEYCODE_ENTER      );
    set_keymap( index++, SDL_SCANCODE_SEMICOLON,      ";",         KEYCODE_COLON      );
    set_keymap( index++, SDL_SCANCODE_APOSTROPHE,     "'",         KEYCODE_QUOTE      );
    set_keymap( index++, SDL_SCANCODE_BACKSLASH,      "\\",        KEYCODE_BACKSLASH  );
    set_keymap( index++, SDL_SCANCODE_NONUSBACKSLASH, "",          KEYCODE_BACKSLASH2 );
    set_keymap( index++, SDL_SCANCODE_COMMA,          ",",         KEYCODE_COMMA      );
    set_keymap( index++, SDL_SCANCODE_PERIOD,         ".",         KEYCODE_STOP       );
    set_keymap( index++, SDL_SCANCODE_SLASH,          "/",         KEYCODE_SLASH      );
    set_keymap( index++, SDL_SCANCODE_SPACE,          "SPACE",     KEYCODE_SPACE      );
    set_keymap( index++, SDL_SCANCODE_INSERT,         "INS",       KEYCODE_INSERT     );
    set_keymap( index++, SDL_SCANCODE_DELETE,         "DEL",       KEYCODE_DEL        );
    set_keymap( index++, SDL_SCANCODE_HOME,           "HOME",      KEYCODE_HOME       );
    set_keymap( index++, SDL_SCANCODE_END,            "END",       KEYCODE_END        );
    set_keymap( index++, SDL_SCANCODE_PAGEUP,         "PGUP",      KEYCODE_PGUP       );
    set_keymap( index++, SDL_SCANCODE_PAGEDOWN,       "PGDN",      KEYCODE_PGDN       );
    set_keymap( index++, SDL_SCANCODE_LEFT,           "LEFT",      KEYCODE_LEFT       );
    set_keymap( index++, SDL_SCANCODE_RIGHT,          "RIGHT",     KEYCODE_RIGHT      );
    set_keymap( index++, SDL_SCANCODE_UP,             "UP",        KEYCODE_UP         );
    set_keymap( index++, SDL_SCANCODE_DOWN,           "DOWN",      KEYCODE_DOWN       );
    set_keymap( index++, SDL_SCANCODE_KP_DIVIDE,      "/ PAD",     KEYCODE_SLASH_PAD  );
    set_keymap( index++, SDL_SCANCODE_KP_MULTIPLY,    "* PAD",     KEYCODE_ASTERISK   );
    set_keymap( index++, SDL_SCANCODE_KP_MINUS,       "- PAD",     KEYCODE_MINUS_PAD  );
    set_keymap( index++, SDL_SCANCODE_KP_PLUS,        "+ PAD",     KEYCODE_PLUS_PAD   );
    set_keymap( index++, SDL_SCANCODE_KP_PERIOD,      ". PAD",     KEYCODE_DEL_PAD    );
    set_keymap( index++, SDL_SCANCODE_KP_ENTER,       "ENTER PAD", KEYCODE_ENTER_PAD  );
    set_keymap( index++, SDL_SCANCODE_PRINTSCREEN,    "PRTSCR",    KEYCODE_PRTSCR     );
    set_keymap( index++, SDL_SCANCODE_PAUSE,          "PAUSE",     KEYCODE_PAUSE      );
    set_keymap( index++, SDL_SCANCODE_LSHIFT,         "LSHIFT",    KEYCODE_LSHIFT     );
    set_keymap( index++, SDL_SCANCODE_RSHIFT,         "RSHIFT",    KEYCODE_RSHIFT     );
    set_keymap( index++, SDL_SCANCODE_LCTRL,          "LCTRL",     KEYCODE_LCONTROL   );
    set_keymap( index++, SDL_SCANCODE_RCTRL,          "RCTRL",     KEYCODE_RCONTROL   );
    set_keymap( index++, SDL_SCANCODE_LALT,           "ALT",       KEYCODE_LALT       );
    set_keymap( index++, SDL_SCANCODE_RALT,           "ALTGR",     KEYCODE_RALT       );
    /*
    set_keymap( index++, SDL_SCANCODE_LGUI,           "LWIN",      KEYCODE_OTHER      );
    set_keymap( index++, SDL_SCANCODE_RGUI,           "RWIN",      KEYCODE_OTHER      );
    set_keymap( index++, SDL_SCANCODE_MENU,           "MENU",      KEYCODE_OTHER      );
    */
    set_keymap( index++, SDL_SCANCODE_SCROLLLOCK,     "SCRLOCK",   KEYCODE_SCRLOCK    );
    set_keymap( index++, SDL_SCANCODE_NUMLOCKCLEAR,   "NUMLOCK",   KEYCODE_NUMLOCK    );
    set_keymap( index++, SDL_SCANCODE_CAPSLOCK,       "CAPSLOCK",  KEYCODE_CAPSLOCK   );
}

void keep_alive(void)
{
    // to be called in tight loops.

    // ensure that SDL events continue to be polled
    poll_events();

    // also, periodically refresh the display
    if( now() - last_display_update_time > KEEP_ALIVE_DISPLAY_TIME ) {
        do_update_display(Machine->scrbitmap);
    }
}

void poll_events()
{
    SDL_Event e;
    while( SDL_PollEvent(&e) ) {
        if( e.type == SDL_KEYDOWN ) {
            int osd_key = keymap_by_scancode[e.key.keysym.scancode].standardcode;

            // hack - turn sound off when paused or dipswitch
            if( osd_key == KEYCODE_P || osd_key == KEYCODE_TAB ) {
                stop_all_sound();
            }

            keysdown[osd_key] = 1;
            last_key_pressed = osd_key;
        }
        else if( e.type == SDL_KEYUP ) {
            int osd_key = keymap_by_scancode[e.key.keysym.scancode].standardcode;
            keysdown[osd_key] = 0;
        }
        if( e.type == SDL_QUIT ) {
            keysdown[KEYCODE_ESC] = 1;
        }
    }
}

const struct KeyboardInfo *osd_get_key_list(void)
{
    return keylist;
}

/*
 * Check if a key is pressed. The keycode is the standard PC keyboard
 * code, as defined in osdepend.h. Return 0 if the key is not pressed,
 * nonzero otherwise. Handle pseudo keycodes.
 */
int osd_is_key_pressed(int keycode)
{
    keep_alive(); // this is important.  several places in code: while(osd_keypressed(key));

    if (keycode >= __code_max) return 0;

    if (keycode == KEYCODE_PAUSE)
    {
        static int pressed,counter;
        int res;

        res = keysdown[KEYCODE_PAUSE] ^ pressed;
        if (res)
        {
            if (counter > 0)
            {
                if (--counter == 0)
                    pressed = keysdown[KEYCODE_PAUSE];
            }
            else counter = 10;
        }

        return res;
    }

    return keysdown[keycode];
}

/*
 * Wait for a key press and return the keycode.
 * Translate certain keys (or key combinations) back to
 * the pseudo key values defined in osdepend.h.
 */
int osd_wait_keypress(void)
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

int osd_readkey_unicode(int flush)
{
    struct KeyboardInfo *key = &keymap_by_keycode[osd_wait_keypress()];
    int unicode = 0;
    if( key->name ) {
        unicode = key->name[0]; // TOMCXXXX this is inadequate
    }
    return unicode;
}

#define MAX_JOY 256
#define MAX_JOY_NAME_LEN 40

static struct JoystickInfo joylist[MAX_JOY] =
{
    /* will be filled later */
    { 0, 0, 0 }    /* end of table */
};

const struct JoystickInfo *osd_get_joy_list(void)
{
    return joylist;
}

int osd_is_joy_pressed(int joycode)
{
    return 0;
}

void osd_poll_joysticks(void)
{
}

/* return a value in the range -128 .. 128 (yes, 128, not 127) */
void osd_analogjoy_read(int player, int *analog_x, int *analog_y)
{
    *analog_x = 0;
    *analog_y = 0;
}

int osd_joystick_needs_calibration (void)
{
    return 0;
}

void osd_joystick_start_calibration (void)
{
}

char *osd_joystick_calibrate_next (void)
{
    return 0;
}

void osd_joystick_calibrate (void)
{
}

void osd_joystick_end_calibration (void)
{
}

void osd_trak_read(int player, int *deltax,int *deltay)
{
    *deltax = *deltay = 0;
}

void osd_customize_inputport_defaults(struct ipd *defaults)
{
}

void osd_led_w(int led,int on)
{
}


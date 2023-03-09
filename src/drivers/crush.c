/***************************************************************************

Crush Roller memory map (preliminary)

This is basically the same hardware as Pac Man, but dip switches behave
differently.

0000-3fff ROM
4000-43ff Video RAM
4400-47ff Color RAM
4c00-4fff RAM

memory mapped ports:

read:
5000      IN0
5040      IN1
5080      DSW1


*
 * IN0 (all bits are inverted)
 * bit 7 : CREDIT
 * bit 6 : COIN 2
 * bit 5 : COIN 1
 * bit 4 : COCKTAIL or UPRIGHT cabinet (0 = UPRIGHT)
 * bit 3 : DOWN player 1
 * bit 2 : RIGHT player 1
 * bit 1 : LEFT player 1
 * bit 0 : UP player 1
 *
*
 * IN1 (all bits are inverted)
 * bit 7 : ??
 * bit 6 : START 2
 * bit 5 : START 1
 * bit 4 : ??
 * bit 3 : DOWN player 2 (TABLE only)
 * bit 2 : RIGHT player 2 (TABLE only)
 * bit 1 : LEFT player 2 (TABLE only)
 * bit 0 : UP player 2 (TABLE ony)
 *
*
 * DSW1 (all bits are inverted)
 * bit 7 :  ??
 * bit 6 :  difficulty level
 *                       1 = Normal  0 = Harder
 * bit 5 : Teleport holes 0 = on 1 = off
 * bit 4 : First pattern difficulty 1 = easy 0 = hard
 * bit 3 :\ nr of lives
 * bit 2 :/ 00 = 3  01 = 4  10 = 5  11 = 6
 * bit 1 :\ play mode
 * bit 0 :/ 00 = free play   01 = 1 coin 1 credit
 *          10 = 1 coin 2 credits   11 = 2 coins 1 credit
 *

write:
4ff2-4ffd 6 pairs of two bytes:
          the first byte contains the sprite image number (bits 2-7), Y flip (bit 0),
		  X flip (bit 1); the second byte the color
5000      interrupt enable
5001      sound enable
5002      ????
5003      flip screen
5004      1 player start lamp
5005      2 players start lamp
5006      related to the credits. don't know what it was used for.
5007      coin counter (pulsing it would increase the counter on the machine)
5040-5044 sound voice 1 accumulator (nibbles) (used by the sound hardware only)
5045      sound voice 1 waveform (nibble)
5046-5049 sound voice 2 accumulator (nibbles) (used by the sound hardware only)
504a      sound voice 2 waveform (nibble)
504b-504e sound voice 3 accumulator (nibbles) (used by the sound hardware only)
504f      sound voice 3 waveform (nibble)
5050-5054 sound voice 1 frequency (nibbles)
5055      sound voice 1 volume (nibble)
5056-5059 sound voice 2 frequency (nibbles)
505a      sound voice 2 volume (nibble)
505b-505e sound voice 3 frequency (nibbles)
505f      sound voice 3 volume (nibble)
5062-506d Sprite coordinates, x/y pairs for 6 sprites
50c0      Watchdog reset

***************************************************************************/

#include "driver.h"


extern unsigned char *pengo_videoram;
extern unsigned char *pengo_colorram;
extern unsigned char *pengo_spritecode;
extern unsigned char *pengo_spritepos;
extern unsigned char *pengo_soundregs;
extern void pengo_videoram_w(int offset,int data);
extern void pengo_colorram_w(int offset,int data);
extern int pengo_vh_start(void);
extern void pengo_vh_stop(void);
extern void pengo_vh_screenrefresh(struct osd_bitmap *bitmap);

extern void pengo_sound_enable_w(int offset,int data);
extern void pengo_sound_w(int offset,int data);
extern void pengo_sh_update(void);



static struct MemoryReadAddress readmem[] =
{
	{ 0x4c00, 0x4fff, MRA_RAM },	/* includeing sprite codes at 4ff0-4fff */
	{ 0x4000, 0x47ff, MRA_RAM },	/* video and color RAM */
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x5000, 0x503f, input_port_0_r },	/* IN0 */
	{ 0x5040, 0x507f, input_port_1_r },	/* IN1 */
	{ 0x5080, 0x50bf, input_port_2_r },	/* DSW1 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x4c00, 0x4fef, MWA_RAM },
	{ 0x4000, 0x43ff, pengo_videoram_w, &pengo_videoram },
	{ 0x4400, 0x47ff, pengo_colorram_w, &pengo_colorram },
	{ 0x5040, 0x505f, pengo_sound_w, &pengo_soundregs },
	{ 0x4ff0, 0x4fff, MWA_RAM, &pengo_spritecode},
	{ 0x5060, 0x506f, MWA_RAM, &pengo_spritepos },
	{ 0x5000, 0x5000, interrupt_enable_w },
	{ 0x50c0, 0x50c0, MWA_NOP },
	{ 0x5001, 0x5001, pengo_sound_enable_w },
	{ 0x5002, 0x5007, MWA_NOP },
	{ 0x0000, 0x3fff, MWA_ROM },
	{ -1 }	/* end of table */
};



static struct InputPort input_ports[] =
{
	{	/* IN0 */
		0xef,	/* standup cabinet */
		{ OSD_KEY_UP, OSD_KEY_LEFT, OSD_KEY_RIGHT, OSD_KEY_DOWN,
				0, 0, 0, OSD_KEY_3 },
		{ OSD_JOY_UP, OSD_JOY_LEFT, OSD_JOY_RIGHT, OSD_JOY_DOWN,
				0, 0, 0, 0 }
	},
	{	/* IN1 */
		0xff,
		{ 0, 0, 0, 0, 0, OSD_KEY_1, OSD_KEY_2, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{	/* DSW1 */
		0xf1,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{ -1 }	/* end of table */
};



static struct DSW dsw[] =
{
	{ 2, 0x0c, "LIVES", { "3", "4", "5", "6" }, },
	{ 2, 0x20, "TELEPORT HOLES", { "ON", "OFF" }, 1 },
	{ 2, 0x10, "FIRST PATTERN", { "HARD", "EASY" }, 1 },
	{ -1 }
};



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 }, /* characters are rotated 90 degrees */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },	/* bits are packed in groups of four */
	16*8	/* every char takes 16 bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 39 * 8, 38 * 8, 37 * 8, 36 * 8, 35 * 8, 34 * 8, 33 * 8, 32 * 8,
			7 * 8, 6 * 8, 5 * 8, 4 * 8, 3 * 8, 2 * 8, 1 * 8, 0 * 8 },
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3 },
	64*8	/* every sprite takes 64 bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,   0, 32 },
	{ 1, 0x1000, &spritelayout, 0, 32 },
	{ -1 } /* end of array */
};



static unsigned char palette[] =
{
	0x00,0x00,0x00,	/* BLACK */
	0xdb,0x00,0x00,	/* RED */
	0xdb,0x92,0x49,	/* BROWN */
	0xff,0xb6,0xdb,	/* PINK */
	0x00,0x00,0x00,	/* UNUSED */
	0x00,0xdb,0xdb,	/* CYAN */
	0x49,0xb6,0xdb,	/* DKCYAN */
	0xff,0xb6,0x49,	/* DKORANGE */
	0x88,0x88,0x88,	/* UNUSED */
	0xdb,0xdb,0x00,	/* YELLOW */
	0xff,0x00,0xdb,	/* UNUSED */
	0x24,0x24,0xdb,	/* BLUE */
	0x00,0xdb,0x00,	/* GREEN */
	0x49,0xb6,0x92,	/* DKGREEN */
	0xff,0xb6,0x92,	/* LTORANGE */
	0xdb,0xdb,0xdb	/* GREY */
};

enum {BLACK,RED,BROWN,PINK,UNUSED1,CYAN,DKCYAN,DKORANGE,
		UNUSED2,YELLOW,UNUSED3,BLUE,GREEN,DKGREEN,LTORANGE,GREY};

#define WALLS 11
#define GROUND 15
#define CAR CYAN

static unsigned char colortable[] =
{
	BLACK,2,WALLS,4,	/* squasher */
	BLACK,GREY,BLUE,RED,	/* brush */
	BLACK,GROUND,WALLS,PINK,	/* 2nd level paint */
	BLACK,GROUND,WALLS,GROUND,	/* background, text, maze */
	BLACK,GROUND,WALLS,DKORANGE,	/* 3rd level paint */
	BLACK,1,BLUE,CYAN,	/* monster 2 */
	BLACK,GROUND,WALLS,GREEN,	/* 1st level paint */
	BLACK,1,BLUE,YELLOW,	/* monster 1 / cat */
	5,5,5,5,
	BLACK,WALLS,RED,YELLOW,	/* squashed cat, closed barn (sprite), mouse */
	10,10,10,10,
	BLACK,BROWN,CAR,RED,	/* driver's head */
	BLACK,13,CAR,RED,	/* car door */
	0,CAR,WALLS,YELLOW,	/* car */
	14,14,14,14,
	BLACK,RED,GREEN,GREY,	/* bird */
	7,BLACK,9,10,	/* squashed monster */
	2,2,2,2,
	1,1,1,1,
	3,3,3,3,
	BLACK,RED,YELLOW,13,	/* bird */
	0,6,7,8,	/* evil cat (I think) */
	6,6,6,6,
	7,7,7,7,
	15,15,15,15,
	0,GROUND,4,WALLS,	/* barn */
	10,GROUND,4,WALLS,	/* closed barn */
	BLACK,WALLS,CYAN,YELLOW,	/* big cat */
	BLACK,WALLS,GREEN,GREY,	/* tree */
	BLACK,WALLS,DKORANGE,YELLOW,	/* cat, bird's head */
	9,BROWN,WALLS,12,	/* under cat gone */
	9,BROWN,WALLS,12,	/* under cat */
};



/* waveforms for the audio hardware */
static unsigned char samples[8*32] =
{
	0xff,0x11,0x22,0x33,0x44,0x55,0x55,0x66,0x66,0x66,0x55,0x55,0x44,0x33,0x22,0x11,
	0xff,0xdd,0xcc,0xbb,0xaa,0x99,0x99,0x88,0x88,0x88,0x99,0x99,0xaa,0xbb,0xcc,0xdd,

	0xff,0x44,0x66,0x66,0x55,0x33,0x11,0x22,0x33,0x33,0x22,0x11,0xee,0xcc,0xbb,0xdd,
	0xff,0x11,0x33,0x22,0x00,0xdd,0xcc,0xbb,0xbb,0xcc,0xdd,0xbb,0x99,0x88,0x88,0xaa,

	0xff,0x22,0x44,0x55,0x66,0x55,0x44,0x22,0xff,0xcc,0xaa,0x99,0x88,0x99,0xaa,0xcc,
	0xff,0x33,0x55,0x66,0x55,0x33,0xff,0xbb,0x99,0x88,0x99,0xbb,0xff,0x66,0xff,0x88,

	0xff,0x55,0x33,0x00,0x33,0x55,0x11,0xee,0x33,0x66,0x44,0xff,0x11,0x22,0xee,0xaa,
	0xff,0x44,0x00,0xcc,0xdd,0xff,0xaa,0x88,0xbb,0x00,0xdd,0x99,0xbb,0xee,0xbb,0x99,

	0x88,0x00,0x77,0xff,0x99,0x00,0x66,0xff,0xaa,0x00,0x55,0xff,0xbb,0x00,0x44,0xff,
	0xcc,0x00,0x33,0xff,0xdd,0x00,0x22,0xff,0xee,0x00,0x11,0xff,0xff,0x00,0x00,0xff,

	0xff,0x00,0xee,0x11,0xdd,0x22,0xcc,0x33,0xbb,0x44,0xaa,0x55,0x99,0x66,0x88,0x77,
	0x88,0x77,0x99,0x66,0xaa,0x55,0xbb,0x44,0xcc,0x33,0xdd,0x22,0xee,0x11,0xff,0x00,

	0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
	0x77,0x66,0x55,0x44,0x33,0x22,0x11,0x00,0xff,0xee,0xdd,0xcc,0xbb,0xaa,0x99,0x88,

	0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
	0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77
};



const struct MachineDriver crush_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz */
			0,
			readmem,writemem,0,0,
			interrupt,1
		}
	},
	60,
	input_ports,dsw,
	0,

	/* video hardware */
	28*8, 36*8, { 0*8, 28*8-1, 0*8, 36*8-1 },
	gfxdecodeinfo,
	sizeof(palette)/3,sizeof(colortable),
	0,0,palette,colortable,
	0,'A',
	0x0f,0x09,
	8*11,8*19,0x01,
	0,
	pengo_vh_start,
	pengo_vh_stop,
	pengo_vh_screenrefresh,

	/* sound hardware */
	samples,
	0,
	0,
	0,
	pengo_sh_update
};

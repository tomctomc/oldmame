/***************************************************************************

Mr Do! memory map (preliminary)

0000-7fff ROM
8000-83ff color RAM 1
8400-87ff video RAM 1
8800-8bff color RAM 2
8c00-8fff video RAM 2
e000-efff RAM

memory mapped ports:

read:
9803      SECRE 1/6-J2-11
a000      IN0
a001      IN1
a002      DSW1
a003      DSW2

*
 * IN0 (all bits are inverted)
 * bit 7 : TILT if this is 0 coins are not accepted
 * bit 6 : START 2
 * bit 5 : START 1
 * bit 4 : FIRE player 1
 * bit 3 : UP player 1
 * bit 2 : RIGHT player 1
 * bit 1 : DOWN player 1
 * bit 0 : LEFT player 1
 *
*
 * IN1 (all bits are inverted)
 * bit 7 : COIN 2
 * bit 6 : COIN 1
 * bit 5 : unused
 * bit 4 : FIRE player 2 (TABLE only)
 * bit 3 : UP player 2 (TABLE only)
 * bit 2 : RIGHT player 2 (TABLE only)
 * bit 1 : DOWN player 2 (TABLE only)
 * bit 0 : LEFT player 2 (TABLE only)
 *
*
 * DSW1 (all bits are inverted)
 * bit 7 : DIP SWITCH 8\ Number of lives
 * bit 6 : DIP SWITCH 7/ 11 = 3  10 = 4  01 = 5  11 = 2
 * bit 5 : DIP SWITCH 6  COCKTAIL or UPRIGHT (0 = UPRIGHT)
 * bit 4 : DIP SWITCH 5  "EXTRA" difficulty  1 = easy  0 = hard
 * bit 3 : DIP SWITCH 4  "SPECIAL" difficulty  1 = easy  0 = hard
 * bit 2 : DIP SWITCH 3  RACK TEST
 * bit 1 : DIP SWITCH 2\ Difficulty level
 * bit 0 : DIP SWITCH 1/ 11 = 1st (easy) 10 = 2nd 01 = 3rd 00 = 4th (hard)
 *
*
 * DSW2 (all bits are inverted)
 * bit 7 : DIP SWITCH 8\
 * bit 6 : DIP SWITCH 7| Left coin slot
 * bit 5 : DIP SWITCH 6|
 * bit 4 : DIP SWITCH 5/
 * bit 3 : DIP SWITCH 4\
 * bit 2 : DIP SWITCH 3| Right coin slot
 * bit 1 : DIP SWITCH 2|
 * bit 0 : DIP SWITCH 1/
 *                       1111 = 1 coin 1 play
 *                       1110 = 1 coin 2 plays
 *                       1101 = 1 coin 3 plays
 *                       1100 = 1 coin 4 plays
 *                       1011 = 1 coin 5 plays
 *                       1010 = 2 coins 1 play
 *                       1001 = 2 coins 3 plays
 *                       1000 = 3 coins 1 play
 *                       0111 = 3 coins 2 plays
 *                       0110 = 4 coins 1 play
 *                       0000 = FREE PLAY
 *                 all others = 1 coin 1 play
 *

write:
9000-90ff sprites, 64 groups of 4 bytes.
9800      flip (bit 0) playfield priority selector? (bits 1-3)
9801      sound port 1
9802      sound port 2
f000      playfield 0 Y scroll position (not used by Mr. Do!)
f800      playfield 0 X scroll position

***************************************************************************/

#include "driver.h"


extern int mrdo_SECRE_r(int offset);

extern unsigned char *mrdo_videoram1;
extern unsigned char *mrdo_colorram1;
extern unsigned char *mrdo_videoram2;
extern unsigned char *mrdo_colorram2;
extern unsigned char *mrdo_spriteram;
extern void mrdo_videoram1_w(int offset,int data);
extern void mrdo_colorram1_w(int offset,int data);
extern void mrdo_videoram2_w(int offset,int data);
extern void mrdo_colorram2_w(int offset,int data);
extern void mrdo_scrollx_w(int offset,int data);
extern void mrdo_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
extern int mrdo_vh_start(void);
extern void mrdo_vh_stop(void);
extern void mrdo_vh_screenrefresh(struct osd_bitmap *bitmap);

extern void ladybug_sound1_w(int offset,int data);
extern void ladybug_sound2_w(int offset,int data);
extern int ladybug_sh_start(void);
extern void ladybug_sh_update(void);



static struct MemoryReadAddress readmem[] =
{
	{ 0xe000, 0xefff, MRA_RAM },
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },	/* video and color RAM */
	{ 0xa000, 0xa000, input_port_0_r },	/* IN0 */
	{ 0xa001, 0xa001, input_port_1_r },	/* IN1 */
	{ 0xa002, 0xa002, input_port_2_r },	/* DSW1 */
	{ 0xa003, 0xa003, input_port_3_r },	/* DSW2 */
	{ 0x9803, 0x9803, mrdo_SECRE_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0x8000, 0x83ff, mrdo_colorram1_w, &mrdo_colorram1 },
	{ 0x8400, 0x87ff, mrdo_videoram1_w, &mrdo_videoram1 },
	{ 0x8800, 0x8bff, mrdo_colorram2_w, &mrdo_colorram2 },
	{ 0x8c00, 0x8fff, mrdo_videoram2_w, &mrdo_videoram2 },
	{ 0x9000, 0x90ff, MWA_RAM, &mrdo_spriteram },
	{ 0x9801, 0x9801, ladybug_sound1_w },
	{ 0x9802, 0x9802, ladybug_sound2_w },
	{ 0xf800, 0xffff, mrdo_scrollx_w },
	{ 0x9800, 0x9800, MWA_NOP },
	{ 0xf000, 0xf7ff, MWA_NOP },
	{ 0x0000, 0x7fff, MWA_ROM },
	{ -1 }	/* end of table */
};



static struct InputPort input_ports[] =
{
	{	/* IN0 */
		0xff,
		{ OSD_KEY_LEFT, OSD_KEY_DOWN, OSD_KEY_RIGHT, OSD_KEY_UP,
				OSD_KEY_CONTROL, OSD_KEY_1, OSD_KEY_2, 0 },
		{ OSD_JOY_LEFT, OSD_JOY_DOWN, OSD_JOY_RIGHT, OSD_JOY_UP,
				OSD_JOY_FIRE, 0, 0, 0 }
	},
	{	/* IN1 */
		0xff,
		{ 0, 0, 0, 0, 0, 0, OSD_KEY_3, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{	/* DSW1 */
		0xdf,
		{ 0, 0, OSD_KEY_F1, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{	/* DSW2 */
		0xff,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{ -1 }	/* end of table */
};



static struct DSW dsw[] =
{
	{ 2, 0xc0, "LIVES", { "2", "5", "4", "3" }, 1 },
	{ 2, 0x03, "DIFFICULTY", { "HARDEST", "HARD", "MEDIUM", "EASY" }, 1 },
	{ 2, 0x10, "EXTRA", { "HARD", "EASY" }, 1 },
	{ 2, 0x08, "SPECIAL", { "HARD", "EASY" }, 1 },
	{ -1 }
};



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 0, 512*8*8 },	/* the two bitplanes are separated */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	2,	/* 2 bits per pixel */
	{ 4, 0 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0*16, 2*16, 4*16, 6*16, 8*16, 10*16, 12*16, 14*16,
			16*16, 18*16, 20*16, 22*16, 24*16, 26*16, 28*16, 30*16 },
	{ 24+0, 24+1, 24+2, 24+3, 16+0, 16+1, 16+2, 16+3,
			8+0, 8+1, 8+2, 8+3, 0, 1, 2, 3 },
	64*8	/* every sprite takes 64 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,       0, 128 },
	{ 1, 0x2000, &charlayout,       0, 128 },
	{ 1, 0x4000, &spritelayout, 4*128,  16 },
	{ -1 } /* end of array */
};



static unsigned char color_prom[] =
{
	/* palette (high bits) */
	0x00,0x0C,0x03,0x00,0x0F,0x0B,0x0C,0x3F,0x0D,0x0F,0x0F,0x0C,0x0C,0x3C,0x0C,0x30,
	0x0C,0x03,0x30,0x03,0x0C,0x0F,0x00,0x3F,0x03,0x1E,0x00,0x0F,0x37,0x36,0x0D,0x33,
	/* palette (low bits) */
	0x00,0x0C,0x03,0x00,0x0C,0x03,0x00,0x3F,0x0F,0x03,0x0F,0x3F,0x0C,0x0F,0x0F,0x3A,
	0x03,0x0F,0x00,0x0C,0x00,0x0F,0x3F,0x03,0x2A,0x0C,0x00,0x0A,0x0C,0x0E,0x3F,0x0F,
	/* sprite color lookup table */
	0x00,0x97,0x71,0xF9,0x00,0x27,0xA5,0x13,0x00,0x32,0x77,0x3F,0x00,0xA7,0x72,0xF9,
	0x00,0x1F,0x9A,0x77,0x00,0x15,0x27,0x38,0x00,0xC2,0x55,0x69,0x00,0x7F,0x76,0x7A
};



/* waveforms for the audio hardware */
static unsigned char samples[32] =	/* a simple sine (sort of) wave */
{
	0x00,0x00,0x00,0x00,0x22,0x22,0x22,0x22,0x44,0x44,0x44,0x44,0x22,0x22,0x22,0x22,
	0x00,0x00,0x00,0x00,0xdd,0xdd,0xdd,0xdd,0xbb,0xbb,0xbb,0xbb,0xdd,0xdd,0xdd,0xdd
};



const struct MachineDriver mrdo_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz */
			0,
			readmem,writemem,0,0,
			interrupt,1
		}
	},
	60,
	input_ports,dsw,
	0,

	/* video hardware */
	32*8, 32*8, { 4*8, 28*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	256,4*144,
	color_prom,mrdo_vh_convert_color_prom,0,0,
	0,10,
	0x09,0x3e,
	8*17,8*29,0x2c,
	0,
	mrdo_vh_start,
	mrdo_vh_stop,
	mrdo_vh_screenrefresh,

	/* sound hardware */
	samples,
	0,
	ladybug_sh_start,
	0,
	ladybug_sh_update
};

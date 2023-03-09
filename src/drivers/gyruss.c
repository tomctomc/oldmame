/***************************************************************************

Gyruss memory map (preliminary)

Main processor memory map.
0000-5fff ROM (6000-7fff diagnostics)
8000-83ff Color RAM
8400-87ff Video RAM
9000-a7ff RAM
a000-a17f \ sprites
a200-a27f /

memory mapped ports:

read:
c080      IN0
c0a0      IN1
c0c0      IN2
c0e0      DSW0
c000      DSW1
c100      DSW2

write:
a000-a1ff  Odd frame spriteram
a200-a3ff  Even frame spriteram
a700       Frame odd or even?
a701       Semaphore system:  tells 6809 to draw queued sprites
a702       Semaphore system:  tells 6809 to queue sprites
c000       watchdog reset
c080       trigger interrupt on audio CPU
c100       command for the audio CPU
c180       interrupt enable
c185       flip screen

interrupts:
standard NMI at 0x66


SOUND BOARD:
0000-3fff  Audio ROM (4000-5fff diagnostics)
6000-63ff  Audio RAM
8000       Read Sound Command

I/O:

Gyruss has 5 PSGs:
1)  Control: 0x00    Read: 0x01    Write: 0x02
2)  Control: 0x04    Read: 0x05    Write: 0x06
3)  Control: 0x08    Read: 0x09    Write: 0x0a
4)  Control: 0x0c    Read: 0x0d    Write: 0x0e
5)  Control: 0x10    Read: 0x11    Write: 0x12

and 1 SFX channel controlled by an 8039:
1)  SoundOn: 0x14    SoundData: 0x18

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "I8039/I8039.h"


/*#define USE_SAMPLES*/


extern unsigned char *gyruss_spritebank,*gyruss_6809_drawplanet,*gyruss_6809_drawship;
void gyruss_queuereg_w(int offset, int data);
void gyruss_flipscreen_w(int offset, int data);
void gyruss_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
void gyruss_vh_screenrefresh(struct osd_bitmap *bitmap);

int gyruss_portA_r(int offset);
void gyruss_sh_irqtrigger_w(int offset,int data);
void gyruss_init_machine(void);
void gyruss_i8039_irq_w(int offset,int data);
void gyruss_i8039_command_w(int offset,int data);
int gyruss_i8039_command_r(int offset);
void gyruss_digital_out(int offset, int data);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x9000, 0x9fff, MRA_RAM },
	{ 0xa700, 0xa700, MRA_RAM },
	{ 0xa7fc, 0xa7fd, MRA_RAM },
	{ 0xc000, 0xc000, input_port_4_r },	/* DSW1 */
	{ 0xc080, 0xc080, input_port_0_r },	/* IN0 */
	{ 0xc0a0, 0xc0a0, input_port_1_r },	/* IN1 */
	{ 0xc0c0, 0xc0c0, input_port_2_r },	/* IN2 */
	{ 0xc0e0, 0xc0e0, input_port_3_r },	/* DSW0 */
	{ 0xc100, 0xc100, input_port_5_r },	/* DSW2 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },                 /* rom space+1        */
	{ 0x8000, 0x83ff, colorram_w, &colorram },
	{ 0x8400, 0x87ff, videoram_w, &videoram, &videoram_size },
	{ 0x9000, 0x9fff, MWA_RAM },
	{ 0xa000, 0xa17f, MWA_RAM, &spriteram, &spriteram_size },     /* odd frame spriteram */
	{ 0xa200, 0xa37f, MWA_RAM, &spriteram_2 },   /* even frame spriteram */
	{ 0xa700, 0xa700, MWA_RAM, &gyruss_spritebank },
	{ 0xa701, 0xa701, MWA_NOP },        /* semaphore system   */
	{ 0xa702, 0xa702, gyruss_queuereg_w },       /* semaphore system   */
	{ 0xa7fc, 0xa7fc, MWA_RAM, &gyruss_6809_drawplanet },
	{ 0xa7fd, 0xa7fd, MWA_RAM, &gyruss_6809_drawship },
	{ 0xc000, 0xc000, MWA_NOP },	/* watchdog reset */
	{ 0xc080, 0xc080, gyruss_sh_irqtrigger_w },
	{ 0xc100, 0xc100, soundlatch_w },         /* command to soundb  */
	{ 0xc180, 0xc180, interrupt_enable_w },      /* NMI enable         */
	{ 0xc185, 0xc185, gyruss_flipscreen_w },
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },                 /* rom soundboard     */
	{ 0x6000, 0x63ff, MRA_RAM },                 /* ram soundboard     */
	{ 0x8000, 0x8000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },                 /* rom soundboard     */
	{ 0x6000, 0x63ff, MWA_RAM },                 /* ram soundboard     */
	{ -1 }	/* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x01, 0x01, AY8910_read_port_0_r },
  	{ 0x05, 0x05, AY8910_read_port_1_r },
	{ 0x09, 0x09, AY8910_read_port_2_r },
  	{ 0x0d, 0x0d, AY8910_read_port_3_r },
  	{ 0x11, 0x11, AY8910_read_port_4_r },
	{ -1 }
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x02, 0x02, AY8910_write_port_0_w },
	{ 0x04, 0x04, AY8910_control_port_1_w },
	{ 0x06, 0x06, AY8910_write_port_1_w },
	{ 0x08, 0x08, AY8910_control_port_2_w },
	{ 0x0a, 0x0a, AY8910_write_port_2_w },
	{ 0x0c, 0x0c, AY8910_control_port_3_w },
	{ 0x0e, 0x0e, AY8910_write_port_3_w },
	{ 0x10, 0x10, AY8910_control_port_4_w },
	{ 0x12, 0x12, AY8910_write_port_4_w },
	{ 0x14, 0x14, gyruss_i8039_irq_w },
	{ 0x18, 0x18, gyruss_i8039_command_w },
	{ -1 }	/* end of table */
};



#ifndef USE_SAMPLES
static struct MemoryReadAddress i8039_readmem[] =
{
	/* kludge: the code actually does
	00000013: ba 00       	mov r2,#0
	00000015: 05          	en  i
	00000016: fa          	mov a,r2
	and expects r2 to retrieve the command from an external latch. Since the
	8039 emulator doesn't allow that, we kick in later, when the code does
	00000062: 53 0f       	anl a,#$0f
	00000064: 03 32       	add a,#$32
	00000066: b3          	jmpp @a
	since r2 is always 0, it always fetches the first byte of the jump table,
	so we wedge in there and return the correct jump address */
	{ 0x0032, 0x0032, gyruss_i8039_command_r },
	{ 0x0000, 0x0fff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress i8039_writemem[] =
{
	{ -1 }	/* end of table */
};

static struct IOReadPort i8039_readport[] =
{
	{ -1 }
};

static struct IOWritePort i8039_writeport[] =
{
	{ I8039_p1, I8039_p1, gyruss_digital_out },
	{ I8039_p2, I8039_p2, IOWP_NOP },
	{ -1 }	/* end of table */
};
#endif



INPUT_PORTS_START( gyruss_input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_2WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* 1p shoot 2 - unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* 2p shoot 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* 2p shoot 2 - unused */
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0xf0, 0xf0, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x20, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x50, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x80, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x40, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x10, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0xf0, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x30, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x70, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0xe0, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x60, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0xd0, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0xc0, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0xb0, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0xa0, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x90, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Free Play" )
	PORT_DIPNAME( 0x0f, 0x0f, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x05, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x08, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x04, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x01, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0f, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x03, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x07, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0e, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x06, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0x0d, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x0c, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x0b, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0x0a, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x09, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Free Play" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "255", IP_KEY_NONE, IP_JOY_NONE, 0 )
	PORT_DIPNAME( 0x04, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x04, "Cocktail" )
	PORT_DIPNAME( 0x08, 0x08, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x08, "30000 60000" )
	PORT_DIPSETTING(    0x00, "40000 70000" )
	PORT_DIPNAME( 0x70, 0x70, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x70, "1 (Easiest)" )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0x50, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x30, "5 (Average)" )
	PORT_DIPSETTING(    0x20, "6" )
	PORT_DIPSETTING(    0x10, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )
	PORT_DIPNAME( 0x80, 0x00, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x80, "Off" )
	PORT_DIPSETTING(    0x00, "On" )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x00, "Demo Music", IP_KEY_NONE )
	PORT_DIPSETTING(    0x01, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	/* other bits probably unused */
INPUT_PORTS_END

/* This is identical to gyruss except for the bonus that has different
   values */
INPUT_PORTS_START( gyrussce_input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_2WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* 1p shoot 2 - unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* 2p shoot 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* 2p shoot 2 - unused */
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0xf0, 0xf0, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x20, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x50, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x80, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x40, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x10, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0xf0, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x30, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x70, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0xe0, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x60, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0xd0, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0xc0, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0xb0, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0xa0, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x90, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Free Play" )
	PORT_DIPNAME( 0x0f, 0x0f, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x05, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x08, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x04, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x01, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0f, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x03, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x07, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0e, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x06, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0x0d, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x0c, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x0b, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0x0a, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x09, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Free Play" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "255", IP_KEY_NONE, IP_JOY_NONE, 0 )
	PORT_DIPNAME( 0x04, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x04, "Cocktail" )
	PORT_DIPNAME( 0x08, 0x08, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x08, "50000 70000" )
	PORT_DIPSETTING(    0x00, "60000 80000" )
	PORT_DIPNAME( 0x70, 0x70, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x70, "1 (Easiest)" )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0x50, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x30, "5 (Average)" )
	PORT_DIPSETTING(    0x20, "6" )
	PORT_DIPSETTING(    0x10, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )
	PORT_DIPNAME( 0x80, 0x00, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x80, "Off" )
	PORT_DIPSETTING(    0x00, "On" )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x00, "Demo Music", IP_KEY_NONE )
	PORT_DIPSETTING(    0x01, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	/* other bits probably unused */
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3, 8*8+0,8*8+1,8*8+2,8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 consecutive bytes */
};
static struct GfxLayout spritelayout1 =
{
	8,16,	/* 16*8 sprites */
	256,	/* 256 sprites */
	4,	/* 4 bits per pixel */
	{ 0x4000*8+4, 0x4000*8+0, 4, 0  },
	{ 0, 1, 2, 3,  8*8, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 consecutive bytes */
};
static struct GfxLayout spritelayout2 =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	4,	/* 4 bits per pixel */
	{ 0x4000*8+4, 0x4000*8+0, 4, 0  },
	{ 0, 1, 2, 3,  8*8, 8*8+1, 8*8+2, 8*8+3,
		16*8+0, 16*8+1, 16*8+2, 16*8+3,  24*8, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,       0, 16 },
	{ 1, 0x2000, &spritelayout1, 16*4, 16 },	/* upper half */
	{ 1, 0x2010, &spritelayout1, 16*4, 16 },	/* lower half */
	{ 1, 0x2000, &spritelayout2, 16*4, 16 },
	{ -1 } /* end of array */
};



/* these are NOT the original color PROMs */
static unsigned char color_prom[] =
{
	/* 2A - palette */
	0x00,0x07,0x3F,0x38,0xF8,0xE8,0xD8,0x90,0x27,0x1E,0x15,0x0C,0xAD,0xA4,0x5B,0xFF,
	0x00,0x37,0x2E,0x25,0x1C,0x13,0x15,0x0C,0x03,0x02,0x60,0xB0,0x58,0xF6,0xA4,0x52,
	/* 6F - sprite lookup table */
	0x00,0x0F,0x05,0x06,0x07,0x0F,0x05,0x05,0x06,0x05,0x00,0x06,0x07,0x0F,0x0D,0x01,
	0x00,0x0F,0x05,0x00,0x00,0x05,0x06,0x07,0x07,0x00,0x00,0x06,0x07,0x0F,0x0D,0x01,
	0x00,0x0F,0x05,0x06,0x07,0x0F,0x05,0x05,0x06,0x05,0x03,0x06,0x07,0x0F,0x0D,0x01,
	0x00,0x0F,0x05,0x00,0x00,0x05,0x06,0x07,0x07,0x00,0x03,0x06,0x07,0x0F,0x0D,0x01,
	0x00,0x0C,0x0D,0x05,0x07,0x0E,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x08,0x00,0x07,
	0x00,0x0F,0x0C,0x0D,0x0E,0x06,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x09,0x00,0x06,
	0x00,0x04,0x05,0x06,0x07,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0x00,0x05,
	0x00,0x08,0x09,0x0A,0x0B,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0x00,0x04,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x0F,0x0F,0x0F,0x0D,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x03,
	0x00,0x0F,0x04,0x06,0x0E,0x0E,0x0E,0x0E,0x0A,0x0A,0x0A,0x0A,0x07,0x07,0x07,0x07,
	0x00,0x01,0x0A,0x0F,0x0B,0x0B,0x0B,0x0B,0x07,0x07,0x07,0x07,0x0E,0x0E,0x0E,0x0E,
	0x00,0x04,0x0B,0x07,0x0D,0x0D,0x0D,0x0D,0x09,0x09,0x09,0x09,0x06,0x06,0x06,0x06,
	0x00,0x03,0x0C,0x09,0x0A,0x0A,0x0A,0x0A,0x06,0x06,0x06,0x06,0x0D,0x0D,0x0D,0x0D,
	0x00,0x09,0x0C,0x02,0x0F,0x0C,0x0D,0x0E,0x08,0x09,0x0A,0x0B,0x04,0x05,0x06,0x07,
	0x00,0x01,0x02,0x03,0x08,0x09,0x0A,0x0B,0x04,0x05,0x06,0x07,0x0F,0x0C,0x0D,0x0E,
	/* 3E - character lookup table */
	0x00,0x04,0x03,0x02,0x00,0x03,0x02,0x01,0x00,0x06,0x0E,0x0D,0x00,0x09,0x04,0x05,
	0x00,0x04,0x05,0x0D,0x00,0x0F,0x0E,0x0D,0x00,0x0C,0x0A,0x0B,0x05,0x02,0x04,0x03,
	0x03,0x07,0x05,0x04,0x00,0x09,0x08,0x04,0x00,0x0D,0x0E,0x0B,0x00,0x0A,0x0D,0x0B,
	0x00,0x0D,0x0A,0x0E,0x00,0x08,0x07,0x06,0x00,0x09,0x08,0x07,0x00,0x05,0x04,0x03,
	0x00,0x06,0x0E,0x0D,0x00,0x03,0x04,0x05,0x00,0x0D,0x0E,0x0F,0x00,0x05,0x01,0x03,
	0x00,0x08,0x0A,0x0D,0x00,0x04,0x0D,0x0E,0x00,0x07,0x0B,0x0F,0x00,0x0B,0x0C,0x07,
	0x00,0x03,0x05,0x08,0x00,0x06,0x03,0x09,0x00,0x02,0x0F,0x0A,0x00,0x01,0x0E,0x0B,
	0x00,0x06,0x0A,0x0C,0x00,0x02,0x08,0x0D,0x00,0x06,0x03,0x0E,0x00,0x09,0x0A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F
};



static struct AY8910interface ay8910_interface =
{
	5,	/* 5 chips */
	1789772,	/* 1.789772727 MHz */
	{ 0x20ff, 0x20ff, 0x38ff, 0x38ff, 0x38ff },
	/*  R       L   |   R       R       L */
	/*   effects    |         music       */
	{ 0, 0, gyruss_portA_r },
	{ 0 },
	{ 0 },
	{ 0 }
};

#ifndef USE_SAMPLES
static struct DACinterface dac_interface =
{
	1,
	441000,
	{ 128, 128 },
	{  1,  1 }
};
#else
static struct Samplesinterface samples_interface =
{
	1	/* 1 channel */
};
#endif


static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz (?) */
			0,
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.5795454 Mhz */
			2,	/* memory region #2 */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		},
#ifndef USE_SAMPLES
		{
			CPU_I8039 | CPU_AUDIO_CPU,
			8000000/15,	/* 8Mhz crystal */
			4,
			i8039_readmem,i8039_writemem,i8039_readport,i8039_writeport,
			ignore_interrupt,1
		}
#endif
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	gyruss_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32,16*4+16*16,
	gyruss_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	gyruss_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		},
#ifndef USE_SAMPLES
		{
			SOUND_DAC,
			&dac_interface
		}
#else
		{
			SOUND_SAMPLES,
			&samples_interface
		}
#endif
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( gyruss_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "gyrussk.1", 0x0000, 0x2000, 0x859dab03 )
	ROM_LOAD( "gyrussk.2", 0x2000, 0x2000, 0x99244034 )
	ROM_LOAD( "gyrussk.3", 0x4000, 0x2000, 0xe77f026b )
	/* the diagnostics ROM would go here */

	ROM_REGION(0xa000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "gyrussk.4",  0x0000, 0x2000, 0x98f88b6e )
	ROM_LOAD( "gyrussk.6",  0x2000, 0x2000, 0x90c095a2 )
	ROM_LOAD( "gyrussk.5",  0x4000, 0x2000, 0x2fb43a10 )
	ROM_LOAD( "gyrussk.8",  0x6000, 0x2000, 0xe7297079 )
	ROM_LOAD( "gyrussk.7",  0x8000, 0x2000, 0xff46ed2e )

	ROM_REGION(0x10000)	/* 64k for the audio CPU */
	ROM_LOAD( "gyrussk.1a", 0x0000, 0x2000, 0x4fa107c1 )
	ROM_LOAD( "gyrussk.2a", 0x2000, 0x2000, 0xd20aa58c )
	/* the diagnostics ROM would go here */

	ROM_REGION(0x2000)	/* Gyruss also contains a 6809, we don't need to emulate it */
						/* but need the data tables contained in its ROM */
	ROM_LOAD( "gyrussk.9",  0x0000, 0x2000, 0xef92fcd8 )

	ROM_REGION(0x1000)	/* 8039 */
	ROM_LOAD( "gyrussk.3a", 0x0000, 0x1000, 0x18d6bc42 )
ROM_END

ROM_START( gyrussce_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "gya-1.bin", 0x0000, 0x2000, 0x9fef8629 )
	ROM_LOAD( "gya-2.bin", 0x2000, 0x2000, 0xe7243234 )
	ROM_LOAD( "gya-3.bin", 0x4000, 0x2000, 0x8c1eeeec )
	/* the diagnostics ROM would go here */

	ROM_REGION(0xa000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "gy-6.bin",  0x0000, 0x2000, 0x98f88b6e )
	ROM_LOAD( "gy-10.bin", 0x2000, 0x2000, 0x90c095a2 )
	ROM_LOAD( "gy-9.bin",  0x4000, 0x2000, 0x2fb43a10 )
	ROM_LOAD( "gy-8.bin",  0x6000, 0x2000, 0xe7297079 )
	ROM_LOAD( "gy-7.bin",  0x8000, 0x2000, 0xff46ed2e )

	ROM_REGION(0x10000)	/* 64k for the audio CPU */
	ROM_LOAD( "gy-11.bin", 0x0000, 0x2000, 0x4fa107c1 )
	ROM_LOAD( "gy-12.bin", 0x2000, 0x2000, 0xd20aa58c )
	/* the diagnostics ROM would go here */

	ROM_REGION(0x2000)	/* Gyruss also contains a 6809, we don't need to emulate it */
						/* but need the data tables contained in its ROM */
	ROM_LOAD( "gy-5.bin",  0x0000, 0x2000, 0xef92fcd8 )

	ROM_REGION(0x1000)	/* 8039 */
	ROM_LOAD( "gy-13.bin", 0x0000, 0x1000, 0x18d6bc42 )
ROM_END


#ifdef USE_SAMPLES
static const char *gyruss_sample_names[] =
{
	"*gyruss",
	"AUDIO01.SAM",
	"AUDIO02.SAM",
	"AUDIO03.SAM",
	"AUDIO04.SAM",
	"AUDIO05.SAM",
	"AUDIO06.SAM",
	"AUDIO07.SAM",
	0	/* end of array */
};
#endif



static int hiload(void)
{
	/* get RAM pointer (this game is multiCPU, we can't assume the global */
	/* RAM pointer is pointing to the right place) */
	unsigned char *RAM = Machine->memory_region[0];

	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0x9489],"\x00\x00\x01",3) == 0 &&
			memcmp(&RAM[0x94a9],"\x00\x43\x00",3) == 0)
	{
		void *f;


		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread(f,&RAM[0x9488],8*5);
			RAM[0x940b] = RAM[0x9489];
			RAM[0x940c] = RAM[0x948a];
			RAM[0x940d] = RAM[0x948b];
			osd_fclose(f);
		}

		return 1;
	}
	else return 0;	/* we can't load the hi scores yet */
}

static void hisave(void)
{
	void *f;
	/* get RAM pointer (this game is multiCPU, we can't assume the global */
	/* RAM pointer is pointing to the right place) */
	unsigned char *RAM = Machine->memory_region[0];


	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
		osd_fwrite(f,&RAM[0x9488],8*5);
		osd_fclose(f);
	}
}



struct GameDriver gyruss_driver =
{
	"Gyruss (Konami)",
	"gyruss",
	"Mike Cuddy (hardware info)\nPete Ground (hardware info)\nMirko Buffoni (MAME driver)\nNicola Salmoria (MAME driver)\nTim Lindquist (color info)\nMarco Cassili",
	&machine_driver,

	gyruss_rom,
	0, 0,
#ifdef USE_SAMPLES
	gyruss_sample_names,
#else
	0,
#endif
	0,	/* sound_prom */

	gyruss_input_ports,

	color_prom, 0, 0,
	ORIENTATION_ROTATE_90,

	hiload, hisave
};

struct GameDriver gyrussce_driver =
{
	"Gyruss (Centuri)",
	"gyrussce",
	"Mike Cuddy (hardware info)\nPete Ground (hardware info)\nMirko Buffoni (MAME driver)\nNicola Salmoria (MAME driver)\nTim Lindquist (color info)\nMarco Cassili",
	&machine_driver,

	gyrussce_rom,
	0, 0,
#ifdef USE_SAMPLES
	gyruss_sample_names,
#else
	0,
#endif
	0,	/* sound_prom */

	gyrussce_input_ports,

	color_prom, 0, 0,
	ORIENTATION_ROTATE_90,

	hiload, hisave
};

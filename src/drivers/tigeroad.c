/***************************************************************************

remaining issues:
	sound not hooked up (YM2151 Z80)
	sprites are out of sync while scrolling
	(I haven't tried tweaking the vblank parameter)
	busy loop should be patched out

Please contact Phil Stroffolino (phil@maya.com) if there are any questions
regarding this driver.

Tiger Road is (C) 1987 Romstar/Capcom USA

Memory Overview:
	0xfe0800	sprites
	0xfec000	text
	0xfe4000	input ports,dip switches (read); sound out, video reg (write)
	0xfe8000	scroll registers
	0xff8200	palette
	0xffC000	working RAM
	0xffEC70	high scores (not saved)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "Z80/Z80.h"


extern int tigerroad_base_bank, tigerroad_scrollx, tigerroad_scrolly;

void tigerroad_vh_screenrefresh(struct osd_bitmap *bitmap);

int tigerroad_vh_start (void);

void tigerroad_vh_stop (void);

void tigerroad_SetPaletteEntry( int offset, int data );


static int tigeroad_spriteram_r(int offset){ return READ_WORD (&spriteram[offset]); }
static void tigeroad_spriteram_w(int offset, int data){ COMBINE_WORD_MEM (&spriteram[offset], data); }

static int tigeroad_videoram_r(int offset){ return READ_WORD (&videoram[offset]); }
static void tigeroad_videoram_w(int offset, int data){ COMBINE_WORD_MEM (&videoram[offset], data); }

static int tigeroad_scrollram_r( int offset ){
	switch( offset ){
		case 0: return tigerroad_scrollx;
		case 2: return tigerroad_scrolly;
	}
	return 0;
}

static void tigeroad_scrollram_w( int offset, int data ){
	switch( offset ){
		case 0: tigerroad_scrollx = data; break;
		case 2: tigerroad_scrolly = data; break;
	}
}

static int tigeroad_colorram_r( int offset ){ return READ_WORD (&colorram[offset]); }
static void tigeroad_colorram_w( int offset, int data );
static void tigeroad_colorram_w( int offset, int data ){
	data = COMBINE_WORD_MEM (&colorram[offset], data );
	tigerroad_SetPaletteEntry( offset, data );
}

static void tigeroad_control_w( int offset, int data )
{
	switch( offset )
	{
		case 0:
			tigerroad_base_bank = (data>>8)&0xF;
			break;
		case 2:
			soundlatch_w(offset,(data >> 8) & 0xff);
			break;
	}
}

static int tigeroad_input_r (int offset);
static int tigeroad_input_r (int offset){
	switch (offset){
		case 0: return (input_port_1_r( offset )<<8) |
						input_port_0_r( offset );

		case 2: return (input_port_3_r( offset )<<8) |
						input_port_2_r( offset );

		case 4: return (input_port_5_r( offset )<<8) |
						input_port_4_r( offset );
	}
	return 0x00;
}

int tigerroad_interrupt(void){
	return 2;
}


/***************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xfe0800, 0xfe3fff, tigeroad_spriteram_r, &spriteram, &spriteram_size },
	{ 0xfe4000, 0xfe4007, tigeroad_input_r },
	{ 0xfec000, 0xfec7ff, tigeroad_videoram_r, &videoram, &videoram_size },
	{ 0xfe8000, 0xfe800f, tigeroad_scrollram_r },
	{ 0xff8200, 0xffbfff, tigeroad_colorram_r, &colorram },
	{ 0xffc000, 0xffffff, MRA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xfe0800, 0xfe3fff, tigeroad_spriteram_w },
	{ 0xfe4000, 0xfe4003, tigeroad_control_w },
	{ 0xfec000, 0xfec7ff, tigeroad_videoram_w },
	{ 0xfe8000, 0xfe800f, tigeroad_scrollram_w },
	{ 0xff8200, 0xffbfff, tigeroad_colorram_w },
	{ 0xffc000, 0xffffff, MWA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8000, YM2203_status_port_0_r },
	{ 0xa000, 0xa000, YM2203_status_port_1_r },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xe000, 0xe000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8000, YM2203_control_port_0_w },
	{ 0x8001, 0x8001, YM2203_write_port_0_w },
	{ 0xa000, 0xa000, YM2203_control_port_1_w },
	{ 0xa001, 0xa001, YM2203_write_port_1_w },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* dipswitch A */
	PORT_DIPNAME( 0x07, 0x07, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x01, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x02, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x07, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x06, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x05, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x04, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x03, "1 Coin/6 Credits" )
	PORT_DIPNAME( 0x38, 0x38, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x08, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x10, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x38, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x30, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x28, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x20, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x18, "1 Coin/6 Credits" )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x40, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x80, 0x80, "Flip Screen", IP_KEY_NONE )
	PORT_DIPSETTING(    0x80, "Off")
	PORT_DIPSETTING(    0x00, "On" )

	PORT_START /* dipswitch B */
	PORT_DIPNAME( 0x03, 0x03, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright")
	PORT_DIPSETTING(    0x04, "Cocktail")
	PORT_DIPNAME( 0x18, 0x18, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x18, "20000 70000 70000" )
	PORT_DIPSETTING(    0x10, "20000 80000 80000" )
	PORT_DIPSETTING(    0x08, "30000 80000 80000" )
	PORT_DIPSETTING(    0x00, "30000 90000 90000" )
	PORT_DIPNAME( 0x60, 0x60, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x20, "Very Easy" )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "No" )
	PORT_DIPSETTING(    0x80, "Yes" )
INPUT_PORTS_END



static struct GfxLayout text_layout =
{
	8,8,	/* character size */
	2048,	/* number of characters */
	2,		/* bits per pixel */
	{ 4, 0 }, /* plane offsets */
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 }, /* x offsets */
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 }, /* y offsets */
	16*8
};

static struct GfxLayout tile_layout =
{
	32,32,	/* tile size */
	256*8,	/* number of tiles */
	4,		/* bits per pixel */

	{ 0x80000*8+4, 0x80000*8, 4, 0 },

	{ /* x offsets */
		0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
		64*8+0, 64*8+1, 64*8+2, 64*8+3, 64*8+8+0, 64*8+8+1, 64*8+8+2, 64*8+8+3,
		64*8*2+0, 64*8*2+1, 64*8*2+2, 64*8*2+3,	64*8*2+ 8+0, 64*8*2+ 8+1, 64*8*2+ 8+2, 64*8*2+ 8+3,
		64*8*3+0, 64*8*3+1, 64*8*3+2, 64*8*3+3,	64*8*3+8+0, 64*8*3+8+1, 64*8*3+8+2, 64*8*3+8+3,
	},

	{ /* y offsets */
		0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16,
		16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16,
		24*16, 25*16, 26*16, 27*16, 28*16, 29*16, 30*16, 31*16
	},

	64*32
};

static struct GfxLayout sprite_layout =
{
	16,16,	/* tile size */
	4096,	/* number of tiles */
	4,		/* bits per pixel */
	{ 0x020000*8*3, 0x020000*8*2, 0x020000*8*1,0x020000*8*0 }, /* plane offsets */
	{ /* x offsets */
		0, 1, 2, 3, 4, 5, 6, 7,
		128+0, 128+1, 128+2, 128+3, 128+4, 128+5, 128+6, 128+7
	},
	{ /* y offsets */
		0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8
	},
	256
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x000000, &tile_layout,   0,           16 },
	{ 1, 0x100000, &sprite_layout, 16*16,       16 },
	{ 1, 0x180000, &text_layout,   16*16+16*16, 16 },
	{ -1 } /* end of array */
};



/* handler called by the 2203 emulator when the internal timers cause an IRQ */
static void irqhandler(void)
{
	cpu_cause_interrupt(1,0xff);
}

static struct YM2203interface ym2203_interface =
{
	2,			/* 2 chips */
	3500000,	/* 3.5 MHz ? */
	{ YM2203_VOL(100,255), YM2203_VOL(100,255) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};



static struct MachineDriver machine_driver =
{
	{
		{
			CPU_M68000,
			6000000, /* ? Main clock is 24MHz */
			0,
			readmem,writemem,0,0,
			tigerroad_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,	/* 4 Mhz ??? */
			3,	/* memory region #3 */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2203 */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
				/* vblank duration hand tuned to get proper sprite/background alignment */
	1,	/* CPU slices per frame */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,16*16+16*16+16*4,
	0, /* convert color prom routine */

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_16BIT,
	0,
	tigerroad_vh_start,
	tigerroad_vh_stop,
	tigerroad_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

ROM_START( tigerroad_rom )
	ROM_REGION(0x40000)	/* 256K for 68000 code */
	ROM_LOAD_ODD(  "tru04.bin", 0x00000, 0x20000, 0x048f38b5 )
	ROM_LOAD_EVEN( "tru02.bin", 0x00000, 0x20000, 0xab4a273e )

	ROM_REGION(0x188000) /* temporary space for graphics */
	ROM_LOAD("tr-01a.bin",	0x000000, 0x20000, 0x1059094b )	/* tiles */
	ROM_LOAD("tr-04a.bin",	0x020000, 0x20000, 0xc22b045b )
	ROM_LOAD("tr-02a.bin",	0x040000, 0x20000, 0xe36c6358 )
	ROM_LOAD("tr05.bin",	0x060000, 0x20000, 0x63f7aa59 )
	ROM_LOAD("tr-03a.bin",	0x080000, 0x20000, 0x08193fc5 )
	ROM_LOAD("tr-06a.bin",	0x0A0000, 0x20000, 0x4f61a23d )
	ROM_LOAD("tr-07a.bin",	0x0C0000, 0x20000, 0x4742c952 )
	ROM_LOAD("tr08.bin",	0x0E0000, 0x20000, 0x01e5018f )
	ROM_LOAD("tr-09a.bin",	0x100000, 0x20000, 0xaccdc4d9 )	/* sprites */
	ROM_LOAD("tr-10a.bin",	0x120000, 0x20000, 0x370e3954 )
	ROM_LOAD("tr-11a.bin",	0x140000, 0x20000, 0x35a273f2 )
	ROM_LOAD("tr-12a.bin",	0x160000, 0x20000, 0x81bf63fd )
	ROM_LOAD("tr01.bin",	0x180000, 0x08000, 0x35379305 )	/* 8x8 text */

	ROM_REGION( 0x08000 ) /* tilemap for background */
	ROM_LOAD( "tr13.bin", 	0x00000, 0x08000, 0x8ad0fd92 )

	ROM_REGION( 0x10000 ) /* audio CPU */
	ROM_LOAD( "tru05.bin", 	0x00000, 0x08000, 0xe626a5ba )
ROM_END

struct GameDriver tigeroad_driver =
{
	"Tiger Road",
	"tigeroad",
	"Phil Stroffolino (MAME driver)\nTim Lindquist",
	&machine_driver,

	tigerroad_rom,
	0,0,0,0,

	input_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	0, 0
};

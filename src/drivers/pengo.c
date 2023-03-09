/***************************************************************************

Pengo memory map (preliminary)

0000-7fff ROM
8000-83ff Video RAM
8400-87ff Color RAM
8800-8fff RAM

memory mapped ports:

read:
9000      DSW1
9040      DSW0
9080      IN1
90c0      IN0
see the input_ports definition below for details on the input bits

write:
8ff2-8ffd 6 pairs of two bytes:
          the first byte contains the sprite image number (bits 2-7), Y flip (bit 0),
		  X flip (bit 1); the second byte the color
9005      sound voice 1 waveform (nibble)
9011-9013 sound voice 1 frequency (nibble)
9015      sound voice 1 volume (nibble)
900a      sound voice 2 waveform (nibble)
9016-9018 sound voice 2 frequency (nibble)
901a      sound voice 2 volume (nibble)
900f      sound voice 3 waveform (nibble)
901b-901d sound voice 3 frequency (nibble)
901f      sound voice 3 volume (nibble)
9022-902d Sprite coordinates, x/y pairs for 6 sprites
9040      interrupt enable
9041      sound enable
9042      palette bank selector
9043      flip screen
9044-9045 coin counters
9046      color lookup table bank selector
9047      character/sprite bank selector
9070      watchdog reset

Main clock: XTAL = 18.432 MHz
Z80 Clock: XTAL/6 = 3.072 MHz
Horizontal video frequency: HSYNC = XTAL/3/192/2 = 16 kHz
Video frequency: VSYNC = HSYNC/132/2 = 60.606060 Hz
VBlank duration: 1/VSYNC * (20/132) = 2500 us

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



void pengo_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
void pengo_updatehook0(int offset);
void pengo_gfxbank_w(int offset,int data);
int pengo_vh_start(void);
void pengo_vh_screenrefresh(struct osd_bitmap *bitmap);

extern unsigned char *pengo_soundregs;
void pengo_sound_enable_w(int offset,int data);
void pengo_sound_w(int offset,int data);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },	/* video and color RAM, scratchpad RAM, sprite codes */
	{ 0x9000, 0x903f, input_port_3_r },	/* DSW1 */
	{ 0x9040, 0x907f, input_port_2_r },	/* DSW0 */
	{ 0x9080, 0x90bf, input_port_1_r },	/* IN1 */
	{ 0x90c0, 0x90ff, input_port_0_r },	/* IN0 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x83ff, videoram00_w, &videoram00 },
	{ 0x8400, 0x87ff, videoram01_w, &videoram01 },
	{ 0x8800, 0x8fef, MWA_RAMROM },
	{ 0x8ff0, 0x8fff, MWA_RAM, &spriteram, &spriteram_size},
	{ 0x9000, 0x901f, pengo_sound_w, &pengo_soundregs },
	{ 0x9020, 0x902f, MWA_RAM, &spriteram_2 },
	{ 0x9040, 0x9040, interrupt_enable_w },
	{ 0x9041, 0x9041, pengo_sound_enable_w },
	{ 0x9042, 0x9042, MWA_NOP },
	{ 0x9043, 0x9043, MWA_RAM, &flip_screen },
	{ 0x9044, 0x9046, MWA_NOP },
	{ 0x9047, 0x9047, pengo_gfxbank_w },
	{ 0x9070, 0x9070, MWA_NOP },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	/* the coin input must stay low for no less than 2 frames and no more */
	/* than 9 frames to pass the self test check. */
	/* Moreover, this way we avoid the game freezing until the user releases */
	/* the "coin" key. */
	PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_COIN1 | IPF_IMPULSE,
			"Coin A", IP_KEY_DEFAULT, IP_JOY_DEFAULT, 2 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_COIN2 | IPF_IMPULSE,
			"Coin B", IP_KEY_DEFAULT, IP_JOY_DEFAULT, 2 )
	/* Coin Aux doesn't need IMPULSE to pass the test, but it still needs it */
	/* to avoid the freeze. */
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_COIN3 | IPF_IMPULSE,
			"Coin Aux", IP_KEY_DEFAULT, IP_JOY_DEFAULT, 2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_SERVICE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPSETTING(    0x01, "50000" )
	PORT_DIPNAME( 0x02, 0x00, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x04, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x04, "Cocktail" )
	PORT_DIPNAME( 0x18, 0x10, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x18, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_BITX(    0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Rack Test", OSD_KEY_F1, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x20, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0xc0, 0x80, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0xc0, "Easy" )
	PORT_DIPSETTING(    0x80, "Medium" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0c, "A Coin/Cred", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "4/1" )
	PORT_DIPSETTING(    0x08, "3/1" )
	PORT_DIPSETTING(    0x04, "2/1" )
	PORT_DIPSETTING(    0x09, "2/1+Bonus each 5" )
	PORT_DIPSETTING(    0x05, "2/1+Bonus each 4" )
	PORT_DIPSETTING(    0x0c, "1/1" )
	PORT_DIPSETTING(    0x0d, "1/1+Bonus each 5" )
	PORT_DIPSETTING(    0x03, "1/1+Bonus each 4" )
	PORT_DIPSETTING(    0x0b, "1/1+Bonus each 2" )
	PORT_DIPSETTING(    0x02, "1/2" )
	PORT_DIPSETTING(    0x07, "1/2+Bonus each 5" )
	PORT_DIPSETTING(    0x0f, "1/2+Bonus each 4" )
	PORT_DIPSETTING(    0x0a, "1/3" )
	PORT_DIPSETTING(    0x06, "1/4" )
	PORT_DIPSETTING(    0x0e, "1/5" )
	PORT_DIPSETTING(    0x01, "1/6" )
	PORT_DIPNAME( 0xf0, 0xc0, "B Coin/Cred", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "4/1" )
	PORT_DIPSETTING(    0x80, "3/1" )
	PORT_DIPSETTING(    0x40, "2/1" )
	PORT_DIPSETTING(    0x90, "2/1+Bonus each 5" )
	PORT_DIPSETTING(    0x50, "2/1+Bonus each 4" )
	PORT_DIPSETTING(    0xc0, "1/1" )
	PORT_DIPSETTING(    0xd0, "1/1+Bonus each 5" )
	PORT_DIPSETTING(    0x30, "1/1+Bonus each 4" )
	PORT_DIPSETTING(    0xb0, "1/1+Bonus each 2" )
	PORT_DIPSETTING(    0x20, "1/2" )
	PORT_DIPSETTING(    0x70, "1/2+Bonus each 5" )
	PORT_DIPSETTING(    0xf0, "1/2+Bonus each 4" )
	PORT_DIPSETTING(    0xa0, "1/3" )
	PORT_DIPSETTING(    0x60, "1/4" )
	PORT_DIPSETTING(    0xe0, "1/5" )
	PORT_DIPSETTING(    0x10, "1/6" )
INPUT_PORTS_END



static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x1000, &spritelayout,    0, 32 },
	{ 1, 0x3000, &spritelayout, 4*32, 32 },
	{ -1 } /* end of array */
};



static struct GfxTileLayout tilelayout =
{
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },	/* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 bytes */
};

static struct GfxTileDecodeInfo gfxtiledecodeinfo[] =
{
	{ 1, 0x0000, &tilelayout,      0, 32, 0 },	/* first bank */
	{ 1, 0x2000, &tilelayout,   4*32, 32, 0 },	/* second bank */
	{ -1 } /* end of array */
};



static struct MachineLayer machine_layers[MAX_LAYERS] =
{
	{
		LAYER_TILE,
		36*8,28*8,
		gfxtiledecodeinfo,
		0,
		pengo_updatehook0,pengo_updatehook0,0,0
	}
};



static struct namco_interface namco_interface =
{
	3072000/32,	/* sample rate */
	3,			/* number of voices */
	32,			/* gain adjustment */
	255			/* playback volume */
};



static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
/*			18432000/6,	* 3.072 Mhz */
			3020000,	/* The correct speed is 3.072 Mhz, but 3.020 gives a more */
						/* accurate emulation speed (time for two attract mode */
						/* cycles after power up, until the high score list appears */
						/* for the second time: 3'39") */
			0,
			readmem,writemem,0,0,
			interrupt,1
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	32,4*64,
	pengo_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	machine_layers,
	pengo_vh_start,
	generic_vh_stop,
	pengo_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NAMCO,
			&namco_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( pengo_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "ic8",  0x0000, 0x1000, 0x9efcdacc )
	ROM_LOAD( "ic7",  0x1000, 0x1000, 0xaf3edc40 )
	ROM_LOAD( "ic15", 0x2000, 0x1000, 0x474d646b )
	ROM_LOAD( "ic14", 0x3000, 0x1000, 0x76f727ad )
	ROM_LOAD( "ic21", 0x4000, 0x1000, 0xe766e256 )
	ROM_LOAD( "ic20", 0x5000, 0x1000, 0xf3400000 )
	ROM_LOAD( "ic32", 0x6000, 0x1000, 0x4f9816ba )
	ROM_LOAD( "ic31", 0x7000, 0x1000, 0x673f6491 )

	ROM_REGION(0x4000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ic92",  0x0000, 0x2000, 0x6865b315 )
	ROM_LOAD( "ic105", 0x2000, 0x2000, 0xbb009a64 )
ROM_END

ROM_START( pengoa_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "pengo.u8",  0x0000, 0x1000, 0x63680136 )
	ROM_LOAD( "pengo.u7",  0x1000, 0x1000, 0xe9ee4c30 )
	ROM_LOAD( "pengo.u15", 0x2000, 0x1000, 0x13baf5dc )
	ROM_LOAD( "pengo.u14", 0x3000, 0x1000, 0x5d563bbc )
	ROM_LOAD( "pengo.u21", 0x4000, 0x1000, 0x6e1ee25e )
	ROM_LOAD( "pengo.u20", 0x5000, 0x1000, 0x67866864 )
	ROM_LOAD( "pengo.u32", 0x6000, 0x1000, 0x9938161a )
	ROM_LOAD( "pengo.u31", 0x7000, 0x1000, 0x4f0eeb9e )

	ROM_REGION(0x4000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "pengo.u92", 0x0000, 0x2000, 0x6865b315 )
	ROM_LOAD( "pengo.105", 0x2000, 0x2000, 0xbb009a64 )
ROM_END

ROM_START( penta_rom )
	ROM_REGION(0x10000)     /* 64k for code */
	ROM_LOAD( "008_PN01.BIN", 0x0000, 0x1000, 0x91f72dcf )
	ROM_LOAD( "007_PN05.BIN", 0x1000, 0x1000, 0x9e4c7c42 )
	ROM_LOAD( "015_PN02.BIN", 0x2000, 0x1000, 0xf1576ecb )
	ROM_LOAD( "014_PN06.BIN", 0x3000, 0x1000, 0xea2b272d )
	ROM_LOAD( "021_PN03.BIN", 0x4000, 0x1000, 0xd4da6a7e )
	ROM_LOAD( "020_PN07.BIN", 0x5000, 0x1000, 0xa3400000 )
	ROM_LOAD( "032_PN04.BIN", 0x6000, 0x1000, 0xb68a3498 )
	ROM_LOAD( "031_PN08.BIN", 0x7000, 0x1000, 0x7b3da013 )

	ROM_REGION(0x4000)      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "092_PN09.BIN", 0x0000, 0x2000, 0x7aa4e020 )
	ROM_LOAD( "105_PN10.BIN", 0x2000, 0x2000, 0xbb009a64 )
ROM_END



/* waveforms for the audio hardware */
static unsigned char sound_prom[] =
{
	0x08,0x08,0x08,0x08,0x08,0x0F,0x0F,0x08,0x08,0x00,0x00,0x00,0x08,0x08,0x08,0x08,
	0x0F,0x0F,0x0F,0x08,0x00,0x00,0x08,0x08,0x08,0x08,0x0F,0x0F,0x08,0x08,0x00,0x00,
	0x07,0x09,0x0A,0x0B,0x07,0x0D,0x0D,0x07,0x0E,0x07,0x0D,0x0D,0x07,0x0B,0x0A,0x09,
	0x07,0x05,0x07,0x03,0x07,0x01,0x07,0x00,0x07,0x00,0x07,0x01,0x07,0x03,0x07,0x05,
	0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x08,0x08,0x08,0x00,0x08,0x08,0x0F,0x0F,0x00,0x00,0x08,0x08,0x08,0x0F,0x0F,0x0F,
	0x00,0x08,0x08,0x00,0x0F,0x0F,0x08,0x08,0x08,0x08,0x0F,0x08,0x00,0x00,0x00,0x08,
	0x07,0x0A,0x0C,0x0D,0x0E,0x0D,0x0C,0x0A,0x07,0x04,0x02,0x01,0x00,0x01,0x02,0x04,
	0x07,0x0B,0x0D,0x0E,0x0D,0x0B,0x07,0x03,0x01,0x00,0x01,0x03,0x07,0x0E,0x07,0x00,
	0x07,0x0E,0x0C,0x09,0x0C,0x0E,0x0A,0x07,0x0C,0x0F,0x0D,0x08,0x0A,0x0B,0x07,0x02,
	0x08,0x0D,0x09,0x04,0x05,0x07,0x02,0x00,0x03,0x08,0x05,0x01,0x03,0x06,0x03,0x01,
	0x07,0x08,0x0A,0x0C,0x0E,0x0D,0x0C,0x0C,0x0B,0x0A,0x08,0x07,0x05,0x06,0x07,0x08,
	0x08,0x09,0x0A,0x0B,0x09,0x08,0x06,0x05,0x04,0x04,0x03,0x02,0x04,0x06,0x08,0x09,
	0x0A,0x0C,0x0C,0x0A,0x07,0x07,0x08,0x0B,0x0D,0x0E,0x0D,0x0A,0x06,0x05,0x05,0x07,
	0x09,0x09,0x08,0x04,0x01,0x00,0x01,0x03,0x06,0x07,0x07,0x04,0x02,0x02,0x04,0x07
};



static unsigned char color_prom[] =
{
	/* palette */
	0x00,0xF6,0x07,0x38,0xC9,0xF8,0x3F,0xEF,0x6F,0x16,0x2F,0x7F,0xF0,0x36,0xDB,0xC6,
	0x00,0xF6,0xD8,0xF0,0xF8,0x16,0x07,0x2F,0x36,0x3F,0x7F,0x28,0x32,0x38,0xEF,0xC6,
	/* color lookup table (512x8, but only the first 256 bytes are used) */
	0x00,0x00,0x00,0x00,0x00,0x05,0x03,0x01,0x00,0x05,0x02,0x01,0x00,0x05,0x06,0x01,
	0x00,0x05,0x07,0x01,0x00,0x05,0x0A,0x01,0x00,0x05,0x0B,0x01,0x00,0x05,0x0C,0x01,
	0x00,0x05,0x0D,0x01,0x00,0x05,0x04,0x01,0x00,0x03,0x06,0x01,0x00,0x03,0x02,0x01,
	0x00,0x03,0x07,0x01,0x00,0x03,0x05,0x01,0x00,0x02,0x03,0x01,0x00,0x00,0x00,0x00,
	0x00,0x08,0x03,0x01,0x00,0x09,0x02,0x05,0x00,0x08,0x05,0x0D,0x04,0x04,0x04,0x04,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x02,0x02,0x00,0x03,0x03,0x03,
	0x00,0x06,0x06,0x06,0x00,0x07,0x07,0x07,0x00,0x0A,0x0A,0x0A,0x00,0x0B,0x0B,0x0B,
	0x00,0x01,0x01,0x01,0x00,0x05,0x05,0x05,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x00,0x00,0x00,0x00,0x03,0x07,0x0D,0x00,0x0C,0x0F,0x0B,0x00,0x0C,0x0E,0x0B,
	0x00,0x0C,0x06,0x0B,0x00,0x0C,0x07,0x0B,0x00,0x0C,0x03,0x0B,0x00,0x0C,0x08,0x0B,
	0x00,0x0C,0x0D,0x0B,0x00,0x0C,0x04,0x0B,0x00,0x0C,0x09,0x0B,0x00,0x0C,0x05,0x0B,
	0x00,0x0C,0x02,0x0B,0x00,0x0C,0x0B,0x02,0x00,0x08,0x0C,0x02,0x00,0x08,0x0F,0x02,
	0x00,0x03,0x02,0x01,0x00,0x02,0x0F,0x03,0x00,0x0F,0x0E,0x02,0x00,0x0E,0x07,0x0F,
	0x00,0x07,0x06,0x0E,0x00,0x06,0x05,0x07,0x00,0x05,0x00,0x06,0x00,0x00,0x0B,0x05,
	0x00,0x0B,0x0C,0x00,0x00,0x0C,0x0D,0x0B,0x00,0x0D,0x08,0x0C,0x00,0x08,0x09,0x0D,
	0x00,0x09,0x0A,0x08,0x00,0x0A,0x01,0x09,0x00,0x01,0x04,0x0A,0x00,0x04,0x03,0x01
};



static void pengo_decode(void)
{
/*
	the values vary, but the translation mask is always layed out like this:

	  0 1 2 3 4 5 6 7 8 9 a b c d e f
	0 A A A A A A A A B B B B B B B B
	1 A A A A A A A A B B B B B B B B
	2 C C C C C C C C D D D D D D D D
	3 C C C C C C C C D D D D D D D D
	4 A A A A A A A A B B B B B B B B
	5 A A A A A A A A B B B B B B B B
	6 C C C C C C C C D D D D D D D D
	7 C C C C C C C C D D D D D D D D
	8 D D D D D D D D C C C C C C C C
	9 D D D D D D D D C C C C C C C C
	a B B B B B B B B A A A A A A A A
	b B B B B B B B B A A A A A A A A
	c D D D D D D D D C C C C C C C C
	d D D D D D D D D C C C C C C C C
	e B B B B B B B B A A A A A A A A
	f B B B B B B B B A A A A A A A A

	(e.g. 0xc0 is XORed with D)
	therefore in the following tables we only keep track of A, B, C and D.
*/
	static const unsigned char data_xortable[16][4] =
	{
		{ 0x28,0xa0,0x28,0xa0 },	/* ...0...0...0...0 */
		{ 0xa0,0x88,0x88,0xa0 },	/* ...0...0...0...1 */
		{ 0xa0,0x88,0x00,0x28 },	/* ...0...0...1...0 */
		{ 0xa0,0x88,0x88,0xa0 },	/* ...0...0...1...1 */
		{ 0x28,0xa0,0x28,0xa0 },	/* ...0...1...0...0 */
		{ 0x08,0x08,0xa8,0xa8 },	/* ...0...1...0...1 */
		{ 0xa0,0x88,0x00,0x28 },	/* ...0...1...1...0 */
		{ 0x00,0x00,0x00,0x00 },	/* ...0...1...1...1 */
		{ 0xa0,0x88,0x00,0x28 },	/* ...1...0...0...0 */
		{ 0x00,0x00,0x00,0x00 },	/* ...1...0...0...1 */
		{ 0x08,0x20,0xa8,0x80 },	/* ...1...0...1...0 */
		{ 0xa0,0x88,0x00,0x28 },	/* ...1...0...1...1 */
		{ 0x88,0x88,0x28,0x28 },	/* ...1...1...0...0 */
		{ 0x88,0x88,0x28,0x28 },	/* ...1...1...0...1 */
		{ 0x08,0x20,0xa8,0x80 },	/* ...1...1...1...0 */
		{ 0xa0,0x88,0x00,0x28 }		/* ...1...1...1...1 */
	};
	static const unsigned char opcode_xortable[16][4] =
	{
		{ 0xa0,0x88,0x88,0xa0 },	/* ...0...0...0...0 */
		{ 0x28,0xa0,0x28,0xa0 },	/* ...0...0...0...1 */
		{ 0xa0,0x88,0x00,0x28 },	/* ...0...0...1...0 */
		{ 0x08,0x20,0xa8,0x80 },	/* ...0...0...1...1 */
		{ 0x08,0x08,0xa8,0xa8 },	/* ...0...1...0...0 */
		{ 0xa0,0x88,0x00,0x28 },	/* ...0...1...0...1 */
		{ 0xa0,0x88,0x00,0x28 },	/* ...0...1...1...0 */
		{ 0xa0,0x88,0x00,0x28 },	/* ...0...1...1...1 */
		{ 0x88,0x88,0x28,0x28 },	/* ...1...0...0...0 */
		{ 0x88,0x88,0x28,0x28 },	/* ...1...0...0...1 */
		{ 0x08,0x20,0xa8,0x80 },	/* ...1...0...1...0 */
		{ 0xa0,0x88,0x88,0xa0 },	/* ...1...0...1...1 */
		{ 0x08,0x08,0xa8,0xa8 },	/* ...1...1...0...0 */
		{ 0x00,0x00,0x00,0x00 },	/* ...1...1...0...1 */
		{ 0x08,0x20,0xa8,0x80 },	/* ...1...1...1...0 */
		{ 0x08,0x08,0xa8,0xa8 }		/* ...1...1...1...1 */
	};
	int A;


	for (A = 0x0000;A < 0x8000;A++)
	{
		int i,j;
		unsigned char src;


		src = RAM[A];

		/* pick the translation table from bits 0, 4, 8 and 12 of the address */
		i = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);

		/* pick the offset in the table from bits 3 and 5 of the source data */
		j = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80) j = 3 - j;

		/* decode the ROM data */
		RAM[A] = src ^ data_xortable[i][j];

		/* now decode the opcodes */
		ROM[A] = src ^ opcode_xortable[i][j];
	}
}

static void penta_decode(void)
{
/*
	the values vary, but the translation mask is always layed out like this:

	  0 1 2 3 4 5 6 7 8 9 a b c d e f
	0 A A B B A A B B C C D D C C D D
	1 A A B B A A B B C C D D C C D D
	2 E E F F E E F F G G H H G G H H
	3 E E F F E E F F G G H H G G H H
	4 A A B B A A B B C C D D C C D D
	5 A A B B A A B B C C D D C C D D
	6 E E F F E E F F G G H H G G H H
	7 E E F F E E F F G G H H G G H H
	8 H H G G H H G G F F E E F F E E
	9 H H G G H H G G F F E E F F E E
	a D D C C D D C C B B A A B B A A
	b D D C C D D C C B B A A B B A A
	c H H G G H H G G F F E E F F E E
	d H H G G H H G G F F E E F F E E
	e D D C C D D C C B B A A B B A A
	f D D C C D D C C B B A A B B A A

	(e.g. 0xc0 is XORed with H)
	therefore in the following tables we only keep track of A, B, C, D, E, F, G and H.
*/
	static const unsigned char data_xortable[2][8] =
	{
		{ 0xa0,0x82,0x28,0x0a,0x82,0xa0,0x0a,0x28 },	/* ...............0 */
		{ 0x88,0x0a,0x82,0x00,0x88,0x0a,0x82,0x00 }		/* ...............1 */
	};
	static const unsigned char opcode_xortable[8][8] =
	{
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 },	/* ...0...0...0.... */
		{ 0x88,0x88,0x00,0x00,0x88,0x88,0x00,0x00 },	/* ...0...0...1.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },	/* ...0...1...0.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },	/* ...0...1...1.... */
		{ 0x2a,0x08,0x2a,0x08,0x8a,0xa8,0x8a,0xa8 },	/* ...1...0...0.... */
		{ 0x2a,0x08,0x2a,0x08,0x8a,0xa8,0x8a,0xa8 },	/* ...1...0...1.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },	/* ...1...1...0.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 }		/* ...1...1...1.... */
	};
	int A;


	for (A = 0x0000;A < 0x8000;A++)
	{
		int i,j;
		unsigned char src;


		src = RAM[A];

		/* pick the translation table from bit 0 of the address */
		i = A & 1;

		/* pick the offset in the table from bits 1, 3 and 5 of the source data */
		j = ((src >> 1) & 1) + (((src >> 3) & 1) << 1) + (((src >> 5) & 1) << 2);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80) j = 7 - j;

		/* decode the ROM data */
		RAM[A] = src ^ data_xortable[i][j];

		/* now decode the opcodes */
		/* pick the translation table from bits 4, 8 and 12 of the address */
		i = ((A >> 4) & 1) + (((A >> 8) & 1) << 1) + (((A >> 12) & 1) << 2);
		ROM[A] = src ^ opcode_xortable[i][j];
	}
}



static int hiload(void)
{
	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0x8840],"\xd0\x07",2) == 0 &&
			memcmp(&RAM[0x8858],"\xd0\x07",2) == 0 &&
			memcmp(&RAM[0x880c],"\xd0\x07",2) == 0)	/* high score */
	{
		void *f;


		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread(f,&RAM[0x8840],6*5);
			RAM[0x880c] = RAM[0x8858];
			RAM[0x880d] = RAM[0x8859];
			osd_fclose(f);
		}

		return 1;
	}
	else return 0;	/* we can't load the hi scores yet */
}

static int pengoa_hiload(void)
{
	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0x8840],"\x00\x00\x01\x55\x55\x55",6) == 0 &&
			memcmp(&RAM[0x8858],"\x00\x00",2) == 0 &&
			memcmp(&RAM[0x880c],"\x00\x00",2) == 0)	/* hi-score */
	{
		void *f;


		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread(f,&RAM[0x8840],6*5);
			RAM[0x880c] = RAM[0x8858];
			RAM[0x880d] = RAM[0x8859];
			osd_fclose(f);
		}

		return 1;
	}
	else return 0;	/* we can't load the hi scores yet */
}

static void hisave(void)
{
	void *f;


	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
		osd_fwrite(f,&RAM[0x8840],6*5);
		osd_fclose(f);
	}
}



struct GameDriver pengo_driver =
{
	"Pengo",
	"pengo",
	"Allard van der Bas (original code)\nNicola Salmoria (MAME driver)\nSergio Munoz (color and sound info)",
	&machine_driver,

	pengo_rom,
	0, pengo_decode,
	0,
	sound_prom,	/* sound_prom */

	input_ports,

	color_prom, 0, 0,
	ORIENTATION_ROTATE_90,

	hiload, hisave
};

struct GameDriver pengoa_driver =
{
	"Pengo (alternate)",
	"pengoa",
	"Allard van der Bas (original code)\nNicola Salmoria (MAME driver)\nSergio Munoz (color and sound info)\nGerrit Van Goethem (high score fix)",
	&machine_driver,

	pengoa_rom,
	0, 0,
	0,
	sound_prom,	/* sound_prom */

	input_ports,

	color_prom, 0, 0,
	ORIENTATION_ROTATE_90,

	pengoa_hiload, hisave
};

struct GameDriver penta_driver =
{
	"Penta",
	"penta",
	"Allard van der Bas (original code)\nNicola Salmoria (MAME driver)\nSergio Munoz (color and sound info)",
	&machine_driver,

	penta_rom,
	0, penta_decode,
	0,
	sound_prom,	/* sound_prom */

	input_ports,

	color_prom, 0, 0,
	ORIENTATION_ROTATE_90,

	hiload, hisave
};

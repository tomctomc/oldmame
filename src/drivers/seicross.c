/***************************************************************************

Seicross memory map (preliminary)

0000-77ff ROM
7800-7fff RAM
9000-93ff videoram
9c00-9fff colorram

Read:
A000      Joystick + Players start button
A800      player #2 controls + coin + ?
B000      test switches
B800      watchdog reset?

Write:
8820-887f Sprite ram
9800-981f Scroll control
9880-989f ? (always 0?)

I/O ports:
0         8910 control
1         8910 write
4         8910 read

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



int seicross_protection_r(int offset);

extern unsigned char *seicross_row_scroll;
void seicross_colorram_w(int offset,int data);
void seicross_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
void seicross_vh_screenrefresh(struct osd_bitmap *bitmap);



static int seicross_portB_r(int offset)
{
	return readinputport(5);
}

int seicross_protection_r(int offset)
{
        return offset+2;
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x77ff, MRA_ROM },
/* there is probably a microcontroller on board, which writes to the range 7810-781f */
/* 7816 and 7817 are read if you keep F1 pressed */
	{ 0x7814, 0x7815, seicross_protection_r },	/* ??? */
	{ 0x7810, 0x7810, MRA_NOP },
	{ 0x7816, 0x7816, input_port_3_r },
	{ 0x7817, 0x7817, input_port_4_r },
	{ 0x7800, 0x7fff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },	/* video RAM */
	{ 0x9800, 0x981f, MRA_RAM },
	{ 0x9c00, 0x9fff, MRA_RAM },	/* color RAM */
	{ 0xa000, 0xa000, input_port_0_r },	/* IN0 */
	{ 0xa800, 0xa800, input_port_1_r },	/* IN1 */
	{ 0xb000, 0xb000, input_port_2_r },	/* test */
	{ 0xb800, 0xb800, watchdog_reset_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x77ff, MWA_ROM },
	{ 0x7800, 0x7fff, MWA_RAM },
	{ 0x8820, 0x887f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
	{ 0x9800, 0x981f, MWA_RAM, &seicross_row_scroll },
	{ 0x9880, 0x989f, MWA_RAM },	/* ? */
	{ 0x9c00, 0x9fff, seicross_colorram_w, &colorram },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x04, 0x04, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( input_ports )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START      /* Test */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, 0, "Port Test", OSD_KEY_F1, 0, 0 )
/* actually it looks like bit 1 is not a test switch, but an error report from */
/* a microcontroller on board. */
	PORT_BITX(0x02, IP_ACTIVE_HIGH, 0, "Test 2", OSD_KEY_F2, 0, 0 )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x01, 0x00, "Unknown 1", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x01, "On" )
	PORT_DIPNAME( 0x02, 0x00, "Unknown 2", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x02, "On" )
	PORT_DIPNAME( 0x04, 0x00, "Unknown 3", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x04, "On" )
	PORT_DIPNAME( 0x08, 0x00, "Unknown 4", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x08, "On" )
	PORT_DIPNAME( 0x10, 0x00, "Unknown 5", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x10, "On" )
	PORT_DIPNAME( 0x20, 0x00, "Unknown 6", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x20, "On" )
	PORT_DIPNAME( 0x40, 0x00, "Unknown 7", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x40, "On" )
	PORT_DIPNAME( 0x80, 0x00, "Unknown 8", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x80, "On" )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x01, 0x00, "Unknown 9", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x01, "On" )
	PORT_DIPNAME( 0x02, 0x00, "Unknown a", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x02, "On" )
	PORT_DIPNAME( 0x04, 0x00, "Unknown b", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x04, "On" )
	PORT_DIPNAME( 0x08, 0x00, "Unknown c", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x08, "On" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x1f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x20, 0x20, "Debug Mode", IP_KEY_NONE )
	PORT_DIPSETTING(    0x20, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x40, 0x40, "Demo Mode", IP_KEY_NONE )
	PORT_DIPSETTING(    0x40, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x80, 0x00, "SW7B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x80, "1" )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes are packed in one byte */
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 16 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes are packed in one byte */
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 17*8+0, 17*8+1, 17*8+2, 17*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,   0, 16 },
	{ 1, 0x1000, &charlayout,   0, 16 },
	{ 1, 0x2000, &spritelayout, 0, 16 },
	{ 1, 0x3000, &spritelayout, 0, 16 },
	{ -1 } /* end of array */
};



static unsigned char color_prom[] =
{
	/* sz73 */
	0x00,0xFE,0xA4,0x52,0x00,0x17,0x05,0x03,0x00,0x2F,0x1F,0x0F,0x00,0x22,0x19,0x10,
	0x00,0x2B,0x20,0x18,0x00,0xD8,0xD1,0x80,0x00,0xE9,0xD8,0xC8,0x00,0x6F,0xD8,0x80,
	/* sz74 */
	0x00,0x1D,0x13,0x11,0x00,0x55,0x4C,0x42,0x00,0x60,0x58,0x50,0x00,0x25,0x1B,0x12,
	0x00,0x1F,0xE9,0xD8,0x00,0x0F,0xD8,0x80,0x00,0x84,0x83,0x42,0x00,0x1D,0x13,0x0A
};



static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	1536000,	/* 1.536 MHz ?? */
	{ 255 },
	{ 0 },
	{ seicross_portB_r },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz */
			0,
			readmem,writemem,readport,writeport,
			interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	64,16*4,
	seicross_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	seicross_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( seicross_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "smc1", 0x0000, 0x1000, 0x05933acb )
	ROM_LOAD( "smc2", 0x1000, 0x1000, 0xc2e976d5 )
	ROM_LOAD( "smc3", 0x2000, 0x1000, 0x2a0a1a9a )
	ROM_LOAD( "smc4", 0x3000, 0x1000, 0xed8b26a7 )
	ROM_LOAD( "smc5", 0x4000, 0x1000, 0x17996809 )
	ROM_LOAD( "smc6", 0x5000, 0x1000, 0x4ef8da3e )
	ROM_LOAD( "smc7", 0x6000, 0x1000, 0xd30a5b96 )
	ROM_LOAD( "smc8", 0x7000, 0x0800, 0xd8629306 )

	ROM_REGION(0x4000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "smcc", 0x0000, 0x1000, 0x7c5e0864 )
	ROM_LOAD( "smcd", 0x1000, 0x1000, 0x7e8ee1ca )
	ROM_LOAD( "smca", 0x2000, 0x1000, 0x006b5bd9 )
	ROM_LOAD( "smcb", 0x3000, 0x1000, 0x44a1bb81 )
ROM_END

ROM_START( friskyt_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "ftom.01",   0x0000, 0x1000, 0xb470690c )
	ROM_LOAD( "ftom.02",   0x1000, 0x1000, 0x3ed50877 )
	ROM_LOAD( "ftom.03",   0x2000, 0x1000, 0xf7a4dfdc )
	ROM_LOAD( "ftom.04",   0x3000, 0x1000, 0xa3524850 )
	ROM_LOAD( "ftom.05",   0x4000, 0x1000, 0x5e0a3478 )
	ROM_LOAD( "ftom.06",   0x5000, 0x1000, 0xddcd90b3 )
	ROM_LOAD( "ftom.07",   0x6000, 0x1000, 0x7a9bb4b9 )
	ROM_LOAD( "ft8_8.rom", 0x7000, 0x0800, 0x53d8d1b2 )

	ROM_REGION(0x4000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ftom.11", 0x0000, 0x1000, 0xd04cb5a0 )
	ROM_LOAD( "ftom.12", 0x1000, 0x1000, 0x72890739 )
	ROM_LOAD( "ftom.09", 0x2000, 0x1000, 0xff45c4cd )
	ROM_LOAD( "ftom.10", 0x3000, 0x1000, 0x9450edb0 )
ROM_END


struct GameDriver seicross_driver =
{
	"Seicross",
	"seicross",
	"Mirko Buffoni\nNicola Salmoria",
	&machine_driver,

	seicross_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	input_ports,

	color_prom, 0, 0,
	ORIENTATION_ROTATE_90,

	0, 0
};

struct GameDriver friskyt_driver =
{
	"Frisky Tom",
	"friskyt",
	"Mirko Buffoni\nNicola Salmoria",
	&machine_driver,

	friskyt_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	input_ports,

	color_prom, 0, 0,	/* wrong! it's the Seicross one */
	ORIENTATION_DEFAULT,

	0, 0
};

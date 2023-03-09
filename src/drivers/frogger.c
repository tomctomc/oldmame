/***************************************************************************

Frogger memory map (preliminary)

0000-3fff ROM
8000-87ff RAM
a800-abff Video RAM
b000-b0ff Object RAM
b000-b03f screen attributes
b040-b05f sprites
b060-b0ff unused?

read:
8800      Watchdog Reset
e000      IN0
e002      IN1
e004      IN2

*
 * IN0 (all bits are inverted)
 * bit 7 : COIN 1
 * bit 6 : COIN 2
 * bit 5 : LEFT player 1
 * bit 4 : RIGHT player 1
 * bit 3 : SHOOT 1 player 1
 * bit 2 : CREDIT
 * bit 1 : SHOOT 2 player 1
 * bit 0 : UP player 2 (TABLE only)
 *
*
 * IN1 (all bits are inverted)
 * bit 7 : START 1
 * bit 6 : START 2
 * bit 5 : LEFT player 2 (TABLE only)
 * bit 4 : RIGHT player 2 (TABLE only)
 * bit 3 : SHOOT 1 player 2 (TABLE only)
 * bit 2 : SHOOT 2 player 2 (TABLE only)
 * bit 1 :\ nr of lives
 * bit 0 :/ 00 = 3  01 = 5  10 = 7  11 = 256
*
 * IN2 (all bits are inverted)
 * bit 7 : unused
 * bit 6 : DOWN player 1
 * bit 5 : unused
 * bit 4 : UP player 1
 * bit 3 : COCKTAIL or UPRIGHT cabinet (0 = UPRIGHT)
 * bit 2 :\ coins per play
 * bit 1 :/
 * bit 0 : DOWN player 2 (TABLE only)
 *

write:
b808      interrupt enable
b80c      screen horizontal flip
b810      screen vertical flip
b818      coin counter 1
b81c      coin counter 2
d000      To AY-3-8910 port A (commands for the second Z80)
d002      trigger interrupt on sound CPU


SOUND BOARD:
0000-17ff ROM
4000-43ff RAM

I/0 ports:
read:
40      8910 #1  read

write
40      8910 #1  write
80      8910 #1  control

interrupts:
interrupt mode 1 triggered by the main CPU

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *frogger_attributesram;
void frogger_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
void frogger_attributes_w(int offset,int data);
void frogger_vh_screenrefresh(struct osd_bitmap *bitmap);
void frogger2_vh_screenrefresh(struct osd_bitmap *bitmap);

int frogger_portB_r(int offset);
void frogger_sh_irqtrigger_w(int offset,int data);
void frogger2_sh_irqtrigger_w(int offset,int data);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8800, 0x8800, watchdog_reset_r },
	{ 0xa800, 0xabff, MRA_RAM },	/* video RAM */
	{ 0xb000, 0xb05f, MRA_RAM },	/* screen attributes, sprites */
	{ 0xe000, 0xe000, input_port_0_r },	/* IN0 */
	{ 0xe002, 0xe002, input_port_1_r },	/* IN1 */
	{ 0xe004, 0xe004, input_port_2_r },	/* IN2 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xa800, 0xabff, videoram_w, &videoram, &videoram_size },
	{ 0xb000, 0xb03f, frogger_attributes_w, &frogger_attributesram },
	{ 0xb040, 0xb05f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xb808, 0xb808, interrupt_enable_w },
	{ 0xd000, 0xd000, soundlatch_w },
	{ 0xd002, 0xd002, frogger_sh_irqtrigger_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress frogger2_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },	/* video RAM */
	{ 0x9800, 0x985f, MRA_RAM },	/* screen attributes, sprites */
	{ 0xa000, 0xa000, input_port_0_r },	/* IN0 */
	{ 0xa800, 0xa800, input_port_1_r },	/* IN1 */
	{ 0xb000, 0xb000, input_port_2_r },	/* IN2 */
	{ 0xb800, 0xb800, watchdog_reset_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress frogger2_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
	{ 0x9800, 0x983f, frogger_attributes_w, &frogger_attributesram },
	{ 0x9840, 0x985f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xa800, 0xa800, soundlatch_w },
	{ 0xb000, 0xb000, interrupt_enable_w },
	{ 0xb001, 0xb001, frogger2_sh_irqtrigger_w },
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x17ff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x17ff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ -1 }	/* end of table */
};



static struct IOReadPort sound_readport[] =
{
	{ 0x40, 0x40, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x80, 0x80, AY8910_control_port_0_w },
	{ 0x40, 0x40, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* 1P shoot2 - unused */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* 1P shoot1 - unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x03, 0x00, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_BITX( 0,       0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "256", IP_KEY_NONE, IP_JOY_NONE, 0 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* 2P shoot2 - unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* 2P shoot1 - unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_DIPNAME( 0x06, 0x00, "Coinage", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "A 2/1 B 2/1 C 2/1" )
	PORT_DIPSETTING(    0x04, "A 2/1 B 1/3 C 2/1" )
	PORT_DIPSETTING(    0x00, "A 1/1 B 1/1 C 1/1" )
	PORT_DIPSETTING(    0x06, "A 1/1 B 1/6 C 1/1" )
	PORT_DIPNAME( 0x08, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x08, "Cocktail" )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( frogger2_input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN3 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_DIPNAME( 0xc0, 0xc0, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0x40, "7" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "256", IP_KEY_NONE, IP_JOY_NONE, 0 )

	PORT_START	/* IN2 */
	PORT_DIPNAME( 0x01, 0x01, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x01, "Upright" )
	PORT_DIPSETTING(    0x00, "Cocktail" )
	PORT_DIPNAME( 0x06, 0x06, "Coinage", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "A 2/1 B 2/1 C 2/1" )
	PORT_DIPSETTING(    0x04, "A 2/1 B 1/3 C 2/1" )
	PORT_DIPSETTING(    0x06, "A 1/1 B 1/1 C 1/1" )
	PORT_DIPSETTING(    0x00, "A 1/1 B 1/6 C 1/1" )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 256*8*8, 0 },	/* the two bitplanes are separated */
	{ 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 64*16*16, 0 },	/* the two bitplanes are separated */
	{ 23*8, 22*8, 21*8, 20*8, 19*8, 18*8, 17*8, 16*8,
			7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	32*8	/* every sprite takes 32 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,     0, 16 },
	{ 1, 0x0000, &spritelayout,   0,  8 },
	{ -1 } /* end of array */
};



static unsigned char color_prom[] =
{
	0x00,0x07,0xC0,0xF6,0x00,0xF6,0x5E,0x5C,0x00,0xF0,0x3C,0xD7,0x00,0xC0,0xC4,0x07,
	0x00,0x31,0x17,0xF0,0x00,0x31,0xC7,0x3F,0x00,0xF6,0x07,0x31,0x00,0x3F,0x07,0xC4
};



static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	14318000/8,	/* 1.78975 Mhz */
	{ 0x60ff },
	{ soundlatch_r },
	{ frogger_portB_r },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			0,
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			2,	/* memory region #2 */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 2*8, 30*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	32,64,
	frogger_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	frogger_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver frogger2_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			0,
			frogger2_readmem,frogger2_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			2,	/* memory region #2 */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 2*8, 30*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	32,64,
	frogger_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	frogger2_vh_screenrefresh,

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

ROM_START( frogger_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "frogger.ic5", 0x0000, 0x1000, 0xb77be5cb )
	ROM_LOAD( "frogger.ic6", 0x1000, 0x1000, 0x02dc7158 )
	ROM_LOAD( "frogger.ic7", 0x2000, 0x1000, 0x71e62ce0 )
	ROM_LOAD( "frogger.ic8", 0x3000, 0x1000, 0x568b11cd )

	ROM_REGION(0x1000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "frogger.606", 0x0000, 0x0800, 0xd04c173a )
	ROM_LOAD( "frogger.607", 0x0800, 0x0800, 0xb474d87c )

	ROM_REGION(0x10000)	/* 64k for the audio CPU */
	ROM_LOAD( "frogger.608", 0x0000, 0x0800, 0x57851ff5 )
	ROM_LOAD( "frogger.609", 0x0800, 0x0800, 0xd77b3859 )
	ROM_LOAD( "frogger.610", 0x1000, 0x0800, 0x7ec0f39e )
ROM_END

ROM_START( frogsega_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "frogger.ic5", 0x0000, 0x1000, 0x65a3e115 )
	ROM_LOAD( "frogger.ic6", 0x1000, 0x1000, 0x039a96c8 )
	ROM_LOAD( "frogger.ic7", 0x2000, 0x1000, 0xb48737eb )

	ROM_REGION(0x1000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "frogger.606", 0x0000, 0x0800, 0xd04c173a )
	ROM_LOAD( "frogger.607", 0x0800, 0x0800, 0xb474d87c )

	ROM_REGION(0x10000)	/* 64k for the audio CPU */
	ROM_LOAD( "frogger.608", 0x0000, 0x0800, 0x57851ff5 )
	ROM_LOAD( "frogger.609", 0x0800, 0x0800, 0xd77b3859 )
	ROM_LOAD( "frogger.610", 0x1000, 0x0800, 0x7ec0f39e )
ROM_END

ROM_START( frogger2_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "epr-1031.15", 0x0000, 0x1000, 0xea2517c3 )
	ROM_LOAD( "epr-1032.16", 0x1000, 0x1000, 0x1ccaed52 )
	ROM_LOAD( "epr-1033.33", 0x2000, 0x1000, 0xd495e393 )
	ROM_LOAD( "epr-1034.34", 0x3000, 0x1000, 0xcc74908a )

	ROM_REGION(0x1000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "epr-1036.1k", 0x0000, 0x0800, 0xf8175729 )
	ROM_LOAD( "epr-1037.1h", 0x0800, 0x0800, 0xb474d87c )

	ROM_REGION(0x10000)	/* 64k for the audio CPU */
	ROM_LOAD( "epr-1082.42", 0x0000, 0x1000, 0x4aeb27b7 )
	ROM_LOAD( "epr-1035.43", 0x1000, 0x0800, 0xed40329a )
ROM_END



static void frogger_decode(void)
{
	int A;
	unsigned char *RAM;


	/* the first ROM of the second CPU has data lines D0 and D1 swapped. Decode it. */
	RAM = Machine->memory_region[Machine->drv->cpu[1].memory_region];
	for (A = 0;A < 0x0800;A++)
		RAM[A] = (RAM[A] & 0xfc) | ((RAM[A] & 1) << 1) | ((RAM[A] & 2) >> 1);

	/* likewise, the first gfx ROM has data lines D0 and D1 swapped. Decode it. */
	RAM = Machine->memory_region[1];
	for (A = 0;A < 0x0800;A++)
		RAM[A] = (RAM[A] & 0xfc) | ((RAM[A] & 1) << 1) | ((RAM[A] & 2) >> 1);
}

static void frogger2_decode(void)
{
	int A;
	unsigned char *RAM;


	/* the first ROM of the second CPU has data lines D0 and D1 swapped. Decode it. */
	RAM = Machine->memory_region[Machine->drv->cpu[1].memory_region];
	for (A = 0;A < 0x1000;A++)
		RAM[A] = (RAM[A] & 0xfc) | ((RAM[A] & 1) << 1) | ((RAM[A] & 2) >> 1);
}



static int hiload(void)
{
	/* get RAM pointer (this game is multiCPU, we can't assume the global */
	/* RAM pointer is pointing to the right place) */
	unsigned char *RAM = Machine->memory_region[0];


	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0x83f1],"\x63\x04",2) == 0 &&
			memcmp(&RAM[0x83f9],"\x27\x01",2) == 0)
	{
		void *f;


		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread(f,&RAM[0x83f1],2*5);
			RAM[0x83ef] = RAM[0x83f1];
			RAM[0x83f0] = RAM[0x83f2];
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
		osd_fwrite(f,&RAM[0x83f1],2*5);
		osd_fclose(f);
	}
}



struct GameDriver frogger_driver =
{
	"Frogger",
	"frogger",
	"Robert Anschuetz\nNicola Salmoria\nMirko Buffoni\nGerald Vanderick (color info)\nMarco Cassili",
	&machine_driver,

	frogger_rom,
	frogger_decode, 0,
	0,
	0,	/* sound_prom */

	input_ports,

	color_prom, 0, 0,
	ORIENTATION_DEFAULT,

	hiload, hisave
};

struct GameDriver frogsega_driver =
{
	"Frogger (alternate version)",
	"frogsega",
	"Robert Anschuetz\nNicola Salmoria\nMirko Buffoni\nGerald Vanderick (color info)\nMarco Cassili",
	&machine_driver,

	frogsega_rom,
	frogger_decode, 0,
	0,
	0,	/* sound_prom */

	input_ports,

	color_prom, 0, 0,
	ORIENTATION_DEFAULT,

	hiload, hisave
};

struct GameDriver frogger2_driver =
{
	"Frogger (alternate version #2)",
	"frogger2",
	"Robert Anschuetz\nNicola Salmoria\nMirko Buffoni\nGerald Vanderick (color info)\nMarco Cassili",
	&frogger2_machine_driver,

	frogger2_rom,
	frogger2_decode, 0,
	0,
	0,	/* sound_prom */

	frogger2_input_ports,

	color_prom, 0, 0,
	ORIENTATION_DEFAULT,

	hiload, hisave
};

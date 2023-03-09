/***************************************************************************

Space Firebird memory map (preliminary)

  Memory Map figured out by Chris Hardy (chrish@kcbbs.gen.nz), Paul Johnson and Andy Clark
  Mame driver by Chris Hardy

  Schematics scanned and provided by James Twine
  Thanks to Gary Walton for lending me his REAL Space Firebird

  The way the sprites are drawn ARE correct. They are identical to the real
  pcb.

TODO
	- Add "Red" flash when you die.
	- Add Starfield. It is NOT a Galaxians starfield
	-

0000-3FFF ROM		Code
8000-83FF RAM		Char/Sprite RAM
C000-C7FF RAM		Game ram

IO Ports

IN:
Port 0 - Player 1 I/O
Port 1 - Player 2 I/O
Port 2 - P1/P2 Start Test and Service
Port 3 - Dipswitch


OUT:
Port 0 - RV,VREF and CREF
Port 1 - Comms to the Sound card (-> 8212)
Port 2 - Video contrast values (used by sound board only)
Port 3 - Unused


IN:
Port 0

   bit 0 = Player 1 Right
   bit 1 = Player 1 Left
   bit 2 = unused
   bit 3 = unused
   bit 4 = Player 1 Escape
   bit 5 = unused
   bit 6 = unused
   bit 7 = Player 1 Fire

Port 1

   bit 0 = Player 2 Right
   bit 1 = Player 2 Left
   bit 2 = unused
   bit 3 = unused
   bit 4 = Player 2 Escape
   bit 5 = unused
   bit 6 = unused
   bit 7 = Player 2 Fire

Port 2

   bit 0 = unused
   bit 1 = unused
   bit 2 = Start 1 Player game
   bit 3 = Start 2 Players game
   bit 4 = unused
   bit 5 = unused
   bit 6 = Test switch
   bit 7 = Coin and Service switch

Port 3

   bit 0 = Dipswitch 1
   bit 1 = Dipswitch 2
   bit 2 = Dipswitch 3
   bit 3 = Dipswitch 4
   bit 4 = Dipswitch 5
   bit 5 = Dipswitch 6
   bit 6 = unused (Debug switch - Code jumps to $3800 on reset if on)
   bit 7 = unused

OUT:
Port 0 - Video

   bit 0 = Screen flip. (RV)
   bit 1 = unused
   bit 2 = unused
   bit 3 = unused
   bit 4 = unused
   bit 5 = Char/Sprite Bank switch (VREF)
   bit 6 = CREF
   bit 7 = unused

Port 1
	8 bits gets "sent" to a 8212 chip

Port 2 - Video control

These are passed to the sound board and are used to produce a
red flash effect when you die.

   bit 0 = CONT R
   bit 1 = CONT G
   bit 2 = CONT B
   bit 3 = ALRD
   bit 4 = ALBU
   bit 5 = unused
   bit 6 = unused
   bit 7 = ALBA


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


void spacefb_vh_screenrefresh(struct osd_bitmap *bitmap);
void spacefb_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);

void spacefb_port_0_w(int offset,int data);
void spacefb_port_1_w(int offset,int data);
void spacefb_port_2_w(int offset,int data);
void spacefb_port_3_w(int offset,int data);

int spacefb_interrupt(void);

static struct MemoryReadAddress readmem[] =
{
	{ 0xC000, 0xC7FF, MRA_RAM },
	{ 0x8000, 0x83FF, MRA_RAM },
	{ 0x0000, 0x3FFF, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0xC000, 0xC7FF, MWA_RAM },
	{ 0x8000, 0x83FF, videoram_w, &videoram, &videoram_size },
	{ 0x0000, 0x3FFF, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r }, /* IN 0 */
	{ 0x01, 0x01, input_port_1_r }, /* IN 1 */
	{ 0x02, 0x02, input_port_2_r }, /* Coin - Start */
	{ 0x03, 0x03, input_port_3_r }, /* DSW0 */
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, spacefb_port_0_w },
	{ 0x01, 0x01, spacefb_port_1_w },
	{ 0x02, 0x02, spacefb_port_2_w },
	{ 0x03, 0x03, spacefb_port_3_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( input_ports )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START      /* Coin - Start */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Test ? */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, "Coinage", IP_KEY_NONE )
	PORT_DIPSETTING(    0x04, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x00, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x0c, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x08, "1 Coin/3 Credits" )
	PORT_DIPNAME( 0x10, 0x00, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x10, "8000" )
	PORT_DIPNAME( 0x20, 0x20, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x20, "Upright" )
	PORT_DIPSETTING(    0x00, "Cocktail" )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 256*8*8 },	/* the two bitplanes are separated */
	{ 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	8*8	/* every char takes 8 consecutive bytes */
};
/*
 * The bullests are stored in a 256x4bit PROM but the .bin file is
 * 256*8bit
 */
static struct GfxLayout bulletlayout =
{
	4,8,	/* 4*4 characters */
	256/4,	/* 64 characters */
	1,	/* 1 bits per pixel */
	{ 0 },	/* the two bitplanes are separated */
	{ 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	4*8	/* every char takes 4 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,   0, 8 },
	{ 1, 0x1000, &bulletlayout, 0, 8 },
	{ -1 } /* end of array */
};


static unsigned char colorprom[] =
{
	0x00,0x17,0x27,0xD0,0x00,0xA4,0xC0,0x16,0x00,0x07,0xC7,0x37,0x00,0x3F,0xD8,0x07,
	0x00,0x3B,0xC0,0x16,0x00,0xDB,0xC0,0xC7,0x00,0x07,0xC7,0x37,0x00,0x3F,0xD8,0x07
};

static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz? */
			0,
			readmem,writemem,readport,writeport,
			spacefb_interrupt,2 /* two int's per frame */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
  	32*8, 32*8, { 2*8, 32*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,

	32,32,
	spacefb_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	spacefb_vh_screenrefresh,

	/* sound hardware */
	0,
	0,
	0,
	0
};



ROM_START( spacefb_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "5e.cpu", 0x0000, 0x0800, 0xc1d7a67b )         /* Code */
	ROM_LOAD( "5f.cpu", 0x0800, 0x0800, 0x5f06218a )
	ROM_LOAD( "5h.cpu", 0x1000, 0x0800, 0x78ffa2af )
	ROM_LOAD( "5i.cpu", 0x1800, 0x0800, 0xf95a50b8 )
	ROM_LOAD( "5j.cpu", 0x2000, 0x0800, 0x30672a65 )
	ROM_LOAD( "5k.cpu", 0x2800, 0x0800, 0xeb8086de )
	ROM_LOAD( "5m.cpu", 0x3000, 0x0800, 0x2c696757 )
	ROM_LOAD( "5n.cpu", 0x3800, 0x0800, 0x79e64f86 )

	ROM_REGION(0x1100)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "6k.vid", 0x0000, 0x0800, 0x5076d18e )
	ROM_LOAD( "5k.vid", 0x0800, 0x0800, 0xe945e879 )
	ROM_LOAD( "4i.vid", 0x1000, 0x0100, 0x75c90c07 )
ROM_END



static int hiload(void)
{
	/* get RAM pointer (this game is multiCPU, we can't assume the global */
	/* RAM pointer is pointing to the right place) */
	unsigned char *RAM = Machine->memory_region[0];
	unsigned char *from, *to;
	int i, index, digit, started;

	/* check if the hi score table has already been initialized */
	if (RAM[0xc0db] == 0xc1 &&	/* check that the scores have been zeroed */
		videoram[0x299] == 0x0f && /* and that the videoram has been 'cleared' */
		videoram[0x29a] == 0x0f &&
		videoram[0x29b] == 0x0f &&
		videoram[0x299 + 47] == 0x0f &&	/* all the way down */
		videoram[0x299 + 48] == 0x0f &&
		videoram[0x299 + 49] == 0x0f &&
		videoram[0x299 + 50] == 0x05 &&
		videoram[0x299 + 59] == 0x05)
	{
		void *f;

		if ((f = osd_fopen(Machine->gamedrv->name, 0,
						   OSD_FILETYPE_HIGHSCORE, 0)) != 0)
		{
			osd_fread(f, &RAM[0xc0a0], 3 * 10);
			osd_fseek(f, 0, SEEK_SET);
			osd_fread(f, &RAM[0xc0e0], 3);
			osd_fclose(f);

			from = &RAM[0xc0a0];
			index = 0x299;

			for (i = 0; i < 10; i++)
			{
				started = 0;

				if (!(digit = ((*from & 0xf0) >> 4)) && !started) digit = 10; else started = 1;
				videoram_w(index++, digit + 5);

				if (!(digit = (*from++ & 0x0f)) && !started) digit = 10; else started = 1;
				videoram_w(index++, digit + 5);

				if (!(digit = ((*from & 0xf0) >> 4)) && !started) digit = 10; else started = 1;
				videoram_w(index++, digit + 5);

				if (!(digit = (*from++ & 0x0f)) && !started) digit = 10; else started = 1;
				videoram_w(index++, digit + 5);

				if (!(digit = ((*from++ & 0xf0) >> 4)) && !started) digit = 10; else started = 1;
				videoram_w(index++, digit + 5);
			}

			from = &RAM[0xc0a0];
			to = &RAM[0xc773];
			index = 0x251;

			videoram_w(index++, *to++ = ((*from & 0xf0) >> 4) + 5);
			videoram_w(index++, *to++ = (*from++ & 0x0f) + 5);
			videoram_w(index++, *to++ = ((*from & 0xf0) >> 4) + 5);
			videoram_w(index++, *to++ = (*from++ & 0x0f) + 5);
			videoram_w(index++, *to++ = ((*from++ & 0xf0) >> 4) + 5);
		}

		return 1;
	}
	else return 0;	/* we can't load the hi scores yet */
}

static void hisave(void)
{
	/* get RAM pointer (this game is multiCPU, we can't assume the global */
	/* RAM pointer is pointing to the right place) */
	unsigned char *RAM = Machine->memory_region[0];
	void *f;

	if ((f = osd_fopen(Machine->gamedrv->name, 0,
					   OSD_FILETYPE_HIGHSCORE, 1)) != 0)
    {
		osd_fwrite(f, &RAM[0xc0a0], 3 * 10);
		osd_fclose(f);
	}
}



struct GameDriver spacefb_driver =
{
	"Space Firebird",
	"spacefb",
	"Chris Hardy\nAndy Clark\nPaul Johnson\nChris Moore (high score save)\nMarco Cassili",
	&machine_driver,

	spacefb_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	input_ports,

	colorprom, 0, 0,
	ORIENTATION_DEFAULT,

	hiload, hisave
};


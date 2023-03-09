/***************************************************************************

Millipede memory map (preliminary)

0400-040F       POKEY 1
0800-080F       POKEY 2
1000-13BF       SCREEN RAM (8x8 TILES, 32x30 SCREEN)
13C0-13CF       SPRITE IMAGE OFFSETS
13D0-13DF       SPRITE HORIZONTAL OFFSETS
13E0-13EF       SPRITE VERTICAL OFFSETS
13F0-13FF       SPRITE COLOR OFFSETS

2000            BIT 1-4 trackball
                BIT 5 IS P1 FIRE
                BIT 6 IS P1 START
                BIT 7 IS SERVICE

2001            BIT 1-4 trackball
                BIT 5 IS P2 FIRE
                BIT 6 IS P2 START
                BIT 7,8 (?)

2010            BIT 1 IS P1 RIGHT
                BIT 2 IS P1 LEFT
                BIT 3 IS P1 DOWN
                BIT 4 IS P1 UP
                BIT 5 IS SLAM, LEFT COIN, AND UTIL COIN
                BIT 6,7 (?)
                BIT 8 IS RIGHT COIN

2480-249F       COLOR RAM
2600            INTERRUPT ACKNOWLEDGE
2680            CLEAR WATCHDOG
4000-7FFF       GAME CODE

Known issues:

* Color ram isn't emulated. I think it fills 2480-249f.

* The dipswitches under $2000 aren't fully emulated. There must be some sort of
trick to reading them properly.

*************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *milliped_paletteram;

void milliped_vh_convert_color_prom (unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
void milliped_paletteram_w(int offset,int data);
void milliped_vh_screenrefresh(struct osd_bitmap *bitmap);

int milliped_IN0_r(int offset);	/* JB 971220 */
int milliped_IN1_r(int offset);	/* JB 971220 */



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x0200, MRA_RAM },
	{ 0x0400, 0x040f, pokey1_r },
	{ 0x0800, 0x080f, pokey2_r },
	{ 0x1000, 0x13ff, MRA_RAM },
	{ 0x2000, 0x2000, milliped_IN0_r },	/* JB 971220 */
	{ 0x2001, 0x2001, milliped_IN1_r },	/* JB 971220 */
	{ 0x2010, 0x2010, input_port_2_r },
	{ 0x2011, 0x2011, input_port_3_r },
	{ 0x4000, 0x7fff, MRA_ROM },
	{ 0xf000, 0xffff, MRA_ROM },		/* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};



static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0200, MWA_RAM },
	{ 0x0400, 0x040f, pokey1_w },
	{ 0x0800, 0x080f, pokey2_w },
	{ 0x1000, 0x13ff, videoram_w, &videoram, &videoram_size },
	{ 0x13c0, 0x13ff, MWA_RAM, &spriteram },
	{ 0x2480, 0x249f, milliped_paletteram_w, &milliped_paletteram },
	{ 0x4000, 0x73ff, MWA_ROM },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( input_ports )
	PORT_START	/* IN0 $2000 */	/* see port 6 for x trackball */
	PORT_DIPNAME (0x03, 0x00, "Language", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "English" )
	PORT_DIPSETTING (   0x01, "German" )
	PORT_DIPSETTING (   0x02, "French" )
	PORT_DIPSETTING (   0x03, "Spanish" )
	PORT_DIPNAME (0x0c, 0x04, "Bonus", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "0" )
	PORT_DIPSETTING (   0x04, "0 1x" )
	PORT_DIPSETTING (   0x08, "0 1x 2x" )
	PORT_DIPSETTING (   0x0c, "0 1x 2x 3x" )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 $2001 */	/* see port 7 for y trackball */
	PORT_BIT ( 0x03, IP_ACTIVE_HIGH, IPT_UNKNOWN )				/* JB 971220 */
	PORT_DIPNAME (0x04, 0x00, "Credit Minimum", IP_KEY_NONE )	/* JB 971220 */
	PORT_DIPSETTING (   0x00, "1" )
	PORT_DIPSETTING (   0x04, "2" )
	PORT_DIPNAME (0x08, 0x00, "Coin Counters", IP_KEY_NONE )	/* JB 971220 */
	PORT_DIPSETTING (   0x00, "1" )
	PORT_DIPSETTING (   0x08, "2" )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 $2010 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )

	PORT_START	/* IN3 $2011 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* 4 */ /* DSW1 $0408 */
	PORT_DIPNAME (0x01, 0x00, "Millipede Head", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "Easy" )
	PORT_DIPSETTING (   0x01, "Hard" )
	PORT_DIPNAME (0x02, 0x00, "Beetle", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "Easy" )
	PORT_DIPSETTING (   0x02, "Hard" )
	PORT_DIPNAME (0x0c, 0x04, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "2" )
	PORT_DIPSETTING (   0x04, "3" )
	PORT_DIPSETTING (   0x08, "4" )
	PORT_DIPSETTING (   0x0c, "5" )
	PORT_DIPNAME (0x30, 0x10, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "12000" )
	PORT_DIPSETTING (   0x10, "15000" )
	PORT_DIPSETTING (   0x20, "20000" )
	PORT_DIPSETTING (   0x30, "None" )
	PORT_DIPNAME (0x40, 0x00, "Spider", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "Easy" )
	PORT_DIPSETTING (   0x40, "Hard" )
	PORT_DIPNAME (0x80, 0x00, "Starting Score Select", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "On" )
	PORT_DIPSETTING (   0x80, "Off" )

	PORT_START	/* 5 */ /* DSW2 $0808 */
	PORT_DIPNAME (0x03, 0x02, "Coinage", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "Free Play" )
	PORT_DIPSETTING (   0x01, "1 Coin/2 Credits" )
	PORT_DIPSETTING (   0x02, "1 Coin/1 Credit" )
	PORT_DIPSETTING (   0x03, "2 Coins/1 Credit" )
	PORT_DIPNAME (0x0c, 0x00, "Right Coin", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "*1" )
	PORT_DIPSETTING (   0x04, "*4" )
	PORT_DIPSETTING (   0x08, "*5" )
	PORT_DIPSETTING (   0x0c, "*6" )
	PORT_DIPNAME (0x10, 0x00, "Left Coin", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "*1" )
	PORT_DIPSETTING (   0x10, "*2" )
	PORT_DIPNAME (0xe0, 0x00, "Bonus Coins", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "None" )
	PORT_DIPSETTING (   0x20, "3 credits/2 coins" )
	PORT_DIPSETTING (   0x40, "5 credits/4 coins" )
	PORT_DIPSETTING (   0x60, "6 credits/4 coins" )
	PORT_DIPSETTING (   0x80, "6 credits/5 coins" )
	PORT_DIPSETTING (   0xa0, "4 credits/3 coins" )
	PORT_DIPSETTING (   0xc0, "Demo mode" )

	/* JB 971220 */
	PORT_START	/* IN6: FAKE - used for trackball-x at $2000 */
	PORT_ANALOGX ( 0xff, 0x00, IPT_TRACKBALL_X | IPF_REVERSE | IPF_CENTER, 100, 7, 0, 0, IP_KEY_NONE, IP_KEY_NONE, IP_JOY_NONE, IP_JOY_NONE, 4 )
//	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START	/* IN7: FAKE - used for trackball-y at $2001 */
	PORT_ANALOGX ( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_CENTER, 100, 7, 0, 0, IP_KEY_NONE, IP_KEY_NONE, IP_JOY_NONE, IP_JOY_NONE, 4 )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 256*8*8 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	8,16,	/* 16*8 sprites */
	128,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 128*16*8 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,       0, 4 },
	{ 1, 0x0000, &spritelayout,   4*4, 1 },
	{ -1 } /* end of array */
};



static struct POKEYinterface pokey_interface =
{
	2,	/* 2 chips */
	1500000,	/* 1.5 MHz??? */
	255,
	POKEY_DEFAULT_GAIN/4,
	NO_CLIP,
	/* The 8 pot handlers */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	/* The allpot handler */
	{ input_port_4_r, input_port_5_r },
};



static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1000000,	/* 1 Mhz ???? */
			0,
			readmem,writemem,0,0,
			interrupt,2		/* ASG -- 2 per frame, once for VBLANK and once not? */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	32,4*4+4,
	milliped_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY|VIDEO_MODIFIES_PALETTE,
	0,
	generic_vh_start,
	generic_vh_stop,
	milliped_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( milliped_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "milliped.104", 0x4000, 0x1000, 0xd13b2ed1 )
	ROM_LOAD( "milliped.103", 0x5000, 0x1000, 0x8d016c93 )
	ROM_LOAD( "milliped.102", 0x6000, 0x1000, 0x0a7b24db )
	ROM_LOAD( "milliped.101", 0x7000, 0x1000, 0x35374cb3 )
	ROM_RELOAD(               0xf000, 0x1000 )	/* for the reset and interrupt vectors */

	ROM_REGION(0x1000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "milliped.106", 0x0000, 0x0800, 0x006170b5 )
	ROM_LOAD( "milliped.107", 0x0800, 0x0800, 0x7bd67d9e )
ROM_END



static int hiload(void)
{
	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0x0064],"\x75\x91\x08",3) == 0 &&
			memcmp(&RAM[0x0079],"\x16\x19\x04",3) == 0)
	{
		void *f;


		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread(f,&RAM[0x0064],6*8);
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
		osd_fwrite(f,&RAM[0x0064],6*8);
		osd_fclose(f);
	}
}



struct GameDriver milliped_driver =
{
	"Millipede",
	"milliped",
	"IVAN MACKINTOSH\nNICOLA SALMORIA\nJOHN BUTLER\nAARON GILES\nBERND WIEBELT\nBRAD OLIVER",
	&machine_driver,

	milliped_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	input_ports,

	0, 0, 0,
	ORIENTATION_ROTATE_270,

	hiload, hisave
};


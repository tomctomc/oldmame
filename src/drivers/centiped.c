/***************************************************************************

              Centipede Memory map and Dip Switches
              -------------------------------------

Memory map for Centipede directly from the Atari schematics (1981).

 Address  R/W  D7 D6 D5 D4 D3 D2 D1 D0   Function
--------------------------------------------------------------------------------------
0000-03FF       D  D  D  D  D  D  D  D   RAM
--------------------------------------------------------------------------------------
0400-07BF       D  D  D  D  D  D  D  D   Playfield RAM
07C0-07CF       D  D  D  D  D  D  D  D   Motion Object Picture
07D0-07DF       D  D  D  D  D  D  D  D   Motion Object Vert.
07E0-07EF       D  D  D  D  D  D  D  D   Motion Object Horiz.
07F0-07FF             D  D  D  D  D  D   Motion Object Color
--------------------------------------------------------------------------------------
0800       R    D  D  D  D  D  D  D  D   Option Switch 1 (0 = On)
0801       R    D  D  D  D  D  D  D  D   Option Switch 2 (0 = On)
--------------------------------------------------------------------------------------
0C00       R    D           D  D  D  D   Horizontal Mini-Track Ball tm Inputs
           R       D                     VBLANK  (1 = VBlank)
           R          D                  Self-Test  (0 = On)
           R             D               Cocktail Cabinet  (1 = Cocktail)
0C01       R    D  D  D                  R,C,L Coin Switches (0 = On)
           R             D               SLAM  (0 = On)
           R                D            Player 2 Fire Switch (0 = On)
           R                   D         Player 1 Fire Switch (0 = On)
           R                      D      Player 2 Start Switch (0 = On)
           R                         D   Player 1 Start Switch (0 = On)

0C02       R    D           D  D  D  D   Vertical Mini-Track Ball tm Inputs
0C03       R    D  D  D  D               Player 1 Joystick (R,L,Down,Up)
           R                D  D  D  D   Player 2 Joystick   (0 = On)
--------------------------------------------------------------------------------------
1000-100F R/W   D  D  D  D  D  D  D  D   Custom Audio Chip
1404       W                D  D  D  D   Playfield Color RAM
140C       W                D  D  D  D   Motion Object Color RAM
--------------------------------------------------------------------------------------
1600       W    D  D  D  D  D  D  D  D   EA ROM Address & Data Latch
1680       W                D  D  D  D   EA ROM Control Latch
1700       R    D  D  D  D  D  D  D  D   EA ROM Read Data
--------------------------------------------------------------------------------------
1800       W                             IRQ Acknowledge
--------------------------------------------------------------------------------------
1C00       W    D                        Left Coin Counter (1 = On)
1C01       W    D                        Center Coin Counter (1 = On)
1C02       W    D                        Right Coin Counter (1 = On)
1C03       W    D                        Player 1 Start LED (0 = On)
1C04       W    D                        Player 2 Start LED (0 = On)
1C07       W    D                        Track Ball Flip Control (0 = Player 1)
--------------------------------------------------------------------------------------
2000       W                             WATCHDOG
2400       W                             Clear Mini-Track Ball Counters
--------------------------------------------------------------------------------------
2000-3FFF  R                             Program ROM
--------------------------------------------------------------------------------------

-EA ROM is an Erasable Reprogrammable rom to save the top 3 high scores
  and other stuff.


 Dip switches at N9 on the PCB

 8    7    6    5    4    3    2    1    Option
-------------------------------------------------------------------------------------
                              On   On    English $
                              On   Off   German
                              Off  On    French
                              Off  Off   Spanish
-------------------------------------------------------------------------------------
                    On   On              2 lives per game
                    On   Off             3 lives per game $
                    Off  On              4 lives per game
                    Off  Off             5 lives per game
-------------------------------------------------------------------------------------
                                         Bonus life granted at every:
          On   On                        10,000 points
          On   Off                       12.000 points $
          Off  On                        15,000 points
          Off  Off                       20,000 points
-------------------------------------------------------------------------------------
     On                                  Hard game difficulty
     Off                                 Easy game difficulty $
-------------------------------------------------------------------------------------
On                                       1-credit minimum $
Off                                      2-credit minimum
-------------------------------------------------------------------------------------

$ = Manufacturer's suggested settings


 Dip switches at N8 on the PCB

 8    7    6    5    4    3    2    1    Option
-------------------------------------------------------------------------------------
                              On   On    Free play
                              On   Off   1 coin for 2 credits
                              Off  On    1 coin for 1 credit $
                              Off  Off   2 coins for 1 credit
-------------------------------------------------------------------------------------
                    On   On              Right coin mech X 1 $
                    On   Off             Right coin mech X 4
                    Off  On              Right coin mech X 5
                    Off  Off             Right coin mech X 6
-------------------------------------------------------------------------------------
               On                        Left coin mech X 1 $
               Off                       Left coin mech X 2
-------------------------------------------------------------------------------------
On   On   On                             No bonus coins $
On   On   Off                            For every 2 coins inserted, game logic
                                          adds 1 more coin
On   Off  On                             For every 4 coins inserted, game logic
                                          adds 1 more coin
On   Off  Off                            For every 4 coins inserted, game logic
                                          adds 2 more coin
Off  On   On                             For every 5 coins inserted, game logic
                                          adds 1 more coin
Off  On   Off                            For every 3 coins inserted, game logic
                                          adds 1 more coin
-------------------------------------------------------------------------------------
$ = Manufacturer's suggested settings

Known issues:

* The self-test mode doesn't work. This may be related to interrupts much like
the Atari vector games.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


int centiped_IN0_r(int offset);
int centiped_IN2_r(int offset);	/* JB 971220 */

extern unsigned char *centiped_paletteram;

void centiped_vh_convert_color_prom (unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
void centiped_paletteram_w (int offset, int data);
void centiped_vh_flipscreen_w (int offset,int data);
void centiped_vh_screenrefresh (struct osd_bitmap *bitmap);



void centiped_led_w(int offset,int data)
{
	osd_led_w(offset,~data >> 7);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0400, 0x07ff, MRA_RAM },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },	/* for the reset / interrupt vectors */
	{ 0x0c00, 0x0c00, centiped_IN0_r },	/* IN0 */
	{ 0x0c01, 0x0c01, input_port_1_r },	/* IN1 */
	{ 0x0c02, 0x0c02, centiped_IN2_r },	/* IN2 */	/* JB 971220 */
	{ 0x0c03, 0x0c03, input_port_3_r },	/* IN3 */
	{ 0x0800, 0x0800, input_port_4_r },	/* DSW1 */
	{ 0x0801, 0x0801, input_port_5_r },	/* DSW2 */
	{ 0x1000, 0x100f, pokey1_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07bf, videoram_w, &videoram, &videoram_size },
	{ 0x07c0, 0x07ff, MWA_RAM, &spriteram },
	{ 0x1000, 0x100f, pokey1_w },
	{ 0x1400, 0x140f, centiped_paletteram_w, &centiped_paletteram },
	{ 0x1800, 0x1800, MWA_NOP },
	{ 0x1c00, 0x1c02, MWA_NOP },
	{ 0x1c03, 0x1c04, centiped_led_w },
	{ 0x1c05, 0x1c06, MWA_NOP },
	{ 0x1c07, 0x1c07, centiped_vh_flipscreen_w },
	{ 0x1680, 0x1680, MWA_NOP },
	{ 0x2000, 0x2000, MWA_NOP },
	{ 0x2000, 0x3fff, MWA_ROM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( input_ports )
	PORT_START	/* IN0 */
	/* The lower 4 bits and bit 7 are for trackball x input. */
	/* They are handled by fake input port 6 and a custom routine. */
	PORT_BIT ( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME (0x10, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "Upright" )
	PORT_DIPSETTING (   0x10, "Cocktail" )
	PORT_BITX(    0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x20, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* IN2 */
	PORT_ANALOGX ( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_CENTER, 100, 7, 0, 0, IP_KEY_NONE, IP_KEY_NONE, IP_JOY_NONE, IP_JOY_NONE, 4 )
	/* The lower 4 bits are the input, and bit 7 is the direction. */
	/* The state of bit 7 does not change if the trackball is not */
	/* moved. JB 971220, BW 980121 */

	PORT_START	/* IN3 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )

	PORT_START	/* IN4 */
	PORT_DIPNAME (0x03, 0x00, "Language", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "English" )
	PORT_DIPSETTING (   0x01, "German" )
	PORT_DIPSETTING (   0x02, "French" )
	PORT_DIPSETTING (   0x03, "Spanish" )
	PORT_DIPNAME (0x0c, 0x04, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "2" )
	PORT_DIPSETTING (   0x04, "3" )
	PORT_DIPSETTING (   0x08, "4" )
	PORT_DIPSETTING (   0x0c, "5" )
	PORT_DIPNAME (0x30, 0x10, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "10000" )
	PORT_DIPSETTING (   0x10, "12000" )
	PORT_DIPSETTING (   0x20, "15000" )
	PORT_DIPSETTING (   0x30, "20000" )
	PORT_DIPNAME (0x40, 0x10, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "Hard" )
	PORT_DIPSETTING (   0x10, "Easy" )
	PORT_DIPNAME (0x80, 0x00, "Credit Minimum", IP_KEY_NONE )
	PORT_DIPSETTING (   0x00, "1" )
	PORT_DIPSETTING (   0x10, "2" )

	PORT_START	/* IN5 */
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
	PORT_DIPSETTING (   0x0a, "4 credits/3 coins" )

	PORT_START	/* IN6, fake trackball input port. */
	PORT_ANALOGX ( 0xff, 0x00, IPT_TRACKBALL_X | IPF_REVERSE | IPF_CENTER, 100, 7, 0, 0, IP_KEY_NONE, IP_KEY_NONE, IP_JOY_NONE, IP_JOY_NONE, 4 )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 256*8*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	8,16,	/* 16*8 sprites */
	128,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 128*16*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,   0, 1 },
	{ 1, 0x0000, &spritelayout, 4, 1 },
	{ -1 } /* end of array */
};



static struct POKEYinterface pokey_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz??? */
	255,
	POKEY_DEFAULT_GAIN,
	NO_CLIP,
	/* The 8 pot handlers */
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	/* The allpot handler */
	{ 0 },
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
			interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	16, 2*4,
	centiped_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY|VIDEO_MODIFIES_PALETTE,
	0,
	generic_vh_start,
	generic_vh_stop,
	centiped_vh_screenrefresh,

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

ROM_START( centiped_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "centiped.307", 0x2000, 0x0800, 0x2409fc03 )
	ROM_LOAD( "centiped.308", 0x2800, 0x0800, 0x209922dd )
	ROM_LOAD( "centiped.309", 0x3000, 0x0800, 0x57cee11e )
	ROM_LOAD( "centiped.310", 0x3800, 0x0800, 0xb959c639 )
	ROM_RELOAD(         0xf800, 0x0800 )	/* for the reset and interrupt vectors */

	ROM_REGION(0x1000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "centiped.211", 0x0000, 0x0800, 0x704a2608 )
	ROM_LOAD( "centiped.212", 0x0800, 0x0800, 0xc9016e3f )
ROM_END



static int hiload(void)
{
	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0x0002],"\x43\x65\x01",3) == 0 &&
			memcmp(&RAM[0x0017],"\x02\x21\x01",3) == 0)
	{
		void *f;


		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread(f,&RAM[0x0002],6*8);
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
		osd_fwrite(f,&RAM[0x0002],6*8);
		osd_fclose(f);
	}
}



struct GameDriver centiped_driver =
{
	"Centipede",
	"centiped",
	"Ivan Mackintosh (hardware info)\nEdward Massey (MageX emulator)\nPete Rittwage (hardware info)\nNicola Salmoria (MAME driver)\nMirko Buffoni (MAME driver)\nBrad Oliver (additional code)",
	&machine_driver,

	centiped_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	input_ports,

	0, 0, 0,
	ORIENTATION_ROTATE_270,

	hiload, hisave
};

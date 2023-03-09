/***************************************************************************
  Star Fire

Memory Map:

Read:
0000 - 5FFF: ROM
8000 - 9FFF: Working RAM
A000 - BFFF: Pallette RAM (best I can figure)
C000 - DFFF: Video RAM

9400,9800  Dip switch

9401:   Bit 0: Coin
        Bit 1: Start
        Bit 2: Fire
        Bit 3: Tie sound on
        Bit 4: Laser sound on
        Bit 5: Slam/Static
        Bit 6,7: high

9805:   Velocity ADC
9806:   Horizontal motion ADC
9807:   Vertical motion ADC

Write:
8000 - 9FFF: Working RAM
A000 - BFFF: Pallette RAM (best I can figure)
C000 - DFFF: Video RAM

9402:   Bit 0: Size (something to do with laser sound)
        Bit 1: Explosion sound
        Bit 2: Tie Weapon sound
        Bit 3: Laser sound
        Bit 4: Tacking Computer sound
        Bit 5: Lock sound
        Bit 6: Scanner sound
        Bit 7: Overheat sound

9400:   Video shift control
        Right half of screen:
        Bit 0: Mirror bits
        Bit 1..3: Roll right x bits
        Left half of screen:
        Bit 4: Mirror bits
        Bit 5..7: Roll right x bits

9401:   Bit 0..3: Video write logic function (A = Data written, B = Data in mem)
            0:   A
            1:   A or B
            7:   !A or B
            C:   0
            D:   !A and B
        Bit 4: Roll (something to do with video)
        Bit 5: PROT  (something to do with video)
        Bit 6: TRANS (something to do with pallette RAM)
        Bit 7: CDRM (something to do with pallette RAM)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

void starfire_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
void starfire_vh_screenrefresh(struct osd_bitmap *bitmap);

extern void starfire_soundctrl_w(int offset, int data);
extern int starfire_io1_r(int offset);
extern int starfire_vh_start(void);
extern void starfire_vh_stop(void);
extern void starfire_vidctrl_w(int offset,int data);
extern void starfire_vidctrl1_w(int offset,int data);
extern int starfire_interrupt (void);
extern void starfire_videoram_w(int offset,int data);
extern int starfire_videoram_r(int offset);
extern void starfire_colorram_w(int offset,int data);
extern int starfire_colorram_r(int offset);


int starfire_throttle;

int starfire_throttle_r (int offest) {
    if (osd_key_pressed(OSD_KEY_Z)) starfire_throttle = 0xFF;
    if (osd_key_pressed(OSD_KEY_X)) starfire_throttle = 0x00;
    return starfire_throttle;

}


static struct MemoryReadAddress readmem[] =
{
        { 0x0000, 0x57FF, MRA_ROM },
        { 0x5800, 0x8fff, MRA_RAM },
        { 0x9400, 0x9400, input_port_0_r },
        { 0x9800, 0x9800, input_port_0_r },
        { 0x9401, 0x9401, starfire_io1_r },
        { 0x9801, 0x9801, starfire_io1_r },
        { 0x9806, 0x9806, input_port_2_r },
        { 0x9807, 0x9807, input_port_3_r },
        { 0x9805, 0x9805, starfire_throttle_r },
        { 0xA000, 0xBfff, starfire_colorram_r },
        { 0xC000, 0xDFFF, starfire_videoram_r },

        { 0xE000, 0xffff, MRA_RAM },
        { -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
        { 0x0000, 0x57ff, MWA_ROM },
        { 0x5800, 0x8fff, MWA_RAM },
        { 0x9400, 0x9400, starfire_vidctrl_w },
        { 0x9401, 0x9401, starfire_vidctrl1_w },
        { 0x9402, 0x9402, starfire_soundctrl_w },
        { 0xA000, 0xBfff, starfire_colorram_w },
        { 0xC000, 0xDFFF, starfire_videoram_w },
        { 0xE000, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( starfire_input_ports )

        PORT_START      /* DSW0 */
        PORT_DIPNAME( 0x03, 0x00, "Time", IP_KEY_NONE )
        PORT_DIPSETTING(    0x00, "90 Sec" )
        PORT_DIPSETTING(    0x01, "80 Sec" )
        PORT_DIPSETTING(    0x02, "70 Sec" )
        PORT_DIPSETTING(    0x03, "60 Sec" )
        PORT_DIPNAME( 0x04, 0x00, "Coins to Start",IP_KEY_NONE)
        PORT_DIPSETTING(    0x00, "1 Coint" )
        PORT_DIPSETTING(    0x04, "2 Coins" )
        PORT_DIPNAME( 0x08, 0x00, "Fuel",IP_KEY_NONE)
        PORT_DIPSETTING(    0x00, "300" )
        PORT_DIPSETTING(    0x08, "600" )
        PORT_DIPNAME( 0x30, 0x00, "Bonus",IP_KEY_NONE)
        PORT_DIPSETTING(    0x00, "no bonus" )
        PORT_DIPSETTING(    0x10, "700 points" )
        PORT_DIPSETTING(    0x20, "500 points" )
        PORT_DIPSETTING(    0x30, "300 points" )
        PORT_DIPNAME( 0x40, 0x00, "Score Table Hold",IP_KEY_NONE)
        PORT_DIPSETTING(    0x00, "fixed length" )
        PORT_DIPSETTING(    0x40, "fixed length+fire" )
        PORT_DIPNAME( 0x80, 0x80, "Mode",IP_KEY_NONE)
        PORT_DIPSETTING(    0x80, "Diagnostic" )
        PORT_DIPSETTING(    0x00, "Normal" )

        PORT_START      /* IN1 */
        PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1)
        PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1)
        PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1)
        PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN)
        PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN)
        PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN)
        PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN)
        PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN)

    PORT_START  /* IN2 */
    PORT_ANALOG ( 0xff, 0x80, IPT_AD_STICK_X, 100, 0, 0, 255 )

    PORT_START  /* IN3 */
    PORT_ANALOG ( 0xff, 0x80, IPT_AD_STICK_Y | IPF_REVERSE, 100, 0, 0, 255 )

INPUT_PORTS_END

static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
            2500000,    /* 2.5 Mhz */
			0,
			readmem,writemem,0,0,
            starfire_interrupt,2
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
    0,

	/* video hardware */
    256,256,
    { 0, 256-1, 0, 256-1 },
    0,
    256,
    0,
    starfire_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
    starfire_vh_start,
    starfire_vh_stop,
    starfire_vh_screenrefresh,

	/* sound hardware */
    0,
    0,
    0,
    0
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( starfire_rom )
        ROM_REGION(0x10000)     /* 64k for code */
        ROM_LOAD( "sfire.1a", 0x0000, 0x0800, 0x0d2f552d )
        ROM_LOAD( "sfire.2a", 0x0800, 0x0800, 0xd11f059d )
        ROM_LOAD( "sfire.1b", 0x1000, 0x0800, 0x4fa1db37 )
        ROM_LOAD( "sfire.2b", 0x1800, 0x0800, 0x06a9c821 )
        ROM_LOAD( "sfire.1c", 0x2000, 0x0800, 0x6ac59d37 )
        ROM_LOAD( "sfire.2c", 0x2800, 0x0800, 0x5da6f2a2 )
        ROM_LOAD( "sfire.1d", 0x3000, 0x0800, 0xa3876a03 )
        ROM_LOAD( "sfire.2d", 0x3800, 0x0800, 0x6512ad78 )
        ROM_LOAD( "sfire.1e", 0x4000, 0x0800, 0x7e8b4531 )
        ROM_LOAD( "sfire.2e", 0x4800, 0x0800, 0x65220d9a )
        ROM_LOAD( "sfire.1f", 0x5000, 0x0800, 0x286a8a0e )
ROM_END

struct GameDriver starfire_driver =
{
	"Starfire (Exidy)",
	"starfire",
	"Daniel Boris\n",
	&machine_driver,

	starfire_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	starfire_input_ports,

	0, 0, 0,
	ORIENTATION_DEFAULT,
	0,0
};


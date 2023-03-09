/***************************************************************************

Gauntlet Memory Map
-----------------------------------

GAUNTLET 68010 MEMORY MAP

Function                           Address        R/W  DATA
-------------------------------------------------------------
Program ROM/Operating System       000000-00FFFF  R    D0-D15
Program ROM/SLAPSTIC               038000-03FFFF  R    D0-D15
Program ROM/Main                   040000-07FFFF  R    D0-D15
Spare RAM                          800000-801FFF  R/W  D0-D15

EEPROM                             802001-802FFF  R/W  D7-D0

Player 1 Input (See detail below)  803001         R    D0-D71
Player 2 Input                     803003         R    D0-D7
Player 3 Input                     803005         R    D0-D7
Player 4 Input                     803007         R    D0-D7

Player Inputs:
  Joystick Up                                          D7
  Joystick Down                                        D6
  Joystick Left                                        D5
  Joystick Right                                       D4
  Spare                                                D3
  Spare                                                D2
  Fire                                                 D1
  Magic/Start                                          D0

VBLANK (Active Low)                803009         R    D6
Outbut/Buffer Full (@803170)       803009         R    D5
 (Active High)
Input/Buffer Full (@80300F)        803009         R    D4
 (Active High)
Self-Test (Active Low)             803009         R    D3

Read Sound Processor (6502)        80300F         R    D0-D7

Watchdog (128 msec. timeout)       803100         W    xx

LED-1 (Low On)                     803121         W    D0
LED-2 (Low-On)                     803123         W    D0
LED-3 (Low On)                     803125         W    D0
LED-4 (Low On)                     803127         W    D0
Sound Processor Reset (Low Reset)  80312F         W    D0

VBlank Acknowledge                 803140         W    xx
Unlock EEPROM                      803150         W    xx
Write Sound Processor (6502)       803171         W    D0-D7

Playfield RAM                      900000-901FFF  R/W  D0-D15
Motion Object Picture              902000-9027FF  R/W  D0-D15
Motion Object Horizontal Position  902800-902FFF  R/W  D0-D15
Motion Object Vertical Position    903000-9037FF  R/W  D0-D15
Motion Object Link                 903800-903FFF  R/W  D0-D15
Spare RAM                          904000-904FFF  R/W  D0-D15
Alphanumerics RAM                  905000-905FFF  R/W  D0-D15

Playfield Vertical Scroll          905F6E,905F6F  R/W  D7-D15
Playfield ROM Bank Select          905F6F         R/W

Color RAM Alpha                    910000-9101FF  R/W  D0-D15
Color RAM Motion Object            910200-9103FF  R/W  D0-D15
Color RAM Playfield Shadow         910400-9104FF  R/W  D0-D15
Color RAM Playfield                910500-9105FF  R/W  D0-D15
Color RAM (Spare)                  910600-9107FF  R/W  D0-D15

Playfield Horizontal Scroll        930000,930001  W    D0-D8

NOTE: All addresses can be accessed in byte or word mode.


GAUNTLET 6502 MEMORY MAP

Function                                  Address     R/W  Data
---------------------------------------------------------------
Program RAM                               0000-0FFF   R/W  D0-D7

Write 68010 Port (Outbut Buffer)          1000        W    D0-D7
Read 68010 Port (Input Buffer)            1010        R    D0-D7

Audio Mix:
 Speech Mix                               1020        W    D5-D7
 Effects Mix                              1020        W    D3,D4
 Music Mix                                1020        W    D0-D2

Coin 1 (Left)                             1020        R    D3
Coin 2                                    1020        R    D2
Coin 3                                    1020        R    D1
Coin 4 (Right)                            1020        R    D0

Data Available (@ 1010) (Active High)     1030-1030   R    D7
Output Buffer Full (@1000) (Active High)  1030        R    D6
Speech Ready (Active Low)                 1030        R    D5
Self-Test (Active Low)                    1030        R    D4

Music Reset (Low Reset)                   1030        W    D7
Speech Write (Active Low)                 1031        W    D7
Speech Reset (Active Low)                 1032        W    D7
Speech Squeak (Low = 650KHz Clock)        1033        W    D7
Coin Counter Right (Active High)          1034        W    D7
Coin Counter Left (Active High)           1035        W    D7

Effects                                   1800-180F   R/W  D0-D7
Music                                     1810-1811   R/W  D0-D7
Speech                                    1820        W    D0-D7
Interrupt Acknowledge                     1830        R/W  xx

Program ROM (48K bytes)                   4000-FFFF   R    D0-D7
***************************************************************************/



#include "driver.h"
#include "vidhrdw/generic.h"
#include "sndhrdw/pokyintf.h"
#include "sndhrdw/5220intf.h"
#include "sndhrdw/2151intf.h"


extern int gauntlet_spriteram_size;
extern int gauntlet_playfieldram_size;
extern int gauntlet_alpharam_size;
extern int gauntlet_paletteram_size;
int gauntlet_eeprom_size;

extern unsigned char *gauntlet_eeprom;
extern unsigned char *gauntlet_slapstic_base;
extern unsigned char *gauntlet_playfieldram;
extern unsigned char *gauntlet_spriteram;
extern unsigned char *gauntlet_alpharam;
extern unsigned char *gauntlet_paletteram;
extern unsigned char *gauntlet_vscroll;
extern unsigned char *gauntlet_hscroll;
extern unsigned char *gauntlet_bank3;
extern unsigned char *gauntlet_speed_check;

int gauntlet_control_r (int offset);
int gauntlet_io_r (int offset);
int gauntlet_eeprom_r (int offset);
int gauntlet_slapstic_r (int offset);
int gauntlet_68010_speedup_r (int offset);
int gauntlet_6502_speedup_r (int offset);
int gauntlet_6502_sound_r (int offset);
int gauntlet_6502_switch_r (int offset);
int gauntlet_ym2151_r (int offset);
int gauntlet_playfieldram_r (int offset);
int gauntlet_alpharam_r (int offset);
int gauntlet_vscroll_r (int offset);
int gauntlet_paletteram_r (int offset);

void gauntlet_io_w (int offset, int data);
void gauntlet_eeprom_w (int offset, int data);
void gauntlet_eeprom_enable_w (int offset, int data);
void gauntlet_sound_w (int offset, int data);
void gauntlet_slapstic_w (int offset, int data);
void gauntlet_68010_speedup_w (int offset, int data);
void gauntlet_6502_sound_w (int offset, int data);
void gauntlet_6502_mix_w (int offset, int data);
void gauntlet_sound_ctl_w (int offset, int data);
void gauntlet_ym2151_w (int offset, int data);
void gauntlet_tms_w (int offset, int data);
void gauntlet_playfieldram_w (int offset, int data);
void gauntlet_alpharam_w (int offset, int data);
void gauntlet_vscroll_w (int offset, int data);
void gauntlet_paletteram_w (int offset, int data);

int gauntlet_interrupt(void);
int gauntlet_sound_interrupt(void);

void gauntlet_init_machine(void);
void gaunt2p_init_machine(void);
void gauntlet2_init_machine(void);

int gauntlet_vh_start(void);
void gauntlet_vh_stop(void);

void gauntlet_vh_screenrefresh(struct osd_bitmap *bitmap);


/*************************************
 *
 *		Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress gauntlet_readmem[] =
{
	{ 0x000000, 0x037fff, MRA_ROM },
	{ 0x038000, 0x03ffff, gauntlet_slapstic_r, &gauntlet_slapstic_base },
	{ 0x040000, 0x07ffff, MRA_ROM },
	{ 0x800000, 0x801fff, MRA_BANK1 },
	{ 0x802000, 0x802fff, gauntlet_eeprom_r, &gauntlet_eeprom, &gauntlet_eeprom_size },
	{ 0x803000, 0x803007, gauntlet_control_r },
	{ 0x803008, 0x80300f, gauntlet_io_r },
	{ 0x900000, 0x901fff, gauntlet_playfieldram_r, &gauntlet_playfieldram, &gauntlet_playfieldram_size },
	{ 0x902000, 0x903fff, MRA_BANK4, &gauntlet_spriteram, &gauntlet_spriteram_size },
	{ 0x904000, 0x904003, gauntlet_68010_speedup_r, &gauntlet_speed_check },
	{ 0x904004, 0x904fff, MRA_BANK2 },
	{ 0x905f6c, 0x905f6f, gauntlet_vscroll_r, &gauntlet_vscroll },
	{ 0x905000, 0x905eff, gauntlet_alpharam_r, &gauntlet_alpharam, &gauntlet_alpharam_size },
	{ 0x905f00, 0x905fff, MRA_BANK3, &gauntlet_bank3 },
	{ 0x910000, 0x9107ff, gauntlet_paletteram_r, &gauntlet_paletteram, &gauntlet_paletteram_size },
	{ 0x930000, 0x930003, MRA_BANK5, &gauntlet_hscroll },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress gauntlet_writemem[] =
{
	{ 0x000000, 0x037fff, MWA_ROM },
	{ 0x038000, 0x03ffff, gauntlet_slapstic_w },
	{ 0x040000, 0x07ffff, MWA_ROM },
	{ 0x800000, 0x801fff, MWA_BANK1 },
	{ 0x802000, 0x802fff, gauntlet_eeprom_w },
	{ 0x803100, 0x803103, MWA_NOP },
	{ 0x803120, 0x80312f, gauntlet_io_w },
	{ 0x803140, 0x803143, MWA_NOP },
	{ 0x803150, 0x803153, gauntlet_eeprom_enable_w },
	{ 0x803170, 0x803173, gauntlet_sound_w },
	{ 0x900000, 0x901fff, gauntlet_playfieldram_w },
	{ 0x902000, 0x903fff, MWA_BANK4 },
	{ 0x904000, 0x904003, gauntlet_68010_speedup_w },
	{ 0x904004, 0x904fff, MWA_BANK2 },
	{ 0x905f6c, 0x905f6f, gauntlet_vscroll_w },
	{ 0x905000, 0x905eff, gauntlet_alpharam_w },
	{ 0x905f00, 0x905fff, MWA_BANK3 },
	{ 0x910000, 0x9107ff, gauntlet_paletteram_w },
	{ 0x930000, 0x930003, MWA_BANK5 },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *		Sound CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress gauntlet_sound_readmem[] =
{
	{ 0x0211, 0x0211, gauntlet_6502_speedup_r },
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x1010, 0x101f, gauntlet_6502_sound_r },
	{ 0x1020, 0x102f, input_port_4_r },
	{ 0x1030, 0x103f, gauntlet_6502_switch_r },
	{ 0x1800, 0x180f, pokey1_r },
	{ 0x1811, 0x1811, YM2151_status_port_0_r },
	{ 0x1830, 0x183f, MRA_NOP },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress gauntlet_sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x100f, gauntlet_6502_sound_w },
	{ 0x1020, 0x102f, MWA_NOP/*gauntlet_6502_mix_w*/ },
	{ 0x1030, 0x103f, gauntlet_sound_ctl_w },
	{ 0x1800, 0x180f, pokey1_w },
	{ 0x1810, 0x1810, YM2151_register_port_0_w },
	{ 0x1811, 0x1811, YM2151_data_port_0_w },
	{ 0x1820, 0x182f, gauntlet_tms_w },
	{ 0x1830, 0x183f, MWA_NOP },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *		Port definitions
 *
 *************************************/

INPUT_PORTS_START( gauntlet_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 | IPF_8WAY )

	PORT_START	/* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_VBLANK )
	PORT_BIT( 0x30, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX(    0x08, 0x08, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Self Test", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x08, "Off")
	PORT_DIPSETTING(    0x00, "On")
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* Fake! */
	PORT_DIPNAME( 0x03, 0x00, "Player 1 Plays", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Red/Warrior" )
	PORT_DIPSETTING(    0x01, "Blue/Valkyrie" )
	PORT_DIPSETTING(    0x02, "Yellow/Wizard" )
	PORT_DIPSETTING(    0x03, "Green/Elf" )

INPUT_PORTS_END



/*************************************
 *
 *		Graphics definitions
 *
 *************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	1024,	/* 1024 chars */
	2,		/* 2 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8, 9, 10, 11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16	/* every char takes 16 consecutive bytes */
};


static struct GfxLayout spritelayout =
{
	8,8,	/* 8*8 sprites */
	12288,	/* up to 12288 of them */
	4,		/* 4 bits per pixel */
	{ 3*8*0x18000, 2*8*0x18000, 1*8*0x18000, 0*8*0x18000 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x60000, &charlayout,      0, 64 },		/* characters 8x8 */
	{ 1, 0x00000, &spritelayout,  256, 32 },		/* sprites & playfield */
	{ -1 } /* end of array */
};



/*************************************
 *
 *		Sound definitions
 *
 *************************************/

static struct POKEYinterface pokey_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz??? */
	128,
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
	{ 0 }
};


static struct TMS5220interface tms5220_interface =
{
    640000,     /* clock speed (80*samplerate) */
    255,        /* volume */
    0           /* irq handler */
};


static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3582071,	/* seems to be the standard */
	{ 255 },
	{ 0 }
};



/*************************************
 *
 *		Machine driver
 *
 *************************************/

static struct MachineDriver gauntlet_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			7159000,
			0,
			gauntlet_readmem,gauntlet_writemem,0,0,
			gauntlet_interrupt,1
		},
		{
			CPU_M6502,
			1966080,		/* Clocked by the 2H signal; best guess = (64*8)*(32*8)*60fps/4 = 1.966MHz */
			2,
			gauntlet_sound_readmem,gauntlet_sound_writemem,0,0,
			0,0,
			gauntlet_sound_interrupt,250
		},
	},
	60, 2000,	/* frames per second, vblank duration */
	10,
	gauntlet_init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	256,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_16BIT | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	gauntlet_vh_start,
	gauntlet_vh_stop,
	gauntlet_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151_ALT,
			&ym2151_interface
		},
		{
			SOUND_TMS5220,
			&tms5220_interface
		},
		{
			SOUND_POKEY,
			&pokey_interface
		}
	}
};


static struct MachineDriver gaunt2p_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			7159000,
			0,
			gauntlet_readmem,gauntlet_writemem,0,0,
			gauntlet_interrupt,1
		},
		{
			CPU_M6502,
			1966080,		/* Clocked by the 2H signal; best guess = (64*8)*(32*8)*60fps/4 = 1.966MHz */
			2,
			gauntlet_sound_readmem,gauntlet_sound_writemem,0,0,
			0,0,
			gauntlet_sound_interrupt,250
		},
	},
	60, 2000,	/* frames per second, vblank duration */
	10,
	gaunt2p_init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	256,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_16BIT | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	gauntlet_vh_start,
	gauntlet_vh_stop,
	gauntlet_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151_ALT,
			&ym2151_interface
		},
		{
			SOUND_TMS5220,
			&tms5220_interface
		},
		{
			SOUND_POKEY,
			&pokey_interface
		}
	}
};


static struct MachineDriver gauntlet2_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			7159000,
			0,
			gauntlet_readmem,gauntlet_writemem,0,0,
			gauntlet_interrupt,1
		},
		{
			CPU_M6502,
			1966080,		/* Clocked by the 2H signal; best guess = (64*8)*(32*8)*60fps/4 = 1.966MHz */
			2,
			gauntlet_sound_readmem,gauntlet_sound_writemem,0,0,
			0,0,
			gauntlet_sound_interrupt,250
		},
	},
	60, 2000,	/* frames per second, vblank duration */
	10,
	gauntlet2_init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	256,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_16BIT | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	gauntlet_vh_start,
	gauntlet_vh_stop,
	gauntlet_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151_ALT,
			&ym2151_interface
		},
		{
			SOUND_TMS5220,
			&tms5220_interface
		},
		{
			SOUND_POKEY,
			&pokey_interface
		}
	}
};



/*************************************
 *
 *		High score save/load
 *
 *************************************/

static int gauntlet_hiload (void)
{
   void *f;

	f = osd_fopen (Machine->gamedrv->name, 0, OSD_FILETYPE_HIGHSCORE, 0);
   if (f)
   {
		osd_fread (f, gauntlet_eeprom, gauntlet_eeprom_size);
		osd_fclose (f);
   }
   else
   	memset (gauntlet_eeprom, 0xff, gauntlet_eeprom_size);

   return 1;
}


static int gauntlet2_hiload (void)
{
   void *f;

	f = osd_fopen (Machine->gamedrv->name, 0, OSD_FILETYPE_HIGHSCORE, 0);
   if (f)
   {
		osd_fread (f, gauntlet_eeprom, gauntlet_eeprom_size);
		osd_fclose (f);
   }
   else
   	memset (gauntlet_eeprom, 0xff, gauntlet_eeprom_size);

   return 1;
}


static void hisave (void)
{
   void *f;

	f = osd_fopen (Machine->gamedrv->name, 0, OSD_FILETYPE_HIGHSCORE, 1);
   if (f)
   {
      osd_fwrite (f, gauntlet_eeprom, gauntlet_eeprom_size);
      osd_fclose (f);
   }
}



/*************************************
 *
 *		ROM definition(s)
 *
 *************************************/

ROM_START( gauntlet_rom )
	ROM_REGION(0x80000)	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "gauntlt1.9a",  0x00000, 0x08000, 0xf0e62fde )
	ROM_LOAD_ODD ( "gauntlt1.9b",  0x00000, 0x08000, 0xdf7c4044 )
	ROM_LOAD_EVEN( "gauntlt1.10a", 0x38000, 0x04000, 0x3ccbead5 )
	ROM_LOAD_ODD ( "gauntlt1.10b", 0x38000, 0x04000, 0xdd2387c5 )
	ROM_LOAD_EVEN( "gauntlt1.7a",  0x40000, 0x08000, 0x54702330 )
	ROM_LOAD_ODD ( "gauntlt1.7b",  0x40000, 0x08000, 0x6f769cc4 )

	ROM_REGION(0x64000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "gauntlt1.1a",  0x00000, 0x08000, 0xc39784c3 )
	ROM_LOAD( "gauntlt1.1b",  0x08000, 0x08000, 0x7d468690 )
	ROM_LOAD( "gauntlt1.1l",  0x18000, 0x08000, 0x61312119 )
	ROM_LOAD( "gauntlt1.1mn", 0x20000, 0x08000, 0xf1f0618c )
	ROM_LOAD( "gauntlt1.2a",  0x30000, 0x08000, 0x9306abfc )
	ROM_LOAD( "gauntlt1.2b",  0x38000, 0x08000, 0xae5eded2 )
	ROM_LOAD( "gauntlt1.2l",  0x48000, 0x08000, 0x24614385 )
	ROM_LOAD( "gauntlt1.2mn", 0x50000, 0x08000, 0x063a7d0c )
	ROM_LOAD( "gauntlt1.6p",  0x60000, 0x04000, 0xd0cae034 )        /* alpha font */

	ROM_REGION(0x10000)	/* 64k for 6502 code */
	ROM_LOAD( "gauntlt1.16r", 0x4000, 0x4000, 0x5e94f5c8 )
	ROM_LOAD( "gauntlt1.16s", 0x8000, 0x8000, 0x051bc3d3 )
ROM_END


ROM_START( gauntir1_rom )
	ROM_REGION(0x80000)	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "gaun1ir1.9a",  0x00000, 0x08000, 0x70ba8384 )
	ROM_LOAD_ODD ( "gaun1ir1.9b",  0x00000, 0x08000, 0x32b5699d )
	ROM_LOAD_EVEN( "gaun1ir1.10a", 0x38000, 0x04000, 0x71980136 )
	ROM_LOAD_ODD ( "gaun1ir1.10b", 0x38000, 0x04000, 0xcf467ac6 )
	ROM_LOAD_EVEN( "gaun1ir1.7a",  0x40000, 0x08000, 0x7a76071e )
	ROM_LOAD_ODD ( "gaun1ir1.7b",  0x40000, 0x08000, 0xbe4bb1c3 )

	ROM_REGION(0x64000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "gauntlt1.1a",  0x00000, 0x08000, 0xc39784c3 )
	ROM_LOAD( "gauntlt1.1b",  0x08000, 0x08000, 0x7d468690 )
	ROM_LOAD( "gauntlt1.1l",  0x18000, 0x08000, 0x61312119 )
	ROM_LOAD( "gauntlt1.1mn", 0x20000, 0x08000, 0xf1f0618c )
	ROM_LOAD( "gauntlt1.2a",  0x30000, 0x08000, 0x9306abfc )
	ROM_LOAD( "gauntlt1.2b",  0x38000, 0x08000, 0xae5eded2 )
	ROM_LOAD( "gauntlt1.2l",  0x48000, 0x08000, 0x24614385 )
	ROM_LOAD( "gauntlt1.2mn", 0x50000, 0x08000, 0x063a7d0c )
	ROM_LOAD( "gauntlt1.6p",  0x60000, 0x04000, 0xd0cae034 )        /* alpha font */

	ROM_REGION(0x10000)	/* 64k for 6502 code */
	ROM_LOAD( "gauntlt1.16r", 0x4000, 0x4000, 0x5e94f5c8 )
	ROM_LOAD( "gauntlt1.16s", 0x8000, 0x8000, 0x051bc3d3 )
ROM_END


ROM_START( gauntir2_rom )
	ROM_REGION(0x80000)	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "gaun1ir2.9a",  0x00000, 0x08000, 0x70ba8384 )
	ROM_LOAD_ODD ( "gaun1ir2.9b",  0x00000, 0x08000, 0x32b5699d )
	ROM_LOAD_EVEN( "gaun1ir2.10a", 0x38000, 0x04000, 0x71980136 )
	ROM_LOAD_ODD ( "gaun1ir2.10b", 0x38000, 0x04000, 0xcf467ac6 )
	ROM_LOAD_EVEN( "gaun1ir2.7a",  0x40000, 0x08000, 0x7642eb0e )
	ROM_LOAD_ODD ( "gaun1ir2.7b",  0x40000, 0x08000, 0x6247fb0d )

	ROM_REGION(0x64000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "gauntlt1.1a",  0x00000, 0x08000, 0xc39784c3 )
	ROM_LOAD( "gauntlt1.1b",  0x08000, 0x08000, 0x7d468690 )
	ROM_LOAD( "gauntlt1.1l",  0x18000, 0x08000, 0x61312119 )
	ROM_LOAD( "gauntlt1.1mn", 0x20000, 0x08000, 0xf1f0618c )
	ROM_LOAD( "gauntlt1.2a",  0x30000, 0x08000, 0x9306abfc )
	ROM_LOAD( "gauntlt1.2b",  0x38000, 0x08000, 0xae5eded2 )
	ROM_LOAD( "gauntlt1.2l",  0x48000, 0x08000, 0x24614385 )
	ROM_LOAD( "gauntlt1.2mn", 0x50000, 0x08000, 0x063a7d0c )
	ROM_LOAD( "gauntlt1.6p",  0x60000, 0x04000, 0xd0cae034 )        /* alpha font */

	ROM_REGION(0x10000)	/* 64k for 6502 code */
	ROM_LOAD( "gauntlt1.16r", 0x4000, 0x4000, 0x5e94f5c8 )
	ROM_LOAD( "gauntlt1.16s", 0x8000, 0x8000, 0x051bc3d3 )
ROM_END


ROM_START( gaunt2p_rom )
	ROM_REGION(0x80000)	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "gaunt2p.9a",  0x00000, 0x08000, 0x034caf48 )
	ROM_LOAD_ODD ( "gaunt2p.9b",  0x00000, 0x08000, 0x303161ab )
	ROM_LOAD_EVEN( "gauntlt1.10a", 0x38000, 0x04000, 0x3ccbead5 )
	ROM_LOAD_ODD ( "gauntlt1.10b", 0x38000, 0x04000, 0xdd2387c5 )
	ROM_LOAD_EVEN( "gaunt2p.7a",  0x40000, 0x08000, 0x0ea79849 )
	ROM_LOAD_ODD ( "gaunt2p.7b",  0x40000, 0x08000, 0x9c246c0e )

	ROM_REGION(0x64000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "gauntlt1.1a",  0x00000, 0x08000, 0xc39784c3 )
	ROM_LOAD( "gauntlt1.1b",  0x08000, 0x08000, 0x7d468690 )
	ROM_LOAD( "gauntlt1.1l",  0x18000, 0x08000, 0x61312119 )
	ROM_LOAD( "gauntlt1.1mn", 0x20000, 0x08000, 0xf1f0618c )
	ROM_LOAD( "gauntlt1.2a",  0x30000, 0x08000, 0x9306abfc )
	ROM_LOAD( "gauntlt1.2b",  0x38000, 0x08000, 0xae5eded2 )
	ROM_LOAD( "gauntlt1.2l",  0x48000, 0x08000, 0x24614385 )
	ROM_LOAD( "gauntlt1.2mn", 0x50000, 0x08000, 0x063a7d0c )
	ROM_LOAD( "gauntlt1.6p",  0x60000, 0x04000, 0xd0cae034 )        /* alpha font */

	ROM_REGION(0x10000)	/* 64k for 6502 code */
	ROM_LOAD( "gauntlt1.16r", 0x4000, 0x4000, 0x5e94f5c8 )
	ROM_LOAD( "gauntlt1.16s", 0x8000, 0x8000, 0x051bc3d3 )
ROM_END


ROM_START( gaunt2_rom )
	ROM_REGION(0x80000)	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "gauntlt2.9a",  0x00000, 0x08000, 0xf0e62fde )
	ROM_LOAD_ODD ( "gauntlt2.9b",  0x00000, 0x08000, 0xdf7c4044 )
	ROM_LOAD_EVEN( "gauntlt2.10a", 0x38000, 0x04000, 0x685f5aaf )
	ROM_LOAD_ODD ( "gauntlt2.10b", 0x38000, 0x04000, 0x89522898 )
	ROM_LOAD_EVEN( "gauntlt2.7a",  0x40000, 0x08000, 0x8065942b )
	ROM_LOAD_ODD ( "gauntlt2.7b",  0x40000, 0x08000, 0x79789e8e )
	ROM_LOAD_EVEN( "gauntlt2.6a",  0x50000, 0x08000, 0xa2c7e013 )
	ROM_LOAD_ODD ( "gauntlt2.6b",  0x50000, 0x08000, 0xa07ed244 )

	ROM_REGION(0x64000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "gauntlt2.1a",  0x00000, 0x08000, 0xc49a61f0 )
	ROM_LOAD( "gauntlt2.1b",  0x08000, 0x08000, 0x7d468690 )
	ROM_LOAD( "gauntlt2.1c",  0x10000, 0x04000, 0x6d83303b )
	ROM_RELOAD(               0x14000, 0x04000 )
	ROM_LOAD( "gauntlt2.1l",  0x18000, 0x08000, 0xbf7d1bc3 )
	ROM_LOAD( "gauntlt2.1mn", 0x20000, 0x08000, 0xf1f0618c )
	ROM_LOAD( "gauntlt2.1p",  0x28000, 0x04000, 0x51f65028 )
	ROM_RELOAD(               0x2c000, 0x04000 )
	ROM_LOAD( "gauntlt2.2a",  0x30000, 0x08000, 0xec274b73 )
	ROM_LOAD( "gauntlt2.2b",  0x38000, 0x08000, 0xae5eded2 )
	ROM_LOAD( "gauntlt2.2c",  0x40000, 0x04000, 0x9dbb2f8b )
	ROM_RELOAD(               0x44000, 0x04000 )
	ROM_LOAD( "gauntlt2.2l",  0x48000, 0x08000, 0x1e07bcbd )
	ROM_LOAD( "gauntlt2.2mn", 0x50000, 0x08000, 0x063a7d0c )
	ROM_LOAD( "gauntlt2.2p",  0x58000, 0x04000, 0xc32da49d )
	ROM_RELOAD(               0x5c000, 0x04000 )
	ROM_LOAD( "gauntlt2.6p",  0x60000, 0x04000, 0xe379811d )        /* alpha font */

	ROM_REGION(0x10000)	/* 64k for 6502 code */
	ROM_LOAD( "gauntlt2.16r", 0x4000, 0x4000, 0x8b4770d1 )
	ROM_LOAD( "gauntlt2.16s", 0x8000, 0x8000, 0x480179d5 )
ROM_END



/*************************************
 *
 *		ROM decoding
 *
 *************************************/

static void gauntlet_rom_decode (void)
{
	unsigned long *p1, *p2, temp;
	int i;

	/* swap the top and bottom halves of the main CPU ROM images */
	p1 = (unsigned long *)&Machine->memory_region[0][0x000000];
	p2 = (unsigned long *)&Machine->memory_region[0][0x008000];
	for (i = 0; i < 0x8000 / 4; i++)
		temp = *p1, *p1++ = *p2, *p2++ = temp;
	p1 = (unsigned long *)&Machine->memory_region[0][0x040000];
	p2 = (unsigned long *)&Machine->memory_region[0][0x048000];
	for (i = 0; i < 0x8000 / 4; i++)
		temp = *p1, *p1++ = *p2, *p2++ = temp;
	p1 = (unsigned long *)&Machine->memory_region[0][0x050000];
	p2 = (unsigned long *)&Machine->memory_region[0][0x058000];
	for (i = 0; i < 0x8000 / 4; i++)
		temp = *p1, *p1++ = *p2, *p2++ = temp;
	p1 = (unsigned long *)&Machine->memory_region[0][0x060000];
	p2 = (unsigned long *)&Machine->memory_region[0][0x068000];
	for (i = 0; i < 0x8000 / 4; i++)
		temp = *p1, *p1++ = *p2, *p2++ = temp;
	p1 = (unsigned long *)&Machine->memory_region[0][0x070000];
	p2 = (unsigned long *)&Machine->memory_region[0][0x078000];
	for (i = 0; i < 0x8000 / 4; i++)
		temp = *p1, *p1++ = *p2, *p2++ = temp;

	/* also invert the graphics bits on the playfield and motion objects */
	for (i = 0; i < 0x60000; i++)
		Machine->memory_region[1][i] ^= 0xff;
}



/*************************************
 *
 *		Game driver(s)
 *
 *************************************/

struct GameDriver gauntlet_driver =
{
	"Gauntlet",
	"gauntlet",
	"Aaron Giles (MAME driver)\nMike Balfour (graphics info)\nFrank Palazzolo (Slapstic decoding)",
	&gauntlet_machine_driver,

	gauntlet_rom,
	gauntlet_rom_decode,
	0,
	0,
	0,	/* sound_prom */

	gauntlet_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	gauntlet_hiload, hisave
};


struct GameDriver gauntir1_driver =
{
	"Gauntlet (Intermediate Release 1)",
	"gauntir1",
	"Aaron Giles (MAME driver)\nMike Balfour (graphics info)\nFrank Palazzolo (Slapstic decoding)",
	&gauntlet_machine_driver,

	gauntir1_rom,
	gauntlet_rom_decode,
	0,
	0,
	0,	/* sound_prom */

	gauntlet_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	gauntlet_hiload, hisave
};


struct GameDriver gauntir2_driver =
{
	"Gauntlet (Intermediate Release 2)",
	"gauntir2",
	"Aaron Giles (MAME driver)\nMike Balfour (graphics info)\nFrank Palazzolo (Slapstic decoding)",
	&gauntlet_machine_driver,

	gauntir2_rom,
	gauntlet_rom_decode,
	0,
	0,
	0,	/* sound_prom */

	gauntlet_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	gauntlet_hiload, hisave
};


struct GameDriver gaunt2p_driver =
{
	"Gauntlet (2 Player)",
	"gaunt2p",
	"Aaron Giles (MAME driver)\nMike Balfour (graphics info)\nFrank Palazzolo (Slapstic decoding)",
	&gaunt2p_machine_driver,

	gaunt2p_rom,
	gauntlet_rom_decode,
	0,
	0,
	0,	/* sound_prom */

	gauntlet_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	gauntlet_hiload, hisave
};


struct GameDriver gaunt2_driver =
{
	"Gauntlet 2",
	"gaunt2",
	"Aaron Giles (MAME driver)\nMike Balfour (graphics info)\nFrank Palazzolo (Slapstic decoding)",
	&gauntlet2_machine_driver,

	gaunt2_rom,
	gauntlet_rom_decode,
	0,
	0,
	0,	/* sound_prom */

	gauntlet_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	gauntlet2_hiload, hisave
};

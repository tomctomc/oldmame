/****************************************************************************

Master processor
0000-07ff   R/W     Z-page Working RAM
0800-08ff   R/W     NVRAM
0C00        R       Bit 7 = Right Coin
                    Bit 6 = Left Coin
                    Bit 5 = Aux Coin Switch
                    Bit 4 = Self Test
                    Bit 3 = Spare (High)
                    Bit 2 = Left thumb switch
                    Bit 1 = Right thumb switch
0C01        R       Bit 7 = VBLANK
                    Bit 6 = Sound CPU comm latch full flag
                    Bit 5 = Sound CPU ack latch flag
                    Bit 4 = Not used (High)
                    Bit 3 = Not used (High)
                    Bit 2 = Slam
                    Bit 1 = Not used (High)
                    Bit 0 = Not used (High)
1400        R       Sound CPU ack latch
1800        R       Read A/D conversion
1C00        W       Enable NVRAM
1C01        W       Disable NVRAM
1C80        W       Start A/D conversion (horizontal)
1C82        W       Start A/D conversion (vertical)
1D00        W       NVRAM store
1D80        W       Watchdog clear
1E00        W       IRQ ack
1E80        W       Left coin counter
1E81        W       Right coin counter
1E82        W       LED 1(not used)
1E83        W       LED 2(not used)
1E84        W       Alphanumeric ROM bank select
1E85        W       Not used
1E86        W       Sound CPU reset
1E87        W       Video off
1F00        W       Sound CPU comm latch
1F80        W       Bit 0..2: Program ROM bank select
2000-23FF   R/W     Scrolling playfield (low)
2400-27FF   R/W     Scrolling playfield (high)
2800-2BFF   R/W     Color RAM (low)
2C00-2FFF   R/W     Color RAM (high)
3000-37BF   R/W     Alphanumeric RAM

37C0-37EF   R/W     Motion object picture
3800-382F   R/W     Bit 6,2,1 = Motion object picture bank select
                    Bit 5 = Motion object vertical reflect
                    Bit 4 = Motion object horizontal reflect
                    Bit 3 = Motion object 32 pixels tall
                    Bit 0 = Motion object horizontal position (D8)
3840-386F   R/W     Motion object vertical position
38C0-38EF   R/W     Motion object horizontal position (D7-D0)
3C00-3C01   W       Scrolling playfield vertical position
3D00-3D01   W       Scrolling playfield horizontal position
3E00-3FFF   W       PIXI graphics expander RAM
4000-7FFF   R       Banked program ROM
8000-FFFF   R       Fixed program ROM

Sound processor
0000-07ff   R/W     Z-page Working RAM
0800-083f   R/W     Custom I/O (Quad Pokey)
1000        W       IRQ Ack
1100        W       Speech Data
1200        W       Speech write strobe on
1300        W       Speech write strobe off
1400        W       Main CPU ack latch
1500        W       Bit 0 = Speech chip reset
1800        R       Main CPU comm latch
1C00        R       Bit 7 = Speech chip ready
1C01        R       Bit 7 = Sound CPU comm latch full flag
            R       Bit 6 = Sound CPU ack latch full flag
8000-FFFF   R       Program ROM

****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* sndhrdw jedi.c */
void jedi_speech_w( int offset, int data);
int jedi_speech_ready_r( int offset );

/* vidhrdw jedi.c */
void jedi_paletteram_w(int offset,int data);
void jedi_backgroundram_w(int offset,int data);
int  jedi_vh_start(void);
void jedi_vh_stop(void);
void jedi_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
void jedi_vh_screenrefresh(struct osd_bitmap *bitmap);
void jedi_vscroll_w(int offset,int data);
void jedi_hscroll_w(int offset,int data);

extern unsigned char *jedi_PIXIRAM;
extern unsigned char *jedi_backgroundram;
extern int jedi_backgroundram_size;
extern unsigned char *jedi_paletteram;
unsigned char *jedi_nvRAM;

void jedi_soundlatch_w(int offset,int data);
void jedi_soundacklatch_w(int offset, int data);
int jedi_soundacklatch_r(int offset);
int jedi_soundlatch_r(int offset);
int jedi_soundstat_r(int offset);
int jedi_mainstat_r(int offset);
int jedi_control_r (int offset);
void jedi_control_w (int offset, int data);
void jedi_alpha_banksel (int offset, int data);
void jedi_sound_reset( int offset, int data);
void jedi_rom_banksel( int offset, int data);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
    { 0x0800, 0x08ff, MRA_RAM, &jedi_nvRAM },
    { 0x0C00, 0x0C00, input_port_0_r },
    { 0x0C01, 0x0C01, jedi_mainstat_r }, /* IN1 */
    { 0x1400, 0x1400, jedi_soundacklatch_r },
    { 0x1800, 0x1800, jedi_control_r },
    { 0x2000, 0x27FF, MRA_RAM },
    { 0x2800, 0x2FFF, MRA_RAM },
    { 0x3000, 0x37BF, MRA_RAM },
    { 0x37C0, 0x3BFF, MRA_RAM },
    { 0x4000, 0x7FFF, MRA_BANK1 },
    { 0x8000, 0xFFFF, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
    { 0x0000, 0x07ff, MWA_RAM },
    { 0x0800, 0x08ff, MWA_RAM, &jedi_nvRAM },
    { 0x1C80, 0x1C82, jedi_control_w},
    { 0x1D80, 0x1D80, watchdog_reset_w },
    { 0x1E84, 0x1E84, jedi_alpha_banksel },
    { 0x1E86, 0x1E86, jedi_sound_reset },
    { 0x1F00, 0x1F00, jedi_soundlatch_w },
    { 0x1F80, 0x1F80, jedi_rom_banksel },
    { 0x2000, 0x27FF, jedi_backgroundram_w, &jedi_backgroundram, &jedi_backgroundram_size },
    { 0x2800, 0x2FFF, jedi_paletteram_w, &jedi_paletteram },
    { 0x3000, 0x37BF, videoram_w, &videoram, &videoram_size },
    { 0x37C0, 0x3Bff, MWA_RAM, &spriteram, &spriteram_size },
    { 0x3C00, 0x3C01, jedi_vscroll_w },
    { 0x3D00, 0x3D01, jedi_hscroll_w },
    { 0x3E00, 0x3FFF, MWA_RAM, &jedi_PIXIRAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem2[] =
{
    { 0x0000, 0x07FF, MRA_RAM },
    { 0x0800, 0x080F, pokey1_r },
    { 0x0810, 0x081F, pokey2_r },
    { 0x0820, 0x082F, pokey3_r },
    { 0x0830, 0x083F, pokey4_r },
    { 0x1800, 0x1800, jedi_soundlatch_r },
    { 0x1C00, 0x1C00, jedi_speech_ready_r },
    { 0x1C01, 0x1C01, jedi_soundstat_r },
    { 0x8000, 0xFFFF, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem2[] =
{
	{ 0x0000, 0x07FF, MWA_RAM },
	{ 0x0800, 0x080F, pokey1_w },
	{ 0x0810, 0x081F, pokey2_w },
	{ 0x0820, 0x082F, pokey3_w },
	{ 0x0830, 0x083F, pokey4_w },
	{ 0x1100, 0x13FF, jedi_speech_w },
	{ 0x1400, 0x1400, jedi_soundacklatch_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( input_ports )
    PORT_START  /* IN0 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
    PORT_BIT (0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
    PORT_BITX(    0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x10, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

    PORT_START  /* IN1 */
    PORT_BIT( 0x03, IP_ACTIVE_HIGH, IPT_UNUSED )
    PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_TILT )
    PORT_BIT( 0x78, IP_ACTIVE_HIGH, IPT_UNUSED )
    PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

    PORT_START  /* IN2 */
    PORT_ANALOG ( 0xff, 0x80, IPT_AD_STICK_Y, 100, 0, 0, 255 )

    PORT_START  /* IN3 */
    PORT_ANALOG ( 0xff, 0x80, IPT_AD_STICK_X, 100, 0, 0, 255 )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
    512,    /* 512 characters */
    2,      /* 2 bits per pixel */
    { 0, 1 }, /* the bitplanes are packed in one nibble */
    { 0, 2, 4, 6, 8, 10, 12, 14 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8   /* every char takes 16 consecutive bytes */
};

static struct GfxLayout pflayout =
{
	16,16,	/* 16*16 characters (8x8 doubled) */
	2048,	/* 2048 characters */
	4,	/* 4 bits per pixel */
	{ 0, 4, 2048*16*8, 2048*16*8+4 },
	{ 0,0, 1,1, 2,2, 3,3, 8+0,8+0, 8+1,8+1, 8+2,8+2, 8+3,8+3 },
	{ 0*16,0*16, 1*16,1*16, 2*16,2*16, 3*16,3*16,
			4*16,4*16, 5*16,5*16, 6*16,6*16, 7*16,7*16,
			8*16,8*16, 9*16,9*16, 10*16,10*16, 11*16,11*16,
			12*16,12*16, 13*16,13*16, 14*16,14*16, 15*16,15*16 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	8,16,	/* 8*16 sprites */
	2048,	/* 2048 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 4, 2048*32*8, 2048*32*8+4 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3},
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	32*8	/* every sprite takes 32 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
    { 1, 0x00000, &charlayout,      0, 1 },
    { 1, 0x02000, &pflayout,        4, 1 },
    { 1, 0x12000, &spritelayout, 4+16, 1 },
	{ -1 }
};



static struct POKEYinterface pokey_interface =
{
    4,  /* 4 chips */
	1500000,	/* 1.5 MHz? */
    127,
    POKEY_DEFAULT_GAIN/8,
	NO_CLIP,
	/* The 8 pot handlers */
    { 0, 0 ,0 ,0},
    { 0, 0 ,0 ,0},
    { 0, 0 ,0 ,0},
    { 0, 0 ,0 ,0},
    { 0, 0 ,0 ,0},
    { 0, 0 ,0 ,0},
    { 0, 0 ,0 ,0},
    { 0, 0 ,0 ,0},
	/* The allpot handler */
    { 0,0,0,0 }
};

static struct TMS5220interface tms5220_interface =
{
    672000,     /* clock speed (80*samplerate) */
    255,        /* volume */
    0           /* IRQ handler */
};



static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
            2500000,    /* 2.5 Mhz */
			0,
			readmem,writemem,0,0,
            interrupt,4
		},
		{
            CPU_M6502,
            1500000,        /* 1.5 Mhz */
            2,
			readmem2,writemem2,0,0,
            interrupt,4
		}
	},
    60, DEFAULT_REAL_60HZ_VBLANK_DURATION,   /* frames per second, vblank duration */
    4,  /* 4 cycles per frame - enough for the two CPUs to properly synchronize */
	0,

	/* video hardware */
    37*8, 30*8, { 0*8, 37*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
    256,4+16+16,
    jedi_vh_convert_color_prom,

    VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
    jedi_vh_start,
    jedi_vh_stop,
    jedi_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
    {
		{
			SOUND_POKEY,
			&pokey_interface
		},
		{
			SOUND_TMS5220,
			&tms5220_interface
		}
    }

};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( jedi_rom )
	ROM_REGION(0x1C000)	/* 64k for code + 48k for banked ROMs */
	ROM_LOAD( "14f_221.bin", 0x08000, 0x4000, 0x81b8d806 )
	ROM_LOAD( "13f_222.bin", 0x0c000, 0x4000, 0x4ecc94fc )
	ROM_LOAD( "13d_123.bin", 0x10000, 0x4000, 0xe7508418 )	/* Page 0 */
	ROM_LOAD( "13b_124.bin", 0x14000, 0x4000, 0x3b502580 )	/* Page 1 */
	ROM_LOAD( "13a_122.bin", 0x18000, 0x4000, 0xfcf2c162 )	/* Page 2 */

	ROM_REGION(0x32000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "11t_215.bin", 0x00000, 0x2000, 0x9a6e2e9e )	/* Alphanumeric */
	ROM_LOAD( "06r_126.bin", 0x02000, 0x8000, 0xb751b39f )	/* Playfield */
	ROM_LOAD( "06n_127.bin", 0x0a000, 0x8000, 0x72c11703 )
	ROM_LOAD( "01h_130.bin", 0x12000, 0x8000, 0xc10a8a70 )	/* Sprites */
	ROM_LOAD( "01f_131.bin", 0x1a000, 0x8000, 0x60681a2c )
	ROM_LOAD( "01m_128.bin", 0x22000, 0x8000, 0x1b26b546 )
	ROM_LOAD( "01k_129.bin", 0x2a000, 0x8000, 0x7d487088 )

	ROM_REGION(0x10000)	/* space for the sound ROMs */
	ROM_LOAD( "01c_133.bin", 0x8000, 0x4000, 0x73521580 )
	ROM_LOAD( "01a_134.bin", 0xC000, 0x4000, 0x3c5aa3aa )
ROM_END

static int novram_load(void)
{
/* get RAM pointer (if game is multiCPU, we can't assume the global */
/* RAM pointer is pointing to the right place) */
unsigned char *RAM = Machine->memory_region[0];

	void *f;
	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
	{
        osd_fread(f,&RAM[0x0800],256);
		osd_fclose(f);
	}
	return 1;
}

static void novram_save(void)
{
	void *f;
	/* get RAM pointer (if game is multiCPU, we can't assume the global */
	/* RAM pointer is pointing to the right place) */
	unsigned char *RAM = Machine->memory_region[0];

	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
        osd_fwrite(f,&RAM[0x0800],256);
		osd_fclose(f);
	}
}


struct GameDriver jedi_driver =
{
	"Return of the Jedi",
	"jedi",
	"Dan Boris",
	&machine_driver,

	jedi_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	input_ports,

	0, 0, 0,
	ORIENTATION_DEFAULT,

	novram_load, novram_save /* Highscore load, save */
};

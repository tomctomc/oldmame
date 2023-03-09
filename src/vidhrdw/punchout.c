/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


#define TOP_MONITOR_ROWS 30
#define BOTTOM_MONITOR_ROWS 30

#define BIGSPRITE_WIDTH 128
#define BIGSPRITE_HEIGHT 256

unsigned char *punchout_videoram2;
int punchout_videoram2_size;
unsigned char *punchout_bigsprite1ram;
int punchout_bigsprite1ram_size;
unsigned char *punchout_bigsprite2ram;
int punchout_bigsprite2ram_size;
unsigned char *punchout_scroll;
unsigned char *punchout_bigsprite1;
unsigned char *punchout_bigsprite2;
static unsigned char *dirtybuffer2,*bs1dirtybuffer,*bs2dirtybuffer;
static struct osd_bitmap *bs1tmpbitmap,*bs2tmpbitmap;

static int top_palette_bank,bottom_palette_bank;

static struct rectangle topvisiblearea =
{
	0*8, 32*8-1,
	0*8, (TOP_MONITOR_ROWS-2)*8-1
};
static struct rectangle bottomvisiblearea =
{
	0*8, 32*8-1,
	(TOP_MONITOR_ROWS+2)*8, (TOP_MONITOR_ROWS+BOTTOM_MONITOR_ROWS)*8-1
};
static struct rectangle backgroundvisiblearea =
{
	0*8, 64*8-1,
	(TOP_MONITOR_ROWS+2)*8, (TOP_MONITOR_ROWS+BOTTOM_MONITOR_ROWS)*8-1
};



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Punch Out has a six 512x4 palette PROMs (one per gun; three for the top
  monitor chars, three for everything else).
  I don't know the exact values of the resistors between the RAM and the
  RGB output. I assumed these values (the same as Commando)

  bit 3 -- 220 ohm resistor -- inverter  -- RED/GREEN/BLUE
        -- 470 ohm resistor -- inverter  -- RED/GREEN/BLUE
        -- 1  kohm resistor -- inverter  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor -- inverter  -- RED/GREEN/BLUE

***************************************************************************/
void punchout_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom)
{
	int i,j,used;
	unsigned char allocated[3*256];
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + (offs)])


	/* The game has 1024 colors, but we are limited to a maximum of 256. */
	/* Luckily, many of the colors are duplicated, so the total number of */
	/* different colors is less than 256. We select the unique colors and */
	/* put them in our palette. */

	memset(palette,0,3 * Machine->drv->total_colors);

	/* transparent black */
	allocated[0] = 0xff;
	allocated[1] = 0xff;
	allocated[2] = 0xff;
	palette[0] = 0;
	palette[1] = 0;
	palette[2] = 0;
	/* non transparent black */
	allocated[3] = 0xff;
	allocated[4] = 0xff;
	allocated[5] = 0xff;
	palette[3] = 0;
	palette[4] = 0;
	palette[5] = 0;
	used = 2;

	for (i = 0;i < 1024;i++)
	{
		for (j = 0;j < used;j++)
		{
			if (allocated[j] == color_prom[i] &&
					allocated[j+256] == color_prom[i+1024] &&
					allocated[j+2*256] == color_prom[i+2*1024])
				break;
		}
		if (j == used)
		{
			int bit0,bit1,bit2,bit3;


			used++;

			allocated[j] = color_prom[i];
			allocated[j+256] = color_prom[i+1024];
			allocated[j+2*256] = color_prom[i+2*1024];

			bit0 = (color_prom[i] >> 0) & 0x01;
			bit1 = (color_prom[i] >> 1) & 0x01;
			bit2 = (color_prom[i] >> 2) & 0x01;
			bit3 = (color_prom[i] >> 3) & 0x01;
			palette[3*j] = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);
			bit0 = (color_prom[i+1024] >> 0) & 0x01;
			bit1 = (color_prom[i+1024] >> 1) & 0x01;
			bit2 = (color_prom[i+1024] >> 2) & 0x01;
			bit3 = (color_prom[i+1024] >> 3) & 0x01;
			palette[3*j + 1] = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);
			bit0 = (color_prom[i+2*1024] >> 0) & 0x01;
			bit1 = (color_prom[i+2*1024] >> 1) & 0x01;
			bit2 = (color_prom[i+2*1024] >> 2) & 0x01;
			bit3 = (color_prom[i+2*1024] >> 3) & 0x01;
			palette[3*j + 2] = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);
		}

		if (i < 512)
		{
			/* top monitor chars - palette order is inverted */
			COLOR(0,i ^ 0x03) = j;
		}
		else
		{
			int ii,jj;


			ii = i - 512;

			/* bottom monitor chars */
			COLOR(1,ii) = j;

			/* big sprite #1 - palette order is inverted */
			jj = j;
			if (ii % 8 == 0) jj = 0;	/* preserve transparency */
			else if (jj == 0) jj = 1;	/* avoid undesired transparency */
			COLOR(2,ii ^ 0x07) = jj;

			/* big sprite #2 */
			jj = j;
			if (ii % 4 == 0) jj = 0;	/* preserve transparency */
			else if (jj == 0) jj = 1;	/* avoid undesired transparency */
			COLOR(3,ii) = jj;
		}
	}
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int punchout_vh_start(void)
{
	if ((dirtybuffer = malloc(videoram_size)) == 0)
		return 1;
	memset(dirtybuffer,1,videoram_size);

	if ((dirtybuffer2 = malloc(punchout_videoram2_size)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}
	memset(dirtybuffer2,1,punchout_videoram2_size);

	if ((tmpbitmap = osd_create_bitmap(512,480)) == 0)
	{
		free(dirtybuffer);
		free(dirtybuffer2);
		return 1;
	}

	if ((bs1dirtybuffer = malloc(punchout_bigsprite1ram_size)) == 0)
	{
		osd_free_bitmap(tmpbitmap);
		free(dirtybuffer);
		free(dirtybuffer2);
		return 1;
	}
	memset(bs1dirtybuffer,1,punchout_bigsprite1ram_size);

	if ((bs1tmpbitmap = osd_create_bitmap(BIGSPRITE_WIDTH,BIGSPRITE_HEIGHT)) == 0)
	{
		osd_free_bitmap(tmpbitmap);
		free(dirtybuffer);
		free(dirtybuffer2);
		free(bs1dirtybuffer);
		return 1;
	}

	if ((bs2dirtybuffer = malloc(punchout_bigsprite2ram_size)) == 0)
	{
		osd_free_bitmap(tmpbitmap);
		osd_free_bitmap(bs1tmpbitmap);
		free(dirtybuffer);
		free(dirtybuffer2);
		free(bs1dirtybuffer);
		return 1;
	}
	memset(bs2dirtybuffer,1,punchout_bigsprite2ram_size);

	if ((bs2tmpbitmap = osd_create_bitmap(BIGSPRITE_WIDTH,BIGSPRITE_HEIGHT)) == 0)
	{
		osd_free_bitmap(tmpbitmap);
		osd_free_bitmap(bs1tmpbitmap);
		free(dirtybuffer);
		free(dirtybuffer2);
		free(bs1dirtybuffer);
		free(bs2dirtybuffer);
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void punchout_vh_stop(void)
{
	free(dirtybuffer);
	free(dirtybuffer2);
	free(bs1dirtybuffer);
	free(bs2dirtybuffer);
	osd_free_bitmap(tmpbitmap);
	osd_free_bitmap(bs1tmpbitmap);
	osd_free_bitmap(bs2tmpbitmap);
}



void punchout_videoram2_w(int offset,int data)
{
	if (punchout_videoram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		punchout_videoram2[offset] = data;
	}
}

void punchout_bigsprite1ram_w(int offset,int data)
{
	if (punchout_bigsprite1ram[offset] != data)
	{
		bs1dirtybuffer[offset] = 1;

		punchout_bigsprite1ram[offset] = data;
	}
}

void punchout_bigsprite2ram_w(int offset,int data)
{
	if (punchout_bigsprite2ram[offset] != data)
	{
		bs2dirtybuffer[offset] = 1;

		punchout_bigsprite2ram[offset] = data;
	}
}



void punchout_palettebank_w(int offset,int data)
{
	if (top_palette_bank != ((data >> 1) & 0x01))
	{
		top_palette_bank = (data >> 1) & 0x01;
		memset(dirtybuffer,1,videoram_size);
	}
	if (bottom_palette_bank != ((data >> 0) & 0x01))
	{
		bottom_palette_bank = (data >> 0) & 0x01;
		memset(dirtybuffer2,1,punchout_videoram2_size);
		memset(bs1dirtybuffer,1,punchout_bigsprite1ram_size);
		memset(bs2dirtybuffer,1,punchout_bigsprite2ram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void punchout_vh_screenrefresh(struct osd_bitmap *bitmap)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 2;offs >= 0;offs -= 2)
	{
		if (dirtybuffer[offs] || dirtybuffer[offs + 1])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;
			dirtybuffer[offs + 1] = 0;

			sx = offs/2 % 32;
			sy = offs/2 / 32;

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 256 * (videoram[offs + 1] & 0x03),
					((videoram[offs + 1] & 0x7c) >> 2) + 64 * top_palette_bank,
					videoram[offs + 1] & 0x80,0,
					8*sx,8*sy - 8*(32-TOP_MONITOR_ROWS),
					&topvisiblearea,TRANSPARENCY_NONE,0);
		}
	}

	for (offs = punchout_videoram2_size - 2;offs >= 0;offs -= 2)
	{
		if (dirtybuffer2[offs] | dirtybuffer2[offs + 1])
		{
			int sx,sy;


			dirtybuffer2[offs] = 0;
			dirtybuffer2[offs + 1] = 0;

			sx = offs/2 % 64;
			sy = offs/2 / 64;

			drawgfx(tmpbitmap,Machine->gfx[1],
					punchout_videoram2[offs] + 256 * (punchout_videoram2[offs + 1] & 0x03),
					32 + ((~punchout_videoram2[offs + 1] & 0x7c) >> 2) + 64 * bottom_palette_bank,
					punchout_videoram2[offs + 1] & 0x80,0,
					8*sx,8*sy + 8*TOP_MONITOR_ROWS,
					&backgroundvisiblearea,TRANSPARENCY_NONE,0);
		}
	}

	for (offs = punchout_bigsprite1ram_size - 4;offs >= 0;offs -= 4)
	{
		if (bs1dirtybuffer[offs] | bs1dirtybuffer[offs + 1] | bs1dirtybuffer[offs + 3])
		{
			int sx,sy;


			bs1dirtybuffer[offs] = 0;
			bs1dirtybuffer[offs + 1] = 0;
			bs1dirtybuffer[offs + 3] = 0;

			sx = offs/4 % 16;
			sy = offs/4 / 16;

			drawgfx(bs1tmpbitmap,Machine->gfx[2],
					punchout_bigsprite1ram[offs] + 256 * (punchout_bigsprite1ram[offs + 1] & 0x1f),
					(~punchout_bigsprite1ram[offs + 3] & 0x1f) + 32 * bottom_palette_bank,
					punchout_bigsprite1ram[offs + 3] & 0x80,0,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}

	for (offs = punchout_bigsprite2ram_size - 4;offs >= 0;offs -= 4)
	{
		if (bs2dirtybuffer[offs] | bs2dirtybuffer[offs + 1] | bs2dirtybuffer[offs + 3])
		{
			int sx,sy;


			bs2dirtybuffer[offs] = 0;
			bs2dirtybuffer[offs + 1] = 0;
			bs2dirtybuffer[offs + 3] = 0;

			sx = offs/4 % 16;
			sy = offs/4 / 16;

			drawgfx(bs2tmpbitmap,Machine->gfx[3],
					punchout_bigsprite2ram[offs] + 256 * (punchout_bigsprite2ram[offs + 1] & 0x0f),
					(~punchout_bigsprite2ram[offs + 3] & 0x3f) + 64 * bottom_palette_bank,
					punchout_bigsprite2ram[offs + 3] & 0x80,0,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the character mapped graphics */
	{
		int scroll[64];


		for (offs = 0;offs < TOP_MONITOR_ROWS;offs++)
			scroll[offs] = 0;
		for (offs = 0;offs < BOTTOM_MONITOR_ROWS;offs++)
			scroll[TOP_MONITOR_ROWS + offs] = -(58 + punchout_scroll[2*offs] + 256 * (punchout_scroll[2*offs + 1] & 0x01));

		copyscrollbitmap(bitmap,tmpbitmap,TOP_MONITOR_ROWS + BOTTOM_MONITOR_ROWS,scroll,0,0,&Machine->drv->visible_area,TRANSPARENCY_NONE,0);
	}

	/* copy the two big sprites */
	{
		int sx,sy,zoom,height;


		zoom = punchout_bigsprite1[0] + 256 * (punchout_bigsprite1[1] & 0x0f);
		if (zoom)
		{
			sx = 1024 - (punchout_bigsprite1[2] + 256 * (punchout_bigsprite1[3] & 0x0f)) / 4;
			if (sx > 1024-127) sx -= 1024;
			sx = sx * 0x1000 / zoom / 4;	/* adjust x position basing on zoom */
			sx -= 57;	/* adjustment to match the screen shots */

			sy = -punchout_bigsprite1[4] + 256 * (punchout_bigsprite1[5] & 1);
			sy = sy * 0x1000 / zoom / 4;	/* adjust y position basing on zoom */

			/* when the sprite is reduced, it fits more than */
			/* once in the screen, so if the first draw is */
			/* offscreen the second can be visible */
			height = 256 * 0x1000 / zoom / 4;	/* height of the zoomed sprite */
			if (sy <= -height+16) sy += 2*height;	/* if offscreen, try moving it lower */

			sy += 3;	/* adjustment to match the screen shots */
				/* have to be at least 3, using 2 creates a blank line at the bottom */
				/* of the screen when you win the championship and jump around with */
				/* the belt */

			if (punchout_bigsprite1[7] & 1)	/* display in top monitor */
			{
				copybitmapzoom(bitmap,bs1tmpbitmap,
						punchout_bigsprite1[6] & 1,0,
						sx,sy - 8*(32-TOP_MONITOR_ROWS),
						&topvisiblearea,TRANSPARENCY_COLOR,0,
						0x10000 * 0x1000 / zoom / 4,0x10000 * 0x1000 / zoom / 4);
			}
			if (punchout_bigsprite1[7] & 2)	/* display in bottom monitor */
			{
				copybitmapzoom(bitmap,bs1tmpbitmap,
						punchout_bigsprite1[6] & 1,0,
						sx,sy + 8*TOP_MONITOR_ROWS,
						&bottomvisiblearea,TRANSPARENCY_COLOR,0,
						0x10000 * 0x1000 / zoom / 4,0x10000 * 0x1000 / zoom / 4);
			}
		}
	}
	{
		int sx,sy;


		sx = 512 - (punchout_bigsprite2[0] + 256 * (punchout_bigsprite2[1] & 1));
		if (sx > 512-127) sx -= 512;
		sx -= 55;	/* adjustment to match the screen shots */

		sy = -punchout_bigsprite2[2] + 256 * (punchout_bigsprite2[3] & 1);
		sy += 3;	/* adjustment to match the screen shots */

		copybitmap(bitmap,bs2tmpbitmap,
				punchout_bigsprite2[4] & 1,0,
				sx,sy + 8*TOP_MONITOR_ROWS,
				&bottomvisiblearea,TRANSPARENCY_COLOR,0);
	}
}

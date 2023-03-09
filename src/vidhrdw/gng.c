/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *gng_paletteram;

unsigned char *gng_bgvideoram,*gng_bgcolorram;
int gng_bgvideoram_size;
unsigned char *gng_scrollx, *gng_scrolly;
static unsigned char *dirtybuffer2;
static struct osd_bitmap *tmpbitmap2;
static int flipscreen;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Ghosts 'n Goblins doesn't have color PROMs, it uses RAM instead.

  I don't know the exact values of the resistors between the RAM and the
  RGB output. I assumed these values (the same as Commando)
  bit 7 -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
        -- 1  kohm resistor  -- RED
        -- 2.2kohm resistor  -- RED
        -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
  bit 0 -- 2.2kohm resistor  -- GREEN

  bit 7 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 1  kohm resistor  -- BLUE
        -- 2.2kohm resistor  -- BLUE
        -- unused
        -- unused
        -- unused
  bit 0 -- unused

***************************************************************************/
void gng_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom)
{
	int i;


	/* the palette will be initialized by the game. We just set it to some */
	/* pre-cooked values so the startup copyright notice can be displayed. */
	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		*(palette++) = ((i & 1) >> 0) * 0xff;
		*(palette++) = ((i & 2) >> 1) * 0xff;
		*(palette++) = ((i & 4) >> 2) * 0xff;
	}

	/* initialize the color table */
	for (i = 0;i < Machine->drv->color_table_len;i++)
		colortable[i] = i;
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int gng_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((dirtybuffer2 = malloc(gng_bgvideoram_size)) == 0)
	{
		generic_vh_stop();
		return 1;
	}
	memset(dirtybuffer2,1,gng_bgvideoram_size);

	/* the background area is twice as tall and twice as large as the screen */
	if ((tmpbitmap2 = osd_create_bitmap(2*Machine->drv->screen_width,2*Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer2);
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void gng_vh_stop(void)
{
	osd_free_bitmap(tmpbitmap2);
	free(dirtybuffer2);
	generic_vh_stop();
}



void gng_bgvideoram_w(int offset,int data)
{
	if (gng_bgvideoram[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		gng_bgvideoram[offset] = data;
	}
}



void gng_bgcolorram_w(int offset,int data)
{
	if (gng_bgcolorram[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		gng_bgcolorram[offset] = data;
	}
}



void gng_paletteram_w(int offset,int data)
{
	int bit0,bit1,bit2,bit3;
	int r,g,b,val;

	gng_paletteram[offset] = data;

	val = gng_paletteram[offset & ~0x100];
	bit0 = (val >> 4) & 0x01;
	bit1 = (val >> 5) & 0x01;
	bit2 = (val >> 6) & 0x01;
	bit3 = (val >> 7) & 0x01;
	r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	val = gng_paletteram[offset | 0x100];
	bit0 = (val >> 4) & 0x01;
	bit1 = (val >> 5) & 0x01;
	bit2 = (val >> 6) & 0x01;
	bit3 = (val >> 7) & 0x01;
	b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	osd_modify_pen(Machine->pens[(offset & ~0x100)],r,g,b);
}



void gng_flipscreen_w(int offset,int data)
{
	if (flipscreen != (~data & 1))
	{
		flipscreen = ~data & 1;
		memset(dirtybuffer2,1,gng_bgvideoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void gng_vh_screenrefresh(struct osd_bitmap *bitmap)
{
	int offs;


	for (offs = gng_bgvideoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer2[offs])
		{
			int sx,sy,flipx,flipy;


			dirtybuffer2[offs] = 0;

			sx = offs / 32;
			sy = offs % 32;
			flipx = gng_bgcolorram[offs] & 0x10;
			flipy = gng_bgcolorram[offs] & 0x20;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap2,Machine->gfx[1],
					gng_bgvideoram[offs] + 4*(gng_bgcolorram[offs] & 0xc0),
					gng_bgcolorram[offs] & 0x07,
					flipx,flipy,
					16 * sx,16 * sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the background graphics */
	{
		int scrollx,scrolly;


		scrollx = -(gng_scrollx[0] + 256 * gng_scrollx[1]);
		scrolly = -(gng_scrolly[0] + 256 * gng_scrolly[1]);
		if (flipscreen)
		{
			scrollx = 256 - scrollx;
			scrolly = 256 - scrolly;
		}

		copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,1,&scrolly,&Machine->drv->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int sx,sy,flipx,flipy,bank;


		sx = spriteram[offs + 3] - 0x100 * (spriteram[offs + 1] & 0x01);
		sy = spriteram[offs + 2];
		flipx = spriteram[offs + 1] & 0x04;
		flipy = spriteram[offs + 1] & 0x08;
		bank = ((spriteram[offs + 1] >> 6) & 3);

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (bank < 3)
			drawgfx(bitmap,Machine->gfx[2],
					spriteram[offs] + 256* bank,
					(spriteram[offs + 1] >> 4) & 3,
					flipx,flipy,
					sx,sy,
					&Machine->drv->visible_area,TRANSPARENCY_PEN,15);
	}


	/* redraw the background tiles which have priority over sprites */
	{
		int scrollx,scrolly;


		scrollx = -(gng_scrollx[0] + 256 * gng_scrollx[1]);
		scrolly = -(gng_scrolly[0] + 256 * gng_scrolly[1]);
		if (flipscreen)
		{
			scrollx = 256 - scrollx;
			scrolly = 256 - scrolly;
		}

		for (offs = gng_bgvideoram_size - 1;offs >= 0;offs--)
		{
			if (gng_bgcolorram[offs] & 0x08)
			{
				int sx,sy,flipx,flipy;


				sx = offs / 32;
				sy = offs % 32;
				flipx = gng_bgcolorram[offs] & 0x10;
				flipy = gng_bgcolorram[offs] & 0x20;
				if (flipscreen)
				{
					sx = 31 - sx;
					sy = 31 - sy;
					flipx = !flipx;
					flipy = !flipy;
				}
				sx = ((16 * sx + scrollx + 16) & 0x1ff) - 16;
				sy = ((16 * sy + scrolly + 16) & 0x1ff) - 16;

				drawgfx(bitmap,Machine->gfx[1],
						gng_bgvideoram[offs] + 256*((gng_bgcolorram[offs] >> 6) & 0x03),
						gng_bgcolorram[offs] & 0x07,
						flipx,flipy,
						sx,sy,
						&Machine->drv->visible_area,TRANSPARENCY_PEN,0);
			}
		}
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy,flipx,flipy;


		sx = offs % 32;
		sy = offs / 32;
		flipx = colorram[offs] & 0x10;	/* ? */
		flipy = colorram[offs] & 0x20;	/* ? */

		if (flipscreen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				videoram[offs] + 4 * (colorram[offs] & 0xc0),
				colorram[offs] & 0x0f,
				flipx,flipy,
				8*sx,8*sy,
				&Machine->drv->visible_area,TRANSPARENCY_PEN,3);
	}
}

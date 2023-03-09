/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



static int gfxbank;
static unsigned char *superqix_palette;
static unsigned char *superqix_bitmapram,*superqix_bitmapram2;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Super Qix doesn't have colors PROMs, it uses RAM. The meaning of the bits are
  bit 7 -- Blue
        -- Blue
        -- Green
        -- Green
        -- Red
        -- Red
        -- Intensity
  bit 0 -- Intensity

***************************************************************************/
void superqix_vh_convert_color_prom(unsigned char *palette,unsigned char *colortable,const unsigned char *color_prom)
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
	for (i = 0;i < Machine->drv->total_colors;i++)
		colortable[i] = i;
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int superqix_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((superqix_palette = malloc(256 * sizeof(unsigned char))) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	if ((superqix_bitmapram = malloc(0x7000 * sizeof(unsigned char))) == 0)
	{
		free(superqix_palette);
		generic_vh_stop();
		return 1;
	}

	if ((superqix_bitmapram2 = malloc(0x7000 * sizeof(unsigned char))) == 0)
	{
		free(superqix_bitmapram);
		free(superqix_palette);
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void superqix_vh_stop(void)
{
	free(superqix_bitmapram2);
	free(superqix_bitmapram);
	free(superqix_palette);
	generic_vh_stop();
}



int superqix_palette_r(int offset)
{
	return superqix_palette[offset];
}

void superqix_palette_w(int offset,int data)
{
	int bits,intensity,r,g,b;


	superqix_palette[offset] = data;

	intensity = (data >> 0) & 0x03;
	/* red component */
	bits = (data >> 2) & 0x03;
	r = 0x44 * bits + 0x11 * intensity;
	/* green component */
	bits = (data >> 4) & 0x03;
	g = 0x44 * bits + 0x11 * intensity;
	/* blue component */
	bits = (data >> 6) & 0x03;
	b = 0x44 * bits + 0x11 * intensity;

	osd_modify_pen(Machine->pens[offset],r,g,b);
}



int superqix_bitmapram_r(int offset)
{
	return superqix_bitmapram[offset];
}

void superqix_bitmapram_w(int offset,int data)
{
	superqix_bitmapram[offset] = data;
}

int superqix_bitmapram2_r(int offset)
{
	return superqix_bitmapram2[offset];
}

void superqix_bitmapram2_w(int offset,int data)
{
	superqix_bitmapram2[offset] = data;
}



void superqix_0410_w(int offset,int data)
{
	int bankaddress;


	/* bits 0-1 select the tile bank */
	if (gfxbank != (data & 0x03))
	{
		gfxbank = data & 0x03;
		memset(dirtybuffer,1,videoram_size);
	}

	/* bit 3 enables NMI */
	interrupt_enable_w(offset,data & 0x08);

	/* bits 4-5 control ROM bank */
	bankaddress = 0x10000 + ((data & 0x30) >> 4) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void superqix_vh_screenrefresh(struct osd_bitmap *bitmap)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			drawgfx(tmpbitmap,Machine->gfx[(colorram[offs] & 0x04) ? 0 : (1 + gfxbank)],
					videoram[offs] + 256 * (colorram[offs] & 0x03),
					(colorram[offs] & 0xf0) >> 4,
					0,0,
					8*sx,8*sy,
					&Machine->drv->visible_area,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->drv->visible_area,TRANSPARENCY_NONE,0);

{
	int x,y;


	for (y = 0;y < 224;y++)
	{
		for (x = 0;x < 128;x++)
		{
			int sx,sy,d;


			d = superqix_bitmapram[y*128+x];
			/* TODO: bitmapram2 is used for player 2 in cocktail mode */

			if (d & 0xf0)
			{
				sx = 2*x;
				sy = y+16;
				if (Machine->orientation & ORIENTATION_SWAP_XY)
				{
					int temp;


					temp = sx;
					sx = sy;
					sy = temp;
				}
				if (Machine->orientation & ORIENTATION_FLIP_X)
					sx = bitmap->width - 1 - sx;
				if (Machine->orientation & ORIENTATION_FLIP_Y)
					sy = bitmap->height - 1 - sy;

				bitmap->line[sy][sx] = Machine->pens[d >> 4];
			}

			if (d & 0x0f)
			{
				sx = 2*x + 1;
				sy = y+16;
				if (Machine->orientation & ORIENTATION_SWAP_XY)
				{
					int temp;


					temp = sx;
					sx = sy;
					sy = temp;
				}
				if (Machine->orientation & ORIENTATION_FLIP_X)
					sx = bitmap->width - 1 - sx;
				if (Machine->orientation & ORIENTATION_FLIP_Y)
					sy = bitmap->height - 1 - sy;

				bitmap->line[sy][sx] = Machine->pens[d & 0x0f];
			}
		}
	}
}


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		/* TODO: I haven't looked for the flip bits, but they are there, and used e.g. */
		/* in the animation at the end of round 5 */
		drawgfx(bitmap,Machine->gfx[5],
				spriteram[offs] + 256 * (spriteram[offs + 3] & 0x01),
				(spriteram[offs + 3] & 0xf0) >> 4,
				0,0,
				spriteram[offs + 1],spriteram[offs + 2],
				&Machine->drv->visible_area,TRANSPARENCY_PEN,0);
	}


	/* redraw characters which have priority over the bitmap */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (colorram[offs] & 0x08)
		{
			int sx,sy;


			sx = offs % 32;
			sy = offs / 32;

			drawgfx(bitmap,Machine->gfx[(colorram[offs] & 0x04) ? 0 : 1],
					videoram[offs] + 256 * (colorram[offs] & 0x03),
					(colorram[offs] & 0xf0) >> 4,
					0,0,
					8*sx,8*sy,
					&Machine->drv->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}

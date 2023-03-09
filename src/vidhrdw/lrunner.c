/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *lrunner_scroll_low;
unsigned char *lrunner_scroll_high;


static struct rectangle spritevisiblearea =
{
	0*8, 48*8-1,
	1*8, 32*8-1
};



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Kung Fu Master has a six 256x4 palette PROMs (one per gun; three for
  characters, three for sprites).
  I don't know the exact values of the resistors between the RAM and the
  RGB output. I assumed these values (the same as Commando)

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void lrunner_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom)
{
	int i,j,used;
	unsigned char allocated[3*256];


	/* The game has 512 colors, but we are limited to a maximum of 256. */
	/* Luckily, many of the colors are duplicated, so the total number of */
	/* different colors is less than 256. We select the unique colors and */
	/* put them in our palette. */

	memset(palette,0,3 * Machine->drv->total_colors);

	used = 0;
	for (i = 0;i < 512;i++)
	{
		for (j = 0;j < used;j++)
		{
			if (allocated[j] == color_prom[i] &&
					allocated[j+256] == color_prom[i+512] &&
					allocated[j+2*256] == color_prom[i+2*512])
				break;
		}
		if (j == used)
		{
			int bit0,bit1,bit2,bit3;


			used++;

			allocated[j] = color_prom[i];
			allocated[j+256] = color_prom[i+512];
			allocated[j+2*256] = color_prom[i+2*512];

			bit0 = (color_prom[i] >> 0) & 0x01;
			bit1 = (color_prom[i] >> 1) & 0x01;
			bit2 = (color_prom[i] >> 2) & 0x01;
			bit3 = (color_prom[i] >> 3) & 0x01;
			palette[3*j] = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
			bit0 = (color_prom[i+512] >> 0) & 0x01;
			bit1 = (color_prom[i+512] >> 1) & 0x01;
			bit2 = (color_prom[i+512] >> 2) & 0x01;
			bit3 = (color_prom[i+512] >> 3) & 0x01;
			palette[3*j + 1] = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
			bit0 = (color_prom[i+512*2] >> 0) & 0x01;
			bit1 = (color_prom[i+512*2] >> 1) & 0x01;
			bit2 = (color_prom[i+512*2] >> 2) & 0x01;
			bit3 = (color_prom[i+512*2] >> 3) & 0x01;
			palette[3*j + 2] = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		}

		colortable[i] = j;
	}
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int lrunner_vh_start(void)
{
	if ((dirtybuffer = malloc(videoram_size)) == 0)
		return 1;
	memset(dirtybuffer,1,videoram_size);

	if ((tmpbitmap = osd_create_bitmap(2 * Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void lrunner_vh_stop(void)
{
	free(dirtybuffer);
        osd_free_bitmap(tmpbitmap);
}





/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void lrunner_vh_screenrefresh(struct osd_bitmap *bitmap)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = 0 ; offs < videoram_size ; offs += 2 )
	{
		if ( (dirtybuffer[offs]) || (dirtybuffer[offs+1]) )
		{
			int sx,sy,flipx,flipy;


			dirtybuffer[offs] = 0;
			dirtybuffer[offs+1] = 0;

			sx = ( offs % 128 ) / 2 - 8 ;
			sy = offs / 128 ;
			flipx = 0;
			flipy = 0;

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 4 * (videoram[offs+1] & 0xc0) ,
					videoram[offs+1] & 0x1F ,
					0,0,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->drv->visible_area,TRANSPARENCY_NONE,0);


	/* Draw the sprites. */
	for (offs = 0;offs < spriteram_size;offs += 8)
	{
		int code,col,flipx,flipy,sx,sy;


		code = spriteram[offs+4];

		if (code != 0)
		{
			col = spriteram[offs+0] & 0x1f;
			sx = (256 * spriteram[offs+7] + spriteram[offs+6]) - 64,
			sy = 256+128-15 - (256 * spriteram[offs+3] + spriteram[offs+2]),
			flipx = spriteram[offs+5] & 0x40;
			flipy = spriteram[offs+5] & 0x80;

			if ( code < 72 )
				sy = sy - 16 ;

			drawgfx(bitmap,Machine->gfx[1],
					code,col,
					flipx,flipy,
					sx,sy,
					&spritevisiblearea,TRANSPARENCY_PEN,0);

			if ( code < 72 )
			{
				drawgfx(bitmap,Machine->gfx[1],
						code+1,col,
						flipx,flipy,
						sx,sy+16,
						&spritevisiblearea,TRANSPARENCY_PEN,0);
			}
		}
	}
}

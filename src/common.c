/*********************************************************************

  common.c

  Generic functions, mostly ROM and graphics related.

*********************************************************************/

#include "driver.h"

/* LBO */
#ifdef LSB_FIRST
#define intelLong(x) (x)
#define BL0 0
#define BL1 1
#define BL2 2
#define BL3 3
#define WL0 0
#define WL1 1
#else
#define intelLong(x) (((x << 24) | (((unsigned long) x) >> 24) | (( x & 0x0000ff00) << 8) | (( x & 0x00ff0000) >> 8)))
#define BL0 3
#define BL1 2
#define BL2 1
#define BL3 0
#define WL0 1
#define WL1 0
#endif

#define TA

#ifdef ACORN /* GSL 980108 read/write nonaligned dword routine for ARM processor etc */

static inline int read_dword(int *address)
{
	if ((int)address & 3)
	{

  		return (    *((char *)address) +
  			   (*((char *)address+1) << 8)  +
  		   	   (*((char *)address+2) << 16) +
  		           (*((char *)address+3) << 24) );
	}
	else
		return *(int *)address;
}

static inline void write_dword(int *address, int data)
{
  	if ((int)address & 3)
	{
    		*((char *)address) =    data;
    		*((char *)address+1) = (data >> 8);
    		*((char *)address+2) = (data >> 16);
    		*((char *)address+3) = (data >> 24);
		return;
  	}
  	else
		*(int *)address = data;

}
#else
#define read_dword(address) *(int *)address
#define write_dword(address,data) *(int *)address=data
#endif



void showdisclaimer(void)   /* MAURY_BEGIN: dichiarazione */
{
	printf("MAME is an emulator: it reproduces, more or less faithfully, the behaviour of\n"
		 "several arcade machines. But hardware is useless without software, so an image\n"
		 "of the ROMs which run on that hardware is required. Such ROMs, like any other\n"
		 "commercial software, are copyrighted material and it is therefore illegal to\n"
		 "use them if you don't own the original arcade machine. Needless to say, ROMs\n"
		 "are not distributed together with MAME. Distribution of MAME together with ROM\n"
		 "images is a violation of copyright law and should be promptly reported to the\n"
		 "authors so that appropriate legal action can be taken.\n\n");
}                           /* MAURY_END: dichiarazione */

/***************************************************************************

  Read ROMs into memory.

  Arguments:
  const struct RomModule *romp - pointer to an array of Rommodule structures,
                                 as defined in common.h.

  const char *basename - Name of the directory where the files are
                         stored.

***************************************************************************/
int readroms(const struct RomModule *rommodule,const char *basename)
{
	int region;
	const struct RomModule *romp;
	int checksumwarning;


	checksumwarning = 0;
	romp = rommodule;

	for (region = 0;region < MAX_MEMORY_REGIONS;region++)
		Machine->memory_region[region] = 0;

	region = 0;

	while (romp->name || romp->offset || romp->length)
	{
		unsigned int region_size;
		const char *name;


		if (romp->name || romp->length)
		{
			printf("Error in RomModule definition: expecting ROM_REGION\n");
			goto getout;
		}

		region_size = romp->offset & ~ROMFLAG_MASK;
		if ((Machine->memory_region[region] = malloc(region_size)) == 0)
		{
			printf("Unable to allocate %d bytes of RAM\n",region_size);
			goto getout;
		}

		/* some games (i.e. Pleiades) want the memory clear on startup */
		memset(Machine->memory_region[region],0,region_size);

		romp++;

		while (romp->length)
		{
			void *f;
			int sum,xor,expchecksum;


			if (romp->name == 0)
			{
				printf("Error in RomModule definition: ROM_CONTINUE not preceded by ROM_LOAD\n");
				goto getout;
			}
			else if (romp->name == (char *)-1)
			{
				printf("Error in RomModule definition: ROM_RELOAD not preceded by ROM_LOAD\n");
				goto getout;
			}

			name = romp->name;
			if ((f = osd_fopen(basename,name,OSD_FILETYPE_ROM,0)) == 0)
			{
				#ifdef macintosh	/* JB 971005 */
					printf ("Unable to open ROM %s\n", name);
				#else
				fprintf(stderr, "Unable to open ROM %s\n",name);
				#endif
				goto printromlist;
			}

			sum = 0;
			xor = 0;
			expchecksum = romp->checksum;

			do
			{
				unsigned char *c;
				unsigned int i;
				int length = romp->length & ~ROMFLAG_MASK;


				/* ROM_RELOAD */
				if (romp->name == (char *)-1)
				{
					osd_fseek(f,0,SEEK_SET);
					/* reinitialize checksum counters as well */
					sum = 0;
					xor = 0;
				}

				if (romp->offset + length > region_size)
				{
					printf("Error in RomModule definition: %s out of memory region space\n",name);
					osd_fclose(f);
					goto getout;
				}

				if (romp->length & ROMFLAG_ALTERNATE)
				{
					unsigned char *temp;


					temp = malloc (length);

					if (!temp)
					{
						printf("Out of memory reading ROM %s\n",name);
						osd_fclose(f);
						goto getout;
					}

					int actual;
					if (actual = osd_fread(f,temp,length) != length)
					{
						printf("(alt) Unable to read ROM %s   length=%d   actual=%d\n",name, length, actual);
						free(temp);
						osd_fclose(f);
						goto printromlist;
					}

					/* copy the ROM data and calculate the checksum */
					#ifdef LSB_FIRST
					c = Machine->memory_region[region] + (romp->offset ^ 1);
					#else
					c = Machine->memory_region[region] + romp->offset;
					#endif
					for (i = 0;i < length;i+=2)
					{
						int j;


						j = 256 * temp[i] + temp[i+1];
						sum += j;
						xor ^= j;

						c[i*2] = temp[i];
						c[i*2+2] = temp[i+1];
					}

					free(temp);
				}
				else
				{
					int wide = romp->length & ROMFLAG_WIDE;
					#ifdef LSB_FIRST
					int swap = (romp->length & ROMFLAG_SWAP) ^ ROMFLAG_SWAP;
					#else
					int swap = romp->length & ROMFLAG_SWAP;
					#endif

					int actual;
					if (actual = osd_fread(f,Machine->memory_region[region] + romp->offset,length) != length)
					{
						printf("Unable to read ROM %s   length=%d   actual=%d\n",name, length, actual);
						osd_fclose(f);
						goto printromlist;
					}

					/* calculate the checksum */
					c = Machine->memory_region[region] + romp->offset;
					for (i = 0;i < length;i+=2)
					{
						int j;


						j = 256 * c[i] + c[i+1];
						sum += j;
						xor ^= j;
					}

					/* apply swappage */
					if (wide && swap)
					{
						for (i = 0; i < length; i += 2)
						{
							int temp = c[i];
							c[i] = c[i+1];
							c[i+1] = temp;
						}
					}
				}

				romp++;
			} while (romp->length && (romp->name == 0 || romp->name == (char *)-1));

			sum = ((sum & 0xffff) << 16) | (xor & 0xffff);
			if (expchecksum != sum)
			{
				#ifdef macintosh	/* JB 971005 */
					printf("The checksum of ROM '%-12s' does not match the one MacMAME was tested with.\r"
					"WARNING: the game might not run correctly.\r"
					"Expected:%08x  Found:%08x\r",name,expchecksum,sum);
				#else
				if (checksumwarning == 0)
					printf("The checksum of some ROMs does not match that of the ones MAME was tested with.\n"
							"WARNING: the game might not run correctly.\n"
							"Name         Expected  Found\n");
				checksumwarning++;
				printf("%-12s %08x %08x\n",name,expchecksum,sum);
				#endif
			}

			osd_fclose(f);
		}

		region++;
	}

	#ifndef macintosh	/* JB 971005 */
	if (checksumwarning > 0)
	{
		printf("Press return to continue\n");
		getchar();
	}
	#endif

	return 0;


printromlist:
	#ifndef macintosh	/* JB 971005 */
	printf("\n");                         /* MAURY_BEGIN: dichiarazione */
#ifdef NAG
	showdisclaimer();
	printf("Press return to continue\n"); /* MAURY_END: dichiarazione */
	getchar();
#endif /* NAG */

	printromlist(rommodule,basename);
	#endif


getout:
	for (region = 0;region < MAX_MEMORY_REGIONS;region++)
	{
		free(Machine->memory_region[region]);
		Machine->memory_region[region] = 0;
	}
	return 1;
}



void printromlist(const struct RomModule *romp,const char *basename)
{
	printf("This is the list of the ROMs required.\n"
			"All the ROMs must reside in a subdirectory called \"ROMS\\%s\".\n"
			"Name             Size      Checksum\n",basename);
	while (romp->name || romp->offset || romp->length)
	{
		romp++;	/* skip memory region definition */

		while (romp->length)
		{
			char name[100];
			int length,expchecksum;


			sprintf(name,romp->name,basename);
			expchecksum = romp->checksum;

			length = 0;

			do
			{
				/* ROM_RELOAD */
				if (romp->name == (char *)-1)
					length = 0;	/* restart */

				length += romp->length & ~ROMFLAG_MASK;

				romp++;
			} while (romp->length && (romp->name == 0 || romp->name == (char *)-1));

			printf("%-12s %5d bytes   %08x\n",name,length,expchecksum);
		}
	}
}



/***************************************************************************

  Read samples into memory.
  This function is different from readroms() because it doesn't fail if
  it doesn't find a file: it will load as many samples as it can find.

***************************************************************************/
struct GameSamples *readsamples(const char **samplenames,const char *basename)
/* V.V - avoids samples duplication */
/* if first samplename is *dir, looks for samples into "basename" first, then "dir" */
{
	int i;
	struct GameSamples *samples;
	int skipfirst = 0;

	if (samplenames == 0 || samplenames[0] == 0) return 0;

	if (samplenames[0][0] == '*')
		skipfirst = 1;

	i = 0;
	while (samplenames[i+skipfirst] != 0) i++;

	if ((samples = malloc(sizeof(struct GameSamples) + (i-1)*sizeof(struct GameSample))) == 0)
		return 0;

	samples->total = i;
	for (i = 0;i < samples->total;i++)
		samples->sample[i] = 0;

	for (i = 0;i < samples->total;i++)
	{
		void *f;
		char buf[100];


		if (samplenames[i+skipfirst][0])
		{
			if ((f = osd_fopen(basename,samplenames[i+skipfirst],OSD_FILETYPE_SAMPLE,0)) == 0)
				if (skipfirst)
					f = osd_fopen(samplenames[0]+1,samplenames[i+skipfirst],OSD_FILETYPE_SAMPLE,0);
			if (f != 0)
			{
				if (osd_fseek(f,0,SEEK_END) == 0)
				{
					int dummy;
					unsigned char smpvol=0, smpres=0;
					unsigned smplen=0, smpfrq=0;

					osd_fseek(f,0,SEEK_SET);
					osd_fread(f,buf,4);
					if (memcmp(buf, "MAME", 4) == 0)
					{
						osd_fread(f,&smplen,4);         /* all datas are LITTLE ENDIAN */
						osd_fread(f,&smpfrq,4);
						smplen = intelLong (smplen);  /* so convert them in the right endian-ness */
						smpfrq = intelLong (smpfrq);
						osd_fread(f,&smpres,1);
						osd_fread(f,&smpvol,1);
						osd_fread(f,&dummy,2);
						if ((smplen != 0) && (samples->sample[i] = malloc(sizeof(struct GameSample) + (smplen)*sizeof(char))) != 0)
						{
						   samples->sample[i]->length = smplen;
						   samples->sample[i]->volume = smpvol;
						   samples->sample[i]->smpfreq = smpfrq;
						   samples->sample[i]->resolution = smpres;
						   osd_fread(f,samples->sample[i]->data,smplen);
						}
					}
				}

				osd_fclose(f);
			}
		}
	}

	return samples;
}



void freesamples(struct GameSamples *samples)
{
	int i;


	if (samples == 0) return;

	for (i = 0;i < samples->total;i++)
		free(samples->sample[i]);

	free(samples);
}



/***************************************************************************

  Function to convert the information stored in the graphic roms into a
  more usable format.

  Back in the early '80s, arcade machines didn't have the memory or the
  speed to handle bitmaps like we do today. They used "character maps",
  instead: they had one or more sets of characters (usually 8x8 pixels),
  which would be placed on the screen in order to form a picture. This was
  very fast: updating a character mapped display is, rougly speaking, 64
  times faster than updating an equivalent bitmap display, since you only
  modify groups of 8x8 pixels and not the single pixels. However, it was
  also much less versatile than a bitmap screen, since with only 256
  characters you had to do all of your graphics. To overcome this
  limitation, some special hardware graphics were used: "sprites". A sprite
  is essentially a bitmap, usually larger than a character, which can be
  placed anywhere on the screen (not limited to character boundaries) by
  just telling the hardware the coordinates. Moreover, sprites can be
  flipped along the major axis just by setting the appropriate bit (some
  machines can flip characters as well). This saves precious memory, since
  you need only one copy of the image instead of four.

  What about colors? Well, the early machines had a limited palette (let's
  say 16-32 colors) and each character or sprite could not use all of them
  at the same time. Characters and sprites data would use a limited amount
  of bits per pixel (typically 2, which allowed them to address only four
  different colors). You then needed some way to tell to the hardware which,
  among the available colors, were the four colors. This was done using a
  "color attribute", which typically was an index for a lookup table.

  OK, after this brief and incomplete introduction, let's come to the
  purpose of this section: how to interpret the data which is stored in
  the graphic roms. Unfortunately, there is no easy answer: it depends on
  the hardware. The easiest way to find how data is encoded, is to start by
  making a bit by bit dump of the rom. You will usually be able to
  immediately recognize some pattern: if you are lucky, you will see
  letters and numbers right away, otherwise you will see something which
  looks like letters and numbers, but with halves switched, dilated, or
  something like that. You'll then have to find a way to put it all
  together to obtain our standard one byte per pixel representation. Two
  things to remember:
  - keep in mind that every pixel has typically two (or more) bits
    associated with it, and they are not necessarily near to each other.
  - characters might be rotated 90 degrees. That's because many games used a
    tube rotated 90 degrees. Think how your monitor would look like if you
	put it on its side ;-)

  After you have successfully decoded the characters, you have to decode
  the sprites. This is usually more difficult, because sprites are larger,
  maybe have more colors, and are more difficult to recognize when they are
  messed up, since they are pure graphics without letters and numbers.
  However, with some work you'll hopefully be able to work them out as
  well. As a rule of thumb, the sprites should be encoded in a way not too
  dissimilar from the characters.

***************************************************************************/
static int readbit(const unsigned char *src,int bitnum)
{
	return (src[bitnum / 8] >> (7 - bitnum % 8)) & 1;
}



void decodechar(struct GfxElement *gfx,int num,const unsigned char *src,const struct GfxLayout *gl)
{
	int plane,x,y;
	unsigned char *dp;



	for (plane = 0;plane < gl->planes;plane++)
	{
		int offs;


		offs = num * gl->charincrement + gl->planeoffset[plane];

		for (y = 0;y < gfx->height;y++)
		{
			dp = gfx->gfxdata->line[num * gfx->height + y];

			for (x = 0;x < gfx->width;x++)
			{
				int xoffs,yoffs;


				if (plane == 0) dp[x] = 0;
				else dp[x] <<= 1;

				xoffs = x;
				yoffs = y;

				if (Machine->orientation & ORIENTATION_FLIP_X)
					xoffs = gfx->width-1 - xoffs;

				if (Machine->orientation & ORIENTATION_FLIP_Y)
					yoffs = gfx->height-1 - yoffs;

				if (Machine->orientation & ORIENTATION_SWAP_XY)
				{
					int temp;


					temp = xoffs;
					xoffs = yoffs;
					yoffs = temp;
				}

				dp[x] += readbit(src,offs + gl->yoffset[yoffs] + gl->xoffset[xoffs]);
			}
		}
	}


	if (gfx->pen_usage)
	{
		/* fill the pen_usage array with info on the used pens */
		gfx->pen_usage[num] = 0;

		for (y = 0;y < gfx->height;y++)
		{
			dp = gfx->gfxdata->line[num * gfx->height + y];

			for (x = 0;x < gfx->width;x++)
			{
				gfx->pen_usage[num] |= 1 << dp[x];
			}
		}
	}
}



struct GfxElement *decodegfx(const unsigned char *src,const struct GfxLayout *gl)
{
	int c;
	struct osd_bitmap *bm;
	struct GfxElement *gfx;


	if ((gfx = malloc(sizeof(struct GfxElement))) == 0)
		return 0;

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		gfx->width = gl->height;
		gfx->height = gl->width;

		if ((bm = osd_create_bitmap(gl->total * gfx->height,gfx->width)) == 0)
		{
			free(gfx);
			return 0;
		}
	}
	else
	{
		gfx->width = gl->width;
		gfx->height = gl->height;

		if ((bm = osd_create_bitmap(gfx->width,gl->total * gfx->height)) == 0)
		{
			free(gfx);
			return 0;
		}
	}

	gfx->total_elements = gl->total;
	gfx->color_granularity = 1 << gl->planes;
	gfx->gfxdata = bm;

	gfx->pen_usage = 0; /* need to make sure this is NULL if the next test fails) */
	if (gfx->color_granularity <= 32)	/* can't handle more than 32 pens */
		gfx->pen_usage = malloc(gfx->total_elements * sizeof(int));
		/* no need to check for failure, the code can work without pen_usage */

	for (c = 0;c < gl->total;c++)
		decodechar(gfx,c,src,gl);

	return gfx;
}



void freegfx(struct GfxElement *gfx)
{
	if (gfx)
	{
		free(gfx->pen_usage);
		osd_free_bitmap(gfx->gfxdata);
		free(gfx);
	}
}


/***************************************************************************

  Draw graphic elements in the specified bitmap.

  transparency == TRANSPARENCY_NONE - no transparency.
  transparency == TRANSPARENCY_PEN - bits whose _original_ value is == transparent_color
                                     are transparent. This is the most common kind of
									 transparency.
  transparency == TRANSPARENCY_PENS - as above, but transparent_color is a mask of
  									 transparent pens.
  transparency == TRANSPARENCY_COLOR - bits whose _remapped_ value is == Machine->pens[transparent_color]
                                     are transparent. This is used by e.g. Pac Man.
  transparency == TRANSPARENCY_THROUGH - if the _destination_ pixel is == Machine->pens[transparent_color],
                                     the source pixel is drawn over it. This is used by
									 e.g. Jr. Pac Man to draw the sprites when the background
									 has priority over them.

***************************************************************************/
/* ASG 971011 -- moved this into a "core" function */
/* ASG 980209 -- changed to drawgfx_core8 */
void drawgfx_core8(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int dirty)
{
	int ox,oy,ex,ey,y,start,dy;
	const unsigned char *sd;
	unsigned char *bm,*bme;
	int col;
	int *sd4;
	int trans4,col4;
	struct rectangle myclip;


	if (!gfx) return;

	code %= gfx->total_elements;
	color %= gfx->total_colors;

	/* if necessary, remap the transparent color */
	if (transparency == TRANSPARENCY_COLOR || transparency == TRANSPARENCY_THROUGH)
		transparent_color = Machine->pens[transparent_color];

	if (gfx->pen_usage)
	{
		int transmask;


		transmask = 0;

		if (transparency == TRANSPARENCY_PEN)
		{
			transmask = 1 << transparent_color;
		}
		else if (transparency == TRANSPARENCY_PENS)
		{
			transmask = transparent_color;
		}
		else if (transparency == TRANSPARENCY_COLOR && gfx->colortable)
		{
			int i;
			const unsigned short *paldata;


			paldata = &gfx->colortable[gfx->color_granularity * color];

			for (i = gfx->color_granularity - 1;i >= 0;i--)
			{
				if (paldata[i] == transparent_color)
					transmask |= 1 << i;
			}
		}

		if ((gfx->pen_usage[code] & ~transmask) == 0)
			/* character is totally transparent, no need to draw */
			return;
		else if ((gfx->pen_usage[code] & transmask) == 0 && transparency != TRANSPARENCY_THROUGH)
			/* character is totally opaque, can disable transparency */
			transparency = TRANSPARENCY_NONE;
	}

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = sx;
		sx = sy;
		sy = temp;

		temp = flipx;
		flipx = flipy;
		flipy = temp;

		if (clip)
		{
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = clip->min_y;
			myclip.min_y = temp;
			temp = clip->max_x;
			myclip.max_x = clip->max_y;
			myclip.max_y = temp;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		sx = dest->width - gfx->width - sx;
		if (clip)
		{
			int temp;


			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = dest->width-1 - clip->max_x;
			myclip.max_x = dest->width-1 - temp;
			myclip.min_y = clip->min_y;
			myclip.max_y = clip->max_y;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		sy = dest->height - gfx->height - sy;
		if (clip)
		{
			int temp;


			myclip.min_x = clip->min_x;
			myclip.max_x = clip->max_x;
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_y;
			myclip.min_y = dest->height-1 - clip->max_y;
			myclip.max_y = dest->height-1 - temp;
			clip = &myclip;
		}
	}


	/* check bounds */
	ox = sx;
	oy = sy;
	ex = sx + gfx->width-1;
	if (sx < 0) sx = 0;
	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (ex >= dest->width) ex = dest->width-1;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;
	ey = sy + gfx->height-1;
	if (sy < 0) sy = 0;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (ey >= dest->height) ey = dest->height-1;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	if (dirty)
		osd_mark_dirty (sx,sy,ex,ey,0);	/* ASG 971011 */

	/* start = code * gfx->height; */
	if (flipy)	/* Y flop */
	{
		start = code * gfx->height + gfx->height-1 - (sy-oy);
		dy = -1;
	}
	else		/* normal */
	{
		start = code * gfx->height + (sy-oy);
		dy = 1;
	}


	if (gfx->colortable)	/* remap colors */
	{
		const unsigned short *paldata;	/* ASG 980209 */

		paldata = &gfx->colortable[gfx->color_granularity * color];

		switch (transparency)
		{
			case TRANSPARENCY_NONE:
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm += sx ; bm <= bme-7 ; bm+=8 )
						{
							sd-=8;
							bm[0] = paldata[sd[8]];
							bm[1] = paldata[sd[7]];
							bm[2] = paldata[sd[6]];
							bm[3] = paldata[sd[5]];
							bm[4] = paldata[sd[4]];
							bm[5] = paldata[sd[3]];
							bm[6] = paldata[sd[2]];
							bm[7] = paldata[sd[1]];
						}
						for( ; bm <= bme ; bm++ )
							*bm = paldata[*(sd--)];
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + (sx-ox);
						for( bm += sx ; bm <= bme-7 ; bm+=8 )
						{
							bm[0] = paldata[sd[0]];
							bm[1] = paldata[sd[1]];
							bm[2] = paldata[sd[2]];
							bm[3] = paldata[sd[3]];
							bm[4] = paldata[sd[4]];
							bm[5] = paldata[sd[5]];
							bm[6] = paldata[sd[6]];
							bm[7] = paldata[sd[7]];
							sd+=8;
						}
						for( ; bm <= bme ; bm++ )
							*bm = paldata[*(sd++)];
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_PEN:
				trans4 = transparent_color * 0x01010101;
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + gfx->width -1 - (sx-ox) -3);
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if ((col4=read_dword(sd4)) != trans4){
								col = (col4>>24)&0xff;
								if (col != transparent_color) bm[BL0] = paldata[col];
								col = (col4>>16)&0xff;
								if (col != transparent_color) bm[BL1] = paldata[col];
								col = (col4>>8)&0xff;
								if (col != transparent_color) bm[BL2] = paldata[col];
								col = col4&0xff;
								if (col != transparent_color) bm[BL3] = paldata[col];
							}
							sd4--;
						}
						sd = (unsigned char *)sd4+3;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd--);
							if (col != transparent_color) *bm = paldata[col];
						}
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + (sx-ox));
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if ((col4=read_dword(sd4)) != trans4){
								col = col4&0xff;
								if (col != transparent_color) bm[BL0] = paldata[col];
								col = (col4>>8)&0xff;
								if (col != transparent_color) bm[BL1] = paldata[col];
								col = (col4>>16)&0xff;
								if (col != transparent_color) bm[BL2] = paldata[col];
								col = (col4>>24)&0xff;
								if (col != transparent_color) bm[BL3] = paldata[col];
							}
							sd4++;
						}
						sd = (unsigned char *)sd4;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd++);
							if (col != transparent_color) *bm = paldata[col];
						}
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_PENS:
				trans4 = transparent_color * 0x01010101;
#define PEN_IS_OPAQUE ((1<<col)&transparent_color) == 0

				if (flipx) /* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + gfx->width -1 - (sx-ox) -3);
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if ((col4=read_dword(sd4)) != trans4)
							{
								col = (col4>>24)&0xff;
								if (PEN_IS_OPAQUE) bm[BL0] = paldata[col];
								col = (col4>>16)&0xff;
								if (PEN_IS_OPAQUE) bm[BL1] = paldata[col];
								col = (col4>>8)&0xff;
								if (PEN_IS_OPAQUE) bm[BL2] = paldata[col];
								col = col4&0xff;
								if (PEN_IS_OPAQUE) bm[BL3] = paldata[col];
							}
							sd4--;
						}
						sd = (unsigned char *)sd4+3;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd--);
							if (PEN_IS_OPAQUE) *bm = paldata[col];
						}
						start+=dy;
					}
				}
				else /* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + (sx-ox));
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if ((col4=read_dword(sd4)) != trans4)
							{
								col = col4&0xff;
								if (PEN_IS_OPAQUE) bm[BL0] = paldata[col];
								col = (col4>>8)&0xff;
								if (PEN_IS_OPAQUE) bm[BL1] = paldata[col];
								col = (col4>>16)&0xff;
								if (PEN_IS_OPAQUE) bm[BL2] = paldata[col];
								col = (col4>>24)&0xff;
								if (PEN_IS_OPAQUE) bm[BL3] = paldata[col];
							}
							sd4++;
						}
						sd = (unsigned char *)sd4;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd++);
							if (PEN_IS_OPAQUE) *bm = paldata[col];
						}
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_COLOR:
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm += sx ; bm <= bme ; bm++ )
						{
							col = paldata[*(sd--)];
							if (col != transparent_color) *bm = col;
						}
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + (sx-ox);
						for( bm += sx ; bm <= bme ; bm++ )
						{
							col = paldata[*(sd++)];
							if (col != transparent_color) *bm = col;
						}
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_THROUGH:
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm += sx ; bm <= bme ; bm++ )
						{
							if (*bm == transparent_color)
								*bm = paldata[*sd];
							sd--;
						}
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + (sx-ox);
						for( bm += sx ; bm <= bme ; bm++ )
						{
							if (*bm == transparent_color)
								*bm = paldata[*sd];
							sd++;
						}
						start+=dy;
					}
				}
				break;
		}
	}
	else
	{
		switch (transparency)
		{
			case TRANSPARENCY_NONE:		/* do a verbatim copy (faster) */
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm += sx ; bm <= bme-7 ; bm+=8 )
						{
							sd-=8;
							bm[0] = sd[8];
							bm[1] = sd[7];
							bm[2] = sd[6];
							bm[3] = sd[5];
							bm[4] = sd[4];
							bm[5] = sd[3];
							bm[6] = sd[2];
							bm[7] = sd[1];
						}
						for( ; bm <= bme ; bm++ )
							*bm = *(sd--);
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm = dest->line[y] + sx;
						sd = gfx->gfxdata->line[start] + (sx-ox);
						memcpy(bm,sd,ex-sx+1);
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_PEN:
			case TRANSPARENCY_COLOR:
				trans4 = transparent_color * 0x01010101;

				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox) - 3);
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if( (col4=read_dword(sd4)) != trans4 )
							{
								col = (col4>>24)&0xff;
								if (col != transparent_color) bm[BL0] = col;
								col = (col4>>16)&0xff;
								if (col != transparent_color) bm[BL1] = col;
								col = (col4>>8)&0xff;
								if (col != transparent_color) bm[BL2] = col;
								col = col4&0xff;
								if (col != transparent_color) bm[BL3] = col;
							}
							sd4--;
						}
						sd = (unsigned char *)sd4+3;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd--);
							if (col != transparent_color) *bm = col;
						}
						start+=dy;
					}
				}
				else	/* normal */
				{
					int xod4;

					for (y = sy;y <= ey;y++)
					{
						bm = dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + (sx-ox));
						bm += sx;
						while( bm <= bme-3 )
						{
							/* bypass loop */
							while( bm <= bme-3 && read_dword(sd4) == trans4 )
							{ sd4++; bm+=4; }
							/* drawing loop */
							while( bm <= bme-3 && (col4=read_dword(sd4)) != trans4 )
							{
								xod4 = col4^trans4;
								if( (xod4&0x000000ff) && (xod4&0x0000ff00) &&
								    (xod4&0x00ff0000) && (xod4&0xff000000) )
								{
									write_dword((int *)bm,col4);
								}
								else
								{
									if(xod4&0x000000ff) bm[BL0] = col4;
									if(xod4&0x0000ff00) bm[BL1] = col4>>8;
									if(xod4&0x00ff0000) bm[BL2] = col4>>16;
									if(xod4&0xff000000) bm[BL3] = col4>>24;
								}
								sd4++;
								bm+=4;
							}
						}
						sd = (unsigned char *)sd4;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd++);
							if (col != transparent_color) *bm = col;
						}
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_THROUGH:
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm = bm+sx ; bm <= bme ; bm++ )
						{
							if (*bm == transparent_color)
								*bm = *sd;
							sd--;
						}
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm = dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + (sx-ox);
						for( bm = bm+sx ; bm <= bme ; bm++ )
						{
							if (*bm == transparent_color)
								*bm = *sd;
							sd++;
						}
						start+=dy;
					}
				}
				break;
		}
	}
}

void drawgfx_core16(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int dirty)
{
	int ox,oy,ex,ey,y,start,dy;
	unsigned short *bm,*bme;
	int col;
	struct rectangle myclip;


	if (!gfx) return;

	code %= gfx->total_elements;
	color %= gfx->total_colors;

	/* if necessary, remap the transparent color */
	if (transparency == TRANSPARENCY_COLOR || transparency == TRANSPARENCY_THROUGH)
		transparent_color = Machine->pens[transparent_color];

	if (gfx->pen_usage)
	{
		int transmask;


		transmask = 0;

		if (transparency == TRANSPARENCY_PEN)
		{
			transmask = 1 << transparent_color;
		}
		else if (transparency == TRANSPARENCY_PENS)
		{
			transmask = transparent_color;
		}
		else if (transparency == TRANSPARENCY_COLOR && gfx->colortable)
		{
			int i;
			const unsigned short *paldata;


			paldata = &gfx->colortable[gfx->color_granularity * color];

			for (i = gfx->color_granularity - 1;i >= 0;i--)
			{
				if (paldata[i] == transparent_color)
					transmask |= 1 << i;
			}
		}

		if ((gfx->pen_usage[code] & ~transmask) == 0)
			/* character is totally transparent, no need to draw */
			return;
		else if ((gfx->pen_usage[code] & transmask) == 0 && transparency != TRANSPARENCY_THROUGH)
			/* character is totally opaque, can disable transparency */
			transparency = TRANSPARENCY_NONE;
	}

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = sx;
		sx = sy;
		sy = temp;

		temp = flipx;
		flipx = flipy;
		flipy = temp;

		if (clip)
		{
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = clip->min_y;
			myclip.min_y = temp;
			temp = clip->max_x;
			myclip.max_x = clip->max_y;
			myclip.max_y = temp;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		sx = dest->width - gfx->width - sx;
		if (clip)
		{
			int temp;


			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = dest->width-1 - clip->max_x;
			myclip.max_x = dest->width-1 - temp;
			myclip.min_y = clip->min_y;
			myclip.max_y = clip->max_y;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		sy = dest->height - gfx->height - sy;
		if (clip)
		{
			int temp;


			myclip.min_x = clip->min_x;
			myclip.max_x = clip->max_x;
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_y;
			myclip.min_y = dest->height-1 - clip->max_y;
			myclip.max_y = dest->height-1 - temp;
			clip = &myclip;
		}
	}


	/* check bounds */
	ox = sx;
	oy = sy;
	ex = sx + gfx->width-1;
	if (sx < 0) sx = 0;
	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (ex >= dest->width) ex = dest->width-1;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;
	ey = sy + gfx->height-1;
	if (sy < 0) sy = 0;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (ey >= dest->height) ey = dest->height-1;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	if (dirty)
		osd_mark_dirty (sx,sy,ex,ey,0);	/* ASG 971011 */

	/* start = code * gfx->height; */
	if (flipy)	/* Y flop */
	{
		start = code * gfx->height + gfx->height-1 - (sy-oy);
		dy = -1;
	}
	else		/* normal */
	{
		start = code * gfx->height + (sy-oy);
		dy = 1;
	}


	if (gfx->colortable)	/* remap colors -- assumes 8-bit source */
	{
		int *sd4;
		int trans4,col4;
		const unsigned char *sd;
		const unsigned short *paldata;

		paldata = &gfx->colortable[gfx->color_granularity * color];

		switch (transparency)
		{
			case TRANSPARENCY_NONE:
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm += sx ; bm <= bme-7 ; bm+=8 )
						{
							sd-=8;
							bm[0] = paldata[sd[8]];
							bm[1] = paldata[sd[7]];
							bm[2] = paldata[sd[6]];
							bm[3] = paldata[sd[5]];
							bm[4] = paldata[sd[4]];
							bm[5] = paldata[sd[3]];
							bm[6] = paldata[sd[2]];
							bm[7] = paldata[sd[1]];
						}
						for( ; bm <= bme ; bm++ )
							*bm = paldata[*(sd--)];
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + (sx-ox);
						for( bm += sx ; bm <= bme-7 ; bm+=8 )
						{
							bm[0] = paldata[sd[0]];
							bm[1] = paldata[sd[1]];
							bm[2] = paldata[sd[2]];
							bm[3] = paldata[sd[3]];
							bm[4] = paldata[sd[4]];
							bm[5] = paldata[sd[5]];
							bm[6] = paldata[sd[6]];
							bm[7] = paldata[sd[7]];
							sd+=8;
						}
						for( ; bm <= bme ; bm++ )
							*bm = paldata[*(sd++)];
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_PEN:
				trans4 = transparent_color * 0x01010101;
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + gfx->width -1 - (sx-ox) -3);
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if ((col4=read_dword(sd4)) != trans4){
								col = (col4>>24)&0xff;
								if (col != transparent_color) bm[BL0] = paldata[col];
								col = (col4>>16)&0xff;
								if (col != transparent_color) bm[BL1] = paldata[col];
								col = (col4>>8)&0xff;
								if (col != transparent_color) bm[BL2] = paldata[col];
								col = col4&0xff;
								if (col != transparent_color) bm[BL3] = paldata[col];
							}
							sd4--;
						}
						sd = (unsigned char *)sd4+3;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd--);
							if (col != transparent_color) *bm = paldata[col];
						}
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + (sx-ox));
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if ((col4=read_dword(sd4)) != trans4){
								col = col4&0xff;
								if (col != transparent_color) bm[BL0] = paldata[col];
								col = (col4>>8)&0xff;
								if (col != transparent_color) bm[BL1] = paldata[col];
								col = (col4>>16)&0xff;
								if (col != transparent_color) bm[BL2] = paldata[col];
								col = (col4>>24)&0xff;
								if (col != transparent_color) bm[BL3] = paldata[col];
							}
							sd4++;
						}
						sd = (unsigned char *)sd4;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd++);
							if (col != transparent_color) *bm = paldata[col];
						}
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_PENS:
				trans4 = transparent_color * 0x01010101;
#define PEN_IS_OPAQUE ((1<<col)&transparent_color) == 0

				if (flipx) /* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + gfx->width -1 - (sx-ox) -3);
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if ((col4=read_dword(sd4)) != trans4)
							{
								col = (col4>>24)&0xff;
								if (PEN_IS_OPAQUE) bm[BL0] = paldata[col];
								col = (col4>>16)&0xff;
								if (PEN_IS_OPAQUE) bm[BL1] = paldata[col];
								col = (col4>>8)&0xff;
								if (PEN_IS_OPAQUE) bm[BL2] = paldata[col];
								col = col4&0xff;
								if (PEN_IS_OPAQUE) bm[BL3] = paldata[col];
							}
							sd4--;
						}
						sd = (unsigned char *)sd4+3;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd--);
							if (PEN_IS_OPAQUE) *bm = paldata[col];
						}
						start+=dy;
					}
				}
				else /* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd4 = (int *)(gfx->gfxdata->line[start] + (sx-ox));
						for( bm += sx ; bm <= bme-3 ; bm+=4 )
						{
							if ((col4=read_dword(sd4)) != trans4)
							{
								col = col4&0xff;
								if (PEN_IS_OPAQUE) bm[BL0] = paldata[col];
								col = (col4>>8)&0xff;
								if (PEN_IS_OPAQUE) bm[BL1] = paldata[col];
								col = (col4>>16)&0xff;
								if (PEN_IS_OPAQUE) bm[BL2] = paldata[col];
								col = (col4>>24)&0xff;
								if (PEN_IS_OPAQUE) bm[BL3] = paldata[col];
							}
							sd4++;
						}
						sd = (unsigned char *)sd4;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd++);
							if (PEN_IS_OPAQUE) *bm = paldata[col];
						}
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_COLOR:
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm += sx ; bm <= bme ; bm++ )
						{
							col = paldata[*(sd--)];
							if (col != transparent_color) *bm = col;
						}
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + (sx-ox);
						for( bm += sx ; bm <= bme ; bm++ )
						{
							col = paldata[*(sd++)];
							if (col != transparent_color) *bm = col;
						}
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_THROUGH:
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm += sx ; bm <= bme ; bm++ )
						{
							if (*bm == transparent_color)
								*bm = paldata[*sd];
							sd--;
						}
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = gfx->gfxdata->line[start] + (sx-ox);
						for( bm += sx ; bm <= bme ; bm++ )
						{
							if (*bm == transparent_color)
								*bm = paldata[*sd];
							sd++;
						}
						start+=dy;
					}
				}
				break;
		}
	}
	else	/* not palette mapped -- assumes 16-bit to 16-bit */
	{
		int *sd2;
		int trans2,col2;
		const unsigned short *sd;

		switch (transparency)
		{
			case TRANSPARENCY_NONE:		/* do a verbatim copy (faster) */
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = (unsigned short *)gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm += sx ; bm <= bme-7 ; bm+=8 )
						{
							sd-=8;
							bm[0] = sd[8];
							bm[1] = sd[7];
							bm[2] = sd[6];
							bm[3] = sd[5];
							bm[4] = sd[4];
							bm[5] = sd[3];
							bm[6] = sd[2];
							bm[7] = sd[1];
						}
						for( ; bm <= bme ; bm++ )
							*bm = *(sd--);
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm = (unsigned short *)dest->line[y] + sx;
						sd = (unsigned short *)gfx->gfxdata->line[start] + (sx-ox);
						memcpy(bm,sd,(ex-sx+1)*2);
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_PEN:
			case TRANSPARENCY_COLOR:
				trans2 = transparent_color * 0x00010001;

				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm  = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd2 = (int *)((unsigned short *)gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox) - 3);
						for( bm += sx ; bm <= bme-1 ; bm+=2 )
						{
							if( (col2=read_dword(sd2)) != trans2 )
							{
								col = (col2>>16)&0xffff;
								if (col != transparent_color) bm[WL0] = col;
								col = col2&0xffff;
								if (col != transparent_color) bm[WL1] = col;
							}
							sd2--;
						}
						sd = (unsigned short *)sd2+1;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd--);
							if (col != transparent_color) *bm = col;
						}
						start+=dy;
					}
				}
				else	/* normal */
				{
					int xod2;

					for (y = sy;y <= ey;y++)
					{
						bm = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd2 = (int *)((unsigned short *)gfx->gfxdata->line[start] + (sx-ox));
						bm += sx;
						while( bm <= bme-1 )
						{
							/* bypass loop */
							while( bm <= bme-1 && read_dword(sd2) == trans2 )
							{ sd2++; bm+=2; }
							/* drawing loop */
							while( bm <= bme-1 && (col2=read_dword(sd2)) != trans2 )
							{
								xod2 = col2^trans2;
								if( (xod2&0x0000ffff) && (xod2&0xffff0000) )
								{
									write_dword((int *)bm, col2);
								}
								else
								{
									if(xod2&0x0000ffff) bm[WL0] = col2;
									if(xod2&0xffff0000) bm[WL1] = col2>>16;
								}
								sd2++;
								bm+=2;
							}
						}
						sd = (unsigned short *)sd2;
						for( ; bm <= bme ; bm++ )
						{
							col = *(sd++);
							if (col != transparent_color) *bm = col;
						}
						start+=dy;
					}
				}
				break;

			case TRANSPARENCY_THROUGH:
				if (flipx)	/* X flip */
				{
					for (y = sy;y <= ey;y++)
					{
						bm = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = (unsigned short *)gfx->gfxdata->line[start] + gfx->width-1 - (sx-ox);
						for( bm = bm+sx ; bm <= bme ; bm++ )
						{
							if (*bm == transparent_color)
								*bm = *sd;
							sd--;
						}
						start+=dy;
					}
				}
				else		/* normal */
				{
					for (y = sy;y <= ey;y++)
					{
						bm = (unsigned short *)dest->line[y];
						bme = bm + ex;
						sd = (unsigned short *)gfx->gfxdata->line[start] + (sx-ox);
						for( bm = bm+sx ; bm <= bme ; bm++ )
						{
							if (*bm == transparent_color)
								*bm = *sd;
							sd++;
						}
						start+=dy;
					}
				}
				break;
		}
	}
}

/* ASG 971011 - this is the real draw gfx now */
void drawgfx(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	/* ASG 980209 -- separate 8-bit from 16-bit here */
	if (dest->depth != 16)
		drawgfx_core8(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,1);
	else
		drawgfx_core16(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,1);
}

/***************************************************************************

  Use drawgfx() to copy a bitmap onto another at the given position.
  This function will very likely change in the future.

***************************************************************************/
void copybitmap(struct osd_bitmap *dest,struct osd_bitmap *src,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	static struct GfxElement mygfx =
	{
		0,0,0,	/* filled in later */
		1,1,0,1
	};

	mygfx.width = src->width;
	mygfx.height = src->height;
	mygfx.gfxdata = src;

	/* ASG 980209 -- separate 8-bit from 16-bit here */
	if (dest->depth != 16)
		drawgfx_core8(dest,&mygfx,0,0,flipx,flipy,sx,sy,clip,transparency,transparent_color,0);	/* ASG 971011 */
	else
		drawgfx_core16(dest,&mygfx,0,0,flipx,flipy,sx,sy,clip,transparency,transparent_color,0);	/* ASG 971011 */
}

void copybitmapzoom(struct osd_bitmap *dest,struct osd_bitmap *src,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int scalex,int scaley)
{
	static struct GfxElement mygfx =
	{
		0,0,0,	/* filled in later */
		1,1,0,1
	};
unsigned short hacktable[256];
int i;

	mygfx.width = src->width;
	mygfx.height = src->height;
	mygfx.gfxdata = src;
mygfx.colortable = hacktable;
for (i = 0;i < 256;i++) hacktable[i] = i;
	drawgfxzoom(dest,&mygfx,0,0,flipx,flipy,sx,sy,clip,/*transparency,transparent_color,*/scalex,scaley);	/* ASG 971011 */
}



/***************************************************************************

  Copy a bitmap onto another with scroll and wraparound.
  This function supports multiple independently scrolling rows/columns.
  "rows" is the number of indepentently scrolling rows. "rowscroll" is an
  array of integers telling how much to scroll each row. Same thing for
  "cols" and "colscroll".
  If the bitmap cannot scroll in one direction, set rows or columns to 0.
  If the bitmap scrolls as a whole, set rows and/or cols to 1.
  Bidirectional scrolling is, of course, supported only if the bitmap
  scrolls as a whole in at least one direction.

***************************************************************************/
void copyscrollbitmap(struct osd_bitmap *dest,struct osd_bitmap *src,
		int rows,const int *rowscroll,int cols,const int *colscroll,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	int srcwidth,srcheight;


	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		srcwidth = src->height;
		srcheight = src->width;
	}
	else
	{
		srcwidth = src->width;
		srcheight = src->height;
	}

	if (rows == 0)
	{
		/* scrolling columns */
		int col,colwidth;
		struct rectangle myclip;


		colwidth = srcwidth / cols;

		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;

		col = 0;
		while (col < cols)
		{
			int cons,scroll;


			/* count consecutive columns scrolled by the same amount */
			scroll = colscroll[col];
			cons = 1;
			while (col + cons < cols &&	colscroll[col + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcheight - (-scroll) % srcheight;
			else scroll %= srcheight;

			myclip.min_x = col * colwidth;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,0,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,0,scroll - srcheight,&myclip,transparency,transparent_color);

			col += cons;
		}
	}
	else if (cols == 0)
	{
		/* scrolling rows */
		int row,rowheight;
		struct rectangle myclip;


		rowheight = srcheight / rows;

		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;

		row = 0;
		while (row < rows)
		{
			int cons,scroll;


			/* count consecutive rows scrolled by the same amount */
			scroll = rowscroll[row];
			cons = 1;
			while (row + cons < rows &&	rowscroll[row + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcwidth - (-scroll) % srcwidth;
			else scroll %= srcwidth;

			myclip.min_y = row * rowheight;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,0,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,0,&myclip,transparency,transparent_color);

			row += cons;
		}
	}
	else if (rows == 1 && cols == 1)
	{
		/* XY scrolling playfield */
		int scrollx,scrolly;


		if (rowscroll[0] < 0) scrollx = srcwidth - (-rowscroll[0]) % srcwidth;
		else scrollx = rowscroll[0] % srcwidth;

		if (colscroll[0] < 0) scrolly = srcheight - (-colscroll[0]) % srcheight;
		else scrolly = colscroll[0] % srcheight;

		copybitmap(dest,src,0,0,scrollx,scrolly,clip,transparency,transparent_color);
		copybitmap(dest,src,0,0,scrollx,scrolly - srcheight,clip,transparency,transparent_color);
		copybitmap(dest,src,0,0,scrollx - srcwidth,scrolly,clip,transparency,transparent_color);
		copybitmap(dest,src,0,0,scrollx - srcwidth,scrolly - srcheight,clip,transparency,transparent_color);
	}
	else if (rows == 1)
	{
		/* scrolling columns + horizontal scroll */
		int col,colwidth;
		int scrollx;
		struct rectangle myclip;


		if (rowscroll[0] < 0) scrollx = srcwidth - (-rowscroll[0]) % srcwidth;
		else scrollx = rowscroll[0] % srcwidth;

		colwidth = srcwidth / cols;

		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;

		col = 0;
		while (col < cols)
		{
			int cons,scroll;


			/* count consecutive columns scrolled by the same amount */
			scroll = colscroll[col];
			cons = 1;
			while (col + cons < cols &&	colscroll[col + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcheight - (-scroll) % srcheight;
			else scroll %= srcheight;

			myclip.min_x = col * colwidth + scrollx;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1 + scrollx;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,scrollx,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scrollx,scroll - srcheight,&myclip,transparency,transparent_color);

			myclip.min_x = col * colwidth + scrollx - srcwidth;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1 + scrollx - srcwidth;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,scrollx - srcwidth,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scrollx - srcwidth,scroll - srcheight,&myclip,transparency,transparent_color);

			col += cons;
		}
	}
	else if (cols == 1)
	{
		/* scrolling rows + vertical scroll */
		int row,rowheight;
		int scrolly;
		struct rectangle myclip;


		if (colscroll[0] < 0) scrolly = srcheight - (-colscroll[0]) % srcheight;
		else scrolly = colscroll[0] % srcheight;

		rowheight = srcheight / rows;

		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;

		row = 0;
		while (row < rows)
		{
			int cons,scroll;


			/* count consecutive rows scrolled by the same amount */
			scroll = rowscroll[row];
			cons = 1;
			while (row + cons < rows &&	rowscroll[row + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcwidth - (-scroll) % srcwidth;
			else scroll %= srcwidth;

			myclip.min_y = row * rowheight + scrolly;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1 + scrolly;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,scrolly,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,scrolly,&myclip,transparency,transparent_color);

			myclip.min_y = row * rowheight + scrolly - srcheight;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1 + scrolly - srcheight;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,scrolly - srcheight,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,scrolly - srcheight,&myclip,transparency,transparent_color);

			row += cons;
		}
	}
}



/* fill a bitmap using the specified pen */
void fillbitmap(struct osd_bitmap *dest,int pen,const struct rectangle *clip)
{
	int sx,sy,ex,ey,y;
	struct rectangle myclip;


	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		if (clip)
		{
			myclip.min_x = clip->min_y;
			myclip.max_x = clip->max_y;
			myclip.min_y = clip->min_x;
			myclip.max_y = clip->max_x;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		if (clip)
		{
			int temp;


			temp = clip->min_x;
			myclip.min_x = dest->width-1 - clip->max_x;
			myclip.max_x = dest->width-1 - temp;
			myclip.min_y = clip->min_y;
			myclip.max_y = clip->max_y;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		if (clip)
		{
			int temp;


			myclip.min_x = clip->min_x;
			myclip.max_x = clip->max_x;
			temp = clip->min_y;
			myclip.min_y = dest->height-1 - clip->max_y;
			myclip.max_y = dest->height-1 - temp;
			clip = &myclip;
		}
	}


	sx = 0;
	ex = dest->width - 1;
	sy = 0;
	ey = dest->height - 1;

	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	osd_mark_dirty (sx,sy,ex,ey,0);	/* ASG 971011 */

	/* ASG 980211 */
	if (dest->depth == 16)
	{
		if ((pen >> 8) == (pen & 0xff))
		{
			for (y = sy;y <= ey;y++)
				memset(&dest->line[y][sx*2],pen&0xff,(ex-sx+1)*2);
		}
		else
		{
			for (y = sy;y <= ey;y++)
			{
				unsigned short *p = (unsigned short *)&dest->line[y][sx*2];
				int x;
				for (x = sx;x <= ex;x++)
					*p++ = pen;
			}
		}
	}
	else
	{
		for (y = sy;y <= ey;y++)
			memset(&dest->line[y][sx],pen,ex-sx+1);
	}
}



void drawgfxzoom( struct osd_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int scalex, int scaley )
{
	struct rectangle myclip;

	/*
	scalex and scaley are 16.16 fixed point numbers
	1<<15 : shrink to 50%
	1<<16 : uniform scale
	1<<17 : double to 200%
	*/


	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = sx;
		sx = sy;
		sy = temp;

		temp = flipx;
		flipx = flipy;
		flipy = temp;

		temp = scalex;
		scalex = scaley;
		scaley = temp;

		if (clip)
		{
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = clip->min_y;
			myclip.min_y = temp;
			temp = clip->max_x;
			myclip.max_x = clip->max_y;
			myclip.max_y = temp;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		sx = dest_bmp->width - ((gfx->width * scalex) >> 16) - sx;
		if (clip)
		{
			int temp;


			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = dest_bmp->width-1 - clip->max_x;
			myclip.max_x = dest_bmp->width-1 - temp;
			myclip.min_y = clip->min_y;
			myclip.max_y = clip->max_y;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		sy = dest_bmp->height - ((gfx->height * scaley) >> 16) - sy;
		if (clip)
		{
			int temp;


			myclip.min_x = clip->min_x;
			myclip.max_x = clip->max_x;
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_y;
			myclip.min_y = dest_bmp->height-1 - clip->max_y;
			myclip.max_y = dest_bmp->height-1 - temp;
			clip = &myclip;
		}
	}


	/* ASG 980209 -- added 16-bit version */
	if (dest_bmp->depth != 16)
	{
		if( gfx && gfx->colortable )
		{
			const unsigned short *pal = &gfx->colortable[gfx->color_granularity * (color % gfx->total_colors)]; /* ASG 980209 */
			struct osd_bitmap *source_bmp = gfx->gfxdata;
			int source_base = (code % gfx->total_elements) * gfx->height;

			int sprite_screen_height = (scaley*gfx->height)>>16;
			int sprite_screen_width = (scalex*gfx->width)>>16;

			/* compute sprite increment per screen pixel */
			int dx = (gfx->width<<16)/sprite_screen_width;
			int dy = (gfx->height<<16)/sprite_screen_height;

			int ex = sx+sprite_screen_width;
			int ey = sy+sprite_screen_height;

			int x_index_base;
			int y_index;

			if( flipx )
			{
				x_index_base = (sprite_screen_width-1)*dx;
				dx = -dx;
			}
			else
			{
				x_index_base = 0;
			}

			if( flipy )
			{
				y_index = (sprite_screen_height-1)*dy;
				dy = -dy;
			}
			else
			{
				y_index = 0;
			}

			if( clip )
			{
				if( sx < clip->min_x)
				{ /* clip left */
					int pixels = clip->min_x-sx;
					sx += pixels;
					x_index_base += pixels*dx;
				}
				if( sy < clip->min_y )
				{ /* clip top */
					int pixels = clip->min_y-sy;
					sy += pixels;
					y_index += pixels*dy;
				}
				/* NS 980211 - fixed incorrect clipping */
				if( ex > clip->max_x+1 )
				{ /* clip right */
					int pixels = ex-clip->max_x-1;
					ex -= pixels;
				}
				if( ey > clip->max_y+1 )
				{ /* clip bottom */
					int pixels = ey-clip->max_y-1;
					ey -= pixels;
				}
			}

			if( ex>sx )
			{ /* skip if inner loop doesn't draw anything */
				int y;
				int col = Machine->pens[0];
				for( y=sy; y<ey; y++ )
				{
					unsigned char *source = source_bmp->line[source_base+(y_index>>16)];
					unsigned char *dest = dest_bmp->line[y];

					int x, x_index = x_index_base;
					for( x=sx; x<ex; x++ )
					{
						int c = source[x_index>>16];
						if( c != col ) dest[x] = pal[c];
						x_index += dx;
					}

					y_index += dy;
				}
			}

		}
	}

	/* ASG 980209 -- new 16-bit part */
	else
	{
		if( gfx && gfx->colortable )
		{
			const unsigned short *pal = &gfx->colortable[gfx->color_granularity * (color % gfx->total_colors)]; /* ASG 980209 */
			struct osd_bitmap *source_bmp = gfx->gfxdata;
			int source_base = (code % gfx->total_elements) * gfx->height;

			int sprite_screen_height = (scaley*gfx->height)>>16;
			int sprite_screen_width = (scalex*gfx->width)>>16;

			/* compute sprite increment per screen pixel */
			int dx = (gfx->width<<16)/sprite_screen_width;
			int dy = (gfx->height<<16)/sprite_screen_height;

			int ex = sx+sprite_screen_width;
			int ey = sy+sprite_screen_height;

			int x_index_base;
			int y_index;

			if( flipx )
			{
				x_index_base = (sprite_screen_width-1)*dx;
				dx = -dx;
			}
			else
			{
				x_index_base = 0;
			}

			if( flipy )
			{
				y_index = (sprite_screen_height-1)*dy;
				dy = -dy;
			}
			else
			{
				y_index = 0;
			}

			if( clip )
			{
				if( sx < clip->min_x)
				{ /* clip left */
					int pixels = clip->min_x-sx;
					sx += pixels;
					x_index_base += pixels*dx;
				}
				if( sy < clip->min_y )
				{ /* clip top */
					int pixels = clip->min_y-sy;
					sy += pixels;
					y_index += pixels*dy;
				}
				/* NS 980211 - fixed incorrect clipping */
				if( ex > clip->max_x+1 )
				{ /* clip right */
					int pixels = ex-clip->max_x-1;
					ex -= pixels;
				}
				if( ey > clip->max_y+1 )
				{ /* clip bottom */
					int pixels = ey-clip->max_y-1;
					ey -= pixels;
				}
			}

			if( ex>sx )
			{ /* skip if inner loop doesn't draw anything */
				int y;
				for( y=sy; y<ey; y++ )
				{
					unsigned char *source = source_bmp->line[source_base+(y_index>>16)];
					unsigned short *dest = (unsigned short *)dest_bmp->line[y];

					int x, x_index = x_index_base;
					for( x=sx; x<ex; x++ )
					{
						int c = source[x_index>>16];
						if( c ) dest[x] = pal[c];
						x_index += dx;
					}

					y_index += dy;
				}
			}
		}
	}
}

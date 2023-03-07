
#include "osd_sdl.h"

static const int safety = 16;

struct osd_bitmap *osd_alloc_bitmap(int width,int height,int depth)
{
    struct osd_bitmap *bitmap;
    //printf( "TOMCXXX: osd_alloc_bitmap(%d,%d,%d)\n", width, height, depth);

    if ((bitmap = debug_malloc(sizeof(struct osd_bitmap))) != 0) {
        int i,rowlen,rdwidth;
        unsigned char *bm;

        if (depth != 8 && depth != 16) depth = 8;

        bitmap->depth = depth;
        bitmap->width = width;
        bitmap->height = height;

        rdwidth = (width + 7) & ~7;     /* round width to a quadword */
        if (depth == 16)
            rowlen = 2 * (rdwidth + 2 * safety) * sizeof(unsigned char);
        else
            rowlen =     (rdwidth + 2 * safety) * sizeof(unsigned char);

        if ((bm = malloc((height + 2 * safety) * rowlen)) == 0)
        {
            free(bitmap);
            return 0;
        }

        /* clear ALL bitmap, including safety area, to avoid garbage on right */
        /* side of screen is width is not a multiple of 4 */
        memset(bm,0,(height + 2 * safety) * rowlen);

        if ((bitmap->line = malloc((height + 2 * safety) * sizeof(unsigned char *))) == 0)
        {
            free(bm);
            free(bitmap);
            return 0;
        }

        for (int i = 0;i < height + 2 * safety;i++) {
            if (depth == 16)
                bitmap->line[i] = &bm[i * rowlen + 2*safety];
            else
                bitmap->line[i] = &bm[i * rowlen + safety];
        }
        bitmap->line += safety;

        bitmap->_private = bm;

        osd_clearbitmap(bitmap);
    }

    //printf("TOMCXXX: osd_alloc_bitmap(%d,%d,%d) -> %p\n", width, height, depth, bitmap);
    return bitmap;
}

void osd_clearbitmap(struct osd_bitmap *bitmap)
{
    for( int i = 0; i < bitmap->height; i++ ) {
        if (bitmap->depth == 16)
            memset(bitmap->line[i],0,2*bitmap->width);
        else
            memset(bitmap->line[i],BACKGROUND,bitmap->width);
    }

    if( bitmap == Machine->scrbitmap ) {
        extern int bitmap_dirty;    /* in mame.c */

        osd_mark_dirty (0,0,bitmap->width-1,bitmap->height-1,1);
        bitmap_dirty = 1;
    }
}

void osd_free_bitmap(struct osd_bitmap *bitmap)
{
    if (bitmap) {
        debug_free(bitmap->_private);
        debug_free(bitmap);
    }
}

void osd_mark_dirty(int xmin, int ymin, int xmax, int ymax, int ui)
{
    //printf( "TOMCXXX: osd_mark_dirty(%d,%d,%d,%d,%d)\n", xmin, ymin, xmax, ymax, ui);
}

void osd_set_visible_area(int min_x,int max_x,int min_y,int max_y)
{
    //printf( "TOMCXXX: osd_set_visible_area(%d,%d,%d,%d)\n", min_x, max_x, min_y, max_y);
    visible_min_x = min_x;
    visible_max_x = max_x;
    visible_min_y = min_y;
    visible_max_y = max_y;
}

int osd_create_display(int width,int height,int depth,int fps,int attributes,int orientation)
{
    //printf( "TOMCXXX: osd_create_display(%d,%d,%d,%d,%d,%d)\n", width, height, depth, fps, attributes, orientation);
    logerror("width %d, height %d\n", width,height);

    video_depth       = depth;
    video_fps         = fps;
    video_attributes  = attributes;
    video_orientation = orientation;

    visible_min_x = 0;
    visible_max_x = width-1;
    visible_min_y = 0;
    visible_max_y = height-1;

    brightness = 100;
    brightness_paused_adjust = 1.0;
    dirty_bright = 1;

    if (frameskip < 0) frameskip = 0;
    if (frameskip >= FRAMESKIP_LEVELS) frameskip = FRAMESKIP_LEVELS-1;

    if (attributes & VIDEO_TYPE_VECTOR)
        vector_game = 1;
    else
        vector_game = 0;

    if ((attributes & VIDEO_SUPPORTS_DIRTY) || vector_game)
        use_dirty = 1;
    else
        use_dirty = 0;

    game_width  = width;
    game_height = height;
    game_attributes = attributes;

    if (vector_game)
    {
        /* for vector games, use_double == 0 means miniaturized. */
        /* TOMCXXX
        if (use_double == 0)
            scale_vectorgames(gfx_width/2,gfx_height/2,&width, &height);
        else
            scale_vectorgames(gfx_width,gfx_height,&width, &height);
        TOMCXXX */
        /* vector games are always non-doubling */
        use_double = 0;
        /* center display */
        //TOMCXXXXX adjust_display(0, 0, width-1, height-1);
    }

    int pixel_format = SDL_PIXELFORMAT_ARGB8888;

    SDLCHK(40101,displayWindow   = SDL_CreateWindow( "origmame", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, game_width*displayScale, game_height*displayScale, SDL_WINDOW_RESIZABLE ));
    SDLCHK(50101,displayRenderer = SDL_CreateRenderer( displayWindow, -1, 0 ));
    SDLCHK(60101,displayTexture  = SDL_CreateTexture(displayRenderer, pixel_format, SDL_TEXTUREACCESS_STREAMING, game_width, game_height));

    if( !osd_set_display(width, height, depth, attributes, orientation) ) {
        return 1;
    }

    return 0;
}

int osd_set_display(int width,int height,int depth,int attributes,int orientation)
{
    //printf( "TOMCXXX: osd_set_display(%d,%d,%d,%d,%d)\n", width, height, depth, attributes, orientation);
    pixelsMappedToARGB8888 = (uint32_t *) debug_malloc(width * height * sizeof(uint32_t));
    return 1;
}

void osd_close_display(void)
{
    if( pixelsMappedToARGB8888 ) {
        debug_free(pixelsMappedToARGB8888);
        pixelsMappedToARGB8888 = (uint32_t *) 0;
    }
    if( displayTexture ) {
        SDL_DestroyTexture( displayTexture );
        displayTexture = NULL;
    }
    if( displayRenderer ) {
        SDL_DestroyRenderer( displayRenderer );
        displayRenderer = NULL;
    }
    if( displayWindow ) {
        SDL_DestroyWindow( displayWindow );
        displayWindow = NULL;
    }

    if( dirtycolor ) {
        debug_free(dirtycolor);
        dirtycolor = NULL;
    }
    if( current_palette ) {
        debug_free(current_palette);
        current_palette = NULL;
    }
    if( paletteMapping ) {
        debug_free(paletteMapping);
        paletteMapping = NULL;
    }
    if( palette_16bit_lookup ) {
        debug_free(palette_16bit_lookup);
        palette_16bit_lookup = NULL;
    }
}

static const int RGB16BIT_RED_MASK = 0x1f;
static const int RGB16BIT_GRN_MASK = 0x1f;
static const int RGB16BIT_BLU_MASK = 0x1f;
static const int RGB16BIT_RED_SHIFT =  0;
static const int RGB16BIT_GRN_SHIFT =  5;
static const int RGB16BIT_BLU_SHIFT = 10;
int rgbToARGB(int r, int g, int b)
{
    int pixel;

    if( video_depth == 8 ) {
        pixel = (     0xff  << 24) |
                ((r & 0xff) << 16) |
                ((g & 0xff) <<  8) |
                ((b & 0xff)      );
    }
    else if( video_depth == 16 ) {
        // 565
        pixel = ((r & RGB16BIT_RED_MASK) << RGB16BIT_RED_SHIFT) |
                ((g & RGB16BIT_GRN_MASK) << RGB16BIT_GRN_SHIFT) |
                ((b & RGB16BIT_BLU_MASK) << RGB16BIT_BLU_SHIFT);
        pixel = (     0xff  << 24) |
                ((r & 0xff) << 16) |
                ((g & 0xff) <<  8) |
                ((b & 0xff)      );
    }
    else {
        fprintf(stderr,"unsupported video_depth %d\n", video_depth);
        exit(1);
    }
}

void ARGBTorgb(int pixel, unsigned char *red, unsigned char *green, unsigned char *blue)
{
    if( video_depth == 8 ) {
        *red   = (unsigned char) ( ( pixel >> 16 ) & 0xff );
        *green = (unsigned char) ( ( pixel >>  8 ) & 0xff );
        *blue  = (unsigned char) ( ( pixel       ) & 0xff );
    }
    else if( video_depth == 16 ) {
        // 565
        *red   = (unsigned char) ( ( pixel >> RGB16BIT_RED_SHIFT ) & RGB16BIT_RED_MASK );
        *green = (unsigned char) ( ( pixel >> RGB16BIT_GRN_SHIFT ) & RGB16BIT_GRN_MASK );
        *blue  = (unsigned char) ( ( pixel >> RGB16BIT_BLU_SHIFT ) & RGB16BIT_BLU_MASK );
        *red   = (unsigned char) ( ( pixel >> 16 ) & 0xff );
        *green = (unsigned char) ( ( pixel >>  8 ) & 0xff );
        *blue  = (unsigned char) ( ( pixel       ) & 0xff );
    }
    else {
        fprintf(stderr,"unsupported video_depth %d\n", video_depth);
        exit(1);
    }
}

int osd_allocate_colors(unsigned int totalcolors,const unsigned char *palette,unsigned short *pens,int modifiable)
{
    //printf( "TOMCXXX: osd_allocate_colors(%d %p %p %d)\n", totalcolors, palette, pens, modifiable);

    modifiable_palette = modifiable;
    screen_colors = totalcolors;
    if (video_depth != 8)
        screen_colors += 2;
    else screen_colors = 256;

    dirtycolor           = debug_malloc(    screen_colors * sizeof(int));
    current_palette      = debug_malloc(3 * screen_colors * sizeof(unsigned char));
    paletteMapping       = debug_malloc(    screen_colors * sizeof(uint32_t));
    palette_16bit_lookup = debug_malloc(    screen_colors * sizeof(palette_16bit_lookup[0]));
    if (dirtycolor == 0 || current_palette == 0 || palette_16bit_lookup == 0)
        return 1;

    for (int i = 0;i < screen_colors;i++)
        dirtycolor[i] = 1;
    dirtypalette = 1;
    for (int i = 0;i < screen_colors;i++)
        current_palette[3*i+0] = current_palette[3*i+1] = current_palette[3*i+2] = 0;

    if (video_depth != 8 && modifiable_palette == 0) {
        //printf( "TOMCXXX: ALLOCATE COLORS NON-8-BIT MODIFIABLE PALETTE video_depth=%d  totalcolors=%d  modifiable=%d   screen_colors=%d\n", video_depth, totalcolors, modifiable, screen_colors);
        int r,g,b;


        for (int i = 0;i < totalcolors;i++)
        {
            r = 255 * brightness * pow(palette[3*i+0] / 255.0, 1 / osd_gamma_correction) / 100;
            g = 255 * brightness * pow(palette[3*i+1] / 255.0, 1 / osd_gamma_correction) / 100;
            b = 255 * brightness * pow(palette[3*i+2] / 255.0, 1 / osd_gamma_correction) / 100;
            *pens++ = rgbToARGB(r,g,b);
        }

        Machine->uifont->colortable[0] = rgbToARGB(0x00,0x00,0x00);
        Machine->uifont->colortable[1] = rgbToARGB(0xff,0xff,0xff);
        Machine->uifont->colortable[2] = rgbToARGB(0xff,0xff,0xff);
        Machine->uifont->colortable[3] = rgbToARGB(0x00,0x00,0x00);
    }
    else {
        if (video_depth == 8 && totalcolors >= 255) {
            //printf( "TOMCXXX: ALLOCATE COLORS 8 BIT MANY COLORS  video_depth=%d  totalcolors=%d  modifiable=%d   screen_colors=%d\n", video_depth, totalcolors, modifiable, screen_colors);
            int bestblack,bestwhite;
            int bestblackscore,bestwhitescore;


            bestblack = bestwhite = 0;
            bestblackscore = 3*255*255;
            bestwhitescore = 0;
            for (int i = 0;i < totalcolors;i++)
            {
                int r,g,b,score;

                r = palette[3*i+0];
                g = palette[3*i+1];
                b = palette[3*i+2];
                score = r*r + g*g + b*b;

                if (score < bestblackscore)
                {
                    bestblack = i;
                    bestblackscore = score;
                }
                if (score > bestwhitescore)
                {
                    bestwhite = i;
                    bestwhitescore = score;
                }
            }

            for (int i = 0;i < totalcolors;i++)
                pens[i] = i;

            /* map black to pen 0, otherwise the screen border will not be black */
            pens[bestblack] = 0;
            pens[0] = bestblack;

            Machine->uifont->colortable[0] = pens[bestblack];
            Machine->uifont->colortable[1] = pens[bestwhite];
            Machine->uifont->colortable[2] = pens[bestwhite];
            Machine->uifont->colortable[3] = pens[bestblack];
        }
        else {
            //printf( "TOMCXXX: ALLOCATE COLORS OTHER  video_depth=%d  totalcolors=%d  modifiable=%d   screen_colors=%d\n", video_depth, totalcolors, modifiable, screen_colors);
            pens[0] = 0x00000000; // TOMCXXXX
            /* reserve color 1 for the user interface text */
            current_palette[3*1+0] = current_palette[3*1+1] = current_palette[3*1+2] = 0xff;
            Machine->uifont->colortable[0] = 0;
            Machine->uifont->colortable[1] = 1;
            Machine->uifont->colortable[2] = 1;
            Machine->uifont->colortable[3] = 0;

            /* fill the palette starting from the end, so we mess up badly written */
            /* drivers which don't go through Machine->pens[] */
            for (int i = 1;i < totalcolors;i++) {
                pens[i] = (screen_colors-1)-i;
            }
        }

        for( int i = 0; i < totalcolors; i++ ) {
            current_palette[3*pens[i]+0] = palette[3*i];
            current_palette[3*pens[i]+1] = palette[3*i+1];
            current_palette[3*pens[i]+2] = palette[3*i+2];

            // tomc - this is our fast 24 bit mapping
            paletteMapping[pens[i]] = rgbToARGB(current_palette[3*pens[i]+0],
                                                current_palette[3*pens[i]+1],
                                                current_palette[3*pens[i]+2]);
        }
    }

    return 0;
}

void do_update_display(struct osd_bitmap *displayBitmap)
{
    if( !displayTexture || !displayRenderer || !displayBitmap ) {
        // we can be called by keepalive while processing keyboard events before create_display is called.
        return;
    }

    int npixels = displayBitmap->width * displayBitmap->height;
    uint32_t *dstptr = pixelsMappedToARGB8888;
    for( int y=0; y<game_height; y++ ) {
        if( video_depth == 16 ) {
            unsigned short *srcptr = ((unsigned short *) (displayBitmap->line)[y + visible_min_y]) + visible_min_x;
            for( int x=0; x<game_width; x++ ) {
                *dstptr++ = paletteMapping[*srcptr++];
            }
        }
        else {
            unsigned char *srcptr = displayBitmap->line[y + visible_min_y] + visible_min_x;
            for( int x=0; x<game_width; x++ ) {
                *dstptr++ = paletteMapping[*srcptr++];
            }
        }
    }

    SDL_UpdateTexture(displayTexture, NULL, pixelsMappedToARGB8888, game_width*sizeof(uint32_t));
    SDL_RenderCopy(displayRenderer, displayTexture, NULL, NULL);
    SDL_RenderPresent(displayRenderer);

    /* now wait until it's time to update the screen */
    uint64_t curr;
    if (throttle) {
        //printf( "TOMCXXX: frameskip=%d  Machine->drv->frames_per_second=%d\n", frameskip, Machine->drv->frames_per_second);
        while(1) {
            curr = now();
            int64_t delta  = (int64_t) curr - (int64_t) last_display_update_time;
            int64_t target = (frameskip+1) * NS_PER_SECOND/Machine->drv->frames_per_second;
            if( delta >= target ) {
                // time's up
                break;
            }

            poll_events(); // keep alive

            // recompute delta
            curr = now();
            delta  = (int64_t) curr - (int64_t) last_display_update_time;
            int sleep_usecs = (target - delta) / 1000000;
            if( sleep_usecs > 2000 ) { // leave some room
                usleep(1000);
            }
            //printf("TOMCXXX: curr=%lld   last_display_update_time=%lld   delta = %lld   target=%lld\n", (long long) curr, (long long) last_display_update_time, (long long) delta, (long long) target);
        }
    }

    last_display_update_time = now();
}

void osd_modify_pen(int pen,unsigned char red, unsigned char green, unsigned char blue)
{
    if (modifiable_palette == 0)
    {
        logerror("error: osd_modify_pen() called with modifiable_palette == 0\n");
        return;
    }


    if (    current_palette[3*pen+0] != red ||
            current_palette[3*pen+1] != green ||
            current_palette[3*pen+2] != blue)
    {
        current_palette[3*pen+0] = red;
        current_palette[3*pen+1] = green;
        current_palette[3*pen+2] = blue;

        // tomc - this is our fast 24 bit mapping
        paletteMapping[pen] = rgbToARGB(current_palette[3*pen+0],
                                        current_palette[3*pen+1],
                                        current_palette[3*pen+2]);

        dirtycolor[pen] = 1;
        dirtypalette = 1;
    }
}

void osd_get_pen(int pen,unsigned char *red, unsigned char *green, unsigned char *blue)
{
    if (video_depth != 8 && modifiable_palette == 0) {
        ARGBTorgb(pen,red,green,blue);
    }
    else {
        *red   = current_palette[3*pen+0];
        *green = current_palette[3*pen+1];
        *blue  = current_palette[3*pen+2];
    }
    //printf( "TOMCXXX: osd_get_pen(depth=%d,%d,(%d,%d,%d))\n", displayBitmap->depth, pen, (int) *red, (int) *green, (int) *blue);
}

int osd_skip_this_frame(void)
{
    static const int skiptable[FRAMESKIP_LEVELS][FRAMESKIP_LEVELS] =
    {
        { 0,0,0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0,0,1 },
        { 0,0,0,0,0,1,0,0,0,0,0,1 },
        { 0,0,0,1,0,0,0,1,0,0,0,1 },
        { 0,0,1,0,0,1,0,0,1,0,0,1 },
        { 0,1,0,0,1,0,1,0,0,1,0,1 },
        { 0,1,0,1,0,1,0,1,0,1,0,1 },
        { 0,1,0,1,1,0,1,0,1,1,0,1 },
        { 0,1,1,0,1,1,0,1,1,0,1,1 },
        { 0,1,1,1,0,1,1,1,0,1,1,1 },
        { 0,1,1,1,1,1,0,1,1,1,1,1 },
        { 0,1,1,1,1,1,1,1,1,1,1,1 }
    };

    return skiptable[frameskip][frameskip_counter];
}

/* Update the display. */
void osd_update_video_and_audio(struct osd_bitmap *bitmap)
{
    //printf( "TOMCXXX: osd_update_video_and_audio(%p)\n", bitmap);
    frame_count++;

    do_update_display(bitmap);

    poll_events();

#ifdef TOMCXXXX
    static const int waittable[FRAMESKIP_LEVELS][FRAMESKIP_LEVELS] =
    {
        { 1,1,1,1,1,1,1,1,1,1,1,1 },
        { 2,1,1,1,1,1,1,1,1,1,1,0 },
        { 2,1,1,1,1,0,2,1,1,1,1,0 },
        { 2,1,1,0,2,1,1,0,2,1,1,0 },
        { 2,1,0,2,1,0,2,1,0,2,1,0 },
        { 2,0,2,1,0,2,0,2,1,0,2,0 },
        { 2,0,2,0,2,0,2,0,2,0,2,0 },
        { 2,0,2,0,0,3,0,2,0,0,3,0 },
        { 3,0,0,3,0,0,3,0,0,3,0,0 },
        { 4,0,0,0,4,0,0,0,4,0,0,0 },
        { 6,0,0,0,0,0,6,0,0,0,0,0 },
        {12,0,0,0,0,0,0,0,0,0,0,0 }
    };
    int i;
    static int showfps,showfpstemp;
    TICKER curr;
    static TICKER prev_measure,this_frame_base,prev;
    static int speed = 100;
    static int vups,vfcount;
    int have_to_clear_bitmap = 0;
    int already_synced;


    if (warming_up)
    {
        /* first time through, initialize timer */
        prev_measure = ticker() - FRAMESKIP_LEVELS * TICKS_PER_SEC/video_fps;
        warming_up = 0;
    }

    if (frameskip_counter == 0)
        this_frame_base = prev_measure + FRAMESKIP_LEVELS * TICKS_PER_SEC/video_fps;

    if (throttle)
    {
        static TICKER last;

        /* if too much time has passed since last sound update, disable throttling */
        /* temporarily - we wouldn't be able to keep synch anyway. */
        curr = ticker();
        if ((curr - last) > 2*TICKS_PER_SEC / video_fps)
            throttle = 0;
        last = curr;

        already_synced = msdos_update_audio();

        throttle = 1;
    }
    else
        already_synced = msdos_update_audio();


    if (osd_skip_this_frame() == 0)
    {
        if (showfpstemp)
        {
            showfpstemp--;
            if (showfps == 0 && showfpstemp == 0)
            {
                have_to_clear_bitmap = 1;
            }
        }


        if (input_ui_pressed(IPT_UI_SHOW_FPS))
        {
            if (showfpstemp)
            {
                showfpstemp = 0;
                have_to_clear_bitmap = 1;
            }
            else
            {
                showfps ^= 1;
                if (showfps == 0)
                {
                    have_to_clear_bitmap = 1;
                }
            }
        }


        /* now wait until it's time to update the screen */
        if (throttle)
        {
            profiler_mark(PROFILER_IDLE);
            if (video_sync)
            {
                static TICKER last;


                do
                {
                    vsync();
                    curr = ticker();
                } while (TICKS_PER_SEC / (curr - last) > video_fps * 11 /10);

                last = curr;
            }
            else
            {
                TICKER target;


                /* wait for video sync but use normal throttling */
                if (wait_vsync)
                    vsync();

                curr = ticker();

                if (already_synced == 0)
                {
                /* wait only if the audio update hasn't synced us already */

                    target = this_frame_base +
                            frameskip_counter * TICKS_PER_SEC/video_fps;

                    if (curr - target < 0)
                    {
                        do
                        {
                            curr = ticker();
                        } while (curr - target < 0);
                    }
                }

            }
            profiler_mark(PROFILER_END);
        }
        else curr = ticker();


        /* for the FPS average calculation */
        if (++frames_displayed == FRAMES_TO_SKIP)
            start_time = curr;
        else
            end_time = curr;


        if (frameskip_counter == 0)
        {
            int divdr;


            divdr = video_fps * (curr - prev_measure) / (100 * FRAMESKIP_LEVELS);
            speed = (TICKS_PER_SEC + divdr/2) / divdr;

            prev_measure = curr;
        }

        prev = curr;

        vfcount += waittable[frameskip][frameskip_counter];
        if (vfcount >= video_fps)
        {
            extern int vector_updates; /* avgdvg_go_w()'s per Mame frame, should be 1 */


            vfcount = 0;
            vups = vector_updates;
            vector_updates = 0;
        }

        if (showfps || showfpstemp)
        {
            int fps;
            char buf[30];
            int divdr;


            divdr = 100 * FRAMESKIP_LEVELS;
            fps = (video_fps * (FRAMESKIP_LEVELS - frameskip) * speed + (divdr / 2)) / divdr;
            sprintf(buf,"%s%2d%4d%%%4d/%d fps",autoframeskip?"auto":"fskp",frameskip,speed,fps,(int)(video_fps+0.5));
            ui_text(bitmap,buf,Machine->uiwidth-strlen(buf)*Machine->uifontwidth,0);
            if (vector_game)
            {
                sprintf(buf," %d vector updates",vups);
                ui_text(bitmap,buf,Machine->uiwidth-strlen(buf)*Machine->uifontwidth,Machine->uifontheight);
            }
        }

        if (bitmap->depth == 8)
        {
            if (dirty_bright)
            {
                dirty_bright = 0;
                for (i = 0;i < 256;i++)
                {
                    float rate = brightness * brightness_paused_adjust * pow(i / 255.0, 1 / osd_gamma_correction) / 100;
                    bright_lookup[i] = 63 * rate + 0.5;
                }
            }
            if (dirtypalette)
            {
                dirtypalette = 0;
                for (i = 0;i < screen_colors;i++)
                {
                    if (dirtycolor[i])
                    {
                        RGB adjusted_palette;

                        dirtycolor[i] = 0;

                        adjusted_palette.r = current_palette[3*i+0];
                        adjusted_palette.g = current_palette[3*i+1];
                        adjusted_palette.b = current_palette[3*i+2];
                        if (i != Machine->uifont->colortable[1])    /* don't adjust the user interface text */
                        {
                            adjusted_palette.r = bright_lookup[adjusted_palette.r];
                            adjusted_palette.g = bright_lookup[adjusted_palette.g];
                            adjusted_palette.b = bright_lookup[adjusted_palette.b];
                        }
                        else
                        {
                            adjusted_palette.r >>= 2;
                            adjusted_palette.g >>= 2;
                            adjusted_palette.b >>= 2;
                        }
                        set_color(i,&adjusted_palette);
                    }
                }
            }
        }
        else
        {
            if (dirty_bright)
            {
                dirty_bright = 0;
                for (i = 0;i < 256;i++)
                {
                    float rate = brightness * brightness_paused_adjust * pow(i / 255.0, 1 / osd_gamma_correction) / 100;
                    bright_lookup[i] = 255 * rate + 0.5;
                }
            }
            if (dirtypalette)
            {
                if (use_dirty) init_dirty(1);    /* have to redraw the whole screen */

                dirtypalette = 0;
                for (i = 0;i < screen_colors;i++)
                {
                    if (dirtycolor[i])
                    {
                        int r,g,b;

                        dirtycolor[i] = 0;

                        r = current_palette[3*i+0];
                        g = current_palette[3*i+1];
                        b = current_palette[3*i+2];
                        if (i != Machine->uifont->colortable[1])    /* don't adjust the user interface text */
                        {
                            r = bright_lookup[r];
                            g = bright_lookup[g];
                            b = bright_lookup[b];
                        }
                        palette_16bit_lookup[i] = rgbToARGB(r,g,b) * 0x10001;
                    }
                }
            }
        }

        /* copy the bitmap to screen memory */
        profiler_mark(PROFILER_BLIT);
        update_screen(bitmap);
        profiler_mark(PROFILER_END);

        /* see if we need to give the card enough time to draw both odd/even fields of the interlaced display
            (req. for 15.75KHz Arcade Monitor Modes */
        interlace_sync();


        if (have_to_clear_bitmap)
            osd_clearbitmap(bitmap);

        if (use_dirty)
        {
            if (!vector_game)
                swap_dirty();
            init_dirty(0);
        }

        if (have_to_clear_bitmap)
            osd_clearbitmap(bitmap);


        if (throttle && autoframeskip && frameskip_counter == 0)
        {
            static int frameskipadjust;
            int adjspeed;

            /* adjust speed to video refresh rate if vsync is on */
            adjspeed = speed * video_fps / vsync_frame_rate;

            if (adjspeed >= 100)
            {
                frameskipadjust++;
                if (frameskipadjust >= 3)
                {
                    frameskipadjust = 0;
                    if (frameskip > 0) frameskip--;
                }
            }
            else
            {
                if (adjspeed < 80)
                    frameskipadjust -= (90 - adjspeed) / 5;
                else
                {
                    /* don't push frameskip too far if we are close to 100% speed */
                    if (frameskip < 8)
                        frameskipadjust--;
                }

                while (frameskipadjust <= -2)
                {
                    frameskipadjust += 2;
                    if (frameskip < FRAMESKIP_LEVELS-1) frameskip++;
                }
            }
        }
    }

    /* Check for PGUP, PGDN and pan screen */
    pan_display();

    if (input_ui_pressed(IPT_UI_FRAMESKIP_INC))
    {
        if (autoframeskip)
        {
            autoframeskip = 0;
            frameskip = 0;
        }
        else
        {
            if (frameskip == FRAMESKIP_LEVELS-1)
            {
                frameskip = 0;
                autoframeskip = 1;
            }
            else
                frameskip++;
        }

        if (showfps == 0)
            showfpstemp = 2*video_fps;

        /* reset the frame counter every time the frameskip key is pressed, so */
        /* we'll measure the average FPS on a consistent status. */
        frames_displayed = 0;
    }

    if (input_ui_pressed(IPT_UI_FRAMESKIP_DEC))
    {
        if (autoframeskip)
        {
            autoframeskip = 0;
            frameskip = FRAMESKIP_LEVELS-1;
        }
        else
        {
            if (frameskip == 0)
                autoframeskip = 1;
            else
                frameskip--;
        }

        if (showfps == 0)
            showfpstemp = 2*video_fps;

        /* reset the frame counter every time the frameskip key is pressed, so */
        /* we'll measure the average FPS on a consistent status. */
        frames_displayed = 0;
    }

    if (input_ui_pressed(IPT_UI_THROTTLE))
    {
        throttle ^= 1;

        /* reset the frame counter every time the throttle key is pressed, so */
        /* we'll measure the average FPS on a consistent status. */
        frames_displayed = 0;
    }


    frameskip_counter = (frameskip_counter + 1) % FRAMESKIP_LEVELS;
#endif /* TOMCXXXX */
}

void osd_set_gamma(float _gamma)
{
}

float osd_get_gamma(void)
{
}

void osd_set_brightness(int brightness)
{
}

int osd_get_brightness(void)
{
    return 0;
}

void osd_save_snapshot(struct osd_bitmap *bitmap)
{
    save_screen_snapshot(bitmap);
}

void osd_pause(int paused)
{
#ifdef TOMCXXX
    int i;

    if (paused) brightness_paused_adjust = 0.65;
    else brightness_paused_adjust = 1.0;

    for (i = 0;i < screen_colors;i++)
        dirtycolor[i] = 1;
    dirtypalette = 1;
    dirty_bright = 1;
#endif /* TOMCXXX */
}

void osd_mark_vector_dirty(int x, int y)
{
    //printf( "TOMCXXX: osd_mark_vector_dirty(%d,%d)\n", x, y);
}

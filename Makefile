CC	= gcc
LD	= gcc

DEFS   = -DLSB_FIRST

DEBUG_OR_OPTIMIZE=-ggdb
STRIP_OR_NOT=

#DEBUG_OR_OPTIMIZE= -O3 -fomit-frame-pointer -funroll-loops
#STRIP_OR_NOT=-s

#-DX86_ASM

CFLAGS = ${DEBUG_OR_OPTIMIZE} -DUNIX -Isrc -Isrc/Z80 -I/usr/include/SDL2 -Wall #-Werror
LIBS   = -lSDL2 -lSDL2_mixer
OBJS   = obj/mame.o obj/common.o obj/driver.o obj/osdepend.o \
         obj/machine/pacman.o obj/vidhrdw/pacman.o obj/drivers/pacman.o \
				 obj/drivers/crush.o \
         obj/vidhrdw/pengo.o obj/sndhrdw/pengo.o obj/drivers/pengo.o \
         obj/machine/ladybug.o obj/vidhrdw/ladybug.o obj/sndhrdw/ladybug.o obj/drivers/ladybug.o \
         obj/machine/mrdo.o obj/vidhrdw/mrdo.o obj/drivers/mrdo.o \
         obj/vidhrdw/cclimber.o obj/sndhrdw/cclimber.o obj/drivers/cclimber.o \
         obj/vidhrdw/ckong.o obj/drivers/ckong.o \
         obj/vidhrdw/dkong.o obj/drivers/dkong.o \
         obj/machine/bagman.o obj/vidhrdw/bagman.o obj/drivers/bagman.o \
         obj/vidhrdw/wow.o obj/drivers/wow.o \
         obj/drivers/galaxian.o \
         obj/vidhrdw/mooncrst.o obj/sndhrdw/mooncrst.o obj/drivers/mooncrst.o \
         obj/vidhrdw/moonqsr.o obj/drivers/moonqsr.o \
         obj/drivers/theend.o \
         obj/vidhrdw/frogger.o obj/drivers/frogger.o \
         obj/machine/scramble.o obj/vidhrdw/scramble.o obj/drivers/scramble.o \
         obj/drivers/scobra.o \
         obj/vidhrdw/amidar.o obj/drivers/amidar.o \
         obj/vidhrdw/rallyx.o obj/drivers/rallyx.o \
         obj/vidhrdw/pooyan.o obj/drivers/pooyan.o \
         obj/machine/phoenix.o obj/vidhrdw/phoenix.o obj/drivers/phoenix.o \
         obj/machine/carnival.o obj/vidhrdw/carnival.o obj/drivers/carnival.o \
         obj/machine/invaders.o obj/vidhrdw/invaders.o obj/drivers/invaders.o \
         obj/vidhrdw/mario.o obj/drivers/mario.o \
         obj/machine/zaxxon.o obj/vidhrdw/zaxxon.o obj/drivers/zaxxon.o \
         obj/vidhrdw/bombjack.o obj/drivers/bombjack.o \
         obj/Z80/Z80.o

VPATH = src src/Z80

all: mame

obj:
	mkdir -p $@
	mkdir -p $@/drivers
	mkdir -p $@/machine
	mkdir -p $@/vidhrdw
	mkdir -p $@/sndhrdw
	mkdir -p $@/Z80

mame: obj $(OBJS)
	$(LD) ${DEBUG_OR_OPTIMIZE} ${STRIP_OR_NOT} -o $@ $(OBJS) $(LIBS)

obj/osdepend.o: src/sdl/sdl.c
	 $(CC) $(DEFS) $(CFLAGS) -Isrc/sdl -o $@ -c $<

obj/%.o: src/%.c
	 $(CC) $(DEFS) $(CFLAGS) -o $@ -c $<

# dependencies
obj/%.o:	    common.h machine.h driver.h
obj/Z80.o:  Z80.c Z80.h Z80Codes.h Z80IO.h DAA.h


clean:
	rm -f obj/*.o
	rm -f obj/Z80/*.o
	rm -f obj/pacman/*.o
	rm -f obj/crush/*.o
	rm -f obj/pengo/*.o
	rm -f obj/ladybug/*.o

cleanall: clean
	rm -f src/Z80/*.o src/Z80/z80dasm mame roms/*/*.dsw #dsw is dip switch settings
	rm -rf obj

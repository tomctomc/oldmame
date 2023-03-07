CC	= gcc
LD	= gcc

DEFS   = -DLSB_FIRST

DEBUG_OR_OPTIMIZE=-ggdb
STRIP_OR_NOT=

#DEBUG_OR_OPTIMIZE= -O3 -fomit-frame-pointer -funroll-loops
#STRIP_OR_NOT=-s

#-DX86_ASM

CFLAGS = ${DEBUG_OR_OPTIMIZE} -Isrc -Isrc/Z80 -I/usr/include/SDL2 -Wall -Werror
LIBS   = -lSDL2 -lSDL2_mixer
OBJS   = obj/mame.o obj/common.o obj/machine.o obj/driver.o obj/osdepend.o \
         obj/pacman/machine.o \
		 obj/pacman/driver.o obj/crush/machine.o obj/crush/driver.o \
         obj/pengo/machine.o obj/pengo/vidhrdw.o obj/pengo/sndhrdw.o obj/pengo/driver.o \
         obj/ladybug/machine.o obj/ladybug/vidhrdw.o obj/ladybug/sndhrdw.o obj/ladybug/driver.o \
         obj/Z80/Z80.o

VPATH = src src/Z80

all: mame

obj:
	mkdir -p $@

obj/pacman:
	mkdir -p $@

obj/crush:
	mkdir -p $@

obj/pengo:
	mkdir -p $@

obj/ladybug:
	mkdir -p $@

obj/Z80:
	mkdir -p $@

mame: obj obj/pacman obj/crush obj/pengo obj/ladybug obj/Z80 $(OBJS)
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

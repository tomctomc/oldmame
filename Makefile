# set this to mame, mess or the destination you want to build
TARGET = mame
# TARGET = mess
# TARGET = neomame
# example for a tiny compile
# TARGET = tiny

# uncomment next line to include the debugger
# DEBUG = 1

# uncomment next line to include the symbols for symify
# SYMBOLS = 1

# uncomment next line to use Assembler 68k engine
# currently the Psikyo games don't work with it
# X86_ASM_68K = 1

# set this the operating system you're building for
# (actually you'll probably need your own main makefile anyways)
OS = sdl

# extension for executables
EXE =

# CPU core include paths
VPATH=src $(wildcard src/cpu/*)

# compiler, linker and utilities
AR = @ar
CC = @gcc
LD = @gcc
#ASM = @nasm
ASM = nasmw
ASMFLAGS = -f coff

ifdef DEBUG
NAME = $(TARGET)d
else
NAME = $(TARGET)
endif

# build the targets in different object dirs, since mess changes
# some structures and thus they can't be linked against each other.
# cleantiny isn't needed anymore, because the tiny build has its
# own object directory too.
OBJ = $(NAME).obj

EMULATOR = $(NAME)$(EXE)

DEFS = -DLSB_FIRST

WALL=-Wall
WERROR=-Werror
WALL=-w
WERROR=
OPTIMIZE_OR_DEBUG=-ggdb

INCLUDES=-Isrc -Isrc/sdl -I$(OBJ)/cpu/m68000 -Isrc/cpu/m68000 -Isrc/sdl -I/usr/include/SDL2 -DINLINE=static
ifdef SYMBOLS
CFLAGS = ${INCLUDES} \
	-pedantic ${WALL} ${WERROR} -Wno-unused ${OPTIMIZE_OR_DEBUG}
else
CFLAGS = ${INCLUDES} \
	-DNDEBUG \
	$(ARCH) ${OPTIMIZE_OR_DEBUG} -fomit-frame-pointer -fstrict-aliasing \
	${WALL} ${WERROR} -Wno-sign-compare -Wunused \
	-Wpointer-arith -Wbad-function-cast -Wcast-align -Waggregate-return \
	-pedantic \
	-Wshadow \
	-Wstrict-prototypes
#	-W had to remove because of the "missing initializer" warning
#	-Wredundant-decls \
#	-Wlarger-than-27648 \
#	-Wcast-qual \
#	-Wwrite-strings \
#	-Wconversion \
#	-Wmissing-prototypes \
#	-Wmissing-declarations
endif

ifdef SYMBOLS
LDFLAGS = -ggdb
else
#LDFLAGS = -s -Wl,--warn-common
LDFLAGS = -ggdb
endif

LIBS   = -lSDL2 -lSDL2_mixer -lm -lz

# check that the required libraries are available

#if obj subdirectory doesn't exist, create the tree before proceeding
ifeq ($(wildcard $(OBJ)),)
noobj:
	${MAKE} maketree
	${MAKE} all
endif

all:	$(EMULATOR) extra

# include the various .mak files
include src/core.mak
include src/$(TARGET).mak
include src/rules.mak
include src/$(OS)/$(OS).mak

ifdef DEBUG
DBGDEFS = -DMAME_DEBUG
else
DBGDEFS =
DBGOBJS =
endif

extra: $(TEXTS)
#$(TOOLS)
#romcmp$(EXE)

# combine the various definitions to one
CDEFS = $(DEFS) $(COREDEFS) $(CPUDEFS) $(SOUNDDEFS) $(ASMDEFS) $(DBGDEFS)

$(EMULATOR): $(OBJS) $(COREOBJS) $(OSOBJS) $(LIBS) $(DRVLIBS)
# always recompile the version string
	$(CC) $(CDEFS) $(CFLAGS) -c src/version.c -o $(OBJ)/version.o
	@echo Linking $@...
	$(LD) $(LDFLAGS) $(OBJS) $(COREOBJS) $(OSOBJS) $(LIBS) $(DRVLIBS) -o $@

romcmp$(EXE): $(OBJ)/romcmp.o $(OBJ)/unzip.o
	@echo Linking $@...
	$(LD) $(LDFLAGS) $^ -lz -o $@

$(OBJ)/%.o: src/%.c
	@echo Compiling $<...
	$(CC) $(CDEFS) $(CFLAGS) -c $< -o $@

# compile generated C files for the 68000 emulator
$(M68000_GENERATED_OBJS): $(OBJ)/cpu/m68000/m68kmake$(EXE)
	@echo Compiling $(subst .o,.c,$@)...
	$(CC) $(CDEFS) $(CFLAGS) -c $*.c -o $@

# additional rule, because m68kcpu.c includes the generated m68kops.h :-/
$(OBJ)/cpu/m68000/m68kcpu.o: $(OBJ)/cpu/m68000/m68kmake$(EXE)

# generate C source files for the 68000 emulator
$(OBJ)/cpu/m68000/m68kmake$(EXE): src/cpu/m68000/m68kmake.c
	@echo M68K make $<...
	$(CC) $(CDEFS) $(CFLAGS) -DDOS -o $(OBJ)/cpu/m68000/m68kmake$(EXE) $<
	@echo Generating M68K source files...
	$(OBJ)/cpu/m68000/m68kmake $(OBJ)/cpu/m68000 src/cpu/m68000/m68k_in.c

# generate asm source files for the 68000 emulator
$(OBJ)/cpu/m68000/68kem.asm:  src/cpu/m68000/make68k.c
	@echo Compiling $<...
	$(CC) $(CDEFS) $(CFLAGS) -O0 -DDOS -o $(OBJ)/cpu/m68000/make68k$(EXE) $<
	@echo Generating $@...
	@$(OBJ)/cpu/m68000/make68k$(EXE) $@ $(OBJ)/cpu/m68000/comptab.asm

# generated asm files for the 68000 emulator
$(OBJ)/cpu/m68000/68kem.o:  $(OBJ)/cpu/m68000/68kem.asm
	@echo Assembling $<...
	$(ASM) -o $@ $(ASMFLAGS) $(subst -D,-d,$(ASMDEFS)) $<

$(OBJ)/%.a:
	@echo Archiving $@...
	$(AR) cr $@ $^

makedir:
	@echo make makedir is no longer necessary, just type make

maketree:
	@echo Making MAME object tree in $(OBJ)...
	@mkdir $(OBJ)
	@mkdir $(OBJ)/cpu
	@mkdir $(OBJ)/cpu/z80
	@mkdir $(OBJ)/cpu/z80gb
	@mkdir $(OBJ)/cpu/m6502
	@mkdir $(OBJ)/cpu/h6280
	@mkdir $(OBJ)/cpu/i86
	@mkdir $(OBJ)/cpu/nec
	@mkdir $(OBJ)/cpu/i8039
	@mkdir $(OBJ)/cpu/i8085
	@mkdir $(OBJ)/cpu/m6800
	@mkdir $(OBJ)/cpu/m6805
	@mkdir $(OBJ)/cpu/m6809
	@mkdir $(OBJ)/cpu/konami
	@mkdir $(OBJ)/cpu/m68000
	@mkdir $(OBJ)/cpu/s2650
	@mkdir $(OBJ)/cpu/t11
	@mkdir $(OBJ)/cpu/tms34010
	@mkdir $(OBJ)/cpu/tms9900
	@mkdir $(OBJ)/cpu/z8000
	@mkdir $(OBJ)/cpu/tms32010
	@mkdir $(OBJ)/cpu/ccpu
	@mkdir $(OBJ)/cpu/adsp2100
	@mkdir $(OBJ)/cpu/pdp1
	@mkdir $(OBJ)/cpu/mips
	@mkdir $(OBJ)/cpu/sc61860
	@mkdir $(OBJ)/sound
	@mkdir $(OBJ)/sdl
	@mkdir $(OBJ)/drivers
	@mkdir $(OBJ)/machine
	@mkdir $(OBJ)/vidhrdw
	@mkdir $(OBJ)/sndhrdw
ifdef MESS
	@echo Making MESS object tree in $(OBJ)/mess...
	@mkdir $(OBJ)/mess
	@mkdir $(OBJ)/mess/systems
	@mkdir $(OBJ)/mess/machine
	@mkdir $(OBJ)/mess/vidhrdw
	@mkdir $(OBJ)/mess/sndhrdw
	@mkdir $(OBJ)/mess/tools
endif

clean:
	@echo Deleting object tree $(OBJ)...
	rm -rf $(OBJ)
	@echo Deleting $(EMULATOR)...
	@rm -f $(EMULATOR)

cleandebug:
	@echo Deleting debug obj tree...
	@rm -f $(OBJ)/*.o
	@rm -f $(OBJ)/cpu/z80/*.o
	@rm -f $(OBJ)/cpu/z80gb/*.o
	@rm -f $(OBJ)/cpu/m6502/*.o
	@rm -f $(OBJ)/cpu/h6280/*.o
	@rm -f $(OBJ)/cpu/i86/*.o
	@rm -f $(OBJ)/cpu/nec/*.o
	@rm -f $(OBJ)/cpu/i8039/*.o
	@rm -f $(OBJ)/cpu/i8085/*.o
	@rm -f $(OBJ)/cpu/m6800/*.o
	@rm -f $(OBJ)/cpu/m6805/*.o
	@rm -f $(OBJ)/cpu/m6809/*.o
	@rm -f $(OBJ)/cpu/konami/*.o
	@rm -f $(OBJ)/cpu/m68000/*.o
	@rm -f $(OBJ)/cpu/m68000/*.c
	@rm -f $(OBJ)/cpu/s2650/*.o
	@rm -f $(OBJ)/cpu/t11/*.o
	@rm -f $(OBJ)/cpu/tms34010/*.o
	@rm -f $(OBJ)/cpu/tms9900/*.o
	@rm -f $(OBJ)/cpu/z8000/*.o
	@rm -f $(OBJ)/cpu/tms32010/*.o
	@rm -f $(OBJ)/cpu/ccpu/*.o
	@rm -f $(OBJ)/cpu/adsp2100/*.o
	@rm -f $(OBJ)/cpu/pdp1/*.o
	@rm -f $(OBJ)/cpu/mips/*.o
	@rm -f $(EMULATOR)

cleanall: clean
	rm -rf gamelist.txt obj

fromdos_everything:
	for i in `find src | grep \\\.c$$` ; do fromdos "$$i" ; done
	for i in `find src | grep \\\.h$$` ; do fromdos "$$i" ; done
	fromdos *.txt src/*txt src/*/*txt

newfix:
	for i in `grep -rl "GAMENAME\#\# " src`; do sed -e "s,GAMENAME\#\# ,GAMENAME ,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "\#\#name\#\#" src`; do sed -e "s,\#\#name\#\#,name##,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "\#\#paltype\#\#" src`; do sed -e "s,\#\#paltype\#\#,paltype,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done

fix:
	rm -f src/drivers/jrcrypt.c # RAM undefined (no decryption?)
	rm -f src/romcmp.c # msdos stuff in there
	-mv src/sound/tms5220r.c src/sound/tms5220r.included_c # this c file is included in another.  we don't want to auto include it in our c source list, so rename it
	for i in `grep -rl "MAX_OUTPUT.*0x7fff" src`; do sed -e "s,MAX_OUTPUT.*0x7fff,MAX_OUTPUT ((double) 0x7fff),g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
#define MAX_OUTPUT ( (double) 0x7fff )
	for i in `grep -rl "tms5220r.c" src`; do sed -e "s,tms5220r.c,tms5220r.included_c,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "ay8910u.c" src`; do sed -e "s,ay8910u.c,ay8910u.included_c,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "sn76496u.c" src`; do sed -e "s,sn76496u.c,sn76496u.included_c,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "rda##advance" src`; do sed -e "s,rda##advance,rda advance,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "s16sprit.c" src`; do grep -v "s16sprit.c" "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Ii]8085/[Ii]8085" src`; do sed -e "s,[Ii]8085/[Ii]8085,i8085/i8085,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "I8085cpu.h" src`; do sed -e "s,[Ii]8085cpu.h,i8085cpu.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "I8085daa.h" src`; do sed -e "s,[Ii]8085daa.h,i8085daa.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Tt]11/[Tt]11.h" src`; do sed -e "s,[Tt]11/[Tt]11.h,t11/t11.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "machine/[Zz]80fmly.h" src`; do sed -e "s,machine/[Zz]80fmly.h,machine/z80fmly.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Zz]80.[Zz]80.h" src`; do sed -e "s,[Zz]80.[Zz]80.h,z80/z80.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Tt][Mm][Ss]9900/[Tt][Mm][Ss]9900" src`; do sed -e "s,[Tt][Mm][Ss]9900/[Tt][Mm][Ss]9900,tms9900/tms9900,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Tt][Mm][Ss]34010/[Tt][Mm][Ss]34010" src`; do sed -e "s,[Tt][Mm][Ss]34010/[Tt][Mm][Ss]34010,tms34010/tms34010,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Tt][Mm][Ss]34061/[Tt][Mm][Ss]34061" src`; do sed -e "s,[Tt][Mm][Ss]34061/[Tt][Mm][Ss]34061,tms34061/tms34061,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Mm]68000/[Mm]68000.h" src`; do sed -e "s,[Mm]68000/[Mm]68000.h,m68000/m68000.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Mm]6502/[Mm]6502.h" src`; do sed -e "s,[Mm]6502/[Mm]6502.h,m6502/m6502.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Mm]6805/[Mm]6805.h" src`; do sed -e "s,[Mm]6805/[Mm]6805.h,m6805/m6805.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Mm]6808/[Mm]6808.h" src`; do sed -e "s,[Mm]6808/[Mm]6808.h,m6808/m6808.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Mm]6809/[Mm]6809.h" src`; do sed -e "s,[Mm]6809/[Mm]6809.h,m6809/m6809.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Mm]68000.h" src`; do sed -e "s,[Mm]68000.h,m68000.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	for i in `grep -rl "[Mm]6809.h" src`; do sed -e "s,[Mm]6809.h,m6809.h,g" < "$$i" > /tmp/joe; mv /tmp/joe "$$i"; done
	sed -e "s,\&\#\#,\&,g" < "src/cpuintrf.c" > /tmp/joe; mv /tmp/joe "src/cpuintrf.c"
	grep -v M_PI src/driver.h > /tmp/joe
	echo "#define PI M_PI" >> /tmp/joe
	mv /tmp/joe src/driver.h

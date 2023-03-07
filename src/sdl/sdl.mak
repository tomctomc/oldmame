# only MS-DOS specific output files and rules
OSOBJS = $(OBJ)/sdl/sdl.o \
	$(OBJ)/sdl/stricmp.o \
	$(OBJ)/sdl/input.o \
	$(OBJ)/sdl/fileio.o \
	$(OBJ)/sdl/video.o \
	$(OBJ)/sdl/config.o \
	$(OBJ)/sdl/sound.o \
	$(OBJ)/sdl/fronthlp.o \


# check if this is a MESS build
ifdef MESS
# additional OS specific object files
OSOBJS += $(OBJ)/mess/sdl.o $(OBJ)/sdl/nec765.o
endif


# cpp Makefile for libprimis
# intended for g++ c++17 and linux builds only, use vcpp project for Windows builds

# -o3: slowest compile times, but fastest code
# -ffast-math: force less accurate but faster floating point calcs
# -std=c++17: compile to c++17 standard, needed for some std library functions
# -march=x86-64: compile only for x64 platforms (32 bit not officially supported)
# -Wall: show "all" level of compiler warnings (which is not actually all possible warnings)
# -fsigned-char: have the `char` type be signed (as opposed to `uchar`)
# -fno-rtti: disable runtime type interpretation, it's not used
# -fpic: compile position independent code for library creation

CXXFLAGS= -O3 -ffast-math -std=c++17 -march=x86-64 -Wall -fsigned-char -fno-rtti -fpic

	CLIENT_INCLUDES= -Ishared -Iengine $(INCLUDES) -I/usr/X11R6/include `sdl2-config --cflags`
	CLIENT_LIBS= -L/usr/X11R6/lib -lX11 `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lm -lz -lGL -lrt

COVERAGE_BUILD ?= 0

ifeq (1,$(COVERAGE_BUILD))
	CXXFLAGS += --coverage -fno-inline -fno-inline-small-functions -fno-default-inline
endif
#list of source code files to be compiled
CLIENT_OBJS= \
	shared/geom.o \
	shared/glemu.o \
	shared/stream.o \
	shared/tools.o \
	shared/zip.o \
	engine/interface/command.o \
	engine/interface/control.o \
	engine/interface/cubestd.o \
	engine/interface/console.o \
	engine/interface/input.o \
	engine/interface/menus.o \
	engine/interface/sound.o \
	engine/interface/textedit.o \
	engine/interface/ui.o \
	engine/model/animmodel.o \
	engine/model/hitzone.o \
	engine/model/obj.o \
	engine/model/ragdoll.o \
	engine/model/skelmodel.o \
	engine/render/aa.o \
	engine/render/ao.o \
	engine/render/csm.o \
	engine/render/grass.o \
	engine/render/hdr.o \
	engine/render/hud.o \
	engine/render/normal.o \
	engine/render/octarender.o \
	engine/render/radiancehints.o \
	engine/render/renderalpha.o \
	engine/render/rendergl.o \
	engine/render/renderlights.o \
	engine/render/rendermodel.o \
	engine/render/renderparticles.o \
	engine/render/rendersky.o \
	engine/render/rendertext.o \
	engine/render/rendertimers.o \
	engine/render/renderva.o \
	engine/render/renderwindow.o \
	engine/render/shader.o \
	engine/render/stain.o \
	engine/render/texture.o \
	engine/render/water.o \
	engine/world/bih.o \
	engine/world/dynlight.o \
	engine/world/heightmap.o \
	engine/world/light.o \
	engine/world/material.o \
	engine/world/mpr.o \
	engine/world/octaedit.o \
	engine/world/octaworld.o \
	engine/world/physics.o \
	engine/world/raycube.o \
	engine/world/world.o \
	engine/world/worldio.o \

#default: compiles the library and places it in the same directory as this file
default: client

#install/emplace: places the library into the /usr/lib directory
install: client emplace

clean:
	-$(RM) -r $(CLIENT_OBJS)

#compiles the objects
$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)

#change whether -lgcov is included depending on whether the build is intended
#for gcc coverage checking
ifeq (1,$(COVERAGE_BUILD))
client: $(CLIENT_OBJS)
	$(CXX) -shared -o libprimis.so $(CLIENT_OBJS) -lgcov
else
client: $(CLIENT_OBJS)
	$(CXX) -shared -o libprimis.so $(CLIENT_OBJS)
endif

emplace:
	sudo cp libprimis.so /usr/lib/libprimis.so


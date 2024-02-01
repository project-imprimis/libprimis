# cpp Makefile for libprimis
# intended for g++/clang c++17 on linux builds only, use vcpp project for Windows builds

# -o3: slowest compile times, but fastest code
# -ffast-math: force less accurate but faster floating point calcs
# -std=c++17: compile to c++17 standard, needed for some std library functions
# -march=x86-64: compile only for x64 platforms (32 bit not officially supported)
# -Wall: show "all" level of compiler warnings (which is not actually all possible warnings)

# -fsigned-char: have the `char` type be signed (as opposed to `uchar`)
# -fno-rtti: disable runtime type interpretation, it's not used
# -fpic: compile position independent code for library creation

CXXFLAGS ?= -ffast-math -Wall -Wsuggest-override
CXXFLAGS += -march=x86-64 -fsigned-char -fno-rtti -fpic
#-Ishared
CLIENT_INCLUDES=  -Iengine $(INCLUDES) `sdl2-config --cflags`

COVERAGE_BUILD ?= 0

CPP_20 ?= 0

PREFIX ?= /usr/local

# if you want to compile with coverage flags, use `make -Csrc COVERAGE_BUILD=1`

ifeq (1,$(COVERAGE_BUILD))
	CXXFLAGS += -O0 -fno-inline -fno-inline-small-functions -fno-default-inline -fprofile-arcs -ftest-coverage
else
	CXXFLAGS += -O3
endif

# if you want to compile with c++20, use `make -Csrc CPP_20=1`, else c++17
ifeq (1,$(CPP_20))
	CXXFLAGS += -std=c++20
else
	CXXFLAGS += -std=c++17
endif

#list of source code files to be compiled
CLIENT_OBJS= \
	shared/geom.o \
	shared/glemu.o \
	shared/matrix.o \
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
	engine/model/gltf.o \
	engine/model/gltfloader.o \
	engine/model/md5.o \
	engine/model/obj.o \
	engine/model/ragdoll.o \
	engine/model/skelmodel.o \
	engine/model/vertmodel.o \
	engine/render/aa.o \
	engine/render/ao.o \
	engine/render/csm.o \
	engine/render/grass.o \
	engine/render/hdr.o \
	engine/render/hud.o \
	engine/render/imagedata.o \
	engine/render/lightsphere.o \
	engine/render/normal.o \
	engine/render/octarender.o \
	engine/render/postfx.o \
	engine/render/radiancehints.o \
	engine/render/renderalpha.o \
	engine/render/rendergl.o \
	engine/render/renderlights.o \
	engine/render/rendermodel.o \
	engine/render/renderparticles.o \
	engine/render/rendersky.o \
	engine/render/rendertext.o \
	engine/render/renderttf.o \
	engine/render/rendertimers.o \
	engine/render/renderva.o \
	engine/render/renderwindow.o \
	engine/render/shader.o \
	engine/render/shaderparam.o \
	engine/render/stain.o \
	engine/render/texture.o \
	engine/render/vacollect.o \
	engine/render/water.o \
	engine/world/bih.o \
	engine/world/dynlight.o \
	engine/world/entities.o \
	engine/world/heightmap.o \
	engine/world/light.o \
	engine/world/material.o \
	engine/world/mpr.o \
	engine/world/octaedit.o \
	engine/world/octacube.o \
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
	-$(RM) -r engine/*/*.gcda engine/*/*.gcno shared/*.gcda shared/*.gcno

#compiles the objects
$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)

#change whether -lgcov is included depending on whether the build is intended
#for gcc coverage checking
ifeq (1,$(COVERAGE_BUILD))
client: $(CLIENT_OBJS)
	$(CXX) -shared -o libprimis.so $(CLIENT_OBJS) --coverage
else
client: $(CLIENT_OBJS)
	$(CXX) -shared -o libprimis.so $(CLIENT_OBJS)
endif

emplace:
	mkdir --parents $(DESTDIR)$(PREFIX)/lib/
	cp libprimis.so $(DESTDIR)$(PREFIX)/lib/libprimis.so

uninstall:
	rm $(DESTDIR)$(PREFIX)/lib/libprimis.so


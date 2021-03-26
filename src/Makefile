# -o3: slowest compile times
# -ffast-math:
# -std=c++17: compile to c++17 standard, needed for some std library functions
# -march=x86-64: compile only for x64 platforms (32 bit not officially supported)
# -Wall: show "all" level of compiler warnings
# -fsigned-char: have the `char` type be signed (as opposed to `uchar`)
# -fno-rtti: disable runtime type interpretation, it's not used
# -fpic: compile position independent code for library creation

CXXFLAGS= -O3 -ffast-math -std=c++17 -march=x86-64 -Wall -fsigned-char -fno-rtti -fpic

	CLIENT_INCLUDES= -Ishared -Iengine -Igame $(INCLUDES) -I/usr/X11R6/include `sdl2-config --cflags`
	CLIENT_LIBS= -L/usr/X11R6/lib -lX11 `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lm -lz -lGL -lrt

#list of source code files to be compiled
CLIENT_OBJS= \
	shared/crypto.o \
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
	engine/world/octa.o \
	engine/world/octaedit.o \
	engine/world/physics.o \
	engine/world/raycube.o \
	engine/world/world.o \
	engine/world/worldio.o \

default: client
install: client emplace

clean:
	-$(RM) -r $(CLIENT_OBJS)

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)

client: $(CLIENT_OBJS)
	$(CXX) -shared -o libprimis.so $(CLIENT_OBJS)

emplace:
	sudo cp libprimis.so /usr/lib/libprimis.so


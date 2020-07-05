CXXFLAGS= -O3 -fomit-frame-pointer -ffast-math -std=c++14 -march=x86-64
override CXXFLAGS+= -Wall -fsigned-char -fno-exceptions -fno-rtti

PLATFORM= $(shell uname -s)
PLATFORM_ARCH= $(shell uname -m)
PLATFORM_PREFIX= native

INCLUDES= -Ishared -Iengine -Igame -Ienet/include

STRIP=
ifeq (,$(findstring -g,$(CXXFLAGS)))
    ifeq (,$(findstring -pg,$(CXXFLAGS)))
        STRIP=strip
    endif
endif

MV=mv
MKDIR_P=mkdir -p

#set appropriate library includes depending on platform
ifneq (,$(findstring MINGW,$(PLATFORM)))
    WINDRES= windres
    ifneq (,$(findstring 64,$(PLATFORM)))
        ifneq (,$(findstring CROSS,$(PLATFORM)))
            CXX=x86_64-w64-mingw32-g++
            WINDRES=x86_64-w64-mingw32-windres
            ifneq (,$(STRIP))
                STRIP=x86_64-w64-mingw32-strip
            endif
        endif
        WINLIB=lib64
        WINBIN=../bin64
        override CXX+= -m64
        override WINDRES+= -F pe-x86-64
    else
    ifneq (,$(findstring CROSS,$(PLATFORM)))
        CXX=i686-w64-mingw32-g++
        WINDRES=i686-w64-mingw32-windres
        ifneq (,$(STRIP))
            STRIP=i686-w64-mingw32-strip
        endif
    endif
        WINLIB=lib
        WINBIN=../bin
        override CXX+= -m32
        override WINDRES+= -F pe-i386
    endif
    CLIENT_INCLUDES= $(INCLUDES) -Iinclude
    STD_LIBS= -static-libgcc -static-libstdc++
    CLIENT_LIBS= -mwindows $(STD_LIBS) -L$(WINBIN) -L$(WINLIB) -lSDL2 -lSDL2_image -lSDL2_mixer -lzlib1 -lopengl32 -lenet -lws2_32 -lwinmm
    else
        CLIENT_INCLUDES= $(INCLUDES) -I/usr/X11R6/include `sdl2-config --cflags`
        CLIENT_LIBS= -Lenet -lenet -L/usr/X11R6/lib -lX11 `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lz -lGL
    endif
    ifeq ($(PLATFORM),Linux)
        CLIENT_LIBS+= -lrt
    else
    ifneq (,$(findstring GNU,$(PLATFORM)))
        CLIENT_LIBS+= -lrt
    endif
endif

#list of source code files to be compiled
CLIENT_OBJS= \
	shared/crypto.o \
	shared/geom.o \
	shared/glemu.o \
	shared/stream.o \
	shared/tools.o \
	shared/zip.o \
	engine/main.o \
	engine/interface/client.o \
	engine/interface/command.o \
	engine/interface/console.o \
	engine/interface/input.o \
	engine/interface/menus.o \
	engine/interface/server.o \
	engine/interface/serverbrowser.o \
	engine/interface/sound.o \
	engine/interface/ui.o \
	engine/render/aa.o \
	engine/render/grass.o \
	engine/render/normal.o \
	engine/render/octarender.o \
	engine/render/radiancehints.o \
	engine/render/rendergl.o \
	engine/render/renderlights.o \
	engine/render/rendermodel.o \
	engine/render/renderparticles.o \
	engine/render/rendersky.o \
	engine/render/rendertext.o \
	engine/render/renderva.o \
	engine/render/renderwindow.o \
	engine/render/shader.o \
	engine/render/stain.o \
	engine/render/texture.o \
	engine/render/water.o \
	engine/world/bih.o \
	engine/world/dynlight.o \
	engine/world/light.o \
	engine/world/material.o \
	engine/world/octa.o \
	engine/world/octaedit.o \
	engine/world/physics.o \
	engine/world/raycube.o \
	engine/world/world.o \
	engine/world/worldio.o \
	game/ai.o \
	game/client.o \
	game/entities.o \
	game/game.o \
	game/render.o \
	game/scoreboard.o \
	game/server.o \
	game/waypoint.o \
	game/weapon.o

default: client

clean:
	-$(RM) -r $(CLIENT_OBJS) tess_client

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)

#mingw
ifneq (,$(findstring MINGW,$(PLATFORM)))
client: $(CLIENT_OBJS)
	$(WINDRES) -I vcpp -i vcpp/mingw.rc -J rc -o vcpp/mingw.res -O coff
	$(CXX) $(CXXFLAGS) -o $(WINBIN)/tesseract.exe vcpp/mingw.res $(CLIENT_OBJS) $(CLIENT_LIBS)

install: client
else
#native (gcc et. al.)
client:	libenet $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o tess_client $(CLIENT_OBJS) $(CLIENT_LIBS)

install: client
	cp -f tess_client	../bin_unix/$(PLATFORM_PREFIX)_client
ifneq (,$(STRIP))
	$(STRIP) ../bin_unix/$(PLATFORM_PREFIX)_client
endif
endif

enet/libenet.a:
	$(MAKE) -C enet
libenet: enet/libenet.a

depend:
	makedepend -Y -Ishared -Iengine -Igame $(CLIENT_OBJS:.o=.cpp)

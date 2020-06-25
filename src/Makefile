CXXFLAGS= -O3 -fomit-frame-pointer -ffast-math -std=c++14
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

CLIENT_PCH= shared/cube.h.gch engine/engine.h.gch game/game.h.gch

STANDALONE_PATHS= \
	standalone/ \
	standalone/engine/ \
	standalone/engine/world \
	standalone/engine/interface \
	standalone/engine/render \
	standalone/game/ \
	standalone/shared/

SERVER_INCLUDES= -DSTANDALONE -Istandalone/shared -Istandalone/engine -Istandalone/game $(INCLUDES)
ifneq (,$(findstring MINGW,$(PLATFORM)))
SERVER_INCLUDES+= -Iinclude
SERVER_LIBS= -mwindows $(STD_LIBS) -L$(WINBIN) -L$(WINLIB) -lzlib1 -lenet -lws2_32 -lwinmm
else
SERVER_LIBS= -Lenet -lenet -lz
endif

SERVER_OBJS= \
	standalone/shared/crypto.o \
	standalone/shared/stream.o \
	standalone/shared/tools.o \
	standalone/engine/interface/command.o \
	standalone/engine/interface/server.o \
	standalone/engine/world/worldio.o \
	standalone/game/entities.o \
	standalone/game/server.o

SERVER_PCH= standalone/shared/cube.h.gch standalone/engine/engine.h.gch standalone/game/game.h.gch

SERVER_MASTER_OBJS= $(SERVER_OBJS) $(filter-out $(SERVER_OBJS),$(MASTER_OBJS))

default: all

all: mkpaths client server

clean:
	-$(RM) -r $(CLIENT_PCH) $(CLIENT_OBJS) $(SERVER_PCH) $(SERVER_MASTER_OBJS) $(STANDALONE_PATHS) tess_client tess_server tess_master

fixspace:
	sed -i 's/[ \t]*$$//; :rep; s/^\([ ]*\)\t/\1    /g; trep' shared/*.c shared/*.cpp shared/*.h engine/*.cpp engine/*.h game/*.cpp game/*.h

mkpaths:
	$(MKDIR_P) $(STANDALONE_PATHS)

$(filter-out shared/%,$(CLIENT_PCH)): $(filter shared/%,$(CLIENT_PCH))
$(CLIENT_PCH): %.h.gch: %.h
	$(CXX) $(CXXFLAGS) -x c++-header -o $@.tmp $<
	$(MV) $@.tmp $@

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)
$(filter shared/%,$(CLIENT_OBJS)): $(filter shared/%,$(CLIENT_PCH))
$(filter engine/%,$(CLIENT_OBJS)): $(filter engine/%,$(CLIENT_PCH))
$(filter game/%,$(CLIENT_OBJS)): $(filter game/%,$(CLIENT_PCH))

$(filter-out standalone/shared/%,$(SERVER_PCH)): $(filter standalone/shared/%,$(SERVER_PCH))
$(SERVER_PCH): standalone/%.h.gch: %.h
	$(CXX) $(CXXFLAGS) -x c++-header -o $@.tmp $<
	$(MV) $@.tmp $@

$(SERVER_MASTER_OBJS): CXXFLAGS += $(SERVER_INCLUDES)
$(filter standalone/shared/%,$(SERVER_MASTER_OBJS)): $(filter standalone/shared/%,$(SERVER_PCH))
$(filter standalone/engine/%,$(SERVER_MASTER_OBJS)): $(filter standalone/engine/%,$(SERVER_PCH))
$(filter standalone/game/%,$(SERVER_MASTER_OBJS)): $(filter standalone/game/%,$(SERVER_PCH))

$(SERVER_MASTER_OBJS): standalone/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

ifneq (,$(findstring MINGW,$(PLATFORM)))
client: $(CLIENT_OBJS)
	$(WINDRES) -I vcpp -i vcpp/mingw.rc -J rc -o vcpp/mingw.res -O coff
	$(CXX) $(CXXFLAGS) -o $(WINBIN)/tesseract.exe vcpp/mingw.res $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(WINDRES) -I vcpp -i vcpp/mingw.rc -J rc -o vcpp/mingw.res -O coff
	$(CXX) $(CXXFLAGS) -o $(WINBIN)/tess_server.exe vcpp/mingw.res $(SERVER_OBJS) $(SERVER_LIBS)


install: all
else
client:	libenet $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o tess_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server:	libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o tess_server $(SERVER_OBJS) $(SERVER_LIBS)

install: all
	cp -f tess_client	../bin_unix/$(PLATFORM_PREFIX)_client
	cp -f tess_server	../bin_unix/$(PLATFORM_PREFIX)_server
ifneq (,$(STRIP))
	$(STRIP) ../bin_unix/$(PLATFORM_PREFIX)_client
	$(STRIP) ../bin_unix/$(PLATFORM_PREFIX)_server
endif
endif

enet/libenet.a:
	$(MAKE) -C enet
libenet: enet/libenet.a

depend:
	makedepend -Y -Ishared -Iengine -Igame $(CLIENT_OBJS:.o=.cpp)
	makedepend -a -o.h.gch -Y -Ishared -Iengine -Igame $(CLIENT_PCH:.h.gch=.h)
	makedepend -a -pstandalone/ -Y -DSTANDALONE -Ishared -Iengine -Igame $(SERVER_MASTER_OBJS:standalone/%.o=%.cpp)
	makedepend -a -pstandalone/ -o.h.gch -Y -DSTANDALONE -Ishared -Iengine -Igame $(SERVER_PCH:standalone/%.h.gch=%.h)

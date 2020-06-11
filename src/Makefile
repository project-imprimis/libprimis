CXXFLAGS= -O3 -fomit-frame-pointer -ffast-math
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

# DO NOT DELETE

#shared
shared/crypto.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/crypto.o: shared/command.h shared/glexts.h shared/glemu.h
shared/crypto.o: shared/iengine.h shared/igame.h
shared/geom.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/geom.o: shared/command.h shared/glexts.h shared/glemu.h
shared/geom.o: shared/iengine.h shared/igame.h
shared/glemu.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/glemu.o: shared/command.h shared/glexts.h shared/glemu.h
shared/glemu.o: shared/iengine.h shared/igame.h
shared/stream.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/stream.o: shared/command.h shared/glexts.h shared/glemu.h
shared/stream.o: shared/iengine.h shared/igame.h
shared/tools.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/tools.o: shared/command.h shared/glexts.h shared/glemu.h
shared/tools.o: shared/iengine.h shared/igame.h
shared/zip.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/zip.o: shared/command.h shared/glexts.h shared/glemu.h
shared/zip.o: shared/iengine.h shared/igame.h
engine/main.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/main.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/main.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/main.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/interface/client.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/interface/client.o: shared/ents.h shared/command.h shared/glexts.h
engine/interface/client.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/interface/client.o: engine/world/world.h engine/world/octa.h engine/world/light.h engine/render/texture.h
engine/interface/client.o: engine/world/bih.h engine/model/model.h
engine/interface/command.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/interface/command.o: shared/ents.h shared/command.h shared/glexts.h
engine/interface/command.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/interface/command.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/interface/command.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/interface/console.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/interface/console.o: shared/ents.h shared/command.h shared/glexts.h
engine/interface/console.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/interface/console.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/interface/console.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/interface/menus.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/interface/menus.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/interface/menus.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/interface/menus.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/interface/server.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/interface/server.o: shared/ents.h shared/command.h shared/glexts.h
engine/interface/server.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/interface/server.o: engine/world/world.h engine/world/octa.h engine/world/light.h engine/render/texture.h
engine/interface/server.o: engine/world/bih.h engine/model/model.h
engine/interface/serverbrowser.o: engine/engine.h shared/cube.h shared/tools.h
engine/interface/serverbrowser.o: shared/geom.h shared/ents.h shared/command.h
engine/interface/serverbrowser.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/interface/serverbrowser.o: shared/igame.h engine/world/world.h engine/world/octa.h
engine/interface/serverbrowser.o: engine/world/light.h engine/render/texture.h engine/world/bih.h
engine/interface/serverbrowser.o: engine/model/model.h
engine/interface/ui.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/interface/ui.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/interface/ui.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/interface/ui.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/interface/ui.o: engine/interface/textedit.h
engine/render/aa.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/aa.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/aa.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/aa.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/grass.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/grass.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/grass.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/grass.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/normal.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/normal.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/normal.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/normal.o: engine/world/world.h engine/world/octa.h engine/world/light.h engine/render/texture.h
engine/render/normal.o: engine/world/bih.h engine/model/model.h
engine/render/octarender.o: engine/engine.h shared/cube.h shared/tools.h
engine/render/octarender.o: shared/geom.h shared/ents.h shared/command.h
engine/render/octarender.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/render/octarender.o: shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/octarender.o: engine/world/light.h engine/render/texture.h engine/world/bih.h
engine/render/octarender.o: engine/model/model.h
engine/render/radiancehints.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/radiancehints.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/radiancehints.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/radiancehints.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/radiancehints.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/radiancehints.o: engine/render/radiancehints.h
engine/render/rendergl.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/rendergl.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/rendergl.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/rendergl.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/rendergl.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/rendergl.o: engine/render/radiancehints.h
engine/render/renderlights.o: engine/engine.h shared/cube.h shared/tools.h
engine/render/renderlights.o: shared/geom.h shared/ents.h shared/command.h
engine/render/renderlights.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/render/renderlights.o: shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/renderlights.o: engine/world/light.h engine/render/texture.h engine/world/bih.h
engine/render/renderlights.o: engine/model/model.h engine/render/radiancehints.h
engine/render/rendermodel.o: engine/engine.h shared/cube.h shared/tools.h
engine/render/rendermodel.o: shared/geom.h shared/ents.h shared/command.h
engine/render/rendermodel.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/render/rendermodel.o: shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/rendermodel.o: engine/world/light.h engine/render/texture.h engine/world/bih.h
engine/render/rendermodel.o: engine/model/model.h engine/model/ragdoll.h engine/model/animmodel.h
engine/render/rendermodel.o: engine/model/vertmodel.h engine/model/skelmodel.h engine/world/hitzone.h
engine/render/rendermodel.o: engine/model/md5.h engine/model/obj.h
engine/render/rendermodel.o: engine/engine.h shared/cube.h shared/tools.h
engine/render/rendermodel.o: shared/geom.h shared/ents.h shared/command.h
engine/render/rendermodel.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/render/rendermodel.o: shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/rendermodel.o: engine/world/light.h engine/render/texture.h engine/world/bih.h
engine/render/rendermodel.o: engine/model/model.h engine/world/explosion.h
engine/render/rendermodel.o: engine/render/radiancehints.h
engine/render/rendersky.o: engine/engine.h shared/cube.h shared/tools.h
engine/render/rendersky.o: shared/geom.h shared/ents.h shared/command.h
engine/render/rendersky.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/render/rendersky.o: shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/rendersky.o: engine/world/light.h engine/render/texture.h engine/world/bih.h
engine/render/rendersky.o: engine/model/model.h
engine/render/rendertext.o: engine/engine.h shared/cube.h shared/tools.h
engine/render/rendertext.o: shared/geom.h shared/ents.h shared/command.h
engine/render/rendertext.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/render/rendertext.o: shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/rendertext.o: engine/world/light.h engine/render/texture.h engine/world/bih.h
engine/render/rendertext.o: engine/model/model.h
engine/render/renderva.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/renderva.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/renderva.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/renderva.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/renderva.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/shader.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/shader.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/shader.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/shader.o: engine/world/world.h engine/world/octa.h engine/world/light.h engine/render/texture.h
engine/render/shader.o: engine/world/bih.h engine/model/model.h
engine/render/sound.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/sound.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/sound.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/sound.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/stain.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/stain.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/stain.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/stain.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/texture.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/texture.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/texture.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/texture.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/texture.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/bih.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/bih.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/bih.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/bih.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/dynlight.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/dynlight.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/dynlight.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/dynlight.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/dynlight.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/light.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/light.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/light.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/light.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/light.o: engine/render/radiancehints.h
engine/render/material.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/material.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/material.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/material.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/material.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/octa.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/octa.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/octa.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/octa.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/octaedit.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/octaedit.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/octaedit.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/octaedit.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/octaedit.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/physics.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/physics.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/physics.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/physics.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/physics.o: engine/render/texture.h engine/world/bih.h engine/model/model.h engine/mpr.h
engine/render/water.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/water.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/water.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/water.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/world.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/world.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/render/world.o: shared/iengine.h shared/igame.h engine/world/world.h engine/world/octa.h
engine/render/world.o: engine/world/light.h engine/render/texture.h engine/world/bih.h engine/model/model.h
engine/render/worldio.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/render/worldio.o: shared/ents.h shared/command.h shared/glexts.h
engine/render/worldio.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/render/worldio.o: engine/world/world.h engine/world/octa.h engine/world/light.h
engine/render/worldio.o: engine/render/texture.h engine/world/bih.h engine/model/model.h
game/ai.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/ai.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/ai.o: shared/iengine.h shared/igame.h game/ai.h
game/client.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/client.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/client.o: shared/iengine.h shared/igame.h game/ai.h game/ctf.h
game/entities.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/entities.o: shared/ents.h shared/command.h shared/glexts.h
game/entities.o: shared/glemu.h shared/iengine.h shared/igame.h game/ai.h
game/game.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/game.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/game.o: shared/iengine.h shared/igame.h game/ai.h
game/render.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/render.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/render.o: shared/iengine.h shared/igame.h game/ai.h
game/scoreboard.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/scoreboard.o: shared/ents.h shared/command.h shared/glexts.h
game/scoreboard.o: shared/glemu.h shared/iengine.h shared/igame.h game/ai.h
game/server.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/server.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/server.o: shared/iengine.h shared/igame.h game/ai.h game/ctf.h
game/waypoint.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/waypoint.o: shared/ents.h shared/command.h shared/glexts.h
game/waypoint.o: shared/glemu.h shared/iengine.h shared/igame.h game/ai.h
game/weapon.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/weapon.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/weapon.o: shared/iengine.h shared/igame.h game/ai.h
shared/cube.h.gch: shared/tools.h shared/geom.h shared/ents.h
shared/cube.h.gch: shared/command.h shared/glexts.h shared/glemu.h
shared/cube.h.gch: shared/iengine.h shared/igame.h
engine/engine.h.gch: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/engine.h.gch: shared/command.h shared/glexts.h shared/glemu.h
engine/engine.h.gch: shared/iengine.h shared/igame.h engine/world/world.h
engine/engine.h.gch: engine/world/octa.h engine/world/light.h engine/render/texture.h
engine/engine.h.gch: engine/world/bih.h engine/model/model.h
game/game.h.gch: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
game/game.h.gch: shared/command.h shared/glexts.h shared/glemu.h
game/game.h.gch: shared/iengine.h shared/igame.h game/ai.h
standalone/shared/crypto.o: shared/cube.h shared/tools.h shared/geom.h
standalone/shared/crypto.o: shared/ents.h shared/command.h shared/iengine.h
standalone/shared/crypto.o: shared/igame.h
standalone/shared/stream.o: shared/cube.h shared/tools.h shared/geom.h
standalone/shared/stream.o: shared/ents.h shared/command.h shared/iengine.h
standalone/shared/stream.o: shared/igame.h
standalone/shared/tools.o: shared/cube.h shared/tools.h shared/geom.h
standalone/shared/tools.o: shared/ents.h shared/command.h shared/iengine.h
standalone/shared/tools.o: shared/igame.h
standalone/engine/interface/command.o: engine/engine.h shared/cube.h shared/tools.h
standalone/engine/interface/command.o: shared/geom.h shared/ents.h shared/command.h
standalone/engine/interface/command.o: shared/iengine.h shared/igame.h engine/world/world.h
standalone/engine/interface/server.o: engine/engine.h shared/cube.h shared/tools.h
standalone/engine/interface/server.o: shared/geom.h shared/ents.h shared/command.h
standalone/engine/interface/server.o: shared/iengine.h shared/igame.h engine/world/world.h
standalone/engine/world/worldio.o: engine/engine.h shared/cube.h shared/tools.h
standalone/engine/world/worldio.o: shared/geom.h shared/ents.h shared/command.h
standalone/engine/world/worldio.o: shared/iengine.h shared/igame.h engine/world/world.h
standalone/game/entities.o: game/game.h shared/cube.h shared/tools.h
standalone/game/entities.o: shared/geom.h shared/ents.h shared/command.h
standalone/game/entities.o: shared/iengine.h shared/igame.h game/ai.h
standalone/game/server.o: game/game.h shared/cube.h shared/tools.h
standalone/game/server.o: shared/geom.h shared/ents.h shared/command.h
standalone/game/server.o: shared/iengine.h shared/igame.h game/ai.h
standalone/game/server.o: game/ctf.h
standalone/shared/cube.h.gch: shared/tools.h shared/geom.h shared/ents.h
standalone/shared/cube.h.gch: shared/command.h shared/iengine.h
standalone/shared/cube.h.gch: shared/igame.h
standalone/engine/engine.h.gch: shared/cube.h shared/tools.h shared/geom.h
standalone/engine/engine.h.gch: shared/ents.h shared/command.h
standalone/engine/engine.h.gch: shared/iengine.h shared/igame.h
standalone/engine/engine.h.gch: engine/world/world.h
standalone/game/game.h.gch: shared/cube.h shared/tools.h shared/geom.h
standalone/game/game.h.gch: shared/ents.h shared/command.h shared/iengine.h
standalone/game/game.h.gch: shared/igame.h game/ai.h

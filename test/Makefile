# test suite Makefile
# assumes library is in ld library path
# creates binary libprimis_testsuite, run with no arguments

CXXFLAGS= -O0 -ffast-math -std=c++17 --coverage -Wall -fsigned-char

CLIENT_INCLUDES=-I../src/shared -I../src/engine `sdl2-config --cflags` -Igame -Ienet/include 
CLIENT_LIBS=`sdl2-config --cflags` -lprimis `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lz -lGL -lGLEW

#list of source code files to be compiled
CLIENT_OBJS= \
	main.o \
	testcs.o \
	testgltfloader.o \
	testidents.o \
	testocta.o \
	testprops.o \
	testshadowatlas.o \
	testutils.o \

#default: compiles the test executable and places it in the same directory as this file
default: client

clean:
	-$(RM) -r $(CLIENT_OBJS)

#compiles the objects
$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o libprimis_testsuite $(CLIENT_OBJS) $(CLIENT_LIBS)

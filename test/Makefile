# test suite Makefile
# assumes library is in ld library path
# creates binary libprimis_testsuite, run with no arguments

CXXFLAGS= -O0 -ffast-math -Wall -fsigned-char -ftest-coverage -fprofile-arcs -fno-rtti

CLIENT_INCLUDES=-I../src/shared -I../src/engine `sdl2-config --cflags` -Igame -Ienet/include
CLIENT_LIBS=`sdl2-config --cflags` -lprimis `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lz -lGL -lGLEW -lgcov

#BUILD_TYPE 2 for gdb
ifeq (2,$(BUILD_TYPE))
	CXXFLAGS += -ggdb3
endif

CPP_VERSION ?= 20

# if you want to compile with c++20, use `make -Csrc CPP_VERSION=20`, CPP_VERSION=23 for c++23
ifeq (20,$(CPP_VERSION))
	CXXFLAGS += -std=c++20
endif

ifeq (23,$(CPP_VERSION))
	CXXFLAGS += -std=c++23
endif

#list of source code files to be compiled
CLIENT_OBJS= \
	main.o \
	testanimmodel.o \
	testcs.o \
	testgeom.o \
	testgeomexts.o \
	testgltfloader.o \
	testidents.o \
	testmatrix.o \
	testmd5.o \
	testocta.o \
	testprops.o \
	testragdoll.o \
	testskel.o \
	testshadowatlas.o \
	testslot.o \
	testutils.o \
	testbih.o \

#default: compiles the test executable and places it in the same directory as this file
default: client

clean:
	-$(RM) -r $(CLIENT_OBJS)
	-$(RM) -r *.gcno *.gcda

#compiles the objects
$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o libprimis_testsuite $(CLIENT_OBJS) $(CLIENT_LIBS)

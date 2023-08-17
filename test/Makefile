CXXFLAGS= -O0 -ffast-math -std=c++17 --coverage -Wall -fsigned-char -fno-exceptions

INCLUDES=-I../src/shared -I../src/engine `sdl2-config --cflags`

all: libprimis-testsuite

libprimis-testsuite : main.o testutils.o testidents.o testcs.o testshadowatlas.o testprops.o
	g++ $(CXXFLAGS) $(INCLUDES) -o libprimis_testsuite main.o testutils.o testidents.o testcs.o testshadowatlas.o testprops.o -Igame -Ienet/include `sdl2-config --cflags` -lprimis `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lz -lGL -lGLEW


main.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c main.cpp

testutils.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c testutils.cpp

testidents.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c testidents.cpp

testcs.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c testcs.cpp

testshadowatlas.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c testshadowatlas.cpp

testprops.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c testprops.cpp

clean:
		rm -f main.o testutils.o testidents.o testcs.o testshadowatlas.o libprimis_testsuite

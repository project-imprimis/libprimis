CXXFLAGS= -O0 -fomit-frame-pointer -ffast-math -std=c++17 --coverage
override CXXFLAGS+= -Wall -fsigned-char -fno-exceptions

INCLUDES=-I../src/shared -I../src/engine `sdl2-config --cflags`

all: libprimis-testsuite

libprimis-testsuite : main.o testutils.o
		g++ $(CXXFLAGS) $(INCLUDES) -o libprimis_testsuite main.o testutils.o -Igame -Ienet/include -I/usr/X11R6/include `sdl2-config --cflags` -lprimis -L/usr/X11R6/lib -lX11 `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lz -lGL -lgcov


main.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c main.cpp

testutils.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c testutils.cpp

clean:
		rm -f main.o testutils.o libprimis_testsuite

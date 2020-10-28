#ifndef CUBE_H_
#define CUBE_H_

#ifdef WIN32
    #define _USE_MATH_DEFINES
#endif
#include <math.h>

#include <string>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdarg>
#include <climits>
#include <cfloat>
#include <cassert>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <utility>
#include <vector>
#include <queue>

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #ifdef _WIN32_WINNT
        #undef _WIN32_WINNT
    #endif
    #define _WIN32_WINNT 0x0500
    #include "windows.h"
    #ifndef _WINDOWS
        #define _WINDOWS
    #endif
    #ifndef __GNUC__
        #include <eh.h>
        #include <dbghelp.h>
        #include <intrin.h>
    #endif
    #define ZLIB_DLL
#endif

#include <SDL.h>
#include <SDL_opengl.h>

#include <zlib.h>

#include "tools.h"
#include "geom.h"
#include "ents.h"
#include "command.h"
#include "world/world.h"
#include "world/octa.h"

#include "glexts.h"
#include "glemu.h"

#include "consts.h"
#endif


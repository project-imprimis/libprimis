#include "engine.h"

#include "console.h"
#include "control.h"
#include "menus.h"

#include "render/renderwindow.h"

bool inbetweenframes = false,
     renderedframe = true;

// logging

static const int logstrlen = 512;
FILE *logfile = nullptr;

FILE *getlogfile()
{
#ifdef WIN32
    return logfile;
#else
    return logfile ? logfile : stdout;
#endif
}

static void writelog(FILE *file, const char *buf)
{
    static uchar ubuf[512];
    size_t len = strlen(buf),
           carry = 0;
    while(carry < len)
    {
        size_t numu = encodeutf8(ubuf, sizeof(ubuf)-1, &(reinterpret_cast<const uchar*>(buf))[carry], len - carry, &carry);
        if(carry >= len)
        {
            ubuf[numu++] = '\n';
        }
        fwrite(ubuf, 1, numu, file);
    }
}

static void writelogv(FILE *file, const char *fmt, va_list args)
{
    static char buf[logstrlen];
    vformatstring(buf, fmt, args, sizeof(buf));
    writelog(file, buf);
}

void logoutfv(const char *fmt, va_list args, FILE *f)
{
    if(f)
    {
        writelogv(f, fmt, args);
    }
}

void logoutf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logoutfv(fmt, args);
    va_end(args);
}

//error handling

void fatal(const char *s, ...)    // failure exit
{
    static int errors = 0;
    errors++;

    if(errors <= 2) // print up to one extra recursive error
    {
        DEFV_FORMAT_STRING(msg,s,s);
        logoutf("%s", msg);

        if(errors <= 1) // avoid recursion
        {
            if(SDL_WasInit(SDL_INIT_VIDEO))
            {
                SDL_ShowCursor(SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                if(screen)
                {
                    SDL_SetWindowGrab(screen, SDL_FALSE);
                }
                cleargamma();
            }
            SDL_Quit();
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Libprimis engine fatal error", msg, nullptr);
        }
    }

    exit(EXIT_FAILURE);
}

int curtime = 0,
    lastmillis = 1,
    elapsedtime = 0,
    totalmillis = 1;

dynent *player = nullptr;

int initing = Init_Not;

bool initwarning(const char *desc, int level, int type)
{
    if(initing < level)
    {
        addchange(desc, type);
        return true;
    }
    return false;
}

static int clockrealbase = 0,
           clockvirtbase = 0;
static void clockreset()
{
    clockrealbase = SDL_GetTicks();
    clockvirtbase = totalmillis;
}
VARFP(clockerror, 990000, 1000000, 1010000, clockreset());
VARFP(clockfix, 0, 0, 1, clockreset());

int getclockmillis()
{
    int millis = SDL_GetTicks() - clockrealbase;
    if(clockfix)
    {
        millis = static_cast<int>(millis*(static_cast<double>(clockerror)/1000000));
    }
    millis += clockvirtbase;
    return std::max(millis, totalmillis);
}

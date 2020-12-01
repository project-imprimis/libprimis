#include "engine.h"

#include "menus.h"

bool inbetweenframes = false,
     renderedframe = true;

extern void cleargamma();

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
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Imprimis fatal error", msg, NULL);
        }
    }

    exit(EXIT_FAILURE);
}

int curtime = 0,
    lastmillis = 1,
    elapsedtime = 0,
    totalmillis = 1;

dynent *player = NULL;

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
    return max(millis, totalmillis);
}

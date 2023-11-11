/* renderwindow: screen rendering functionality
 *
 * screen rendering functions, such as background, progress bar
 * also handles stuff such as main menu rendering and other non-intensive rendering
 * as well as global rendering settings such as gamma
 */
#include "SDL_ttf.h"

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "hud.h"
#include "octarender.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "renderparticles.h"
#include "rendertext.h"
#include "renderttf.h"
#include "renderva.h"
#include "renderwindow.h"
#include "shader.h"
#include "shaderparam.h"
#include "stain.h"
#include "texture.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/input.h"
#include "interface/menus.h"

#include "world/octaedit.h"

VARFN(screenw, scr_w, SCR_MINW, -1, SCR_MAXW, initwarning("screen resolution"));
VARFN(screenh, scr_h, SCR_MINH, -1, SCR_MAXH, initwarning("screen resolution"));

VAR(menufps, 0, 60, 1000);   //maximum framerate while in main menu
VARP(maxfps, 0, 240, 1000);  //maximum framerate while world is being rendered

VAR(desktopw, 1, 0, 0);
VAR(desktoph, 1, 0, 0);
int screenw = 0,
    screenh = 0;
SDL_Window   *screen    = nullptr;
static SDL_GLContext glcontext = nullptr;

//helper function for main menu rendering routines
//returns w and h if both are above 1024x768
//returns w and h multiplied by the factor by which the smallest dimension is smaller than 1024x768
static void getbackgroundres(int &w, int &h)
{
    float wk = 1,
          hk = 1;
    if(w < 1024)
    {
        wk = 1024.0f/w; //calculate w subsize factor (if greater than 1)
    }
    if(h < 768)
    {
        hk = 768.0f/h; //calculate h subsize factor (if greater than 1)
    }
    wk = hk = std::max(wk, hk); //pick the largest factor and multiply both by this
    w = static_cast<int>(std::ceil(w*wk));
    h = static_cast<int>(std::ceil(h*hk));
}

static std::string backgroundcaption   = "",
                   backgroundmapname   = "",
                   backgroundmapinfo   = "";
static Texture *backgroundmapshot = nullptr;

static void bgquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1)
{
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(x,   y);   gle::attribf(tx,      ty);
    gle::attribf(x+w, y);   gle::attribf(tx + tw, ty);
    gle::attribf(x,   y+h); gle::attribf(tx,      ty + th);
    gle::attribf(x+w, y+h); gle::attribf(tx + tw, ty + th);
    gle::end();
}

// void renderbackgroundview(int, int, const char*, Texture*, const char*, const char*)
//   Background picture / text handler

/*
Notes:
    * Unsure what 'w' and 'h' refers to, maybe screen resolution?
*/

static void renderbackgroundview(int win_w, int win_h, const char *caption, Texture *mapshot, const char *mapname, const char *mapinfo)
{
    static int lastupdate  = -1,
               lastw       = -1,
               lasth       = -1;
    static float backgroundu = 0,
                 backgroundv = 0;
    const bool needsRefresh =
        (renderedframe && !mainmenu && lastupdate != lastmillis)
        || lastw != win_w
        || lasth != win_h;
    if(needsRefresh)
    {
        lastupdate = lastmillis;
        lastw = win_w;
        lasth = win_h;

        backgroundu = randomfloat(1);
        backgroundv = randomfloat(1);
    }
    else if(lastupdate != lastmillis)
    {
        lastupdate = lastmillis;
    }
    hudmatrix.ortho(0, win_w, win_h, 0, -1, 1);
    resethudmatrix();
    resethudshader();

    gle::defvertex(2);
    gle::deftexcoord0();
    settexture("media/interface/background.png", 0); //main menu background
    float bu = win_w*0.67f/256.0f,
          bv = win_h*0.67f/256.0f;
    bgquad(0, 0, win_w, win_h, backgroundu, backgroundv, bu, bv);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    settexture("media/interface/shadow.png", 3); //peripheral shadow effect
    bgquad(0, 0, win_w, win_h);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // Set position and size of logo
    float logo_h = (1.f/3.f)*std::min(win_w, win_h),
          logo_w = logo_h*(2.f/1.f), // Aspect ratio of logo, defined here
          logo_x = 0.5f*(win_w - logo_w),
          logo_y = 0.5f*(win_h*0.5f - logo_h);

    settexture( (maxtexsize >= 1024 || maxtexsize == 0) && (hudw() > 1280 || hudh() > 800)
              ? "<premul>media/interface/logo_1024.png" //1024x wide logo
              : "<premul>media/interface/logo.png", //512x wide logo for small screens
        3);
    bgquad(logo_x, logo_y, logo_w, logo_h);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if(caption)
    {
        int tw = text_width(caption);
        float tsz = 0.04f*std::min(win_w, win_h)/FONTH,
              tx  = 0.5f*(win_w - tw*tsz),
              ty  = win_h - 0.075f*1.5f*std::min(win_w, win_h) - FONTH*tsz;
        pushhudtranslate(tx, ty, tsz);
        //draw_text(caption, 0, 0);
        ttr.renderttf(caption, {0xFF, 0xFF, 0xFF, 0}, 0, 0);
        pophudmatrix();
    }
    if(mapshot || mapname)
    {
        float infowidth = 14*FONTH,
              sz  = 0.35f*std::min(win_w, win_h),
              msz = (0.85f*std::min(win_w, win_h) - sz)/(infowidth + FONTH),
              x   = 0.5f*win_w,
              y   = logo_y+logo_h - sz/15,
              mx  = 0,
              my  = 0,
              mw  = 0,
              mh  = 0;
        // Prepare text area for map info
        if(mapinfo)
        {
            text_boundsf(mapinfo, mw, mh, infowidth);
            x -= 0.5f * mw * msz;
            if (mapshot && mapshot!=notexture)
            {
                x -= 0.5f*FONTH * msz;
                mx = sz + FONTH * msz;
            }
        }
        // Map shot was provided and isn't empty
        if(mapshot && mapshot!=notexture)
        {
            x -= 0.5f * sz;
            resethudshader();
            glBindTexture(GL_TEXTURE_2D, mapshot->id);
            bgquad(x, y, sz, sz);
        }
        // Map name was provided
        if(mapname)
        {
            float tw  = text_widthf(mapname),
                  tsz = sz/(8*FONTH),
                  tx  = std::max(0.5f * (mw*msz - tw * tsz), 0.0f);
            pushhudtranslate(x + mx + tx, y, tsz);
            //draw_text(mapname, 0, 0);
            ttr.fontsize(42);
            ttr.renderttf(mapname, {0xFF, 0xFF, 0xFF, 0}, 0, 0);
            pophudmatrix();
            my = 1.5f*FONTH*tsz;
        }
        // Map info was provided
        if(mapinfo)
        {
            pushhudtranslate(x + mx, y + my, msz);
            //draw_text(mapinfo, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, -1, infowidth);
            ttr.fontsize(42);
            ttr.renderttf(mapinfo, {0xFF, 0xFF, 0xFF, 0}, 0, 0);
            pophudmatrix();
        }
    }
    glDisable(GL_BLEND);
}

void swapbuffers(bool)
{
    gle::disable();
    SDL_GL_SwapWindow(screen);
}

static void setbackgroundinfo(const char *caption = nullptr, Texture *mapshot = nullptr, const char *mapname = nullptr, const char *mapinfo = nullptr)
{
    renderedframe = false;
    backgroundcaption = std::string(caption ? caption : "");
    backgroundmapshot = mapshot;
    backgroundmapname = std::string(mapname ? mapname : "");
    std::string minfo = std::string(mapinfo ? mapinfo : "");
    if(minfo != backgroundmapinfo)
    {
        backgroundmapinfo = "";
        if(!minfo.empty())
        {
            backgroundmapinfo = std::string(mapinfo);
        }
        else
        {
            backgroundmapinfo = "";
        }
    }
}

void renderbackground(const char *caption, Texture *mapshot, const char *mapname, const char *mapinfo, bool force)
{
    if(!inbetweenframes && !force)
    {
        return;
    }
    int w = hudw(),
        h = hudh();
    if(forceaspect)
    {
        w = std::ceil(h*forceaspect);
    }
    getbackgroundres(w, h);
    gettextres(w, h);
    if(force)
    {
        renderbackgroundview(w, h, caption, mapshot, mapname, mapinfo);
        return;
    }
    //renders renderbackgroundview three times, with an identical call each time
    for(int i = 0; i < 3; ++i)
    {
        renderbackgroundview(w, h, caption, mapshot, mapname, mapinfo);
        swapbuffers(false);
    }
    setbackgroundinfo(caption, mapshot, mapname, mapinfo);
}

static void restorebackground(int w, int h, bool force = false)
{
    if(renderedframe)
    {
        if(!force)
        {
            return;
        }
        setbackgroundinfo();
    }
    renderbackgroundview(w, h, backgroundcaption.c_str(), backgroundmapshot, backgroundmapname.c_str(), backgroundmapinfo.c_str());
}

float loadprogress = 0;

static void renderprogressview(int w, int h, float bar, const char *text)   // also used during loading
{
    hudmatrix.ortho(0, w, h, 0, -1, 1);
    resethudmatrix();
    resethudshader();

    gle::defvertex(2);
    gle::deftexcoord0();

    float fh = 0.060f*std::min(w, h),
          fw = fh * 15,
          fx = renderedframe ? w - fw - fh/4 : 0.5f * (w - fw),
          fy = renderedframe ? fh/4 : h - fh * 1.5f;
    settexture("media/interface/loading_frame.png", 3);
    bgquad(fx, fy, fw, fh);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float bw  = fw * (512 - 2*8)/512.0f,
          bh  = fh * 20/32.0f,
          bx  = fx + fw * 8/512.0f,
          by  = fy + fh * 6/32.0f,
          su1 = 0/32.0f,
          su2 = 8/32.0f,
          sw  = fw * 8/512.0f,
          eu1 = 24/32.0f,
          eu2 = 32/32.0f,
          ew  = fw * 8/512.0f,
          mw  = bw - sw - ew,
          ex  = bx+sw + std::max(mw*bar, fw * 8/512.0f);
    if(bar > 0)
    {
        settexture("media/interface/loading_bar.png", 3);
        bgquad(bx, by, sw, bh, su1, 0, su2-su1, 1);
        bgquad(bx+sw, by, ex-(bx+sw), bh, su2, 0, eu1-su2, 1);
        bgquad(ex, by, ew, bh, eu1, 0, eu2-eu1, 1);
    }
    if(text)
    {
        int tw = text_width(text);
        float tsz = bh * 0.6f/FONTH;
        if(tw * tsz > mw)
        {
            tsz = mw/tw;
        }
        pushhudtranslate(bx+sw, by + (bh - FONTH*tsz)/2, tsz);
        //draw_text(text, 0, 0);
        ttr.fontsize(50);
        ttr.renderttf(text, {0xFF, 0xFF, 0xFF, 0}, 0, 4);
        pophudmatrix();
    }
    glDisable(GL_BLEND);
}

VAR(progressbackground, 0, 0, 1); //force rendering progress bar background texture
static int curvsync = -1;

void renderprogress(float bar, const char *text, bool background)   // also used during loading
{
    if(!inbetweenframes || drawtex)
    {
        return;
    }
    int fps = menufps ? (maxfps ? std::min(maxfps, menufps) : menufps) : maxfps;
    if(fps)
    {
        static int lastprogress = 0;
        int ticks = SDL_GetTicks(),
            diff = ticks - lastprogress;
        if(bar > 0 && diff >= 0 && diff < (1000 + fps-1)/fps)
        {
            return;
        }
        lastprogress = ticks;
    }
    int w = hudw(),
        h = hudh();
    if(forceaspect)
    {
        w = static_cast<int>(std::ceil(h*forceaspect));
    }
    getbackgroundres(w, h);
    gettextres(w, h);

    bool forcebackground = progressbackground || (mesa_swap_bug && (curvsync || totalmillis==1));
    if(background || forcebackground)
    {
        restorebackground(w, h, forcebackground);
    }
    renderprogressview(w, h, bar, text);
    swapbuffers(false);
}

static bool initwindowpos = false;

void setfullscreen(bool enable)
{
    if(!screen)
    {
        return;
    }
    //initwarning(enable ? "fullscreen" : "windowed");
    SDL_SetWindowFullscreen(screen, enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if(!enable)
    {
        SDL_SetWindowSize(screen, scr_w, scr_h);
        if(initwindowpos)
        {
            int winx = SDL_WINDOWPOS_CENTERED,
                winy = SDL_WINDOWPOS_CENTERED;
            SDL_SetWindowPosition(screen, winx, winy);
            initwindowpos = false;
        }
    }
}

VARF(fullscreen, 0, 1, 1, setfullscreen(fullscreen!=0));

/* screenres: sets the window size to w * h pixels, or reduces fullscreen
 * resolution to w * h pixels
 *
 * arguments:
 *    w: width of new screen res
 *    h: height of new screen res
 */
void screenres(int w, int h)
{
    //need to cast enum to int for std's clamp implementation
    scr_w = std::clamp(w, static_cast<int>(SCR_MINW), static_cast<int>(SCR_MAXW));
    scr_h = std::clamp(h, static_cast<int>(SCR_MINH), static_cast<int>(SCR_MAXH));
    if(screen)
    {
        scr_w = std::min(scr_w, desktopw);
        scr_h = std::min(scr_h, desktoph);
        if(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN)
        {
            gl_resize();
        }
        else
        {
            SDL_SetWindowSize(screen, scr_w, scr_h);
        }
    }
    else
    {
        initwarning("screen resolution");
    }
}


static void setgamma(int val)
{
    if(screen && SDL_SetWindowBrightness(screen, val/100.0f) < 0)
    {
        conoutf(Console_Error, "Could not set gamma: %s", SDL_GetError());
    }
}

static int curgamma = 100;
static VARFNP(gamma, reqgamma, 30, 100, 300,
{
    if(initing || reqgamma == curgamma)
    {
        return;
    }
    curgamma = reqgamma;
    setgamma(curgamma);
});

/* restoregamma: sets gamma to the previous set value, useful for reverting bad-
 * looking gamma trial settings
 *
 * used in iengine.h
 */
void restoregamma()
{
    if(initing || reqgamma == 100)
    {
        return;
    }
    curgamma = reqgamma;
    setgamma(curgamma);
}

void cleargamma()
{
    if(curgamma != 100 && screen)
    {
        SDL_SetWindowBrightness(screen, 1.0f);
    }
}

void restorevsync(); //prototype to fix chicken-egg initialization problem caused by VARFP

VARFP(vsync, 0, 0, 1, restorevsync());                      //vertical sync of framebuffer to refresh rate
VARFP(vsynctear, 0, 0, 1, { if(vsync) restorevsync(); });   //toggles sdl2's adaptive sync function

void restorevsync()
{
    if(initing || !glcontext)
    {
        return;
    }
    if(!SDL_GL_SetSwapInterval(vsync ? (vsynctear ? -1 : 1) : 0))
    {
        curvsync = vsync;
    }
}

//used in iengine.h
void setupscreen()
{
    //clear prior gl context/screen if present
    if(glcontext)
    {
        SDL_GL_DeleteContext(glcontext);
        glcontext = nullptr;
    }
    if(screen)
    {
        SDL_DestroyWindow(screen);
        screen = nullptr;
    }
    curvsync = -1;

    SDL_Rect desktop;
    if(SDL_GetDisplayBounds(0, &desktop) < 0)
    {
        fatal("failed querying desktop bounds: %s", SDL_GetError());
    }
    desktopw = desktop.w;
    desktoph = desktop.h;

    if(scr_h < 0)
    {
        scr_h = SCR_DEFAULTH;
    }
    if(scr_w < 0)
    {
        scr_w = (scr_h*desktopw)/desktoph;
    }
    scr_w = std::min(scr_w, desktopw);
    scr_h = std::min(scr_h, desktoph);

    int winx  = SDL_WINDOWPOS_UNDEFINED,
        winy  = SDL_WINDOWPOS_UNDEFINED,
        winw  = scr_w,
        winh  = scr_h,
        flags = SDL_WINDOW_RESIZABLE;
    if(fullscreen)
    {
        winw   = desktopw;
        winh   = desktoph;
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        initwindowpos = true;
    }

    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    uint32_t windowflags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | flags;
    //create new screen       title          x     y     w     h  flags
    screen = SDL_CreateWindow("Imprimis", winx, winy, winw, winh, windowflags);
    ttr.initttf();
    ttr.openfont("media/interface/font/default.ttf", 24);

    if(!screen)
    {
        fatal("failed to create OpenGL window: %s", SDL_GetError());
    }
    SDL_Surface *icon = loadsurface("media/interface/icon.png"); //path to taskbar icon
    if(icon)
    {
        SDL_SetWindowIcon(screen, icon);
        SDL_FreeSurface(icon); //don't need it any more
    }

    SDL_SetWindowMinimumSize(screen, SCR_MINW, SCR_MINH);
    SDL_SetWindowMaximumSize(screen, SCR_MAXW, SCR_MAXH);
    //set opengl version to 4.0
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    //set core profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    glcontext = SDL_GL_CreateContext(screen);

    //
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
      /* Problem: glewInit failed, something is seriously wrong. */
      logoutf("Error: %s", glewGetErrorString(err));
    }
    logoutf("init: GLEW %s", glewGetString(GLEW_VERSION));
    //check if OpenGL context is sane
    if(!glcontext)
    {
        fatal("failed to create OpenGL context: %s", SDL_GetError());
    }
    SDL_GetWindowSize(screen, &screenw, &screenh);
}

//full reset of renderer
void resetgl()
{
    if(!glslversion)
    {
        conoutf(Console_Error, "Cannot reset GL without GL initialized, operation not performed");
        return;
    }
    clearchanges(Change_Graphics|Change_Shaders);

    renderbackground("resetting OpenGL");

    rootworld.cleanupva();
    cleanupparticles();
    cleanupstains();
    cleanupmodels();
    cleanupprefabs();
    cleanuptextures();
    cleanuplights();
    cleanupshaders();
    cleanupgl();

    setupscreen();

    inputgrab(grabinput);

    gl_init();

    inbetweenframes = false;
    //texture reloading
    if(!notexture->reload() ||
       !reloadtexture("<premul>media/interface/logo.png") ||
       !reloadtexture("<premul>media/interface/logo_1024.png") ||
       !reloadtexture("media/interface/background.png") ||
       !reloadtexture("media/interface/shadow.png") ||
       !reloadtexture("media/interface/mapshot_frame.png") ||
       !reloadtexture("media/interface/loading_frame.png") ||
       !reloadtexture("media/interface/loading_bar.png"))
    {
        fatal("failed to reload core texture");
    }
    reloadfonts();
    inbetweenframes = true;
    renderbackground("initializing...");
    restoregamma();
    restorevsync();
    initgbuffer();
    reloadshaders();
    reloadtextures();
    rootworld.allchanged(true);
}

/* limitfps: uses SDL_Delay to delay a frame, given the time the last frame was
 * rendered and the current time
 *
 * Arguments:
 *    millis: the time (in ms) since program started
 *    curmillis: the last registered frame time
 */
void limitfps(int &millis, int curmillis)
{
    int limit = (mainmenu || minimized) && menufps ? (maxfps ? std::min(maxfps, menufps) : menufps) : maxfps;
    if(!limit)
    {
        return;
    }
    static int fpserror = 0;
    int delay = 1000/limit - (millis-curmillis);
    if(delay < 0)
    {
        fpserror = 0;
    }
    else
    {
        fpserror += 1000%limit;
        if(fpserror >= limit)
        {
            ++delay;
            fpserror -= limit;
        }
        if(delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        }
    }
}

#ifdef WIN32
    // Force Optimus setups to use the NVIDIA GPU
    // or also for AMD dual graphics
    extern "C"
    {
        #ifdef __GNUC__
            __attribute__((dllexport))
        #else
            __declspec(dllexport)
        #endif
            DWORD NvOptimusEnablement = 1;

        #ifdef __GNUC__
            __attribute__((dllexport))
        #else
            __declspec(dllexport)
        #endif
        DWORD AmdPowerXpressRequestHighPerformance = 1;
    }
#endif

static constexpr int maxfpshistory = 60;

int fpspos = 0;
std::array<int, maxfpshistory> fpshistory;

void resetfpshistory()
{
    fpshistory.fill(1);
    fpspos = 0;
}

void updatefpshistory(int millis)
{
    fpshistory[fpspos++] = std::max(1, std::min(1000, millis));
    if(fpspos>=maxfpshistory)
    {
        fpspos = 0;
    }
}

void getfps(int &fps, int &bestdiff, int &worstdiff)
{
    int total = fpshistory.at(maxfpshistory-1),
        best = total,
        worst = total;
    for(const int &millis : fpshistory)
    {
        total += millis;
        if(millis < best)
        {
            best = millis;
        }
        if(millis > worst)
        {
            worst = millis;
        }
    }
    if(total) //guard against div by 0
    {
        fps = (1000*maxfpshistory)/total;
        bestdiff = 1000/best-fps;
        worstdiff = fps-1000/worst;
    }
    else
    {
        fps = 0;
        bestdiff = 0;
        worstdiff = 0;
    }
}

void getfpscmd(const int *raw)
{
    if(*raw)
    {
        floatret(1000.0f/fpshistory[(fpspos+maxfpshistory-1)%maxfpshistory]);
    }
    else
    {
        int fps, bestdiff, worstdiff;
        getfps(fps, bestdiff, worstdiff);
        intret(fps);
    }
}

void initrenderwindowcmds()
{
    addcommand("getfps", reinterpret_cast<identfun>(getfpscmd), "i", Id_Command);
    addcommand("resetgl", reinterpret_cast<identfun>(resetgl), "", Id_Command);
    addcommand("screenres", reinterpret_cast<identfun>(screenres), "ii", Id_Command);
}

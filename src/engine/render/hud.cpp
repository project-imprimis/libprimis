/* hud.cpp: hud & main menu rendering code
 *
 * includes hud compass, hud readouts, crosshair handling
 * main hud rendering
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "hud.h"
#include "rendergl.h"
#include "renderlights.h"
#include "renderparticles.h"
#include "rendertext.h"
#include "renderttf.h"
#include "rendertimers.h"
#include "renderwindow.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/input.h"
#include "interface/menus.h"
#include "interface/ui.h"

#include "world/octaedit.h"

//internal functionality not seen by other files
namespace
{
    //damagecompass* vars control display of directional hints as to damage location
    VARNP(damagecompass, usedamagecompass, 0, 1, 1);
    VARP(damagecompassfade, 1, 1000, 10000); //sets milliseconds before damage hints fade
    VARP(damagecompasssize, 1, 30, 100);
    VARP(damagecompassalpha, 1, 25, 100);
    VARP(damagecompassmin, 1, 25, 1000);
    VARP(damagecompassmax, 1, 200, 1000);

    std::array<float, 8> damagedirs = { 0, 0, 0, 0, 0, 0, 0, 0 };

    void drawdamagecompass(int w, int h)
    {
        hudnotextureshader->set();

        int dirs = 0;
        float size = damagecompasssize/100.0f*std::min(h, w)/2.0f;
        for(float &dir : damagedirs)
        {
            if(dir > 0)
            {
                if(!dirs)
                {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    gle::colorf(1, 0, 0, damagecompassalpha/100.0f);
                    gle::defvertex();
                    gle::begin(GL_TRIANGLES);
                }
                dirs++;

                float logscale = 32,
                      scale = log(1 + (logscale - 1)*dir) / std::log(logscale),
                      offset = -size/2.0f-std::min(h, w)/4.0f;
                matrix4x3 m;
                m.identity();
                m.settranslation(w/2, h/2, 0);
                m.rotate_around_z(dir*45/RAD);
                m.translate(0, offset, 0);
                m.scale(size*scale);

                gle::attrib(m.transform(vec2(1, 1)));
                gle::attrib(m.transform(vec2(-1, 1)));
                gle::attrib(m.transform(vec2(0, 0)));

                // fade in log space so short blips don't disappear too quickly
                scale -= static_cast<float>(curtime)/damagecompassfade;
                dir = scale > 0 ? (std::pow(logscale, scale) - 1) / (logscale - 1) : 0;
            }
        }
        if(dirs)
        {
            gle::end();
        }
    }

    int damageblendmillis = 0;

    //damagescreen variables control the display of a texture upon player being damaged
    VARFP(damagescreen, 0, 1, 1, { if(!damagescreen) damageblendmillis = 0; });
    VARP(damagescreenfactor, 1, 75, 100);
    VARP(damagescreenalpha, 1, 45, 100);
    VARP(damagescreenfade, 0, 1000, 1000); //number of ms before screen damage fades
    VARP(damagescreenmin, 1, 10, 1000);
    VARP(damagescreenmax, 1, 100, 1000);

    void drawdamagescreen(int w, int h)
    {
        static Texture *damagetex = nullptr;
        //preload this texture even if not going to draw, to prevent stutter when first hit
        if(!damagetex)
        {
            damagetex = textureload("media/interface/hud/damage.png", 3);
        }
        if(lastmillis >= damageblendmillis)
        {
            return;
        }
        hudshader->set();
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, damagetex->id);
        float fade = damagescreenalpha/100.0f;
        if(damageblendmillis - lastmillis < damagescreenfade)
        {
            fade *= static_cast<float>(damageblendmillis - lastmillis)/damagescreenfade;
        }
        gle::colorf(fade, fade, fade, fade);

        hudquad(0, 0, w, h);
    }

    VAR(showstats, 0, 1, 1);

    //crosshair & cursor vars
    VARP(crosshairsize, 0, 15, 50);
    VARP(cursorsize, 0, 15, 30);
    VARP(crosshairfx, 0, 1, 1); // hit fx


    const char *defaultcrosshair(int index)
    {
        switch(index)
        {
            case 2:
            {
                return "media/interface/crosshair/default_hit.png";
            }
            case 1:
            {
                return "media/interface/crosshair/teammate.png";
            }
            default:
            {
                return "media/interface/crosshair/default.png";
            }
        }
    }

    const int maxcrosshairs = 4;
    std::array<Texture *, maxcrosshairs> crosshairs = { nullptr, nullptr, nullptr, nullptr };

    void loadcrosshair(const char *name, int i)
    {
        if(i < 0 || i >= maxcrosshairs)
        {
            return;
        }
        crosshairs[i] = name ? textureload(name, 3, true) : notexture;
        if(crosshairs[i] == notexture)
        {
            name = defaultcrosshair(i);
            if(!name)
            {
                name = "media/interface/crosshair/default.png";
            }
            crosshairs[i] = textureload(name, 3, true);
        }
    }

    void getcrosshair(int *i)
    {
        const char *name = "";
        if(*i >= 0 && *i < maxcrosshairs)
        {
            name = crosshairs[*i] ? crosshairs[*i]->name : defaultcrosshair(*i);
            if(!name)
            {
                name = "media/interface/crosshair/default.png";
            }
        }
        result(name);
    }

    void drawcrosshair(int w, int h, int crosshairindex)
    {
        bool windowhit = UI::hascursor();
        if(!windowhit && (!showhud || mainmenu))
        {
            return; //(!showhud || player->state==CS_SPECTATOR || player->state==CS_DEAD)) return;
        }
        float cx = 0.5f,
              cy = 0.5f,
              chsize;
        Texture *crosshair;
        if(windowhit)
        {
            static Texture *cursor = nullptr;
            if(!cursor)
            {
                cursor = textureload("media/interface/cursor.png", 3, true);
            }
            crosshair = cursor;
            chsize = cursorsize*w/900.0f;
            UI::getcursorpos(cx, cy);
        }
        else
        {
            int index = crosshairindex;
            if(index < 0)
            {
                return;
            }
            if(!crosshairfx)
            {
                index = 0;
            }
            crosshair = crosshairs[index];
            if(!crosshair)
            {
                loadcrosshair(nullptr, index);
                crosshair = crosshairs[index];
            }
            chsize = crosshairsize*w/900.0f;
        }
        vec color = vec(1, 1, 1);
        if(crosshair->type&Texture::ALPHA)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else
        {
            glBlendFunc(GL_ONE, GL_ONE);
        }
        hudshader->set();
        gle::color(color);
        float x = cx*w - (windowhit ? 0 : chsize/2.0f),
              y = cy*h - (windowhit ? 0 : chsize/2.0f);
        glBindTexture(GL_TEXTURE_2D, crosshair->id);

        hudquad(x, y, chsize, chsize);
    }

    //hud time displays
    VARP(wallclock, 0, 0, 1);     //toggles hud readout
    VARP(wallclock24, 0, 0, 1);   //toggles 12h (US) or 24h time
    VARP(wallclocksecs, 0, 0, 1); //seconds precision on hud readout

    time_t walltime = 0;

    //hud fps displays
    VARP(showfps, 0, 1, 1);      //toggles showing game framerate
    VARP(showfpsrange, 0, 0, 1); //toggles showing min/max framerates as well
}

//externally relevant functionality
//from here to iengine section, functions stay local to the libprimis codebase

void gl_drawmainmenu()
{
    renderbackground(nullptr, nullptr, nullptr, nullptr, true);
}

void gl_drawhud(int crosshairindex, void(* hud2d)())
{
    /* we want to get the length of the frame at the end of the frame,
     * not the middle, so we have a persistent variable inside the
     * function scope
     */
    static int framemillis = 0;

    int w = hudw(),
        h = hudh();
    if(forceaspect)
    {
        w = static_cast<int>(ceil(h*forceaspect));
    }

    gettextres(w, h);

    hudmatrix.ortho(0, w, h, 0, -1, 1);
    resethudmatrix();
    resethudshader();

    pushfont();
    setfont("default_outline");

    debuglights();

    glEnable(GL_BLEND);

    if(!mainmenu)
    {
        drawdamagescreen(w, h);
        drawdamagecompass(w, h);
    }

    float conw = w/conscale,
          conh = h/conscale,
          abovehud = conh - FONTH;
    if(showhud && !mainmenu)
    {
        if(showstats)
        {
            pushhudscale(conscale);
            ttr.fontsize(42);
            int roffset = 0;
            if(showfps)
            {
                static int lastfps = 0;
                static std::array<int, 3> prevfps = { 0, 0, 0 },
                                          curfps = { 0, 0, 0 };
                if(totalmillis - lastfps >= statrate)
                {
                    prevfps = curfps;
                    lastfps = totalmillis - (totalmillis%statrate);
                }
                std::array<int, 3> nextfps;
                getfps(nextfps[0], nextfps[1], nextfps[2]);
                for(size_t i = 0; i < curfps.size(); ++i)
                {
                    if(prevfps[i]==curfps[i])
                    {
                        curfps[i] = nextfps[i];
                    }
                }
                if(showfpsrange)
                {
                    char fpsstring[20];
                    std::sprintf(fpsstring, "fps %d+%d-%d", curfps[0], curfps[1], curfps[2]);
                    ttr.renderttf(fpsstring, {0xFF, 0xFF, 0xFF, 0},  conw-(1000*conscale), conh-(360*conscale));
                    //draw_textf("fps %d+%d-%d", conw-7*FONTH, conh-FONTH*3/2, curfps[0], curfps[1], curfps[2]);
                }
                else
                {
                    char fpsstring[20];
                    std::sprintf(fpsstring, "fps %d", curfps[0]);
                    ttr.renderttf(fpsstring, {0xFF, 0xFF, 0xFF, 0},  conw-(1000*conscale), conh-(360*conscale));
                }
                roffset += FONTH;
            }
            printtimers(conw, conh, framemillis);
            if(wallclock)
            {
                if(!walltime)
                {
                    walltime = std::time(nullptr);
                    walltime -= totalmillis/1000;
                    if(!walltime)
                    {
                        walltime++;
                    }
                }
                time_t walloffset = walltime + totalmillis/1000;
                std::tm* localvals = std::localtime(&walloffset);
                static string buf;
                if(localvals && std::strftime(buf, sizeof(buf), wallclocksecs ? (wallclock24 ? "%H:%M:%S" : "%I:%M:%S%p") : (wallclock24 ? "%H:%M" : "%I:%M%p"), localvals))
                {
                    // hack because not all platforms (windows) support %P lowercase option
                    // also strip leading 0 from 12 hour time
                    char *dst = buf;
                    const char *src = &buf[!wallclock24 && buf[0]=='0' ? 1 : 0];
                    while(*src)
                    {
                        *dst++ = tolower(*src++);
                    }
                    *dst++ = '\0';

                    ttr.renderttf(buf, { 0xFF, 0xFF, 0xFF, 0 }, conw-(1000*conscale), conh-(540*conscale));
                    //draw_text(buf, conw-5*FONTH, conh-FONTH*3/2-roffset);
                    roffset += FONTH;
                }
            }
            pophudmatrix();
        }
        if(!editmode)
        {
            resethudshader();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            hud2d();
            abovehud = std::min(abovehud, conh);
        }
        rendertexturepanel(w, h);
    }

    abovehud = std::min(abovehud, conh*UI::abovehud());

    pushhudscale(conscale);
    abovehud -= rendercommand(FONTH/2, abovehud - FONTH/2, conw-FONTH);
    if(showhud && !UI::uivisible("fullconsole"))
    {
        renderconsole(conw, conh, abovehud - FONTH/2);
    }
    pophudmatrix();
    drawcrosshair(w, h, crosshairindex);
    glDisable(GL_BLEND);
    popfont();
    if(frametimer)
    {
        glFinish();
        framemillis = getclockmillis() - totalmillis;
    }
}

void writecrosshairs(std::fstream& f)
{
    for(int i = 0; i < maxcrosshairs; ++i)
    {
        if(crosshairs[i] && crosshairs[i]!=notexture)
        {
            f << "loadcrosshair " << escapestring(crosshairs[i]->name) << " " << i << std::endl;
        }
    }
    f << std::endl;
}

void resethudshader()
{
    hudshader->set();
    gle::colorf(1, 1, 1);
}

FVARP(conscale, 1e-3f, 0.33f, 1e3f); //size of readouts, console, and history
//note: fps displayed is the average over the statrate duration
VAR(statrate, 1, 200, 1000);  //update time for fps and edit stats
VAR(showhud, 0, 1, 1);

void vectoryawpitch(const vec &v, float &yaw, float &pitch)
{
    if(v.iszero())
    {
        yaw = pitch = 0;
    }
    else
    {
        yaw = -std::atan2(v.x, v.y)*RAD;
        pitch = std::asin(v.z/v.magnitude())*RAD;
    }
}

// iengine functionality
void damagecompass(int n, const vec &loc)
{
    if(!usedamagecompass || minimized)
    {
        return;
    }
    vec delta(loc);
    delta.sub(camera1->o);
    float yaw = 0,
          pitch;
    if(delta.magnitude() > 4)
    {
        vectoryawpitch(delta, yaw, pitch);
        yaw -= camera1->yaw;
    }
    if(yaw >= 360)
    {
        yaw = std::fmod(yaw, 360);
    }
    else if(yaw < 0)
    {
        yaw = 360 - std::fmod(-yaw, 360);
    }
    int dir = (static_cast<int>(yaw+22.5f)%360)/45; //360/45 = 8, so divide into 8 octants with 0 degrees centering octant 0
    damagedirs[dir] += std::max(n, damagecompassmin)/static_cast<float>(damagecompassmax);
    if(damagedirs[dir]>1)
    {
        damagedirs[dir] = 1;
    }
}

void damageblend(int n)
{
    if(!damagescreen || minimized)
    {
        return;
    }
    if(lastmillis > damageblendmillis)
    {
        damageblendmillis = lastmillis;
    }
    damageblendmillis += std::clamp(n, damagescreenmin, damagescreenmax)*damagescreenfactor;
}

void inithudcmds()
{
    addcommand("loadcrosshair", reinterpret_cast<identfun>(+[](const char *name, int *i){loadcrosshair(name, *i);}), "si", Id_Command);
    addcommand("getcrosshair", reinterpret_cast<identfun>(getcrosshair), "i", Id_Command);
}

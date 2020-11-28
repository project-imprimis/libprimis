/* dynlight.cpp: handling of changing lights
 *
 * while the lighting system is dynamic, the changing state of the light entities
 * for the renderer to handle must be updated with new values that reflect the tyoe
 * of light to be drawn
 *
 * this includes pulsating lights (which change in radius dynamically)
 * and multicolored lights (which change hue dynamically)
 */
#include "engine.h"

#include "physics.h"

VARNP(dynlights, usedynlights, 0, 1, 1);
VARP(dynlightdist, 0, 1024, 10000);

struct dynlight
{
    vec o, hud;
    float radius, initradius, curradius, dist;
    vec color, initcolor, curcolor;
    int fade, peak, expire, flags;
    physent *owner;
    vec dir;
    int spot;

    void calcradius()
    {
        if(fade + peak > 0)
        {
            int remaining = expire - lastmillis;
            if(flags&DynLight_Expand)
            {
                curradius = initradius + (radius - initradius) * (1.0f - remaining/static_cast<float>(fade + peak));
            }
            else if(!(flags&DynLight_Flash) && remaining > fade)
            {
                curradius = initradius + (radius - initradius) * (1.0f - static_cast<float>(remaining - fade)/peak);
            }
            else if(flags&DynLight_Shrink)
            {
                curradius = (radius*remaining)/fade;
            }
            else
            {
                curradius = radius;
            }
        }
        else
        {
            curradius = radius;
        }
    }

    void calccolor()
    {
        if(flags&DynLight_Flash || peak <= 0)
        {
            curcolor = color;
        }
        else
        {
            int peaking = expire - lastmillis - fade;
            if(peaking <= 0)
            {
                curcolor = color;
            }
            else
            {
                curcolor.lerp(initcolor, color, 1.0f - static_cast<float>(peaking)/peak);
            }
        }
        float intensity = 1.0f;
        if(fade > 0)
        {
            int fading = expire - lastmillis;
            if(fading < fade)
            {
                intensity = static_cast<float>(fading)/fade;
            }
        }
        curcolor.mul(intensity);
    }
};

std::vector<dynlight> dynlights;
std::vector<dynlight *> closedynlights;

void adddynlight(const vec &o, float radius, const vec &color, int fade, int peak, int flags, float initradius, const vec &initcolor, physent *owner, const vec &dir, int spot)
{
    if(!usedynlights)
    {
        return;
    }
    if(o.dist(camera1->o) > dynlightdist || radius <= 0)
    {
        return;
    }
    int insert = 0,
        expire = fade + peak + lastmillis;
    for(uint i = dynlights.size(); --i >=0;) //note reverse iteration
    {
        if(expire>=dynlights[i].expire)
        {
            insert = i+1;
            break;
        }
    }
    dynlight d;
    d.o = d.hud = o;
    d.radius = radius;
    d.initradius = initradius;
    d.color = color;
    d.initcolor = initcolor;
    d.fade = fade;
    d.peak = peak;
    d.expire = expire;
    d.flags = flags;
    d.owner = owner;
    d.dir = dir;
    d.spot = spot;
    dynlights.insert(dynlights.begin() + insert, d);
}

void cleardynlights()
{
    int faded = -1;
    for(uint i = 0; i < dynlights.size(); i++)
    {
        if(lastmillis<dynlights[i].expire)
        {
            faded = i;
            break;
        }
    }
    if(faded<0)
    {
        dynlights.clear();
    }
    else if(faded>0)
    {
        dynlights.erase(dynlights.begin(), dynlights.begin() + faded);
    }
}

void removetrackeddynlights(physent *owner)
{
    for(uint i = dynlights.size(); --i >=0;) //note reverse iteration
    {
        if(owner ? dynlights[i].owner == owner : dynlights[i].owner != NULL)
        {
            dynlights.erase(dynlights.begin() + i);
        }
    }
}

void updatedynlights()
{
    cleardynlights();

    for(uint i = 0; i < dynlights.size(); i++)
    {
        dynlight &d = dynlights[i];
        d.calcradius();
        d.calccolor();
    }
}

int finddynlights()
{
    closedynlights.clear();
    if(!usedynlights)
    {
        return 0;
    }
    physent e;
    e.type = PhysEnt_Camera;
    for(uint j = 0; j < dynlights.size(); j++)
    {
        dynlight &d = dynlights[j];
        if(d.curradius <= 0)
        {
            continue;
        }
        d.dist = camera1->o.dist(d.o) - d.curradius;
        if(d.dist > dynlightdist || isfoggedsphere(d.curradius, d.o))
        {
            continue;
        }
        e.o = d.o;
        e.radius = e.xradius = e.yradius = e.eyeheight = e.aboveeye = d.curradius;
        if(!collide(&e, vec(0, 0, 0), 0, false))
        {
            continue;
        }
        int insert = 0;
        for(uint i = closedynlights.size(); --i >=0;) //note reverse iteration
        {
            if(d.dist >= closedynlights[i]->dist)
            {
                insert = i+1;
                break;
            }
        }
        closedynlights.insert(closedynlights.begin() + insert, &d);
    }
    return closedynlights.size();
}

bool getdynlight(int n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags)
{
    if(static_cast<int>(closedynlights.size()) < n)
    {
        return false;
    }
    dynlight &d = *closedynlights[n];
    o = d.o;
    radius = d.curradius;
    color = d.curcolor;
    spot = d.spot;
    dir = d.dir;
    flags = d.flags & 0xFF;
    return true;
}


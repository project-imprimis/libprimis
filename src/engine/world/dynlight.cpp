/* dynlight.cpp: handling of changing lights
 *
 * while the lighting system is dynamic, the changing state of the light entities
 * for the renderer to handle must be updated with new values that reflect the type
 * of light to be drawn
 *
 * this includes pulsating lights (which change in radius dynamically)
 * and multicolored lights (which change hue dynamically)
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include "dynlight.h"
#include "physics.h"

#include "interface/control.h"

#include "render/rendergl.h"
#include "render/renderva.h"

//internally relevant functionality
namespace
{
    VARNP(dynlights, usedynlights, 0, 1, 1); //toggle using dynamic lights
    VARP(dynlightdist, 0, 1024, 10000); //distance after which dynamic lights are not rendered (1024 = 128m)

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

    vector<dynlight> dynlights;
    vector<dynlight *> closedynlights;

    //cleans up dynlights, deletes dynlights contents once none have expire field
    void cleardynlights()
    {
        int faded = -1;
        for(int i = 0; i < dynlights.length(); i++)
        {
            if(lastmillis<dynlights[i].expire)
            {
                faded = i;
                break;
            }
        }
        if(faded<0) //if any light has lastmillis > expire field
        {
            dynlights.setsize(0);
        }
        else if(faded>0)
        {
            dynlights.remove(0, faded);
        }
    }
}
//externally relevant functionality

//adds a dynamic light object to the dynlights vector with the attributes indicated (radius, color, fade, peak, flags, etc..)
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
    for(int i = dynlights.length(); --i >=0;) //note reverse iteration
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
    dynlights.insert(insert, d);
}

void removetrackeddynlights(physent *owner)
{
    for(int i = dynlights.length(); --i >=0;) //note reverse iteration
    {
        if(owner ? dynlights[i].owner == owner : dynlights[i].owner != nullptr)
        {
            dynlights.remove(i);
        }
    }
}

//finds which dynamic lights are near enough and are visible to the player
//returns the number of lights (and sets `closedynlights` vector contents to the appropriate nearby light ents)
int finddynlights()
{
    closedynlights.setsize(0);
    if(!usedynlights)
    {
        return 0;
    }
    physent e;
    e.type = PhysEnt_Camera;
    for(int j = 0; j < dynlights.length(); j++)
    {
        dynlight &d = dynlights[j];
        if(d.curradius <= 0)
        {
            continue;
        }
        d.dist = camera1->o.dist(d.o) - d.curradius;
        if(d.dist > dynlightdist || view.isfoggedsphere(d.curradius, d.o))
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
        for(int i = closedynlights.length(); --i >=0;) //note reverse iteration
        {
            if(d.dist >= closedynlights[i]->dist)
            {
                insert = i+1;
                break;
            }
        }
        closedynlights.insert(insert, &d);
    }
    return closedynlights.length();
}

/* getdynlight: gets the nth dynlight near camera and sets references to its values
 *
 * Parameters:
 *  n: the nth closest dynamic light
 *  o: a reference to set as the location of the specified dynlight
 *  radius: a reference to set as the radius of the specified dynlight
 *  color: a reference to set as the color of the specifeid dynlight
 *  dir: a reference to set as the direction the dynlight is pointing
 *  flags: a reference to the flag bitmap for the dynlight
 * Returns:
 *  bool: true if light at position n was found, false otherwise
 *
 */
bool getdynlight(int n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags)
{
    if(!closedynlights.inrange(n))
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

void updatedynlights()
{
    cleardynlights();

    for(int i = 0; i < dynlights.length(); i++)
    {
        dynlight &d = dynlights[i];
        d.calcradius();
        d.calccolor();
    }
}

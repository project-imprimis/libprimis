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

    class dynlight
    {
        public:

        vec o;
        float curradius, dist;
        int expire;
        const physent *owner;

        dynlight(vec o, int expire, physent *owner, float radius, float initradius, vec color, vec initcolor, int fade, int peak, int flags, vec dir, int spot) :
            o(o), expire(expire), owner(owner), radius(radius), initradius(initradius), color(color), initcolor(initcolor), fade(fade), peak(peak), flags(flags), dir(dir), spot(spot)
        {
        }

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

        /* dynlightinfo: gets information about this dynlight
         *
         * Parameters:
         *  n: the nth closest dynamic light
         *  o: a reference to set as the location of the specified dynlight
         *  radius: a reference to set as the radius of the specified dynlight
         *  color: a reference to set as the color of the specifeid dynlight
         *  spot: a reference to the spotlight information of the dynlight
         *  dir: a reference to set as the direction the dynlight is pointing
         *  flags: a reference to the flag bitmap for the dynlight
         */
        void dynlightinfo(vec &origin, float &radius, vec &color, vec &direction, int &spotlight, int &flagmask) const
        {
            origin = o;
            radius = curradius;
            color = curcolor;
            spotlight = spot;
            direction = dir;
            flagmask = flags & 0xFF;
        }
        private:
            float radius, initradius;
            vec color, initcolor, curcolor;
            int fade, peak, flags;
            vec dir;
            int spot;
    };

    std::vector<dynlight> dynlights;
    std::vector<const dynlight *> closedynlights;

    //cleans up dynlights, deletes dynlights contents once none have expire field
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
        if(faded<0) //if any light has lastmillis > expire field
        {
            dynlights.clear();
        }
        else if(faded>0)
        {
            dynlights.erase(dynlights.begin(), dynlights.begin() + faded);
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
    for(int i = dynlights.size(); --i >=0;) //note reverse iteration
    {
        if(expire>=dynlights[i].expire)
        {
            insert = i+1;
            break;
        }
    }
    dynlight d(o, expire, owner, radius, initradius, color, initcolor, fade, peak, flags, dir, spot);
    dynlights.insert(dynlights.begin() + insert, d);
}

void removetrackeddynlights(const physent *owner)
{
    for(int i = dynlights.size(); --i >=0;) //note reverse iteration
    {
        if(owner ? dynlights[i].owner == owner : dynlights[i].owner != nullptr)
        {
            dynlights.erase(dynlights.begin() + i);
        }
    }
}

//finds which dynamic lights are near enough and are visible to the player
//returns the number of lights (and sets `closedynlights` vector contents to the appropriate nearby light ents)
size_t finddynlights()
{
    closedynlights.clear();
    if(!usedynlights)
    {
        return 0;
    }
    physent e;
    e.type = physent::PhysEnt_Camera;
    for(dynlight &d : dynlights)
    {
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
        for(int i = closedynlights.size(); --i >=0;) //note reverse iteration
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

/* getdynlight: gets the nth dynlight near camera and sets references to its values
 *
 * Parameters:
 *  n: the nth closest dynamic light
 *  o: a reference to set as the location of the specified dynlight
 *  radius: a reference to set as the radius of the specified dynlight
 *  color: a reference to set as the color of the specifeid dynlight
 *  spot: a reference to the spotlight information of the dynlight
 *  dir: a reference to set as the direction the dynlight is pointing
 *  flags: a reference to the flag bitmap for the dynlight
 * Returns:
 *  bool: true if light at position n was found, false otherwise
 *
 */
bool getdynlight(size_t n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags)
{
    if(!(closedynlights.size() > n))
    {
        return false;
    }
    const dynlight &d = *closedynlights[n];
    d.dynlightinfo(o, radius, color, dir, spot, flags);
    return true;
}

void updatedynlights()
{
    cleardynlights();
    for(dynlight &d : dynlights)
    {
        d.calcradius();
        d.calccolor();
    }
}

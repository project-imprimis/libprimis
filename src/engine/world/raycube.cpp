
/////////////////////////  ray - cube collision ///////////////////////////////////////////////

//functions for finding where vectors hit world geometry on the octree

/* Unlike vector geometry, finding collision with world surfaces has to interact with the
 * octree geometry. Octree geometry behaves differently for identifying collision due to
 * its recursive behavior, and as a result the algorithms for identifying collision with
 * the world looks different than other world geometry systems.
 *
 * Octree cube collision is necessary for physics (as players need to interact with the world geometry)
 * and for other world interaction by players (e.g. identifying where structure in the world is)
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include "bih.h"
#include "entities.h"
#include "octaworld.h"
#include "raycube.h"
#include "world/world.h"

//internally relevant functionality
namespace
{
    const clipplanes &getclipplanes(const cube &c, const ivec &o, int size)
    {
        clipplanes &p = rootworld.getclipbounds(c, o, size, c.visible&0x80 ? 2 : 0);
        if(p.visible&0x80)
        {
            genclipplanes(c, o, size, p, false, false);
        }
        return p;
    }

    bool intersectplanes(const clipplanes& p, const vec &v, const vec& ray, float &enterdist, float &exitdist, int &entry)
    {
        for(int i = 0; i < p.size; ++i)
        {
            float pdist = p.p[i].dist(v),
                  facing = ray.dot(p.p[i]);
            if(facing < 0)
            {
                pdist /= -facing;
                if(pdist > enterdist)
                {
                    if(pdist > exitdist)
                    {
                        return false;
                    }
                    enterdist = pdist;
                    entry = i;
                }
            }
            else if(facing > 0)
            {
                pdist /= -facing;
                if(pdist < exitdist)
                {
                    if(pdist < enterdist)
                    {
                        return false;
                    }
                    exitdist = pdist;
                }
            }
            else if(pdist > 0)
            {
                return false;
            }
        }
        return true;
    }

    bool intersectbox(const clipplanes& p, const vec &v, const vec& ray, const vec& invray, float &enterdist, float &exitdist, int &entry)
    {
        for(int i = 0; i < 3; ++i)
        {
            if(ray[i])
            {
                float prad = std::fabs(p.r[i] * invray[i]),
                      pdist = (p.o[i] - v[i]) * invray[i],
                      pmin = pdist - prad,
                      pmax = pdist + prad;
                if(pmin > enterdist)
                {
                    if(pmin > exitdist)
                    {
                        return false;
                    }
                    enterdist = pmin;
                    entry = i;
                }
                if(pmax < exitdist)
                {
                    if(pmax < enterdist)
                    {
                        return false;
                    }
                    exitdist = pmax;
                }
            }
            else if(v[i] < p.o[i]-p.r[i] || v[i] > p.o[i]+p.r[i])
            {
                return false;
            }
        }
        return true;
    }

    bool raycubeintersect(const clipplanes &p, const vec &v, const vec &ray, const vec &invray, float maxdist, float &dist)
    {
        int entry   = -1,
            bbentry = -1;
        float enterdist = -1e16f,
              exitdist  = 1e16f;
        if(!intersectplanes(p, v, ray, enterdist, exitdist, entry))
        {
            return false;
        }
        if(!intersectbox(p, v, ray, invray, enterdist, exitdist, bbentry))
        {
            return false;
        }
        if(exitdist < 0)
        {
            return false;
        }
        dist = std::max(enterdist+0.1f, 0.0f);
        if(dist < maxdist)
        {
            if(bbentry>=0)
            {
                hitsurface = vec(0, 0, 0);
                hitsurface[bbentry] = ray[bbentry]>0 ? -1 : 1;
            }
            else
            {
                hitsurface = p.p[entry];
            }
        }
        return true;
    }

    float hitentdist;
    int hitent, hitorient;

    float disttoent(const octaentities *oc, const vec &o, const vec &ray, float radius, int mode, const extentity *t)
    {
        vec eo, es;
        int orient = -1;
        float dist = radius,
              f = 0.0f;
        const std::vector<extentity *> &ents = entities::getents();
    //=======ENT_SEL_INTERSECT ENT_INTERSECT
        #define ENT_INTERSECT(type, func) do { \
            for(size_t i = 0; i < oc->type.size(); i++) \
            { \
                extentity &e = *ents[oc->type[i]]; \
                if(!(e.flags&EntFlag_Octa) || &e==t) \
                { \
                    continue; \
                } \
                func; \
                if(f < dist && f > 0 && vec(ray).mul(f).add(o).insidebb(oc->o, oc->size)) \
                { \
                    hitentdist = dist = f; \
                    hitent = oc->type[i]; \
                    hitorient = orient; \
                } \
            } \
        } while(0)

        if((mode&Ray_Poly) == Ray_Poly)
        {
            ENT_INTERSECT(mapmodels,
            {
                if(!mmintersect(e, o, ray, radius, mode, f))
                {
                    continue;
                }
            });
        }

        #define ENT_SEL_INTERSECT(type) ENT_INTERSECT(type, { \
            entselectionbox(e, eo, es); \
            if(!rayboxintersect(eo, es, o, ray, f, orient)) \
            { \
                continue; \
            } \
        })

        if((mode&Ray_Ents) == Ray_Ents)
        {
            ENT_SEL_INTERSECT(other);
            ENT_SEL_INTERSECT(mapmodels);
            ENT_SEL_INTERSECT(decals);
        }

        return dist;
    }
    #undef ENT_INTERSECT
    #undef ENT_SEL_INTERSECT
    //======================================
    float disttooutsideent(const vec &o, const vec &ray, float radius, const extentity *t)
    {
        vec eo, es;
        int orient;
        float dist = radius,
              f = 0.0f;
        const std::vector<extentity *> &ents = entities::getents();
        for(const int &i : outsideents)
        {
            const extentity &e = *ents[i];
            if(!(e.flags&EntFlag_Octa) || &e == t)
            {
                continue;
            }
            entselectionbox(e, eo, es);
            if(!rayboxintersect(eo, es, o, ray, f, orient))
            {
                continue;
            }
            if(f < dist && f > 0)
            {
                hitentdist = dist = f;
                hitent = outsideents[i];
                hitorient = orient;
            }
        }
        return dist;
    }

    // optimized shadow version
    float shadowent(const octaentities *oc, const vec &o, const vec &ray, float radius, int mode, const extentity *t)
    {
        float dist = radius,
              f = 0.0f;
        const std::vector<extentity *> &ents = entities::getents();
        for(const int &i : oc->mapmodels)
        {
            const extentity &e = *ents[i];
            if(!(e.flags&EntFlag_Octa) || &e==t)
            {
                continue;
            }
            if(!mmintersect(e, o, ray, radius, mode, f))
            {
                continue;
            }
            if(f > 0 && f < dist)
            {
                dist = f;
            }
        }
        return dist;
    }

    /**
     * @brief Finds the closest value and returns it to ref parameters
     *
     * @param xval value to return if dx is smallest
     * @param yval value to return if dy is smallest
     * @param zval value to return if dz is smallest
     * @param lsizemask scale mask to shift
     * @param invray inverse of the ray to track (elementwise inverse)
     * @param lo origin of the coordinates
     * @param lshift shift factor for the sizemask
     * @param v returns sum of itself and ray scaled to dist value
     * @param dist returns sum of itself and whichever of dx/dy/dz returned
     * @param ray direction of value to add to v
     *
     * @return closest returns closest of xval, yval, zval
     */
    int findclosest(int xval, int yval, int zval, const ivec &lsizemask, const vec &invray, const ivec &lo, const int &lshift, vec &v, float &dist, const vec& ray)
    {
        float dx = (lo.x+(lsizemask.x<<lshift)-v.x)*invray.x,
              dy = (lo.y+(lsizemask.y<<lshift)-v.y)*invray.y,
              dz = (lo.z+(lsizemask.z<<lshift)-v.z)*invray.z;
        float disttonext = dx;
        int closest = xval;
        if(dy < disttonext)
        {
            disttonext = dy;
            closest = yval;
        }
        if(dz < disttonext)
        {
            disttonext = dz;
            closest = zval;
        }
        disttonext += 0.1f;
        v.add(vec(ray).mul(disttonext));
        dist += disttonext;
        return closest;
    }
}

//externally relevant functionality
bool insideworld(const vec &o)
{
    return o.x>=0 && o.x<rootworld.mapsize() && o.y>=0 && o.y<rootworld.mapsize() && o.z>=0 && o.z<rootworld.mapsize();
}

bool insideworld(const ivec &o)
{
    return static_cast<uint>(o.x) < static_cast<uint>(rootworld.mapsize()) &&
           static_cast<uint>(o.y) < static_cast<uint>(rootworld.mapsize()) &&
           static_cast<uint>(o.z) < static_cast<uint>(rootworld.mapsize());
}

vec hitsurface;

//will only change &outrad and &dist if not inside the world
//true if outside world, false if inside
bool cubeworld::checkinsideworld(const vec &invray, float radius, float &outrad, const vec &o, vec &v, const vec& ray, float &dist) const
{
    if(!insideworld(o))
    {
        float disttoworld = 0,
              exitworld = 1e16f;
        for(int i = 0; i < 3; ++i)
        {
            float c = v[i];
            if(c<0 || c>=mapsize())
            {
                float d = ((invray[i]>0?0:mapsize())-c)*invray[i];
                if(d<0)
                {
                    outrad =  radius>0 ? radius :-1;
                    return true;
                }
                disttoworld = std::max(disttoworld, 0.1f + d);
            }
            float e = ((invray[i]>0?mapsize():0)-c)*invray[i];
            exitworld = std::min(exitworld, e);
        }
        if(disttoworld > exitworld)
        {
            outrad = (radius>0?radius:-1);
            return true;
        }
        v.add(vec(ray).mul(disttoworld));
        dist += disttoworld;
    }
    return false;
}

bool cubeworld::upoctree(const vec &v, int &x, int &y, int &z, const ivec &lo, int &lshift) const
{
    x = static_cast<int>(v.x);
    y = static_cast<int>(v.y);
    z = static_cast<int>(v.z);
    uint diff = static_cast<uint>(lo.x^x)|static_cast<uint>(lo.y^y)|static_cast<uint>(lo.z^z);
    if(diff >= static_cast<uint>(mapsize()))
    {
        return true;
    }
    diff >>= lshift;
    if(!diff)
    {
        return true;
    }
    do
    {
        lshift++;
        diff >>= 1;
    } while(diff);
    return false;
}

//=================================================================== DOWNOCTREE

#define DOWNOCTREE(disttoent, earlyexit) \
        cube *lc = r.levels[r.lshift]; \
        for(;;) \
        { \
            r.lshift--; \
            lc += OCTA_STEP(x, y, z, r.lshift); \
            if(lc->ext && lc->ext->ents && r.lshift < r.elvl) \
            { \
                float edist = disttoent(lc->ext->ents, o, ray, r.dent, mode, t); \
                if(edist < r.dent) \
                { \
                    earlyexit return std::min(edist, r.dist); \
                    r.elvl = r.lshift; \
                    r.dent = std::min(r.dent, edist); \
                } \
            } \
            if(lc->children==nullptr) \
            { \
                break; \
            } \
            lc = &(*lc->children)[0]; \
            r.levels[r.lshift] = lc; \
        }

struct raycubeinfo final
{
    float dist,
          dent;
    vec v,
        invray;
    std::array<cube *, 20> levels; //NOTE: levels[20] magically assumes mapscale <20
    int lshift,
        elvl;
    ivec lsizemask;

    raycubeinfo(float radius, const vec &o, const vec &ray, int worldscale, int mode, cube *r)
    {
        dist = 0;
        dent = radius > 0 ? radius : 1e16f;
        v = o;
        invray = vec(ray.x ? 1/ray.x : 1e16f, ray.y ? 1/ray.y : 1e16f, ray.z ? 1/ray.z : 1e16f);
        levels[worldscale] = r;
        lshift = worldscale;
        elvl = mode&Ray_BB ? worldscale : 0;
        lsizemask = ivec(invray.x>0 ? 1 : 0, invray.y>0 ? 1 : 0, invray.z>0 ? 1 : 0);
    }
};

float cubeworld::raycube(const vec &o, const vec &ray, float radius, int mode, int size, const extentity *t) const
{
    if(ray.iszero())
    {
        return 0;
    }
    raycubeinfo r(radius, o, ray, worldscale, mode, &(*worldroot)[0]);
    //scope limiting brackets
    {
        float outrad = 0.f;
        if(checkinsideworld(r.invray, radius, outrad, o, r.v, ray, r.dist))
        {
            return outrad;
        }
    }
    int closest = -1,
        x = static_cast<int>(r.v.x),
        y = static_cast<int>(r.v.y),
        z = static_cast<int>(r.v.z);
    for(;;)
    {
        DOWNOCTREE(disttoent, if(mode&Ray_Shadow));

        int lsize = 1<<r.lshift;

        const cube &c = *lc;
        if((r.dist>0 || !(mode&Ray_SkipFirst)) &&
           (((mode&Ray_ClipMat) && IS_CLIPPED(c.material&MatFlag_Volume)) ||
            ((mode&Ray_EditMat) && c.material != Mat_Air) ||
            (!(mode&Ray_Pass) && lsize==size && !(c.isempty())) ||
            c.issolid() ||
            r.dent < r.dist) &&
            (!(mode&Ray_ClipMat) || (c.material&MatFlag_Clip)!=Mat_NoClip))
        {
            if(r.dist < r.dent)
            {
                if(closest < 0)
                {
                    float dx = ((x&(~0U<<r.lshift))+(r.invray.x>0 ? 0 : 1<<r.lshift)-r.v.x)*r.invray.x,
                          dy = ((y&(~0U<<r.lshift))+(r.invray.y>0 ? 0 : 1<<r.lshift)-r.v.y)*r.invray.y,
                          dz = ((z&(~0U<<r.lshift))+(r.invray.z>0 ? 0 : 1<<r.lshift)-r.v.z)*r.invray.z;
                    closest = dx > dy ? (dx > dz ? 0 : 2) : (dy > dz ? 1 : 2);
                }
                hitsurface = vec(0, 0, 0);
                hitsurface[closest] = ray[closest]>0 ? -1 : 1;
                return r.dist;
            }
            return r.dent;
        }

        const ivec lo(x&(~0U<<r.lshift), y&(~0U<<r.lshift), z&(~0U<<r.lshift));

        if(!(c.isempty()))
        {
            const clipplanes &p = getclipplanes(c, lo, lsize);
            float f = 0;
            if(raycubeintersect(p, r.v, ray, r.invray, r.dent-r.dist, f) && (r.dist+f>0 || !(mode&Ray_SkipFirst)) && (!(mode&Ray_ClipMat) || (c.material&MatFlag_Clip)!=Mat_NoClip))
            {
                return std::min(r.dent, r.dist+f);
            }
        }
        closest = findclosest(0, 1, 2, r.lsizemask, r.invray, lo, r.lshift, r.v, r.dist, ray);
        if(radius>0 && r.dist>=radius)
        {
            return std::min(r.dent, r.dist);
        }
        if(upoctree(r.v, x, y, z, lo, r.lshift))
        {
            return std::min(r.dent, radius>0 ? radius : r.dist);
        }
    }
}

// optimized version for light shadowing... every cycle here counts!!!
float cubeworld::shadowray(const vec &o, const vec &ray, float radius, int mode, const extentity *t)
{
    raycubeinfo r(radius, o, ray, worldscale, mode, &(*worldroot)[0]);
    //scope limiting brackets
    {
        float outrad = 0.f;
        if(checkinsideworld(r.invray, radius, outrad, o, r.v, ray, r.dist))
        {
            return outrad;
        }
    }
    int side = Orient_Bottom,
        x = static_cast<int>(r.v.x),
        y = static_cast<int>(r.v.y),
        z = static_cast<int>(r.v.z);
    for(;;)
    {
        DOWNOCTREE(shadowent, );

        const cube &c = *lc;
        ivec lo(x&(~0U<<r.lshift),
             y&(~0U<<r.lshift),
             z&(~0U<<r.lshift));

        if(!(c.isempty()) && !(c.material&Mat_Alpha))
        {
            if(c.issolid())
            {
                return c.texture[side]==Default_Sky && mode&Ray_SkipSky ? radius : r.dist;
            }
            const clipplanes &p = getclipplanes(c, lo, 1<<r.lshift);
            float enterdist = -1e16f,
                  exitdist  = 1e16f;
            int i = 0;
            bool intersected = intersectplanes(p, r.v, ray, enterdist, exitdist, i);
            side = p.side[i];
            if(!intersected)
            {
                goto nextcube;
            }
            intersected = intersectbox(p, r.v, ray, r.invray, enterdist, exitdist, i);
            side = (i<<1) + 1 - r.lsizemask[i];
            if(!intersected)
            {
                goto nextcube;
            }
            if(exitdist >= 0)
            {
                return c.texture[side]==Default_Sky && mode&Ray_SkipSky ? radius : r.dist+std::max(enterdist+0.1f, 0.0f);
            }
        }

    nextcube:
        side = findclosest(Orient_Right - r.lsizemask.x, Orient_Front - r.lsizemask.y, Orient_Top - r.lsizemask.z, r.lsizemask, r.invray, lo, r.lshift, r.v, r.dist, ray);
        if(r.dist>=radius)
        {
            return r.dist;
        }
        if(upoctree(r.v, x, y, z, lo, r.lshift))
        {
            return radius;
        }
    }
}
#undef DOWNOCTREE
//==============================================================================
float rayent(const vec &o, const vec &ray, float radius, int mode, int size, int &orient, int &ent)
{
    hitent = -1;
    hitentdist = radius;
    hitorient = -1;
    float dist = rootworld.raycube(o, ray, radius, mode, size);
    if((mode&Ray_Ents) == Ray_Ents)
    {
        float dent = disttooutsideent(o, ray, dist < 0 ? 1e16f : dist, nullptr);
        if(dent < 1e15f && (dist < 0 || dent < dist))
        {
            dist = dent;
        }
    }
    orient = hitorient;
    ent = hitentdist == dist ? hitent : -1;
    return dist;
}

float raycubepos(const vec &o, const vec &ray, vec &hitpos, float radius, int mode, int size)
{
    hitpos = ray;
    float dist = rootworld.raycube(o, ray, radius, mode, size);
    if(radius>0 && dist>=radius)
    {
        dist = radius;
    }
    hitpos.mul(dist).add(o);
    return dist;
}

bool raycubelos(const vec &o, const vec &dest, vec &hitpos)
{
    vec ray(dest);
    ray.sub(o);
    float mag = ray.magnitude();
    ray.mul(1/mag);
    float distance = raycubepos(o, ray, hitpos, mag, Ray_ClipMat|Ray_Poly);
    return distance >= mag;
}

float rayfloor(const vec &o, vec &floor, int mode, float radius)
{
    if(o.z<=0)
    {
        return -1;
    }
    hitsurface = vec(0, 0, 1);
    float dist = rootworld.raycube(o, vec(0, 0, -1), radius, mode);
    if(dist<0 || (radius>0 && dist>=radius))
    {
        return dist;
    }
    floor = hitsurface;
    return dist;
}

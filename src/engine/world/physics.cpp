/* physics.cpp: no physics books were hurt nor consulted in the construction of this code.
 * All physics computations and constants were invented on the fly and simply tweaked until
 * they "felt right", and have no basis in reality. Collision detection is simplistic but
 * very robust (uses discrete steps at fixed fps).
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include <memory>
#include <optional>

#include "bih.h"
#include "entities.h"
#include "mpr.h"
#include "octaworld.h"
#include "physics.h"
#include "raycube.h"
#include "world.h"

#include "interface/console.h"

#include "model/model.h"

#include "render/rendermodel.h"

int numdynents; //updated by engine, visible through iengine.h
std::vector<dynent *> dynents;

static constexpr int maxclipoffset = 4;
static constexpr int maxclipplanes = 1024;
static std::array<clipplanes, maxclipplanes> clipcache;
static int clipcacheversion = -maxclipoffset;

clipplanes &cubeworld::getclipbounds(const cube &c, const ivec &o, int size, int offset)
{
    //index is naive hash of difference between addresses (not necessarily contiguous) modulo cache size
    clipplanes &p = clipcache[static_cast<int>(&c - &((*worldroot)[0])) & (maxclipplanes-1)];
    if(p.owner != &c || p.version != clipcacheversion+offset)
    {
        p.owner = &c;
        p.version = clipcacheversion+offset;
        genclipbounds(c, o, size, p);
    }
    return p;
}

static clipplanes &getclipbounds(const cube &c, const ivec &o, int size, const physent &d)
{
    int offset = !(c.visible&0x80) || d.type==physent::PhysEnt_Player ? 0 : 1;
    return rootworld.getclipbounds(c, o, size, offset);
}

static int forceclipplanes(const cube &c, const ivec &o, int size, clipplanes &p)
{
    if(p.visible&0x80)
    {
        bool collide = true,
             noclip = false;
        if(p.version&1)
        {
            collide = false;
            noclip  = true;
        }
        genclipplanes(c, o, size, p, collide, noclip);
    }
    return p.visible;
}

void cubeworld::resetclipplanes()
{
    clipcacheversion += maxclipoffset;
    if(!clipcacheversion)
    {
        for(clipplanes &i : clipcache)
        {
            i.clear();
        }
        clipcacheversion = maxclipoffset;
    }
}

/////////////////////////  entity collision  ///////////////////////////////////////////////

// info about collisions
int collideinside; // whether an internal collision happened
const physent *collideplayer; // whether the collection hit a player

static CollisionInfo ellipseboxcollide(const physent *d, const vec &dir, const vec &origin, const vec &center, float yaw, float xr, float yr, float hi, float lo)
{
    vec cwall(0,0,0);
    float below = (origin.z+center.z-lo) - (d->o.z+d->aboveeye),
          above = (d->o.z-d->eyeheight) - (origin.z+center.z+hi);
    if(below>=0 || above>=0)
    {
        return {false, cwall};
    }
    vec yo(d->o);
    yo.sub(origin);
    yo.rotate_around_z(-yaw/RAD);
    yo.sub(center);

    float dx = std::clamp(yo.x, -xr, xr) - yo.x,
          dy = std::clamp(yo.y, -yr, yr) - yo.y,
          dist = sqrtf(dx*dx + dy*dy) - d->radius;
    if(dist < 0)
    {
        int sx = yo.x <= -xr ? -1 : (yo.x >= xr ? 1 : 0),
            sy = yo.y <= -yr ? -1 : (yo.y >= yr ? 1 : 0);
        if(dist > (yo.z < 0 ? below : above) && (sx || sy))
        {
            vec ydir(dir);
            ydir.rotate_around_z(-yaw/RAD);
            if(sx*yo.x - xr > sy*yo.y - yr)
            {
                if(dir.iszero() || sx*ydir.x < -1e-6f)
                {
                    cwall = vec(sx, 0, 0);
                    cwall.rotate_around_z(yaw/RAD);
                    return {true, cwall};
                }
            }
            else if(dir.iszero() || sy*ydir.y < -1e-6f)
            {
                cwall = vec(0, sy, 0);
                cwall.rotate_around_z(yaw/RAD);
                return {true, cwall};
            }
        }
        if(yo.z < 0)
        {
            if(dir.iszero() || (dir.z > 0 && (d->type!=physent::PhysEnt_Player || below >= d->zmargin-(d->eyeheight+d->aboveeye)/4.0f)))
            {
                cwall = vec(0, 0, -1);
                return {true, cwall};
            }
        }
        else if(dir.iszero() || (dir.z < 0 && (d->type!=physent::PhysEnt_Player || above >= d->zmargin-(d->eyeheight+d->aboveeye)/3.0f)))
        {
            cwall = vec(0, 0, 1);
            return {true, cwall};
        }
        collideinside++;
    }
    return {false, vec(0,0,0)};
}

static CollisionInfo ellipsecollide(const physent *d, const vec &dir, const vec &o, const vec &center, float yaw, float xr, float yr, float hi, float lo)
{
    vec cwall(0,0,0);
    float below = (o.z+center.z-lo) - (d->o.z+d->aboveeye),
          above = (d->o.z-d->eyeheight) - (o.z+center.z+hi);
    if(below>=0 || above>=0)
    {
        return {false, cwall};
    }
    vec yo(center);
    yo.rotate_around_z(yaw/RAD);
    yo.add(o);
    float x = yo.x - d->o.x,
          y = yo.y - d->o.y,
          angle = atan2f(y, x),
          dangle = angle-d->yaw/RAD,
          eangle = angle-yaw/RAD,
          dx = d->xradius*std::cos(dangle),
          dy = d->yradius*std::sin(dangle),
          ex = xr*std::cos(eangle),
          ey = yr*std::sin(eangle),
          dist = sqrtf(x*x + y*y) - sqrtf(dx*dx + dy*dy) - sqrtf(ex*ex + ey*ey);
    if(dist < 0)
    {
        if(dist > (d->o.z < yo.z ? below : above) && (dir.iszero() || x*dir.x + y*dir.y > 0))
        {
            cwall = vec(-x, -y, 0).rescale(1);
            return {true, cwall};
        }
        if(d->o.z < yo.z)
        {
            if(dir.iszero() || (dir.z > 0 && (d->type!=physent::PhysEnt_Player || below >= d->zmargin-(d->eyeheight+d->aboveeye)/4.0f)))
            {
                cwall = vec(0, 0, -1);
                return {true, cwall};
            }
        }
        else if(dir.iszero() || (dir.z < 0 && (d->type!=physent::PhysEnt_Player || above >= d->zmargin-(d->eyeheight+d->aboveeye)/3.0f)))
        {
            cwall = vec(0, 0, 1);
            return {true, cwall};
        }
        collideinside++;
    }
    return {false, vec(0,0,0)};
}

static constexpr int dynentcachesize = 1024;

static size_t dynentframe = 0;

static struct dynentcacheentry final
{
    int x, y;
    size_t frame;
    std::vector<const physent *> dynents;
} dynentcache[dynentcachesize];

//resets the dynentcache[] array entries
//used in iengine
void cleardynentcache()
{
    dynentframe++;
    if(!dynentframe || dynentframe == 1)
    {
        for(int i = 0; i < dynentcachesize; ++i)
        {
            dynentcache[i].frame = 0;
        }
    }
    if(!dynentframe)
    {
        dynentframe = 1;
    }
}

//returns the dynent at location i in the dynents vector
//used in iengine
dynent *iterdynents(int i)
{
    if(i < static_cast<int>(dynents.size()))
    {
        return dynents[i];
    }
    return nullptr;
}

VARF(dynentsize, 4, 7, 12, cleardynentcache());

static int dynenthash(int x, int y)
{
    return (((((x)^(y))<<5) + (((x)^(y))>>5)) & (dynentcachesize - 1));
}

static const std::vector<const physent *> &checkdynentcache(int x, int y)
{
    dynentcacheentry &dec = dynentcache[dynenthash(x, y)];
    if(dec.x == x && dec.y == y && dec.frame == dynentframe)
    {
        return dec.dynents;
    }
    dec.x = x;
    dec.y = y;
    dec.frame = dynentframe;
    dec.dynents.clear();
    int numdyns = numdynents,
        dsize = 1<<dynentsize,
        dx = x<<dynentsize,
        dy = y<<dynentsize;
    for(int i = 0; i < numdyns; ++i)
    {
        dynent *d = iterdynents(i);
        if(d->ragdoll ||
           d->o.x+d->radius <= dx || d->o.x-d->radius >= dx+dsize ||
           d->o.y+d->radius <= dy || d->o.y-d->radius >= dy+dsize)
        {
            continue;
        }
        dec.dynents.push_back(d);
    }
    return dec.dynents;
}

//============================================================== LOOPDYNENTCACHE
#define LOOPDYNENTCACHE(curx, cury, o, radius) \
    for(int curx = std::max(static_cast<int>(o.x-radius), 0)>>dynentsize, endx = std::min(static_cast<int>(o.x+radius), rootworld.mapsize()-1)>>dynentsize; curx <= endx; curx++) \
        for(int cury = std::max(static_cast<int>(o.y-radius), 0)>>dynentsize, endy = std::min(static_cast<int>(o.y+radius), rootworld.mapsize()-1)>>dynentsize; cury <= endy; cury++)

//used in iengine
void updatedynentcache(physent *d)
{
    LOOPDYNENTCACHE(x, y, d->o, d->radius)
    {
        dynentcacheentry &dec = dynentcache[dynenthash(x, y)];
        if(dec.x != x || dec.y != y || dec.frame != dynentframe || (std::find(dec.dynents.begin(), dec.dynents.end(), d) != dec.dynents.end()))
        {
            continue;
        }
        dec.dynents.push_back(d);
    }
}

template<class O>
static CollisionInfo plcollide(const physent *d, const vec &dir, const physent *o)
{
    mpr::EntOBB entvol(d);
    O obvol(o);
    vec cp;
    if(mpr::collide(entvol, obvol, nullptr, nullptr, &cp))
    {
        vec wn = cp.sub(obvol.center());
        vec cwall = obvol.contactface(wn, dir.iszero() ? wn.neg() : dir);
        if(!cwall.iszero())
        {
            return {true, cwall};
        }
        collideinside++;
    }
    return {false, vec(0,0,0)};
}

static CollisionInfo plcollide(const physent *d, const vec &dir, const physent *o)
{
    switch(d->collidetype)
    {
        case Collide_Ellipse:
        {
            if(o->collidetype == Collide_Ellipse)
            {
                return ellipsecollide(d, dir, o->o, vec(0, 0, 0), o->yaw, o->xradius, o->yradius, o->aboveeye, o->eyeheight);
            }
            else
            {
                return ellipseboxcollide(d, dir, o->o, vec(0, 0, 0), o->yaw, o->xradius, o->yradius, o->aboveeye, o->eyeheight);
            }
        }
        case Collide_OrientedBoundingBox:
        {
            if(o->collidetype == Collide_Ellipse)
            {
                return plcollide<mpr::EntCylinder>(d, dir, o);
            }
            else
            {
                return plcollide<mpr::EntOBB>(d, dir, o);
            }
        }
        default:
        {
            return {false, vec(0,0,0)};
        }
    }
}

CollisionInfo plcollide(const physent *d, const vec &dir, bool insideplayercol)    // collide with player
{
    if(d->type==physent::PhysEnt_Camera)
    {
        return {false, vec(0,0,0)};
    }
    int lastinside = collideinside;
    const physent *insideplayer = nullptr;
    LOOPDYNENTCACHE(x, y, d->o, d->radius)
    {
        const std::vector<const physent *> &dynents = checkdynentcache(x, y);
        for(const physent* const& o: dynents)
        {
            if(o==d || d->o.reject(o->o, d->radius+o->radius))
            {
                continue;
            }
            if(CollisionInfo ci = plcollide(d, dir, o); ci.collided)
            {
                collideplayer = o;
                return ci;
            }
            if(collideinside > lastinside)
            {
                lastinside = collideinside;
                insideplayer = o;
            }
        }
    }
    if(insideplayer && insideplayercol)
    {
        collideplayer = insideplayer;
        return {true, vec(0,0,0)};
    }
    return {false, vec(0,0,0)};
}

#undef LOOPDYNENTCACHE
//==============================================================================

//orient consists of {yaw, pitch, roll}
//cwall -> collide wall
template<class M>
static CollisionInfo mmcollide(const physent *d, const vec &dir, const extentity &e, const vec &center, const vec &radius, const ivec &orient)
{
    mpr::EntOBB entvol(d);
    M mdlvol(e.o, center, radius, orient.x, orient.y, orient.z);
    vec cp;
    if(mpr::collide(entvol, mdlvol, nullptr, nullptr, &cp))
    {
        vec wn = cp.sub(mdlvol.center());
        vec cwall = mdlvol.contactface(wn, dir.iszero() ? wn.neg() : dir);
        if(!cwall.iszero())
        {
            return {true, cwall};
        }
        collideinside++;
    }
    return {false, vec(0,0,0)};
}

/**
 * @brief Preliminary check for fuzzycollide functions.
 *
 * @tparam E the mpr object type to evaluate
 * @param d the physent to get object location and other information from
 * @param mdlvol the model volume bounding values to check
 * @param bbradius the bounding box radius to check against
 *
 * @return true if it is possible for these models to collide
 * @return false if there is no way for these models to collide
 */
template<class E>
static bool checkfuzzycollidebounds(const physent *d, const E &mdlvol, const vec &bbradius)
{
    if(std::fabs(d->o.x - mdlvol.o.x) > bbradius.x + d->radius ||
       std::fabs(d->o.y - mdlvol.o.y) > bbradius.y + d->radius ||
       d->o.z + d->aboveeye < mdlvol.o.z - bbradius.z ||
       d->o.z - d->eyeheight > mdlvol.o.z + bbradius.z)
    {
        return false;
    }
    return true;
}

//cwall -> collide wall
//orient {yaw, pitch, roll}
static CollisionInfo fuzzycollidebox(const physent *d, const vec &dir, float cutoff, const vec &o, const vec &center, const vec &radius, const ivec &orient)
{
    mpr::ModelOBB mdlvol(o, center, radius, orient.x, orient.y, orient.z);
    vec bbradius = mdlvol.orient.abstransposedtransform(radius);
    if(!checkfuzzycollidebounds(d, mdlvol, bbradius))
    {
        return {false, vec(0,0,0)};
    }
    mpr::EntCapsule entvol(d);
    vec cwall(0, 0, 0);
    float bestdist = -1e10f;
    for(int i = 0; i < 6; ++i)
    {
        vec w;
        float dist;
        switch(i)
        {
            default:
            case 0:
            {
                w = mdlvol.orient.rowx().neg();
                dist = -radius.x;
                break;
            }
            case 1:
            {
                w = mdlvol.orient.rowx();
                dist = -radius.x;
                break;
            }
            case 2:
            {
                w = mdlvol.orient.rowy().neg();
                dist = -radius.y;
                break;
            }
            case 3:
            {
                w = mdlvol.orient.rowy();
                dist = -radius.y;
                break;
            }
            case 4:
            {
                w = mdlvol.orient.rowz().neg();
                dist = -radius.z;
                break;
            }
            case 5:
            {
                w = mdlvol.orient.rowz();
                dist = -radius.z;
                break;
            }
        }
        vec pw = entvol.supportpoint(w.neg());
        dist += w.dot(pw.sub(mdlvol.o));
        if(dist >= 0)
        {
            return {false, cwall};
        }
        if(dist <= bestdist)
        {
            continue;
        }
        cwall = vec(0, 0, 0);
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude())
            {
                continue;
            }
            //nasty ternary in the indented part
            if(d->type==physent::PhysEnt_Player &&
                    dist < (dir.z*w.z < 0 ?
                        d->zmargin-(d->eyeheight+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                        (dir.x*w.x < 0 || dir.y*w.y < 0 ? -d->radius : 0)))
            {
                continue;
            }
        }
        cwall = w;
    }
    if(cwall.iszero())
    {
        collideinside++;
        return {false, cwall};
    }
    return {true, cwall};
}

//orient consists of {yaw, pitch, roll}
//cwall -> collide wall
template<class E>
static CollisionInfo fuzzycollideellipse(const physent *d, const vec &dir, float cutoff, const vec &o, const vec &center, const vec &radius, const ivec &orient)
{
    mpr::ModelEllipse mdlvol(o, center, radius, orient.x, orient.y, orient.z);
    vec bbradius = mdlvol.orient.abstransposedtransform(radius);

    if(!checkfuzzycollidebounds(d, mdlvol, bbradius))
    {
        return {false, vec(0,0,0)};
    }
    E entvol(d);
    vec cwall(0, 0, 0);
    float bestdist = -1e10f;
    for(int i = 0; i < 3; ++i)
    {
        vec w;
        float dist;
        switch(i)
        {
            default:
            case 0:
            {
                w = mdlvol.orient.rowz();
                dist = -radius.z;
                break;
            }
            case 1:
            {
                w = mdlvol.orient.rowz().neg();
                dist = -radius.z;
                break;
            }
            case 2:
            {
                vec2 ln(mdlvol.orient.transform(entvol.center().sub(mdlvol.o)));
                float r = ln.magnitude();
                if(r < 1e-6f)
                {
                    continue;
                }
                vec2 lw = vec2(ln.x*radius.y, ln.y*radius.x).normalize();
                w = mdlvol.orient.transposedtransform(lw);
                dist = -vec2(ln.x*radius.x, ln.y*radius.y).dot(lw)/r;
                break;
            }
        }
        vec pw = entvol.supportpoint(vec(w).neg());
        dist += w.dot(vec(pw).sub(mdlvol.o));
        if(dist >= 0)
        {
            return {false, vec(0,0,0)};
        }
        if(dist <= bestdist)
        {
            continue;
        }
        cwall = vec(0, 0, 0);
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude())
            {
                continue;
            }
            if(d->type==physent::PhysEnt_Player &&
                dist < (dir.z*w.z < 0 ?
                    d->zmargin-(d->eyeheight+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                    (dir.x*w.x < 0 || dir.y*w.y < 0 ? -d->radius : 0)))
            {
                continue;
            }
        }
        cwall = w;
    }
    if(cwall.iszero())
    {
        collideinside++;
        return {false, cwall};
    }
    return {true, cwall};
}

//force a collision type:
// 0: do not force
// 1: Collide_Ellipse
// 2: Collide_OrientedBoundingBox
VAR(testtricol, 0, 0, 2);

static CollisionInfo mmcollide(const physent *d, const vec &dir, float cutoff, const octaentities &oc) // collide with a mapmodel
{
    const std::vector<extentity *> &ents = entities::getents();
    for(const int &i : oc.mapmodels)
    {
        extentity &e = *ents[i];
        if(e.flags&EntFlag_NoCollide || !(static_cast<int>(mapmodel::mapmodels.size()) > e.attr1))
        {
            continue;
        }
        mapmodelinfo &mmi = mapmodel::mapmodels[e.attr1];
        model *m = mmi.collide;
        if(!m)
        {
            if(!mmi.m && !loadmodel("", e.attr1))
            {
                continue;
            }
            if(!mmi.m->collidemodel.empty())
            {
                m = loadmodel(mmi.m->collidemodel);
            }
            if(!m)
            {
                m = mmi.m;
            }
            mmi.collide = m;
        }
        int mcol = mmi.m->collide;
        if(!mcol)
        {
            continue;
        }
        vec center, radius;
        float rejectradius = m->collisionbox(center, radius),
              scale = e.attr5 > 0 ? e.attr5/100.0f : 1;
        center.mul(scale);
        if(d->o.reject(vec(e.o).add(center), d->radius + rejectradius*scale))
        {
            continue;
        }
        int yaw   = e.attr2,
            pitch = e.attr3,
            roll  = e.attr4;
        if(mcol == Collide_TRI || testtricol)
        {
            m->setBIH();
            switch(testtricol ? testtricol : d->collidetype)
            {
                case Collide_Ellipse:
                {
                    if(CollisionInfo ci = m->bih->ellipsecollide(d, dir, cutoff, e.o, yaw, pitch, roll, scale); ci.collided)
                    {
                        return ci;
                    }
                    break;
                }
                case Collide_OrientedBoundingBox:
                {
                    if(CollisionInfo ci = m->bih->boxcollide(d, dir, cutoff, e.o, yaw, pitch, roll, scale); ci.collided)
                    {
                        return ci;
                    }
                    break;
                }
                default:
                {
                    continue;
                }
            }
        }
        else
        {
            radius.mul(scale);
            switch(d->collidetype)
            {
                case Collide_Ellipse:
                {
                    if(mcol == Collide_Ellipse)
                    {
                        if(pitch || roll)
                        {
                            CollisionInfo ci = fuzzycollideellipse<mpr::EntCapsule>(d, dir, cutoff, e.o, center, radius, {yaw, pitch, roll});
                            if(ci.collided)
                            {
                                return ci;
                            }
                        }
                        else if(CollisionInfo ci = ellipsecollide(d, dir, e.o, center, yaw, radius.x, radius.y, radius.z, radius.z); ci.collided)
                        {
                            return ci;
                        }
                    }
                    else if(pitch || roll)
                    {
                        if(CollisionInfo ci = fuzzycollidebox(d, dir, cutoff, e.o, center, radius, {yaw, pitch, roll}); ci.collided)
                        {
                            return ci;
                        }
                    }
                    else if(CollisionInfo ci = ellipseboxcollide(d, dir, e.o, center, yaw, radius.x, radius.y, radius.z, radius.z); ci.collided)
                    {
                        return ci;
                    }
                    break;
                }
                case Collide_OrientedBoundingBox:
                {
                    if(mcol == Collide_Ellipse)
                    {
                        if(CollisionInfo ci =  mmcollide<mpr::ModelEllipse>(d, dir, e, center, radius, {yaw, pitch, roll}); ci.collided)
                        {
                            return ci;
                        }
                    }
                    else if(CollisionInfo ci = mmcollide<mpr::ModelOBB>(d, dir, e, center, radius, {yaw, pitch, roll}); ci.collided)
                    {
                        return ci;
                    }
                    break;
                }
                default:
                {
                    continue;
                }
            }
        }
    }
    return {false, vec(0,0,0)};
}

static bool checkside(const physent &d, int side, const vec &dir, const int visible, const float cutoff, float distval, float dotval, float margin, vec normal, vec &collidewall, float &bestdist)
{
    if(visible&(1<<side))
    {
        float dist = distval;
        if(dist > 0)
        {
            return false;
        }
        if(dist <= bestdist)
        {
            return true;
        }
        if(!dir.iszero())
        {
            if(dotval >= -cutoff*dir.magnitude())
            {
                return true;
            }
            if(d.type==physent::PhysEnt_Player && dotval < 0 && dist < margin)
            {
                return true;
            }
        }
        collidewall = normal;
        bestdist = dist;
    }
    return true;
}

//cwall -> collide wall
static CollisionInfo fuzzycollidesolid(const physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with solid cube geometry
{
    vec cwall(0, 0, 0);
    int crad = size/2;
    if(std::fabs(d->o.x - co.x - crad) > d->radius + crad || std::fabs(d->o.y - co.y - crad) > d->radius + crad ||
       d->o.z + d->aboveeye < co.z || d->o.z - d->eyeheight > co.z + size)
    {
        return {false, cwall};
    }
    float bestdist = -1e10f;
    int visible = !(c.visible&0x80) || d->type==physent::PhysEnt_Player ? c.visible : 0xFF;

    //if any of these checks are false (NAND of all of these checks)
    if(!( checkside(*d, Orient_Left, dir, visible, cutoff, co.x - (d->o.x + d->radius), -dir.x, -d->radius, vec(-1, 0, 0), cwall, bestdist)
       && checkside(*d, Orient_Right, dir, visible, cutoff, d->o.x - d->radius - (co.x + size), dir.x, -d->radius, vec(1, 0, 0), cwall, bestdist)
       && checkside(*d, Orient_Back, dir, visible, cutoff, co.y - (d->o.y + d->radius), -dir.y, -d->radius, vec(0, -1, 0), cwall, bestdist)
       && checkside(*d, Orient_Front, dir, visible, cutoff, d->o.y - d->radius - (co.y + size), dir.y, -d->radius, vec(0, 1, 0), cwall, bestdist)
       && checkside(*d, Orient_Bottom, dir, visible, cutoff, co.z - (d->o.z + d->aboveeye), -dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/4.0f, vec(0, 0, -1), cwall, bestdist)
       && checkside(*d, Orient_Top, dir, visible, cutoff, d->o.z - d->eyeheight - (co.z + size), dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/3.0f, vec(0, 0, 1), cwall, bestdist))
       )
    {
        return {false, cwall};
    }
    if(cwall.iszero())
    {
        collideinside++;
        return {false, cwall};
    }
    return {true, cwall};
}

template<class E>
static bool clampcollide(const clipplanes &p, const E &entvol, const plane &w, const vec &pw)
{
    if(w.x && (w.y || w.z) && std::fabs(pw.x - p.o.x) > p.r.x)
    {
        vec c = entvol.center();
        float fv = pw.x < p.o.x ? p.o.x-p.r.x : p.o.x+p.r.x,
              fdist = (w.x*fv + w.y*c.y + w.z*c.z + w.offset) / (w.y*w.y + w.z*w.z);
        vec fdir(fv - c.x, -w.y*fdist, -w.z*fdist);
        if((pw.y-c.y-fdir.y)*w.y + (pw.z-c.z-fdir.z)*w.z >= 0 && entvol.supportpoint(fdir).squaredist(c) < fdir.squaredlen())
        {
            return true;
        }
    }
    if(w.y && (w.x || w.z) && std::fabs(pw.y - p.o.y) > p.r.y)
    {
        vec c = entvol.center();
        float fv = pw.y < p.o.y ? p.o.y-p.r.y : p.o.y+p.r.y,
              fdist = (w.x*c.x + w.y*fv + w.z*c.z + w.offset) / (w.x*w.x + w.z*w.z);
        vec fdir(-w.x*fdist, fv - c.y, -w.z*fdist);
        if((pw.x-c.x-fdir.x)*w.x + (pw.z-c.z-fdir.z)*w.z >= 0 && entvol.supportpoint(fdir).squaredist(c) < fdir.squaredlen())
        {
            return true;
        }
    }
    if(w.z && (w.x || w.y) && std::fabs(pw.z - p.o.z) > p.r.z)
    {
        vec c = entvol.center();
        float fv = pw.z < p.o.z ? p.o.z-p.r.z : p.o.z+p.r.z,
              fdist = (w.x*c.x + w.y*c.y + w.z*fv + w.offset) / (w.x*w.x + w.y*w.y);
        vec fdir(-w.x*fdist, -w.y*fdist, fv - c.z);
        if((pw.x-c.x-fdir.x)*w.x + (pw.y-c.y-fdir.y)*w.y >= 0 && entvol.supportpoint(fdir).squaredist(c) < fdir.squaredlen())
        {
            return true;
        }
    }
    return false;
}

static CollisionInfo fuzzycollideplanes(const physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with deformed cube geometry
{
    vec cwall(0, 0, 0);

    clipplanes &p = getclipbounds(c, co, size, *d);

    if(std::fabs(d->o.x - p.o.x) > p.r.x + d->radius || std::fabs(d->o.y - p.o.y) > p.r.y + d->radius ||
       d->o.z + d->aboveeye < p.o.z - p.r.z || d->o.z - d->eyeheight > p.o.z + p.r.z)
    {
        return {false, cwall};
    }
    float bestdist = -1e10f;
    int visible = forceclipplanes(c, co, size, p);

    if(!( checkside(*d, Orient_Left, dir, visible, cutoff,   p.o.x - p.r.x - (d->o.x + d->radius),   -dir.x, -d->radius, vec(-1, 0, 0), cwall, bestdist)
       && checkside(*d, Orient_Right, dir, visible, cutoff,  d->o.x - d->radius - (p.o.x + p.r.x),    dir.x, -d->radius, vec(1, 0, 0), cwall, bestdist)
       && checkside(*d, Orient_Back, dir, visible, cutoff,   p.o.y - p.r.y - (d->o.y + d->radius),   -dir.y, -d->radius, vec(0, -1, 0), cwall, bestdist)
       && checkside(*d, Orient_Front, dir, visible, cutoff,  d->o.y - d->radius - (p.o.y + p.r.y),    dir.y, -d->radius, vec(0, 1, 0), cwall, bestdist)
       && checkside(*d, Orient_Bottom, dir, visible, cutoff, p.o.z - p.r.z - (d->o.z + d->aboveeye), -dir.z,  d->zmargin-(d->eyeheight+d->aboveeye)/4.0f, vec(0, 0, -1), cwall, bestdist)
       && checkside(*d, Orient_Top, dir, visible, cutoff,    d->o.z - d->eyeheight - (p.o.z + p.r.z), dir.z,  d->zmargin-(d->eyeheight+d->aboveeye)/3.0f, vec(0, 0, 1), cwall, bestdist))
       )
    {
        return {false, cwall};
    }

    mpr::EntCapsule entvol(d);
    int bestplane = -1;
    for(int i = 0; i < p.size; ++i)
    {
        const plane &w = p.p[i];
        vec pw = entvol.supportpoint(vec(w).neg());
        float dist = w.dist(pw);
        if(dist >= 0)
        {
            return {false, cwall};
        }
        if(dist <= bestdist)
        {
            continue;
        }
        bestplane = -1;
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude())
            {
                continue;
            }
            //nasty ternary
            if(d->type==physent::PhysEnt_Player &&
                dist < (dir.z*w.z < 0 ?
                        d->zmargin-(d->eyeheight+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                        (dir.x*w.x < 0 || dir.y*w.y < 0 ? -d->radius : 0)))
            {
                continue;
            }
        }
        if(clampcollide(p, entvol, w, pw))
        {
            continue;
        }
        bestplane = i;
    }

    if(bestplane >= 0)
    {
        cwall = p.p[bestplane];
    }
    else if(cwall.iszero())
    {
        collideinside++;
        return {false, cwall};
    }
    return {true, cwall};
}

//cwall -> collide wall
static CollisionInfo cubecollidesolid(const physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with solid cube geometry
{
    vec cwall(0, 0, 0);
    int crad = size/2;
    if(std::fabs(d->o.x - co.x - crad) > d->radius + crad || std::fabs(d->o.y - co.y - crad) > d->radius + crad ||
       d->o.z + d->aboveeye < co.z || d->o.z - d->eyeheight > co.z + size)
    {
        return {false, cwall};
    }
    mpr::EntOBB entvol(d);
    bool collided = mpr::collide(mpr::SolidCube(co, size), entvol);
    if(!collided)
    {
        return {false, cwall};
    }
    float bestdist = -1e10f;
    int visible = !(c.visible&0x80) || d->type==physent::PhysEnt_Player ? c.visible : 0xFF;

    if(!( checkside(*d, Orient_Left, dir, visible, cutoff, co.x - entvol.right(), -dir.x, -d->radius, vec(-1, 0, 0), cwall, bestdist)
       && checkside(*d, Orient_Right, dir, visible, cutoff, entvol.left() - (co.x + size), dir.x, -d->radius, vec(1, 0, 0), cwall, bestdist)
       && checkside(*d, Orient_Back, dir, visible, cutoff, co.y - entvol.front(), -dir.y, -d->radius, vec(0, -1, 0), cwall, bestdist)
       && checkside(*d, Orient_Front, dir, visible, cutoff, entvol.back() - (co.y + size), dir.y, -d->radius, vec(0, 1, 0), cwall, bestdist)
       && checkside(*d, Orient_Bottom, dir, visible, cutoff, co.z - entvol.top(), -dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/4.0f, vec(0, 0, -1), cwall, bestdist)
       && checkside(*d, Orient_Top, dir, visible, cutoff, entvol.bottom() - (co.z + size), dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/3.0f, vec(0, 0, 1), cwall, bestdist))
      )
    {
        return {false, cwall};
    }

    if(cwall.iszero())
    {
        collideinside++;
        return {false, cwall};
    }
    return {true, cwall};
}

//cwall -> collide wall
static CollisionInfo cubecollideplanes(const physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with deformed cube geometry
{
    vec cwall(0, 0, 0);
    clipplanes &p = getclipbounds(c, co, size, *d);
    if(std::fabs(d->o.x - p.o.x) > p.r.x + d->radius || std::fabs(d->o.y - p.o.y) > p.r.y + d->radius ||
       d->o.z + d->aboveeye < p.o.z - p.r.z || d->o.z - d->eyeheight > p.o.z + p.r.z)
    {
        return {false, cwall};
    }
    mpr::EntOBB entvol(d);
    bool collided = mpr::collide(mpr::CubePlanes(p), entvol);
    if(!collided)
    {
        return {false, cwall};
    }
    float bestdist = -1e10f;
    int visible = forceclipplanes(c, co, size, p);
    if(!( checkside(*d, Orient_Left, dir, visible, cutoff, p.o.x - p.r.x - entvol.right(),  -dir.x, -d->radius, vec(-1, 0, 0), cwall, bestdist)
       && checkside(*d, Orient_Right, dir, visible, cutoff, entvol.left() - (p.o.x + p.r.x), dir.x, -d->radius, vec(1, 0, 0), cwall, bestdist)
       && checkside(*d, Orient_Back, dir, visible, cutoff, p.o.y - p.r.y - entvol.front(),  -dir.y, -d->radius, vec(0, -1, 0), cwall, bestdist)
       && checkside(*d, Orient_Front, dir, visible, cutoff, entvol.back() - (p.o.y + p.r.y), dir.y, -d->radius, vec(0, 1, 0), cwall, bestdist)
       && checkside(*d, Orient_Bottom, dir, visible, cutoff, p.o.z - p.r.z - entvol.top(),  -dir.z,  d->zmargin-(d->eyeheight+d->aboveeye)/4.0f, vec(0, 0, -1), cwall, bestdist)
       && checkside(*d, Orient_Top, dir, visible, cutoff, entvol.bottom() - (p.o.z + p.r.z), dir.z,  d->zmargin-(d->eyeheight+d->aboveeye)/3.0f, vec(0, 0, 1), cwall, bestdist))
      )
    {
        return {false, cwall};
    }

    int bestplane = -1;
    for(int i = 0; i < p.size; ++i)
    {
        const plane &w = p.p[i];
        vec pw = entvol.supportpoint(vec(w).neg());
        float dist = w.dist(pw);
        if(dist <= bestdist)
        {
            continue;
        }
        bestplane = -1;
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude())
            {
                continue;
            }
            if(d->type==physent::PhysEnt_Player &&
                dist < (dir.z*w.z < 0 ?
                d->zmargin-(d->eyeheight+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                (dir.x*w.x < 0 || dir.y*w.y < 0 ? -d->radius : 0)))
            {
                continue;
            }
        }
        if(clampcollide(p, entvol, w, pw))
        {
            continue;
        }
        bestplane = i;
    }

    if(bestplane >= 0)
    {
        cwall = p.p[bestplane];
    }
    else if(cwall.iszero())
    {
        collideinside++;
        return {false, cwall};
    }
    return {true, cwall};
}

static CollisionInfo cubecollide(const physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size, bool solid)
{
    switch(d->collidetype)
    {
        case Collide_OrientedBoundingBox:
        {
            if(c.issolid() || solid)
            {
                return cubecollidesolid(d, dir, cutoff, c, co, size);
            }
            else
            {
                return cubecollideplanes(d, dir, cutoff, c, co, size);
            }
        }
        case Collide_Ellipse:
        {
            if(c.issolid() || solid)
            {
                return fuzzycollidesolid(d, dir, cutoff, c, co, size);
            }
            else
            {
                return fuzzycollideplanes(d, dir, cutoff, c, co, size);
            }
        }
        default:
        {
            return {false, vec(0,0,0)};
        }
    }
}

static CollisionInfo octacollide(const physent *d, const vec &dir, float cutoff, const ivec &bo, const ivec &bs, const std::array<cube, 8> &c, const ivec &cor, int size) // collide with octants
{
    LOOP_OCTA_BOX(cor, size, bo, bs)
    {
        if(c[i].ext && c[i].ext->ents)
        {
            CollisionInfo ci = mmcollide(d, dir, cutoff, *c[i].ext->ents);
            if(ci.collided)
            {
                return ci;
            }
        }
        ivec o(i, cor, size);
        if(c[i].children)
        {
            CollisionInfo ci = octacollide(d, dir, cutoff, bo, bs, *(c[i].children), o, size>>1);
            if(ci.collided)
            {
                return ci;
            }
        }
        else
        {
            bool solid = false;
            switch(c[i].material&MatFlag_Clip)
            {
                case Mat_NoClip:
                {
                    continue;
                }
                case Mat_Clip:
                {
                    if(IS_CLIPPED(c[i].material&MatFlag_Volume) || d->type==physent::PhysEnt_Player)
                    {
                        solid = true;
                    }
                    break;
                }
            }
            if(!solid && c[i].isempty())
            {
                continue;
            }
            CollisionInfo ci = cubecollide(d, dir, cutoff, c[i], o, size, solid);
            if(ci.collided)
            {
                return ci;
            }
        }
    }
    return {false, vec(0,0,0)};
}

/**
 * @brief Returns whether the entity passed has collided with this octaworld.
 *
 * @param d the physent to check
 * @param dir the direction at which to check for a collision
 * @param cutoff the model cutoff factor
 * @param bo the vector for the minimum position of the model
 * @param bs the vector for the maximum position of the model
 * @param cw the cube octree world to check collision against
 */
static CollisionInfo octacollide(const physent *d, const vec &dir, float cutoff, const ivec &bo, const ivec &bs, const cubeworld &cw)
{
    int diff = (bo.x^bs.x) | (bo.y^bs.y) | (bo.z^bs.z),
        scale = cw.mapscale()-1;
    if(diff&~((1<<scale)-1) || static_cast<uint>(bo.x|bo.y|bo.z|bs.x|bs.y|bs.z) >= static_cast<uint>(cw.mapsize()))
    {
       return octacollide(d, dir, cutoff, bo, bs, *cw.worldroot, ivec(0, 0, 0), cw.mapsize()>>1);
    }
    const cube *c = &((*cw.worldroot)[OCTA_STEP(bo.x, bo.y, bo.z, scale)]);
    if(c->ext && c->ext->ents)
    {
        if(CollisionInfo ci = mmcollide(d, dir, cutoff, *c->ext->ents); ci.collided)
        {
            return ci;
        }
    }
    scale--;
    while(c->children && !(diff&(1<<scale)))
    {
        c = &((*c->children)[OCTA_STEP(bo.x, bo.y, bo.z, scale)]);
        if(c->ext && c->ext->ents)
        {
            if(CollisionInfo ci = mmcollide(d, dir, cutoff, *c->ext->ents); ci.collided)
            {
                return ci;
            }
        }
        scale--;
    }
    if(c->children)
    {
        return ::octacollide(d, dir, cutoff, bo, bs, *c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale);
    }
    bool solid = false;
    switch(c->material&MatFlag_Clip)
    {
        case Mat_NoClip:
        {
            return {false, vec(0,0,0)};
        }
        case Mat_Clip:
        {
            if(IS_CLIPPED(c->material&MatFlag_Volume) || d->type==physent::PhysEnt_Player)
            {
                solid = true;
            }
            break;
        }
    }
    if(!solid && c->isempty())
    {
        return {false, vec(0,0,0)};
    }
    int csize = 2<<scale,
        cmask = ~(csize-1);
    return cubecollide(d, dir, cutoff, *c, ivec(bo).mask(cmask), csize, solid);
}

// all collision happens here
//

//used in iengine
bool collide(const physent *d, vec *cwall, const vec &dir, float cutoff, bool insideplayercol)
{
    collideinside = 0;
    collideplayer = nullptr;
    ivec bo(static_cast<int>(d->o.x-d->radius), static_cast<int>(d->o.y-d->radius), static_cast<int>(d->o.z-d->eyeheight)),
         bs(static_cast<int>(d->o.x+d->radius), static_cast<int>(d->o.y+d->radius), static_cast<int>(d->o.z+d->aboveeye));
    bo.sub(1);
    bs.add(1);  // guard space for rounding errors

    if(CollisionInfo ci = octacollide(d, dir, cutoff, bo, bs, rootworld); ci.collided)
    {
        if(cwall)
        {
            *cwall = ci.collidewall;
        }
        return ci.collided;
    }
    else
    {
        ci = plcollide(d, dir, insideplayercol);
        if(cwall)
        {
            *cwall = ci.collidewall;
        }
        return ci.collided;
    }
}

//used in iengine
void recalcdir(const physent *d, const vec &oldvel, vec &dir)
{
    float speed = oldvel.magnitude();
    if(speed > 1e-6f)
    {
        float step = dir.magnitude();
        dir = d->vel;
        dir.add(d->falling);
        dir.mul(step/speed);
    }
}

//used in iengine
void slideagainst(physent *d, vec &dir, const vec &obstacle, bool foundfloor, bool slidecollide)
{
    vec wall(obstacle);
    if(foundfloor ? wall.z > 0 : slidecollide)
    {
        wall.z = 0;
        if(!wall.iszero())
        {
            wall.normalize();
        }
    }
    vec oldvel(d->vel);
    oldvel.add(d->falling);
    d->vel.project(wall);
    d->falling.project(wall);
    recalcdir(d, oldvel, dir);
}

//used in iengine
void avoidcollision(physent *d, const vec &dir, const physent *obstacle, float space)
{
    float rad = obstacle->radius+d->radius;
    vec bbmin(obstacle->o);
    bbmin.x -= rad;
    bbmin.y -= rad;
    bbmin.z -= obstacle->eyeheight+d->aboveeye;
    bbmin.sub(space);
    vec bbmax(obstacle->o);
    bbmax.x += rad;
    bbmax.y += rad;
    bbmax.z += obstacle->aboveeye+d->eyeheight;
    bbmax.add(space);

    for(int i = 0; i < 3; ++i)
    {
        if(d->o[i] <= bbmin[i] || d->o[i] >= bbmax[i])
        {
            return;
        }
    }

    float mindist = 1e16f;
    for(int i = 0; i < 3; ++i)
    {
        if(dir[i] != 0)
        {
            float dist = ((dir[i] > 0 ? bbmax[i] : bbmin[i]) - d->o[i]) / dir[i];
            mindist = std::min(mindist, dist);
        }
    }
    if(mindist >= 0.0f && mindist < 1e15f)
    {
        d->o.add(static_cast<vec>(dir).mul(mindist));
    }
}

//used in iengine
bool movecamera(physent *pl, const vec &dir, float dist, float stepdist)
{
    int steps = static_cast<int>(ceil(dist/stepdist));
    if(steps <= 0)
    {
        return true;
    }
    vec d(dir);
    d.mul(dist/steps);
    for(int i = 0; i < steps; ++i)
    {
        vec oldpos(pl->o);
        pl->o.add(d);
        if(collide(pl, nullptr, vec(0, 0, 0), 0, false))
        {
            pl->o = oldpos;
            return false;
        }
    }
    return true;
}

static bool droptofloor(vec &o, float radius, float height)
{
    static struct dropent final : physent
    {
        dropent()
        {
            type = PhysEnt_Bounce;
            vel = vec(0, 0, -1);
        }
    } d;
    d.o = o;
    if(!insideworld(d.o))
    {
        if(d.o.z < rootworld.mapsize())
        {
            return false;
        }
        d.o.z = rootworld.mapsize() - 1e-3f;
        if(!insideworld(d.o))
        {
            return false;
        }
    }
    vec v(0.0001f, 0.0001f, -1);
    v.normalize();
    if(rootworld.raycube(d.o, v, rootworld.mapsize()) >= rootworld.mapsize())
    {
        return false;
    }
    d.radius = d.xradius = d.yradius = radius;
    d.eyeheight = height;
    d.aboveeye = radius;
    if(!movecamera(&d, d.vel, rootworld.mapsize(), 1))
    {
        o = d.o;
        return true;
    }
    return false;
}

static float dropheight(const entity &e)
{
    switch(e.type)
    {
        case EngineEnt_Particles:
        case EngineEnt_Mapmodel:
        {
            return 0.0f;
        }
        default:
        {
            return 4.0f;
        }
    }
}

//used in iengine
void dropenttofloor(entity *e)
{
    droptofloor(e->o, 1.0f, dropheight(*e));
}

//used in iengine
void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m)
{
    if(move)
    {
        m.x = move*-std::sin(yaw/RAD);
        m.y = move*std::cos(yaw/RAD);
    }
    else
    {
        m.x = m.y = 0;
    }

    if(pitch)
    {
        m.x *= std::cos(pitch/RAD);
        m.y *= std::cos(pitch/RAD);
        m.z = move*std::sin(pitch/RAD);
    }
    else
    {
        m.z = 0;
    }

    if(strafe)
    {
        m.x += strafe*std::cos(yaw/RAD);
        m.y += strafe*std::sin(yaw/RAD);
    }
}

//used in iengine
bool entinmap(dynent *d, bool avoidplayers)        // brute force but effective way to find a free spawn spot in the map
{
    d->o.z += d->eyeheight; // pos specified is at feet
    vec orig = d->o;
    // try max 100 times
    for(int i = 0; i < 100; ++i)
    {
        if(i)
        {
            d->o = orig;
            d->o.x += (randomint(21)-10)*i/5;  // increasing distance
            d->o.y += (randomint(21)-10)*i/5;
            d->o.z += (randomint(21)-10)*i/5;
        }
        if(!collide(d) && !collideinside)
        {
            if(collideplayer)
            {
                if(!avoidplayers)
                {
                    continue;
                }
                d->o = orig;
                d->resetinterp();
                return false;
            }

            d->resetinterp();
            return true;
        }
    }
    // leave ent at original pos, possibly stuck
    d->o = orig;
    d->resetinterp();
    conoutf(Console_Warn, "can't find entity spawn spot! (%.1f, %.1f, %.1f)", d->o.x, d->o.y, d->o.z);
    return false;
}

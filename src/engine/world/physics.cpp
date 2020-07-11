// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "engine.h"
#include "mpr.h"
#include "raycube.h"

static const int maxclipoffset = 4;
static const int maxclipplanes = 1024;
static const int inairsounddelay = 800; //time before midair players are allowed to land with a "thud"
static clipplanes clipcache[maxclipplanes];
static int clipcacheversion = -maxclipoffset;

clipplanes &getclipbounds(const cube &c, const ivec &o, int size, int offset)
{
    clipplanes &p = clipcache[int(&c - worldroot)&(maxclipplanes-1)];
    if(p.owner != &c || p.version != clipcacheversion+offset)
    {
        p.owner = &c;
        p.version = clipcacheversion+offset;
        genclipbounds(c, o, size, p);
    }
    return p;
}

static inline clipplanes &getclipbounds(const cube &c, const ivec &o, int size, physent *d)
{
    int offset = !(c.visible&0x80) || d->type==PhysEnt_Player ? 0 : 1;
    return getclipbounds(c, o, size, offset);
}

static inline int forceclipplanes(const cube &c, const ivec &o, int size, clipplanes &p)
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

void resetclipplanes()
{
    clipcacheversion += maxclipoffset;
    if(!clipcacheversion)
    {
        memset(clipcache, 0, sizeof(clipcache));
        clipcacheversion = maxclipoffset;
    }
}

/////////////////////////  entity collision  ///////////////////////////////////////////////

// info about collisions
int collideinside; // whether an internal collision happened
physent *collideplayer; // whether the collection hit a player
vec collidewall; // just the normal vectors.

static const float stairheight = 4.1f; //max height in cubits of an allowable step (4 = 0.5m)
static const float floorz = 0.867f; //to be considered a level floor, slope is below this
static const float slopez = 0.5f; //maximum climbable slope
static const float wallz = 0.2f; //steeper than this is considered a wall
static const float jumpvel = 85.0f; //impulse scale for player jump
const float gravity = 200.0f; //downwards force scale

bool ellipseboxcollide(physent *d, const vec &dir, const vec &o, const vec &center, float yaw, float xr, float yr, float hi, float lo)
{
    float below = (o.z+center.z-lo) - (d->o.z+d->aboveeye),
          above = (d->o.z-d->eyeheight) - (o.z+center.z+hi);
    if(below>=0 || above>=0)
    {
        return false;
    }
    vec yo(d->o);
    yo.sub(o);
    yo.rotate_around_z(-yaw*RAD);
    yo.sub(center);

    float dx = clamp(yo.x, -xr, xr) - yo.x,
          dy = clamp(yo.y, -yr, yr) - yo.y,
          dist = sqrtf(dx*dx + dy*dy) - d->radius;
    if(dist < 0)
    {
        int sx = yo.x <= -xr ? -1 : (yo.x >= xr ? 1 : 0),
            sy = yo.y <= -yr ? -1 : (yo.y >= yr ? 1 : 0);
        if(dist > (yo.z < 0 ? below : above) && (sx || sy))
        {
            vec ydir(dir);
            ydir.rotate_around_z(-yaw*RAD);
            if(sx*yo.x - xr > sy*yo.y - yr)
            {
                if(dir.iszero() || sx*ydir.x < -1e-6f)
                {
                    collidewall = vec(sx, 0, 0);
                    collidewall.rotate_around_z(yaw*RAD);
                    return true;
                }
            }
            else if(dir.iszero() || sy*ydir.y < -1e-6f)
            {
                collidewall = vec(0, sy, 0);
                collidewall.rotate_around_z(yaw*RAD);
                return true;
            }
        }
        if(yo.z < 0)
        {
            if(dir.iszero() || (dir.z > 0 && (d->type!=PhysEnt_Player || below >= d->zmargin-(d->eyeheight+d->aboveeye)/4.0f)))
            {
                collidewall = vec(0, 0, -1);
                return true;
            }
        }
        else if(dir.iszero() || (dir.z < 0 && (d->type!=PhysEnt_Player || above >= d->zmargin-(d->eyeheight+d->aboveeye)/3.0f)))
        {
            collidewall = vec(0, 0, 1);
            return true;
        }
        collideinside++;
    }
    return false;
}

bool ellipsecollide(physent *d, const vec &dir, const vec &o, const vec &center, float yaw, float xr, float yr, float hi, float lo)
{
    float below = (o.z+center.z-lo) - (d->o.z+d->aboveeye),
          above = (d->o.z-d->eyeheight) - (o.z+center.z+hi);
    if(below>=0 || above>=0)
    {
        return false;
    }
    vec yo(center);
    yo.rotate_around_z(yaw*RAD);
    yo.add(o);
    float x = yo.x - d->o.x,
          y = yo.y - d->o.y,
          angle = atan2f(y, x),
          dangle = angle-d->yaw*RAD,
          eangle = angle-yaw*RAD,
          dx = d->xradius*cosf(dangle),
          dy = d->yradius*sinf(dangle),
          ex = xr*cosf(eangle),
          ey = yr*sinf(eangle),
          dist = sqrtf(x*x + y*y) - sqrtf(dx*dx + dy*dy) - sqrtf(ex*ex + ey*ey);
    if(dist < 0)
    {
        if(dist > (d->o.z < yo.z ? below : above) && (dir.iszero() || x*dir.x + y*dir.y > 0))
        {
            collidewall = vec(-x, -y, 0).rescale(1);
            return true;
        }
        if(d->o.z < yo.z)
        {
            if(dir.iszero() || (dir.z > 0 && (d->type!=PhysEnt_Player || below >= d->zmargin-(d->eyeheight+d->aboveeye)/4.0f)))
            {
                collidewall = vec(0, 0, -1);
                return true;
            }
        }
        else if(dir.iszero() || (dir.z < 0 && (d->type!=PhysEnt_Player || above >= d->zmargin-(d->eyeheight+d->aboveeye)/3.0f)))
        {
            collidewall = vec(0, 0, 1);
            return true;
        }
        collideinside++;
    }
    return false;
}

#define DYNENTCACHESIZE 1024

static uint dynentframe = 0;

static struct dynentcacheentry
{
    int x, y;
    uint frame;
    vector<physent *> dynents;
} dynentcache[DYNENTCACHESIZE];

void cleardynentcache()
{
    dynentframe++;
    if(!dynentframe || dynentframe == 1)
    {
        for(int i = 0; i < DYNENTCACHESIZE; ++i)
        {
            dynentcache[i].frame = 0;
        }
    }
    if(!dynentframe)
    {
        dynentframe = 1;
    }
}

VARF(dynentsize, 4, 7, 12, cleardynentcache());

#define DYNENTHASH(x, y) (((((x)^(y))<<5) + (((x)^(y))>>5)) & (DYNENTCACHESIZE - 1))

const vector<physent *> &checkdynentcache(int x, int y)
{
    dynentcacheentry &dec = dynentcache[DYNENTHASH(x, y)];
    if(dec.x == x && dec.y == y && dec.frame == dynentframe)
    {
        return dec.dynents;
    }
    dec.x = x;
    dec.y = y;
    dec.frame = dynentframe;
    dec.dynents.shrink(0);
    int numdyns = game::numdynents(),
        dsize = 1<<dynentsize,
        dx = x<<dynentsize,
        dy = y<<dynentsize;
    for(int i = 0; i < numdyns; ++i)
    {
        dynent *d = game::iterdynents(i);
        if(d->state != ClientState_Alive ||
           d->o.x+d->radius <= dx || d->o.x-d->radius >= dx+dsize ||
           d->o.y+d->radius <= dy || d->o.y-d->radius >= dy+dsize)
        {
            continue;
        }
        dec.dynents.add(d);
    }
    return dec.dynents;
}

//============================================================== LOOPDYNENTCACHE
#define LOOPDYNENTCACHE(curx, cury, o, radius) \
    for(int curx = max(static_cast<int>(o.x-radius), 0)>>dynentsize, endx = min(static_cast<int>(o.x+radius), worldsize-1)>>dynentsize; curx <= endx; curx++) \
    for(int cury = max(static_cast<int>(o.y-radius), 0)>>dynentsize, endy = min(static_cast<int>(o.y+radius), worldsize-1)>>dynentsize; cury <= endy; cury++)

void updatedynentcache(physent *d)
{
    LOOPDYNENTCACHE(x, y, d->o, d->radius)
    {
        dynentcacheentry &dec = dynentcache[DYNENTHASH(x, y)];
        if(dec.x != x || dec.y != y || dec.frame != dynentframe || dec.dynents.find(d) >= 0)
        {
            continue;
        }
        dec.dynents.add(d);
    }
}

bool overlapsdynent(const vec &o, float radius)
{
    LOOPDYNENTCACHE(x, y, o, radius)
    {
        const vector<physent *> &dynents = checkdynentcache(x, y);
        for(int i = 0; i < dynents.length(); i++)
        {
            physent *d = dynents[i];
            if(o.dist(d->o)-d->radius < radius)
            {
                return true;
            }
        }
    }
    return false;
}

template<class E, class O>
static inline bool plcollide(physent *d, const vec &dir, physent *o)
{
    E entvol(d);
    O obvol(o);
    vec cp;
    if(mpr::collide(entvol, obvol, NULL, NULL, &cp))
    {
        vec wn = vec(cp).sub(obvol.center());
        collidewall = obvol.contactface(wn, dir.iszero() ? vec(wn).neg() : dir);
        if(!collidewall.iszero())
        {
            return true;
        }
        collideinside++;
    }
    return false;
}

static inline bool plcollide(physent *d, const vec &dir, physent *o)
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
                return plcollide<mpr::EntOBB, mpr::EntCylinder>(d, dir, o);
            }
            else
            {
                return plcollide<mpr::EntOBB, mpr::EntOBB>(d, dir, o);
            }
        }
        default:
        {
            return false;
        }
    }
}

bool plcollide(physent *d, const vec &dir, bool insideplayercol)    // collide with player
{
    if(d->type==PhysEnt_Camera || d->state!=ClientState_Alive)
    {
        return false;
    }
    int lastinside = collideinside;
    physent *insideplayer = NULL;
    LOOPDYNENTCACHE(x, y, d->o, d->radius)
    {
        const vector<physent *> &dynents = checkdynentcache(x, y);
        for(int i = 0; i < dynents.length(); i++)
        {
            physent *o = dynents[i];
            if(o==d || d->o.reject(o->o, d->radius+o->radius))
            {
                continue;
            }
            if(plcollide(d, dir, o))
            {   
                collideplayer = o;
                return true;
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
        return true;
    }
    return false;
}

#undef LOOPDYNENTCACHE
//==============================================================================

void rotatebb(vec &center, vec &radius, int yaw, int pitch, int roll)
{
    matrix3 orient;
    orient.identity();
    if(yaw)
    {
        orient.rotate_around_z(sincosmod360(yaw));
    }
    if(pitch)
    {
        orient.rotate_around_x(sincosmod360(pitch));
    }
    if(roll)
    {
        orient.rotate_around_y(sincosmod360(-roll));
    }
    center = orient.transform(center);
    radius = orient.abstransform(radius);
}

template<class E, class M>
static inline bool mmcollide(physent *d, const vec &dir, const extentity &e, const vec &center, const vec &radius, int yaw, int pitch, int roll)
{
    E entvol(d);
    M mdlvol(e.o, center, radius, yaw, pitch, roll);
    vec cp;
    if(mpr::collide(entvol, mdlvol, NULL, NULL, &cp))
    {
        vec wn = vec(cp).sub(mdlvol.center());
        collidewall = mdlvol.contactface(wn, dir.iszero() ? vec(wn).neg() : dir);
        if(!collidewall.iszero())
        {
            return true;
        }
        collideinside++;
    }
    return false;
}

template<class E>
static bool fuzzycollidebox(physent *d, const vec &dir, float cutoff, const vec &o, const vec &center, const vec &radius, int yaw, int pitch, int roll)
{
    mpr::ModelOBB mdlvol(o, center, radius, yaw, pitch, roll);
    vec bbradius = mdlvol.orient.abstransposedtransform(radius);
    if(fabs(d->o.x - mdlvol.o.x) > bbradius.x + d->radius || fabs(d->o.y - mdlvol.o.y) > bbradius.y + d->radius ||
       d->o.z + d->aboveeye < mdlvol.o.z - bbradius.z || d->o.z - d->eyeheight > mdlvol.o.z + bbradius.z)
    {
        return false;
    }
    E entvol(d);
    collidewall = vec(0, 0, 0);
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
        vec pw = entvol.supportpoint(vec(w).neg());
        dist += w.dot(vec(pw).sub(mdlvol.o));
        if(dist >= 0)
        {
            return false;
        }
        if(dist <= bestdist)
        {
            continue;
        }
        collidewall = vec(0, 0, 0);
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude())
            {
                continue;
            }
            //nasty ternary in the indented part
            if(d->type==PhysEnt_Player &&
                    dist < (dir.z*w.z < 0 ?
                    d->zmargin-(d->eyeheight+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                    (dir.x*w.x < 0 || dir.y*w.y < 0 ? -d->radius : 0)))
            {
                continue;
            }
        }
        collidewall = w;
    }
    if(collidewall.iszero())
    {
        collideinside++;
        return false;
    }
    return true;
}

template<class E>
static bool fuzzycollideellipse(physent *d, const vec &dir, float cutoff, const vec &o, const vec &center, const vec &radius, int yaw, int pitch, int roll)
{
    mpr::ModelEllipse mdlvol(o, center, radius, yaw, pitch, roll);
    vec bbradius = mdlvol.orient.abstransposedtransform(radius);

    if(fabs(d->o.x - mdlvol.o.x) > bbradius.x + d->radius || fabs(d->o.y - mdlvol.o.y) > bbradius.y + d->radius ||
       d->o.z + d->aboveeye < mdlvol.o.z - bbradius.z || d->o.z - d->eyeheight > mdlvol.o.z + bbradius.z)
        return false;

    E entvol(d);
    collidewall = vec(0, 0, 0);
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
                w = mdlvol.orient.rowz(); dist = -radius.z;
                break;
            }
            case 1:
            {
                w = mdlvol.orient.rowz().neg(); dist = -radius.z;
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
            return false;
        }
        if(dist <= bestdist)
        {
            continue;
        }
        collidewall = vec(0, 0, 0);
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude())
            {
                continue;
            }
            if(d->type==PhysEnt_Player &&
                dist < (dir.z*w.z < 0 ?
                    d->zmargin-(d->eyeheight+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                    (dir.x*w.x < 0 || dir.y*w.y < 0 ? -d->radius : 0)))
            {
                continue;
            }
        }
        collidewall = w;
    }
    if(collidewall.iszero())
    {
        collideinside++;
        return false;
    }
    return true;
}

VAR(testtricol, 0, 0, 2);

bool mmcollide(physent *d, const vec &dir, float cutoff, octaentities &oc) // collide with a mapmodel
{
    const vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < oc.mapmodels.length(); i++)
    {
        extentity &e = *ents[oc.mapmodels[i]];
        if(e.flags&EntFlag_NoCollide || !mapmodels.inrange(e.attr1))
        {
            continue;
        }
        mapmodelinfo &mmi = mapmodels[e.attr1];
        model *m = mmi.collide;
        if(!m)
        {
            if(!mmi.m && !loadmodel(NULL, e.attr1))
            {
                continue;
            }
            if(mmi.m->collidemodel)
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
            if(!m->bih && !m->setBIH())
            {
                continue;
            }
            switch(testtricol ? testtricol : d->collidetype)
            {
                case Collide_Ellipse:
                {
                    if(m->bih->ellipsecollide(d, dir, cutoff, e.o, yaw, pitch, roll, scale))
                    {
                        return true;
                    }
                    break;
                }
                case Collide_OrientedBoundingBox:
                {
                    if(m->bih->boxcollide(d, dir, cutoff, e.o, yaw, pitch, roll, scale))
                    {
                        return true;
                    }
                    break;
                }
                default: continue;
            }
        }
        else
        {
            radius.mul(scale);
            switch(d->collidetype)
            {
                case Collide_Ellipse:
                    if(mcol == Collide_Ellipse)
                    {
                        if(pitch || roll)
                        {
                            if(fuzzycollideellipse<mpr::EntCapsule>(d, dir, cutoff, e.o, center, radius, yaw, pitch, roll))
                            {
                                return true;
                            }
                        }
                        else if(ellipsecollide(d, dir, e.o, center, yaw, radius.x, radius.y, radius.z, radius.z))
                        {
                            return true;
                        }
                    }
                    else if(pitch || roll)
                    {
                        if(fuzzycollidebox<mpr::EntCapsule>(d, dir, cutoff, e.o, center, radius, yaw, pitch, roll))
                        {
                            return true;
                        }
                    }
                    else if(ellipseboxcollide(d, dir, e.o, center, yaw, radius.x, radius.y, radius.z, radius.z))
                    {
                        return true;
                    }
                    break;
                case Collide_OrientedBoundingBox:
                    if(mcol == Collide_Ellipse)
                    {
                        if(mmcollide<mpr::EntOBB, mpr::ModelEllipse>(d, dir, e, center, radius, yaw, pitch, roll))
                        {
                            return true;
                        }
                    }
                    else if(mmcollide<mpr::EntOBB, mpr::ModelOBB>(d, dir, e, center, radius, yaw, pitch, roll))
                    {
                        return true;
                    }
                    break;
                default:
                {
                    continue;
                }
            }
        }
    }
    return false;
}

template<class E>
static bool fuzzycollidesolid(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with solid cube geometry
{
    int crad = size/2;
    if(fabs(d->o.x - co.x - crad) > d->radius + crad || fabs(d->o.y - co.y - crad) > d->radius + crad ||
       d->o.z + d->aboveeye < co.z || d->o.z - d->eyeheight > co.z + size)
    {
        return false;
    }
    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    int visible = !(c.visible&0x80) || d->type==PhysEnt_Player ? c.visible : 0xFF;
    #define CHECKSIDE(side, distval, dotval, margin, normal) \
        if(visible&(1<<side)) \
        { \
            do \
            { \
                float dist = distval; \
                if(dist > 0) \
                { \
                    return false; \
                } \
                if(dist <= bestdist) \
                { \
                    continue; \
                } \
                if(!dir.iszero()) \
                { \
                    if(dotval >= -cutoff*dir.magnitude()) \
                    { \
                        continue; \
                    } \
                    if(d->type==PhysEnt_Player && dotval < 0 && dist < margin) \
                    { \
                        continue; \
                    } \
                } \
                collidewall = normal; \
                bestdist = dist; \
            } while(0); \
        }
    CHECKSIDE(Orient_Left, co.x - (d->o.x + d->radius), -dir.x, -d->radius, vec(-1, 0, 0));
    CHECKSIDE(Orient_Right, d->o.x - d->radius - (co.x + size), dir.x, -d->radius, vec(1, 0, 0));
    CHECKSIDE(Orient_Back, co.y - (d->o.y + d->radius), -dir.y, -d->radius, vec(0, -1, 0));
    CHECKSIDE(Orient_Front, d->o.y - d->radius - (co.y + size), dir.y, -d->radius, vec(0, 1, 0));
    CHECKSIDE(Orient_Bottom, co.z - (d->o.z + d->aboveeye), -dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/4.0f, vec(0, 0, -1));
    CHECKSIDE(Orient_Top, d->o.z - d->eyeheight - (co.z + size), dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/3.0f, vec(0, 0, 1));
    if(collidewall.iszero())
    {
        collideinside++;
        return false;
    }
    return true;
}

template<class E>
static inline bool clampcollide(const clipplanes &p, const E &entvol, const plane &w, const vec &pw)
{
    if(w.x && (w.y || w.z) && fabs(pw.x - p.o.x) > p.r.x)
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
    if(w.y && (w.x || w.z) && fabs(pw.y - p.o.y) > p.r.y)
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
    if(w.z && (w.x || w.y) && fabs(pw.z - p.o.z) > p.r.z)
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

template<class E>
static bool fuzzycollideplanes(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with deformed cube geometry
{
    clipplanes &p = getclipbounds(c, co, size, d);

    if(fabs(d->o.x - p.o.x) > p.r.x + d->radius || fabs(d->o.y - p.o.y) > p.r.y + d->radius ||
       d->o.z + d->aboveeye < p.o.z - p.r.z || d->o.z - d->eyeheight > p.o.z + p.r.z)
    {
        return false;
    }
    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    int visible = forceclipplanes(c, co, size, p);
    CHECKSIDE(Orient_Left, p.o.x - p.r.x - (d->o.x + d->radius), -dir.x, -d->radius, vec(-1, 0, 0));
    CHECKSIDE(Orient_Right, d->o.x - d->radius - (p.o.x + p.r.x), dir.x, -d->radius, vec(1, 0, 0));
    CHECKSIDE(Orient_Back, p.o.y - p.r.y - (d->o.y + d->radius), -dir.y, -d->radius, vec(0, -1, 0));
    CHECKSIDE(Orient_Front, d->o.y - d->radius - (p.o.y + p.r.y), dir.y, -d->radius, vec(0, 1, 0));
    CHECKSIDE(Orient_Bottom, p.o.z - p.r.z - (d->o.z + d->aboveeye), -dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/4.0f, vec(0, 0, -1));
    CHECKSIDE(Orient_Top, d->o.z - d->eyeheight - (p.o.z + p.r.z), dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/3.0f, vec(0, 0, 1));

    E entvol(d);
    int bestplane = -1;
    for(int i = 0; i < p.size; ++i)
    {
        const plane &w = p.p[i];
        vec pw = entvol.supportpoint(vec(w).neg());
        float dist = w.dist(pw);
        if(dist >= 0)
        {
            return false;
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
            if(d->type==PhysEnt_Player &&
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
        collidewall = p.p[bestplane];
    }
    else if(collidewall.iszero())
    {
        collideinside++;
        return false;
    }
    return true;
}

template<class E>
static bool cubecollidesolid(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with solid cube geometry
{
    int crad = size/2;
    if(fabs(d->o.x - co.x - crad) > d->radius + crad || fabs(d->o.y - co.y - crad) > d->radius + crad ||
       d->o.z + d->aboveeye < co.z || d->o.z - d->eyeheight > co.z + size)
    {
        return false;
    }
    E entvol(d);
    bool collided = mpr::collide(mpr::SolidCube(co, size), entvol);
    if(!collided)
    {
        return false;
    }
    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    int visible = !(c.visible&0x80) || d->type==PhysEnt_Player ? c.visible : 0xFF;
    CHECKSIDE(Orient_Left, co.x - entvol.right(), -dir.x, -d->radius, vec(-1, 0, 0));
    CHECKSIDE(Orient_Right, entvol.left() - (co.x + size), dir.x, -d->radius, vec(1, 0, 0));
    CHECKSIDE(Orient_Back, co.y - entvol.front(), -dir.y, -d->radius, vec(0, -1, 0));
    CHECKSIDE(Orient_Front, entvol.back() - (co.y + size), dir.y, -d->radius, vec(0, 1, 0));
    CHECKSIDE(Orient_Bottom, co.z - entvol.top(), -dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/4.0f, vec(0, 0, -1));
    CHECKSIDE(Orient_Top, entvol.bottom() - (co.z + size), dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/3.0f, vec(0, 0, 1));
    if(collidewall.iszero())
    {
        collideinside++;
        return false;
    }
    return true;
}

template<class E>
static bool cubecollideplanes(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with deformed cube geometry
{
    clipplanes &p = getclipbounds(c, co, size, d);
    if(fabs(d->o.x - p.o.x) > p.r.x + d->radius || fabs(d->o.y - p.o.y) > p.r.y + d->radius ||
       d->o.z + d->aboveeye < p.o.z - p.r.z || d->o.z - d->eyeheight > p.o.z + p.r.z)
    {
        return false;
    }
    E entvol(d);
    bool collided = mpr::collide(mpr::CubePlanes(p), entvol);
    if(!collided)
    {
        return false;
    }
    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    int visible = forceclipplanes(c, co, size, p);
    CHECKSIDE(Orient_Left, p.o.x - p.r.x - entvol.right(), -dir.x, -d->radius, vec(-1, 0, 0));
    CHECKSIDE(Orient_Right, entvol.left() - (p.o.x + p.r.x), dir.x, -d->radius, vec(1, 0, 0));
    CHECKSIDE(Orient_Back, p.o.y - p.r.y - entvol.front(), -dir.y, -d->radius, vec(0, -1, 0));
    CHECKSIDE(Orient_Front, entvol.back() - (p.o.y + p.r.y), dir.y, -d->radius, vec(0, 1, 0));
    CHECKSIDE(Orient_Bottom, p.o.z - p.r.z - entvol.top(), -dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/4.0f, vec(0, 0, -1));
    CHECKSIDE(Orient_Top, entvol.bottom() - (p.o.z + p.r.z), dir.z, d->zmargin-(d->eyeheight+d->aboveeye)/3.0f, vec(0, 0, 1));

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
            if(d->type==PhysEnt_Player &&
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

    if(bestplane >= 0) collidewall = p.p[bestplane];
    else if(collidewall.iszero())
    {
        collideinside++;
        return false;
    }
    return true;
}

static inline bool cubecollide(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size, bool solid)
{
    switch(d->collidetype)
    {
        case Collide_OrientedBoundingBox:
        {
            if(IS_ENTIRELY_SOLID(c) || solid)
            {
                return cubecollidesolid<mpr::EntOBB>(d, dir, cutoff, c, co, size);
            }
            else
            {
                return cubecollideplanes<mpr::EntOBB>(d, dir, cutoff, c, co, size);
            }
        }
        case Collide_Ellipse:
        {
            if(IS_ENTIRELY_SOLID(c) || solid)
            {
                return fuzzycollidesolid<mpr::EntCapsule>(d, dir, cutoff, c, co, size);
            }
            else
            {
                return fuzzycollideplanes<mpr::EntCapsule>(d, dir, cutoff, c, co, size);
            }
        }
        default:
        {
            return false;
        }
    }
}

static inline bool octacollide(physent *d, const vec &dir, float cutoff, const ivec &bo, const ivec &bs, const cube *c, const ivec &cor, int size) // collide with octants
{
    LOOP_OCTA_BOX(cor, size, bo, bs)
    {
        if(c[i].ext && c[i].ext->ents) if(mmcollide(d, dir, cutoff, *c[i].ext->ents))
        {
            return true;
        }
        ivec o(i, cor, size);
        if(c[i].children)
        {
            if(octacollide(d, dir, cutoff, bo, bs, c[i].children, o, size>>1))
            {
                return true;
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
                    if(IS_CLIPPED(c[i].material&MatFlag_Volume) || d->type==PhysEnt_Player)
                    {
                        solid = true;
                    }
                    break;
                }
            }
            if(!solid && IS_EMPTY(c[i]))
            {
                continue;
            }
            if(cubecollide(d, dir, cutoff, c[i], o, size, solid))
            {
                return true;
            }
        }
    }
    return false;
}

static inline bool octacollide(physent *d, const vec &dir, float cutoff, const ivec &bo, const ivec &bs)
{
    int diff = (bo.x^bs.x) | (bo.y^bs.y) | (bo.z^bs.z),
        scale = worldscale-1;
    if(diff&~((1<<scale)-1) || static_cast<uint>(bo.x|bo.y|bo.z|bs.x|bs.y|bs.z) >= static_cast<uint>(worldsize))
    {
       return octacollide(d, dir, cutoff, bo, bs, worldroot, ivec(0, 0, 0), worldsize>>1);
   }
    const cube *c = &worldroot[OCTA_STEP(bo.x, bo.y, bo.z, scale)];
    if(c->ext && c->ext->ents && mmcollide(d, dir, cutoff, *c->ext->ents))
    {
        return true;
    }
    scale--;
    while(c->children && !(diff&(1<<scale)))
    {
        c = &c->children[OCTA_STEP(bo.x, bo.y, bo.z, scale)];
        if(c->ext && c->ext->ents && mmcollide(d, dir, cutoff, *c->ext->ents))
        {
            return true;
        }
        scale--;
    }
    if(c->children)
    {
        return octacollide(d, dir, cutoff, bo, bs, c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale);
    }
    bool solid = false;
    switch(c->material&MatFlag_Clip)
    {
        case Mat_NoClip:
        {
            return false;
        }
        case Mat_Clip:
        {
            if(IS_CLIPPED(c->material&MatFlag_Volume) || d->type==PhysEnt_Player)
            {
                solid = true;
            }
            break;
        }
    }
    if(!solid && IS_EMPTY(*c))
    {
        return false;
    }
    int csize = 2<<scale,
        cmask = ~(csize-1);
    return cubecollide(d, dir, cutoff, *c, ivec(bo).mask(cmask), csize, solid);
}

// all collision happens here
bool collide(physent *d, const vec &dir, float cutoff, bool playercol, bool insideplayercol)
{
    collideinside = 0;
    collideplayer = NULL;
    collidewall = vec(0, 0, 0);
    ivec bo(static_cast<int>(d->o.x-d->radius), static_cast<int>(d->o.y-d->radius), static_cast<int>(d->o.z-d->eyeheight)),
         bs(static_cast<int>(d->o.x+d->radius), static_cast<int>(d->o.y+d->radius), static_cast<int>(d->o.z+d->aboveeye));
    bo.sub(1); bs.add(1);  // guard space for rounding errors
    return octacollide(d, dir, cutoff, bo, bs) || (playercol && plcollide(d, dir, insideplayercol)); // collide with world
}

void recalcdir(physent *d, const vec &oldvel, vec &dir)
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

void switchfloor(physent *d, vec &dir, const vec &floor)
{
    if(floor.z >= floorz)
    {
        d->falling = vec(0, 0, 0);
    }
    vec oldvel(d->vel);
    oldvel.add(d->falling);
    if(dir.dot(floor) >= 0)
    {
        if(d->physstate < PhysEntState_Slide || fabs(dir.dot(d->floor)) > 0.01f*dir.magnitude())
        {
            return;
        }
        d->vel.projectxy(floor, 0.0f);
    }
    else
    {
        d->vel.projectxy(floor);
    }
    d->falling.project(floor);
    recalcdir(d, oldvel, dir);
}

bool trystepup(physent *d, vec &dir, const vec &obstacle, float maxstep, const vec &floor)
{
    vec old(d->o),
        stairdir = (obstacle.z >= 0 && obstacle.z < slopez ? vec(-obstacle.x, -obstacle.y, 0) : vec(dir.x, dir.y, 0)).rescale(1);
    bool cansmooth = true;
    /* check if there is space atop the stair to move to */
    if(d->physstate != PhysEntState_StepUp)
    {
        vec checkdir = stairdir;
        checkdir.mul(0.1f);
        checkdir.z += maxstep + 0.1f;
        d->o.add(checkdir);
        if(collide(d))
        {
            d->o = old;
            if(!collide(d, vec(0, 0, -1), slopez))
            {
                return false;
            }
            cansmooth = false;
        }
    }

    if(cansmooth)
    {
        vec checkdir = stairdir;
        checkdir.z += 1;
        checkdir.mul(maxstep);
        d->o = old;
        d->o.add(checkdir);
        int scale = 2;
        if(collide(d, checkdir))
        {
            if(!collide(d, vec(0, 0, -1), slopez))
            {
                d->o = old;
                return false;
            }
            d->o.add(checkdir);
            if(collide(d, vec(0, 0, -1), slopez))
            {
                scale = 1;
            }
        }
        if(scale != 1)
        {
            d->o = old;
            d->o.sub(checkdir.mul(vec(2, 2, 1)));
            if(!collide(d, vec(0, 0, -1), slopez))
            {
                scale = 1;
            }
        }

        d->o = old;
        vec smoothdir(dir.x, dir.y, 0);
        float magxy = smoothdir.magnitude();
        if(magxy > 1e-9f)
        {
            if(magxy > scale*dir.z)
            {
                smoothdir.mul(1/magxy);
                smoothdir.z = 1.0f/scale;
                smoothdir.mul(dir.magnitude()/smoothdir.magnitude());
            }
            else
            {
                smoothdir.z = dir.z;
            }
            d->o.add(smoothdir);
            d->o.z += maxstep + 0.1f;
            if(!collide(d, smoothdir))
            {
                d->o.z -= maxstep + 0.1f;
                if(d->physstate == PhysEntState_Fall || d->floor != floor)
                {
                    d->timeinair = 0;
                    d->floor = floor;
                    switchfloor(d, dir, d->floor);
                }
                d->physstate = PhysEntState_StepUp;
                return true;
            }
        }
    }

    /* try stepping up */
    d->o = old;
    d->o.z += dir.magnitude();
    if(!collide(d, vec(0, 0, 1)))
    {
        if(d->physstate == PhysEntState_Fall || d->floor != floor)
        {
            d->timeinair = 0;
            d->floor = floor;
            switchfloor(d, dir, d->floor);
        }
        if(cansmooth)
        {
            d->physstate = PhysEntState_StepUp;
        }
        return true;
    }
    d->o = old;
    return false;
}

bool trystepdown(physent *d, vec &dir, float step, float xy, float z, bool init = false)
{
    vec stepdir(dir.x, dir.y, 0);
    stepdir.z = -stepdir.magnitude2()*z/xy;
    if(!stepdir.z)
    {
        return false;
    }
    stepdir.normalize();

    vec old(d->o);
    d->o.add(vec(stepdir).mul(stairheight/fabs(stepdir.z))).z -= stairheight;
    d->zmargin = -stairheight;
    if(collide(d, vec(0, 0, -1), slopez))
    {
        d->o = old;
        d->o.add(vec(stepdir).mul(step));
        d->zmargin = 0;
        if(!collide(d, vec(0, 0, -1)))
        {
            vec stepfloor(stepdir);
            stepfloor.mul(-stepfloor.z).z += 1;
            stepfloor.normalize();
            if(d->physstate >= PhysEntState_Slope && d->floor != stepfloor)
            {
                // prevent alternating step-down/step-up states if player would keep bumping into the same floor
                vec stepped(d->o);
                d->o.z -= 0.5f;
                d->zmargin = -0.5f;
                if(collide(d, stepdir) && collidewall == d->floor)
                {
                    d->o = old;
                    if(!init)
                    {
                        d->o.x += dir.x;
                        d->o.y += dir.y;
                        if(dir.z <= 0 || collide(d, dir))
                        {
                            d->o.z += dir.z;
                        }
                    }
                    d->zmargin = 0;
                    d->physstate = PhysEntState_StepDown;
                    d->timeinair = 0;
                    return true;
                }
                d->o = init ? old : stepped;
                d->zmargin = 0;
            }
            else if(init)
            {
                d->o = old;
            }
            switchfloor(d, dir, stepfloor);
            d->floor = stepfloor;
            d->physstate = PhysEntState_StepDown;
            d->timeinair = 0;
            return true;
        }
    }
    d->o = old;
    d->zmargin = 0;
    return false;
}

bool trystepdown(physent *d, vec &dir, bool init = false)
{
    if((!d->move && !d->strafe) || !game::allowmove(d))
    {
        return false;
    }
    vec old(d->o);
    d->o.z -= stairheight;
    d->zmargin = -stairheight;
    if(!collide(d, vec(0, 0, -1), slopez))
    {
        d->o = old;
        d->zmargin = 0;
        return false;
    }
    d->o = old;
    d->zmargin = 0;
    float step = dir.magnitude();
    // weaker check, just enough to avoid hopping up slopes
    if(trystepdown(d, dir, step, 4, 1, init))
    {
        return true;
    }
    return false;
}

void falling(physent *d, vec &dir, const vec &floor)
{
    if(floor.z > 0.0f && floor.z < slopez)
    {
        if(floor.z >= wallz)
        {
            switchfloor(d, dir, floor);
        }
        d->timeinair = 0;
        d->physstate = PhysEntState_Slide;
        d->floor = floor;
    }
    else if(d->physstate < PhysEntState_Slope || dir.dot(d->floor) > 0.01f*dir.magnitude() || (floor.z != 0.0f && floor.z != 1.0f) || !trystepdown(d, dir, true))
    {
        d->physstate = PhysEntState_Fall;
    }
}

void landing(physent *d, vec &dir, const vec &floor, bool collided)
{

    switchfloor(d, dir, floor);
    d->timeinair = 0;
    if((d->physstate!=PhysEntState_StepUp && d->physstate!=PhysEntState_StepDown) || !collided)
    {
        d->physstate = floor.z >= floorz ? PhysEntState_Floor : PhysEntState_Slope;
    }
    d->floor = floor;
}

bool findfloor(physent *d, const vec &dir, bool collided, const vec &obstacle, bool &slide, vec &floor)
{
    bool found = false;
    vec moved(d->o);
    d->o.z -= 0.1f;
    if(collide(d, vec(0, 0, -1), d->physstate == PhysEntState_Slope || d->physstate == PhysEntState_StepDown ? slopez : floorz))
    {
        if(d->physstate == PhysEntState_StepUp && d->floor != collidewall)
        {
            vec old(d->o), checkfloor(collidewall), checkdir = vec(dir).projectxydir(checkfloor).rescale(dir.magnitude());
            d->o.add(checkdir);
            if(!collide(d, checkdir))
            {
                floor = checkfloor;
                found = true;
                goto foundfloor;
            }
            d->o = old;
        }
        else
        {
            floor = collidewall;
            found = true;
            goto foundfloor;
        }
    }
    if(collided && obstacle.z >= slopez)
    {
        floor = obstacle;
        found = true;
        slide = false;
    }
    else if(d->physstate == PhysEntState_StepUp || d->physstate == PhysEntState_Slide)
    {
        if(collide(d, vec(0, 0, -1)) && collidewall.z > 0.0f)
        {
            floor = collidewall;
            if(floor.z >= slopez)
            {
                found = true;
            }
        }
    }
    else if(d->physstate >= PhysEntState_Slope && d->floor.z < 1.0f)
    {
        if(collide(d, vec(d->floor).neg(), 0.95f) || collide(d, vec(0, 0, -1)))
        {
            floor = collidewall;
            if(floor.z >= slopez && floor.z < 1.0f)
            {
                found = true;
            }
        }
    }
foundfloor:
    if(collided && (!found || obstacle.z > floor.z))
    {
        floor = obstacle;
        slide = !found && (floor.z < wallz || floor.z >= slopez);
    }
    d->o = moved;
    return found;
}

bool move(physent *d, vec &dir)
{
    vec old(d->o);
    bool collided = false,
         slidecollide = false;
    vec obstacle;
    d->o.add(dir);
    if(collide(d, dir))
    {
        obstacle = collidewall;
        /* check to see if there is an obstacle that would prevent this one from being used as a floor (or ceiling bump) */
        if(d->type==PhysEnt_Player && ((collidewall.z>=slopez && dir.z<0) || (collidewall.z<=-slopez && dir.z>0)) && (dir.x || dir.y) && collide(d, vec(dir.x, dir.y, 0)))
        {
            if(collidewall.dot(dir) >= 0)
            {
                slidecollide = true;
            }
            obstacle = collidewall;
        }
        d->o = old;
        d->o.z -= stairheight;
        d->zmargin = -stairheight;
        if(d->physstate == PhysEntState_Slope || d->physstate == PhysEntState_Floor || (collide(d, vec(0, 0, -1), slopez) && (d->physstate==PhysEntState_StepUp || d->physstate==PhysEntState_StepDown || collidewall.z>=floorz)))
        {
            d->o = old;
            d->zmargin = 0;
            if(trystepup(d, dir, obstacle, stairheight, d->physstate == PhysEntState_Slope || d->physstate == PhysEntState_Floor ? d->floor : vec(collidewall)))
            {
                return true;
            }
        }
        else
        {
            d->o = old;
            d->zmargin = 0;
        }
        /* can't step over the obstacle, so just slide against it */
        collided = true;
    }
    else if(d->physstate == PhysEntState_StepUp)
    {
        if(collide(d, vec(0, 0, -1), slopez))
        {
            d->o = old;
            if(trystepup(d, dir, vec(0, 0, 1), stairheight, vec(collidewall)))
            {
                return true;
            }
            d->o.add(dir);
        }
    }
    else if(d->physstate == PhysEntState_StepDown && dir.dot(d->floor) <= 1e-6f)
    {
        vec moved(d->o);
        d->o = old;
        if(trystepdown(d, dir))
        {
            return true;
        }
        d->o = moved;
    }
    vec floor(0, 0, 0);
    bool slide = collided,
         found = findfloor(d, dir, collided, obstacle, slide, floor);
    if(slide || (!collided && floor.z > 0 && floor.z < wallz))
    {
        slideagainst(d, dir, slide ? obstacle : floor, found, slidecollide);
        d->blocked = true;
    }
    if(found)
    {
        landing(d, dir, floor, collided);
    }
    else
    {
        falling(d, dir, floor);
    }
    return !collided;
}

void crouchplayer(physent *pl, int moveres, bool local)
{
    if(!curtime)
    {
        return;
    }
    float minheight = pl->maxheight * crouchheight, speed = (pl->maxheight - minheight) * curtime / static_cast<float>(crouchtime);
    if(pl->crouching < 0)
    {
        if(pl->eyeheight > minheight)
        {
            float diff = min(pl->eyeheight - minheight, speed);
            pl->eyeheight -= diff;
            if(pl->physstate > PhysEntState_Fall)
            {
                pl->o.z -= diff;
                pl->newpos.z -= diff;
            }
        }
    }
    else if(pl->eyeheight < pl->maxheight)
    {
        float diff = min(pl->maxheight - pl->eyeheight, speed),
              step = diff/moveres;
        pl->eyeheight += diff;
        if(pl->physstate > PhysEntState_Fall)
        {
            pl->o.z += diff;
            pl->newpos.z += diff;
        }
        pl->crouching = 0;
        for(int i = 0; i < moveres; ++i)
        {
            if(!collide(pl, vec(0, 0, pl->physstate <= PhysEntState_Fall ? -1 : 1), 0, true))
            {
                break;
            }
            pl->crouching = 1;
            pl->eyeheight -= step;
            if(pl->physstate > PhysEntState_Fall)
            {
                pl->o.z -= step;
                pl->newpos.z -= step;
            }
        }
    }
}

bool bounce(physent *d, float secs, float elasticity, float waterfric, float grav)
{
    // make sure bouncers don't start inside geometry
    if(d->physstate!=PhysEntState_Bounce && collide(d, vec(0, 0, 0), 0, false))
    {
        return true;
    }
    int mat = lookupmaterial(vec(d->o.x, d->o.y, d->o.z + (d->aboveeye - d->eyeheight)/2));
    bool water = IS_LIQUID(mat);
    if(water)
    {
        d->vel.z -= grav*gravity/16*secs;
        d->vel.mul(max(1.0f - secs/waterfric, 0.0f));
    }
    else
    {
        d->vel.z -= grav*gravity*secs;
    }
    vec old(d->o);
    for(int i = 0; i < 2; ++i)
    {
        vec dir(d->vel);
        dir.mul(secs);
        d->o.add(dir);
        if(!collide(d, dir, 0, true, true))
        {
            if(collideinside)
            {
                d->o = old;
                d->vel.mul(-elasticity);
            }
            break;
        }
        else if(collideplayer)
        {
            break;
        }
        d->o = old;
        game::bounced(d, collidewall);
        float c = collidewall.dot(d->vel),
              k = 1.0f + (1.0f-elasticity)*c/d->vel.magnitude();
        d->vel.mul(k);
        d->vel.sub(vec(collidewall).mul(elasticity*2.0f*c));
    }
    if(d->physstate!=PhysEntState_Bounce)
    {
        // make sure bouncers don't start inside geometry
        if(d->o == old)
        {
            return !collideplayer;
        }
        d->physstate = PhysEntState_Bounce;
    }
    return collideplayer!=NULL;
}

void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space)
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
            mindist = min(mindist, dist);
        }
    }
    if(mindist >= 0.0f && mindist < 1e15f)
    {
        d->o.add(vec(dir).mul(mindist));
    }
}

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
        if(collide(pl, vec(0, 0, 0), 0, false))
        {
            pl->o = oldpos;
            return false;
        }
    }
    return true;
}

bool droptofloor(vec &o, float radius, float height)
{
    static struct dropent : physent
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
        if(d.o.z < worldsize)
        {
            return false;
        }
        d.o.z = worldsize - 1e-3f;
        if(!insideworld(d.o))
        {
            return false;
        }
    }
    vec v(0.0001f, 0.0001f, -1);
    v.normalize();
    if(raycube(d.o, v, worldsize) >= worldsize)
    {
        return false;
    }
    d.radius = d.xradius = d.yradius = radius;
    d.eyeheight = height;
    d.aboveeye = radius;
    if(!movecamera(&d, d.vel, worldsize, 1))
    {
        o = d.o;
        return true;
    }
    return false;
}

float dropheight(entity &e)
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

void dropenttofloor(entity *e)
{
    droptofloor(e->o, 1.0f, dropheight(*e));
}

void phystest()
{
    static const char * const states[] = {"float", "fall", "slide", "slope", "floor", "step up", "step down", "bounce"};
    printf ("PHYS(pl): %s, air %d, floor: (%f, %f, %f), vel: (%f, %f, %f), g: (%f, %f, %f)\n", states[player->physstate], player->timeinair, player->floor.x, player->floor.y, player->floor.z, player->vel.x, player->vel.y, player->vel.z, player->falling.x, player->falling.y, player->falling.z);
    printf ("PHYS(cam): %s, air %d, floor: (%f, %f, %f), vel: (%f, %f, %f), g: (%f, %f, %f)\n", states[camera1->physstate], camera1->timeinair, camera1->floor.x, camera1->floor.y, camera1->floor.z, camera1->vel.x, camera1->vel.y, camera1->vel.z, camera1->falling.x, camera1->falling.y, camera1->falling.z);
}

COMMAND(phystest, "");

void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m)
{
    if(move)
    {
        m.x = move*-sinf(RAD*yaw);
        m.y = move*cosf(RAD*yaw);
    }
    else
    {
        m.x = m.y = 0;
    }

    if(pitch)
    {
        m.x *= cosf(RAD*pitch);
        m.y *= cosf(RAD*pitch);
        m.z = move*sinf(RAD*pitch);
    }
    else
    {
        m.z = 0;
    }

    if(strafe)
    {
        m.x += strafe*cosf(RAD*yaw);
        m.y += strafe*sinf(RAD*yaw);
    }
}

void vectoyawpitch(const vec &v, float &yaw, float &pitch)
{
    if(v.iszero())
    {
        yaw = pitch = 0;
    }
    else
    {
        yaw = -atan2(v.x, v.y)/RAD;
        pitch = asin(v.z/v.magnitude())/RAD;
    }
}

#define PHYSFRAMETIME 8

VARP(maxroll, 0, 0, 20);
FVAR(straferoll, 0, 0.033f, 90);
FVAR(faderoll, 0, 0.95f, 1);
VAR(floatspeed, 1, 100, 10000);

void modifyvelocity(physent *pl, bool local, bool water, bool floating, int curtime)
{
    bool allowmove = game::allowmove(pl);
    if(floating)
    {
        if(pl->jumping && allowmove)
        {
            pl->jumping = false;
            pl->vel.z = max(pl->vel.z, jumpvel);
        }
    }
    else if(pl->physstate >= PhysEntState_Slope || water)
    {
        if(water && !pl->inwater)
        {
            pl->vel.div(8);
        }
        if(pl->jumping && allowmove)
        {
            pl->jumping = false;
            pl->vel.z = max(pl->vel.z, jumpvel); // physics impulse upwards
            if(water)
            {
                pl->vel.x /= 8.0f;
                pl->vel.y /= 8.0f;
            } // dampen velocity change even harder, gives correct water feel
            game::physicstrigger(pl, local, 1, 0);
        }
    }
    if(!floating && pl->physstate == PhysEntState_Fall)
    {
        pl->timeinair += curtime;
    }
    vec m(0.0f, 0.0f, 0.0f);
    if((pl->move || pl->strafe) && allowmove)
    {
        vecfromyawpitch(pl->yaw, floating || water || pl->type==PhysEnt_Camera ? pl->pitch : 0, pl->move, pl->strafe, m);
        if(!floating && pl->physstate >= PhysEntState_Slope)
        {
            /* move up or down slopes in air
             * but only move up slopes in water
             */
            float dz = -(m.x*pl->floor.x + m.y*pl->floor.y)/pl->floor.z;
            m.z = water ? max(m.z, dz) : dz;
        }
        m.normalize();
    }

    vec d(m);
    d.mul(pl->maxspeed);
    if(pl->type==PhysEnt_Player)
    {
        if(floating)
        {
            if(pl==player)
            {
                d.mul(floatspeed/100.0f);
            }
        }
        else if(pl->crouching)
        {
            d.mul(0.4f);
        }
    }
    float fric = water && !floating ? 20.0f : (pl->physstate >= PhysEntState_Slope || floating ? 6.0f : 30.0f);
    pl->vel.lerp(d, pl->vel, pow(1 - 1/fric, curtime/20.0f));
// old fps friction
//    float friction = water && !floating ? 20.0f : (pl->physstate >= PhysEntState_Slope || floating ? 6.0f : 30.0f);
//    float fpsfric = min(curtime/(20.0f*friction), 1.0f);
//    pl->vel.lerp(pl->vel, d, fpsfric);
}

void modifygravity(physent *pl, bool water, int curtime)
{
    float secs = curtime/1000.0f;
    vec g(0, 0, 0);
    if(pl->physstate == PhysEntState_Fall)
    {
        g.z -= gravity*secs;
    }
    else if(pl->floor.z > 0 && pl->floor.z < floorz)
    {
        g.z = -1;
        g.project(pl->floor);
        g.normalize();
        g.mul(gravity*secs);
    }
    if(!water || !game::allowmove(pl) || (!pl->move && !pl->strafe))
    {
        pl->falling.add(g);
    }
    if(water || pl->physstate >= PhysEntState_Slope)
    {
        float fric = water ? 2.0f : 6.0f,
              c = water ? 1.0f : clamp((pl->floor.z - slopez)/(floorz-slopez), 0.0f, 1.0f);
        pl->falling.mul(pow(1 - c/fric, curtime/20.0f));
// old fps friction
//        float friction = water ? 2.0f : 6.0f,
//              fpsfric = friction/curtime*20.0f,
//              c = water ? 1.0f : clamp((pl->floor.z - slopez)/(floorz-slopez), 0.0f, 1.0f);
//        pl->falling.mul(1 - c/fpsfric);
    }
}

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

bool moveplayer(physent *pl, int moveres, bool local, int curtime)
{
    int material = lookupmaterial(vec(pl->o.x, pl->o.y, pl->o.z + (3*pl->aboveeye - pl->eyeheight)/4));
    bool water = IS_LIQUID(material&MatFlag_Volume);
    bool floating = pl->type==PhysEnt_Player && (pl->state==ClientState_Editing || pl->state==ClientState_Spectator);
    float secs = curtime/1000.f;

    // apply gravity
    if(!floating)
    {
        modifygravity(pl, water, curtime);
    }
    // apply any player generated changes in velocity
    modifyvelocity(pl, local, water, floating, curtime);

    vec d(pl->vel);
    if(!floating && water)
    {
        d.mul(0.5f);
    }
    d.add(pl->falling);
    d.mul(secs);

    pl->blocked = false;

    if(floating)                // just apply velocity
    {
        if(pl->physstate != PhysEntState_Float)
        {
            pl->physstate = PhysEntState_Float;
            pl->timeinair = 0;
            pl->falling = vec(0, 0, 0);
        }
        pl->o.add(d);
    }
    else                        // apply velocity with collision
    {
        const float f = 1.0f/moveres;
        const int timeinair = pl->timeinair;
        int collisions = 0;

        d.mul(f);
        for(int i = 0; i < moveres; ++i)
        {
            if(!move(pl, d) && ++collisions<5)
            {
                i--; // discrete steps collision detection & sliding
            }
        }
        if(timeinair > inairsounddelay && !pl->timeinair && !water) // if we land after long time must have been a high jump, make thud sound
        {
            game::physicstrigger(pl, local, -1, 0);
        }
    }
    if(pl->state==ClientState_Alive)
    {
        updatedynentcache(pl);
    }
    // automatically apply smooth roll when strafing
    if(pl->strafe && maxroll)
    {
        pl->roll = clamp(pl->roll - pow(clamp(1.0f + pl->strafe*pl->roll/maxroll, 0.0f, 1.0f), 0.33f)*pl->strafe*curtime*straferoll, -maxroll, maxroll);
    }
    else
    {
        pl->roll *= curtime == PHYSFRAMETIME ? faderoll : pow(faderoll, curtime/static_cast<float>(PHYSFRAMETIME));
    }
    // play sounds on water transitions
    if(pl->inwater && !water)
    {
        material = lookupmaterial(vec(pl->o.x, pl->o.y, pl->o.z + (pl->aboveeye - pl->eyeheight)/2));
        water = IS_LIQUID(material&MatFlag_Volume);
    }
    if(!pl->inwater && water)
    {
        game::physicstrigger(pl, local, 0, -1, material&MatFlag_Volume);
    }
    else if(pl->inwater && !water)
    {
        game::physicstrigger(pl, local, 0, 1, pl->inwater);
    }
    pl->inwater = water ? material&MatFlag_Volume : Mat_Air;
    //tell players who enter deatmat who are alive to kill themselves
    if(pl->state==ClientState_Alive && (pl->o.z < 0 || material&Mat_Death))
    {
        game::suicide(pl);
    }
    return true;
}

int physsteps = 0, physframetime = PHYSFRAMETIME, lastphysframe = 0;

void physicsframe()          // optimally schedule physics frames inside the graphics frames
{
    int diff = lastmillis - lastphysframe;
    if(diff <= 0)
    {
        physsteps = 0;
    }
    else
    {
        physframetime = clamp(game::scaletime(PHYSFRAMETIME)/100, 1, PHYSFRAMETIME);
        physsteps = (diff + physframetime - 1)/physframetime;
        lastphysframe += physsteps * physframetime;
    }
    cleardynentcache();
}

VAR(physinterp, 0, 1, 1);

void interppos(physent *pl)
{
    pl->o = pl->newpos;

    int diff = lastphysframe - lastmillis;
    if(diff <= 0 || !physinterp)
    {
        return;
    }
    vec deltapos(pl->deltapos);
    deltapos.mul(min(diff, physframetime)/static_cast<float>(physframetime));
    pl->o.add(deltapos);
}

void moveplayer(physent *pl, int moveres, bool local)
{
    if(physsteps <= 0)
    {
        if(local)
        {
            interppos(pl);
        }
        return;
    }
    if(local)
    {
        pl->o = pl->newpos;
    }
    for(int i = 0; i < physsteps-1; ++i)
    {
        moveplayer(pl, moveres, local, physframetime);
    }
    if(local)
    {
        pl->deltapos = pl->o;
    }
    moveplayer(pl, moveres, local, physframetime);
    if(local)
    {
        pl->newpos = pl->o;
        pl->deltapos.sub(pl->newpos);
        interppos(pl);
    }
}

bool bounce(physent *d, float elasticity, float waterfric, float grav)
{
    if(physsteps <= 0)
    {
        interppos(d);
        return false;
    }
    d->o = d->newpos;
    bool hitplayer = false;
    for(int i = 0; i < physsteps-1; ++i)
    {
        if(bounce(d, physframetime/1000.0f, elasticity, waterfric, grav))
        {
            hitplayer = true;
        }
    }
    d->deltapos = d->o;
    if(bounce(d, physframetime/1000.0f, elasticity, waterfric, grav))
    {
        hitplayer = true;
    }
    d->newpos = d->o;
    d->deltapos.sub(d->newpos);
    interppos(d);
    return hitplayer;
}

void updatephysstate(physent *d)
{
    if(d->physstate == PhysEntState_Fall)
    {
        return;
    }
    d->timeinair = 0;
    vec old(d->o);
    /* Attempt to reconstruct the floor state.
     * May be inaccurate since movement collisions are not considered.
     * If good floor is not found, just keep the old floor and hope it's correct enough.
     */
    switch(d->physstate)
    {
        case PhysEntState_Slope:
        case PhysEntState_Floor:
        case PhysEntState_StepDown:
            d->o.z -= 0.15f;
            if(collide(d, vec(0, 0, -1), d->physstate == PhysEntState_Slope || d->physstate == PhysEntState_StepDown ? slopez : floorz))
            {
                d->floor = collidewall;
            }
            break;

        case PhysEntState_StepUp:
            d->o.z -= stairheight+0.15f;
            if(collide(d, vec(0, 0, -1), slopez))
            {
                d->floor = collidewall;
            }
            break;

        case PhysEntState_Slide:
            d->o.z -= 0.15f;
            if(collide(d, vec(0, 0, -1)) && collidewall.z < slopez)
            {
                d->floor = collidewall;
            }
            break;
    }
    if(d->physstate > PhysEntState_Fall && d->floor.z <= 0)
    {
        d->floor = vec(0, 0, 1);
    }
    d->o = old;
}

#define DIR(name,v,d,s,os) ICOMMAND(name, "D", (int *down), { player->s = *down!=0; player->v = player->s ? d : (player->os ? -(d) : 0); });

DIR(backward, move,   -1, k_down,  k_up);
DIR(forward,  move,    1, k_up,    k_down);
DIR(left,     strafe,  1, k_left,  k_right);
DIR(right,    strafe, -1, k_right, k_left);

#undef DIR

ICOMMAND(jump,   "D", (int *down), { if(!*down || game::canjump()) player->jumping = *down!=0; });
ICOMMAND(crouch, "D", (int *down), { if(!*down) player->crouching = abs(player->crouching); else if(game::cancrouch()) player->crouching = -1; });

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

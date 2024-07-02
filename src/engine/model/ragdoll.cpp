/* ragdoll.cpp: ragdoll physics support
 *
 * Libprimis supports limited client-side ragdolls to simulate corpses. Ragdolls
 * are created when actors are killed and support basic physical interaction with
 * terrain (but not other players).
 *
 * This file contains implementation functions for the ragdoll class. For the
 * class definition, see ragdoll.h.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include <memory>
#include <optional>

#include "interface/console.h"
#include "interface/control.h"

#include "render/rendergl.h"

#include "world/entities.h"
#include "world/octaworld.h"
#include "world/physics.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"

/*               ragdollskel                  */

//ragdollskel::tri

bool ragdollskel::tri::shareverts(const tri &t) const
{
    for(int i = 0; i < 3; ++i)
    {
        for(int j = 0; j < 3; ++j)
        {
            if(vert[i] == t.vert[j])
            {
                return true;
            }
        }
    }
    return false;
}

//ragdollskel

void ragdollskel::setupjoints()
{
    for(vert &i : verts)
    {
        i.weight = 0;
    }
    for(joint &j : joints)
    {
        j.weight = 0;
        vec pos(0, 0, 0);
        for(int k = 0; k < 3; ++k)
        {
            if(j.vert[k]>=0)
            {
                pos.add(verts[j.vert[k]].pos);
                j.weight++;
                verts[j.vert[k]].weight++;
            }
        }
        for(int k = 0; k < j.weight; ++k)
        {
            j.weight = 1/j.weight;
        }
        pos.mul(j.weight);

        tri &t = tris[j.tri];
        matrix4x3 &m = j.orient;
        const vec &v1 = verts[t.vert[0]].pos,
                  &v2 = verts[t.vert[1]].pos,
                  &v3 = verts[t.vert[2]].pos;
        m.a = vec(v2).sub(v1).normalize();
        m.c.cross(m.a, vec(v3).sub(v1)).normalize();
        m.b.cross(m.c, m.a);
        m.d = pos;
        m.transpose();
    }
    for(vert &i : verts)
    {
        if(i.weight)
        {
            i.weight = 1/i.weight;
        }
    }
    reljoints.clear();
}

void ragdollskel::setuprotfrictions()
{
    rotfrictions.clear();
    for(uint i = 0; i < tris.size(); i++)
    {
        for(uint j = i+1; j < tris.size(); j++)
        {
            if(tris[i].shareverts(tris[j]))
            {
                rotfriction r;
                r.tri[0] = i;
                r.tri[1] = j;
                rotfrictions.push_back(r);
            }
        }
    }
}

void ragdollskel::setup()
{
    setupjoints();
    setuprotfrictions();

    loaded = true;
}

void ragdollskel::addreljoint(int bone, int parent)
{
    reljoint r;
    r.bone = bone;
    r.parent = parent;
    reljoints.push_back(r);
}
/*                  ragdolldata                   */

ragdolldata::ragdolldata(ragdollskel *skel, float scale)
    : skel(skel),
      millis(lastmillis),
      collidemillis(0),
      lastmove(lastmillis),
      radius(0),
      tris(skel->tris.size()),
      animjoints(!skel->animjoints || skel->joints.empty() ? nullptr : new matrix4x3[skel->joints.size()]),
      reljoints(skel->reljoints.empty() ? nullptr : new dualquat[skel->reljoints.size()]),
      verts(skel->verts.size()),
      collisions(0),
      floating(0),
      unsticks(INT_MAX),
      timestep(0),
      scale(scale)
{
}

ragdolldata::~ragdolldata()
{
    if(animjoints)
    {
        delete[] animjoints;
    }
    if(reljoints)
    {
        delete[] reljoints;
    }
}

/*
    seed particle position = avg(modelview * base2anim * spherepos)
    mapped transform = invert(curtri) * origtrig
    parented transform = parent{invert(curtri) * origtrig} * (invert(parent{base2anim}) * base2anim)
*/

void ragdolldata::calcanimjoint(int i, const matrix4x3 &anim)
{
    if(!animjoints)
    {
        return;
    }
    const ragdollskel::joint &j = skel->joints[i];
    vec pos(0, 0, 0);
    for(int k = 0; k < 3; ++k)
    {
        if(j.vert[k]>=0)
        {
            pos.add(verts[j.vert[k]].pos);
        }
    }
    pos.mul(j.weight);

    const ragdollskel::tri &t = skel->tris[j.tri];
    matrix4x3 m;
    const vec &v1 = verts[t.vert[0]].pos,
              &v2 = verts[t.vert[1]].pos,
              &v3 = verts[t.vert[2]].pos;
    m.a = vec(v2).sub(v1).normalize();
    m.c.cross(m.a, vec(v3).sub(v1)).normalize();
    m.b.cross(m.c, m.a);
    m.d = pos;
    animjoints[i].transposemul(m, anim);
}

void ragdolldata::calctris()
{
    for(uint i = 0; i < skel->tris.size(); i++)
    {
        const ragdollskel::tri &t = skel->tris[i];
        matrix3 &m = tris[i];
        const vec &v1 = verts[t.vert[0]].pos,
                  &v2 = verts[t.vert[1]].pos,
                  &v3 = verts[t.vert[2]].pos;
        m.a = vec(v2).sub(v1).normalize();
        m.c.cross(m.a, vec(v3).sub(v1)).normalize();
        m.b.cross(m.c, m.a);
    }
}

void ragdolldata::calcboundsphere()
{
    center = vec(0, 0, 0);
    for(const vert &v : verts)
    {
        center.add(v.pos);
    }
    center.div(verts.size());
    radius = 0;
    for(const vert &v : verts)
    {
        radius = std::max(radius, v.pos.dist(center));
    }
}

VAR(ragdolltimestepmin, 1, 5, 50);
VAR(ragdolltimestepmax, 1, 10, 50);

void ragdolldata::init(const dynent *d)
{
    float ts = ragdolltimestepmin/1000.0f;
    for(vert &v : verts)
    {
        (v.oldpos = v.pos).sub(vec(d->vel).add(d->falling).mul(ts));
    }
    timestep = ts;

    calctris();
    calcboundsphere();
    offset = d->o;
    offset.sub(skel->eye >= 0 ? verts[skel->eye].pos : center);
    offset.z += (d->eyeheight + d->aboveeye)/2;
}

void ragdolldata::constraindist()
{
    for(const ragdollskel::distlimit &d : skel->distlimits)
    {
        vert &v1 = verts[d.vert[0]],
             &v2 = verts[d.vert[1]];
        vec dir = vec(v2.pos).sub(v1.pos);
        float dist = dir.magnitude()/scale,
              cdist;
        if(dist < d.mindist)
        {
            cdist = d.mindist;
        }
        else if(dist > d.maxdist)
        {
            cdist = d.maxdist;
        }
        else
        {
            continue;
        }
        if(dist > 1e-4f)
        {
            dir.mul(cdist*0.5f/dist);
        }
        else
        {
            dir = vec(0, 0, cdist*0.5f*scale);
        }
        vec center = vec(v1.pos).add(v2.pos).mul(0.5f);
        v1.newpos.add(vec(center).sub(dir));
        v1.weight++;
        v2.newpos.add(vec(center).add(dir));
        v2.weight++;
    }
}

void ragdolldata::applyrotlimit(const ragdollskel::tri &t1, const ragdollskel::tri &t2, float angle, const vec &axis)
{
    vert &v1a = verts[t1.vert[0]],
         &v1b = verts[t1.vert[1]],
         &v1c = verts[t1.vert[2]],
         &v2a = verts[t2.vert[0]],
         &v2b = verts[t2.vert[1]],
         &v2c = verts[t2.vert[2]];
    // vec() copy constructor used below to deal with the fact that vec.add/sub() are destructive operations
    vec m1 = vec(v1a.pos).add(v1b.pos).add(v1c.pos).div(3),
        m2 = vec(v2a.pos).add(v2b.pos).add(v2c.pos).div(3),
        q1a, q1b, q1c, q2a, q2b, q2c;
    float w1 = q1a.cross(axis, vec(v1a.pos).sub(m1)).magnitude() +
               q1b.cross(axis, vec(v1b.pos).sub(m1)).magnitude() +
               q1c.cross(axis, vec(v1c.pos).sub(m1)).magnitude(),
          w2 = q2a.cross(axis, vec(v2a.pos).sub(m2)).magnitude() +
               q2b.cross(axis, vec(v2b.pos).sub(m2)).magnitude() +
               q2c.cross(axis, vec(v2c.pos).sub(m2)).magnitude();
    angle /= w1 + w2 + 1e-9f;
    float a1 = angle*w2,
          a2 = -angle*w1,
          s1 = std::sin(a1),
          s2 = std::sin(a2);
    vec c1 = vec(axis).mul(1 - std::cos(a1)),
        c2 = vec(axis).mul(1 - std::cos(a2));
    v1a.newpos.add(vec().cross(c1, q1a).madd(q1a, s1).add(v1a.pos));
    v1a.weight++;
    v1b.newpos.add(vec().cross(c1, q1b).madd(q1b, s1).add(v1b.pos));
    v1b.weight++;
    v1c.newpos.add(vec().cross(c1, q1c).madd(q1c, s1).add(v1c.pos));
    v1c.weight++;
    v2a.newpos.add(vec().cross(c2, q2a).madd(q2a, s2).add(v2a.pos));
    v2a.weight++;
    v2b.newpos.add(vec().cross(c2, q2b).madd(q2b, s2).add(v2b.pos));
    v2b.weight++;
    v2c.newpos.add(vec().cross(c2, q2c).madd(q2c, s2).add(v2c.pos));
    v2c.weight++;
}

void ragdolldata::constrainrot()
{
    calctris();
    for(const ragdollskel::rotlimit &r : skel->rotlimits)
    {
        matrix3 rot;
        rot.mul(tris[r.tri[0]], r.middle);
        rot.multranspose(tris[r.tri[1]]);

        vec axis;
        float angle,
              tr = rot.trace();
        if(tr >= r.maxtrace || !rot.calcangleaxis(tr, angle, axis))
        {
            continue;
        }
        angle = r.maxangle - angle + 1e-3f;
        applyrotlimit(skel->tris[r.tri[0]], skel->tris[r.tri[1]], angle, axis);
    }
}

void ragdolldata::calcrotfriction()
{
    for(ragdollskel::rotfriction &r : skel->rotfrictions)
    {
        r.middle.transposemul(tris[r.tri[0]], tris[r.tri[1]]);
    }
}

void ragdolldata::applyrotfriction(float ts)
{
    static FVAR(ragdollrotfric, 0, 0.85f, 1);
    static FVAR(ragdollrotfricstop, 0, 0.1f, 1);

    calctris();
    float stopangle = 2*M_PI*ts*ragdollrotfricstop,
          rotfric = 1.0f - std::pow(ragdollrotfric, ts*1000.0f/ragdolltimestepmin);
    for(const ragdollskel::rotfriction &r : skel->rotfrictions)
    {
        matrix3 rot;
        rot.mul(tris[r.tri[0]], r.middle);
        rot.multranspose(tris[r.tri[1]]);

        vec axis;
        float angle;
        if(!rot.calcangleaxis(angle, axis))
        {
            continue;
        }
        angle *= -(std::fabs(angle) >= stopangle ? rotfric : 1.0f);
        applyrotlimit(skel->tris[r.tri[0]], skel->tris[r.tri[1]], angle, axis);
    }
    for(vert &v : verts)
    {
        if(!v.weight)
        {
            continue;
        }
        v.pos = v.newpos.div(v.weight);
        v.newpos = vec(0, 0, 0);
        v.weight = 0;
    }
}

void ragdolldata::tryunstick(float speed)
{
    vec unstuck(0, 0, 0);
    size_t stuck = 0;
    for(uint i = 0; i < verts.size(); i++)
    {
        vert &v = verts[i];
        if(v.stuck)
        {
            if(collidevert(v.pos, vec(0, 0, 0), skel->verts[i].radius))
            {
                stuck++;
                continue;
            }
            v.stuck = false;
        }
        unstuck.add(v.pos);
    }
    unsticks = 0;
    if(!stuck || stuck >= verts.size())
    {
        return;
    }
    unstuck.div(verts.size() - stuck);
    for(vert &v : verts)
    {
        if(v.stuck)
        {
            v.pos.add(vec(unstuck).sub(v.pos).rescale(speed));
            unsticks++;
        }
    }
}

void ragdolldata::constrain()
{
    static VAR(ragdollconstrain, 1, 7, 100); //number of iterations to run ragdolldata::constrain() for
    //note: this for loop does not use the loop variable `i` anywhere
    for(int i = 0; i < ragdollconstrain; ++i)
    {
        constraindist();
        for(vert &v : verts)
        {
            v.undo = v.pos;
            if(v.weight)
            {
                v.pos = v.newpos.div(v.weight);
                v.newpos = vec(0, 0, 0);
                v.weight = 0;
            }
        }

        constrainrot();
        for(uint j = 0; j < verts.size(); j++)
        {
            vert &v = verts[j];
            if(v.weight)
            {
                v.pos = v.newpos.div(v.weight);
                v.newpos = vec(0, 0, 0);
                v.weight = 0;
            }
            if(v.pos != v.undo && collidevert(v.pos, vec(v.pos).sub(v.undo), skel->verts[j].radius))
            {
                vec dir = vec(v.pos).sub(v.oldpos);
                float facing = dir.dot(collidewall);
                if(facing < 0)
                {
                    v.oldpos = vec(v.undo).sub(dir.msub(collidewall, 2*facing));
                }
                v.pos = v.undo;
                v.collided = true;
            }
        }
    }
}

void ragdolldata::move(dynent *pl, float ts)
{

    static FVAR(ragdollbodyfric, 0, 0.95f, 1);
    static FVAR(ragdollbodyfricscale, 0, 2, 10);
    static FVAR(ragdollwaterfric, 0, 0.85f, 1);
    static FVAR(ragdollgroundfric, 0, 0.8f, 1);
    static FVAR(ragdollairfric, 0, 0.996f, 1);
    static FVAR(ragdollunstick, 0, 10, 1e3f);
    static VAR(ragdollexpireoffset, 0, 2500, 30000);
    static VAR(ragdollwaterexpireoffset, 0, 4000, 30000);

    if(collidemillis && lastmillis > collidemillis)
    {
        return;
    }
    const int material = rootworld.lookupmaterial(vec(center.x, center.y, center.z + radius/2));
    const bool water = (material&MatFlag_Volume) == Mat_Water;
    pl->inwater = water ? material&MatFlag_Volume : Mat_Air;

    calcrotfriction();
    const float tsfric = timestep ? ts/timestep : 1,
                airfric = ragdollairfric + std::min((ragdollbodyfricscale*collisions)/verts.size(), 1.0f)*(ragdollbodyfric - ragdollairfric);
    collisions = 0;
    for(uint i = 0; i < verts.size(); i++)
    {
        vert &v = verts[i];
        vec dpos = vec(v.pos).sub(v.oldpos);
        dpos.z -= 100*ts*ts;
        if(water)
        {
            //reinterpret cast pointer -> "random" seed value
            dpos.z += 0.25f*std::sin(detrnd(reinterpret_cast<size_t>(this)+i, 360)/RAD + lastmillis/10000.0f*M_PI)*ts;
        }
        dpos.mul(std::pow((water ? ragdollwaterfric : 1.0f) * (v.collided ? ragdollgroundfric : airfric), ts*1000.0f/ragdolltimestepmin)*tsfric);
        v.oldpos = v.pos;
        v.pos.add(dpos);
    }
    applyrotfriction(ts);
    for(uint i = 0; i < verts.size(); i++)
    {
        vert &v = verts[i];
        if(v.pos.z < 0)
        {
            v.pos.z = 0;
            v.oldpos = v.pos;
            collisions++;
        }
        vec dir = vec(v.pos).sub(v.oldpos);
        v.collided = collidevert(v.pos, dir, skel->verts[i].radius);
        if(v.collided)
        {
            v.pos = v.oldpos;
            v.oldpos.sub(dir.reflect(collidewall));
            collisions++;
        }
    }
    if(unsticks && ragdollunstick)
    {
        tryunstick(ts*ragdollunstick);
    }
    timestep = ts;
    if(collisions)
    {
        floating = 0;
        if(!collidemillis)
        {
            collidemillis = lastmillis + (water ? ragdollwaterexpireoffset : ragdollexpireoffset);
        }
    }
    else if(++floating > 1 && lastmillis < collidemillis)
    {
        collidemillis = 0;
    }
    constrain();
    calctris();
    calcboundsphere();
}

bool ragdolldata::collidevert(const vec &pos, const vec &dir, float radius)
{
    static struct vertent : physent
    {
        vertent()
        {
            type = PhysEnt_Bounce;
            radius = xradius = yradius = eyeheight = aboveeye = 1;
        }
    } v;
    v.o = pos;
    if(v.radius != radius)
    {
        v.radius = v.xradius = v.yradius = v.eyeheight = v.aboveeye = radius;
    }
    return collide(&v, dir, 0, false);
}

//used in iengine
void moveragdoll(dynent *d)
{
    static FVAR(ragdolleyesmooth, 0, 0.5f, 1);
    static VAR(ragdolleyesmoothmillis, 1, 250, 10000);
    if(!curtime || !d->ragdoll)
    {
        return;
    }
    if(!d->ragdoll->collidemillis || lastmillis < d->ragdoll->collidemillis)
    {
        const int lastmove = d->ragdoll->lastmove;
        while(d->ragdoll->lastmove + (lastmove == d->ragdoll->lastmove ? ragdolltimestepmin : ragdolltimestepmax) <= lastmillis)
        {
            int timestep = std::min(ragdolltimestepmax, lastmillis - d->ragdoll->lastmove);
            d->ragdoll->move(d, timestep/1000.0f);
            d->ragdoll->lastmove += timestep;
        }
    }

    vec eye = d->ragdoll->skel->eye >= 0 ? d->ragdoll->verts[d->ragdoll->skel->eye].pos : d->ragdoll->center;
    eye.add(d->ragdoll->offset);
    const float k = std::pow(ragdolleyesmooth, static_cast<float>(curtime)/ragdolleyesmoothmillis);
    d->o.lerp(eye, 1-k);
}

void cleanragdoll(dynent *d)
{
    if(d->ragdoll)
    {
        delete d->ragdoll;
        d->ragdoll = nullptr;
    }
}

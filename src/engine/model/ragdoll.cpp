
#include "engine.h"

#include "interface/console.h"
#include "interface/control.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"

#include "world/physics.h"

#include "ragdoll.h"

FVAR(ragdollbodyfric, 0, 0.95f, 1);
FVAR(ragdollbodyfricscale, 0, 2, 10);
FVAR(ragdollwaterfric, 0, 0.85f, 1);
FVAR(ragdollgroundfric, 0, 0.8f, 1);
FVAR(ragdollairfric, 0, 0.996f, 1);
FVAR(ragdollunstick, 0, 10, 1e3f);
VAR(ragdollexpireoffset, 0, 2500, 30000);
VAR(ragdollwaterexpireoffset, 0, 4000, 30000);

/*
    seed particle position = avg(modelview * base2anim * spherepos)
    mapped transform = invert(curtri) * origtrig
    parented transform = parent{invert(curtri) * origtrig} * (invert(parent{base2anim}) * base2anim)
*/

void ragdolldata::constraindist()
{
    float invscale = 1.0f/scale;
    for(int i = 0; i < skel->distlimits.length(); i++)
    {
        ragdollskel::distlimit &d = skel->distlimits[i];
        vert &v1 = verts[d.vert[0]],
             &v2 = verts[d.vert[1]];
        vec dir = vec(v2.pos).sub(v1.pos);
        float dist = dir.magnitude()*invscale, cdist;
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
            dir = vec(0, 0, cdist*0.5f/invscale);
        }
        vec center = vec(v1.pos).add(v2.pos).mul(0.5f);
        v1.newpos.add(vec(center).sub(dir));
        v1.weight++;
        v2.newpos.add(vec(center).add(dir));
        v2.weight++;
    }
}

inline void ragdolldata::applyrotlimit(ragdollskel::tri &t1, ragdollskel::tri &t2, float angle, const vec &axis)
{
    vert &v1a = verts[t1.vert[0]],
         &v1b = verts[t1.vert[1]],
         &v1c = verts[t1.vert[2]],
         &v2a = verts[t2.vert[0]],
         &v2b = verts[t2.vert[1]],
         &v2c = verts[t2.vert[2]];
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
    float a1 = angle*w2, a2 = -angle*w1,
          s1 = sinf(a1), s2 = sinf(a2);
    vec c1 = vec(axis).mul(1 - cosf(a1)),
        c2 = vec(axis).mul(1 - cosf(a2));
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
    for(int i = 0; i < skel->rotlimits.length(); i++)
    {
        ragdollskel::rotlimit &r = skel->rotlimits[i];
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

VAR(ragdolltimestepmin, 1, 5, 50);
VAR(ragdolltimestepmax, 1, 10, 50);
FVAR(ragdollrotfric, 0, 0.85f, 1);
FVAR(ragdollrotfricstop, 0, 0.1f, 1);

void ragdolldata::calcrotfriction()
{
    for(int i = 0; i < skel->rotfrictions.length(); i++)
    {
        ragdollskel::rotfriction &r = skel->rotfrictions[i];
        r.middle.transposemul(tris[r.tri[0]], tris[r.tri[1]]);
    }
}

void ragdolldata::applyrotfriction(float ts)
{
    calctris();
    float stopangle = 2*M_PI*ts*ragdollrotfricstop,
          rotfric = 1.0f - pow(ragdollrotfric, ts*1000.0f/ragdolltimestepmin);
    for(int i = 0; i < skel->rotfrictions.length(); i++)
    {
        ragdollskel::rotfriction &r = skel->rotfrictions[i];
        matrix3 rot;
        rot.mul(tris[r.tri[0]], r.middle);
        rot.multranspose(tris[r.tri[1]]);

        vec axis;
        float angle;
        if(!rot.calcangleaxis(angle, axis))
        {
            continue;
        }
        angle *= -(fabs(angle) >= stopangle ? rotfric : 1.0f);
        applyrotlimit(skel->tris[r.tri[0]], skel->tris[r.tri[1]], angle, axis);
    }
    for(int i = 0; i < skel->verts.length(); i++)
    {
        vert &v = verts[i];
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
    int stuck = 0;
    for(int i = 0; i < skel->verts.length(); i++)
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
    if(!stuck || stuck >= skel->verts.length())
    {
        return;
    }
    unstuck.div(skel->verts.length() - stuck);
    for(int i = 0; i < skel->verts.length(); i++)
    {
        vert &v = verts[i];
        if(v.stuck)
        {
            v.pos.add(vec(unstuck).sub(v.pos).rescale(speed));
            unsticks++;
        }
    }
}

VAR(ragdollconstrain, 1, 7, 100);

void ragdolldata::constrain()
{
    for(int i = 0; i < ragdollconstrain; ++i)
    {
        constraindist();
        for(int j = 0; j < skel->verts.length(); j++)
        {
            vert &v = verts[j];
            v.undo = v.pos;
            if(v.weight)
            {
                v.pos = v.newpos.div(v.weight);
                v.newpos = vec(0, 0, 0);
                v.weight = 0;
            }
        }

        constrainrot();
        for(int j = 0; j < skel->verts.length(); j++)
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
    if(collidemillis && lastmillis > collidemillis)
    {
        return;
    }
    int material = lookupmaterial(vec(center.x, center.y, center.z + radius/2));
    bool water = IS_LIQUID(material&MatFlag_Volume);
    pl->inwater = water ? material&MatFlag_Volume : Mat_Air;

    calcrotfriction();
    float tsfric = timestep ? ts/timestep : 1,
          airfric = ragdollairfric + min((ragdollbodyfricscale*collisions)/skel->verts.length(), 1.0f)*(ragdollbodyfric - ragdollairfric);
    collisions = 0;
    for(int i = 0; i < skel->verts.length(); i++)
    {
        vert &v = verts[i];
        vec dpos = vec(v.pos).sub(v.oldpos);
        dpos.z -= gravity*ts*ts;
        if(water)
        {
            dpos.z += 0.25f*sinf(detrnd(size_t(this)+i, 360)*RAD + lastmillis/10000.0f*M_PI)*ts;
        }
        dpos.mul(pow((water ? ragdollwaterfric : 1.0f) * (v.collided ? ragdollgroundfric : airfric), ts*1000.0f/ragdolltimestepmin)*tsfric);
        v.oldpos = v.pos;
        v.pos.add(dpos);
    }
    applyrotfriction(ts);
    for(int i = 0; i < skel->verts.length(); i++)
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

FVAR(ragdolleyesmooth, 0, 0.5f, 1);
VAR(ragdolleyesmoothmillis, 1, 250, 10000);

void moveragdoll(dynent *d)
{
    if(!curtime || !d->ragdoll)
    {
        return;
    }
    if(!d->ragdoll->collidemillis || lastmillis < d->ragdoll->collidemillis)
    {
        int lastmove = d->ragdoll->lastmove;
        while(d->ragdoll->lastmove + (lastmove == d->ragdoll->lastmove ? ragdolltimestepmin : ragdolltimestepmax) <= lastmillis)
        {
            int timestep = min(ragdolltimestepmax, lastmillis - d->ragdoll->lastmove);
            d->ragdoll->move(d, timestep/1000.0f);
            d->ragdoll->lastmove += timestep;
        }
    }

    vec eye = d->ragdoll->skel->eye >= 0 ? d->ragdoll->verts[d->ragdoll->skel->eye].pos : d->ragdoll->center;
    eye.add(d->ragdoll->offset);
    float k = pow(ragdolleyesmooth, static_cast<float>(curtime)/ragdolleyesmoothmillis);
    d->o.lerp(eye, 1-k);
}

void cleanragdoll(dynent *d)
{
    DELETEP(d->ragdoll);
}

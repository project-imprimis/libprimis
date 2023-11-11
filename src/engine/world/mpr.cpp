/*This code is based off the Minkowski Portal Refinement algorithm by Gary Snethen
 * in XenoCollide & Game Programming Gems 7.
 *
 * Minkowski Portal Refinement is a way of finding whether two hulls intersect
 * efficiently, useful for finding if two models are intersecting quickly
 *
 * used in physics.cpp and for model-related collision purposes
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include "octaworld.h"

#include "mpr.h"

namespace mpr
{

    //CubePlanes

    vec CubePlanes::center() const
    {
        return p.o;
    }

    vec CubePlanes::supportpoint(const vec &n) const
    {
        int besti = 7;
        float bestd = n.dot(p.v[7]);
        for(int i = 0; i < 7; ++i)
        {
            float d = n.dot(p.v[i]);
            if(d > bestd)
            {
                besti = i;
                bestd = d;
            }
        }
        return p.v[besti];
    }

    //SolidCube

    vec SolidCube::center() const
    {
        return vec(o).add(size/2);
    }

    vec SolidCube::supportpoint (const vec &n) const
    {
        vec p(o);
        if(n.x > 0)
        {
            p.x += size;
        }
        if(n.y > 0)
        {
            p.y += size;
        }
        if(n.z > 0)
        {
            p.z += size;
        }
        return p;
    }

    //Ent

    vec Ent::center() const
    {
        return vec(ent->o.x, ent->o.y, ent->o.z + (ent->aboveeye - ent->eyeheight)/2);
    }

    //EntOBB

    EntOBB::EntOBB(const physent *ent) : Ent(ent)
    {
        orient.setyaw(ent->yaw/RAD);
    }

    vec EntOBB::contactface(const vec &wn, const vec &wdir) const
    {
        vec n = orient.transform(wn).div(vec(ent->xradius, ent->yradius, (ent->aboveeye + ent->eyeheight)/2)),
            dir = orient.transform(wdir),
            an(std::fabs(n.x), std::fabs(n.y), dir.z ? std::fabs(n.z) : 0),
            fn(0, 0, 0);
        if(an.x > an.y)
        {
            if(an.x > an.z)
            {
                fn.x = n.x*dir.x < 0 ? (n.x > 0 ? 1 : -1) : 0;
            }
            else if(an.z > 0)
            {
                fn.z = n.z*dir.z < 0 ? (n.z > 0 ? 1 : -1) : 0;
            }
        }
        else if(an.y > an.z)
        {
            fn.y = n.y*dir.y < 0 ? (n.y > 0 ? 1 : -1) : 0;
        }
        else if(an.z > 0)
        {
            fn.z = n.z*dir.z < 0 ? (n.z > 0 ? 1 : -1) : 0;
        }
        return orient.transposedtransform(fn);
    }

    vec EntOBB::localsupportpoint(const vec &ln) const
    {
        return vec(ln.x > 0 ? ent->xradius : -ent->xradius,
                   ln.y > 0 ? ent->yradius : -ent->yradius,
                   ln.z > 0 ? ent->aboveeye : -ent->eyeheight);
    }
    vec EntOBB::supportpoint(const vec &n) const
    {
        return orient.transposedtransform(localsupportpoint(orient.transform(n))).add(ent->o);
    }

    float EntOBB::supportcoordneg(const vec &p) const
    {
        return localsupportpoint(vec(p).neg()).dot(p);
    }

    float EntOBB::supportcoord(const vec &p) const
    {
        return localsupportpoint(p).dot(p);
    }

    float EntOBB::left()   const { return supportcoordneg(orient.a) + ent->o.x; }
    float EntOBB::right()  const { return supportcoord(orient.a) + ent->o.x; }
    float EntOBB::back()   const { return supportcoordneg(orient.b) + ent->o.y; }
    float EntOBB::front()  const { return supportcoord(orient.b) + ent->o.y; }
    float EntOBB::bottom() const { return ent->o.z - ent->eyeheight; }
    float EntOBB::top()    const { return ent->o.z + ent->aboveeye; }

    //EntFuzzy

    float EntFuzzy::left()   const { return ent->o.x - ent->radius; }
    float EntFuzzy::right()  const { return ent->o.x + ent->radius; }
    float EntFuzzy::back()   const { return ent->o.y - ent->radius; }
    float EntFuzzy::front()  const { return ent->o.y + ent->radius; }
    float EntFuzzy::bottom() const { return ent->o.z - ent->eyeheight; }
    float EntFuzzy::top()    const { return ent->o.z + ent->aboveeye; }

    //EntCylinder

    vec EntCylinder::contactface(const vec &n, const vec &dir) const
    {
        float dxy = n.dot2(n)/(ent->radius*ent->radius),
              dz = n.z*n.z*4/(ent->aboveeye + ent->eyeheight);
        vec fn(0, 0, 0);
        if(dz > dxy && dir.z)
        {
            fn.z = n.z*dir.z < 0 ? (n.z > 0 ? 1 : -1) : 0;
        }
        else if(n.dot2(dir) < 0)
        {
            fn.x = n.x;
            fn.y = n.y;
            fn.normalize();
        }
        return fn;
    }

    vec EntCylinder::supportpoint(const vec &n) const
    {
        vec p(ent->o);
        if(n.z > 0)
        {
            p.z += ent->aboveeye;
        }
        else
        {
            p.z -= ent->eyeheight;
        }
        if(n.x || n.y)
        {
            float r = ent->radius / n.magnitude2();
            p.x += n.x*r;
            p.y += n.y*r;
        }
        return p;
    }

    //EntCapsule

    vec EntCapsule::supportpoint(const vec &n) const
    {
        vec p(ent->o);
        if(n.z > 0)
        {
            p.z += ent->aboveeye - ent->radius;
        }
        else
        {
            p.z -= ent->eyeheight - ent->radius;
        }
        p.add(vec(n).mul(ent->radius / n.magnitude()));
        return p;
    }

    //EntEllipsoid

    vec EntEllipsoid::supportpoint(const vec &dir) const
    {
        vec p(ent->o);
        vec n = vec(dir).normalize();
        p.x += ent->radius*n.x;
        p.y += ent->radius*n.y;
        p.z += (ent->aboveeye + ent->eyeheight)/2*(1 + n.z) - ent->eyeheight;
        return p;
    }

    //Model

    Model::Model(const vec &ent, const vec &center, const vec &radius, int yaw, int pitch, int roll) : o(ent), radius(radius)
    {
        orient.identity();
        if(roll)
        {
            orient.rotate_around_y(sincosmod360(roll));
        }
        if(pitch)
        {
            orient.rotate_around_x(sincosmod360(-pitch));
        }
        if(yaw)
        {
            orient.rotate_around_z(sincosmod360(-yaw));
        }
        o.add(orient.transposedtransform(center));
    }

    vec Model::center() const
    {
        return o;
    }

    //ModelOBB

    vec ModelOBB::contactface(const vec &wn, const vec &wdir) const
    {
        vec n = orient.transform(wn).div(radius),
            dir = orient.transform(wdir),
            an(std::fabs(n.x), std::fabs(n.y), dir.z ? std::fabs(n.z) : 0),
            fn(0, 0, 0);
        if(an.x > an.y)
        {
            if(an.x > an.z)
            {
                fn.x = n.x*dir.x < 0 ? (n.x > 0 ? 1 : -1) : 0;
            }
            else if(an.z > 0)
            {
                fn.z = n.z*dir.z < 0 ? (n.z > 0 ? 1 : -1) : 0;
            }
        }
        else if(an.y > an.z)
        {
            fn.y = n.y*dir.y < 0 ? (n.y > 0 ? 1 : -1) : 0;
        }
        else if(an.z > 0)
        {
            fn.z = n.z*dir.z < 0 ? (n.z > 0 ? 1 : -1) : 0;
        }
        return orient.transposedtransform(fn);
    }

    vec ModelOBB::supportpoint(const vec &n) const
    {
        vec ln = orient.transform(n),
            p(0, 0, 0);
        if(ln.x > 0)
        {
            p.x += radius.x;
        }
        else
        {
            p.x -= radius.x;
        }
        if(ln.y > 0)
        {
            p.y += radius.y;
        }
        else
        {
            p.y -= radius.y;
        }
        if(ln.z > 0)
        {
            p.z += radius.z;
        }
        else
        {
            p.z -= radius.z;
        }
        return orient.transposedtransform(p).add(o);
    }

    //ModelEllipse

    vec ModelEllipse::contactface(const vec &wn, const vec &wdir) const
    {
        vec n = orient.transform(wn).div(radius),
            dir = orient.transform(wdir);
        float dxy = n.dot2(n),
              dz = n.z*n.z;
        vec fn(0, 0, 0);
        if(dz > dxy && dir.z)
        {
            fn.z = n.z*dir.z < 0 ? (n.z > 0 ? 1 : -1) : 0;
        }
        else if(n.dot2(dir) < 0)
        {
            fn.x = n.x*radius.y;
            fn.y = n.y*radius.x;
            fn.normalize();
        }
        return orient.transposedtransform(fn);
    }

    vec ModelEllipse::supportpoint(const vec &n) const
    {
        vec ln = orient.transform(n),
            p(0, 0, 0);
        if(ln.z > 0)
        {
            p.z += radius.z;
        }
        else
        {
            p.z -= radius.z;
        }
        if(ln.x || ln.y)
        {
            float r = ln.magnitude2();
            p.x += ln.x*radius.x/r;
            p.y += ln.y*radius.y/r;
        }
        return orient.transposedtransform(p).add(o);
    }
}

/**
 * @brief Definition of methods in ents.h shared header.
 *
 * This file implements the behavior in the ents.h interface header.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include <memory>
#include <optional>

#include "entities.h"
#include "bih.h"
#include "interface/control.h"

#include "model/model.h"
#include "model/ragdoll.h"

//extentity

extentity::extentity() :
            flags(0),
            attached(nullptr)
{
}

bool extentity::spawned() const
{
    return (flags&EntFlag_Spawned) != 0;
}

void extentity::setspawned(bool val)
{
    if(val)
    {
        flags |= EntFlag_Spawned;
    }
    else
    {
        flags &= ~EntFlag_Spawned;
    }
}

void extentity::setspawned()
{
    flags |= EntFlag_Spawned;
}

void extentity::clearspawned()
{
    flags &= ~EntFlag_Spawned;
}

//physent

physent::physent() :
            o(0, 0, 0),
            deltapos(0, 0, 0),
            newpos(0, 0, 0),
            yaw(0),
            pitch(0),
            roll(0),
            maxspeed(35),
            radius(4.0f),
            eyeheight(14),
            maxheight(15),
            aboveeye(2),
            xradius(4.1f),
            yradius(4.1f),
            zmargin(0),
            state(0),
            editstate(0),
            type(PhysEnt_Player),
            collidetype(Collide_Ellipse),
            blocked(false)
{
    reset();
}

void physent::resetinterp()
{
    newpos = o;
    deltapos = vec(0, 0, 0);
}

void physent::reset()
{
    inwater = 0;
    timeinair = 0;
    eyeheight = maxheight;
    jumping = false;
    strafe = move = crouching = 0;
    physstate = PhysEntState_Fall;
    vel = falling = vec(0, 0, 0);
    floor = vec(0, 0, 1);
}

vec physent::feetpos(float offset) const
{
    return vec(o).addz(offset - eyeheight);
}
vec physent::headpos(float offset) const
{
    return vec(o).addz(offset);
}

bool physent::crouched() const
{
    return std::fabs(eyeheight - maxheight*crouchheight) < 1e-4f;
}

//modelattach

modelattach::modelattach() :
                tag(nullptr),
                name(nullptr),
                anim(-1),
                basetime(0),
                pos(nullptr),
                m(nullptr)
{
}

modelattach::modelattach(const char *tag, const char *name, int anim, int basetime) :
                tag(tag),
                name(name),
                anim(anim),
                basetime(basetime),
                pos(nullptr),
                m(nullptr)
{
}

modelattach::modelattach(const char *tag, vec *pos) :
                tag(tag),
                name(nullptr),
                anim(-1),
                basetime(0),
                pos(pos),
                m(nullptr)
{
}

//animinfo

animinfo::animinfo() : anim(0), frame(0), range(0), basetime(0), speed(100.0f), varseed(0)
{
}

bool animinfo::operator==(const animinfo &o) const
{
    return frame==o.frame
        && range==o.range
        && (anim&(Anim_SetTime | Anim_Dir)) == (o.anim & (Anim_SetTime | Anim_Dir))
        && (anim & Anim_SetTime || basetime == o.basetime)
        && speed == o.speed;
}

bool animinfo::operator!=(const animinfo &o) const
{
    return frame!=o.frame
        || range!=o.range
        || (anim&(Anim_SetTime | Anim_Dir)) != (o.anim & (Anim_SetTime | Anim_Dir))
        || (!(anim & Anim_SetTime) && basetime != o.basetime)
        || speed != o.speed;
}

//animinterpinfo

animinterpinfo::animinterpinfo() :
                    lastswitch(-1),
                    lastmodel(nullptr)
{
}

void animinterpinfo::reset()
{
    lastswitch = -1;
}

//dynent

dynent::dynent() :
        ragdoll(nullptr),
        query(nullptr),
        lastrendered(0)
{
    reset();
}

dynent::~dynent()
{
    if(ragdoll)
    {
        cleanragdoll(this);
    }
}

void dynent::stopmoving()
{
    k_left = k_right = k_up = k_down = jumping = false;
    move = strafe = crouching = 0;
}

void dynent::reset()
{
    physent::reset();
    stopmoving();
    for(size_t i = 0; i < maxanimparts; ++i)
    {
        animinterp[i].reset();
    }
}

vec dynent::abovehead() const
{
    return vec(o).addz(aboveeye+4);
}

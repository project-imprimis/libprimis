
#ifndef ENTITIES_H_
#define ENTITIES_H_

enum
{
    EntFlag_NoVis      = 1<<0,
    EntFlag_NoShadow   = 1<<1,
    EntFlag_NoCollide  = 1<<2,
    EntFlag_Anim       = 1<<3,
    EntFlag_ShadowMesh = 1<<4,
    EntFlag_Octa       = 1<<5,
    EntFlag_Render     = 1<<6,
    EntFlag_Sound      = 1<<7,
    EntFlag_Spawned    = 1<<8,
};

enum
{
    Collide_None = 0,
    Collide_Ellipse,
    Collide_OrientedBoundingBox,
    Collide_TRI
};

#endif

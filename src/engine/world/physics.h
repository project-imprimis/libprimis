#ifndef PHYSICS_H_
#define PHYSICS_H_

extern int collideinside;

struct CollisionInfo
{
    bool collided;
    vec collidewall;
};

extern bool collide(const physent *d, vec *cwall = nullptr, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f, bool insideplayercol = false);

#endif

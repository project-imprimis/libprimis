#ifndef PHYSICS_H_
#define PHYSICS_H_

extern vec collidewall;
extern int collideinside;

extern bool collide(const physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f, bool playercol = true, bool insideplayercol = false);

#endif

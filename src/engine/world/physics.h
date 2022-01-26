#ifndef PHYSICS_H_
#define PHYSICS_H_

extern vec collidewall;
extern int collideinside;
extern physent *collideplayer;

extern void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space);
extern bool movecamera(physent *pl, const vec &dir, float dist, float stepdist);
extern void dropenttofloor(entity *e);
extern bool droptofloor(vec &o, float radius, float height);

extern void resetclipplanes();

struct clipplanes;

extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f, bool playercol = true, bool insideplayercol = false);
extern void modifyorient(float yaw, float pitch);

extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m);
extern void updatephysstate(physent *d);
extern void cleardynentcache();
extern void updatedynentcache(physent *d);
extern bool entinmap(dynent *d, bool avoidplayers = false);
extern void findplayerspawn(dynent *d, int forceent = -1, int tag = 0);

extern const float gravity;

#endif

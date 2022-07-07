#ifndef RAYCUBE_H_
#define RAYCUBE_H_

extern vec hitsurface;

extern float raycubepos(const vec &o, const vec &ray, vec &hit, float radius = 0, int mode = Ray_ClipMat, int size = 0);
extern float rayent(const vec &o, const vec &ray, float radius, int mode, int size, int &orient, int &ent);
extern float rayfloor  (const vec &o, vec &floor, int mode = 0, float radius = 0);

extern bool insideworld(const vec &o);
extern bool insideworld(const ivec &o);

#endif

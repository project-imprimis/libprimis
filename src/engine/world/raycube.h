#ifndef RAYCUBE_H_
#define RAYCUBE_H_

extern vec hitsurface;

extern float raycubepos(const vec &o, const vec &ray, vec &hit, float radius = 0, int mode = Ray_ClipMat, int size = 0);
extern float rayent(const vec &o, const vec &ray, float radius, int mode, int size, int &orient, int &ent);
extern float rayfloor  (const vec &o, vec &floor, int mode = 0, float radius = 0);

/**
 * @brief Returns whether the passed vec lies inside the bounds of the rootworld.
 *
 * Returns true if all dimensions are within the range [0, rootworld.mapsize).
 * This is inclusive of 0 and exclusive of the mapsize.
 * Since worlds are cubes, all dimensions are measured against the same limits.
 *
 * @param o the vec to check
 *
 * @return whether the vec is inside the world
 */
extern bool insideworld(const vec &o);
extern bool insideworld(const ivec &o);

#endif

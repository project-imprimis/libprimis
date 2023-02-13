#ifndef LIGHT_H_
#define LIGHT_H_

struct vertinfo;

struct surfaceinfo;

extern bvec ambient, skylight, sunlight;
extern float ambientscale, skylightscale, sunlightscale;
extern float sunlightyaw, sunlightpitch;
extern vec sunlightdir;
extern int fullbright, fullbrightlevel;

extern void clearlights();
extern void initlights();
extern void clearlightcache(int id = -1);
extern void brightencube(cube &c);
extern void setsurface(cube &c, int orient, const surfaceinfo &surf, const vertinfo *verts, int numverts);

extern void clearnormals();
extern void resetsmoothgroups();
extern int smoothangle(int id, int angle);
extern void findnormal(const vec &key, int smooth, const vec &surface, vec &v);

#endif

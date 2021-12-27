#ifndef OCTAWORLD_H_
#define OCTAWORLD_H_

extern cube *newcubes(uint face = faceempty, int mat = Mat_Air);
extern cubeext *growcubeext(cubeext *ext, int maxverts);
extern void setcubeext(cube &c, cubeext *ext);
extern cubeext *newcubeext(cube &c, int maxverts = 0, bool init = true);
extern void getcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern void setcubevector(cube &c, int d, int x, int y, int z, const ivec &p);
extern int familysize(const cube &c);
extern void freeocta(cube *c);
extern void validatec(cube *c, int size = 0);
extern ivec lu;
extern int lusize;
extern cube &lookupcube(const ivec &to, int tsize = 0, ivec &ro = lu, int &rsize = lusize);
extern const cube *neighborstack[32];
extern int neighbordepth;
extern int getmippedtexture(const cube &p, int orient);
extern void forcemip(cube &c, bool fixtex = true);
extern bool subdividecube(cube &c, bool fullcheck=true, bool brighten=true);
extern int faceconvexity(const ivec v[4]);
extern int faceconvexity(const ivec v[4], int &vis);
extern int faceconvexity(const vertinfo *verts, int numverts, int size);
extern int faceconvexity(const cube &c, int orient);
extern uint faceedges(const cube &c, int orient);
extern bool flataxisface(const cube &c, int orient);
extern bool collideface(const cube &c, int orient);
extern void genclipbounds(const cube &c, const ivec &co, int size, clipplanes &p);
extern void genclipplanes(const cube &c, const ivec &co, int size, clipplanes &p, bool collide = true, bool noclip = false);
extern bool visibleface(const cube &c, int orient, const ivec &co, int size, ushort mat = Mat_Air, ushort nmat = Mat_Air, ushort matmask = MatFlag_Volume);
extern int classifyface(const cube &c, int orient, const ivec &co, int size);
extern int visibletris(const cube &c, int orient, const ivec &co, int size, ushort vmat = Mat_Air, ushort nmat = Mat_Alpha, ushort matmask = Mat_Alpha);
extern int visibleorient(const cube &c, int orient);
extern void genfaceverts(const cube &c, int orient, ivec v[4]);
extern int calcmergedsize(int orient, const ivec &co, int size, const vertinfo *verts, int numverts);
extern void invalidatemerges(cube &c);
extern void remip();
extern int lookupmaterial(const vec &o);

inline cubeext &ext(cube &c)
{
    return *(c.ext ? c.ext : newcubeext(c));
}

#endif

#ifndef OCTARENDER_H_
#define OCTARENDER_H_

extern ivec worldmin, worldmax;
extern std::vector<tjoint> tjoints;
extern vector<vtxarray *> varoot, valist;
extern int filltjoints;

extern ushort encodenormal(const vec &n);
extern void guessnormals(const vec *pos, int numverts, vec *normals);
extern void reduceslope(ivec &n);
extern void findtjoints();
extern void octarender();
extern void allchanged(bool load = false);
extern void clearvas(cube *c);
extern void destroyva(vtxarray *va, bool reparent = true);
extern void updatevabb(vtxarray *va, bool force = false);
extern void updatevabbs(bool force = false);

#endif

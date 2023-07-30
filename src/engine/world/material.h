#ifndef MATERIAL_H_
#define MATERIAL_H_

struct materialsurface
{
    ivec o;
    ushort csize, rsize;
    ushort material, skip;
    uchar orient, visible;
    uchar ends;
};

struct vtxarray;

extern std::array<std::vector<materialsurface>, 4> watersurfs, waterfallsurfs;

extern int showmat;

extern vec matnormals(int i); //returns one of the six basis vectors for 0 <= i <= 6; 0,0,0 otherwise
extern int findmaterial(const char *name);
extern const char *findmaterialname(int mat);
extern const char *getmaterialdesc(int mat, const char *prefix = "");
extern void genmatsurfs(const cube &c, const ivec &co, int size, std::vector<materialsurface> &matsurfs);
extern void calcmatbb(vtxarray *va, const ivec &co, int size, const std::vector<materialsurface> &matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);
extern void setupmaterials(int start = 0, int len = 0);
extern void rendersolidmaterials();
extern void rendereditmaterials();
extern void renderminimapmaterials();

#endif

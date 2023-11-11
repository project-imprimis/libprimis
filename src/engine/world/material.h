#ifndef MATERIAL_H_
#define MATERIAL_H_

struct materialsurface;
struct vtxarray;

extern std::vector<materialsurface> watersurfs[4], waterfallsurfs[4];

extern int showmat;

extern vec matnormals(int i); //returns one of the six basis vectors for 0 <= i <= 6; 0,0,0 otherwise
extern std::optional<int> findmaterial(const char *name);
extern const char *findmaterialname(int mat);
extern const char *getmaterialdesc(int mat, const char *prefix = "");
extern void genmatsurfs(const cube &c, const ivec &co, int size, std::vector<materialsurface> &matsurfs);
extern void calcmatbb(vtxarray *va, const ivec &co, int size, const std::vector<materialsurface> &matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);
extern void setupmaterials(int start = 0, int len = 0);
extern void rendermaterialmask();
extern void renderliquidmaterials();
extern void rendersolidmaterials();
extern void rendereditmaterials();
extern void renderminimapmaterials();

#endif

#ifndef MATERIAL_H_
#define MATERIAL_H_

struct materialsurface;

extern float matliquidsx1, matliquidsy1, matliquidsx2, matliquidsy2;
extern float matsolidsx1, matsolidsy1, matsolidsx2, matsolidsy2;
extern float matrefractsx1, matrefractsy1, matrefractsx2, matrefractsy2;
extern uint matliquidtiles[lighttilemaxheight], matsolidtiles[lighttilemaxheight];
extern vector<materialsurface> editsurfs, glasssurfs[4], watersurfs[4], waterfallsurfs[4];
inline const vec matnormals[6] =
{
    vec(-1, 0, 0),
    vec( 1, 0, 0),
    vec(0, -1, 0),
    vec(0,  1, 0),
    vec(0, 0, -1),
    vec(0, 0,  1)
};

extern int showmat;

extern int findmaterial(const char *name);
extern const char *findmaterialname(int mat);
extern const char *getmaterialdesc(int mat, const char *prefix = "");
extern void genmatsurfs(const cube &c, const ivec &co, int size, std::vector<materialsurface> &matsurfs);
extern void calcmatbb(vtxarray *va, const ivec &co, int size, std::vector<materialsurface> &matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);
extern void setupmaterials(int start = 0, int len = 0);
extern int findmaterials();
extern void rendermaterialmask();
extern void renderliquidmaterials();
extern void rendersolidmaterials();
extern void rendereditmaterials();
extern void renderminimapmaterials();

#endif

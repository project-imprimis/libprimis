#ifndef MATERIAL_H_
#define MATERIAL_H_

struct materialsurface final
{
    ivec o;
    ushort csize, rsize;
    ushort material, skip;
    uchar orient, visible;
    uchar ends;
};

class vtxarray;

extern std::array<std::vector<materialsurface>, 4> watersurfs, waterfallsurfs;

extern vec matnormals(int i); //returns one of the six basis vectors for 0 <= i <= 6; 0,0,0 otherwise

/** @brief Get bitmask associated with a material name.
 *
 * Given a material name, returns the bitmask ID of the material as an integer.
 * Mappings between number and material are defined in the `materials` global.
 * Only matches a single material name and not combinations of materials.
 *
 * @param name the material name to find
 *
 * @return bitmask id number of the passed name, or -1 if not found
 */
extern int findmaterial(const char *name);

/** @brief Given a material id, returns the name of the material as a C string
 *
 * The mappings between material and number are defined in the `materials` global.
 * Ony matches single materials and not combinations of materials.
 *
 * @param mat the material id to search for
 *
 * @return C string containing the human readable name of that material
 */
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

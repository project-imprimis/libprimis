#ifndef OCTARENDER_H_
#define OCTARENDER_H_

struct vertex
{
    vec pos;
    vec4<uchar> norm;
    vec tc;
    vec4<uchar> tangent;
};

struct materialsurface
{
    ivec o;
    ushort csize, rsize;
    ushort material, skip;
    uchar orient, visible;
    uchar ends;
};

struct vtxarray
{
    vtxarray *parent;
    vector<vtxarray *> children;
    vtxarray *next, *rnext;  // linked list of visible VOBs
    vertex *vdata;           // vertex data
    ushort voffset, eoffset, skyoffset, decaloffset; // offset into vertex data
    ushort *edata, *skydata, *decaldata; // vertex indices
    GLuint vbuf, ebuf, skybuf, decalbuf; // VBOs
    ushort minvert, maxvert; // DRE info
    elementset *texelems, *decalelems;   // List of element indices sets (range) per texture
    materialsurface *matbuf; // buffer of material surfaces
    int verts,
        tris,
        texs,
        alphabacktris, alphaback,
        alphafronttris, alphafront,
        refracttris, refract,
        texmask,
        sky,
        matsurfs, matmask,
        distance, rdistance,
        dyntexs, //dynamic if vscroll presentss
        decaltris, decaltexs;
    ivec o;
    int size;                // location and size of cube.
    ivec geommin, geommax;   // BB of geom
    ivec alphamin, alphamax; // BB of alpha geom
    ivec refractmin, refractmax; // BB of refract geom
    ivec skymin, skymax;     // BB of any sky geom
    ivec watermin, watermax; // BB of any water
    ivec glassmin, glassmax; // BB of any glass
    ivec bbmin, bbmax;       // BB of everything including children
    uchar curvfc, occluded;
    occludequery *query;
    vector<octaentities *> mapmodels, decals;
    vector<grasstri> grasstris;
    int hasmerges, mergelevel;
    int shadowmask;
};

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

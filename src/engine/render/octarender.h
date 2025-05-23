#ifndef OCTARENDER_H_
#define OCTARENDER_H_

struct octaentities;

struct tjoint final
{
    int next;
    ushort offset;
    uchar edge;
};

struct vertex final
{
    vec pos;
    vec4<uchar> norm;
    vec tc;
    vec4<uchar> tangent;
};

struct materialsurface;

struct elementset final
{
    ushort texture;
    union
    {
        struct
        {
            uchar orient, layer;
        } attrs;
        ushort reuse;
    };
    ushort length, minvert, maxvert;
};

struct grasstri final
{
    std::array<vec, 4> v;
    int numv;
    plane surface;
    vec center;
    float radius;
    float minz, maxz;
    ushort texture;
};

constexpr int vasortsize = 64;

class vtxarray final
{
    public:
        vtxarray *parent;
        std::vector<vtxarray *> children;
        vtxarray *next, *rnext;  // linked list of visible VOBs
        vertex *vdata;           // vertex data
        ushort voffset, eoffset, skyoffset, decaloffset; // offset into vertex data
        ushort *edata, *skydata, *decaldata; // vertex indices
        GLuint vbuf, ebuf, skybuf, decalbuf; // VBOs
        ushort minvert, maxvert; // DRE info
        elementset *texelems, *decalelems;   // List of element indices sets (range) per texture
        std::vector<materialsurface> matbuf; // buffer of material surfaces
        int verts,
            tris,
            texs,
            alphabacktris, alphaback,
            alphafronttris, alphafront,
            refracttris, refract,
            texmask,
            matsurfs, matmask,
            distance, rdistance,
            dyntexs, //dynamic if vscroll presentss
            decaltris, decaltexs;
        size_t sky;
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
        std::vector<octaentities *> mapmodels, decals;
        std::vector<grasstri> grasstris;
        int hasmerges, mergelevel;
        int shadowmask;
        void updatevabb(bool force = false);
        void findspotshadowvas(std::array<vtxarray *, vasortsize> &vasort);
        void findrsmshadowvas(std::array<vtxarray *, vasortsize> &vasort);
        void findcsmshadowvas(std::array<vtxarray *, vasortsize> &vasort);
    private:
};

extern ivec worldmin, worldmax;
extern std::vector<tjoint> tjoints;
extern std::vector<vtxarray *> varoot, valist;
extern int filltjoints;
extern int allocva;

extern ushort encodenormal(const vec &n);
extern void guessnormals(const vec *pos, int numverts, vec *normals);
extern void reduceslope(ivec &n);
extern void clearvas(std::array<cube, 8> &c);
extern void destroyva(vtxarray *va, bool reparent = true);
extern void updatevabbs(bool force = false);

#endif

#ifndef OCTARENDER_H_
#define OCTARENDER_H_

struct renderstate
{
    bool colormask, depthmask;
    int alphaing;
    GLuint vbuf;
    bool vattribs, vquery;
    vec colorscale;
    float alphascale;
    float refractscale;
    vec refractcolor;
    int globals, tmu;
    GLuint textures[7];
    Slot *slot, *texgenslot;
    VSlot *vslot, *texgenvslot;
    vec2 texgenscroll;
    int texgenorient, texgenmillis;

    renderstate();
};

struct decalrenderer;

class vacollect;

class vtxarray
{
    public:
        vtxarray *parent;
        vector<vtxarray *> children;
        vtxarray *next, *rnext;  // linked list of visible VOBs
        vertex *vdata;           // vertex data
        ushort eoffset, skyoffset, decaloffset; // offset into vertex data
        ushort *edata, *skydata; // vertex indices
        GLuint vbuf, ebuf, skybuf, decalbuf; // VBOs
        elementset *texelems;
        materialsurface *matbuf; // buffer of material surfaces
        int verts,
            tris,
            texs,
            alphabacktris,
            alphafronttris,
            refracttris,
            sky,
            matsurfs,
            distance, rdistance,
            dyntexs, //dynamic if vscroll presentss
            decaltris;
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
        void updatevabb(bool force = false);
        void findspotshadowvas();
        void findrsmshadowvas();
        void findcsmshadowvas();
        void finddecals();
        void mergedecals();
        void findshadowvas();
        void calcgeombb(const ivec &co, int size);

        bool bbinsideva(const ivec &bo, const ivec &br);
        void renderva(renderstate &cur, int pass = 0, bool doquery = false);
        void drawvatris(GLsizei numindices, int offset);
        void drawvaskytris();
        void calcmatbb(const ivec &co, int size, std::vector<materialsurface> &matsurfs); //from material.cpp
        void gengrassquads(); //from grass.cpp

        template<bool fullvis, bool resetocclude>
        void findvisiblevas();

        void changevbuf(decalrenderer &cur);
        void changevbuf(renderstate &cur, int pass);
        void setupdata(vacollect* vacol);
        void vavbo(GLuint vbo, int type, uchar * data);
        void renderoutline();
        void rendergeom();

        //basically a destructor
        void destroyva(bool reparent = true);

    private:
        ushort voffset;
        ushort *decaldata; // vertex indices
        ushort minvert, maxvert; // DRE info
        elementset *decalelems;   // List of element indices sets (range) per texture
        int alphaback, alphafront;
        int refract;
        int texmask;
        int matmask;
        int decaltexs;

        float vadist(const vec &p);
        void addvisibleva();
        void mergetexs(renderstate &cur, elementset *texs = nullptr, int offset = 0);
        void renderzpass(renderstate &cur);
        void addshadowva(float dist);
        uchar * addvbo(int type, int numelems, int elemsize);
        void renderquery(renderstate &cur, bool full = true);
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
extern void updatevabbs(bool force = false);

#endif

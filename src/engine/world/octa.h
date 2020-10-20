// 6-directional octree heightfield map format

struct elementset
{
    ushort texture;
    union
    {
        struct
        {
            uchar orient, layer;
        };
        ushort reuse;
    };
    ushort length, minvert, maxvert;
};

struct materialsurface
{
    ivec o;
    ushort csize, rsize;
    ushort material, skip;
    uchar orient, visible;
    uchar ends;
};

struct vertinfo
{
    ushort x, y, z, norm;

    void setxyz(ushort a, ushort b, ushort c)
    {
        x = a;
        y = b;
        z = c;
    }

    void setxyz(const ivec &v)
    {
        setxyz(v.x, v.y, v.z);
    }

    void set(ushort a, ushort b, ushort c, ushort n = 0)
    {
        setxyz(a, b, c);
        norm = n;
    }

    void set(const ivec &v, ushort n = 0)
    {
        set(v.x, v.y, v.z, n);
    }

    ivec getxyz() const
    {
        return ivec(x, y, z);
    }
};

enum
{
    BlendLayer_Top    = (1<<5),
    BlendLayer_Bottom = (1<<6),
    BlendLayer_Blend  = BlendLayer_Top|BlendLayer_Bottom,
};

const int Face_MaxVerts = 15;

struct surfaceinfo
{
    uchar verts, numverts;

    int totalverts() const
    {
        return numverts&Face_MaxVerts;
    }

    bool used() const
    {
        return (numverts&~BlendLayer_Top) != 0;
    }

    void clear()
    {
        numverts = (numverts&Face_MaxVerts) | BlendLayer_Top;
    }

    void brighten()
    {
        clear();
    }
};

const surfaceinfo topsurface = {0, BlendLayer_Top};

struct grasstri
{
    vec v[4];
    int numv;
    plane surface;
    vec center;
    float radius;
    float minz, maxz;
    ushort texture;
};

struct occludequery
{
    void *owner;
    GLuint id;
    int fragments;
};

struct vtxarray;

struct octaentities
{
    vector<int> mapmodels, decals, other;
    occludequery *query;
    octaentities *next, *rnext;
    int distance;
    ivec o;
    int size;
    ivec bbmin, bbmax;

    octaentities(const ivec &o, int size) : query(0), o(o), size(size), bbmin(o), bbmax(o)
    {
        bbmin.add(size);
    }
};

enum
{
    Occlude_Nothing = 0,
    Occlude_Geom,
    Occlude_BB,
    Occlude_Parent
};

enum
{
    Merge_Origin = 1<<0,
    Merge_Part   = 1<<1,
    Merge_Use    = 1<<2
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

struct cube;

struct clipplanes
{
    vec o, r, v[8];
    plane p[12];
    uchar side[12];
    uchar size, visible;
    const cube *owner;
    int version;
};

struct facebounds
{
    ushort u1, u2, v1, v2;

    bool empty() const { return u1 >= u2 || v1 >= v2; }
};

struct tjoint
{
    int next;
    ushort offset;
    uchar edge;
};

struct cubeext
{
    vtxarray *va;            // vertex array for children, or NULL
    octaentities *ents;      // map entities inside cube
    surfaceinfo surfaces[6]; // render info for each surface
    int tjoints;             // linked list of t-joints
    uchar maxverts;          // allocated space for verts

    vertinfo *verts()
    {
        return reinterpret_cast<vertinfo *>(this+1);
    }
};

struct cube
{
    cube *children;          // points to 8 cube structures which are its children, or NULL. -Z first, then -Y, -X
    cubeext *ext;            // extended info for the cube
    union
    {
        uchar edges[12];     // edges of the cube, each uchar is 2 4bit values denoting the range.
                             // see documentation jpgs for more info.
        uint faces[3];       // 4 edges of each dimension together representing 2 perpendicular faces
    };
    ushort texture[6];       // one for each face. same order as orient.
    ushort material;         // empty-space material
    uchar merged;            // merged faces of the cube
    union
    {
        uchar escaped;       // mask of which children have escaped merges
        uchar visible;       // visibility info for faces
    };
};

struct selinfo
{
    int corner;
    int cx, cxs, cy, cys;
    ivec o, s; //two corners of the selection (s is an _offset_ vector
    int grid, orient;
    selinfo() : corner(0), cx(0), cxs(0), cy(0), cys(0), o(0, 0, 0), s(0, 0, 0), grid(8), orient(0) {}
    int size() const
    {
        return s.x*s.y*s.z;
    }

    int us(int d) const
    {
        return s[d]*grid;
    }

    bool operator==(const selinfo &sel) const
    {
        return o==sel.o && s==sel.s && grid==sel.grid && orient==sel.orient;
    }

    bool validate()
    {
        extern int worldsize;
        if(grid <= 0 || grid >= worldsize)
        {
            return false;
        }
        if(o.x >= worldsize || o.y >= worldsize || o.z >= worldsize)
        {
            return false;
        }
        if(o.x < 0)
        {
            s.x -= (grid - 1 - o.x)/grid;
            o.x = 0;
        }
        if(o.y < 0)
        {
            s.y -= (grid - 1 - o.y)/grid;
            o.y = 0;
        }
        if(o.z < 0)
        {
            s.z -= (grid - 1 - o.z)/grid;
            o.z = 0;
        }
        s.x = std::clamp(s.x, 0, (worldsize - o.x)/grid);
        s.y = std::clamp(s.y, 0, (worldsize - o.y)/grid);
        s.z = std::clamp(s.z, 0, (worldsize - o.z)/grid);
        return s.x > 0 && s.y > 0 && s.z > 0;
    }
};

struct block3
{
    ivec o, s;
    int grid, orient;
    block3() {}
    block3(const selinfo &sel) : o(sel.o), s(sel.s), grid(sel.grid), orient(sel.orient) {}

    cube *c()
    {
        return reinterpret_cast<cube *>(this+1);
    }

    int size() const
    {
        return s.x*s.y*s.z;
    }
};

struct editinfo
{
    block3 *copy;
    editinfo() : copy(NULL) {}
};


struct prefabheader
{
    char magic[4];
    int version;
};

struct undoent
{
    int i;
    entity e;
};

struct undoblock // undo header, all data sits in payload
{
    undoblock *prev, *next;
    int size, timestamp, numents; // if numents is 0, is a cube undo record, otherwise an entity undo record

    block3 *block()
    {
        return reinterpret_cast<block3 *>(this + 1);
    }

    uchar *gridmap()
    {
        block3 *ub = block();
        return reinterpret_cast<uchar *>(ub->c() + ub->size());
    }

    undoent *ents()
    {
        return reinterpret_cast<undoent *>(this + 1);
    }
};

extern cube *worldroot;             // the world data. only a ptr to 8 cubes (ie: like cube.children above)
extern int wtris, wverts,
           vtris, vverts,
           glde, gbatches,
           rplanes;
extern int allocnodes, allocva, selchildcount, selchildmat;

const uint faceempty = 0;             // all edges in the range (0,0)
const uint facesolid = 0x80808080;    // all edges in the range (0,8)

//returns if the cube is empty (face 0 does not exist)
//note that a non-empty (but distorted) cube missing faces for an axis is impossible
//unless there are no faces at all (impossible to construct a 3d cube otherwise)
inline bool iscubeempty(cube c)
{
    return c.faces[0]==faceempty;
}

//returns if the cube passed is entirely solid (no distortions)
inline bool iscubesolid(cube c)
{
    return (c).faces[0]==facesolid &&
           (c).faces[1]==facesolid &&
           (c).faces[2]==facesolid; //check all three
}

//sets the faces to a given value `face` given
inline void setcubefaces(cube &c, uint face)
{
    c.faces[0] = c.faces[1] = c.faces[2] = face;
}

#define EDGE_GET(edge, coord) ((coord) ? (edge)>>4 : (edge)&0xF)
#define EDGE_SET(edge, coord, val) ((edge) = ((coord) ? ((edge)&0xF)|((val)<<4) : ((edge)&0xF0)|(val)))

#define CUBE_EDGE(c, d, x, y) ((c).edges[(((d)<<2)+((y)<<1)+(x))])

inline int octadim(int d)
{
    return 1<<d;
}

#define OCTA_COORD(d, i)     (((i)&octadim(d))>>(d))
#define OPPOSITE_OCTA(d, i)  ((i)^octadim(D[d]))
#define OCTA_INDEX(d,x,y,z)  (((z)<<D[d])+((y)<<C[d])+((x)<<R[d]))
#define OCTA_STEP(x, y, z, scale) (((((z)>>(scale))&1)<<2) | ((((y)>>(scale))&1)<<1) | (((x)>>(scale))&1))

//note that these macros actually loop in the opposite order: e.g. loopxy runs a for loop of x inside y
#define LOOP_XY(b)        for(int y = 0; y < (b).s[C[DIMENSION((b).orient)]]; ++y) for(int x = 0; x < (b).s[R[DIMENSION((b).orient)]]; ++x)
#define LOOP_XYZ(b, r, f) { for(int z = 0; z < (b).s[D[DIMENSION((b).orient)]]; ++z) LOOP_XY((b)) { cube &c = blockcube(x,y,z,b,r); f; } }
#define LOOP_SEL_XYZ(f)    { if(local) makeundo(); LOOP_XYZ(sel, sel.grid, f); changed(sel); }
#define SELECT_CUBE(x, y, z) blockcube(x, y, z, sel, sel.grid)

// guard against subdivision
#define PROTECT_SEL(f) { undoblock *_u = newundocube(sel); f; if(_u) { pasteundoblock(_u->block(), _u->gridmap()); freeundo(_u); } }

inline uchar octaboxoverlap(const ivec &o, int size, const ivec &bbmin, const ivec &bbmax)
{
    uchar p = 0xFF; // bitmask of possible collisions with octants. 0 bit = 0 octant, etc
    ivec mid = ivec(o).add(size);
    if(mid.z <= bbmin.z)
    {
        p &= 0xF0; // not in a -ve Z octant
    }
    else if(mid.z >= bbmax.z)
    {
        p &= 0x0F; // not in a +ve Z octant
    }
    if(mid.y <= bbmin.y)
    {
        p &= 0xCC; // not in a -ve Y octant
    }
    else if(mid.y >= bbmax.y)
    {
        p &= 0x33; // etc..
    }
    if(mid.x <= bbmin.x)
    {
        p &= 0xAA;
    }
    else if(mid.x >= bbmax.x)
    {
        p &= 0x55;
    }
    return p;
}

#define LOOP_OCTA_BOX(o, size, bbmin, bbmax) uchar possible = octaboxoverlap(o, size, bbmin, bbmax); for(int i = 0; i < 8; ++i) if(possible&(1<<i))
#define LOOP_OCTA_BOX_SIZE(o, size, bborigin, bbsize) uchar possible = octaboxoverlap(o, size, bborigin, ivec(bborigin).add(bbsize)); for(int i = 0; i < 8; ++i) if(possible&(1<<i))

enum
{
    Orient_Left = 0,
    Orient_Right,
    Orient_Back,
    Orient_Front,
    Orient_Bottom,
    Orient_Top,
    Orient_Any
};

#define DIMENSION(orient)  ((orient)>>1)
#define DIM_COORD(orient)  ((orient)&1)
#define OPPOSITE(orient)   ((orient)^1)

enum
{
    ViewFrustumCull_FullyVisible = 0,
    ViewFrustumCull_PartlyVisible,
    ViewFrustumCull_Fogged,
    ViewFrustumCull_NotVisible,
};

#define GENCUBEVERTS(x0,x1, y0,y1, z0,z1) \
    GENCUBEVERT(0, x1, y1, z0) \
    GENCUBEVERT(1, x0, y1, z0) \
    GENCUBEVERT(2, x0, y1, z1) \
    GENCUBEVERT(3, x1, y1, z1) \
    GENCUBEVERT(4, x1, y0, z1) \
    GENCUBEVERT(5, x0, y0, z1) \
    GENCUBEVERT(6, x0, y0, z0) \
    GENCUBEVERT(7, x1, y0, z0)

#define GENFACEVERTX(o,n, x,y,z, xv,yv,zv) GENFACEVERT(o,n, x,y,z, xv,yv,zv)
#define GENFACEVERTSX(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEORIENT(0, GENFACEVERTX(0,0, x0,y1,z1, d0,r1,c1), GENFACEVERTX(0,1, x0,y1,z0, d0,r1,c0), GENFACEVERTX(0,2, x0,y0,z0, d0,r0,c0), GENFACEVERTX(0,3, x0,y0,z1, d0,r0,c1)) \
    GENFACEORIENT(1, GENFACEVERTX(1,0, x1,y1,z1, d1,r1,c1), GENFACEVERTX(1,1, x1,y0,z1, d1,r0,c1), GENFACEVERTX(1,2, x1,y0,z0, d1,r0,c0), GENFACEVERTX(1,3, x1,y1,z0, d1,r1,c0))
#define GENFACEVERTY(o,n, x,y,z, xv,yv,zv) GENFACEVERT(o,n, x,y,z, xv,yv,zv)
#define GENFACEVERTSY(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEORIENT(2, GENFACEVERTY(2,0, x1,y0,z1, c1,d0,r1), GENFACEVERTY(2,1, x0,y0,z1, c0,d0,r1), GENFACEVERTY(2,2, x0,y0,z0, c0,d0,r0), GENFACEVERTY(2,3, x1,y0,z0, c1,d0,r0)) \
    GENFACEORIENT(3, GENFACEVERTY(3,0, x0,y1,z0, c0,d1,r0), GENFACEVERTY(3,1, x0,y1,z1, c0,d1,r1), GENFACEVERTY(3,2, x1,y1,z1, c1,d1,r1), GENFACEVERTY(3,3, x1,y1,z0, c1,d1,r0))
#define GENFACEVERTZ(o,n, x,y,z, xv,yv,zv) GENFACEVERT(o,n, x,y,z, xv,yv,zv)
#define GENFACEVERTSZ(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEORIENT(4, GENFACEVERTZ(4,0, x0,y0,z0, r0,c0,d0), GENFACEVERTZ(4,1, x0,y1,z0, r0,c1,d0), GENFACEVERTZ(4,2, x1,y1,z0, r1,c1,d0), GENFACEVERTZ(4,3, x1,y0,z0, r1,c0,d0)) \
    GENFACEORIENT(5, GENFACEVERTZ(5,0, x0,y0,z1, r0,c0,d1), GENFACEVERTZ(5,1, x1,y0,z1, r1,c0,d1), GENFACEVERTZ(5,2, x1,y1,z1, r1,c1,d1), GENFACEVERTZ(5,3, x0,y1,z1, r0,c1,d1))
#define GENFACEVERTSXY(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEVERTSX(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEVERTSY(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1)
#define GENFACEVERTS(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEVERTSXY(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEVERTSZ(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1)

struct undolist
{
    undoblock *first, *last;

    undolist() : first(NULL), last(NULL) {}

    bool empty() { return !first; }

    void add(undoblock *u)
    {
        u->next = NULL;
        u->prev = last;
        if(!first)
        {
            first = last = u;
        }
        else
        {
            last->next = u;
            last = u;
        }
    }

    undoblock *popfirst()
    {
        undoblock *u = first;
        first = first->next;
        if(first)
        {
            first->prev = NULL;
        }
        else
        {
            last = NULL;
        }
        return u;
    }

    undoblock *poplast()
    {
        undoblock *u = last;
        last = last->prev;
        if(last)
        {
            last->next = NULL;
        }
        else
        {
            first = NULL;
        }
        return u;
    }
};

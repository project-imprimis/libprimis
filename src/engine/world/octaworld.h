#ifndef OCTAWORLD_H_
#define OCTAWORLD_H_

enum BlendMapLayers
{
    BlendLayer_Top    = (1<<5),
    BlendLayer_Bottom = (1<<6),
    BlendLayer_Blend  = BlendLayer_Top|BlendLayer_Bottom,
};

struct prefab;

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

struct occludequery
{
    void *owner;
    GLuint id;
    int fragments;
};

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

enum OcclusionLevels
{
    Occlude_Nothing = 0,
    Occlude_Geom,
    Occlude_BB,
    Occlude_Parent
};

enum CubeMerges
{
    Merge_Origin = 1<<0,
    Merge_Part   = 1<<1,
    Merge_Use    = 1<<2
};

class cube;

struct clipplanes
{
    vec o, r, v[8];
    plane p[12];
    uchar side[12];
    uchar size, visible;
    const cube *owner;
    int version;
};

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

struct vtxarray;

struct cubeext
{
    vtxarray *va;            /**< Vertex array for children, or nullptr. */
    octaentities *ents;      /**< Map entities inside cube. */
    surfaceinfo surfaces[6]; // render info for each surface
    int tjoints;             // linked list of t-joints
    uchar maxverts;          // allocated space for verts

    vertinfo *verts()
    {
        return reinterpret_cast<vertinfo *>(this+1);
    }
};

enum CubeFaceOrientation
{
    Orient_Left = 0,
    Orient_Right,
    Orient_Back,
    Orient_Front,
    Orient_Bottom,
    Orient_Top,
    Orient_Any
};

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

#define OCTA_COORD(d, i)     (((i)&octadim(d))>>(d))
#define OPPOSITE_OCTA(d, i)  ((i)^octadim(D[d]))
#define OCTA_INDEX(d,x,y,z)  (((z)<<D[d])+((y)<<C[d])+((x)<<R[d]))
#define OCTA_STEP(x, y, z, scale) (((((z)>>(scale))&1)<<2) | ((((y)>>(scale))&1)<<1) | (((x)>>(scale))&1))

extern int wtris, wverts,
           vtris, vverts,
           glde, gbatches,
           rplanes;

extern int allocnodes, allocva;

enum
{
    ViewFrustumCull_FullyVisible = 0,
    ViewFrustumCull_PartlyVisible,
    ViewFrustumCull_Fogged,
    ViewFrustumCull_NotVisible,
};

struct prefabheader
{
    char magic[4];
    int version;
};


extern cube *newcubes(uint face = faceempty, int mat = Mat_Air);
extern cubeext *growcubeext(cubeext *ext, int maxverts);
extern void setcubeext(cube &c, cubeext *ext);
extern cubeext *newcubeext(cube &c, int maxverts = 0, bool init = true);
extern void getcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern void setcubevector(cube &c, int d, int x, int y, int z, const ivec &p);
extern int familysize(const cube &c);
extern void validatec(cube *c, int size = 0);

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
extern void mincubeface(const cube &cu, int orient, const ivec &o, int size, const facebounds &orig, facebounds &cf, ushort nmat, ushort matmask);

inline cubeext &ext(cube &c)
{
    return *(c.ext ? c.ext : newcubeext(c));
}

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

#endif

#ifndef OCTAWORLD_H_
#define OCTAWORLD_H_

#define EDGE_GET(edge, coord) ((coord) ? (edge)>>4 : (edge)&0xF)
#define EDGE_SET(edge, coord, val) ((edge) = ((coord) ? ((edge)&0xF)|((val)<<4) : ((edge)&0xF0)|(val)))

#define CUBE_EDGE(c, d, x, y) ((c).edges[(((d)<<2)+((y)<<1)+(x))])

inline int oppositeorient(int orient)
{
    return orient^1;
}

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

struct octaentities
{
    std::vector<int> mapmodels, //set of indices refering to a position in the getentities() vector corresponding to a mapmodel
                     decals,    //set of indices refering to a position in the getentities() vector corresponding to a decal
                     other;     //set of indices refering to a position in the getentities() vector corresponding to a non-mapmodel non-decal entity (sound etc.)
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
    vec o, r;
    std::array<vec, 8> v;
    std::array<plane, 12> p;
    std::array<uchar, 12> side;
    uchar size, //should always be between 0..11 (a valid index to p/side arrays above)
          visible;
    const cube *owner;
    int version;

    void clear()
    {
        o = vec(0,0,0);
        r = vec(0,0,0);
        for(int i = 0; i < 8; ++i)
        {
          v[i] = vec(0,0,0);
        }
        for(int i = 0; i < 12; ++i)
        {
            p[i] = plane();
            side[i] = 0;
        }
        size = 0;
        visible = 0;
        owner = nullptr;
        version = 0;
    }
};

struct surfaceinfo
{
    uchar verts, numverts;

    surfaceinfo() : verts(0), numverts(BlendLayer_Top)
    {
    }
    surfaceinfo(uchar verts, uchar num) : verts(verts), numverts(num)
    {
    }

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

struct vtxarray;

struct cubeext
{
    vtxarray *va;            /**< Vertex array for children, or nullptr. */
    octaentities *ents;      /**< Map entities inside cube. */
    std::array<surfaceinfo, 6> surfaces; // render info for each surface
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

extern uchar octaboxoverlap(const ivec &o, int size, const ivec &bbmin, const ivec &bbmax);

#define LOOP_OCTA_BOX(o, size, bbmin, bbmax) uchar possible = octaboxoverlap(o, size, bbmin, bbmax); for(int i = 0; i < 8; ++i) if(possible&(1<<i))

#define OCTA_COORD(d, i)     (((i)&octadim(d))>>(d))
#define OCTA_STEP(x, y, z, scale) (((((z)>>(scale))&1)<<2) | ((((y)>>(scale))&1)<<1) | (((x)>>(scale))&1))

extern int wtris, wverts,
           vtris, vverts,
           glde, gbatches,
           rplanes;

extern int allocnodes;

enum
{
    ViewFrustumCull_FullyVisible = 0,
    ViewFrustumCull_PartlyVisible,
    ViewFrustumCull_Fogged,
    ViewFrustumCull_NotVisible,
};

extern std::array<cube, 8> *newcubes(uint face = faceempty, int mat = Mat_Air);
extern cubeext *growcubeext(cubeext *ext, int maxverts);
extern void setcubeext(cube &c, cubeext *ext);
extern cubeext *newcubeext(cube &c, int maxverts = 0, bool init = true);
extern void getcubevector(const cube &c, int d, int x, int y, int z, ivec &p);
extern void setcubevector(cube &c, int d, int x, int y, int z, const ivec &p);
extern int familysize(const cube &c);
extern void freeocta(std::array<cube, 8> *&c);
extern void validatec(std::array<cube, 8> *&c, int size = 0);

extern const cube *neighborstack[32];
extern int neighbordepth;
extern int getmippedtexture(const cube &p, int orient);
extern void forcemip(cube &c, bool fixtex = true);
extern bool subdividecube(cube &c, bool fullcheck=true, bool brighten=true);
extern int faceconvexity(const std::array<ivec, 4> &v);
extern int faceconvexity(const std::array<ivec, 4> &v, int &vis);
extern int faceconvexity(const vertinfo *verts, int numverts, int size);
extern int faceconvexity(const cube &c, int orient);
extern uint faceedges(const cube &c, int orient);
extern bool flataxisface(const cube &c, int orient);
extern void genclipbounds(const cube &c, const ivec &co, int size, clipplanes &p);
extern void genclipplanes(const cube &c, const ivec &co, int size, clipplanes &p, bool collide = true, bool noclip = false);
extern bool visibleface(const cube &c, int orient, const ivec &co, int size, ushort mat = Mat_Air, ushort nmat = Mat_Air, ushort matmask = MatFlag_Volume);
extern int classifyface(const cube &c, int orient, const ivec &co, int size);
extern int visibletris(const cube &c, int orient, const ivec &co, int size, ushort vmat = Mat_Air, ushort nmat = Mat_Alpha, ushort matmask = Mat_Alpha);
extern int visibleorient(const cube &c, int orient);
extern void genfaceverts(const cube &c, int orient, std::array<ivec, 4> &v);
extern int calcmergedsize(int orient, const ivec &co, int size, const vertinfo *verts, int numverts);
extern void invalidatemerges(cube &c);
extern void remip();

inline cubeext &ext(cube &c)
{
    return *(c.ext ? c.ext : newcubeext(c));
}

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

// core world management routines

/* octaworld.cpp: the octree world
 *
 * the octree world structure, consisting of nested cube objects (with tex, geom info)
 * is defined here
 *
 * unlike vector worlds (like most engines), the octree relies on different mathmatical
 * semantics due to not being vector-based
 *
 * also handles geometry simplification, where octree nodes are combined to reduce rendering
 * overhead
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include "light.h"
#include "octacube.h"
#include "octaworld.h"
#include "raycube.h"
#include "world.h"

#include "interface/console.h"
#include "interface/control.h"

#include "render/octarender.h"
#include "render/renderwindow.h"

int allocnodes = 0;

const uchar faceedgesidx[6][4] = // ordered edges surrounding each orient
{//0..1 = row edges, 2..3 = column edges
    { 4,  5,  8, 10 },
    { 6,  7,  9, 11 },
    { 8,  9,  0, 2 },
    { 10, 11, 1, 3 },
    { 0,  1,  4, 6 },
    { 2,  3,  5, 7 },
};

cubeworld rootworld;

cubeext *growcubeext(cubeext *old, int maxverts)
{
    cubeext *ext = reinterpret_cast<cubeext *>(new uchar[sizeof(cubeext) + maxverts*sizeof(vertinfo)]);
    if(old)
    {
        ext->va = old->va;
        ext->ents = old->ents;
        ext->tjoints = old->tjoints;
    }
    else
    {
        ext->va = nullptr;
        ext->ents = nullptr;
        ext->tjoints = -1;
    }
    ext->maxverts = maxverts;
    return ext;
}

void setcubeext(cube &c, cubeext *ext)
{
    cubeext *old = c.ext;
    if(old == ext)
    {
        return;
    }
    c.ext = ext;
    if(old)
    {
        delete[] reinterpret_cast<uchar *>(old);
    }
}

cubeext *newcubeext(cube &c, int maxverts, bool init)
{
    if(c.ext && c.ext->maxverts >= maxverts)
    {
        return c.ext;
    }
    cubeext *ext = growcubeext(c.ext, maxverts);
    if(init)
    {
        if(c.ext)
        {
            std::copy(ext->surfaces.begin(), ext->surfaces.end(), c.ext->surfaces.begin());
            std::memcpy(ext->verts(), c.ext->verts(), c.ext->maxverts*sizeof(vertinfo));
        }
        else
        {
            ext->surfaces.fill({0,0});
        }
    }
    setcubeext(c, ext);
    return ext;
}

/**
 * @brief Returns an octet of cubes allocated on the heap.
 *
 * These cubes should be freed with freeocta() to prevent a leak.
 *
 * `allocnodes` is incremented by one for each call of this function.
 *
 * @param face The face values to set the eight cubes
 * @param mat The material mask to assign to the cubes
 *
 * @return a std::array<cube, 8> pointer pointing to an array containing the created cubes
 */
std::array<cube, 8> *newcubes(uint face, int mat)
{
    std::array<cube, 8> *c = new std::array<cube, 8> ;
    for(int i = 0; i < 8; ++i)
    {
        (*c)[i].children = nullptr;
        (*c)[i].ext = nullptr;
        (*c)[i].visible = 0;
        (*c)[i].merged = 0;
        setcubefaces((*c)[i], face);
        for(int l = 0; l < 6; ++l) //note this is a loop l (level 4)
        {
            (*c)[i].texture[l] = Default_Geom;
        }
        (*c)[i].material = mat;
    }
    allocnodes++;
    return c;
}

//returns the size of the tree starting from the specified cube going down
//the cube in question is counted as part of the family
int familysize(const cube &c)
{
    int size = 1;
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            size += familysize((*c.children)[i]);
        }
    }
    return size;
}

void freeocta(std::array<cube, 8> *&c)
{
    if(!c)
    {
        return;
    }
    for(int i = 0; i < 8; ++i)
    {
        (*c)[i].discardchildren();
    }
    delete c;
    c = nullptr;
    allocnodes--;
}

void getcubevector(const cube &c, int d, int x, int y, int z, ivec &p)
{
    ivec v(d, x, y, z);
    for(int i = 0; i < 3; ++i)
    {
        p[i] = EDGE_GET(CUBE_EDGE(c, i, v[R[i]], v[C[i]]), v[D[i]]);
    }
}

void setcubevector(cube &c, int d, int x, int y, int z, const ivec &p)
{
    ivec v(d, x, y, z);
    for(int i = 0; i < 3; ++i)
    {
        EDGE_SET(CUBE_EDGE(c, i, v[R[i]], v[C[i]]), v[D[i]], p[i]);
    }
}

static void getcubevector(const cube &c, int i, ivec &p)
{
    p.x = EDGE_GET(CUBE_EDGE(c, 0, (i>>R[0])&1, (i>>C[0])&1), (i>>D[0])&1);
    p.y = EDGE_GET(CUBE_EDGE(c, 1, (i>>R[1])&1, (i>>C[1])&1), (i>>D[1])&1);
    p.z = EDGE_GET(CUBE_EDGE(c, 2, (i>>R[2])&1, (i>>C[2])&1), (i>>D[2])&1);
}

static void setcubevector(cube &c, int i, const ivec &p)
{
    EDGE_SET(CUBE_EDGE(c, 0, (i>>R[0])&1, (i>>C[0])&1), (i>>D[0])&1, p.x);
    EDGE_SET(CUBE_EDGE(c, 1, (i>>R[1])&1, (i>>C[1])&1), (i>>D[1])&1, p.y);
    EDGE_SET(CUBE_EDGE(c, 2, (i>>R[2])&1, (i>>C[2])&1), (i>>D[2])&1, p.z);
}

void optiface(const uchar *p, cube &c)
{
    uint f = *reinterpret_cast<const uint *>(p);
    if(((f>>4)&0x0F0F0F0FU) == (f&0x0F0F0F0FU))
    {
        setcubefaces(c, faceempty);
    }
}

void printcube()
{
    const cube &c = rootworld.lookupcube(lu); // assume this is cube being pointed at
    conoutf(Console_Debug, "= %p = (%d, %d, %d) @ %d", static_cast<const void *>(&c), lu.x, lu.y, lu.z, lusize);
    conoutf(Console_Debug, " x  %.8x", c.faces[0]);
    conoutf(Console_Debug, " y  %.8x", c.faces[1]);
    conoutf(Console_Debug, " z  %.8x", c.faces[2]);
}

void validatec(std::array<cube, 8> *&c, int size)
{
    for(int i = 0; i < 8; ++i)
    {
        if((*c)[i].children)
        {
            if(size<=1)
            {
                setcubefaces((*c)[i], facesolid);
                (*c)[i].discardchildren(true);
            }
            else
            {
                validatec((*c)[i].children, size>>1);
            }
        }
        else if(size > 0x1000)
        {
            subdividecube((*c)[i], true, false);
            validatec((*c)[i].children, size>>1);
        }
        else
        {
            for(int j = 0; j < 3; ++j)
            {
                uint f  = (*c)[i].faces[j],
                     e0 = f&0x0F0F0F0FU,
                     e1 = (f>>4)&0x0F0F0F0FU;
                if(e0 == e1 || ((e1+0x07070707U)|(e1-e0))&0xF0F0F0F0U)
                {
                    setcubefaces((*c)[i], faceempty);
                    break;
                }
            }
        }
    }
}

ivec lu;
int lusize;

cube &cubeworld::lookupcube(const ivec &to, int tsize, ivec &ro, int &rsize)
{
    int tx = std::clamp(to.x, 0, mapsize()-1),
        ty = std::clamp(to.y, 0, mapsize()-1),
        tz = std::clamp(to.z, 0, mapsize()-1);
    int scale = worldscale-1,
        csize = std::abs(tsize);
    cube *c = &(*worldroot)[OCTA_STEP(tx, ty, tz, scale)];
    if(!c)
    {
        return emptycube;
    }
    if(!(csize>>scale))
    {
        do
        {
            if(!c->children)
            {
                if(tsize > 0)
                {
                    do
                    {
                        subdividecube(*c);
                        scale--;
                        c = &(*c->children)[OCTA_STEP(tx, ty, tz, scale)];
                    } while(!(csize>>scale));
                }
                break;
            }
            scale--;
            c = &(*c->children)[OCTA_STEP(tx, ty, tz, scale)];
        } while(!(csize>>scale));
    }
    ro = ivec(tx, ty, tz).mask(~0U<<scale);
    rsize = 1<<scale;
    return *c;
}

int cubeworld::lookupmaterial(const vec &v)
{
    ivec o(v);
    if(!insideworld(o))
    {
        return Mat_Air;
    }
    int scale = worldscale-1;
    cube *c = &(*worldroot)[OCTA_STEP(o.x, o.y, o.z, scale)];
    while(c->children)
    {
        scale--;
        c = &(*c->children)[OCTA_STEP(o.x, o.y, o.z, scale)];
    }
    return c->material;
}

const cube *neighborstack[32];
int neighbordepth = -1;

const cube &cubeworld::neighborcube(int orient, const ivec &co, int size, ivec &ro, int &rsize)
{
    ivec n = co;
    int dim = DIMENSION(orient);
    uint diff = n[dim];
    if(DIM_COORD(orient))
    {
        n[dim] += size;
    }
    else
    {
        n[dim] -= size;
    }
    diff ^= n[dim];
    if(diff >= static_cast<uint>(mapsize()))
    {
        ro = n;
        rsize = size;
        return emptycube;
    }
    int scale = worldscale;
    const cube *nc = &(*worldroot)[0];
    if(neighbordepth >= 0)
    {
        scale -= neighbordepth + 1;
        diff >>= scale;
        do
        {
            scale++;
            diff >>= 1;
        } while(diff);
        nc = neighborstack[worldscale - scale];
    }
    scale--;
    nc = &nc[OCTA_STEP(n.x, n.y, n.z, scale)];
    if(!(size>>scale) && nc->children)
    {
        do
        {
            scale--;
            nc = &(*nc->children)[OCTA_STEP(n.x, n.y, n.z, scale)];
        } while(!(size>>scale) && nc->children);
    }
    ro = n.mask(~0U<<scale);
    rsize = 1<<scale;
    return *nc;
}

/**
 * @brief Returns the index (0-7) of a given cube.
 *
 * for a given dimension (x,y,z) orientation and 0/1 for x, y, and z, returns the
 * cube child index (0-7) in the cube array
 *
 * @param d the dimension to look from
 * @param x position in x direction (0/1)
 * @param y position in y direction (0/1)
 * @param z position in z direction (0/1)
 * @return an integer between 0 and 7 corresponding to the cube child index of the given cube
 */
static int octacubeindex(int d, int x, int y, int z)
{
    return (((z)<<D[d])+
            ((y)<<C[d])+
            ((x)<<R[d]));
}

////////// (re)mip //////////

int getmippedtexture(const cube &p, int orient)
{
    std::array<cube, 8> &c = *p.children;
    int d = DIMENSION(orient),
        dc = DIM_COORD(orient),
        texs[4] = { -1, -1, -1, -1 },
        numtexs = 0;
    for(int x = 0; x < 2; ++x)
    {
        for(int y = 0; y < 2; ++y)
        {
            int n = octacubeindex(d, x, y, dc);
            if(c[n].isempty())
            {
                n = n^octadim(D[d]);
                if(c[n].isempty())
                {
                    continue;
                }
            }
            int tex = c[n].texture[orient];
            if(tex > Default_Sky)
            {
                for(int i = 0; i < numtexs; ++i)
                {
                    if(texs[i] == tex)
                    {
                        return tex;
                    }
                }
            }
            texs[numtexs++] = tex;
        }
    }
    for(int i = numtexs; --i >= 0;) //note reverse iteration
    {
        if(!i || texs[i] > Default_Sky)
        {
            return texs[i];
        }
    }
    return Default_Geom;
}

void forcemip(cube &c, bool fixtex)
{
    std::array<cube, 8> &ch = *c.children;
    setcubefaces(c, faceempty);
    for(int i = 0; i < 8; ++i)
    {
        for(int j = 0; j < 8; ++j)
        {
            int n = i^(j==3 ? 4 : (j==4 ? 3 : j));
            if(!(ch[n].isempty())) // breadth first search for cube near vert
            {
                ivec v;
                getcubevector(ch[n], i, v);
                // adjust vert to parent size
                setcubevector(c, i, ivec(n, v, 8).shr(1));
                break;
            }
        }
    }
    if(fixtex)
    {
        for(int j = 0; j < 6; ++j)
        {
            c.texture[j] = getmippedtexture(c, j);
        }
    }
}

static int midedge(const ivec &a, const ivec &b, int xd, int yd, bool &perfect)
{
    int ax = a[xd],
        ay = a[yd],
        bx = b[xd],
        by = b[yd];
    if(ay==by)
    {
        return ay;
    }
    if(ax==bx)
    {
        perfect = false;
        return ay;
    }
    bool crossx = (ax<8 && bx>8) || (ax>8 && bx<8),
         crossy = (ay<8 && by>8) || (ay>8 && by<8);
    if(crossy && !crossx)
    {
        midedge(a,b,yd,xd,perfect);
        return 8;
    } // to test perfection
    if(ax<=8 && bx<=8)
    {
        return ax>bx ? ay : by;
    }
    if(ax>=8 && bx>=8)
    {
        return ax<bx ? ay : by;
    }
    int risex = (by-ay)*(8-ax)*256,
        s = risex/(bx-ax),
        y = s/256 + ay;
    if(((std::abs(s)&0xFF)!=0) || // ie: rounding error
        (crossy && y!=8) ||
        (y<0 || y>16))
    {
        perfect = false;
    }
    return crossy ? 8 : std::min(std::max(y, 0), 16);
}

static bool crosscenter(const ivec &a, const ivec &b, int xd, int yd)
{
    int ax = a[xd],
        ay = a[yd],
        bx = b[xd],
        by = b[yd];
    return (((ax <= 8 && bx <= 8) || (ax >= 8 && bx >= 8)) && ((ay <= 8 && by <= 8) || (ay >= 8 && by >= 8))) ||
           (ax + bx == 16 && ay + by == 16);
}

bool subdividecube(cube &c, bool fullcheck, bool brighten)
{
    if(c.children)
    {
        return true;
    }
    if(c.ext)
    {
        c.ext->surfaces.fill({0,0});
    }
    if(c.isempty() || c.issolid())
    {
        c.children = newcubes(c.isempty() ? faceempty : facesolid, c.material);
        for(int i = 0; i < 8; ++i)
        {
            for(int l = 0; l < 6; ++l) //note this is a loop l (level 4)
            {
                (*c.children)[i].texture[l] = c.texture[l];
            }
            if(brighten && !(c.isempty()))
            {
                brightencube((*c.children)[i]);
            }
        }
        return true;
    }
    c.children = newcubes(facesolid, c.material);
    std::array<cube, 8> &ch = *c.children;
    bool perfect = true;
    ivec v[8];
    for(int i = 0; i < 8; ++i)
    {
        getcubevector(c, i, v[i]);
        v[i].mul(2);
    }

    for(int j = 0; j < 6; ++j)
    {
        int d = DIMENSION(j),
            z = DIM_COORD(j);
        const ivec &v00 = v[octacubeindex(d, 0, 0, z)],
                   &v10 = v[octacubeindex(d, 1, 0, z)],
                   &v01 = v[octacubeindex(d, 0, 1, z)],
                   &v11 = v[octacubeindex(d, 1, 1, z)];
        int e[3][3];
        // corners
        e[0][0] = v00[d];
        e[0][2] = v01[d];
        e[2][0] = v10[d];
        e[2][2] = v11[d];
        // edges
        e[0][1] = midedge(v00, v01, C[d], d, perfect);
        e[1][0] = midedge(v00, v10, R[d], d, perfect);
        e[1][2] = midedge(v11, v01, R[d], d, perfect);
        e[2][1] = midedge(v11, v10, C[d], d, perfect);
        // center
        bool p1 = perfect,
             p2 = perfect;
        int c1 = midedge(v00, v11, R[d], d, p1),
            c2 = midedge(v01, v10, R[d], d, p2);
        if(z ? c1 > c2 : c1 < c2)
        {
            e[1][1] = c1;
            perfect = p1 && (c1 == c2 || crosscenter(v00, v11, C[d], R[d]));
        }
        else
        {
            e[1][1] = c2;
            perfect = p2 && (c1 == c2 || crosscenter(v01, v10, C[d], R[d]));
        }

        for(int i = 0; i < 8; ++i)
        {
            ch[i].texture[j] = c.texture[j];
            int rd = (i>>R[d])&1,
                cd = (i>>C[d])&1,
                dd = (i>>D[d])&1;
            EDGE_SET(CUBE_EDGE(ch[i], d, 0, 0), z, std::clamp(e[rd][cd] - dd*8, 0, 8));
            EDGE_SET(CUBE_EDGE(ch[i], d, 1, 0), z, std::clamp(e[1+rd][cd] - dd*8, 0, 8));
            EDGE_SET(CUBE_EDGE(ch[i], d, 0, 1), z, std::clamp(e[rd][1+cd] - dd*8, 0, 8));
            EDGE_SET(CUBE_EDGE(ch[i], d, 1, 1), z, std::clamp(e[1+rd][1+cd] - dd*8, 0, 8));
        }
    }

    validatec(c.children);
    if(fullcheck)
    {
        for(int i = 0; i < 8; ++i)
        {
            if(!ch[i].isvalidcube()) // not so good...
            {
                setcubefaces(ch[i], faceempty);
                perfect=false;
            }
        }
    }
    if(brighten)
    {
        for(int i = 0; i < 8; ++i)
        {
            if(!(ch[i].isempty()))
            {
                brightencube(ch[i]);
            }
        }
    }
    return perfect;
}

static bool crushededge(uchar e, int dc)
{
    return dc ? e==0 : e==0x88;
}

int visibleorient(const cube &c, int orient)
{
    for(int i = 0; i < 2; ++i)
    {
        int a = faceedgesidx[orient][i*2 + 0],
            b = faceedgesidx[orient][i*2 + 1];
        for(int j = 0; j < 2; ++j)
        {
            if(crushededge(c.edges[a],j) &&
               crushededge(c.edges[b],j) &&
                touchingface(c, orient))
            {
                return ((a>>2)<<1) + j;
            }
        }
    }
    return orient;
}

VAR(mipvis, 0, 0, 1);

static bool remip(cube &c, const ivec &co, int size)
{
    std::array<cube, 8> *ch = nullptr;
    if(!c.children)
    {
        if(size<<1 <= 0x1000)
        {
            return true;
        }
        subdividecube(c);
        ch = c.children;
    }
    else
    {
        ch = c.children;
    }
    bool perfect = true;
    for(int i = 0; i < 8; ++i)
    {
        ivec o(i, co, size);
        if(!remip((*ch)[i], o, size>>1))
        {
            perfect = false;
        }
    }
    setcubefaces(c, facesolid); // so texmip is more consistent
    for(int j = 0; j < 6; ++j)
    {
        c.texture[j] = getmippedtexture(c, j); // parents get child texs regardless
    }
    if(!perfect)
    {
        return false;
    }
    if(size<<1 > 0x1000)
    {
        return false;
    }
    ushort mat = Mat_Air;
    for(int i = 0; i < 8; ++i)
    {
        mat = (*ch)[i].material;
        if((mat&MatFlag_Clip) == Mat_NoClip || mat&Mat_Alpha)
        {
            if(i > 0)
            {
                return false;
            }
            while(++i < 8)
            {
                if((*ch)[i].material != mat)
                {
                    return false;
                }
            }
            break;
        }
        else if(!((*ch)[i].issolid()))
        {
            while(++i < 8)
            {
                int omat = (*ch)[i].material;
                if((*ch)[i].issolid() ? (omat&MatFlag_Clip) == Mat_NoClip || omat&Mat_Alpha : mat != omat)
                {
                    return false;
                }
            }
            break;
        }
    }
    cube n = c;
    n.ext = nullptr;
    forcemip(n);
    n.children = nullptr;
    if(!subdividecube(n, false, false))
    {
        freeocta(n.children);
        return false;
    }
    std::array<cube, 8> *nh = n.children;
    uchar vis[6] = {0, 0, 0, 0, 0, 0};
    for(int i = 0; i < 8; ++i)
    {
        if((*ch)[i].faces[0] != (*nh)[i].faces[0] ||
           (*ch)[i].faces[1] != (*nh)[i].faces[1] ||
           (*ch)[i].faces[2] != (*nh)[i].faces[2])
        {
            freeocta(nh);
            return false;
        }

        if((*ch)[i].isempty() && (*nh)[i].isempty())
        {
            continue;
        }

        ivec o(i, co, size);
        for(int orient = 0; orient < 6; ++orient)
            if(visibleface((*ch)[i], orient, o, size, Mat_Air, (mat&Mat_Alpha)^Mat_Alpha, Mat_Alpha))
            {
                if((*ch)[i].texture[orient] != n.texture[orient])
                {
                    freeocta(nh);
                    return false;
                }
                vis[orient] |= 1<<i;
            }
    }
    if(mipvis)
    {
        for(int orient = 0; orient < 6; ++orient)
        {
            int mask = 0;
            for(int x = 0; x < 2; ++x)
            {
                for(int y = 0; y < 2; ++y)
                {
                    mask |= 1<<octacubeindex(DIMENSION(orient), x, y, DIM_COORD(orient));
                }
            }
            if(vis[orient]&mask && (vis[orient]&mask)!=mask)
            {
                freeocta(nh);
                return false;
            }
        }
    }
    freeocta(nh);
    c.discardchildren();
    for(int i = 0; i < 3; ++i)
    {
        c.faces[i] = n.faces[i];
    }
    c.material = mat;
    for(int i = 0; i < 6; ++i)
    {
        if(vis[i])
        {
            c.visible |= 1<<i;
        }
    }
    if(c.visible)
    {
        c.visible |= 0x40;
    }
    brightencube(c);
    return true;
}

void cubeworld::remip()
{
    for(int i = 0; i < 8; ++i)
    {
        ivec o(i, ivec(0, 0, 0), mapsize()>>1);
        ::remip((*worldroot)[i], o, mapsize()>>2);
    }
    (*worldroot)[0].calcmerges(&(*worldroot)[0]); //created as result of calcmerges being cube member
}

const ivec cubecoords[8] =
{
    ivec(8, 8, 0),
    ivec(0, 8, 0),
    ivec(0, 8, 8),
    ivec(8, 8, 8),
    ivec(8, 0, 8),
    ivec(0, 0, 8),
    ivec(0, 0, 0),
    ivec(8, 0, 0)
};

//================================================================= GENCUBEVERTS
#define GENCUBEVERTS(x0,x1, y0,y1, z0,z1) \
    GENCUBEVERT(0, x1, y1, z0) \
    GENCUBEVERT(1, x0, y1, z0) \
    GENCUBEVERT(2, x0, y1, z1) \
    GENCUBEVERT(3, x1, y1, z1) \
    GENCUBEVERT(4, x1, y0, z1) \
    GENCUBEVERT(5, x0, y0, z1) \
    GENCUBEVERT(6, x0, y0, z0) \
    GENCUBEVERT(7, x1, y0, z0)

static void gencubevert(const cube &c, int i, vec &v)
{
    switch(i)
    {
        default:
//================================================================== GENCUBEVERT
#define GENCUBEVERT(n, x, y, z) \
        case n: \
            v = vec(EDGE_GET(CUBE_EDGE(c, 0, y, z), x), \
                    EDGE_GET(CUBE_EDGE(c, 1, z, x), y), \
                    EDGE_GET(CUBE_EDGE(c, 2, x, y), z)); \
            break;
        GENCUBEVERTS(0, 1, 0, 1, 0, 1)
#undef GENCUBEVERT
//==============================================================================
    }
}

void genfaceverts(const cube &c, int orient, std::array<ivec, 4> &v)
{
    switch(orient)
    {
        default:
//==================================================== GENFACEORIENT GENFACEVERT
#define GENFACEORIENT(o, v0, v1, v2, v3) \
        case o: v0 v1 v2 v3 break;
#define GENFACEVERT(o, n, x,y,z, xv,yv,zv) \
            v[n] = ivec(EDGE_GET(CUBE_EDGE(c, 0, y, z), x), \
                        EDGE_GET(CUBE_EDGE(c, 1, z, x), y), \
                        EDGE_GET(CUBE_EDGE(c, 2, x, y), z));
        GENFACEVERTS(0, 1, 0, 1, 0, 1, , , , , , )
    #undef GENFACEORIENT
    #undef GENFACEVERT
    }
}
//==============================================================================

#undef GENCUBEVERTS
//==============================================================================

bool flataxisface(const cube &c, int orient)
{
    uint face = c.faces[DIMENSION(orient)];
    if(DIM_COORD(orient))
    {
        face >>= 4;
    }
    return (face&0x0F0F0F0F) == 0x01010101*(face&0x0F);
}

bool collideface(const cube &c, int orient)
{
    if(flataxisface(c, orient))
    {
        uchar r1 = c.edges[faceedgesidx[orient][0]],
              r2 = c.edges[faceedgesidx[orient][1]];
        if(static_cast<uchar>((r1>>4)|(r2&0xF0)) == static_cast<uchar>((r1&0x0F)|(r2<<4)))
        {
            return false;
        }
        uchar c1 = c.edges[faceedgesidx[orient][2]],
              c2 = c.edges[faceedgesidx[orient][3]];
        if(static_cast<uchar>((c1>>4)|(c2&0xF0)) == static_cast<uchar>((c1&0x0F)|(c2<<4)))
        {
            return false;
        }
    }
    return true;
}

int faceconvexity(const std::array<ivec, 4> &v)
{
    ivec n;
    n.cross(ivec(v[1]).sub(v[0]), ivec(v[2]).sub(v[0]));
    return ivec(v[0]).sub(v[3]).dot(n);
    // 1 if convex, -1 if concave, 0 if flat
}

int faceconvexity(const vertinfo *verts, int numverts, int size)
{
    if(numverts < 4)
    {
        return 0;
    }
    ivec v0 = verts[0].getxyz(),
         e1 = verts[1].getxyz().sub(v0),
         e2 = verts[2].getxyz().sub(v0),
         n;
    if(size >= (8<<5))
    {
        if(size >= (8<<10))
        {
            n.cross(e1.shr(10), e2.shr(10));
        }
        else
        {
            n.cross(e1, e2).shr(10);
        }
    }
    else
    {
        n.cross(e1, e2);
    }
    return verts[3].getxyz().sub(v0).dot(n);
}

int faceconvexity(const std::array<ivec, 4> &v, int &vis)
{
    ivec e1, e2, e3, n;
    n.cross((e1 = v[1]).sub(v[0]), (e2 = v[2]).sub(v[0]));
    int convex = (e3 = v[0]).sub(v[3]).dot(n);
    if(!convex)
    {
        if(!ivec().cross(e3, e2))
        {
            if(n)
            {
                vis = 1;
            }
        }
        else if(!n)
        {
            vis = 2;
        }
        return 0;
    }
    return convex;
}

int faceconvexity(const cube &c, int orient)
{
    if(flataxisface(c, orient))
    {
        return 0;
    }
    std::array<ivec, 4> v;
    genfaceverts(c, orient, v);
    return faceconvexity(v);
}

int faceorder(const cube &c, int orient) // gets above 'fv' so that each face is convex
{
    return faceconvexity(c, orient)<0 ? 1 : 0;
}

static void faceedges(const cube &c, int orient, std::array<uchar, 4> &edges)
{
    for(size_t k = 0; k < edges.size(); ++k)
    {
        edges[k] = c.edges[faceedgesidx[orient][k]];
    }
}

uint faceedges(const cube &c, int orient)
{
    union
    {
        std::array<uchar, 4> edges;
        uint face;
    } u;
    faceedges(c, orient, u.edges);
    return u.face;
}


static int genfacevecs(const cube &cu, int orient, const ivec &pos, int size, bool solid, std::array<ivec2, 4> &fvecs, const ivec *v = nullptr)
{
    int i = 0;
    if(solid)
    {
        switch(orient)
        {
        #define GENFACEORIENT(orient, v0, v1, v2, v3) \
            case orient: \
            { \
                if(DIM_COORD(orient)) \
                { \
                    v0 v1 v2 v3 \
                } \
                else \
                { \
                    v3 v2 v1 v0 \
                } \
                break; \
            }
        #define GENFACEVERT(orient, vert, xv,yv,zv, x,y,z) \
            { \
                ivec2 &f = fvecs[i]; \
                x ((xv)<<3); \
                y ((yv)<<3); \
                z ((zv)<<3); \
                i++; \
            }
            GENFACEVERTS(pos.x, pos.x+size, pos.y, pos.y+size, pos.z, pos.z+size, f.x = , f.x = , f.y = , f.y = , (void), (void))
        #undef GENFACEVERT
        }
        return 4;
    }
    std::array<ivec, 4> buf;
    if(!v)
    {
        genfaceverts(cu, orient, buf);
        v = buf.data();
    }
    ivec2 prev(INT_MAX, INT_MAX);
    switch(orient)
    {
    #define GENFACEVERT(orient, vert, sx,sy,sz, dx,dy,dz) \
        { \
            const ivec &e = v[vert]; \
            ivec ef; \
            ef.dx = e.sx; \
            ef.dy = e.sy; \
            ef.dz = e.sz; \
            if(ef.z == DIM_COORD(orient)*8) \
            { \
                ivec2 &f = fvecs[i]; \
                ivec pf; \
                pf.dx = pos.sx; \
                pf.dy = pos.sy; \
                pf.dz = pos.sz; \
                f = ivec2(ef.x*size + (pf.x<<3), ef.y*size + (pf.y<<3)); \
                if(f != prev) \
                { \
                    prev = f; \
                    i++; \
                } \
            } \
        }
        GENFACEVERTS(x, x, y, y, z, z, x, x, y, y, z, z)
    #undef GENFACEORIENT
    #undef GENFACEVERT
    }
    if(fvecs[0] == prev)
    {
        i--;
    }
    return i;
}

static int clipfacevecy(const ivec2 &o, const ivec2 &dir, int cx, int cy, int size, ivec2 &r)
{
    if(dir.x >= 0)
    {
        if(cx <= o.x || cx >= o.x+dir.x)
        {
            return 0;
        }
    }
    else if(cx <= o.x+dir.x || cx >= o.x)
    {
        return 0;
    }
    int t = (o.y-cy) + (cx-o.x)*dir.y/dir.x;
    if(t <= 0 || t >= size)
    {
        return 0;
    }
    r.x = cx;
    r.y = cy + t;
    return 1;
}

static int clipfacevecx(const ivec2 &o, const ivec2 &dir, int cx, int cy, int size, ivec2 &r)
{
    if(dir.y >= 0)
    {
        if(cy <= o.y || cy >= o.y+dir.y)
        {
            return 0;
        }
    }
    else if(cy <= o.y+dir.y || cy >= o.y)
    {
        return 0;
    }
    int t = (o.x-cx) + (cy-o.y)*dir.x/dir.y;
    if(t <= 0 || t >= size)
    {
        return 0;
    }
    r.x = cx + t;
    r.y = cy;
    return 1;
}

//param rvecs: an array of ivec2s
static int clipfacevec(const ivec2 &o, const ivec2 &dir, int cx, int cy, int size, ivec2 *rvecs)
{
    int r = 0;
    if(o.x >= cx && o.x <= cx+size &&
       o.y >= cy && o.y <= cy+size &&
       ((o.x != cx && o.x != cx+size) || (o.y != cy && o.y != cy+size)))
    {
        rvecs[0] = o;
        r++;
    }
    r += clipfacevecx(o, dir, cx, cy, size, rvecs[r]);
    r += clipfacevecx(o, dir, cx, cy+size, size, rvecs[r]);
    r += clipfacevecy(o, dir, cx, cy, size, rvecs[r]);
    r += clipfacevecy(o, dir, cx+size, cy, size, rvecs[r]);
    return r;
}

static bool insideface(const ivec2 *p, int nump, const ivec2 *o, int numo)
{
    int bounds = 0;
    ivec2 prev = o[numo-1];
    for(int i = 0; i < numo; ++i)
    {
        const ivec2 &cur = o[i];
        ivec2 dir = ivec2(cur).sub(prev);
        int offset = dir.cross(prev);
        for(int j = 0; j < nump; ++j)
        {
            if(dir.cross(p[j]) > offset)
            {
                return false;
            }
        }
        bounds++;
        prev = cur;
    }
    return bounds>=3;
}

static int clipfacevecs(const ivec2 *o, int numo, int cx, int cy, int size, ivec2 *rvecs)
{
    cx <<= 3;
    cy <<= 3;
    size <<= 3;
    int r = 0;
    if(numo <= 0)
    {
        logoutf("Invalid clipface index %d\n", numo);
        return 0; //protection agains numo negative array access
    }
    ivec2 prev = o[numo-1];
    for(int i = 0; i < numo; ++i)
    {
        const ivec2 &cur = o[i];
        r += clipfacevec(prev, ivec2(cur).sub(prev), cx, cy, size, &rvecs[r]);
        prev = cur;
    }
    ivec2 corner[4] = {ivec2(cx, cy), ivec2(cx+size, cy), ivec2(cx+size, cy+size), ivec2(cx, cy+size)};
    for(int i = 0; i < 4; ++i)
    {
        if(insideface(&corner[i], 1, o, numo))
        {
            rvecs[r++] = corner[i];
        }
    }
    return r;
}

static bool collapsedface(const cube &c, int orient)
{
    int e0 = c.edges[faceedgesidx[orient][0]],
        e1 = c.edges[faceedgesidx[orient][1]],
        e2 = c.edges[faceedgesidx[orient][2]],
        e3 = c.edges[faceedgesidx[orient][3]],
        face = DIMENSION(orient)*4,
        f0 = c.edges[face+0],
        f1 = c.edges[face+1],
        f2 = c.edges[face+2],
        f3 = c.edges[face+3];
    if(DIM_COORD(orient))
    {
        f0 >>= 4;
        f1 >>= 4;
        f2 >>= 4;
        f3 >>= 4;
    }
    else
    {
        f0 &= 0xF;
        f1 &= 0xF;
        f2 &= 0xF;
        f3 &= 0xF;
    }
    ivec v0(e0&0xF, e2&0xF, f0),
         v1(e0>>4, e3&0xF, f1),
         v2(e1>>4, e3>>4, f3),
         v3(e1&0xF, e2>>4, f2);
    return !ivec().cross(v1.sub(v0), v2.sub(v0)) &&
           !ivec().cross(v2, v3.sub(v0));
}

static bool occludesface(const cube &c, int orient, const ivec &o, int size, const ivec &vo, int vsize, ushort vmat, ushort nmat, ushort matmask, const ivec2 *vf, int numv)
{
    int dim = DIMENSION(orient);
    if(!c.children)
    {
        if(c.material)
        {
            if(nmat != Mat_Air && (c.material&matmask) == nmat)
            {
                ivec2 nf[8];
                return clipfacevecs(vf, numv, o[C[dim]], o[R[dim]], size, nf) < 3;
            }
            if(vmat != Mat_Air && ((c.material&matmask) == vmat || (IS_LIQUID(vmat) && IS_CLIPPED(c.material&MatFlag_Volume))))
            {
                return true;
            }
        }
        if(c.issolid())
        {
            return true;
        }
        if(touchingface(c, orient) && faceedges(c, orient) == facesolid)
        {
            return true;
        }
        ivec2 cf[8];
        int numc = clipfacevecs(vf, numv, o[C[dim]], o[R[dim]], size, cf);
        if(numc < 3)
        {
            return true;
        }
        if(c.isempty() || notouchingface(c, orient))
        {
            return false;
        }
        std::array<ivec2, 4> of;
        int numo = genfacevecs(c, orient, o, size, false, of);
        return numo >= 3 && insideface(&cf[0], numc, &of[0], numo);
    }
    size >>= 1;
    int coord = DIM_COORD(orient);
    for(int i = 0; i < 8; ++i)
    {
        if(OCTA_COORD(dim, i) == coord)
        {
            if(!occludesface((*c.children)[i], orient, ivec(i, o, size), size, vo, vsize, vmat, nmat, matmask, vf, numv))
            {
                return false;
            }
        }
    }
    return true;
}

bool visibleface(const cube &c, int orient, const ivec &co, int size, ushort mat, ushort nmat, ushort matmask)
{
    if(mat != Mat_Air)
    {
        if(mat != Mat_Clip && faceedges(c, orient) == facesolid && touchingface(c, orient))
        {
            return false;
        }
    }
    else
    {
        if(collapsedface(c, orient))
        {
            return false;
        }
        if(!touchingface(c, orient))
        {
            return true;
        }
    }
    ivec no;
    int nsize;
    const cube &o = ::rootworld.neighborcube(orient, co, size, no, nsize);
    int opp = oppositeorient(orient);
    if(nsize > size || (nsize == size && !o.children))
    {
        if(o.material)
        {
            if(nmat != Mat_Air && (o.material&matmask) == nmat)
            {
                return true;
            }
            if(mat != Mat_Air && ((o.material&matmask) == mat || (IS_LIQUID(mat) && IS_CLIPPED(o.material&MatFlag_Volume))))
            {
                return false;
            }
        }
        if(o.issolid())
        {
            return false;
        }
        if(o.isempty() || notouchingface(o, opp))
        {
            return true;
        }
        if(touchingface(o, opp) && faceedges(o, opp) == facesolid)
        {
            return false;
        }
        ivec vo = ivec(co).mask(0xFFF);
        no.mask(0xFFF);
        std::array<ivec2, 4> cf,
                             of;
        int numc = genfacevecs(c, orient, vo, size, mat != Mat_Air, cf),
            numo = genfacevecs(o, opp, no, nsize, false, of);
        return numo < 3 || !insideface(&cf[0], numc, &of[0], numo);
    }
    ivec vo = ivec(co).mask(0xFFF);
    no.mask(0xFFF);
    std::array<ivec2, 4> cf;
    int numc = genfacevecs(c, orient, vo, size, mat != Mat_Air, cf);
    return !occludesface(o, opp, no, nsize, vo, size, mat, nmat, matmask, cf.data(), numc);
}

int classifyface(const cube &c, int orient, const ivec &co, int size)
{
    int vismask = 2,
        forcevis = 0;
    bool solid = false;
    switch(c.material&MatFlag_Clip)
    {
        case Mat_NoClip:
        {
            vismask = 0;
            break;
        }
        case Mat_Clip:
        {
            solid = true;
            break;
        }
    }
    if(c.isempty() || collapsedface(c, orient))
    {
        if(!vismask)
        {
            return 0;
        }
    }
    else if(!touchingface(c, orient))
    {
        forcevis = 1;
        if(!solid)
        {
            if(vismask && collideface(c, orient))
            {
                forcevis |= 2;
            }
            return forcevis;
        }
    }
    else
    {
        vismask |= 1;
    }
    ivec no;
    int nsize;
    const cube &o = ::rootworld.neighborcube(orient, co, size, no, nsize);
    if(&o==&c)
    {
        return 0;
    }
    int opp = oppositeorient(orient);
    if(nsize > size || (nsize == size && !o.children))
    {
        if(o.material)
        {
            if((~c.material & o.material) & Mat_Alpha)
            {
                forcevis |= vismask&1;
                vismask &= ~1;
            }
            switch(o.material&MatFlag_Clip)
            {
                case Mat_Clip:
                {
                    vismask &= ~2;
                    break;
                }
                case Mat_NoClip:
                {
                    forcevis |= vismask&2;
                    vismask &= ~2;
                    break;
                }
            }
        }
        if(vismask && !(o.issolid()))
        {
            if(o.isempty() || notouchingface(o, opp))
            {
                forcevis |= vismask;
            }
            else if(!touchingface(o, opp) || faceedges(o, opp) != facesolid)
            {
                ivec vo = ivec(co).mask(0xFFF);
                no.mask(0xFFF);
                std::array<ivec2, 4> cf,
                                     of;
                int numo = genfacevecs(o, opp, no, nsize, false, of);
                if(numo < 3)
                {
                    forcevis |= vismask;
                }
                else
                {
                    int numc = 0;
                    if(vismask&2 && solid)
                    {
                        numc = genfacevecs(c, orient, vo, size, true, cf);
                        if(!insideface(&cf[0], numc, &of[0], numo))
                        {
                            forcevis |= 2;
                        }
                        vismask &= ~2;
                    }
                    if(vismask)
                    {
                        numc = genfacevecs(c, orient, vo, size, false, cf);
                        if(!insideface(&cf[0], numc, &of[0], numo))
                        {
                            forcevis |= vismask;
                        }
                    }
                }
            }
        }
    }
    else
    {
        ivec vo = ivec(co).mask(0xFFF);
        no.mask(0xFFF);
        std::array<ivec2, 4> cf;
        int numc = 0;
        if(vismask&1)
        {
            numc = genfacevecs(c, orient, vo, size, false, cf);
            if(!occludesface(o, opp, no, nsize, vo, size, Mat_Air, (c.material&Mat_Alpha)^Mat_Alpha, Mat_Alpha, &cf[0], numc))
            {
                forcevis |= 1;
            }
        }
        if(vismask&2)
        {
            if(!numc || solid)
            {
                numc = genfacevecs(c, orient, vo, size, solid, cf);
            }
            if(!occludesface(o, opp, no, nsize, vo, size, Mat_Clip, Mat_NoClip, MatFlag_Clip, &cf[0], numc))
            {
                forcevis |= 2;
            }
        }
    }
    if(forcevis&2 && !solid && !collideface(c, orient))
    {
        forcevis &= ~2;
    }
    return forcevis;
}

// more expensive version that checks both triangles of a face independently
int visibletris(const cube &c, int orient, const ivec &co, int size, ushort vmat, ushort nmat, ushort matmask)
{
    int vis = 3,
        touching = 0xF;
    std::array<ivec, 4> v;
    ivec e1,
         e2,
         e3,
         n;
    genfaceverts(c, orient, v);
    n.cross((e1 = v[1]).sub(v[0]), (e2 = v[2]).sub(v[0]));
    int convex = (e3 = v[0]).sub(v[3]).dot(n);
    if(!convex)
    {
        if(!ivec().cross(e3, e2) || v[1] == v[3])
        {
            if(!n)
            {
                return 0;
            }
            vis = 1;
            touching = 0xF&~(1<<3);
        }
        else if(!n)
        {
            vis = 2;
            touching = 0xF&~(1<<1);
        }
    }
    int dim = DIMENSION(orient), coord = DIM_COORD(orient);
    if(v[0][dim] != coord*8)
    {
        touching &= ~(1<<0);
    }
    if(v[1][dim] != coord*8)
    {
        touching &= ~(1<<1);
    }
    if(v[2][dim] != coord*8)
    {
        touching &= ~(1<<2);
    }
    if(v[3][dim] != coord*8)
    {
        touching &= ~(1<<3);
    }
    static const int notouchmasks[2][16] = // mask of triangles not touching
    { // order 0: flat or convex
       // 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
        { 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 1, 3, 0 },
      // order 1: concave
        { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 2, 0 },
    };
    int order = convex < 0 ? 1 : 0,
        notouch = notouchmasks[order][touching];
    if((vis&notouch)==vis)
    {
        return vis;
    }
    ivec no;
    int nsize;
    const cube &o = ::rootworld.neighborcube(orient, co, size, no, nsize);
    if((c.material&matmask) == nmat)
    {
        nmat = Mat_Air;
    }
    ivec vo = ivec(co).mask(0xFFF);
    no.mask(0xFFF);
    std::array<ivec2, 4> cf,
                         of;
    int opp = oppositeorient(orient),
        numo = 0,
        numc;
    if(nsize > size || (nsize == size && !o.children))
    {
        if(o.material)
        {
            if(vmat != Mat_Air && (o.material&matmask) == vmat)
            {
                return vis&notouch;
            }
            if(nmat != Mat_Air && (o.material&matmask) == nmat)
            {
                return vis;
            }
        }
        if(o.isempty() || notouchingface(o, opp))
        {
            return vis;
        }
        if(o.issolid() || (touchingface(o, opp) && faceedges(o, opp) == facesolid))
        {
            return vis&notouch;
        }
        numc = genfacevecs(c, orient, vo, size, false, cf, v.data());
        numo = genfacevecs(o, opp, no, nsize, false, of);
        if(numo < 3)
        {
            return vis;
        }
        if(insideface(&cf[0], numc, &of[0], numo))
        {
            return vis&notouch;
        }
    }
    else
    {
        numc = genfacevecs(c, orient, vo, size, false, cf, v.data());
        if(occludesface(o, opp, no, nsize, vo, size, vmat, nmat, matmask, &cf[0], numc))
        {
            return vis&notouch;
        }
    }
    if(vis != 3 || notouch)
    {
        return vis;
    }
    static const int triverts[2][2][2][3] =
    { // order
        { // coord
            { { 1, 2, 3 }, { 0, 1, 3 } }, // verts
            { { 0, 1, 2 }, { 0, 2, 3 } }
        },
        { // coord
            { { 0, 1, 2 }, { 3, 0, 2 } }, // verts
            { { 1, 2, 3 }, { 1, 3, 0 } }
        }
    };
    do
    {
        for(int i = 0; i < 2; ++i)
        {
            const int *verts = triverts[order][coord][i];
            ivec2 tf[3] = { cf[verts[0]], cf[verts[1]], cf[verts[2]] };
            if(numo > 0)
            {
                if(!insideface(tf, 3, &of[0], numo))
                {
                    continue;
                }
            }
            else if(!occludesface(o, opp, no, nsize, vo, size, vmat, nmat, matmask, tf, 3))
            {
                continue;
            }
            return vis & ~(1<<i);
        }
        vis |= 4;
    } while(++order <= 1);
    return 3;
}

static void calcvert(const cube &c, const ivec &co, int size, vec &v, int i, bool solid = false)
{
    if(solid)
    {
        v = vec(cubecoords[i]);
    }
    else
    {
        gencubevert(c, i, v);
    }
    v.mul(size/8.0f).add(vec(co));
}

//sets clipplanes values for a cube c and location co
void genclipbounds(const cube &c, const ivec &co, int size, clipplanes &p)
{
    // generate tight bounding box
    calcvert(c, co, size, p.v[0], 0);
    vec mx = p.v[0],
        mn = p.v[0];
    for(int i = 1; i < 8; i++)
    {
        calcvert(c, co, size, p.v[i], i);
        mx.max(p.v[i]);
        mn.min(p.v[i]);
    }
    p.r = mx.sub(mn).mul(0.5f);
    p.o = mn.add(p.r);
    p.size = 0;
    p.visible = 0x80;
}

void genclipplanes(const cube &c, const ivec &co, int size, clipplanes &p, bool collide, bool noclip)
{
    static const uchar fv[6][4] = // indexes for cubecoords, per each vert of a face orientation
    {
        { 2, 1, 6, 5 },
        { 3, 4, 7, 0 },
        { 4, 5, 6, 7 },
        { 1, 2, 3, 0 },
        { 6, 1, 0, 7 },
        { 5, 4, 3, 2 },
    };

    p.visible &= ~0x80;
    if(collide || (c.visible&0xC0) == 0x40)
    {
        for(int i = 0; i < 6; ++i)
        {
            if(c.visible&(1<<i))
            {
                int vis;
                if(flataxisface(c, i))
                {
                    p.visible |= 1<<i;
                }
                else if((vis = visibletris(c, i, co, size, Mat_Clip, Mat_NoClip, MatFlag_Clip)))
                {
                    int convex = faceconvexity(c, i),
                        order = vis&4 || convex < 0 ? 1 : 0;
                    const vec &v0 = p.v[fv[i][order]],
                              &v1 = p.v[fv[i][order+1]],
                              &v2 = p.v[fv[i][order+2]],
                              &v3 = p.v[fv[i][(order+3)&3]];
                    if(vis&1)
                    {
                        p.side[p.size] = i;
                        p.p[p.size++].toplane(v0, v1, v2);
                    }
                    if(vis&2 && (!(vis&1) || convex))
                    {
                        p.side[p.size] = i;
                        p.p[p.size++].toplane(v0, v2, v3);
                    }
                }
            }
        }
    }
    else if(c.visible&0x80)
    {
        const ushort nmat = noclip ? Mat_NoClip : Mat_Alpha,
                     matmask = noclip ? static_cast<ushort>(MatFlag_Clip) : Mat_Alpha; //cast to avoid enum mismatch warning
        int vis;
        for(int i = 0; i < 6; ++i)
        {
            if((vis = visibletris(c, i, co, size, Mat_Air, nmat, matmask)))
            {
                if(flataxisface(c, i))
                {
                    p.visible |= 1<<i;
                }
                else
                {
                    int convex = faceconvexity(c, i),
                        order = vis&4 || convex < 0 ? 1 : 0;
                    const vec &v0 = p.v[fv[i][order]],
                              &v1 = p.v[fv[i][order+1]],
                              &v2 = p.v[fv[i][order+2]],
                              &v3 = p.v[fv[i][(order+3)&3]];
                    if(vis&1)
                    {
                        p.side[p.size] = i;
                        p.p[p.size++].toplane(v0, v1, v2);
                    }
                    if(vis&2 && (!(vis&1) || convex))
                    {
                        p.side[p.size] = i;
                        p.p[p.size++].toplane(v0, v2, v3);
                    }
                }
            }
        }
    }
}

void cube::mincubeface(const cube &cu, int orient, const ivec &o, int size, const facebounds &orig, facebounds &cf, ushort nmat, ushort matmask) const
{
    int dim = DIMENSION(orient);
    if(cu.children)
    {
        size >>= 1;
        int coord = DIM_COORD(orient);
        for(int i = 0; i < 8; ++i)
        {
            if(OCTA_COORD(dim, i) == coord)
            {
                mincubeface((*cu.children)[i], orient, ivec(i, o, size), size, orig, cf, nmat, matmask);
            }
        }
        return;
    }
    int c = C[dim],
        r = R[dim];
    ushort uco = (o[c]&0xFFF)<<3,
           vco = (o[r]&0xFFF)<<3;
    ushort uc1 = uco,
           vc1 = vco,
           uc2 = static_cast<ushort>(size<<3)+uco,
           vc2 = static_cast<ushort>(size<<3)+vco;
    uc1 = std::max(uc1, orig.u1);
    uc2 = std::min(uc2, orig.u2);
    vc1 = std::max(vc1, orig.v1);
    vc2 = std::min(vc2, orig.v2);
    if(!(cu.isempty()) && touchingface(cu, orient) && !(nmat!=Mat_Air && (cu.material&matmask)==nmat))
    {
        uchar r1 = cu.edges[faceedgesidx[orient][0]],
              r2 = cu.edges[faceedgesidx[orient][1]],
              c1 = cu.edges[faceedgesidx[orient][2]],
              c2 = cu.edges[faceedgesidx[orient][3]];
        ushort u1 = std::max(c1&0xF, c2&0xF)*size+uco,
               u2 = std::min(c1>>4, c2>>4)*size+uco,
               v1 = std::max(r1&0xF, r2&0xF)*size+vco,
               v2 = std::min(r1>>4, r2>>4)*size+vco;
        u1 = std::max(u1, orig.u1);
        u2 = std::min(u2, orig.u2);
        v1 = std::max(v1, orig.v1);
        v2 = std::min(v2, orig.v2);
        if(v2-v1==vc2-vc1)
        {
            if(u2-u1==uc2-uc1)
            {
                return;
            }
            if(u1==uc1)
            {
                uc1 = u2;
            }
            if(u2==uc2)
            {
                uc2 = u1;
            }
        }
        else if(u2-u1==uc2-uc1)
        {
            if(v1==vc1)
            {
                vc1 = v2;
            }
            if(v2==vc2)
            {
                vc2 = v1;
            }
        }
    }
    if(uc1==uc2 || vc1==vc2)
    {
        return;
    }
    cf.u1 = std::min(cf.u1, uc1);
    cf.u2 = std::max(cf.u2, uc2);
    cf.v1 = std::min(cf.v1, vc1);
    cf.v2 = std::max(cf.v2, vc2);
}

int calcmergedsize(int orient, const ivec &co, int size, const vertinfo *verts, int numverts)
{
    ushort x1 = verts[0].x,
           y1 = verts[0].y,
           z1 = verts[0].z,
           x2 = x1,
           y2 = y1,
           z2 = z1;
    for(int i = 1; i < numverts; i++)
    {
        const vertinfo &v = verts[i];
        x1 = std::min(x1, v.x);
        x2 = std::max(x2, v.x);
        y1 = std::min(y1, v.y);
        y2 = std::max(y2, v.y);
        z1 = std::min(z1, v.z);
        z2 = std::max(z2, v.z);
    }
    int bits = 0;
    while(1<<bits < size)
    {
        ++bits;
    }
    bits += 3;
    ivec mo(co);
    mo.mask(0xFFF);
    mo.shl(3);
    while(bits<15)
    {
        mo.mask(~((1<<bits)-1));
        if(mo.x <= x1 && mo.x + (1<<bits) >= x2 &&
           mo.y <= y1 && mo.y + (1<<bits) >= y2 &&
           mo.z <= z1 && mo.z + (1<<bits) >= z2)
        {
            break;
        }
        bits++;
    }
    return bits-3;
}

void invalidatemerges(cube &c)
{
    if(c.merged)
    {
        brightencube(c);
        c.merged = 0;
    }
    if(c.ext)
    {
        if(c.ext->va)
        {
            if(!(c.ext->va->hasmerges&(Merge_Part | Merge_Origin)))
            {
                return;
            }
            destroyva(c.ext->va);
            c.ext->va = nullptr;
        }
        if(c.ext->tjoints >= 0)
        {
            c.ext->tjoints = -1;
        }
    }
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            invalidatemerges((*c.children)[i]);
        }
    }
}

uchar octaboxoverlap(const ivec &o, int size, const ivec &bbmin, const ivec &bbmax)
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

void initoctaworldcmds()
{
    addcommand("printcube", reinterpret_cast<identfun>(printcube), "", Id_Command);
}

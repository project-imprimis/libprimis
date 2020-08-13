// core world management routines

#include "engine.h"
#include "light.h"
#include "raycube.h"
#include "render/renderwindow.h"

static struct emptycube : cube
{
    emptycube()
    {
        children = NULL;
        ext = NULL;
        visible = 0;
        merged = 0;
        material = Mat_Air;
        SET_FACES(*this, faceempty);
        for(int i = 0; i < 6; ++i)
        {
            texture[i] = Default_Sky;
        }
    }
} emptycube;

cube *worldroot = newcubes(facesolid);
int allocnodes = 0;
void calcmerges();

cubeext *growcubeext(cubeext *old, int maxverts)
{
    cubeext *ext = (cubeext *)new uchar[sizeof(cubeext) + maxverts*sizeof(vertinfo)];
    if(old)
    {
        ext->va = old->va;
        ext->ents = old->ents;
        ext->tjoints = old->tjoints;
    }
    else
    {
        ext->va = NULL;
        ext->ents = NULL;
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
        delete[] (uchar *)old;
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
            memcpy(ext->surfaces, c.ext->surfaces, sizeof(ext->surfaces));
            memcpy(ext->verts(), c.ext->verts(), c.ext->maxverts*sizeof(vertinfo));
        }
        else
        {
            memset(ext->surfaces, 0, sizeof(ext->surfaces));
        }
    }
    setcubeext(c, ext);
    return ext;
}

cube *newcubes(uint face, int mat)
{
    cube *c = new cube[8];
    for(int i = 0; i < 8; ++i)
    {
        c->children = NULL;
        c->ext = NULL;
        c->visible = 0;
        c->merged = 0;
        SET_FACES(*c, face);
        for(int l = 0; l < 6; ++l) //note this is a loop l (level 4)
        {
            c->texture[l] = Default_Geom;
        }
        c->material = mat;
        c++;
    }
    allocnodes++;
    return c-8;
}

int familysize(const cube &c)
{
    int size = 1;
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            size += familysize(c.children[i]);
        }
    }
    return size;
}

void freeocta(cube *c)
{
    if(!c)
    {
        return;
    }
    for(int i = 0; i < 8; ++i)
    {
        discardchildren(c[i]);
    }
    delete[] c;
    allocnodes--;
}

void freecubeext(cube &c)
{
    if(c.ext)
    {
        delete[] reinterpret_cast<uchar *>(c.ext);
        c.ext = NULL;
    }
}

void discardchildren(cube &c, bool fixtex, int depth)
{
    c.material = Mat_Air;
    c.visible = 0;
    c.merged = 0;
    if(c.ext)
    {
        if(c.ext->va)
        {
            destroyva(c.ext->va);
        }
        c.ext->va = NULL;
        c.ext->tjoints = -1;
        freeoctaentities(c);
        freecubeext(c);
    }
    if(c.children)
    {
        uint filled = faceempty;
        for(int i = 0; i < 8; ++i)
        {
            discardchildren(c.children[i], fixtex, depth+1);
            filled |= c.children[i].faces[0];
        }
        if(fixtex)
        {
            for(int i = 0; i < 6; ++i)
            {
                c.texture[i] = getmippedtexture(c, i);
            }
            if(depth > 0 && filled != faceempty)
            {
                c.faces[0] = facesolid;
            }
        }
        DELETEA(c.children);
        allocnodes--;
    }
}

void getcubevector(cube &c, int d, int x, int y, int z, ivec &p)
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

static inline void getcubevector(cube &c, int i, ivec &p)
{
    p.x = EDGE_GET(CUBE_EDGE(c, 0, (i>>R[0])&1, (i>>C[0])&1), (i>>D[0])&1);
    p.y = EDGE_GET(CUBE_EDGE(c, 1, (i>>R[1])&1, (i>>C[1])&1), (i>>D[1])&1);
    p.z = EDGE_GET(CUBE_EDGE(c, 2, (i>>R[2])&1, (i>>C[2])&1), (i>>D[2])&1);
}

static inline void setcubevector(cube &c, int i, const ivec &p)
{
    EDGE_SET(CUBE_EDGE(c, 0, (i>>R[0])&1, (i>>C[0])&1), (i>>D[0])&1, p.x);
    EDGE_SET(CUBE_EDGE(c, 1, (i>>R[1])&1, (i>>C[1])&1), (i>>D[1])&1, p.y);
    EDGE_SET(CUBE_EDGE(c, 2, (i>>R[2])&1, (i>>C[2])&1), (i>>D[2])&1, p.z);
}

void optiface(uchar *p, cube &c)
{
    uint f = *(uint *)p;
    if(((f>>4)&0x0F0F0F0FU) == (f&0x0F0F0F0FU))
    {
        EMPTY_FACES(c);
    }
}

void printcube()
{
    cube &c = lookupcube(lu); // assume this is cube being pointed at
    conoutf(Console_Debug, "= %p = (%d, %d, %d) @ %d", (void *)&c, lu.x, lu.y, lu.z, lusize);
    conoutf(Console_Debug, " x  %.8x", c.faces[0]);
    conoutf(Console_Debug, " y  %.8x", c.faces[1]);
    conoutf(Console_Debug, " z  %.8x", c.faces[2]);
}

COMMAND(printcube, "");

bool isvalidcube(const cube &c)
{
    clipplanes p;
    genclipbounds(c, ivec(0, 0, 0), 256, p);
    genclipplanes(c, ivec(0, 0, 0), 256, p);
    // test that cube is convex
    for(int i = 0; i < 8; ++i)
    {
        vec v = p.v[i];
        for(int j = 0; j < p.size; ++j)
        {
            if(p.p[j].dist(v)>1e-3f)
            {
                return false;
            }
        }
    }
    return true;
}

void validatec(cube *c, int size)
{
    for(int i = 0; i < 8; ++i)
    {
        if(c[i].children)
        {
            if(size<=1)
            {
                SOLID_FACES(c[i]);
                discardchildren(c[i], true);
            }
            else
            {
                validatec(c[i].children, size>>1);
            }
        }
        else if(size > 0x1000)
        {
            subdividecube(c[i], true, false);
            validatec(c[i].children, size>>1);
        }
        else
        {
            for(int j = 0; j < 3; ++j)
            {
                uint f  = c[i].faces[j],
                     e0 = f&0x0F0F0F0FU,
                     e1 = (f>>4)&0x0F0F0F0FU;
                if(e0 == e1 || ((e1+0x07070707U)|(e1-e0))&0xF0F0F0F0U)
                {
                    EMPTY_FACES(c[i]);
                    break;
                }
            }
        }
    }
}

ivec lu;
int lusize;
cube &lookupcube(const ivec &to, int tsize, ivec &ro, int &rsize)
{
    int tx = std::clamp(to.x, 0, worldsize-1),
        ty = std::clamp(to.y, 0, worldsize-1),
        tz = std::clamp(to.z, 0, worldsize-1);
    int scale = worldscale-1,
        csize = abs(tsize);
    cube *c = &worldroot[OCTA_STEP(tx, ty, tz, scale)];
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
                        c = &c->children[OCTA_STEP(tx, ty, tz, scale)];
                    } while(!(csize>>scale));
                }
                break;
            }
            scale--;
            c = &c->children[OCTA_STEP(tx, ty, tz, scale)];
        } while(!(csize>>scale));
    }
    ro = ivec(tx, ty, tz).mask(~0U<<scale);
    rsize = 1<<scale;
    return *c;
}

int lookupmaterial(const vec &v)
{
    ivec o(v);
    if(!insideworld(o))
    {
        return Mat_Air;
    }
    int scale = worldscale-1;
    cube *c = &worldroot[OCTA_STEP(o.x, o.y, o.z, scale)];
    while(c->children)
    {
        scale--;
        c = &c->children[OCTA_STEP(o.x, o.y, o.z, scale)];
    }
    return c->material;
}

const cube *neighborstack[32];
int neighbordepth = -1;

const cube &neighborcube(const cube &c, int orient, const ivec &co, int size, ivec &ro, int &rsize)
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
    if(diff >= static_cast<uint>(worldsize))
    {
        ro = n;
        rsize = size;
        return emptycube;
    }
    int scale = worldscale;
    const cube *nc = worldroot;
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
            nc = &nc->children[OCTA_STEP(n.x, n.y, n.z, scale)];
        } while(!(size>>scale) && nc->children);
    }
    ro = n.mask(~0U<<scale);
    rsize = 1<<scale;
    return *nc;
}

////////// (re)mip //////////

int getmippedtexture(const cube &p, int orient)
{
    cube *c = p.children;
    int d = DIMENSION(orient),
        dc = DIM_COORD(orient),
        texs[4] = { -1, -1, -1, -1 },
        numtexs = 0;
    for(int x = 0; x < 2; ++x)
    {
        for(int y = 0; y < 2; ++y)
        {
            int n = OCTA_INDEX(d, x, y, dc);
            if(IS_EMPTY(c[n]))
            {
                n = OPPOSITE_OCTA(d, n);
                if(IS_EMPTY(c[n]))
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
    cube *ch = c.children;
    EMPTY_FACES(c);
    for(int i = 0; i < 8; ++i)
    {
        for(int j = 0; j < 8; ++j)
        {
            int n = i^(j==3 ? 4 : (j==4 ? 3 : j));
            if(!IS_EMPTY(ch[n])) // breadth first search for cube near vert
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
    if(((abs(s)&0xFF)!=0) || // ie: rounding error
        (crossy && y!=8) ||
        (y<0 || y>16))
    {
        perfect = false;
    }
    return crossy ? 8 : min(max(y, 0), 16);
}

static inline bool crosscenter(const ivec &a, const ivec &b, int xd, int yd)
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
        memset(c.ext->surfaces, 0, sizeof(c.ext->surfaces));
    }
    if(IS_EMPTY(c) || IS_ENTIRELY_SOLID(c))
    {
        c.children = newcubes(IS_EMPTY(c) ? faceempty : facesolid, c.material);
        for(int i = 0; i < 8; ++i)
        {
            for(int l = 0; l < 6; ++l) //note this is a loop l (level 4)
            {
                c.children[i].texture[l] = c.texture[l];
            }
            if(brighten && !IS_EMPTY(c))
            {
                brightencube(c.children[i]);
            }
        }
        return true;
    }
    cube *ch = c.children = newcubes(facesolid, c.material);
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
        const ivec &v00 = v[OCTA_INDEX(d, 0, 0, z)],
                   &v10 = v[OCTA_INDEX(d, 1, 0, z)],
                   &v01 = v[OCTA_INDEX(d, 0, 1, z)],
                   &v11 = v[OCTA_INDEX(d, 1, 1, z)];
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

    validatec(ch);
    if(fullcheck)
    {
        for(int i = 0; i < 8; ++i)
        {
            if(!isvalidcube(ch[i])) // not so good...
            {
                EMPTY_FACES(ch[i]);
                perfect=false;
            }
        }
    }
    if(brighten)
    {
        for(int i = 0; i < 8; ++i)
        {
            if(!IS_EMPTY(ch[i]))
            {
                brightencube(ch[i]);
            }
        }
    }
    return perfect;
}

bool crushededge(uchar e, int dc)
{
    return dc ? e==0 : e==0x88;
}

bool touchingface(const cube &c, int orient)
{
    uint face = c.faces[DIMENSION(orient)];
    return DIM_COORD(orient) ? (face&0xF0F0F0F0)==0x80808080 : (face&0x0F0F0F0F)==0;
}

bool notouchingface(const cube &c, int orient)
{
    uint face = c.faces[DIMENSION(orient)];
    return DIM_COORD(orient) ? (face&0x80808080)==0 : ((0x88888888-face)&0x08080808) == 0;
}

int visibleorient(const cube &c, int orient)
{
    for(int i = 0; i < 2; ++i)
    {
        int a = faceedgesidx[orient][i*2 + 0];
        int b = faceedgesidx[orient][i*2 + 1];
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

static int remipprogress = 0,
           remiptotal = 0;

bool remip(cube &c, const ivec &co, int size)
{
    cube *ch = c.children;
    if(!ch)
    {
        if(size<<1 <= 0x1000)
        {
            return true;
        }
        subdividecube(c);
        ch = c.children;
    }
    bool perfect = true;
    for(int i = 0; i < 8; ++i)
    {
        ivec o(i, co, size);
        if(!remip(ch[i], o, size>>1))
        {
            perfect = false;
        }
    }
    SOLID_FACES(c); // so texmip is more consistent
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
        mat = ch[i].material;
        if((mat&MatFlag_Clip) == Mat_NoClip || mat&Mat_Alpha)
        {
            if(i > 0)
            {
                return false;
            }
            while(++i < 8)
            {
                if(ch[i].material != mat)
                {
                    return false;
                }
            }
            break;
        }
        else if(!IS_ENTIRELY_SOLID(ch[i]))
        {
            while(++i < 8)
            {
                int omat = ch[i].material;
                if(IS_ENTIRELY_SOLID(ch[i]) ? (omat&MatFlag_Clip) == Mat_NoClip || omat&Mat_Alpha : mat != omat)
                {
                    return false;
                }
            }
            break;
        }
    }
    cube n = c;
    n.ext = NULL;
    forcemip(n);
    n.children = NULL;
    if(!subdividecube(n, false, false))
    {
        freeocta(n.children);
        return false;
    }
    cube *nh = n.children;
    uchar vis[6] = {0, 0, 0, 0, 0, 0};
    for(int i = 0; i < 8; ++i)
    {
        if(ch[i].faces[0] != nh[i].faces[0] ||
           ch[i].faces[1] != nh[i].faces[1] ||
           ch[i].faces[2] != nh[i].faces[2])
        {
            freeocta(nh);
            return false;
        }

        if(IS_EMPTY(ch[i]) && IS_EMPTY(nh[i]))
        {
            continue;
        }

        ivec o(i, co, size);
        for(int orient = 0; orient < 6; ++orient)
            if(visibleface(ch[i], orient, o, size, Mat_Air, (mat&Mat_Alpha)^Mat_Alpha, Mat_Alpha))
            {
                if(ch[i].texture[orient] != n.texture[orient])
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
                    mask |= 1<<OCTA_INDEX(DIMENSION(orient), x, y, DIM_COORD(orient));
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
    discardchildren(c);
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

void remip()
{
    remipprogress = 1;
    remiptotal = allocnodes;
    for(int i = 0; i < 8; ++i)
    {
        ivec o(i, ivec(0, 0, 0), worldsize>>1);
        remip(worldroot[i], o, worldsize>>2);
    }
    calcmerges();
}

void mpremip(bool local)
{
    extern selinfo sel;
    if(local)
    {
        game::edittrigger(sel, Edit_Remip);
    }
    remip();
    allchanged();
}

ICOMMAND(remip, "", (), mpremip(true));

const ivec cubecoords[8] = // verts of bounding cube
{
//================================================================== GENCUBEVERT
#define GENCUBEVERT(n, x, y, z) ivec(x, y, z),
    GENCUBEVERTS(0, 8, 0, 8, 0, 8)
#undef GENCUBEVERT
//==============================================================================
};

template<class T>
static inline void gencubevert(const cube &c, int i, T &v)
{
    switch(i)
    {
        default:
//================================================================== GENCUBEVERT
#define GENCUBEVERT(n, x, y, z) \
        case n: \
            v = T(EDGE_GET(CUBE_EDGE(c, 0, y, z), x), \
                  EDGE_GET(CUBE_EDGE(c, 1, z, x), y), \
                  EDGE_GET(CUBE_EDGE(c, 2, x, y), z)); \
            break;
        GENCUBEVERTS(0, 1, 0, 1, 0, 1)
#undef GENCUBEVERT
//==============================================================================
    }
}

void genfaceverts(const cube &c, int orient, ivec v[4])
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
//==================================================== GENFACEORIENT GENFACEVERT
const ivec facecoords[6][4] =
{
#define GENFACEORIENT(o, v0, v1, v2, v3) \
    { v0, v1, v2, v3 },
#define GENFACEVERT(o, n, x,y,z, xv,yv,zv) \
        ivec(x,y,z)
    GENFACEVERTS(0, 8, 0, 8, 0, 8, , , , , , )
#undef GENFACEORIENT
#undef GENFACEVERT
};
//==============================================================================
const uchar fv[6][4] = // indexes for cubecoords, per each vert of a face orientation
{
    { 2, 1, 6, 5 },
    { 3, 4, 7, 0 },
    { 4, 5, 6, 7 },
    { 1, 2, 3, 0 },
    { 6, 1, 0, 7 },
    { 5, 4, 3, 2 },
};

const uchar fvmasks[64] = // mask of verts used given a mask of visible face orientations
{
    0x00, 0x66, 0x99, 0xFF, 0xF0, 0xF6, 0xF9, 0xFF,
    0x0F, 0x6F, 0x9F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xC3, 0xE7, 0xDB, 0xFF, 0xF3, 0xF7, 0xFB, 0xFF,
    0xCF, 0xEF, 0xDF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x3C, 0x7E, 0xBD, 0xFF, 0xFC, 0xFE, 0xFD, 0xFF,
    0x3F, 0x7F, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

const uchar faceedgesidx[6][4] = // ordered edges surrounding each orient
{//0..1 = row edges, 2..3 = column edges
    { 4,  5,  8, 10 },
    { 6,  7,  9, 11 },
    { 8,  9,  0, 2 },
    { 10, 11, 1, 3 },
    { 0,  1,  4, 6 },
    { 2,  3,  5, 7 },
};

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

int faceconvexity(const ivec v[4])
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

int faceconvexity(const ivec v[4], int &vis)
{
    ivec e1, e2, e3, n;
    n.cross((e1 = v[1]).sub(v[0]), (e2 = v[2]).sub(v[0]));
    int convex = (e3 = v[0]).sub(v[3]).dot(n);
    if(!convex)
    {
        if(ivec().cross(e3, e2).iszero())
        {
            if(!n.iszero())
            {
                vis = 1;
            }
        }
        else if(n.iszero())
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
    ivec v[4];
    genfaceverts(c, orient, v);
    return faceconvexity(v);
}

int faceorder(const cube &c, int orient) // gets above 'fv' so that each face is convex
{
    return faceconvexity(c, orient)<0 ? 1 : 0;
}

static inline void faceedges(const cube &c, int orient, uchar edges[4])
{
    for(int k = 0; k < 4; ++k)
    {
        edges[k] = c.edges[faceedgesidx[orient][k]];
    }
}

uint faceedges(const cube &c, int orient)
{
    union
    {
        uchar edges[4];
        uint face;
    } u;
    faceedges(c, orient, u.edges);
    return u.face;
}


static inline int genfacevecs(const cube &cu, int orient, const ivec &pos, int size, bool solid, ivec2 *fvecs, const ivec *v = NULL)
{
    int i = 0;
    if(solid)
    {
        switch(orient)
        {
        #define GENFACEORIENT(orient, v0, v1, v2, v3) \
            case orient: \
            { \
                if(DIM_COORD(orient)) { v0 v1 v2 v3 } else { v3 v2 v1 v0 } \
                break; \
            }
        #define GENFACEVERT(orient, vert, xv,yv,zv, x,y,z) \
            { ivec2 &f = fvecs[i]; x ((xv)<<3); y ((yv)<<3); z ((zv)<<3); i++; }
            GENFACEVERTS(pos.x, pos.x+size, pos.y, pos.y+size, pos.z, pos.z+size, f.x = , f.x = , f.y = , f.y = , (void), (void))
        #undef GENFACEVERT
        }
        return 4;
    }
    ivec buf[4];
    if(!v)
    {
        genfaceverts(cu, orient, buf);
        v = buf;
    }
    ivec2 prev(INT_MAX, INT_MAX);
    switch(orient)
    {
    #define GENFACEVERT(orient, vert, sx,sy,sz, dx,dy,dz) \
        { \
            const ivec &e = v[vert]; \
            ivec ef; \
            ef.dx = e.sx; ef.dy = e.sy; ef.dz = e.sz; \
            if(ef.z == DIM_COORD(orient)*8) \
            { \
                ivec2 &f = fvecs[i]; \
                ivec pf; \
                pf.dx = pos.sx; pf.dy = pos.sy; pf.dz = pos.sz; \
                f = ivec2(ef.x*size + (pf.x<<3), ef.y*size + (pf.y<<3)); \
                if(f != prev) { prev = f; i++; } \
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

static inline int clipfacevecy(const ivec2 &o, const ivec2 &dir, int cx, int cy, int size, ivec2 &r)
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

static inline int clipfacevecx(const ivec2 &o, const ivec2 &dir, int cx, int cy, int size, ivec2 &r)
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

static inline int clipfacevec(const ivec2 &o, const ivec2 &dir, int cx, int cy, int size, ivec2 *rvecs)
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

static inline bool insideface(const ivec2 *p, int nump, const ivec2 *o, int numo)
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

static inline int clipfacevecs(const ivec2 *o, int numo, int cx, int cy, int size, ivec2 *rvecs)
{
    cx <<= 3;
    cy <<= 3;
    size <<= 3;
    int r = 0;
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

bool collapsedface(const cube &c, int orient)
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
    return ivec().cross(v1.sub(v0), v2.sub(v0)).iszero() &&
           ivec().cross(v2, v3.sub(v0)).iszero();
}

static inline bool occludesface(const cube &c, int orient, const ivec &o, int size, const ivec &vo, int vsize, ushort vmat, ushort nmat, ushort matmask, const ivec2 *vf, int numv)
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
        if(IS_ENTIRELY_SOLID(c))
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
        if(IS_EMPTY(c) || notouchingface(c, orient))
        {
            return false;
        }
        ivec2 of[4];
        int numo = genfacevecs(c, orient, o, size, false, of);
        return numo >= 3 && insideface(cf, numc, of, numo);
    }
    size >>= 1;
    int coord = DIM_COORD(orient);
    for(int i = 0; i < 8; ++i)
    {
        if(OCTA_COORD(dim, i) == coord)
        {
            if(!occludesface(c.children[i], orient, ivec(i, o, size), size, vo, vsize, vmat, nmat, matmask, vf, numv))
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
    const cube &o = neighborcube(c, orient, co, size, no, nsize);
    int opp = OPPOSITE(orient);
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
        if(IS_ENTIRELY_SOLID(o))
        {
            return false;
        }
        if(IS_EMPTY(o) || notouchingface(o, opp))
        {
            return true;
        }
        if(touchingface(o, opp) && faceedges(o, opp) == facesolid)
        {
            return false;
        }
        ivec vo = ivec(co).mask(0xFFF);
        no.mask(0xFFF);
        ivec2 cf[4], of[4];
        int numc = genfacevecs(c, orient, vo, size, mat != Mat_Air, cf),
            numo = genfacevecs(o, opp, no, nsize, false, of);
        return numo < 3 || !insideface(cf, numc, of, numo);
    }
    ivec vo = ivec(co).mask(0xFFF);
    no.mask(0xFFF);
    ivec2 cf[4];
    int numc = genfacevecs(c, orient, vo, size, mat != Mat_Air, cf);
    return !occludesface(o, opp, no, nsize, vo, size, mat, nmat, matmask, cf, numc);
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
    if(IS_EMPTY(c) || collapsedface(c, orient))
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
    const cube &o = neighborcube(c, orient, co, size, no, nsize);
    if(&o==&c)
    {
        return 0;
    }
    int opp = OPPOSITE(orient);
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
        if(vismask && !IS_ENTIRELY_SOLID(o))
        {
            if(IS_EMPTY(o) || notouchingface(o, opp))
            {
                forcevis |= vismask;
            }
            else if(!touchingface(o, opp) || faceedges(o, opp) != facesolid)
            {
                ivec vo = ivec(co).mask(0xFFF);
                no.mask(0xFFF);
                ivec2 cf[4], of[4];
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
                        if(!insideface(cf, numc, of, numo))
                        {
                            forcevis |= 2;
                        }
                        vismask &= ~2;
                    }
                    if(vismask)
                    {
                        numc = genfacevecs(c, orient, vo, size, false, cf);
                        if(!insideface(cf, numc, of, numo))
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
        ivec2 cf[4];
        int numc = 0;
        if(vismask&1)
        {
            numc = genfacevecs(c, orient, vo, size, false, cf);
            if(!occludesface(o, opp, no, nsize, vo, size, Mat_Air, (c.material&Mat_Alpha)^Mat_Alpha, Mat_Alpha, cf, numc))
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
            if(!occludesface(o, opp, no, nsize, vo, size, Mat_Clip, Mat_NoClip, MatFlag_Clip, cf, numc))
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
    ivec v[4], e1, e2, e3, n;
    genfaceverts(c, orient, v);
    n.cross((e1 = v[1]).sub(v[0]), (e2 = v[2]).sub(v[0]));
    int convex = (e3 = v[0]).sub(v[3]).dot(n);
    if(!convex)
    {
        if(ivec().cross(e3, e2).iszero() || v[1] == v[3])
        {
            if(n.iszero())
            {
                return 0;
            }
            vis = 1;
            touching = 0xF&~(1<<3);
        }
        else if(n.iszero())
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
    int order = convex < 0 ? 1 : 0, notouch = notouchmasks[order][touching];
    if((vis&notouch)==vis)
    {
        return vis;
    }
    ivec no;
    int nsize;
    const cube &o = neighborcube(c, orient, co, size, no, nsize);
    if((c.material&matmask) == nmat)
    {
        nmat = Mat_Air;
    }
    ivec vo = ivec(co).mask(0xFFF);
    no.mask(0xFFF);
    ivec2 cf[4], of[4];
    int opp = OPPOSITE(orient),
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
        if(IS_EMPTY(o) || notouchingface(o, opp))
        {
            return vis;
        }
        if(IS_ENTIRELY_SOLID(o) || (touchingface(o, opp) && faceedges(o, opp) == facesolid))
        {
            return vis&notouch;
        }
        numc = genfacevecs(c, orient, vo, size, false, cf, v);
        numo = genfacevecs(o, opp, no, nsize, false, of);
        if(numo < 3)
        {
            return vis;
        }
        if(insideface(cf, numc, of, numo))
        {
            return vis&notouch;
        }
    }
    else
    {
        numc = genfacevecs(c, orient, vo, size, false, cf, v);
        if(occludesface(o, opp, no, nsize, vo, size, vmat, nmat, matmask, cf, numc))
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
                if(!insideface(tf, 3, of, numo))
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
        ushort nmat = Mat_Alpha, matmask = Mat_Alpha;
        if(noclip)
        {
            nmat = Mat_NoClip;
            matmask = MatFlag_Clip;
        }
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

static inline bool mergefacecmp(const facebounds &x, const facebounds &y)
{
    if(x.v2 < y.v2)
    {
        return true;
    }
    if(x.v2 > y.v2)
    {
        return false;
    }
    if(x.u1 < y.u1)
    {
        return true;
    }
    if(x.u1 > y.u1)
    {
        return false;
    }
    return false;
}

struct cfkey
{
    uchar orient;
    ushort material, tex;
    ivec n;
    int offset;
};

static inline bool htcmp(const cfkey &x, const cfkey &y)
{
    return x.orient == y.orient && x.tex == y.tex && x.n == y.n && x.offset == y.offset && x.material==y.material;
}

static inline uint hthash(const cfkey &k)
{
    return hthash(k.n)^k.offset^k.tex^k.orient^k.material;
}

void mincubeface(const cube &cu, int orient, const ivec &o, int size, const facebounds &orig, facebounds &cf, ushort nmat, ushort matmask)
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
                mincubeface(cu.children[i], orient, ivec(i, o, size), size, orig, cf, nmat, matmask);
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
    uc1 = max(uc1, orig.u1);
    uc2 = min(uc2, orig.u2);
    vc1 = max(vc1, orig.v1);
    vc2 = min(vc2, orig.v2);
    if(!IS_EMPTY(cu) && touchingface(cu, orient) && !(nmat!=Mat_Air && (cu.material&matmask)==nmat))
    {
        uchar r1 = cu.edges[faceedgesidx[orient][0]],
              r2 = cu.edges[faceedgesidx[orient][1]],
              c1 = cu.edges[faceedgesidx[orient][2]],
              c2 = cu.edges[faceedgesidx[orient][3]];
        ushort u1 = max(c1&0xF, c2&0xF)*size+uco,
               u2 = min(c1>>4, c2>>4)*size+uco,
               v1 = max(r1&0xF, r2&0xF)*size+vco,
               v2 = min(r1>>4, r2>>4)*size+vco;
        u1 = max(u1, orig.u1);
        u2 = min(u2, orig.u2);
        v1 = max(v1, orig.v1);
        v2 = min(v2, orig.v2);
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
    cf.u1 = min(cf.u1, uc1);
    cf.u2 = max(cf.u2, uc2);
    cf.v1 = min(cf.v1, vc1);
    cf.v2 = max(cf.v2, vc2);
}

bool mincubeface(const cube &cu, int orient, const ivec &co, int size, facebounds &orig)
{
    ivec no;
    int nsize;
    const cube &nc = neighborcube(cu, orient, co, size, no, nsize);
    facebounds mincf;
    mincf.u1 = orig.u2;
    mincf.u2 = orig.u1;
    mincf.v1 = orig.v2;
    mincf.v2 = orig.v1;
    mincubeface(nc, OPPOSITE(orient), no, nsize, orig, mincf, cu.material&Mat_Alpha ? Mat_Air : Mat_Alpha, Mat_Alpha);
    bool smaller = false;
    if(mincf.u1 > orig.u1)
    {
        orig.u1 = mincf.u1;
        smaller = true;
    }
    if(mincf.u2 < orig.u2)
    {
        orig.u2 = mincf.u2;
        smaller = true;
    }
    if(mincf.v1 > orig.v1)
    {
        orig.v1 = mincf.v1;
        smaller = true;
    }
    if(mincf.v2 < orig.v2)
    {
        orig.v2 = mincf.v2;
        smaller = true;
    }
    return smaller;
}

VAR(maxmerge, 0, 6, 12); //max gridpower to remip merge
VAR(minface, 0, 4, 12);

struct pvert
{
    ushort x, y;

    pvert() {}
    pvert(ushort x, ushort y) : x(x), y(y) {}

    bool operator==(const pvert &o) const
    {
        return x == o.x && y == o.y;
    }
    bool operator!=(const pvert &o) const
    {
        return x != o.x || y != o.y;
    }
};

struct pedge
{
    pvert from, to;

    pedge() {}
    pedge(const pvert &from, const pvert &to) : from(from), to(to) {}

    bool operator==(const pedge &o) const
    {
        return from == o.from && to == o.to;
    }
    bool operator!=(const pedge &o) const
    {
        return from != o.from || to != o.to;
    }
};

static inline uint hthash(const pedge &x)
{
    return static_cast<uint>(x.from.x)^(static_cast<uint>(x.from.y)<<8);
}
static inline bool htcmp(const pedge &x, const pedge &y)
{
    return x == y;
}

struct poly
{
    cube *c;
    int numverts;
    bool merged;
    pvert verts[Face_MaxVerts];
};

bool clippoly(poly &p, const facebounds &b)
{
    pvert verts1[Face_MaxVerts+4], verts2[Face_MaxVerts+4];
    int numverts1 = 0,
        numverts2 = 0,
        px = p.verts[p.numverts-1].x,
        py = p.verts[p.numverts-1].y;
    for(int i = 0; i < p.numverts; ++i)
    {
        int x = p.verts[i].x,
            y = p.verts[i].y;
        if(x < b.u1)
        {
            if(px > b.u2)
            {
                verts1[numverts1++] = pvert(b.u2, y + ((y - py)*(b.u2 - x))/(x - px));
            }
            if(px > b.u1)
            {
                verts1[numverts1++] = pvert(b.u1, y + ((y - py)*(b.u1 - x))/(x - px));
            }
        }
        else if(x > b.u2)
        {
            if(px < b.u1)
            {
                verts1[numverts1++] = pvert(b.u1, y + ((y - py)*(b.u1 - x))/(x - px));
            }
            if(px < b.u2)
            {
                verts1[numverts1++] = pvert(b.u2, y + ((y - py)*(b.u2 - x))/(x - px));
            }
        }
        else
        {
            if(px < b.u1)
            {
                if(x > b.u1)
                {
                    verts1[numverts1++] = pvert(b.u1, y + ((y - py)*(b.u1 - x))/(x - px));
                }
            }
            else if(px > b.u2 && x < b.u2)
            {
                verts1[numverts1++] = pvert(b.u2, y + ((y - py)*(b.u2 - x))/(x - px));
            }
            verts1[numverts1++] = pvert(x, y);
        }
        px = x;
        py = y;
    }
    if(numverts1 < 3)
    {
        return false;
    }
    px = verts1[numverts1-1].x;
    py = verts1[numverts1-1].y;
    for(int i = 0; i < numverts1; ++i)
    {
        int x = verts1[i].x, y = verts1[i].y;
        if(y < b.v1)
        {
            if(py > b.v2)
            {
                verts2[numverts2++] = pvert(x + ((x - px)*(b.v2 - y))/(y - py), b.v2);
            }
            if(py > b.v1)
            {
                verts2[numverts2++] = pvert(x + ((x - px)*(b.v1 - y))/(y - py), b.v1);
            }
        }
        else if(y > b.v2)
        {
            if(py < b.v1)
            {
                verts2[numverts2++] = pvert(x + ((x - px)*(b.v1 - y))/(y - py), b.v1);
            }
            if(py < b.v2)
            {
                verts2[numverts2++] = pvert(x + ((x - px)*(b.v2 - y))/(y - py), b.v2);
            }
        }
        else
        {
            if(py < b.v1)
            {
                if(y > b.v1)
                {
                    verts2[numverts2++] = pvert(x + ((x - px)*(b.v1 - y))/(y - py), b.v1);
                }
            }
            else if(py > b.v2 && y < b.v2)
            {
                verts2[numverts2++] = pvert(x + ((x - px)*(b.v2 - y))/(y - py), b.v2);
            }
            verts2[numverts2++] = pvert(x, y);
        }
        px = x;
        py = y;
    }
    if(numverts2 < 3)
    {
        return false;
    }
    if(numverts2 > Face_MaxVerts)
    {
        return false;
    }
    memcpy(p.verts, verts2, numverts2*sizeof(pvert));
    p.numverts = numverts2;
    return true;
}

bool genpoly(cube &cu, int orient, const ivec &o, int size, int vis, ivec &n, int &offset, poly &p)
{
    int dim = DIMENSION(orient),
        coord = DIM_COORD(orient);
    ivec v[4];
    genfaceverts(cu, orient, v);
    if(flataxisface(cu, orient))
    {
         n = ivec(0, 0, 0);
         n[dim] = coord ? 1 : -1;
    }
    else
    {
        if(faceconvexity(v))
        {
            return false;
        }
        n.cross(ivec(v[1]).sub(v[0]), ivec(v[2]).sub(v[0]));
        if(n.iszero())
        {
            n.cross(ivec(v[2]).sub(v[0]), ivec(v[3]).sub(v[0]));
        }
        reduceslope(n);
    }

    ivec po = ivec(o).mask(0xFFF).shl(3);
    for(int k = 0; k < 4; ++k)
    {
        v[k].mul(size).add(po);
    }
    offset = -n.dot(v[3]);
    int r = R[dim],
        c = C[dim],
        order = vis&4 ? 1 : 0;
    p.numverts = 0;
    if(coord)
    {
        const ivec &v0 = v[order];
        p.verts[p.numverts++] = pvert(v0[c], v0[r]);
        if(vis&1)
        {
            const ivec &v1 = v[order+1];
            p.verts[p.numverts++] = pvert(v1[c], v1[r]);
        }
        const ivec &v2 = v[order+2];
        p.verts[p.numverts++] = pvert(v2[c], v2[r]);
        if(vis&2)
        {
            const ivec &v3 = v[(order+3)&3];
            p.verts[p.numverts++] = pvert(v3[c], v3[r]);
        }
    }
    else
    {
        //3&1 are checked, 0&2 are not
        if(vis&2)
        {
            const ivec &v3 = v[(order+3)&3];
            p.verts[p.numverts++] = pvert(v3[c], v3[r]);
        }
        const ivec &v2 = v[order+2];
        p.verts[p.numverts++] = pvert(v2[c], v2[r]);
        if(vis&1)
        {
            const ivec &v1 = v[order+1];
            p.verts[p.numverts++] = pvert(v1[c], v1[r]);
        }
        const ivec &v0 = v[order];
        p.verts[p.numverts++] = pvert(v0[c], v0[r]);
    }

    if(faceedges(cu, orient) != facesolid)
    {
        int px = static_cast<int>(p.verts[p.numverts-2].x) - static_cast<int>(p.verts[p.numverts-3].x),
            py = static_cast<int>(p.verts[p.numverts-2].y) - static_cast<int>(p.verts[p.numverts-3].y),
            cx = static_cast<int>(p.verts[p.numverts-1].x) - static_cast<int>(p.verts[p.numverts-2].x),
            cy = static_cast<int>(p.verts[p.numverts-1].y) - static_cast<int>(p.verts[p.numverts-2].y),
            dir = px*cy - py*cx;
        if(dir > 0)
        {
            return false;
        }
        if(!dir)
        {
            if(p.numverts < 4)
            {
                return false;
            }
            p.verts[p.numverts-2] = p.verts[p.numverts-1];
            p.numverts--;
        }
        px = cx;
        py = cy;
        cx = static_cast<int>(p.verts[0].x) - static_cast<int>(p.verts[p.numverts-1].x);
        cy = static_cast<int>(p.verts[0].y) - static_cast<int>(p.verts[p.numverts-1].y);
        dir = px*cy - py*cx;
        if(dir > 0)
        {
            return false;
        }
        if(!dir)
        {
            if(p.numverts < 4)
            {
                return false;
            }
            p.numverts--;
        }
        px = cx;
        py = cy;
        cx = static_cast<int>(p.verts[1].x) - static_cast<int>(p.verts[0].x);
        cy = static_cast<int>(p.verts[1].y) - static_cast<int>(p.verts[0].y);
        dir = px*cy - py*cx;
        if(dir > 0)
        {
            return false;
        }
        if(!dir)
        {
            if(p.numverts < 4)
            {
                return false;
            }
            p.verts[0] = p.verts[p.numverts-1];
            p.numverts--;
        }
        px = cx; py = cy;
        cx = static_cast<int>(p.verts[2].x) - static_cast<int>(p.verts[1].x);
        cy = static_cast<int>(p.verts[2].y) - static_cast<int>(p.verts[1].y);
        dir = px*cy - py*cx;
        if(dir > 0)
        {
            return false;
        }
        if(!dir)
        {
            if(p.numverts < 4)
            {
                return false;
            }
            p.verts[1] = p.verts[2];
            p.verts[2] = p.verts[3];
            p.numverts--;
        }
    }
    p.c = &cu;
    p.merged = false;
    if(minface && size >= 1<<minface && touchingface(cu, orient))
    {
        facebounds b;
        b.u1 = b.u2 = p.verts[0].x;
        b.v1 = b.v2 = p.verts[0].y;
        for(int i = 1; i < p.numverts; i++)
        {
            const pvert &v = p.verts[i];
            b.u1 = min(b.u1, v.x);
            b.u2 = max(b.u2, v.x);
            b.v1 = min(b.v1, v.y);
            b.v2 = max(b.v2, v.y);
        }
        if(mincubeface(cu, orient, o, size, b) && clippoly(p, b))
        {
            p.merged = true;
        }
    }
    return true;
}
struct plink : pedge
{
    int polys[2];

    plink()
    {
        clear();
    }

    plink(const pedge &p) : pedge(p)
    {
        clear();
    }

    void clear()
    {
        polys[0] = polys[1] = -1;
    }
};

bool mergepolys(int orient, hashset<plink> &links, vector<plink *> &queue, int owner, poly &p, poly &q, const pedge &e)
{
    int pe = -1,
        qe = -1;
    for(int i = 0; i < p.numverts; ++i)
    {
        if(p.verts[i] == e.from)
        {
            pe = i;
            break;
        }
    }
    for(int i = 0; i < q.numverts; ++i)
    {
        if(q.verts[i] == e.to)
        {
            qe = i;
            break;
        }
    }
    if(pe < 0 || qe < 0)
    {
        return false;
    }
    if(p.verts[(pe+1)%p.numverts] != e.to || q.verts[(qe+1)%q.numverts] != e.from)
    {
        return false;
    }
    /*
     *  c----d
     *  |    |
     *  F----T
     *  |  P |
     *  b----a
     */
    pvert verts[2*Face_MaxVerts];
    int numverts = 0;
    int index = pe+2; // starts at A = T+1, ends at F = T+p.numverts
    for(int i = 0; i < p.numverts-1; ++i)
    {
        if(index >= p.numverts)
        {
            index -= p.numverts;
        }
        verts[numverts++] = p.verts[index++];
    }
    index = qe+2; // starts at C = T+2 = F+1, ends at T = T+q.numverts
    int px = static_cast<int>(verts[numverts-1].x) - static_cast<int>(verts[numverts-2].x);
    int py = static_cast<int>(verts[numverts-1].y) - static_cast<int>(verts[numverts-2].y);
    for(int i = 0; i < q.numverts-1; ++i)
    {
        if(index >= q.numverts)
        {
            index -= q.numverts;
        }
        pvert &src = q.verts[index++];
        int cx = static_cast<int>(src.x) - static_cast<int>(verts[numverts-1].x);
        int cy = static_cast<int>(src.y) - static_cast<int>(verts[numverts-1].y);
        int dir = px*cy - py*cx;
        if(dir > 0)
        {
            return false;
        }
        if(!dir)
        {
            numverts--;
        }
        verts[numverts++] = src;
        px = cx;
        py = cy;
    }
    int cx = static_cast<int>(verts[0].x) - static_cast<int>(verts[numverts-1].x);
    int cy = static_cast<int>(verts[0].y) - static_cast<int>(verts[numverts-1].y);
    int dir = px*cy - py*cx;
    if(dir > 0)
    {
        return false;
    }
    if(!dir)
    {
        numverts--;
    }
    if(numverts > Face_MaxVerts)
    {
        return false;
    }
    q.merged = true;
    q.numverts = 0;
    p.merged = true;
    p.numverts = numverts;
    memcpy(p.verts, verts, numverts*sizeof(pvert));
    int prev = p.numverts-1;
    for(int j = 0; j < p.numverts; ++j)
    {
        pedge e(p.verts[prev], p.verts[j]);
        int order = e.from.x > e.to.x || (e.from.x == e.to.x && e.from.y > e.to.y) ? 1 : 0;
        if(order)
        {
            swap(e.from, e.to);
        }
        plink &l = links.access(e, e);
        bool shouldqueue = l.polys[order] < 0 && l.polys[order^1] >= 0;
        l.polys[order] = owner;
        if(shouldqueue)
        {
            queue.add(&l);
        }
        prev = j;
    }

    return true;
}

void addmerge(cube &cu, int orient, const ivec &co, const ivec &n, int offset, poly &p)
{
    cu.merged |= 1<<orient;
    if(!p.numverts)
    {
        if(cu.ext)
        {
            cu.ext->surfaces[orient] = AMBIENT_SURFACE;
        }
        return;
    }
    surfaceinfo surf = BRIGHT_SURFACE;
    vertinfo verts[Face_MaxVerts];
    surf.numverts |= p.numverts;
    int dim = DIMENSION(orient),
        coord = DIM_COORD(orient),
        c = C[dim],
        r = R[dim];
    for(int k = 0; k < p.numverts; ++k)
    {
        pvert &src = p.verts[coord ? k : p.numverts-1-k];
        vertinfo &dst = verts[k];
        ivec v;
        v[c] = src.x;
        v[r] = src.y;
        v[dim] = -(offset + n[c]*src.x + n[r]*src.y)/n[dim];
        dst.set(v);
    }
    if(cu.ext)
    {
        const surfaceinfo &oldsurf = cu.ext->surfaces[orient];
        int numverts = oldsurf.numverts&Face_MaxVerts;
        if(numverts == p.numverts)
        {
            ivec v0 = verts[0].getxyz();
            const vertinfo *oldverts = cu.ext->verts() + oldsurf.verts;
            for(int j = 0; j < numverts; ++j)
            {
                if(v0 == oldverts[j].getxyz())
                {
                    for(int k = 1; k < numverts; ++k)
                    {
                        if(++j >= numverts)
                        {
                            j = 0;
                        }
                        if(verts[k].getxyz() != oldverts[j].getxyz())
                        {
                            goto nomatch;
                        }
                    }
                    return;
                }
            }
        nomatch:;
        }
    }
    setsurface(cu, orient, surf, verts, p.numverts);
}

static inline void clearmerge(cube &c, int orient)
{
    if(c.merged&(1<<orient))
    {
        c.merged &= ~(1<<orient);
        if(c.ext)
        {
            c.ext->surfaces[orient] = BRIGHT_SURFACE;
        }
    }
}

void addmerges(int orient, const ivec &co, const ivec &n, int offset, vector<poly> &polys)
{
    for(int i = 0; i < polys.length(); i++)
    {
        poly &p = polys[i];
        if(p.merged)
        {
            addmerge(*p.c, orient, co, n, offset, p);
        }
        else
        {
            clearmerge(*p.c, orient);
        }
    }
}

void mergepolys(int orient, const ivec &co, const ivec &n, int offset, vector<poly> &polys)
{
    if(polys.length() <= 1)
    {
        addmerges(orient, co, n, offset, polys);
        return;
    }
    hashset<plink> links(polys.length() <= 32 ? 128 : 1024);
    vector<plink *> queue;
    for(int i = 0; i < polys.length(); i++)
    {
        poly &p = polys[i];
        int prev = p.numverts-1;
        for(int j = 0; j < p.numverts; ++j)
        {
            pedge e(p.verts[prev], p.verts[j]);
            int order = e.from.x > e.to.x || (e.from.x == e.to.x && e.from.y > e.to.y) ? 1 : 0;
            if(order)
            {
                swap(e.from, e.to);
            }
            plink &l = links.access(e, e);
            l.polys[order] = i;
            if(l.polys[0] >= 0 && l.polys[1] >= 0)
            {
                queue.add(&l);
            }
            prev = j;
        }
    }
    vector<plink *> nextqueue;
    while(queue.length())
    {
        for(int i = 0; i < queue.length(); i++)
        {
            plink &l = *queue[i];
            if(l.polys[0] >= 0 && l.polys[1] >= 0)
            {
                mergepolys(orient, links, nextqueue, l.polys[0], polys[l.polys[0]], polys[l.polys[1]], l);
            }
        }
        queue.setsize(0);
        queue.move(nextqueue);
    }
    addmerges(orient, co, n, offset, polys);
}

static int genmergeprogress = 0;

struct cfpolys
{
    vector<poly> polys;
};

static hashtable<cfkey, cfpolys> cpolys;

void genmerges(cube *c = worldroot, const ivec &o = ivec(0, 0, 0), int size = worldsize>>1)
{
    neighborstack[++neighbordepth] = c;
    for(int i = 0; i < 8; ++i)
    {
        ivec co(i, o, size);
        int vis;
        if(c[i].children)
        {
            genmerges(c[i].children, co, size>>1);
        }
        else if(!IS_EMPTY(c[i]))
        {
            for(int j = 0; j < 6; ++j)
            {
                if((vis = visibletris(c[i], j, co, size)))
                {
                    cfkey k;
                    poly p;
                    if(size < 1<<maxmerge && c != worldroot)
                    {
                        if(genpoly(c[i], j, co, size, vis, k.n, k.offset, p))
                        {
                            k.orient = j;
                            k.tex = c[i].texture[j];
                            k.material = c[i].material&Mat_Alpha;
                            cpolys[k].polys.add(p);
                            continue;
                        }
                    }
                    else if(minface && size >= 1<<minface && touchingface(c[i], j))
                    {
                        if(genpoly(c[i], j, co, size, vis, k.n, k.offset, p) && p.merged)
                        {
                            addmerge(c[i], j, co, k.n, k.offset, p);
                            continue;
                        }
                    }
                    clearmerge(c[i], j);
                }
            }
        }
        if((size == 1<<maxmerge || c == worldroot) && cpolys.numelems)
        {
            ENUMERATE_KT(cpolys, cfkey, key, cfpolys, val,
            {
                mergepolys(key.orient, co, key.n, key.offset, val.polys);
            });
            cpolys.clear();
        }
    }
    --neighbordepth;
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
        x1 = min(x1, v.x);
        x2 = max(x2, v.x);
        y1 = min(y1, v.y);
        y2 = max(y2, v.y);
        z1 = min(z1, v.z);
        z2 = max(z2, v.z);
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
            c.ext->va = NULL;
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
            invalidatemerges(c.children[i]);
        }
    }
}

void calcmerges()
{
    genmergeprogress = 0;
    genmerges();
}

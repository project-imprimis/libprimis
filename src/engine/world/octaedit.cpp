/* octaedit.cpp: world modification core functionality
 *
 * modifying the octree grid can be done by changing the states of cube nodes within
 * the world, which is made easier with octaedit.cpp's notions of selections (a
 * rectangular selection of cubes with which to modify together). Selections can
 * be modified all at once, copied, and pasted throughout the world instead of individual
 * cubes being modified.
 *
 * additionally, this file contains core functionality for rendering of selections
 * and other constructs generally useful for modifying the level, such as entity
 * locations and radii.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include "light.h"
#include "octaedit.h"
#include "octaworld.h"
#include "physics.h"
#include "raycube.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/input.h"

#include "render/hud.h"
#include "render/octarender.h"
#include "render/rendergl.h"
#include "render/renderlights.h"
#include "render/renderva.h"
#include "render/texture.h"

#include "material.h"
#include "world.h"

bool boxoutline = false;

void boxs(int orient, vec o, const vec &s, float size)
{
    int d  = DIMENSION(orient),
        dc = DIM_COORD(orient);
    float f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;
    o[D[d]] += dc * s[D[d]] + f;

    vec r(0, 0, 0),
        c(0, 0, 0);
    r[R[d]] = s[R[d]];
    c[C[d]] = s[C[d]];

    vec v1 = o,
        v2 = vec(o).add(r),
        v3 = vec(o).add(r).add(c),
        v4 = vec(o).add(c);

    r[R[d]] = 0.5f*size;
    c[C[d]] = 0.5f*size;

    gle::defvertex();
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attrib(vec(v1).sub(r).sub(c));
    gle::attrib(vec(v1).add(r).add(c));

    gle::attrib(vec(v2).add(r).sub(c));
    gle::attrib(vec(v2).sub(r).add(c));

    gle::attrib(vec(v3).add(r).add(c));
    gle::attrib(vec(v3).sub(r).sub(c));

    gle::attrib(vec(v4).sub(r).add(c));
    gle::attrib(vec(v4).add(r).sub(c));

    gle::attrib(vec(v1).sub(r).sub(c));
    gle::attrib(vec(v1).add(r).add(c));
    xtraverts += gle::end();
}

void boxs(int orient, vec origin, const vec &s)
{
    int d  = DIMENSION(orient),
        dc = DIM_COORD(orient);
    float f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;
    origin[D[d]] += dc * s[D[d]] + f;

    gle::defvertex();
    gle::begin(GL_LINE_LOOP);
    //draw four surfaces
    gle::attrib(origin); origin[R[d]] += s[R[d]];
    gle::attrib(origin); origin[C[d]] += s[C[d]];
    gle::attrib(origin); origin[R[d]] -= s[R[d]];
    gle::attrib(origin);

    xtraverts += gle::end();
}

void boxs3D(const vec &origin, vec s, int g)
{
    s.mul(g); //multiply displacement by g(ridpower)
    for(int i = 0; i < 6; ++i) //for each face
    {
        boxs(i, origin, s);
    }
}

void boxsgrid(int orient, vec origin, vec s, int g)
{
    int d  = DIMENSION(orient),
        dc = DIM_COORD(orient);
    float ox = origin[R[d]],
          oy = origin[C[d]],
          xs = s[R[d]],
          ys = s[C[d]],
          f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;

    origin[D[d]] += dc * s[D[d]]*g + f;

    gle::defvertex();
    gle::begin(GL_LINES);
    for(int x = 0; x < xs; ++x)
    {
        origin[R[d]] += g;
        gle::attrib(origin);
        origin[C[d]] += ys*g;
        gle::attrib(origin);
        origin[C[d]] = oy;
    }
    for(int y = 0; y < ys; ++y)
    {
        origin[C[d]] += g;
        origin[R[d]] = ox;
        gle::attrib(origin);
        origin[R[d]] += xs*g;
        gle::attrib(origin);
    }
    xtraverts += gle::end();
}

selinfo sel, lastsel, savedsel;

int orient = 0,
    gridsize = 8;
ivec cor, lastcor,
     cur, lastcur;

bool editmode     = false,
     multiplayer  = false,
     allowediting = false,
     havesel      = false,
     hmapsel      = false;
int horient  = 0,
    entmoving = 0;


VARF(entediting, 0, 0, 1,
{
    if(!entediting)
    {
        entcancel();
    }
});


void multiplayerwarn()
{
    conoutf(Console_Error, "operation not available in multiplayer");
}

bool pointinsel(const selinfo &sel, const vec &origin)
{
    return(origin.x <= sel.o.x+sel.s.x*sel.grid
        && origin.x >= sel.o.x
        && origin.y <= sel.o.y+sel.s.y*sel.grid
        && origin.y >= sel.o.y
        && origin.z <= sel.o.z+sel.s.z*sel.grid
        && origin.z >= sel.o.z);
}

VARF(dragging, 0, 0, 1,
    if(!dragging || cor[0]<0)
    {
        return;
    }
    lastcur = cur;
    lastcor = cor;
    sel.grid = gridsize;
    sel.orient = orient;
);

int moving = 0;

void movingcmd(int *n)
{
    if(*n >= 0)
    {
        if(!*n || (moving<=1 && !pointinsel(sel, vec(cur).add(1))))
        {
            moving = 0;
        }
        else if(!moving)
        {
            moving = 1;
        }
    }
    intret(moving);
}
COMMANDN(moving, movingcmd, "b");

VARF(gridpower, 0, 3, 12,
{
    if(dragging)
    {
        return;
    }
    gridsize = 1<<gridpower;
    if(gridsize>=worldsize)
    {
        gridsize = worldsize/2;
    }
    cancelsel();
});

VAR(passthroughsel, 0, 0, 1);
VAR(selectcorners, 0, 0, 1);
VARF(hmapedit, 0, 0, 1, horient = sel.orient);

void forcenextundo() { lastsel.orient = -1; }

namespace hmap { void cancel(); }

void cubecancel()
{
    havesel = false;
    moving = dragging = hmapedit = passthroughsel = 0;
    forcenextundo();
    hmap::cancel();
}

void cancelsel()
{
    cubecancel();
    entcancel();
}

bool haveselent()
{
    return entgroup.length() > 0;
}

bool noedit(bool inview, bool msg)
{
    if(!editmode)
    {
        if(msg)
        {
            conoutf(Console_Error, "operation only allowed in edit mode");
        }
        return true;
    }
    if(inview || haveselent())
    {
        return false;
    }
    vec o(sel.o), s(sel.s);
    s.mul(sel.grid / 2.0f);
    o.add(s);
    float r = std::max(std::max(s.x, s.y), s.z);
    bool viewable = view.isvisiblesphere(r, o) != ViewFrustumCull_NotVisible;
    if(!viewable && msg)
    {
        conoutf(Console_Error, "selection not in view");
    }
    return !viewable;
}

void reorient()
{
    sel.cx = 0;
    sel.cy = 0;
    sel.cxs = sel.s[R[DIMENSION(orient)]]*2;
    sel.cys = sel.s[C[DIMENSION(orient)]]*2;
    sel.orient = orient;
}

void selextend()
{
    if(noedit(true))
    {
        return;
    }
    for(int i = 0; i < 3; ++i)
    {
        if(cur[i]<sel.o[i])
        {
            sel.s[i] += (sel.o[i]-cur[i])/sel.grid;
            sel.o[i] = cur[i];
        }
        else if(cur[i]>=sel.o[i]+sel.s[i]*sel.grid)
        {
            sel.s[i] = (cur[i]-sel.o[i])/sel.grid+1;
        }
    }
}

COMMAND(entcancel, "");
COMMAND(cubecancel, "");
COMMAND(cancelsel, "");
COMMAND(reorient, "");
COMMAND(selextend, "");

void selmoved()
{
    if(noedit(true))
    {
        return;
    }
    intret(sel.o != savedsel.o ? 1 : 0);
}
COMMAND(selmoved, "");

void selsave()
{
    if(noedit(true))
    {
        return;
    }
    savedsel = sel;
}
COMMAND(selsave, "");

void selrestore()
{
    if(noedit(true))
    {
        return;
    }
    sel = savedsel;
}
COMMAND(selrestore, "");

void selswap()
{
    if(noedit(true))
    {
        return;
    }
    std::swap(sel, savedsel);
}
COMMAND(selswap, "");

///////// selection support /////////////

cube &blockcube(int x, int y, int z, const block3 &b, int rgrid) // looks up a world cube, based on coordinates mapped by the block
{
    int dim = DIMENSION(b.orient),
        dc = DIM_COORD(b.orient);
    ivec s(dim, x*b.grid, y*b.grid, dc*(b.s[dim]-1)*b.grid);
    s.add(b.o);
    if(dc)
    {
        s[dim] -= z*b.grid;
    }
    else
    {
        s[dim] += z*b.grid;
    }
    return rootworld.lookupcube(s, rgrid);
}

////////////// cursor ///////////////

int selchildcount = 0,
    selchildmat = -1;

void haveselcmd()
{
    intret(havesel ? selchildcount : 0);
}
COMMANDN(havesel, haveselcmd, "");

void selchildcountcmd()
{
    if(selchildcount < 0)
    {
        result(tempformatstring("1/%d", -selchildcount));
    }
    else
    {
        intret(selchildcount);
    }
}
COMMANDN(selchildcount, selchildcountcmd, "");

void selchildmatcmd(char *prefix)
{
    if(selchildmat > 0)
    {
        result(getmaterialdesc(selchildmat, prefix));
    }
}
COMMANDN(selchildmat, selchildmatcmd, "s");

void countselchild(cube *c, const ivec &cor, int size)
{
    ivec ss = ivec(sel.s).mul(sel.grid);
    LOOP_OCTA_BOX_SIZE(cor, size, sel.o, ss)
    {
        ivec o(i, cor, size);
        if(c[i].children)
        {
            countselchild(c[i].children, o, size/2);
        }
        else
        {
            selchildcount++;
            if(c[i].material != Mat_Air && selchildmat != Mat_Air)
            {
                if(selchildmat < 0)
                {
                    selchildmat = c[i].material;
                }
                else if(selchildmat != c[i].material)
                {
                    selchildmat = Mat_Air;
                }
            }
        }
    }
}

void normalizelookupcube(const ivec &o)
{
    if(lusize>gridsize)
    {
        lu.x += (o.x-lu.x)/gridsize*gridsize;
        lu.y += (o.y-lu.y)/gridsize*gridsize;
        lu.z += (o.z-lu.z)/gridsize*gridsize;
    }
    else if(gridsize>lusize)
    {
        lu.x &= ~(gridsize-1);
        lu.y &= ~(gridsize-1);
        lu.z &= ~(gridsize-1);
    }
    lusize = gridsize;
}

void updateselection()
{
    sel.o.x = std::min(lastcur.x, cur.x);
    sel.o.y = std::min(lastcur.y, cur.y);
    sel.o.z = std::min(lastcur.z, cur.z);
    sel.s.x = std::abs(lastcur.x-cur.x)/sel.grid+1;
    sel.s.y = std::abs(lastcur.y-cur.y)/sel.grid+1;
    sel.s.z = std::abs(lastcur.z-cur.z)/sel.grid+1;
}

bool editmoveplane(const vec &o, const vec &ray, int d, float off, vec &handle, vec &dest, bool first)
{
    plane pl(d, off);
    float dist = 0.0f;
    if(!pl.rayintersect(player->o, ray, dist))
    {
        return false;
    }
    dest = vec(ray).mul(dist).add(player->o);
    if(first)
    {
        handle = vec(dest).sub(o);
    }
    dest.sub(handle);
    return true;
}

namespace hmap
{
    bool isheightmap(int o, int d, bool empty, cube &c);
}

//////////// ready changes to vertex arrays ////////////

static bool haschanged = false;

static void readychanges(const ivec &bbmin, const ivec &bbmax, cube *c, const ivec &cor, int size)
{
    LOOP_OCTA_BOX(cor, size, bbmin, bbmax)
    {
        ivec o(i, cor, size);
        if(c[i].ext)
        {
            if(c[i].ext->va)             // removes va s so that octarender will recreate
            {
                int hasmerges = c[i].ext->va->hasmerges;
                destroyva(c[i].ext->va);
                c[i].ext->va = nullptr;
                if(hasmerges)
                {
                    invalidatemerges(c[i]);
                }
            }
            freeoctaentities(c[i]);
            c[i].ext->tjoints = -1;
        }
        if(c[i].children)
        {
            if(size<=1)
            {
                setcubefaces(c[i], facesolid);
                c[i].discardchildren(true);
                brightencube(c[i]);
            }
            else
            {
                readychanges(bbmin, bbmax, c[i].children, o, size/2);
            }
        }
        else
        {
            brightencube(c[i]);
        }
    }
}

void commitchanges(bool force)
{
    if(!force && !haschanged)
    {
        return;
    }
    haschanged = false;
    int oldlen = valist.length();
    resetclipplanes();
    entitiesinoctanodes();
    inbetweenframes = false;
    rootworld.octarender();
    inbetweenframes = true;
    setupmaterials(oldlen);
    clearshadowcache();
    updatevabbs();
}

void cubeworld::changed(const ivec &bbmin, const ivec &bbmax, bool commit)
{
    readychanges(bbmin, bbmax, worldroot, ivec(0, 0, 0), worldsize/2);
    haschanged = true;

    if(commit)
    {
        commitchanges();
    }
}

void cubeworld::changed(const block3 &sel, bool commit)
{
    if(sel.s.iszero())
    {
        return;
    }
    readychanges(ivec(sel.o).sub(1), ivec(sel.s).mul(sel.grid).add(sel.o).add(1), worldroot, ivec(0, 0, 0), worldsize/2);
    haschanged = true;
    if(commit)
    {
        commitchanges();
    }
}

//////////// copy and undo /////////////
static void copycube(const cube &src, cube &dst)
{
    dst = src;
    dst.visible = 0;
    dst.merged = 0;
    dst.ext = nullptr; // src cube is responsible for va destruction
    //recursively apply to children
    if(src.children)
    {
        dst.children = newcubes(faceempty);
        for(int i = 0; i < 8; ++i)
        {
            copycube(src.children[i], dst.children[i]);
        }
    }
}

void pastecube(const cube &src, cube &dst)
{
    dst.discardchildren();
    copycube(src, dst);
}

void blockcopy(const block3 &s, int rgrid, block3 *b)
{
    *b = s;
    cube *q = b->c();
    LOOP_XYZ(s, rgrid, copycube(c, *q++));
}

block3 *blockcopy(const block3 &s, int rgrid)
{
    int bsize = sizeof(block3)+sizeof(cube)*s.size();
    if(bsize <= 0 || bsize > (100<<20))
    {
        return nullptr;
    }
    block3 *b = reinterpret_cast<block3 *>(new uchar[bsize]); //create a new block3 pointing to an appropriate sized memory area
    if(b) //should always be true
    {
        blockcopy(s, rgrid, b); //copy the block3 s to b
    }
    return b;
}

void freeblock(block3 *b, bool alloced = true)
{
    cube *q = b->c();
    for(int i = 0; i < b->size(); ++i)
    {
        (*q++).discardchildren(); //note: incrementing pointer
    }
    if(alloced)
    {
        delete[] b;
    }
}

void selgridmap(const selinfo &sel, uchar *g)                           // generates a map of the cube sizes at each grid point
{
    LOOP_XYZ(sel, -sel.grid, (*g++ = BITSCAN(lusize), static_cast<void>(c)));
}

void freeundo(undoblock *u)
{
    if(!u->numents)
    {
        freeblock(u->block(), false);
    }
    delete[] reinterpret_cast<uchar *>(u);  //re-cast to uchar array so it can be destructed properly
}

static int undosize(undoblock *u)
{
    if(u->numents)
    {
        return u->numents*sizeof(undoent);
    }
    else
    {
        block3 *b = u->block();
        cube *q = b->c();
        int size = b->size(),
            total = size;
        for(int j = 0; j < size; ++j)
        {
            total += familysize(*q++)*sizeof(cube);
        }
        return total;
    }
}

undolist undos, redos;
VARP(undomegs, 0, 5, 100);                              // bounded by n megs, zero means no undo history
int totalundos = 0;

void pruneundos(int maxremain)                          // bound memory
{
    while(totalundos > maxremain && !undos.empty())
    {
        undoblock *u = undos.popfirst();
        totalundos -= u->size;
        freeundo(u);
    }
    //conoutf(CON_DEBUG, "undo: %d of %d(%%%d)", totalundos, undomegs<<20, totalundos*100/(undomegs<<20));
    while(!redos.empty())
    {
        undoblock *u = redos.popfirst();
        totalundos -= u->size;
        freeundo(u);
    }
}

void clearundos()
{
    pruneundos(0);
}

COMMAND(clearundos, ""); //run pruneundos but with a cache size of zero

undoblock *newundocube(const selinfo &s)
{
    int ssize = s.size(),
        selgridsize = ssize,
        blocksize = sizeof(block3)+ssize*sizeof(cube);
    if(blocksize <= 0 || blocksize > (undomegs<<20))
    {
        return nullptr;
    }
    undoblock *u = reinterpret_cast<undoblock *>(new uchar[sizeof(undoblock) + blocksize + selgridsize]);
    if(!u)
    {
        return nullptr;
    }
    u->numents = 0;
    block3 *b = u->block();
    blockcopy(s, -s.grid, b);
    uchar *g = u->gridmap();
    selgridmap(s, g);
    return u;
}

void addundo(undoblock *u)
{
    u->size = undosize(u);
    u->timestamp = totalmillis;
    undos.add(u);
    totalundos += u->size;
    pruneundos(undomegs<<20);
}

VARP(nompedit, 0, 1, 1);

static int countblock(cube *c, int n = 8)
{
    int r = 0;
    for(int i = 0; i < n; ++i)
    {
        if(c[i].children)
        {
            r += countblock(c[i].children);
        }
        else
        {
            ++r;
        }
    }
    return r;
}

int countblock(block3 *b)
{
    return countblock(b->c(), b->size());
}

std::vector<editinfo *> editinfos;
editinfo *localedit = nullptr;

template<class B>
static void packcube(cube &c, B &buf)
{
    //recursvely apply to children
    if(c.children)
    {
        buf.put(0xFF);
        for(int i = 0; i < 8; ++i)
        {
            packcube(c.children[i], buf);
        }
    }
    else
    {
        cube data = c;
        buf.put(c.material&0xFF);
        buf.put(c.material>>8);
        buf.put(data.edges, sizeof(data.edges));
        buf.put(reinterpret_cast<uchar *>(data.texture), sizeof(data.texture));
    }
}

template<class B>
static bool packblock(block3 &b, B &buf)
{
    if(b.size() <= 0 || b.size() > (1<<20))
    {
        return false;
    }
    block3 hdr = b;
    buf.put(reinterpret_cast<const uchar *>(&hdr), sizeof(hdr));
    cube *c = b.c();
    for(int i = 0; i < static_cast<int>(b.size()); ++i)
    {
        packcube(c[i], buf);
    }
    return true;
}

struct vslothdr
{
    ushort index;
    ushort slot;
};

static void packvslots(cube &c, vector<uchar> &buf, vector<ushort> &used)
{
    //recursively apply to children
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            packvslots(c.children[i], buf, used);
        }
    }
    else
    {
        for(int i = 0; i < 6; ++i) //for each face
        {
            ushort index = c.texture[i];
            if(vslots.inrange(index) && vslots[index]->changed && used.find(index) < 0)
            {
                used.add(index);
                VSlot &vs = *vslots[index];
                vslothdr &hdr = *reinterpret_cast<vslothdr *>(buf.pad(sizeof(vslothdr)));
                hdr.index = index;
                hdr.slot = vs.slot->index;
                packvslot(buf, vs);
            }
        }
    }
}

static void packvslots(block3 &b, vector<uchar> &buf)
{
    vector<ushort> used;
    cube *c = b.c();
    for(int i = 0; i < b.size(); ++i)
    {
        packvslots(c[i], buf, used);
    }
    memset(buf.pad(sizeof(vslothdr)), 0, sizeof(vslothdr));
}

template<class B>
static void unpackcube(cube &c, B &buf)
{
    int mat = buf.get();
    if(mat == 0xFF)
    {
        c.children = newcubes(faceempty);
        //recursively apply to children
        for(int i = 0; i < 8; ++i)
        {
            unpackcube(c.children[i], buf);
        }
    }
    else
    {
        c.material = mat | (buf.get()<<8);
        buf.get(c.edges, sizeof(c.edges));
        buf.get(reinterpret_cast<uchar *>(c.texture), sizeof(c.texture));
    }
}

template<class B>
static bool unpackblock(block3 *&b, B &buf)
{
    if(b)
    {
        freeblock(b);
        b = nullptr;
    }
    block3 hdr;
    if(buf.get(reinterpret_cast<uchar *>(&hdr), sizeof(hdr)) < static_cast<int>(sizeof(hdr)))
    {
        return false;
    }
    if(hdr.size() > (1<<20) || hdr.grid <= 0 || hdr.grid > (1<<12))
    {
        return false;
    }
    b = reinterpret_cast<block3 *>(new uchar[sizeof(block3)+hdr.size()*sizeof(cube)]);
    if(!b)
    {
        return false;
    }
    *b = hdr;
    cube *c = b->c();
    memset(c, 0, b->size()*sizeof(cube));
    for(int i = 0; i < b->size(); ++i)
    {
        unpackcube(c[i], buf);
    }
    return true;
}

std::vector<vslotmap> unpackingvslots;

static void unpackvslots(cube &c, ucharbuf &buf)
{
    //recursively apply to children
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            unpackvslots(c.children[i], buf);
        }
    }
    else
    {
        for(int i = 0; i < 6; ++i) //one for each face
        {
            ushort tex = c.texture[i];
            for(uint j = 0; j < unpackingvslots.size(); j++)
            {
                if(unpackingvslots[j].index == tex)
                {
                    c.texture[i] = unpackingvslots[j].vslot->index;
                    break;
                }
            }
        }
    }
}

static void unpackvslots(block3 &b, ucharbuf &buf)
{
    while(buf.remaining() >= static_cast<int>(sizeof(vslothdr)))
    {
        vslothdr &hdr = *reinterpret_cast<vslothdr *>(buf.pad(sizeof(vslothdr)));
        if(!hdr.index)
        {
            break;
        }
        VSlot &vs = *lookupslot(hdr.slot, false).variants;
        VSlot ds;
        if(!unpackvslot(buf, ds, false))
        {
            break;
        }
        if(vs.index < 0 || vs.index == Default_Sky)
        {
            continue;
        }
        VSlot *edit = editvslot(vs, ds);
        unpackingvslots.emplace_back(vslotmap(hdr.index, edit ? edit : &vs));
    }

    cube *c = b.c();
    for(int i = 0; i < b.size(); ++i)
    {
        unpackvslots(c[i], buf);
    }

    unpackingvslots.clear();
}

static bool compresseditinfo(const uchar *inbuf, int inlen, uchar *&outbuf, int &outlen)
{
    uLongf len = compressBound(inlen);
    if(len > (1<<20))
    {
        return false;
    }
    outbuf = new uchar[len];
    if(!outbuf || compress2(static_cast<Bytef *>(outbuf), &len, static_cast<const Bytef *>(inbuf), inlen, Z_BEST_COMPRESSION) != Z_OK || len > (1<<16))
    {
        delete[] outbuf;
        outbuf = nullptr;
        return false;
    }
    outlen = len;
    return true;
}

bool uncompresseditinfo(const uchar *inbuf, int inlen, uchar *&outbuf, int &outlen)
{
    if(compressBound(outlen) > (1<<20))
    {
        return false;
    }
    uLongf len = outlen;
    outbuf = new uchar[len];
    if(!outbuf || uncompress(static_cast<Bytef *>(outbuf), &len, static_cast<const Bytef *>(inbuf), inlen) != Z_OK)
    {
        delete[] outbuf;
        outbuf = nullptr;
        return false;
    }
    outlen = len;
    return true;
}

bool packeditinfo(editinfo *e, int &inlen, uchar *&outbuf, int &outlen)
{
    vector<uchar> buf;
    if(!e || !e->copy || !packblock(*e->copy, buf))
    {
        return false;
    }
    packvslots(*e->copy, buf);
    inlen = buf.length();
    return compresseditinfo(buf.getbuf(), buf.length(), outbuf, outlen);
}

bool unpackeditinfo(editinfo *&e, const uchar *inbuf, int inlen, int outlen)
{
    if(e && e->copy)
    {
        freeblock(e->copy);
        e->copy = nullptr;
    }
    uchar *outbuf = nullptr;
    if(!uncompresseditinfo(inbuf, inlen, outbuf, outlen))
    {
        return false;
    }
    ucharbuf buf(outbuf, outlen);
    if(!e)
    {
        editinfo *e;
        editinfos.push_back(e);
    }
    if(!unpackblock(e->copy, buf))
    {
        delete[] outbuf;
        return false;
    }
    unpackvslots(*e->copy, buf);
    delete[] outbuf;
    return true;
}

void freeeditinfo(editinfo *&e)
{
    if(!e)
    {
        return;
    }
    editinfos.erase(std::find(editinfos.begin(), editinfos.end(), e));
    if(e->copy)
    {
        freeblock(e->copy);
    }
    delete e;
    e = nullptr;
}

bool packundo(undoblock *u, int &inlen, uchar *&outbuf, int &outlen)
{
    vector<uchar> buf;
    buf.reserve(512);
    *reinterpret_cast<ushort *>(buf.pad(2)) = static_cast<ushort>(u->numents);
    if(u->numents)
    {
        undoent *ue = u->ents();
        for(int i = 0; i < u->numents; ++i)
        {
            *reinterpret_cast<ushort *>(buf.pad(2)) = static_cast<ushort>(ue[i].i);
            entity &e = *reinterpret_cast<entity *>(buf.pad(sizeof(entity)));
            e = ue[i].e;
        }
    }
    else
    {
        block3 &b = *u->block();
        if(!packblock(b, buf))
        {
            return false;
        }
        buf.put(u->gridmap(), b.size());
        packvslots(b, buf);
    }
    inlen = buf.length();
    return compresseditinfo(buf.getbuf(), buf.length(), outbuf, outlen);
}

bool packundo(bool undo, int &inlen, uchar *&outbuf, int &outlen)
{
    if(undo)
    {
        return !undos.empty() && packundo(undos.last, inlen, outbuf, outlen);
    }
    else
    {
        return !redos.empty() && packundo(redos.last, inlen, outbuf, outlen);
    }
}

struct prefab : editinfo
{
    char *name;
    GLuint ebo, vbo;
    int numtris, numverts;

    prefab() : name(nullptr), ebo(0), vbo(0), numtris(0), numverts(0) {}
    ~prefab()
    {
        delete[] name;
        if(copy)
        {
            freeblock(copy);
        }
    }

    void cleanup()
    {
        if(ebo)
        {
            glDeleteBuffers(1, &ebo);
            ebo = 0;
        }
        if(vbo)
        {
            glDeleteBuffers(1, &vbo);
            vbo = 0;
        }
        numtris = numverts = 0;
    }
};

static hashnameset<prefab> prefabs;

void cleanupprefabs()
{
    ENUMERATE(prefabs, prefab, p, p.cleanup());
}

void delprefab(char *name)
{
    prefab *p = prefabs.access(name);
    if(p)
    {
        p->cleanup();
        prefabs.remove(name);
        conoutf("deleted prefab %s", name);
    }
}
COMMAND(delprefab, "s");

void pasteundoblock(block3 *b, uchar *g)
{
    cube *s = b->c();
    LOOP_XYZ(*b, 1<<std::min(static_cast<int>(*g++), worldscale-1), pastecube(*s++, c));
}

//used in client prefab unpacking, handles the octree unpacking (not the entities,
// which are game-dependent)
void unpackundocube(ucharbuf buf, uchar *outbuf)
{
    block3 *b = nullptr;
    if(!unpackblock(b, buf) || b->grid >= worldsize || buf.remaining() < b->size())
    {
        freeblock(b);
        delete[] outbuf;
        return;
    }
    uchar *g = buf.pad(b->size());
    unpackvslots(*b, buf);
    pasteundoblock(b, g);
    rootworld.changed(*b, false);
    freeblock(b);
}
/* saveprefab: saves the current selection to a prefab file
 *
 * Parameters:
 *  char * name: a string containing the name of the prefab to save (sans file type)
 * Returns:
 *  void
 * Effects:
 * Using the global variables for selection information, writes the current selection
 * to a prefab file with the given name. Does not save slot information, so pasting
 * into a map with a different texture slot list will result in meaningless textures.
 *
 */
void saveprefab(char *name)
{
    if(!name[0] || noedit(true) || (nompedit && multiplayer))
    {
        multiplayerwarn();
        return;
    }
    prefab *b = prefabs.access(name);
    if(!b)
    {
        b = &prefabs[name];
        b->name = newstring(name);
    }
    if(b->copy)
    {
        freeblock(b->copy);
    }
    PROTECT_SEL(b->copy = blockcopy(block3(sel), sel.grid));
    rootworld.changed(sel);
    DEF_FORMAT_STRING(filename, "media/prefab/%s.obr", name);
    path(filename);
    stream *f = opengzfile(filename, "wb");
    if(!f)
    {
        conoutf(Console_Error, "could not write prefab to %s", filename);
        return;
    }
    prefabheader hdr;
    std::string headermagic = "OEBR";
    std::copy(headermagic.begin(), headermagic.end(), hdr.magic);
    hdr.version = 0;
    f->write(&hdr, sizeof(hdr));
    streambuf<uchar> s(f);
    if(!packblock(*b->copy, s))
    {
        delete f;
        conoutf(Console_Error, "could not pack prefab %s", filename);
        return;
    }
    delete f;
    conoutf("wrote prefab file %s", filename);
}
COMMAND(saveprefab, "s");

void makeundo(selinfo &s)
{
    undoblock *u = newundocube(s);
    if(u)
    {
        addundo(u);
    }
}

void makeundo()                        // stores state of selected cubes before editing
{
    if(lastsel==sel || sel.s.iszero())
    {
        return;
    }
    lastsel=sel;
    makeundo(sel);
}

void pasteblock(block3 &b, selinfo &sel, bool local)
{
    sel.s = b.s;
    int o = sel.orient;
    sel.orient = b.orient;
    cube *s = b.c();
    LOOP_SEL_XYZ(if(!(s->isempty()) || s->children || s->material != Mat_Air) pastecube(*s, c); s++); // 'transparent'. old opaque by 'delcube; paste'
    sel.orient = o;
}

prefab *loadprefab(const char *name, bool msg = true)
{
    prefab *b = prefabs.access(name);
    if(b)
    {
        return b;
    }
    DEF_FORMAT_STRING(filename, "media/prefab/%s.obr", name);
    path(filename);
    stream *f = opengzfile(filename, "rb");
    if(!f)
    {
        if(msg)
        {
            conoutf(Console_Error, "could not read prefab %s", filename);
        }
        return nullptr;
    }
    prefabheader hdr;
    if(f->read(&hdr, sizeof(hdr)) != sizeof(prefabheader) || memcmp(hdr.magic, "OEBR", 4))
    {
        delete f;
        if(msg)
        {
            conoutf(Console_Error, "prefab %s has malformatted header", filename);
            return nullptr;
        }
    }
    if(hdr.version != 0)
    {
        delete f;
        if(msg)
        {
           conoutf(Console_Error, "prefab %s uses unsupported version", filename);
           return nullptr;
        }
    }
    streambuf<uchar> s(f);
    block3 *copy = nullptr;
    if(!unpackblock(copy, s))
    {
        delete f;
        if(msg)
        {
            conoutf(Console_Error, "could not unpack prefab %s", filename);
            return nullptr;
        }
    }
    delete f;

    b = &prefabs[name];
    b->name = newstring(name);
    b->copy = copy;

    return b;
}

static void pasteprefab(char *name)
{
    if(!name[0] || noedit() || (nompedit && multiplayer))
    {
        multiplayerwarn();
        return;
    }
    prefab *b = loadprefab(name, true);
    if(b)
    {
        pasteblock(*b->copy, sel, true);
    }
}
COMMAND(pasteprefab, "s");

class prefabmesh
{
    public:
        struct vertex
        {
            vec pos;
            vec4<uchar> norm;
        };

        static constexpr int prefabmeshsize = 1<<9;
        int table[prefabmeshsize];
        std::vector<vertex> verts;
        std::vector<int> chain;
        std::vector<ushort> tris;

        prefabmesh() { memset(table, -1, sizeof(table)); }

        int addvert(const vec &pos, const bvec &norm)
        {
            vertex vtx;
            vtx.pos = pos;
            vtx.norm = norm;
            return addvert(vtx);
        }

        void setup(prefab &p)
        {
            if(tris.empty())
            {
                return;
            }
            p.cleanup();

            for(uint i = 0; i < verts.size(); i++)
            {
                verts[i].norm.flip();
            }
            if(!p.vbo)
            {
                glGenBuffers(1, &p.vbo);
            }
            gle::bindvbo(p.vbo);
            glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(vertex), verts.data(), GL_STATIC_DRAW);
            gle::clearvbo();
            p.numverts = verts.size();

            if(!p.ebo)
            {
                glGenBuffers(1, &p.ebo);
            }
            gle::bindebo(p.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, tris.size()*sizeof(ushort), tris.data(), GL_STATIC_DRAW);
            gle::clearebo();
            p.numtris = tris.size()/3;
        }
    private:
        int addvert(const vertex &v)
        {
            uint h = hthash(v.pos)&(prefabmeshsize-1);
            for(int i = table[h]; i>=0; i = chain[i])
            {
                const vertex &c = verts[i];
                if(c.pos==v.pos && c.norm==v.norm)
                {
                    return i;
                }
            }
            if(verts.size() >= USHRT_MAX)
            {
                return -1;
            }
            verts.emplace_back(v);
            chain.emplace_back(table[h]);
            return table[h] = verts.size()-1;
        }

};

static void genprefabmesh(prefabmesh &r, cube &c, const ivec &co, int size)
{
    //recursively apply to children
    if(c.children)
    {
        neighborstack[++neighbordepth] = c.children;
        for(int i = 0; i < 8; ++i)
        {
            ivec o(i, co, size/2);
            genprefabmesh(r, c.children[i], o, size/2);
        }
        --neighbordepth;
    }
    else if(!(c.isempty()))
    {
        int vis;
        for(int i = 0; i < 6; ++i) //for each face
        {
            if((vis = visibletris(c, i, co, size)))
            {
                ivec v[4];
                genfaceverts(c, i, v);
                int convex = 0;
                if(!flataxisface(c, i))
                {
                    convex = faceconvexity(v);
                }
                int order = vis&4 || convex < 0 ? 1 : 0, numverts = 0;
                vec vo(co), pos[4], norm[4];
                pos[numverts++] = vec(v[order]).mul(size/8.0f).add(vo);
                if(vis&1)
                {
                    pos[numverts++] = vec(v[order+1]).mul(size/8.0f).add(vo);
                }
                pos[numverts++] = vec(v[order+2]).mul(size/8.0f).add(vo);
                if(vis&2)
                {
                    pos[numverts++] = vec(v[(order+3)&3]).mul(size/8.0f).add(vo);
                }
                guessnormals(pos, numverts, norm);
                int index[4];
                for(int j = 0; j < numverts; ++j)
                {
                    index[j] = r.addvert(pos[j], bvec(norm[j]));
                }
                for(int j = 0; j < numverts-2; ++j)
                {
                    if(index[0]!=index[j+1] && index[j+1]!=index[j+2] && index[j+2]!=index[0])
                    {
                        r.tris.emplace_back(index[0]);
                        r.tris.emplace_back(index[j+1]);
                        r.tris.emplace_back(index[j+2]);
                    }
                }
            }
        }
    }
}

void cubeworld::genprefabmesh(prefab &p)
{
    block3 b = *p.copy;
    b.o = ivec(0, 0, 0);

    cube *oldworldroot = worldroot;
    int oldworldscale = worldscale,
        oldworldsize = worldsize;

    worldroot = newcubes();
    worldscale = 1;
    worldsize = 2;
    while(worldsize < std::max(std::max(b.s.x, b.s.y), b.s.z)*b.grid)
    {
        worldscale++;
        worldsize *= 2;
    }

    cube *s = p.copy->c();
    LOOP_XYZ(b, b.grid, if(!(s->isempty()) || s->children) pastecube(*s, c); s++);

    prefabmesh r;
    neighborstack[++neighbordepth] = worldroot;
    //recursively apply to children
    for(int i = 0; i < 8; ++i)
    {
        ::genprefabmesh(r, worldroot[i], ivec(i, ivec(0, 0, 0), worldsize/2), worldsize/2);
    }
    --neighbordepth;
    r.setup(p);

    freeocta(worldroot);

    worldroot = oldworldroot;
    worldscale = oldworldscale;
    worldsize = oldworldsize;

    useshaderbyname("prefab");
}

static void renderprefab(prefab &p, const vec &o, float yaw, float pitch, float roll, float size, const vec &color)
{
    if(!p.numtris)
    {
        rootworld.genprefabmesh(p);
        if(!p.numtris)
        {
            return;
        }
    }

    block3 &b = *p.copy;

    matrix4 m;
    m.identity();
    m.settranslation(o);
    if(yaw)
    {
        m.rotate_around_z(yaw*RAD);
    }
    if(pitch)
    {
        m.rotate_around_x(pitch*RAD);
    }
    if(roll)
    {
        m.rotate_around_y(-roll*RAD);
    }
    matrix3 w(m);
    if(size > 0 && size != 1)
    {
        m.scale(size);
    }
    m.translate(vec(b.s).mul(-b.grid*0.5f));

    gle::bindvbo(p.vbo);
    gle::bindebo(p.ebo);
    gle::enablevertex();
    gle::enablenormal();
    prefabmesh::vertex *v = (prefabmesh::vertex *)0;
    gle::vertexpointer(sizeof(prefabmesh::vertex), v->pos.v);
    gle::normalpointer(sizeof(prefabmesh::vertex), v->norm.v, GL_BYTE);

    matrix4 pm;
    pm.mul(camprojmatrix, m);
    GLOBALPARAM(prefabmatrix, pm);
    GLOBALPARAM(prefabworld, w);
    SETSHADER(prefab);
    gle::color(vec(color).mul(ldrscale));
    glDrawRangeElements_(GL_TRIANGLES, 0, p.numverts-1, p.numtris*3, GL_UNSIGNED_SHORT, (ushort *)0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    pm.mul(camprojmatrix, m);
    GLOBALPARAM(prefabmatrix, pm);
    SETSHADER(prefab);
    gle::color((outlinecolor).tocolor().mul(ldrscale));
    glDrawRangeElements_(GL_TRIANGLES, 0, p.numverts-1, p.numtris*3, GL_UNSIGNED_SHORT, (ushort *)0);

    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    gle::disablevertex();
    gle::disablenormal();
    gle::clearebo();
    gle::clearvbo();
}

void renderprefab(const char *name, const vec &o, float yaw, float pitch, float roll, float size, const vec &color)
{
    prefab *p = loadprefab(name, false);
    if(p)
    {
        renderprefab(*p, o, yaw, pitch, roll, size, color);
    }
}

void previewprefab(const char *name, const vec &color)
{
    prefab *p = loadprefab(name, false);
    if(p)
    {
        block3 &b = *p->copy;
        float yaw;
        vec o = calcmodelpreviewpos(vec(b.s).mul(b.grid*0.5f), yaw);
        renderprefab(*p, o, yaw, 0, 0, 1, color);
    }
}

std::vector<int *> editingvslots;

void compacteditvslots()
{
    for(uint i = 0; i < editingvslots.size(); i++)
    {
        if(*editingvslots[i])
        {
            compactvslot(*editingvslots[i]);
        }
    }
    for(uint i = 0; i < unpackingvslots.size(); i++)
    {
        compactvslot(*unpackingvslots[i].vslot);
    }
    for(uint i = 0; i < editinfos.size(); i++)
    {
        editinfo *e = editinfos[i];
        compactvslots(e->copy->c(), e->copy->size());
    }
    for(undoblock *u = undos.first; u; u = u->next)
    {
        if(!u->numents)
        {
            compactvslots(u->block()->c(), u->block()->size());
        }
    }
    for(undoblock *u = redos.first; u; u = u->next)
    {
        if(!u->numents)
        {
            compactvslots(u->block()->c(), u->block()->size());
        }
    }
}

///////////// height maps ////////////////

ushort getmaterial(cube &c)
{
    if(c.children)
    {
        ushort mat = getmaterial(c.children[7]);
        for(int i = 0; i < 7; ++i)
        {
            if(mat != getmaterial(c.children[i]))
            {
                return Mat_Air;
            }
        }
        return mat;
    }
    return c.material;
}

/////////// texture editing //////////////////

int curtexindex = -1,
    lasttex = 0,
    lasttexmillis = -1;
int texpaneltimer = 0;
std::vector<ushort> texmru;

selinfo repsel;
int reptex = -1;

std::vector<vslotmap> remappedvslots;

static VSlot *remapvslot(int index, bool delta, const VSlot &ds)
{
    for(uint i = 0; i < remappedvslots.size(); i++)
    {
        if(remappedvslots[i].index == index)
        {
            return remappedvslots[i].vslot;
        }
    }
    VSlot &vs = lookupvslot(index, false);
    if(vs.index < 0 || vs.index == Default_Sky)
    {
        return nullptr;
    }
    VSlot *edit = nullptr;
    if(delta)
    {
        VSlot ms;
        mergevslot(ms, vs, ds);
        edit = ms.changed ? editvslot(vs, ms) : vs.slot->variants;
    }
    else
    {
        edit = ds.changed ? editvslot(vs, ds) : vs.slot->variants;
    }
    if(!edit)
    {
        edit = &vs;
    }
    remappedvslots.emplace_back(vslotmap(vs.index, edit));
    return edit;
}

void remapvslots(cube &c, bool delta, const VSlot &ds, int orient, bool &findrep, VSlot *&findedit)
{
    //recursively apply to children
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            remapvslots(c.children[i], delta, ds, orient, findrep, findedit);
        }
        return;
    }
    static VSlot ms;
    if(orient<0)
    {
        for(int i = 0; i < 6; ++i) //for each face
        {
            VSlot *edit = remapvslot(c.texture[i], delta, ds);
            if(edit)
            {
                c.texture[i] = edit->index;
                if(!findedit)
                {
                    findedit = edit;
                }
            }
        }
    }
    else
    {
        int i = visibleorient(c, orient);
        VSlot *edit = remapvslot(c.texture[i], delta, ds);
        if(edit)
        {
            if(findrep)
            {
                if(reptex < 0)
                {
                    reptex = c.texture[i];
                }
                else if(reptex != c.texture[i])
                {
                    findrep = false;
                }
            }
            c.texture[i] = edit->index;
            if(!findedit)
            {
                findedit = edit;
            }
        }
    }
}

void compactmruvslots()
{
    remappedvslots.clear();
    for(uint i = texmru.size(); --i >=0;) //note reverse iteration
    {
        if(vslots.inrange(texmru[i]))
        {
            VSlot &vs = *vslots[texmru[i]];
            if(vs.index >= 0)
            {
                texmru[i] = vs.index;
                continue;
            }
        }
        if(static_cast<uint>(curtexindex) > i)
        {
            curtexindex--;
        }
        else if(static_cast<uint>(curtexindex) == i)
        {
            curtexindex = -1;
        }
        texmru.erase(texmru.begin() + i);
    }
    if(vslots.inrange(lasttex))
    {
        VSlot &vs = *vslots[lasttex];
        lasttex = vs.index >= 0 ? vs.index : 0;
    }
    else
    {
        lasttex = 0;
    }
    reptex = vslots.inrange(reptex) ? vslots[reptex]->index : -1;
}

void edittexcube(cube &c, int tex, int orient, bool &findrep)
{
    if(orient<0)
    {
        for(int i = 0; i < 6; ++i) //for each face
        {
            c.texture[i] = tex;
        }
    }
    else
    {
        int i = visibleorient(c, orient);
        if(findrep)
        {
            if(reptex < 0)
            {
                reptex = c.texture[i];
            }
            else if(reptex != c.texture[i])
            {
                findrep = false;
            }
        }
        c.texture[i] = tex;
    }
    //recursively apply to children
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            edittexcube(c.children[i], tex, orient, findrep);
        }
    }
}

/*setmat: sets a cube's materials, given a material & filter to use
 * Arguments:
 *  c: the cube object to use
 *  mat: material index to apply
 *  matmask: material mask
 *  filtermat: if nonzero, determines what existing mats to apply to
 *  filtermask: filter material mask
 *  filtergeom: type of geometry inside the cube (empty, solid, partially solid)
 */
void cube::setmat(ushort mat, ushort matmask, ushort filtermat, ushort filtermask, int filtergeom)
{
    //recursively sets material for all child nodes
    if(children)
    {
        for(int i = 0; i < 8; ++i)
        {
            children[i].setmat( mat, matmask, filtermat, filtermask, filtergeom);
        }
    }
    else if((material&filtermask) == filtermat)
    {
        switch(filtergeom)
        {
            case EditMatFlag_Empty:
            {
                if(isempty())
                {
                    break;
                }
                return;
            }
            case EditMatFlag_NotEmpty:
            {
                if(!(isempty()))
                {
                    break;
                }
                return;
            }
            case EditMatFlag_Solid:
            {
                if(issolid())
                {
                    break;
                }
                return;
            }
            case EditMatFlag_NotSolid:
            {
                if(!(issolid()))
                {
                    break;
                }
                return;
            }
        }
        if(mat!=Mat_Air)
        {
            material &= matmask;
            material |= mat;
        }
        else
        {
            material = Mat_Air;
        }
    }
}

void rendertexturepanel(int w, int h)
{
    if((texpaneltimer -= curtime)>0 && editmode)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        pushhudmatrix();
        hudmatrix.scale(h/1800.0f, h/1800.0f, 1);
        flushhudmatrix(false);
        SETSHADER(hudrgb);
        int y = 50,
        gap = 10;
        gle::defvertex(2);
        gle::deftexcoord0();

        for(int i = 0; i < 7; ++i)
        {
            int s = (i == 3 ? 285 : 220),
                ti = curtexindex+i-3;
            if(static_cast<int>(texmru.size()) > ti)
            {
                VSlot &vslot = lookupvslot(texmru[ti]);
                Slot &slot = *vslot.slot;
                Texture *tex = slot.sts.empty() ? notexture : slot.sts[0].t,
                        *glowtex = nullptr;
                if(slot.texmask&(1 << Tex_Glow))
                {
                    for(int j = 0; j < slot.sts.length(); j++)
                    {
                        if(slot.sts[j].type == Tex_Glow)
                        {
                            glowtex = slot.sts[j].t;
                            break;
                        }
                    }
                }
                float sx = std::min(1.0f, tex->xs/static_cast<float>(tex->ys)),
                      sy = std::min(1.0f, tex->ys/static_cast<float>(tex->xs));
                int x = w*1800/h-s-50,
                    r = s;
                vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
                float xoff = vslot.offset.x,
                      yoff = vslot.offset.y;
                if(vslot.rotation)
                {
                    const texrotation &r = texrotations[vslot.rotation];
                    if(r.swapxy)
                    {
                        std::swap(xoff, yoff);
                        for(int k = 0; k < 4; ++k)
                        {
                            std::swap(tc[k].x, tc[k].y);
                        }
                    }
                    if(r.flipx)
                    {
                        xoff *= -1;
                        for(int k = 0; k < 4; ++k)
                        {
                            tc[k].x *= -1;
                        }
                    }
                    if(r.flipy)
                    {
                        yoff *= -1;
                        for(int k = 0; k < 4; ++k)
                        {
                            tc[k].y *= -1;
                        }
                    }
                }
                for(int k = 0; k < 4; ++k)
                {
                    tc[k].x = tc[k].x/sx - xoff/tex->xs;
                    tc[k].y = tc[k].y/sy - yoff/tex->ys;
                }
                glBindTexture(GL_TEXTURE_2D, tex->id);
                for(int j = 0; j < (glowtex ? 3 : 2); ++j)
                {
                    if(j < 2)
                    {
                        gle::color(vec(vslot.colorscale).mul(j), texpaneltimer/1000.0f);
                    }
                    else
                    {
                        glBindTexture(GL_TEXTURE_2D, glowtex->id);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                        gle::color(vslot.glowcolor, texpaneltimer/1000.0f);
                    }
                    gle::begin(GL_TRIANGLE_STRIP);
                    gle::attribf(x,   y);   gle::attrib(tc[0]);
                    gle::attribf(x+r, y);   gle::attrib(tc[1]);
                    gle::attribf(x,   y+r); gle::attrib(tc[3]);
                    gle::attribf(x+r, y+r); gle::attrib(tc[2]);
                    xtraverts += gle::end();
                    if(!j)
                    {
                        r -= 10;
                        x += 5;
                        y += 5;
                    }
                    else if(j == 2)
                    {
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    }
                }
            }
            y += s+gap;
        }

        pophudmatrix(true, false);
        resethudshader();
    }
}
//defines editing readonly variables, useful for the HUD
#define EDITSTAT(name, type, val) \
    void editstat##name() \
    { \
        static int laststat = 0; \
        static type prevstat = 0; \
        static type curstat = 0; \
        if(totalmillis - laststat >= statrate) \
        { \
            prevstat = curstat; \
            laststat = totalmillis - (totalmillis%statrate); \
        } \
        if(prevstat == curstat) curstat = (val); \
        type##ret(curstat); \
    } \
    COMMAND(editstat##name, "");

EDITSTAT(wtr, int, wtris);
EDITSTAT(vtr, int, (vtris*100)/std::max(wtris, 1));
EDITSTAT(wvt, int, wverts);
EDITSTAT(vvt, int, (vverts*100)/std::max(wverts, 1));
EDITSTAT(evt, int, xtraverts);
EDITSTAT(eva, int, xtravertsva);
EDITSTAT(octa, int, allocnodes*8);
EDITSTAT(va, int, allocva);
EDITSTAT(glde, int, glde);
EDITSTAT(geombatch, int, gbatches);
EDITSTAT(oq, int, getnumqueries());

#undef EDITSTAT

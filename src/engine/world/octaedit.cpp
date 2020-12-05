#include "engine.h"

#include "light.h"
#include "octaedit.h"
#include "physics.h"
#include "raycube.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/input.h"
#include "interface/ui.h"

#include "render/octarender.h"
#include "render/rendergl.h"

#include "world/material.h"

bool boxoutline = false;

void boxs(int orient, vec o, const vec &s, float size)
{
    int d  = DIMENSION(orient),
        dc = DIM_COORD(orient);
    float f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;
    o[D[d]] += dc * s[D[d]] + f;

    vec r(0, 0, 0), c(0, 0, 0);
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

void boxs(int orient, vec o, const vec &s)
{
    int d  = DIMENSION(orient),
        dc = DIM_COORD(orient);
    float f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;
    o[D[d]] += dc * s[D[d]] + f;

    gle::defvertex();
    gle::begin(GL_LINE_LOOP);
    //draw four surfaces
    gle::attrib(o); o[R[d]] += s[R[d]];
    gle::attrib(o); o[C[d]] += s[C[d]];
    gle::attrib(o); o[R[d]] -= s[R[d]];
    gle::attrib(o);

    xtraverts += gle::end();
}

void boxs3D(const vec &o, vec s, int g)
{
    s.mul(g); //multiply displacement by g(ridpower)
    for(int i = 0; i < 6; ++i) //for each face
    {
        boxs(i, o, s);
    }
}

void boxsgrid(int orient, vec o, vec s, int g)
{
    int d  = DIMENSION(orient),
        dc = DIM_COORD(orient);
    float ox = o[R[d]],
          oy = o[C[d]],
          xs = s[R[d]],
          ys = s[C[d]],
          f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;

    o[D[d]] += dc * s[D[d]]*g + f;

    gle::defvertex();
    gle::begin(GL_LINES);
    for(int x = 0; x < xs; ++x)
    {
        o[R[d]] += g;
        gle::attrib(o);
        o[C[d]] += ys*g;
        gle::attrib(o);
        o[C[d]] = oy;
    }
    for(int y = 0; y < ys; ++y)
    {
        o[C[d]] += g;
        o[R[d]] = ox;
        gle::attrib(o);
        o[R[d]] += xs*g;
        gle::attrib(o);
    }
    xtraverts += gle::end();
}

selinfo sel, lastsel, savedsel;

int orient = 0;
int gridsize = 8;
ivec cor, lastcor;
ivec cur, lastcur;

extern int entediting;
bool editmode = false;
bool multiplayer = false;
bool allowediting = false;
bool havesel = false;
bool hmapsel = false;
int horient  = 0;

void multiplayerwarn()
{
    conoutf(Console_Error, "operation not available in multiplayer");
}

extern int entmoving;

bool pointinsel(const selinfo &sel, const vec &o)
{
    return(o.x <= sel.o.x+sel.s.x*sel.grid
        && o.x >= sel.o.x
        && o.y <= sel.o.y+sel.s.y*sel.grid
        && o.y >= sel.o.y
        && o.z <= sel.o.z+sel.s.z*sel.grid
        && o.z >= sel.o.z);
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
ICOMMAND(moving, "b", (int *n),
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
});

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

bool noedit(bool view, bool msg)
{
    if(!editmode)
    {
        if(msg)
        {
            conoutf(Console_Error, "operation only allowed in edit mode");
        }
        return true;
    }
    if(view || haveselent())
    {
        return false;
    }
    vec o(sel.o), s(sel.s);
    s.mul(sel.grid / 2.0f);
    o.add(s);
    float r = max(s.x, s.y, s.z);
    bool viewable = (isvisiblesphere(r, o) != ViewFrustumCull_NotVisible);
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

ICOMMAND(selmoved, "", (), { if(noedit(true)) return; intret(sel.o != savedsel.o ? 1 : 0); });
ICOMMAND(selsave, "", (), { if(noedit(true)) return; savedsel = sel; });
ICOMMAND(selrestore, "", (), { if(noedit(true)) return; sel = savedsel; });
ICOMMAND(selswap, "", (), { if(noedit(true)) return; swap(sel, savedsel); });

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
    return lookupcube(s, rgrid);
}

////////////// cursor ///////////////

int selchildcount = 0,
    selchildmat = -1;

ICOMMAND(havesel, "", (), intret(havesel ? selchildcount : 0));
ICOMMAND(selchildcount, "", (),
{
    if(selchildcount < 0)
    {
        result(tempformatstring("1/%d", -selchildcount));
    }
    else
    {
        intret(selchildcount);
    }
});
ICOMMAND(selchildmat, "s", (char *prefix),
{
    if(selchildmat > 0)
    {
        result(getmaterialdesc(selchildmat, prefix));
    }
});

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
    sel.o.x = min(lastcur.x, cur.x);
    sel.o.y = min(lastcur.y, cur.y);
    sel.o.z = min(lastcur.z, cur.z);
    sel.s.x = abs(lastcur.x-cur.x)/sel.grid+1;
    sel.s.y = abs(lastcur.y-cur.y)/sel.grid+1;
    sel.s.z = abs(lastcur.z-cur.z)/sel.grid+1;
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
    inline bool isheightmap(int orient, int d, bool empty, cube *c);
}
extern void entdrag(const vec &ray);
extern bool hoveringonent(int ent, int orient);
extern void renderentselection(const vec &o, const vec &ray, bool entmoving);

VAR(gridlookup, 0, 0, 1);
VAR(passthroughcube, 0, 1, 1);

void rendereditcursor()
{
    int d   = DIMENSION(sel.orient),
        od  = DIMENSION(orient),
        odc = DIM_COORD(orient);

    bool hidecursor = UI::hascursor(),
         hovering   = false;
    hmapsel = false;

    if(moving)
    {
        static vec dest, handle;
        if(editmoveplane(vec(sel.o), camdir, od, sel.o[D[od]]+odc*sel.grid*sel.s[D[od]], handle, dest, moving==1))
        {
            if(moving==1)
            {
                dest.add(handle);
                handle = vec(ivec(handle).mask(~(sel.grid-1)));
                dest.sub(handle);
                moving = 2;
            }
            ivec o = ivec(dest).mask(~(sel.grid-1));
            sel.o[R[od]] = o[R[od]];
            sel.o[C[od]] = o[C[od]];
        }
    }
    else if(entmoving)
    {
        entdrag(camdir);
    }
    else
    {
        ivec w;
        float sdist = 0,
              wdist = 0,
              t;
        int entorient = 0,
            ent = -1;

        wdist = rayent(player->o, camdir, 1e16f,
                       (editmode && showmat ? Ray_EditMat : 0)   // select cubes first
                       | (!dragging && entediting ? Ray_Ents : 0)
                       | Ray_SkipFirst
                       | (passthroughcube==1 ? Ray_Pass : 0), gridsize, entorient, ent);

        if((havesel || dragging) && !passthroughsel && !hmapedit)     // now try selecting the selection
            if(rayboxintersect(vec(sel.o), vec(sel.s).mul(sel.grid), player->o, camdir, sdist, orient))
            {   // and choose the nearest of the two
                if(sdist < wdist)
                {
                    wdist = sdist;
                    ent   = -1;
                }
            }

        if((hovering = hoveringonent(hidecursor ? -1 : ent, entorient)))
        {
           if(!havesel)
           {
               selchildcount = 0;
               selchildmat = -1;
               sel.s = ivec(0, 0, 0);
           }
        }
        else
        {
            vec w = vec(camdir).mul(wdist+0.05f).add(player->o);
            if(!insideworld(w))
            {
                for(int i = 0; i < 3; ++i)
                {
                    wdist = min(wdist, ((camdir[i] > 0 ? worldsize : 0) - player->o[i]) / camdir[i]);
                }
                w = vec(camdir).mul(wdist-0.05f).add(player->o);
                if(!insideworld(w))
                {
                    wdist = 0;
                    for(int i = 0; i < 3; ++i)
                    {
                        w[i] = std::clamp(player->o[i], 0.0f, float(worldsize));
                    }
                }
            }
            cube *c = &lookupcube(ivec(w));
            if(gridlookup && !dragging && !moving && !havesel && hmapedit!=1)
            {
                gridsize = lusize;
            }
            int mag = lusize / gridsize;
            normalizelookupcube(ivec(w));
            if(sdist == 0 || sdist > wdist)
            {
                rayboxintersect(vec(lu), vec(gridsize), player->o, camdir, t=0, orient); // just getting orient
            }
            cur = lu;
            cor = ivec(vec(w).mul(2).div(gridsize));
            od = DIMENSION(orient);
            d = DIMENSION(sel.orient);

            if(hmapedit==1 && DIM_COORD(horient) == (camdir[DIMENSION(horient)]<0))
            {
                hmapsel = hmap::isheightmap(horient, DIMENSION(horient), false, c);
                if(hmapsel)
                {
                    od = DIMENSION(orient = horient);
                }
            }
            if(dragging)
            {
                updateselection();
                sel.cx   = min(cor[R[d]], lastcor[R[d]]);
                sel.cy   = min(cor[C[d]], lastcor[C[d]]);
                sel.cxs  = max(cor[R[d]], lastcor[R[d]]);
                sel.cys  = max(cor[C[d]], lastcor[C[d]]);
                if(!selectcorners)
                {
                    sel.cx &= ~1;
                    sel.cy &= ~1;
                    sel.cxs &= ~1;
                    sel.cys &= ~1;
                    sel.cxs -= sel.cx-2;
                    sel.cys -= sel.cy-2;
                }
                else
                {
                    sel.cxs -= sel.cx-1;
                    sel.cys -= sel.cy-1;
                }
                sel.cx  &= 1;
                sel.cy  &= 1;
                havesel = true;
            }
            else if(!havesel)
            {
                sel.o = lu;
                sel.s.x = sel.s.y = sel.s.z = 1;
                sel.cx = sel.cy = 0;
                sel.cxs = sel.cys = 2;
                sel.grid = gridsize;
                sel.orient = orient;
                d = od;
            }
            sel.corner = (cor[R[d]]-(lu[R[d]]*2)/gridsize)+(cor[C[d]]-(lu[C[d]]*2)/gridsize)*2;
            selchildcount = 0;
            selchildmat = -1;
            countselchild(worldroot, ivec(0, 0, 0), worldsize/2);
            if(mag>=1 && selchildcount==1)
            {
                selchildmat = c->material;
                if(mag>1)
                {
                    selchildcount = -mag;
                }
            }
        }
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    // cursors

    ldrnotextureshader->set();

    renderentselection(player->o, camdir, entmoving!=0);

    boxoutline = outline!=0;

    enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    if(!moving && !hovering && !hidecursor)
    {
        if(hmapedit==1)
        {
            gle::colorub(0, hmapsel ? 255 : 40, 0);
        }
        else
        {
            gle::colorub(120,120,120);
        }
        boxs(orient, vec(lu), vec(lusize));
    }

    // selections
    if(havesel || moving)
    {
        d = DIMENSION(sel.orient);
        gle::colorub(50,50,50);   // grid
        boxsgrid(sel.orient, vec(sel.o), vec(sel.s), sel.grid);
        gle::colorub(200,0,0);    // 0 reference
        boxs3D(vec(sel.o).sub(0.5f*min(gridsize*0.25f, 2.0f)), vec(min(gridsize*0.25f, 2.0f)), 1);
        gle::colorub(200,200,200);// 2D selection box
        vec co(sel.o.v), cs(sel.s.v);
        co[R[d]] += 0.5f*(sel.cx*gridsize);
        co[C[d]] += 0.5f*(sel.cy*gridsize);
        cs[R[d]]  = 0.5f*(sel.cxs*gridsize);
        cs[C[d]]  = 0.5f*(sel.cys*gridsize);
        cs[D[d]] *= gridsize;
        boxs(sel.orient, co, cs);
        if(hmapedit==1)         // 3D selection box
        {
            gle::colorub(0,120,0);
        }
        else
        {
            gle::colorub(0,0,120);
        }
        boxs3D(vec(sel.o), vec(sel.s), sel.grid);
    }

    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    boxoutline = false;

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

//////////// ready changes to vertex arrays ////////////

static bool haschanged = false;

void readychanges(const ivec &bbmin, const ivec &bbmax, cube *c, const ivec &cor, int size)
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
                c[i].ext->va = NULL;
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
                discardchildren(c[i], true);
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
    octarender();
    inbetweenframes = true;
    setupmaterials(oldlen);
    clearshadowcache();
    updatevabbs();
}

void changed(const ivec &bbmin, const ivec &bbmax, bool commit)
{
    readychanges(bbmin, bbmax, worldroot, ivec(0, 0, 0), worldsize/2);
    haschanged = true;

    if(commit)
    {
        commitchanges();
    }
}

void changed(const block3 &sel, bool commit = true)
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
static inline void copycube(const cube &src, cube &dst)
{
    dst = src;
    dst.visible = 0;
    dst.merged = 0;
    dst.ext = NULL; // src cube is responsible for va destruction
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
    discardchildren(dst);
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
        return NULL;
    }
    block3 *b = reinterpret_cast<block3 *>(new uchar[bsize]);
    if(b)
    {
        blockcopy(s, rgrid, b);
    }
    return b;
}

void freeblock(block3 *b, bool alloced = true)
{
    cube *q = b->c();
    for(int i = 0; i < static_cast<int>(b->size()); ++i)
    {
        discardchildren(*q++);
    }
    if(alloced)
    {
        delete[] b;
    }
}

void selgridmap(const selinfo &sel, uchar *g)                           // generates a map of the cube sizes at each grid point
{
    LOOP_XYZ(sel, -sel.grid, (*g++ = BITSCAN(lusize), (void)c));
}

void freeundo(undoblock *u)
{
    if(!u->numents)
    {
        freeblock(u->block(), false);
    }
    delete[] reinterpret_cast<uchar *>(u);
}

static inline int undosize(undoblock *u)
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
        return NULL;
    }
    undoblock *u = reinterpret_cast<undoblock *>(new uchar[sizeof(undoblock) + blocksize + selgridsize]);
    if(!u)
    {
        return NULL;
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

static inline int countblock(cube *c, int n = 8)
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
editinfo *localedit = NULL;

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
        buf.put((uchar *)data.texture, sizeof(data.texture));
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
        b = NULL;
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
        vslothdr &hdr = *(vslothdr *)buf.pad(sizeof(vslothdr));
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
    if(!outbuf || compress2((Bytef *)outbuf, &len, (const Bytef *)inbuf, inlen, Z_BEST_COMPRESSION) != Z_OK || len > (1<<16))
    {
        delete[] outbuf;
        outbuf = NULL;
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
    if(!outbuf || uncompress((Bytef *)outbuf, &len, (const Bytef *)inbuf, inlen) != Z_OK)
    {
        delete[] outbuf;
        outbuf = NULL;
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
        e->copy = NULL;
    }
    uchar *outbuf = NULL;
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
    e = NULL;
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
            entity &e = *(entity *)buf.pad(sizeof(entity));
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

bool packundo(int op, int &inlen, uchar *&outbuf, int &outlen)
{
    switch(op)
    {
        case Edit_Undo:
        {
            return !undos.empty() && packundo(undos.last, inlen, outbuf, outlen);
        }
        case Edit_Redo:
        {
            return !redos.empty() && packundo(redos.last, inlen, outbuf, outlen);
        }
        default:
        {
            return false;
        }
    }
}

struct prefab : editinfo
{
    char *name;
    GLuint ebo, vbo;
    int numtris, numverts;

    prefab() : name(NULL), ebo(0), vbo(0), numtris(0), numverts(0) {}
    ~prefab()
    {
        DELETEA(name);
        if(copy)
        {
            freeblock(copy);
        }
    }

    void cleanup()
    {
        if(ebo)
        {
            glDeleteBuffers_(1, &ebo);
            ebo = 0;
        }
        if(vbo)
        {
            glDeleteBuffers_(1, &vbo);
            vbo = 0;
        }
        numtris = numverts = 0;
    }
};

hashnameset<prefab> prefabs;

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
    LOOP_XYZ(*b, 1<<min(static_cast<int>(*g++), worldscale-1), pastecube(*s++, c));
}

//used in client prefab unpacking, handles the octree unpacking (not the entities,
// which are game-dependent)
void unpackundocube(ucharbuf buf, uchar *outbuf)
{
    block3 *b = NULL;
    if(!unpackblock(b, buf) || b->grid >= worldsize || buf.remaining() < b->size())
    {
        freeblock(b);
        delete[] outbuf;
        return;
    }
    uchar *g = buf.pad(b->size());
    unpackvslots(*b, buf);
    pasteundoblock(b, g);
    changed(*b, false);
    freeblock(b);
}

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
    changed(sel);
    DEF_FORMAT_STRING(filename, "media/prefab/%s.obr", name);
    path(filename);
    stream *f = opengzfile(filename, "wb");
    if(!f)
    {
        conoutf(Console_Error, "could not write prefab to %s", filename);
        return;
    }
    prefabheader hdr;
    memcpy(hdr.magic, "OEBR", 4);
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
    LOOP_SEL_XYZ(if(!iscubeempty(*s) || s->children || s->material != Mat_Air) pastecube(*s, c); s++); // 'transparent'. old opaque by 'delcube; paste'
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
        return NULL;
    }
    prefabheader hdr;
    if(f->read(&hdr, sizeof(hdr)) != sizeof(prefabheader) || memcmp(hdr.magic, "OEBR", 4))
    {
        delete f;
        if(msg)
        {
            conoutf(Console_Error, "prefab %s has malformatted header", filename);
            return NULL;
        }
    }
    if(hdr.version != 0)
    {
        delete f;
        if(msg)
        {
           conoutf(Console_Error, "prefab %s uses unsupported version", filename);
           return NULL;
        }
    }
    streambuf<uchar> s(f);
    block3 *copy = NULL;
    if(!unpackblock(copy, s))
    {
        delete f;
        if(msg)
        {
            conoutf(Console_Error, "could not unpack prefab %s", filename);
            return NULL;
        }
    }
    delete f;

    b = &prefabs[name];
    b->name = newstring(name);
    b->copy = copy;

    return b;
}

void pasteprefab(char *name)
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

struct prefabmesh
{
    struct vertex { vec pos; bvec4 norm; };

    static const int prefabmeshsize = 1<<9;
    int table[prefabmeshsize];
    vector<vertex> verts;
    vector<int> chain;
    vector<ushort> tris;

    prefabmesh() { memset(table, -1, sizeof(table)); }

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
        if(verts.length() >= USHRT_MAX)
        {
            return -1;
        }
        verts.add(v);
        chain.add(table[h]);
        return table[h] = verts.length()-1;
    }

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

        for(int i = 0; i < verts.length(); i++)
        {
            verts[i].norm.flip();
        }
        if(!p.vbo)
        {
            glGenBuffers_(1, &p.vbo);
        }
        gle::bindvbo(p.vbo);
        glBufferData_(GL_ARRAY_BUFFER, verts.length()*sizeof(vertex), verts.getbuf(), GL_STATIC_DRAW);
        gle::clearvbo();
        p.numverts = verts.length();

        if(!p.ebo)
        {
            glGenBuffers_(1, &p.ebo);
        }
        gle::bindebo(p.ebo);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, tris.length()*sizeof(ushort), tris.getbuf(), GL_STATIC_DRAW);
        gle::clearebo();
        p.numtris = tris.length()/3;
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
    else if(!iscubeempty(c))
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
                        r.tris.add(index[0]);
                        r.tris.add(index[j+1]);
                        r.tris.add(index[j+2]);
                    }
                }
            }
        }
    }
}

void genprefabmesh(prefab &p)
{
    block3 b = *p.copy;
    b.o = ivec(0, 0, 0);

    cube *oldworldroot = worldroot;
    int oldworldscale = worldscale,
        oldworldsize = worldsize;

    worldroot = newcubes();
    worldscale = 1;
    worldsize = 2;
    while(worldsize < max(max(b.s.x, b.s.y), b.s.z)*b.grid)
    {
        worldscale++;
        worldsize *= 2;
    }

    cube *s = p.copy->c();
    LOOP_XYZ(b, b.grid, if(!iscubeempty(*s) || s->children) pastecube(*s, c); s++);

    prefabmesh r;
    neighborstack[++neighbordepth] = worldroot;
    //recursively apply to children
    for(int i = 0; i < 8; ++i)
    {
        genprefabmesh(r, worldroot[i], ivec(i, ivec(0, 0, 0), worldsize/2), worldsize/2);
    }
    --neighbordepth;
    r.setup(p);

    freeocta(worldroot);

    worldroot = oldworldroot;
    worldscale = oldworldscale;
    worldsize = oldworldsize;

    useshaderbyname("prefab");
}

extern bvec outlinecolor;

static void renderprefab(prefab &p, const vec &o, float yaw, float pitch, float roll, float size, const vec &color)
{
    if(!p.numtris)
    {
        genprefabmesh(p);
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

namespace hmap
{
    vector<int> textures;

    void cancel() { textures.setsize(0); }

    ICOMMAND(hmapcancel, "", (), cancel());
    ICOMMAND(hmapselect, "", (),
        int t = lookupcube(cur).texture[orient];
        int i = textures.find(t);
        if(i<0)
        {
            textures.add(t);
        }
        else
        {
            textures.remove(i);
        }
    );

    inline bool isheightmap(int o, int d, bool empty, cube *c)
    {
        return havesel ||
            (empty && iscubeempty(*c)) ||
            textures.empty() ||
            textures.find(c->texture[o]) >= 0;
    }
    //max brush consts
    static const int maxbrush  = 64,
                     maxbrushc = 63,
                     maxbrush2 = 32;

    int brush[maxbrush][maxbrush];
    VARN(hbrushx, brushx, 0, maxbrush2, maxbrush);
    VARN(hbrushy, brushy, 0, maxbrush2, maxbrush);
    bool paintbrush = 0;
    int brushmaxx = 0,
        brushminx = maxbrush,
        brushmaxy = 0,
        brushminy = maxbrush;

    void clearhbrush()
    {
        memset(brush, 0, sizeof brush);
        brushmaxx = brushmaxy = 0;
        brushminx = brushminy = maxbrush;
        paintbrush = false;
    }
    COMMAND(clearhbrush, "");

    void hbrushvert(int *x, int *y, int *v)
    {
        *x += maxbrush2 - brushx + 1; // +1 for automatic padding
        *y += maxbrush2 - brushy + 1;
        if(*x<0 || *y<0 || *x>=maxbrush || *y>=maxbrush)
        {
            return;
        }
        brush[*x][*y] = std::clamp(*v, 0, 8);
        paintbrush = paintbrush || (brush[*x][*y] > 0);
        brushmaxx = min(maxbrush-1, max(brushmaxx, *x+1));
        brushmaxy = min(maxbrush-1, max(brushmaxy, *y+1));
        brushminx = max(0,          min(brushminx, *x-1));
        brushminy = max(0,          min(brushminy, *y-1));
    }
    COMMAND(hbrushvert, "iii");

    static const int painted = 1,
                     nothmap = 2,
                     mapped  = 16;
    uchar  flags[maxbrush][maxbrush];
    cube   *cmap[maxbrushc][maxbrushc][4];
    int  mapz[maxbrushc][maxbrushc],
         map [maxbrush][maxbrush];

    selinfo changes;
    bool selecting;
    int d, dc, dr, dcr, biasup, br, hws, fg;
    int gx, gy, gz, mx, my, mz, nx, ny, nz, bmx, bmy, bnx, bny;
    uint fs;
    selinfo hundo;

    cube *getcube(ivec t, int f)
    {
        t[d] += dcr*f*gridsize;
        if(t[d] > nz || t[d] < mz)
        {
            return NULL;
        }
        cube *c = &lookupcube(t, gridsize);
        if(c->children)
        {
            forcemip(*c, false);
        }
        discardchildren(*c, true);
        if(!isheightmap(sel.orient, d, true, c))
        {
            return NULL;
        }
        if     (t.x < changes.o.x) changes.o.x = t.x;
        else if(t.x > changes.s.x) changes.s.x = t.x;
        if     (t.y < changes.o.y) changes.o.y = t.y;
        else if(t.y > changes.s.y) changes.s.y = t.y;
        if     (t.z < changes.o.z) changes.o.z = t.z;
        else if(t.z > changes.s.z) changes.s.z = t.z;
        return c;
    }

    uint getface(cube *c, int d)
    {
        return  0x0f0f0f0f & ((dc ? c->faces[d] : 0x88888888 - c->faces[d]) >> fs);
    }

    void pushside(cube &c, int d, int x, int y, int z)
    {
        ivec a;
        getcubevector(c, d, x, y, z, a);
        a[R[d]] = 8 - a[R[d]];
        setcubevector(c, d, x, y, z, a);
    }

    void addpoint(int x, int y, int z, int v)
    {
        if(!(flags[x][y] & mapped))
        {
            map[x][y] = v + (z*8);
        }
        flags[x][y] |= mapped;
    }

    void select(int x, int y, int z)
    {
        if((nothmap & flags[x][y]) || (painted & flags[x][y]))
        {
            return;
        }
        ivec t(d, x+gx, y+gy, dc ? z : hws-z);
        t.shl(gridpower);

        // selections may damage; must makeundo before
        hundo.o = t;
        hundo.o[D[d]] -= dcr*gridsize*2;
        makeundo(hundo);

        cube **c = cmap[x][y];
        for(int k = 0; k < 4; ++k)
        {
            c[k] = NULL;
        }
        c[1] = getcube(t, 0);
        if(!c[1] || !iscubeempty(*c[1]))
        {   // try up
            c[2] = c[1];
            c[1] = getcube(t, 1);
            if(!c[1] || iscubeempty(*c[1]))
            {
                c[0] = c[1];
                c[1] = c[2];
                c[2] = NULL;
            }
            else
            {
                z++;
                t[d]+=fg;
            }
        }
        else // drop down
        {
            z--;
            t[d]-= fg;
            c[0] = c[1];
            c[1] = getcube(t, 0);
        }

        if(!c[1] || iscubeempty(*c[1]))
        {
            flags[x][y] |= nothmap;
            return;
        }
        flags[x][y] |= painted;
        mapz [x][y]  = z;
        if(!c[0])
        {
            c[0] = getcube(t, 1);
        }
        if(!c[2])
        {
            c[2] = getcube(t, -1);
        }
        c[3] = getcube(t, -2);
        c[2] = !c[2] || iscubeempty(*c[2]) ? NULL : c[2];
        c[3] = !c[3] || iscubeempty(*c[3]) ? NULL : c[3];

        uint face = getface(c[1], d);
        if(face == 0x08080808 && (!c[0] || !iscubeempty(*c[0])))
        {
            flags[x][y] |= nothmap;
            return;
        }
        if(c[1]->faces[R[d]] == facesolid)   // was single
        {
            face += 0x08080808;
        }
        else                               // was pair
        {
            face += c[2] ? getface(c[2], d) : 0x08080808;
        }
        face += 0x08080808;                // c[3]
        uchar *f = reinterpret_cast<uchar*>(&face);
        addpoint(x,   y,   z, f[0]);
        addpoint(x+1, y,   z, f[1]);
        addpoint(x,   y+1, z, f[2]);
        addpoint(x+1, y+1, z, f[3]);

        if(selecting) // continue to adjacent cubes
        {
            if(x>bmx)
            {
                select(x-1, y, z);
            }
            if(x<bnx)
            {
                select(x+1, y, z);
            }
            if(y>bmy)
            {
                select(x, y-1, z);
            }
            if(y<bny)
            {
                select(x, y+1, z);
            }
        }
    }

    void ripple(int x, int y, int z, bool force)
    {
        if(force)
        {
            select(x, y, z);
        }
        if((nothmap & flags[x][y]) || !(painted & flags[x][y]))
        {
            return;
        }

        bool changed = false;
        int *o[4], best, par,
            q = 0;
        for(int i = 0; i < 2; ++i)
        {
            for(int j = 0; j < 2; ++j)
            {
                o[i+j*2] = &map[x+i][y+j];
            }
        }

        #define PULL_HEIGHTMAP(I, LT, GT, M, N, A) \
        do \
        { \
            best = I; \
            for(int i = 0; i < 4; ++i) \
            { \
                if(*o[i] LT best) \
                { \
                    best = *o[q = i] - M; \
                } \
            } \
            par = (best&(~7)) + N; \
            /* dual layer for extra smoothness */ \
            if(*o[q^3] GT par && !(*o[q^1] LT par || *o[q^2] LT par)) \
            { \
                if(*o[q^3] GT par A 8 || *o[q^1] != par || *o[q^2] != par) \
                { \
                    *o[q^3] = (*o[q^3] GT par A 8 ? par A 8 : *o[q^3]); \
                    *o[q^1] = *o[q^2] = par; \
                    changed = true; \
                } \
            /* single layer */ \
            } \
            else { \
                for(int j = 0; j < 4; ++j) \
                    if(*o[j] GT par) \
                    { \
                        *o[j] = par; \
                        changed = true; \
                    } \
            } \
        } while(0)

        if(biasup)
        {
            PULL_HEIGHTMAP(0, >, <, 1, 0, -);
        }
        else
        {
            PULL_HEIGHTMAP(worldsize*8, <, >, 0, 8, +);
        }

        #undef PULL_HEIGHTMAP

        cube **c  = cmap[x][y];
        int e[2][2];
        int notempty = 0;

        for(int k = 0; k < 4; ++k)
        {
            if(c[k])
            {
                for(int i = 0; i < 2; ++i)
                {
                    for(int j = 0; j < 2; ++j)
                    {
                        {
                            e[i][j] = min(8, map[x+i][y+j] - (mapz[x][y]+3-k)*8);
                            notempty |= e[i][j] > 0;
                        }
                    }
                }
                if(notempty)
                {
                    c[k]->texture[sel.orient] = c[1]->texture[sel.orient];
                    setcubefaces(*c[k], facesolid);
                    for(int i = 0; i < 2; ++i)
                    {
                        for(int j = 0; j < 2; ++j)
                        {
                            int f = e[i][j];
                            if(f<0 || (f==0 && e[1-i][j]==0 && e[i][1-j]==0))
                            {
                                f=0;
                                pushside(*c[k], d, i, j, 0);
                                pushside(*c[k], d, i, j, 1);
                            }
                            EDGE_SET(CUBE_EDGE(*c[k], d, i, j), dc, dc ? f : 8-f);
                        }
                    }
                }
                else
                {
                    setcubefaces(*c[k], faceempty);
                }
            }
        }
        if(!changed)
        {
            return;
        }
        if(x>mx) ripple(x-1, y, mapz[x][y], true);
        if(x<nx) ripple(x+1, y, mapz[x][y], true);
        if(y>my) ripple(x, y-1, mapz[x][y], true);
        if(y<ny) ripple(x, y+1, mapz[x][y], true);

#define DIAGONAL_RIPPLE(a,b,exp) \
    if(exp) { \
            if(flags[x a][ y] & painted) \
            { \
                ripple(x a, y b, mapz[x a][y], true); \
            } \
            else if(flags[x][y b] & painted) \
            { \
                ripple(x a, y b, mapz[x][y b], true); \
            } \
        }
        DIAGONAL_RIPPLE(-1, -1, (x>mx && y>my)); // do diagonals because adjacents
        DIAGONAL_RIPPLE(-1, +1, (x>mx && y<ny)); //    won't unless changed
        DIAGONAL_RIPPLE(+1, +1, (x<nx && y<ny));
        DIAGONAL_RIPPLE(+1, -1, (x<nx && y>my));
    }

#undef DIAGONAL_RIPPLE

#define LOOP_BRUSH(i) for(int x=bmx; x<=bnx+i; x++) for(int y=bmy; y<=bny+i; y++)

    void paint()
    {
        LOOP_BRUSH(1)
            map[x][y] -= dr * brush[x][y];
    }

    void smooth()
    {
        int sum, div;
        LOOP_BRUSH(-2)
        {
            sum = 0;
            div = 9;
            for(int i = 0; i < 3; ++i)
            {
                for(int j = 0; j < 3; ++j)
                {
                    if(flags[x+i][y+j] & mapped)
                    {
                        sum += map[x+i][y+j];
                    }
                    else
                    {
                        div--;
                    }
                }
            }
            if(div)
            {
                map[x+1][y+1] = sum / div;
            }
        }
    }

    void rippleandset()
    {
        LOOP_BRUSH(0)
            ripple(x, y, gz, false);
    }

#undef LOOP_BRUSH

    void run(int dir, int mode)
    {
        d  = DIMENSION(sel.orient);
        dc = DIM_COORD(sel.orient);
        dcr= dc ? 1 : -1;
        dr = dir>0 ? 1 : -1;
        br = dir>0 ? 0x08080808 : 0;
     //   biasup = mode == dir<0;
        biasup = dir < 0;
        bool paintme = paintbrush;
        int cx = (sel.corner&1 ? 0 : -1),
            cy = (sel.corner&2 ? 0 : -1);
        hws= (worldsize>>gridpower);
        gx = (cur[R[d]] >> gridpower) + cx - maxbrush2;
        gy = (cur[C[d]] >> gridpower) + cy - maxbrush2;
        gz = (cur[D[d]] >> gridpower);
        fs = dc ? 4 : 0;
        fg = dc ? gridsize : -gridsize;
        mx = max(0, -gx); // ripple range
        my = max(0, -gy);
        nx = min(maxbrush-1, hws-gx) - 1;
        ny = min(maxbrush-1, hws-gy) - 1;
        if(havesel)
        {   // selection range
            bmx = mx = max(mx, (sel.o[R[d]]>>gridpower)-gx);
            bmy = my = max(my, (sel.o[C[d]]>>gridpower)-gy);
            bnx = nx = min(nx, (sel.s[R[d]]+(sel.o[R[d]]>>gridpower))-gx-1);
            bny = ny = min(ny, (sel.s[C[d]]+(sel.o[C[d]]>>gridpower))-gy-1);
        }
        if(havesel && mode<0) // -ve means smooth selection
        {
            paintme = false;
        }
        else
        {   // brush range
            bmx = max(mx, brushminx);
            bmy = max(my, brushminy);
            bnx = min(nx, brushmaxx-1);
            bny = min(ny, brushmaxy-1);
        }
        nz = worldsize-gridsize;
        mz = 0;
        hundo.s = ivec(d,1,1,5);
        hundo.orient = sel.orient;
        hundo.grid = gridsize;
        forcenextundo();

        changes.grid = gridsize;
        changes.s = changes.o = cur;
        memset(map, 0, sizeof map);
        memset(flags, 0, sizeof flags);

        selecting = true;
        select(std::clamp(maxbrush2-cx, bmx, bnx),
               std::clamp(maxbrush2-cy, bmy, bny),
               dc ? gz : hws - gz);
        selecting = false;
        if(paintme)
        {
            paint();
        }
        else
        {
            smooth();
        }
        rippleandset();                       // pull up points to cubify, and set
        changes.s.sub(changes.o).shr(gridpower).add(1);
        changed(changes);
    }
}

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
vector<ushort> texmru;

selinfo repsel;
int reptex = -1;

vector<vslotmap> remappedvslots;

static VSlot *remapvslot(int index, bool delta, const VSlot &ds)
{
    for(int i = 0; i < remappedvslots.length(); i++)
    {
        if(remappedvslots[i].index == index)
        {
            return remappedvslots[i].vslot;
        }
    }
    VSlot &vs = lookupvslot(index, false);
    if(vs.index < 0 || vs.index == Default_Sky)
    {
        return NULL;
    }
    VSlot *edit = NULL;
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
    remappedvslots.add(vslotmap(vs.index, edit));
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
void setmat(cube &c, ushort mat, ushort matmask, ushort filtermat, ushort filtermask, int filtergeom)
{
    //recursively sets material for all child nodes
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            setmat(c.children[i], mat, matmask, filtermat, filtermask, filtergeom);
        }
    }
    else if((c.material&filtermask) == filtermat)
    {
        switch(filtergeom)
        {
            case EditMatFlag_Empty:
            {
                if(iscubeempty(c))
                {
                    break;
                }
                return;
            }
            case EditMatFlag_NotEmpty:
            {
                if(!iscubeempty(c))
                {
                    break;
                }
                return;
            }
            case EditMatFlag_Solid:
            {
                if(iscubesolid(c))
                {
                    break;
                }
                return;
            }
            case EditMatFlag_NotSolid:
            {
                if(!iscubesolid(c))
                {
                    break;
                }
                return;
            }
        }
        if(mat!=Mat_Air)
        {
            c.material &= matmask;
            c.material |= mat;
        }
        else
        {
            c.material = Mat_Air;
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
            if(texmru.inrange(ti))
            {
                VSlot &vslot = lookupvslot(texmru[ti]);
                Slot &slot = *vslot.slot;
                Texture *tex = slot.sts.empty() ? notexture : slot.sts[0].t,
                        *glowtex = NULL;
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
                float sx = min(1.0f, tex->xs/static_cast<float>(tex->ys)),
                      sy = min(1.0f, tex->ys/static_cast<float>(tex->xs));
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
                        swap(xoff, yoff);
                        for(int k = 0; k < 4; ++k)
                        {
                            swap(tc[k].x, tc[k].y);
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
    ICOMMAND(editstat##name, "", (), \
    { \
        extern int statrate; \
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
    });
EDITSTAT(wtr, int, wtris);
EDITSTAT(vtr, int, (vtris*100)/max(wtris, 1));
EDITSTAT(wvt, int, wverts);
EDITSTAT(vvt, int, (vverts*100)/max(wverts, 1));
EDITSTAT(evt, int, xtraverts);
EDITSTAT(eva, int, xtravertsva);
EDITSTAT(octa, int, allocnodes*8);
EDITSTAT(va, int, allocva);
EDITSTAT(glde, int, glde);
EDITSTAT(geombatch, int, gbatches);
EDITSTAT(oq, int, getnumqueries());


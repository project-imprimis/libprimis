#include "engine.h"
#include "light.h"
#include "interface/input.h"
#include "raycube.h"

extern int outline;

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
bool havesel = false;
bool hmapsel = false;
int horient  = 0;

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
VAR(editing, 1, 0, 0);
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

void toggleedit(bool force)
{
    if(!force)
    {
        if(!isconnected())
        {
            return;
        }
        if(player->state!=ClientState_Alive && player->state!=ClientState_Dead && player->state!=ClientState_Editing)
        {
            return; // do not allow dead players to edit to avoid state confusion
        }
        if(!game::allowedittoggle())
        {
            return;         // not in most multiplayer modes
        }
    }
    if(!(editmode = !editmode))
    {
        player->state = player->editstate;
        player->o.z -= player->eyeheight;       // entinmap wants feet pos
        entinmap(player);                       // find spawn closest to current floating pos
    }
    else
    {
        player->editstate = player->state;
        player->state = ClientState_Editing;
    }
    cancelsel();
    keyrepeat(editmode, KeyRepeat_EditMode);
    editing = entediting = editmode;
    if(!force)
    {
        game::edittoggled(editmode);
    }
    execident("resethud");
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

ICOMMAND(edittoggle, "", (), toggleedit(false));
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
//note that these macros actually loop in the opposite order: e.g. loopxy runs a for loop of x inside y
#define LOOP_XY(b)        for(int y = 0; y < (b).s[C[DIMENSION((b).orient)]]; ++y) for(int x = 0; x < (b).s[R[DIMENSION((b).orient)]]; ++x)
#define LOOP_XYZ(b, r, f) { for(int z = 0; z < (b).s[D[DIMENSION((b).orient)]]; ++z) LOOP_XY((b)) { cube &c = blockcube(x,y,z,b,r); f; } }
#define LOOP_SEL_XYZ(f)    { if(local) makeundo(); LOOP_XYZ(sel, sel.grid, f); changed(sel); }
#define SELECT_CUBE(x, y, z) blockcube(x, y, z, sel, sel.grid)

////////////// cursor ///////////////

int selchildcount = 0, selchildmat = -1;

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
extern float rayent(const vec &o, const vec &ray, float radius, int mode, int size, int &orient, int &ent);

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
                SOLID_FACES(c[i]);
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

static inline void pastecube(const cube &src, cube &dst)
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

void pasteundoblock(block3 *b, uchar *g)
{
    cube *s = b->c();
    LOOP_XYZ(*b, 1<<min(static_cast<int>(*g++), worldscale-1), pastecube(*s++, c));
}

void pasteundo(undoblock *u)
{
    if(u->numents)
    {
        pasteundoents(u);
    }
    else
    {
        pasteundoblock(u->block(), u->gridmap());
    }
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

static int countblock(block3 *b)
{
    return countblock(b->c(), b->size());
}

void swapundo(undolist &a, undolist &b, int op)
{
    if(noedit())
    {
        return;
    }
    if(a.empty())
    {
        conoutf(Console_Warn, "nothing more to %s", op == Edit_Redo ? "redo" : "undo");
        return;
    }
    int ts = a.last->timestamp;
    if(multiplayer(false))
    {
        int n   = 0,
            ops = 0;
        for(undoblock *u = a.last; u && ts==u->timestamp; u = u->prev)
        {
            ++ops;
            n += u->numents ? u->numents : countblock(u->block());
            if(ops > 10 || n > 2500)
            {
                conoutf(Console_Warn, "undo too big for multiplayer");
                if(nompedit)
                {
                    multiplayer();
                    return;
                }
                op = -1;
                break;
            }
        }
    }
    selinfo l = sel;
    while(!a.empty() && ts==a.last->timestamp)
    {
        if(op >= 0)
        {
            game::edittrigger(sel, op);
        }
        undoblock *u = a.poplast(), *r;
        if(u->numents)
        {
            r = copyundoents(u);
        }
        else
        {
            block3 *ub = u->block();
            l.o = ub->o;
            l.s = ub->s;
            l.grid = ub->grid;
            l.orient = ub->orient;
            r = newundocube(l);
        }
        if(r)
        {
            r->size = u->size;
            r->timestamp = totalmillis;
            b.add(r);
        }
        pasteundo(u);
        if(!u->numents)
        {
            changed(*u->block(), false);
        }
        freeundo(u);
    }
    commitchanges();
    if(!hmapsel)
    {
        sel = l;
        reorient();
    }
    forcenextundo();
}

void editundo() { swapundo(undos, redos, Edit_Undo); }
void editredo() { swapundo(redos, undos, Edit_Redo); }

// guard against subdivision
#define PROTECT_SEL(f) { undoblock *_u = newundocube(sel); f; if(_u) { pasteundo(_u); freeundo(_u); } }

vector<editinfo *> editinfos;
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

struct vslotmap
{
    int index;
    VSlot *vslot;

    vslotmap() {}
    vslotmap(int index, VSlot *vslot) : index(index), vslot(vslot) {}
};
static vector<vslotmap> unpackingvslots;

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
            for(int j = 0; j < unpackingvslots.length(); j++)
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
        unpackingvslots.add(vslotmap(hdr.index, edit ? edit : &vs));
    }

    cube *c = b.c();
    for(int i = 0; i < b.size(); ++i)
    {
        unpackvslots(c[i], buf);
    }

    unpackingvslots.setsize(0);
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

static bool uncompresseditinfo(const uchar *inbuf, int inlen, uchar *&outbuf, int &outlen)
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
        e = editinfos.add(new editinfo);
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
    editinfos.removeobj(e);
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

bool unpackundo(const uchar *inbuf, int inlen, int outlen)
{
    uchar *outbuf = NULL;
    if(!uncompresseditinfo(inbuf, inlen, outbuf, outlen)) return false;
    ucharbuf buf(outbuf, outlen);
    if(buf.remaining() < 2)
    {
        delete[] outbuf;
        return false;
    }
    int numents = *reinterpret_cast<const ushort *>(buf.pad(2));
    if(numents)
    {
        if(buf.remaining() < numents*static_cast<int>(2 + sizeof(entity)))
        {
            delete[] outbuf;
            return false;
        }
        for(int i = 0; i < numents; ++i)
        {
            int idx = *reinterpret_cast<const ushort *>(buf.pad(2));
            entity &e = *reinterpret_cast<entity *>(buf.pad(sizeof(entity)));
            pasteundoent(idx, e);
        }
    }
    else
    {
        block3 *b = NULL;
        if(!unpackblock(b, buf) || b->grid >= worldsize || buf.remaining() < b->size())
        {
            freeblock(b);
            delete[] outbuf;
            return false;
        }
        uchar *g = buf.pad(b->size());
        unpackvslots(*b, buf);
        pasteundoblock(b, g);
        changed(*b, false);
        freeblock(b);
    }
    delete[] outbuf;
    commitchanges();
    return true;
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

struct prefabheader
{
    char magic[4];
    int version;
};

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

void saveprefab(char *name)
{
    if(!name[0] || noedit(true) || (nompedit && multiplayer()))
    {
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

void pasteblock(block3 &b, selinfo &sel, bool local)
{
    sel.s = b.s;
    int o = sel.orient;
    sel.orient = b.orient;
    cube *s = b.c();
    LOOP_SEL_XYZ(if(!IS_EMPTY(*s) || s->children || s->material != Mat_Air) pastecube(*s, c); s++); // 'transparent'. old opaque by 'delcube; paste'
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
    if(!name[0] || noedit() || (nompedit && multiplayer()))
    {
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

    static const int SIZE = 1<<9;
    int table[SIZE];
    vector<vertex> verts;
    vector<int> chain;
    vector<ushort> tris;

    prefabmesh() { memset(table, -1, sizeof(table)); }

    int addvert(const vertex &v)
    {
        uint h = hthash(v.pos)&(SIZE-1);
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
    else if(!IS_EMPTY(c))
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
    LOOP_XYZ(b, b.grid, if(!IS_EMPTY(*s) || s->children) pastecube(*s, c); s++);

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

void mpcopy(editinfo *&e, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Copy);
    }
    if(e==NULL)
    {
        e = editinfos.add(new editinfo);
    }
    if(e->copy)
    {
        freeblock(e->copy);
    }
    e->copy = NULL;
    PROTECT_SEL(e->copy = blockcopy(block3(sel), sel.grid));
    changed(sel);
}

void mppaste(editinfo *&e, selinfo &sel, bool local)
{
    if(e==NULL)
    {
        return;
    }
    if(local)
    {
        game::edittrigger(sel, Edit_Paste);
    }
    if(e->copy)
    {
        pasteblock(*e->copy, sel, local);
    }
}

void copy()
{
    if(noedit(true))
    {
        return;
    }
    mpcopy(localedit, sel, true);
}

void pastehilite()
{
    if(!localedit)
    {
        return;
    }
    sel.s = localedit->copy->s;
    reorient();
    havesel = true;
}

void paste()
{
    if(noedit(true))
    {
        return;
    }
    mppaste(localedit, sel, true);
}

COMMAND(copy, "");
COMMAND(pastehilite, "");
COMMAND(paste, "");
COMMANDN(undo, editundo, "");
COMMANDN(redo, editredo, "");

static vector<int *> editingvslots;
struct vslotref
{
    vslotref(int &index) { editingvslots.add(&index); }
    ~vslotref() { editingvslots.pop(); }
};

void compacteditvslots()
{
    for(int i = 0; i < editingvslots.length(); i++)
    {
        if(*editingvslots[i])
        {
            compactvslot(*editingvslots[i]);
        }
    }
    for(int i = 0; i < unpackingvslots.length(); i++)
    {
        compactvslot(*unpackingvslots[i].vslot);
    }
    for(int i = 0; i < editinfos.length(); i++)
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
            (empty && IS_EMPTY(*c)) ||
            textures.empty() ||
            textures.find(c->texture[o]) >= 0;
    }
    //max brush consts
    static const int maxbrush  = 64;
    static const int maxbrushc = 63;
    static const int maxbrush2 = 32;

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

    static const int painted = 1;
    static const int nothmap = 2;
    static const int mapped  = 16;
    uchar  flags[maxbrush][maxbrush];
    cube   *cmap[maxbrushc][maxbrushc][4];
    int    mapz[maxbrushc][maxbrushc];
    int    map [maxbrush][maxbrush];

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
        if(!c[1] || !IS_EMPTY(*c[1]))
        {   // try up
            c[2] = c[1];
            c[1] = getcube(t, 1);
            if(!c[1] || IS_EMPTY(*c[1]))
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

        if(!c[1] || IS_EMPTY(*c[1]))
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
        c[2] = !c[2] || IS_EMPTY(*c[2]) ? NULL : c[2];
        c[3] = !c[3] || IS_EMPTY(*c[3]) ? NULL : c[3];

        uint face = getface(c[1], d);
        if(face == 0x08080808 && (!c[0] || !IS_EMPTY(*c[0])))
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
            if(x>bmx) select(x-1, y, z);
            if(x<bnx) select(x+1, y, z);
            if(y>bmy) select(x, y-1, z);
            if(y<bny) select(x, y+1, z);
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
                    SOLID_FACES(*c[k]);
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
                    EMPTY_FACES(*c[k]);
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

void edithmap(int dir, int mode)
{
    if((nompedit && multiplayer()) || !hmapsel)
    {
        return;
    }
    hmap::run(dir, mode);
}

///////////// main cube edit ////////////////

int bounded(int n)
{
    return n<0 ? 0 : (n>8 ? 8 : n);
}

void pushedge(uchar &edge, int dir, int dc)
{
    int ne = bounded(EDGE_GET(edge, dc)+dir);
    EDGE_SET(edge, dc, ne);
    int oe = EDGE_GET(edge, 1-dc);
    if((dir<0 && dc && oe>ne) || (dir>0 && dc==0 && oe<ne))
    {
        EDGE_SET(edge, 1-dc, ne);
    }
}

void linkedpush(cube &c, int d, int x, int y, int dc, int dir)
{
    ivec v, p;
    getcubevector(c, d, x, y, dc, v);

    for(int i = 0; i < 2; ++i)
    {
        for(int j = 0; j < 2; ++j)
        {
            getcubevector(c, d, i, j, dc, p);
            if(v==p)
            {
                pushedge(CUBE_EDGE(c, d, i, j), dir, dc);
            }
        }
    }
}

static ushort getmaterial(cube &c)
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

VAR(invalidcubeguard, 0, 1, 1);

/*mpplacecube: places a cube at the location passed
 * Arguments:
 *  sel: selection info object containing objects to be filled
 *  tex: texture to cover the cube with
 *  local: toggles the engine sending out packets to others, or to not broadcast
 *         due to having recieved the command from another client
 * Returns:
 *  void
 */
void mpplacecube(selinfo &sel, int tex, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_AddCube);
    }
    LOOP_SEL_XYZ(
        discardchildren(c, true);
        SOLID_FACES(c);
    );
}


void mpeditface(int dir, int mode, selinfo &sel, bool local)
{
    if(mode==1 && (sel.cx || sel.cy || sel.cxs&1 || sel.cys&1))
    {
        mode = 0;
    }
    int d = DIMENSION(sel.orient);
    int dc = DIM_COORD(sel.orient);
    int seldir = dc ? -dir : dir;

    if(local)
        game::edittrigger(sel, Edit_Face, dir, mode);

    if(mode==1)
    {
        int h = sel.o[d]+dc*sel.grid;
        if(((dir>0) == dc && h<=0) || ((dir<0) == dc && h>=worldsize))
        {
            return;
        }
        if(dir<0)
        {
            sel.o[d] += sel.grid * seldir;
        }
    }

    if(dc)
    {
        sel.o[d] += sel.us(d)-sel.grid;
    }
    sel.s[d] = 1;

    LOOP_SEL_XYZ(
        if(c.children)
        {
            SOLID_FACES(c);
        }
        ushort mat = getmaterial(c);
        discardchildren(c, true);
        c.material = mat;
        if(mode==1) // fill command
        {
            if(dir<0)
            {
                SOLID_FACES(c);
                cube &o = blockcube(x, y, 1, sel, -sel.grid);
                for(int i = 0; i < 6; ++i) //for each face
                {
                    c.texture[i] = o.children ? Default_Geom : o.texture[i];
                }
            }
            else
                EMPTY_FACES(c);
        }
        else
        {
            uint bak = c.faces[d];
            uchar *p = reinterpret_cast<uchar *>(&c.faces[d]);
            if(mode==2)
            {
                linkedpush(c, d, sel.corner&1, sel.corner>>1, dc, seldir); // corner command
            }
            else
            {
                for(int mx = 0; mx < 2; ++mx) // pull/push edges command
                {
                    for(int my = 0; my < 2; ++my)
                    {
                        if(x==0 && mx==0 && sel.cx)
                        {
                            continue;
                        }
                        if(y==0 && my==0 && sel.cy)
                        {
                            continue;
                        }
                        if(x==sel.s[R[d]]-1 && mx==1 && (sel.cx+sel.cxs)&1)
                        {
                            continue;
                        }
                        if(y==sel.s[C[d]]-1 && my==1 && (sel.cy+sel.cys)&1)
                        {
                            continue;
                        }
                        if(p[mx+my*2] != ((uchar *)&bak)[mx+my*2])
                        {
                            continue;
                        }
                        linkedpush(c, d, mx, my, dc, seldir);
                    }
                }
            }
            optiface(p, c);
            if(invalidcubeguard==1 && !isvalidcube(c))
            {
                uint newbak = c.faces[d];
                uchar *m = reinterpret_cast<uchar *>(&bak);
                uchar *n = reinterpret_cast<uchar *>(&newbak);
                for(int k = 0; k < 4; ++k)
                {
                    if(n[k] != m[k]) // tries to find partial edit that is valid
                    {
                        c.faces[d] = bak;
                        c.edges[d*4+k] = n[k];
                        if(isvalidcube(c))
                        {
                            m[k] = n[k];
                        }
                    }
                }
                c.faces[d] = bak;
            }
        }
    );
    if (mode==1 && dir>0)
    {
        sel.o[d] += sel.grid * seldir;
    }
}

void editface(int *dir, int *mode)
{
    if(noedit(moving!=0))
    {
        return;
    }
    if(hmapedit!=1)
    {
        mpeditface(*dir, *mode, sel, true);
    }
    else
    {
        edithmap(*dir, *mode);
    }
}

VAR(selectionsurf, 0, 0, 1);

void pushsel(int *dir)
{
    if(noedit(moving!=0))
    {
        return;
    }
    int d = DIMENSION(orient);
    int s = DIM_COORD(orient) ? -*dir : *dir;
    sel.o[d] += s*sel.grid;
    if(selectionsurf==1)
    {
        player->o[d] += s*sel.grid;
        player->resetinterp();
    }
}

/*mpdelcube: deletes a cube by discarding cubes insel's children and emptying faces
 * Arguments:
 *  sel:   the selection which is to be deleted
 *  local: whether to send a network message or not (nonlocal cube deletions don't
 *             send messages, as this would cause an infinite loop)
 * Returns:
 *  void
 */

void mpdelcube(selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_DelCube);
    }
    LOOP_SEL_XYZ(discardchildren(c, true); EMPTY_FACES(c));
}

void delcube()
{
    if(noedit(true))
    {
        return;
    }
    mpdelcube(sel, true);
}

COMMAND(pushsel, "i");
COMMAND(editface, "ii");
COMMAND(delcube, "");

/////////// texture editing //////////////////

int curtexindex = -1,
    lasttex = 0,
    lasttexmillis = -1;
int texpaneltimer = 0;
vector<ushort> texmru;

void tofronttex() // maintain most recently used of the texture lists when applying texture
{
    int c = curtexindex;
    if(texmru.inrange(c))
    {
        texmru.insert(0, texmru.remove(c));
        curtexindex = -1;
    }
}

selinfo repsel;
int reptex = -1;

static vector<vslotmap> remappedvslots;

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

static void remapvslots(cube &c, bool delta, const VSlot &ds, int orient, bool &findrep, VSlot *&findedit)
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

void mpeditvslot(int delta, VSlot &ds, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_VSlot, delta, allfaces, 0, &ds);
        if(!(lastsel==sel))
        {
            tofronttex();
        }
        if(allfaces || !(repsel == sel))
        {
            reptex = -1;
        }
        repsel = sel;
    }
    bool findrep = local && !allfaces && reptex < 0;
    VSlot *findedit = NULL;
    LOOP_SEL_XYZ(remapvslots(c, delta != 0, ds, allfaces ? -1 : sel.orient, findrep, findedit));
    remappedvslots.setsize(0);
    if(local && findedit)
    {
        lasttex = findedit->index;
        lasttexmillis = totalmillis;
        curtexindex = texmru.find(lasttex);
        if(curtexindex < 0)
        {
            curtexindex = texmru.length();
            texmru.add(lasttex);
        }
    }
}

#define EDITING_VSLOT(a) vslotref vslotrefs[] = { a }; (void)vslotrefs;

bool mpeditvslot(int delta, int allfaces, selinfo &sel, ucharbuf &buf)
{
    VSlot ds;
    if(!unpackvslot(buf, ds, delta != 0))
    {
        return false;
    }
    mpeditvslot(delta, ds, allfaces, sel, false);
    return true;
}

VAR(allfaces, 0, 0, 1);
VAR(usevdelta, 1, 0, 0);

void vdelta(uint *body)
{
    if(noedit())
    {
        return;
    }
    usevdelta++;
    execute(body);
    usevdelta--;
}
COMMAND(vdelta, "e");

void vrotate(int *n)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Rotation;
    ds.rotation = usevdelta ? *n : std::clamp(*n, 0, 7);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vrotate, "i");

void vangle(float *a)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Angle;
    ds.angle = vec(*a, sinf(RAD**a), cosf(RAD**a));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vangle, "f");

void voffset(int *x, int *y)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Offset;
    ds.offset = usevdelta ? ivec2(*x, *y) : ivec2(*x, *y).max(0);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(voffset, "ii");

void vscroll(float *s, float *t)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Scroll;
    ds.scroll = vec2(*s/1000.0f, *t/1000.0f);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vscroll, "ff");

void vscale(float *scale)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Scale;
    ds.scale = *scale <= 0 ? 1 : (usevdelta ? *scale : std::clamp(*scale, 1/8.0f, 8.0f));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vscale, "f");

void valpha(float *front, float *back)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Alpha;
    ds.alphafront = std::clamp(*front, 0.0f, 1.0f);
    ds.alphaback = std::clamp(*back, 0.0f, 1.0f);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(valpha, "ff");

void vcolor(float *r, float *g, float *b)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Color;
    ds.colorscale = vec(std::clamp(*r, 0.0f, 2.0f), std::clamp(*g, 0.0f, 2.0f), std::clamp(*b, 0.0f, 2.0f));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vcolor, "fff");

void vrefract(float *k, float *r, float *g, float *b)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Refract;
    ds.refractscale = std::clamp(*k, 0.0f, 1.0f);
    if(ds.refractscale > 0 && (*r > 0 || *g > 0 || *b > 0))
    {
        ds.refractcolor = vec(std::clamp(*r, 0.0f, 1.0f), std::clamp(*g, 0.0f, 1.0f), std::clamp(*b, 0.0f, 1.0f));
    }
    else
    {
        ds.refractcolor = vec(1, 1, 1);
    }
    mpeditvslot(usevdelta, ds, allfaces, sel, true);

}
COMMAND(vrefract, "ffff");

void vreset()
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vreset, "");

void vshaderparam(const char *name, float *x, float *y, float *z, float *w)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_ShParam;
    if(name[0])
    {
        SlotShaderParam p = { getshaderparamname(name), -1, 0, {*x, *y, *z, *w} };
        ds.params.add(p);
    }
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vshaderparam, "sfFFf");

void mpedittex(int tex, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Tex, tex, allfaces);
        if(allfaces || !(repsel == sel))
        {
            reptex = -1;
        }
        repsel = sel;
    }
    bool findrep = local && !allfaces && reptex < 0;
    LOOP_SEL_XYZ(edittexcube(c, tex, allfaces ? -1 : sel.orient, findrep));
}

static int unpacktex(int &tex, ucharbuf &buf, bool insert = true)
{
    if(tex < 0x10000)
    {
        return true;
    }
    VSlot ds;
    if(!unpackvslot(buf, ds, false))
    {
        return false;
    }
    VSlot &vs = *lookupslot(tex & 0xFFFF, false).variants;
    if(vs.index < 0 || vs.index == Default_Sky)
    {
        return false;
    }
    VSlot *edit = insert ? editvslot(vs, ds) : findvslot(*vs.slot, vs, ds);
    if(!edit)
    {
        return false;
    }
    tex = edit->index;
    return true;
}

int shouldpacktex(int index)
{
    if(vslots.inrange(index))
    {
        VSlot &vs = *vslots[index];
        if(vs.changed)
        {
            return 0x10000 + vs.slot->index;
        }
    }
    return 0;
}

bool mpedittex(int tex, int allfaces, selinfo &sel, ucharbuf &buf)
{
    if(!unpacktex(tex, buf))
    {
        return false;
    }
    mpedittex(tex, allfaces, sel, false);
    return true;
}

static void filltexlist()
{
    if(texmru.length()!=vslots.length())
    {
        for(int i = texmru.length(); --i >=0;) //note reverse iteration
        {
            if(texmru[i]>=vslots.length())
            {
                if(curtexindex > i)
                {
                    curtexindex--;
                }
                else if(curtexindex == i)
                {
                    curtexindex = -1;
                }
                texmru.remove(i);
            }
        }
        for(int i = 0; i < vslots.length(); i++)
        {
            if(texmru.find(i)<0)
            {
                texmru.add(i);
            }
        }
    }
}

void compactmruvslots()
{
    remappedvslots.setsize(0);
    for(int i = texmru.length(); --i >=0;) //note reverse iteration
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
        if(curtexindex > i)
        {
            curtexindex--;
        }
        else if(curtexindex == i)
        {
            curtexindex = -1;
        }
        texmru.remove(i);
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

void edittex(int i, bool save = true)
{
    lasttex = i;
    lasttexmillis = totalmillis;
    if(save)
    {
        for(int j = 0; j < texmru.length(); j++)
        {
            if(texmru[j]==lasttex)
            {
                curtexindex = j;
                break;
            }
        }
    }
    mpedittex(i, allfaces, sel, true);
}

void edittex_(int *dir)
{
    if(noedit())
    {
        return;
    }
    filltexlist();
    if(texmru.empty())
    {
        return;
    }
    texpaneltimer = 5000;
    if(!(lastsel==sel))
    {
        tofronttex();
    }
    curtexindex = std::clamp(curtexindex<0 ? 0 : curtexindex+*dir, 0, texmru.length()-1);
    edittex(texmru[curtexindex], false);
}

void gettex()
{
    if(noedit(true))
    {
        return;
    }
    filltexlist();
    int tex = -1;
    LOOP_XYZ(sel, sel.grid, tex = c.texture[sel.orient]);
    for(int i = 0; i < texmru.length(); i++)
    {
        if(texmru[i]==tex)
        {
            curtexindex = i;
            tofronttex();
            return;
        }
    }
}

void getcurtex()
{
    if(noedit(true))
    {
        return;
    }
    filltexlist();
    int index = curtexindex < 0 ? 0 : curtexindex;
    if(!texmru.inrange(index))
    {
        return;
    }
    intret(texmru[index]);
}

void getseltex()
{
    if(noedit(true))
    {
        return;
    }
    cube &c = lookupcube(sel.o, -sel.grid);
    if(c.children || IS_EMPTY(c))
    {
        return;
    }
    intret(c.texture[sel.orient]);
}

void gettexname(int *tex, int *subslot)
{
    if(*tex<0)
    {
        return;
    }
    VSlot &vslot = lookupvslot(*tex, false);
    Slot &slot = *vslot.slot;
    if(!slot.sts.inrange(*subslot))
    {
        return;
    }
    result(slot.sts[*subslot].name);
}

void getslottex(int *idx)
{
    if(*idx < 0 || !slots.inrange(*idx))
    {
        intret(-1);
        return;
    }
    Slot &slot = lookupslot(*idx, false);
    intret(slot.variants->index);
}

COMMANDN(edittex, edittex_, "i");
ICOMMAND(settex, "i", (int *tex), { if(!vslots.inrange(*tex) || noedit()) return; filltexlist(); edittex(*tex); });
COMMAND(gettex, "");
COMMAND(getcurtex, "");
COMMAND(getseltex, "");
ICOMMAND(getreptex, "", (), { if(!noedit()) intret(vslots.inrange(reptex) ? reptex : -1); });
COMMAND(gettexname, "ii");
ICOMMAND(texmru, "b", (int *idx), { filltexlist(); intret(texmru.inrange(*idx) ? texmru[*idx] : texmru.length()); });
ICOMMAND(looptexmru, "re", (ident *id, uint *body),
{
    LOOP_START(id, stack);
    filltexlist();
    for(int i = 0; i < texmru.length(); i++)
    {
        loopiter(id, stack, texmru[i]); execute(body);
    }
    loopend(id, stack);
});
ICOMMAND(numvslots, "", (), intret(vslots.length()));
ICOMMAND(numslots, "", (), intret(slots.length()));
COMMAND(getslottex, "i");
ICOMMAND(texloaded, "i", (int *tex), intret(slots.inrange(*tex) && slots[*tex]->loaded ? 1 : 0));

void replacetexcube(cube &c, int oldtex, int newtex)
{
    for(int i = 0; i < 6; ++i) //for each face
    {
        if(c.texture[i] == oldtex)
        {
            c.texture[i] = newtex;
        }
    }
    //recursively apply to children
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            replacetexcube(c.children[i], oldtex, newtex);
        }
    }
}

void mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Replace, oldtex, newtex, insel ? 1 : 0);
    }
    if(insel)
    {
        LOOP_SEL_XYZ(replacetexcube(c, oldtex, newtex));
    }
    //recursively apply to children
    else
    {
        for(int i = 0; i < 8; ++i)
        {
            replacetexcube(worldroot[i], oldtex, newtex);
        }
    }
    allchanged();
}

bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf)
{
    if(!unpacktex(oldtex, buf, false))
    {
        return false;
    }
    EDITING_VSLOT(oldtex);
    if(!unpacktex(newtex, buf))
    {
        return false;
    }
    mpreplacetex(oldtex, newtex, insel, sel, false);
    return true;
}

/*replace: replaces one texture slot with the most recently queued one
 * Arguments:
 *  insel: toggles whether to apply to the whole map or just the part in selection
 * Returns:
 *  void
 */
void replace(bool insel)
{
    if(noedit())
    {
        return;
    }
    if(reptex < 0)
    {
        conoutf(Console_Error, "can only replace after a texture edit");
        return;
    }
    mpreplacetex(reptex, lasttex, insel, sel, true);
}

#undef EDITING_VSLOT

ICOMMAND(replace, "", (), replace(false));
ICOMMAND(replacesel, "", (), replace(true));

////////// flip and rotate ///////////////
static inline uint dflip(uint face)
{
    return face == faceempty ? face : 0x88888888 - (((face & 0xF0F0F0F0) >> 4) | ((face & 0x0F0F0F0F) << 4));
}

static inline uint cflip(uint face)
{
    return ((face&0xFF00FF00)>>8) | ((face&0x00FF00FF)<<8);
}

static inline uint rflip(uint face)
{
    return ((face&0xFFFF0000)>>16)| ((face&0x0000FFFF)<<16);
}

static inline uint mflip(uint face)
{
    return (face&0xFF0000FF) | ((face&0x00FF0000)>>8) | ((face&0x0000FF00)<<8);
}

void flipcube(cube &c, int d)
{
    swap(c.texture[d*2], c.texture[d*2+1]);
    c.faces[D[d]] = dflip(c.faces[D[d]]);
    c.faces[C[d]] = cflip(c.faces[C[d]]);
    c.faces[R[d]] = rflip(c.faces[R[d]]);
    if(c.children)
    {
        //recursively apply to children
        for(int i = 0; i < 8; ++i)
        {
            if(i&OCTA_DIM(d))
            {
                swap(c.children[i], c.children[i-OCTA_DIM(d)]);
            }
        }
        for(int i = 0; i < 8; ++i)
        {
            flipcube(c.children[i], d);
        }
    }
}

//reassign a quartet of cubes by rotating them 90 deg
static inline void rotatequad(cube &a, cube &b, cube &c, cube &d)
{
    cube t = a;
         a = b;
         b = c;
         c = d;
         d = t;
}

/*rotatecube: rotates a cube by a given number of 90 degree increments
 * Arguments:
 *  c: the cube to rotate
 *  d: the number of 90 degree clockwise increments to rotate by
 * Returns:
 *  void
 *
 * Note that the orientation of rotation is clockwise about the axis normal to
 * the current selection's selected face (using the left-hand rule)
 */
void rotatecube(cube &c, int d)
{
    c.faces[D[d]] = cflip(mflip(c.faces[D[d]]));
    c.faces[C[d]] = dflip(mflip(c.faces[C[d]]));
    c.faces[R[d]] = rflip(mflip(c.faces[R[d]]));
    swap(c.faces[R[d]], c.faces[C[d]]);
    //reassign textures
    swap(c.texture[2*R[d]], c.texture[2*C[d]+1]);
    swap(c.texture[2*C[d]], c.texture[2*R[d]+1]);
    swap(c.texture[2*C[d]], c.texture[2*C[d]+1]);
    //move child members
    if(c.children)
    {
        int row = OCTA_DIM(R[d]);
        int col = OCTA_DIM(C[d]);
        for(int i=0; i<=OCTA_DIM(d); i+=OCTA_DIM(d)) rotatequad
        (
            c.children[i+row],
            c.children[i],
            c.children[i+col],
            c.children[i+col+row]
        );
        //recursively apply to children
        for(int i = 0; i < 8; ++i)
        {
            rotatecube(c.children[i], d);
        }
    }
}

/*mpflip: flips a selection across a plane defined by the selection's face
 * Arguments:
 *  sel: the selection to be fliped
 *  local: whether to send a network message, true if created locally, false if
 *         from another client
 * Returns:
 *  void
 */
void mpflip(selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Flip);
        makeundo();
    }
    int zs = sel.s[DIMENSION(sel.orient)];
    LOOP_XY(sel)
    {
        for(int z = 0; z < zs; ++z)
        {
            flipcube(SELECT_CUBE(x, y, z), DIMENSION(sel.orient));
        }
        for(int z = 0; z < zs/2; ++z)
        {
            cube &a = SELECT_CUBE(x, y, z);
            cube &b = SELECT_CUBE(x, y, zs-z-1);
            swap(a, b);
        }
    }
    changed(sel);
}

void flip()
{
    if(noedit())
    {
        return;
    }
    mpflip(sel, true);
}

void mprotate(int cw, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Rotate, cw);
    }
    int d = DIMENSION(sel.orient);
    if(!DIM_COORD(sel.orient))
    {
        cw = -cw;
    }
    int m = sel.s[C[d]] < sel.s[R[d]] ? C[d] : R[d],
        ss = sel.s[m] = max(sel.s[R[d]], sel.s[C[d]]);
    if(local)
    {
        makeundo();
    }
    for(int z = 0; z < sel.s[D[d]]; ++z)
    {
        for(int i = 0; i < (cw>0 ? 1 : 3); ++i)
        {
            LOOP_XY(sel)
            {
                rotatecube(SELECT_CUBE(x,y,z), d);
            }
            for(int y = 0; y < ss/2; ++y)
            {
                for(int x = 0; x < ss-1-2*y; ++x)
                {
                    rotatequad
                    (
                        SELECT_CUBE(ss-1-y, x+y, z),
                        SELECT_CUBE(x+y, y, z),
                        SELECT_CUBE(y, ss-1-x-y, z),
                        SELECT_CUBE(ss-1-x-y, ss-1-y, z)
                    );
                }
            }
        }
    }
    changed(sel);
}

void rotate(int *cw)
{
    if(noedit())
    {
        return;
    }
    mprotate(*cw, sel, true);
}

COMMAND(flip, "");
COMMAND(rotate, "i");

enum
{
    EditMatFlag_Empty = 0x10000,
    EditMatFlag_NotEmpty = 0x20000,
    EditMatFlag_Solid = 0x30000,
    EditMatFlag_NotSolid = 0x40000
};

static const struct
{
    const char *name;
    int filter;
} editmatfilters[] =
{
    { "empty", EditMatFlag_Empty },
    { "notempty", EditMatFlag_NotEmpty },
    { "solid", EditMatFlag_Solid },
    { "notsolid", EditMatFlag_NotSolid }
};

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
                if(IS_EMPTY(c))
                {
                    break;
                }
                return;
            }
            case EditMatFlag_NotEmpty:
            {
                if(!IS_EMPTY(c))
                {
                    break;
                }
                return;
            }
            case EditMatFlag_Solid:
            {
                if(IS_ENTIRELY_SOLID(c))
                {
                    break;
                }
                return;
            }
            case EditMatFlag_NotSolid:
            {
                if(!IS_ENTIRELY_SOLID(c))
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

/*mpeditmat: selectively fills a selection with selected materials
 * Arguments:
 *  matid: material id to fill
 *  filter: if nonzero, determines what existing mats to apply to
 *  sel: selection of cubes to fill
 *  local: whether to send a network message, true if created locally, false if
 *         from another client
 * Returns:
 *  void
 */
void mpeditmat(int matid, int filter, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Mat, matid, filter);
    }

    ushort filtermat = 0,
           filtermask = 0,
           matmask;
    int filtergeom = 0;
    if(filter >= 0)
    {
        filtermat = filter&0xFF;
        filtermask = filtermat&(MatFlag_Volume|MatFlag_Index) ? MatFlag_Volume|MatFlag_Index : (filtermat&MatFlag_Clip ? MatFlag_Clip : filtermat);
        filtergeom = filter&~0xFF;
    }
    if(matid < 0)
    {
        matid = 0;
        matmask = filtermask;
        if(IS_CLIPPED(filtermat&MatFlag_Volume))
        {
            matmask &= ~MatFlag_Clip;
        }
    }
    else
    {
        matmask = matid&(MatFlag_Volume|MatFlag_Index) ? 0 : (matid&MatFlag_Clip ? ~MatFlag_Clip : ~matid);
        if(IS_CLIPPED(matid&MatFlag_Volume))
        {
            matid |= Mat_Clip;
        }
    }
    LOOP_SEL_XYZ(setmat(c, matid, matmask, filtermat, filtermask, filtergeom));
}

/*editmat: takes the globally defined selection and fills it with a material
 * Arguments:
 *  name: the name of the material (water, alpha, etc.)
 *  filtername: the name of the existing material to apply to
 * Returns:
 *  void
 */
void editmat(char *name, char *filtername)
{
    if(noedit())
    {
        return;
    }
    int filter = -1;
    if(filtername[0])
    {
        for(int i = 0; i < static_cast<int>(sizeof(editmatfilters)/sizeof(editmatfilters[0])); ++i)
        {
            if(!strcmp(editmatfilters[i].name, filtername))
            {
                filter = editmatfilters[i].filter;
                break;
            }
        }
        if(filter < 0)
        {
            filter = findmaterial(filtername);
        }
        if(filter < 0)
        {
            conoutf(Console_Error, "unknown material \"%s\"", filtername);
            return;
        }
    }
    int id = -1;
    if(name[0] || filter < 0)
    {
        id = findmaterial(name);
        if(id<0)
        {
            conoutf(Console_Error, "unknown material \"%s\"", name);
            return;
        }
    }
    mpeditmat(id, filter, sel, true);
}

COMMAND(editmat, "ss");

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


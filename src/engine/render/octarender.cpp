// octarender.cpp: fill vertex arrays with different cube surfaces.
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "grass.h"
#include "octarender.h"
#include "rendergl.h"
#include "renderlights.h"
#include "renderparticles.h"
#include "rendersky.h"
#include "renderva.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"
#include "vacollect.h"

#include "interface/menus.h"

#include "world/entities.h"
#include "world/light.h"
#include "world/material.h"
#include "world/octaworld.h"
#include "world/world.h"


/* global variables */
//////////////////////

int allocva  = 0,
    wtris    = 0,
    wverts   = 0,
    vtris    = 0,
    vverts   = 0,
    glde     = 0,
    gbatches = 0;


/* A vector that carries identically all elements also in the various varoot objects.
 * The entries in the vector will first be the children of varoot[0] followed by
 * varoot[0] itself, followed by the same for the other VAs in varoot. The last
 * element should always be `varoot[7]`.
 */
std::vector<vtxarray *> valist;

/*
 * A vector containing the highest-level vertex array objects.
 * There will always be at least eight VAs in varoot, corresponding to the eight
 * subdivisions of the worldroot cube.
 *
 * If the eight subdivisions of the worldroot cube are larger than `vamaxsize`,
 * these cubes will not host the root VA; children of these cubes will be assigned
 * the root VAs by dropping through child nodes until the cube is no larger than
 * `vamaxsize`. For a worldroot 4x the linear size of `vamaxsize` this will yield 64
 * entries in varoot; for a worldroot 8x the linear size, this will yield 512 entries
 * and so on.
 */
std::vector<vtxarray *> varoot;

ivec worldmin(0, 0, 0),
     worldmax(0, 0, 0);

std::vector<tjoint> tjoints;

VARFP(filltjoints, 0, 1, 1, rootworld.allchanged()); //eliminate "sparklies" by filling in geom t-joints

/* internally relevant functionality */
///////////////////////////////////////

//edgegroup: struct used for tjoint joining (to reduce sparklies between geom faces)
struct edgegroup
{
    ivec slope, origin;
    int axis;

    edgegroup();

    bool operator==(const edgegroup &y) const
    {
        return slope==y.slope && origin==y.origin;
    }
};

edgegroup::edgegroup()
{
    axis = 0;
}

template<>
struct std::hash<edgegroup>
{
    size_t operator()(const edgegroup &g) const
    {
        return g.slope.x^g.slope.y^g.slope.z^g.origin.x^g.origin.y^g.origin.z;
    }
};

namespace
{
    enum
    {
        CubeEdge_Start = 1<<0,
        CubeEdge_End   = 1<<1,
        CubeEdge_Flip  = 1<<2,
        CubeEdge_Dup   = 1<<3
    };

    struct cubeedge
    {
        cube *c;
        int next, offset;
        ushort size;
        uchar index, flags;
    };

    std::vector<cubeedge> cubeedges;
    std::unordered_map<edgegroup, int> edgegroups;

    void gencubeedges(cube &c, const ivec &co, int size)
    {
        std::array<ivec, Face_MaxVerts> pos;
        int vis;
        for(int i = 0; i < 6; ++i)
        {
            if((vis = visibletris(c, i, co, size)))
            {
                int numverts = c.ext ? c.ext->surfaces[i].numverts&Face_MaxVerts : 0;
                if(numverts)
                {
                    const vertinfo *verts = c.ext->verts() + c.ext->surfaces[i].verts;
                    ivec vo = ivec(co).mask(~0xFFF).shl(3);
                    for(int j = 0; j < numverts; ++j)
                    {
                        const vertinfo &v = verts[j];
                        pos[j] = ivec(v.x, v.y, v.z).add(vo);
                    }
                }
                else if(c.merged&(1<<i))
                {
                    continue;
                }
                else
                {
                    std::array <ivec, 4> v;
                    genfaceverts(c, i, v);
                    int order = vis&4 || (!flataxisface(c, i) && faceconvexity(v) < 0) ? 1 : 0;
                    ivec vo = ivec(co).shl(3);
                    pos[numverts++] = v[order].mul(size).add(vo);
                    if(vis&1)
                    {
                        pos[numverts++] = v[order+1].mul(size).add(vo);
                    }
                    pos[numverts++] = v[order+2].mul(size).add(vo);
                    if(vis&2)
                    {
                        pos[numverts++] = v[(order+3)&3].mul(size).add(vo);
                    }
                }
                for(int j = 0; j < numverts; ++j)
                {
                    int e1 = j,
                        e2 = j+1 < numverts ? j+1 : 0;
                    ivec d = pos[e2];
                    d.sub(pos[e1]);
                    if(!d)
                    {
                        continue;
                    }
                    //axistemp1/2 used in `axis` only
                    // x = 0, y = 1, z = 2, `int axis` is the largest component of `d` using
                    // axistemp1/2 to determine if x,y > z and then if x > y
                    int axistemp1 = std::abs(d.x) > std::abs(d.z) ? 0 : 2,
                        axistemp2 = std::abs(d.y) > std::abs(d.z) ? 1 : 2,
                        axis = std::abs(d.x) > std::abs(d.y) ? axistemp1 : axistemp2;
                    if(d[axis] < 0)
                    {
                        d.neg();
                        std::swap(e1, e2);
                    }
                    reduceslope(d);
                    int t1 = pos[e1][axis]/d[axis],
                        t2 = pos[e2][axis]/d[axis];
                    edgegroup g;
                    g.origin = ivec(pos[e1]).sub(ivec(d).mul(t1));
                    g.slope = d;
                    g.axis = axis;
                    cubeedge ce;
                    ce.c = &c;
                    ce.offset = t1;
                    ce.size = t2 - t1;
                    ce.index = i*(Face_MaxVerts+1)+j;
                    ce.flags = CubeEdge_Start | CubeEdge_End | (e1!=j ? CubeEdge_Flip : 0);
                    ce.next = -1;
                    bool insert = true;
                    auto exists = edgegroups.find(g);
                    if(exists != edgegroups.end())
                    {
                        int prev = -1,
                            cur  = (*exists).second;
                        while(cur >= 0)
                        {
                            cubeedge &p = cubeedges[cur];
                            if(p.flags&CubeEdge_Dup ?
                                ce.offset>=p.offset && ce.offset+ce.size<=p.offset+p.size :
                                ce.offset==p.offset && ce.size==p.size)
                            {
                                p.flags |= CubeEdge_Dup;
                                insert = false;
                                break;
                            }
                            else if(ce.offset >= p.offset)
                            {
                                if(ce.offset == p.offset+p.size)
                                {
                                    ce.flags &= ~CubeEdge_Start;
                                }
                                prev = cur;
                                cur = p.next;
                            }
                            else
                            {
                                break;
                            }
                        }
                        if(insert)
                        {
                            ce.next = cur;
                            while(cur >= 0)
                            {
                                const cubeedge &p = cubeedges[cur];
                                if(ce.offset+ce.size==p.offset)
                                {
                                    ce.flags &= ~CubeEdge_End;
                                    break;
                                }
                                cur = p.next;
                            }
                            if(prev>=0)
                            {
                                cubeedges[prev].next = cubeedges.size();
                            }
                            else
                            {
                                (*exists).second = cubeedges.size();
                            }
                        }
                    }
                    else
                    {
                        edgegroups[g] = cubeedges.size();
                    }
                    if(insert)
                    {
                        cubeedges.push_back(ce);
                    }
                }
            }
        }
    }

    void gencubeedges(std::array<cube, 8> &c, const ivec &co = ivec(0, 0, 0), int size = rootworld.mapsize()>>1)
    {
        neighborstack[++neighbordepth] = &c[0];
        for(int i = 0; i < 8; ++i)
        {
            ivec o(i, co, size);
            if(c[i].ext)
            {
                c[i].ext->tjoints = -1;
            }
            if(c[i].children)
            {
                gencubeedges(*(c[i].children), o, size>>1);
            }
            else if(!(c[i].isempty()))
            {
                gencubeedges(c[i], o, size);
            }
        }
        --neighbordepth;
    }

    void addtjoint(const edgegroup &g, const cubeedge &e, int offset)
    {
        int vcoord = (g.slope[g.axis]*offset + g.origin[g.axis]) & 0x7FFF;
        tjoint tj = tjoint();
        tj.offset = vcoord / g.slope[g.axis];
        tj.edge = e.index;
        int prev = -1,
            cur  = ext(*e.c).tjoints;
        while(cur >= 0)
        {
            tjoint &o = tjoints[cur];
            if(tj.edge < o.edge || (tj.edge==o.edge && (e.flags&CubeEdge_Flip ? tj.offset > o.offset : tj.offset < o.offset)))
            {
                break;
            }
            prev = cur;
            cur = o.next;
        }
        tj.next = cur;
        tjoints.push_back(tj);
        if(prev < 0)
        {
            e.c->ext->tjoints = tjoints.size()-1;
        }
        else
        {
            tjoints[prev].next = tjoints.size()-1;
        }
    }

    void precachetextures()
    {
        std::vector<int> texs;
        for(uint i = 0; i < valist.size(); i++)
        {
            const vtxarray *va = valist[i];
            for(int j = 0; j < va->texs; ++j)
            {
                int tex = va->texelems[j].texture;
                if(std::find(texs.begin(), texs.end(), tex) != texs.end())
                {
                    texs.push_back(tex);
                }
            }
        }
        for(uint i = 0; i < texs.size(); i++)
        {
            lookupvslot(texs[i]);
        }
    }
}

/* externally relevant functionality */
///////////////////////////////////////

void findtjoints(int cur, const edgegroup &g)
{
    int active = -1;
    while(cur >= 0)
    {
        cubeedge &e = cubeedges[cur];
        int prevactive = -1,
            curactive  = active;
        while(curactive >= 0)
        {
            const cubeedge &a = cubeedges[curactive];
            if(a.offset+a.size <= e.offset)
            {
                if(prevactive >= 0)
                {
                    cubeedges[prevactive].next = a.next;
                }
                else
                {
                    active = a.next;
                }
            }
            else
            {
                prevactive = curactive;
                if(!(a.flags&CubeEdge_Dup))
                {
                    if(e.flags&CubeEdge_Start && e.offset > a.offset && e.offset < a.offset+a.size)
                    {
                        addtjoint(g, a, e.offset);
                    }
                    if(e.flags&CubeEdge_End && e.offset+e.size > a.offset && e.offset+e.size < a.offset+a.size)
                    {
                        addtjoint(g, a, e.offset+e.size);
                    }
                }
                if(!(e.flags&CubeEdge_Dup))
                {
                    if(a.flags&CubeEdge_Start && a.offset > e.offset && a.offset < e.offset+e.size)
                    {
                        addtjoint(g, e, a.offset);
                    }
                    if(a.flags&CubeEdge_End && a.offset+a.size > e.offset && a.offset+a.size < e.offset+e.size)
                    {
                        addtjoint(g, e, a.offset+a.size);
                    }
                }
            }
            curactive = a.next;
        }
        int next = e.next;
        e.next = active;
        active = cur;
        cur = next;
    }
}

//takes a 3d vec3 and transforms it into a packed ushort vector
//the output ushort is in base 360 and has yaw in the first place and pitch in the second place
//the second place has pitch as a range from 0 to 90
//since this is a normal vector, no magnitude needed
ushort encodenormal(const vec &n)
{
    if(n.iszero())
    {
        return 0;
    }
    int yaw = static_cast<int>(-std::atan2(n.x, n.y)*RAD), //arctangent in degrees
        pitch = static_cast<int>(std::asin(n.z)*RAD); //arcsin in degrees
    return static_cast<ushort>(std::clamp(pitch + 90, 0, 180)*360 + (yaw < 0 ? yaw%360 + 360 : yaw%360) + 1);
}

void reduceslope(ivec &n)
{
    int mindim = -1,
        minval = 64;
    for(int i = 0; i < 3; ++i)
    {
        if(n[i])
        {
            int val = std::abs(n[i]);
            if(mindim < 0 || val < minval)
            {
                mindim = i;
                minval = val;
            }
        }
    }
    if(!(n[R[mindim]]%minval) && !(n[C[mindim]]%minval))
    {
        n.div(minval);
    }
    while(!((n.x|n.y|n.z)&1))
    {
        n.shr(1); //shift right 1 to reduce slope
    }
}

void guessnormals(const vec *pos, int numverts, vec *normals)
{
    vec n1, n2;
    n1.cross(pos[0], pos[1], pos[2]);
    if(numverts != 4)
    {
        n1.normalize();
        for(int k = 0; k < numverts; ++k)
        {
            normals[k] = n1;
        }
        return;
    }
    n2.cross(pos[0], pos[2], pos[3]);
    if(n1.iszero())
    {
        n2.normalize();
        for(int k = 0; k < 4; ++k)
        {
            normals[k] = n2;
        }
        return;
    }
    else
    {
        n1.normalize();
    }
    if(n2.iszero())
    {
        for(int k = 0; k < 4; ++k)
        {
            normals[k] = n1;
        }
        return;
    }
    else
    {
        n2.normalize();
    }
    vec avg = vec(n1).add(n2).normalize();
    normals[0] = avg;
    normals[1] = n1;
    normals[2] = avg;
    normals[3] = n2;
}

//va external fxns

/* destroyva
 * destroys the vertex array object, its various buffer objects and information from
 * the valist object
 *
 * if reparent is set to true, assigns child vertex arrays to the parent of the selected va
 */
void destroyva(vtxarray *va, bool reparent)
{
    wverts -= va->verts;
    wtris -= va->tris + va->alphabacktris + va->alphafronttris + va->refracttris + va->decaltris;
    allocva--;
    auto itr = std::find(valist.begin(), valist.end(), va);
    if(itr != valist.end())
    {
        valist.erase(itr);
    }
    if(!va->parent)
    {
        auto itr2 = std::find(valist.begin(), valist.end(), va);
        if(itr2 != valist.end())
        {
            valist.erase(itr2);
        }
    }
    if(reparent)
    {
        if(va->parent)
        {
            auto itr = std::find(va->parent->children.begin(), va->parent->children.end(), va);
            if(itr != va->parent->children.end())
            {
                va->parent->children.erase(itr);
            }
        }
        for(uint i = 0; i < va->children.size(); i++)
        {
            vtxarray *child = va->children[i];
            child->parent = va->parent;
            if(child->parent)
            {
                child->parent->children.push_back(child);
            }
        }
    }
    if(va->vbuf)
    {
        destroyvbo(va->vbuf);
    }
    if(va->ebuf)
    {
        destroyvbo(va->ebuf);
    }
    if(va->skybuf)
    {
        destroyvbo(va->skybuf);
    }
    if(va->decalbuf)
    {
        destroyvbo(va->decalbuf);
    }
    if(va->texelems)
    {
        delete[] va->texelems;
    }
    if(va->decalelems)
    {
        delete[] va->decalelems;
    }
    delete va;
}

//recursively clear vertex arrays for an array of eight cube objects and their children
void clearvas(std::array<cube, 8> &c)
{
    for(int i = 0; i < 8; ++i)
    {
        if(c[i].ext)
        {
            if(c[i].ext->va)
            {
                destroyva(c[i].ext->va, false);
            }
            c[i].ext->va = nullptr;
            c[i].ext->tjoints = -1;
        }
        if(c[i].children)
        {
            clearvas(*c[i].children);
        }
    }
}

void updatevabb(vtxarray *va, bool force)
{
    if(!force && va->bbmin.x >= 0)
    {
        return;
    }
    va->bbmin = va->geommin;
    va->bbmax = va->geommax;
    va->bbmin.min(va->watermin);
    va->bbmax.max(va->watermax);
    va->bbmin.min(va->glassmin);
    va->bbmax.max(va->glassmax);
    for(vtxarray *child : va->children)
    {
        updatevabb(child, force);
        va->bbmin.min(child->bbmin);
        va->bbmax.max(child->bbmax);
    }
    for(octaentities *oe : va->mapmodels)
    {
        va->bbmin.min(oe->bbmin);
        va->bbmax.max(oe->bbmax);
    }
    for(octaentities *oe : va->decals)
    {
        va->bbmin.min(oe->bbmin);
        va->bbmax.max(oe->bbmax);
    }
    va->bbmin.max(va->o);
    va->bbmax.min(ivec(va->o).add(va->size));
    worldmin.min(va->bbmin);
    worldmax.max(va->bbmax);
}

//update vertex array bounding boxes recursively from the root va object down to all children
void updatevabbs(bool force)
{
    if(force)
    {
        worldmin = ivec(rootworld.mapsize(), rootworld.mapsize(), rootworld.mapsize());
        worldmax = ivec(0, 0, 0);
        for(uint i = 0; i < varoot.size(); i++)
        {
            updatevabb(varoot[i], true);
        }
        if(worldmin.x >= worldmax.x)
        {
            worldmin = ivec(0, 0, 0);
            worldmax = ivec(rootworld.mapsize(), rootworld.mapsize(), rootworld.mapsize());
        }
    }
    else
    {
        for(uint i = 0; i < varoot.size(); i++)
        {
            updatevabb(varoot[i]);
        }
    }
}

void cubeworld::findtjoints()
{
    gencubeedges(*worldroot);
    tjoints.clear();
    for(auto &[k, t] : edgegroups)
    {
        ::findtjoints(t, k);
    }
    cubeedges.clear();
    edgegroups.clear();
}

void cubeworld::allchanged(bool load)
{
    if(!worldroot)
    {
        return;
    }
    if(mainmenu)
    {
        load = false;
    }
    if(load)
    {
        initlights();
    }
    clearvas(*worldroot);
    occlusionengine.resetqueries();
    resetclipplanes();
    entitiesinoctanodes();
    tjoints.clear();
    if(filltjoints)
    {
        findtjoints();
    }
    octarender();
    if(load)
    {
        precachetextures();
    }
    setupmaterials();
    clearshadowcache();
    updatevabbs(true);
    if(load)
    {
        genshadowmeshes();
        seedparticles();
    }
}

void initoctarendercmds()
{
    addcommand("recalc", reinterpret_cast<identfun>(+[](){rootworld.allchanged(true);}), "", Id_Command);
}

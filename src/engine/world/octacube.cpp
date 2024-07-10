/**
 * @brief cube object member functions
 *
 * This file implements the methods applying to the cube object, the individual
 * unit which acts as the nodes in a cubeworld octal tree.
 */

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include "light.h"
#include "octaworld.h"
#include "raycube.h"
#include "world.h"

#include "interface/console.h"

#include "render/octarender.h"
#include "render/renderwindow.h"

VAR(maxmerge, 0, 6, 12); //max gridpower to remip merge
VAR(minface, 0, 4, 12);

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

bool cube::mincubeface(const cube &cu, int orient, const ivec &co, int size, facebounds &orig) const
{
    ivec no;
    int nsize;
    const cube &nc = rootworld.neighborcube(orient, co, size, no, nsize);
    facebounds mincf;
    mincf.u1 = orig.u2;
    mincf.u2 = orig.u1;
    mincf.v1 = orig.v2;
    mincf.v2 = orig.v1;
    mincubeface(nc, oppositeorient(orient), no, nsize, orig, mincf, cu.material&Mat_Alpha ? Mat_Air : Mat_Alpha, Mat_Alpha);
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

template <> struct std::hash<cube::plink>
{
    size_t operator()(const cube::plink& x) const
    {
        return static_cast<uint>(x.from.x)^(static_cast<uint>(x.from.y)<<8);
    }
};

void cube::freecubeext(cube &c)
{
    if(c.ext)
    {
        delete[] reinterpret_cast<uchar *>(c.ext);
        c.ext = nullptr;
    }
}

void cube::discardchildren(bool fixtex, int depth)
{
    visible = 0;
    merged = 0;
    if(ext)
    {
        if(ext->va)
        {
            destroyva(ext->va);
        }
        ext->va = nullptr;
        ext->tjoints = -1;
        freeoctaentities(*this);
        freecubeext(*this);
    }
    if(children)
    {
        uint filled = faceempty;
        for(int i = 0; i < 8; ++i)
        {
            (*children)[i].discardchildren(fixtex, depth+1);
            filled |= (*children)[i].faces[0];
        }
        if(fixtex)
        {
            for(int i = 0; i < 6; ++i)
            {
                texture[i] = getmippedtexture(*this, i);
            }
            if(depth > 0 && filled != faceempty)
            {
                faces[0] = facesolid;
            }
        }
        delete children;
        children = nullptr;
        allocnodes--;
    }
}

bool cube::isvalidcube() const
{
    clipplanes p;
    genclipbounds(*this, ivec(0, 0, 0), 256, p);
    genclipplanes(*this, ivec(0, 0, 0), 256, p);
    // test that cube is convex
    for(uint i = 0; i < 8; ++i)
    {
        vec v = p.v[i];
        for(uint j = 0; j < p.size; ++j)
        {
            if(p.p[j].dist(v)>1e-3f)
            {
                return false;
            }
        }
    }
    return true;
}

bool cube::poly::clippoly(const facebounds &b)
{
    std::array<pvert, Face_MaxVerts+4> verts1,
                                       verts2;
    int numverts1 = 0,
        numverts2 = 0,
        px = verts[numverts-1].x,
        py = verts[numverts-1].y;
    for(int i = 0; i < numverts; ++i)
    {
        int x = verts[i].x,
            y = verts[i].y;
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
    std::memcpy(verts, verts2.data(), numverts2*sizeof(pvert));
    numverts = numverts2;
    return true;
}

bool cube::genpoly(int orient, const ivec &o, int size, int vis, ivec &n, int &offset, poly &p)
{
    int dim = DIMENSION(orient),
        coord = DIM_COORD(orient);
    std::array<ivec, 4> v;
    genfaceverts(*this, orient, v);
    if(flataxisface(*this, orient))
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
        if(!n)
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

    if(faceedges(*this, orient) != facesolid)
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
        px = cx;
        py = cy;
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
    p.c = this;
    p.merged = false;
    if(minface && size >= 1<<minface && touchingface(*this, orient))
    {
        facebounds b;
        b.u1 = b.u2 = p.verts[0].x;
        b.v1 = b.v2 = p.verts[0].y;
        for(int i = 1; i < p.numverts; i++)
        {
            const pvert &v = p.verts[i];
            b.u1 = std::min(b.u1, v.x);
            b.u2 = std::max(b.u2, v.x);
            b.v1 = std::min(b.v1, v.y);
            b.v2 = std::max(b.v2, v.y);
        }
        if(mincubeface(*this, orient, o, size, b) && p.clippoly(b))
        {
            p.merged = true;
        }
    }
    return true;
}

bool cube::poly::mergepolys(std::unordered_set<plink> &links, std::deque<const plink *> &queue, int owner, poly &q, const pedge &e)
{
    int pe = -1,
        qe = -1;
    for(int i = 0; i < numverts; ++i)
    {
        if(verts[i] == e.from)
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
    if(verts[(pe+1)%numverts] != e.to || q.verts[(qe+1)%q.numverts] != e.from)
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
    pvert mergeverts[2*Face_MaxVerts];
    int nummergeverts = 0,
        index = pe+2; // starts at A = T+1, ends at F = T+this.numverts
    for(int i = 0; i < numverts-1; ++i)
    {
        if(index >= numverts)
        {
            index -= numverts;
        }
        mergeverts[nummergeverts++] = verts[index++];
    }
    index = qe+2; // starts at C = T+2 = F+1, ends at T = T+q.numverts
    int px = static_cast<int>(mergeverts[nummergeverts-1].x) - static_cast<int>(mergeverts[nummergeverts-2].x),
        py = static_cast<int>(mergeverts[nummergeverts-1].y) - static_cast<int>(mergeverts[nummergeverts-2].y);
    for(int i = 0; i < q.numverts-1; ++i)
    {
        if(index >= q.numverts)
        {
            index -= q.numverts;
        }
        const pvert &src = q.verts[index++];
        int cx = static_cast<int>(src.x) - static_cast<int>(mergeverts[nummergeverts-1].x),
            cy = static_cast<int>(src.y) - static_cast<int>(mergeverts[nummergeverts-1].y),
            dir = px*cy - py*cx;
        if(dir > 0)
        {
            return false;
        }
        if(!dir)
        {
            nummergeverts--;
        }
        mergeverts[nummergeverts++] = src;
        px = cx;
        py = cy;
    }
    int cx = static_cast<int>(mergeverts[0].x) - static_cast<int>(mergeverts[nummergeverts-1].x),
        cy = static_cast<int>(mergeverts[0].y) - static_cast<int>(mergeverts[nummergeverts-1].y),
        dir = px*cy - py*cx;
    if(dir > 0)
    {
        return false;
    }
    if(!dir)
    {
        nummergeverts--;
    }
    if(nummergeverts > Face_MaxVerts)
    {
        return false;
    }
    q.merged = true;
    q.numverts = 0;
    merged = true;
    numverts = nummergeverts;
    std::memcpy(verts, mergeverts, nummergeverts*sizeof(pvert));
    int prev = numverts-1;
    for(int j = 0; j < numverts; ++j)
    {
        pedge e(verts[prev], verts[j]);
        int order = e.from.x > e.to.x || (e.from.x == e.to.x && e.from.y > e.to.y) ? 1 : 0;
        if(order)
        {
            std::swap(e.from, e.to);
        }

        plink l;
        auto itr = links.find(e); //search for a plink that looks like the pedge we have
        if(itr != links.end())
        {
            l = *itr;
            links.erase(e); // we will place an updated verson of this immutable object at the end
        }
        else
        {
            l = e; //even though we searched find(e), l and e are NOT the same because they are of different types (l is derived)
        }
        bool shouldqueue = l.polys[order] < 0 && l.polys[order^1] >= 0;
        l.polys[order] = owner;
        links.insert(l);
        if(shouldqueue)
        {
            queue.push_back(&*links.find(l));
        }
        prev = j;
    }

    return true;
}

void cube::addmerge(int orient, const ivec &n, int offset, poly &p)
{
    merged |= 1<<orient;
    if(!p.numverts)
    {
        if(ext)
        {
            ext->surfaces[orient] = surfaceinfo();
        }
        return;
    }
    std::array<vertinfo, Face_MaxVerts> verts;
    surfaceinfo surf = surfaceinfo();
    surf.numverts |= p.numverts;
    int dim = DIMENSION(orient),
        coord = DIM_COORD(orient),
        c = C[dim],
        r = R[dim];
    for(int k = 0; k < p.numverts; ++k)
    {
        const pvert &src = p.verts[coord ? k : p.numverts-1-k];
        vertinfo &dst = verts[k];
        ivec v;
        v[c] = src.x;
        v[r] = src.y;
        v[dim] = -(offset + n[c]*src.x + n[r]*src.y)/n[dim];
        dst.set(v);
    }
    if(ext)
    {
        const surfaceinfo &oldsurf = ext->surfaces[orient];
        int numverts = oldsurf.numverts&Face_MaxVerts;
        if(numverts == p.numverts)
        {
            ivec v0 = verts[0].getxyz();
            const vertinfo *oldverts = ext->verts() + oldsurf.verts;
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
    setsurface(*this, orient, surf, verts.data(), p.numverts);
}

void cube::clearmerge(int orient)
{
    if(merged&(1<<orient))
    {
        merged &= ~(1<<orient);
        if(ext)
        {
            ext->surfaces[orient] = surfaceinfo();
        }
    }
}

void cube::addmerges(int orient, const ivec &n, int offset, std::deque<poly> &polys)
{
    for(poly &p : polys)
    {
        if(p.merged)
        {
            (*(p.c)).addmerge(orient, n, offset, p);
        }
        else
        {
            (*(p.c)).clearmerge(orient);
        }
    }
}

void cube::mergepolys(int orient, const ivec &n, int offset, std::deque<poly> &polys)
{
    if(polys.size() <= 1)
    {
        addmerges(orient, n, offset, polys);
        return;
    }
    std::unordered_set<plink> links(polys.size() <= 32 ? 128 : 1024);
    std::deque<const plink *> queue;
    for(uint i = 0; i < polys.size(); i++)
    {
        const poly &p = polys[i];
        int prev = p.numverts-1;
        for(int j = 0; j < p.numverts; ++j)
        {
            pedge e(p.verts[prev], p.verts[j]);
            int order = e.from.x > e.to.x || (e.from.x == e.to.x && e.from.y > e.to.y) ? 1 : 0;
            if(order)
            {
                std::swap(e.from, e.to);
            }
            plink l;
            auto itr = links.find(e);
            if(itr != links.end())
            {
                l = *itr;
                links.erase(e);
            }
            l.polys[order] = i;
            links.insert(l);
            if(l.polys[0] >= 0 && l.polys[1] >= 0)
            {
                queue.push_back(&*links.find(l));
            }
            prev = j;
        }
    }
    std::deque<const plink *> nextqueue;
    while(queue.size())
    {
        for(const plink *&l : queue)
        {
            if(l->polys[0] >= 0 && l->polys[1] >= 0)
            {
                polys[l->polys[0]].mergepolys(links, nextqueue, l->polys[0], polys[l->polys[1]], *l);
            }
        }
        queue.clear();
        queue.insert(queue.end(), nextqueue.begin(), nextqueue.end());
        nextqueue.clear();
    }
    addmerges(orient, n, offset, polys);
}

bool operator==(const cube::cfkey &x, const cube::cfkey &y)
{
    return x.orient == y.orient && x.tex == y.tex && x.n == y.n && x.offset == y.offset && x.material==y.material;
}

template<>
struct std::hash<cube::cfkey>
{
    size_t operator()(const cube::cfkey &k) const
    {
        auto ivechash = std::hash<ivec>();
        return ivechash(k.n)^k.offset^k.tex^k.orient^k.material;
    }
};

//recursively goes through children of cube passed and attempts to merge faces together
void cube::genmerges(cube * root, const ivec &o, int size)
{
    static std::unordered_map<cfkey, cfpolys> cpolys;
    neighborstack[++neighbordepth] = this;
    for(int i = 0; i < 8; ++i)
    {
        ivec co(i, o, size);
        int vis;
        if(this[i].children)
        {
            (this[i]).children->at(0).genmerges(root, co, size>>1);
        }
        else if(!(this[i].isempty()))
        {
            for(int j = 0; j < 6; ++j)
            {
                if((vis = visibletris(this[i], j, co, size)))
                {
                    cfkey k;
                    poly p;
                    if(size < 1<<maxmerge && this != root)
                    {
                        if(genpoly(j, co, size, vis, k.n, k.offset, p))
                        {
                            k.orient = j;
                            k.tex = this[i].texture[j];
                            k.material = this[i].material&Mat_Alpha;
                            cpolys[k].polys.push_back(p);
                            continue;
                        }
                    }
                    else if(minface && size >= 1<<minface && touchingface(this[i], j))
                    {
                        if(genpoly(j, co, size, vis, k.n, k.offset, p) && p.merged)
                        {
                            this[i].addmerge( j, k.n, k.offset, p);
                            continue;
                        }
                    }
                    this[i].clearmerge(j);
                }
            }
        }
        if((size == 1<<maxmerge || this == root) && cpolys.size())
        {
            for(auto &[k, t] : cpolys)
            {
                mergepolys(k.orient, k.n, k.offset, t.polys);
            }
            cpolys.clear();
        }
    }
    --neighbordepth;
}

void cube::calcmerges(cube * root)
{
    genmerges(root);
}

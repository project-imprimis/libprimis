/* bounding interval hierarchy (BIH)
 *
 * the BIH code is used to calculate the intersection of models with other models
 * and the world, specifically for physics collision calculations. BIH methods
 * are also used for the procedural ragdoll physics.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"

#include <memory>
#include <optional>

#include "entities.h"
#include "physics.h"
#include "raycube.h"

#include "render/rendermodel.h"
#include "render/shaderparam.h"
#include "render/stain.h"
#include "render/texture.h"

#include "world/bih.h"

#include "model/model.h"

int BIH::node::axis() const
{
    return child[0]>>14;
}

int BIH::node::childindex(int which) const
{
    return child[which]&0x3FFF;
}

bool BIH::node::isleaf(int which) const
{
    return (child[1]&(1<<(14+which)))!=0;
}

bool BIH::mesh::tribb::outside(const ivec &bo, const ivec &br) const
{
    return std::abs(bo.x - center.x) > br.x + radius.x ||
           std::abs(bo.y - center.y) > br.y + radius.y ||
           std::abs(bo.z - center.z) > br.z + radius.z;
}

BIH::mesh::mesh() : numnodes(0), numtris(0), tex(nullptr), flags(0) {}

vec BIH::mesh::getpos(int i) const
{
    return *reinterpret_cast<const vec *>(pos + i*posstride);
}
vec2 BIH::mesh::gettc(int i) const
{
    return *reinterpret_cast<const vec2 *>(tc + i*tcstride);
}

void BIH::mesh::setmesh(const tri *tris, int numtris,
                        const uchar *pos, int posstride,
                        const uchar *tc, int tcstride)
{
    this->tris = tris;
    this->numtris = numtris;
    this->pos = pos;
    this->posstride = posstride;
    this->tc = tc;
    this->tcstride = tcstride;
}


matrix4x3 BIH::mesh::invxform() const
{
    matrix4x3 ixf(xform);
    ixf.invert();
    return ixf;
}

matrix3 BIH::mesh::xformnorm() const
{
    matrix3 xfn(xform);
    xfn.normalize();
    return xfn;
}

matrix3 BIH::mesh::invxformnorm() const
{
    matrix3 ixfn(xformnorm());
    ixfn.invert();
    return ixfn;
}

float BIH::mesh::scale() const
{
    return xform.a.magnitude();
}

/* diagram of a,b,c,n vectors
 * a is the vector between the origin and the point 0 indicated
 * n is the triangle normal
 * b,c are displacement vectors from 1->2
 * there is no explicit vector from points 1 to 2
 *            →       →
 *            a       n
 *             0—————————>
 *            / \
 *        →  /   \  →
 *        b /     \ c
 *         /       \
 *        1—————————2
 */

bool BIH::triintersect(const mesh &m, int tidx, const vec &mo, const vec &mray, float maxdist, float &dist, int mode) const
{
    const mesh::tri &t = m.tris[tidx];
    vec a = m.getpos(t.vert[0]), //position of vert 0
        b = m.getpos(t.vert[1]).sub(a), //displacement vector from vert 0->1
        c = m.getpos(t.vert[2]).sub(a), //displacement vector from vert 0->2
        n = vec().cross(b, c), //normal of the triangle
        r = vec(a).sub(mo), //mo is transform of o
        e = vec().cross(r, mray); //mray is transform of ray
    float det = mray.dot(n),
          v, w, f;
    if(det >= 0)
    {
        if(!(mode&Ray_Shadow) && m.flags&Mesh_CullFace)
        {
            return false;
        }
        v = e.dot(c);
        if(v < 0 || v > det)
        {
            return false;
        }
        w = -e.dot(b);
        if(w < 0 || v + w > det)
        {
            return false;
        }
        f = r.dot(n)*m.scale();
        if(f < 0 || f > maxdist*det || !det)
        {
            return false;
        }
    }
    else
    {
        v = e.dot(c);
        if(v > 0 || v < det)
        {
            return false;
        }
        w = -e.dot(b);
        if(w > 0 || v + w < det)
        {
            return false;
        }
        f = r.dot(n)*m.scale();
        if(f > 0 || f < maxdist*det)
        {
            return false;
        }
    }
    float invdet = 1/det;
    if(m.flags&Mesh_Alpha && (mode&Ray_Shadow)==Ray_Shadow && m.tex->alphamask)
    {
        vec2 at = m.gettc(t.vert[0]),
             bt = m.gettc(t.vert[1]).sub(at).mul(v*invdet),
             ct = m.gettc(t.vert[2]).sub(at).mul(w*invdet);
        at.add(bt).add(ct);
        int si = std::clamp(static_cast<int>(m.tex->xs * at.x), 0, m.tex->xs-1),
            ti = std::clamp(static_cast<int>(m.tex->ys * at.y), 0, m.tex->ys-1);
        if(!(m.tex->alphamask[ti*((m.tex->xs+7)/8) + si/8] & (1<<(si%8))))
        {
            return false;
        }
    }
    if(!(mode&Ray_Shadow))
    {
        hitsurface = m.xformnorm().transform(n).normalize();
    }
    dist = f*invdet;
    return true; //true if collided
}

bool BIH::traverse(const mesh &m, const vec &o, const vec &ray, const vec &invray, float maxdist, float &dist, int mode, const node *curnode, float tmin, float tmax) const
{
    struct traversestate
    {
        const BIH::node *node;
        float tmin, tmax;
    };
    std::array<traversestate, 128> stack;
    size_t stacksize = 0;
    ivec order(ray.x>0 ? 0 : 1, ray.y>0 ? 0 : 1, ray.z>0 ? 0 : 1);
    vec mo = m.invxform().transform(o), //invxform is inverse transform 4x3 matrix; transform by vec o
        mray = m.invxformnorm().transform(ray);
    for(;;)
    {
        int axis = curnode->axis();
        int nearidx = order[axis],
            faridx = nearidx^1;
        float nearsplit = (curnode->split[nearidx] - o[axis])*invray[axis],
              farsplit = (curnode->split[faridx] - o[axis])*invray[axis];
        if(nearsplit <= tmin)
        {
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    tmin = std::max(tmin, farsplit);
                    continue;
                }
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, dist, mode))
                {
                    return true;
                }
            }
        }
        else if(curnode->isleaf(nearidx))
        {
            if(triintersect(m, curnode->childindex(nearidx), mo, mray, maxdist, dist, mode))
            {
                return true;
            }
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    tmin = std::max(tmin, farsplit);
                    continue;
                }
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, dist, mode))
                {
                    return true;
                }
            }
        }
        else
        {
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    if(stacksize < stack.size())
                    {
                        traversestate &save = stack[stacksize++];
                        save.node = curnode + curnode->childindex(faridx);
                        save.tmin = std::max(tmin, farsplit);
                        save.tmax = tmax;
                    }
                    else
                    {
                        if(traverse(m, o, ray, invray, maxdist, dist, mode, curnode + curnode->childindex(nearidx), tmin, std::min(tmax, nearsplit)))
                        {
                            return true;
                        }
                        curnode += curnode->childindex(faridx);
                        tmin = std::max(tmin, farsplit);
                        continue;
                    }
                }
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, dist, mode))
                {
                    return true;
                }
            }
            curnode += curnode->childindex(nearidx);
            tmax = std::min(tmax, nearsplit);
            continue;
        }
        if(stacksize <= 0)
        {
            return false;
        }
        traversestate &restore = stack[--stacksize];
        curnode = restore.node;
        tmin = restore.tmin;
        tmax = restore.tmax;
    }
}

bool BIH::traverse(const vec &o, const vec &ray, float maxdist, float &dist, int mode) const
{
    //if components are zero, set component to large value: 1e16, else invert
    vec invray(ray.x ? 1/ray.x : 1e16f, ray.y ? 1/ray.y : 1e16f, ray.z ? 1/ray.z : 1e16f);
    for(const mesh &m : meshes)
    {
        if(!(m.flags&Mesh_Render) || (!(mode&Ray_Shadow) && m.flags&Mesh_NoClip))
        {
            continue;
        }
        float t1 = (m.bbmin.x - o.x)*invray.x,
              t2 = (m.bbmax.x - o.x)*invray.x,
              tmin, tmax;
        if(invray.x > 0)
        {
            tmin = t1;
            tmax = t2;
        }
        else
        {
            tmin = t2;
            tmax = t1;
        }
        t1 = (m.bbmin.y - o.y)*invray.y;
        t2 = (m.bbmax.y - o.y)*invray.y;
        if(invray.y > 0)
        {
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
        }
        else
        {
            tmin = std::max(tmin, t2);
            tmax = std::min(tmax, t1);
        }
        t1 = (m.bbmin.z - o.z)*invray.z;
        t2 = (m.bbmax.z - o.z)*invray.z;
        if(invray.z > 0)
        {
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
        }
        else
        {
            tmin = std::max(tmin, t2);
            tmax = std::min(tmax, t1);
        }
        tmax = std::min(tmax, maxdist);
        if(tmin < tmax && traverse(m, o, ray, invray, maxdist, dist, mode, m.nodes, tmin, tmax))
        {
            return true;
        }
    }
    return false;
}

void BIH::build(mesh &m, uint *indices, int numindices, const ivec &vmin, const ivec &vmax) const
{
    int axis = 2;
    for(int k = 0; k < 2; ++k)
    {
        if(vmax[k] - vmin[k] > vmax[axis] - vmin[axis])
        {
            axis = k;
        }
    }
    ivec leftmin, leftmax, rightmin, rightmax;
    int splitleft, splitright,
        left, right;
    for(int k = 0; k < 3; ++k)
    {
        leftmin = rightmin = ivec(INT_MAX, INT_MAX, INT_MAX);
        leftmax = rightmax = ivec(INT_MIN, INT_MIN, INT_MIN);
        int split = (vmax[axis] + vmin[axis])/2;
        for(left = 0, right = numindices, splitleft = SHRT_MIN, splitright = SHRT_MAX; left < right;)
        {
            const mesh::tribb &tri = m.tribbs[indices[left]];
            ivec trimin = ivec(tri.center).sub(ivec(tri.radius)),
                 trimax = ivec(tri.center).add(ivec(tri.radius));
            int amin = trimin[axis],
                amax = trimax[axis];
            if(std::max(split - amin, 0) > std::max(amax - split, 0))
            {
                ++left;
                splitleft = std::max(splitleft, amax);
                leftmin.min(trimin);
                leftmax.max(trimax);
            }
            else
            {
                --right;
                std::swap(indices[left], indices[right]);
                splitright = std::min(splitright, amin);
                rightmin.min(trimin);
                rightmax.max(trimax);
            }
        }
        if(left > 0 && right < numindices)
        {
            break;
        }
        axis = (axis+1)%3;
    }

    if(!left || right==numindices)
    {
        leftmin = rightmin = ivec(INT_MAX, INT_MAX, INT_MAX);
        leftmax = rightmax = ivec(INT_MIN, INT_MIN, INT_MIN);
        left = right = numindices/2;
        splitleft = SHRT_MIN;
        splitright = SHRT_MAX;
        for(int i = 0; i < numindices; ++i)
        {
            const mesh::tribb &tri = m.tribbs[indices[i]];
            ivec trimin = static_cast<ivec>(tri.center).sub(static_cast<ivec>(tri.radius)),
                 trimax = static_cast<ivec>(tri.center).add(static_cast<ivec>(tri.radius));
            if(i < left)
            {
                splitleft = std::max(splitleft, trimax[axis]);
                leftmin.min(trimin);
                leftmax.max(trimax);
            }
            else
            {
                splitright = std::min(splitright, trimin[axis]);
                rightmin.min(trimin);
                rightmax.max(trimax);
            }
        }
    }

    int offset = m.numnodes++;
    node &curnode = m.nodes[offset];
    curnode.split[0] = static_cast<short>(splitleft);
    curnode.split[1] = static_cast<short>(splitright);

    if(left==1)
    {
        curnode.child[0] = (axis<<14) | indices[0];
    }
    else
    {
        curnode.child[0] = (axis<<14) | (m.numnodes - offset);
        build(m, indices, left, leftmin, leftmax);
    }

    if(numindices-right==1)
    {
        curnode.child[1] = (1<<15) | (left==1 ? 1<<14 : 0) | indices[right];
    }
    else
    {
        curnode.child[1] = (left==1 ? 1<<14 : 0) | (m.numnodes - offset);
        build(m, &indices[right], numindices-right, rightmin, rightmax);
    }
}

BIH::BIH(const std::vector<mesh> &buildmeshes)
  : nodes(nullptr), numnodes(0), bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f), center(0, 0, 0), radius(0)
{
    mesh::tribb *tribbs = nullptr;
    int numtris = 0;
    if(buildmeshes.empty())
    {
        return;
    }
    for(const mesh &i : buildmeshes)
    {
        numtris += i.numtris;
    }
    if(!numtris)
    {
        return;
    }
    meshes.assign(buildmeshes.begin(), buildmeshes.end());
    tribbs = new mesh::tribb[numtris];
    mesh::tribb *dsttri = tribbs;
    for(mesh &m : meshes)
    {
        m.tribbs = dsttri;
        const mesh::tri *srctri = m.tris;
        vec mmin(1e16f, 1e16f, 1e16f), mmax(-1e16f, -1e16f, -1e16f);
        for(int j = 0; j < m.numtris; ++j)
        {
            vec s0 = m.getpos(srctri->vert[0]),
                s1 = m.getpos(srctri->vert[1]),
                s2 = m.getpos(srctri->vert[2]),
                v0 = m.xform.transform(s0),
                v1 = m.xform.transform(s1),
                v2 = m.xform.transform(s2),
                vmin = vec(v0).min(v1).min(v2),
                vmax = vec(v0).max(v1).max(v2);
            mmin.min(vmin);
            mmax.max(vmax);
            ivec imin = ivec::floor(vmin),
                 imax = ivec::ceil(vmax);
            dsttri->center = static_cast<svec>(static_cast<ivec>(imin).add(imax).div(2));
            dsttri->radius = static_cast<svec>(static_cast<ivec>(imax).sub(imin).add(1).div(2));
            ++srctri;
            ++dsttri;
        }
        for(int k = 0; k < 3; ++k)
        {
            if(std::fabs(mmax[k] - mmin[k]) < 0.125f)
            {
                float mid = (mmin[k] + mmax[k]) / 2;
                mmin[k] = mid - 0.0625f;
                mmax[k] = mid + 0.0625f;
            }
        }
        m.bbmin = mmin;
        m.bbmax = mmax;
        bbmin.min(mmin);
        bbmax.max(mmax);
    }

    center = vec(bbmin).add(bbmax).mul(0.5f);
    radius = vec(bbmax).sub(bbmin).mul(0.5f).magnitude();

    nodes = new node[numtris];
    node *curnode = nodes;
    uint *indices = new uint[numtris];
    for(mesh &m : meshes)
    {
        m.nodes = curnode;
        for(int j = 0; j < m.numtris; ++j)
        {
            indices[j] = j;
        }
        build(m, indices, m.numtris, ivec::floor(m.bbmin), ivec::ceil(m.bbmax));
        curnode += m.numnodes;
    }
    delete[] indices;
    numnodes = static_cast<int>(curnode - nodes);
}

BIH::~BIH()
{
    delete[] nodes;
}

bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist)
{
    model *m = loadmapmodel(e.attr1);
    if(!m)
    {
        return false;
    }
    if(mode&Ray_Shadow)
    {
        if(!m->shadow || e.flags&EntFlag_NoShadow)
        {
            return false;
        }
    }
    else if((mode&Ray_Ents)!=Ray_Ents && (!m->collide || e.flags&EntFlag_NoCollide))
    {
        return false;
    }
    if(!m->bih && !m->setBIH())
    {
        return false;
    }
    float scale = e.attr5 ? 100.0f/e.attr5 : 1.0f;
    vec mo = static_cast<vec>(o).sub(e.o).mul(scale), mray(ray);
    float v = mo.dot(mray),
          inside = m->bih->getentradius() - mo.squaredlen();
    if((inside < 0 && v > 0) || inside + v*v < 0)
    {
        return false;
    }
    int yaw   = e.attr2,
        pitch = e.attr3,
        roll  = e.attr4;
    //reorientation of rotated mmodels
    if(yaw != 0)
    {
        const vec2 &rot = sincosmod360(-yaw);
        mo.rotate_around_z(rot);
        mray.rotate_around_z(rot);
    }
    if(pitch != 0)
    {
        const vec2 &rot = sincosmod360(-pitch);
        mo.rotate_around_x(rot);
        mray.rotate_around_x(rot);
    }
    if(roll != 0)
    {
        const vec2 &rot = sincosmod360(roll);
        mo.rotate_around_y(-rot);
        mray.rotate_around_y(-rot);
    }
    if(m->bih->traverse(mo, mray, maxdist ? maxdist*scale : 1e16f, dist, mode))
    {
        dist /= scale;
        if(!(mode&Ray_Shadow))
        {
            //reorientation
            if(roll != 0)
            {
                hitsurface.rotate_around_y(sincosmod360(roll));
            }
            if(pitch != 0)
            {
                hitsurface.rotate_around_x(sincosmod360(pitch));
            }
            if(yaw != 0)
            {
                hitsurface.rotate_around_z(sincosmod360(yaw));
            }
        }
        return true;
    }
    return false;
}

static float segmentdistance(const vec &d1, const vec &d2, const vec &r)
{
    float a = d1.squaredlen(),
          e = d2.squaredlen(),
          f = d2.dot(r),
          s, t;
    if(a <= 1e-4f)
    {
        if(e <= 1e-4f)
        {
            return r.squaredlen();
        }
        s = 0;
        t = std::clamp(-f / e, 0.0f, 1.0f);
    }
    else
    {
        float c = d1.dot(r);
        if(e <= 1e-4f)
        {
            t = 0;
            s = std::clamp(c / a, 0.0f, 1.0f);
        }
        else
        {
            float b = d1.dot(d2),
                  denom = a*e - b*b;
            s = denom ? std::clamp((c*e - b*f) / denom, 0.0f, 1.0f) : 0.0f;
            t = b*s - f;
            if(t < 0)
            {
                t = 0;
                s = std::clamp(c / a, 0.0f, 1.0f);
            }
            else if(t > e)
            {
                t = 1;
                s = std::clamp((b + c) / a, 0.0f, 1.0f);
            }
            else
            {
                t /= e;
            }
        }
    }
    vec c1 = static_cast<vec>(d1).mul(s),
        c2 = static_cast<vec>(d2).mul(t);
    return vec(c2).sub(c1).add(r).squaredlen();
}

static float trisegmentdistance(const vec &a, const vec &b, const vec &c, const vec &p, const vec &q)
{
    //displacement vectors
    vec pq = vec(q).sub(p),
        ab = vec(b).sub(a),
        bc = vec(c).sub(b),
        ca = vec(a).sub(c),
        ap = vec(p).sub(a),
        bp = vec(p).sub(b),
        cp = vec(p).sub(c),
        aq = vec(q).sub(a),
        bq = vec(q).sub(b);
    vec n, nab, nbc, nca;
    n.cross(ab, bc);
    nab.cross(n, ab);
    nbc.cross(n, bc);
    nca.cross(n, ca);
    float dp = n.dot(ap),
          dq = n.dot(aq),
          dist;
    if(ap.dot(nab) < 0) // P outside AB
    {
        dist = segmentdistance(ab, pq, ap);
        if(bq.dot(nbc) < 0)
        {
            dist = std::min(dist, segmentdistance(bc, pq, bp)); // Q outside BC
        }
        else if(aq.dot(nca) < 0)
        {
            dist = std::min(dist, segmentdistance(pq, ca, cp)); // Q outside CA
        }
        else if(aq.dot(nab) >= 0)
        {
            dist = std::min(dist, dq*dq/n.squaredlen()); // Q inside AB
        }
        else
        {
            return dist;
        }
    }
    else if(bp.dot(nbc) < 0) // P outside BC
    {
        dist = segmentdistance(bc, pq, bp);
        if(aq.dot(nca) < 0)
        {
            dist = std::min(dist, segmentdistance(ca, pq, cp)); // Q outside CA
        }
        else if(aq.dot(nab) < 0)
        {
            dist = std::min(dist, segmentdistance(ab, pq, ap)); // Q outside AB
        }
        else if(bq.dot(nbc) >= 0)
        {
            dist = std::min(dist, dq*dq/n.squaredlen()); // Q inside BC
        }
        else
        {
            return dist;
        }
    }
    else if(cp.dot(nca) < 0) // P outside CA
    {
        dist = segmentdistance(ca, pq, cp);
        if(aq.dot(nab) < 0)
        {
            dist = std::min(dist, segmentdistance(ab, pq, ap)); // Q outside AB
        }
        else if(bq.dot(nbc) < 0)
        {
            dist = std::min(dist, segmentdistance(bc, pq, bp)); // Q outside BC
        }
        else if(aq.dot(nca) >= 0)
        {
            dist = std::min(dist, dq*dq/n.squaredlen()); // Q inside CA
        }
        else return dist;
    }
    else if(aq.dot(nab) < 0)
    {
        dist = std::min(segmentdistance(ab, pq, ap), dp); // Q outside AB
    }
    else if(bq.dot(nbc) < 0)
    {
        dist = std::min(segmentdistance(bc, pq, bp), dp); // Q outside BC
    }
    else if(aq.dot(nca) < 0)
    {
        dist = std::min(segmentdistance(ca, pq, cp), dp); // Q outside CA
    }
    else // both P and Q inside
    {
        if(dp > 0 ? dq <= 0 : dq >= 0)
        {
            return 0; // P and Q on different sides of triangle
        }
        dist = std::min(dp*dp, dq*dq)/n.squaredlen();
        return dist;
    }
    if(dp > 0 ? dq >= 0 : dq <= 0)
    {
        return dist; // both P and Q on same side of triangle
    }
    vec e = vec().cross(pq, ap);
    float det = std::fabs(dq - dp),
          v = ca.dot(e);
    if(v < 0 || v > det)
    {
        return dist;
    }
    float w = ab.dot(e);
    if(w < 0 || v + w > det)
    {
        return dist;
    }
    return 0; // segment intersects triangle
}

static bool triboxoverlap(const vec &radius, const vec &a, const vec &b, const vec &c)
{

    static auto testaxis = [] (const vec &v0, const vec &v1, const vec &v2,
                               const vec &e, const int &s, const int &t,
                               const vec &radius)
    {
        float p = v0[s]*v1[t] - v0[t]*v1[s],
              q = v2[s]*e[t] - v2[t]*e[s],
              r = radius[s]*std::fabs(e[t]) + radius[t]*std::fabs(e[s]);
        if(p < q)
        {
            if(q < -r || p > r)
            {
                return false;
            }
        }
        else if(p < -r || q > r)
        {
            return false;
        }
        return true;
    };

    static auto testface = [] (const vec &a,  const vec &b,  const vec &c,
                               const vec &ab, const vec &bc, const vec &ca,
                               uint axis, const vec &radius)
    {
        if(a.v[axis] < b.v[axis])
        {
            if(b.v[axis] < c.v[axis])
            {
                if(c.v[axis] < -radius.v[axis] || a.v[axis] > radius.v[axis])
                {
                    return false;
                }
            }
            else if(b.v[axis] < -radius.v[axis] || std::min(a.v[axis], c.v[axis]) > radius.v[axis])
            {
                return false;
            }
        }
        else if(a.v[axis] < c.v[axis])
        {
            if(c.v[axis] < -radius.v[axis] || b.v[axis] > radius.v[axis])
            {
                return false;
            }
        }
        else if(a.v[axis] < -radius.v[axis] || std::min(b.v[axis], c.v[axis]) > radius.v[axis])
        {
            return false;
        }
        return true;
    };

    vec ab = vec(b).sub(a),
        bc = vec(c).sub(b),
        ca = vec(a).sub(c);

    if(!testaxis(a, b, c, ab, 2, 1, radius)) {return false;};
    if(!testaxis(a, b, c, ab, 0, 2, radius)) {return false;};
    if(!testaxis(a, b, c, ab, 1, 0, radius)) {return false;};

    if(!testaxis(b, c, a, bc, 2, 1, radius)) {return false;};
    if(!testaxis(b, c, a, bc, 0, 2, radius)) {return false;};
    if(!testaxis(a, b, c, ab, 1, 0, radius)) {return false;};

    if(!testaxis(c, a, b, ca, 2, 1, radius)) {return false;};
    if(!testaxis(c, a, b, ca, 0, 2, radius)) {return false;};
    if(!testaxis(c, a, b, ca, 1, 0, radius)) {return false;};

    if(!testface(a, b, c, ab, bc, ca, 0, radius)) //x
    {
        return false;
    }
    else if(!testface(a, b, c, ab, bc, ca, 1, radius)) //y
    {
        return false;
    }
    else if(!testface(a, b, c, ab, bc, ca, 2, radius)) //z
    {
        return false;
    }
    return true;
}

//used in the tricollide templates below
//returns true if physent is a player and passed vec is close enough to matter (determined by radius,pdist)
bool BIH::playercollidecheck(const physent *d, float pdist, vec dir, vec n, vec radius) const
{
    float a = 2*radius.z*(d->zmargin/(d->aboveeye+d->eyeheight)-(dir.z < 0 ? 1/3.0f : 1/4.0f)),
          b = (dir.x*n.x < 0 || dir.y*n.y < 0 ? -radius.x : 0);
    if(d->type==physent::PhysEnt_Player)
    {
        if(pdist < (dir.z*n.z < 0 ? a : b))
        {
            return true;
        }
    }
    return false;
}

template<>
void BIH::tricollide<Collide_Ellipse>(const mesh &m, int tidx, const physent *d, const vec &dir, float cutoff, const vec &, const vec &radius, const matrix4x3 &orient, float &dist, const ivec &bo, const ivec &br) const
{
    if(m.tribbs[tidx].outside(bo, br))
    {
        return;
    }
    const mesh::tri &t = m.tris[tidx];
    vec a = m.getpos(t.vert[0]),
        b = m.getpos(t.vert[1]),
        c = m.getpos(t.vert[2]),
        zdir = vec(orient.rowz()).mul((radius.z - radius.x)/(m.scale()*m.scale()));
    if(trisegmentdistance(a, b, c, vec(center).sub(zdir), vec(center).add(zdir)) > (radius.x*radius.x)/(m.scale()*m.scale()))
    {
        return;
    }
    vec n;
    n.cross(a, b, c).normalize();
    float pdist = (n.dot(vec(center).sub(a)) - std::fabs(n.dot(zdir)))*m.scale() - radius.x;
    if(pdist > 0 || pdist <= dist)
    {
        return;
    }
    collideinside = true;
    n = orient.transformnormal(n).div(m.scale());
    if(!dir.iszero())
    {
        if(n.dot(dir) >= -cutoff*dir.magnitude())
        {
            return;
        }
        //see playercollidecheck defined above
        if(playercollidecheck(d, pdist, dir, n, radius))
        {
            return;
        }
    }
    dist = pdist;
    collidewall = n;
}

template<>
void BIH::tricollide<Collide_OrientedBoundingBox>(const mesh &m, int tidx, const physent *d, const vec &dir, float cutoff, const vec &, const vec &radius, const matrix4x3 &orient, float &dist, const ivec &bo, const ivec &br) const
{
    if(m.tribbs[tidx].outside(bo, br))
    {
        return;
    }
    const mesh::tri &t = m.tris[tidx];
    vec a = orient.transform(m.getpos(t.vert[0])),
        b = orient.transform(m.getpos(t.vert[1])),
        c = orient.transform(m.getpos(t.vert[2]));
    if(!triboxoverlap(radius, a, b, c))
    {
        return;
    }
    vec n;
    n.cross(a, b, c).normalize();
    float pdist = -n.dot(a),
          r = radius.absdot(n);
    if(std::fabs(pdist) > r)
    {
        return;
    }
    pdist -= r;
    if(pdist <= dist)
    {
        return;
    }
    collideinside = true;
    if(!dir.iszero())
    {
        if(n.dot(dir) >= -cutoff*dir.magnitude())
        {
            return;
        }
        if(playercollidecheck(d, pdist, dir, n, radius))
        {
            return;
        }
    }
    dist = pdist;
    collidewall = n;
}

template<int C>
void BIH::collide(const mesh &m, const physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, node *curnode, const ivec &bo, const ivec &br) const
{
    node *stack[128];
    int stacksize = 0;
    ivec bmin = ivec(bo).sub(br),
         bmax = ivec(bo).add(br);
    for(;;)
    {
        int axis = curnode->axis();
        const int nearidx = 0,
                  faridx = nearidx^1;
        int nearsplit = bmin[axis] - curnode->split[nearidx],
            farsplit = curnode->split[faridx] - bmax[axis];

        if(nearsplit > 0)
        {
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    continue;
                }
                else
                {
                    tricollide<C>(m, curnode->childindex(faridx), d, dir, cutoff, center, radius, orient, dist, bo, br);
                }
            }
        }
        else if(curnode->isleaf(nearidx))
        {
            tricollide<C>(m, curnode->childindex(nearidx), d, dir, cutoff, center, radius, orient, dist, bo, br);
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    continue;
                }
                else
                {
                    tricollide<C>(m, curnode->childindex(faridx), d, dir, cutoff, center, radius, orient, dist, bo, br);
                }
            }
        }
        else
        {
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    if(stacksize < static_cast<int>(sizeof(stack)/sizeof(stack[0])))
                    {
                        stack[stacksize++] = curnode + curnode->childindex(faridx);
                    }
                    else
                    {
                        collide<C>(m, d, dir, cutoff, center, radius, orient, dist, &nodes[curnode->childindex(nearidx)], bo, br);
                        curnode += curnode->childindex(faridx);
                        continue;
                    }
                }
                else
                {
                    tricollide<C>(m, curnode->childindex(faridx), d, dir, cutoff, center, radius, orient, dist, bo, br);
                }
            }
            curnode += curnode->childindex(nearidx);
            continue;
        }
        if(stacksize <= 0)
        {
            return;
        }
        curnode = stack[--stacksize];
    }
}

bool BIH::ellipsecollide(const physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale) const
{
    if(!numnodes)
    {
        return false;
    }
    vec center(d->o.x, d->o.y, d->o.z + 0.5f*(d->aboveeye - d->eyeheight)),
        radius(d->radius, d->radius, 0.5f*(d->eyeheight + d->aboveeye));
    center.sub(o);
    if(scale != 1)
    {
        float invscale = 1/scale;
        center.mul(invscale);
        radius.mul(invscale);
    }
    matrix3 orient;
    orient.identity();
    if(yaw)
    {
        orient.rotate_around_z(sincosmod360(yaw));
    }
    if(pitch)
    {
        orient.rotate_around_x(sincosmod360(pitch));
    }
    if(roll)
    {
        orient.rotate_around_y(sincosmod360(-roll));
    }
    vec bo = orient.transposedtransform(center),
        br = orient.abstransposedtransform(radius);
    if(bo.x + br.x < bbmin.x || bo.y + br.y < bbmin.y || bo.z + br.z < bbmin.z ||
       bo.x - br.x > bbmax.x || bo.y - br.y > bbmax.y || bo.z - br.z > bbmax.z)
    {
        return false;
    }
    ivec imin = ivec::floor(vec(bo).sub(br)),
         imax = ivec::ceil(vec(bo).add(br)),
         icenter = imin.add(imax).div(2),
         iradius = imax.sub(imin).add(1).div(2);

    float dist = -1e10f;
    for(const mesh &m : meshes)
    {
        if(!(m.flags&Mesh_Collide) || m.flags&Mesh_NoClip)
        {
            continue;
        }
        matrix4x3 morient;
        morient.mul(orient, m.xform);
        collide<Collide_Ellipse>(m, d, dir, cutoff, m.invxform().transform(bo), radius, morient, dist, m.nodes, icenter, iradius);
    }
    return dist > maxcollidedistance;
}

bool BIH::boxcollide(const physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale) const
{
    if(!numnodes)
    {
        return false;
    }
    vec center(d->o.x, d->o.y, d->o.z + 0.5f*(d->aboveeye - d->eyeheight)),
        radius(d->xradius, d->yradius, 0.5f*(d->eyeheight + d->aboveeye));
    center.sub(o);
    if(scale != 1)
    {
        float invscale = 1/scale;
        center.mul(invscale);
        radius.mul(invscale);
    }
    matrix3 orient;
    orient.identity();
    if(yaw)
    {
        orient.rotate_around_z(sincosmod360(yaw));
    }
    if(pitch)
    {
        orient.rotate_around_x(sincosmod360(pitch));
    }
    if(roll)
    {
        orient.rotate_around_y(sincosmod360(-roll));
    }
    vec bo = orient.transposedtransform(center),
        br = orient.abstransposedtransform(vec(d->radius, d->radius, radius.z));
    if(bo.x + br.x < bbmin.x || bo.y + br.y < bbmin.y || bo.z + br.z < bbmin.z ||
       bo.x - br.x > bbmax.x || bo.y - br.y > bbmax.y || bo.z - br.z > bbmax.z)
    {
        return false;
    }
    ivec imin = ivec::floor(vec(bo).sub(br)),
         imax = ivec::ceil(vec(bo).add(br)),
         icenter = ivec(imin).add(imax).div(2),
         iradius = ivec(imax).sub(imin).add(1).div(2);
    matrix3 drot, dorient;
    drot.setyaw(d->yaw/RAD);
    vec ddir = drot.transform(dir),
        dcenter = drot.transform(center).neg();
    dorient.mul(drot, orient);
    float dist = -1e10f;
    for(const mesh &m : meshes)
    {
        if(!(m.flags&Mesh_Collide) || m.flags&Mesh_NoClip)
        {
            continue;
        }
        matrix4x3 morient;
        morient.mul(dorient, dcenter, m.xform);
        collide<Collide_OrientedBoundingBox>(m, d, ddir, cutoff, center, radius, morient, dist, m.nodes, icenter, iradius);
    }
    if(dist > maxcollidedistance)
    {
        collidewall = drot.transposedtransform(collidewall);
        return true;
    }
    return false;
}

void BIH::genstaintris(std::vector<std::array<vec, 3>> &tris, const mesh &m, int tidx, const vec &, float, const matrix4x3 &orient, const ivec &bo, const ivec &br) const
{
    if(m.tribbs[tidx].outside(bo, br))
    {
        return;
    }
    const mesh::tri &t = m.tris[tidx];
    std::array<vec, 3> v =
    {
        orient.transform(m.getpos(t.vert[0])),
        orient.transform(m.getpos(t.vert[1])),
        orient.transform(m.getpos(t.vert[2]))
    };
    tris.push_back(v);
}

void BIH::genstaintris(std::vector<std::array<vec, 3>> &tris, const mesh &m, const vec &center, float radius, const matrix4x3 &orient, node *curnode, const ivec &bo, const ivec &br) const
{
    std::stack<node *> stack;
    ivec bmin = static_cast<ivec>(bo).sub(br),
         bmax = static_cast<ivec>(bo).add(br);
    for(;;)
    {
        int axis = curnode->axis();
        constexpr int nearidx = 0,
                      faridx = nearidx ^ 1; //xor last bit
        int nearsplit = bmin[axis] - curnode->split[nearidx],
            farsplit = curnode->split[faridx] - bmax[axis];
        if(nearsplit > 0)
        {
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    continue;
                }
                else
                {
                    genstaintris(tris, m, curnode->childindex(faridx), center, radius, orient, bo, br);
                }
            }
        }
        else if(curnode->isleaf(nearidx))
        {
            genstaintris(tris, m, curnode->childindex(nearidx), center, radius, orient, bo, br);
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    continue;
                }
                else
                {
                    genstaintris(tris, m, curnode->childindex(faridx), center, radius, orient, bo, br);
                }
            }
        }
        else
        {
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    if(stack.size() < 128)
                    {
                        stack.push(curnode + curnode->childindex(faridx));
                    }
                    else
                    {
                        genstaintris(tris, m, center, radius, orient, &nodes[curnode->childindex(nearidx)], bo, br);
                        curnode += curnode->childindex(faridx);
                        continue;
                    }
                }
                else
                {
                    genstaintris(tris, m, curnode->childindex(faridx), center, radius, orient, bo, br);
                }
            }
            curnode += curnode->childindex(nearidx);
            continue;
        }
        if(stack.size() <= 0)
        {
            return;
        }
        curnode = stack.top();
        stack.pop();
    }
}

void BIH::genstaintris(std::vector<std::array<vec, 3>> &tris, const vec &staincenter, float stainradius, const vec &o, int yaw, int pitch, int roll, float scale) const
{
    if(!numnodes)
    {
        return;
    }
    vec center = vec(staincenter).sub(o);
    float radius = stainradius;
    if(scale != 1)
    {
        float invscale = 1/scale;
        center.mul(invscale);
        radius *= invscale;
    }
    matrix3 orient;
    orient.identity();
    //reorientation
    if(yaw)
    {
        orient.rotate_around_z(sincosmod360(yaw));
    }
    if(pitch)
    {
        orient.rotate_around_x(sincosmod360(pitch));
    }
    if(roll)
    {
        orient.rotate_around_y(sincosmod360(-roll));
    }
    vec bo = orient.transposedtransform(center);
    if(bo.x + radius < bbmin.x || bo.y + radius < bbmin.y || bo.z + radius < bbmin.z ||
       bo.x - radius > bbmax.x || bo.y - radius > bbmax.y || bo.z - radius > bbmax.z)
    {
        return;
    }
    orient.scale(scale);
    ivec imin = ivec::floor(vec(bo).sub(radius)),
         imax = ivec::ceil(vec(bo).add(radius)),
         icenter = ivec(imin).add(imax).div(2),
         iradius = ivec(imax).sub(imin).add(1).div(2);
    for(const mesh &m : meshes)
    {
        if(!(m.flags&Mesh_Render) || m.flags&Mesh_Alpha)
        {
            continue;
        }
        matrix4x3 morient;
        morient.mul(orient, o, m.xform);
        genstaintris(tris, m, m.invxform().transform(bo), radius, morient, m.nodes, icenter, iradius);
    }
}

float BIH::getentradius() const
{
    return std::max(bbmin.squaredlen(), bbmax.squaredlen());
}

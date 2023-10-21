/*hitzone.cpp: implementations of hitbox functionality exposed in hitzone.h
 *
 * hitzone implements the mechanics required to perform collision calculations between
 * skeletal models in arbitrary positions and the surrounding geometry (or projectiles)
 *
 * hitzone implements part of skelmodel's functionality, but is separated as it
 * implements a seperate class of functionality; skeletal models not using collision
 * will not use hitzone functions.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"
#include "../../shared/hashtable.h"

#include <optional>

#include "render/rendergl.h"
#include "render/rendermodel.h"
#include "render/shader.h"
#include "render/shaderparam.h"
#include "render/texture.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/cs.h"

#include "world/entities.h"
#include "world/octaworld.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"
#include "animmodel.h"
#include "vertmodel.h"
#include "skelmodel.h"
#include "hitzone.h"

class skelzonebounds
{
    public:
        skelzonebounds();
        int owner;

        bool empty() const;
        vec calccenter() const;
        void addvert(const vec &p);
        float calcradius() const;
    private:
        vec bbmin, bbmax;
};

template<>
struct std::hash<skelzonekey>
{
    size_t operator()(const skelzonekey &k) const
    {
        union
        {
            uint i[3];
            uchar b[12];
        } conv;
        std::memcpy(conv.b, k.bones.data(), sizeof(conv.b));
        return conv.i[0]^conv.i[1]^conv.i[2];
    }
};

//gets used just twice, in skelhitdata::skelbih::triintersect, skelhitdata::skelhitzone::triintersect
static bool skeltriintersect(vec a, vec b, vec c, vec o,
                                    const animmodel::skin* s,
                                    const skelhittri t,
                                    const skelmodel::vert va,
                                    const skelmodel::vert vb,
                                    const skelmodel::vert vc,
                                    const skelmodel::skelmesh* tm,
                                    vec ray)
{
    vec eb = vec(b).sub(a), //eb,ec are displacement vectors for b,c from a
        ec = vec(c).sub(a);
    vec p;
    p.cross(ray, ec); //p is the cross product of impinging ray and ec displacement vector
    float det = eb.dot(p); // eb * p being zero implies that eb is normal to p
    if(det == 0)
    {
        return false;
    }
    vec r = vec(o).sub(a); // r is displacement between origin and the "a" vertex
    float u = r.dot(p) / det;
    if(u < 0 || u > 1) //u < 0 implies r * p is negative: r faces away from p
    {
        return false;
    }
    vec q;
    q.cross(r, eb);
    float v = ray.dot(q) / det;
    if(v < 0 || u + v > 1) // v < 0 implies ray * q is negative: ray faces away from q
    {
        return false;
    }
    float f = ec.dot(q) / det;
    if(f < 0 || f*skelmodel::intersectscale > skelmodel::intersectdist)
    {
        return false;
    }
    if(!(skelmodel::intersectmode&Ray_Shadow) && tm->noclip)
    {
        return false;
    }
    if((skelmodel::intersectmode&Ray_AlphaPoly)==Ray_AlphaPoly)
    {
        Texture *tex = s[t.Mesh].tex;
        if(tex->type&Texture::ALPHA && (tex->alphamask || tex->loadalphamask()))
        {
            int si = std::clamp(static_cast<int>(tex->xs * (va.tc.x + u*(vb.tc.x - va.tc.x) + v*(vc.tc.x - va.tc.x))), 0, tex->xs-1),
                ti = std::clamp(static_cast<int>(tex->ys * (va.tc.y + u*(vb.tc.y - va.tc.y) + v*(vc.tc.y - va.tc.y))), 0, tex->ys-1);
            if(!(tex->alphamask[ti*((tex->xs+7)/8) + si/8] & (1<<(si%8))))
            {
                return false;
            }
        }
    }
    skelmodel::intersectdist = f*skelmodel::intersectscale;
    skelmodel::intersectresult = t.id&0x80 ? -1 : t.id;
    return true;
}

int skelhitdata::skelbih::node::axis() const
{
    return child[0]>>14;
}

int skelhitdata::skelbih::node::childindex(int which) const
{
    return child[which]&0x3FFF;
}

bool skelhitdata::skelbih::node::isleaf(int which) const
{
    return (child[1]&(1<<(14+which)))!=0;
}

vec skelhitdata::skelbih::calccenter() const
{
    return vec(bbmin).add(bbmax).mul(0.5f);
}

float skelhitdata::skelbih::calcradius() const
{
    return vec(bbmax).sub(bbmin).mul(0.5f).magnitude();
}

bool skelhitdata::skelbih::triintersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, int tidx, const vec &o, const vec &ray) const
{
    const skelhittri &t = tris[tidx];
    skelmodel::skelmesh *tm = static_cast<skelmodel::skelmesh *>(m->meshes[t.Mesh]);
    const skelmodel::vert &va = tm->verts[t.vert[0]],
                          &vb = tm->verts[t.vert[1]],
                          &vc = tm->verts[t.vert[2]];
    return skeltriintersect(va.pos, vb.pos, vc.pos, o, s, t, va, vb, vc, tm, ray);
}

void skelhitdata::skelbih::intersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, const vec &o, const vec &ray, const vec &invray, node *curnode, float smin, float smax) const
{
    skelbihstack stack[128];
    int stacksize = 0;
    ivec order(ray.x>0 ? 0 : 1, ray.y>0 ? 0 : 1, ray.z>0 ? 0 : 1);
    float tmin = smin,
          tmax = smax;
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
                else if(triintersect(m, s, curnode->childindex(faridx), o, ray))
                {
                    smax = std::min(smax, skelmodel::intersectdist/skelmodel::intersectscale);
                }
            }
        }
        else if(curnode->isleaf(nearidx))
        {
            if(triintersect(m, s, curnode->childindex(nearidx), o, ray))
            {
                smax = std::min(smax, skelmodel::intersectdist/skelmodel::intersectscale);
                tmax = std::min(tmax, smax);
            }
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    tmin = std::max(tmin, farsplit);
                    continue;
                }
                else if(triintersect(m, s, curnode->childindex(faridx), o, ray))
                {
                    smax = std::min(smax, skelmodel::intersectdist/skelmodel::intersectscale);
                }
            }
        }
        else
        {
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    if(stacksize < static_cast<int>(sizeof(stack)/sizeof(stack[0])))
                    {
                        skelbihstack &save = stack[stacksize++];
                        save.node = curnode + curnode->childindex(faridx);
                        save.tmin = std::max(tmin, farsplit);
                        save.tmax = tmax;
                    }
                    else
                    {
                        intersect(m, s, o, ray, invray, curnode + curnode->childindex(nearidx), tmin, tmax);
                        curnode += curnode->childindex(faridx);
                        tmin = std::max(tmin, farsplit);
                    }
                }
                else if(triintersect(m, s, curnode->childindex(faridx), o, ray))
                {
                    smax = std::min(smax, skelmodel::intersectdist/skelmodel::intersectscale);
                    tmax = std::min(tmax, smax);
                }
            }
            curnode += curnode->childindex(nearidx);
            tmax = std::min(tmax, nearsplit);
            continue;
        }
        if(stacksize <= 0)
        {
            return;
        }
        skelbihstack &restore = stack[--stacksize];
        curnode = restore.node;
        tmin = restore.tmin;
        tmax = std::min(restore.tmax, smax);
    }
}

void skelhitdata::skelbih::intersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, const vec &o, const vec &ray)
{
    vec invray(ray.x ? 1/ray.x : 1e16f, ray.y ? 1/ray.y : 1e16f, ray.z ? 1/ray.z : 1e16f);
    float tmin, tmax,
          t1 = (bbmin.x - o.x)*invray.x,
          t2 = (bbmax.x - o.x)*invray.x;
    if(invray.x > 0)
    {
        tmin = t1; tmax = t2;
    }
    else
    {
        tmin = t2;
        tmax = t1;
    }
    t1 = (bbmin.y - o.y)*invray.y;
    t2 = (bbmax.y - o.y)*invray.y;
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
    t1 = (bbmin.z - o.z)*invray.z;
    t2 = (bbmax.z - o.z)*invray.z;
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
    tmax = std::min(tmax, skelmodel::intersectdist/skelmodel::intersectscale);
    if(tmin >= tmax)
    {
        return;
    }
    if(nodes)
    {
        intersect(m, s, o, ray, invray, nodes, tmin, tmax);
    }
    else
    {
        triintersect(m, s, 0, o, ray);
    }
}

void skelhitdata::skelbih::build(const skelmodel::skelmeshgroup *m, ushort *indices, int numindices, const vec &vmin, const vec &vmax)
{
    int axis = 2;
    for(int k = 0; k < 2; ++k)
    {
        if(vmax[k] - vmin[k] > vmax[axis] - vmin[axis])
        {
            axis = k;
        }
    }

    vec leftmin, leftmax, rightmin, rightmax;
    float splitleft, splitright;
    int left, right;
    for(int k = 0; k < 3; ++k)
    {
        leftmin = rightmin = vec(1e16f, 1e16f, 1e16f);
        leftmax = rightmax = vec(-1e16f, -1e16f, -1e16f);
        float split = 0.5f*(vmax[axis] + vmin[axis]);
        for(left = 0, right = numindices, splitleft = SHRT_MIN, splitright = SHRT_MAX; left < right;)
        {
            const skelhittri &tri = tris[indices[left]];
            const skelmodel::skelmesh *tm = static_cast<skelmodel::skelmesh *>(m->meshes[tri.Mesh]);
            const vec &ta = tm->verts[tri.vert[0]].pos,
                      &tb = tm->verts[tri.vert[1]].pos,
                      &tc = tm->verts[tri.vert[2]].pos;
            vec trimin = vec(ta).min(tb).min(tc),
                trimax = vec(ta).max(tb).max(tc);
            float amin = trimin[axis],
                  amax = trimax[axis];
            if(std::max(split - amin, 0.0f) > std::max(amax - split, 0.0f))
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
        leftmin = rightmin = vec(1e16f, 1e16f, 1e16f);
        leftmax = rightmax = vec(-1e16f, -1e16f, -1e16f);
        left = right = numindices/2;
        splitleft = SHRT_MIN;
        splitright = SHRT_MAX;
        for(int i = 0; i < numindices; ++i)
        {
            const skelhittri &tri = tris[indices[i]];
            const skelmodel::skelmesh *tm = static_cast<skelmodel::skelmesh *>(m->meshes[tri.Mesh]);
            const vec &ta = tm->verts[tri.vert[0]].pos,
                      &tb = tm->verts[tri.vert[1]].pos,
                      &tc = tm->verts[tri.vert[2]].pos;
            vec trimin = vec(ta).min(tb).min(tc),
                trimax = vec(ta).max(tb).max(tc);
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
    int offset = numnodes++;
    node &curnode = nodes[offset];
    curnode.split[0] = static_cast<short>(std::ceil(splitleft));
    curnode.split[1] = static_cast<short>(std::floor(splitright));
    if(left==1)
    {
        curnode.child[0] = (axis<<14) | indices[0];
    }
    else
    {
        curnode.child[0] = (axis<<14) | (numnodes - offset);
        build(m, indices, left, leftmin, leftmax);
    }

    if(numindices-right==1)
    {
        curnode.child[1] = (1<<15) | (left==1 ? 1<<14 : 0) | indices[right];
    }
    else
    {
        curnode.child[1] = (left==1 ? 1<<14 : 0) | (numnodes - offset);
        build(m, &indices[right], numindices-right, rightmin, rightmax);
    }
}

skelhitdata::skelbih::skelbih(const skelmodel::skelmeshgroup *m, int numtris, const skelhittri *tris)
  : nodes(nullptr), numnodes(0), tris(tris), bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f)
{
    for(int i = 0; i < numtris; ++i)
    {
        const skelhittri &tri = tris[i];
        const skelmodel::skelmesh *tm = static_cast<skelmodel::skelmesh *>(m->meshes[tri.Mesh]);
        const vec &ta = tm->verts[tri.vert[0]].pos,
                  &tb = tm->verts[tri.vert[1]].pos,
                  &tc = tm->verts[tri.vert[2]].pos;
        bbmin.min(ta).min(tb).min(tc);
        bbmax.max(ta).max(tb).max(tc);
    }
    if(numtris > 1)
    {
        nodes = new node[numtris];
        ushort *indices = new ushort[numtris];
        for(int i = 0; i < numtris; ++i)
        {
            indices[i] = i;
        }
        build(m, indices, numtris, bbmin, bbmax);
        delete[] indices;
    }
}

//skelhitzone

skelhitdata::skelhitzone::skelhitzone() : numparents(0), numchildren(0), parents(nullptr), children(nullptr), center(0, 0, 0), radius(0), visited(-1), animcenter(0, 0, 0)
{
    blend = -1;
    bih = nullptr;
}

skelhitdata::skelhitzone::~skelhitzone()
{
    if(!numchildren)
    {
        if(bih)
        {
            delete bih;
            bih = nullptr;
        }
    }
    else
    {
        delete[] tris;
    }
}

void skelhitdata::skelhitzone::intersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, const dualquat *bdata1, const dualquat *bdata2, int numblends, const vec &o, const vec &ray)
{
    if(!numchildren)
    {
        if(bih)
        {
            const dualquat &b = blend < numblends ? bdata2[blend] : bdata1[m->skel->bones[blend - numblends].interpindex];
            vec bo = b.transposedtransform(o),
                bray = b.transposedtransformnormal(ray);
            bih->intersect(m, s, bo, bray);
        }
    }
    else if(shellintersect(o, ray))
    {
        for(int i = 0; i < numtris; ++i)
        {
            triintersect(m, s, bdata1, bdata2, numblends, tris[i], o, ray);
        }
        for(int i = 0; i < numchildren; ++i)
        {
            if(children[i]->visited != visited)
            {
                children[i]->visited = visited;
                children[i]->intersect(m, s, bdata1, bdata2, numblends, o, ray);
            }
        }
    }
}

void skelhitdata::skelhitzone::propagate(const skelmodel::skelmeshgroup *m, const dualquat *bdata1, const dualquat *bdata2, int numblends)
{
    if(!numchildren)
    {
        const dualquat &b = blend < numblends ? bdata2[blend] : bdata1[m->skel->bones[blend - numblends].interpindex];
        animcenter = b.transform(center);
    }
    else
    {
        animcenter = children[numchildren-1]->animcenter;
        radius = children[numchildren-1]->radius;
        for(int i = 0; i < numchildren-1; ++i)
        {
            const skelhitzone *child = children[i];
            vec n = child->animcenter;
            n.sub(animcenter);
            float dist = n.magnitude();
            if(child->radius >= dist + radius)
            {
                animcenter = child->animcenter;
                radius = child->radius;
            }
            else if(radius < dist + child->radius)
            {
                float newradius = 0.5f*(radius + dist + child->radius);
                animcenter.add(n.mul((newradius - radius)/dist));
                radius = newradius;
            }
        }
    }
}

bool skelhitdata::skelhitzone::triintersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, const dualquat *bdata1, const dualquat *bdata2, int numblends, const skelhittri &t, const vec &o, const vec &ray)
{
    skelmodel::skelmesh *tm = static_cast<skelmodel::skelmesh *>(m->meshes[t.Mesh]);
    const skelmodel::vert &va = tm->verts[t.vert[0]],
                          &vb = tm->verts[t.vert[1]],
                          &vc = tm->verts[t.vert[2]];
    vec a = (va.blend < numblends ? bdata2[va.blend] : bdata1[m->blendcombos[va.blend].bonedata[0].interpbones]).transform(va.pos),
        b = (vb.blend < numblends ? bdata2[vb.blend] : bdata1[m->blendcombos[vb.blend].bonedata[0].interpbones]).transform(vb.pos),
        c = (vc.blend < numblends ? bdata2[vc.blend] : bdata1[m->blendcombos[vc.blend].bonedata[0].interpbones]).transform(vc.pos);
    return skeltriintersect(a, b, c, o, s, t, va, vb, vc, tm, ray);
}

bool skelhitdata::skelhitzone::shellintersect(const vec &o, const vec &ray)
{
    vec c(animcenter);
    c.sub(o);
    float v = c.dot(ray),
          inside = radius*radius - c.squaredlen();
    if(inside < 0 && v < 0)
    {
        return false;
    }
    float d = inside + v*v;
    if(d < 0)
    {
        return false;
    }
    v -= skelmodel::intersectdist/skelmodel::intersectscale;
    return v < 0 || d >= v*v;
}

//skelzonekey

skelzonekey::skelzonekey() : blend(-1)
{
    bones.fill(0xFF);
}

skelzonekey::skelzonekey(int bone) : blend(INT_MAX)
{
    bones.fill(0xFF);
    bones[0] = bone;
}

skelzonekey::skelzonekey(const skelmodel::skelmesh *m, const skelmodel::tri &t) : blend(-1)
{
    bones.fill(0xFF);
    addbones(m, t);
}

bool skelzonekey::includes(const skelzonekey &o) const
{
    size_t j = 0;
    for(size_t i = 0; i < sizeof(bones); ++i)
    {
        if(bones[i] > o.bones[j])
        {
            return false;
        }
        if(bones[i] == o.bones[j])
        {
            j++;
        }
    }
    return j < sizeof(bones) ? o.bones[j] == 0xFF : blend < 0 || blend == o.blend;
}

void skelzonekey::subtract(const skelzonekey &o)
{
    size_t len = 0,
           j   = 0;
    for(size_t i = 0; i < sizeof(bones); ++i)
    {
    retry:
        if(j >= sizeof(o.bones) || bones[i] < o.bones[j])
        {
            bones[len++] = bones[i];
            continue;
        }
        if(bones[i] == o.bones[j])
        {
            j++;
            continue;
        }
        do
        {
            j++;
        } while(j < sizeof(o.bones) && bones[i] > o.bones[j]);
        goto retry;
    }
    std::memset(&bones[len], 0xFF, sizeof(bones) - len);
}

bool skelzonekey::hasbone(int n) const
{
    for(size_t i = 0; i < sizeof(bones); ++i)
    {
        if(bones[i] == n)
        {
            return true;
        }
        if(bones[i] == 0xFF)
        {
            break;
        }
    }
    return false;
}

void skelzonekey::addbone(int n)
{
    for(uint i = 0; i < sizeof(bones); ++i)
    {
        if(n <= bones[i])
        {
            if(n < bones[i])
            {
                std::memmove(&bones[i+1], &bones[i], sizeof(bones) - (i+1));
                bones[i] = n;
            }
            return;
        }
    }
}

void skelzonekey::addbones(const skelmodel::skelmesh *m, const skelmodel::tri &t)
{
    const skelmodel::skelmeshgroup *g = reinterpret_cast<skelmodel::skelmeshgroup *>(m->group);
    int b0 = m->verts[t.vert[0]].blend,
        b1 = m->verts[t.vert[1]].blend,
        b2 = m->verts[t.vert[1]].blend;
    for(const skelmodel::blendcombo::BoneData &b : g->blendcombos[b0].bonedata)
    {
        if(b.weights)
        {
            addbone(b.bones);
        }
    }
    if(b0 != b1 || b0 != b2)
    {
        for(const skelmodel::blendcombo::BoneData &b : g->blendcombos[b1].bonedata)
        {
            if(b.bones)
            {
                addbone(b.bones);
            }
        }
        for(const skelmodel::blendcombo::BoneData &b : g->blendcombos[b2].bonedata)
        {
            if(b.weights)
            {
                addbone(b.bones);
            }
        }
    }
    else
    {
        blend = b0;
    }
}

bool skelzonekey::operator==(const skelzonekey &k) const
{
    return blend == k.blend && bones == k.bones;
}

//skelzonebounds

skelzonebounds::skelzonebounds() : owner(-1), bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f)
{
}

bool skelzonebounds::empty() const
{
    return bbmin.x > bbmax.x;
}

vec skelzonebounds::calccenter() const
{
    return vec(bbmin).add(bbmax).mul(0.5f);
}

void skelzonebounds::addvert(const vec &p)
{
    bbmin.x = std::min(bbmin.x, p.x);
    bbmin.y = std::min(bbmin.y, p.y);
    bbmin.z = std::min(bbmin.z, p.z);
    bbmax.x = std::max(bbmax.x, p.x);
    bbmax.y = std::max(bbmax.y, p.y);
    bbmax.z = std::max(bbmax.z, p.z);
}

float skelzonebounds::calcradius() const
{
    return vec(bbmax).sub(bbmin).mul(0.5f).magnitude();
}

//skelhitdata

skelhitdata::skelhitdata() : numblends(0), numzones(0), rootzones(0), visited(0), zones(nullptr), links(nullptr), tris(nullptr)
{
}

skelhitdata::~skelhitdata()
{
    delete[] zones;
    delete[] links;
    delete[] tris;
    delete[] blendcache.bdata;
}

int skelhitdata::getblendcount()
{
    return numblends;
}

skelmodel::blendcacheentry &skelhitdata::getcache()
{
    return blendcache;
}

void skelhitdata::propagate(const skelmodel::skelmeshgroup *m, const dualquat *bdata1, dualquat *bdata2)
{
    visited = 0;
    for(int i = 0; i < numzones; ++i)
    {
        zones[i].visited = -1;
        zones[i].propagate(m, bdata1, bdata2, numblends);
    }
}

void skelhitdata::cleanup()
{
    blendcache.owner = -1;
}

void skelhitdata::intersect(const skelmodel::skelmeshgroup *m, skelmodel::skin *s, const dualquat *bdata1, dualquat *bdata2, const vec &o, const vec &ray)
{
    if(++visited < 0)
    {
        visited = 0;
        for(int i = 0; i < numzones; ++i)
        {
            zones[i].visited = -1;
        }
    }
    for(int i = numzones - rootzones; i < numzones; i++)
    {
        zones[i].visited = visited;
        zones[i].intersect(m, s, bdata1, bdata2, numblends, o, ray);
    }
}

uchar skelhitdata::chooseid(const skelmodel::skelmeshgroup *g, const skelmodel::skelmesh *m, const skelmodel::tri &t, const uchar *ids)
{
    size_t numused = 0;

    std::array<std::pair<uchar, float>, 12> weights;
    for(size_t i = 0; i < 3; ++i)
    {
        const skelmodel::vert &v = m->verts[t.vert[i]];
        const skelmodel::blendcombo &c = g->blendcombos[v.blend];
        for(size_t j = 0; j < c.bonedata.size(); ++j)
        {
            if(c.bonedata[j].weights)
            {
                uchar id = ids[c.bonedata[j].bones];
                for(size_t k = 0; k < numused; ++k)
                {
                    if(weights[k].first == id)
                    {
                        weights[k].second += c.bonedata[j].weights;
                        goto nextbone;
                    }
                }
                weights[numused].first = id;
                weights[numused].second = c.bonedata[j].weights;
                numused++;
            nextbone:;
            }
        }
    }
    uchar bestid = 0xFF;
    float bestweight = 0;
    for(size_t i = 0; i < numused; ++i)
    {
        if(weights[i].second > bestweight || (weights[i].second == bestweight && weights[i].first < bestid))
        {
            bestid = weights[i].first;
            bestweight = weights[i].second;
        }
    }
    return bestid;
}

void skelhitdata::build(const skelmodel::skelmeshgroup *g, const uchar *ids)
{
    if(numzones)
    {
        return;
    }
    std::unordered_map<skelzonekey, skelzoneinfo> infomap;
    std::vector<skelzoneinfo *> info;
    skelzonebounds *bounds = new skelzonebounds[g->skel->numbones];
    numblends = g->blendcombos.size();
    for(uint i = 0; i < g->blendcombos.size(); i++)
    {
        if(!g->blendcombos[i].bonedata[1].weights)
        {
            numblends = i;
            break;
        }
    }
    blendcache.bdata = numblends > 0 ? new dualquat[numblends] : nullptr;
    for(size_t i = 0; i < std::min<size_t>(g->meshes.size(), 0x100); ++i)
    {
        skelmodel::skelmesh *m = reinterpret_cast<skelmodel::skelmesh *>(g->meshes[i]);
        for(int j = 0; j < m->numtris; ++j)
        {
            const skelmodel::tri &t = m->tris[j];
            for(int k = 0; k < 3; ++k)
            {
                const skelmodel::vert &v = m->verts[t.vert[k]];
                const skelmodel::blendcombo &c = g->blendcombos[v.blend];
                for(size_t l = 0; l < c.bonedata.size(); ++l) //note this is a loop l (level 4)
                {
                    if(c.bonedata[l].weights)
                    {
                        bounds[c.bonedata[l].bones].addvert(v.pos);
                    }
                }
            }
            const skelzonekey key(m, t);
            auto itr = infomap.find(key);
            if(itr == infomap.end())
            {
                itr = infomap.insert( { key, skelzoneinfo(key) } ).first;
            }
            skelzoneinfo &zi = (*itr).second;
            if(key.blend >= numblends && zi.index < 0)
            {
                bounds[key.bones[0]].owner = zi.index = info.size();
                info.push_back(&zi);
            }
            zi.tris.emplace_back();
            skelhittri &zt = zi.tris.back();
            zt.Mesh = i;
            zt.id = chooseid(g, m, t, ids);
            std::memcpy(zt.vert, t.vert, sizeof(zt.vert));
        }
    }
    for(int i = 0; i < g->skel->numbones; ++i)
    {
        if(bounds[i].empty() || bounds[i].owner >= 0)
        {
            continue;
        }
        const skelzonekey key(i);
        auto itr = infomap.find(key);
        if(itr == infomap.end())
        {
            itr = infomap.insert( { key, skelzoneinfo(key) } ).first;
        }
        skelzoneinfo &zi = (*itr).second;
        zi.index = info.size();
        info.push_back(&zi);
    }
    int leafzones = info.size();
    for(auto &[k, i] : infomap)
    {
        if(i.index < 0)
        {
            info.push_back(&i);
        }
    }
    for(size_t i = leafzones; i < info.size(); i++)
    {
        skelzoneinfo &zi = *info[i];
        if(zi.key.blend >= 0)
        {
            continue;
        }
        for(size_t j = 0; j < info.size(); j++)
        {
            if(i != j && zi.key.includes(info[j]->key))
            {
                skelzoneinfo &zj = *info[j];
                for(size_t k = 0; k< zi.children.size(); k++)
                {
                    skelzoneinfo &zk = *zi.children[k];
                    if(zk.key.includes(zj.key))
                    {
                        goto nextzone; //basically `continue` except for the top level loop
                    }
                    if(zj.key.includes(zk.key))
                    {
                        zk.parents--;
                        zj.parents++;
                        zi.children[k] = &zj;
                        while(++k < zi.children.size())
                        {
                            if(zj.key.includes(zi.children[k]->key))
                            {
                                zi.children[k]->parents--;
                                zi.children.erase(zi.children.begin() + k);
                                k--;
                            }
                        }
                        goto nextzone; //basically `continue` except for the top level loop
                    }
                }
                zj.parents++;
                zi.children.push_back(&zj);
            nextzone:;
            }
        }
        skelzonekey deps = zi.key;
        for(const skelzoneinfo *zj : zi.children)
        {
            if(zj->key.blend < 0 || zj->key.blend >= numblends)
            {
                deps.subtract(zj->key);
            }
        }
        for(size_t j = 0; j < sizeof(deps.bones); ++j)
        {
            if(deps.bones[j]==0xFF)
            {
                break;
            }
            const skelzonekey dep(deps.bones[j]);
            auto itr = infomap.find(dep);
            if(itr == infomap.end())
            {
                itr = infomap.insert( { dep, skelzoneinfo(dep) } ).first;
            }
            skelzoneinfo &zj = (*itr).second;
            zj.parents++;
            zi.children.push_back(&zj);
        }
    }
    for(uint i = leafzones; i < info.size(); i++)
    {
        skelzoneinfo &zi = *info[i];
        for(uint j = 0; j < zi.children.size(); j++)
        {
            skelzoneinfo &zj = *zi.children[j];
            if(zj.tris.size() <= 2 && zj.parents == 1)
            {
                zj.tris.clear();
                if(zj.index < 0)
                {
                    zj.parents = 0;
                    zi.children.erase(zi.children.begin() + j);
                    j--;
                }
                zj.children.clear();
            }
        }
    }
    int numlinks = 0,
        numtris = 0;
    for(int i = info.size(); --i >=0;) //note reverse iteration
    {
        skelzoneinfo &zi = *info[i];
        if(zi.parents || zi.tris.empty())
        {
            info.erase(info.begin() + i);
        }
        zi.conflicts = zi.parents;
        numlinks += zi.parents + zi.children.size();
        numtris += zi.tris.size();
    }
    rootzones = info.size();
    for(uint i = 0; i < info.size(); i++)
    {
        skelzoneinfo &zi = *info[i];
        zi.index = i;
        for(skelzoneinfo *&zj : zi.children)
        {
            if(!--zj->conflicts)
            {
                info.push_back(zj);
            }
        }
    }
    numzones = info.size();
    zones = new skelhitzone[numzones];
    links = numlinks ? new skelhitzone *[numlinks] : nullptr;
    tris = new skelhittri[numtris];
    skelhitzone **curlink = links;
    skelhittri *curtris = tris;
    for(int i = 0; i < numzones; ++i)
    {
        skelhitzone &z = zones[i];
        const skelzoneinfo &zi = *info[info.size()-1 - i];
        std::memcpy(curtris, zi.tris.data(), zi.tris.size()*sizeof(skelhittri));
        if(zi.key.blend >= numblends)
        {
            z.blend = zi.key.bones[0] + numblends;
            if(zi.tris.size())
            {
                z.bih = new skelbih(g, zi.tris.size(), curtris);
            }
            const skelzonebounds &b = bounds[zi.key.bones[0]];
            z.center = b.calccenter();
            z.radius = b.calcradius();
        }
        else if(zi.key.blend >= 0)
        {
            z.blend = zi.key.blend;
            z.bih = new skelbih(g, zi.tris.size(), curtris);
            z.center = z.bih->calccenter();
            z.radius = z.bih->calcradius();
        }
        else
        {
            z.numtris = zi.tris.size();
            z.tris = curtris;
        }
        curtris += zi.tris.size();
        z.parents = curlink;
        curlink += zi.parents;
        z.numchildren = zi.children.size();
        z.children = curlink;
        for(uint j = 0; j < zi.children.size(); j++)
        {
            z.children[j] = &zones[info.size()-1 - zi.children[j]->index];
        }
        curlink += zi.children.size();
    }
    for(int i = 0; i < numzones; ++i)
    {
        skelhitzone &z = zones[i];
        for(int j = 0; j < z.numchildren; ++j)
        {
            z.children[j]->parents[z.children[j]->numparents++] = &z;
        }
    }
    delete[] bounds;
}

void skelmodel::skelmeshgroup::cleanuphitdata()
{
    if(hitdata)
    {
        hitdata->cleanup();
    }
}

void skelmodel::skelmeshgroup::deletehitdata()
{
    if(hitdata)
    {
        delete hitdata;
        hitdata = nullptr;
    }
}

void skelmodel::skelmeshgroup::intersect(skelhitdata *z, part *p, const skelmodel::skelcacheentry &sc, const vec &o, const vec &ray) const
{
    int owner = &sc - &skel->skelcache[0];
    skelmodel::blendcacheentry &bc = z->getcache();
    if(bc.owner != owner || bc != sc)
    {
        bc.owner = owner;
        bc.millis = lastmillis;
        static_cast<animcacheentry &>(bc) = sc;
        blendbones(sc.bdata, bc.bdata, blendcombos.data(), z->getblendcount());
        z->propagate(this, sc.bdata, bc.bdata);
    }
    z->intersect(this, p->skins.data(), sc.bdata, bc.bdata, o, ray);
}

void skelmodel::skelmeshgroup::buildhitdata(const uchar *hitzones)
{
    if(hitdata)
    {
        return;
    }
    hitdata = new skelhitdata;
    hitdata->build(this, hitzones);
}

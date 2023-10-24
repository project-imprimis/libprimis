/* normal.cpp: cube geometry normal interpolation
 *
 * cube geometry in the libprimis engine is faceted, only allowing 8 ticks of
 * movement; as a result, normal vectors of geometry are not very smooth
 *
 * to resolve this, adjacent cube faces with low differences in their angle can
 * have their faces "merged" by interpolating the normal maps of their respective
 * faces
 *
 * this is controlled by the lerp variables and is generally uniformly done for
 * all geometry on the map; see `lerpangle` for the threshold variable
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include "octarender.h"

#include "world/octaworld.h"
#include "world/world.h"

struct normalkey
{
    vec pos;
    int smooth;

    bool operator==(const normalkey &k) const
    {
        return k.pos == pos && smooth == k.smooth;
    }
};

template<>
struct std::hash<normalkey>
{
    size_t operator()(const normalkey &k) const
    {
        auto vechash = std::hash<vec>();
        return vechash(k.pos);
    }
};

namespace //internal functionality not seen by other files
{
    struct normalgroup
    {
        vec pos;
        int smooth, flat, normals, tnormals;

        normalgroup() : smooth(0), flat(0), normals(-1), tnormals(-1) {}
        normalgroup(const normalkey &key) : pos(key.pos), smooth(key.smooth), flat(0), normals(-1), tnormals(-1) {}
    };

    struct normal
    {
        int next;
        vec surface;
    };

    struct tnormal
    {
        int next;
        float offset;
        int normals[2];
        normalgroup *groups[2];
    };

    std::unordered_map<normalkey, normalgroup> normalgroups;
    std::vector<normal> normals;
    std::vector<tnormal> tnormals;
    std::vector<int> smoothgroups;

    VARR(lerpangle, 0, 44, 180); //max angle to merge octree faces' normals smoothly

    bool usetnormals = true;

    int addnormal(const vec &pos, int smooth, const vec &surface)
    {
        normalkey key = { pos, smooth };
        auto itr = normalgroups.find(key);
        if(itr == normalgroups.end())
        {
            itr = normalgroups.insert( { key, normalgroup(key) } ).first;
        }
        normal n;
        n.next = (*itr).second.normals;
        n.surface = surface;
        normals.push_back(n);
        return (*itr).second.normals = normals.size()-1;
    }

    void addtnormal(const vec &pos, int smooth, float offset, int normal1, int normal2, const vec &pos1, const vec &pos2)
    {
        normalkey key = { pos, smooth };
        auto itr = normalgroups.find(key);
        if(itr == normalgroups.end())
        {
            itr = normalgroups.insert( { key, normalgroup(key) } ).first;
        }
        tnormal n;
        n.next = (*itr).second.tnormals;
        n.offset = offset;
        n.normals[0] = normal1;
        n.normals[1] = normal2;
        normalkey key1 = { pos1, smooth },
                  key2 = { pos2, smooth };
        n.groups[0] = &((*normalgroups.find(key1)).second);
        n.groups[1] = &((*normalgroups.find(key2)).second);
        tnormals.push_back(n);
        (*itr).second.tnormals = tnormals.size()-1;
    }

    int addnormal(const vec &pos, int smooth, int axis)
    {
        normalkey key = { pos, smooth };
        auto itr = normalgroups.find(key);
        if(itr == normalgroups.end())
        {
            itr = normalgroups.insert( { key, normalgroup(key) } ).first;
        }
        (*itr).second.flat += 1<<(4*axis);
        return axis - 6;
    }

    void findnormal(const normalgroup &g, float lerpthreshold, const vec &surface, vec &v)
    {
        v = vec(0, 0, 0);
        int total = 0;
        //check if abs value of x component of the surface normal is greater than the lerp threshold
        //note the assignments to the int n are bitshifted to pack all three axes within a single int
        if(surface.x >= lerpthreshold)
        {
            int n = (g.flat>>4)&0xF;
            v.x += n; total += n;
        }
        else if(surface.x <= -lerpthreshold)
        {
            int n = g.flat&0xF;
            v.x -= n;
            total += n;
        }
        //ditto y component
        if(surface.y >= lerpthreshold)
        {
            int n = (g.flat>>12)&0xF;
            v.y += n;
            total += n;
        }
        else if(surface.y <= -lerpthreshold)
        {
            int n = (g.flat>>8)&0xF;
            v.y -= n;
            total += n;
        }
        //ditto z component
        if(surface.z >= lerpthreshold)
        {
            int n = (g.flat>>20)&0xF;
            v.z += n;
            total += n;
        }
        else if(surface.z <= -lerpthreshold)
        {
            int n = (g.flat>>16)&0xF;
            v.z -= n;
            total += n;
        }

        for(int cur = g.normals; cur >= 0;)
        {
            normal &o = normals[cur];
            if(o.surface.dot(surface) >= lerpthreshold)
            {
                v.add(o.surface);
                total++;
            }
            cur = o.next;
        }
        if(total > 1)
        {
            v.normalize();
        }
        else if(!total)
        {
            v = surface;
        }
    }

    bool findtnormal(const normalgroup &g, float lerpthreshold, const vec &surface, vec &v)
    {
        float bestangle = lerpthreshold;
        const tnormal *bestnorm = nullptr;
        for(int cur = g.tnormals; cur >= 0;)
        {
            const tnormal &o = tnormals[cur];
            const std::array<vec, 6> flats = { vec(-1,  0,  0),
                                               vec( 1,  0,  0),
                                               vec( 0, -1,  0),
                                               vec( 0,  1,  0),
                                               vec( 0,  0, -1),
                                               vec( 0,  0,  1) };
            vec n1 = o.normals[0] < 0 ? flats[o.normals[0]+6] : normals[o.normals[0]].surface,
                n2 = o.normals[1] < 0 ? flats[o.normals[1]+6] : normals[o.normals[1]].surface,
                nt;
            nt.lerp(n1, n2, o.offset).normalize();
            float tangle = nt.dot(surface);
            if(tangle >= bestangle)
            {
                bestangle = tangle;
                bestnorm = &o;
            }
            cur = o.next;
        }
        if(!bestnorm)
        {
            return false;
        }
        vec n1, n2;
        findnormal(*bestnorm->groups[0], lerpthreshold, surface, n1);
        findnormal(*bestnorm->groups[1], lerpthreshold, surface, n2);
        v.lerp(n1, n2, bestnorm->offset).normalize();
        return true;
    }

    VARR(lerpsubdiv, 0, 2, 4);      //Linear intERPolation SUBDIVisions
    VARR(lerpsubdivsize, 4, 4, 128);//Linear intERPolation SUBDIVision cube SIZE

    void addnormals(const cube &c, const ivec &o, int size)
    {
        if(c.children)
        {
            size >>= 1;
            for(size_t i = 0; i < c.children->size(); ++i)
            {
                addnormals((*c.children)[i], ivec(i, o, size), size);
            }
            return;
        }
        else if(c.isempty())
        {
            return;
        }
        vec pos[Face_MaxVerts];
        int norms[Face_MaxVerts],
            tj = usetnormals && c.ext ? c.ext->tjoints : -1, vis;
        for(int i = 0; i < 6; ++i)
        {
            if((vis = visibletris(c, i, o, size)))
            {
                if(c.texture[i] == Default_Sky)
                {
                    continue;
                }

                std::array<vec, 2> planes;
                int numverts = c.ext ? c.ext->surfaces[i].numverts&Face_MaxVerts : 0,
                    convex = 0,
                    numplanes = 0;
                if(numverts)
                {
                    vertinfo *verts = c.ext->verts() + c.ext->surfaces[i].verts;
                    vec vo(static_cast<ivec>(o).mask(~0xFFF));
                    for(int j = 0; j < numverts; ++j)
                    {
                        vertinfo &v = verts[j];
                        pos[j] = vec(v.x, v.y, v.z).mul(1.0f/8).add(vo);
                    }
                    if(!(c.merged&(1<<i)) && !flataxisface(c, i))
                    {
                        convex = faceconvexity(verts, numverts, size);
                    }
                }
                else if(c.merged&(1<<i))
                {
                    continue;
                }
                else
                {
                    std::array<ivec, 4> v;
                    genfaceverts(c, i, v);
                    if(!flataxisface(c, i))
                    {
                        convex = faceconvexity(v);
                    }
                    int order = vis&4 || convex < 0 ? 1 : 0;
                    vec vo(o);
                    pos[numverts++] = static_cast<vec>(v[order]).mul(size/8.0f).add(vo);
                    if(vis&1)
                    {
                        pos[numverts++] = static_cast<vec>(v[order+1]).mul(size/8.0f).add(vo);
                    }
                    pos[numverts++] = static_cast<vec>(v[order+2]).mul(size/8.0f).add(vo);
                    if(vis&2)
                    {
                        pos[numverts++] = static_cast<vec>(v[(order+3)&3]).mul(size/8.0f).add(vo);
                    }
                }

                if(!flataxisface(c, i))
                {
                    planes[numplanes++].cross(pos[0], pos[1], pos[2]).normalize();
                    if(convex)
                    {
                        planes[numplanes++].cross(pos[0], pos[2], pos[3]).normalize();
                    }
                }

                const VSlot &vslot = lookupvslot(c.texture[i], false);
                int smooth = vslot.slot->smooth;

                if(!numplanes)
                {
                    for(int k = 0; k < numverts; ++k)
                    {
                        norms[k] = addnormal(pos[k], smooth, i);
                    }
                }
                else if(numplanes==1)
                {
                    for(int k = 0; k < numverts; ++k)
                    {
                        norms[k] = addnormal(pos[k], smooth, planes[0]);
                    }
                }
                else
                {
                    vec avg = vec(planes[0]).add(planes[1]).normalize();
                    norms[0] = addnormal(pos[0], smooth, avg);
                    norms[1] = addnormal(pos[1], smooth, planes[0]);
                    norms[2] = addnormal(pos[2], smooth, avg);
                    for(int k = 3; k < numverts; k++)
                    {
                        norms[k] = addnormal(pos[k], smooth, planes[1]);
                    }
                }

                while(tj >= 0 && tjoints[tj].edge < i*(Face_MaxVerts+1))
                {
                    tj = tjoints[tj].next;
                }
                while(tj >= 0 && tjoints[tj].edge < (i+1)*(Face_MaxVerts+1))
                {
                    int edge = tjoints[tj].edge,
                        e1 = edge%(Face_MaxVerts+1),
                        e2 = (e1+1)%numverts;
                    const vec &v1 = pos[e1],
                              &v2 = pos[e2];
                    ivec d(vec(v2).sub(v1).mul(8));
                    int axis = std::abs(d.x) > std::abs(d.y) ? (std::abs(d.x) > std::abs(d.z) ? 0 : 2) : (std::abs(d.y) > std::abs(d.z) ? 1 : 2);
                    if(d[axis] < 0)
                    {
                        d.neg();
                    }
                    reduceslope(d);
                    int origin  =  static_cast<int>(std::min(v1[axis], v2[axis])*8)&~0x7FFF,
                        offset1 = (static_cast<int>(v1[axis]*8) - origin) / d[axis],
                        offset2 = (static_cast<int>(v2[axis]*8) - origin) / d[axis];
                    vec o = vec(v1).sub(vec(d).mul(offset1/8.0f)),
                        n1, n2;
                    float doffset = 1.0f / (offset2 - offset1);
                    while(tj >= 0)
                    {
                        tjoint &t = tjoints[tj];
                        if(t.edge != edge)
                        {
                            break;
                        }
                        float offset = (t.offset - offset1) * doffset;
                        vec tpos = vec(d).mul(t.offset/8.0f).add(o);
                        addtnormal(tpos, smooth, offset, norms[e1], norms[e2], v1, v2);
                        tj = t.next;
                    }
                }
            }
        }
    }
}

/* externally relevant functionality */
///////////////////////////////////////

void findnormal(const vec &pos, int smooth, const vec &surface, vec &v)
{
    normalkey key = { pos, smooth };
    auto itr = normalgroups.find(key);
    if(smooth < 0)
    {
        smooth = 0;
    }
    bool usegroup = (static_cast<int>(smoothgroups.size()) > smooth) && smoothgroups[smooth] >= 0;
    if(itr != normalgroups.end())
    {
        int angle = usegroup ? smoothgroups[smooth] : lerpangle;
        float lerpthreshold = cos360(angle) - 1e-5f;
        if((*itr).second.tnormals < 0 || !findtnormal((*itr).second, lerpthreshold, surface, v))
        {
            findnormal((*itr).second, lerpthreshold, surface, v);
        }
    }
    else
    {
        v = surface;
    }
}

void cubeworld::calcnormals(bool lerptjoints)
{
    usetnormals = lerptjoints;
    if(usetnormals)
    {
        findtjoints();
    }
    for(size_t i = 0; i < worldroot->size(); ++i)
    {
        addnormals((*worldroot)[i], ivec(i, ivec(0, 0, 0), mapsize()/2), mapsize()/2);
    }
}

void clearnormals()
{
    normalgroups.clear();
    normals.clear();
    tnormals.clear();
}

void resetsmoothgroups()
{
    smoothgroups.clear();
}

static constexpr int maxsmoothgroups = 10000;
//returns the smoothgroup at the idth location in the smoothgroups vector
//returns -1 (failure) if you try to ask for an id greater than 10,000
int smoothangle(int id, int angle)
{
    if(id < 0)
    {
        id = smoothgroups.size();
    }
    if(id >= maxsmoothgroups)
    {
        return -1;
    }
    while(static_cast<int>(smoothgroups.size()) <= id)
    {
        smoothgroups.push_back(-1);
    }
    if(angle >= 0)
    {
        smoothgroups[id] = std::min(angle, 180);
    }
    return id;
}

void initnormalcmds()
{
    addcommand("smoothangle", reinterpret_cast<identfun>(+[] (int *id, int *angle) {intret(smoothangle(*id, *angle));}), "ib", Id_Command);
}

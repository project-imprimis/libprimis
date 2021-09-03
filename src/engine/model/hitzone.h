#ifndef SKELBIH_H_
#define SKELBIH_H_
class skelbih
{
    public:
        struct tri : skelmodel::tri
        {
            uchar Mesh, id;
        };
        vec calccenter() const
        {
            return vec(bbmin).add(bbmax).mul(0.5f);
        }

        float calcradius() const
        {
            return vec(bbmax).sub(bbmin).mul(0.5f).magnitude();
        }

        skelbih(skelmodel::skelmeshgroup *m, int numtris, tri *tris);

        ~skelbih()
        {
            DELETEA(nodes);
        }

        struct node
        {
            short split[2];
            ushort child[2];

            int axis() const
            {
                return child[0]>>14;
            }

            int childindex(int which) const
            {
                return child[which]&0x3FFF;
            }

            bool isleaf(int which) const
            {
                return (child[1]&(1<<(14+which)))!=0;
            }
        };

        void intersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, const vec &o, const vec &ray);

    private:
        node *nodes;
        int numnodes;
        tri *tris;

        vec bbmin, bbmax;

        bool triintersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, int tidx, const vec &o, const vec &ray);
        void build(skelmodel::skelmeshgroup *m, ushort *indices, int numindices, const vec &vmin, const vec &vmax);
        void intersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, const vec &o, const vec &ray, const vec &invray, node *curnode, float tmin, float tmax);
};

class skelhitzone
{
    public:
        typedef skelbih::tri tri;

        int numparents, numchildren;
        skelhitzone **parents, **children;
        vec center;
        float radius;
        int visited;
        union
        {
            int blend;
            int numtris;
        };
        union
        {
            tri *tris;
            skelbih *bih;
        };

        skelhitzone() : numparents(0), numchildren(0), parents(nullptr), children(nullptr), center(0, 0, 0), radius(0), visited(-1), animcenter(0, 0, 0)
        {
            blend = -1;
            bih = nullptr;
        }

        ~skelhitzone()
        {
            if(!numchildren)
            {
                DELETEP(bih);
            }
            else
            {
                DELETEA(tris);
            }
        }

        void intersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, const dualquat *bdata1, const dualquat *bdata2, int numblends, const vec &o, const vec &ray)
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

        void propagate(skelmodel::skelmeshgroup *m, const dualquat *bdata1, const dualquat *bdata2, int numblends)
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
                    skelhitzone *child = children[i];
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

    private:
        vec animcenter;
        static bool triintersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, const dualquat *bdata1, const dualquat *bdata2, int numblends, const tri &t, const vec &o, const vec &ray);

        bool shellintersect(const vec &o, const vec &ray)
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
};

class skelzonekey
{
    public:
        skelzonekey() : blend(-1) { memset(bones, 0xFF, sizeof(bones)); }
        skelzonekey(int bone) : blend(INT_MAX) { bones[0] = bone; memset(&bones[1], 0xFF, sizeof(bones)-1); }
        skelzonekey(skelmodel::skelmesh *m, const skelmodel::tri &t) : blend(-1)
        {
            memset(bones, 0xFF, sizeof(bones));
            addbones(m, t);
        }

        int blend;
        uchar bones[12];

        bool includes(const skelzonekey &o)
        {
            int j = 0;
            for(int i = 0; i < static_cast<int>(sizeof(bones)); ++i)
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
            return j < static_cast<int>(sizeof(bones)) ? o.bones[j] == 0xFF : blend < 0 || blend == o.blend;
        }

        void subtract(const skelzonekey &o)
        {
            int len = 0,
                j   = 0;
            for(int i = 0; i < static_cast<int>(sizeof(bones)); ++i)
            {
            retry:
                if(j >= static_cast<int>(sizeof(o.bones)) || bones[i] < o.bones[j])
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
                } while(j < static_cast<int>(sizeof(o.bones)) && bones[i] > o.bones[j]);
                goto retry;
            }
            memset(&bones[len], 0xFF, sizeof(bones) - len);
        }

    private:
        bool hasbone(int n)
        {
            for(int i = 0; i < static_cast<int>(sizeof(bones)); ++i)
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

        int numbones()
        {
            for(int i = 0; i < static_cast<int>(sizeof(bones)); ++i)
            {
                if(bones[i] == 0xFF)
                {
                    return i;
                }
            }
            return sizeof(bones);
        }

        void addbone(int n)
        {
            for(int i = 0; i < static_cast<int>(sizeof(bones)); ++i)
            {
                if(n <= bones[i])
                {
                    if(n < bones[i])
                    {
                        memmove(&bones[i+1], &bones[i], sizeof(bones) - (i+1));
                        bones[i] = n;
                    }
                    return;
                }
            }
        }

        void addbones(skelmodel::skelmesh *m, const skelmodel::tri &t)
        {
            skelmodel::skelmeshgroup *g = reinterpret_cast<skelmodel::skelmeshgroup *>(m->group);
            int b0 = m->verts[t.vert[0]].blend,
                b1 = m->verts[t.vert[1]].blend,
                b2 = m->verts[t.vert[1]].blend;
            const skelmodel::blendcombo &c0 = g->blendcombos[b0];
            for(int i = 0; i < 4; ++i)
            {
                if(c0.weights[i])
                {
                    addbone(c0.bones[i]);
                }
            }
            if(b0 != b1 || b0 != b2)
            {
                const skelmodel::blendcombo &c1 = g->blendcombos[b1];
                for(int i = 0; i < 4; ++i)
                {
                    if(c1.weights[i])
                    {
                        addbone(c1.bones[i]);
                    }
                }
                const skelmodel::blendcombo &c2 = g->blendcombos[b2];
                for(int i = 0; i < 4; ++i)
                {
                    if(c2.weights[i]) addbone(c2.bones[i]);
                }
            }
            else
            {
                blend = b0;
            }
        }

};

class skelzonebounds
{
    public:
        skelzonebounds() : owner(-1), bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f) {}
        int owner;

        bool empty() const
        {
            return bbmin.x > bbmax.x;
        }

        vec calccenter() const
        {
            return vec(bbmin).add(bbmax).mul(0.5f);
        }
        void addvert(const vec &p)
        {
            bbmin.x = std::min(bbmin.x, p.x);
            bbmin.y = std::min(bbmin.y, p.y);
            bbmin.z = std::min(bbmin.z, p.z);
            bbmax.x = std::max(bbmax.x, p.x);
            bbmax.y = std::max(bbmax.y, p.y);
            bbmax.z = std::max(bbmax.z, p.z);
        }
        float calcradius() const
        {
            return vec(bbmax).sub(bbmin).mul(0.5f).magnitude();
        }
    private:
        vec bbmin, bbmax;
};

class skelzoneinfo
{
    public:
        int index, parents, conflicts;
        skelzonekey key;
        vector<skelzoneinfo *> children;
        vector<skelhitzone::tri> tris;

        skelzoneinfo() : index(-1), parents(0), conflicts(0) {}
        skelzoneinfo(const skelzonekey &key) : index(-1), parents(0), conflicts(0), key(key) {}
};

static inline bool htcmp(const skelzonekey &x, const skelzoneinfo &y)
{
    return !memcmp(x.bones, y.key.bones, sizeof(x.bones)) && (x.bones[1] == 0xFF || x.blend == y.key.blend);
}

static inline uint hthash(const skelzonekey &k)
{
    union
    {
        uint i[3];
        uchar b[12];
    } conv;
    memcpy(conv.b, k.bones, sizeof(conv.b));
    return conv.i[0]^conv.i[1]^conv.i[2];
}

class skelhitdata
{
    public:
        int numblends;
        skelmodel::blendcacheentry blendcache;
        skelhitdata() : numblends(0), numzones(0), rootzones(0), visited(0), zones(nullptr), links(nullptr), tris(nullptr) {}
        ~skelhitdata()
        {
            DELETEA(zones);
            DELETEA(links);
            DELETEA(tris);
            DELETEA(blendcache.bdata);
        }
        void build(skelmodel::skelmeshgroup *g, const uchar *ids);

        void propagate(skelmodel::skelmeshgroup *m, const dualquat *bdata1, dualquat *bdata2)
        {
            visited = 0;
            for(int i = 0; i < numzones; ++i)
            {
                zones[i].visited = -1;
                zones[i].propagate(m, bdata1, bdata2, numblends);
            }
        }

        void cleanup()
        {
            blendcache.owner = -1;
        }

        void intersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, const dualquat *bdata1, dualquat *bdata2, const vec &o, const vec &ray)
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
    private:
        int numzones, rootzones, visited;
        skelhitzone *zones;
        skelhitzone **links;
        skelhitzone::tri *tris;

        uchar chooseid(skelmodel::skelmeshgroup *g, skelmodel::skelmesh *m, const skelmodel::tri &t, const uchar *ids);
};

#endif

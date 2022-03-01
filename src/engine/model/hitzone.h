#ifndef SKELBIH_H_
#define SKELBIH_H_
class skelbih
{
    public:
        struct tri : skelmodel::tri
        {
            uchar Mesh, id;
        };
        vec calccenter() const;
        float calcradius() const;
        skelbih(skelmodel::skelmeshgroup *m, int numtris, tri *tris);

        ~skelbih()
        {
            delete[] nodes;
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

        struct skelbihstack
        {
            skelbih::node *node;
            float tmin, tmax;
        };

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

        void intersect(skelmodel::skelmeshgroup *m,
                       skelmodel::skin *s,
                       const dualquat *bdata1,
                       const dualquat *bdata2,
                       int numblends,
                       const vec &o,
                       const vec &ray);

        void propagate(skelmodel::skelmeshgroup *m,
                       const dualquat *bdata1,
                       const dualquat *bdata2,
                       int numblends);

    private:
        vec animcenter;
        static bool triintersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, const dualquat *bdata1, const dualquat *bdata2, int numblends, const tri &t, const vec &o, const vec &ray);
        bool shellintersect(const vec &o, const vec &ray);

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

        bool includes(const skelzonekey &o);
        void subtract(const skelzonekey &o);

        int blend;
        uchar bones[12];

    private:
        bool hasbone(int n);

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

class skelhitdata
{
    public:
        int numblends;
        skelmodel::blendcacheentry blendcache;
        skelhitdata() : numblends(0), numzones(0), rootzones(0), visited(0), zones(nullptr), links(nullptr), tris(nullptr) {}
        ~skelhitdata()
        {
            delete[] zones;
            delete[] links;
            delete[] tris;
            delete[] blendcache.bdata;
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
    //need to set htcmp to friend because it must be in global scope for hashtable macro to find it
    friend bool htcmp(const skelzonekey &x, const skelhitdata::skelzoneinfo &y);

};

uint hthash(const skelzonekey &k);

#endif

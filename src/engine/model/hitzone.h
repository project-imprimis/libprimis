#ifndef SKELBIH_H_
#define SKELBIH_H_

class skelhitzone;

struct skelhittri : skelmodel::tri
{
    uchar Mesh, id;
};

class skelzonekey
{
    public:
        skelzonekey();
        skelzonekey(int bone);
        skelzonekey(const skelmodel::skelmesh *m, const skelmodel::tri &t);

        bool includes(const skelzonekey &o) const;
        void subtract(const skelzonekey &o);
        bool operator==(const skelzonekey &k) const;

        int blend;
        std::array<uchar, 12> bones;
    private:
        bool hasbone(int n) const;

        void addbone(int n);
        void addbones(const skelmodel::skelmesh *m, const skelmodel::tri &t);
};

class skelhitdata
{
    public:
        skelhitdata();
        ~skelhitdata();
        void build(const skelmodel::skelmeshgroup *g, const uchar *ids);
        void propagate(const skelmodel::skelmeshgroup *m, const dualquat *bdata1, dualquat *bdata2);
        void cleanup();
        void intersect(const skelmodel::skelmeshgroup *m, skelmodel::skin *s, const dualquat *bdata1, dualquat *bdata2, const vec &o, const vec &ray);

        int getblendcount();
        skelmodel::blendcacheentry &getcache();

    private:
        class skelbih
        {
            public:
                vec calccenter() const;
                float calcradius() const;
                skelbih(const skelmodel::skelmeshgroup *m, int numtris, const skelhittri *tris);

                ~skelbih()
                {
                    delete[] nodes;
                }

                void intersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, const vec &o, const vec &ray);

            private:
                struct node
                {
                    short split[2];
                    ushort child[2];

                    int axis() const;
                    int childindex(int which) const;
                    bool isleaf(int which) const;
                };
                node *nodes;
                int numnodes;
                const skelhittri *tris;

                vec bbmin, bbmax;

                bool triintersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, int tidx, const vec &o, const vec &ray) const;
                void build(const skelmodel::skelmeshgroup *m, ushort *indices, int numindices, const vec &vmin, const vec &vmax);
                void intersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, const vec &o, const vec &ray, const vec &invray, node *curnode, float tmin, float tmax) const;

                struct skelbihstack
                {
                    skelbih::node *node;
                    float tmin, tmax;
                };

        };

        class skelhitzone
        {
            public:
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
                    skelhittri *tris;
                    skelbih *bih;
                };

                skelhitzone();
                ~skelhitzone();

                void intersect(const skelmodel::skelmeshgroup *m,
                               const skelmodel::skin *s,
                               const dualquat *bdata1,
                               const dualquat *bdata2,
                               int numblends,
                               const vec &o,
                               const vec &ray);

                void propagate(const skelmodel::skelmeshgroup *m,
                               const dualquat *bdata1,
                               const dualquat *bdata2,
                               int numblends);

            private:
                vec animcenter;
                static bool triintersect(const skelmodel::skelmeshgroup *m, const skelmodel::skin *s, const dualquat *bdata1, const dualquat *bdata2, int numblends, const skelhittri &t, const vec &o, const vec &ray);
                bool shellintersect(const vec &o, const vec &ray);

        };

        int numblends;
        skelmodel::blendcacheentry blendcache;
        int numzones, rootzones, visited;
        skelhitzone *zones;
        skelhitzone **links;
        skelhittri *tris;

        uchar chooseid(const skelmodel::skelmeshgroup *g, const skelmodel::skelmesh *m, const skelmodel::tri &t, const uchar *ids);

        class skelzoneinfo
        {
            public:
                int index, parents, conflicts;
                skelzonekey key;
                std::vector<skelzoneinfo *> children;
                std::vector<skelhittri> tris;

                skelzoneinfo() : index(-1), parents(0), conflicts(0) {}
                skelzoneinfo(const skelzonekey &key) : index(-1), parents(0), conflicts(0), key(key) {}
        };
};

#endif

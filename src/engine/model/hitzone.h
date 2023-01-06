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

            int axis() const;
            int childindex(int which) const;
            bool isleaf(int which) const;
        };

        void intersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, const vec &o, const vec &ray);

    private:
        node *nodes;
        int numnodes;
        tri *tris;

        vec bbmin, bbmax;

        bool triintersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, int tidx, const vec &o, const vec &ray);
        void build(const skelmodel::skelmeshgroup *m, ushort *indices, int numindices, const vec &vmin, const vec &vmax);
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

        skelhitzone();
        ~skelhitzone();

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
        skelzonekey();
        skelzonekey(int bone);
        skelzonekey(skelmodel::skelmesh *m, const skelmodel::tri &t);

        bool includes(const skelzonekey &o);
        void subtract(const skelzonekey &o);

        int blend;
        uchar bones[12];

    private:
        bool hasbone(int n);

        int numbones();
        void addbone(int n);
        void addbones(skelmodel::skelmesh *m, const skelmodel::tri &t);
};

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

class skelhitdata
{
    public:
        int numblends;
        skelmodel::blendcacheentry blendcache;
        skelhitdata();
        ~skelhitdata();
        void build(skelmodel::skelmeshgroup *g, const uchar *ids);

        void propagate(skelmodel::skelmeshgroup *m, const dualquat *bdata1, dualquat *bdata2);

        void cleanup();
        void intersect(skelmodel::skelmeshgroup *m, skelmodel::skin *s, const dualquat *bdata1, dualquat *bdata2, const vec &o, const vec &ray);
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
                std::vector<skelzoneinfo *> children;
                std::vector<skelhitzone::tri> tris;

                skelzoneinfo() : index(-1), parents(0), conflicts(0) {}
                skelzoneinfo(const skelzonekey &key) : index(-1), parents(0), conflicts(0), key(key) {}
        };
    //need to set htcmp to friend because it must be in global scope for hashtable macro to find it
    friend bool htcmp(const skelzonekey &x, const skelhitdata::skelzoneinfo &y);

};

uint hthash(const skelzonekey &k);

#endif

class BIH
{
    public:
        struct node
        {
            std::array<short, 2> split;
            std::array<ushort, 2> child;

            int axis() const;
            int childindex(int which) const;
            bool isleaf(int which) const;
        };

        enum
        {
            Mesh_Render   = 1<<1,
            Mesh_NoClip   = 1<<2,
            Mesh_Alpha    = 1<<3,
            Mesh_Collide  = 1<<4,
            Mesh_CullFace = 1<<5
        };

        class mesh
        {
            public:
                static constexpr int maxtriangles = 1<<14;

                matrix4x3 xform;
                matrix4x3 invxform() const;
                matrix3 xformnorm() const;
                matrix3 invxformnorm() const;
                float scale() const;

                node *nodes;
                int numnodes;

                struct tri
                {
                    std::array<uint, 3> vert;
                };
                const tri *tris;

                struct tribb
                {
                    svec center, radius;

                    bool outside(const ivec &bo, const ivec &br) const;
                };
                const tribb *tribbs;
                int numtris;
                const Texture *tex;
                int flags;
                vec bbmin, bbmax;

                mesh();

                vec getpos(int i) const;
                vec2 gettc(int i) const;
                void setmesh(const tri *tris, int numtris,
                             const uchar *pos, int posstride,
                             const uchar *tc, int tcstride);
            private:
                const uchar *pos, *tc;
                int posstride, tcstride;
        };

        BIH(const std::vector<mesh> &buildmeshes);

        ~BIH();

        bool traverse(const vec &o, const vec &ray, float maxdist, float &dist, int mode) const;
        bool triintersect(const mesh &m, int tidx, const vec &mo, const vec &mray, float maxdist, float &dist, int mode) const;
        bool boxcollide(const physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale = 1) const;
        bool ellipsecollide(const physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale = 1) const;
        void genstaintris(std::vector<std::array<vec, 3>> &tris, const vec &staincenter, float stainradius, const vec &o, int yaw, int pitch, int roll, float scale = 1) const;
        float getentradius() const;
    private:
        std::vector<mesh> meshes;
        node *nodes;
        int numnodes;
        vec bbmin, bbmax, center;
        float radius;

        static constexpr float maxcollidedistance = -1e9f;

        template<int C>
        void collide(const mesh &m, const physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, node *curnode, const ivec &bo, const ivec &br) const;
        template<int C>
        void tricollide(const mesh &m, int tidx, const physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, const ivec &bo, const ivec &br) const;

        void build(mesh &m, uint *indices, int numindices, const ivec &vmin, const ivec &vmax) const;
        bool traverse(const mesh &m, const vec &o, const vec &ray, const vec &invray, float maxdist, float &dist, int mode, const node *curnode, float tmin, float tmax) const;
        void genstaintris(std::vector<std::array<vec, 3>> &tris, const mesh &m, const vec &center, float radius, const matrix4x3 &orient, node *curnode, const ivec &bo, const ivec &br) const;
        void genstaintris(std::vector<std::array<vec, 3>> &tris, const mesh &m, int tidx, const vec &center, float radius, const matrix4x3 &orient, const ivec &bo, const ivec &br) const;
        bool playercollidecheck(const physent *d, float pdist, vec dir, vec n, vec radius) const;
};

extern bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);


#ifndef BIH_H_
#define BIH_H_

struct CollisionInfo final
{
    bool collided;
    vec collidewall;
};

class BIH final
{
    public:
        struct Node final
        {
            /**
             * @brief Array containing left and right splits.
             *
             * The 0th element contains the left split, and the 1st element
             * contains the right split.
             */
            std::array<short, 2> split;

            /**
             * @brief Array of child indices, packed with leaf and axis information.
             *
             * 14 bits of index data in each ushort, with two bits of axis data
             * and two bits of leaf data in the first two bits of the 0th and 1st
             * entry respectively:
             *
             *  |aaii'iiii'iiii'iiii|llii'iiii'iiii'iiii|
             *
             */
            std::array<ushort, 2> child;

            /**
             * @brief Returns top two bits of child[0] which store axis information
             *
             * The bottom 14 bits should be accessed separately with childindex(0).
             *
             * @return top two bits (0-3).
             */
            int axis() const;

            /**
             * @brief Returns last 14 bits of the child index (0-16383).
             *
             * @param which element of child array, valid values: 0,1
             *
             * @return last 14 bits (0-16383).
             */
            int childindex(int which) const;

            /**
             * @brief Returns leaf information from top 1|2 bits of child[1].
             *
             * @param whether to check top 1 or 2 bits (1, 0 args)
             *
             * @return whether top 1|2 bits are set
             */
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

        class mesh final
        {
            public:
                static constexpr int maxtriangles = 1<<14;

                matrix4x3 xform;
                matrix4x3 invxform() const;
                matrix3 xformnorm() const;
                matrix3 invxformnorm() const;
                float scale() const;

                Node *nodes;
                int numnodes;

                struct tri
                {
                    std::array<uint, 3> vert;
                };
                const tri *tris;

                struct tribb
                {
                    svec center, radius;

                    /**
                     * @brief Returns whether point at given radius is outside this tribb
                     *
                     * Returns whether the difference between the test point and the
                     * tribb is larger than the sums of their radiuses in each dimension.
                     * This checks whether the two points' radii interact with each
                     * other (neither point must be within the radius of the other point,
                     * it is sufficient that the radii overlap).
                     *
                     * @param bo the origin of the test point
                     * @param br the radius of the text point
                     */
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

                /**
                 * @brief Sets up arrays for this mesh.
                 *
                 * @param tris triangle array
                 * @param numtris number of triangles
                 * @param pos position array
                 * @param posstride distance between position entries
                 * @param tc texture coordinate array
                 * @param tcstride distance between tc entries
                 */
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
        CollisionInfo boxcollide(const physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale = 1) const;
        CollisionInfo ellipsecollide(const physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale = 1) const;
        void genstaintris(std::vector<std::array<vec, 3>> &tris, const vec &staincenter, float stainradius, const vec &o, int yaw, int pitch, int roll, float scale = 1) const;
        float getentradius() const;
    private:
        std::vector<mesh> meshes;
        Node *nodes;
        int numnodes;
        vec bbmin, bbmax, center;
        float radius;

        static constexpr float maxcollidedistance = -1e9f;

        template<int C>
        void collide(const mesh &m, const physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, Node *curnode, const ivec &bo, const ivec &br, vec &cwall) const;
        template<int C>
        void tricollide(const mesh &m, int tidx, const physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, const ivec &bo, const ivec &br, vec &cwall) const;

        void build(mesh &m, uint *indices, int numindices, const ivec &vmin, const ivec &vmax) const;
        bool traverse(const mesh &m, const vec &o, const vec &ray, const vec &invray, float maxdist, float &dist, int mode, const Node *curnode, float tmin, float tmax) const;
        void genstaintris(std::vector<std::array<vec, 3>> &tris, const mesh &m, const vec &center, float radius, const matrix4x3 &orient, Node *curnode, const ivec &bo, const ivec &br) const;
        void genstaintris(std::vector<std::array<vec, 3>> &tris, const mesh &m, int tidx, const vec &center, float radius, const matrix4x3 &orient, const ivec &bo, const ivec &br) const;
        bool playercollidecheck(const physent *d, float pdist, vec dir, vec n, vec radius) const;
};

extern bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);

#endif

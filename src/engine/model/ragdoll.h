#ifndef RAGDOLL_H_
#define RAGDOLL_H_

/**
 * @brief A representation of a ragdoll for a class of models.
 *
 * ragdollskel defines a skeletal animation object for use by skelmodel, which
 * is able to be dynamically modified by physics (rather than by an animation file)
 *
 * ragdollskel objects are owned by skelmodel::skeleton objects and therefore there
 * is exactly one `ragdollskel` no matter how many entities use a particular model
 */

class ragdollskel final
{
    public:
        ragdollskel() : loaded(false), animjoints(false), eye(-1) {}

        bool loaded, animjoints;

        struct tri final
        {
            int vert[3];

            /**
             * @brief Determines whether two tris share any vertex indices.
             *
             * Returns true if the the passed triangle has any of the same vertex indices,
             * regardless of order (e.g. if this.vert[0] and t.vert[2] are the same, returns true)
             *
             * @param t the tri to compare to
             *
             * @return true if any indices from this are the same as any from t
             * @return false if no vertex indices match
             */
            bool shareverts(const tri &t) const;
        };
        std::vector<tri> tris;

        struct reljoint final
        {
            int bone, parent;
        };
        std::vector<reljoint> reljoints;

        struct vert final
        {
            vec pos;
            float radius, weight;
        };
        std::vector<vert> verts;

        struct joint final
        {
            int bone, tri, vert[3];
            float weight;
            matrix4x3 orient;
        };
        std::vector<joint> joints;

        struct rotlimit final
        {
            int tri[2];
            float maxangle, maxtrace;
            matrix3 middle;
        };
        std::vector<rotlimit> rotlimits;

        struct rotfriction final
        {
            int tri[2];
        };
        std::vector<rotfriction> rotfrictions;

        //a distance constraint between two specified vertices
        //vert specifies the indices of the vertices in the verts vector, and
        //min/maxdist the limits to keep them within
        struct distlimit final
        {
            int vert[2];
            float mindist, maxdist;
        };
        std::vector<distlimit> distlimits;

        int eye;

        void setup();
        void addreljoint(int bone, int parent);

    private:
        void setupjoints();

        /**
         * @brief Adds indices for rotation frictions.
         *
         * For each pair of triangles in the tris vector of this object where one or more
         * vertices in those two triangles refer to the same vertex, adds a new rotfriction
         * to the rotfrictions vector with those triangles' indices within the tris vector.
         *
         * The rotfrictions vector is cleared before adding these entries; does not append
         * to existing entries that may be present.
         */
        void setuprotfrictions();
};

/**
 * @brief An individual instantiation of a ragdoll for a skeletal model
 *
 * ragdolldata defines a class corresponding to a specific instantiation of a ragdoll
 * in the context of a dynent. Many ragdolldata objects may point to the same ragdollskel
 * object.
 */
class ragdolldata final
{
    public:
        const ragdollskel *skel;
        int millis, collidemillis, lastmove;
        float radius;
        vec offset, center;

        //shadows the elements in skel->tris, should not be resized after construction
        std::vector<matrix3> tris;

        //shadows the elements in skel->animjoints
        matrix4x3 *animjoints;

        //shadows the elements in skel->reljoints
        dualquat *reljoints;

        struct vert final
        {
            vec oldpos, pos, newpos, undo;
            float weight;
            bool collided, stuck;
            vert() : oldpos(0, 0, 0), pos(0, 0, 0), newpos(0, 0, 0), undo(0, 0, 0), weight(0), collided(false), stuck(true) {}
        };

        //shadows the elements in skel->verts, should not be resized after construction
        std::vector<vert> verts;

        ragdolldata(const ragdollskel *skel, float scale = 1);
        ~ragdolldata();

        /**
         * @brief Moves the joints of the ragdoll.
         *
         * Moves the ragdoll joints by an amount indicated by the amount of time
         * passed to `ts`. The ragdoll movement will be calculated using
         * the ragdollwaterfric variable if water is true, and ragdollairfric if not.
         *
         * @param water whether the ragdoll is moving through air (false) or water(true)
         * @param ts the time to use to calculate physics with, in seconds
         */
        void move(bool water, float ts);

        /**
         * @brief Returns a transformation matrix according to the animation matrix and position
         *
         * Assumes that there are joints to use to calcuate.
         *
         * @param i the index of the joint to calculate
         * @param anim the animation matrix to transform the joint with
         *
         * @return an orientation matrix corresponding to the animation data passed
         */
        matrix4x3 calcanimjoint(int i, const matrix4x3 &anim) const;
        void init(const dynent *d);

    private:
        int collisions, floating, unsticks;
        float timestep, scale;

        std::vector<matrix3> rotfrictions;

        /**
         * @brief Sets new values in the tris matrix vector based on ragdollskel tris
         *
         * Const with respect to all values outside of the vector of tris
         *
         * Reads values from the associated ragdollskel, and sets the vecs inside the
         * corresponding matrix in the ragdolldata as follows:
         *
         *               /|
         *              /
         *          m.c/
         *            /
         *        v1 /   m.a     v2
         *          *---------->*
         *          |
         *          |
         *          |
         *       m.b|
         *          v
         *
         *
         *          *
         *           v3
         *
         *        ----→    ----→
         *  m.c = v1 v2  x v1 v3
         *        --→   ----→
         *  m.b = m.c x v1 v2
         *
         * m.a points from v1 to v2
         * m.b points from v1 to v3
         * m.c points along the normal of the triangle v1, v2, v3
         *
         * Prior values that may be in the matrix vector are disregarded and have no
         * effect on the output values.
         */
        void calctris();

        /**
         * @brief Calculates the ragdolldata's radius and center position
         *
         * Sets the center position to be the average of all the vertices in the ragdoll
         * Sets the radius to be the distance between the center and the farthest vert
         *
         * This is not necessarily the smallest sphere encapsulating all of the points
         * in the vertex array, since that would be the midpoint of the farthest points in
         * the vertex array.
         */
        void calcboundsphere();
        void constrain(vec &cwall);

        /**
         * @brief Adds weights to keep pairs of vertices within specified bounds
         *
         * For each vertex pair in the defined distlimits vector, adds weights to
         * those verts to pull or push those vertices to lie within the specified min/maxdist
         * distance apart.
         *
         * Modifies vertex weights and newpos fields and no other components of ragdolldata.
         */
        void constraindist();

        /**
         * @brief Applies a rotation around a specified axis to a pair of triangles.
         *
         * Modifies the vertices pointed to by the two assigned tri objects by rotating them
         * to the amount indicated by the angle and about the axis specified by axis.
         *
         * For each vertex indicated by the indices in the triangle objects, adds to the
         * newpos vec a value corresponding to the rotation desired. Increments the weight
         * field for each vertex so modified by one. Does not modify the vert's pos or oldpos,
         * only newpos.
         *
         * @param t1 the first triangle to retrieve vertices with
         * @param t2 the second triangle to retrieve vertices with
         * @param angle the angle in radians to rotate by
         * @param axis the axis by which to rotate around
         */
        void applyrotlimit(const ragdollskel::tri &t1, const ragdollskel::tri &t2, float angle, const vec &axis);

        /**
         * @brief Calculates and applies a rotation limit each defined rotation limit
         *
         * For each rotlimit in the associated skeleton, calculates the angle and rotation
         * from that rotlimit and applies it to the associated vertices.
         */
        void constrainrot();

        /**
         * @brief Sets the shadowing elements in rotfrictions according to their respective values in the pointed ragdollskel.
         *
         * For each member of ragdolldata::rotfrictions, sets its value to the transposed multiplication of
         * the data pointed to by the indices of the first two vertices in the ragdollskel::rotfrictions tri data.
         *
         * Previous values in ragdolldata::rotfrictions are ignored and overwritten.
         */
        void calcrotfriction();

        /**
         * @brief Applies rotation friction and propagates it to the model's vertices
         *
         * Calculates rotation limits with applyrotlimit() and then moves the set `newpos`
         * values to the `pos` values in each vert.
         *
         * @param ts the time duration to apply in seconds
         */
        void applyrotfriction(float ts);

        /**
         * @brief Modifies stuck verticies in the vertex array.
         *
         * Only affects those vertices which have the `stuck` field set to `true`.
         *
         * Moves those `stuck` vertices towards the average position of the unstuck vertices,
         * with a magnitude equal to the `speed` parameter.
         *
         * @param speed the magnitude by which to modify stuck vertices
         *
         */
        void tryunstick(float speed, vec &cwall);

        /**
         * @brief Checks collision of `dir` with spherical volume at `pos` with radius `radius`.
         *
         * Checks collision of defined sphere with the cubeworld, in direction `dir`.
         * physics' `collide()` used for collision detection against the cubeworld.
         *
         * @param pos positon of sphere center
         * @param dir direction to check collision against
         * @param radius radius of sphere center
         * @param [out] cwall collision wall data found by the collision check
         *
         * @return true if collision occured
         * @return false if no collision occured
         */
        static bool collidevert(const vec &pos, const vec &dir, float radius, vec &cwall);
};

/**
 * @brief Deletes the heap-allocated ragdoll associated with the passed dynent.
 *
 * If there is no ragdoll associated with `d`, then there is no effect.
 *
 * @param d the dynent to delete the ragdoll of
 */
extern void cleanragdoll(dynent *d);

#endif

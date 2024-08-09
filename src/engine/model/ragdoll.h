#ifndef RAGDOLL_H_
#define RAGDOLL_H_

/* ragdollskel defines a skeletal animation object for use by skelmodel, which
 * is able to be dynamically modified by physics (rather than by an animation file)
 *
 */
class ragdollskel
{
    public:
        ragdollskel() : loaded(false), animjoints(false), eye(-1) {}

        bool loaded, animjoints;

        struct tri
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

        struct reljoint
        {
            int bone, parent;
        };
        std::vector<reljoint> reljoints;

        struct vert
        {
            vec pos;
            float radius, weight;
        };
        std::vector<vert> verts;

        struct joint
        {
            int bone, tri, vert[3];
            float weight;
            matrix4x3 orient;
        };
        std::vector<joint> joints;

        struct rotlimit
        {
            int tri[2];
            float maxangle, maxtrace;
            matrix3 middle;
        };
        std::vector<rotlimit> rotlimits;

        struct rotfriction
        {
            int tri[2];
            matrix3 middle;
        };
        std::vector<rotfriction> rotfrictions;

        struct distlimit
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

class ragdolldata
{
    public:
        ragdollskel *skel;
        int millis, collidemillis, lastmove;
        float radius;
        vec offset, center;

        //shadows the elements in skel->tris, should not be resized after construction
        std::vector<matrix3> tris;

        //shadows the elements in skel->animjoints
        matrix4x3 *animjoints;

        //shadows the elements in skel->reljoints
        dualquat *reljoints;

        struct vert
        {
            vec oldpos, pos, newpos, undo;
            float weight;
            bool collided, stuck;
            vert() : oldpos(0, 0, 0), pos(0, 0, 0), newpos(0, 0, 0), undo(0, 0, 0), weight(0), collided(false), stuck(true) {}

            vert(ragdollskel::vert &v) : oldpos(0, 0, 0), pos(v.pos), newpos(0, 0, 0), undo(0, 0, 0), weight(v.weight), collided(false), stuck(true) {}
        };

        //shadows the elements in skel->verts, should not be resized after construction
        std::vector<vert> verts;

        ragdolldata(ragdollskel *skel, float scale = 1);
        ~ragdolldata();

        void move(bool water, float ts);
        void calcanimjoint(int i, const matrix4x3 &anim);
        void init(const dynent *d);

    private:
        int collisions, floating, unsticks;
        float timestep, scale;

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
        void constrain();
        void constraindist();
        void applyrotlimit(const ragdollskel::tri &t1, const ragdollskel::tri &t2, float angle, const vec &axis);
        void constrainrot();
        void calcrotfriction();
        void applyrotfriction(float ts);
        void tryunstick(float speed);

        bool collidevert(const vec &pos, const vec &dir, float radius);
};

extern void cleanragdoll(dynent *d);

#endif

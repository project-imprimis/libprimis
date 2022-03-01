#ifndef RAGDOLL_H_
#define RAGDOLL_H_

/* ragdollskel defines a skeletal animation object for use by skelmodel, which
 * is able to be dynamically modified by physics (rather than by an animation file
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
        vector<rotlimit> rotlimits;

        struct rotfriction
        {
            int tri[2];
            matrix3 middle;
        };
        vector<rotfriction> rotfrictions;

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
        void setuprotfrictions();
};

class ragdolldata
{
    public:
        ragdollskel *skel;
        int millis, collidemillis, lastmove;
        float radius;
        vec offset, center;
        matrix3 *tris;
        matrix4x3 *animjoints;
        dualquat *reljoints;

        struct vert
        {
            vec oldpos, pos, newpos, undo;
            float weight;
            bool collided, stuck;
            vert() : oldpos(0, 0, 0), pos(0, 0, 0), newpos(0, 0, 0), undo(0, 0, 0), weight(0), collided(false), stuck(true) {}
        };

        vert *verts;

        ragdolldata(ragdollskel *skel, float scale = 1)
            : skel(skel),
              millis(lastmillis),
              collidemillis(0),
              lastmove(lastmillis),
              radius(0),
              tris(new matrix3[skel->tris.size()]),
              animjoints(!skel->animjoints || skel->joints.empty() ? nullptr : new matrix4x3[skel->joints.size()]),
              reljoints(skel->reljoints.empty() ? nullptr : new dualquat[skel->reljoints.size()]),
              verts(new vert[skel->verts.size()]),
              collisions(0),
              floating(0),
              unsticks(INT_MAX),
              timestep(0),
              scale(scale)
        {
        }

        ~ragdolldata()
        {
            delete[] verts;
            delete[] tris;
            if(animjoints)
            {
                delete[] animjoints;
            }
            if(reljoints)
            {
                delete[] reljoints;
            }
        }

        void move(dynent *pl, float ts);
        void calcanimjoint(int i, const matrix4x3 &anim);
        void init(dynent *d);

    private:
        int collisions, floating, unsticks;
        float timestep, scale;

        void calctris();
        void calcboundsphere();
        void constrain();
        void constraindist();
        void applyrotlimit(ragdollskel::tri &t1, ragdollskel::tri &t2, float angle, const vec &axis);
        void constrainrot();
        void calcrotfriction();
        void applyrotfriction(float ts);
        void tryunstick(float speed);

        static inline bool collidevert(const vec &pos, const vec &dir, float radius)
        {
            static struct vertent : physent
            {
                vertent()
                {
                    type = PhysEnt_Bounce;
                    radius = xradius = yradius = eyeheight = aboveeye = 1;
                }
            } v;
            v.o = pos;
            if(v.radius != radius)
            {
                v.radius = v.xradius = v.yradius = v.eyeheight = v.aboveeye = radius;
            }
            return collide(&v, dir, 0, false);
        }
};

extern void cleanragdoll(dynent *d);
extern void moveragdoll(dynent *d);

#endif

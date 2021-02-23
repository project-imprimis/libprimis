#ifndef RAGDOLL_H_
#define RAGDOLL_H_

struct ragdollskel
{
    struct vert
    {
        vec pos;
        float radius, weight;
    };

    struct tri
    {
        int vert[3];

        bool shareverts(const tri &t) const;
    };

    struct distlimit
    {
        int vert[2];
        float mindist, maxdist;
    };

    struct rotlimit
    {
        int tri[2];
        float maxangle, maxtrace;
        matrix3 middle;
    };

    struct rotfriction
    {
        int tri[2];
        matrix3 middle;
    };

    struct joint
    {
        int bone, tri, vert[3];
        float weight;
        matrix4x3 orient;
    };

    struct reljoint
    {
        int bone, parent;
    };

    bool loaded, animjoints;
    int eye;
    vector<vert> verts;
    vector<tri> tris;
    vector<distlimit> distlimits;
    vector<rotlimit> rotlimits;
    vector<rotfriction> rotfrictions;
    vector<joint> joints;
    vector<reljoint> reljoints;

    ragdollskel() : loaded(false), animjoints(false), eye(-1) {}

    void setupjoints();
    void setuprotfrictions();
    void setup();
    void addreljoint(int bone, int parent);

};

struct ragdolldata
{
    struct vert
    {
        vec oldpos, pos, newpos, undo;
        float weight;
        bool collided, stuck;
        vert() : oldpos(0, 0, 0), pos(0, 0, 0), newpos(0, 0, 0), undo(0, 0, 0), weight(0), collided(false), stuck(true) {}
    };

    ragdollskel *skel;
    int millis, collidemillis, collisions, floating, lastmove, unsticks;
    vec offset, center;
    float radius, timestep, scale;
    vert *verts;
    matrix3 *tris;
    matrix4x3 *animjoints;
    dualquat *reljoints;

    ragdolldata(ragdollskel *skel, float scale = 1)
        : skel(skel),
          millis(lastmillis),
          collidemillis(0),
          collisions(0),
          floating(0),
          lastmove(lastmillis),
          unsticks(INT_MAX),
          radius(0),
          timestep(0),
          scale(scale),
          verts(new vert[skel->verts.length()]),
          tris(new matrix3[skel->tris.length()]),
          animjoints(!skel->animjoints || skel->joints.empty() ? NULL : new matrix4x3[skel->joints.length()]),
          reljoints(skel->reljoints.empty() ? NULL : new dualquat[skel->reljoints.length()])
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

    void calcanimjoint(int i, const matrix4x3 &anim);
    void calctris();
    void calcboundsphere();
    void init(dynent *d);
    void move(dynent *pl, float ts);
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

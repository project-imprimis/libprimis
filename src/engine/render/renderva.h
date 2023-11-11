#ifndef RENDERVA_H_
#define RENDERVA_H_

struct vtxarray;

struct occludequery
{
    void *owner;
    GLuint id;
    int fragments;

    void startquery() const;
};

class vfc
{
    public:
        int isfoggedcube(const ivec &o, int size) const;
        int isvisiblecube(const ivec &o, int size) const;
        void visiblecubes(bool cull = true);
        bool isfoggedsphere(float rad, const vec &cv) const;
        int isvisiblesphere(float rad, const vec &cv) const;
        int isvisiblebb(const ivec &bo, const ivec &br) const;
        int cullfrustumsides(const vec &lightpos, float lightradius, float size, float border);
    private:
        static constexpr uint numvfc = 5;
        void calcvfcD();
        void setvfcP(const vec &bbmin = vec(-1, -1, -1), const vec &bbmax = vec(1, 1, 1));

        std::array<plane, numvfc> vfcP;  // perpindictular vectors to view frustrum bounding planes
        float vfcDfog;  // far plane culling distance (fog limit).
        std::array<float, numvfc> vfcDnear,
                             vfcDfar;
};

class Occluder
{
    public:
        void setupmodelquery(occludequery *q);
        void clearqueries();
        void flipqueries();
        void endquery();
        void endmodelquery(); // starts the model query (!)
        bool checkquery(occludequery *query, bool nowait = false);
        void resetqueries();
        int getnumqueries() const;
        occludequery *newquery(void *owner)
        {
            return queryframes[flipquery].newquery(owner);
        }
    private:
        class queryframe
        {
            public:
                int cur;

                queryframe() : cur(0), max(0), defer(0) {}

                void flip();
                occludequery *newquery(void *owner);
                void reset();
                void cleanup();
            private:
                static constexpr int maxquery = 2048;

                int max, defer;
                occludequery queries[maxquery];
        };
        static constexpr int maxqueryframes = 2;
        std::array<queryframe, maxqueryframes> queryframes;
        uint flipquery = 0;

        occludequery *modelquery = nullptr;
        int modelquerybatches = -1,
           modelquerymodels = -1,
           modelqueryattached = -1;

};

extern Occluder occlusionengine;

extern vfc view;

extern int oqfrags;

extern vec shadoworigin, shadowdir;
extern float shadowradius, shadowbias;
extern size_t shadowside;
extern int shadowspot;

extern vtxarray *visibleva;

extern void renderrefractmask();
extern void renderalphageom(int side);
extern void rendermapmodels();
extern void renderoutline();
extern bool renderexplicitsky(bool outline = false);
extern bvec outlinecolor;

extern int deferquery;
extern void startbb(bool mask = true);
extern void endbb(bool mask = true);
extern void drawbb(const ivec &bo, const ivec &br);

extern void renderdecals();

struct shadowmesh;
extern void clearshadowmeshes();
extern void genshadowmeshes();
extern shadowmesh *findshadowmesh(int idx, const extentity &e);
extern void rendershadowmesh(const shadowmesh *m);
extern void dynamicshadowvabounds(int mask, vec &bbmin, vec &bbmax);

#endif

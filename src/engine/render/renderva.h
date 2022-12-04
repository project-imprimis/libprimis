#ifndef RENDERVA_H_
#define RENDERVA_H_

struct vtxarray;

struct occludequery
{
    void *owner;
    GLuint id;
    int fragments;

    void startmodelquery();
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
        void calcvfcD();
        void setvfcP(const vec &bbmin = vec(-1, -1, -1), const vec &bbmax = vec(1, 1, 1));

        plane vfcP[5];  // perpindictular vectors to view frustrum bounding planes
        float vfcDfog;  // far plane culling distance (fog limit).
        float vfcDnear[5], //near plane culling
              vfcDfar[5];  //far plane culling
};

extern vfc view;

extern int oqfrags;

extern vec shadoworigin, shadowdir;
extern float shadowradius, shadowbias;
extern int shadowside, shadowspot;

extern vtxarray *visibleva;

extern void rendergeom();
extern void renderrefractmask();
extern void renderalphageom(int side);
extern void rendermapmodels();
extern void renderoutline();
extern bool renderexplicitsky(bool outline = false);
extern bvec outlinecolor;

extern int deferquery;
extern void flipqueries();
extern occludequery *newquery(void *owner);
extern void endquery();
extern bool checkquery(occludequery *query, bool nowait = false);
extern void resetqueries();
extern int getnumqueries();
extern void startbb(bool mask = true);
extern void endbb(bool mask = true);
extern void drawbb(const ivec &bo, const ivec &br);

extern void renderdecals();

struct shadowmesh;
extern void clearshadowmeshes();
extern void genshadowmeshes();
extern shadowmesh *findshadowmesh(int idx, const extentity &e);
extern void rendershadowmesh(shadowmesh *m);

#endif

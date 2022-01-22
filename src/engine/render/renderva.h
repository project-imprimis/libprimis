#ifndef RENDERVA_H_
#define RENDERVA_H_

struct vtxarray;

class vfc
{
    public:
        int isfoggedcube(const ivec &o, int size);
        int isvisiblecube(const ivec &o, int size);
        void visiblecubes(bool cull = true);
        bool isfoggedsphere(float rad, const vec &cv);
        int isvisiblesphere(float rad, const vec &cv);
        int isvisiblebb(const ivec &bo, const ivec &br);
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

extern int outline;
extern int oqfrags;

extern vec shadoworigin, shadowdir;
extern float shadowradius, shadowbias;
extern int shadowside, shadowspot;

extern float alphafrontsx1, alphafrontsx2, alphafrontsy1, alphafrontsy2, alphabacksx1, alphabacksx2, alphabacksy1, alphabacksy2, alpharefractsx1, alpharefractsx2, alpharefractsy1, alpharefractsy2;
extern uint alphatiles[];
extern vtxarray *visibleva;

extern void rendergeom();
extern int findalphavas();
extern void renderrefractmask();
extern void renderalphageom(int side);
extern void rendermapmodels();
extern void renderoutline();
extern bool renderexplicitsky(bool outline = false);
extern void cleanupva();
extern bvec outlinecolor;

extern bool bboccluded(const ivec &bo, const ivec &br);

extern int deferquery;
extern void flipqueries();
extern occludequery *newquery(void *owner);
extern void startquery(occludequery *query);
extern void endquery();
extern bool checkquery(occludequery *query, bool nowait = false);
extern void resetqueries();
extern int getnumqueries();
extern void startbb(bool mask = true);
extern void endbb(bool mask = true);
extern void drawbb(const ivec &bo, const ivec &br);
extern int calctrisidemask(const vec &p1, const vec &p2, const vec &p3, float bias);

extern void renderdecals();

struct shadowmesh;
extern void clearshadowmeshes();
extern void genshadowmeshes();
extern shadowmesh *findshadowmesh(int idx, extentity &e);
extern void rendershadowmesh(shadowmesh *m);

#endif

#ifndef RENDERMODEL_H_
#define RENDERMODEL_H_

struct mapmodelinfo { string name; model *m, *collide; };

extern vector<mapmodelinfo> mapmodels;

std::vector<int> findanims(const char *pattern);

extern float transmdlsx1, transmdlsy1, transmdlsx2, transmdlsy2;
extern uint transmdltiles[lighttilemaxheight];

extern void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks);
extern model *loadmodel(const char *name, int i = -1, bool msg = false);
extern void resetmodelbatches();
extern void startmodelquery(occludequery *query);
extern void endmodelquery();
extern void rendershadowmodelbatches(bool dynmodel = true);
extern void shadowmaskbatchedmodels(bool dynshadow = true);
extern void rendermapmodelbatches();
extern void rendermodelbatches();
extern void rendertransparentmodelbatches(int stencil = 0);
extern void rendermodel(const char *mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int cull = Model_CullVFC | Model_CullDist | Model_CullOccluded, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float size = 1, const vec4 &color = vec4(1, 1, 1, 1));
extern void rendermapmodel(int idx, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int flags = Model_CullVFC | Model_CullDist, int basetime = 0, float size = 1);
extern void clearbatchedmapmodels();
extern int batcheddynamicmodels();
extern int batcheddynamicmodelbounds(int mask, vec &bbmin, vec &bbmax);
extern void cleanupmodels();
extern model *loadmapmodel(int n);
extern std::vector<int> findanims(const char *pattern);

inline mapmodelinfo *getmminfo(int n) { return mapmodels.inrange(n) ? &mapmodels[n] : NULL; }

#endif

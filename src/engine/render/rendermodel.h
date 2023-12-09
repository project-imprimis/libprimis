#ifndef RENDERMODEL_H_
#define RENDERMODEL_H_

struct mapmodelinfo
{
    std::string name;
    model *m,
          *collide;
};

extern std::vector<mapmodelinfo> mapmodels;
extern std::vector<std::string> animnames; //set by game at runtime

extern void loadskin(const std::string &dir, const std::string &altdir, Texture *&skin, Texture *&masks);
extern model *loadmodel(const char *name, int i = -1, bool msg = false);
extern void resetmodelbatches();
extern void rendershadowmodelbatches(bool dynmodel = true);
extern void shadowmaskbatchedmodels(bool dynshadow = true);
extern void rendermapmodelbatches();
extern void rendertransparentmodelbatches(int stencil = 0);
extern void rendermodel(const char *mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int cull = Model_CullVFC | Model_CullDist | Model_CullOccluded, dynent *d = nullptr, modelattach *a = nullptr, int basetime = 0, int basetime2 = 0, float size = 1, const vec4<float> &color = vec4<float>(1, 1, 1, 1));
extern void rendermapmodel(int idx, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int flags = Model_CullVFC | Model_CullDist, int basetime = 0, float size = 1);
extern void clearbatchedmapmodels();
extern int batcheddynamicmodels();
extern int batcheddynamicmodelbounds(int mask, vec &bbmin, vec &bbmax);
extern void cleanupmodels();
extern model *loadmapmodel(int n);
extern std::vector<size_t> findanims(const char *pattern);

#endif

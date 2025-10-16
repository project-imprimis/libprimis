#ifndef RENDERMODEL_H_
#define RENDERMODEL_H_

struct MapModelInfo final
{
    std::string name;
    model *m,
          *collide;
};

namespace mapmodel
{
    extern std::vector<MapModelInfo> mapmodels;
}

namespace batching
{
    extern void resetmodelbatches();
    extern void rendershadowmodelbatches(bool dynmodel = true);
    extern void shadowmaskbatchedmodels(bool dynshadow = true);
    extern void rendermapmodelbatches();
    extern void rendertransparentmodelbatches(int stencil = 0);
    extern void clearbatchedmapmodels();
    extern int batcheddynamicmodels();
    extern int batcheddynamicmodelbounds(int mask, vec &bbmin, vec &bbmax);
}

extern std::vector<std::string> animnames; //set by game at runtime

extern void loadskin(const std::string &dir, const std::string &altdir, Texture *&skin, Texture *&masks);
extern model *loadmodel(std::string_view, int i = -1, bool msg = false);
extern void rendermodel(std::string_view mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int cull = Model_CullVFC | Model_CullDist | Model_CullOccluded, dynent *d = nullptr, modelattach *a = nullptr, int basetime = 0, int basetime2 = 0, float size = 1, const vec4<float> &color = vec4<float>(1, 1, 1, 1));
extern void rendermapmodel(int idx, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int flags = Model_CullVFC | Model_CullDist, int basetime = 0, float size = 1);

extern void cleanupmodels();

/**
 * @brief Gets or loads a model by index.
 *
 * Gets the mapmodel at the nth index in the mapmodels vector. If there is no entry
 * at that index (but that index is valid) then a new empty model will be created.
 *
 * If the index passed is too large then nullptr is returned and no model is created.
 * Therefore, the length of the mapmodels vector is invariant regardless of input.
 *
 * @param n the index to load
 *
 * @return pointer to the model at index n, or nullptr
 */
extern model *loadmapmodel(int n);
extern std::vector<size_t> findanims(std::string_view pattern);

#endif

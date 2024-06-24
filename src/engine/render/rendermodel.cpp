/* rendermodel.cpp: world static and dynamic models
 *
 * Libprimis can handle static ("mapmodel") type models which are placed in levels
 * as well as dynamic, animated models such as players or other actors. For animated
 * models, the md5 model format is supported; simpler static models can use the
 * common Wavefront (obj) model format.
 *
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include <optional>
#include <memory>

#include "aa.h"
#include "csm.h"
#include "radiancehints.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "renderva.h"
#include "renderwindow.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/cs.h"

#include "world/entities.h"
#include "world/octaedit.h"
#include "world/octaworld.h"
#include "world/bih.h"
#include "world/world.h"

VAR(oqdynent, 0, 1, 1); //occlusion query dynamic ents

std::vector<std::string> animnames; //set by game at runtime

//need the above vars inited before these headers will load properly

#include "model/model.h"
#include "model/ragdoll.h"
#include "model/animmodel.h"
#include "model/vertmodel.h"
#include "model/skelmodel.h"

model *loadmapmodel(int n)
{
    if(static_cast<int>(mapmodels.size()) > n)
    {
        model *m = mapmodels[n].m;
        return m ? m : loadmodel(nullptr, n);
    }
    return nullptr;
}

//need the above macros & fxns inited before these headers will load properly
#include "model/md5.h"
#include "model/obj.h"
#include "model/gltf.h"

// mapmodels

std::vector<mapmodelinfo> mapmodels;
static const char * const mmprefix = "mapmodel/";
static const int mmprefixlen = std::strlen(mmprefix);

void mapmodel(const char *name)
{
    mapmodelinfo mmi;
    if(name[0])
    {
        mmi.name = std::string().append(mmprefix).append(name);
    }
    else
    {
        mmi.name.clear();
    }
    mmi.m = mmi.collide = nullptr;
    mapmodels.push_back(mmi);
}

void mapmodelreset(const int *n)
{
    if(!(identflags&Idf_Overridden) && !allowediting)
    {
        return;
    }
    mapmodels.resize(std::clamp(*n, 0, static_cast<int>(mapmodels.size())));
}

const char *mapmodelname(int i)
{
    return (static_cast<int>(mapmodels.size()) > i) ? mapmodels[i].name.c_str() : nullptr;
}

void mapmodelnamecmd(const int *index, const int *prefix)
{
    if(static_cast<int>(mapmodels.size()) > *index)
    {
        result(mapmodels[*index].name.empty() ? mapmodels[*index].name.c_str() + (*prefix ? 0 : mmprefixlen) : "");
    }
}

void mapmodelloaded(const int *index)
{
    intret(static_cast<int>(mapmodels.size()) > *index && mapmodels[*index].m ? 1 : 0);
}

void nummapmodels()
{
    intret(mapmodels.size());
}

// model registry

std::unordered_map<std::string, model *> models;
std::vector<std::string> preloadmodels;

//used in iengine
void preloadmodel(std::string name)
{
    if(name.empty() || models.find(name) != models.end() || std::find(preloadmodels.begin(), preloadmodels.end(), name) != preloadmodels.end() )
    {
        return;
    }
    preloadmodels.push_back(name);
}

void flushpreloadedmodels(bool msg)
{
    for(uint i = 0; i < preloadmodels.size(); i++)
    {
        loadprogress = static_cast<float>(i+1)/preloadmodels.size();
        model *m = loadmodel(preloadmodels[i].c_str(), -1, msg);
        if(!m)
        {
            if(msg)
            {
                conoutf(Console_Warn, "could not load model: %s", preloadmodels[i].c_str());
            }
        }
        else
        {
            m->preloadmeshes();
            m->preloadshaders();
        }
    }
    preloadmodels.clear();

    loadprogress = 0;
}

void preloadusedmapmodels(bool msg, bool bih)
{
    std::vector<extentity *> &ents = entities::getents();
    std::vector<int> used;
    for(extentity *&e : ents)
    {
        if(e->type==EngineEnt_Mapmodel && e->attr1 >= 0 && std::find(used.begin(), used.end(), e->attr1) != used.end() )
        {
            used.push_back(e->attr1);
        }
    }

    std::vector<std::string> col;
    for(uint i = 0; i < used.size(); i++)
    {
        loadprogress = static_cast<float>(i+1)/used.size();
        int mmindex = used[i];
        if(!(static_cast<int>(mapmodels.size()) > (mmindex)))
        {
            if(msg)
            {
                conoutf(Console_Warn, "could not find map model: %d", mmindex);
            }
            continue;
        }
        const mapmodelinfo &mmi = mapmodels[mmindex];
        if(mmi.name.empty())
        {
            continue;
        }
        model *m = loadmodel(nullptr, mmindex, msg);
        if(!m)
        {
            if(msg)
            {
                conoutf(Console_Warn, "could not load map model: %s", mmi.name.c_str());
            }
        }
        else
        {
            if(bih)
            {
                m->preloadBIH();
            }
            else if(m->collide == Collide_TRI && m->collidemodel.empty() && m->bih)
            {
                m->setBIH();
            }
            m->preloadmeshes();
            m->preloadshaders();
            if(!m->collidemodel.empty() && std::find(col.begin(), col.end(), m->collidemodel) == col.end())
            {
                col.push_back(m->collidemodel);
            }
        }
    }

    for(uint i = 0; i < col.size(); i++)
    {
        loadprogress = static_cast<float>(i+1)/col.size();
        model *m = loadmodel(col[i].c_str(), -1, msg);
        if(!m)
        {
            if(msg)
            {
                conoutf(Console_Warn, "could not load collide model: %s", col[i].c_str());
            }
        }
        else if(!m->bih)
        {
            m->setBIH();
        }
    }

    loadprogress = 0;
}

model *loadmodel(const char *name, int i, bool msg)
{
    model *(__cdecl *md5loader)(const std::string &filename) = +[] (const std::string &filename) -> model* { return new md5(filename); };
    model *(__cdecl *objloader)(const std::string &filename) = +[] (const std::string &filename) -> model* { return new obj(filename); };
    model *(__cdecl *gltfloader)(const std::string &filename) = +[] (const std::string &filename) -> model* { return new gltf(filename); };

    std::vector<model *(__cdecl *)(const std::string &)> loaders;
    loaders.push_back(md5loader);
    loaders.push_back(objloader);
    loaders.push_back(gltfloader);
    std::unordered_set<std::string> failedmodels;

    if(!name)
    {
        if(!(static_cast<int>(mapmodels.size()) > i))
        {
            return nullptr;
        }
        const mapmodelinfo &mmi = mapmodels[i];
        if(mmi.m)
        {
            return mmi.m;
        }
        name = mmi.name.c_str();
    }
    auto itr = models.find(name);
    model *m;
    if(itr != models.end())
    {
        m = (*models.find(name)).second;
    }
    else
    {
        if(!name[0] || failedmodels.find(name) != failedmodels.end())
        {
            return nullptr;
        }
        if(msg)
        {
            std::string filename;
            filename.append(modelpath).append(name);
            renderprogress(loadprogress, filename.c_str());
        }
        for(model *(__cdecl *i)(const std::string &) : loaders)
        {
            m = i(name); //call model ctor
            if(!m)
            {
                continue;
            }
            if(m->load()) //now load the model
            {
                break;
            }
            //delete model if not successful
            delete m;
            m = nullptr;
        }
        if(!m)
        {
            failedmodels.insert(name);
            return nullptr;
        }
        if(models.find(m->modelname()) == models.end())
        {
            models[m->modelname()] = m;
        }
    }
    if((mapmodels.size() > static_cast<uint>(i)) && !mapmodels[i].m)
    {
        mapmodels[i].m = m;
    }
    return m;
}

//used in iengine.h
void clear_models()
{
    for(auto [k, i] : models)
    {
        delete i;
    }
}

void cleanupmodels()
{
    for(auto [k, i] : models)
    {
        i->cleanup();
    }
}

static void clearmodel(const char *name)
{
    model *m = nullptr;
    auto it = models.find(name);
    if(it != models.end())
    {
        m = (*it).second;
    }
    if(!m)
    {
        conoutf("model %s is not loaded", name);
        return;
    }
    for(mapmodelinfo &mmi : mapmodels)
    {
        if(mmi.m == m)
        {
            mmi.m = nullptr;
        }
        if(mmi.collide == m)
        {
            mmi.collide = nullptr;
        }
    }
    models.erase(name);
    m->cleanup();
    delete m;
    conoutf("cleared model %s", name);
}

static bool modeloccluded(const vec &center, float radius)
{
    ivec bbmin(vec(center).sub(radius)),
         bbmax(vec(center).add(radius+1));
    return rootworld.bboccluded(bbmin, bbmax);
}

struct batchedmodel
{
    //orient = yaw, pitch, roll
    vec pos, orient, center;
    float radius, sizescale;
    vec4<float> colorscale;
    int anim, basetime, basetime2, flags, attached;
    union
    {
        int visible;
        int culled;
    };
    dynent *d;
    int next;
};
struct modelbatch
{
    const model *m;
    int flags, batched;
};
static std::vector<batchedmodel> batchedmodels;
static std::vector<modelbatch> batches;
static std::vector<modelattach> modelattached;

void resetmodelbatches()
{
    batchedmodels.clear();
    batches.clear();
    modelattached.clear();
}

void addbatchedmodel(model *m, batchedmodel &bm, int idx)
{
    modelbatch *b = nullptr;
    if(batches.size() > static_cast<uint>(m->batch))
    {
        b = &batches[m->batch];
        if(b->m == m && (b->flags & Model_Mapmodel) == (bm.flags & Model_Mapmodel))
        {
            goto foundbatch; //skip some shit
        }
    }
    m->batch = batches.size();
    batches.emplace_back();
    b = &batches.back();
    b->m = m;
    b->flags = 0;
    b->batched = -1;

foundbatch:
    b->flags |= bm.flags;
    bm.next = b->batched;
    b->batched = idx;
}

static void renderbatchedmodel(const model *m, const batchedmodel &b)
{
    modelattach *a = nullptr;
    if(b.attached>=0)
    {
        a = &modelattached[b.attached];
    }
    int anim = b.anim;
    if(shadowmapping > ShadowMap_Reflect)
    {
        anim |= Anim_NoSkin;
    }
    else
    {
        if(b.flags&Model_FullBright)
        {
            anim |= Anim_FullBright;
        }
    }

    m->render(anim, b.basetime, b.basetime2, b.pos, b.orient.x, b.orient.y, b.orient.z, b.d, a, b.sizescale, b.colorscale);
}

//ratio between model size and distance at which to cull: at 200, model must be 200 times smaller than distance to model
VAR(maxmodelradiusdistance, 10, 200, 1000);

static void rendercullmodelquery(const model *m, dynent *d, const vec &center, float radius)
{
    if(std::fabs(camera1->o.x-center.x) < radius+1 &&
       std::fabs(camera1->o.y-center.y) < radius+1 &&
       std::fabs(camera1->o.z-center.z) < radius+1)
    {
        d->query = nullptr;
        return;
    }
    d->query = occlusionengine.newquery(d);
    if(!d->query)
    {
        return;
    }
    d->query->startquery();
    int br = static_cast<int>(radius*2)+1;
    drawbb(ivec(static_cast<float>(center.x-radius), static_cast<float>(center.y-radius), static_cast<float>(center.z-radius)), ivec(br, br, br));
    occlusionengine.endquery();
}

static int cullmodel(const model *m, const vec &center, float radius, int flags, dynent *d = nullptr)
{
    if(flags&Model_CullDist && (center.dist(camera1->o) / radius) > maxmodelradiusdistance)
    {
        return Model_CullDist;
    }
    if(flags&Model_CullVFC && view.isfoggedsphere(radius, center))
    {
        return Model_CullVFC;
    }
    if(flags&Model_CullOccluded && modeloccluded(center, radius))
    {
        return Model_CullOccluded;
    }
    else if(flags&Model_CullQuery && d->query && d->query->owner==d && occlusionengine.checkquery(d->query))
    {
        return Model_CullQuery;
    }
    return 0;
}

static int shadowmaskmodel(const vec &center, float radius)
{
    switch(shadowmapping)
    {
        case ShadowMap_Reflect:
            return calcspherersmsplits(center, radius);
        case ShadowMap_CubeMap:
        {
            vec scenter = vec(center).sub(shadoworigin);
            float sradius = radius + shadowradius;
            if(scenter.squaredlen() >= sradius*sradius)
            {
                return 0;
            }
            return calcspheresidemask(scenter, radius, shadowbias);
        }
        case ShadowMap_Cascade:
        {
            return csm.calcspherecsmsplits(center, radius);
        }
        case ShadowMap_Spot:
        {
            vec scenter = vec(center).sub(shadoworigin);
            float sradius = radius + shadowradius;
            return scenter.squaredlen() < sradius*sradius && sphereinsidespot(shadowdir, shadowspot, scenter, radius) ? 1 : 0;
        }
    }
    return 0;
}

void shadowmaskbatchedmodels(bool dynshadow)
{
    for(batchedmodel &b : batchedmodels)
    {
        if(b.flags&(Model_Mapmodel | Model_NoShadow)) //mapmodels are not dynamic models by definition
        {
            break;
        }
        b.visible = dynshadow && (b.colorscale.a >= 1 || b.flags&(Model_OnlyShadow | Model_ForceShadow)) ? shadowmaskmodel(b.center, b.radius) : 0;
    }
}

int batcheddynamicmodels()
{
    int visible = 0;
    for(const batchedmodel &b : batchedmodels)
    {
        if(b.flags&Model_Mapmodel) //mapmodels are not dynamic models by definition
        {
            break;
        }
        visible |= b.visible;
    }
    for(const modelbatch &b : batches)
    {
        if(!(b.flags&Model_Mapmodel) || !b.m->animated())
        {
            continue;
        }
        for(int j = b.batched; j >= 0;)
        {
            const batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            visible |= bm.visible;
        }
    }
    return visible;
}

int batcheddynamicmodelbounds(int mask, vec &bbmin, vec &bbmax)
{
    int vis = 0;
    for(const batchedmodel &b : batchedmodels)
    {
        if(b.flags&Model_Mapmodel) //mapmodels are not dynamic models by definition
        {
            break;
        }
        if(b.visible&mask)
        {
            bbmin.min(vec(b.center).sub(b.radius));
            bbmax.max(vec(b.center).add(b.radius));
            ++vis;
        }
    }
    for(const modelbatch &b : batches)
    {
        if(!(b.flags&Model_Mapmodel) || !b.m->animated())
        {
            continue;
        }
        for(int j = b.batched; j >= 0;)
        {
            const batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            if(bm.visible&mask)
            {
                bbmin.min(vec(bm.center).sub(bm.radius));
                bbmax.max(vec(bm.center).add(bm.radius));
                ++vis;
            }
        }
    }
    return vis;
}

void rendershadowmodelbatches(bool dynmodel)
{
    for(const modelbatch &b : batches)
    {
        if(!b.m->shadow || (!dynmodel && (!(b.flags&Model_Mapmodel) || b.m->animated())))
        {
            continue;
        }
        bool rendered = false;
        for(int j = b.batched; j >= 0;)
        {
            const batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            if(!(bm.visible&(1<<shadowside)))
            {
                continue;
            }
            if(!rendered)
            {
                b.m->startrender();
                rendered = true;
            }
            renderbatchedmodel(b.m, bm);
        }
        if(rendered)
        {
            b.m->endrender();
        }
    }
}

void rendermapmodelbatches()
{
    aamask::enable();
    for(const modelbatch &b : batches)
    {
        if(!(b.flags&Model_Mapmodel))
        {
            continue;
        }
        b.m->startrender();
        aamask::set(b.m->animated());
        for(int j = b.batched; j >= 0;)
        {
            const batchedmodel &bm = batchedmodels[j];
            renderbatchedmodel(b.m, bm);
            j = bm.next;
        }
        b.m->endrender();
    }
    aamask::disable();
}

void GBuffer::rendermodelbatches()
{
    tmodelinfo.mdlsx1 = tmodelinfo.mdlsy1 = 1;
    tmodelinfo.mdlsx2 = tmodelinfo.mdlsy2 = -1;
    tmodelinfo.mdltiles.fill(0);

    aamask::enable();
    for(const modelbatch &b : batches)
    {
        if(b.flags&Model_Mapmodel)
        {
            continue;
        }
        bool rendered = false;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            bm.culled = cullmodel(b.m, bm.center, bm.radius, bm.flags, bm.d);
            if(bm.culled || bm.flags&Model_OnlyShadow)
            {
                continue;
            }
            if(bm.colorscale.a < 1 || bm.flags&Model_ForceTransparent)
            {
                float sx1, sy1, sx2, sy2;
                ivec bbmin(vec(bm.center).sub(bm.radius)), bbmax(vec(bm.center).add(bm.radius+1));
                if(calcbbscissor(bbmin, bbmax, sx1, sy1, sx2, sy2))
                {
                    tmodelinfo.mdlsx1 = std::min(tmodelinfo.mdlsx1, sx1);
                    tmodelinfo.mdlsy1 = std::min(tmodelinfo.mdlsy1, sy1);
                    tmodelinfo.mdlsx2 = std::max(tmodelinfo.mdlsx2, sx2);
                    tmodelinfo.mdlsy2 = std::max(tmodelinfo.mdlsy2, sy2);
                    masktiles(tmodelinfo.mdltiles.data(), sx1, sy1, sx2, sy2);
                }
                continue;
            }
            if(!rendered)
            {
                b.m->startrender();
                rendered = true;
                aamask::set(true);
            }
            if(bm.flags&Model_CullQuery)
            {
                bm.d->query = occlusionengine.newquery(bm.d);
                if(bm.d->query)
                {
                    bm.d->query->startquery();
                    renderbatchedmodel(b.m, bm);
                    occlusionengine.endquery();
                    continue;
                }
            }
            renderbatchedmodel(b.m, bm);
        }
        if(rendered)
        {
            b.m->endrender();
        }
        if(b.flags&Model_CullQuery)
        {
            bool queried = false;
            for(int j = b.batched; j >= 0;)
            {
                batchedmodel &bm = batchedmodels[j];
                j = bm.next;
                if(bm.culled&(Model_CullOccluded|Model_CullQuery) && bm.flags&Model_CullQuery)
                {
                    if(!queried)
                    {
                        if(rendered)
                        {
                            aamask::set(false);
                        }
                        startbb();
                        queried = true;
                    }
                    rendercullmodelquery(b.m, bm.d, bm.center, bm.radius);
                }
            }
            if(queried)
            {
                endbb();
            }
        }
    }
    aamask::disable();
}

void rendertransparentmodelbatches(int stencil)
{
    aamask::enable(stencil);
    for(modelbatch &b : batches)
    {
        if(b.flags&Model_Mapmodel)
        {
            continue;
        }
        bool rendered = false;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            bm.culled = cullmodel(b.m, bm.center, bm.radius, bm.flags, bm.d);
            if(bm.culled || !(bm.colorscale.a < 1 || bm.flags&Model_ForceTransparent) || bm.flags&Model_OnlyShadow)
            {
                continue;
            }
            if(!rendered)
            {
                b.m->startrender();
                rendered = true;
                aamask::set(true);
            }
            if(bm.flags&Model_CullQuery)
            {
                bm.d->query = occlusionengine.newquery(bm.d);
                if(bm.d->query)
                {
                    bm.d->query->startquery();
                    renderbatchedmodel(b.m, bm);
                    occlusionengine.endquery();
                    continue;
                }
            }
            renderbatchedmodel(b.m, bm);
        }
        if(rendered)
        {
            b.m->endrender();
        }
    }
    aamask::disable();
}

void Occluder::setupmodelquery(occludequery *q)
{
    modelquery = q;
    modelquerybatches = batches.size();
    modelquerymodels = batchedmodels.size();
    modelqueryattached = modelattached.size();
}

void Occluder::endmodelquery()
{
    if(static_cast<int>(batchedmodels.size()) == modelquerymodels)
    {
        modelquery->fragments = 0;
        modelquery = nullptr;
        return;
    }
    aamask::enable();
    modelquery->startquery();
    for(modelbatch &b : batches)
    {
        int j = b.batched;
        if(j < modelquerymodels)
        {
            continue;
        }
        b.m->startrender();
        aamask::set(!(b.flags&Model_Mapmodel) || b.m->animated());
        do
        {
            const batchedmodel &bm = batchedmodels[j];
            renderbatchedmodel(b.m, bm);
            j = bm.next;
        } while(j >= modelquerymodels);
        b.batched = j;
        b.m->endrender();
    }
    occlusionengine.endquery();
    modelquery = nullptr;
    batches.resize(modelquerybatches);
    batchedmodels.resize(modelquerymodels);
    modelattached.resize(modelqueryattached);
    aamask::disable();
}

void clearbatchedmapmodels()
{
    for(uint i = 0; i < batches.size(); i++)
    {
        const modelbatch &b = batches[i];
        if(b.flags&Model_Mapmodel)
        {
            batchedmodels.resize(b.batched);
            batches.resize(i);
            break;
        }
    }
}

void rendermapmodel(int idx, int anim, const vec &o, float yaw, float pitch, float roll, int flags, int basetime, float size)
{
    if(!(static_cast<int>(mapmodels.size()) > idx))
    {
        return;
    }
    const mapmodelinfo &mmi = mapmodels[idx];
    model *m = mmi.m ? mmi.m : loadmodel(mmi.name.c_str());
    if(!m)
    {
        return;
    }
    vec center, bbradius;
    m->boundbox(center, bbradius);
    float radius = bbradius.magnitude();
    center.mul(size);
    if(roll)
    {
        center.rotate_around_y(roll/RAD);
    }
    if(pitch && m->pitched())
    {
        center.rotate_around_x(pitch/RAD);
    }
    center.rotate_around_z(yaw/RAD);
    center.add(o);
    radius *= size;

    int visible = 0;
    if(shadowmapping)
    {
        if(!m->shadow)
        {
            return;
        }
        visible = shadowmaskmodel(center, radius);
        if(!visible)
        {
            return;
        }
    }
    else if(flags&(Model_CullVFC|Model_CullDist|Model_CullOccluded) && cullmodel(m, center, radius, flags))
    {
        return;
    }
    batchedmodels.emplace_back();
    batchedmodel &b = batchedmodels.back();
    b.pos = o;
    b.center = center;
    b.radius = radius;
    b.anim = anim;
    b.orient = {yaw, pitch, roll};
    b.basetime = basetime;
    b.basetime2 = 0;
    b.sizescale = size;
    b.colorscale = vec4<float>(1, 1, 1, 1);
    b.flags = flags | Model_Mapmodel;
    b.visible = visible;
    b.d = nullptr;
    b.attached = -1;
    addbatchedmodel(m, b, batchedmodels.size()-1);
}

void rendermodel(const char *mdl, int anim, const vec &o, float yaw, float pitch, float roll, int flags, dynent *d, modelattach *a, int basetime, int basetime2, float size, const vec4<float> &color)
{
    model *m = loadmodel(mdl);
    if(!m)
    {
        return;
    }

    vec center, bbradius;
    m->boundbox(center, bbradius);
    float radius = bbradius.magnitude();
    if(d)
    {
        if(d->ragdoll)
        {
            if(anim & Anim_Ragdoll && d->ragdoll->millis >= basetime)
            {
                radius = std::max(radius, d->ragdoll->radius);
                center = d->ragdoll->center;
                goto hasboundbox; //skip roll and pitch stuff
            }
            if(d->ragdoll)
            {
                delete d->ragdoll;
                d->ragdoll = nullptr;
            }
        }
        if(anim & Anim_Ragdoll)
        {
            flags &= ~(Model_CullVFC | Model_CullOccluded | Model_CullQuery);
        }
    }
    center.mul(size);
    if(roll)
    {
        center.rotate_around_y(roll/RAD);
    }
    if(pitch && m->pitched())
    {
        center.rotate_around_x(pitch/RAD);
    }
    center.rotate_around_z(yaw/RAD);
    center.add(o);
hasboundbox:
    radius *= size;

    if(flags&Model_NoRender)
    {
        anim |= Anim_NoRender;
    }

    if(a)
    {
        for(int i = 0; a[i].tag; i++)
        {
            if(a[i].name)
            {
                a[i].m = loadmodel(a[i].name);
            }
        }
    }

    if(flags&Model_CullQuery)
    {
        if(!oqfrags || !oqdynent || !d)
        {
            flags &= ~Model_CullQuery;
        }
    }

    if(flags&Model_NoBatch)
    {
        int culled = cullmodel(m, center, radius, flags, d);
        if(culled)
        {
            if(culled&(Model_CullOccluded|Model_CullQuery) && flags&Model_CullQuery)
            {
                startbb();
                rendercullmodelquery(m, d, center, radius);
                endbb();
            }
            return;
        }
        aamask::enable();
        if(flags&Model_CullQuery)
        {
            d->query = occlusionengine.newquery(d);
            if(d->query)
            {
                d->query->startquery();
            }
        }
        m->startrender();
        aamask::set(true);
        if(flags&Model_FullBright)
        {
            anim |= Anim_FullBright;
        }
        m->render(anim, basetime, basetime2, o, yaw, pitch, roll, d, a, size, color);
        m->endrender();
        if(flags&Model_CullQuery && d->query)
        {
            occlusionengine.endquery();
        }
        aamask::disable();
        return;
    }

    batchedmodels.emplace_back();
    batchedmodel &b = batchedmodels.back();
    b.pos = o;
    b.center = center;
    b.radius = radius;
    b.anim = anim;
    b.orient = {yaw, pitch, roll};
    b.basetime = basetime;
    b.basetime2 = basetime2;
    b.sizescale = size;
    b.colorscale = color;
    b.flags = flags;
    b.visible = 0;
    b.d = d;
    b.attached = a ? modelattached.size() : -1;
    if(a)
    {
        for(int i = 0;; i++)
        {
            modelattached.push_back(a[i]);
            if(!a[i].tag)
            {
                break;
            }
        }
    }
    addbatchedmodel(m, b, batchedmodels.size()-1);
}

int intersectmodel(const char *mdl, int anim, const vec &pos, float yaw, float pitch, float roll, const vec &o, const vec &ray, float &dist, int mode, dynent *d, modelattach *a, int basetime, int basetime2, float size)
{
    const model *m = loadmodel(mdl);
    if(!m)
    {
        return -1;
    }
    if(d && d->ragdoll && (!(anim & Anim_Ragdoll) || d->ragdoll->millis < basetime))
    {
        if(d->ragdoll)
        {
            delete d->ragdoll;
            d->ragdoll = nullptr;
        }
    }
    if(a)
    {
        for(int i = 0; a[i].tag; i++)
        {
            if(a[i].name)
            {
                a[i].m = loadmodel(a[i].name);
            }
        }
    }
    return m->intersect(anim, basetime, basetime2, pos, yaw, pitch, roll, d, a, size, o, ray, dist);
}

void abovemodel(vec &o, const char *mdl)
{
    model *m = loadmodel(mdl);
    if(!m)
    {
        return;
    }
    o.z += m->above();
}

std::vector<size_t> findanims(const char *pattern)
{
    std::vector<size_t> anims;
    for(size_t i = 0; i < animnames.size(); ++i)
    {
        if(!animnames.at(i).compare(pattern))
        {
            anims.push_back(i);
        }
    }
    return anims;
}

void findanimscmd(char *name)
{
    std::vector<size_t> anims = findanims(name);
    std::vector<char> buf;
    string num;
    for(size_t i = 0; i < anims.size(); i++)
    {
        formatstring(num, "%lu", anims[i]);
        if(i > 0)
        {
            buf.push_back(' ');
        }
        for(uint i = 0; i < std::strlen(num); ++i)
        {
            buf.push_back(num[i]);
        }
    }
    buf.push_back('\0');
    result(buf.data());
}

void loadskin(const std::string &dir, const std::string &altdir, Texture *&skin, Texture *&masks) // model skin sharing
{
    //goes and attempts a textureload for png, jpg four times using the cascading if statements, first for default then for alt directory
    static auto tryload = [] (Texture *tex, std::string name, const std::string &mdir, const std::string &maltdir) -> bool
    {
        if((tex = textureload(makerelpath(mdir.c_str(), name.append(".jpg").c_str(), nullptr, nullptr), 0, true, false))==notexture)
        {
            if((tex = textureload(makerelpath(mdir.c_str(), name.append(".png").c_str(), nullptr, nullptr), 0, true, false))==notexture)
            {
                if((tex = textureload(makerelpath(maltdir.c_str(), name.append(".jpg").c_str(), nullptr, nullptr), 0, true, false))==notexture)
                {
                    if((tex = textureload(makerelpath(maltdir.c_str(), name.append(".png").c_str(), nullptr, nullptr), 0, true, false))==notexture)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    std::string mdir,
                maltdir;
    mdir.append(modelpath).append(dir);
    mdir.append(modelpath).append(altdir);
    masks = notexture;
    if(tryload(skin, "skin", mdir, maltdir))
    {
        return;
    }
    if(tryload(masks, "masks", mdir, maltdir))
    {
        return;
    }
}

void setbbfrommodel(dynent *d, const char *mdl)
{
    model *m = loadmodel(mdl);
    if(!m)
    {
        return;
    }
    vec center, radius;
    m->collisionbox(center, radius);
    if(m->collide != Collide_Ellipse)
    {
        d->collidetype = Collide_OrientedBoundingBox;
    }
    d->xradius   = radius.x + std::fabs(center.x);
    d->yradius   = radius.y + std::fabs(center.y);
    d->radius    = d->collidetype==Collide_OrientedBoundingBox ? sqrtf(d->xradius*d->xradius + d->yradius*d->yradius) : std::max(d->xradius, d->yradius);
    d->eyeheight = (center.z-radius.z) + radius.z*2*m->eyeheight;
    d->aboveeye  = radius.z*2*(1.0f-m->eyeheight);
    if (d->aboveeye + d->eyeheight <= 0.5f)
    {
        float zrad = (0.5f - (d->aboveeye + d->eyeheight)) / 2;
        d->aboveeye += zrad;
        d->eyeheight += zrad;
    }
}

void initrendermodelcmds()
{
    addcommand("mapmodelreset", reinterpret_cast<identfun>(mapmodelreset), "i", Id_Command);
    addcommand("mapmodel", reinterpret_cast<identfun>(mapmodel), "s", Id_Command);
    addcommand("mapmodelname", reinterpret_cast<identfun>(mapmodelnamecmd), "ii", Id_Command);
    addcommand("mapmodelloaded", reinterpret_cast<identfun>(mapmodelloaded), "i", Id_Command);
    addcommand("nummapmodels", reinterpret_cast<identfun>(nummapmodels), "", Id_Command);
    addcommand("clearmodel", reinterpret_cast<identfun>(clearmodel), "s", Id_Command);
    addcommand("findanims", reinterpret_cast<identfun>(findanimscmd), "s", Id_Command);
}

/* animmodel.cpp: implementation for animated models
 *
 * animmodel.cpp implements the animmodel object in animmodel.h and is the
 * working form of an an animated model loaded from a file. The animmodel object
 * supports skeletal animation along with the basics it inherits from model.h;
 * see that file for non-animated functionality (e.g. normal, specular, etc mapping,
 * position, orientation, etc).
 *
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include <memory>
#include <optional>

#include "interface/console.h"
#include "interface/control.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"
#include "render/renderlights.h"
#include "render/rendermodel.h"
#include "render/renderparticles.h"
#include "render/shader.h"
#include "render/shaderparam.h"
#include "render/texture.h"

#include "world/entities.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"
#include "animmodel.h"

//animmodel
VARP(fullbrightmodels, 0, 0, 200); //sets minimum amount of brightness for a model: 200 is 100% brightness
VAR(testtags, 0, 0, 1);            //not used by animmodel object, used by children vert/skelmodel
VARF(debugcolmesh, 0, 0, 1,
{
    cleanupmodels();
});

VAR(animationinterpolationtime, 0, 200, 1000);

std::unordered_map<std::string, animmodel::meshgroup *> animmodel::meshgroups;

bool animmodel::enabletc = false,
     animmodel::enabletangents = false,
     animmodel::enablebones = false,
     animmodel::enablecullface = true,
     animmodel::enabledepthoffset = false;

float animmodel::sizescale = 1;

vec4<float> animmodel::colorscale(1, 1, 1, 1);

GLuint animmodel::lastvbuf = 0,
       animmodel::lasttcbuf = 0,
       animmodel::lastxbuf = 0,
       animmodel::lastbbuf = 0,
       animmodel::lastebuf = 0;

const Texture *animmodel::lasttex = nullptr,
              *animmodel::lastdecal = nullptr,
              *animmodel::lastmasks = nullptr,
              *animmodel::lastnormalmap = nullptr;

std::stack<matrix4> animmodel::matrixstack;

template <>
struct std::hash<animmodel::shaderparams>
{
    size_t operator()(const animmodel::shaderparams &k) const
    {
        return memhash(&k, sizeof(k));
    }
};

std::unordered_map<animmodel::shaderparams, animmodel::skin::ShaderParamsKey> animmodel::skin::ShaderParamsKey::keys;

int animmodel::skin::ShaderParamsKey::firstversion = 0,
    animmodel::skin::ShaderParamsKey::lastversion = 1;

//animmodel

animmodel::animmodel(std::string name) : model(name)
{
}

animmodel::~animmodel()
{
    for(part * i : parts)
    {
        delete i;
    }
    parts.clear();
}

// AnimPos

void animmodel::AnimPos::setframes(const animinfo &info)
{
    anim = info.anim;
    if(info.range<=1)
    {
        fr1 = 0;
        t = 0;
    }
    else
    {
        int time = info.anim & Anim_SetTime ? info.basetime : lastmillis - info.basetime;
        fr1 = static_cast<int>(time/info.speed); // round to full frames
        t = (time-fr1*info.speed)/info.speed; // progress of the frame, value from 0.0f to 1.0f
    }
    if(info.anim & Anim_Loop)
    {
        fr1 = fr1%info.range+info.frame;
        fr2 = fr1+1;
        if(fr2>=info.frame+info.range)
        {
            fr2 = info.frame;
        }
    }
    else
    {
        fr1 = std::min(fr1, info.range-1)+info.frame;
        fr2 = std::min(fr1+1, info.frame+info.range-1);
    }
    if(info.anim & Anim_Reverse)
    {
        fr1 = (info.frame+info.range-1)-(fr1-info.frame);
        fr2 = (info.frame+info.range-1)-(fr2-info.frame);
    }
}

bool animmodel::AnimPos::operator==(const AnimPos &a) const
{
    return fr1==a.fr1 && fr2==a.fr2 && (fr1==fr2 || t==a.t);
}
bool animmodel::AnimPos::operator!=(const AnimPos &a) const
{
    return fr1!=a.fr1 || fr2!=a.fr2 || (fr1!=fr2 && t!=a.t);
}

// AnimState

bool animmodel::AnimState::operator==(const AnimState &a) const
{
    return cur==a.cur && (interp<1 ? interp==a.interp && prev==a.prev : a.interp>=1);
}
bool animmodel::AnimState::operator!=(const AnimState &a) const
{
    return cur!=a.cur || (interp<1 ? interp!=a.interp || prev!=a.prev : a.interp<1);
}

// ShaderParamsKey

bool animmodel::skin::ShaderParamsKey::checkversion()
{
    if(version >= firstversion)
    {
        return true;
    }
    version = lastversion;
    if(++lastversion <= 0)
    {
        for(auto &[k, t] : keys)
        {
            t.version = -1;
        }
        firstversion = 0;
        lastversion = 1;
        version = 0;
    }
    return false;
}

//skin

bool animmodel::skin::masked() const
{
    return masks != notexture;
}

bool animmodel::skin::bumpmapped() const
{
    return normalmap != nullptr;
}

bool animmodel::skin::alphatested() const
{
    return alphatest > 0 && tex->type&Texture::ALPHA;
}

bool animmodel::skin::decaled() const
{
    return decal != nullptr;
}

void animmodel::skin::setkey()
{
    key = &ShaderParamsKey::keys[*this];
}

void animmodel::skin::setshaderparams(Mesh &m, const AnimState *as, bool skinned)
{
    if(!Shader::lastshader)
    {
        return;
    }
    if(key->checkversion() && Shader::lastshader->owner == key)
    {
        return;
    }
    Shader::lastshader->owner = key;

    LOCALPARAMF(texscroll, scrollu*lastmillis/1000.0f, scrollv*lastmillis/1000.0f);
    if(alphatested())
    {
        LOCALPARAMF(alphatest, alphatest);
    }

    if(!skinned)
    {
        return;
    }

    if(color.r < 0)
    {
        LOCALPARAM(colorscale, colorscale);
    }
    else
    {
        LOCALPARAMF(colorscale, color.r, color.g, color.b, colorscale.a);
    }
    if(fullbright)
    {
        LOCALPARAMF(fullbright, 0.0f, fullbright);
    }
    else
    {
        LOCALPARAMF(fullbright, 1.0f, as->cur.anim & Anim_FullBright ? 0.5f * fullbrightmodels / 100.0f : 0.0f);
    }
    float curglow = glow;
    if(glowpulse > 0)
    {
        float curpulse = lastmillis*glowpulse;
        curpulse -= std::floor(curpulse);
        curglow += glowdelta*2*std::fabs(curpulse - 0.5f);
    }
    LOCALPARAMF(maskscale, spec, gloss, curglow);
}

Shader *animmodel::skin::loadshader()
{
    if(shadowmapping == ShadowMap_Reflect)
    {
        if(rsmshader)
        {
            return rsmshader;
        }
        std::string opts;
        if(alphatested())
        {
            opts.push_back('a');
        }
        if(!cullface)
        {
            opts.push_back('c');
        }

        DEF_FORMAT_STRING(name, "rsmmodel%s", opts.c_str());
        rsmshader = generateshader(name, "rsmmodelshader \"%s\"", opts.c_str());
        return rsmshader;
    }
    if(shader)
    {
        return shader;
    }
    std::string opts;
    if(alphatested())
    {
        opts.push_back('a');
    }
    if(decaled())
    {
        opts.push_back(decal->type&Texture::ALPHA ? 'D' : 'd');
    }
    if(bumpmapped())
    {
        opts.push_back('n');
    }
    else if(masked())
    {
        opts.push_back('m');
    }
    if(!cullface)
    {
        opts.push_back('c');
    }

    DEF_FORMAT_STRING(name, "model%s", opts.c_str());
    shader = generateshader(name, "modelshader \"%s\"", opts.c_str());
    return shader;
}

void animmodel::skin::cleanup()
{
    if(shader && shader->standard)
    {
        shader = nullptr;
    }
}

void animmodel::skin::preloadBIH() const
{
    if(alphatested() && !tex->alphamask)
    {
        tex->loadalphamask();
    }
}

void animmodel::skin::preloadshader()
{
    loadshader();
    useshaderbyname(alphatested() && owner->model->alphashadow ? "alphashadowmodel" : "shadowmodel");
    if(useradiancehints())
    {
        useshaderbyname(alphatested() ? "rsmalphamodel" : "rsmmodel");
    }
}

void animmodel::skin::setshader(Mesh &m, const AnimState *as, bool usegpuskel, int vweights)
{
    m.setshader(loadshader(), usegpuskel, vweights, gbuf.istransparentlayer());
}

void animmodel::skin::bind(Mesh &b, const AnimState *as, bool usegpuskel, int vweights)
{
    if(cullface > 0)
    {
        if(!enablecullface)
        {
            glEnable(GL_CULL_FACE);
            enablecullface = true;
        }
    }
    else if(enablecullface)
    {
        glDisable(GL_CULL_FACE);
        enablecullface = false;
    }

    if(as->cur.anim & Anim_NoSkin)
    {
        if(alphatested() && owner->model->alphashadow)
        {
            if(tex!=lasttex)
            {
                glBindTexture(GL_TEXTURE_2D, tex->id);
                lasttex = tex;
            }
            static Shader *alphashadowmodelshader = nullptr;
            if(!alphashadowmodelshader)
            {
                alphashadowmodelshader = useshaderbyname("alphashadowmodel");
            }
            b.setshader(alphashadowmodelshader, usegpuskel, vweights);
            setshaderparams(b, as, false);
        }
        else
        {
            static Shader *shadowmodelshader = nullptr;
            if(!shadowmodelshader)
            {
                shadowmodelshader = useshaderbyname("shadowmodel");
            }
            b.setshader(shadowmodelshader, usegpuskel, vweights);
        }
        return;
    }
    int activetmu = 0;
    if(tex!=lasttex)
    {
        glBindTexture(GL_TEXTURE_2D, tex->id);
        lasttex = tex;
    }
    if(bumpmapped() && normalmap!=lastnormalmap)
    {
        glActiveTexture(GL_TEXTURE3);
        activetmu = 3;
        glBindTexture(GL_TEXTURE_2D, normalmap->id);
        lastnormalmap = normalmap;
    }
    if(decaled() && decal!=lastdecal)
    {
        glActiveTexture(GL_TEXTURE4);
        activetmu = 4;
        glBindTexture(GL_TEXTURE_2D, decal->id);
        lastdecal = decal;
    }
    if(masked() && masks!=lastmasks)
    {
        glActiveTexture(GL_TEXTURE1);
        activetmu = 1;
        glBindTexture(GL_TEXTURE_2D, masks->id);
        lastmasks = masks;
    }
    if(activetmu != 0)
    {
        glActiveTexture(GL_TEXTURE0);
    }
    setshader(b, as, usegpuskel, vweights);
    setshaderparams(b, as);
}

void animmodel::skin::invalidateshaderparams()
{
    ShaderParamsKey::invalidate();
}

//Mesh

void animmodel::Mesh::genBIH(const skin &s, std::vector<BIH::mesh> &bih, const matrix4x3 &t)
{
    bih.emplace_back();
    BIH::mesh &m = bih.back();
    m.xform = t;
    m.tex = s.tex;
    if(canrender)
    {
        m.flags |= BIH::Mesh_Render;
    }
    if(cancollide)
    {
        m.flags |= BIH::Mesh_Collide;
    }
    if(s.alphatested())
    {
        m.flags |= BIH::Mesh_Alpha;
    }
    if(noclip)
    {
        m.flags |= BIH::Mesh_NoClip;
    }
    if(s.cullface > 0)
    {
        m.flags |= BIH::Mesh_CullFace;
    }
    genBIH(m);
    while(bih.back().numtris > BIH::mesh::maxtriangles)
    {
        bih.push_back(bih.back());
        BIH::mesh &overflow = bih.back();
        overflow.tris += BIH::mesh::maxtriangles;
        overflow.numtris -= BIH::mesh::maxtriangles;
        bih[bih.size()-2].numtris = BIH::mesh::maxtriangles;
    }
}

void animmodel::Mesh::fixqtangent(quat &q, float bt)
{
    static constexpr float bias = -1.5f/65535;
    static const float biasscale = sqrtf(1 - bias*bias); //cannot be constexpr, sqrtf is not compile time
    if(bt < 0)
    {
        if(q.w >= 0)
        {
            q.neg();
        }
        if(q.w > bias)
        {
            q.mul3(biasscale);
            q.w = bias;
        }
    }
    else if(q.w < 0)
    {
        q.neg();
    }
}

//meshgroup

animmodel::meshgroup::meshgroup()
{
}

animmodel::meshgroup::~meshgroup()
{
    for(Mesh * i : meshes)
    {
        delete i;
    }
    meshes.clear();
}

std::vector<std::vector<animmodel::Mesh *>::const_iterator> animmodel::meshgroup::getmeshes(std::string_view meshname) const
{
    std::vector<std::vector<animmodel::Mesh *>::const_iterator> meshlist;
    for(std::vector<animmodel::Mesh *>::const_iterator i = meshes.begin(); i != meshes.end(); ++i)
    {
        const animmodel::Mesh &tempmesh = **i;
        if(!std::strcmp(meshname.data(), "*") || (tempmesh.name && !std::strcmp(tempmesh.name, meshname.data())))
        {
            meshlist.push_back(i);
        }
    }
    return meshlist;
}

std::vector<size_t> animmodel::meshgroup::getskins(std::string_view meshname) const
{
    std::vector<size_t> skinlist;
    for(uint i = 0; i < meshes.size(); i++)
    {
        const animmodel::Mesh &m = *(meshes[i]);
        if(!std::strcmp(meshname.data(), "*") || (m.name && !std::strcmp(m.name, meshname.data())))
        {
            skinlist.push_back(i);
        }
    }
    return skinlist;
}

void animmodel::meshgroup::calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &t) const
{
    auto rendermeshes = getrendermeshes();
    for(auto i : rendermeshes)
    {
        (*i)->calcbb(bbmin, bbmax, t);
    }
}

void animmodel::meshgroup::genBIH(const std::vector<skin> &skins, std::vector<BIH::mesh> &bih, const matrix4x3 &t) const
{
    for(uint i = 0; i < meshes.size(); i++)
    {
        meshes[i]->genBIH(skins[i], bih, t);
    }
}

void animmodel::meshgroup::genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &t) const
{
    auto rendermeshes = getrendermeshes();
    for(auto i : rendermeshes)
    {
        (*i)->genshadowmesh(tris, t);
    }
}

bool animmodel::meshgroup::hasframe(int i) const
{
    return i>=0 && i<totalframes();
}

bool animmodel::meshgroup::hasframes(int i, int n) const
{
    return i>=0 && i+n<=totalframes();
}

int animmodel::meshgroup::clipframes(int i, int n) const
{
    return std::min(n, totalframes() - i);
}

const std::string &animmodel::meshgroup::groupname() const
{
    return name;
}

std::vector<std::vector<animmodel::Mesh *>::const_iterator> animmodel::meshgroup::getrendermeshes() const
{
    std::vector<std::vector<animmodel::Mesh *>::const_iterator> rendermeshes;
    for(std::vector<animmodel::Mesh *>::const_iterator i = meshes.begin(); i != meshes.end(); ++i)
    {
        if((*i)->canrender || debugcolmesh)
        {
            rendermeshes.push_back(i);
        }
    }
    return rendermeshes;
}

//identical to above but non-const iterator and non const this
std::vector<std::vector<animmodel::Mesh *>::iterator> animmodel::meshgroup::getrendermeshes()
{
    std::vector<std::vector<animmodel::Mesh *>::iterator> rendermeshes;
    for(std::vector<animmodel::Mesh *>::iterator i = meshes.begin(); i != meshes.end(); ++i)
    {
        if((*i)->canrender || debugcolmesh)
        {
            rendermeshes.push_back(i);
        }
    }
    return rendermeshes;
}

void animmodel::meshgroup::bindpos(GLuint ebuf, GLuint vbuf, const void *v, int stride, int type, int size)
{
    if(lastebuf!=ebuf)
    {
        gle::bindebo(ebuf);
        lastebuf = ebuf;
    }
    if(lastvbuf!=vbuf)
    {
        gle::bindvbo(vbuf);
        if(!lastvbuf)
        {
            gle::enablevertex();
        }
        gle::vertexpointer(stride, v, type, size);
        lastvbuf = vbuf;
    }
}
void animmodel::meshgroup::bindpos(GLuint ebuf, GLuint vbuf, const vec *v, int stride)
{
    bindpos(ebuf, vbuf, v, stride, GL_FLOAT, 3);
}

void animmodel::meshgroup::bindpos(GLuint ebuf, GLuint vbuf, const vec4<half> *v, int stride)
{
    bindpos(ebuf, vbuf, v, stride, GL_HALF_FLOAT, 4);
}

void animmodel::meshgroup::bindtc(const void *v, int stride)
{
    if(!enabletc)
    {
        gle::enabletexcoord0();
        enabletc = true;
    }
    if(lasttcbuf!=lastvbuf)
    {
        gle::texcoord0pointer(stride, v, GL_HALF_FLOAT);
        lasttcbuf = lastvbuf;
    }
}

void animmodel::meshgroup::bindtangents(const void *v, int stride)
{
    if(!enabletangents)
    {
        gle::enabletangent();
        enabletangents = true;
    }
    if(lastxbuf!=lastvbuf)
    {
        gle::tangentpointer(stride, v, GL_SHORT);
        lastxbuf = lastvbuf;
    }
}

void animmodel::meshgroup::bindbones(const void *wv, const void *bv, int stride)
{
    if(!enablebones)
    {
        gle::enableboneweight();
        gle::enableboneindex();
        enablebones = true;
    }
    if(lastbbuf!=lastvbuf)
    {
        gle::boneweightpointer(stride, wv);
        gle::boneindexpointer(stride, bv);
        lastbbuf = lastvbuf;
    }
}

//part

animmodel::part::part(const animmodel *model, int index) : model(model), index(index), meshes(nullptr), numanimparts(1), pitchscale(1), pitchoffset(0), pitchmin(0), pitchmax(0)
{
    for(int i = 0; i < maxanimparts; ++i)
    {
        anims[i] = nullptr;
    }
}

animmodel::part::~part()
{
    for(int i = 0; i < maxanimparts; ++i)
    {
        delete[] anims[i];
    }
}

void animmodel::part::cleanup()
{
    if(meshes)
    {
        meshes->cleanup();
    }
    for(skin &i : skins)
    {
        i.cleanup();
    }
}

void animmodel::part::disablepitch()
{
    pitchscale = pitchoffset = pitchmin = pitchmax = 0;
}

void animmodel::part::calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m, float modelscale) const
{
    matrix4x3 t = m;
    t.scale(modelscale);
    meshes->calcbb(bbmin, bbmax, t);
    for(const linkedpart &i : links)
    {
        matrix4x3 n;
        meshes->concattagtransform(i.tag, m, n);
        n.translate(i.translate, modelscale);
        i.p->calcbb(bbmin, bbmax, n, modelscale);
    }
}

void animmodel::part::genBIH(std::vector<BIH::mesh> &bih, const matrix4x3 &m, float modelscale) const
{
    matrix4x3 t = m;
    t.scale(modelscale);
    meshes->genBIH(skins, bih, t);
    for(const linkedpart &i : links)
    {
        matrix4x3 n;
        meshes->concattagtransform(i.tag, m, n);
        n.translate(i.translate, modelscale);
        i.p->genBIH(bih, n, modelscale);
    }
}

void animmodel::part::genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m, float modelscale) const
{
    matrix4x3 t = m;
    t.scale(modelscale);
    meshes->genshadowmesh(tris, t);
    for(const linkedpart &i : links)
    {
        matrix4x3 n;
        meshes->concattagtransform(i.tag, m, n);
        n.translate(i.translate, modelscale);
        i.p->genshadowmesh(tris, n, modelscale);
    }
}

bool animmodel::part::link(part *p, const char *tag, const vec &translate, int anim, int basetime, vec *pos)
{
    int i = meshes ? meshes->findtag(tag) : -1;
    if(i<0)
    {
        for(const linkedpart &i : links)
        {
            if(i.p && i.p->link(p, tag, translate, anim, basetime, pos))
            {
                return true;
            }
        }
        return false;
    }
    links.emplace_back();
    linkedpart &l = links.back();
    l.p = p;
    l.tag = i;
    l.anim = anim;
    l.basetime = basetime;
    l.translate = translate;
    l.pos = pos;
    return true;
}

bool animmodel::part::unlink(const part *p)
{
    for(int i = links.size(); --i >=0;) //note reverse iteration
    {
        if(links[i].p==p)
        {
            links.erase(links.begin() + i);
            return true;
        }
    }
    for(const linkedpart &i : links)
    {
        if(i.p && i.p->unlink(p))
        {
            return true;
        }
    }
    return false;
}

void animmodel::part::initskins(Texture *tex, Texture *masks, uint limit)
{
    if(!limit)
    {
        if(!meshes)
        {
            return;
        }
        limit = meshes->meshes.size();
    }
    while(skins.size() < limit)
    {
        skins.emplace_back(this, tex, masks);
    }
}

bool animmodel::part::alphatested() const
{
    for(const skin &i : skins)
    {
        if(i.alphatested())
        {
            return true;
        }
    }
    return false;
}

void animmodel::part::preloadBIH() const
{
    for(const skin &i : skins)
    {
        i.preloadBIH();
    }
}

void animmodel::part::preloadshaders()
{
    for(skin &i : skins)
    {
        i.preloadshader();
    }
}

void animmodel::part::preloadmeshes()
{
    if(meshes)
    {
        meshes->preload();
    }
}

void animmodel::part::getdefaultanim(animinfo &info) const
{
    info.frame = 0;
    info.range = 1;
}

bool animmodel::part::calcanim(int animpart, int anim, int basetime, int basetime2, dynent *d, int interp, animinfo &info, int &animinterptime) const
{
    //varseed uses an UGLY reinterpret cast from a pointer address to a size_t int
    //presumably the address should be a fairly random value
    uint varseed = static_cast<uint>(reinterpret_cast<size_t>(d));
    info.anim = anim;
    info.basetime = basetime;
    info.varseed = varseed;
    info.speed = anim & Anim_SetSpeed ? basetime2 : 100.0f;
    if((anim & Anim_Index) == Anim_All)
    {
        info.frame = 0;
        info.range = meshes->totalframes();
    }
    else
    {
        const animspec *spec = nullptr;
        if(anims[animpart])
        {
            const std::vector<animspec> &primary = anims[animpart][anim & Anim_Index];
            if(primary.size())
            {
                spec = &primary[(varseed + static_cast<uint>(basetime))%primary.size()];
            }
            if((anim >> Anim_Secondary) & (Anim_Index | Anim_Dir))
            {
                const std::vector<animspec> &secondary = anims[animpart][(anim >> Anim_Secondary) & Anim_Index];
                if(secondary.size())
                {
                    const animspec &spec2 = secondary[(varseed + static_cast<uint>(basetime2))%secondary.size()];
                    if(!spec || spec2.priority > spec->priority)
                    {
                        spec = &spec2;
                        info.anim >>= Anim_Secondary;
                        info.basetime = basetime2;
                    }
                }
            }
        }
        if(spec)
        {
            info.frame = spec->frame;
            info.range = spec->range;
            if(spec->speed>0)
            {
                info.speed = 1000.0f/spec->speed;
            }
        }
        else
        {
            getdefaultanim(info);
        }
    }

    info.anim &= (1 << Anim_Secondary) - 1;
    info.anim |= anim & Anim_Flags;
    if(info.anim & Anim_Loop)
    {
        info.anim &= ~Anim_SetTime;
        if(!info.basetime)
        {
            info.basetime = -(static_cast<int>(reinterpret_cast<size_t>(d)) & 0xFFF);
        }
        if(info.anim & Anim_Clamp)
        {
            if(info.anim & Anim_Reverse)
            {
                info.frame += info.range-1;
            }
            info.range = 1;
        }
    }

    if(!meshes->hasframes(info.frame, info.range))
    {
        if(!meshes->hasframe(info.frame))
        {
            return false;
        }
        info.range = meshes->clipframes(info.frame, info.range);
    }

    if(d && interp>=0)
    {
        animinterpinfo &animationinterpolation = d->animinterp[interp];
        if((info.anim&(Anim_Loop | Anim_Clamp)) == Anim_Clamp)
        {
            animinterptime = std::min(animinterptime, static_cast<int>(info.range*info.speed*0.5e-3f));
        }
        void *ak = meshes->animkey();
        if(d->ragdoll && d->ragdoll->millis != lastmillis)
        {
            animationinterpolation.prev.range = animationinterpolation.cur.range = 0;
            animationinterpolation.lastswitch = -1;
        }
        else if(animationinterpolation.lastmodel!=ak || animationinterpolation.lastswitch<0 || lastmillis-d->lastrendered>animinterptime)
        {
            animationinterpolation.prev = animationinterpolation.cur = info;
            animationinterpolation.lastswitch = lastmillis-animinterptime*2;
        }
        else if(animationinterpolation.cur!=info)
        {
            if(lastmillis-animationinterpolation.lastswitch>animinterptime/2)
            {
                animationinterpolation.prev = animationinterpolation.cur;
            }
            animationinterpolation.cur = info;
            animationinterpolation.lastswitch = lastmillis;
        }
        else if(info.anim & Anim_SetTime)
        {
            animationinterpolation.cur.basetime = info.basetime;
        }
        animationinterpolation.lastmodel = ak;
    }
    return true;
}

void animmodel::part::intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray)
{
    AnimState as[maxanimparts];
    intersect(anim, basetime, basetime2, pitch, axis, forward, d, o, ray, as);
}

void animmodel::part::intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray, AnimState *as)
{
    if((anim & Anim_Reuse) != Anim_Reuse)
    {
        for(int i = 0; i < numanimparts; ++i)
        {
            animinfo info;
            int interp = d && index+numanimparts<=maxanimparts ? index+i : -1,
                animinterptime = animationinterpolationtime;
            if(!calcanim(i, anim, basetime, basetime2, d, interp, info, animinterptime))
            {
                return;
            }
            AnimState &p = as[i];
            p.owner = this;
            p.cur.setframes(info);
            p.interp = 1;
            if(interp>=0 && d->animinterp[interp].prev.range>0)
            {
                int diff = lastmillis-d->animinterp[interp].lastswitch;
                if(diff<animinterptime)
                {
                    p.prev.setframes(d->animinterp[interp].prev);
                    p.interp = diff/static_cast<float>(animinterptime);
                }
            }
        }
    }

    float resize = model->scale * sizescale;
    size_t oldpos = matrixstack.size();
    vec oaxis, oforward, oo, oray;
    matrixstack.top().transposedtransformnormal(axis, oaxis);
    float pitchamount = pitchscale*pitch + pitchoffset;
    if(pitchmin || pitchmax)
    {
        pitchamount = std::clamp(pitchamount, pitchmin, pitchmax);
    }
    if(as->cur.anim & Anim_NoPitch || (as->interp < 1 && as->prev.anim & Anim_NoPitch))
    {
        pitchamount *= (as->cur.anim & Anim_NoPitch ? 0 : as->interp) + (as->interp < 1 && as->prev.anim & Anim_NoPitch ? 0 : 1 - as->interp);
    }
    if(pitchamount)
    {
        matrixstack.push(matrixstack.top());
        matrixstack.top().rotate(pitchamount/RAD, oaxis);
    }
    if(this == model->parts[0] && !model->translate.iszero())
    {
        if(oldpos == matrixstack.size())
        {
            matrixstack.push(matrixstack.top());
        }
        matrixstack.top().translate(model->translate, resize);
    }
    matrixstack.top().transposedtransformnormal(forward, oforward);
    matrixstack.top().transposedtransform(o, oo);
    oo.div(resize);
    matrixstack.top().transposedtransformnormal(ray, oray);

    if((anim & Anim_Reuse) != Anim_Reuse)
    {
        for(linkedpart &link : links)
        {
            if(!link.p)
            {
                continue;
            }
            link.matrix.translate(link.translate, resize);
            matrix4 mul;
            mul.mul(matrixstack.top(), link.matrix);
            matrixstack.push(mul);

            int nanim = anim,
                nbasetime  = basetime,
                nbasetime2 = basetime2;
            if(link.anim>=0)
            {
                nanim = link.anim | (anim & Anim_Flags);
                nbasetime = link.basetime;
                nbasetime2 = 0;
            }
            link.p->intersect(nanim, nbasetime, nbasetime2, pitch, axis, forward, d, o, ray);

            matrixstack.pop();
        }
    }

    while(matrixstack.size() > oldpos)
    {
        matrixstack.pop();
    }
}

void animmodel::part::render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d)
{
    AnimState as[maxanimparts];
    render(anim, basetime, basetime2, pitch, axis, forward, d, as);
}

void animmodel::part::render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, AnimState *as)
{
    if((anim & Anim_Reuse) != Anim_Reuse)
    {
        for(int i = 0; i < numanimparts; ++i)
        {
            animinfo info;
            int interp = d && index+numanimparts<=maxanimparts ? index+i : -1, animinterptime = animationinterpolationtime;
            if(!calcanim(i, anim, basetime, basetime2, d, interp, info, animinterptime))
            {
                return;
            }
            AnimState &p = as[i];
            p.owner = this;
            p.cur.setframes(info);
            p.interp = 1;
            if(interp>=0 && d->animinterp[interp].prev.range>0)
            {
                int diff = lastmillis-d->animinterp[interp].lastswitch;
                if(diff<animinterptime)
                {
                    p.prev.setframes(d->animinterp[interp].prev);
                    p.interp = diff/static_cast<float>(animinterptime);
                }
            }
        }
    }

    float resize = model->scale * sizescale;
    size_t oldpos = matrixstack.size();
    vec oaxis, oforward;
    matrixstack.top().transposedtransformnormal(axis, oaxis);
    float pitchamount = pitchscale*pitch + pitchoffset;
    if(pitchmin || pitchmax)
    {
        pitchamount = std::clamp(pitchamount, pitchmin, pitchmax);
    }
    if(as->cur.anim & Anim_NoPitch || (as->interp < 1 && as->prev.anim & Anim_NoPitch))
    {
        pitchamount *= (as->cur.anim & Anim_NoPitch ? 0 : as->interp) + (as->interp < 1 && as->prev.anim & Anim_NoPitch ? 0 : 1 - as->interp);
    }
    if(pitchamount)
    {
        matrixstack.push(matrixstack.top());
        matrixstack.top().rotate(pitchamount/RAD, oaxis);
    }
    if(this == model->parts[0] && !model->translate.iszero())
    {
        if(oldpos == matrixstack.size())
        {
            matrixstack.push(matrixstack.top());
        }
        matrixstack.top().translate(model->translate, resize);
    }
    matrixstack.top().transposedtransformnormal(forward, oforward);

    if(!(anim & Anim_NoRender))
    {
        matrix4 modelmatrix;
        modelmatrix.mul(shadowmapping ? shadowmatrix : camprojmatrix, matrixstack.top());
        if(resize!=1)
        {
            modelmatrix.scale(resize);
        }
        GLOBALPARAM(modelmatrix, modelmatrix);
        if(!(anim & Anim_NoSkin))
        {
            GLOBALPARAM(modelworld, matrix3(matrixstack.top()));

            vec modelcamera;
            matrixstack.top().transposedtransform(camera1->o, modelcamera);
            modelcamera.div(resize);
            GLOBALPARAM(modelcamera, modelcamera);
        }
    }

    meshes->render(as, pitch, oaxis, oforward, d, this);

    if((anim & Anim_Reuse) != Anim_Reuse)
    {
        for(linkedpart &link : links)
        {
            link.matrix.translate(link.translate, resize);
            matrix4 mul;
            mul.mul(matrixstack.top(), link.matrix);
            matrixstack.push(mul);
            if(link.pos)
            {
                *link.pos = matrixstack.top().gettranslation();
            }
            if(!link.p)
            {
                matrixstack.pop();
                continue;
            }
            int nanim = anim,
                nbasetime = basetime,
                nbasetime2 = basetime2;
            if(link.anim>=0)
            {
                nanim = link.anim | (anim & Anim_Flags);
                nbasetime = link.basetime;
                nbasetime2 = 0;
            }
            link.p->render(nanim, nbasetime, nbasetime2, pitch, axis, forward, d);
            matrixstack.pop();
        }
    }

    while(matrixstack.size() > oldpos)
    {
        matrixstack.pop();
    }
}

void animmodel::part::setanim(int animpart, int num, int frame, int range, float speed, int priority)
{
    if(animpart<0 || animpart>=maxanimparts || num<0 || num >= static_cast<int>(animnames.size()))
    {
        return;
    }
    if(frame<0 || range<=0 || !meshes || !meshes->hasframes(frame, range))
    {
        conoutf("invalid frame %d, range %d in model %s", frame, range, model->modelname().c_str());
        return;
    }
    if(!anims[animpart])
    {
        anims[animpart] = new std::vector<animspec>[animnames.size()];
    }
    anims[animpart][num].push_back({frame, range, speed, priority});
}

bool animmodel::part::animated() const
{
    for(int i = 0; i < maxanimparts; ++i)
    {
        if(anims[i])
        {
            return true;
        }
    }
    return false;
}

void animmodel::part::loaded()
{
    for(skin &i : skins)
    {
        i.setkey();
    }
}

int animmodel::linktype(const animmodel *, const part *) const
{
    return Link_Tag;
}

void animmodel::intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a, const vec &o, const vec &ray) const
{
    int numtags = 0;
    if(a)
    {
        int index = parts.back()->index + parts.back()->numanimparts;
        for(int i = 0; a[i].tag; i++)
        {
            numtags++;
            animmodel *m = static_cast<animmodel *>(a[i].m);
            if(!m)
            {
                continue;
            }
            part *p = m->parts[0];
            switch(linktype(m, p))
            {
                case Link_Tag:
                {
                    p->index = link(p, a[i].tag, vec(0, 0, 0), a[i].anim, a[i].basetime, a[i].pos) ? index : -1;
                    break;
                }
                default:
                {
                    continue;
                }
            }
            index += p->numanimparts;
        }
    }

    AnimState as[maxanimparts];
    parts[0]->intersect(anim, basetime, basetime2, pitch, axis, forward, d, o, ray, as);

    for(part *p : parts)
    {
        switch(linktype(this, p))
        {
            case Link_Reuse:
            {
                p->intersect(anim | Anim_Reuse, basetime, basetime2, pitch, axis, forward, d, o, ray, as);
                break;
            }
        }
    }

    if(a)
    {
        for(int i = numtags-1; i >= 0; i--)
        {
            animmodel *m = static_cast<animmodel *>(a[i].m);
            if(!m)
            {
                continue;
            }

            part *p = m->parts[0];
            switch(linktype(m, p))
            {
                case Link_Tag:
                {
                    if(p->index >= 0)
                    {
                        unlink(p);
                    }
                    p->index = 0;
                    break;
                }
                case Link_Reuse:
                {
                    p->intersect(anim | Anim_Reuse, basetime, basetime2, pitch, axis, forward, d, o, ray, as);
                    break;
                }
            }
        }
    }
}

int animmodel::intersect(int anim, int basetime, int basetime2, const vec &pos, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec &o, const vec &ray, float &dist) const
{
    vec axis(1, 0, 0), forward(0, 1, 0);

    matrixstack.push(matrix4());
    matrixstack.top().identity();
    if(!d || !d->ragdoll || d->ragdoll->millis == lastmillis)
    {
        float secs = lastmillis/1000.0f;
        yaw += spin.x*secs;
        pitch += spin.y*secs;
        roll += spin.z*secs;

        matrixstack.top().settranslation(pos);
        matrixstack.top().rotate_around_z(yaw/RAD);
        bool usepitch = pitched();
        if(roll && !usepitch)
        {
            matrixstack.top().rotate_around_y(-roll/RAD);
        }
        matrixstack.top().transformnormal(vec(axis), axis);
        matrixstack.top().transformnormal(vec(forward), forward);
        if(roll && usepitch)
        {
            matrixstack.top().rotate_around_y(-roll/RAD);
        }
        if(orientation.x)
        {
            matrixstack.top().rotate_around_z(orientation.x/RAD);
        }
        if(orientation.y)
        {
            matrixstack.top().rotate_around_x(orientation.y/RAD);
        }
        if(orientation.z)
        {
            matrixstack.top().rotate_around_y(-orientation.z/RAD);
        }
    }
    else
    {
        matrixstack.top().settranslation(d->ragdoll->center);
        pitch = 0;
    }
    sizescale = size;

    intersect(anim, basetime, basetime2, pitch, axis, forward, d, a, o, ray);
    return -1;
}

void animmodel::render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a) const
{
    int numtags = 0;
    if(a)
    {
        int index = parts.back()->index + parts.back()->numanimparts;
        for(int i = 0; a[i].tag; i++)
        {
            numtags++;

            animmodel *m = static_cast<animmodel *>(a[i].m);
            if(!m)
            {
                if(a[i].pos)
                {
                    link(nullptr, a[i].tag, vec(0, 0, 0), 0, 0, a[i].pos);
                }
                continue;
            }
            part *p = m->parts[0];
            switch(linktype(m, p))
            {
                case Link_Tag:
                {
                    p->index = link(p, a[i].tag, vec(0, 0, 0), a[i].anim, a[i].basetime, a[i].pos) ? index : -1;
                    break;
                }
                default:
                {
                    continue;
                }
            }
            index += p->numanimparts;
        }
    }

    AnimState as[maxanimparts];
    parts[0]->render(anim, basetime, basetime2, pitch, axis, forward, d, as);

    for(uint i = 1; i < parts.size(); i++)
    {
        part *p = parts[i];
        switch(linktype(this, p))
        {
            case Link_Reuse:
            {
                p->render(anim | Anim_Reuse, basetime, basetime2, pitch, axis, forward, d, as);
                break;
            }
        }
    }

    if(a)
    {
        for(int i = numtags-1; i >= 0; i--)
        {
            animmodel *m = static_cast<animmodel *>(a[i].m);
            if(!m)
            {
                if(a[i].pos)
                {
                    unlink(nullptr);
                }
                continue;
            }
            part *p = m->parts[0];
            switch(linktype(m, p))
            {
                case Link_Tag:
                {
                    if(p->index >= 0)
                    {
                        unlink(p);
                    }
                    p->index = 0;
                    break;
                }
                case Link_Reuse:
                {
                    p->render(anim | Anim_Reuse, basetime, basetime2, pitch, axis, forward, d, as);
                    break;
                }
            }
        }
    }
}

void animmodel::render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec4<float> &color) const
{
    vec axis(1, 0, 0),
        forward(0, 1, 0);

    matrixstack = {};
    matrixstack.push(matrix4());
    matrixstack.top().identity();
    if(!d || !d->ragdoll || d->ragdoll->millis == lastmillis)
    {
        float secs = lastmillis/1000.0f;
        yaw += spin.x*secs;
        pitch += spin.y*secs;
        roll += spin.z*secs;

        matrixstack.top().settranslation(o);
        matrixstack.top().rotate_around_z(yaw/RAD);
        bool usepitch = pitched();
        if(roll && !usepitch)
        {
            matrixstack.top().rotate_around_y(-roll/RAD);
        }
        matrixstack.top().transformnormal(vec(axis), axis);
        matrixstack.top().transformnormal(vec(forward), forward);
        if(roll && usepitch)
        {
            matrixstack.top().rotate_around_y(-roll/RAD);
        }
        if(orientation.x)
        {
            matrixstack.top().rotate_around_z(orientation.x/RAD);
        }
        if(orientation.y)
        {
            matrixstack.top().rotate_around_x(orientation.y/RAD);
        }
        if(orientation.z)
        {
            matrixstack.top().rotate_around_y(-orientation.z/RAD);
        }
    }
    else
    {
        matrixstack.top().settranslation(d->ragdoll->center);
        pitch = 0;
    }

    sizescale = size;

    if(anim & Anim_NoRender)
    {
        render(anim, basetime, basetime2, pitch, axis, forward, d, a);
        if(d)
        {
            d->lastrendered = lastmillis;
        }
        return;
    }

    if(!(anim & Anim_NoSkin))
    {
        if(colorscale != color)
        {
            colorscale = color;
            skin::invalidateshaderparams();
        }
    }

    if(depthoffset && !enabledepthoffset)
    {
        enablepolygonoffset(GL_POLYGON_OFFSET_FILL);
        enabledepthoffset = true;
    }

    render(anim, basetime, basetime2, pitch, axis, forward, d, a);

    if(d)
    {
        d->lastrendered = lastmillis;
    }
}

void animmodel::cleanup()
{
    for(part *p : parts)
    {
        p->cleanup();
    }
}

animmodel::part &animmodel::addpart()
{
    part *p = new part(this, parts.size());
    parts.push_back(p);
    return *p;
}

void animmodel::initmatrix(matrix4x3 &m) const
{
    m.identity();
    if(orientation.x)
    {
        m.rotate_around_z(orientation.x/RAD);
    }
    if(orientation.y)
    {
        m.rotate_around_x(orientation.y/RAD);
    }
    if(orientation.z)
    {
        m.rotate_around_y(-orientation.z/RAD);
    }
    m.translate(translate, scale);
}

void animmodel::genBIH(std::vector<BIH::mesh> &bih)
{
    if(parts.empty())
    {
        return;
    }
    matrix4x3 m;
    initmatrix(m);
    for(const skin &s : parts[0]->skins)
    {
        s.tex->loadalphamask();
    }
    for(uint i = 1; i < parts.size(); i++)
    {
        const part *p = parts[i];
        switch(linktype(this, p))
        {
            case Link_Reuse:
            {
                for(skin &s : parts[i]->skins)
                {
                    s.tex->loadalphamask();
                }
                p->genBIH(bih, m, scale);
                break;
            }
        }
    }
}

bool animmodel::link(part *p, const char *tag, const vec &translate, int anim, int basetime, vec *pos) const
{
    if(parts.empty())
    {
        return false;
    }
    return parts[0]->link(p, tag, translate, anim, basetime, pos);
}

void animmodel::loaded()
{
    for(part *p : parts)
    {
        p->loaded();
    }
}

bool animmodel::unlink(const part *p) const
{
    if(parts.empty())
    {
        return false;
    }
    return parts[0]->unlink(p);
}

void animmodel::genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &orient)
{
    if(parts.empty())
    {
        return;
    }
    matrix4x3 m;
    initmatrix(m);
    m.mul(orient, matrix4x3(m));
    parts[0]->genshadowmesh(tris, m, scale);
    for(uint i = 1; i < parts.size(); i++)
    {
        const part *p = parts[i];
        switch(linktype(this, p))
        {
            case Link_Reuse:
            {
                p->genshadowmesh(tris, m, scale);
                break;
            }
        }
    }
}

void animmodel::preloadBIH()
{
    if(!bih)
    {
        setBIH();
    }
    if(bih)
    {
        for(const part* i : parts)
        {
            i->preloadBIH();
        }
    }
}

//always will return true (note that this overloads model::setBIH())
bool animmodel::setBIH()
{
    if(bih)
    {
        return true;
    }
    std::vector<BIH::mesh> meshes;
    genBIH(meshes);
    bih = std::make_unique<BIH>(meshes);
    return true;
}

bool animmodel::animated() const
{
    if(spin.x || spin.y || spin.z)
    {
        return true;
    }
    for(const part* i : parts)
    {
        if(i->animated())
        {
            return true;
        }
    }
    return false;
}

bool animmodel::pitched() const
{
    return parts[0]->pitchscale != 0;
}

bool animmodel::alphatested() const
{
    for(const part* i : parts)
    {
        if(i->alphatested())
        {
            return true;
        }
    }
    return false;
}

bool animmodel::load()
{
    startload();
    bool success = loadconfig(modelname()) && parts.size(); // configured model, will call the model commands below
    if(!success)
    {
        success = loaddefaultparts(); // model without configuration, try default tris and skin
    }
    endload();
    if(flipy())
    {
        translate.y = -translate.y;
    }

    if(!success)
    {
        return false;
    }
    for(const part *i : parts)
    {
        if(!i->meshes)
        {
            return false;
        }
    }
    loaded();
    return true;
}

void animmodel::preloadshaders()
{
    for(part* i : parts)
    {
        i->preloadshaders();
    }
}

void animmodel::preloadmeshes()
{
    for(part* i : parts)
    {
        i->preloadmeshes();
    }
}

void animmodel::setshader(Shader *shader)
{
    if(parts.empty())
    {
        loaddefaultparts();
    }
    for(part* i : parts)
    {
        for(skin& j : i->skins)
        {
            j.shader = shader;
        }
    }
}

void animmodel::setspec(float spec)
{
    if(parts.empty())
    {
        loaddefaultparts();
    }
    for(part* i : parts)
    {
        for(skin& j : i->skins)
        {
            j.spec = spec;
        }
    }
}

void animmodel::setgloss(int gloss)
{
    if(parts.empty())
    {
        loaddefaultparts();
    }
    for(part* i : parts)
    {
        for(skin& j : i->skins)
        {
            j.gloss = gloss;
        }
    }
}

void animmodel::setglow(float glow, float delta, float pulse)
{
    if(parts.empty())
    {
        loaddefaultparts();
    }
    for(part* i : parts)
    {
        for(skin &s : i->skins)
        {
            s.glow = glow;
            s.glowdelta = delta;
            s.glowpulse = pulse;
        }
    }
}

void animmodel::setalphatest(float alphatest)
{
    if(parts.empty())
    {
        loaddefaultparts();
    }
    for(part* i : parts)
    {
        for(skin& j : i->skins)
        {
            j.alphatest = alphatest;
        }
    }
}

void animmodel::setfullbright(float fullbright)
{
    if(parts.empty())
    {
        loaddefaultparts();
    }
    for(part* i : parts)
    {
        for(skin& j : i->skins)
        {
            j.fullbright = fullbright;
        }
    }
}

void animmodel::setcullface(int cullface)
{
    if(parts.empty())
    {
        loaddefaultparts();
    }
    for(part* i : parts)
    {
        for(skin& j : i->skins)
        {
            j.cullface = cullface;
        }
    }
}

void animmodel::setcolor(const vec &color)
{
    if(parts.empty())
    {
        loaddefaultparts();
    }
    for(part* i : parts)
    {
        for(skin& j : i->skins)
        {
            j.color = color;
        }
    }
}

void animmodel::settransformation(const std::optional<vec> pos,
                                  const std::optional<vec> rotate,
                                  const std::optional<vec> orient,
                                  const std::optional<float> size)
{
    if(pos)
    {
        translate = pos.value();
    }
    if(rotate)
    {
        spin = rotate.value();
    }
    if(orient)
    {
        orientation = orient.value();
    }
    if(size)
    {
        scale = size.value();
    }
}

vec4<float> animmodel::locationsize() const
{
    return vec4<float>(translate.x, translate.y, translate.z, scale);
}

void animmodel::calcbb(vec &center, vec &radius) const
{
    if(parts.empty())
    {
        return;
    }
    vec bbmin(1e16f, 1e16f, 1e16f),
        bbmax(-1e16f, -1e16f, -1e16f);
    matrix4x3 m;
    initmatrix(m);
    parts[0]->calcbb(bbmin, bbmax, m, scale);
    for(const part *p : parts)
    {
        switch(linktype(this, p))
        {
            case Link_Reuse:
            {
                p->calcbb(bbmin, bbmax, m, scale);
                break;
            }
        }
    }
    radius = bbmax;
    radius.sub(bbmin);
    radius.mul(0.5f);
    center = bbmin;
    center.add(radius);
}

void animmodel::calctransform(matrix4x3 &m) const
{
    initmatrix(m);
    m.scale(scale);
}

void animmodel::startrender() const
{
    enabletc = enabletangents = enablebones = enabledepthoffset = false;
    enablecullface = true;
    lastvbuf = lasttcbuf = lastxbuf = lastbbuf = lastebuf =0;
    lastmasks = lastnormalmap = lastdecal = lasttex = nullptr;
    skin::invalidateshaderparams();
}

void animmodel::endrender() const
{
    if(lastvbuf || lastebuf)
    {
        disablevbo();
    }
    if(!enablecullface)
    {
        glEnable(GL_CULL_FACE);
    }
    if(enabledepthoffset)
    {
        disablepolygonoffset(GL_POLYGON_OFFSET_FILL);
    }
}

void animmodel::boundbox(vec &center, vec &radius)
{
    if(bbradius.x < 0)
    {
        calcbb(bbcenter, bbradius);
        bbradius.add(bbextend);
    }
    center = bbcenter;
    radius = bbradius;
}

float animmodel::collisionbox(vec &center, vec &radius)
{
    if(collideradius.x < 0)
    {
        boundbox(collidecenter, collideradius);
        if(collidexyradius)
        {
            collidecenter.x = collidecenter.y = 0;
            collideradius.x = collideradius.y = collidexyradius;
        }
        if(collideheight)
        {
            collidecenter.z = collideradius.z = collideheight/2;
        }
        rejectradius = collideradius.magnitude();
    }
    center = collidecenter;
    radius = collideradius;
    return rejectradius;
}

float animmodel::above()
{
    vec center, radius;
    boundbox(center, radius);
    return center.z+radius.z;
}

const std::string &animmodel::modelname() const
{
    return name;
}

void animmodel::disablebones()
{
    gle::disableboneweight();
    gle::disableboneindex();
    enablebones = false;
}

void animmodel::disabletangents()
{
    gle::disabletangent();
    enabletangents = false;
}

void animmodel::disabletc()
{
    gle::disabletexcoord0();
    enabletc = false;
}

void animmodel::disablevbo()
{
    if(lastebuf)
    {
        gle::clearebo();
    }
    if(lastvbuf)
    {
        gle::clearvbo();
        gle::disablevertex();
    }
    if(enabletc)
    {
        disabletc();
    }
    if(enabletangents)
    {
        disabletangents();
    }
    if(enablebones)
    {
        disablebones();
    }
    lastvbuf = lasttcbuf = lastxbuf = lastbbuf = lastebuf = 0;
}

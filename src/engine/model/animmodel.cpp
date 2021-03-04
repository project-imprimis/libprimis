
#include "engine.h"

#include "interface/console.h"
#include "interface/control.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"
#include "render/rendermodel.h"

#include "world/physics.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"
#include "animmodel.h"

//animmodel
VARP(fullbrightmodels, 0, 0, 200);
VAR(testtags, 0, 0, 1);
VARF(debugcolmesh, 0, 0, 1,
{
    cleanupmodels();
});

hashnameset<animmodel::meshgroup *> animmodel::meshgroups;
int animmodel::intersectresult = -1,
    animmodel::intersectmode = 0;
float animmodel::intersectdist = 0,
      animmodel::intersectscale = 1;
bool animmodel::enabletc = false,
     animmodel::enabletangents = false,
     animmodel::enablebones = false,
     animmodel::enablecullface = true,
     animmodel::enabledepthoffset = false;
float animmodel::sizescale = 1;
vec4 animmodel::colorscale(1, 1, 1, 1);
GLuint animmodel::lastvbuf = 0,
       animmodel::lasttcbuf = 0,
       animmodel::lastxbuf = 0,
       animmodel::lastbbuf = 0,
       animmodel::lastebuf = 0;
Texture *animmodel::lasttex = nullptr,
        *animmodel::lastdecal = nullptr,
        *animmodel::lastmasks = nullptr,
        *animmodel::lastnormalmap = nullptr;
int animmodel::matrixpos = 0;
matrix4 animmodel::matrixstack[64];

hashtable<animmodel::shaderparams, animmodel::ShaderParamsKey> animmodel::ShaderParamsKey::keys;
int animmodel::ShaderParamsKey::firstversion = 0,
    animmodel::ShaderParamsKey::lastversion = 1;

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

// ShaderParamsKey

bool animmodel::ShaderParamsKey::checkversion()
{
    if(version >= firstversion)
    {
        return true;
    }
    version = lastversion;
    if(++lastversion <= 0)
    {
        ENUMERATE(keys, ShaderParamsKey, key, key.version = -1);
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
        curglow += glowdelta*2*fabs(curpulse - 0.5f);
    }
    LOCALPARAMF(maskscale, spec, gloss, curglow);
}

Shader *animmodel::skin::loadshader()
{
    #define DOMODELSHADER(name, body) \
        do { \
            static Shader *name##shader = nullptr; \
            if(!name##shader) name##shader = useshaderbyname(#name); \
            body; \
        } while(0)
    #define SETMODELSHADER(m, name) DOMODELSHADER(name, (m).setshader(name##shader))

    if(shadowmapping == ShadowMap_Reflect)
    {
        if(rsmshader)
        {
            return rsmshader;
        }
        string opts;
        int optslen = 0;
        if(alphatested())
        {
            opts[optslen++] = 'a';
        }
        if(!cullface)
        {
            opts[optslen++] = 'c';
        }
        opts[optslen++] = '\0';

        DEF_FORMAT_STRING(name, "rsmmodel%s", opts);
        rsmshader = generateshader(name, "rsmmodelshader \"%s\"", opts);
        return rsmshader;
    }
    if(shader)
    {
        return shader;
    }
    string opts;
    int optslen = 0;
    if(alphatested())
    {
        opts[optslen++] = 'a';
    }
    if(decaled())
    {
        opts[optslen++] = decal->type&Texture::ALPHA ? 'D' : 'd';
    }
    if(bumpmapped())
    {
        opts[optslen++] = 'n';
    }
    else if(masked())
    {
        opts[optslen++] = 'm';
    }
    if(!cullface)
    {
        opts[optslen++] = 'c';
    }
    opts[optslen++] = '\0';

    DEF_FORMAT_STRING(name, "model%s", opts);
    shader = generateshader(name, "modelshader \"%s\"", opts);
    return shader;
}

void animmodel::skin::cleanup()
{
    if(shader && shader->standard)
    {
        shader = nullptr;
    }
}

void animmodel::skin::preloadBIH()
{
    if(alphatested() && !tex->alphamask)
    {
        loadalphamask(tex);
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

void animmodel::skin::setshader(Mesh &m, const AnimState *as)
{
    m.setshader(loadshader(), transparentlayer ? 1 : 0);
}

void animmodel::skin::bind(Mesh &b, const AnimState *as)
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
            SETMODELSHADER(b, alphashadowmodel);
            setshaderparams(b, as, false);
        }
        else
        {
            SETMODELSHADER(b, shadowmodel);
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
        glActiveTexture_(GL_TEXTURE3);
        activetmu = 3;
        glBindTexture(GL_TEXTURE_2D, normalmap->id);
        lastnormalmap = normalmap;
    }
    if(decaled() && decal!=lastdecal)
    {
        glActiveTexture_(GL_TEXTURE4);
        activetmu = 4;
        glBindTexture(GL_TEXTURE_2D, decal->id);
        lastdecal = decal;
    }
    if(masked() && masks!=lastmasks)
    {
        glActiveTexture_(GL_TEXTURE1);
        activetmu = 1;
        glBindTexture(GL_TEXTURE_2D, masks->id);
        lastmasks = masks;
    }
    if(activetmu != 0)
    {
        glActiveTexture_(GL_TEXTURE0);
    }
    setshader(b, as);
    setshaderparams(b, as);
}

//Mesh

void animmodel::Mesh::genBIH(skin &s, vector<BIH::mesh> &bih, const matrix4x3 &t)
{
    BIH::mesh &m = bih.add();
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
    while(bih.last().numtris > BIH::mesh::Max_Triangles)
    {
        BIH::mesh &overflow = bih.dup();
        overflow.tris += BIH::mesh::Max_Triangles;
        overflow.numtris -= BIH::mesh::Max_Triangles;
        bih[bih.length()-2].numtris = BIH::mesh::Max_Triangles;
    }
}

void animmodel::Mesh::fixqtangent(quat &q, float bt)
{
    static const float bias = -1.5f/65535,
                       biasscale = sqrtf(1 - bias*bias);
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

void animmodel::meshgroup::calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &t)
{
    LOOP_RENDER_MESHES(Mesh, m, m.calcbb(bbmin, bbmax, t));
}

void animmodel::meshgroup::genBIH(vector<skin> &skins, vector<BIH::mesh> &bih, const matrix4x3 &t)
{
    for(int i = 0; i < meshes.length(); i++)
    {
        meshes[i]->genBIH(skins[i], bih, t);
    }
}

void animmodel::meshgroup::genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &t)
{
    LOOP_RENDER_MESHES(Mesh, m, m.genshadowmesh(tris, t));
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

void animmodel::meshgroup::bindpos(GLuint ebuf, GLuint vbuf, void *v, int stride, int type, int size)
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
void animmodel::meshgroup::bindpos(GLuint ebuf, GLuint vbuf, vec *v, int stride)
{
    bindpos(ebuf, vbuf, v, stride, GL_FLOAT, 3);
}

void animmodel::meshgroup::bindpos(GLuint ebuf, GLuint vbuf, GenericVec4<half> *v, int stride)
{
    bindpos(ebuf, vbuf, v, stride, GL_HALF_FLOAT, 4);
}

void animmodel::meshgroup::bindtc(void *v, int stride)
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

void animmodel::meshgroup::bindtangents(void *v, int stride)
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

void animmodel::meshgroup::bindbones(void *wv, void *bv, int stride)
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

void animmodel::part::cleanup()
{
    if(meshes)
    {
        meshes->cleanup();
    }
    for(int i = 0; i < skins.length(); i++)
    {
        skins[i].cleanup();
    }
}

void animmodel::part::disablepitch()
{
    pitchscale = pitchoffset = pitchmin = pitchmax = 0;
}

void animmodel::part::calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m)
{
    matrix4x3 t = m;
    t.scale(model->scale);
    meshes->calcbb(bbmin, bbmax, t);
    for(int i = 0; i < links.length(); i++)
    {
        matrix4x3 n;
        meshes->concattagtransform(this, links[i].tag, m, n);
        n.translate(links[i].translate, model->scale);
        links[i].p->calcbb(bbmin, bbmax, n);
    }
}

void animmodel::part::genBIH(vector<BIH::mesh> &bih, const matrix4x3 &m)
{
    matrix4x3 t = m;
    t.scale(model->scale);
    meshes->genBIH(skins, bih, t);
    for(int i = 0; i < links.length(); i++)
    {
        matrix4x3 n;
        meshes->concattagtransform(this, links[i].tag, m, n);
        n.translate(links[i].translate, model->scale);
        links[i].p->genBIH(bih, n);
    }
}

void animmodel::part::genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m)
{
    matrix4x3 t = m;
    t.scale(model->scale);
    meshes->genshadowmesh(tris, t);
    for(int i = 0; i < links.length(); i++)
    {
        matrix4x3 n;
        meshes->concattagtransform(this, links[i].tag, m, n);
        n.translate(links[i].translate, model->scale);
        links[i].p->genshadowmesh(tris, n);
    }
}

bool animmodel::part::link(part *p, const char *tag, const vec &translate, int anim, int basetime, vec *pos)
{
    int i = meshes ? meshes->findtag(tag) : -1;
    if(i<0)
    {
        for(int i = 0; i < links.length(); i++)
        {
            if(links[i].p && links[i].p->link(p, tag, translate, anim, basetime, pos))
            {
                return true;
            }
        }
        return false;
    }
    linkedpart &l = links.add();
    l.p = p;
    l.tag = i;
    l.anim = anim;
    l.basetime = basetime;
    l.translate = translate;
    l.pos = pos;
    return true;
}

bool animmodel::part::unlink(part *p)
{
    for(int i = links.length(); --i >=0;) //note reverse iteration
    {
        if(links[i].p==p)
        {
            links.remove(i, 1);
            return true;
        }
    }
    for(int i = 0; i < links.length(); i++)
    {
        if(links[i].p && links[i].p->unlink(p))
        {
            return true;
        }
    }
    return false;
}

void animmodel::part::initskins(Texture *tex, Texture *masks, int limit)
{
    if(!limit)
    {
        if(!meshes)
        {
            return;
        }
        limit = meshes->meshes.length();
    }
    while(skins.length() < limit)
    {
        skin &s = skins.add();
        s.owner = this;
        s.tex = tex;
        s.masks = masks;
    }
}

bool animmodel::part::alphatested() const
{
    for(int i = 0; i < skins.length(); i++)
    {
        if(skins[i].alphatested())
        {
            return true;
        }
    }
    return false;
}

void animmodel::part::preloadBIH()
{
    for(int i = 0; i < skins.length(); i++)
    {
        skins[i].preloadBIH();
    }
}

void animmodel::part::preloadshaders()
{
    for(int i = 0; i < skins.length(); i++)
    {
        skins[i].preloadshader();
    }
}

void animmodel::part::preloadmeshes()
{
    if(meshes)
    {
        meshes->preload(this);
    }
}

void animmodel::part::getdefaultanim(animinfo &info, int anim, uint varseed, dynent *d)
{
    info.frame = 0;
    info.range = 1;
}

bool animmodel::part::calcanim(int animpart, int anim, int basetime, int basetime2, dynent *d, int interp, animinfo &info, int &animinterptime)
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
        animspec *spec = nullptr;
        if(anims[animpart])
        {
            vector<animspec> &primary = anims[animpart][anim & Anim_Index];
            if(primary.length())
            {
                spec = &primary[static_cast<uint>(varseed + basetime)%primary.length()];
            }
            if((anim >> Anim_Secondary) & (Anim_Index | Anim_Dir))
            {
                vector<animspec> &secondary = anims[animpart][(anim >> Anim_Secondary) & Anim_Index];
                if(secondary.length())
                {
                    animspec &spec2 = secondary[static_cast<uint>(varseed + basetime2)%secondary.length()];
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
            getdefaultanim(info, anim, static_cast<uint>(varseed + info.basetime), d);
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
    int oldpos = matrixpos;
    vec oaxis, oforward, oo, oray;
    matrixstack[matrixpos].transposedtransformnormal(axis, oaxis);
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
        ++matrixpos;
        matrixstack[matrixpos] = matrixstack[matrixpos-1];
        matrixstack[matrixpos].rotate(pitchamount*RAD, oaxis);
    }
    if(this == model->parts[0] && !model->translate.iszero())
    {
        if(oldpos == matrixpos)
        {
            ++matrixpos;
            matrixstack[matrixpos] = matrixstack[matrixpos-1];
        }
        matrixstack[matrixpos].translate(model->translate, resize);
    }
    matrixstack[matrixpos].transposedtransformnormal(forward, oforward);
    matrixstack[matrixpos].transposedtransform(o, oo);
    oo.div(resize);
    matrixstack[matrixpos].transposedtransformnormal(ray, oray);

    intersectscale = resize;
    meshes->intersect(as, pitch, oaxis, oforward, d, this, oo, oray);

    if((anim & Anim_Reuse) != Anim_Reuse)
    {
        for(int i = 0; i < links.length(); i++)
        {
            linkedpart &link = links[i];
            if(!link.p)
            {
                continue;
            }
            link.matrix.translate(link.translate, resize);

            matrixpos++;
            matrixstack[matrixpos].mul(matrixstack[matrixpos-1], link.matrix);

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

            matrixpos--;
        }
    }

    matrixpos = oldpos;
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
    int oldpos = matrixpos;
    vec oaxis, oforward;
    matrixstack[matrixpos].transposedtransformnormal(axis, oaxis);
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
        ++matrixpos;
        matrixstack[matrixpos] = matrixstack[matrixpos-1];
        matrixstack[matrixpos].rotate(pitchamount*RAD, oaxis);
    }
    if(this == model->parts[0] && !model->translate.iszero())
    {
        if(oldpos == matrixpos)
        {
            ++matrixpos;
            matrixstack[matrixpos] = matrixstack[matrixpos-1];
        }
        matrixstack[matrixpos].translate(model->translate, resize);
    }
    matrixstack[matrixpos].transposedtransformnormal(forward, oforward);

    if(!(anim & Anim_NoRender))
    {
        matrix4 modelmatrix;
        modelmatrix.mul(shadowmapping ? shadowmatrix : camprojmatrix, matrixstack[matrixpos]);
        if(resize!=1)
        {
            modelmatrix.scale(resize);
        }
        GLOBALPARAM(modelmatrix, modelmatrix);
        if(!(anim & Anim_NoSkin))
        {
            GLOBALPARAM(modelworld, matrix3(matrixstack[matrixpos]));

            vec modelcamera;
            matrixstack[matrixpos].transposedtransform(camera1->o, modelcamera);
            modelcamera.div(resize);
            GLOBALPARAM(modelcamera, modelcamera);
        }
    }

    meshes->render(as, pitch, oaxis, oforward, d, this);

    if((anim & Anim_Reuse) != Anim_Reuse)
    {
        for(int i = 0; i < links.length(); i++)
        {
            linkedpart &link = links[i];
            link.matrix.translate(link.translate, resize);
            matrixpos++;
            matrixstack[matrixpos].mul(matrixstack[matrixpos-1], link.matrix);
            if(link.pos)
            {
                *link.pos = matrixstack[matrixpos].gettranslation();
            }
            if(!link.p)
            {
                matrixpos--;
                continue;
            }
            int nanim = anim, nbasetime = basetime, nbasetime2 = basetime2;
            if(link.anim>=0)
            {
                nanim = link.anim | (anim & Anim_Flags);
                nbasetime = link.basetime;
                nbasetime2 = 0;
            }
            link.p->render(nanim, nbasetime, nbasetime2, pitch, axis, forward, d);
            matrixpos--;
        }
    }

    matrixpos = oldpos;
}

void animmodel::part::setanim(int animpart, int num, int frame, int range, float speed, int priority)
{
    if(animpart<0 || animpart>=maxanimparts || num<0 || num >= numanims)
    {
        return;
    }
    if(frame<0 || range<=0 || !meshes || !meshes->hasframes(frame, range))
    {
        conoutf("invalid frame %d, range %d in model %s", frame, range, model->name);
        return;
    }
    if(!anims[animpart])
    {
        anims[animpart] = new vector<animspec>[numanims];
    }
    animspec &spec = anims[animpart][num].add();
    spec.frame = frame;
    spec.range = range;
    spec.speed = speed;
    spec.priority = priority;
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
    meshes->shared++;
    for(int i = 0; i < skins.length(); i++)
    {
        skins[i].setkey();
    }
}

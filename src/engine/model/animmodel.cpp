
#include "engine.h"

#include "interface/console.h"
#include "interface/control.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"

#include "world/physics.h"

#include "ragdoll.h"
#include "animmodel.h"

//animmodel
VARP(fullbrightmodels, 0, 0, 200);
VAR(testtags, 0, 0, 1);
VARF(dbgcolmesh, 0, 0, 1,
{
    extern void cleanupmodels();
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
Texture *animmodel::lasttex = NULL,
        *animmodel::lastdecal = NULL,
        *animmodel::lastmasks = NULL,
        *animmodel::lastnormalmap = NULL;
int animmodel::matrixpos = 0;
matrix4 animmodel::matrixstack[64];

hashtable<animmodel::shaderparams, animmodel::shaderparamskey> animmodel::shaderparamskey::keys;
int animmodel::shaderparamskey::firstversion = 0,
    animmodel::shaderparamskey::lastversion = 1;

// animpos

void animmodel::animpos::setframes(const animinfo &info)
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
        fr1 = min(fr1, info.range-1)+info.frame;
        fr2 = min(fr1+1, info.frame+info.range-1);
    }
    if(info.anim & Anim_Reverse)
    {
        fr1 = (info.frame+info.range-1)-(fr1-info.frame);
        fr2 = (info.frame+info.range-1)-(fr2-info.frame);
    }
}

// shaderparamskey

bool animmodel::shaderparamskey::checkversion()
{
    if(version >= firstversion)
    {
        return true;
    }
    version = lastversion;
    if(++lastversion <= 0)
    {
        ENUMERATE(keys, shaderparamskey, key, key.version = -1);
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
    return normalmap != NULL;
}

bool animmodel::skin::alphatested() const
{
    return alphatest > 0 && tex->type&Texture::ALPHA;
}

bool animmodel::skin::decaled() const
{
    return decal != NULL;
}

void animmodel::skin::setkey()
{
    key = &shaderparamskey::keys[*this];
}

void animmodel::skin::setshaderparams(mesh &m, const animstate *as, bool skinned)
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
        curpulse -= floor(curpulse);
        curglow += glowdelta*2*fabs(curpulse - 0.5f);
    }
    LOCALPARAMF(maskscale, spec, gloss, curglow);
}

Shader *animmodel::skin::loadshader()
{
    #define DOMODELSHADER(name, body) \
        do { \
            static Shader *name##shader = NULL; \
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
        shader = NULL;
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

void animmodel::skin::setshader(mesh &m, const animstate *as)
{
    m.setshader(loadshader(), transparentlayer ? 1 : 0);
}

void animmodel::skin::bind(mesh &b, const animstate *as)
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

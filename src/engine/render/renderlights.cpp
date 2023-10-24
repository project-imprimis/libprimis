/* renderlights.cpp: render lights to deferred buffers
 *
 * light entities and sunlight in the game is rendered to deferred buffers
 * "g-buffers" which are used to compose a scene
 * lights are cached using a shadow map to allow rendering less than once per
 * frame, improving performance and light count allowed
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "aa.h"
#include "ao.h"
#include "csm.h"
#include "hdr.h"
#include "lightsphere.h"
#include "octarender.h"
#include "postfx.h"
#include "radiancehints.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "rendersky.h"
#include "rendertimers.h"
#include "renderva.h"
#include "renderwindow.h"
#include "shader.h"
#include "shaderparam.h"
#include "stain.h"
#include "texture.h"

#include "interface/control.h"
#include "interface/console.h"

#include "world/dynlight.h"
#include "world/light.h"
#include "world/material.h"
#include "world/octaedit.h"
#include "world/octaworld.h"
#include "world/world.h"

int vieww = -1,
    viewh = -1;

int gw = -1,
    gh = -1;

GBuffer gbuf;

int hdrclear = 0;

int spotlights       = 0,
    volumetriclights = 0,
    nospeclights     = 0;
std::vector<vec2> msaapositions;

//`g`-buffer `scale`
VARFP(gscale, 25, 100, 100, gbuf.cleanupgbuffer()); //size of g buffer, approximately correlates to g buffer linear dimensions
VARFP(gscalecubic, 0, 0, 1, gbuf.cleanupgbuffer()); //g-buffer scale cubic: use cubic interpolation for g buffer upscaling to screen output
VARFP(gscalenearest, 0, 0, 1, gbuf.cleanupgbuffer()); //g buffer nearest neighbor interpolation

matrix4 worldmatrix, screenmatrix;

static std::array<Shader *, 2> bilateralshader = { nullptr, nullptr };

Shader *loadbilateralshader(int pass)
{
    if(!aobilateral)
    {
        return nullshader;
    }
    std::string opts;
    bool linear = aoreducedepth && (aoreduce || aoreducedepth > 1),
         upscale = aoreduce && aobilateralupscale,
         reduce = aoreduce && (upscale || (!linear && !aopackdepth));
    if(reduce)
    {
        opts.push_back('r');
        opts.push_back('0' + aoreduce);
    }
    if(upscale)
    {
        opts.push_back('u');
    }
    else if(linear)
    {
        opts.push_back('l');

    }
    if(aopackdepth)
    {
        opts.push_back('p');
    }

    DEF_FORMAT_STRING(name, "bilateral%c%s%d", 'x' + pass, opts.c_str(), aobilateral);
    return generateshader(name, "bilateralshader \"%s\" %d %d", opts.c_str(), aobilateral, reduce ? aoreduce : 0);
}
/* loadbilateralshaders: sets bilateralshader array using bilateralshader()
 * args:
 *      void
 * returns:
 *      void
 * other state changes:
 *      bilateralshader[2] elements point to Shader objects representing the two passes
 */
void loadbilateralshaders()
{
    for(int k = 0; k < 2; ++k)
    {
        bilateralshader[k] = loadbilateralshader(k);
    }
}

/* clearsbilateralshaders: clears bilateralarray
 * args:
 *      void
 * returns:
 *      void
 * other state changes:
 *      bilateralshader[2] elements point to the null pointer
 */
void clearbilateralshaders()
{
    bilateralshader.fill(nullptr);
}

static void setbilateralparams(int radius, float depth)
{
    float sigma = blursigma*2*radius;
    LOCALPARAMF(bilateralparams, 1.0f/(M_LN2*2*sigma*sigma), 1.0f/(M_LN2*depth*depth));
}

/* clearsbilateralshaders: sets values for one of the bilateralshader[] elements
 * args:
 *      int radius: the bilateral filter radius to set
 *      int pass: [0-1] the element of the bilateralshader() array to change
 *      float depth: the depth of the bilateral filtering to set
 * returns:
 *      void
 * other state changes:
 *      bilateralshader[2] elements' referenced Shader objects have their parameters changed
 */
void setbilateralshader(int radius, int pass, float depth)
{
    bilateralshader[pass]->set();
    setbilateralparams(radius, depth);
}

//debug commands
//for individual debug commands, see respective functions lower in the file
VAR(debugfullscreen, 0, 0, 1);

void GBuffer::cleanupscale()
{
    for(GLuint &i : scalefbo)
    {
        if(i)
        {
            glDeleteFramebuffers(1, &i);
            i = 0;
        }
    }
    for(GLuint &i : scaletex)
    {
        if(i)
        {
            glDeleteTextures(1, &i);
            i = 0;
        }
    }
    scalew = scaleh = -1;
}

void GBuffer::setupscale(int sw, int sh, int w, int h)
{
    scalew = w;
    scaleh = h;

    for(int i = 0; i < (gscalecubic ? 2 : 1); ++i)
    {
        if(!scaletex[i])
        {
            glGenTextures(1, &scaletex[i]);
        }
        if(!scalefbo[i])
        {
            glGenFramebuffers(1, &scalefbo[i]);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, scalefbo[i]);

        createtexture(scaletex[i], sw, i ? h : sh, nullptr, 3, gscalecubic || !gscalenearest ? 1 : 0, GL_RGB, GL_TEXTURE_2D);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scaletex[i], 0);
        if(!i)
        {
            bindgdepth();
        }
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fatal("failed allocating scale buffer!");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if(gscalecubic)
    {
        useshaderbyname("scalecubicx");
        useshaderbyname("scalecubicy");
    }
}

GLuint GBuffer::shouldscale() const
{
    return scalefbo[0];
}

void GBuffer::doscale(GLuint outfbo) const
{
    if(!scaletex[0])
    {
        return;
    }
    timer *scaletimer = begintimer("scaling");
    if(gscalecubic)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, scalefbo[1]);
        glViewport(0, 0, gw, hudh());
        glBindTexture(GL_TEXTURE_2D, scaletex[0]);
        SETSHADER(scalecubicy);
        screenquad(1, 1);
        glBindFramebuffer(GL_FRAMEBUFFER, outfbo);
        glViewport(0, 0, hudw(), hudh());
        glBindTexture(GL_TEXTURE_2D, scaletex[1]);
        SETSHADER(scalecubicx);
        screenquad(1, 1);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, outfbo);
        glViewport(0, 0, hudw(), hudh());
        glBindTexture(GL_TEXTURE_2D, scaletex[0]);
        SETSHADER(scalelinear);
        screenquad(1, 1);
    }

    endtimer(scaletimer);
}

VARFP(glineardepth, 0, 0, 3, initwarning("g-buffer setup", Init_Load, Change_Shaders)); // g-buffer linear depth buffer
VAR(gdepthformat, 1, 0, 0);                                                             // g-buffer depth buffer format
VARF(gstencil, 0, 0, 1, initwarning("g-buffer setup", Init_Load, Change_Shaders));      // g-buffer stenciling
VARF(gdepthstencil, 0, 2, 2, initwarning("g-buffer setup", Init_Load, Change_Shaders)); // g-buffer depth buffer stenciling
VAR(ghasstencil, 1, 0, 0);                                                              // g buffer has stencil
VARFP(msaa, 0, 0, 16, initwarning("MSAA setup", Init_Load, Change_Shaders));            // multi-sample antialiasing
VARF(msaadepthstencil, 0, 2, 2, initwarning("MSAA setup", Init_Load, Change_Shaders));  // multi-sample antialiasing depth buffer stenciling
VARF(msaastencil, 0, 0, 1, initwarning("MSAA setup", Init_Load, Change_Shaders));       // multi-sample antialiasing stenciling
VARF(msaaedgedetect, 0, 1, 1, gbuf.cleanupgbuffer());                                        // multi-sample antialiasing edge detection
VARFP(msaalineardepth, -1, -1, 3, initwarning("MSAA setup", Init_Load, Change_Shaders));// multi-sample antialiasing linear depth
VARFP(msaatonemap, 0, 0, 1, gbuf.cleanupgbuffer());                                          // multi-sample antialiasing tone mapping
VAR(msaamaxsamples, 1, 0, 0);                                                           // multi-sample antialiasing maximum samples
VAR(msaamaxdepthtexsamples, 1, 0, 0);                                                   // multi-sample antialiasing maximum depth buffer texture sample count
VAR(msaamaxcolortexsamples, 1, 0, 0);                                                   // multi-sample antialiasing maximum color buffer texture sample count
VAR(msaaminsamples, 1, 0, 0);                                                           // multi-sample antialiasing minimum sample count
VAR(msaasamples, 1, 0, 0);                                                              // multi-sample antialiasing sampling
VAR(msaalight, 1, 0, 0);                                                                // multi-sample antialias lights
VARF(msaapreserve, -1, 0, 1, initwarning("MSAA setup", Init_Load, Change_Shaders));     // preserve multi-sample antialiasing

void checkmsaasamples()
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);

    GLint samples;
    glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaaminsamples, GL_RGBA8, 1, 1, GL_TRUE);
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &samples);
    msaasamples = samples;

    glDeleteTextures(1, &tex);
}

void initgbuffer()
{
    msaamaxsamples = msaamaxdepthtexsamples = msaamaxcolortexsamples = msaaminsamples = msaasamples = msaalight = 0;
    msaapositions.clear();

    GLint val;
    glGetIntegerv(GL_MAX_SAMPLES, &val);
    msaamaxsamples = val;
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &val);
    msaamaxdepthtexsamples = val;
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &val);
    msaamaxcolortexsamples = val;

    int maxsamples = std::min(msaamaxsamples, msaamaxcolortexsamples),
        reqsamples = std::min(msaa, maxsamples);
    if(reqsamples >= 2)
    {
        msaaminsamples = 2;
        while(msaaminsamples*2 <= reqsamples)
        {
            msaaminsamples *= 2;
        }
    }

    int lineardepth = glineardepth;
    if(msaaminsamples)
    {
        if(msaamaxdepthtexsamples < msaaminsamples)
        {
            if(msaalineardepth > 0)
            {
                lineardepth = msaalineardepth;
            }
            else if(!lineardepth)
            {
                lineardepth = 1;
            }
        }
        else if(msaalineardepth >= 0)
        {
            lineardepth = msaalineardepth;
        }
    }
    gdepthformat = lineardepth;
    if(msaaminsamples)
    {
        ghasstencil = (msaadepthstencil > 1 || (msaadepthstencil && gdepthformat)) ? 2 : (msaastencil ? 1 : 0);
        checkmsaasamples();
        if(msaapreserve >= 0)
        {
            msaalight = 3;
        }
    }
    else
    {
        ghasstencil = (gdepthstencil > 1 || (gdepthstencil && gdepthformat)) ? 2 : (gstencil ? 1 : 0);
    }
    initao();
}

VARF(forcepacknorm, 0, 0, 1, initwarning("g-buffer setup", Init_Load, Change_Shaders));

bool usepacknorm()
{
    return forcepacknorm || msaasamples || !useavatarmask();
}

void maskgbuffer(const char *mask)
{
    GLenum drawbufs[4];
    int numbufs = 0;
    while(*mask)
    {
        switch(*mask++)
        {
            case 'c':
            {
                drawbufs[numbufs++] = GL_COLOR_ATTACHMENT0;
                break;
            }
            case 'n':
            {
                drawbufs[numbufs++] = GL_COLOR_ATTACHMENT1;
                break;
            }
            case 'd':
            {
                if(gdepthformat)
                {
                    drawbufs[numbufs++] = GL_COLOR_ATTACHMENT3;
                }
                break;
            }
            case 'g':
            {
                drawbufs[numbufs++] = GL_COLOR_ATTACHMENT2;
                break;
            }
        }
    }
    glDrawBuffers(numbufs, drawbufs);
}

void GBuffer::cleanupmsbuffer()
{
    if(msfbo)        { glDeleteFramebuffers(1, &msfbo);        msfbo        = 0; }
    if(msdepthtex)   { glDeleteTextures(1, &msdepthtex);        msdepthtex   = 0; }
    if(mscolortex)   { glDeleteTextures(1, &mscolortex);        mscolortex   = 0; }
    if(msnormaltex)  { glDeleteTextures(1, &msnormaltex);       msnormaltex  = 0; }
    if(msglowtex)    { glDeleteTextures(1, &msglowtex);         msglowtex    = 0; }
    if(msstencilrb)  { glDeleteRenderbuffers(1, &msstencilrb); msstencilrb  = 0; }
    if(msdepthrb)    { glDeleteRenderbuffers(1, &msdepthrb);   msdepthrb    = 0; }
    if(mshdrfbo)     { glDeleteFramebuffers(1, &mshdrfbo);     mshdrfbo     = 0; }
    if(mshdrtex)     { glDeleteTextures(1, &mshdrtex);          mshdrtex     = 0; }
    if(msrefractfbo) { glDeleteFramebuffers(1, &msrefractfbo); msrefractfbo = 0; }
    if(msrefracttex) { glDeleteTextures(1, &msrefracttex);      msrefracttex = 0; }
}

void GBuffer::bindmsdepth() const
{
    if(gdepthformat)
    {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msdepthrb);
        if(ghasstencil > 1)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msdepthrb);
        }
        else if(msaalight && ghasstencil)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msstencilrb);
        }
    }
    else
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, msdepthtex, 0);
        if(ghasstencil > 1)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, msdepthtex, 0);
        }
        else if(msaalight && ghasstencil)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msstencilrb);
        }
    }
}

void GBuffer::setupmsbuffer(int w, int h)
{
    if(!msfbo)
    {
        glGenFramebuffers(1, &msfbo);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, msfbo);

    stencilformat = ghasstencil > 1 ? GL_DEPTH24_STENCIL8 : (ghasstencil ? GL_STENCIL_INDEX8 : 0);

    if(gdepthformat)
    {
        if(!msdepthrb)
        {
            glGenRenderbuffers(1, &msdepthrb);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, msdepthrb);
        glRenderbufferStorageMultisample_(GL_RENDERBUFFER, msaasamples, ghasstencil > 1 ? stencilformat : GL_DEPTH_COMPONENT24, w, h);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    if(msaalight && ghasstencil == 1)
    {
        if(!msstencilrb)
        {
            glGenRenderbuffers(1, &msstencilrb);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, msstencilrb);
        glRenderbufferStorageMultisample_(GL_RENDERBUFFER, msaasamples, GL_STENCIL_INDEX8, w, h);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    if(!msdepthtex)
    {
        glGenTextures(1, &msdepthtex);
    }
    if(!mscolortex)
    {
        glGenTextures(1, &mscolortex);
    }
    if(!msnormaltex)
    {
        glGenTextures(1, &msnormaltex);
    }

    maskgbuffer(msaalight ? "cndg" : "cnd");

    static const GLenum depthformats[] = { GL_RGBA8, GL_R16F, GL_R32F };
    GLenum depthformat = gdepthformat ? depthformats[gdepthformat-1] : (ghasstencil > 1 ? stencilformat : GL_DEPTH_COMPONENT24);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, depthformat, w, h, GL_TRUE);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mscolortex);
    glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, GL_RGBA8, w, h, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
    glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, GL_RGBA8, w, h, GL_TRUE);
    if(msaalight)
    {
        if(!msglowtex)
        {
            glGenTextures(1, &msglowtex);
        }
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msglowtex);
        glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, hdrformat, w, h, GL_TRUE);
    }

    bindmsdepth();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mscolortex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE, msnormaltex, 0);
    if(msaalight)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, msglowtex, 0);
    }
    if(gdepthformat)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D_MULTISAMPLE, msdepthtex, 0);
    }

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        if(msaalight)
        {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msglowtex);
            glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, GL_RGBA8, w, h, GL_TRUE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, msglowtex, 0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                fatal("failed allocating MSAA g-buffer!");
            }
        }
        else
        {
            fatal("failed allocating MSAA g-buffer!");
        }
    }

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (ghasstencil ? GL_STENCIL_BUFFER_BIT : 0));

    msaapositions.clear();
    for(int i = 0; i < msaasamples; ++i)
    {
        GLfloat vals[2];
        glGetMultisamplefv(GL_SAMPLE_POSITION, i, vals);
        msaapositions.emplace_back(vec2(vals[0], vals[1]));
    }

    if(msaalight)
    {
        if(!mshdrtex)
        {
            glGenTextures(1, &mshdrtex);
        }
        if(!mshdrfbo)
        {
            glGenFramebuffers(1, &mshdrfbo);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, mshdrfbo);
        bindmsdepth();
        hdrformat = 0;
        for(int prec = hdrprec; prec >= 0; prec--)
        {
            GLenum format = gethdrformat(prec);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
            glGetError();
            glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, format, w, h, GL_TRUE);
            if(glGetError() == GL_NO_ERROR)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mshdrtex, 0);
                if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
                {
                    hdrformat = format;
                    break;
                }
            }
        }

        if(!hdrformat || glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fatal("failed allocating MSAA HDR buffer!");
        }
        if(!msrefracttex)
        {
            glGenTextures(1, &msrefracttex);
        }
        if(!msrefractfbo)
        {
            glGenFramebuffers(1, &msrefractfbo);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, msrefractfbo);

        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
        glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, GL_RGB, w, h, GL_TRUE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, msrefracttex, 0);
        bindmsdepth();

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fatal("failed allocating MSAA refraction buffer!");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    useshaderbyname("msaaedgedetect");
    useshaderbyname("msaaresolve");
    useshaderbyname("msaareducew");
    useshaderbyname("msaareduce");
    if(!msaalight)
    {
        useshaderbyname("msaaresolvedepth");
    }
    if(msaalight > 1 && msaatonemap)
    {
        useshaderbyname("msaatonemap");
        if(msaalight > 2)
        {
            useshaderbyname("msaatonemapsample");
        }
    }
}

void GBuffer::bindgdepth() const
{
    if(gdepthformat || msaalight)
    {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gdepthrb);
        if(ghasstencil > 1)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gdepthrb);
        }
        else if(!msaalight || ghasstencil)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gstencilrb);
        }
    }
    else
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_RECTANGLE, gdepthtex, 0);
        if(ghasstencil > 1)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_RECTANGLE, gdepthtex, 0);
        }
        else if(ghasstencil)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gstencilrb);
        }
    }
}

void GBuffer::setupgbuffer()
{
    //start with screen resolution
    int sw = renderw(),
        sh = renderh();
    //scale sw and sh if gscale (g-buffer scale) is not 100%
    if(gscale != 100)
    {
        sw = std::max((renderw()*gscale + 99)/100, 1);
        sh = std::max((renderh()*gscale + 99)/100, 1);
    }

    if(gw == sw && gh == sh && ((sw >= hudw() && sh >= hudh() && !scalefbo[0]) || (scalew == hudw() && scaleh == hudh())))
    {
        return;
    }
    //clean up various buffers & info with them
    cleanupscale();
    cleanupbloom();
    cleanupao();
    cleanupvolumetric();
    cleanupaa();
    cleanuppostfx();

    gw = sw;
    gh = sh;

    hdrformat = gethdrformat(hdrprec);
    stencilformat = ghasstencil > 1 ? GL_DEPTH24_STENCIL8 : (ghasstencil ? GL_STENCIL_INDEX8 : 0);

    if(msaasamples)
    {
        setupmsbuffer(gw, gh);
    }
    hdrfloat = floatformat(hdrformat);
    hdrclear = 3;
    gdepthinit = false;

    if(gdepthformat || msaalight)
    {
        if(!gdepthrb)
        {
            glGenRenderbuffers(1, &gdepthrb);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, gdepthrb);
        glRenderbufferStorage(GL_RENDERBUFFER, ghasstencil > 1 ? stencilformat : GL_DEPTH_COMPONENT24, gw, gh);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    if(!msaalight && ghasstencil == 1)
    {
        if(!gstencilrb)
        {
            glGenRenderbuffers(1, &gstencilrb);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, gstencilrb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, gw, gh);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    if(!msaalight)
    {
        if(!gdepthtex)
        {
            glGenTextures(1, &gdepthtex);
        }
        if(!gcolortex)
        {
            glGenTextures(1, &gcolortex);
        }
        if(!gnormaltex)
        {
            glGenTextures(1, &gnormaltex);
        }
        if(!gglowtex)
        {
            glGenTextures(1, &gglowtex);
        }
        if(!gfbo)
        {
            glGenFramebuffers(1, &gfbo);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, gfbo);

        maskgbuffer("cndg");

        static const GLenum depthformats[] = { GL_RGBA8, GL_R16F, GL_R32F };
        GLenum depthformat = gdepthformat ? depthformats[gdepthformat-1] : (ghasstencil > 1 ? stencilformat : GL_DEPTH_COMPONENT24);
        createtexture(gdepthtex, gw, gh, nullptr, 3, 0, depthformat, GL_TEXTURE_RECTANGLE);

        createtexture(gcolortex, gw, gh, nullptr, 3, 0, GL_RGBA8, GL_TEXTURE_RECTANGLE);
        createtexture(gnormaltex, gw, gh, nullptr, 3, 0, GL_RGBA8, GL_TEXTURE_RECTANGLE);
        createtexture(gglowtex, gw, gh, nullptr, 3, 0, hdrformat, GL_TEXTURE_RECTANGLE);

        bindgdepth();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gcolortex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, gnormaltex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gglowtex, 0);
        if(gdepthformat)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_RECTANGLE, gdepthtex, 0);
        }
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            createtexture(gglowtex, gw, gh, nullptr, 3, 0, GL_RGBA8, GL_TEXTURE_RECTANGLE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gglowtex, 0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                fatal("failed allocating g-buffer!");
            }
        }

        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (ghasstencil ? GL_STENCIL_BUFFER_BIT : 0));
    }

    if(!hdrtex)
    {
        glGenTextures(1, &hdrtex);
    }
    if(!hdrfbo)
    {
        glGenFramebuffers(1, &hdrfbo);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);

    createtexture(hdrtex, gw, gh, nullptr, 3, 1, hdrformat, GL_TEXTURE_RECTANGLE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, hdrtex, 0);
    bindgdepth();

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fatal("failed allocating HDR buffer!");
    }

    if(!msaalight || (msaalight > 2 && msaatonemap && msaatonemapblit))
    {
        if(!refracttex)
        {
            glGenTextures(1, &refracttex);
        }
        if(!refractfbo)
        {
            glGenFramebuffers(1, &refractfbo);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, refractfbo);
        createtexture(refracttex, gw, gh, nullptr, 3, 0, GL_RGB, GL_TEXTURE_RECTANGLE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, refracttex, 0);
        bindgdepth();

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fatal("failed allocating refraction buffer!");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if(gw < hudw() || gh < hudh())
    {
        setupscale(gw, gh, hudw(), hudh());
    }
}

    void GBuffer::cleanupgbuffer()
    {
        if(gfbo)       { glDeleteFramebuffers(1, &gfbo);        gfbo       = 0; }
        if(gdepthtex)  { glDeleteTextures(1, &gdepthtex);        gdepthtex  = 0; }
        if(gcolortex)  { glDeleteTextures(1, &gcolortex);        gcolortex  = 0; }
        if(gnormaltex) { glDeleteTextures(1, &gnormaltex);       gnormaltex = 0; }
        if(gglowtex)   { glDeleteTextures(1, &gglowtex);         gglowtex   = 0; }
        if(gstencilrb) { glDeleteRenderbuffers(1, &gstencilrb); gstencilrb = 0; }
        if(gdepthrb)   { glDeleteRenderbuffers(1, &gdepthrb);   gdepthrb   = 0; }
        if(hdrfbo)     { glDeleteFramebuffers(1, &hdrfbo);      hdrfbo     = 0; }
        if(hdrtex)     { glDeleteTextures(1, &hdrtex);           hdrtex     = 0; }
        if(refractfbo) { glDeleteFramebuffers(1, &refractfbo);  refractfbo = 0; }
        if(refracttex) { glDeleteTextures(1, &refracttex);       refracttex = 0; }
        gw = gh = -1;
        cleanupscale();
        cleanupmsbuffer();
        cleardeferredlightshaders();
    }

void GBuffer::resolvemsaadepth(int w, int h) const
{
    if(!msaasamples || msaalight)
    {
        return;
    }

    timer *resolvetimer = drawtex ? nullptr : begintimer("msaa depth resolve");

    if(msaadepthblit)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msfbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gfbo);
        if(ghasstencil)
        {
            glClear(GL_STENCIL_BUFFER_BIT);
        }
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
    if(!msaadepthblit || gdepthformat)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gfbo);
        glViewport(0, 0, w, h);
        maskgbuffer("d");
        if(!msaadepthblit)
        {
            if(ghasstencil)
            {
                glStencilFunc(GL_ALWAYS, 0, ~0);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                glEnable(GL_STENCIL_TEST);
            }
            glDepthFunc(GL_ALWAYS);
            SETSHADER(msaaresolvedepth);
        }
        else
        {
             glDisable(GL_DEPTH_TEST);
             SETSHADER(msaaresolve);
        }
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        screenquad();
        maskgbuffer("cnd");
        if(!msaadepthblit)
        {
            if(ghasstencil)
            {
                glDisable(GL_STENCIL_TEST);
            }
            glDepthFunc(GL_LESS);
        }
        else
        {
            glEnable(GL_DEPTH_TEST);
        }
    }

    endtimer(resolvetimer);
}

void GBuffer::resolvemsaacolor(int w, int h)
{
    if(!msaalight)
    {
        return;
    }
    timer *resolvetimer = drawtex ? nullptr : begintimer("msaa resolve");

    glBindFramebuffer(GL_READ_FRAMEBUFFER, mshdrfbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrfbo);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);

    endtimer(resolvetimer);
}

float ldrscale = 1.0f;

float ldrscaleb()
{
    return ldrscale/255;
}

VAR(debugdepth, 0, 0, 1); //toggles showing depth buffer onscreen

void GBuffer::viewdepth() const
{
    int w = (debugfullscreen) ? hudw() : std::min(hudw(), hudh())/2, //if debugfullscreen, set to hudw/hudh size; if not, do small size
        h = (debugfullscreen) ? hudh() : (w*hudh())/hudw();
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    debugquad(0, 0, w, h, 0, 0, gw, gh);
}

VAR(debugstencil, 0, 0, 0xFF);

void viewstencil()
{
    if(!ghasstencil || !hdrfbo)
    {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);
    glViewport(0, 0, gw, gh);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glStencilFunc(GL_NOTEQUAL, 0, debugstencil);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glEnable(GL_STENCIL_TEST);
    SETSHADER(hudnotexture);
    gle::colorf(1, 1, 1);
    debugquad(0, 0, hudw(), hudh(), 0, 0, gw, gh);
    glDisable(GL_STENCIL_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw(), hudh());

    int w = debugfullscreen ? hudw() : std::min(hudw(), hudh())/2, //if debugfullscreen, set to hudw/hudh size; if not, do small size
        h = debugfullscreen ? hudh() : (w*hudh())/hudw();
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
    debugquad(0, 0, w, h, 0, 0, gw, gh);
}

VAR(debugrefract, 0, 0, 1);

void GBuffer::viewrefract()
{
    int w = debugfullscreen ? hudw() : std::min(hudw(), hudh())/2, //if debugfullscreen, set to hudw/hudh size; if not, do small size
        h = debugfullscreen ? hudh() : (w*hudh())/hudw();
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
    debugquad(0, 0, w, h, 0, 0, gw, gh);
}

PackNode shadowatlaspacker(0, 0, shadowatlassize, shadowatlassize);

VAR(smminradius, 0, 16, 10000);

class lightinfo
{
    public:
        int ent, shadowmap, flags;
        vec o, color;
        float radius, dist;
        vec dir, spotx, spoty;
        int spot;
        float sx1, sy1, sx2, sy2, sz1, sz2;
        occludequery *query;

        lightinfo() : query(nullptr)
        {
        }
        lightinfo(const vec &o, const vec &color, float radius, int flags = 0, const vec &dir = vec(0, 0, 0), int spot = 0)
          : ent(-1), shadowmap(-1), flags(flags),
            o(o), color(color), radius(radius), dist(camera1->o.dist(o)),
            dir(dir), spot(spot), query(nullptr)
        {
            if(spot > 0)
            {
                calcspot();
            }
            calcscissor();
        }
        lightinfo(int i, const extentity &e)
          : ent(i), shadowmap(-1), flags(e.attr5),
            o(e.o), color(vec(e.attr2, e.attr3, e.attr4).max(0)), radius(e.attr1), dist(camera1->o.dist(e.o)),
            dir(0, 0, 0), spot(0), query(nullptr)
        {
            if(e.attached && e.attached->type == EngineEnt_Spotlight)
            {
                dir = vec(e.attached->o).sub(e.o).normalize();
                spot = std::clamp(static_cast<int>(e.attached->attr1), 1, 89);
                calcspot();
            }
            calcscissor();
        }

        bool noshadow() const
        {
            return flags&LightEnt_NoShadow || radius <= smminradius;
        }
        bool nospec() const
        {
            return (flags&LightEnt_NoSpecular) != 0;
        }
        bool volumetric() const
        {
            return (flags&LightEnt_Volumetric) != 0;
        }

        void addscissor(float &dx1, float &dy1, float &dx2, float &dy2) const
        {
            dx1 = std::min(dx1, sx1);
            dy1 = std::min(dy1, sy1);
            dx2 = std::max(dx2, sx2);
            dy2 = std::max(dy2, sy2);
        }

        void addscissor(float &dx1, float &dy1, float &dx2, float &dy2, float &dz1, float &dz2) const
        {
            addscissor(dx1, dy1, dx2, dy2);
            dz1 = std::min(dz1, sz1);
            dz2 = std::max(dz2, sz2);
        }

        bool validscissor() const
        {
            return sx1 < sx2 && sy1 < sy2 && sz1 < sz2;
        }

        bool checkquery() const
        {
            return query && query->owner == this && occlusionengine.checkquery(query);
        }

        void calcbb(vec &bbmin, vec &bbmax) const
        {
            if(spot > 0)
            {
                float spotscale = radius * tan360(spot);
                vec up     = vec(spotx).mul(spotscale).abs(),
                    right  = vec(spoty).mul(spotscale).abs(),
                    center = vec(dir).mul(radius).add(o);
                bbmin = bbmax = center;
                bbmin.sub(up).sub(right);
                bbmax.add(up).add(right);
                bbmin.min(o);
                bbmax.max(o);
            }
            else
            {
                bbmin = vec(o).sub(radius);
                bbmax = vec(o).add(radius);
            }
        }
    private:
        void calcspot()
        {
            quat orient(dir, vec(0, 0, dir.z < 0 ? -1 : 1));
            spotx = orient.invertedrotate(vec(1, 0, 0));
            spoty = orient.invertedrotate(vec(0, 1, 0));
        }

        void calcscissor()
        {
            sx1 = sy1 = sz1 = -1;
            sx2 = sy2 = sz2 = 1;
            if(spot > 0)
            {
                calcspotscissor(o, radius, dir, spot, spotx, spoty, sx1, sy1, sx2, sy2, sz1, sz2);
            }
            else
            {
                calcspherescissor(o, radius, sx1, sy1, sx2, sy2, sz1, sz2);
            }
        }
};

struct shadowcachekey
{
    vec o;
    float radius;
    vec dir;
    int spot;

    bool operator==(const shadowcachekey &y) const
    {
        return o == y.o && radius == y.radius && dir == y.dir && spot == y.spot;
    }

    shadowcachekey() {}
    shadowcachekey(const lightinfo &l) : o(l.o), radius(l.radius), dir(l.dir), spot(l.spot) {}
};

template <>
struct std::hash<shadowcachekey>
{
    size_t operator()(const shadowcachekey &k) const
    {
        auto vechash = std::hash<vec>();
        return vechash(k.o);
    }
};

struct shadowcacheval
{
    ushort x, y, size, sidemask;


static inline bool htcmp(const shadowcachekey &x, const shadowcachekey &y)
{
    return x.o == y.o && x.radius == y.radius && x.dir == y.dir && x.spot == y.spot;
}

    shadowcacheval() {}
    shadowcacheval(const shadowmapinfo &sm) : x(sm.x), y(sm.y), size(sm.size), sidemask(sm.sidemask) {}
};

class ShadowAtlas
{
    public:
        GLuint fbo = 0;
        std::unordered_map<shadowcachekey, shadowcacheval> cache;
        bool full = false;

        void cleanup();
        void view();
        void setup();
        void setcomparemode(); //will call one of setsm(non)comparemode()
        void bind();

    private:
        GLuint tex = 0;
        GLenum target = GL_NONE;

        void setsmnoncomparemode();
        void setsmcomparemode();
        bool usesmcomparemode();

};

void ShadowAtlas::cleanup()
{
    if(tex)
    {
        glDeleteTextures(1, &tex);
        tex = 0;
    }
    if(fbo)
    {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    clearshadowcache();
}

void ShadowAtlas::bind()
{
    glBindTexture(target, tex);
}

ShadowAtlas shadowatlas;

//`s`hadow `m`ap vars
FVAR(smpolyfactor, -1e3f, 1, 1e3f);
FVAR(smpolyoffset, -1e3f, 0, 1e3f);
FVAR(smbias, -1e6f, 0.01f, 1e6f);
FVAR(smpolyfactor2, -1e3f, 1.5f, 1e3f);
FVAR(smpolyoffset2, -1e3f, 0, 1e3f);
FVAR(smbias2, -1e6f, 0.02f, 1e6f);
FVAR(smprec, 1e-3f, 1, 1e3f);
FVAR(smcubeprec, 1e-3f, 1, 1e3f);
FVAR(smspotprec, 1e-3f, 1, 1e3f);

VARFP(smsize, 10, 12, 14, shadowatlas.cleanup()); //size of shadow map: 2^size = x,y dimensions (1024x1024 at 10, 16384x16384 at 14)
VARFP(smdepthprec, 0, 0, 2, shadowatlas.cleanup()); //bit depth of sm depth map: 16bpp, 24bpp, or 32bpp respectively
VAR(smsidecull, 0, 1, 1); //`s`hadow `m`ap `side` `cull`: toggles culling lights outside the view frustum (outside the fov)
VAR(smviscull, 0, 1, 1);  //`s`hadow `m`ap `vis`ibility `cull`ing: toggles visibility culling based of distance
VAR(smborder, 0, 3, 16);  //`s`hadow `m`ap border
VAR(smborder2, 0, 4, 16); //smborder if smfilter > 2, for filter bleed reasons
VAR(smminsize, 1, 96, 1024); //min size for individual sm, not whole buffer
VAR(smmaxsize, 1, 384, 1024); //max size for individual sm, not whole buffer
//VAR(smmaxsize, 1, 4096, 4096);
VAR(smused, 1, 0, 0); //read only: shadow map area used
VAR(smquery, 0, 1, 1); // `s`hadow `m`ap `query1: whether to occlusion query lights
VARF(smcullside, 0, 1, 1, shadowatlas.cleanup());
VARF(smcache, 0, 1, 2, shadowatlas.cleanup());
VARFP(smfilter, 0, 2, 3, { cleardeferredlightshaders(); shadowatlas.cleanup(); cleanupvolumetric(); });
VARFP(smgather, 0, 0, 1, { cleardeferredlightshaders(); shadowatlas.cleanup(); cleanupvolumetric(); });
VAR(smnoshadow, 0, 0, 1);
VAR(smdynshadow, 0, 1, 1); //`s`hadow `m`ap `dyn`amic `shadow`

void ShadowAtlas::setsmnoncomparemode() // use texture gather
{
    glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

void ShadowAtlas::setsmcomparemode() // use embedded shadow cmp
{
    glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void ShadowAtlas::setcomparemode()
{
    if(usesmcomparemode())
    {
        setsmcomparemode();
    }
    else
    {
        setsmnoncomparemode();
    }
}

static bool usegatherforsm()
{
    return smfilter > 1 && smgather && usetexgather;
}

bool ShadowAtlas::usesmcomparemode()
{
    return !usegatherforsm() || (usetexgather > 1);
}

void ShadowAtlas::view()
{
    int w = std::min(hudw(), hudh())/2,
        h = (w*hudh())/hudw(),
        x = hudw()-w,
        y = hudh()-h;
    float tw = 1,
          th = 1;
    if(target == GL_TEXTURE_RECTANGLE)
    {
        vec2 sasize = shadowatlaspacker.dimensions();
        tw = sasize.x;
        th = sasize.y;
        SETSHADER(hudrect);
    }
    else
    {
        hudshader->set();
    }
    gle::colorf(1, 1, 1);
    glBindTexture(target, tex);
    if(usesmcomparemode())
    {
        setsmnoncomparemode();
    }
    debugquad(x, y, w, h, 0, 0, tw, th);
    if(usesmcomparemode())
    {
        setsmcomparemode();
    }
}
VAR(debugshadowatlas, 0, 0, 1);

void ShadowAtlas::setup()
{
    int size = std::min((1<<smsize), hwtexsize);
    shadowatlaspacker.resize(size, size);

    if(!tex)
    {
        glGenTextures(1, &tex);
    }
    vec2 sasize = shadowatlaspacker.dimensions();
    target = usegatherforsm() ? GL_TEXTURE_2D : GL_TEXTURE_RECTANGLE;
    createtexture(tex, sasize.x, sasize.y, nullptr, 3, 1, smdepthprec > 1 ? GL_DEPTH_COMPONENT32 : (smdepthprec ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16), target);
    glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    if(!fbo)
    {
        glGenFramebuffers(1, &fbo);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, tex, 0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fatal("failed allocating shadow atlas!");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

const matrix4 cubeshadowviewmatrix[6] =
{
    // sign-preserving cubemap projections
    matrix4(vec(0, 0, 1), vec(0, 1, 0), vec(-1, 0, 0)), // +X
    matrix4(vec(0, 0, 1), vec(0, 1, 0), vec( 1, 0, 0)), // -X
    matrix4(vec(1, 0, 0), vec(0, 0, 1), vec(0, -1, 0)), // +Y
    matrix4(vec(1, 0, 0), vec(0, 0, 1), vec(0,  1, 0)), // -Y
    matrix4(vec(1, 0, 0), vec(0, 1, 0), vec(0, 0, -1)), // +Z
    matrix4(vec(1, 0, 0), vec(0, 1, 0), vec(0, 0,  1))  // -Z
};

static constexpr int LightTile_MaxBatch = 8; //also used in lightbatchkey below

VARF(lighttilebatch, 0, LightTile_MaxBatch, LightTile_MaxBatch, cleardeferredlightshaders());
VARF(batchsunlight, 0, 2, 2, cleardeferredlightshaders());

int shadowmapping = 0;

class lightrect
{
    public:
        uchar x1, y1, x2, y2;

        lightrect() {}
        lightrect(const lightinfo &l)
        {
            calctilebounds(l.sx1, l.sy1, l.sx2, l.sy2, x1, y1, x2, y2);
        }

        bool outside(const lightrect &o) const
        {
            return x1 >= o.x2 || x2 <= o.x1 || y1 >= o.y2 || y2 <= o.y1;
        }

        bool inside(const lightrect &o) const
        {
            return x1 >= o.x1 && x2 <= o.x2 && y1 >= o.y1 && y2 <= o.y2;
        }

        void intersect(const lightrect &o)
        {
            x1 = std::max(x1, o.x1);
            y1 = std::max(y1, o.y1);
            x2 = std::min(x2, o.x2);
            y2 = std::min(y2, o.y2);
        }

        bool overlaps(int tx1, int ty1, int tx2, int ty2, const uint *tilemask) const
        {
            if(static_cast<int>(x2) <= tx1 || static_cast<int>(x1) >= tx2 || static_cast<int>(y2) <= ty1 || static_cast<int>(y1) >= ty2)
            {
                return false;
            }
            if(!tilemask)
            {
                return true;
            }
            uint xmask = (1<<x2) - (1<<x1);
            for(int y = std::max(static_cast<int>(y1), ty1), end = std::min(static_cast<int>(y2), ty2); y < end; y++)
            {
                if(tilemask[y] & xmask)
                {
                    return true;
                }
            }
            return false;
        }
    protected:
        //only called by child batchstack object
        lightrect(uchar x1, uchar y1, uchar x2, uchar y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}
};

//batchflag enum is local to this file
enum
{
    BatchFlag_Spotlight = 1<<0,
    BatchFlag_NoShadow  = 1<<1,
    BatchFlag_NoSun     = 1<<2
};

struct lightbatchkey
{
    uchar flags, numlights;
    ushort lights[LightTile_MaxBatch];
};

struct lightbatch : lightbatchkey
{
    std::vector<lightrect> rects;

    void reset()
    {
        rects.clear();
    }

    bool overlaps(int tx1, int ty1, int tx2, int ty2, const uint *tilemask) const
    {
        if(!tx1 && !ty1 && tx2 >= lighttilew && ty2 >= lighttileh && !tilemask)
        {
            return true;
        }
        for(uint i = 0; i < rects.size(); i++)
        {
            if(rects[i].overlaps(tx1, ty1, tx2, ty2, tilemask))
            {
                return true;
            }
        }
        return false;
    }
};

static std::vector<lightinfo> lights;
static std::vector<int> lightorder;
static std::vector<const lightbatch *> lightbatches;
std::vector<shadowmapinfo> shadowmaps;

void clearshadowcache()
{
    shadowmaps.clear();

    clearradiancehintscache();
    clearshadowmeshes();
}

void addshadowmap(ushort x, ushort y, int size, int &idx, int light, const shadowcacheval *cached)
{
    idx = shadowmaps.size();
    shadowmapinfo sm;
    sm.x = x;
    sm.y = y;
    sm.size = size;
    sm.light = light;
    sm.sidemask = 0;
    sm.cached = cached;
    shadowmaps.push_back(sm);
}

//calculate bouunding box reflective shadow map splits
int calcbbrsmsplits(const ivec &bbmin, const ivec &bbmax)
{
    if(!rsmcull)
    {
        return 1;
    }
    for(int k = 0; k < 4; ++k)
    {
        const plane &p = rsm.cull[k];
        ivec omin, omax;
        if(p.x > 0)
        {
            omin.x = bbmin.x;
            omax.x = bbmax.x;
        }
        else
        {
            omin.x = bbmax.x;
            omax.x = bbmin.x;
        }
        if(p.y > 0)
        {
            omin.y = bbmin.y;
            omax.y = bbmax.y;
        }
        else
        {
            omin.y = bbmax.y;
            omax.y = bbmin.y;
        }
        if(p.z > 0)
        {
            omin.z = bbmin.z;
            omax.z = bbmax.z;
        }
        else
        {
            omin.z = bbmax.z;
            omax.z = bbmin.z;
        }
        if(omax.dist(p) < 0)
        {
            return 0;
        }
        if(omin.dist(p) < 0)
        {
            while(++k < 4)
            {
                const plane &p = rsm.cull[k];
                ivec omax(p.x > 0 ? bbmax.x : bbmin.x, p.y > 0 ? bbmax.y : bbmin.y, p.z > 0 ? bbmax.z : bbmin.z);
                if(omax.dist(p) < 0)
                {
                    return 0;
                }
            }
        }
    }
    return 1;
}

int calcspherersmsplits(const vec &center, float radius)
{
    if(!rsmcull)
    {
        return 1;
    }
    for(int k = 0; k < 4; ++k)
    {
        const plane &p = rsm.cull[k];
        float dist = p.dist(center);
        if(dist < -radius)
        {
            return 0;
        }
        if(dist < radius)
        {
            while(++k < 4)
            {
                const plane &p = rsm.cull[k];
                if(p.dist(center) < -radius)
                {
                    return 0;
                }
            }
        }
    }
    return 1;
}

FVAR(avatarshadowdist, 0, 12, 100);
FVAR(avatarshadowbias, 0, 8, 100);
VARF(avatarshadowstencil, 0, 1, 2, initwarning("g-buffer setup", Init_Load, Change_Shaders));

int avatarmask = 0;

bool useavatarmask()
{
    return avatarshadowstencil && ghasstencil && (!msaasamples || (msaalight && avatarshadowstencil > 1));
}

void enableavatarmask()
{
    if(useavatarmask())
    {
        avatarmask = 0x40;
        glStencilFunc(GL_ALWAYS, avatarmask, ~0);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glEnable(GL_STENCIL_TEST);
    }
}

void disableavatarmask()
{
    if(avatarmask)
    {
        avatarmask = 0;
        glDisable(GL_STENCIL_TEST);
    }
}

VAR(forcespotlights, 1, 0, 0);

static Shader *volumetricshader = nullptr;
std::array<Shader *, 2> volumetricbilateralshader = { nullptr, nullptr };

void clearvolumetricshaders()
{
    volumetricshader = nullptr;
    volumetricbilateralshader.fill(nullptr);
}

VARFP(volumetric, 0, 1, 1, cleanupvolumetric());    //toggles displaying volumetric lights
VARFP(volreduce, 0, 1, 2, cleanupvolumetric());     //size reduction factor for volumetric tex: 1 is 1/4, 2 is 1/16
VARFP(volbilateral, 0, 1, 3, cleanupvolumetric());  //toggles bilateral filtering
FVAR(volbilateraldepth, 0, 4, 1e3f);                //bilateral filtering depth
VARFP(volblur, 0, 1, 3, cleanupvolumetric());
VARFP(volsteps, 1, 32, 128, cleanupvolumetric());   //iterations to run for volumetric algorithm
FVAR(volminstep, 0, 0.0625f, 1e3f);
FVAR(volprefilter, 0, 0.1, 1e3f);
FVAR(voldistclamp, 0, 0.99f, 2);
CVAR1R(volcolor, 0x808080);
FVARR(volscale, 0, 1, 16);

Shader *loadvolumetricshader()
{
    std::string common, shadow;

    if(usegatherforsm())
    {
        common.push_back(smfilter > 2 ? 'G' : 'g');
    }
    else if(smfilter)
    {
        common.push_back(smfilter > 2 ? 'E' : (smfilter > 1 ? 'F' : 'f'));
    }
    if(spotlights || forcespotlights)
    {
        common.push_back('s');
    }

    shadow.push_back('p');

    DEF_FORMAT_STRING(name, "volumetric%s%s%d", common.c_str(), shadow.c_str(), volsteps);
    return generateshader(name, "volumetricshader \"%s\" \"%s\" %d", common.c_str(), shadow.c_str(), volsteps);
}

static void loadvolumetricshaders()
{
    volumetricshader = loadvolumetricshader();

    if(volbilateral)
    {
        for(int i = 0; i < 2; ++i)
        {
            DEF_FORMAT_STRING(name, "volumetricbilateral%c%d%d", 'x' + i, volbilateral, volreduce);
            volumetricbilateralshader[i] = generateshader(name, "volumetricbilateralshader %d %d", volbilateral, volreduce);
        }
    }
}

static int volw = -1,
           volh = -1;
static std::array<GLuint, 2> volfbo = { 0, 0 },
                             voltex = { 0, 0 };

static void setupvolumetric(int w, int h)
{
    volw = w>>volreduce;
    volh = h>>volreduce;

    for(int i = 0; i < (volbilateral || volblur ? 2 : 1); ++i)
    {
        if(!voltex[i])
        {
            glGenTextures(1, &voltex[i]);
        }
        if(!volfbo[i])
        {
            glGenFramebuffers(1, &volfbo[i]);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, volfbo[i]);

        createtexture(voltex[i], volw, volh, nullptr, 3, 1, hdrformat, GL_TEXTURE_RECTANGLE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, voltex[i], 0);

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fatal("failed allocating volumetric buffer!");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    loadvolumetricshaders();
}

void cleanupvolumetric()
{
    for(GLuint &i : volfbo)
    {
        if(i)
        {
            glDeleteFramebuffers(1, &i);
            i = 0;
        }
    }
    for(GLuint &i : voltex)
    {
        if(i)
        {
            glDeleteTextures(1, &i);
            i = 0;
        }
    }
    volw = volh = -1;

    clearvolumetricshaders();
}

static Shader *deferredlightshader      = nullptr,
              *deferredminimapshader    = nullptr,
              *deferredmsaapixelshader  = nullptr,
              *deferredmsaasampleshader = nullptr;

void cleardeferredlightshaders()
{
    deferredlightshader      = nullptr;
    deferredminimapshader    = nullptr;
    deferredmsaapixelshader  = nullptr;
    deferredmsaasampleshader = nullptr;
}

Shader *loaddeferredlightshader(const char *type = nullptr)
{
    string common, shadow, sun;
    int commonlen = 0,
        shadowlen = 0,
        sunlen    = 0;

    bool minimap     = false,
         multisample = false,
         avatar      = true;
    if(type)
    {
        if(std::strchr(type, 'm'))
        {
            minimap = true;
        }
        if(std::strchr(type, 'M'))
        {
            multisample = true;
        }
        if(std::strchr(type, 'D'))
        {
            avatar = false;
        }
        copystring(common, type);
        commonlen = std::strlen(common);
    }
    if(!minimap)
    {
        if(!multisample || msaalight)
        {
            common[commonlen++] = 't';
        }
        if(avatar && useavatarmask())
        {
            common[commonlen++] = 'd';
        }
        if(lighttilebatch)
        {
            common[commonlen++] = 'n';
            common[commonlen++] = '0' + lighttilebatch;
        }
    }
    if(usegatherforsm())
    {
        common[commonlen++] = smfilter > 2 ? 'G' : 'g';
    }
    else if(smfilter)
    {
        common[commonlen++] = smfilter > 2 ? 'E' : (smfilter > 1 ? 'F' : 'f');
    }
    if(spotlights || forcespotlights)
    {
        common[commonlen++] = 's';
    }
    if(nospeclights)
    {
        common[commonlen++] = 'z';
    }
    common[commonlen] = '\0';

    shadow[shadowlen++] = 'p';
    shadow[shadowlen] = '\0';

    int usecsm = 0,
        userh = 0;
    if(!sunlight.iszero() && csm.getcsmproperty(cascadedshadowmap::ShadowMap))
    {
        usecsm = csm.getcsmproperty(cascadedshadowmap::Splits);
        sun[sunlen++] = 'c';
        sun[sunlen++] = '0' + usecsm;
        if(!minimap)
        {
            if(avatar && ao && aosun)
            {
                sun[sunlen++] = 'A';
            }
            if(gi && giscale && gidist)
            {
                userh = rhsplits;
                sun[sunlen++] = 'r';
                sun[sunlen++] = '0' + rhsplits;
            }
        }
    }
    if(!minimap)
    {
        if(avatar && ao)
        {
            sun[sunlen++] = 'a';
        }
        if(lighttilebatch && (!usecsm || batchsunlight > (userh ? 1 : 0)))
        {
            sun[sunlen++] = 'b';
        }
    }
    sun[sunlen] = '\0';

    DEF_FORMAT_STRING(name, "deferredlight%s%s%s", common, shadow, sun);
    return generateshader(name, "deferredlightshader \"%s\" \"%s\" \"%s\" %d %d %d", common, shadow, sun, usecsm, userh, !minimap ? lighttilebatch : 0);
}

void loaddeferredlightshaders()
{
    if(msaasamples)
    {
        string opts;
        if(msaalight > 2)
        {
            copystring(opts, "MS");
        }
        else if(msaalight==2)
        {
            copystring(opts, ghasstencil || !msaaedgedetect ? "MO" : "MOT");
        }
        else
        {
            formatstring(opts, ghasstencil || !msaaedgedetect ? "MR%d" : "MRT%d", msaasamples);
        }
        deferredmsaasampleshader = loaddeferredlightshader(opts);
        deferredmsaapixelshader = loaddeferredlightshader("M");
        deferredlightshader = msaalight ? deferredmsaapixelshader : loaddeferredlightshader("D");
    }
    else
    {
        deferredlightshader = loaddeferredlightshader();
    }
}

static bool sortlights(int x, int y)
{
    const lightinfo &xl = lights[x],
                    &yl = lights[y];
    if(!xl.spot)
    {
        if(yl.spot)
        {
            return true;
        }
    }
    else if(!yl.spot)
    {
        return false;
    }
    if(!xl.noshadow())
    {
        if(yl.noshadow())
        {
            return true;
        }
    }
    else if(!yl.noshadow())
    {
        return false;
    }
    if(xl.sz1 < yl.sz1)
    {
        return true;
    }
    else if(xl.sz1 > yl.sz1)
    {
        return false;
    }
    return xl.dist - xl.radius < yl.dist - yl.radius;
}

VAR(lighttilealignw, 1, 16, 256); // x tiling size for lights inside the shadow cache (pixel grid size to snap to)
VAR(lighttilealignh, 1, 16, 256); // y tiling size for lights
int lighttilemaxw = variable("lighttilew", 1, 10, lighttilemaxwidth, &lighttilemaxw, nullptr, 0);
int lighttilemaxh = variable("lighttileh", 1, 10, lighttilemaxheight, &lighttilemaxh, nullptr, 0);

int lighttilew     = 0,
    lighttileh     = 0,
    lighttilevieww = 0,
    lighttileviewh = 0;

void calctilesize()
{
    lighttilevieww = (vieww + lighttilealignw - 1)/lighttilealignw;
    lighttileviewh = (viewh + lighttilealignh - 1)/lighttilealignh;
    lighttilew = std::min(lighttilevieww, lighttilemaxw);
    lighttileh = std::min(lighttileviewh, lighttilemaxh);
}

void resetlights()
{
    static constexpr int shadowcacheevict = 2;
    static int evictshadowcache = 0;
    shadowatlas.cache.clear();
    if(smcache)
    {
        vec2 sasize = shadowatlaspacker.dimensions();
        int evictx = ((evictshadowcache%shadowcacheevict)*sasize.x)/shadowcacheevict,
            evicty = ((evictshadowcache/shadowcacheevict)*sasize.y)/shadowcacheevict,
            evictx2 = (((evictshadowcache%shadowcacheevict)+1)*sasize.x)/shadowcacheevict,
            evicty2 = (((evictshadowcache/shadowcacheevict)+1)*sasize.y)/shadowcacheevict;
        for(const shadowmapinfo &sm : shadowmaps)
        {
            if(sm.light < 0)
            {
                continue;
            }
            lightinfo &l = lights[sm.light];
            if(sm.cached && shadowatlas.full)
            {
                int w = l.spot ? sm.size : sm.size*3,
                    h = l.spot ? sm.size : sm.size*2;
                if(sm.x < evictx2 && sm.x + w > evictx && sm.y < evicty2 && sm.y + h > evicty)
                {
                    continue;
                }
            }
            shadowatlas.cache[l] = sm;
        }
        if(shadowatlas.full)
        {
            evictshadowcache = (evictshadowcache + 1)%(shadowcacheevict*shadowcacheevict);
            shadowatlas.full = false;
        }
    }

    lights.clear();
    lightorder.clear();

    shadowmaps.clear();
    shadowatlaspacker.reset();

    calctilesize();
}

VAR(depthtestlights, 0, 2, 2);
FVAR(depthtestlightsclamp, 0, 0.999995f, 1); //z margin for light depth testing at depthtestlights = 2
VAR(depthfaillights, 0, 1, 1);

static void lightquads(float z, float sx1, float sy1, float sx2, float sy2)
{
    gle::attribf(sx1, sy1, z);
    gle::attribf(sx1, sy2, z);
    gle::attribf(sx2, sy2, z);
    gle::attribf(sx1, sy1, z);
    gle::attribf(sx2, sy2, z);
    gle::attribf(sx2, sy1, z);

}

static void lightquads(float z, float sx1, float sy1, float sx2, float sy2, int tx1, int ty1, int tx2, int ty2)
{
    int vx1 = std::max(static_cast<int>(std::floor((sx1*0.5f+0.5f)*vieww)), ((tx1*lighttilevieww)/lighttilew)*lighttilealignw),
        vy1 = std::max(static_cast<int>(std::floor((sy1*0.5f+0.5f)*viewh)), ((ty1*lighttileviewh)/lighttileh)*lighttilealignh),
        vx2 = std::min(static_cast<int>(std::ceil((sx2*0.5f+0.5f)*vieww)), std::min(((tx2*lighttilevieww)/lighttilew)*lighttilealignw, vieww)),
        vy2 = std::min(static_cast<int>(std::ceil((sy2*0.5f+0.5f)*viewh)), std::min(((ty2*lighttileviewh)/lighttileh)*lighttilealignh, viewh));
    lightquads(z, (vx1*2.0f)/vieww-1.0f, (vy1*2.0f)/viewh-1.0f, (vx2*2.0f)/vieww-1.0f, (vy2*2.0f)/viewh-1.0f);
}

static void lightquads(float z, float sx1, float sy1, float sx2, float sy2, int x1, int y1, int x2, int y2, const uint *tilemask)
{
    if(!tilemask)
    {
        lightquads(z, sx1, sy1, sx2, sy2, x1, y1, x2, y2);
    }
    else
    {
        for(int y = y1; y < y2;)
        {
            int starty = y;
            uint xmask     = (1<<x2) - (1<<x1),
                 startmask = tilemask[y] & xmask;
            do
            {
                ++y;
            } while(y < y2 && (tilemask[y]&xmask) == startmask);
            for(int x = x1; x < x2;)
            {
                while(x < x2 && !(startmask&(1<<x)))
                {
                    ++x;
                }
                if(x >= x2)
                {
                    break;
                }
                int startx = x;
                do
                {
                    ++x;
                } while(x < x2 && startmask&(1<<x));
                lightquads(z, sx1, sy1, sx2, sy2, startx, starty, x, y);
            }
        }
    }
}

static void lightquad(float sz1, float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask)
{
    int btx1, bty1, btx2, bty2;
    calctilebounds(bsx1, bsy1, bsx2, bsy2, btx1, bty1, btx2, bty2);

    gle::begin(GL_TRIANGLES);
    lightquads(sz1, bsx1, bsy1, bsx2, bsy2, btx1, bty1, btx2, bty2, tilemask);
    gle::end();
}

void GBuffer::bindlighttexs(int msaapass, bool transparent) const
{
    if(msaapass)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mscolortex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gcolortex);
    }
    glActiveTexture(GL_TEXTURE1);
    if(msaapass)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gnormaltex);
    }
    if(transparent)
    {
        glActiveTexture(GL_TEXTURE2);
        if(msaapass)
        {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msglowtex);
        }
        else
        {
            glBindTexture(GL_TEXTURE_RECTANGLE, gglowtex);
        }
    }
    glActiveTexture(GL_TEXTURE3);
    if(msaapass)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    }
    glActiveTexture(GL_TEXTURE4);
    shadowatlas.bind();
    shadowatlas.setcomparemode();
    if(ao)
    {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_RECTANGLE, aotex[2] ? aotex[2] : aotex[0]);
    }
    if(useradiancehints())
    {
        for(int i = 0; i < 4; ++i)
        {
            glActiveTexture(GL_TEXTURE6 + i);
            glBindTexture(GL_TEXTURE_3D, rhtex[i]);
        }
    }
    glActiveTexture(GL_TEXTURE0);
}

static void setlightglobals(bool transparent = false)
{
    vec2 sasize = shadowatlaspacker.dimensions();
    GLOBALPARAMF(shadowatlasscale, 1.0f/sasize.x, 1.0f/sasize.y);
    if(ao)
    {
        if(transparent || drawtex || (editmode && fullbright))
        {
            GLOBALPARAMF(aoscale, 0.0f, 0.0f);
            GLOBALPARAMF(aoparams, 1.0f, 0.0f, 1.0f, 0.0f);
        }
        else
        {
            GLOBALPARAM(aoscale, aotex[2] ? vec2(1, 1) : vec2(static_cast<float>(aow)/vieww, static_cast<float>(aoh)/viewh));
            GLOBALPARAMF(aoparams, aomin, 1.0f-aomin, aosunmin, 1.0f-aosunmin);
        }
    }
    float lightscale = 2.0f*ldrscaleb();
    if(!drawtex && editmode && fullbright)
    {
        GLOBALPARAMF(lightscale, fullbrightlevel*lightscale, fullbrightlevel*lightscale, fullbrightlevel*lightscale, 255*lightscale);
    }
    else
    {
        GLOBALPARAMF(lightscale, ambient.x*lightscale*ambientscale, ambient.y*lightscale*ambientscale, ambient.z*lightscale*ambientscale, 255*lightscale);
    }
    if(!sunlight.iszero() && csm.getcsmproperty(cascadedshadowmap::ShadowMap))
    {
        csm.bindparams();
        rh.bindparams();
        if(!drawtex && editmode && fullbright)
        {
            GLOBALPARAMF(sunlightdir, 0, 0, 0);
            GLOBALPARAMF(sunlightcolor, 0, 0, 0);
            GLOBALPARAMF(giscale, 0);
            GLOBALPARAMF(skylightcolor, 0, 0, 0);
        }
        else
        {
            GLOBALPARAM(sunlightdir, sunlightdir);
            GLOBALPARAMF(sunlightcolor, sunlight.x*lightscale*sunlightscale, sunlight.y*lightscale*sunlightscale, sunlight.z*lightscale*sunlightscale);
            GLOBALPARAMF(giscale, 2*giscale);
            GLOBALPARAMF(skylightcolor, 2*giaoscale*skylight.x*lightscale*skylightscale, 2*giaoscale*skylight.y*lightscale*skylightscale, 2*giaoscale*skylight.z*lightscale*skylightscale);
        }
    }

    matrix4 lightmatrix;
    lightmatrix.identity();
    GLOBALPARAM(lightmatrix, lightmatrix);
}

//values only for interaction between setlightparams() and setlightshader()
struct lightparaminfo
{
    vec4<float> lightposv[8], lightcolorv[8], spotparamsv[8], shadowparamsv[8];
    vec2 shadowoffsetv[8];
};

//sets the ith element of lightposv, lightcolorv, spotparamsv, shadowparamsv, shadowoffsetv
//UB if i > 7
//
static void setlightparams(int i, const lightinfo &l, lightparaminfo &li)
{
    li.lightposv[i]   = vec4<float>(l.o, 1).div(l.radius);
    li.lightcolorv[i] = vec4<float>(vec(l.color).mul(2*ldrscaleb()), l.nospec() ? 0 : 1);
    if(l.spot > 0)
    {
        li.spotparamsv[i] = vec4<float>(vec(l.dir).neg(), 1/(1 - cos360(l.spot)));
    }
    if(l.shadowmap >= 0)
    {
        const shadowmapinfo &sm = shadowmaps[l.shadowmap];
        float smnearclip = SQRT3 / l.radius, smfarclip = SQRT3,
              bias = (smfilter > 2 || shadowatlaspacker.dimensions().x > shadowatlassize ? smbias2 : smbias) * (smcullside ? 1 : -1) * smnearclip * (1024.0f / sm.size);
        int border = smfilter > 2 ? smborder2 : smborder;
        if(l.spot > 0)
        {
            li.shadowparamsv[i] = vec4<float>(
                -0.5f * sm.size * cotan360(l.spot),
                (-smnearclip * smfarclip / (smfarclip - smnearclip) - 0.5f*bias),
                1 / (1 + std::fabs(l.dir.z)),
                0.5f + 0.5f * (smfarclip + smnearclip) / (smfarclip - smnearclip));
        }
        else
        {
            li.shadowparamsv[i] = vec4<float>(
                -0.5f * (sm.size - border),
                -smnearclip * smfarclip / (smfarclip - smnearclip) - 0.5f*bias,
                sm.size,
                0.5f + 0.5f * (smfarclip + smnearclip) / (smfarclip - smnearclip));
        }
        li.shadowoffsetv[i] = vec2(sm.x + 0.5f*sm.size, sm.y + 0.5f*sm.size);
    }
}

static void setlightshader(Shader *s, const lightparaminfo &li, int n, bool baselight, bool shadowmap, bool spotlight, bool transparent = false, bool avatar = false)
{
    static const LocalShaderParam lightpos("lightpos"),
                                  lightcolor("lightcolor"),
                                  spotparams("spotparams"),
                                  shadowparams("shadowparams"),
                                  shadowoffset("shadowoffset");
    s->setvariant(n-1, (shadowmap ? 1 : 0) + (baselight ? 0 : 2) + (spotlight ? 4 : 0) + (transparent ? 8 : 0) + (avatar ? 24 : 0));
    lightpos.setv(li.lightposv, n);
    lightcolor.setv(li.lightcolorv, n);
    if(spotlight)
    {
        spotparams.setv(li.spotparamsv, n);
    }
    if(shadowmap)
    {
        shadowparams.setv(li.shadowparamsv, n);
        shadowoffset.setv(li.shadowoffsetv, n);
    }
}

static void setavatarstencil(int stencilref, bool on)
{
    glStencilFunc(GL_EQUAL, (on ? 0x40 : 0) | stencilref, !(stencilref&0x08) && msaalight==2 ? 0x47 : 0x4F);
}

void GBuffer::rendersunpass(Shader *s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask)
{
    if(hasDBT && depthtestlights > 1)
    {
        glDepthBounds_(0, depthtestlightsclamp);
    }
    int tx1 = std::max(static_cast<int>(std::floor((bsx1*0.5f+0.5f)*vieww)), 0),
        ty1 = std::max(static_cast<int>(std::floor((bsy1*0.5f+0.5f)*viewh)), 0),
        tx2 = std::min(static_cast<int>(std::ceil((bsx2*0.5f+0.5f)*vieww)), vieww),
        ty2 = std::min(static_cast<int>(std::ceil((bsy2*0.5f+0.5f)*viewh)), viewh);
    s->setvariant(transparent ? 0 : -1, 16);
    lightquad(-1, (tx1*2.0f)/vieww-1.0f, (ty1*2.0f)/viewh-1.0f, (tx2*2.0f)/vieww-1.0f, (ty2*2.0f)/viewh-1.0f, tilemask);
    lightpassesused++;

    if(stencilref >= 0)
    {
        setavatarstencil(stencilref, true);

        s->setvariant(0, 17);
        lightquad(-1, (tx1*2.0f)/vieww-1.0f, (ty1*2.0f)/viewh-1.0f, (tx2*2.0f)/vieww-1.0f, (ty2*2.0f)/viewh-1.0f, tilemask);
        lightpassesused++;

        setavatarstencil(stencilref, false);
    }
}

void GBuffer::renderlightsnobatch(Shader *s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2)
{
    lightsphere::enable();

    glEnable(GL_SCISSOR_TEST);

    bool outside = true;
    static lightparaminfo li;
    for(int avatarpass = 0; avatarpass < (stencilref >= 0 ? 2 : 1); ++avatarpass)
    {
        if(avatarpass)
        {
            setavatarstencil(stencilref, true);
        }

        for(uint i = 0; i < lightorder.size(); i++)
        {
            const lightinfo &l = lights[lightorder[i]];
            float sx1 = std::max(bsx1, l.sx1),
                  sy1 = std::max(bsy1, l.sy1),
                  sx2 = std::min(bsx2, l.sx2),
                  sy2 = std::min(bsy2, l.sy2);
            if(sx1 >= sx2 || sy1 >= sy2 || l.sz1 >= l.sz2 || (avatarpass && l.dist - l.radius > avatarshadowdist))
            {
                continue;
            }
            matrix4 lightmatrix = camprojmatrix;
            lightmatrix.translate(l.o);
            lightmatrix.scale(l.radius);
            GLOBALPARAM(lightmatrix, lightmatrix);

            setlightparams(0, l, li);
            setlightshader(s, li, 1, false, l.shadowmap >= 0, l.spot > 0, transparent, avatarpass > 0);

            int tx1 = static_cast<int>(std::floor((sx1*0.5f+0.5f)*vieww)),
                ty1 = static_cast<int>(std::floor((sy1*0.5f+0.5f)*viewh)),
                tx2 = static_cast<int>(std::ceil((sx2*0.5f+0.5f)*vieww)),
                ty2 = static_cast<int>(std::ceil((sy2*0.5f+0.5f)*viewh));
            glScissor(tx1, ty1, tx2-tx1, ty2-ty1);

            if(hasDBT && depthtestlights > 1)
            {
                glDepthBounds_(l.sz1*0.5f + 0.5f, std::min(l.sz2*0.5f + 0.5f, depthtestlightsclamp));
            }

            if(camera1->o.dist(l.o) <= l.radius + nearplane + 1 && depthfaillights)
            {
                if(outside)
                {
                    outside = false;
                    glDepthFunc(GL_GEQUAL);
                    glCullFace(GL_FRONT);
                }
            }
            else if(!outside)
            {
                outside = true;
                glDepthFunc(GL_LESS);
                glCullFace(GL_BACK);
            }

            lightsphere::draw();

            lightpassesused++;
        }

        if(avatarpass)
        {
            setavatarstencil(stencilref, false);
        }
    }

    if(!outside)
    {
        outside = true;
        glDepthFunc(GL_LESS);
        glCullFace(GL_BACK);
    }

    glDisable(GL_SCISSOR_TEST);

    lightsphere::disable();
}

void GBuffer::renderlightbatches(Shader &s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask)
{
    bool sunpass = !sunlight.iszero() && csm.getcsmproperty(cascadedshadowmap::ShadowMap) && batchsunlight <= (gi && giscale && gidist ? 1 : 0);
    int btx1, bty1, btx2, bty2;
    calctilebounds(bsx1, bsy1, bsx2, bsy2, btx1, bty1, btx2, bty2);
    static lightparaminfo li;
    for(uint i = 0; i < lightbatches.size(); i++)
    {
        const lightbatch &batch = *lightbatches[i];
        if(!batch.overlaps(btx1, bty1, btx2, bty2, tilemask))
        {
            continue;
        }

        int n = batch.numlights;
        float sx1 =  1,
              sy1 =  1,
              sx2 = -1,
              sy2 = -1,
              sz1 =  1,
              sz2 = -1;
        for(int j = 0; j < n; ++j)
        {
            const lightinfo &l = lights[batch.lights[j]];
            setlightparams(j, l, li); //set 0...batch.numlights
            l.addscissor(sx1, sy1, sx2, sy2, sz1, sz2);
        }

        bool baselight = !(batch.flags & BatchFlag_NoSun) && !sunpass;
        if(baselight)
        {
            sx1 = bsx1;
            sy1 = bsy1;
            sx2 = bsx2;
            sy2 = bsy2;
            sz1 = -1;
            sz2 =  1;
        }
        else
        {
            sx1 = std::max(sx1, bsx1);
            sy1 = std::max(sy1, bsy1);
            sx2 = std::min(sx2, bsx2);
            sy2 = std::min(sy2, bsy2);
            if(sx1 >= sx2 || sy1 >= sy2 || sz1 >= sz2)
            {
                continue;
            }
        }

        if(n)
        {
            bool shadowmap = !(batch.flags & BatchFlag_NoShadow),
                 spotlight = (batch.flags & BatchFlag_Spotlight) != 0;
            setlightshader(&s, li, n, baselight, shadowmap, spotlight, transparent);
        }
        else
        {
            s.setvariant(transparent ? 0 : -1, 16);
        }

        lightpassesused++;

        if(hasDBT && depthtestlights > 1)
        {
            glDepthBounds_(sz1*0.5f + 0.5f, std::min(sz2*0.5f + 0.5f, depthtestlightsclamp));
        }
        gle::begin(GL_TRIANGLES);
        for(uint j = 0; j < batch.rects.size(); j++)
        {
            const lightrect &r = batch.rects[j];
            int x1 = std::max(static_cast<int>(r.x1), btx1),
                y1 = std::max(static_cast<int>(r.y1), bty1),
                x2 = std::min(static_cast<int>(r.x2), btx2),
                y2 = std::min(static_cast<int>(r.y2), bty2);
            if(x1 < x2 && y1 < y2)
            {
                lightquads(sz1, sx1, sy1, sx2, sy2, x1, y1, x2, y2, tilemask);
            }
        }
        gle::end();
    }

    if(stencilref >= 0)
    {
        setavatarstencil(stencilref, true);

        bool baselight = !sunpass;
        for(int offset = 0; baselight || offset < static_cast<int>(lightorder.size()); baselight = false)
        {
            int n = 0;
            bool shadowmap = false,
                 spotlight = false;
            float sx1 =  1,
                  sy1 =  1,
                  sx2 = -1,
                  sy2 = -1,
                  sz1 =  1,
                  sz2 = -1;
            for(; offset < static_cast<int>(lightorder.size()); offset++)
            {
                const lightinfo &l = lights[lightorder[offset]];
                if(l.dist - l.radius > avatarshadowdist)
                {
                    continue;
                }
                if(!n)
                {
                    shadowmap = l.shadowmap >= 0;
                    spotlight = l.spot > 0;
                }
                else if(n >= lighttilebatch || (l.shadowmap >= 0) != shadowmap || (l.spot > 0) != spotlight)
                {
                    break;
                }
                setlightparams(n++, l, li);
                l.addscissor(sx1, sy1, sx2, sy2, sz1, sz2);
            }
            if(baselight)
            {
                sx1 = bsx1;
                sy1 = bsy1;
                sx2 = bsx2;
                sy2 = bsy2;
                sz1 = -1;
                sz2 =  1;
            }
            else
            {
                if(!n)
                {
                    break;
                }
                sx1 = std::max(sx1, bsx1);
                sy1 = std::max(sy1, bsy1);
                sx2 = std::min(sx2, bsx2);
                sy2 = std::min(sy2, bsy2);
                if(sx1 >= sx2 || sy1 >= sy2 || sz1 >= sz2)
                {
                    continue;
                }
            }

            if(n)
            {
                setlightshader(&s, li, n, baselight, shadowmap, spotlight, false, true);
            }
            else
            {
                s.setvariant(0, 17);
            }
            if(hasDBT && depthtestlights > 1)
            {
                glDepthBounds_(sz1*0.5f + 0.5f, std::min(sz2*0.5f + 0.5f, depthtestlightsclamp));
            }
            lightquad(sz1, sx1, sy1, sx2, sy2, tilemask);
            lightpassesused++;
        }

        setavatarstencil(stencilref, false);
    }
}

void GBuffer::renderlights(float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask, int stencilmask, int msaapass, bool transparent)
{
    Shader *s = drawtex == Draw_TexMinimap ? deferredminimapshader : (msaapass <= 0 ? deferredlightshader : (msaapass > 1 ? deferredmsaasampleshader : deferredmsaapixelshader));
    if(!s || s == nullshader)
    {
        return;
    }

    bool depth = true;
    if(!depthtestlights)
    {
        glDisable(GL_DEPTH_TEST);
        depth = false;
    }
    else
    {
        glDepthMask(GL_FALSE);
    }

    bindlighttexs(msaapass, transparent);
    setlightglobals(transparent);

    gle::defvertex(3);

    bool avatar = useavatarmask() && !transparent && !drawtex;
    int stencilref = -1;
    if(msaapass == 1 && ghasstencil)
    {
        int tx1 = std::max(static_cast<int>(std::floor((bsx1*0.5f+0.5f)*vieww)), 0),
            ty1 = std::max(static_cast<int>(std::floor((bsy1*0.5f+0.5f)*viewh)), 0),
            tx2 = std::min(static_cast<int>(std::ceil((bsx2*0.5f+0.5f)*vieww)), vieww),
            ty2 = std::min(static_cast<int>(std::ceil((bsy2*0.5f+0.5f)*viewh)), viewh);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        if(stencilmask)
        {
            glStencilFunc(GL_EQUAL, stencilmask|0x08, 0x07);
        }
        else
        {
            glStencilFunc(GL_ALWAYS, 0x08, ~0);
            glEnable(GL_STENCIL_TEST);
        }
        if(avatar)
        {
            glStencilMask(~0x40);
        }
        if(depthtestlights && depth)
        {
            glDisable(GL_DEPTH_TEST);
            depth = false;
        }
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        SETSHADER(msaaedgedetect);
        lightquad(-1, (tx1*2.0f)/vieww-1.0f, (ty1*2.0f)/viewh-1.0f, (tx2*2.0f)/vieww-1.0f, (ty2*2.0f)/viewh-1.0f, tilemask);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilFunc(GL_EQUAL, stencilref = stencilmask, (avatar ? 0x40 : 0) | (msaalight==2 ? 0x07 : 0x0F));
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        if(avatar)
        {
            glStencilMask(~0);
        }
        else if(msaalight==2 && !stencilmask)
        {
            glDisable(GL_STENCIL_TEST);
        }
    }
    else if(msaapass == 2)
    {
        if(ghasstencil)
        {
            glStencilFunc(GL_EQUAL, stencilref = stencilmask|0x08, avatar ? 0x4F : 0x0F);
        }
        if(msaalight==2)
        {
            glSampleMaski(0, 2); glEnable(GL_SAMPLE_MASK);
        }
    }
    else if(ghasstencil && (stencilmask || avatar))
    {
        if(!stencilmask)
        {
            glEnable(GL_STENCIL_TEST);
        }
        glStencilFunc(GL_EQUAL, stencilref = stencilmask, avatar ? 0x4F : 0x0F);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }

    if(!avatar)
    {
        stencilref = -1;
    }

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    if(hasDBT && depthtestlights > 1)
    {
        glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
    }

    bool sunpass = !lighttilebatch || drawtex == Draw_TexMinimap || (!sunlight.iszero() && csm.getcsmproperty(cascadedshadowmap::ShadowMap) && batchsunlight <= (gi && giscale && gidist ? 1 : 0));
    if(sunpass)
    {
        if(depthtestlights && depth)
        {
            glDisable(GL_DEPTH_TEST);
            depth = false;
        }
        rendersunpass(s, stencilref, transparent, bsx1, bsy1, bsx2, bsy2, tilemask);
    }

    if(depthtestlights && !depth)
    {
        glEnable(GL_DEPTH_TEST);
        depth = true;
    }

    if(!lighttilebatch || drawtex == Draw_TexMinimap)
    {
        renderlightsnobatch(s, stencilref, transparent, bsx1, bsy1, bsx2, bsy2);
    }
    else
    {
        renderlightbatches(*s, stencilref, transparent, bsx1, bsy1, bsx2, bsy2, tilemask);
    }

    if(msaapass == 1 && ghasstencil)
    {
        if(msaalight==2 && !stencilmask && !avatar)
        {
            glEnable(GL_STENCIL_TEST);
        }
    }
    else if(msaapass == 2)
    {
        if(ghasstencil && !stencilmask)
        {
            glDisable(GL_STENCIL_TEST);
        }
        if(msaalight==2)
        {
            glDisable(GL_SAMPLE_MASK);
        }
    }
    else if(avatar && !stencilmask)
    {
        glDisable(GL_STENCIL_TEST);
    }

    glDisable(GL_BLEND);

    if(!depthtestlights)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDepthMask(GL_TRUE);
        if(hasDBT && depthtestlights > 1)
        {
            glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
        }
    }
}

void GBuffer::rendervolumetric()
{
    if(!volumetric || !volumetriclights || !volscale)
    {
        return;
    }
    float bsx1 =  1,
          bsy1 =  1,
          bsx2 = -1,
          bsy2 = -1;
    for(uint i = 0; i < lightorder.size(); i++)
    {
        const lightinfo &l = lights[lightorder[i]];
        if(!l.volumetric() || l.checkquery())
        {
            continue;
        }

        l.addscissor(bsx1, bsy1, bsx2, bsy2);
    }
    if(bsx1 >= bsx2 || bsy1 >= bsy2)
    {
        return;
    }

    timer *voltimer = begintimer("volumetric lights");

    glBindFramebuffer(GL_FRAMEBUFFER, volfbo[0]);
    glViewport(0, 0, volw, volh);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE3);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    }
    glActiveTexture(GL_TEXTURE4);
    shadowatlas.bind();
    shadowatlas.setcomparemode();
    glActiveTexture(GL_TEXTURE0);
    vec2 sasize = shadowatlaspacker.dimensions();
    GLOBALPARAMF(shadowatlasscale, 1.0f/sasize.x, 1.0f/sasize.y);
    GLOBALPARAMF(volscale, static_cast<float>(vieww)/volw, static_cast<float>(viewh)/volh, static_cast<float>(volw)/vieww, static_cast<float>(volh)/viewh);
    GLOBALPARAMF(volminstep, volminstep);
    GLOBALPARAMF(volprefilter, volprefilter);
    GLOBALPARAMF(voldistclamp, farplane*voldistclamp);

    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_BLEND);

    if(!depthtestlights)
    {
        glDisable(GL_DEPTH_TEST);
    }
    else
    {
        glDepthMask(GL_FALSE);
    }

    lightsphere::enable();

    glEnable(GL_SCISSOR_TEST);

    bool outside = true;
    for(uint i = 0; i < lightorder.size(); i++)
    {
        const lightinfo &l = lights[lightorder[i]];
        if(!l.volumetric() || l.checkquery())
        {
            continue;
        }

        matrix4 lightmatrix = camprojmatrix;
        lightmatrix.translate(l.o);
        lightmatrix.scale(l.radius);
        GLOBALPARAM(lightmatrix, lightmatrix);

        if(l.spot > 0)
        {
            volumetricshader->setvariant(0, l.shadowmap >= 0 ? 2 : 1);
            LOCALPARAM(spotparams, vec4<float>(l.dir, 1/(1 - cos360(l.spot))));
        }
        else if(l.shadowmap >= 0)
        {
            volumetricshader->setvariant(0, 0);
        }
        else
        {
            volumetricshader->set();
        }

        LOCALPARAM(lightpos, vec4<float>(l.o, 1).div(l.radius));
        vec color = vec(l.color).mul(ldrscaleb()).mul(volcolor.tocolor().mul(volscale));
        LOCALPARAM(lightcolor, color);

        if(l.shadowmap >= 0)
        {
            shadowmapinfo &sm = shadowmaps[l.shadowmap];
            float smnearclip = SQRT3 / l.radius,
                  smfarclip = SQRT3,
                  bias = (smfilter > 2 ? smbias2 : smbias) * (smcullside ? 1 : -1) * smnearclip * (1024.0f / sm.size);
            int border = smfilter > 2 ? smborder2 : smborder;
            if(l.spot > 0)
            {
                LOCALPARAMF(shadowparams,
                    0.5f * sm.size * cotan360(l.spot),
                    (-smnearclip * smfarclip / (smfarclip - smnearclip) - 0.5f*bias),
                    1 / (1 + std::fabs(l.dir.z)),
                    0.5f + 0.5f * (smfarclip + smnearclip) / (smfarclip - smnearclip));
            }
            else
            {
                LOCALPARAMF(shadowparams,
                    0.5f * (sm.size - border),
                    -smnearclip * smfarclip / (smfarclip - smnearclip) - 0.5f*bias,
                    sm.size,
                    0.5f + 0.5f * (smfarclip + smnearclip) / (smfarclip - smnearclip));
            }
            LOCALPARAMF(shadowoffset, sm.x + 0.5f*sm.size, sm.y + 0.5f*sm.size);
        }

        int tx1 = static_cast<int>(std::floor((l.sx1*0.5f+0.5f)*volw)),
            ty1 = static_cast<int>(std::floor((l.sy1*0.5f+0.5f)*volh)),
            tx2 = static_cast<int>(std::ceil((l.sx2*0.5f+0.5f)*volw)),
            ty2 = static_cast<int>(std::ceil((l.sy2*0.5f+0.5f)*volh));
        glScissor(tx1, ty1, tx2-tx1, ty2-ty1);

        if(camera1->o.dist(l.o) <= l.radius + nearplane + 1 && depthfaillights)
        {
            if(outside)
            {
                outside = false;
                if(depthtestlights)
                {
                    glDisable(GL_DEPTH_TEST);
                }
                glCullFace(GL_FRONT);
            }
        }
        else if(!outside)
        {
            outside = true;
            if(depthtestlights)
            {
                glEnable(GL_DEPTH_TEST);
            }
            glCullFace(GL_BACK);
        }

        lightsphere::draw();
    }

    if(!outside)
    {
        outside = true;
        glCullFace(GL_BACK);
    }

    lightsphere::disable();

    if(depthtestlights)
    {
        glDepthMask(GL_TRUE);

        glDisable(GL_DEPTH_TEST);
    }

    int cx1 = static_cast<int>(std::floor((bsx1*0.5f+0.5f)*volw))&~1,
        cy1 = static_cast<int>(std::floor((bsy1*0.5f+0.5f)*volh))&~1,
        cx2 = (static_cast<int>(std::ceil((bsx2*0.5f+0.5f)*volw))&~1) + 2,
        cy2 = (static_cast<int>(std::ceil((bsy2*0.5f+0.5f)*volh))&~1) + 2;
    if(volbilateral || volblur)
    {
        int radius = (volbilateral ? volbilateral : volblur)*2;
        cx1 = std::max(cx1 - radius, 0);
        cy1 = std::max(cy1 - radius, 0);
        cx2 = std::min(cx2 + radius, volw);
        cy2 = std::min(cy2 + radius, volh);
        glScissor(cx1, cy1, cx2-cx1, cy2-cy1);

        glDisable(GL_BLEND);

        if(volbilateral)
        {
            for(int i = 0; i < 2; ++i)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, volfbo[(i+1)%2]);
                glViewport(0, 0, volw, volh);
                volumetricbilateralshader[i]->set();
                setbilateralparams(volbilateral, volbilateraldepth);
                glBindTexture(GL_TEXTURE_RECTANGLE, voltex[i%2]);
                screenquadoffset(0.25f, 0.25f, vieww, viewh);
            }
        }
        else
        {
            std::array<float, maxblurradius+1> blurweights,
                                               bluroffsets;
            setupblurkernel(volblur, blurweights.data(), bluroffsets.data());
            for(int i = 0; i < 2; ++i)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, volfbo[(i+1)%2]);
                glViewport(0, 0, volw, volh);
                setblurshader(i%2, 1, volblur, blurweights.data(), bluroffsets.data(), GL_TEXTURE_RECTANGLE);
                glBindTexture(GL_TEXTURE_RECTANGLE, voltex[i%2]);
                screenquad(volw, volh);
            }
        }

        glEnable(GL_BLEND);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, msaalight ? mshdrfbo : hdrfbo);
    glViewport(0, 0, vieww, viewh);

    int margin = (1<<volreduce) - 1;
    cx1 = std::max((cx1 * vieww) / volw - margin, 0);
    cy1 = std::max((cy1 * viewh) / volh - margin, 0);
    cx2 = std::min((cx2 * vieww + margin + volw - 1) / volw, vieww);
    cy2 = std::min((cy2 * viewh + margin + volh - 1) / volh, viewh);
    glScissor(cx1, cy1, cx2-cx1, cy2-cy1);

    bool avatar = useavatarmask();
    if(avatar)
    {
        glStencilFunc(GL_EQUAL, 0, 0x40);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glEnable(GL_STENCIL_TEST);
    }

    SETSHADER(scalelinear);
    glBindTexture(GL_TEXTURE_RECTANGLE, voltex[0]);
    screenquad(volw, volh);

    if(volbilateral || volblur)
    {
        std::swap(volfbo[0], volfbo[1]);
        std::swap(voltex[0], voltex[1]);
    }

    if(avatar)
    {
        glDisable(GL_STENCIL_TEST);
    }

    glDisable(GL_SCISSOR_TEST);

    glEnable(GL_DEPTH_TEST);

    glDisable(GL_BLEND);

    endtimer(voltimer);
}

VAR(oqvol, 0, 1, 1); //`o`cclusion `q`uery `vol`umetrics: toggles occlusion queries of volumetric lights
VAR(oqlights, 0, 1, 1); //`o`cclusion `q`uery `lights: toggles occlusion queries of lights behind geometry
VAR(debuglightscissor, 0, 0, 1); //displays the light scissor map in the corner of the screen

void viewlightscissor()
{
    std::vector<extentity *> &ents = entities::getents();
    gle::defvertex(2);
    for(uint i = 0; i < entgroup.size(); i++)
    {
        int idx = entgroup[i];
        if((static_cast<long>(ents.size()) > idx) && ents[idx]->type == EngineEnt_Light)
        {
            extentity &e = *ents[idx];
            for(uint j = 0; j < lights.size(); j++)
            {
                if(lights[j].o == e.o)
                {
                    lightinfo &l = lights[j];
                    if(!l.validscissor())
                    {
                        break;
                    }
                    gle::colorf(l.color.x/255, l.color.y/255, l.color.z/255);
                    float x1 = (l.sx1+1)/2*hudw(),
                          x2 = (l.sx2+1)/2*hudw(),
                          y1 = (1-l.sy1)/2*hudh(),
                          y2 = (1-l.sy2)/2*hudh();
                    gle::begin(GL_TRIANGLE_STRIP);
                    gle::attribf(x1, y1);
                    gle::attribf(x2, y1);
                    gle::attribf(x1, y2);
                    gle::attribf(x2, y2);
                    gle::end();
                }
            }
        }
    }
}

void collectlights()
{
    if(lights.size())
    {
        return;
    }

    // point lights processed here
    const std::vector<extentity *> &ents = entities::getents();
    if(!editmode || !fullbright)
    {
        for(uint i = 0; i < ents.size(); i++)
        {
            const extentity *e = ents[i];
            if(e->type != EngineEnt_Light || e->attr1 <= 0)
            {
                continue;
            }
            if(smviscull && view.isfoggedsphere(e->attr1, e->o))
            {
                continue;
            }
            lightinfo l = lightinfo(i, *e);
            lights.push_back(l);
            if(l.validscissor())
            {
                lightorder.emplace_back(lights.size()-1);
            }
        }
    }

    size_t numdynlights = 0;
    if(!drawtex)
    {
        updatedynlights();
        numdynlights = finddynlights();
    }
    for(size_t i = 0; i < numdynlights; ++i)
    {
        vec o, color, dir;
        float radius;
        int spot, flags;
        if(!getdynlight(i, o, radius, color, dir, spot, flags))
        {
            continue;
        }
        lightinfo &l = lights.emplace_back(lightinfo(o, vec(color).mul(255).max(0), radius, flags, dir, spot));
        if(l.validscissor())
        {
            lightorder.emplace_back(lights.size()-1);
        }
    }

    std::sort(lightorder.begin(), lightorder.end(), sortlights);

    bool queried = false;
    if(!drawtex && smquery && oqfrags && oqlights)
    {
        for(uint i = 0; i < lightorder.size(); i++)
        {
            int idx = lightorder[i];
            lightinfo &l = lights[idx];
            if((l.noshadow() && (!oqvol || !l.volumetric())) || l.radius >= rootworld.mapsize())
            {
                continue;
            }
            vec bbmin, bbmax;
            l.calcbb(bbmin, bbmax);
            if(!camera1->o.insidebb(bbmin, bbmax, 2))
            {
                l.query = occlusionengine.newquery(&l);
                if(l.query)
                {
                    if(!queried)
                    {
                        startbb(false);
                        queried = true;
                    }
                    l.query->startquery();
                    ivec bo(bbmin),
                         br = ivec(bbmax).sub(bo).add(1);
                    drawbb(bo, br);
                    occlusionengine.endquery();
                }
            }
        }
    }
    if(queried)
    {
        endbb(false);
        glFlush();
    }

    smused = 0;

    if(smcache && !smnoshadow && shadowatlas.cache.size())
    {
        for(int mismatched = 0; mismatched < 2; ++mismatched)
        {
            for(uint i = 0; i < lightorder.size(); i++)
            {
                int idx = lightorder[i];
                lightinfo &l = lights[idx];
                if(l.noshadow())
                {
                    continue;
                }
                auto itr = shadowatlas.cache.find(l);
                if(itr == shadowatlas.cache.end())
                {
                    continue;
                }
                float prec = smprec,
                      lod;
                int w, h;
                if(l.spot)
                {
                    w = 1;
                    h = 1;
                    prec *= tan360(l.spot);
                    lod = smspotprec;
                }
                else
                {
                    w = 3;
                    h = 2;
                    lod = smcubeprec;
                }
                lod *= std::clamp(l.radius * prec / sqrtf(std::max(1.0f, l.dist/l.radius)), static_cast<float>(smminsize), static_cast<float>(smmaxsize));
                const float sasizex = shadowatlaspacker.dimensions().x;
                int size = std::clamp(static_cast<int>(std::ceil((lod * sasizex) / shadowatlassize)), 1, static_cast<int>(sasizex) / w);
                w *= size;
                h *= size;
                const shadowcacheval &cached = (*itr).second;
                if(mismatched)
                {
                    if(cached.size == size)
                    {
                        continue;
                    }
                    ushort x = USHRT_MAX,
                           y = USHRT_MAX;
                    if(!shadowatlaspacker.insert(x, y, w, h))
                    {
                        continue;
                    }
                    addshadowmap(x, y, size, l.shadowmap, idx);
                }
                else
                {
                    if(cached.size != size)
                    {
                        continue;
                    }
                    ushort x = cached.x,
                           y = cached.y;
                    shadowatlaspacker.reserve(x, y, w, h);
                    addshadowmap(x, y, size, l.shadowmap, idx, &cached);
                }
                smused += w*h;
            }
        }
    }
}

VAR(csminoq, 0, 1, 1); //cascaded shadow maps in occlusion queries
VAR(sminoq, 0, 1, 1);  //shadow maps in occlusion queries
VAR(rhinoq, 0, 1, 1);  //radiance hints in occlusion queries

bool shouldworkinoq()
{
    return !drawtex && oqfrags && (!wireframe || !editmode);
}

struct batchrect : lightrect
{
    uchar group;
    ushort idx;

    batchrect() {}
    batchrect(const lightinfo &l, ushort idx)
      : lightrect(l),
        group((l.shadowmap < 0 ? BatchFlag_NoShadow : 0) | (l.spot > 0 ? BatchFlag_Spotlight : 0)),
        idx(idx)
    {}
};

struct batchstack : lightrect
{
    ushort offset, numrects;
    uchar flags;

    batchstack() {}
    batchstack(uchar x1, uchar y1, uchar x2, uchar y2, ushort offset, ushort numrects, uchar flags = 0) : lightrect(x1, y1, x2, y2), offset(offset), numrects(numrects), flags(flags) {}
};

static void batchlights(const batchstack &initstack, std::vector<batchrect> &batchrects, int &lightbatchstacksused, int &lightbatchrectsused)
{
    constexpr size_t stacksize = 32;
    std::stack<batchstack> stack;
    stack.push(initstack);

    while(stack.size() > 0)
    {
        const batchstack s = stack.top();
        stack.pop();
        if(stack.size() + 5 > stacksize)
        {
            batchlights(s, batchrects, lightbatchstacksused, lightbatchrectsused);
            continue;
        }
        ++lightbatchstacksused;
        int groups[BatchFlag_NoSun] = { 0 };
        lightrect split(s);
        ushort splitidx = USHRT_MAX;
        int outside = s.offset,
            inside  = s.offset + s.numrects;
        for(int i = outside; i < inside; ++i)
        {
            const batchrect &r = batchrects[i];
            if(r.outside(s))
            {
                if(i != outside)
                {
                    std::swap(batchrects[i], batchrects[outside]);
                }
                ++outside;
            }
            else if(s.inside(r))
            {
                ++groups[r.group];
                std::swap(batchrects[i--], batchrects[--inside]);
            }
            else if(r.idx < splitidx)
            {
                split = r;
                splitidx = r.idx;
            }
        }

        uchar flags = s.flags;
        int batched = s.offset + s.numrects;
        for(int g = 0; g < BatchFlag_NoShadow; ++g)
        {
            while(groups[g] >= lighttilebatch || (inside == outside && (groups[g] || !(flags & BatchFlag_NoSun))))
            {
                lightbatch *key = new lightbatch();
                key->flags = flags | g;
                flags |= BatchFlag_NoSun;

                int n = std::min(groups[g], lighttilebatch);
                groups[g] -= n;
                key->numlights = n;
                for(int i = 0; i < n; ++i)
                {
                    int best = -1;
                    ushort bestidx = USHRT_MAX;
                    for(int j = inside; j < batched; ++j)
                    {
                        const batchrect &r = batchrects[j];
                        {
                            if(r.group == g && r.idx < bestidx)
                            {
                                best = j;
                                bestidx = r.idx;
                            }
                        }
                    }
                    key->lights[i] = lightorder[bestidx];
                    std::swap(batchrects[best], batchrects[--batched]);
                }

                key->rects.push_back(s);
                lightbatches.push_back(key);
                ++lightbatchrectsused;
            }
        }
        if(splitidx != USHRT_MAX)
        {
            int numoverlap = batched - outside;
            split.intersect(s);

            if(split.y1 > s.y1)
            {
                stack.push(batchstack(s.x1, s.y1, s.x2, split.y1, outside, numoverlap, flags));
            }
            if(split.x1 > s.x1)
            {
                stack.push(batchstack(s.x1, split.y1, split.x1, split.y2, outside, numoverlap, flags));
            }
            stack.push(batchstack(split.x1, split.y1, split.x2, split.y2, outside, numoverlap, flags));
            if(split.x2 < s.x2)
            {
                stack.push(batchstack(split.x2, split.y1, s.x2, split.y2, outside, numoverlap, flags));
            }
            if(split.y2 < s.y2)
            {
                stack.push(batchstack(s.x1, split.y2, s.x2, s.y2, outside, numoverlap, flags));
            }
        }
    }
}

static bool sortlightbatches(const lightbatch *x, const lightbatch *y)
{
    if(x->flags < y->flags)
    {
        return true;
    }
    if(x->flags > y->flags)
    {
        return false;
    }
    return x->numlights > y->numlights;
}

static void batchlights(std::vector<batchrect> &batchrects, int &lightbatchstacksused, int &lightbatchrectsused, int &lightbatchesused)
{
    for(auto &i : lightbatches)
    {
        delete i;
    }
    lightbatches.clear();
    lightbatchstacksused = 0;
    lightbatchrectsused = 0;

    if(lighttilebatch && drawtex != Draw_TexMinimap)
    {
        batchlights(batchstack(0, 0, lighttilew, lighttileh, 0, batchrects.size()), batchrects, lightbatchstacksused, lightbatchrectsused);
        std::sort(lightbatches.begin(), lightbatches.end(), sortlightbatches);
    }

    lightbatchesused = lightbatches.size();
}

void GBuffer::packlights()
{
    lightsvisible = lightsoccluded = 0;
    lightpassesused = 0;
    std::vector<batchrect> batchrects;

    for(uint i = 0; i < lightorder.size(); i++)
    {
        int idx = lightorder[i];
        lightinfo &l = lights[idx];
        if(l.checkquery())
        {
            if(l.shadowmap >= 0)
            {
                shadowmaps[l.shadowmap].light = -1;
                l.shadowmap = -1;
            }
            lightsoccluded++;
            continue;
        }

        if(!l.noshadow() && !smnoshadow && l.shadowmap < 0)
        {
            float prec = smprec,
                  lod;
            int w, h;
            if(l.spot)
            {
                w = 1;
                h = 1;
                prec *= tan360(l.spot);
                lod = smspotprec;
            }
            else
            {
                w = 3;
                h = 2;
                lod = smcubeprec;
            }
            lod *= std::clamp(l.radius * prec / sqrtf(std::max(1.0f, l.dist/l.radius)), static_cast<float>(smminsize), static_cast<float>(smmaxsize));
            const float sasizex = shadowatlaspacker.dimensions().x;
            int size = std::clamp(static_cast<int>(std::ceil((lod * sasizex) / shadowatlassize)), 1, static_cast<int>(sasizex) / w);
            w *= size;
            h *= size;
            ushort x = USHRT_MAX,
                   y = USHRT_MAX;
            if(shadowatlaspacker.insert(x, y, w, h))
            {
                addshadowmap(x, y, size, l.shadowmap, idx);
                smused += w*h;
            }
            else if(smcache)
            {
                shadowatlas.full = true;
            }
        }
        batchrects.push_back(batchrect(l, i));
    }

    lightsvisible = lightorder.size() - lightsoccluded;

    batchlights(batchrects, lightbatchstacksused, lightbatchrectsused, lightbatchesused);
}

void GBuffer::rendercsmshadowmaps() const
{
    if(sunlight.iszero() || !csm.getcsmproperty(cascadedshadowmap::ShadowMap))
    {
        return;
    }
    if(inoq)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowatlas.fbo);
        glDepthMask(GL_TRUE);
    }
    csm.setup();
    shadowmapping = ShadowMap_Cascade;
    shadoworigin = vec(0, 0, 0);
    shadowdir = csm.lightview;
    shadowbias = csm.lightview.project_bb(worldmin, worldmax);
    shadowradius = std::fabs(csm.lightview.project_bb(worldmax, worldmin));

    float polyfactor = csm.getcsmproperty(cascadedshadowmap::PolyFactor),
          polyoffset = csm.getcsmproperty(cascadedshadowmap::PolyOffset);
    if(smfilter > 2)
    {
        csm.setcsmproperty(cascadedshadowmap::PolyFactor, csm.getcsmproperty(cascadedshadowmap::PolyFactor2));
        csm.setcsmproperty(cascadedshadowmap::PolyOffset, csm.getcsmproperty(cascadedshadowmap::PolyOffset2));
    }
    if(polyfactor || polyoffset)
    {
        glPolygonOffset(polyfactor, polyoffset);
        glEnable(GL_POLYGON_OFFSET_FILL);
    }
    glEnable(GL_SCISSOR_TEST);

    findshadowvas();
    findshadowmms();

    shadowmaskbatchedmodels(smdynshadow!=0);
    batchshadowmapmodels();

    for(int i = 0; i < csm.getcsmproperty(cascadedshadowmap::Splits); ++i)
    {
        if(csm.splits[i].idx >= 0)
        {
            const shadowmapinfo &sm = shadowmaps[csm.splits[i].idx];

            shadowmatrix.mul(csm.splits[i].proj, csm.model);
            GLOBALPARAM(shadowmatrix, shadowmatrix);

            glViewport(sm.x, sm.y, sm.size, sm.size);
            glScissor(sm.x, sm.y, sm.size, sm.size);
            glClear(GL_DEPTH_BUFFER_BIT);

            shadowside = i;

            rendershadowmapworld();
            rendershadowmodelbatches();
        }
    }

    clearbatchedmapmodels();

    glDisable(GL_SCISSOR_TEST);

    if(polyfactor || polyoffset)
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    shadowmapping = 0;

    if(inoq)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, msaasamples ? msfbo : gfbo);
        glViewport(0, 0, vieww, viewh);

        glFlush();
    }
}

int calcshadowinfo(const extentity &e, vec &origin, float &radius, vec &spotloc, int &spotangle, float &bias)
{
    if(e.attr5&LightEnt_NoShadow || e.attr1 <= smminradius)
    {
        return ShadowMap_None;
    }
    origin = e.o;
    radius = e.attr1;
    int type, w, border;
    float lod;
    if(e.attached && e.attached->type == EngineEnt_Spotlight)
    {
        type = ShadowMap_Spot;
        w = 1;
        border = 0;
        lod = smspotprec;
        spotloc = e.attached->o;
        spotangle = std::clamp(static_cast<int>(e.attached->attr1), 1, 89);
    }
    else
    {
        type = ShadowMap_CubeMap;
        w = 3;
        lod = smcubeprec;
        border = smfilter > 2 ? smborder2 : smborder;
        spotloc = e.o;
        spotangle = 0;
    }

    lod *= smminsize;
    const float sasizex = shadowatlaspacker.dimensions().x;
    int size = std::clamp(static_cast<int>(std::ceil((lod * sasizex) / shadowatlassize)), 1, static_cast<int>(sasizex) / w);
    bias = border / static_cast<float>(size - border);

    return type;
}

matrix4 shadowmatrix;

void GBuffer::rendershadowmaps(int offset) const
{
    if(!(sminoq && !debugshadowatlas && !inoq && shouldworkinoq()))
    {
        offset = 0;
    }

    for(; offset < static_cast<int>(shadowmaps.size()); offset++)
    {
        if(shadowmaps[offset].light >= 0)
        {
            break;
        }
    }

    if(offset >= static_cast<int>(shadowmaps.size()))
    {
        return;
    }

    if(inoq)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowatlas.fbo);
        glDepthMask(GL_TRUE);
    }

    float polyfactor = smpolyfactor,
          polyoffset = smpolyoffset;
    if(smfilter > 2)
    {
        polyfactor = smpolyfactor2;
        polyoffset = smpolyoffset2;
    }
    if(polyfactor || polyoffset)
    {
        glPolygonOffset(polyfactor, polyoffset);
        glEnable(GL_POLYGON_OFFSET_FILL);
    }

    glEnable(GL_SCISSOR_TEST);

    const std::vector<extentity *> &ents = entities::getents();
    for(uint i = offset; i < shadowmaps.size(); i++)
    {
        shadowmapinfo &sm = shadowmaps[i];
        if(sm.light < 0)
        {
            continue;
        }
        lightinfo &l = lights[sm.light];
        extentity *e = l.ent >= 0 ? ents[l.ent] : nullptr;
        int border, sidemask;
        if(l.spot)
        {
            shadowmapping = ShadowMap_Spot;
            border = 0;
            sidemask = 1;
        }
        else
        {
            shadowmapping = ShadowMap_CubeMap;
            border = smfilter > 2 ? smborder2 : smborder;
            sidemask = drawtex == Draw_TexMinimap ? 0x2F : (smsidecull ? view.cullfrustumsides(l.o, l.radius, sm.size, border) : 0x3F);
        }

        sm.sidemask = sidemask;

        shadoworigin = l.o;
        shadowradius = l.radius;
        shadowbias = border / static_cast<float>(sm.size - border);
        shadowdir = l.dir;
        shadowspot = l.spot;

        shadowmesh *mesh = e ? findshadowmesh(l.ent, *e) : nullptr;

        findshadowvas();
        findshadowmms();

        shadowmaskbatchedmodels(!(l.flags&LightEnt_Static) && smdynshadow);
        batchshadowmapmodels(mesh != nullptr);

        const shadowcacheval *cached = nullptr;
        int cachemask = 0;
        if(smcache)
        {
            int dynmask = smcache <= 1 ? batcheddynamicmodels() : 0;
            cached = sm.cached;
            if(cached)
            {
                if(!debugshadowatlas)
                {
                    cachemask = cached->sidemask & ~dynmask;
                }
                sm.sidemask |= cachemask;
            }
            sm.sidemask &= ~dynmask;

            sidemask &= ~cachemask;
            if(!sidemask)
            {
                clearbatchedmapmodels();
                continue;
            }
        }

        float smnearclip = SQRT3 / l.radius,
              smfarclip = SQRT3;
        matrix4 smprojmatrix(vec4<float>(static_cast<float>(sm.size - border) / sm.size, 0, 0, 0),
                              vec4<float>(0, static_cast<float>(sm.size - border) / sm.size, 0, 0),
                              vec4<float>(0, 0, -(smfarclip + smnearclip) / (smfarclip - smnearclip), -1),
                              vec4<float>(0, 0, -2*smnearclip*smfarclip / (smfarclip - smnearclip), 0));

        if(shadowmapping == ShadowMap_Spot)
        {
            glViewport(sm.x, sm.y, sm.size, sm.size);
            glScissor(sm.x, sm.y, sm.size, sm.size);
            glClear(GL_DEPTH_BUFFER_BIT);

            float invradius = 1.0f / l.radius,
                  spotscale = invradius * cotan360(l.spot);
            matrix4 spotmatrix(vec(l.spotx).mul(spotscale), vec(l.spoty).mul(spotscale), vec(l.dir).mul(-invradius));
            spotmatrix.translate(vec(l.o).neg());
            shadowmatrix.mul(smprojmatrix, spotmatrix);
            GLOBALPARAM(shadowmatrix, shadowmatrix);

            glCullFace((l.dir.z >= 0) == (smcullside != 0) ? GL_BACK : GL_FRONT);

            shadowside = 0;

            if(mesh)
            {
                rendershadowmesh(mesh);
            }
            else
            {
                rendershadowmapworld();
            }
            rendershadowmodelbatches();
        }
        else
        {
            if(!cachemask)
            {
                int cx1 = sidemask & 0x03 ? 0 : (sidemask & 0xC ? sm.size : 2 * sm.size),
                    cx2 = sidemask & 0x30 ? 3 * sm.size : (sidemask & 0xC ? 2 * sm.size : sm.size),
                    cy1 = sidemask & 0x15 ? 0 : sm.size,
                    cy2 = sidemask & 0x2A ? 2 * sm.size : sm.size;
                glScissor(sm.x + cx1, sm.y + cy1, cx2 - cx1, cy2 - cy1);
                glClear(GL_DEPTH_BUFFER_BIT);
            }
            for(int side = 0; side < 6; ++side)
            {
                if(sidemask&(1<<side))
                {
                    int sidex = (side>>1)*sm.size,
                        sidey = (side&1)*sm.size;
                    glViewport(sm.x + sidex, sm.y + sidey, sm.size, sm.size);
                    glScissor(sm.x + sidex, sm.y + sidey, sm.size, sm.size);
                    if(cachemask)
                    {
                        glClear(GL_DEPTH_BUFFER_BIT);
                    }
                    matrix4 cubematrix(cubeshadowviewmatrix[side]);
                    cubematrix.scale(1.0f/l.radius);
                    cubematrix.translate(vec(l.o).neg());
                    shadowmatrix.mul(smprojmatrix, cubematrix);
                    GLOBALPARAM(shadowmatrix, shadowmatrix);

                    glCullFace((side & 1) ^ (side >> 2) ^ smcullside ? GL_FRONT : GL_BACK);

                    shadowside = side;

                    if(mesh)
                    {
                        rendershadowmesh(mesh);
                    }
                    else
                    {
                        rendershadowmapworld();
                    }
                    rendershadowmodelbatches();
                }
            }
        }

        clearbatchedmapmodels();
    }

    glCullFace(GL_BACK);
    glDisable(GL_SCISSOR_TEST);

    if(polyfactor || polyoffset)
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    shadowmapping = 0;
    if(inoq)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, msaasamples ? msfbo : gfbo);
        glViewport(0, 0, vieww, viewh);

        glFlush();
    }
}

void GBuffer::rendershadowatlas()
{
    timer *smcputimer = begintimer("shadow map", false),
          *smtimer = begintimer("shadow map");

    glBindFramebuffer(GL_FRAMEBUFFER, shadowatlas.fbo);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    if(debugshadowatlas)
    {
        glClearDepth(0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glClearDepth(1);
    }

    // sun light
    if(!csminoq || debugshadowatlas || inoq || !shouldworkinoq())
    {
        rendercsmshadowmaps();
    }

    int smoffset = shadowmaps.size();

    packlights();

    // point lights
    rendershadowmaps(smoffset);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    endtimer(smtimer);
    endtimer(smcputimer);
}

void GBuffer::workinoq()
{
    collectlights();

    if(drawtex)
    {
        return;
    }

    if(shouldworkinoq())
    {
        inoq = true;

        if(csminoq && !debugshadowatlas)
        {
            rendercsmshadowmaps();
        }
        if(sminoq && !debugshadowatlas)
        {
            rendershadowmaps();
        }
        if(rhinoq)
        {
            renderradiancehints();
        }

        inoq = false;
    }
}

VAR(gdepthclear, 0, 1, 1); //toggles whether to clear the g depth buffer to 0000/1111 (black or white depending on gdepthformat) upon creation
VAR(gcolorclear, 0, 1, 1); //toggles whether to clear the g buffer to 0,0,0,0 (black) upon creation

void GBuffer::preparegbuffer(bool depthclear)
{
    glBindFramebuffer(GL_FRAMEBUFFER, msaasamples && (msaalight || !drawtex) ? msfbo : gfbo);
    glViewport(0, 0, vieww, viewh);

    if(drawtex && gdepthinit)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, vieww, viewh);
    }
    if(gdepthformat && gdepthclear)
    {
        maskgbuffer("d");
        if(gdepthformat == 1)
        {
            glClearColor(1, 1, 1, 1);
        }
        else
        {
            glClearColor(-farplane, 0, 0, 0);
        }
        glClear(GL_COLOR_BUFFER_BIT);
        maskgbuffer("cn");
    }
    else
    {
        maskgbuffer("cnd");
    }
    if(gcolorclear)
    {
        glClearColor(0, 0, 0, 0);
    }
    glClear((depthclear ? GL_DEPTH_BUFFER_BIT : 0)|(gcolorclear ? GL_COLOR_BUFFER_BIT : 0)|(depthclear && ghasstencil && (!msaasamples || msaalight || ghasstencil > 1) ? GL_STENCIL_BUFFER_BIT : 0));
    if(gdepthformat && gdepthclear)
    {
        maskgbuffer("cnd");
    }
    if(drawtex && gdepthinit)
    {
        glDisable(GL_SCISSOR_TEST);
    }
    gdepthinit = true;

    matrix4 invscreenmatrix,
            invcammatrix,
            invcamprojmatrix;
    invcammatrix.invert(cammatrix);
    invcamprojmatrix.invert(camprojmatrix);
    invscreenmatrix.identity();
    invscreenmatrix.settranslation(-1.0f, -1.0f, -1.0f);
    invscreenmatrix.setscale(2.0f/vieww, 2.0f/viewh, 2.0f);

    eyematrix.muld(projmatrix.inverse(), invscreenmatrix);
    if(drawtex == Draw_TexMinimap)
    {
        linearworldmatrix.muld(invcamprojmatrix, invscreenmatrix);
        if(!gdepthformat)
        {
            worldmatrix = linearworldmatrix;
        }
        linearworldmatrix.a.z = invcammatrix.a.z;
        linearworldmatrix.b.z = invcammatrix.b.z;
        linearworldmatrix.c.z = invcammatrix.c.z;
        linearworldmatrix.d.z = invcammatrix.d.z;
        if(gdepthformat)
        {
            worldmatrix = linearworldmatrix;
        }
        GLOBALPARAMF(radialfogscale, 0, 0, 0, 0);
    }
    else
    {
        float xscale  = eyematrix.a.x,
              yscale  = eyematrix.b.y,
              xoffset = eyematrix.d.x,
              yoffset = eyematrix.d.y,
              zscale  = eyematrix.d.z;
        matrix4 depthmatrix(vec(xscale/zscale, 0, xoffset/zscale), vec(0, yscale/zscale, yoffset/zscale));
        linearworldmatrix.muld(invcammatrix, depthmatrix);
        if(gdepthformat)
        {
            worldmatrix = linearworldmatrix;
        }
        else
        {
            worldmatrix.muld(invcamprojmatrix, invscreenmatrix);
        }

        GLOBALPARAMF(radialfogscale, xscale/zscale, yscale/zscale, xoffset/zscale, yoffset/zscale);
    }

    screenmatrix.identity();
    screenmatrix.settranslation(0.5f*vieww, 0.5f*viewh, 0.5f);
    screenmatrix.setscale(0.5f*vieww, 0.5f*viewh, 0.5f);
    screenmatrix.muld(camprojmatrix);

    GLOBALPARAMF(viewsize, vieww, viewh, 1.0f/vieww, 1.0f/viewh);
    GLOBALPARAMF(gdepthscale, eyematrix.d.z, eyematrix.c.w, eyematrix.d.w);
    GLOBALPARAMF(gdepthinvscale, eyematrix.d.z / eyematrix.c.w, eyematrix.d.w / eyematrix.c.w);
    GLOBALPARAMF(gdepthpackparams, -1.0f/farplane, -255.0f/farplane, -(255.0f*255.0f)/farplane);
    GLOBALPARAMF(gdepthunpackparams, -farplane, -farplane/255.0f, -farplane/(255.0f*255.0f));
    GLOBALPARAM(worldmatrix, worldmatrix);

    GLOBALPARAMF(ldrscale, ldrscale);
    GLOBALPARAMF(hdrgamma, hdrgamma, 1.0f/hdrgamma);
    GLOBALPARAM(camera, camera1->o);
    GLOBALPARAMF(millis, lastmillis/1000.0f);

    glerror();

    if(depthclear)
    {
        resetlights();
    }
    resetmodelbatches();
}


//allows passing nothing to internal uses of rendergbuffer
//(the parameter is for taking a game function to be rendered onscreen)
void GBuffer::dummyfxn()
{
    return;
}

/* rendergbuffer: creates the geometry buffer for the scene
 * args:
 *      bool depthclear: toggles clearing the depth buffer
 *      void (*gamefxn): pointer to a function for game-specific rendering
 * returns:
 *      void
 * other state changes:
 *      renders and copies a fbo (framebuffer object) to msfbo (multisample framebuffer object)
 *      or gfbo (geometry buffer framebuffer object) depending on whether msaa is enabled
 */
void GBuffer::rendergbuffer(bool depthclear, void (*gamefxn)())
{
    timer *gcputimer = drawtex ? nullptr : begintimer("g-buffer", false),
          *gtimer = drawtex ? nullptr : begintimer("g-buffer");

    preparegbuffer(depthclear);

    if(limitsky())
    {
        renderexplicitsky();
        glerror();
    }
    rendergeom();
    glerror();
    renderdecals();
    glerror();
    rendermapmodels();
    glerror();
    gamefxn();
    if(drawtex == Draw_TexMinimap)
    {
        if(depthclear)
        {
            findmaterials();
        }
        renderminimapmaterials();
        glerror();
    }
    else if(!drawtex)
    {
        rendermodelbatches();
        glerror();
        renderstains(StainBuffer_Opaque, true);
        renderstains(StainBuffer_Mapmodel, true);
        glerror();
    }

    endtimer(gtimer);
    endtimer(gcputimer);
}

void GBuffer::shademinimap(const vec &color)
{
    glerror();

    glBindFramebuffer(GL_FRAMEBUFFER, msaalight ? mshdrfbo : hdrfbo);
    glViewport(0, 0, vieww, viewh);

    if(color.x >= 0)
    {
        glClearColor(color.x, color.y, color.z, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    renderlights(-1, -1, 1, 1, nullptr, 0, msaalight ? -1 : 0);
    glerror();
}

void GBuffer::shademodelpreview(int x, int y, int w, int h, bool background, bool scissor)
{
    glerror();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw(), hudh());

    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mscolortex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gcolortex);
    }
    glActiveTexture(GL_TEXTURE1);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gnormaltex);
    }
    glActiveTexture(GL_TEXTURE3);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    }
    glActiveTexture(GL_TEXTURE0);

    float lightscale = 2.0f*ldrscale;
    GLOBALPARAMF(lightscale, 0.1f*lightscale, 0.1f*lightscale, 0.1f*lightscale, lightscale);
    GLOBALPARAM(sunlightdir, vec(0, -1, 2).normalize());
    GLOBALPARAMF(sunlightcolor, 0.6f*lightscale, 0.6f*lightscale, 0.6f*lightscale);

    SETSHADER(modelpreview);

    LOCALPARAMF(cutout, background ? -1 : 0);

    if(scissor)
    {
        glEnable(GL_SCISSOR_TEST);
    }

    int sx = std::clamp(x, 0, hudw()),
        sy = std::clamp(y, 0, hudh()),
        sw = std::clamp(x + w, 0, hudw()) - sx,
        sh = std::clamp(y + h, 0, hudh()) - sy;
    float sxk = 2.0f/hudw(),
          syk = 2.0f/hudh(),
          txk = vieww/static_cast<float>(w),
          tyk = viewh/static_cast<float>(h);
    hudquad(sx*sxk - 1, sy*syk - 1, sw*sxk, sh*syk, (sx-x)*txk, (sy-y)*tyk, sw*txk, sh*tyk);

    if(scissor)
    {
        glDisable(GL_SCISSOR_TEST);
    }

    glerror();
}

void GBuffer::shadesky() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, msaalight ? mshdrfbo : hdrfbo);
    glViewport(0, 0, vieww, viewh);

    drawskybox((hdrclear > 0 ? hdrclear-- : msaalight) > 0);
}

bool GBuffer::istransparentlayer() const
{
    return transparentlayer;
}

void shadegbuffer()
{
    if(msaasamples && !msaalight && !drawtex)
    {
        gbuf.resolvemsaadepth(vieww, viewh);
    }
    glerror();

    timer *shcputimer = begintimer("deferred shading", false),
          *shtimer = begintimer("deferred shading");

    gbuf.shadesky();

    if(msaasamples && (msaalight || !drawtex))
    {
        if((ghasstencil && msaaedgedetect) || msaalight==2)
        {
            for(int i = 0; i < 2; ++i)
            {
                gbuf.renderlights(-1, -1, 1, 1, nullptr, 0, i+1);
            }
        }
        else
        {
            gbuf.renderlights(-1, -1, 1, 1, nullptr, 0, drawtex ? -1 : 3);
        }
    }
    else
    {
        gbuf.renderlights();
    }
    glerror();

    if(!drawtex)
    {
        renderstains(StainBuffer_Opaque, false);
        renderstains(StainBuffer_Mapmodel, false);
    }

    endtimer(shtimer);
    endtimer(shcputimer);
}

void setuplights(GBuffer &buf)
{
    glerror();
    buf.setupgbuffer();
    if(bloomw < 0 || bloomh < 0)
    {
        setupbloom(gw, gh);
    }
    if(ao && (aow < 0 || aoh < 0))
    {
        setupao(gw, gh);
    }
    if(volumetriclights && volumetric && (volw < 0 || volh < 0))
    {
        setupvolumetric(gw, gh);
    }
    if(!shadowatlas.fbo)
    {
        shadowatlas.setup();
    }
    if(useradiancehints() && !rhfbo)
    {
        setupradiancehints();
    }
    if(!deferredlightshader)
    {
        loaddeferredlightshaders();
    }
    if(drawtex == Draw_TexMinimap && !deferredminimapshader)
    {
        deferredminimapshader = loaddeferredlightshader(msaalight ? "mM" : "m");
    }
    setupaa(buf, gw, gh);
    glerror();
}

bool debuglights()
{
    viewao(); //this fxn checks for the appropriate debug var
    if(debugshadowatlas)
    {
        shadowatlas.view();
    }
    else if(debugdepth)
    {
        gbuf.viewdepth();
    }
    else if(debugstencil)
    {
        viewstencil();
    }
    else if(debugrefract)
    {
        gbuf.viewrefract();
    }
    else if(debuglightscissor)
    {
        viewlightscissor();
    }
    else if(debugrsm)
    {
        viewrsm();
    }
    else if(debugrh)
    {
        viewrh();
    }
    else if(!debugaa())
    {
        return false;
    }
    return true;
}

void cleanuplights()
{
    gbuf.cleanupgbuffer();
    cleanupbloom();
    cleanupao();
    cleanupvolumetric();
    shadowatlas.cleanup();
    cleanupradiancehints();
    lightsphere::cleanup();
    cleanupaa();
}

int GBuffer::getlightdebuginfo(uint type) const
{
    switch(type)
    {
        case 0:
        {
            return lightpassesused;
        }
        case 1:
        {
            return lightsvisible;
        }
        case 2:
        {
            return lightsoccluded;
        }
        case 3:
        {
            return lightbatchesused;
        }
        case 4:
        {
            return lightbatchrectsused;
        }
        case 5:
        {
            return lightbatchstacksused;
        }
        default:
        {
            return -1;
        }
    }
}

void initrenderlightscmds()
{
    addcommand("usepacknorm", reinterpret_cast<identfun>(+[](){intret(usepacknorm() ? 1 : 0);}), "", Id_Command);
    addcommand("lightdebuginfo", reinterpret_cast<identfun>(+[] (int * index) {intret(gbuf.getlightdebuginfo(static_cast<uint>(*index)));} ), "i", Id_Command);
    addcommand("getcsmproperty", reinterpret_cast<identfun>(+[] (int * index) {floatret(csm.getcsmproperty(*index));} ), "i", Id_Command);
    addcommand("setcsmproperty", reinterpret_cast<identfun>(+[] (int * index, float * value) {intret(csm.setcsmproperty(*index, *value));} ), "if", Id_Command);
}

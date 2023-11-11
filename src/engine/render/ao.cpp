/* ao.cpp: screenspace ambient occlusion
 *
 * Screenspace ambient occlusion is a way to simulate darkening of corners which
 * do not recieve as much diffuse light as other areas. SSAO relies on the depth
 * buffer of the scene to determine areas which appear to be creases and
 * darkens those areas. Various settings allow for more or less artifact-free
 * rendition of this darkening effect.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "ao.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendertimers.h"
#include "renderwindow.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/control.h"

int aow  = -1,
    aoh  = -1;
GLuint aofbo[4] = { 0, 0, 0, 0 },
       aotex[4] = { 0, 0, 0, 0 },
       aonoisetex = 0;

VARFP(ao, 0, 1, 1, { cleanupao(); cleardeferredlightshaders(); }); //toggles ao use in general
FVARR(aoradius, 0, 5, 256);
FVAR(aocutoff, 0, 2.0f, 1e3f);
FVARR(aodark, 1e-3f, 11.0f, 1e3f);
FVARR(aosharp, 1e-3f, 1, 1e3f);
FVAR(aoprefilterdepth, 0, 1, 1e3f);
FVARR(aomin, 0, 0.25f, 1);
VARFR(aosun, 0, 1, 1, cleardeferredlightshaders()); //toggles ambient occlusion for sunlight
FVARR(aosunmin, 0, 0.5f, 1);
VARP(aoblur, 0, 4, 7);
VARP(aoiter, 0, 0, 4); //number of times to run ao shader (higher is smoother)
VARFP(aoreduce, 0, 1, 2, cleanupao());
VARF(aoreducedepth, 0, 1, 2, cleanupao());
VARFP(aofloatdepth, 0, 1, 2, initwarning("AO setup", Init_Load, Change_Shaders));
VARFP(aoprec, 0, 1, 1, cleanupao()); //toggles between r8 and rgba8 buffer format
VAR(aodepthformat, 1, 0, 0);
VARF(aonoise, 0, 5, 8, cleanupao()); //power or two scale factor for ao noise effect
VARFP(aobilateral, 0, 3, 10, cleanupao());
FVARP(aobilateraldepth, 0, 4, 1e3f);
VARFP(aobilateralupscale, 0, 0, 1, cleanupao());
VARF(aopackdepth, 0, 1, 1, cleanupao());
VARFP(aotaps, 1, 12, 12, cleanupao());
VAR(debugao, 0, 0, 4);

static Shader *ambientobscuranceshader = nullptr;

/* loadambientobscuranceshader
 *
 * creates a new ambient obscurance (ambient occlusion) object with values based
 * on current settings
 */
Shader *loadambientobscuranceshader()
{
    string opts;
    int optslen = 0;

    bool linear = aoreducedepth && (aoreduce || aoreducedepth > 1);
    if(linear)
    {
        opts[optslen++] = 'l';
    }
    if(aobilateral && aopackdepth)
    {
        opts[optslen++] = 'p';
    }
    opts[optslen] = '\0';

    DEF_FORMAT_STRING(name, "ambientobscurance%s%d", opts, aotaps);
    return generateshader(name, "ambientobscuranceshader \"%s\" %d", opts, aotaps);
}

//sets the ambientobscuranceshader gvar to the value created by above fxn
void loadaoshaders()
{
    ambientobscuranceshader = loadambientobscuranceshader();
}

//un-sets the ambientobscuranceshader gvar defined by loadaoshaders
void clearaoshaders()
{
    ambientobscuranceshader = nullptr;
}

void setupao(int w, int h)
{
    int sw = w>>aoreduce,
        sh = h>>aoreduce;

    if(sw == aow && sh == aoh)
    {
        return;
    }
    aow = sw;
    aoh = sh;
    if(!aonoisetex)
    {
        glGenTextures(1, &aonoisetex);
    }
    bvec *noise = new bvec[(1<<aonoise)*(1<<aonoise)];
    for(int k = 0; k < (1<<aonoise)*(1<<aonoise); ++k)
    {
        noise[k] = bvec(vec(randomfloat(2)-1, randomfloat(2)-1, 0).normalize());
    }
    createtexture(aonoisetex, 1<<aonoise, 1<<aonoise, noise, 0, 0, GL_RGB, GL_TEXTURE_2D);
    delete[] noise;

    bool upscale = aoreduce && aobilateral && aobilateralupscale;
    GLenum format = aoprec ? GL_R8 : GL_RGBA8,
           packformat = aobilateral && aopackdepth ? (aodepthformat ? GL_RG16F : GL_RGBA8) : format;
    int packfilter = upscale && aopackdepth && !aodepthformat ? 0 : 1;
    for(int i = 0; i < (upscale ? 3 : 2); ++i)
    {
        //create framebuffer
        if(!aotex[i])
        {
            glGenTextures(1, &aotex[i]);
        }
        if(!aofbo[i])
        {
            glGenFramebuffers(1, &aofbo[i]);
        }
        createtexture(aotex[i], upscale && i ? w : aow, upscale && i >= 2 ? h : aoh, nullptr, 3, i < 2 ? packfilter : 1, i < 2 ? packformat : format, GL_TEXTURE_RECTANGLE);
        glBindFramebuffer(GL_FRAMEBUFFER, aofbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, aotex[i], 0);
        //make sure we have a framebuffer
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fatal("failed allocating AO buffer!");
        }
        if(!upscale && packformat == GL_RG16F)
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    if(aoreducedepth && (aoreduce || aoreducedepth > 1))
    {
        //create framebuffer
        if(!aotex[3])
        {
            glGenTextures(1, &aotex[3]);
        }
        if(!aofbo[3])
        {
            glGenFramebuffers(1, &aofbo[3]);
        }
        createtexture(aotex[3], aow, aoh, nullptr, 3, 0, aodepthformat > 1 ? GL_R32F : (aodepthformat ? GL_R16F : GL_RGBA8), GL_TEXTURE_RECTANGLE);
        glBindFramebuffer(GL_FRAMEBUFFER, aofbo[3]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, aotex[3], 0);
        //make sure we have a framebuffer
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fatal("failed allocating AO buffer!");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    loadaoshaders();
    loadbilateralshaders();
}

/* cleanupao
 *
 * deletes the framebuffer textures for ambient obscurance (ambient occlusion)
 *
 * aofbo[0-3] and aotex[0-3] as well as aonoisetex if enabled
 * sets ao buffer width and height to -1 to indicate buffers not there
 * cleans up ao shaders
 */
void cleanupao()
{
    for(int i = 0; i < 4; ++i)
    {
        if(aofbo[i])
        {
            glDeleteFramebuffers(1, &aofbo[i]);
            aofbo[i] = 0;
        }
    }
    for(int i = 0; i < 4; ++i)
    {
        if(aotex[i])
        {
            glDeleteTextures(1, &aotex[i]);
            aotex[i] = 0;
        }
    }
    if(aonoisetex)
    {
        glDeleteTextures(1, &aonoisetex);
        aonoisetex = 0;
    }
    aow = aoh = -1;

    clearaoshaders();
    clearbilateralshaders();
}

/* initao
 *
 * sets the ao buffer depth format flag depending on the aofloatdepth variable
 */
void initao()
{
    aodepthformat = aofloatdepth ? aofloatdepth : 0;
}

/* viewao
 *
 * displays the raw output of the ao buffer, useful for debugging
 *
 * either fullscreen (if debugfullscreen is 1) or corner of screen
 */
void viewao()
{
    if(!ao || !debugao)
    {
        return;
    }
    int w = debugfullscreen ? hudw() : std::min(hudw(), hudh())/2, //if debugfullscreen, set to hudw/hudh size; if not, do small size
        h = debugfullscreen ? hudh() : (w*hudh())/hudw();
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, aotex[debugao - 1]);
    int tw = aotex[2] ? gw : aow,
        th = aotex[2] ? gh : aoh;
    debugquad(0, 0, w, h, 0, 0, tw, th);
}

void GBuffer::renderao() const
{
    if(!ao)
    {
        return;
    }
    timer *aotimer = begintimer("ambient obscurance");

    if(msaasamples)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    }
    bool linear = aoreducedepth && (aoreduce || aoreducedepth > 1);
    float xscale = eyematrix.a.x,
          yscale = eyematrix.b.y;
    if(linear)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, aofbo[3]);
        glViewport(0, 0, aow, aoh);
        SETSHADER(linearizedepth);
        screenquad(vieww, viewh);

        xscale *= static_cast<float>(vieww)/aow;
        yscale *= static_cast<float>(viewh)/aoh;

        glBindTexture(GL_TEXTURE_RECTANGLE, aotex[3]);
    }

    ambientobscuranceshader->set();

    glBindFramebuffer(GL_FRAMEBUFFER, aofbo[0]);
    glViewport(0, 0, aow, aoh);
    glActiveTexture(GL_TEXTURE1);

    if(msaasamples)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gnormaltex);
    }

    LOCALPARAM(normalmatrix, matrix3(cammatrix));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, aonoisetex);
    glActiveTexture(GL_TEXTURE0);

    LOCALPARAMF(tapparams, aoradius*eyematrix.d.z/xscale, aoradius*eyematrix.d.z/yscale, aoradius*aoradius*aocutoff*aocutoff);
    LOCALPARAMF(contrastparams, (2.0f*aodark)/aotaps, aosharp);
    LOCALPARAMF(offsetscale, xscale/eyematrix.d.z, yscale/eyematrix.d.z, eyematrix.d.x/eyematrix.d.z, eyematrix.d.y/eyematrix.d.z);
    LOCALPARAMF(prefilterdepth, aoprefilterdepth);
    screenquad(vieww, viewh, aow/static_cast<float>(1<<aonoise), aoh/static_cast<float>(1<<aonoise));

    if(aobilateral)
    {
        if(aoreduce && aobilateralupscale)
        {
            for(int i = 0; i < 2; ++i)
            {
                setbilateralshader(aobilateral, i, aobilateraldepth);
                glBindFramebuffer(GL_FRAMEBUFFER, aofbo[i+1]);
                glViewport(0, 0, vieww, i ? viewh : aoh);
                glBindTexture(GL_TEXTURE_RECTANGLE, aotex[i]);
                glActiveTexture(GL_TEXTURE1);
                if(msaasamples)
                {
                    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
                }
                else
                {
                    glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
                }
                glActiveTexture(GL_TEXTURE0);
                screenquad(vieww, viewh, i ? vieww : aow, aoh);
            }
        }
        else
        {
            for(int i = 0; i < 2 + 2*aoiter; ++i)
            {
                setbilateralshader(aobilateral, i%2, aobilateraldepth);
                glBindFramebuffer(GL_FRAMEBUFFER, aofbo[(i+1)%2]);
                glViewport(0, 0, aow, aoh);
                glBindTexture(GL_TEXTURE_RECTANGLE, aotex[i%2]);
                glActiveTexture(GL_TEXTURE1);
                if(linear)
                {
                    glBindTexture(GL_TEXTURE_RECTANGLE, aotex[3]);
                }
                else if(msaasamples)
                {
                    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
                }
                else
                {
                    glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
                }
                glActiveTexture(GL_TEXTURE0);
                screenquad(vieww, viewh);
            }
        }
    }
    else if(aoblur)
    {
        std::array<float, maxblurradius+1> blurweights,
                                           bluroffsets;
        setupblurkernel(aoblur, blurweights.data(), bluroffsets.data());
        for(int i = 0; i < 2+2*aoiter; ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, aofbo[(i+1)%2]);
            glViewport(0, 0, aow, aoh);
            setblurshader(i%2, 1, aoblur, blurweights.data(), bluroffsets.data(), GL_TEXTURE_RECTANGLE);
            glBindTexture(GL_TEXTURE_RECTANGLE, aotex[i%2]);
            screenquad(aow, aoh);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, msaasamples ? msfbo : gfbo);
    glViewport(0, 0, vieww, viewh);

    endtimer(aotimer);
}

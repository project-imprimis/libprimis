/* hdr.cpp: hdr, tonemapping, and bloom
 *
 * Libprimis supports a type of high dynamic range by calculating the average
 * brightness of the scene and slowly changing the calibration point for the
 * viewed image, much as human eyes do. This results in colors being washed out
 * bright, e.g., when stepping into the sunlight, and time being required before
 * a dark area can be seen after being "used to" bright light.
 *
 * Additionally, bloom, or bleed from bright light sources, is calculated in the
 * process of sampling the screen for HDR purposes. This allows very bright
 * objects to appear more overbright (and realistic) than the light entities can
 * project on their own.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "aa.h"
#include "hdr.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendertimers.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/control.h"


GLenum hdrformat = 0;

int bloomw = -1,
    bloomh = -1;

//gl buffers needed for bloom effect
GLuint hdrfbo = 0,
       hdrtex = 0;

namespace
{
    GLuint bloompbo = 0,
           bloomfbo[6] = { 0, 0, 0, 0, 0, 0 },
           bloomtex[6] = { 0, 0, 0, 0, 0, 0 };

    GLenum bloomformat = 0;
    int lasthdraccum = 0;

    FVAR(bloomthreshold, 1e-3f, 0.8f, 1e3f);
    FVARP(bloomscale, 0, 1.0f, 1e3f);           //scale factor for bloom effect
    VARP(bloomblur, 0, 7, 7);                   //blur factor for bloom effect
    VARP(bloomiter, 0, 0, 4);                   //number of interations for bloom generation
    VARFP(bloomsize, 6, 9, 11, cleanupbloom()); //size of HDR buffer: 6 -> 2^6 = 64x64 ... 11 -> 2^11 = 2048x2048
    VARFP(bloomprec, 0, 2, 3, cleanupbloom());  //HDR buffer bit depth: 3: RGB16 2: R11G11B10 1:RGB10 0: RGB8

    FVAR(hdraccumscale, 0, 0.98f, 1);           //for hdr, exponent base for time decay of accumulation buffer (always <= 1 so decaying with time)
    VAR(hdraccummillis, 1, 33, 1000);           //number of ms between samplings for the hdr buffer
    VAR(hdrreduce, 0, 2, 2);                    //number of powers to reduce hdr buffer size relative to g buffer (2 = 1/4, 1 = 1/2, 0 = g-buffer size)
    FVARR(hdrbright, 1e-4f, 1.0f, 1e4f);        //brightness factor for high dynamic range rendering
    FVAR(hdrsaturate, 1e-3f, 0.8f, 1e3f);
}

int gethdrformat(int prec, int fallback)
{
    if(prec >= 3)
    {
        return GL_RGB16F;
    }
    if(prec >= 2)
    {
        return GL_R11F_G11F_B10F;
    }
    if(prec >= 1)
    {
        return GL_RGB10;
    }
    return fallback;
}

void setupbloom(int w, int h)
{
    int maxsize = ((1<<bloomsize)*5)/4;
    while(w >= maxsize || h >= maxsize)
    {
        w /= 2;
        h /= 2;
    }
    w = std::max(w, 1);
    h = std::max(h, 1);
    if(w == bloomw && h == bloomh)
    {
        return;
    }
    bloomw = w;
    bloomh = h;

    for(int i = 0; i < 5; ++i)
    {
        if(!bloomtex[i])
        {
            glGenTextures(1, &bloomtex[i]);
        }
    }

    for(int i = 0; i < 5; ++i)
    {
        if(!bloomfbo[i])
        {
            glGenFramebuffers(1, &bloomfbo[i]);
        }
    }

    bloomformat = gethdrformat(bloomprec);
    createtexture(bloomtex[0], std::max(gw/2, bloomw), std::max(gh/2, bloomh), nullptr, 3, 1, bloomformat, GL_TEXTURE_RECTANGLE);
    createtexture(bloomtex[1], std::max(gw/4, bloomw), std::max(gh/4, bloomh), nullptr, 3, 1, bloomformat, GL_TEXTURE_RECTANGLE);
    createtexture(bloomtex[2], bloomw, bloomh, nullptr, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE);
    createtexture(bloomtex[3], bloomw, bloomh, nullptr, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE);
    if(bloomformat != GL_RGB)
    {
        if(!bloomtex[5])
        {
            glGenTextures(1, &bloomtex[5]);
        }
        if(!bloomfbo[5])
        {
            glGenFramebuffers(1, &bloomfbo[5]);
        }
        createtexture(bloomtex[5], bloomw, bloomh, nullptr, 3, 1, bloomformat, GL_TEXTURE_RECTANGLE);
    }
    static const float grayf[12] = { 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f };
    createtexture(bloomtex[4], bloompbo ? 4 : 1, 1, reinterpret_cast<const void *>(grayf), 3, 1, GL_R16F);
    for(int i = 0; i < (5 + (bloomformat != GL_RGB ? 1 : 0)); ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomfbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, i==4 ? GL_TEXTURE_2D : GL_TEXTURE_RECTANGLE, bloomtex[i], 0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fatal("failed allocating bloom buffer!");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void cleanupbloom()
{
    if(bloompbo)
    {
        glDeleteBuffers(1, &bloompbo);
        bloompbo = 0;
    }
    for(int i = 0; i < 6; ++i)
    {
        if(bloomfbo[i])
        {
            glDeleteFramebuffers(1, &bloomfbo[i]);
            bloomfbo[i] = 0;
        }
    }
    for(int i = 0; i < 6; ++i)
    {
        if(bloomtex[i])
        {
            glDeleteTextures(1, &bloomtex[i]);
            bloomtex[i] = 0;
        }
    }
    bloomw = bloomh = -1;
    lasthdraccum = 0;
}

FVARFP(hdrgamma, 1e-3f, 2, 1e3f, initwarning("HDR setup", Init_Load, Change_Shaders));
VARFP(hdrprec, 0, 2, 3, gbuf.cleanupgbuffer()); //precision of hdr buffer

void copyhdr(int sw, int sh, GLuint fbo, int dw, int dh, bool flipx, bool flipy, bool swapxy)
{
    if(!dw)
    {
        dw = sw;
    }
    if(!dh)
    {
        dh = sh;
    }

    if(msaalight)
    {
        gbuf.resolvemsaacolor(sw, sh);
    }
    glerror();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, dw, dh);

    SETSHADER(reorient);
    vec reorientx(flipx ? -0.5f : 0.5f, 0, 0.5f),
        reorienty(0, flipy ? -0.5f : 0.5f, 0.5f);
    if(swapxy)
    {
        std::swap(reorientx, reorienty);
    }
    reorientx.mul(sw);
    reorienty.mul(sh);
    LOCALPARAM(reorientx, reorientx);
    LOCALPARAM(reorienty, reorienty);

    glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
    screenquad();
    glerror();

    hdrclear = 3;
}

void loadhdrshaders(int aa)
{
    switch(aa)
    {
        case AA_Luma:
        {
            useshaderbyname("hdrtonemapluma");
            useshaderbyname("hdrnopluma");
            if(msaalight > 1 && msaatonemap)
            {
                useshaderbyname("msaatonemapluma");
            }
            break;
        }
        case AA_Masked:
        {
            if(!msaasamples && ghasstencil)
            {
                useshaderbyname("hdrtonemapstencil");
            }
            else
            {
                useshaderbyname("hdrtonemapmasked");
                useshaderbyname("hdrnopmasked");
                if(msaalight > 1 && msaatonemap)
                {
                    useshaderbyname("msaatonemapmasked");
                }
            }
            break;
        }
        case AA_Split:
        {
            useshaderbyname("msaatonemapsplit");
            break;
        }
        case AA_SplitLuma:
        {
            useshaderbyname("msaatonemapsplitluma");
            break;
        }
        case AA_SplitMasked:
        {
            useshaderbyname("msaatonemapsplitmasked");
            break;
        }
        default:
        {
            break;
        }
    }
}

void GBuffer::processhdr(GLuint outfbo, int aa)
{
    timer *hdrtimer = begintimer("hdr processing");

    GLOBALPARAMF(hdrparams, hdrbright, hdrsaturate, bloomthreshold, bloomscale);

    GLuint b0fbo = bloomfbo[1],
           b0tex = bloomtex[1],
           b1fbo =  bloomfbo[0],
           b1tex = bloomtex[0],
           ptex = hdrtex;
    int b0w = std::max(vieww/4, bloomw),
        b0h = std::max(viewh/4, bloomh),
        b1w = std::max(vieww/2, bloomw),
        b1h = std::max(viewh/2, bloomh),
        pw = vieww,
        ph = viewh;
    if(msaalight)
    {
        if(aa < AA_Split && (msaalight <= 1 || !msaatonemap)) //only bind relevant framebuffers
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, mshdrfbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrfbo);
            glBlitFramebuffer(0, 0, vieww, viewh, 0, 0, vieww, viewh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        else if(hasFBMSBS && (vieww > bloomw || viewh > bloomh))
        {
            int cw = std::max(vieww/2, bloomw),
                ch = std::max(viewh/2, bloomh);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, mshdrfbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrfbo);
            glBlitFramebuffer(0, 0, vieww, viewh, 0, 0, cw, ch, GL_COLOR_BUFFER_BIT, GL_SCALED_RESOLVE_FASTEST_EXT);
            pw = cw;
            ph = ch;
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);
            if(vieww/2 >= bloomw)
            {
                pw = vieww/2;
                if(viewh/2 >= bloomh)
                {
                    ph = viewh/2;
                    glViewport(0, 0, pw, ph);
                    SETSHADER(msaareduce);
                }
                else
                {
                    glViewport(0, 0, pw, viewh);
                    SETSHADER(msaareducew);
                }
            }
            else
            {
                glViewport(0, 0, vieww, viewh);
                SETSHADER(msaaresolve);
            }
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
            screenquad(vieww, viewh);
        }
    }
    if(hdrreduce)
    {
        while(pw > bloomw || ph > bloomh)
        {
            GLuint cfbo = b1fbo,
                   ctex = b1tex;
            int cw = std::max(pw/2, bloomw),
                ch = std::max(ph/2, bloomh);

            if(hdrreduce > 1 && cw/2 >= bloomw)
            {
                cw /= 2;
                if(ch/2 >= bloomh)
                {
                    ch /= 2;
                    SETSHADER(hdrreduce2);
                }
                else SETSHADER(hdrreduce2w);
            }
            else
            {
                SETSHADER(hdrreduce);
            }
            if(cw == bloomw && ch == bloomh)
            {
                if(bloomfbo[5])
                {
                    cfbo = bloomfbo[5];
                    ctex = bloomtex[5];
                }
                else
                {
                    cfbo = bloomfbo[2];
                    ctex = bloomtex[2];
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, cfbo);
            glViewport(0, 0, cw, ch);
            glBindTexture(GL_TEXTURE_RECTANGLE, ptex);
            screenquad(pw, ph);

            ptex = ctex;
            pw = cw;
            ph = ch;
            std::swap(b0fbo, b1fbo);
            std::swap(b0tex, b1tex);
            std::swap(b0w, b1w);
            std::swap(b0h, b1h);
        }
    }
    if(!lasthdraccum || lastmillis - lasthdraccum >= hdraccummillis)
    {
        GLuint ltex = ptex;
        int lw = pw,
            lh = ph;
        for(int i = 0; lw > 2 || lh > 2; i++)
        {
            int cw = std::max(lw/2, 2),
                ch = std::max(lh/2, 2);

            if(hdrreduce > 1 && cw/2 >= 2)
            {
                cw /= 2;
                if(ch/2 >= 2)
                {
                    ch /= 2;
                    if(i)
                    {
                        SETSHADER(hdrreduce2);
                    }
                    else
                    {
                        SETSHADER(hdrluminance2);
                    }
                }
                else if(i)
                {
                    SETSHADER(hdrreduce2w);
                }
                else
                {
                    SETSHADER(hdrluminance2w);
                }
            }
            else if(i)
            {
                SETSHADER(hdrreduce);
            }
            else
            {
                SETSHADER(hdrluminance);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, b1fbo);
            glViewport(0, 0, cw, ch);
            glBindTexture(GL_TEXTURE_RECTANGLE, ltex);
            screenquad(lw, lh);

            ltex = b1tex;
            lw = cw;
            lh = ch;
            std::swap(b0fbo, b1fbo);
            std::swap(b0tex, b1tex);
            std::swap(b0w, b1w);
            std::swap(b0h, b1h);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, bloomfbo[4]);
        glViewport(0, 0, bloompbo ? 4 : 1, 1);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
        SETSHADER(hdraccum);
        glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
        LOCALPARAMF(accumscale, lasthdraccum ? std::pow(hdraccumscale, static_cast<float>(lastmillis - lasthdraccum)/hdraccummillis) : 0);
        screenquad(2, 2);
        glDisable(GL_BLEND);

        if(bloompbo)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, bloompbo);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, 4, 1, GL_RED, GL_FLOAT, nullptr);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }

        lasthdraccum = lastmillis;
    }

    if(bloompbo)
    {
        gle::bindvbo(bloompbo);
        gle::enablecolor();
        gle::colorpointer(sizeof(GLfloat), nullptr, GL_FLOAT, 1);
        gle::clearvbo();
    }

    b0fbo = bloomfbo[3];
    b0tex = bloomtex[3];
    b1fbo = bloomfbo[2];
    b1tex = bloomtex[2];
    b0w = b1w = bloomw;
    b0h = b1h = bloomh;

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, bloomtex[4]);
    glActiveTexture(GL_TEXTURE0);

    glBindFramebuffer(GL_FRAMEBUFFER, b0fbo);
    glViewport(0, 0, b0w, b0h);
    SETSHADER(hdrbloom);
    glBindTexture(GL_TEXTURE_RECTANGLE, ptex);
    screenquad(pw, ph);

    if(bloomblur)
    {
        std::array<float, maxblurradius+1> blurweights,
                                           bluroffsets;
        setupblurkernel(bloomblur, blurweights.data(), bluroffsets.data());
        for(int i = 0; i < (2 + 2*bloomiter); ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, b1fbo);
            glViewport(0, 0, b1w, b1h);
            setblurshader(i%2, 1, bloomblur, blurweights.data(), bluroffsets.data(), GL_TEXTURE_RECTANGLE);
            glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
            screenquad(b0w, b0h);
            std::swap(b0w, b1w);
            std::swap(b0h, b1h);
            std::swap(b0tex, b1tex);
            std::swap(b0fbo, b1fbo);
        }
    }

    if(aa >= AA_Split)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, outfbo);
        glViewport(0, 0, vieww, viewh);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
        glActiveTexture(GL_TEXTURE0);
        switch(aa)
        {
            case AA_SplitLuma:
            {
                SETSHADER(msaatonemapsplitluma);
                break;
            }
            case AA_SplitMasked:
            {
                SETSHADER(msaatonemapsplitmasked);
                setaavelocityparams(GL_TEXTURE3);
                break;
            }
            default:
            {
                SETSHADER(msaatonemapsplit);
                break;
            }
        }
        screenquad(vieww, viewh, b0w, b0h);
    }
    else if(msaalight <= 1 || !msaatonemap)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, outfbo);
        glViewport(0, 0, vieww, viewh);
        glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
        glActiveTexture(GL_TEXTURE0);
        switch(aa)
        {
            case AA_Luma:
            {
                SETSHADER(hdrtonemapluma);
                break;
            }
            case AA_Masked:
            {
                if(!msaasamples && ghasstencil)
                {
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    glStencilFunc(GL_EQUAL, 0, 0x80);
                    glEnable(GL_STENCIL_TEST);
                    SETSHADER(hdrtonemap);
                    screenquad(vieww, viewh, b0w, b0h);

                    glStencilFunc(GL_EQUAL, 0x80, 0x80);
                    SETSHADER(hdrtonemapstencil);
                    screenquad(vieww, viewh, b0w, b0h);
                    glDisable(GL_STENCIL_TEST);
                    goto done; //see bottom of fxn
                }
                SETSHADER(hdrtonemapmasked);
                setaavelocityparams(GL_TEXTURE3);
                break;
            }
            default:
            {
                SETSHADER(hdrtonemap);
                break;
            }
        }
        screenquad(vieww, viewh, b0w, b0h);
    }
    else
    {
        bool blit = msaalight > 2 && msaatonemapblit && (!aa || !outfbo);

        glBindFramebuffer(GL_FRAMEBUFFER, blit ? msrefractfbo : outfbo);
        glViewport(0, 0, vieww, viewh);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
        glActiveTexture(GL_TEXTURE0);

        if(blit)
        {
            SETSHADER(msaatonemapsample);
        }
        else
        {
            switch(aa)
            {
                case AA_Luma:
                {
                    SETSHADER(msaatonemapluma);
                    break;
                }
                case AA_Masked:
                {
                    SETSHADER(msaatonemapmasked);
                    setaavelocityparams(GL_TEXTURE3);
                    break;
                }
                default:
                {
                    SETSHADER(msaatonemap);
                    break;
                }
            }
        }
        screenquad(vieww, viewh, b0w, b0h);

        if(blit)
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msrefractfbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, aa || !outfbo ? refractfbo : outfbo);
            glBlitFramebuffer(0, 0, vieww, viewh, 0, 0, vieww, viewh, GL_COLOR_BUFFER_BIT, GL_NEAREST);

            if(!outfbo)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, outfbo);
                glViewport(0, 0, vieww, viewh);
                if(!blit)
                {
                    SETSHADER(hdrnop);
                }
                else
                {
                    switch(aa)
                    {
                        case AA_Luma:
                        {
                            SETSHADER(hdrnopluma);
                            break;
                        }
                        case AA_Masked:
                        {
                            SETSHADER(hdrnopmasked);
                            setaavelocityparams(GL_TEXTURE3);
                            break;
                        }
                        default:
                        {
                            SETSHADER(hdrnop);
                            break;
                        }
                    }
                }
                glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
                screenquad(vieww, viewh);
            }
        }
    }

done:
    if(bloompbo)
    {
        gle::disablecolor();
    }

    endtimer(hdrtimer);
}

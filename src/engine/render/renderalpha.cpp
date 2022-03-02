/* renderalpha.cpp: alpha geoemtry rendering
 *
 * libprimis has support for a single level of alpha geometry, which is rendered
 * using a single stencil layer over the base geometry
 *
 * combinations of alpha materials (glass, alpha, water) therefore do not stack
 * since there is only one stencil and only the nearest layer in the view frustum
 * is rendered
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"

#include "hdr.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "renderparticles.h"
#include "rendertimers.h"
#include "renderva.h"
#include "stain.h"
#include "texture.h"

#include "world/material.h"
#include "world/octaedit.h"

int transparentlayer = 0;

//internally relevant functionality
namespace
{
    FVAR(refractmargin, 0, 0.1f, 1);     //margin for gl scissoring around refractive materials
    FVAR(refractdepth, 1e-3f, 16, 1e3f); //sets depth for refract shader effect

    //sets up alpha handling as needed then executes main particle rendering routine
    void alphaparticles(float allsx1, float allsy1, float allsx2, float allsy2)
    {
        if(particlelayers && ghasstencil)
        {
            bool scissor = allsx1 > -1 || allsy1 > -1 || allsx2 < 1 || allsy2 < 1;
            if(scissor)
            {
                int x1 = static_cast<int>(std::floor((allsx1*0.5f+0.5f)*vieww)),
                    y1 = static_cast<int>(std::floor((allsy1*0.5f+0.5f)*viewh)),
                    x2 = static_cast<int>(std::ceil((allsx2*0.5f+0.5f)*vieww)),
                    y2 = static_cast<int>(std::ceil((allsy2*0.5f+0.5f)*viewh));
                glEnable(GL_SCISSOR_TEST);
                glScissor(x1, y1, x2 - x1, y2 - y1);
            }
            glStencilFunc(GL_NOTEQUAL, 0, 0x07);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            glEnable(GL_STENCIL_TEST);
            gbuf.renderparticles(ParticleLayer_Over);
            glDisable(GL_STENCIL_TEST);
            if(scissor)
            {
                glDisable(GL_SCISSOR_TEST);
            }
            gbuf.renderparticles(ParticleLayer_NoLayer);
        }
        else
        {
            gbuf.renderparticles();
        }
    }
}

//externally relevant functionality

void GBuffer::rendertransparent()
{
    int hasalphavas = findalphavas(),
        hasmats = findmaterials();
    bool hasmodels = transmdlsx1 < transmdlsx2 && transmdlsy1 < transmdlsy2;
    if(!hasalphavas && !hasmats && !hasmodels) //don't transparent render if there is no alpha
    {
        if(!editmode)
        {
            gbuf.renderparticles();
        }
        return;
    }
    if(!editmode && particlelayers && ghasstencil)
    {
        gbuf.renderparticles(ParticleLayer_Under);
    }
    timer *transtimer = begintimer("transparent");
    if(hasalphavas&4 || hasmats&4)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, msaalight ? msrefractfbo : refractfbo);
        glDepthMask(GL_FALSE);
        if(msaalight)
        {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        }
        else
        {
            glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
        }
        float sx1 = std::min(alpharefractsx1, matrefractsx1),
              sy1 = std::min(alpharefractsy1, matrefractsy1),
              sx2 = std::max(alpharefractsx2, matrefractsx2),
              sy2 = std::max(alpharefractsy2, matrefractsy2);
        bool scissor = sx1 > -1 || sy1 > -1 || sx2 < 1 || sy2 < 1;
        if(scissor)
        {
            int x1 = static_cast<int>(std::floor(std::max(sx1*0.5f+0.5f-refractmargin*viewh/vieww, 0.0f)*vieww)),
                y1 = static_cast<int>(std::floor(std::max(sy1*0.5f+0.5f-refractmargin, 0.0f)*viewh)),
                x2 = static_cast<int>(std::ceil(std::min(sx2*0.5f+0.5f+refractmargin*viewh/vieww, 1.0f)*vieww)),
                y2 = static_cast<int>(std::ceil(std::min(sy2*0.5f+0.5f+refractmargin, 1.0f)*viewh));
            glEnable(GL_SCISSOR_TEST);
            glScissor(x1, y1, x2 - x1, y2 - y1);
        }
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        if(scissor)
        {
            glDisable(GL_SCISSOR_TEST);
        }
        GLOBALPARAMF(refractdepth, 1.0f/refractdepth);
        SETSHADER(refractmask);
        if(hasalphavas&4)
        {
            renderrefractmask();
        }
        if(hasmats&4)
        {
            rendermaterialmask();
        }
        glDepthMask(GL_TRUE);
    }

    glActiveTexture_(GL_TEXTURE7);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
    }
    glActiveTexture_(GL_TEXTURE8);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
    }
    glActiveTexture_(GL_TEXTURE9);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    }
    glActiveTexture_(GL_TEXTURE0);
    if(ghasstencil)
    {
        glEnable(GL_STENCIL_TEST);
    }
    matrix4 raymatrix(vec(-0.5f*vieww*projmatrix.a.x, 0, 0.5f*vieww - 0.5f*vieww*projmatrix.c.x),
                      vec(0, -0.5f*viewh*projmatrix.b.y, 0.5f*viewh - 0.5f*viewh*projmatrix.c.y));
    raymatrix.muld(cammatrix);
    GLOBALPARAM(raymatrix, raymatrix);
    GLOBALPARAM(linearworldmatrix, linearworldmatrix);

    uint tiles[lighttilemaxheight];
    float allsx1 =  1,
          allsy1 =  1,
          allsx2 = -1,
          allsy2 = -1;
    float sx1, sy1, sx2, sy2;

    for(int layer = 0; layer < 4; ++layer)
    {
        switch(layer)
        {
            case 0:
            {
                if(!(hasmats&1))
                {
                    continue;
                }
                sx1 = matliquidsx1;
                sy1 = matliquidsy1;
                sx2 = matliquidsx2;
                sy2 = matliquidsy2;
                memcpy(tiles, matliquidtiles, sizeof(tiles));
                break;
            }
            case 1:
            {
                if(!(hasalphavas&1))
                {
                    continue;
                }
                sx1 = alphabacksx1;
                sy1 = alphabacksy1;
                sx2 = alphabacksx2;
                sy2 = alphabacksy2;
                memcpy(tiles, alphatiles, sizeof(tiles));
                break;
            }
            case 2:
            {
                if(!(hasalphavas&2) && !(hasmats&2))
                {
                    continue;
                }
                sx1 = alphafrontsx1;
                sy1 = alphafrontsy1;
                sx2 = alphafrontsx2;
                sy2 = alphafrontsy2;
                memcpy(tiles, alphatiles, sizeof(tiles));
                if(hasmats&2)
                {
                    sx1 = std::min(sx1, matsolidsx1);
                    sy1 = std::min(sy1, matsolidsy1);
                    sx2 = std::max(sx2, matsolidsx2);
                    sy2 = std::max(sy2, matsolidsy2);
                    for(int j = 0; j < lighttilemaxheight; ++j)
                    {
                        tiles[j] |= matsolidtiles[j];
                    }
                }
                break;
            }
            case 3:
            {
                if(!hasmodels)
                {
                    continue;
                }
                sx1 = transmdlsx1;
                sy1 = transmdlsy1;
                sx2 = transmdlsx2;
                sy2 = transmdlsy2;
                memcpy(tiles, transmdltiles, sizeof(tiles));
                break;
            }
            default:
            {
                continue;
            }
        }
        transparentlayer = layer+1;
        allsx1 = std::min(allsx1, sx1);
        allsy1 = std::min(allsy1, sy1);
        allsx2 = std::max(allsx2, sx2);
        allsy2 = std::max(allsy2, sy2);

        glBindFramebuffer(GL_FRAMEBUFFER, msaalight ? msfbo : gfbo);
        if(ghasstencil)
        {
            glStencilFunc(GL_ALWAYS, layer+1, ~0);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        }
        else
        {
            bool scissor = sx1 > -1 || sy1 > -1 || sx2 < 1 || sy2 < 1;
            if(scissor)
            {
                int x1 = static_cast<int>(std::floor((sx1*0.5f+0.5f)*vieww)),
                    y1 = static_cast<int>(std::floor((sy1*0.5f+0.5f)*viewh)),
                    x2 = static_cast<int>(std::ceil((sx2*0.5f+0.5f)*vieww)),
                    y2 = static_cast<int>(std::ceil((sy2*0.5f+0.5f)*viewh));
                glEnable(GL_SCISSOR_TEST);
                glScissor(x1, y1, x2 - x1, y2 - y1);
            }

            maskgbuffer("n");
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            if(scissor)
            {
                glDisable(GL_SCISSOR_TEST);
            }
        }
        maskgbuffer("cndg");

        if(wireframe && editmode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        switch(layer)
        {
            case 0:
            {
                renderliquidmaterials();
                break;
            }
            case 1:
            {
                renderalphageom(1);
                break;
            }
            case 2:
            {
                if(hasalphavas&2)
                {
                    renderalphageom(2);
                }
                if(hasmats&2)
                {
                    rendersolidmaterials();
                }
                renderstains(StainBuffer_Transparent, true, layer+1);
                break;
            }
            case 3:
            {
                rendertransparentmodelbatches(layer+1);
                break;
            }
        }

        if(wireframe && editmode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        if(msaalight)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mshdrfbo);
            if((ghasstencil && msaaedgedetect) || msaalight==2)
            {
                for(int i = 0; i < 2; ++i)
                {
                    renderlights(sx1, sy1, sx2, sy2, tiles, layer+1, i+1, true);
                }
            }
            else
            {
                renderlights(sx1, sy1, sx2, sy2, tiles, layer+1, 3, true);
            }
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);
            renderlights(sx1, sy1, sx2, sy2, tiles, layer+1, 0, true);
        }

        switch(layer)
        {
            case 2:
            {
                renderstains(StainBuffer_Transparent, false, layer+1);
                break;
            }
        }
    }

    transparentlayer = 0;

    if(ghasstencil)
    {
        glDisable(GL_STENCIL_TEST);
    }

    endtimer(transtimer);

    if(editmode)
    {
        return;
    }
    alphaparticles(allsx1, allsy1, allsx2, allsy2);
}

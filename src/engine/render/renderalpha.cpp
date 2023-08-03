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
#include "shader.h"
#include "shaderparam.h"
#include "stain.h"
#include "texture.h"

#include "world/material.h"
#include "world/octaedit.h"

//internally relevant functionality
namespace
{
    FVAR(refractmargin, 0, 0.1f, 1);     //margin for gl scissoring around refractive materials
    FVAR(refractdepth, 1e-3f, 16, 1e3f); //sets depth for refract shader effect
}

//sets up alpha handling as needed then executes main particle rendering routine
//private method of gbuffer
void GBuffer::alphaparticles(float allsx1, float allsy1, float allsx2, float allsy2) const
{
    if(particlelayers && ghasstencil)
    {
        bool scissor = allsx1 > -1 || allsy1 > -1 || allsx2 < 1 || allsy2 < 1;
        if(scissor)
        {
            int x1 = static_cast<int>(std::floor((allsx1*0.5f+0.5f)*static_cast<float>(vieww))),
                y1 = static_cast<int>(std::floor((allsy1*0.5f+0.5f)*static_cast<float>(viewh))),
                x2 = static_cast<int>(std::ceil((allsx2*0.5f+0.5f)*static_cast<float>(vieww))),
                y2 = static_cast<int>(std::ceil((allsy2*0.5f+0.5f)*static_cast<float>(viewh)));
            glEnable(GL_SCISSOR_TEST);
            glScissor(x1, y1, x2 - x1, y2 - y1);
        }
        glStencilFunc(GL_NOTEQUAL, 0, 0x07);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glEnable(GL_STENCIL_TEST);
        renderparticles(ParticleLayer_Over);
        glDisable(GL_STENCIL_TEST);
        if(scissor)
        {
            glDisable(GL_SCISSOR_TEST);
        }
        renderparticles(ParticleLayer_NoLayer);
    }
    else
    {
        renderparticles();
    }
}

//externally relevant functionality

void GBuffer::rendertransparent()
{
    const MaterialInfo &mi = findmaterials(); //generate mat* vars
    const AlphaInfo &ai = findalphavas(); //generate mat* vars
    int hasalphavas = ai.hasalphavas;
    int hasmats = mi.hasmats;
    bool hasmodels = tmodelinfo.mdlsx1 < tmodelinfo.mdlsx2 && tmodelinfo.mdlsy1 < tmodelinfo.mdlsy2;
    if(!hasalphavas && !hasmats && !hasmodels) //don't transparent render if there is no alpha
    {
        if(!editmode)
        {
            renderparticles();
        }
        return;
    }
    if(!editmode && particlelayers && ghasstencil)
    {
        renderparticles(ParticleLayer_Under);
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
        float sx1 = std::min(ai.alpharefractsx1, mi.matrefractsx1),
              sy1 = std::min(ai.alpharefractsy1, mi.matrefractsy1),
              sx2 = std::max(ai.alpharefractsx2, mi.matrefractsx2),
              sy2 = std::max(ai.alpharefractsy2, mi.matrefractsy2);
        bool scissor = sx1 > -1 || sy1 > -1 || sx2 < 1 || sy2 < 1;
        if(scissor)
        {
            int x1 = static_cast<int>(std::floor(std::max(sx1*0.5f+0.5f-refractmargin*static_cast<float>(viewh)/static_cast<float>(vieww), 0.0f)*static_cast<float>(vieww))),
                y1 = static_cast<int>(std::floor(std::max(sy1*0.5f+0.5f-refractmargin, 0.0f)*static_cast<float>(viewh))),
                x2 = static_cast<int>(std::ceil(std::min(sx2*0.5f+0.5f+refractmargin*static_cast<float>(viewh)/static_cast<float>(vieww), 1.0f)*static_cast<float>(vieww))),
                y2 = static_cast<int>(std::ceil(std::min(sy2*0.5f+0.5f+refractmargin, 1.0f)*static_cast<float>(viewh)));
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

    glActiveTexture(GL_TEXTURE7);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
    }
    glActiveTexture(GL_TEXTURE8);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
    }
    glActiveTexture(GL_TEXTURE9);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    }
    glActiveTexture(GL_TEXTURE0);
    if(ghasstencil)
    {
        glEnable(GL_STENCIL_TEST);
    }
    matrix4 raymatrix(vec(-0.5f*static_cast<float>(vieww)*projmatrix.a.x, 0, 0.5f*static_cast<float>(vieww) - 0.5f*static_cast<float>(vieww)*projmatrix.c.x),
                      vec(0, -0.5f*static_cast<float>(viewh)*projmatrix.b.y, 0.5f*static_cast<float>(viewh) - 0.5f*static_cast<float>(viewh)*projmatrix.c.y));
    raymatrix.muld(cammatrix);
    GLOBALPARAM(raymatrix, raymatrix);
    GLOBALPARAM(linearworldmatrix, linearworldmatrix);

    std::array<uint, lighttilemaxheight> tiles;
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
                sx1 = mi.matliquidsx1;
                sy1 = mi.matliquidsy1;
                sx2 = mi.matliquidsx2;
                sy2 = mi.matliquidsy2;
                std::memcpy(tiles.data(), mi.matliquidtiles, sizeof(tiles));
                break;
            }
            case 1:
            {
                if(!(hasalphavas&1))
                {
                    continue;
                }
                sx1 = ai.alphabacksx1;
                sy1 = ai.alphabacksy1;
                sx2 = ai.alphabacksx2;
                sy2 = ai.alphabacksy2;
                std::memcpy(tiles.data(), alphatiles, tiles.size()*sizeof(uint));
                break;
            }
            case 2:
            {
                if(!(hasalphavas&2) && !(hasmats&2))
                {
                    continue;
                }
                sx1 = ai.alphafrontsx1;
                sy1 = ai.alphafrontsy1;
                sx2 = ai.alphafrontsx2;
                sy2 = ai.alphafrontsy2;
                std::memcpy(tiles.data(), alphatiles, tiles.size()*sizeof(uint));
                if(hasmats&2)
                {
                    sx1 = std::min(sx1, mi.matsolidsx1);
                    sy1 = std::min(sy1, mi.matsolidsy1);
                    sx2 = std::max(sx2, mi.matsolidsx2);
                    sy2 = std::max(sy2, mi.matsolidsy2);
                    for(int j = 0; j < lighttilemaxheight; ++j)
                    {
                        tiles[j] |= mi.matsolidtiles[j];
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
                sx1 = tmodelinfo.mdlsx1;
                sy1 = tmodelinfo.mdlsy1;
                sx2 = tmodelinfo.mdlsx2;
                sy2 = tmodelinfo.mdlsy2;
                std::memcpy(tiles.data(), tmodelinfo.mdltiles.data(), tiles.size()*sizeof(uint));
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
                int x1 = static_cast<int>(std::floor((sx1*0.5f+0.5f)*static_cast<float>(vieww))),
                    y1 = static_cast<int>(std::floor((sy1*0.5f+0.5f)*static_cast<float>(viewh))),
                    x2 = static_cast<int>(std::ceil((sx2*0.5f+0.5f)*static_cast<float>(vieww))),
                    y2 = static_cast<int>(std::ceil((sy2*0.5f+0.5f)*static_cast<float>(viewh)));
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
                    renderlights(sx1, sy1, sx2, sy2, tiles.data(), layer+1, i+1, true);
                }
            }
            else
            {
                renderlights(sx1, sy1, sx2, sy2, tiles.data(), layer+1, 3, true);
            }
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);
            renderlights(sx1, sy1, sx2, sy2, tiles.data(), layer+1, 0, true);
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

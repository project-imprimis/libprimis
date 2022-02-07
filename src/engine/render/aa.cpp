/* aa.cpp
 *
 * screenspace antialiasing methods (does NOT include multisample [MSAA]):
 * TQAA (temporal quincunx antialiasing)
 * FXAA (fast approximate antialiasing)
 * SMAA (subpixel morphological antialiasing)
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "aa.h"
#include "hdr.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendertimers.h"
#include "renderwindow.h"
#include "texture.h"

#include "interface/control.h"

//externally used vars
VAR(tqaaresolvegather, 1, 0, 0);
matrix4 nojittermatrix;

namespace //internal functions incl. AA implementations
{
    /* TQAA: Temporal Quincunx Anti Aliasing */
    ///////////////////////////////////////////

    VARFP(tqaa, 0, 0, 1, cleanupaa()); //`t`emporal `q`uincunx `a`nti `a`liasing: toggles temporal/quincunx antialiasing in general
    FVAR(tqaareproject, 0, 75, 1e3f); // `t`emporal `q`uincunx `a`nti `a`liasing `re-project`: factor which determines how much the tqaa algo is allowed to shift frame to frame (lower = more movementallowed)
    VARF(tqaamovemask, 0, 1, 1, cleanupaa());
    VARP(tqaaquincunx, 0, 1, 1); // `t`emporal `q`uincunx `a`nti `a`liasing `quincunx` toggles quincunx antialiasing for temporal AA (half pixel offset)
    FVAR(tqaacolorweightscale, 0, 0.25f, 1e3f);
    FVAR(tqaacolorweightbias, 0, 0.01f, 1);


    int tqaaframe = 0;
    GLuint tqaatex[2] = { 0, 0 },
           tqaafbo[2] = { 0, 0 };
    matrix4 tqaaprevscreenmatrix;
    int tqaatype = -1;

    void loadtqaashaders()
    {
        tqaatype = tqaamovemask ? AA_Masked : AA_Unused;
        loadhdrshaders(tqaatype);
        useshaderbyname("tqaaresolve");
    }

    void setuptqaa(int w, int h)
    {
        for(int i = 0; i < 2; ++i)
        {
            if(!tqaatex[i])
            {
                glGenTextures(1, &tqaatex[i]);
            }
            if(!tqaafbo[i])
            {
                glGenFramebuffers_(1, &tqaafbo[i]);
            }
            glBindFramebuffer_(GL_FRAMEBUFFER, tqaafbo[i]);
            createtexture(tqaatex[i], w, h, nullptr, 3, 1, GL_RGBA8, GL_TEXTURE_RECTANGLE);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, tqaatex[i], 0);
            gbuf.bindgdepth();
            if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                fatal("failed allocating TQAA buffer!");
            }
        }
        glBindFramebuffer_(GL_FRAMEBUFFER, 0);
        tqaaprevscreenmatrix.identity();
        loadtqaashaders();
    }

    void cleanuptqaa()
    {
        tqaatype = -1;
        for(int i = 0; i < 2; ++i)
        {
            if(tqaatex[i])
            {
                glDeleteTextures(1, &tqaatex[i]);
                tqaatex[i] = 0;
            }
        }
        for(int i = 0; i < 2; ++i)
        {
            if(tqaafbo[i])
            {
                glDeleteFramebuffers_(1, &tqaafbo[i]);
                tqaafbo[i] = 0;
            }
        }
        tqaaframe = 0;
    }

    VAR(debugtqaa, 0, 0, 2); //renders either the first or second tqaa texture at 1 or 2

    void viewtqaa()
    {
        int w = debugfullscreen ? hudw : std::min(hudw, hudh)/2,
            h = debugfullscreen ? hudh : (w*hudh)/hudw,
            tw = gw,
            th = gh;
        SETSHADER(hudrect,);
        gle::colorf(1, 1, 1);
        switch(debugtqaa)
        {
            case 1:
            {
                glBindTexture(GL_TEXTURE_RECTANGLE, tqaatex[0]);
                break;
            }
            case 2:
            {
                glBindTexture(GL_TEXTURE_RECTANGLE, tqaatex[1]);
                break;
            }
        }
        debugquad(0, 0, w, h, 0, 0, tw, th);
    }

    void resolvetqaa(GLuint outfbo)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, outfbo);
        SETSHADER(tqaaresolve,);
        LOCALPARAMF(colorweight, tqaacolorweightscale, -tqaacolorweightbias*tqaacolorweightscale);
        glBindTexture(GL_TEXTURE_RECTANGLE, tqaatex[0]);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, tqaaframe ? tqaatex[1] : tqaatex[0]);
        gbuf.setaavelocityparams(GL_TEXTURE2);
        glActiveTexture_(GL_TEXTURE0);
        vec4<float> quincunx(0, 0, 0, 0);
        if(tqaaquincunx)
        {
            quincunx = tqaaframe&1 ? vec4<float>(0.25f, 0.25f, -0.25f, -0.25f) : vec4<float>(-0.25f, -0.25f, 0.25f, 0.25f);
        }
        if(multisampledaa())
        {
            quincunx.x *=  0.5f;
            quincunx.y *= -0.5f;
            quincunx.z *=  0.5f;
            quincunx.w *= -0.5f;
        }
        LOCALPARAM(quincunx, quincunx);
        screenquad(vieww, viewh);

        std::swap(tqaafbo[0], tqaafbo[1]);
        std::swap(tqaatex[0], tqaatex[1]);
        tqaaprevscreenmatrix = screenmatrix;
        tqaaframe++;
    }

    void dotqaa(GLuint outfbo = 0)
    {
        timer *tqaatimer = begintimer("tqaa");

        resolvetqaa(outfbo);

        endtimer(tqaatimer);
    }
    //end of TQAA code

    /* FXAA: Fast approXimate Anti Aliasing */
    //////////////////////////////////////////

    namespace fxaa
    {
        GLuint fxaafbo = 0,
               fxaatex = 0;

        void cleanupfxaa(); //fxn prototype required due to VARFP initialization chicken/egg

        VARFP(fxaa, 0, 0, 1, cleanupfxaa());
        VARFP(fxaaquality, 0, 1, 3, cleanupfxaa());
        VARFP(fxaagreenluma, 0, 0, 1, cleanupfxaa());

        int fxaatype = -1;
        Shader *fxaashader = nullptr;

        void loadfxaashaders()
        {
            fxaatype = tqaatype >= 0 ? tqaatype : (!fxaagreenluma && !intel_texalpha_bug ? AA_Luma : AA_Unused);
            loadhdrshaders(fxaatype);
            string opts;
            int optslen = 0;
            if(tqaa || fxaagreenluma || intel_texalpha_bug)
            {
                opts[optslen++] = 'g';
            }
            opts[optslen] = '\0';
            DEF_FORMAT_STRING(fxaaname, "fxaa%d%s", fxaaquality, opts);
            fxaashader = generateshader(fxaaname, "fxaashaders %d \"%s\"", fxaaquality, opts);
        }

        void clearfxaashaders()
        {
            fxaatype = -1;
            fxaashader = nullptr;
        }

        void setupfxaa(int w, int h)
        {
            if(!fxaatex)
            {
                glGenTextures(1, &fxaatex);
            }
            if(!fxaafbo)
            {
                glGenFramebuffers_(1, &fxaafbo);
            }
            glBindFramebuffer_(GL_FRAMEBUFFER, fxaafbo);
            createtexture(fxaatex, w, h, nullptr, 3, 1, tqaa || (!fxaagreenluma && !intel_texalpha_bug) ? GL_RGBA8 : GL_RGB, GL_TEXTURE_RECTANGLE);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, fxaatex, 0);
            gbuf.bindgdepth();
            if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                fatal("failed allocating FXAA buffer!");
            }
            glBindFramebuffer_(GL_FRAMEBUFFER, 0);

            loadfxaashaders();
        }

        void cleanupfxaa()
        {
            if(fxaafbo)
            {
                glDeleteFramebuffers_(1, &fxaafbo);
                fxaafbo = 0;
            }
            if(fxaatex)
            {
                glDeleteTextures(1, &fxaatex);
                fxaatex = 0;
            }
            clearfxaashaders();
        }

        void dofxaa(GLuint outfbo = 0)
        {
            timer *fxaatimer = begintimer("fxaa");
            glBindFramebuffer_(GL_FRAMEBUFFER, tqaa ? tqaafbo[0] : outfbo);
            fxaashader->set();
            glBindTexture(GL_TEXTURE_RECTANGLE, fxaatex);
            screenquad(vieww, viewh);
            if(tqaa)
            {
                resolvetqaa(outfbo);
            }
            endtimer(fxaatimer);
        }
        //end of FXAA code
    }
    /* SMAA: Subpixel Morphological Anti Aliasing */
    ////////////////////////////////////////////////

    class subpixelaa
    {
        public:
            GLuint smaafbo[4] = { 0, 0, 0, 0 };
            int smaatype = -1;

            void cleanupsmaa();
            void setupsmaa(int w, int h);

            //executes the smaa process on the given output framebuffer object (outfbo)
            //split toggles splitting process into two passes
            void dosmaa(GLuint outfbo = 0, bool split = false);

            //debug view for smaa buffers
            void viewsmaa();
        private:
            //smaa graphics buffers
            GLuint smaaareatex = 0,
                   smaasearchtex = 0,
                   smaatex[5] = { 0, 0, 0, 0, 0 };
            //affects subsample vector direction
            int smaasubsampleorder = -1;
            Shader  *smaalumaedgeshader = nullptr,
                    *smaacoloredgeshader = nullptr,
                    *smaablendweightshader = nullptr,
                    *smaaneighborhoodshader = nullptr;

            static constexpr int smaaareatexwidth = 160,
                                 smaaareatexheight = 560;

            static constexpr int smaasearchtexwidth  = 66,
                                 smaasearchtexheight = 33;

            uchar smaasearchdata[smaasearchtexwidth*smaasearchtexheight];
            uchar smaaareadata[smaaareatexwidth*smaaareatexheight*2];

            bool smaasearchdatainited = false;
            bool smaaareadatainited = false;

            static constexpr int orthoedges[][2] =
            {
                {0, 0}, {3, 0}, {0, 3}, {3, 3}, {1, 0}, {4, 0}, {1, 3}, {4, 3},
                {0, 1}, {3, 1}, {0, 4}, {3, 4}, {1, 1}, {4, 1}, {1, 4}, {4, 4}
            };
            static constexpr int edgesdiag[][2] =
            {
                {0, 0}, {1, 0}, {0, 2}, {1, 2}, {2, 0}, {3, 0}, {2, 2}, {3, 2},
                {0, 1}, {1, 1}, {0, 3}, {1, 3}, {2, 1}, {3, 1}, {2, 3}, {3, 3}
            };

            static constexpr float offsetsortho[] = { 0.0f, -0.25f, 0.25f, -0.125f, 0.125f, -0.375f, 0.375f };
            static constexpr float offsetsdiag[][2] = {{ 0.0f, 0.0f, }, { 0.25f, -0.25f }, { -0.25f, 0.25f }, { 0.125f, -0.125f }, { -0.125f, 0.125f } };

            void gensmaasearchdata();
            vec2 areaunderortho(const vec2 &p1, const vec2 &p2, float x);
            void loadsmaashaders(bool split = false);
            void clearsmaashaders();
            vec2 areaortho(float p1x, float p1y, float p2x, float p2y, float left);
            void smootharea(float d, vec2 &a1, vec2 &a2);
            vec2 areaortho(int pattern, float left, float right, float offset);

            float areaunderdiag(const vec2 &p1, const vec2 &p2, const vec2 &p);
            vec2 areadiag(const vec2 &p1, const vec2 &p2, float left);
            vec2 areadiag(float p1x, float p1y, float p2x, float p2y, float d, float left, const vec2 &offset, int pattern);
            vec2 areadiag2(float p1x, float p1y, float p2x, float p2y, float p3x, float p3y, float p4x, float p4y, float d, float left, const vec2 &offset, int pattern);
            vec2 areadiag(int pattern, float left, float right, const vec2 &offset);
            void gensmaaareadata();
    };

    subpixelaa smaarenderer;

    /* smaa vars are set by `setupsmaa()` automatically: if TQAA and/or MSAA are
     * enabled, the following variables will be set to 1
     *
     * generally, do not change these vars from ingame
     */
    VAR(smaat2x, 1, 0, 0); //SMAA Temporal 2x (temporal antialiasing)
    VAR(smaas2x, 1, 0, 0); //SMAA Split 2x (multisample antialiasing)
    VAR(smaa4x, 1, 0, 0);  //SMAA 4x (both temporal and multisample)
    VARFP(smaa, 0, 0, 1, gbuf.cleanupgbuffer()); //toggles smaa
    VARFP(smaaspatial, 0, 1, 1, gbuf.cleanupgbuffer());
    VARFP(smaaquality, 0, 2, 3, smaarenderer.cleanupsmaa());
    VARFP(smaacoloredge, 0, 0, 1, smaarenderer.cleanupsmaa()); //toggle between color & luma edge shaders
    VARFP(smaagreenluma, 0, 0, 1, smaarenderer.cleanupsmaa());
    VARF(smaadepthmask, 0, 1, 1, smaarenderer.cleanupsmaa());
    VARF(smaastencil, 0, 1, 1, smaarenderer.cleanupsmaa());
    VAR(debugsmaa, 0, 0, 5); //see viewsmaa() below, displays one of the five smaa texs

    void subpixelaa::loadsmaashaders(bool split)
    {
        smaatype = tqaatype >= 0 ? tqaatype : (!smaagreenluma && !intel_texalpha_bug && !smaacoloredge ? AA_Luma : AA_Unused);
        if(split)
        {
            smaatype += AA_Split;
        }
        loadhdrshaders(smaatype);

        string opts;
        int optslen = 0;
        if((smaadepthmask && (!tqaa || msaalight)) || (smaastencil && ghasstencil > (msaasamples ? 1 : 0)))
        {
            opts[optslen++] = 'd';
        }
        if(split)
        {
            opts[optslen++] = 's';
        }
        if(tqaa || smaagreenluma || intel_texalpha_bug)
        {
            opts[optslen++] = 'g';
        }
        if(tqaa)
        {
            opts[optslen++] = 't';
        }
        opts[optslen] = '\0';
        DEF_FORMAT_STRING(lumaedgename, "SMAALumaEdgeDetection%d%s", smaaquality, opts);
        DEF_FORMAT_STRING(coloredgename, "SMAAColorEdgeDetection%d%s", smaaquality, opts);
        DEF_FORMAT_STRING(blendweightname, "SMAABlendingWeightCalculation%d%s", smaaquality, opts);
        DEF_FORMAT_STRING(neighborhoodname, "SMAANeighborhoodBlending%d%s", smaaquality, opts);
        smaalumaedgeshader = lookupshaderbyname(lumaedgename);
        smaacoloredgeshader = lookupshaderbyname(coloredgename);
        smaablendweightshader = lookupshaderbyname(blendweightname);
        smaaneighborhoodshader = lookupshaderbyname(neighborhoodname);

        if(smaalumaedgeshader && smaacoloredgeshader && smaablendweightshader && smaaneighborhoodshader)
        {
            return;
        }
        generateshader(nullptr, "smaashaders %d \"%s\"", smaaquality, opts);
        smaalumaedgeshader = lookupshaderbyname(lumaedgename);
        if(!smaalumaedgeshader)
        {
            smaalumaedgeshader = nullshader;
        }
        smaacoloredgeshader = lookupshaderbyname(coloredgename);
        if(!smaacoloredgeshader)
        {
            smaacoloredgeshader = nullshader;
        }
        smaablendweightshader = lookupshaderbyname(blendweightname);
        if(!smaablendweightshader)
        {
            smaablendweightshader = nullshader;
        }
        smaaneighborhoodshader = lookupshaderbyname(neighborhoodname);
        if(!smaaneighborhoodshader)
        {
            smaaneighborhoodshader = nullshader;
        }
    }

    void subpixelaa::clearsmaashaders()
    {
        smaatype = -1;
        smaalumaedgeshader = nullptr;
        smaacoloredgeshader = nullptr;
        smaablendweightshader = nullptr;
        smaaneighborhoodshader = nullptr;
    }

    void subpixelaa::gensmaasearchdata()
    {
        if(smaasearchdatainited)
        {
            return;
        }
        int edges[33];
        memset(edges, -1, sizeof(edges));
        for(int i = 0; i < 2; ++i)
        {
            for(int j = 0; j < 2; ++j)
            {
                for(int k = 0; k < 2; ++k)
                {
                    for(int l = 0; l < 2; ++l)
                    {
                        edges[(i*1 + j*3) + (k*7 + l*21)] = i + (j<<1) + (k<<2) + (l<<3);
                    }
                }
            }
        }
        memset(smaasearchdata, 0, sizeof(smaasearchdata));
        for(int y = 0; y < 33; ++y)
        {
            for(int x = 0; x < 33; ++x)
            {
                int left = edges[x],
                    top = edges[y];
                if(left < 0 || top < 0)
                {
                    continue;
                }
                uchar deltaLeft = 0;
                if(top&(1<<3))
                {
                    deltaLeft++;
                }
                if(deltaLeft && top&(1<<2) && !(left&(1<<1)) && !(left&(1<<3)))
                {
                    deltaLeft++;
                }
                smaasearchdata[y*66 + x] = deltaLeft;
                uchar deltaRight = 0;
                if(top&(1<<3) && !(left&(1<<1)) && !(left&(1<<3)))
                {
                    deltaRight++;
                }
                if(deltaRight && top&(1<<2) && !(left&(1<<0)) && !(left&(1<<2)))
                {
                    deltaRight++;
                }
                smaasearchdata[y*66 + 33 + x] = deltaRight;
            }
        }
        smaasearchdatainited = true;
    }

    vec2 subpixelaa::areaunderortho(const vec2 &p1, const vec2 &p2, float x)
    {
        vec2 d(p2.x - p1.x, p2.y - p1.y);
        float y1 = p1.y + (x - p1.x)*d.y/d.x,
              y2 = p1.y + (x+1 - p1.x)*d.y/d.x;
        if((x < p1.x || x >= p2.x) && (x+1 <= p1.x || x+1 > p2.x))
        {
            return vec2(0, 0);
        }
        if((y1 < 0) == (y2 < 0) || std::fabs(y1) < 1e-4f || std::fabs(y2) < 1e-4f)
        {
            float a = (y1 + y2) / 2;
            return a < 0.0f ? vec2(-a, 0) : vec2(0, a);
        }
        x = -p1.y*d.x/d.y + p1.x;
        float a1 = x > p1.x ? y1*std::fmod(x, 1.0f)/2 : 0,
              a2 = x < p2.x ? y2*(1-std::fmod(x, 1.0f))/2 : 0;
        vec2 a(std::fabs(a1), std::fabs(a2));
        if((a.x > a.y ? a1 : -a2) >= 0)
        {
            std::swap(a.x, a.y);
        }
        return a;
    }

    vec2 subpixelaa::areaortho(float p1x, float p1y, float p2x, float p2y, float left)
    {
        return areaunderortho(vec2(p1x, p1y), vec2(p2x, p2y), left);
    }

    void subpixelaa::smootharea(float d, vec2 &a1, vec2 &a2)
    {
        vec2 b1(sqrtf(a1.x*2)*0.5f, sqrtf(a1.y*2)*0.5f),
             b2(sqrtf(a2.x*2)*0.5f, sqrtf(a2.y*2)*0.5f);
        float p = std::clamp(d / 32.0f, 0.0f, 1.0f);
        a1.lerp(b1, a1, p);
        a2.lerp(b2, a2, p);
    }

    vec2 subpixelaa::areaortho(int pattern, float left, float right, float offset)
    {
        float d = left + right + 1,
              o1 = offset + 0.5f,
              o2 = offset - 0.5f;
        switch(pattern)
        {
            case 0:
            {
                return vec2(0, 0);
            }
            case 1:
            {
                return left <= right ? areaortho(0, o2, d/2, 0, left) : vec2(0, 0);
            }
            case 2:
            {
                return left >= right ? areaortho(d/2, 0, d, o2, left) : vec2(0, 0);
            }
            case 3:
            {
                vec2 a1 = areaortho(0, o2, d/2, 0, left), a2 = areaortho(d/2, 0, d, o2, left);
                smootharea(d, a1, a2);
                return a1.add(a2);
            }
            case 4:
            {
                return left <= right ? areaortho(0, o1, d/2, 0, left) : vec2(0, 0);
            }
            case 5:
            {
                return vec2(0, 0);
            }
            case 6:
            {
                vec2 a = areaortho(0, o1, d, o2, left);
                if(std::fabs(offset) > 0)
                {
                    a.avg(areaortho(0, o1, d/2, 0, left).add(areaortho(d/2, 0, d, o2, left)));
                }
                return a;
            }
            case 7:
            {
                return areaortho(0, o1, d, o2, left);
            }
            case 8:
            {
                return left >= right ? areaortho(d/2, 0, d, o1, left) : vec2(0, 0);
            }
            case 9:
            {
                vec2 a = areaortho(0, o2, d, o1, left);
                if(std::fabs(offset) > 0)
                {
                    a.avg(areaortho(0, o2, d/2, 0, left).add(areaortho(d/2, 0, d, o1, left)));
                }
                return a;
            }
            case 10:
            {
                return vec2(0, 0);
            }
            case 11:
            {
                return areaortho(0, o2, d, o1, left);
            }
            case 12:
            {
                vec2 a1 = areaortho(0, o1, d/2, 0, left),
                     a2 = areaortho(d/2, 0, d, o1, left);
                smootharea(d, a1, a2);
                return a1.add(a2);
            }
            case 13:
            {
                return areaortho(0, o2, d, o1, left);
            }
            case 14:
            {
                return areaortho(0, o1, d, o2, left);
            }
            case 15:
            {
                return vec2(0, 0);
            }
        }
        return vec2(0, 0);
    }

    float subpixelaa::areaunderdiag(const vec2 &p1, const vec2 &p2, const vec2 &p)
    {
        vec2 d(p2.y - p1.y, p1.x - p2.x);
        float dp = d.dot(vec2(p1).sub(p));
        if(!d.x)
        {
            if(!d.y)
            {
                return 1;
            }
            return std::clamp(d.y > 0 ? 1 - dp/d.y : dp/d.y, 0.0f, 1.0f);
        }
        if(!d.y)
        {
            return std::clamp(d.x > 0 ? 1 - dp/d.x : dp/d.x, 0.0f, 1.0f);
        }
        float l =  dp/d.y,
              r = (dp-d.x)/d.y,
              b =  dp/d.x,
              t = (dp-d.y)/d.x;
        if(0 <= dp)
        {
            if(d.y <= dp)
            {
                if(d.x <= dp)
                {
                    if(d.y+d.x <= dp)
                    {
                        return 0;
                    }
                    return 0.5f*(1-r)*(1-t);
                }
                if(d.y+d.x > dp)
                {
                    return std::min(1-b, 1-t) + 0.5f*std::fabs(b-t);
                }
                return 0.5f*(1-b)*r;
            }
            if(d.x <= dp)
            {
                if(d.y+d.x <= dp)
                {
                    return 0.5f*(1-l)*t;
                }
                return std::min(1-l, 1-r) + 0.5f*std::fabs(r-l);
            }
            return 1 - 0.5f*l*b;
        }
        if(d.y <= dp)
        {
            if(d.x <= dp)
            {
                return 0.5f*l*b;
            }
            if(d.y+d.x <= dp)
            {
                return std::min(l, r) + 0.5f*std::fabs(r-l);
            }
            return 1 - 0.5f*(1-l)*t;
        }
        if(d.x <= dp)
        {
            if(d.y+d.x <= dp)
            {
                return std::min(b, t) + 0.5f*std::fabs(b-t);
            }
            return 1 - 0.5f*(1-b)*r;
        }
        if(d.y+d.x <= dp)
        {
            return 1 - 0.5f*(1-t)*(1-r);
        }
        return 1;
    }

    vec2 subpixelaa::areadiag(const vec2 &p1, const vec2 &p2, float left)
    {
        return vec2(1 - areaunderdiag(p1, p2, vec2(1, 0).add(left)), areaunderdiag(p1, p2, vec2(1, 1).add(left)));
    }

    vec2 subpixelaa::areadiag(float p1x, float p1y, float p2x, float p2y, float d, float left, const vec2 &offset, int pattern)
    {
        vec2 p1(p1x, p1y),
             p2(p2x+d, p2y+d);
        if(edgesdiag[pattern][0])
        {
            p1.add(offset);
        }
        if(edgesdiag[pattern][1])
        {
            p2.add(offset);
        }
        return areadiag(p1, p2, left);
    }

    vec2 subpixelaa::areadiag2(float p1x, float p1y, float p2x, float p2y, float p3x, float p3y, float p4x, float p4y, float d, float left, const vec2 &offset, int pattern)
    {
        vec2 p1(p1x, p1y),
             p2(p2x+d, p2y+d),
             p3(p3x, p3y),
             p4(p4x+d, p4y+d);
        if(edgesdiag[pattern][0])
        {
            p1.add(offset);
            p3.add(offset);
        }
        if(edgesdiag[pattern][1])
        {
            p2.add(offset);
            p4.add(offset);
        }
        return areadiag(p1, p2, left).avg(areadiag(p3, p4, left));
    }

    vec2 subpixelaa::areadiag(int pattern, float left, float right, const vec2 &offset)
    {
        float d = left + right + 1;
        switch(pattern)
        {
            case 0:  return areadiag2(1, 1, 1, 1, 1, 0, 1, 0, d, left, offset, pattern);
            case 1:  return areadiag2(1, 0, 0, 0, 1, 0, 1, 0, d, left, offset, pattern);
            case 2:  return areadiag2(0, 0, 1, 0, 1, 0, 1, 0, d, left, offset, pattern);
            case 3:  return  areadiag(1, 0, 1, 0, d, left, offset, pattern);
            case 4:  return areadiag2(1, 1, 0, 0, 1, 1, 1, 0, d, left, offset, pattern);
            case 5:  return areadiag2(1, 1, 0, 0, 1, 0, 1, 0, d, left, offset, pattern);
            case 6:  return  areadiag(1, 1, 1, 0, d, left, offset, pattern);
            case 7:  return areadiag2(1, 1, 1, 0, 1, 0, 1, 0, d, left, offset, pattern);
            case 8:  return areadiag2(0, 0, 1, 1, 1, 0, 1, 1, d, left, offset, pattern);
            case 9:  return  areadiag(1, 0, 1, 1, d, left, offset, pattern);
            case 10: return areadiag2(0, 0, 1, 1, 1, 0, 1, 0, d, left, offset, pattern);
            case 11: return areadiag2(1, 0, 1, 1, 1, 0, 1, 0, d, left, offset, pattern);
            case 12: return  areadiag(1, 1, 1, 1, d, left, offset, pattern);
            case 13: return areadiag2(1, 1, 1, 1, 1, 0, 1, 1, d, left, offset, pattern);
            case 14: return areadiag2(1, 1, 1, 1, 1, 1, 1, 0, d, left, offset, pattern);
            case 15: return areadiag2(1, 1, 1, 1, 1, 0, 1, 0, d, left, offset, pattern);
        }
        return vec2(0, 0);
    }

    void subpixelaa::gensmaaareadata()
    {
        if(smaaareadatainited)
        {
            return;
        }
        memset(smaaareadata, 0, sizeof(smaaareadata));
        for(int offset = 0; offset < static_cast<int>(sizeof(offsetsortho)/sizeof(offsetsortho[0])); ++offset)
        {
            for(int pattern = 0; pattern < 16; ++pattern)
            {
                int px = orthoedges[pattern][0]*16,
                    py = (5*offset + orthoedges[pattern][1])*16;
                uchar *dst = &smaaareadata[(py*smaaareatexwidth + px)*2];
                for(int y = 0; y < 16; ++y)
                {
                    for(int x = 0; x < 16; ++x)
                    {
                        vec2 a = areaortho(pattern, x*x, y*y, offsetsortho[offset]);
                        dst[0] = static_cast<uchar>(255*a.x);
                        dst[1] = static_cast<uchar>(255*a.y);
                        dst += 2;
                    }
                    dst += (smaaareatexwidth-16)*2;
                }
            }
        }
        for(int offset = 0; offset < static_cast<int>(sizeof(offsetsdiag)/sizeof(offsetsdiag[0])); ++offset)
        {
            for(int pattern = 0; pattern < 16; ++pattern)
            {
                int px = 5*16 + edgesdiag[pattern][0]*20,
                    py = (4*offset + edgesdiag[pattern][1])*20;
                uchar *dst = &smaaareadata[(py*smaaareatexwidth + px)*2];
                for(int y = 0; y < 20; ++y)
                {
                    for(int x = 0; x < 20; ++x)
                    {
                        vec2 a = areadiag(pattern, x, y, vec2(offsetsdiag[offset][0], offsetsdiag[offset][1]));
                        dst[0] = static_cast<uchar>(255*a.x);
                        dst[1] = static_cast<uchar>(255*a.y);
                        dst += 2;
                    }
                    dst += (smaaareatexwidth-20)*2;
                }
            }
        }
        smaaareadatainited = true;
    }

    void subpixelaa::setupsmaa(int w, int h)
    {
        if(!smaaareatex)
        {
            glGenTextures(1, &smaaareatex);
        }
        if(!smaasearchtex)
        {
            glGenTextures(1, &smaasearchtex);
        }
        gensmaasearchdata();
        gensmaaareadata();
        createtexture(  smaaareatex,   smaaareatexwidth,   smaaareatexheight,   smaaareadata, 3, 1, GL_RG8, GL_TEXTURE_RECTANGLE, 0, 0, 0, false);
        createtexture(smaasearchtex, smaasearchtexwidth, smaasearchtexheight, smaasearchdata, 3, 0,  GL_R8, GL_TEXTURE_RECTANGLE, 0, 0, 0, false);
        bool split = multisampledaa();
        smaasubsampleorder = split ? (msaapositions[0].x < 0.5f ? 1 : 0) : -1;
        smaat2x = tqaa ? 1 : 0;
        smaas2x = split ? 1 : 0;
        smaa4x = tqaa && split ? 1 : 0;
        for(int i = 0; i < (split ? 4 : 3); ++i)
        {
            if(!smaatex[i])
            {
                glGenTextures(1, &smaatex[i]);
            }
            if(!smaafbo[i])
            {
                glGenFramebuffers_(1, &smaafbo[i]);
            }
            glBindFramebuffer_(GL_FRAMEBUFFER, smaafbo[i]);
            GLenum format = GL_RGB;
            switch(i)
            {
                case 0:
                {
                    format = tqaa || (!smaagreenluma && !intel_texalpha_bug && !smaacoloredge) ? GL_RGBA8 : GL_RGB;
                    break;
                }
                case 1:
                {
                    format = GL_RG8;
                    break;
                }
                case 2:
                case 3:
                {
                    format = GL_RGBA8;
                    break;
                }
            }
            createtexture(smaatex[i], w, h, nullptr, 3, 1, format, GL_TEXTURE_RECTANGLE);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, smaatex[i], 0);
            if(!i && split)
            {
                if(!smaatex[4])
                {
                    glGenTextures(1, &smaatex[4]);
                }
                createtexture(smaatex[4], w, h, nullptr, 3, 1, format, GL_TEXTURE_RECTANGLE);
                glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, smaatex[4], 0);
                static const GLenum drawbufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
                glDrawBuffers_(2, drawbufs);
            }
            if(!i || (smaadepthmask && (!tqaa || msaalight)) || (smaastencil && ghasstencil > (msaasamples ? 1 : 0)))
            {
                gbuf.bindgdepth();
            }
            if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                fatal("failed allocating SMAA buffer!");
            }
        }
        glBindFramebuffer_(GL_FRAMEBUFFER, 0);

        loadsmaashaders(split);
    }

    void subpixelaa::cleanupsmaa()
    {
        if(smaaareatex)
        {
            glDeleteTextures(1, &smaaareatex);
            smaaareatex = 0;
        }
        if(smaasearchtex)
        {
            glDeleteTextures(1, &smaasearchtex);
            smaasearchtex = 0;
        }
        for(int i = 0; i < 4; ++i)
        {
            if(smaafbo[i])
            {
                glDeleteFramebuffers_(1, &smaafbo[i]);
                smaafbo[i] = 0;
            }
        }
        for(int i = 0; i < 5; ++i)
        {
            if(smaatex[i])
            {
                glDeleteTextures(1, &smaatex[i]);
                smaatex[i] = 0;
            }
        }
        smaasubsampleorder = -1;
        smaat2x = smaas2x = smaa4x = 0;

        clearsmaashaders();
    }

    void subpixelaa::viewsmaa()
    {
        int w = debugfullscreen ? hudw : std::min(hudw, hudh)/2,
            h = debugfullscreen ? hudh : (w*hudh)/hudw,
            tw = gw,
            th = gh;
        SETSHADER(hudrect,);
        gle::colorf(1, 1, 1);
        /* debugsmaa levels:
         *  1: show the output tex resulting from smaa
         *  2: show the raw filtering output applied to the screen buffer
         *  3: show the refined filtering output applied to the screen buffer
         *  4: show the buffer of edge-detect patterns
         *  5: show the smaa search texture
         */
        switch(debugsmaa)
        {
            case 1:
            {
                glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[0]);
                break;
            }
            case 2:
            {
                glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[1]);
                break;
            }
            case 3:
            {
                glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[2]);
                break;
            }
            case 4:
            {
                glBindTexture(GL_TEXTURE_RECTANGLE, smaaareatex);
                tw = smaaareatexwidth;
                th = smaaareatexheight;
                break;
            }
            case 5:
            {
                glBindTexture(GL_TEXTURE_RECTANGLE, smaasearchtex);
                tw = smaasearchtexwidth;
                th = smaasearchtexheight;
                break;
            }
        }
        debugquad(0, 0, w, h, 0, 0, tw, th);
    }

    void subpixelaa::dosmaa(GLuint outfbo, bool split)
    {
        timer *smaatimer = begintimer("smaa");

        int cleardepth = msaalight ? GL_DEPTH_BUFFER_BIT | (ghasstencil > 1 ? GL_STENCIL_BUFFER_BIT : 0) : 0;
        bool depthmask = smaadepthmask && (!tqaa || msaalight),
             stencil = smaastencil && ghasstencil > (msaasamples ? 1 : 0);
        for(int pass = 0; pass < (split ? 2 : 1); ++pass) // loop through multiple passes if doing multisample aa
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, smaafbo[1]);
            if(depthmask || stencil)
            {
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT | (!pass ? cleardepth : 0));
            }
            if(depthmask)
            {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_ALWAYS);
                float depthval = cleardepth ? 0.25f*(pass+1) : 1;
                glDepthRange(depthval, depthval);
            }
            else if(stencil)
            {
                glEnable(GL_STENCIL_TEST);
                glStencilFunc(GL_ALWAYS, 0x10*(pass+1), ~0);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            }
            if(smaacoloredge)
            {
                smaacoloredgeshader->set();
            }
            else
            {
                smaalumaedgeshader->set();
            }
            glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[pass ? 4 : 0]);
            screenquad(vieww, viewh);
            glBindFramebuffer_(GL_FRAMEBUFFER, smaafbo[2 + pass]);
            if(depthmask)
            {
                glDepthFunc(GL_EQUAL);
                glDepthMask(GL_FALSE);
            }
            else if(stencil)
            {
                glStencilFunc(GL_EQUAL, 0x10*(pass+1), ~0);
                glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            }
            if(depthmask || stencil)
            {
                glClear(GL_COLOR_BUFFER_BIT);
            }
            smaablendweightshader->set();
            vec4<float> subsamples(0, 0, 0, 0);
            if(tqaa && split)
            {
                subsamples = tqaaframe&1 ?
                             (pass != smaasubsampleorder ? vec4<float>(6, 4, 2, 4) : vec4<float>(3, 5, 1, 4)) :
                             (pass != smaasubsampleorder ? vec4<float>(4, 6, 2, 3) : vec4<float>(5, 3, 1, 3));
            }
            else if(tqaa)
            {
                subsamples = tqaaframe&1 ? vec4<float>(2, 2, 2, 0) : vec4<float>(1, 1, 1, 0);
            }
            else if(split)
            {
                subsamples = pass != smaasubsampleorder ? vec4<float>(2, 2, 2, 0) : vec4<float>(1, 1, 1, 0);
            }
            LOCALPARAM(subsamples, subsamples);
            glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[1]);
            glActiveTexture_(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_RECTANGLE, smaaareatex);
            glActiveTexture_(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_RECTANGLE, smaasearchtex);
            glActiveTexture_(GL_TEXTURE0);
            screenquad(vieww, viewh);
            if(depthmask)
            {
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glDepthFunc(GL_LESS);
                glDepthRange(0, 1);
            }
            else if(stencil)
            {
                glDisable(GL_STENCIL_TEST);
            }
        }
        glBindFramebuffer_(GL_FRAMEBUFFER, tqaa ? tqaafbo[0] : outfbo);
        smaaneighborhoodshader->set();
        glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[0]);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[2]);
        if(split)
        {
            glActiveTexture_(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[4]);
            glActiveTexture_(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_RECTANGLE, smaatex[3]);
        }
        glActiveTexture_(GL_TEXTURE0);
        screenquad(vieww, viewh);
        if(tqaa)
        {
            resolvetqaa(outfbo);
        }
        endtimer(smaatimer);
    }
    //end of SMAA code

    /* general antialiasing control functions */
    ////////////////////////////////////////////
}

//for temporal aa, called externally
void GBuffer::setaavelocityparams(GLenum tmu)
{
    glActiveTexture_(tmu);
    if(msaalight)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    }
    glActiveTexture_(++tmu);
    if(msaasamples)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE, gnormaltex);
    }
    glActiveTexture_(GL_TEXTURE0);

    matrix4 reproject;
    reproject.muld(tqaaframe ? tqaaprevscreenmatrix : screenmatrix, worldmatrix);
    vec2 jitter = tqaaframe&1 ? vec2(0.5f, 0.5f) : vec2(-0.5f, -0.5f);
    if(multisampledaa())
    {
        jitter.x *= 0.5f;
        jitter.y *= -0.5f;
    }
    if(tqaaframe)
    {
        reproject.jitter(jitter.x, jitter.y);
    }
    LOCALPARAM(reprojectmatrix, reproject);
    float maxvel = sqrtf(vieww*vieww + viewh*viewh)/tqaareproject;
    LOCALPARAMF(maxvelocity, maxvel, 1/maxvel);
}

//general aa fxns
void setupaa(int w, int h)
{
    if(tqaa && !tqaafbo[0])
    {
        setuptqaa(w, h);
    }
    if(smaa)
    {
        if(!smaarenderer.smaafbo[0])
        {
            smaarenderer.setupsmaa(w, h);
        }
    }
    else if(fxaa::fxaa)
    {
        if(!fxaa::fxaafbo)
        {
            fxaa::setupfxaa(w, h);
        }
    }
}

void jitteraa()
{
    nojittermatrix = projmatrix;
    if(!drawtex && tqaa)
    {
        vec2 jitter = tqaaframe&1 ? vec2(0.25f, 0.25f) : vec2(-0.25f, -0.25f);
        if(multisampledaa())
        {
            jitter.x *= 0.5f;
            jitter.y *= -0.5f;
        }
        projmatrix.jitter(jitter.x*2.0f/vieww, jitter.y*2.0f/viewh);
    }
}

int aamaskstencil = -1,
           aamask = -1;

void setaamask(bool on)
{
    int val = on && !drawtex ? 1 : 0;
    if(aamask == val)
    {
        return;
    }
    if(!aamaskstencil)
    {
        glStencilOp(GL_KEEP, GL_KEEP, val ? GL_REPLACE : GL_KEEP);
        if(aamask < 0)
        {
            glStencilFunc(GL_ALWAYS, 0x80, ~0);
            glEnable(GL_STENCIL_TEST);
        }
    }
    else if(aamaskstencil > 0)
    {
        if(val)
        {
            glStencilFunc(GL_ALWAYS, 0x80 | aamaskstencil, ~0);
        }
        else if(aamask >= 0)
        {
            glStencilFunc(GL_ALWAYS, aamaskstencil, ~0);
        }
    }
    aamask = val;
    GLOBALPARAMF(aamask, aamask);
}

void enableaamask(int stencil)
{
    aamask = -1;
    aamaskstencil = !msaasamples && ghasstencil && tqaa && tqaamovemask && !drawtex ? stencil|avatarmask : -1;
}

void disableaamask()
{
    if(aamaskstencil >= 0 && aamask >= 0)
    {
        if(!aamaskstencil)
        {
            glDisable(GL_STENCIL_TEST);
        }
        else if(aamask)
        {
            glStencilFunc(GL_ALWAYS, aamaskstencil, ~0);
        }
        aamask = -1;
    }
}

bool multisampledaa()
{
    return msaasamples == 2 && (smaa ? msaalight && smaaspatial : tqaa);
}

//used by rendergl

/* doaa: executes one type of screenspace aa
 *
 * only one screenspace aa can be used at a time, and smaa will always be used
 * instead of fxaa or tqaa; fxaa will always be used instead of tqaa
 *
 * does not apply to multisample aa, msaa is not a screenspace aa
 *
 * method pointer resolve is used to setup the fbo for the specified aa
 */
void doaa(GLuint outfbo, GBuffer gbuffer)
{
    if(smaa)
    {
        bool split = multisampledaa();
        gbuffer.processhdr(smaarenderer.smaafbo[0], smaarenderer.smaatype);
        smaarenderer.dosmaa(outfbo, split);
    }
    else if(fxaa::fxaa)
    {
        gbuffer.processhdr(fxaa::fxaafbo, fxaa::fxaatype);
        fxaa::dofxaa(outfbo);
    }
    else if(tqaa)
    {
        gbuffer.processhdr(tqaafbo[0], tqaatype);
        dotqaa(outfbo);
    }
    else
    {
        gbuffer.processhdr(outfbo, AA_Unused);
    }
}

bool debugaa()
{
    if(debugsmaa)
    {
        smaarenderer.viewsmaa();
    }
    else if(debugtqaa)
    {
        viewtqaa();
    }
    else
    {
        return false;
    }
    return true;
}

void cleanupaa()
{
    smaarenderer.cleanupsmaa();
    fxaa::cleanupfxaa();
    cleanuptqaa();
}

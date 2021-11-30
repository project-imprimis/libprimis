/* water.cpp: rendering of water special effects
 *
 * water is a special material to render because of its dynamic effects caused
 * by the surface moving over time, and as a result has its own implementations
 * for special water functionality
 *
 * implemented are caustics, (light/dark areas on underwater surfaces due to lensing)
 * screenspace reflection, to capture the reflective surface, dynamic water surface
 * geometry, and dynamic waterfalls
 */
#include "engine.h"

#include "octarender.h"
#include "rendergl.h"
#include "renderlights.h"
#include "water.h"

#include "interface/control.h"

#include "world/material.h"

// ======================= caustics ===================== //

//caustics: lightening on surfaces underwater due to lensing effects from an
// uneven water surface

static constexpr int numcaustics = 32;

static Texture *caustictex[numcaustics] = {nullptr};
bool getentboundingbox(const extentity &e, ivec &o, ivec &r);

void loadcaustics(bool force)
{
    static bool needcaustics = false;
    if(force)
    {
        needcaustics = true;
    }
    if(!caustics || !needcaustics)
    {
        return;
    }
    useshaderbyname("caustics");
    if(caustictex[0])
    {
        return;
    }
    for(int i = 0; i < numcaustics; ++i)
    {
        DEF_FORMAT_STRING(name, "<grey><noswizzle>media/texture/mat_water/caustic/caust%.2d.png", i);
        caustictex[i] = textureload(name);
    }
}

void cleanupcaustics()
{
    for(int i = 0; i < numcaustics; ++i)
    {
        caustictex[i] = nullptr;
    }
}

VARFR(causticscale, 0, 50, 10000, preloadwatershaders());
VARFR(causticmillis, 0, 75, 1000, preloadwatershaders());
FVARR(causticcontrast, 0, 0.6f, 2);
FVARR(causticoffset, 0, 0.7f, 1);
VARFP(caustics, 0, 1, 1, { loadcaustics(); preloadwatershaders(); });

void setupcaustics(int tmu, float surface = -1e16f)
{
    if(!caustictex[0])
    {
        loadcaustics(true);
    }
    vec s = vec(0.011f, 0, 0.0066f).mul(100.0f/causticscale),
        t = vec(0, 0.011f, 0.0066f).mul(100.0f/causticscale);
    int tex = (lastmillis/causticmillis)%numcaustics;
    float frac = static_cast<float>(lastmillis%causticmillis)/causticmillis;
    for(int i = 0; i < 2; ++i)
    {
        glActiveTexture_(GL_TEXTURE0+tmu+i);
        glBindTexture(GL_TEXTURE_2D, caustictex[(tex+i)%numcaustics]->id);
    }
    glActiveTexture_(GL_TEXTURE0);
    float blendscale = causticcontrast, blendoffset = 1;
    if(surface > -1e15f)
    {
        float bz = surface + camera1->o.z + (vertwater ? wateramplitude : 0);
        matrix4 m(vec4(s.x, t.x,  0, 0),
                  vec4(s.y, t.y,  0, 0),
                  vec4(s.z, t.z, -1, 0),
                  vec4(  0,   0, bz, 1));
        m.mul(worldmatrix);
        GLOBALPARAM(causticsmatrix, m);
        blendscale *= 0.5f;
        blendoffset = 0;
    }
    else
    {
        GLOBALPARAM(causticsS, s);
        GLOBALPARAM(causticsT, t);
    }
    GLOBALPARAMF(causticsblend, blendscale*(1-frac), blendscale*frac, blendoffset - causticoffset*blendscale);
}

void rendercaustics(float surface, float syl, float syr)
{
    if(!caustics || !causticscale || !causticmillis)
    {
        return;
    }
    glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
    setupcaustics(0, surface);
    SETSHADER(caustics);
    gle::defvertex(2);
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(1, -1);
    gle::attribf(-1, -1);
    gle::attribf(1, syr);
    gle::attribf(-1, syl);
    gle::end();
}

void GBuffer::renderwaterfog(int mat, float surface)
{
    glDepthFunc(GL_NOTEQUAL);
    glDepthMask(GL_FALSE);
    glDepthRange(1, 1);

    glEnable(GL_BLEND);

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

    vec p[4] =
    {
        invcamprojmatrix.perspectivetransform(vec(-1, -1, -1)),
        invcamprojmatrix.perspectivetransform(vec(-1, 1, -1)),
        invcamprojmatrix.perspectivetransform(vec(1, -1, -1)),
        invcamprojmatrix.perspectivetransform(vec(1, 1, -1))
    };
    float bz = surface + camera1->o.z + (vertwater ? wateramplitude : 0),
          syl = (p[1].z > p[0].z) ? (2*(bz - p[0].z)/(p[1].z - p[0].z) - 1) : 1,
          syr = (p[3].z > p[2].z) ? (2*(bz - p[2].z)/(p[3].z - p[2].z) - 1) : 1;

    if((mat&MatFlag_Volume) == Mat_Water)
    {
        const bvec &deepcolor = getwaterdeepcolor(mat);
        int deep = getwaterdeep(mat);
        GLOBALPARAMF(waterdeepcolor, deepcolor.x*ldrscaleb, deepcolor.y*ldrscaleb, deepcolor.z*ldrscaleb);
        vec deepfade = getwaterdeepfade(mat).tocolor().mul(deep);
        GLOBALPARAMF(waterdeepfade,
            deepfade.x ? calcfogdensity(deepfade.x) : -1e4f,
            deepfade.y ? calcfogdensity(deepfade.y) : -1e4f,
            deepfade.z ? calcfogdensity(deepfade.z) : -1e4f,
            deep ? calcfogdensity(deep) : -1e4f);

        rendercaustics(surface, syl, syr);
    }
    else
    {
        GLOBALPARAMF(waterdeepcolor, 0, 0, 0);
        GLOBALPARAMF(waterdeepfade, -1e4f, -1e4f, -1e4f, -1e4f);
    }

    GLOBALPARAMF(waterheight, bz);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    SETSHADER(waterfog);
    gle::defvertex(3);
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf( 1, -1,  1);
    gle::attribf(-1, -1,  1);
    gle::attribf( 1, syr, 1);
    gle::attribf(-1, syl, 1);
    gle::end();

    glDisable(GL_BLEND);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDepthRange(0, 1);
}

/* vertex water */

// vertex water refers to the ability for the engine to dynamically create geom
// for the water material's surface, to simulate waviness directly by creating
// 3d geometry

//these variables control the vertex water geometry intensity
//(nothing to do with any other rendering)
VARP(watersubdiv, 0, 3, 3); //gridpower of water geometry
VARP(waterlod, 0, 1, 3);    //larger means that geometry is finer for longer distances

static int wx1, wy1, wx2, wy2, wsize;
static float whscale, whoffset;

//========================================================== VERTW VERTWN VERTWT
#define VERTW(vertw, defbody, body) \
    static void def##vertw() \
    { \
        gle::defvertex(); \
        defbody; \
    } \
    static void vertw(float v1, float v2, float v3) \
    { \
        float angle = (v1 - wx1) * (v2 - wy1) * (v1 - wx2) * (v2 - wy2) * whscale + whoffset; \
        float s = angle - static_cast<int>(angle) - 0.5f; \
        s *= 8 - std::fabs(s)*16; \
        float h = wateramplitude*s-wateroffset; \
        gle::attribf(v1, v2, v3+h); \
        body; \
    }
#define VERTWN(vertw, defbody, body) \
    static void def##vertw() \
    { \
        gle::defvertex(); \
        defbody; \
    } \
    static void vertw(float v1, float v2, float v3) \
    { \
        float h = -wateroffset; \
        gle::attribf(v1, v2, v3+h); \
        body; \
    }
#define VERTWT(vertwt, defbody, body) \
    VERTW(vertwt, defbody, { \
        float v = angle - static_cast<int>(angle+0.25f) - 0.25f; \
        v *= 8 - std::fabs(v)*16; \
        float duv = 0.5f*v; \
        body; \
    })

static float wxscale = 1.0f,
             wyscale = 1.0f,
             wscroll = 0.0f;

VERTW(vertwt, {
    gle::deftexcoord0();
}, {
    gle::attribf(wxscale*v1, wyscale*v2);
})
VERTWN(vertwtn, {
    gle::deftexcoord0();
}, {
    gle::attribf(wxscale*v1, wyscale*v2);
})

VERTW(vertl, {
    gle::deftexcoord0();
}, {
    gle::attribf(wxscale*(v1+wscroll), wyscale*(v2+wscroll));
})
VERTWN(vertln, {
    gle::deftexcoord0();
}, {
    gle::attribf(wxscale*(v1+wscroll), wyscale*(v2+wscroll));
})
#undef VERTW
#undef VERTWN
#undef VERTWT
//==============================================================================

void rendervertwater(int subdiv, int xo, int yo, int z, int size, int mat)
{
    wx1 = xo;
    wy1 = yo;
    wx2 = wx1 + size,
    wy2 = wy1 + size;
    wsize = size;
    whscale = 59.0f/(23.0f*wsize*wsize)/(2*M_PI); //59, 23 magic numbers
    if(mat == Mat_Water)
    {
        whoffset = std::fmod(static_cast<float>(lastmillis/600.0f/(2*M_PI)), 1.0f);
        defvertwt();
        gle::begin(GL_TRIANGLE_STRIP, 2*(wy2-wy1 + 1)*(wx2-wx1)/subdiv);
        for(int x = wx1; x<wx2; x += subdiv)
        {
            vertwt(x,        wy1, z);
            vertwt(x+subdiv, wy1, z);
            for(int y = wy1; y<wy2; y += subdiv)
            {
                vertwt(x,        y+subdiv, z);
                vertwt(x+subdiv, y+subdiv, z);
            }
            gle::multidraw();
        }
        xtraverts += gle::end();
    }
}

int calcwatersubdiv(int x, int y, int z, int size)
{
    float dist;
    if(camera1->o.x >= x && camera1->o.x < x + size &&
       camera1->o.y >= y && camera1->o.y < y + size)
    {
        dist = std::fabs(camera1->o.z - static_cast<float>(z));
    }
    else
    {
        dist = vec(x + size/2, y + size/2, z + size/2).dist(camera1->o) - size*1.42f/2;
    }
    int subdiv = watersubdiv + static_cast<int>(dist) / (32 << waterlod);
    return subdiv >= 31 ? INT_MAX : 1<<subdiv;
}

int renderwaterlod(int x, int y, int z, int size, int mat)
{
    if(size <= (32 << waterlod))
    {
        int subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv < size * 2)
        {
            rendervertwater(std::min(subdiv, size), x, y, z, size, mat);
        }
        return subdiv;
    }
    else
    {
        int subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv >= size)
        {
            if(subdiv < size * 2)
            {
                rendervertwater(size, x, y, z, size, mat);
            }
            return subdiv;
        }
        int childsize = size / 2,
            subdiv1 = renderwaterlod(x, y, z, childsize, mat),
            subdiv2 = renderwaterlod(x + childsize, y, z, childsize, mat),
            subdiv3 = renderwaterlod(x + childsize, y + childsize, z, childsize, mat),
            subdiv4 = renderwaterlod(x, y + childsize, z, childsize, mat),
            minsubdiv = subdiv1;
        minsubdiv = std::min(minsubdiv, subdiv2);
        minsubdiv = std::min(minsubdiv, subdiv3);
        minsubdiv = std::min(minsubdiv, subdiv4);
        if(minsubdiv < size * 2)
        {
            if(minsubdiv >= size)
            {
                rendervertwater(size, x, y, z, size, mat);
            }
            else
            {
                if(subdiv1 >= size)
                {
                    rendervertwater(childsize, x, y, z, childsize, mat);
                }
                if(subdiv2 >= size)
                {
                    rendervertwater(childsize, x + childsize, y, z, childsize, mat);
                }
                if(subdiv3 >= size)
                {
                    rendervertwater(childsize, x + childsize, y + childsize, z, childsize, mat);
                }
                if(subdiv4 >= size)
                {
                    rendervertwater(childsize, x, y + childsize, z, childsize, mat);
                }
            }
        }
        return minsubdiv;
    }
}

/* renderflatwater: renders water with no vertex water subdivision
 */
void renderflatwater(int x, int y, int z, int rsize, int csize, int mat)
{
    if(mat == Mat_Water)
    {
        if(gle::attribbuf.empty())
        {
            defvertwtn();
            gle::begin(GL_QUADS);
        }
        vertwtn(x, y, z);
        vertwtn(x+rsize, y, z);
        vertwtn(x+rsize, y+csize, z);
        vertwtn(x, y+csize, z);
        xtraverts += 4;
    }
}

VARFP(vertwater, 0, 1, 1, allchanged());

static void renderwater(const materialsurface &m, int mat = Mat_Water)
{
    if(!vertwater || drawtex == Draw_TexMinimap)
    {
        renderflatwater(m.o.x, m.o.y, m.o.z, m.rsize, m.csize, mat);
    }
    else if(renderwaterlod(m.o.x, m.o.y, m.o.z, m.csize, mat) >= static_cast<int>(m.csize) * 2)
    {
        rendervertwater(m.csize, m.o.x, m.o.y, m.o.z, m.csize, mat);
    }
}

#define WATERVARS(name) \
    CVAR0R(name##color, 0x01212C); \
    CVAR0R(name##deepcolor, 0x010A10); \
    CVAR0R(name##deepfade, 0x60BFFF); \
    CVAR0R(name##refractcolor, 0xFFFFFF); \
    VARR(name##fog, 0, 30, 10000); \
    VARR(name##deep, 0, 50, 10000); \
    VARR(name##spec, 0, 150, 200); \
    FVARR(name##refract, 0, 0.1f, 1e3f); \
    CVARR(name##fallcolor, 0); \
    CVARR(name##fallrefractcolor, 0); \
    VARR(name##fallspec, 0, 150, 200); \
    FVARR(name##fallrefract, 0, 0.1f, 1e3f);

WATERVARS(water)
WATERVARS(water2)
WATERVARS(water3)
WATERVARS(water4)

GETMATIDXVAR(water, color, const bvec &)
GETMATIDXVAR(water, deepcolor, const bvec &)
GETMATIDXVAR(water, deepfade, const bvec &)
GETMATIDXVAR(water, refractcolor, const bvec &)
GETMATIDXVAR(water, fallcolor, const bvec &)
GETMATIDXVAR(water, fallrefractcolor, const bvec &)
GETMATIDXVAR(water, fog, int)
GETMATIDXVAR(water, deep, int)
GETMATIDXVAR(water, spec, int)
GETMATIDXVAR(water, refract, float)
GETMATIDXVAR(water, fallspec, int)
GETMATIDXVAR(water, fallrefract, float)

VARFP(waterreflect, 0, 1, 1, { preloadwatershaders(); });
VARR(waterreflectstep, 1, 32, 10000);

void preloadwatershaders(bool force)
{
    static bool needwater = false;
    if(force)
    {
        needwater = true;
    }
    if(!needwater)
    {
        return;
    }
    if(caustics && causticscale && causticmillis)
    {
        if(waterreflect)
        {
            useshaderbyname("waterreflectcaustics");
        }
        else
        {
            useshaderbyname("watercaustics");
        }
    }
    else
    {
        if(waterreflect)
        {
            useshaderbyname("waterreflect");
        }
        else
        {
            useshaderbyname("water");
        }
    }
    useshaderbyname("underwater");
    useshaderbyname("waterfall");
    useshaderbyname("waterfog");
    useshaderbyname("waterminimap");
}

static float wfwave = 0.0f, //waterfall wave
             wfscroll = 0.0f, //waterfall scroll
             wfxscale = 1.0f, //waterfall x scale
             wfyscale = 1.0f; //waterfall y scale

//"waterfall" refers to any rendered side of water material
static void renderwaterfall(const materialsurface &m, float offset, const vec *normal = nullptr)
{
    if(gle::attribbuf.empty())
    {
        gle::defvertex();
        if(normal)
        {
            gle::defnormal();
        }
        gle::deftexcoord0();
        gle::begin(GL_QUADS);
    }
    float x = m.o.x,
          y = m.o.y,
          zmin = m.o.z,
          zmax = zmin;
    if(m.ends&1)
    {
        zmin += -wateroffset-wateramplitude;
    }
    if(m.ends&2)
    {
        zmax += wfwave;
    }
    int csize = m.csize,
        rsize = m.rsize;
#define GENFACEORIENT(orient, v0, v1, v2, v3) \
        case orient: \
        { \
            v0 v1 v2 v3 break; \
        }
#undef GENFACEVERTX
#define GENFACEVERTX(orient, vert, mx,my,mz, sx,sy,sz) \
            { \
                vec v(mx sx, my sy, mz sz); \
                gle::attribf(v.x, v.y, v.z); \
                GENFACENORMAL \
                gle::attribf(wfxscale*v.y, -wfyscale*(v.z+wfscroll)); \
            }
#undef GENFACEVERTY
#define GENFACEVERTY(orient, vert, mx,my,mz, sx,sy,sz) \
            { \
                vec v(mx sx, my sy, mz sz); \
                gle::attribf(v.x, v.y, v.z); \
                GENFACENORMAL \
                gle::attribf(wfxscale*v.x, -wfyscale*(v.z+wfscroll)); \
            }
#define GENFACENORMAL gle::attribf(n.x, n.y, n.z);
    if(normal)
    {
        vec n = *normal;
        switch(m.orient)
        {
            GENFACEVERTSXY(x, x, y, y, zmin, zmax, /**/, + csize, /**/, + rsize, + offset, - offset)
        }
    }
#undef GENFACENORMAL
#define GENFACENORMAL //empty macro
    else
    {
        switch(m.orient)
        {
            GENFACEVERTSXY(x, x, y, y, zmin, zmax, /**/, + csize, /**/, + rsize, + offset, - offset)
        }
    }
#undef GENFACENORMAL
#undef GENFACEORIENT
#undef GENFACEVERTX
#define GENFACEVERTX(o,n, x,y,z, xv,yv,zv) GENFACEVERT(o,n, x,y,z, xv,yv,zv)
#undef GENFACEVERTY
#define GENFACEVERTY(o,n, x,y,z, xv,yv,zv) GENFACEVERT(o,n, x,y,z, xv,yv,zv)
}

void renderwaterfalls()
{
    for(int k = 0; k < 4; ++k)
    {
        vector<materialsurface> &surfs = waterfallsurfs[k];
        if(surfs.empty())
        {
            continue;
        }
        MatSlot &wslot = lookupmaterialslot(Mat_Water+k);

        Texture *tex = wslot.sts.inrange(2) ? wslot.sts[2].t : (wslot.sts.inrange(0) ? wslot.sts[0].t : notexture);
        float angle = std::fmod(static_cast<float>(lastmillis/600.0f/(2*M_PI)), 1.0f),
              s = angle - static_cast<int>(angle) - 0.5f;
        s *= 8 - std::fabs(s)*16;
        wfwave = vertwater ? wateramplitude*s-wateroffset : -wateroffset;
        wfscroll = 16.0f*lastmillis/1000.0f;
        wfxscale = defaulttexscale/(tex->xs*wslot.scale);
        wfyscale = defaulttexscale/(tex->ys*wslot.scale);
        //waterfall color vectors
        bvec color = getwaterfallcolor(k),
             refractcolor = getwaterfallrefractcolor(k);
        if(color.iszero())
        {
            color = getwatercolor(k);
        }
        if(refractcolor.iszero())
        {
            refractcolor = getwaterrefractcolor(k);
        }
        float colorscale = (0.5f/255),
              refractscale = colorscale/ldrscale,
              refract = getwaterfallrefract(k);
        int spec = getwaterfallspec(k);
        GLOBALPARAMF(waterfallcolor, color.x*colorscale, color.y*colorscale, color.z*colorscale);
        GLOBALPARAMF(waterfallrefract, refractcolor.x*refractscale, refractcolor.y*refractscale, refractcolor.z*refractscale, refract*viewh);
        GLOBALPARAMF(waterfallspec, spec/100.0f);

        SETSHADER(waterfall);

        glBindTexture(GL_TEXTURE_2D, tex->id);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, wslot.sts.inrange(2) ? (wslot.sts.inrange(3) ? wslot.sts[3].t->id : notexture->id) : (wslot.sts.inrange(1) ? wslot.sts[1].t->id : notexture->id));
        glActiveTexture_(GL_TEXTURE0);
        for(int i = 0; i < surfs.length(); i++)
        {
            materialsurface &m = surfs[i];
            renderwaterfall(m, 0.1f, &matnormals[m.orient]);
        }
        xtraverts += gle::end();
    }
}

void renderwater()
{
    for(int k = 0; k < 4; ++k) //four types of water hardcoded
    {
        vector<materialsurface> &surfs = watersurfs[k];
        if(surfs.empty())
        {
            continue;
        }
        MatSlot &wslot = lookupmaterialslot(Mat_Water+k);

        Texture *tex = wslot.sts.inrange(0) ? wslot.sts[0].t: notexture;
        wxscale = defaulttexscale/(tex->xs*wslot.scale);
        wyscale = defaulttexscale/(tex->ys*wslot.scale);
        wscroll = 0.0f;

        glBindTexture(GL_TEXTURE_2D, tex->id);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, wslot.sts.inrange(1) ? wslot.sts[1].t->id : notexture->id);
        if(caustics && causticscale && causticmillis)
        {
            setupcaustics(2);
        }
        glActiveTexture_(GL_TEXTURE0);

        float colorscale = 0.5f/255,
              refractscale = colorscale/ldrscale,
              reflectscale = 0.5f/ldrscale;
        const bvec &color = getwatercolor(k),
                   &deepcolor = getwaterdeepcolor(k),
                   &refractcolor = getwaterrefractcolor(k);
        int fog = getwaterfog(k),
            deep = getwaterdeep(k),
            spec = getwaterspec(k);
        float refract = getwaterrefract(k);
        GLOBALPARAMF(watercolor, color.x*colorscale, color.y*colorscale, color.z*colorscale);
        GLOBALPARAMF(waterdeepcolor, deepcolor.x*colorscale, deepcolor.y*colorscale, deepcolor.z*colorscale);
        float fogdensity = fog ? calcfogdensity(fog) : -1e4f;
        GLOBALPARAMF(waterfog, fogdensity);
        vec deepfade = getwaterdeepfade(k).tocolor().mul(deep);
        GLOBALPARAMF(waterdeepfade,
            deepfade.x ? calcfogdensity(deepfade.x) : -1e4f,
            deepfade.y ? calcfogdensity(deepfade.y) : -1e4f,
            deepfade.z ? calcfogdensity(deepfade.z) : -1e4f,
            deep ? calcfogdensity(deep) : -1e4f);
        GLOBALPARAMF(waterspec, spec/100.0f);
        GLOBALPARAMF(waterreflect, reflectscale, reflectscale, reflectscale, waterreflectstep);
        GLOBALPARAMF(waterrefract, refractcolor.x*refractscale, refractcolor.y*refractscale, refractcolor.z*refractscale, refract*viewh);

        #define SETWATERSHADER(which, name) \
        do { \
            static Shader *name##shader = nullptr; \
            if(!name##shader) \
            { \
                name##shader = lookupshaderbyname(#name); \
            } \
            which##shader = name##shader; \
        } while(0)

        Shader *aboveshader = nullptr;
        if(drawtex == Draw_TexMinimap)
        {
            SETWATERSHADER(above, waterminimap);
        }
        else if(caustics && causticscale && causticmillis)
        {
            if(waterreflect)
            {
                SETWATERSHADER(above, waterreflectcaustics);
            }
            else
            {
                SETWATERSHADER(above, watercaustics);
            }
        }
        else
        {
            if(waterreflect)
            {
                SETWATERSHADER(above, waterreflect);
            }
            else
            {
                SETWATERSHADER(above, water);
            }
        }

        Shader *belowshader = nullptr;
        if(drawtex != Draw_TexMinimap)
        {
            SETWATERSHADER(below, underwater);
        }
        aboveshader->set();
        for(int i = 0; i < surfs.length(); i++)
        {
            materialsurface &m = surfs[i];
            if(camera1->o.z < m.o.z - wateroffset)
            {
                continue;
            }
            renderwater(m);
        }
        xtraverts += gle::end();
        if(belowshader)
        {
            belowshader->set();
            for(int i = 0; i < surfs.length(); i++)
            {
                materialsurface &m = surfs[i];
                if(camera1->o.z >= m.o.z - wateroffset)
                {
                    continue;
                }
                renderwater(m);
            }
            xtraverts += gle::end();
        }
    }
}


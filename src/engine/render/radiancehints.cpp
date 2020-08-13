#include "engine.h"

#include "radiancehints.h"
#include "world/light.h"

GLuint rhtex[8] = { 0, 0, 0, 0, 0, 0, 0, 0 },
       rhrb[4] = { 0, 0, 0, 0 },
       rhfbo = 0;
uint rhclearmasks[2][rhmaxsplits][(rhmaxgrid+2+31)/32];
GLuint rsmdepthtex = 0,
       rsmcolortex = 0,
       rsmnormaltex = 0,
       rsmfbo = 0;

reflectiveshadowmap rsm;
radiancehints rh;

static Shader *radiancehintsshader = NULL;
Shader *rsmworldshader = NULL;

Shader *loadradiancehintsshader()
{
    DEF_FORMAT_STRING(name, "radiancehints%d", rhtaps);
    return generateshader(name, "radiancehintsshader %d", rhtaps);
}

void loadrhshaders()
{
    if(rhborder)
    {
        useshaderbyname("radiancehintsborder");
    }
    if(rhcache)
    {
        useshaderbyname("radiancehintscached");
    }
    useshaderbyname("radiancehintsdisable");
    radiancehintsshader = loadradiancehintsshader();
    rsmworldshader = useshaderbyname("rsmworld");
    useshaderbyname("rsmsky");
}

void clearrhshaders()
{
    radiancehintsshader = NULL;
    rsmworldshader = NULL;
}

void setupradiancehints()
{
    GLenum rhformat = rhprec >= 1 ? GL_RGBA16F : GL_RGBA8;
    for(int i = 0; i < (!rhrect && rhcache ? 8 : 4); ++i)
    {
        if(!rhtex[i])
        {
            glGenTextures(1, &rhtex[i]);
        }
        create3dtexture(rhtex[i], rhgrid+2*rhborder, rhgrid+2*rhborder, (rhgrid+2*rhborder)*rhsplits, NULL, 7, 1, rhformat);
        if(rhborder)
        {
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
            GLfloat border[4] = { 0.5f, 0.5f, 0.5f, 0 };
            glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, border);
        }
    }
    if(!rhfbo)
    {
        glGenFramebuffers_(1, &rhfbo);
    }
    glBindFramebuffer_(GL_FRAMEBUFFER, rhfbo);

    if(rhrect)
    {
        for(int i = 0; i < 4; ++i)
        {
            if(!rhrb[i])
            {
                glGenRenderbuffers_(1, &rhrb[i]);
            }
            glBindRenderbuffer_(GL_RENDERBUFFER, rhrb[i]);
            glRenderbufferStorage_(GL_RENDERBUFFER, rhformat, (rhgrid + 2*rhborder)*(rhgrid + 2*rhborder), (rhgrid + 2*rhborder)*rhsplits);
            glBindRenderbuffer_(GL_RENDERBUFFER, 0);
            glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, rhrb[i]);
        }
    }
    else
    {
        for(int i = 0; i < 4; ++i)
        {
            glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_3D, rhtex[i], 0, 0);
        }
    }

    static const GLenum drawbufs[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers_(4, drawbufs);

    if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fatal("failed allocating radiance hints buffer!");
    }

    if(!rsmdepthtex)
    {
        glGenTextures(1, &rsmdepthtex);
    }
    if(!rsmcolortex)
    {
        glGenTextures(1, &rsmcolortex);
    }
    if(!rsmnormaltex)
    {
        glGenTextures(1, &rsmnormaltex);
    }
    if(!rsmfbo)
    {
        glGenFramebuffers_(1, &rsmfbo);
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, rsmfbo);
    GLenum rsmformat = gethdrformat(rsmprec, GL_RGBA8);

    createtexture(rsmdepthtex, rsmsize, rsmsize, NULL, 3, 0, rsmdepthprec > 1 ? GL_DEPTH_COMPONENT32 : (rsmdepthprec ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16), GL_TEXTURE_RECTANGLE);
    createtexture(rsmcolortex, rsmsize, rsmsize, NULL, 3, 0, rsmformat, GL_TEXTURE_RECTANGLE);
    createtexture(rsmnormaltex, rsmsize, rsmsize, NULL, 3, 0, rsmformat, GL_TEXTURE_RECTANGLE);

    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_RECTANGLE, rsmdepthtex, 0);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, rsmcolortex, 0);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, rsmnormaltex, 0);

    glDrawBuffers_(2, drawbufs);

    if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fatal("failed allocating RSM buffer!");
    }
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    loadrhshaders();
    clearradiancehintscache();
}

void cleanupradiancehints()
{
    clearradiancehintscache();

    for(int i = 0; i < 8; ++i)
    {
        if(rhtex[i])
        {
            glDeleteTextures(1, &rhtex[i]);
            rhtex[i] = 0;
        }
    }
    for(int i = 0; i < 4; ++i)
    {
        if(rhrb[i])
        {
            glDeleteRenderbuffers_(1, &rhrb[i]); rhrb[i] = 0;
        }
    }
    if(rhfbo)
    {
        glDeleteFramebuffers_(1, &rhfbo);
        rhfbo = 0;
    }
    if(rsmdepthtex)
    {
        glDeleteTextures(1, &rsmdepthtex);
        rsmdepthtex = 0;
    }
    if(rsmcolortex)
    {
        glDeleteTextures(1, &rsmcolortex);
        rsmcolortex = 0;
    }
    if(rsmnormaltex)
    {
        glDeleteTextures(1, &rsmnormaltex);
        rsmnormaltex = 0;
    }
    if(rsmfbo)
    {
        glDeleteFramebuffers_(1, &rsmfbo);
        rsmfbo = 0;
    }

    clearrhshaders();
}
//radiance hints (global illumination) vars
VARF(rhrect, 0, 0, 1, cleanupradiancehints());
VARF(rhsplits, 1, 2, rhmaxsplits, { cleardeferredlightshaders(); cleanupradiancehints(); });
VARF(rhborder, 0, 1, 1, cleanupradiancehints());
VARF(rsmsize, 64, 512, 2048, cleanupradiancehints()); //`r`adiance hints `s`hadow `m`ap `size`: resolution (squared) of global illumination
VARF(rhnearplane, 1, 1, 16, clearradiancehintscache());//`r`adiance `h`ints `near plane`: distance in gridpower 0 cubes before global illumination gets rendered
VARF(rhfarplane, 64, 1024, 16384, clearradiancehintscache());//`r`adiance `h`ints `far plane`: distance in gridpower 0 cubes whereafter global illumination no longer gets calculated
FVARF(rsmpradiustweak, 1e-3f, 1, 1e3f, clearradiancehintscache());//`r`adiance hints `s`hadow `m`ap `p`robe `radius tweak`
FVARF(rhpradiustweak, 1e-3f, 1, 1e3f, clearradiancehintscache());//`r`adiance `h`ints `p`robe `radius tweak`
FVARF(rsmdepthrange, 0, 1024, 1e6f, clearradiancehintscache());
FVARF(rsmdepthmargin, 0, 0.1f, 1e3f, clearradiancehintscache());
VARFP(rhprec, 0, 0, 1, cleanupradiancehints());
VARFP(rsmprec, 0, 0, 3, cleanupradiancehints());
VARFP(rsmdepthprec, 0, 0, 2, cleanupradiancehints());
FVAR(rhnudge, 0, 0.5f, 4);
FVARF(rhworldbias, 0, 0.5f, 10, clearradiancehintscache());
FVARF(rhsplitweight, 0.20f, 0.6f, 0.95f, clearradiancehintscache());
VARF(rhgrid, 3, 27, rhmaxgrid, cleanupradiancehints()); //`r`adiance `h`ints `grid`: subdivisions for the radiance hints to calculate
FVARF(rsmspread, 0, 0.35f, 1, clearradiancehintscache()); //smoothness of `r`adiance hints `s`hadow `m`ap: higher is more blurred
VAR(rhclipgrid, 0, 1, 1);
VARF(rhcache, 0, 1, 1, cleanupradiancehints());
VARF(rhforce, 0, 0, 1, cleanupradiancehints());
VAR(rsmcull, 0, 1, 1);
VARFP(rhtaps, 0, 20, 32, cleanupradiancehints()); //`r`adiance `h`ints `taps`: number of sample points for global illumination
VAR(rhdyntex, 0, 0, 1);
VAR(rhdynmm, 0, 0, 1);
VARFR(gidist, 0, 384, 1024, { clearradiancehintscache(); cleardeferredlightshaders(); if(!gidist) cleanupradiancehints(); });
FVARFR(giscale, 0, 1.5f, 1e3f, { cleardeferredlightshaders(); if(!giscale) cleanupradiancehints(); }); //`g`lobal `i`llumination `scale`
FVARR(giaoscale, 0, 3, 1e3f); //`g`lobal `i`llumination `a`mbient `o`cclusion `scale`: scale of ambient occlusion (corner darkening) on globally illuminated surfaces
VARFP(gi, 0, 1, 1, { cleardeferredlightshaders(); cleanupradiancehints(); }); //`g`lobal `i`llumination toggle: 0 disables global illumination

VAR(debugrsm, 0, 0, 2); //displays the `r`adiance hints `s`hadow `m`ap in the bottom right of the screen; 1 for view from sun pos, 2 for view from sun pos, normal map
void viewrsm()
{
    int w = min(hudw, hudh)/2,
        h = (w*hudh)/hudw,
        x = hudw-w,
        y = hudh-h;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, debugrsm == 2 ? rsmnormaltex : rsmcolortex);
    debugquad(x, y, w, h, 0, 0, rsmsize, rsmsize);
}

VAR(debugrh, -1, 0, rhmaxsplits*(rhmaxgrid + 2));
void viewrh()
{
    int w = min(hudw, hudh)/2,
        h = (w*hudh)/hudw,
        x = hudw-w,
        y = hudh-h;
    gle::colorf(1, 1, 1);
    if(debugrh < 0 && rhrect)
    {
        SETSHADER(hudrect);
        glBindTexture(GL_TEXTURE_RECTANGLE, rhtex[5]);
        float tw = (rhgrid+2*rhborder)*(rhgrid+2*rhborder),
              th = (rhgrid+2*rhborder)*rhsplits;
        gle::defvertex(2);
        gle::deftexcoord0(2);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,   y);   gle::attribf(0,  0);
        gle::attribf(x+w, y);   gle::attribf(tw, 0);
        gle::attribf(x,   y+h); gle::attribf(0,  th);
        gle::attribf(x+w, y+h); gle::attribf(tw, th);
        gle::end();
    }
    else
    {
        SETSHADER(hud3d);
        glBindTexture(GL_TEXTURE_3D, rhtex[1]);
        float z = (max(debugrh, 1)-1+0.5f)/static_cast<float>((rhgrid+2*rhborder)*rhsplits);
        gle::defvertex(2);
        gle::deftexcoord0(3);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,   y);   gle::attribf(0, 0, z);
        gle::attribf(x+w, y);   gle::attribf(1, 0, z);
        gle::attribf(x,   y+h); gle::attribf(0, 1, z);
        gle::attribf(x+w, y+h); gle::attribf(1, 1, z);
        gle::end();
    }
}

void reflectiveshadowmap::setup()
{
    getmodelmatrix();
    getprojmatrix();
    gencullplanes();
}

void reflectiveshadowmap::getmodelmatrix()
{
    model = viewmatrix;
    model.rotate_around_x(sunlightpitch*RAD);
    model.rotate_around_z((180-sunlightyaw)*RAD);
}

void reflectiveshadowmap::getprojmatrix()
{
    lightview = vec(sunlightdir).neg();
    // find z extent
    float minz = lightview.project_bb(worldmin, worldmax),
          maxz = lightview.project_bb(worldmax, worldmin),
          zmargin = max((maxz - minz)*rsmdepthmargin, 0.5f*(rsmdepthrange - (maxz - minz)));
    minz -= zmargin;
    maxz += zmargin;
    vec c;
    float radius = calcfrustumboundsphere(rhnearplane, rhfarplane, camera1->o, camdir, c);
    // compute the projected bounding box of the sphere
    vec tc;
    model.transform(c, tc);
    const float pradius = ceil((radius + gidist) * rsmpradiustweak),
                step = (2*pradius) / rsmsize;
    vec2 tcoff = vec2(tc).sub(pradius).div(step);
    tcoff.x = floor(tcoff.x);
    tcoff.y = floor(tcoff.y);
    center = vec(vec2(tcoff).mul(step).add(pradius), -0.5f*(minz + maxz));
    bounds = vec(pradius, pradius, 0.5f*(maxz - minz));

    scale = vec(1/step, 1/step, -1/(maxz - minz));
    offset = vec(-tcoff.x, -tcoff.y, -minz/(maxz - minz));

    proj.identity();
    proj.settranslation(2*offset.x/rsmsize - 1, 2*offset.y/rsmsize - 1, 2*offset.z - 1);
    proj.setscale(2*scale.x/rsmsize, 2*scale.y/rsmsize, 2*scale.z);
}

void reflectiveshadowmap::gencullplanes()
{
    matrix4 mvp;
    mvp.mul(proj, model);
    vec4 px = mvp.rowx(),
         py = mvp.rowy(),
         pw = mvp.roww();
    cull[0] = plane(vec4(pw).add(px)).normalize(); // left plane
    cull[1] = plane(vec4(pw).sub(px)).normalize(); // right plane
    cull[2] = plane(vec4(pw).add(py)).normalize(); // bottom plane
    cull[3] = plane(vec4(pw).sub(py)).normalize(); // top plane
}

void clearradiancehintscache()
{
    rh.clearcache();
    memset(rhclearmasks, 0, sizeof(rhclearmasks));
}

void radiancehints::updatesplitdist()
{
    float lambda = rhsplitweight,
          nd = rhnearplane,
          fd = rhfarplane,
          ratio = fd/nd;
    splits[0].nearplane = nd;
    for(int i = 1; i < rhsplits; ++i)
    {
        float si = i / static_cast<float>(rhsplits);
        splits[i].nearplane = lambda*(nd*pow(ratio, si)) + (1-lambda)*(nd + (fd - nd)*si);
        splits[i-1].farplane = splits[i].nearplane * 1.005f;
    }
    splits[rhsplits-1].farplane = fd;
}

void radiancehints::setup()
{
    updatesplitdist();

    for(int i = 0; i < rhsplits; ++i)
    {
        splitinfo &split = splits[i];

        vec c;
        float radius = calcfrustumboundsphere(split.nearplane, split.farplane, camera1->o, camdir, c);

        // compute the projected bounding box of the sphere
        const float pradius = ceil(radius * rhpradiustweak),
                    step = (2*pradius) / rhgrid;
        vec offset = vec(c).sub(pradius).div(step);
        offset.x = floor(offset.x);
        offset.y = floor(offset.y);
        offset.z = floor(offset.z);
        split.cached = split.bounds == pradius ? split.center : vec(-1e16f, -1e16f, -1e16f);
        split.center = vec(offset).mul(step).add(pradius);
        split.bounds = pradius;

        // modify mvp with a scale and offset
        // now compute the update model view matrix for this split
        split.scale = vec(1/(step*(rhgrid+2*rhborder)), 1/(step*(rhgrid+2*rhborder)), 1/(step*(rhgrid+2*rhborder)*rhsplits));
        split.offset = vec(-(offset.x-rhborder)/(rhgrid+2*rhborder), -(offset.y-rhborder)/(rhgrid+2*rhborder), (i - (offset.z-rhborder)/(rhgrid+2*rhborder))/static_cast<float>(rhsplits));
    }
}

void radiancehints::bindparams()
{
    float step = 2*splits[0].bounds/rhgrid;
    GLOBALPARAMF(rhnudge, rhnudge*step);
    static GlobalShaderParam rhtc("rhtc");
    vec4 *rhtcv = rhtc.reserve<vec4>(rhsplits);
    for(int i = 0; i < rhsplits; ++i)
    {
        splitinfo &split = splits[i];
        rhtcv[i] = vec4(vec(split.center).mul(-split.scale.x), split.scale.x);//split.bounds*(1 + rhborder*2*0.5f/rhgrid));
    }
    GLOBALPARAMF(rhbounds, 0.5f*(rhgrid + rhborder)/static_cast<float>(rhgrid + 2*rhborder));
}

bool useradiancehints()
{
    return !sunlight.iszero() && csmshadowmap && gi && giscale && gidist;
}

static inline void rhquad(float x1, float y1, float x2, float y2, float tx1, float ty1, float tx2, float ty2, float tz)
{
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(x2, y1); gle::attribf(tx2, ty1, tz);
    gle::attribf(x1, y1); gle::attribf(tx1, ty1, tz);
    gle::attribf(x2, y2); gle::attribf(tx2, ty2, tz);
    gle::attribf(x1, y2); gle::attribf(tx1, ty2, tz);
    gle::end();
}

static inline void rhquad(float dx1, float dy1, float dx2, float dy2, float dtx1, float dty1, float dtx2, float dty2, float dtz,
                          float px1, float py1, float px2, float py2, float ptx1, float pty1, float ptx2, float pty2, float ptz)
{
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(dx2, dy1); gle::attribf(dtx2, dty1, dtz);
        gle::attribf(px2, py1); gle::attribf(ptx2, pty1, ptz);
    gle::attribf(dx1, dy1); gle::attribf(dtx1, dty1, dtz);
        gle::attribf(px1, py1); gle::attribf(ptx1, pty1, ptz);
    gle::attribf(dx1, dy2); gle::attribf(dtx1, dty2, dtz);
        gle::attribf(px1, py2); gle::attribf(ptx1, pty2, ptz);
    gle::attribf(dx2, dy2); gle::attribf(dtx2, dty2, dtz);
        gle::attribf(px2, py2); gle::attribf(ptx2, pty2, ptz);
    gle::attribf(dx2, dy1); gle::attribf(dtx2, dty1, dtz);
        gle::attribf(px2, py1); gle::attribf(ptx2, pty1, ptz);
    gle::end();
}

void radiancehints::renderslices()
{
    int sw = rhgrid+2*rhborder,
        sh = rhgrid+2*rhborder;
    glBindFramebuffer_(GL_FRAMEBUFFER, rhfbo);
    if(!rhrect)
    {
        glViewport(0, 0, sw, sh);
        if(rhcache)
        {
            for(int i = 0; i < 4; ++i)
            {
                swap(rhtex[i], rhtex[i+4]);
            }
            uint clearmasks[rhmaxsplits][(rhmaxgrid+2+31)/32];
            memcpy(clearmasks, rhclearmasks[0], sizeof(clearmasks));
            memcpy(rhclearmasks[0], rhclearmasks[1], sizeof(clearmasks));
            memcpy(rhclearmasks[1], clearmasks, sizeof(clearmasks));
        }
    }

    GLOBALPARAMF(rhatten, 1.0f/(gidist*gidist));
    GLOBALPARAMF(rsmspread, gidist*rsmspread*rsm.scale.x, gidist*rsmspread*rsm.scale.y);
    GLOBALPARAMF(rhaothreshold, splits[0].bounds/rhgrid);
    GLOBALPARAMF(rhaoatten, 1.0f/(gidist*rsmspread));
    GLOBALPARAMF(rhaoheight, gidist*rsmspread);

    matrix4 rsmtcmatrix;
    rsmtcmatrix.identity();
    rsmtcmatrix.settranslation(rsm.offset);
    rsmtcmatrix.setscale(rsm.scale);
    rsmtcmatrix.mul(rsm.model);
    GLOBALPARAM(rsmtcmatrix, rsmtcmatrix);

    matrix4 rsmworldmatrix;
    rsmworldmatrix.invert(rsmtcmatrix);
    GLOBALPARAM(rsmworldmatrix, rsmworldmatrix);

    glBindTexture(GL_TEXTURE_RECTANGLE, rsmdepthtex);
    glActiveTexture_(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, rsmcolortex);
    glActiveTexture_(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE, rsmnormaltex);
    if(rhborder)
    {
        for(int i = 0; i < 4; ++i)
        {
            glActiveTexture_(GL_TEXTURE3 + i);
            glBindTexture(GL_TEXTURE_3D, rhtex[i]);
        }
    }
    if(rhcache)
    {
        for(int i = 0; i < 4; ++i)
        {
            glActiveTexture_(GL_TEXTURE7 + i);
            glBindTexture(GL_TEXTURE_3D, rhtex[rhrect ? i : 4+i]);
        }
    }
    glActiveTexture_(GL_TEXTURE0);
    glClearColor(0.5f, 0.5f, 0.5f, 0);
    if(rhrect)
    {
        glEnable(GL_SCISSOR_TEST);
    }
    gle::defvertex(2);
    gle::deftexcoord0(3);

    bool prevcached = true;
    int cx = -1,
        cy = -1;
    for(int i = rhsplits; --i >= 0;) //reverse iterate through rhsplits
    {
        splitinfo &split = splits[i];
        float cellradius = split.bounds/rhgrid,
              step       = 2*cellradius,
              nudge      = rhnudge*2*splits[0].bounds/rhgrid + rhworldbias*step;
        vec cmin, cmax,
            dmin(1e16f, 1e16f, 1e16f),
            dmax(-1e16f, -1e16f, -1e16f),
            bmin(1e16f, 1e16f, 1e16f),
            bmax(-1e16f, -1e16f, -1e16f);
        for(int k = 0; k < 3; ++k)
        {
            cmin[k] = floor((worldmin[k] - nudge - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds;
            cmax[k] = ceil((worldmax[k] + nudge - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds;
        }
        if(prevdynmin.z < prevdynmax.z)
        {
            for(int k = 0; k < 3; ++k)
            {
                dmin[k] = min(dmin[k], static_cast<float>(floor((prevdynmin[k] - gidist - cellradius - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds));
                dmax[k] = max(dmax[k], static_cast<float>(ceil((prevdynmax[k] + gidist + cellradius - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds));
            }
        }
        if(dynmin.z < dynmax.z)
        {
            for(int k = 0; k < 3; ++k)
            {
                dmin[k] = min(dmin[k], static_cast<float>(floor((dynmin[k] - gidist - cellradius - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds));
                dmax[k] = max(dmax[k], static_cast<float>(ceil((dynmax[k] + gidist + cellradius - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds));
            }
        }
        if((rhrect || !rhcache || hasCI) && split.cached == split.center && (!rhborder || prevcached) && !rhforce &&
           (dmin.x > split.center.x + split.bounds || dmax.x < split.center.x - split.bounds ||
            dmin.y > split.center.y + split.bounds || dmax.y < split.center.y - split.bounds ||
            dmin.z > split.center.z + split.bounds || dmax.z < split.center.z - split.bounds))
        {
            if(rhrect || !rhcache || split.copied)
            {
                continue;
            }
            split.copied = true;
            for(int k = 0; k < 4; ++k)
            {
                glCopyImageSubData_(rhtex[4+k], GL_TEXTURE_3D, 0, 0, 0, i*sh, rhtex[k], GL_TEXTURE_3D, 0, 0, 0, i*sh, sw, sh, sh);
            }
            continue;
        }

        prevcached = false;
        split.copied = false;

        GLOBALPARAM(rhcenter, split.center);
        GLOBALPARAMF(rhbounds, split.bounds);
        GLOBALPARAMF(rhspread, cellradius);

        if(rhborder && i + 1 < rhsplits)
        {
            GLOBALPARAMF(bordercenter, 0.5f, 0.5f, static_cast<float>(i+1 + 0.5f)/rhsplits);
            GLOBALPARAMF(borderrange, 0.5f - 0.5f/(rhgrid+2), 0.5f - 0.5f/(rhgrid+2), (0.5f - 0.5f/(rhgrid+2))/rhsplits);
            GLOBALPARAMF(borderscale, rhgrid+2, rhgrid+2, (rhgrid+2)*rhsplits);

            splitinfo &next = splits[i+1];
            for(int k = 0; k < 3; ++k)
            {
                bmin[k] = floor((max(static_cast<float>(worldmin[k] - nudge), next.center[k] - next.bounds) - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds;
                bmax[k] = ceil((min(static_cast<float>(worldmax[k] + nudge), next.center[k] + next.bounds) - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds;
            }
        }

        uint clearmasks[(rhmaxgrid+2+31)/32];
        memset(clearmasks, 0xFF, sizeof(clearmasks));

        int sy = rhrect ? i*sh : 0;
        for(int j = sh; --j >= 0;) //note reverse iteration
        {
            int sx = rhrect ? j*sw : 0;

            #define BIND_SLICE do { \
                if(rhrect) \
                { \
                    glViewport(sx, sy, sw, sh); \
                    glScissor(sx, sy, sw, sh); \
                } \
                else \
                { \
                    glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, rhtex[0], 0, i*sh + j); \
                    glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_3D, rhtex[1], 0, i*sh + j); \
                    glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_3D, rhtex[2], 0, i*sh + j); \
                    glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_3D, rhtex[3], 0, i*sh + j); \
                } \
            } while(0)

            float x1  = split.center.x - split.bounds,
                  x2  = split.center.x + split.bounds,
                  y1  = split.center.y - split.bounds,
                  y2  = split.center.y + split.bounds,
                  z   = split.center.z - split.bounds + (j-rhborder+0.5f)*step,
                  vx1 = -1 + rhborder*2.0f/(rhgrid+2),
                  vx2 = 1 - rhborder*2.0f/(rhgrid+2),
                  vy1 = -1 + rhborder*2.0f/(rhgrid+2),
                  vy2 = 1 - rhborder*2.0f/(rhgrid+2),
                  tx1 = x1,
                  tx2 = x2,
                  ty1 = y1,
                  ty2 = y2;
            bool clipped = false;

            if(rhborder && i + 1 < rhsplits)
            {
                splitinfo &next = splits[i+1];
                float bx1  = x1-step,
                      bx2  = x2+step,
                      by1  = y1-step,
                      by2  = y2+step,
                      bz   = z,
                      bvx1 = -1,
                      bvx2 = 1,
                      bvy1 = -1,
                      bvy2 = 1,
                      btx1 = bx1,
                      btx2 = bx2,
                      bty1 = by1,
                      bty2 = by2;

                if(rhclipgrid)
                {
                    if(bz < bmin.z || bz > bmax.z)
                    {
                        goto noborder;
                    }
                    if(bx1 < bmin.x || bx2 > bmax.x || by1 < bmin.y || by2 > bmax.y)
                    {
                        btx1 = max(bx1, bmin.x);
                        btx2 = min(bx2, bmax.x);
                        bty1 = max(by1, bmin.y);
                        bty2 = min(by2, bmax.y);
                        if(btx1 > tx2 || bty1 > bty2)
                        {
                            goto noborder;
                        }
                        bvx1 += 2*(btx1 - bx1)/(bx2 - bx1);
                        bvx2 += 2*(btx2 - bx2)/(bx2 - bx1);
                        bvy1 += 2*(bty1 - by1)/(by2 - by1);
                        bvy2 += 2*(bty2 - by2)/(by2 - by1);
                        clipped = true;
                    }
                }
                btx1 = btx1*next.scale.x + next.offset.x;
                btx2 = btx2*next.scale.x + next.offset.x;
                bty1 = bty1*next.scale.y + next.offset.y;
                bty2 = bty2*next.scale.y + next.offset.y;
                bz = bz*next.scale.z + next.offset.z;
                BIND_SLICE;
                if(clipped)
                {
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                SETSHADER(radiancehintsborder);
                rhquad(bvx1, bvy1, bvx2, bvy2, btx1, bty1, btx2, bty2, bz);
                clearmasks[j/32] &= ~(1 << (j%32));
            }

        noborder:
            if(j < rhborder || j >= rhgrid + rhborder)
            {
            skipped:
                if(clearmasks[j/32] & (1 << (j%32)) && (!rhrect || cx < 0) && !(rhclearmasks[0][i][j/32] & (1 << (j%32))))
                {
                    BIND_SLICE;
                    glClear(GL_COLOR_BUFFER_BIT);
                    cx = sx;
                    cy = sy;
                }
                continue;
            }

            if(rhclipgrid)
            {
                if(z < cmin.z || z > cmax.z)
                {
                    goto skipped;
                }
                if(x1 < cmin.x || x2 > cmax.x || y1 < cmin.y || y2 > cmax.y)
                {
                    tx1 = max(x1, cmin.x);
                    tx2 = min(x2, cmax.x);
                    ty1 = max(y1, cmin.y);
                    ty2 = min(y2, cmax.y);
                    if(tx1 > tx2 || ty1 > ty2)
                    {
                        goto skipped;
                    }
                    vx1 += 2*rhgrid/static_cast<float>(sw)*(tx1 - x1)/(x2 - x1);
                    vx2 += 2*rhgrid/static_cast<float>(sw)*(tx2 - x2)/(x2 - x1);
                    vy1 += 2*rhgrid/static_cast<float>(sh)*(ty1 - y1)/(y2 - y1);
                    vy2 += 2*rhgrid/static_cast<float>(sh)*(ty2 - y2)/(y2 - y1);
                    clipped = true;
                }
            }
            if(clearmasks[j/32] & (1 << (j%32)))
            {
                BIND_SLICE;
                if(clipped || (rhborder && i + 1 >= rhsplits))
                {
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                clearmasks[j/32] &= ~(1 << (j%32));
            }

            if(rhcache && z > split.cached.z - split.bounds && z < split.cached.z + split.bounds)
            {
                float px1 = max(tx1, split.cached.x - split.bounds),
                      px2 = min(tx2, split.cached.x + split.bounds),
                      py1 = max(ty1, split.cached.y - split.bounds),
                      py2 = min(ty2, split.cached.y + split.bounds);
                if(px1 < px2 && py1 < py2)
                {
                    float pvx1 = -1 + rhborder*2.0f/(rhgrid+2) + 2*rhgrid/static_cast<float>(sw)*(px1 - x1)/(x2 - x1),
                          pvx2 =  1 - rhborder*2.0f/(rhgrid+2) + 2*rhgrid/static_cast<float>(sw)*(px2 - x2)/(x2 - x1),
                          pvy1 = -1 + rhborder*2.0f/(rhgrid+2) + 2*rhgrid/static_cast<float>(sh)*(py1 - y1)/(y2 - y1),
                          pvy2 =  1 - rhborder*2.0f/(rhgrid+2) + 2*rhgrid/static_cast<float>(sh)*(py2 - y2)/(y2 - y1),
                          ptx1 = (px1 + split.center.x - split.cached.x)*split.scale.x + split.offset.x,
                          ptx2 = (px2 + split.center.x - split.cached.x)*split.scale.x + split.offset.x,
                          pty1 = (py1 + split.center.y - split.cached.y)*split.scale.y + split.offset.y,
                          pty2 = (py2 + split.center.y - split.cached.y)*split.scale.y + split.offset.y,
                          pz = (z + split.center.z - split.cached.z)*split.scale.z + split.offset.z;
                    if(px1 != tx1 || px2 != tx2 || py1 != ty1 || py2 != ty2)
                    {
                        radiancehintsshader->set();
                        rhquad(pvx1, pvy1, pvx2, pvy2, px1, py1, px2, py2, z,
                                vx1,  vy1,  vx2,  vy2, tx1, ty1, tx2, ty2, z);
                    }
                    if(z > dmin.z && z < dmax.z)
                    {
                        float dx1 = max(px1, dmin.x), dx2 = min(px2, dmax.x),
                              dy1 = max(py1, dmin.y), dy2 = min(py2, dmax.y);
                        if(dx1 < dx2 && dy1 < dy2)
                        {
                            float dvx1 = -1 + rhborder*2.0f/(rhgrid+2) + 2*rhgrid/static_cast<float>(sw)*(dx1 - x1)/(x2 - x1),
                                  dvx2 = 1 - rhborder*2.0f/(rhgrid+2) + 2*rhgrid/static_cast<float>(sw)*(dx2 - x2)/(x2 - x1),
                                  dvy1 = -1 + rhborder*2.0f/(rhgrid+2) + 2*rhgrid/static_cast<float>(sh)*(dy1 - y1)/(y2 - y1),
                                  dvy2 = 1 - rhborder*2.0f/(rhgrid+2) + 2*rhgrid/static_cast<float>(sh)*(dy2 - y2)/(y2 - y1),
                                  dtx1 = (dx1 + split.center.x - split.cached.x)*split.scale.x + split.offset.x,
                                  dtx2 = (dx2 + split.center.x - split.cached.x)*split.scale.x + split.offset.x,
                                  dty1 = (dy1 + split.center.y - split.cached.y)*split.scale.y + split.offset.y,
                                  dty2 = (dy2 + split.center.y - split.cached.y)*split.scale.y + split.offset.y,
                                  dz = (z + split.center.z - split.cached.z)*split.scale.z + split.offset.z;
                            if(dx1 != px1 || dx2 != px2 || dy1 != py1 || dy2 != py2)
                            {
                                SETSHADER(radiancehintscached);
                                rhquad(dvx1, dvy1, dvx2, dvy2, dtx1, dty1, dtx2, dty2, dz,
                                       pvx1, pvy1, pvx2, pvy2, ptx1, pty1, ptx2, pty2, pz);
                            }
                            radiancehintsshader->set();
                            rhquad(dvx1, dvy1, dvx2, dvy2, dx1, dy1, dx2, dy2, z);
                            goto maskslice;
                        }
                    }
                    SETSHADER(radiancehintscached);
                    rhquad(pvx1, pvy1, pvx2, pvy2, ptx1, pty1, ptx2, pty2, pz);
                    goto maskslice;
                }
            }
            radiancehintsshader->set();
            rhquad(vx1, vy1, vx2, vy2, tx1, ty1, tx2, ty2, z);
        maskslice:
            if(i)
            {
                continue;
            }
            if(gle::attribbuf.empty())
            {
                continue;
            }
            SETSHADER(radiancehintsdisable);
            if(rhborder)
            {
                glScissor(sx + rhborder, sy + rhborder, sw - 2*rhborder, sh - 2*rhborder);
                if(!rhrect)
                {
                    glEnable(GL_SCISSOR_TEST);
                }
            }
            gle::defvertex(2);
            gle::begin(GL_QUADS);
            gle::end();
            if(rhborder && !rhrect)
            {
                glDisable(GL_SCISSOR_TEST);
            }
            gle::defvertex(2);
            gle::deftexcoord0(3);
        }
        if(rhrect)
        {
            for(int k = 0; k < 4; ++k)
            {
                glReadBuffer(GL_COLOR_ATTACHMENT0+k);
                glBindTexture(GL_TEXTURE_3D, rhtex[k]);
                for(int j = 0; j < sh; ++j)
                {
                    if(clearmasks[j/32] & (1 << (j%32)))
                    {
                        if(!(rhclearmasks[0][i][j/32] & (1 << (j%32))))
                        {
                            glCopyTexSubImage3D_(GL_TEXTURE_3D, 0, 0, 0, sy+j, cx, cy, sw, sh);
                        }
                        continue;
                    }
                    glCopyTexSubImage3D_(GL_TEXTURE_3D, 0, 0, 0, sy+j, j*sw, sy, sw, sh);
                }
            }
        }
        memcpy(rhclearmasks[0][i], clearmasks, sizeof(clearmasks));
    }
    if(rhrect)
    {
        glDisable(GL_SCISSOR_TEST);
    }
}

void renderradiancehints()
{
    if(rhinoq && !inoq && shouldworkinoq())
    {
        return;
    }
    if(!useradiancehints())
    {
        return;
    }
    timer *rhcputimer = begintimer("radiance hints", false);
    timer *rhtimer = begintimer("radiance hints");

    rh.setup();
    rsm.setup();

    shadowmapping = ShadowMap_Reflect;
    shadowside = 0;
    shadoworigin = vec(0, 0, 0);
    shadowdir = rsm.lightview;
    shadowbias = rsm.lightview.project_bb(worldmin, worldmax);
    shadowradius = fabs(rsm.lightview.project_bb(worldmax, worldmin));

    findshadowvas();
    findshadowmms();

    shadowmaskbatchedmodels(false);
    batchshadowmapmodels();

    rh.prevdynmin = rh.dynmin;
    rh.prevdynmax = rh.dynmax;
    rh.dynmin = vec(1e16f, 1e16f, 1e16f);
    rh.dynmax = vec(-1e16f, -1e16f, -1e16f);
    if(rhdyntex)
    {
        dynamicshadowvabounds(1<<shadowside, rh.dynmin, rh.dynmax);
    }
    if(rhdynmm)
    {
        batcheddynamicmodelbounds(1<<shadowside, rh.dynmin, rh.dynmax);
    }
    if(rhforce || rh.prevdynmin.z < rh.prevdynmax.z || rh.dynmin.z < rh.dynmax.z || !rh.allcached())
    {
        if(inoq)
        {
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(GL_TRUE);
        }
        glBindFramebuffer_(GL_FRAMEBUFFER, rsmfbo);
        shadowmatrix.mul(rsm.proj, rsm.model);
        GLOBALPARAM(rsmmatrix, shadowmatrix);
        GLOBALPARAMF(rsmdir, -rsm.lightview.x, -rsm.lightview.y, -rsm.lightview.z);

        glViewport(0, 0, rsmsize, rsmsize);
        glClearColor(0, 0, 0, 0);
        glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

        renderrsmgeom(rhdyntex!=0);
        rendershadowmodelbatches(rhdynmm!=0);
        rh.renderslices();
        if(inoq)
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, msaasamples ? msfbo : gfbo);
            glViewport(0, 0, vieww, viewh);

            glFlush();
        }
    }

    clearbatchedmapmodels();
    shadowmapping = 0;
    endtimer(rhtimer);
    endtimer(rhcputimer);
}

/* rendergl.cpp: core opengl rendering stuff
 *
 * rendergl.cpp handles the main rendering functions, which render the scene
 * using OpenGL features aliased in this file. This file also handles the
 * position of the camera and the projection frustum handling.
 *
 * While this file does not handle light and texture rendering, it does handle
 * the simple world depth fog in libprimis.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "aa.h"
#include "grass.h"
#include "hdr.h"
#include "hud.h"
#include "octarender.h"
#include "postfx.h"
#include "radiancehints.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "renderparticles.h"
#include "rendersky.h"
#include "rendertimers.h"
#include "renderva.h"
#include "renderwindow.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"
#include "water.h"

#include "world/material.h"
#include "world/octaedit.h"
#include "world/octaworld.h"
#include "world/raycube.h"
#include "world/world.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/input.h"
#include "interface/menus.h"
#include "interface/ui.h"

bool hasFBMSBS = false,
     hasTQ     = false,
     hasDBT    = false,
     hasEGPU4  = false,
     hasES3    = false,
     hasCI     = false;

VAR(outline, 0, 0, 1); //vertex/edge highlighting in edit mode

//read-only info for gl debugging
VAR(glversion, 1, 0, 0);
VAR(glslversion, 1, 0, 0);

// GL_EXT_framebuffer_blit
PFNGLBLITFRAMEBUFFERPROC         glBlitFramebuffer_         = nullptr;

// GL_EXT_framebuffer_multisample
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample_ = nullptr;

// GL_ARB_texture_multisample
PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample_ = nullptr;

// OpenGL 1.3
#ifdef WIN32
PFNGLACTIVETEXTUREPROC    glActiveTexture_    = nullptr;

PFNGLBLENDEQUATIONEXTPROC glBlendEquation_ = nullptr;
PFNGLBLENDCOLOREXTPROC    glBlendColor_    = nullptr;

PFNGLTEXIMAGE3DPROC        glTexImage3D_        = nullptr;
PFNGLTEXSUBIMAGE3DPROC     glTexSubImage3D_     = nullptr;
PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D_ = nullptr;

PFNGLCOMPRESSEDTEXIMAGE3DPROC    glCompressedTexImage3D_    = nullptr;
PFNGLCOMPRESSEDTEXIMAGE2DPROC    glCompressedTexImage2D_    = nullptr;
PFNGLCOMPRESSEDTEXIMAGE1DPROC    glCompressedTexImage1D_    = nullptr;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D_ = nullptr;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D_ = nullptr;
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D_ = nullptr;
PFNGLGETCOMPRESSEDTEXIMAGEPROC   glGetCompressedTexImage_   = nullptr;

PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements_ = nullptr;
#endif

// GL_EXT_depth_bounds_test
PFNGLDEPTHBOUNDSEXTPROC glDepthBounds_ = nullptr;

// GL_ARB_copy_image
PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData_ = nullptr;

void masktiles(uint *tiles, float sx1, float sy1, float sx2, float sy2)
{
    int tx1, ty1, tx2, ty2;
    calctilebounds(sx1, sy1, sx2, sy2, tx1, ty1, tx2, ty2);
    for(int ty = ty1; ty < ty2; ty++) tiles[ty] |= ((1<<(tx2-tx1))-1)<<tx1;
}

static void *getprocaddress(const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

VAR(glerr, 0, 0, 1);

void glerror(const char *file, int line, GLenum error)
{
    const char *desc = "unknown";
    switch(error)
    {
        case GL_NO_ERROR:
        {
            desc = "no error";
            break;
        }
        case GL_INVALID_ENUM:
        {
            desc = "invalid enum";
            break;
        }
        case GL_INVALID_VALUE:
        {
            desc = "invalid value";
            break;
        }
        case GL_INVALID_OPERATION:
        {
            desc = "invalid operation";
            break;
        }
        case GL_STACK_OVERFLOW:
        {
            desc = "stack overflow";
            break;
        }
        case GL_STACK_UNDERFLOW:
        {
            desc = "stack underflow";
            break;
        }
        case GL_OUT_OF_MEMORY:
        {
            desc = "out of memory";
            break;
        }
    }
    std::printf("GL error: %s:%d: %s (%x)\n", file, line, desc, error);
}

VAR(intel_texalpha_bug, 0, 0, 1);
VAR(mesa_swap_bug, 0, 0, 1);
VAR(usetexgather, 1, 0, 0);
VAR(maxdrawbufs, 1, 0, 0);
VAR(maxdualdrawbufs, 1, 0, 0);

VAR(debugexts, 0, 0, 1);

static std::unordered_set<std::string> glexts;

static void parseglexts()
{
    GLint numexts = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numexts);
    for(int i = 0; i < numexts; ++i)
    {
        //cast from uchar * to char *
        const char *ext = reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, i));
        glexts.insert(ext);
    }
}

static bool hasext(const char *ext)
{
    return glexts.find(ext)!=glexts.end();
}

static bool checkdepthtexstencilrb()
{
    uint w = 256,
         h = 256;
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint depthtex = 0;
    glGenTextures(1, &depthtex);
    createtexture(depthtex, w, h, nullptr, 3, 0, GL_DEPTH_COMPONENT24, GL_TEXTURE_RECTANGLE);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_RECTANGLE, depthtex, 0);

    GLuint stencilrb = 0;
    glGenRenderbuffers(1, &stencilrb);
    glBindRenderbuffer(GL_RENDERBUFFER, stencilrb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilrb);

    bool supported = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &depthtex);
    glDeleteRenderbuffers(1, &stencilrb);

    return supported;
}

void gl_checkextensions()
{
    bool mesa   = false,
         intel  = false,
         amd    = false,
         nvidia = false;
    const char *vendor   = reinterpret_cast<const char *>(glGetString(GL_VENDOR)),
               *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER)),
               *version  = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    conoutf(Console_Init, "Renderer: %s (%s)", renderer, vendor);
    conoutf(Console_Init, "Driver: %s", version);

    if(!renderer || !vendor || !version)
    {
        fatal("Could not get rendering context information!");
    }
    if(std::strstr(renderer, "Mesa") || std::strstr(version, "Mesa"))
    {
        mesa = true;
        if(std::strstr(renderer, "Intel"))
        {
            intel = true;
        }
    }
    else if(std::strstr(vendor, "NVIDIA"))
    {
        nvidia = true;
    }
    else if(std::strstr(vendor, "ATI") || std::strstr(vendor, "Advanced Micro Devices"))
    {
        amd = true;
    }
    else if(std::strstr(vendor, "Intel"))
    {
        intel = true;
    }

    uint glmajorversion, glminorversion;
    if(std::sscanf(version, " %u.%u", &glmajorversion, &glminorversion) != 2)
    {
        glversion = 100; //__really__ legacy systems (which won't run anyways)
    }
    else
    {
        glversion = glmajorversion*100 + glminorversion*10;
    }
    if(glversion < 400)
    {
        fatal("OpenGL 4.0 or greater is required!");
    }

    const char *glslstr = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    conoutf(Console_Init, "GLSL: %s", glslstr ? glslstr : "unknown");

    uint glslmajorversion, glslminorversion;
    if(glslstr && std::sscanf(glslstr, " %u.%u", &glslmajorversion, &glslminorversion) == 2)
    {
        glslversion = glslmajorversion*100 + glslminorversion;
    }
    if(glslversion < 400)
    {
        fatal("GLSL 4.00 or greater is required!");
    }
    parseglexts();
    GLint texsize = 0,
          texunits = 0,
          vtexunits = 0,
          cubetexsize = 0,
          drawbufs = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texsize);
    hwtexsize = texsize;
    if(hwtexsize < 4096)
    {
        fatal("Large texture support is required!");
    }
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texunits);
    hwtexunits = texunits;
    if(hwtexunits < 16)
    {
        fatal("Hardware does not support at least 16 texture units.");
    }
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &vtexunits);
    hwvtexunits = vtexunits;
    if(hwvtexunits < 16)
    {
        fatal("Hardware does not support at least 16 vertex texture units.");
    }
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &cubetexsize);
    hwcubetexsize = cubetexsize;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &drawbufs);
    maxdrawbufs = drawbufs;
    if(maxdrawbufs < 4)
    {
        fatal("Hardware does not support at least 4 draw buffers.");
    }
    //OpenGL 3.0

    if(hasext("GL_EXT_gpu_shader4"))
    {
        hasEGPU4 = true;
        if(debugexts)
        {
            conoutf(Console_Init, "Using GL_EXT_gpu_shader4 extension.");
        }
    }
    glRenderbufferStorageMultisample_ = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)getprocaddress("glRenderbufferStorageMultisample");

    //OpenGL 3.2
    glTexImage2DMultisample_ = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)getprocaddress("glTexImage2DMultisample");
    if(hasext("GL_EXT_framebuffer_multisample_blit_scaled"))
    {
        hasFBMSBS = true;
        if(debugexts)
        {
            conoutf(Console_Init, "Using GL_EXT_framebuffer_multisample_blit_scaled extension.");
        }
    }
    //OpenGL 3.3
    if(hasext("GL_EXT_texture_filter_anisotropic"))
    {
        GLint val = 0;
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &val);
        hwmaxaniso = val;
        if(debugexts)
        {
            conoutf(Console_Init, "Using GL_EXT_texture_filter_anisotropic extension.");
        }
    }
    else
    {
        fatal("Anisotropic filtering support is required!");
    }
    if(hasext("GL_EXT_depth_bounds_test"))
    {
        glDepthBounds_ = (PFNGLDEPTHBOUNDSEXTPROC) getprocaddress("glDepthBoundsEXT");
        hasDBT = true;
        if(debugexts)
        {
            conoutf(Console_Init, "Using GL_EXT_depth_bounds_test extension.");
        }
    }
    GLint dualbufs = 0;
    glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, &dualbufs);
    maxdualdrawbufs = dualbufs;
    usetexgather = !intel && !nvidia ? 2 : 1;
    //OpenGL 4.x
    if(glversion >= 430 || hasext("GL_ARB_ES3_compatibility"))
    {
        hasES3 = true;
        if(glversion < 430 && debugexts)
        {
            conoutf(Console_Init, "Using GL_ARB_ES3_compatibility extension.");
        }
    }

    if(glversion >= 430 || hasext("GL_ARB_copy_image"))
    {
        glCopyImageSubData_ = (PFNGLCOPYIMAGESUBDATAPROC)getprocaddress("glCopyImageSubData");

        hasCI = true;
        if(glversion < 430 && debugexts)
        {
            conoutf(Console_Init, "Using GL_ARB_copy_image extension.");
        }
    }
    else if(hasext("GL_NV_copy_image"))
    {
        glCopyImageSubData_ = (PFNGLCOPYIMAGESUBDATAPROC)getprocaddress("glCopyImageSubDataNV");

        hasCI = true;
        if(debugexts)
        {
            conoutf(Console_Init, "Using GL_NV_copy_image extension.");
        }
    }

    if(amd)
    {
        msaalineardepth = glineardepth = 1; // reading back from depth-stencil still buggy on newer cards, and requires stencil for MSAA
    }
    else if(nvidia) //no errata on nvidia cards (yet)
    {
    }
    else if(intel)
    {
        smgather = 1; // native shadow filter is slow
        if(mesa)
        {
            batchsunlight = 0; // causes massive slowdown in linux driver
            msaalineardepth = 1; // MSAA depth texture access is buggy and resolves are slow
        }
        else
        {
            // causes massive slowdown in windows driver if reading depth-stencil texture
            if(checkdepthtexstencilrb())
            {
                gdepthstencil = 1;
                gstencil = 1;
            }
            // sampling alpha by itself from a texture generates garbage on Intel drivers on Windows
            intel_texalpha_bug = 1;
        }
    }
    if(mesa)
    {
        mesa_swap_bug = 1;
    }
    tqaaresolvegather = 1;
}

/* glext(): checks for existence of glext
 *
 * returns to console 1 if hashtable glexts contains glext (with the name passed)
 * and returns 0 otherwise
 *
 * glexts is a global variable
 */
void glext(char *ext)
{
    intret(hasext(ext) ? 1 : 0);
}


void gl_resize()
{
    gl_setupframe();
    glViewport(0, 0, hudw(), hudh());
}

void gl_init()
{
    glerror();

    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
    glClearStencil(0);
    glDepthFunc(GL_LESS);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, ~0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glEnable(GL_LINE_SMOOTH);
    //glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    gle::setup();
    setupshaders();

    glerror();

    gl_resize();
}

VAR(wireframe, 0, 0, 1);

vec worldpos;

//these three cam() functions replace global variables that previously tracked their respective transforms of cammatrix
vec camdir()
{
    vec out;
    cammatrix.transposedtransformnormal(vec(viewmatrix.b), out);
    return out;
}

vec camright()
{
    vec out;
    cammatrix.transposedtransformnormal(vec(viewmatrix.a).neg(), out);
    return out;
}

vec camup()
{
    vec out;
    cammatrix.transposedtransformnormal(vec(viewmatrix.c), out);
    return out;
}

static void setcammatrix()
{
    // move from RH to Z-up LH quake style worldspace
    cammatrix = viewmatrix;
    cammatrix.rotate_around_y(camera1->roll/RAD);
    cammatrix.rotate_around_x(camera1->pitch/-RAD);
    cammatrix.rotate_around_z(camera1->yaw/-RAD);
    cammatrix.translate(vec(camera1->o).neg());

    if(!drawtex)
    {
        if(raycubepos(camera1->o, camdir(), worldpos, 0, Ray_ClipMat|Ray_SkipFirst) == -1)
        {
            worldpos = camdir().mul(2*rootworld.mapsize()).add(camera1->o); // if nothing is hit, just far away in the view direction
        }
    }
}

void setcamprojmatrix(bool init = true, bool flush = false)
{
    if(init)
    {
        setcammatrix();
    }
    jitteraa();
    camprojmatrix.muld(projmatrix, cammatrix);
    GLOBALPARAM(camprojmatrix, camprojmatrix);
    GLOBALPARAM(lineardepthscale, projmatrix.lineardepthscale()); //(invprojmatrix.c.z, invprojmatrix.d.z));
    if(flush && Shader::lastshader)
    {
        Shader::lastshader->flushparams();
    }
}

matrix4 hudmatrix, hudmatrixstack[64];
int hudmatrixpos = 0;

void resethudmatrix()
{
    hudmatrixpos = 0;
    GLOBALPARAM(hudmatrix, hudmatrix);
}

void pushhudmatrix()
{
    if(hudmatrixpos >= 0 && hudmatrixpos < static_cast<int>(sizeof(hudmatrixstack)/sizeof(hudmatrixstack[0])))
    {
        hudmatrixstack[hudmatrixpos] = hudmatrix;
    }
    ++hudmatrixpos;
}

void flushhudmatrix(bool flushparams)
{
    GLOBALPARAM(hudmatrix, hudmatrix);
    if(flushparams && Shader::lastshader)
    {
        Shader::lastshader->flushparams();
    }
}

void pophudmatrix(bool flush, bool flushparams)
{
    --hudmatrixpos;
    if(hudmatrixpos >= 0 && hudmatrixpos < static_cast<int>(sizeof(hudmatrixstack)/sizeof(hudmatrixstack[0])))
    {
        hudmatrix = hudmatrixstack[hudmatrixpos];
        if(flush)
        {
            flushhudmatrix(flushparams);
        }
    }
}

void pushhudscale(float scale)
{
    pushhudmatrix();
    hudmatrix.scale(scale, scale, 1);
    flushhudmatrix();
}

void pushhudtranslate(float tx, float ty, float sx, float sy)
{
    if(!sy)
    {
        sy = sx;
    }
    pushhudmatrix();
    hudmatrix.translate(tx, ty, 0);
    if(sy)
    {
        hudmatrix.scale(sx, sy, 1);
    }
    flushhudmatrix();
}

float curfov, aspect, fovy;
static float curavatarfov;
int farplane;
VARP(zoominvel, 0, 40, 500);
VARP(zoomoutvel, 0, 50, 500);
VARP(zoomfov, 10, 42, 90);
VARP(fov, 10, 100, 150);
VAR(avatarzoomfov, 1, 1, 1);
VAR(avatarfov, 10, 40, 100);
FVAR(avatardepth, 0, 0.7f, 1);
FVARNP(aspect, forceaspect, 0, 0, 1e3f);

static float zoomprogress = 0;
VAR(zoom, -1, 0, 1);

void disablezoom()
{
    zoom = 0;
    zoomprogress = 0;
}

void computezoom()
{
    if(!zoom)
    {
        zoomprogress = 0;
        curfov = fov;
        curavatarfov = avatarfov;
        return;
    }
    if(zoom > 0)
    {
        zoomprogress = zoominvel ? std::min(zoomprogress + static_cast<float>(elapsedtime) / zoominvel, 1.0f) : 1;
    }
    else
    {
        zoomprogress = zoomoutvel ? std::max(zoomprogress - static_cast<float>(elapsedtime) / zoomoutvel, 0.0f) : 0;
        if(zoomprogress <= 0)
        {
            zoom = 0;
        }
    }
    curfov = zoomfov*zoomprogress + fov*(1 - zoomprogress);
    curavatarfov = avatarzoomfov*zoomprogress + avatarfov*(1 - zoomprogress);
}

FVARP(zoomsens, 1e-4f, 4.5f, 1e4f);
FVARP(zoomaccel, 0, 0, 1000);
VARP(zoomautosens, 0, 1, 1);
FVARP(sensitivity, 0.01f, 3, 100.f);
FVARP(sensitivityscale, 1e-4f, 100, 1e4f);
/* Sensitivity scales:
 * 100: Quake/Source (TF2, Q3, Apex, L4D)
 * 333: COD, Destiny, Overwatch, ~BL2/3
 * 400: Cube/RE
 */
VARP(invmouse, 0, 0, 1); //toggles inverting the mouse
FVARP(mouseaccel, 0, 0, 1000);

physent *camera1 = nullptr;
//used in iengine.h
bool detachedcamera = false;

bool isthirdperson()
{
    return player!=camera1 || detachedcamera;
}

void fixcamerarange()
{
    constexpr float maxpitch = 90.0f;
    if(camera1->pitch>maxpitch)
    {
        camera1->pitch = maxpitch;
    }
    if(camera1->pitch<-maxpitch)
    {
        camera1->pitch = -maxpitch;
    }
    while(camera1->yaw<0.0f)
    {
        camera1->yaw += 360.0f;
    }
    while(camera1->yaw>=360.0f)
    {
        camera1->yaw -= 360.0f;
    }
}

void modifyorient(float yaw, float pitch)
{
    camera1->yaw += yaw;
    camera1->pitch += pitch;
    fixcamerarange();
    if(camera1!=player && !detachedcamera)
    {
        player->yaw = camera1->yaw;
        player->pitch = camera1->pitch;
    }
}

void mousemove(int dx, int dy)
{
    float cursens  = sensitivity,
          curaccel = mouseaccel;
    if(zoom)
    {
        if(zoomautosens)
        {
            cursens  = static_cast<float>(sensitivity*zoomfov)/fov;
            curaccel = static_cast<float>(mouseaccel*zoomfov)/fov;
        }
        else
        {
            cursens = zoomsens;
            curaccel = zoomaccel;
        }
    }
    if(curaccel && curtime && (dx || dy))
    {
        cursens += curaccel * sqrtf(dx*dx + dy*dy)/curtime;
    }
    cursens /= (sensitivityscale/4); //hard factor of 4 for 40 dots/deg like Quake/Source/etc.
    modifyorient(dx*cursens, dy*cursens*(invmouse ? 1 : -1));
}

matrix4 cammatrix, projmatrix, camprojmatrix;

FVAR(nearplane, 0.01f, 0.54f, 2.0f);

vec calcavatarpos(const vec &pos, float dist)
{
    vec eyepos;
    cammatrix.transform(pos, eyepos);
    GLdouble ydist = nearplane * std::tan(curavatarfov/(2*RAD)),
             xdist = ydist * aspect;
    vec4<float> scrpos;
    scrpos.x = eyepos.x*nearplane/xdist;
    scrpos.y = eyepos.y*nearplane/ydist;
    scrpos.z = (eyepos.z*(farplane + nearplane) - 2*nearplane*farplane) / (farplane - nearplane);
    scrpos.w = -eyepos.z;

    vec worldpos = camprojmatrix.inverse().perspectivetransform(scrpos);
    vec dir = vec(worldpos).sub(camera1->o).rescale(dist);
    return dir.add(camera1->o);
}

void renderavatar(void (*hudfxn)())
{
    if(isthirdperson())
    {
        return;
    }
    matrix4 oldprojmatrix = nojittermatrix;
    projmatrix.perspective(curavatarfov, aspect, nearplane, farplane);
    projmatrix.scalez(avatardepth);
    setcamprojmatrix(false);

    enableavatarmask();
    hudfxn();
    disableavatarmask();

    projmatrix = oldprojmatrix;
    setcamprojmatrix(false);
}

FVAR(polygonoffsetfactor, -1e4f, -3.0f, 1e4f);
FVAR(polygonoffsetunits, -1e4f, -3.0f, 1e4f);
FVAR(depthoffset, -1e4f, 0.01f, 1e4f);

static matrix4 nooffsetmatrix;

void enablepolygonoffset(GLenum type)
{
    if(!depthoffset)
    {
        glPolygonOffset(polygonoffsetfactor, polygonoffsetunits);
        glEnable(type);
        return;
    }

    projmatrix = nojittermatrix;
    nooffsetmatrix = projmatrix;
    projmatrix.d.z += depthoffset * projmatrix.c.z;
    setcamprojmatrix(false, true);
}

void disablepolygonoffset(GLenum type)
{
    if(!depthoffset)
    {
        glDisable(type);
        return;
    }

    projmatrix = nooffsetmatrix;
    setcamprojmatrix(false, true);
}

bool calcspherescissor(const vec &center, float size, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2)
{
    //dim must be 0..2
    //dir should be +/- 1
    auto checkplane = [] (int dim, const float &dc, int dir, float focaldist, float &low, float &high, const float &cz, const float &drt, const vec &e)
    {
        float nzc = (cz*cz + 1) / (cz + dir*drt) - cz,
              pz = dc/(nzc*e[dim] - e.z);
        if(pz > 0)
        {
            float c = (focaldist)*nzc,
                  pc = pz*nzc;
            if(pc < e[dim])
            {
                low = c;
            }
            else if(pc > e[dim])
            {
                high = c;
            }
        }
    };

    vec e;
    cammatrix.transform(center, e);
    if(e.z > 2*size)
    {
        sx1 = sy1 = sz1 =  1;
        sx2 = sy2 = sz2 = -1;
        return false;
    }
    if(drawtex == Draw_TexMinimap)
    {
        vec dir(size, size, size);
        if(projmatrix.a.x < 0)
        {
            dir.x = -dir.x;
        }
        if(projmatrix.b.y < 0)
        {
            dir.y = -dir.y;
        }
        if(projmatrix.c.z < 0)
        {
            dir.z = -dir.z;
        }
        sx1 = std::max(projmatrix.a.x*(e.x - dir.x) + projmatrix.d.x, -1.0f);
        sx2 = std::min(projmatrix.a.x*(e.x + dir.x) + projmatrix.d.x, 1.0f);
        sy1 = std::max(projmatrix.b.y*(e.y - dir.y) + projmatrix.d.y, -1.0f);
        sy2 = std::min(projmatrix.b.y*(e.y + dir.y) + projmatrix.d.y, 1.0f);
        sz1 = std::max(projmatrix.c.z*(e.z - dir.z) + projmatrix.d.z, -1.0f);
        sz2 = std::min(projmatrix.c.z*(e.z + dir.z) + projmatrix.d.z, 1.0f);
        return sx1 < sx2 && sy1 < sy2 && sz1 < sz2;
    }
    float zzrr = e.z*e.z - size*size,
          dx = e.x*e.x + zzrr,
          dy = e.y*e.y + zzrr,
          focaldist = 1.0f/std::tan(fovy*0.5f/RAD);
    sx1 = sy1 = -1;
    sx2 = sy2 = 1;
    if(dx > 0)
    {
        float cz  = e.x/e.z,
              drt = sqrtf(dx)/size;
        checkplane(0, dx, -1, focaldist/aspect, sx1, sx2, cz, drt, e);
        checkplane(0, dx,  1, focaldist/aspect, sx1, sx2, cz, drt, e);
    }
    if(dy > 0)
    {
        float cz  = e.y/e.z,
              drt = sqrtf(dy)/size;
        checkplane(1, dy, -1, focaldist, sy1, sy2, cz, drt, e);
        checkplane(1, dy,  1, focaldist, sy1, sy2, cz, drt, e);
    }
    float z1 = std::min(e.z + size, -1e-3f - nearplane),
          z2 = std::min(e.z - size, -1e-3f - nearplane);
    sz1 = (z1*projmatrix.c.z + projmatrix.d.z) / (z1*projmatrix.c.w + projmatrix.d.w);
    sz2 = (z2*projmatrix.c.z + projmatrix.d.z) / (z2*projmatrix.c.w + projmatrix.d.w);
    return sx1 < sx2 && sy1 < sy2 && sz1 < sz2;
}

bool calcbbscissor(const ivec &bbmin, const ivec &bbmax, float &sx1, float &sy1, float &sx2, float &sy2)
{
    auto addxyscissor = [&] (const vec4<float> &p)
    {
        if(p.z >= -p.w)
        {
            float x = p.x / p.w,
                  y = p.y / p.w;
            sx1 = std::min(sx1, x);
            sy1 = std::min(sy1, y);
            sx2 = std::max(sx2, x);
            sy2 = std::max(sy2, y);
        }
    };

    vec4<float> v[8];
    sx1 = sy1 = 1;
    sx2 = sy2 = -1;
    camprojmatrix.transform(vec(bbmin.x, bbmin.y, bbmin.z), v[0]);
    addxyscissor(v[0]);
    camprojmatrix.transform(vec(bbmax.x, bbmin.y, bbmin.z), v[1]);
    addxyscissor(v[1]);
    camprojmatrix.transform(vec(bbmin.x, bbmax.y, bbmin.z), v[2]);
    addxyscissor(v[2]);
    camprojmatrix.transform(vec(bbmax.x, bbmax.y, bbmin.z), v[3]);
    addxyscissor(v[3]);
    camprojmatrix.transform(vec(bbmin.x, bbmin.y, bbmax.z), v[4]);
    addxyscissor(v[4]);
    camprojmatrix.transform(vec(bbmax.x, bbmin.y, bbmax.z), v[5]);
    addxyscissor(v[5]);
    camprojmatrix.transform(vec(bbmin.x, bbmax.y, bbmax.z), v[6]);
    addxyscissor(v[6]);
    camprojmatrix.transform(vec(bbmax.x, bbmax.y, bbmax.z), v[7]);
    addxyscissor(v[7]);
    if(sx1 > sx2 || sy1 > sy2)
    {
        return false;
    }
    for(int i = 0; i < 8; ++i)
    {
        const vec4<float> &p = v[i];
        if(p.z >= -p.w)
        {
            continue;
        }
        for(int j = 0; j < 3; ++j)
        {
            const vec4<float> &o = v[i^(1<<j)];
            if(o.z <= -o.w)
            {
                continue;
            }

            float t = (p.z + p.w)/(p.z + p.w - o.z - o.w),
                  w = p.w + t*(o.w - p.w),
                  x = (p.x + t*(o.x - p.x))/w,
                  y = (p.y + t*(o.y - p.y))/w;
            sx1 = std::min(sx1, x);
            sy1 = std::min(sy1, y);
            sx2 = std::max(sx2, x);
            sy2 = std::max(sy2, y);
        }
    }


    sx1 = std::max(sx1, -1.0f);
    sy1 = std::max(sy1, -1.0f);
    sx2 = std::min(sx2, 1.0f);
    sy2 = std::min(sy2, 1.0f);
    return true;
}

bool calcspotscissor(const vec &origin, float radius, const vec &dir, int spot, const vec &spotx, const vec &spoty, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2)
{
    static auto addxyzscissor = [] (const vec4<float> &p, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2)
    {
        if(p.z >= -p.w)
        {
            float x = p.x / p.w,
                  y = p.y / p.w,
                  z = p.z / p.w;
            sx1 = std::min(sx1, x);
            sy1 = std::min(sy1, y);
            sz1 = std::min(sz1, z);
            sx2 = std::max(sx2, x);
            sy2 = std::max(sy2, y);
            sz2 = std::max(sz2, z);
        }
    };
    float spotscale = radius * tan360(spot);
    vec up     = vec(spotx).mul(spotscale),
        right  = vec(spoty).mul(spotscale),
        center = vec(dir).mul(radius).add(origin);
    vec4<float> v[5];
    sx1 = sy1 = sz1 = 1;
    sx2 = sy2 = sz2 = -1;
    camprojmatrix.transform(vec(center).sub(right).sub(up), v[0]);
    addxyzscissor(v[0], sx1, sy1, sx2, sy2, sz1, sz2);
    camprojmatrix.transform(vec(center).add(right).sub(up), v[1]);
    addxyzscissor(v[1], sx1, sy1, sx2, sy2, sz1, sz2);
    camprojmatrix.transform(vec(center).sub(right).add(up), v[2]);
    addxyzscissor(v[2], sx1, sy1, sx2, sy2, sz1, sz2);
    camprojmatrix.transform(vec(center).add(right).add(up), v[3]);
    addxyzscissor(v[3], sx1, sy1, sx2, sy2, sz1, sz2);
    camprojmatrix.transform(origin, v[4]);
    addxyzscissor(v[4], sx1, sy1, sx2, sy2, sz1, sz2);

    static auto interpxyzscissor = [] (const vec4<float> &p, const vec4<float> &o, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1)
    {
        float t = (p.z + p.w)/(p.z + p.w - o.z - o.w),
              w = p.w + t*(o.w - p.w),
              x = (p.x + t*(o.x - p.x))/w,
              y = (p.y + t*(o.y - p.y))/w;
        sx1 = std::min(sx1, x);
        sy1 = std::min(sy1, y);
        sz1 = std::min(sz1, -1.0f);
        sx2 = std::max(sx2, x);
        sy2 = std::max(sy2, y);
    };

    if(sx1 > sx2 || sy1 > sy2 || sz1 > sz2)
    {
        return false;
    }
    for(int i = 0; i < 4; ++i)
    {
        const vec4<float> &p = v[i];
        if(p.z >= -p.w)
        {
            continue;
        }
        for(int j = 0; j < 2; ++j)
        {
            const vec4<float> &o = v[i^(1<<j)];
            if(o.z <= -o.w)
            {
                continue;
            }

            interpxyzscissor(p, o, sx1, sy1, sx2, sy2, sz1);
        }
        if(v[4].z > -v[4].w)
        {
            interpxyzscissor(p, v[4], sx1, sy1, sx2, sy2, sz1);
        }
    }
    if(v[4].z < -v[4].w)
    {
        for(int j = 0; j < 4; ++j)
        {
            const vec4<float> &o = v[j];
            if(o.z <= -o.w)
            {
                continue;
            }
            interpxyzscissor(v[4], o, sx1, sy1, sx2, sy2, sz1);
        }
    }

    sx1 = std::max(sx1, -1.0f);
    sy1 = std::max(sy1, -1.0f);
    sz1 = std::max(sz1, -1.0f);
    sx2 = std::min(sx2,  1.0f);
    sy2 = std::min(sy2,  1.0f);
    sz2 = std::min(sz2,  1.0f);
    return true;
}

static GLuint screenquadvbo = 0;

static void setupscreenquad()
{
    if(!screenquadvbo)
    {
        glGenBuffers(1, &screenquadvbo);
        gle::bindvbo(screenquadvbo);
        vec2 verts[4] = { vec2(1, -1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1) };
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        gle::clearvbo();
    }
}

static void cleanupscreenquad()
{
    if(screenquadvbo)
    {
        glDeleteBuffers(1, &screenquadvbo);
        screenquadvbo = 0;
    }
}

void screenquad()
{
    setupscreenquad();
    gle::bindvbo(screenquadvbo);
    gle::enablevertex();
    gle::vertexpointer(sizeof(vec2), nullptr, GL_FLOAT, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gle::disablevertex();
    gle::clearvbo();
}

//sets screentexcoord0,screentexcoord1 in glsl
static void setscreentexcoord(int i, float w, float h, float x = 0, float y = 0)
{
    static std::array<LocalShaderParam, 2> screentexcoord =
    {
        LocalShaderParam("screentexcoord0"),
        LocalShaderParam("screentexcoord1")
    };
    screentexcoord[i].setf(w*0.5f, h*0.5f, x + w*0.5f, y + std::fabs(h)*0.5f);
}

void screenquad(float sw, float sh)
{
    setscreentexcoord(0, sw, sh);
    screenquad();
}

void screenquad(float sw, float sh, float sw2, float sh2)
{
    setscreentexcoord(0, sw, sh);
    setscreentexcoord(1, sw2, sh2);
    screenquad();
}

void screenquadoffset(float x, float y, float w, float h)
{
    setscreentexcoord(0, w, h, x, y);
    screenquad();
}

// creates a hud quad for hudquad, debugquad
static void createhudquad(float x1, float y1, float x2, float y2, float sx1, float sy1, float sx2, float sy2) {
    gle::defvertex(2);
    gle::deftexcoord0();
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(x2, y1); gle::attribf(sx2, sy1);
    gle::attribf(x1, y1); gle::attribf(sx1, sy1);
    gle::attribf(x2, y2); gle::attribf(sx2, sy2);
    gle::attribf(x1, y2); gle::attribf(sx1, sy2);
    gle::end();
}

void hudquad(float x, float y, float w, float h, float tx, float ty, float tw, float th)
{
    createhudquad(x, y, x+w, y+h, tx, ty, tx+tw, ty+th);
}

void debugquad(float x, float y, float w, float h, float tx, float ty, float tw, float th)
{
    createhudquad(x, y, x+w, y+h, tx, ty+th, tx+tw, ty);
}

VARR(fog, 16, 4000, 1000024);
CVARR(fogcolor, 0x8099B3);
VAR(fogoverlay, 0, 1, 1);

static float findsurface(int fogmat, const vec &v, int &abovemat)
{
    fogmat &= MatFlag_Volume;
    ivec o(v), co;
    int csize;
    do
    {
        const cube &c = rootworld.lookupcube(o, 0, co, csize);
        int mat = c.material&MatFlag_Volume;
        if(mat != fogmat)
        {
            abovemat = IS_LIQUID(mat) ? c.material : Mat_Air;
            return o.z;
        }
        o.z = co.z + csize;
    } while(o.z < rootworld.mapsize());
    abovemat = Mat_Air;
    return rootworld.mapsize();
}

static void blendfog(int fogmat, float below, float blend, float logblend, float &start, float &end, vec &fogc)
{
    switch(fogmat&MatFlag_Volume)
    {
        case Mat_Water:
        {
            const bvec &wcol = getwatercolor(fogmat),
                       &wdeepcol = getwaterdeepcolor(fogmat);
            int wfog = getwaterfog(fogmat),
                wdeep = getwaterdeep(fogmat);
            float deepfade = std::clamp(below/std::max(wdeep, wfog), 0.0f, 1.0f);
            vec color;
            color.lerp(wcol.tocolor(), wdeepcol.tocolor(), deepfade);
            fogc.add(vec(color).mul(blend));
            end += logblend*std::min(fog, std::max(wfog*2, 16));
            break;
        }
        default:
        {
            fogc.add(fogcolor.tocolor().mul(blend));
            start += logblend*(fog+64)/8;
            end += logblend*fog;
            break;
        }
    }
}

static vec curfogcolor(0, 0, 0);

void setfogcolor(const vec &v)
{
    GLOBALPARAM(fogcolor, v);
}

void zerofogcolor()
{
    setfogcolor(vec(0, 0, 0));
}

void resetfogcolor()
{
    setfogcolor(curfogcolor);
}

FVAR(fogintensity, 0, 0.15f, 1);

float calcfogdensity(float dist)
{
    return std::log(fogintensity)/(M_LN2*dist);
}

FVAR(fogcullintensity, 0, 1e-3f, 1);

float calcfogcull()
{
    return std::log(fogcullintensity) / (M_LN2*calcfogdensity(fog - (fog+64)/8));
}

static void setfog(int fogmat, float below = 0, float blend = 1, int abovemat = Mat_Air)
{
    float start = 0,
          end = 0,
          logscale = 256,
          logblend = std::log(1 + (logscale - 1)*blend) / std::log(logscale);

    curfogcolor = vec(0, 0, 0);
    blendfog(fogmat, below, blend, logblend, start, end, curfogcolor);
    if(blend < 1)
    {
        blendfog(abovemat, 0, 1-blend, 1-logblend, start, end, curfogcolor);
    }
    curfogcolor.mul(ldrscale);
    GLOBALPARAM(fogcolor, curfogcolor);
    float fogdensity = calcfogdensity(end-start);
    GLOBALPARAMF(fogdensity, fogdensity, 1/std::exp(M_LN2*start*fogdensity));
}

static void blendfogoverlay(int fogmat, float below, float blend, vec &overlay)
{
    switch(fogmat&MatFlag_Volume)
    {
        case Mat_Water:
        {
            const bvec &wcol = getwatercolor(fogmat),
                       &wdeepcol = getwaterdeepcolor(fogmat);
            int wfog = getwaterfog(fogmat),
                wdeep = getwaterdeep(fogmat);
            float deepfade = std::clamp(below/std::max(wdeep, wfog), 0.0f, 1.0f);
            vec color = vec(wcol.r, wcol.g, wcol.b).lerp(vec(wdeepcol.r, wdeepcol.g, wdeepcol.b), deepfade);
            overlay.add(color.div(std::min(32.0f + std::max(color.r, std::max(color.g, color.b))*7.0f/8.0f, 255.0f)).max(0.4f).mul(blend));
            break;
        }
        default:
        {
            overlay.add(blend);
            break;
        }
    }
}

void drawfogoverlay(int fogmat, float fogbelow, float fogblend, int abovemat)
{
    SETSHADER(fogoverlay);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    vec overlay(0, 0, 0);
    blendfogoverlay(fogmat, fogbelow, fogblend, overlay);
    blendfogoverlay(abovemat, 0, 1-fogblend, overlay);

    gle::color(overlay);
    screenquad();

    glDisable(GL_BLEND);
}

int drawtex = 0;

/* =========================== minimap functionality ======================== */

static GLuint minimaptex = 0;
vec minimapcenter(0, 0, 0),
    minimapradius(0, 0, 0),
    minimapscale(0, 0, 0);

float calcfrustumboundsphere(float nearplane, float farplane,  const vec &pos, const vec &view, vec &center)
{
    if(drawtex == Draw_TexMinimap)
    {
        center = minimapcenter;
        return minimapradius.magnitude();
    }

    float width = std::tan(fov/(2.0f*RAD)),
          height = width / aspect,
          cdist = ((nearplane + farplane)/2)*(1 + width*width + height*height);
    if(cdist <= farplane)
    {
        center = vec(view).mul(cdist).add(pos);
        return vec(width*nearplane, height*nearplane, cdist-nearplane).magnitude();
    }
    else
    {
        center = vec(view).mul(farplane).add(pos);
        return vec(width*farplane, height*farplane, 0).magnitude();
    }
}

void clearminimap()
{
    if(minimaptex)
    {
        glDeleteTextures(1, &minimaptex);
        minimaptex = 0;
    }
}

VARR(minimapheight, 0, 0, 2<<16); //height above bottom of map to render at
CVARR(minimapcolor, 0);
VARR(minimapclip, 0, 0, 1);
VARP(minimapsize, 7, 10, 12);      //2^n size of the minimap texture (along edge)
VARP(showminimap, 0, 1, 1);
CVARP(nominimapcolor, 0x101010);  //color for the part of the minimap that isn't the map texture

void bindminimap()
{
    glBindTexture(GL_TEXTURE_2D, minimaptex);
}

void clipminimap(ivec &bbmin, ivec &bbmax, const std::array<cube, 8> &c, const ivec &co = ivec(0, 0, 0), int size = rootworld.mapsize()>>1)
{
    for(int i = 0; i < 8; ++i)
    {
        ivec o(i, co, size);
        if(c[i].children)
        {
            clipminimap(bbmin, bbmax, *(c[i].children), o, size>>1);
        }
        else if(!(c[i].issolid()) && (c[i].material&MatFlag_Clip)!=Mat_Clip)
        {
            for(int k = 0; k < 3; ++k)
            {
                bbmin[k] = std::min(bbmin[k], o[k]);
            }
            for(int k = 0; k < 3; ++k)
            {
                bbmax[k] = std::max(bbmax[k], o[k] + size);
            }
        }
    }
}

void drawminimap(int yaw, int pitch, vec loc, const cubeworld& world, int scalefactor)
{
    if(!showminimap)
    {
        if(!minimaptex)
        {
            glGenTextures(1, &minimaptex);
        }
        createtexture(minimaptex, 1, 1, nominimapcolor.v, 3, 0, GL_RGB, GL_TEXTURE_2D);
        return;
    }

    glerror();

    drawtex = Draw_TexMinimap;

    glerror();
    gl_setupframe(true);

    int size = 1<<minimapsize,
        sizelimit = std::min(hwtexsize, std::min(gw, gh));
    while(size > sizelimit)
    {
        size = size - 128;
    }
    if(!minimaptex)
    {
        glGenTextures(1, &minimaptex);
    }
    ivec bbmin(rootworld.mapsize(), rootworld.mapsize(), rootworld.mapsize()),
         bbmax(0, 0, 0);
    for(uint i = 0; i < valist.size(); i++)
    {
        const vtxarray *va = valist[i];
        for(int k = 0; k < 3; ++k)
        {
            if(va->geommin[k]>va->geommax[k])
            {
                continue;
            }
            bbmin[k] = std::min(bbmin[k], va->geommin[k]);
            bbmax[k] = std::max(bbmax[k], va->geommax[k]);
        }
    }
    if(minimapclip)
    {
        ivec clipmin(rootworld.mapsize(), rootworld.mapsize(), rootworld.mapsize()),
             clipmax(0, 0, 0);
        clipminimap(clipmin, clipmax, *world.worldroot);
        for(int k = 0; k < 2; ++k)
        {
            bbmin[k] = std::max(bbmin[k], clipmin[k]);
        }
        for(int k = 0; k < 2; ++k)
        {
            bbmax[k] = std::min(bbmax[k], clipmax[k]);
        }
    }

    minimapradius = vec(bbmax).sub(vec(bbmin)).div(scalefactor);
    minimapcenter = loc;
    minimapradius.x = minimapradius.y = std::max(minimapradius.x, minimapradius.y);
    minimapscale = vec((0.5f - 1.0f/size)/minimapradius.x, (0.5f - 1.0f/size)/minimapradius.y, 1.0f);

    physent *oldcamera = camera1;
    physent cmcamera = *player;
    cmcamera.reset();
    cmcamera.type = physent::PhysEnt_Camera;
    cmcamera.o = loc;
    cmcamera.yaw = yaw;
    cmcamera.pitch = pitch;
    cmcamera.roll = 0;
    camera1 = &cmcamera;

    float oldldrscale = ldrscale;
    int oldfarplane = farplane,
        oldvieww    = vieww,
        oldviewh    = viewh;
    farplane = rootworld.mapsize()*2;
    vieww = viewh = size;

    float zscale = std::max(static_cast<float>(minimapheight), minimapcenter.z + minimapradius.z + 1) + 1;

    projmatrix.ortho(-minimapradius.x, minimapradius.x, -minimapradius.y, minimapradius.y, 0, 2*zscale);
    setcamprojmatrix();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    xtravertsva = xtraverts = glde = gbatches = vtris = vverts = 0;
    occlusionengine.flipqueries();

    ldrscale = 1;

    view.visiblecubes(false);
    gbuf.rendergbuffer();
    gbuf.rendershadowatlas();

    gbuf.shademinimap(minimapcolor.tocolor().mul(ldrscale));

    if(minimapheight > 0 && minimapheight < minimapcenter.z + minimapradius.z)
    {
        camera1->o.z = minimapcenter.z + minimapradius.z + 1;
        projmatrix.ortho(-minimapradius.x, minimapradius.x, -minimapradius.y, minimapradius.y, -zscale, zscale);
        setcamprojmatrix();
        gbuf.rendergbuffer(false);
        gbuf.shademinimap();
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    farplane = oldfarplane;
    vieww = oldvieww;
    viewh = oldviewh;
    ldrscale = oldldrscale;

    camera1 = oldcamera;
    drawtex = 0;

    createtexture(minimaptex, size, size, nullptr, 3, 1, GL_RGB5, GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat border[4] = { minimapcolor.x/255.0f, minimapcolor.y/255.0f, minimapcolor.z/255.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, minimaptex, 0);
    copyhdr(size, size, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    glViewport(0, 0, hudw(), hudh());
}

VAR(modelpreviewfov, 10, 20, 100);    //y axis field of view
VAR(modelpreviewpitch, -90, -15, 90); //pitch above model to render

/* ======================== model preview windows =========================== */


void ModelPreview::start(int xcoord, int ycoord, int width, int height, bool bg, bool usescissor)
{
    x = xcoord;
    y = ycoord;
    w = width;
    h = height;
    background = bg;
    scissor = usescissor;

    gbuf.setupgbuffer();

    useshaderbyname("modelpreview");

    drawtex = Draw_TexModelPreview;

    oldcamera = camera1;
    camera = *camera1;
    camera.reset();
    camera.type = physent::PhysEnt_Camera;
    camera.o = vec(0, 0, 0);
    camera.yaw = 0;
    camera.pitch = modelpreviewpitch;
    camera.roll = 0;
    camera1 = &camera;

    oldaspect = aspect;
    oldfovy = fovy;
    oldfov = curfov;
    oldldrscale = ldrscale;
    oldfarplane = farplane;
    oldvieww = vieww;
    oldviewh = viewh;
    oldprojmatrix = projmatrix;

    aspect = w/static_cast<float>(h);
    fovy = modelpreviewfov;
    curfov = 2*std::atan2(std::tan(fovy/(2*RAD)), 1/aspect)*RAD;
    farplane = 1024;
    vieww = std::min(gw, w);
    viewh = std::min(gh, h);
    ldrscale = 1;

    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void ModelPreview::end()
{
    gbuf.rendermodelbatches();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    gbuf.shademodelpreview(x, y, w, h, background, scissor);

    aspect = oldaspect;
    fovy = oldfovy;
    curfov = oldfov;
    farplane = oldfarplane;
    vieww = oldvieww;
    viewh = oldviewh;
    ldrscale = oldldrscale;

    camera1 = oldcamera;
    drawtex = 0;

    projmatrix = oldprojmatrix;
    setcamprojmatrix();
}

vec calcmodelpreviewpos(const vec &radius, float &yaw)
{
    yaw = std::fmod(lastmillis/10000.0f*360.0f, 360.0f);
    float dist = std::max(radius.magnitude2()/aspect, radius.magnitude())/std::sin(fovy/(2*RAD));
    return vec(0, dist, 0).rotate_around_x(camera1->pitch/RAD);
}

int xtraverts, xtravertsva;

/* ============================= core rendering ============================= */

//main scene rendering function
void gl_drawview(void (*gamefxn)(), void(*hudfxn)(), void(*editfxn)())
{
    GLuint scalefbo = gbuf.shouldscale();
    if(scalefbo)
    {
        vieww = gw;
        viewh = gh;
    }
    float fogmargin = 1 + wateramplitude + nearplane;
    int fogmat = rootworld.lookupmaterial(vec(camera1->o.x, camera1->o.y, camera1->o.z - fogmargin))&(MatFlag_Volume|MatFlag_Index),
        abovemat = Mat_Air;
    float fogbelow = 0;
    if(IS_LIQUID(fogmat&MatFlag_Volume)) //if in the water
    {
        float z = findsurface(fogmat, vec(camera1->o.x, camera1->o.y, camera1->o.z - fogmargin), abovemat) - wateroffset;
        if(camera1->o.z < z + fogmargin)
        {
            fogbelow = z - camera1->o.z;
        }
        else
        {
            fogmat = abovemat;
        }
    }
    else
    {
        fogmat = Mat_Air; //use air fog
    }
    setfog(abovemat);
    //setfog(fogmat, fogbelow, 1, abovemat);

    farplane = rootworld.mapsize()*2;
    //set the camera location
    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    ldrscale = 0.5f;
    //do occlusion culling
    view.visiblecubes();
    //set to wireframe if applicable
    if(wireframe && editmode)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    //construct g-buffer (build basic scene)
    gbuf.rendergbuffer(true, gamefxn);
    if(wireframe && editmode) //done with wireframe mode now
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    else if(limitsky() && editmode)
    {
        renderexplicitsky(true);
    }

    //ambient obscurance (ambient occlusion) on geometry & models only
    gbuf.renderao();
    glerror();

    // render avatar after AO to avoid weird contact shadows
    renderavatar(hudfxn);
    glerror();

    // render grass after AO to avoid disturbing shimmering patterns
    generategrass();
    rendergrass();
    glerror();

    glFlush();
    //global illumination
    gbuf.renderradiancehints();
    glerror();
    //lighting
    gbuf.rendershadowatlas();
    glerror();
    //shading
    shadegbuffer();
    glerror();

    //fog
    if(fogmat)
    {
        setfog(fogmat, fogbelow, 1, abovemat);

        gbuf.renderwaterfog(fogmat, fogbelow);

        setfog(fogmat, fogbelow, std::clamp(fogbelow, 0.0f, 1.0f), abovemat);
    }

    //alpha
    gbuf.rendertransparent();
    glerror();

    if(fogmat)
    {
        setfog(fogmat, fogbelow, 1, abovemat);
    }

    //volumetric lights
    gbuf.rendervolumetric();
    glerror();

    if(editmode)
    {
        if(!wireframe && outline)
        {
            renderoutline(); //edit mode geometry outline
        }
        glerror();
        rendereditmaterials();
        glerror();
        gbuf.renderparticles();
        glerror();
        if(showhud)
        {
            glDepthMask(GL_FALSE);
            editfxn(); //edit cursor, passed as pointer
            glDepthMask(GL_TRUE);
        }
    }

    //we're done with depth/geometry stuff so we don't need this functionality
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    if(fogoverlay && fogmat != Mat_Air)
    {
        drawfogoverlay(fogmat, fogbelow, std::clamp(fogbelow, 0.0f, 1.0f), abovemat);
    }
    //antialiasing
    doaa(setuppostfx(gbuf, vieww, viewh, scalefbo), gbuf);
    //postfx
    renderpostfx(scalefbo);
    if(scalefbo)
    {
        gbuf.doscale();
    }
}

int renderw()
{
    return std::min(scr_w, screenw);
}

int renderh()
{
    return std::min(scr_h, screenh);
}

int hudw()
{
    return screenw;
}

int hudh()
{
    return screenh;
}

void gl_setupframe(bool force)
{
    if(!force)
    {
        return;
    }
    setuplights(gbuf);
}

void gl_drawframe(int crosshairindex, void (*gamefxn)(), void (*hudfxn)(), void (*editfxn)(), void (*hud2d)())
{
    synctimers();
    xtravertsva = xtraverts = glde = gbatches = vtris = vverts = 0;
    occlusionengine.flipqueries();
    aspect = forceaspect ? forceaspect : hudw()/static_cast<float>(hudh());
    fovy = 2*std::atan2(std::tan(curfov/(2*RAD)), aspect)*RAD;
    vieww = hudw();
    viewh = hudh();
    if(mainmenu)
    {
        gl_drawmainmenu();
    }
    else
    {
        gl_drawview(gamefxn, hudfxn, editfxn);
    }
    UI::render();
    gl_drawhud(crosshairindex, hud2d);
}

void cleanupgl()
{
    clearminimap();
    cleanuptimers();
    cleanupscreenquad();
    gle::cleanup();
}

void initrenderglcmds()
{
    addcommand("glext", reinterpret_cast<identfun>(glext), "s", Id_Command);
    addcommand("getcamyaw", reinterpret_cast<identfun>(+[](){floatret(camera1 ? camera1->yaw : 0);}), "", Id_Command);
    addcommand("getcampitch", reinterpret_cast<identfun>(+[](){floatret(camera1 ? camera1->pitch : 0);}), "", Id_Command);
    addcommand("getcamroll", reinterpret_cast<identfun>(+[](){floatret(camera1 ? camera1->roll : 0);}), "", Id_Command);
    addcommand("getcampos", reinterpret_cast<identfun>(+[]()
    {
        if(!camera1)
        {
            result("no camera");
        }
        else
        {
            DEF_FORMAT_STRING(pos, "%s %s %s", floatstr(camera1->o.x), floatstr(camera1->o.y), floatstr(camera1->o.z));
            result(pos);
        }
    }), "", Id_Command);
}

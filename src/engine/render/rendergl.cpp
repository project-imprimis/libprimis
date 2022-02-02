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
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "aa.h"
#include "grass.h"
#include "hdr.h"
#include "hud.h"
#include "octarender.h"
#include "radiancehints.h"
#include "renderalpha.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "renderparticles.h"
#include "rendersky.h"
#include "rendertimers.h"
#include "renderva.h"
#include "renderwindow.h"
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
     hasDBGO   = false,
     hasEGPU4  = false,
     hasES3    = false,
     hasCI     = false;
bool mesa   = false,
     intel  = false,
     amd    = false,
     nvidia = false;

int hasstencil = 0;

//read-only info for gl debugging
VAR(glversion, 1, 0, 0);
VAR(glslversion, 1, 0, 0);

// GL_EXT_timer_query
PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64v_  = nullptr;
PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64v_ = nullptr;

// GL_EXT_framebuffer_object
PFNGLBINDRENDERBUFFERPROC           glBindRenderbuffer_           = nullptr;
PFNGLDELETERENDERBUFFERSPROC        glDeleteRenderbuffers_        = nullptr;
PFNGLGENFRAMEBUFFERSPROC            glGenRenderbuffers_           = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC        glRenderbufferStorage_        = nullptr;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv_ = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC     glCheckFramebufferStatus_     = nullptr;
PFNGLBINDFRAMEBUFFERPROC            glBindFramebuffer_            = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC         glDeleteFramebuffers_         = nullptr;
PFNGLGENFRAMEBUFFERSPROC            glGenFramebuffers_            = nullptr;
PFNGLFRAMEBUFFERTEXTURE1DPROC       glFramebufferTexture1D_       = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC       glFramebufferTexture2D_       = nullptr;
PFNGLFRAMEBUFFERTEXTURE3DPROC       glFramebufferTexture3D_       = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC    glFramebufferRenderbuffer_    = nullptr;
PFNGLGENERATEMIPMAPPROC             glGenerateMipmap_             = nullptr;

// GL_EXT_framebuffer_blit
PFNGLBLITFRAMEBUFFERPROC         glBlitFramebuffer_         = nullptr;

// GL_EXT_framebuffer_multisample
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample_ = nullptr;

// GL_ARB_texture_multisample
PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample_ = nullptr;
PFNGLTEXIMAGE3DMULTISAMPLEPROC glTexImage3DMultisample_ = nullptr;
PFNGLGETMULTISAMPLEFVPROC      glGetMultisamplefv_      = nullptr;
PFNGLSAMPLEMASKIPROC           glSampleMaski_           = nullptr;

// GL_ARB_sample_shading
PFNGLMINSAMPLESHADINGPROC glMinSampleShading_ = nullptr;

// GL_ARB_draw_buffers_blend
PFNGLBLENDEQUATIONIPROC         glBlendEquationi_         = nullptr;
PFNGLBLENDEQUATIONSEPARATEIPROC glBlendEquationSeparatei_ = nullptr;
PFNGLBLENDFUNCIPROC             glBlendFunci_             = nullptr;
PFNGLBLENDFUNCSEPARATEIPROC     glBlendFuncSeparatei_     = nullptr;

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

// OpenGL 2.0
PFNGLMULTIDRAWARRAYSPROC   glMultiDrawArrays_   = nullptr;
PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements_ = nullptr;

PFNGLBLENDFUNCSEPARATEPROC     glBlendFuncSeparate_     = nullptr;
PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate_ = nullptr;
PFNGLSTENCILOPSEPARATEPROC     glStencilOpSeparate_     = nullptr;
PFNGLSTENCILFUNCSEPARATEPROC   glStencilFuncSeparate_   = nullptr;
PFNGLSTENCILMASKSEPARATEPROC   glStencilMaskSeparate_   = nullptr;

PFNGLGENBUFFERSPROC       glGenBuffers_       = nullptr;
PFNGLBINDBUFFERPROC       glBindBuffer_       = nullptr;
PFNGLMAPBUFFERPROC        glMapBuffer_        = nullptr;
PFNGLUNMAPBUFFERPROC      glUnmapBuffer_      = nullptr;
PFNGLBUFFERDATAPROC       glBufferData_       = nullptr;
PFNGLBUFFERSUBDATAPROC    glBufferSubData_    = nullptr;
PFNGLDELETEBUFFERSPROC    glDeleteBuffers_    = nullptr;
PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData_ = nullptr;

PFNGLGENQUERIESPROC        glGenQueries_        = nullptr;
PFNGLDELETEQUERIESPROC     glDeleteQueries_     = nullptr;
PFNGLBEGINQUERYPROC        glBeginQuery_        = nullptr;
PFNGLENDQUERYPROC          glEndQuery_          = nullptr;
PFNGLGETQUERYIVPROC        glGetQueryiv_        = nullptr;
PFNGLGETQUERYOBJECTIVPROC  glGetQueryObjectiv_  = nullptr;
PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv_ = nullptr;

PFNGLCREATEPROGRAMPROC            glCreateProgram_            = nullptr;
PFNGLDELETEPROGRAMPROC            glDeleteProgram_            = nullptr;
PFNGLUSEPROGRAMPROC               glUseProgram_               = nullptr;
PFNGLCREATESHADERPROC             glCreateShader_             = nullptr;
PFNGLDELETESHADERPROC             glDeleteShader_             = nullptr;
PFNGLSHADERSOURCEPROC             glShaderSource_             = nullptr;
PFNGLCOMPILESHADERPROC            glCompileShader_            = nullptr;
PFNGLGETSHADERIVPROC              glGetShaderiv_              = nullptr;
PFNGLGETPROGRAMIVPROC             glGetProgramiv_             = nullptr;
PFNGLATTACHSHADERPROC             glAttachShader_             = nullptr;
PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog_        = nullptr;
PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog_         = nullptr;
PFNGLLINKPROGRAMPROC              glLinkProgram_              = nullptr;
PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation_       = nullptr;
PFNGLUNIFORM1FPROC                glUniform1f_                = nullptr;
PFNGLUNIFORM2FPROC                glUniform2f_                = nullptr;
PFNGLUNIFORM3FPROC                glUniform3f_                = nullptr;
PFNGLUNIFORM4FPROC                glUniform4f_                = nullptr;
PFNGLUNIFORM1FVPROC               glUniform1fv_               = nullptr;
PFNGLUNIFORM2FVPROC               glUniform2fv_               = nullptr;
PFNGLUNIFORM3FVPROC               glUniform3fv_               = nullptr;
PFNGLUNIFORM4FVPROC               glUniform4fv_               = nullptr;
PFNGLUNIFORM1IPROC                glUniform1i_                = nullptr;
PFNGLUNIFORM2IPROC                glUniform2i_                = nullptr;
PFNGLUNIFORM3IPROC                glUniform3i_                = nullptr;
PFNGLUNIFORM4IPROC                glUniform4i_                = nullptr;
PFNGLUNIFORM1IVPROC               glUniform1iv_               = nullptr;
PFNGLUNIFORM2IVPROC               glUniform2iv_               = nullptr;
PFNGLUNIFORM3IVPROC               glUniform3iv_               = nullptr;
PFNGLUNIFORM4IVPROC               glUniform4iv_               = nullptr;
PFNGLUNIFORMMATRIX2FVPROC         glUniformMatrix2fv_         = nullptr;
PFNGLUNIFORMMATRIX3FVPROC         glUniformMatrix3fv_         = nullptr;
PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv_         = nullptr;
PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation_       = nullptr;
PFNGLGETACTIVEUNIFORMPROC         glGetActiveUniform_         = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray_  = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_ = nullptr;

PFNGLVERTEXATTRIB1FPROC           glVertexAttrib1f_           = nullptr;
PFNGLVERTEXATTRIB1FVPROC          glVertexAttrib1fv_          = nullptr;
PFNGLVERTEXATTRIB1SPROC           glVertexAttrib1s_           = nullptr;
PFNGLVERTEXATTRIB1SVPROC          glVertexAttrib1sv_          = nullptr;
PFNGLVERTEXATTRIB2FPROC           glVertexAttrib2f_           = nullptr;
PFNGLVERTEXATTRIB2FVPROC          glVertexAttrib2fv_          = nullptr;
PFNGLVERTEXATTRIB2SPROC           glVertexAttrib2s_           = nullptr;
PFNGLVERTEXATTRIB2SVPROC          glVertexAttrib2sv_          = nullptr;
PFNGLVERTEXATTRIB3FPROC           glVertexAttrib3f_           = nullptr;
PFNGLVERTEXATTRIB3FVPROC          glVertexAttrib3fv_          = nullptr;
PFNGLVERTEXATTRIB3SPROC           glVertexAttrib3s_           = nullptr;
PFNGLVERTEXATTRIB3SVPROC          glVertexAttrib3sv_          = nullptr;
PFNGLVERTEXATTRIB4FPROC           glVertexAttrib4f_           = nullptr;
PFNGLVERTEXATTRIB4FVPROC          glVertexAttrib4fv_          = nullptr;
PFNGLVERTEXATTRIB4SPROC           glVertexAttrib4s_           = nullptr;
PFNGLVERTEXATTRIB4SVPROC          glVertexAttrib4sv_          = nullptr;
PFNGLVERTEXATTRIB4BVPROC          glVertexAttrib4bv_          = nullptr;
PFNGLVERTEXATTRIB4IVPROC          glVertexAttrib4iv_          = nullptr;
PFNGLVERTEXATTRIB4UBVPROC         glVertexAttrib4ubv_         = nullptr;
PFNGLVERTEXATTRIB4UIVPROC         glVertexAttrib4uiv_         = nullptr;
PFNGLVERTEXATTRIB4USVPROC         glVertexAttrib4usv_         = nullptr;
PFNGLVERTEXATTRIB4NBVPROC         glVertexAttrib4Nbv_         = nullptr;
PFNGLVERTEXATTRIB4NIVPROC         glVertexAttrib4Niv_         = nullptr;
PFNGLVERTEXATTRIB4NUBPROC         glVertexAttrib4Nub_         = nullptr;
PFNGLVERTEXATTRIB4NUBVPROC        glVertexAttrib4Nubv_        = nullptr;
PFNGLVERTEXATTRIB4NUIVPROC        glVertexAttrib4Nuiv_        = nullptr;
PFNGLVERTEXATTRIB4NUSVPROC        glVertexAttrib4Nusv_        = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer_      = nullptr;

PFNGLDRAWBUFFERSPROC glDrawBuffers_ = nullptr;

// OpenGL 3.0
PFNGLGETSTRINGIPROC           glGetStringi_           = nullptr;
PFNGLBINDFRAGDATALOCATIONPROC glBindFragDataLocation_ = nullptr;
PFNGLUNIFORM1UIPROC           glUniform1ui_           = nullptr;
PFNGLUNIFORM2UIPROC           glUniform2ui_           = nullptr;
PFNGLUNIFORM3UIPROC           glUniform3ui_           = nullptr;
PFNGLUNIFORM4UIPROC           glUniform4ui_           = nullptr;
PFNGLUNIFORM1UIVPROC          glUniform1uiv_          = nullptr;
PFNGLUNIFORM2UIVPROC          glUniform2uiv_          = nullptr;
PFNGLUNIFORM3UIVPROC          glUniform3uiv_          = nullptr;
PFNGLUNIFORM4UIVPROC          glUniform4uiv_          = nullptr;
PFNGLCLEARBUFFERIVPROC        glClearBufferiv_        = nullptr;
PFNGLCLEARBUFFERUIVPROC       glClearBufferuiv_       = nullptr;
PFNGLCLEARBUFFERFVPROC        glClearBufferfv_        = nullptr;
PFNGLCLEARBUFFERFIPROC        glClearBufferfi_        = nullptr;

// GL_EXT_draw_buffers2
PFNGLCOLORMASKIPROC glColorMaski_ = nullptr;
PFNGLENABLEIPROC    glEnablei_    = nullptr;
PFNGLDISABLEIPROC   glDisablei_   = nullptr;

// GL_NV_conditional_render
PFNGLBEGINCONDITIONALRENDERPROC glBeginConditionalRender_ = nullptr;
PFNGLENDCONDITIONALRENDERPROC   glEndConditionalRender_   = nullptr;

// GL_EXT_texture_integer
PFNGLTEXPARAMETERIIVPROC     glTexParameterIiv_     = nullptr;
PFNGLTEXPARAMETERIUIVPROC    glTexParameterIuiv_    = nullptr;
PFNGLGETTEXPARAMETERIIVPROC  glGetTexParameterIiv_  = nullptr;
PFNGLGETTEXPARAMETERIUIVPROC glGetTexParameterIuiv_ = nullptr;
PFNGLCLEARCOLORIIEXTPROC     glClearColorIi_        = nullptr;
PFNGLCLEARCOLORIUIEXTPROC    glClearColorIui_       = nullptr;

// GL_ARB_uniform_buffer_object
PFNGLGETUNIFORMINDICESPROC       glGetUniformIndices_       = nullptr;
PFNGLGETACTIVEUNIFORMSIVPROC     glGetActiveUniformsiv_     = nullptr;
PFNGLGETUNIFORMBLOCKINDEXPROC    glGetUniformBlockIndex_    = nullptr;
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv_ = nullptr;
PFNGLUNIFORMBLOCKBINDINGPROC     glUniformBlockBinding_     = nullptr;
PFNGLBINDBUFFERBASEPROC          glBindBufferBase_          = nullptr;
PFNGLBINDBUFFERRANGEPROC         glBindBufferRange_         = nullptr;

// GL_ARB_copy_buffer
PFNGLCOPYBUFFERSUBDATAPROC glCopyBufferSubData_ = nullptr;

// GL_EXT_depth_bounds_test
PFNGLDEPTHBOUNDSEXTPROC glDepthBounds_ = nullptr;

// GL_ARB_color_buffer_float
PFNGLCLAMPCOLORPROC glClampColor_ = nullptr;

// GL_ARB_debug_output
PFNGLDEBUGMESSAGECONTROLPROC  glDebugMessageControl_  = nullptr;
PFNGLDEBUGMESSAGEINSERTPROC   glDebugMessageInsert_   = nullptr;
PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback_ = nullptr;
PFNGLGETDEBUGMESSAGELOGPROC   glGetDebugMessageLog_   = nullptr;

// GL_ARB_map_buffer_range
PFNGLMAPBUFFERRANGEPROC         glMapBufferRange_         = nullptr;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange_ = nullptr;

// GL_ARB_vertex_array_object
PFNGLBINDVERTEXARRAYPROC    glBindVertexArray_    = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ = nullptr;
PFNGLGENVERTEXARRAYSPROC    glGenVertexArrays_    = nullptr;
PFNGLISVERTEXARRAYPROC      glIsVertexArray_      = nullptr;

// GL_ARB_blend_func_extended
PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glBindFragDataLocationIndexed_ = nullptr;

// GL_ARB_copy_image
PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData_ = nullptr;

void masktiles(uint *tiles, float sx1, float sy1, float sx2, float sy2)
{
    int tx1, ty1, tx2, ty2;
    calctilebounds(sx1, sy1, sx2, sy2, tx1, ty1, tx2, ty2);
    for(int ty = ty1; ty < ty2; ty++) tiles[ty] |= ((1<<(tx2-tx1))-1)<<tx1;
}

void *getprocaddress(const char *name)
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
VAR(useubo, 1, 0, 0);
VAR(usetexgather, 1, 0, 0);
VAR(maxdrawbufs, 1, 0, 0);
VAR(maxdualdrawbufs, 1, 0, 0);

VAR(debugexts, 0, 0, 1);

hashset<const char *> glexts;

void parseglexts()
{
    GLint numexts = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numexts);
    for(int i = 0; i < numexts; ++i)
    {
        const char *ext = reinterpret_cast<const char *>(glGetStringi_(GL_EXTENSIONS, i));
        glexts.add(newstring(ext));
    }
}

bool hasext(const char *ext)
{
    return glexts.access(ext)!=nullptr;
}

bool checkdepthtexstencilrb()
{
    int w = 256,
        h = 256;
    GLuint fbo = 0;
    glGenFramebuffers_(1, &fbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);

    GLuint depthtex = 0;
    glGenTextures(1, &depthtex);
    createtexture(depthtex, w, h, nullptr, 3, 0, GL_DEPTH_COMPONENT24, GL_TEXTURE_RECTANGLE);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_RECTANGLE, depthtex, 0);

    GLuint stencilrb = 0;
    glGenRenderbuffers_(1, &stencilrb);
    glBindRenderbuffer_(GL_RENDERBUFFER, stencilrb);
    glRenderbufferStorage_(GL_RENDERBUFFER, GL_STENCIL_INDEX8, w, h);
    glBindRenderbuffer_(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilrb);

    bool supported = glCheckFramebufferStatus_(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers_(1, &fbo);
    glDeleteTextures(1, &depthtex);
    glDeleteRenderbuffers_(1, &stencilrb);

    return supported;
}

void gl_checkextensions()
{
    const char *vendor   = reinterpret_cast<const char *>(glGetString(GL_VENDOR)),
               *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER)),
               *version  = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    conoutf(Console_Init, "Renderer: %s (%s)", renderer, vendor);
    conoutf(Console_Init, "Driver: %s", version);

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
    #ifdef WIN32
        glActiveTexture_ =            (PFNGLACTIVETEXTUREPROC)            getprocaddress("glActiveTexture");

        glBlendEquation_ =            (PFNGLBLENDEQUATIONPROC)            getprocaddress("glBlendEquation");
        glBlendColor_ =               (PFNGLBLENDCOLORPROC)               getprocaddress("glBlendColor");

        glTexImage3D_ =               (PFNGLTEXIMAGE3DPROC)               getprocaddress("glTexImage3D");
        glTexSubImage3D_ =            (PFNGLTEXSUBIMAGE3DPROC)            getprocaddress("glTexSubImage3D");
        glCopyTexSubImage3D_ =        (PFNGLCOPYTEXSUBIMAGE3DPROC)        getprocaddress("glCopyTexSubImage3D");

        glCompressedTexImage3D_ =     (PFNGLCOMPRESSEDTEXIMAGE3DPROC)     getprocaddress("glCompressedTexImage3D");
        glCompressedTexImage2D_ =     (PFNGLCOMPRESSEDTEXIMAGE2DPROC)     getprocaddress("glCompressedTexImage2D");
        glCompressedTexImage1D_ =     (PFNGLCOMPRESSEDTEXIMAGE1DPROC)     getprocaddress("glCompressedTexImage1D");
        glCompressedTexSubImage3D_ =  (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)  getprocaddress("glCompressedTexSubImage3D");
        glCompressedTexSubImage2D_ =  (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)  getprocaddress("glCompressedTexSubImage2D");
        glCompressedTexSubImage1D_ =  (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)  getprocaddress("glCompressedTexSubImage1D");
        glGetCompressedTexImage_ =    (PFNGLGETCOMPRESSEDTEXIMAGEPROC)    getprocaddress("glGetCompressedTexImage");

        glDrawRangeElements_ =        (PFNGLDRAWRANGEELEMENTSPROC)        getprocaddress("glDrawRangeElements");
    #endif
    glMultiDrawArrays_ =          (PFNGLMULTIDRAWARRAYSPROC)          getprocaddress("glMultiDrawArrays");
    glMultiDrawElements_ =        (PFNGLMULTIDRAWELEMENTSPROC)        getprocaddress("glMultiDrawElements");

    glBlendFuncSeparate_ =        (PFNGLBLENDFUNCSEPARATEPROC)        getprocaddress("glBlendFuncSeparate");
    glBlendEquationSeparate_ =    (PFNGLBLENDEQUATIONSEPARATEPROC)    getprocaddress("glBlendEquationSeparate");
    glStencilOpSeparate_ =        (PFNGLSTENCILOPSEPARATEPROC)        getprocaddress("glStencilOpSeparate");
    glStencilFuncSeparate_ =      (PFNGLSTENCILFUNCSEPARATEPROC)      getprocaddress("glStencilFuncSeparate");
    glStencilMaskSeparate_ =      (PFNGLSTENCILMASKSEPARATEPROC)      getprocaddress("glStencilMaskSeparate");

    glGenBuffers_ =               (PFNGLGENBUFFERSPROC)               getprocaddress("glGenBuffers");
    glBindBuffer_ =               (PFNGLBINDBUFFERPROC)               getprocaddress("glBindBuffer");
    glMapBuffer_ =                (PFNGLMAPBUFFERPROC)                getprocaddress("glMapBuffer");
    glUnmapBuffer_ =              (PFNGLUNMAPBUFFERPROC)              getprocaddress("glUnmapBuffer");
    glBufferData_ =               (PFNGLBUFFERDATAPROC)               getprocaddress("glBufferData");
    glBufferSubData_ =            (PFNGLBUFFERSUBDATAPROC)            getprocaddress("glBufferSubData");
    glDeleteBuffers_ =            (PFNGLDELETEBUFFERSPROC)            getprocaddress("glDeleteBuffers");
    glGetBufferSubData_ =         (PFNGLGETBUFFERSUBDATAPROC)         getprocaddress("glGetBufferSubData");

    glGetQueryiv_ =               (PFNGLGETQUERYIVPROC)               getprocaddress("glGetQueryiv");
    glGenQueries_ =               (PFNGLGENQUERIESPROC)               getprocaddress("glGenQueries");
    glDeleteQueries_ =            (PFNGLDELETEQUERIESPROC)            getprocaddress("glDeleteQueries");
    glBeginQuery_ =               (PFNGLBEGINQUERYPROC)               getprocaddress("glBeginQuery");
    glEndQuery_ =                 (PFNGLENDQUERYPROC)                 getprocaddress("glEndQuery");
    glGetQueryObjectiv_ =         (PFNGLGETQUERYOBJECTIVPROC)         getprocaddress("glGetQueryObjectiv");
    glGetQueryObjectuiv_ =        (PFNGLGETQUERYOBJECTUIVPROC)        getprocaddress("glGetQueryObjectuiv");

    glCreateProgram_            = (PFNGLCREATEPROGRAMPROC)            getprocaddress("glCreateProgram");
    glDeleteProgram_            = (PFNGLDELETEPROGRAMPROC)            getprocaddress("glDeleteProgram");
    glUseProgram_               = (PFNGLUSEPROGRAMPROC)               getprocaddress("glUseProgram");
    glCreateShader_             = (PFNGLCREATESHADERPROC)             getprocaddress("glCreateShader");
    glDeleteShader_             = (PFNGLDELETESHADERPROC)             getprocaddress("glDeleteShader");
    glShaderSource_             = (PFNGLSHADERSOURCEPROC)             getprocaddress("glShaderSource");
    glCompileShader_            = (PFNGLCOMPILESHADERPROC)            getprocaddress("glCompileShader");
    glGetShaderiv_              = (PFNGLGETSHADERIVPROC)              getprocaddress("glGetShaderiv");
    glGetProgramiv_             = (PFNGLGETPROGRAMIVPROC)             getprocaddress("glGetProgramiv");
    glAttachShader_             = (PFNGLATTACHSHADERPROC)             getprocaddress("glAttachShader");
    glGetProgramInfoLog_        = (PFNGLGETPROGRAMINFOLOGPROC)        getprocaddress("glGetProgramInfoLog");
    glGetShaderInfoLog_         = (PFNGLGETSHADERINFOLOGPROC)         getprocaddress("glGetShaderInfoLog");
    glLinkProgram_              = (PFNGLLINKPROGRAMPROC)              getprocaddress("glLinkProgram");
    glGetUniformLocation_       = (PFNGLGETUNIFORMLOCATIONPROC)       getprocaddress("glGetUniformLocation");
    glUniform1f_                = (PFNGLUNIFORM1FPROC)                getprocaddress("glUniform1f");
    glUniform2f_                = (PFNGLUNIFORM2FPROC)                getprocaddress("glUniform2f");
    glUniform3f_                = (PFNGLUNIFORM3FPROC)                getprocaddress("glUniform3f");
    glUniform4f_                = (PFNGLUNIFORM4FPROC)                getprocaddress("glUniform4f");
    glUniform1fv_               = (PFNGLUNIFORM1FVPROC)               getprocaddress("glUniform1fv");
    glUniform2fv_               = (PFNGLUNIFORM2FVPROC)               getprocaddress("glUniform2fv");
    glUniform3fv_               = (PFNGLUNIFORM3FVPROC)               getprocaddress("glUniform3fv");
    glUniform4fv_               = (PFNGLUNIFORM4FVPROC)               getprocaddress("glUniform4fv");
    glUniform1i_                = (PFNGLUNIFORM1IPROC)                getprocaddress("glUniform1i");
    glUniform2i_                = (PFNGLUNIFORM2IPROC)                getprocaddress("glUniform2i");
    glUniform3i_                = (PFNGLUNIFORM3IPROC)                getprocaddress("glUniform3i");
    glUniform4i_                = (PFNGLUNIFORM4IPROC)                getprocaddress("glUniform4i");
    glUniform1iv_               = (PFNGLUNIFORM1IVPROC)               getprocaddress("glUniform1iv");
    glUniform2iv_               = (PFNGLUNIFORM2IVPROC)               getprocaddress("glUniform2iv");
    glUniform3iv_               = (PFNGLUNIFORM3IVPROC)               getprocaddress("glUniform3iv");
    glUniform4iv_               = (PFNGLUNIFORM4IVPROC)               getprocaddress("glUniform4iv");
    glUniformMatrix2fv_         = (PFNGLUNIFORMMATRIX2FVPROC)         getprocaddress("glUniformMatrix2fv");
    glUniformMatrix3fv_         = (PFNGLUNIFORMMATRIX3FVPROC)         getprocaddress("glUniformMatrix3fv");
    glUniformMatrix4fv_         = (PFNGLUNIFORMMATRIX4FVPROC)         getprocaddress("glUniformMatrix4fv");
    glBindAttribLocation_       = (PFNGLBINDATTRIBLOCATIONPROC)       getprocaddress("glBindAttribLocation");
    glGetActiveUniform_         = (PFNGLGETACTIVEUNIFORMPROC)         getprocaddress("glGetActiveUniform");
    glEnableVertexAttribArray_  = (PFNGLENABLEVERTEXATTRIBARRAYPROC)  getprocaddress("glEnableVertexAttribArray");
    glDisableVertexAttribArray_ = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) getprocaddress("glDisableVertexAttribArray");

    glVertexAttrib1f_      =      (PFNGLVERTEXATTRIB1FPROC)           getprocaddress("glVertexAttrib1f");
    glVertexAttrib1fv_     =      (PFNGLVERTEXATTRIB1FVPROC)          getprocaddress("glVertexAttrib1fv");
    glVertexAttrib1s_      =      (PFNGLVERTEXATTRIB1SPROC)           getprocaddress("glVertexAttrib1s");
    glVertexAttrib1sv_     =      (PFNGLVERTEXATTRIB1SVPROC)          getprocaddress("glVertexAttrib1sv");
    glVertexAttrib2f_      =      (PFNGLVERTEXATTRIB2FPROC)           getprocaddress("glVertexAttrib2f");
    glVertexAttrib2fv_     =      (PFNGLVERTEXATTRIB2FVPROC)          getprocaddress("glVertexAttrib2fv");
    glVertexAttrib2s_      =      (PFNGLVERTEXATTRIB2SPROC)           getprocaddress("glVertexAttrib2s");
    glVertexAttrib2sv_     =      (PFNGLVERTEXATTRIB2SVPROC)          getprocaddress("glVertexAttrib2sv");
    glVertexAttrib3f_      =      (PFNGLVERTEXATTRIB3FPROC)           getprocaddress("glVertexAttrib3f");
    glVertexAttrib3fv_     =      (PFNGLVERTEXATTRIB3FVPROC)          getprocaddress("glVertexAttrib3fv");
    glVertexAttrib3s_      =      (PFNGLVERTEXATTRIB3SPROC)           getprocaddress("glVertexAttrib3s");
    glVertexAttrib3sv_     =      (PFNGLVERTEXATTRIB3SVPROC)          getprocaddress("glVertexAttrib3sv");
    glVertexAttrib4f_      =      (PFNGLVERTEXATTRIB4FPROC)           getprocaddress("glVertexAttrib4f");
    glVertexAttrib4fv_     =      (PFNGLVERTEXATTRIB4FVPROC)          getprocaddress("glVertexAttrib4fv");
    glVertexAttrib4s_      =      (PFNGLVERTEXATTRIB4SPROC)           getprocaddress("glVertexAttrib4s");
    glVertexAttrib4sv_     =      (PFNGLVERTEXATTRIB4SVPROC)          getprocaddress("glVertexAttrib4sv");
    glVertexAttrib4bv_     =      (PFNGLVERTEXATTRIB4BVPROC)          getprocaddress("glVertexAttrib4bv");
    glVertexAttrib4iv_     =      (PFNGLVERTEXATTRIB4IVPROC)          getprocaddress("glVertexAttrib4iv");
    glVertexAttrib4ubv_    =      (PFNGLVERTEXATTRIB4UBVPROC)         getprocaddress("glVertexAttrib4ubv");
    glVertexAttrib4uiv_    =      (PFNGLVERTEXATTRIB4UIVPROC)         getprocaddress("glVertexAttrib4uiv");
    glVertexAttrib4usv_    =      (PFNGLVERTEXATTRIB4USVPROC)         getprocaddress("glVertexAttrib4usv");
    glVertexAttrib4Nbv_    =      (PFNGLVERTEXATTRIB4NBVPROC)         getprocaddress("glVertexAttrib4Nbv");
    glVertexAttrib4Niv_    =      (PFNGLVERTEXATTRIB4NIVPROC)         getprocaddress("glVertexAttrib4Niv");
    glVertexAttrib4Nub_    =      (PFNGLVERTEXATTRIB4NUBPROC)         getprocaddress("glVertexAttrib4Nub");
    glVertexAttrib4Nubv_   =      (PFNGLVERTEXATTRIB4NUBVPROC)        getprocaddress("glVertexAttrib4Nubv");
    glVertexAttrib4Nuiv_   =      (PFNGLVERTEXATTRIB4NUIVPROC)        getprocaddress("glVertexAttrib4Nuiv");
    glVertexAttrib4Nusv_   =      (PFNGLVERTEXATTRIB4NUSVPROC)        getprocaddress("glVertexAttrib4Nusv");
    glVertexAttribPointer_ =      (PFNGLVERTEXATTRIBPOINTERPROC)      getprocaddress("glVertexAttribPointer");
    glDrawBuffers_         =      (PFNGLDRAWBUFFERSPROC)              getprocaddress("glDrawBuffers");
    glGetStringi_          =      (PFNGLGETSTRINGIPROC)          getprocaddress("glGetStringi");

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
    glBindVertexArray_    = (PFNGLBINDVERTEXARRAYPROC)   getprocaddress("glBindVertexArray");
    glDeleteVertexArrays_ = (PFNGLDELETEVERTEXARRAYSPROC)getprocaddress("glDeleteVertexArrays");
    glGenVertexArrays_    = (PFNGLGENVERTEXARRAYSPROC)   getprocaddress("glGenVertexArrays");
    glIsVertexArray_      = (PFNGLISVERTEXARRAYPROC)     getprocaddress("glIsVertexArray");

    glBindFragDataLocation_ = (PFNGLBINDFRAGDATALOCATIONPROC)    getprocaddress("glBindFragDataLocation");
    glUniform1ui_     =           (PFNGLUNIFORM1UIPROC)          getprocaddress("glUniform1ui");
    glUniform2ui_     =           (PFNGLUNIFORM2UIPROC)          getprocaddress("glUniform2ui");
    glUniform3ui_     =           (PFNGLUNIFORM3UIPROC)          getprocaddress("glUniform3ui");
    glUniform4ui_     =           (PFNGLUNIFORM4UIPROC)          getprocaddress("glUniform4ui");
    glUniform1uiv_    =           (PFNGLUNIFORM1UIVPROC)         getprocaddress("glUniform1uiv");
    glUniform2uiv_    =           (PFNGLUNIFORM2UIVPROC)         getprocaddress("glUniform2uiv");
    glUniform3uiv_    =           (PFNGLUNIFORM3UIVPROC)         getprocaddress("glUniform3uiv");
    glUniform4uiv_    =           (PFNGLUNIFORM4UIVPROC)         getprocaddress("glUniform4uiv");
    glClearBufferiv_  =           (PFNGLCLEARBUFFERIVPROC)       getprocaddress("glClearBufferiv");
    glClearBufferuiv_ =           (PFNGLCLEARBUFFERUIVPROC)      getprocaddress("glClearBufferuiv");
    glClearBufferfv_  =           (PFNGLCLEARBUFFERFVPROC)       getprocaddress("glClearBufferfv");
    glClearBufferfi_  =           (PFNGLCLEARBUFFERFIPROC)       getprocaddress("glClearBufferfi");
    if(hasext("GL_EXT_gpu_shader4"))
    {
        hasEGPU4 = true;
        if(debugexts)
        {
            conoutf(Console_Init, "Using GL_EXT_gpu_shader4 extension.");
        }
    }
    glClampColor_ = (PFNGLCLAMPCOLORPROC)getprocaddress("glClampColor");
    glColorMaski_ = (PFNGLCOLORMASKIPROC)getprocaddress("glColorMaski");
    glEnablei_    = (PFNGLENABLEIPROC)   getprocaddress("glEnablei");
    glDisablei_   = (PFNGLENABLEIPROC)   getprocaddress("glDisablei");
    glBeginConditionalRender_ = (PFNGLBEGINCONDITIONALRENDERPROC)getprocaddress("glBeginConditionalRender");
    glEndConditionalRender_   = (PFNGLENDCONDITIONALRENDERPROC)  getprocaddress("glEndConditionalRender");
    glTexParameterIiv_        = (PFNGLTEXPARAMETERIIVPROC)       getprocaddress("glTexParameterIiv");
    glTexParameterIuiv_       = (PFNGLTEXPARAMETERIUIVPROC)      getprocaddress("glTexParameterIuiv");
    glGetTexParameterIiv_     = (PFNGLGETTEXPARAMETERIIVPROC)    getprocaddress("glGetTexParameterIiv");
    glGetTexParameterIuiv_    = (PFNGLGETTEXPARAMETERIUIVPROC)   getprocaddress("glGetTexParameterIuiv");

    glBindRenderbuffer_               = (PFNGLBINDRENDERBUFFERPROC)              getprocaddress("glBindRenderbuffer");
    glDeleteRenderbuffers_            = (PFNGLDELETERENDERBUFFERSPROC)           getprocaddress("glDeleteRenderbuffers");
    glGenRenderbuffers_               = (PFNGLGENFRAMEBUFFERSPROC)               getprocaddress("glGenRenderbuffers");
    glRenderbufferStorage_            = (PFNGLRENDERBUFFERSTORAGEPROC)           getprocaddress("glRenderbufferStorage");
    glGetRenderbufferParameteriv_     = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)    getprocaddress("glGetRenderbufferParameteriv");
    glCheckFramebufferStatus_         = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)        getprocaddress("glCheckFramebufferStatus");
    glBindFramebuffer_                = (PFNGLBINDFRAMEBUFFERPROC)               getprocaddress("glBindFramebuffer");
    glDeleteFramebuffers_             = (PFNGLDELETEFRAMEBUFFERSPROC)            getprocaddress("glDeleteFramebuffers");
    glGenFramebuffers_                = (PFNGLGENFRAMEBUFFERSPROC)               getprocaddress("glGenFramebuffers");
    glFramebufferTexture1D_           = (PFNGLFRAMEBUFFERTEXTURE1DPROC)          getprocaddress("glFramebufferTexture1D");
    glFramebufferTexture2D_           = (PFNGLFRAMEBUFFERTEXTURE2DPROC)          getprocaddress("glFramebufferTexture2D");
    glFramebufferTexture3D_           = (PFNGLFRAMEBUFFERTEXTURE3DPROC)          getprocaddress("glFramebufferTexture3D");
    glFramebufferRenderbuffer_        = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)       getprocaddress("glFramebufferRenderbuffer");
    glGenerateMipmap_                 = (PFNGLGENERATEMIPMAPPROC)                getprocaddress("glGenerateMipmap");
    glBlitFramebuffer_                = (PFNGLBLITFRAMEBUFFERPROC)               getprocaddress("glBlitFramebuffer");
    glRenderbufferStorageMultisample_ = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)getprocaddress("glRenderbufferStorageMultisample");

    glMapBufferRange_         = (PFNGLMAPBUFFERRANGEPROC)        getprocaddress("glMapBufferRange");
    glFlushMappedBufferRange_ = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)getprocaddress("glFlushMappedBufferRange");
    //OpenGL 3.1
    glGetUniformIndices_       = (PFNGLGETUNIFORMINDICESPROC)      getprocaddress("glGetUniformIndices");
    glGetActiveUniformsiv_     = (PFNGLGETACTIVEUNIFORMSIVPROC)    getprocaddress("glGetActiveUniformsiv");
    glGetUniformBlockIndex_    = (PFNGLGETUNIFORMBLOCKINDEXPROC)   getprocaddress("glGetUniformBlockIndex");
    glGetActiveUniformBlockiv_ = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC)getprocaddress("glGetActiveUniformBlockiv");
    glUniformBlockBinding_     = (PFNGLUNIFORMBLOCKBINDINGPROC)    getprocaddress("glUniformBlockBinding");
    glBindBufferBase_          = (PFNGLBINDBUFFERBASEPROC)         getprocaddress("glBindBufferBase");
    glBindBufferRange_         = (PFNGLBINDBUFFERRANGEPROC)        getprocaddress("glBindBufferRange");
    useubo = 1;
    glCopyBufferSubData_ = (PFNGLCOPYBUFFERSUBDATAPROC)getprocaddress("glCopyBufferSubData");
    //OpenGL 3.2
    glTexImage2DMultisample_ = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)getprocaddress("glTexImage2DMultisample");
    glTexImage3DMultisample_ = (PFNGLTEXIMAGE3DMULTISAMPLEPROC)getprocaddress("glTexImage3DMultisample");
    glGetMultisamplefv_      = (PFNGLGETMULTISAMPLEFVPROC)     getprocaddress("glGetMultisamplefv");
    glSampleMaski_           = (PFNGLSAMPLEMASKIPROC)          getprocaddress("glSampleMaski");
    if(hasext("GL_EXT_framebuffer_multisample_blit_scaled"))
    {
        hasFBMSBS = true;
        if(debugexts)
        {
            conoutf(Console_Init, "Using GL_EXT_framebuffer_multisample_blit_scaled extension.");
        }
    }

    if(hasext("GL_EXT_timer_query"))
    {
        glGetQueryObjecti64v_  =  (PFNGLGETQUERYOBJECTI64VEXTPROC)  getprocaddress("glGetQueryObjecti64vEXT");
        glGetQueryObjectui64v_ = (PFNGLGETQUERYOBJECTUI64VEXTPROC) getprocaddress("glGetQueryObjectui64vEXT");
        hasTQ = true;
        if(debugexts)
        {
            conoutf(Console_Init, "Using GL_EXT_timer_query extension.");
        }
    }
    //OpenGL 3.3
    glGetQueryObjecti64v_ =  (PFNGLGETQUERYOBJECTI64VEXTPROC)  getprocaddress("glGetQueryObjecti64v");
    glGetQueryObjectui64v_ = (PFNGLGETQUERYOBJECTUI64VEXTPROC) getprocaddress("glGetQueryObjectui64v");
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
    glBindFragDataLocationIndexed_ = (PFNGLBINDFRAGDATALOCATIONINDEXEDPROC)getprocaddress("glBindFragDataLocationIndexed");
    GLint dualbufs = 0;
    glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, &dualbufs);
    maxdualdrawbufs = dualbufs;
    //OpenGL 4.0
    glMinSampleShading_       = (PFNGLMINSAMPLESHADINGPROC)      getprocaddress("glMinSampleShading");
    glBlendEquationi_         = (PFNGLBLENDEQUATIONIPROC)        getprocaddress("glBlendEquationi");
    glBlendEquationSeparatei_ = (PFNGLBLENDEQUATIONSEPARATEIPROC)getprocaddress("glBlendEquationSeparatei");
    glBlendFunci_             = (PFNGLBLENDFUNCIPROC)            getprocaddress("glBlendFunci");
    glBlendFuncSeparatei_     = (PFNGLBLENDFUNCSEPARATEIPROC)    getprocaddress("glBlendFuncSeparatei");
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

    if(glversion >= 430)
    {
        glDebugMessageControl_  = (PFNGLDEBUGMESSAGECONTROLPROC) getprocaddress("glDebugMessageControl");
        glDebugMessageInsert_   = (PFNGLDEBUGMESSAGEINSERTPROC)  getprocaddress("glDebugMessageInsert");
        glDebugMessageCallback_ = (PFNGLDEBUGMESSAGECALLBACKPROC)getprocaddress("glDebugMessageCallback");
        glGetDebugMessageLog_   = (PFNGLGETDEBUGMESSAGELOGPROC)  getprocaddress("glGetDebugMessageLog");
        hasDBGO = true;
    }
    else
    {
        if(hasext("GL_ARB_debug_output"))
        {
            glDebugMessageControl_ =  (PFNGLDEBUGMESSAGECONTROLPROC) getprocaddress("glDebugMessageControlARB");
            glDebugMessageInsert_ =   (PFNGLDEBUGMESSAGEINSERTPROC)  getprocaddress("glDebugMessageInsertARB");
            glDebugMessageCallback_ = (PFNGLDEBUGMESSAGECALLBACKPROC)getprocaddress("glDebugMessageCallbackARB");
            glGetDebugMessageLog_ =   (PFNGLGETDEBUGMESSAGELOGPROC)  getprocaddress("glGetDebugMessageLogARB");
            hasDBGO = true;
            if(debugexts)
            {
                conoutf(Console_Init, "Using GL_ARB_debug_output extension.");
            }
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
COMMAND(glext, "s");

void gl_resize()
{
    gl_setupframe();
    glViewport(0, 0, hudw, hudh);
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

void getcamyaw()
{
    floatret(camera1->yaw);
}
COMMAND(getcamyaw, "");

void getcampitch()
{
    floatret(camera1->pitch);
}
COMMAND(getcampitch, "");

void getcamroll()
{
    floatret(camera1->roll);
}
COMMAND(getcamroll, "");

void getcampos()
{
    DEF_FORMAT_STRING(pos, "%s %s %s", floatstr(camera1->o.x), floatstr(camera1->o.y), floatstr(camera1->o.z));
    result(pos);
}
COMMAND(getcampos, "");

vec worldpos, camdir, camright, camup;

void setcammatrix()
{
    // move from RH to Z-up LH quake style worldspace
    cammatrix = viewmatrix;
    cammatrix.rotate_around_y(camera1->roll*RAD);
    cammatrix.rotate_around_x(camera1->pitch*-RAD);
    cammatrix.rotate_around_z(camera1->yaw*-RAD);
    cammatrix.translate(vec(camera1->o).neg());

    cammatrix.transposedtransformnormal(vec(viewmatrix.b), camdir);
    cammatrix.transposedtransformnormal(vec(viewmatrix.a).neg(), camright);
    cammatrix.transposedtransformnormal(vec(viewmatrix.c), camup);

    if(!drawtex)
    {
        if(raycubepos(camera1->o, camdir, worldpos, 0, Ray_ClipMat|Ray_SkipFirst) == -1)
        {
            worldpos = vec(camdir).mul(2*worldsize).add(camera1->o); // if nothing is hit, just far away in the view direction
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
    if(init)
    {
        invcammatrix.invert(cammatrix);
        invprojmatrix.invert(projmatrix);
        invcamprojmatrix.invert(camprojmatrix);
    }
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

void pushhudscale(float sx, float sy)
{
    if(!sy)
    {
        sy = sx;
    }
    pushhudmatrix();
    hudmatrix.scale(sx, sy, 1);
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

float curfov, curavatarfov, fovy, aspect;
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
VARP(invmouse, 0, 0, 1);
FVARP(mouseaccel, 0, 0, 1000);

physent *camera1 = nullptr;
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

float calcfrustumboundsphere(float nearplane, float farplane,  const vec &pos, const vec &view, vec &center)
{
    if(drawtex == Draw_TexMinimap)
    {
        center = minimapcenter;
        return minimapradius.magnitude();
    }

    float width = std::tan(fov/2.0f*RAD),
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

matrix4 cammatrix, projmatrix, camprojmatrix, invcammatrix, invcamprojmatrix, invprojmatrix;

FVAR(nearplane, 0.01f, 0.54f, 2.0f);

vec calcavatarpos(const vec &pos, float dist)
{
    vec eyepos;
    cammatrix.transform(pos, eyepos);
    GLdouble ydist = nearplane * std::tan(curavatarfov/2*RAD),
             xdist = ydist * aspect;
    vec4<float> scrpos;
    scrpos.x = eyepos.x*nearplane/xdist;
    scrpos.y = eyepos.y*nearplane/ydist;
    scrpos.z = (eyepos.z*(farplane + nearplane) - 2*nearplane*farplane) / (farplane - nearplane);
    scrpos.w = -eyepos.z;

    vec worldpos = invcamprojmatrix.perspectivetransform(scrpos);
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
          focaldist = 1.0f/std::tan(fovy*0.5f*RAD);
    sx1 = sy1 = -1;
    sx2 = sy2 = 1;
    #define CHECKPLANE(c, dir, focaldist, low, high) \
    do { \
        float nzc = (cz*cz + 1) / (cz dir drt) - cz, \
              pz = (d##c)/(nzc*e.c - e.z); \
        if(pz > 0) \
        { \
            float c = (focaldist)*nzc, \
                  pc = pz*nzc; \
            if(pc < e.c) \
            { \
                low = c; \
            } \
            else if(pc > e.c) \
            { \
                high = c; \
            } \
        } \
    } while(0)
    if(dx > 0)
    {
        float cz  = e.x/e.z,
              drt = sqrtf(dx)/size;
        CHECKPLANE(x, -, focaldist/aspect, sx1, sx2);
        CHECKPLANE(x, +, focaldist/aspect, sx1, sx2);
    }
    if(dy > 0)
    {
        float cz  = e.y/e.z,
              drt = sqrtf(dy)/size;
        CHECKPLANE(y, -, focaldist, sy1, sy2);
        CHECKPLANE(y, +, focaldist, sy1, sy2);
    }
    float z1 = std::min(e.z + size, -1e-3f - nearplane),
          z2 = std::min(e.z - size, -1e-3f - nearplane);
    sz1 = (z1*projmatrix.c.z + projmatrix.d.z) / (z1*projmatrix.c.w + projmatrix.d.w);
    sz2 = (z2*projmatrix.c.z + projmatrix.d.z) / (z2*projmatrix.c.w + projmatrix.d.w);
    return sx1 < sx2 && sy1 < sy2 && sz1 < sz2;
}

#undef CHECKPLANE

bool calcbbscissor(const ivec &bbmin, const ivec &bbmax, float &sx1, float &sy1, float &sx2, float &sy2)
{
#define ADDXYSCISSOR(p) do { \
        if(p.z >= -p.w) \
        { \
            float x = p.x / p.w, \
                  y = p.y / p.w; \
            sx1 = std::min(sx1, x); \
            sy1 = std::min(sy1, y); \
            sx2 = std::max(sx2, x); \
            sy2 = std::max(sy2, y); \
        } \
    } while(0)
    vec4<float> v[8];
    sx1 = sy1 = 1;
    sx2 = sy2 = -1;
    camprojmatrix.transform(vec(bbmin.x, bbmin.y, bbmin.z), v[0]);
    ADDXYSCISSOR(v[0]);
    camprojmatrix.transform(vec(bbmax.x, bbmin.y, bbmin.z), v[1]);
    ADDXYSCISSOR(v[1]);
    camprojmatrix.transform(vec(bbmin.x, bbmax.y, bbmin.z), v[2]);
    ADDXYSCISSOR(v[2]);
    camprojmatrix.transform(vec(bbmax.x, bbmax.y, bbmin.z), v[3]);
    ADDXYSCISSOR(v[3]);
    camprojmatrix.transform(vec(bbmin.x, bbmin.y, bbmax.z), v[4]);
    ADDXYSCISSOR(v[4]);
    camprojmatrix.transform(vec(bbmax.x, bbmin.y, bbmax.z), v[5]);
    ADDXYSCISSOR(v[5]);
    camprojmatrix.transform(vec(bbmin.x, bbmax.y, bbmax.z), v[6]);
    ADDXYSCISSOR(v[6]);
    camprojmatrix.transform(vec(bbmax.x, bbmax.y, bbmax.z), v[7]);
    ADDXYSCISSOR(v[7]);
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
    float spotscale = radius * tan360(spot);
    vec up     = vec(spotx).mul(spotscale),
        right  = vec(spoty).mul(spotscale),
        center = vec(dir).mul(radius).add(origin);
//================================================================ ADDXYZSCISSOR
#define ADDXYZSCISSOR(p) do { \
        if(p.z >= -p.w) \
        { \
            float x = p.x / p.w, \
                  y = p.y / p.w, \
                  z = p.z / p.w; \
            sx1 = std::min(sx1, x); \
            sy1 = std::min(sy1, y); \
            sz1 = std::min(sz1, z); \
            sx2 = std::max(sx2, x); \
            sy2 = std::max(sy2, y); \
            sz2 = std::max(sz2, z); \
        } \
    } while(0)
    vec4<float> v[5];
    sx1 = sy1 = sz1 = 1;
    sx2 = sy2 = sz2 = -1;
    camprojmatrix.transform(vec(center).sub(right).sub(up), v[0]);
    ADDXYZSCISSOR(v[0]);
    camprojmatrix.transform(vec(center).add(right).sub(up), v[1]);
    ADDXYZSCISSOR(v[1]);
    camprojmatrix.transform(vec(center).sub(right).add(up), v[2]);
    ADDXYZSCISSOR(v[2]);
    camprojmatrix.transform(vec(center).add(right).add(up), v[3]);
    ADDXYZSCISSOR(v[3]);
    camprojmatrix.transform(origin, v[4]);
    ADDXYZSCISSOR(v[4]);
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

    #undef ADDXYZSCISSOR
//==============================================================================

//============================================================= INTERPXYZSCISSOR
    #define INTERPXYZSCISSOR(p, o) do { \
        float t = (p.z + p.w)/(p.z + p.w - o.z - o.w), \
              w = p.w + t*(o.w - p.w), \
              x = (p.x + t*(o.x - p.x))/w, \
              y = (p.y + t*(o.y - p.y))/w; \
        sx1 = std::min(sx1, x); \
        sy1 = std::min(sy1, y); \
        sz1 = std::min(sz1, -1.0f); \
        sx2 = std::max(sx2, x); \
        sy2 = std::max(sy2, y); \
        } while(0)

            INTERPXYZSCISSOR(p, o);
        }
        if(v[4].z > -v[4].w)
        {
            INTERPXYZSCISSOR(p, v[4]);
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
            INTERPXYZSCISSOR(v[4], o);
        }
    }

    #undef INTERPXYZSCISSOR
//==============================================================================

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
        glGenBuffers_(1, &screenquadvbo);
        gle::bindvbo(screenquadvbo);
        vec2 verts[4] = { vec2(1, -1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1) };
        glBufferData_(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        gle::clearvbo();
    }
}

static void cleanupscreenquad()
{
    if(screenquadvbo)
    {
        glDeleteBuffers_(1, &screenquadvbo);
        screenquadvbo = 0;
    }
}

void screenquad()
{
    setupscreenquad();
    gle::bindvbo(screenquadvbo);
    gle::enablevertex();
    gle::vertexpointer(sizeof(vec2), (const vec2 *)0, GL_FLOAT, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gle::disablevertex();
    gle::clearvbo();
}

static LocalShaderParam screentexcoord[2] = { LocalShaderParam("screentexcoord0"), LocalShaderParam("screentexcoord1") };

static void setscreentexcoord(int i, float w, float h, float x = 0, float y = 0)
{
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
        cube &c = rootworld.lookupcube(o, 0, co, csize);
        int mat = c.material&MatFlag_Volume;
        if(mat != fogmat)
        {
            abovemat = IS_LIQUID(mat) ? c.material : Mat_Air;
            return o.z;
        }
        o.z = co.z + csize;
    } while(o.z < worldsize);
    abovemat = Mat_Air;
    return worldsize;
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

vec curfogcolor(0, 0, 0);

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

GLuint minimaptex = 0;
vec minimapcenter(0, 0, 0),
    minimapradius(0, 0, 0),
    minimapscale(0, 0, 0);

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
VARP(minimapsize, 7, 9, 10);      //2^n size of the minimap texture (along edge)
VARP(showminimap, 0, 1, 1);
CVARP(nominimapcolor, 0x101010);  //color for the part of the minimap that isn't the map texture

void bindminimap()
{
    glBindTexture(GL_TEXTURE_2D, minimaptex);
}

void clipminimap(ivec &bbmin, ivec &bbmax, cube *c, const ivec &co = ivec(0, 0, 0), int size = worldsize>>1)
{
    for(int i = 0; i < 8; ++i)
    {
        ivec o(i, co, size);
        if(c[i].children)
        {
            clipminimap(bbmin, bbmax, c[i].children, o, size>>1);
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

void drawminimap(int yaw, int pitch, vec loc, cubeworld world)
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
        size /= 2;
    }
    if(!minimaptex)
    {
        glGenTextures(1, &minimaptex);
    }
    ivec bbmin(worldsize, worldsize, worldsize),
         bbmax(0, 0, 0);
    for(int i = 0; i < valist.length(); i++)
    {
        vtxarray *va = valist[i];
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
        ivec clipmin(worldsize, worldsize, worldsize),
             clipmax(0, 0, 0);
        clipminimap(clipmin, clipmax, world.worldroot);
        for(int k = 0; k < 2; ++k)
        {
            bbmin[k] = std::max(bbmin[k], clipmin[k]);
        }
        for(int k = 0; k < 2; ++k)
        {
            bbmax[k] = std::min(bbmax[k], clipmax[k]);
        }
    }

    minimapradius = vec(bbmax).sub(vec(bbmin));
    minimapcenter = loc;
    minimapradius.x = minimapradius.y = std::max(minimapradius.x, minimapradius.y);
    minimapscale = vec((0.5f - 1.0f/size)/minimapradius.x, (0.5f - 1.0f/size)/minimapradius.y, 1.0f);

    physent *oldcamera = camera1;
    physent cmcamera = *player;
    cmcamera.reset();
    cmcamera.type = PhysEnt_Camera;
    cmcamera.o = loc;
    cmcamera.yaw = yaw;
    cmcamera.pitch = pitch;
    cmcamera.roll = 0;
    camera1 = &cmcamera;

    float oldldrscale = ldrscale,
          oldldrscaleb = ldrscaleb;
    int oldfarplane = farplane,
        oldvieww    = vieww,
        oldviewh    = viewh;
    farplane = worldsize*2;
    vieww = viewh = size;

    float zscale = std::max(static_cast<float>(minimapheight), minimapcenter.z + minimapradius.z + 1) + 1;

    projmatrix.ortho(-minimapradius.x, minimapradius.x, -minimapradius.y, minimapradius.y, 0, 2*zscale);
    setcamprojmatrix();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    xtravertsva = xtraverts = glde = gbatches = vtris = vverts = 0;
    flipqueries();

    ldrscale = 1;
    ldrscaleb = ldrscale/255;

    view.visiblecubes(false);
    rendergbuffer();
    rendershadowatlas();

    gbuf.shademinimap(minimapcolor.tocolor().mul(ldrscale));

    if(minimapheight > 0 && minimapheight < minimapcenter.z + minimapradius.z)
    {
        camera1->o.z = minimapcenter.z + minimapradius.z + 1;
        projmatrix.ortho(-minimapradius.x, minimapradius.x, -minimapradius.y, minimapradius.y, -zscale, zscale);
        setcamprojmatrix();
        rendergbuffer(false);
        gbuf.shademinimap();
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    farplane = oldfarplane;
    vieww = oldvieww;
    viewh = oldviewh;
    ldrscale = oldldrscale;
    ldrscaleb = oldldrscaleb;

    camera1 = oldcamera;
    drawtex = 0;

    createtexture(minimaptex, size, size, nullptr, 3, 1, GL_RGB5, GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat border[4] = { minimapcolor.x/255.0f, minimapcolor.y/255.0f, minimapcolor.z/255.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint fbo = 0;
    glGenFramebuffers_(1, &fbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, minimaptex, 0);
    copyhdr(size, size, fbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers_(1, &fbo);

    glViewport(0, 0, hudw, hudh);
}

VAR(modelpreviewfov, 10, 20, 100);    //y axis field of view
VAR(modelpreviewpitch, -90, -15, 90); //pitch above model to render

/* ======================== model preview windows =========================== */

namespace modelpreview
{
    physent *oldcamera;
    physent camera;

    float oldaspect, oldfovy, oldfov, oldldrscale, oldldrscaleb;
    int oldfarplane, oldvieww, oldviewh;
    matrix4 oldprojmatrix;

    int x, y, w, h;
    bool background, scissor;

    void start(int x, int y, int w, int h, bool background, bool scissor)
    {
        modelpreview::x = x;
        modelpreview::y = y;
        modelpreview::w = w;
        modelpreview::h = h;
        modelpreview::background = background;
        modelpreview::scissor = scissor;

        gbuf.setupgbuffer();

        useshaderbyname("modelpreview");

        drawtex = Draw_TexModelPreview;

        oldcamera = camera1;
        camera = *camera1;
        camera.reset();
        camera.type = PhysEnt_Camera;
        camera.o = vec(0, 0, 0);
        camera.yaw = 0;
        camera.pitch = modelpreviewpitch;
        camera.roll = 0;
        camera1 = &camera;

        oldaspect = aspect;
        oldfovy = fovy;
        oldfov = curfov;
        oldldrscale = ldrscale;
        oldldrscaleb = ldrscaleb;
        oldfarplane = farplane;
        oldvieww = vieww;
        oldviewh = viewh;
        oldprojmatrix = projmatrix;

        aspect = w/static_cast<float>(h);
        fovy = modelpreviewfov;
        curfov = 2*std::atan2(std::tan(fovy/2*RAD), 1/aspect)/RAD;
        farplane = 1024;
        vieww = std::min(gw, w);
        viewh = std::min(gh, h);
        ldrscale = 1;
        ldrscaleb = ldrscale/255;

        projmatrix.perspective(fovy, aspect, nearplane, farplane);
        setcamprojmatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    void end()
    {
        rendermodelbatches();

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
        ldrscaleb = oldldrscaleb;

        camera1 = oldcamera;
        drawtex = 0;

        projmatrix = oldprojmatrix;
        setcamprojmatrix();
    }
}

vec calcmodelpreviewpos(const vec &radius, float &yaw)
{
    yaw = std::fmod(lastmillis/10000.0f*360.0f, 360.0f);
    float dist = std::max(radius.magnitude2()/aspect, radius.magnitude())/std::sin(fovy/2*RAD);
    return vec(0, dist, 0).rotate_around_x(camera1->pitch*RAD);
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

    farplane = worldsize*2;
    //set the camera location
    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    ldrscale = 0.5f;
    ldrscaleb = ldrscale/255;
    //do occlusion culling
    view.visiblecubes();
    //set to wireframe if applicable
    if(wireframe && editmode)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    //construct g-buffer (build basic scene)
    rendergbuffer(true, gamefxn);
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
    rendershadowatlas();
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
    doaa(setuppostfx(vieww, viewh, scalefbo), gbuf);
    //postfx
    renderpostfx(scalefbo);
    if(scalefbo)
    {
        gbuf.doscale();
    }
}

int renderw = 0,
    renderh = 0,
    hudw = 0,
    hudh = 0;

void gl_setupframe(bool force)
{
    renderw = std::min(scr_w, screenw);
    renderh = std::min(scr_h, screenh);
    hudw = screenw;
    hudh = screenh;
    if(!force)
    {
        return;
    }
    setuplights();
}

void gl_drawframe(int crosshairindex, void (*gamefxn)(), void (*hudfxn)(), void (*editfxn)(), void (*hud2d)())
{
    synctimers();
    xtravertsva = xtraverts = glde = gbatches = vtris = vverts = 0;
    flipqueries();
    aspect = forceaspect ? forceaspect : hudw/static_cast<float>(hudh);
    fovy = 2*std::atan2(std::tan(curfov/2*RAD), aspect)/RAD;
    vieww = hudw;
    viewh = hudh;
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


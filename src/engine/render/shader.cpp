// shader.cpp: OpenGL GLSL shader management

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include "octarender.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "rendertimers.h"
#include "renderwindow.h"
#include "texture.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/menus.h"

Shader *Shader::lastshader = nullptr;

Shader *nullshader            = nullptr,
       *hudshader             = nullptr,
       *hudtextshader         = nullptr,
       *hudnotextureshader    = nullptr,
       *nocolorshader         = nullptr,
       *foggedshader          = nullptr,
       *foggednotextureshader = nullptr,
       *ldrshader             = nullptr,
       *ldrnotextureshader    = nullptr,
       *stdworldshader        = nullptr;

static hashnameset<GlobalShaderParamState> globalparams(256);
static hashtable<const char *, int> localparams(256);
static hashnameset<Shader> shaders(256);
static Shader *slotshader = nullptr;
static vector<SlotShaderParam> slotparams;
static bool standardshaders = false,
            forceshaders = true,
            loadedshaders = false;

VAR(maxvsuniforms, 1, 0, 0);
VAR(maxfsuniforms, 1, 0, 0);
VAR(mintexoffset, 1, 0, 0);
VAR(maxtexoffset, 1, 0, 0);
VAR(mintexrectoffset, 1, 0, 0);
VAR(maxtexrectoffset, 1, 0, 0);
VAR(debugshader, 0, 1, 2);

void loadshaders()
{
    standardshaders = true;
    execfile("config/glsl.cfg");
    standardshaders = false;

    nullshader = lookupshaderbyname("null");
    hudshader = lookupshaderbyname("hud");
    hudtextshader = lookupshaderbyname("hudtext");
    hudnotextureshader = lookupshaderbyname("hudnotexture");
    stdworldshader = lookupshaderbyname("stdworld");
    if(!nullshader || !hudshader || !hudtextshader || !hudnotextureshader || !stdworldshader)
    {
        fatal("cannot find shader definitions");
    }
    dummyslot.shader = stdworldshader;
    dummydecalslot.shader = nullshader;

    nocolorshader = lookupshaderbyname("nocolor");
    foggedshader = lookupshaderbyname("fogged");
    foggednotextureshader = lookupshaderbyname("foggednotexture");
    ldrshader = lookupshaderbyname("ldr");
    ldrnotextureshader = lookupshaderbyname("ldrnotexture");

    nullshader->set();

    loadedshaders = true;
}

Shader *lookupshaderbyname(const char *name)
{
    Shader *s = shaders.access(name);
    return s && s->loaded() ? s : nullptr;
}

Shader *generateshader(const char *name, const char *fmt, ...)
{
    if(!loadedshaders)
    {
        return nullptr;
    }
    Shader *s = name ? lookupshaderbyname(name) : nullptr;
    if(!s)
    {
        DEFV_FORMAT_STRING(cmd, fmt, fmt);
        bool wasstandard = standardshaders;
        standardshaders = true;
        execute(cmd);
        standardshaders = wasstandard;
        s = name ? lookupshaderbyname(name) : nullptr;
        if(!s)
        {
            s = nullshader;
        }
    }
    return s;
}

static void showglslinfo(GLenum type, GLuint obj, const char *name, const char **parts = nullptr, int numparts = 0)
{
    GLint length = 0;
    if(type)
    {
        glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
    }
    else
    {
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &length);
    }
    if(length > 1)
    {
        conoutf(Console_Error, "GLSL ERROR (%s:%s)", type == GL_VERTEX_SHADER ? "Vertex shader" : (type == GL_FRAGMENT_SHADER ? "Fragment shader" : "PROG"), name);
    }
}

static void compileglslshader(Shader &s, GLenum type, GLuint &obj, const char *def, const char *name, bool msg = true)
{
    const char *source = def + std::strspn(def, " \t\r\n");
    char *modsource = nullptr;
    const char *parts[16];
    int numparts = 0;
    static const struct { int version; const char * const header; } glslversions[] =
    {
        { 400, "#version 400\n" }, //OpenGL 4.0
        { 330, "#version 330\n" }, //OpenGL 3.3 (not supported)
        { 150, "#version 150\n" }, //OpenGL 3.2 (not supported)
        { 140, "#version 140\n" }, //OpenGL 3.1 (not supported)
        { 130, "#version 130\n" }, //OpenGL 3.0 (not supported)
        { 120, "#version 120\n" }  //OpenGL 2.1 (not supported)
    };
    for(int i = 0; i < static_cast<int>(sizeof(glslversions)/sizeof(glslversions[0])); ++i)
    {
        if(glslversion >= glslversions[i].version)
        {
            parts[numparts++] = glslversions[i].header;
            break;
        }
    }

        parts[numparts++] = "#extension GL_ARB_explicit_attrib_location : enable\n";
    //glsl 1.5
    if(type == GL_VERTEX_SHADER) parts[numparts++] =
        "#define attribute in\n"
        "#define varying out\n";
    else if(type == GL_FRAGMENT_SHADER)
    {
        parts[numparts++] = "#define varying in\n";
        parts[numparts++] =
            "#define fragdata(loc) layout(location = loc) out\n"
            "#define fragblend(loc) layout(location = loc, index = 1) out\n";
    }
    parts[numparts++] =
        "#define texture1D(sampler, coords) texture(sampler, coords)\n"
        "#define texture2D(sampler, coords) texture(sampler, coords)\n"
        "#define texture2DOffset(sampler, coords, offset) textureOffset(sampler, coords, offset)\n"
        "#define texture2DProj(sampler, coords) textureProj(sampler, coords)\n"
        "#define shadow2D(sampler, coords) texture(sampler, coords)\n"
        "#define shadow2DOffset(sampler, coords, offset) textureOffset(sampler, coords, offset)\n"
        "#define texture3D(sampler, coords) texture(sampler, coords)\n"
        "#define textureCube(sampler, coords) texture(sampler, coords)\n";
    //glsl 1.4
    parts[numparts++] =
        "#define texture2DRect(sampler, coords) texture(sampler, coords)\n"
        "#define texture2DRectProj(sampler, coords) textureProj(sampler, coords)\n"
        "#define shadow2DRect(sampler, coords) texture(sampler, coords)\n";
    parts[numparts++] =
        "#define texture2DRectOffset(sampler, coords, offset) textureOffset(sampler, coords, offset)\n"
        "#define shadow2DRectOffset(sampler, coords, offset) textureOffset(sampler, coords, offset)\n";
    parts[numparts++] = modsource ? modsource : source;
    //end glsl 1.4
    obj = glCreateShader(type);
    glShaderSource(obj, numparts, (const GLchar **)parts, nullptr);
    glCompileShader(obj);
    GLint success;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        if(msg)
        {
            showglslinfo(type, obj, name, parts, numparts);
        }
        glDeleteShader(obj);
        obj = 0;
    }
    else if(debugshader > 1 && msg)
    {
        showglslinfo(type, obj, name, parts, numparts);
    }
    if(modsource)
    {
        delete[] modsource;
    }
}

VAR(debugubo, 0, 0, 1); //print out to console information about ubos when bindglsluniform is called

static void bindglsluniform(Shader &s, UniformLoc &u)
{
    u.loc = glGetUniformLocation(s.program, u.name);
    if(!u.blockname)
    {
        return;
    }
    GLuint bidx = glGetUniformBlockIndex(s.program, u.blockname),
           uidx = GL_INVALID_INDEX;
    glGetUniformIndices(s.program, 1, &u.name, &uidx);
    if(bidx != GL_INVALID_INDEX && uidx != GL_INVALID_INDEX)
    {
        GLint sizeval   = 0,
              offsetval = 0,
              strideval = 0;
        glGetActiveUniformBlockiv(s.program, bidx, GL_UNIFORM_BLOCK_DATA_SIZE, &sizeval);
        if(sizeval <= 0)
        {
            return;
        }
        glGetActiveUniformsiv(s.program, 1, &uidx, GL_UNIFORM_OFFSET, &offsetval);
        if(u.stride > 0)
        {
            glGetActiveUniformsiv(s.program, 1, &uidx, GL_UNIFORM_ARRAY_STRIDE, &strideval);
            if(strideval > u.stride)
            {
                return;
            }
        }
        u.offset = offsetval;
        u.size = sizeval;
        glUniformBlockBinding_(s.program, bidx, u.binding);
        if(debugubo)
        {
            conoutf(Console_Debug, "UBO: %s:%s:%d, offset: %d, size: %d, stride: %d", u.name, u.blockname, u.binding, offsetval, sizeval, strideval);
        }
    }
}

static void uniformtex(const char * name, int tmu, Shader &s) \
{ \
    do { \
        int loc = glGetUniformLocation(s.program, name); \
        if(loc != -1) \
        { \
            glUniform1i(loc, tmu); \
        } \
    } while(0);
}

static void bindworldtexlocs(Shader &s)
{
    uniformtex("diffusemap", Tex_Diffuse, s);
    uniformtex("normalmap", Tex_Normal, s);
    uniformtex("glowmap", Tex_Glow, s);
    uniformtex("blendmap", 7, s);
    uniformtex("refractmask", 7, s);
    uniformtex("refractlight", 8, s);
}

static void linkglslprogram(Shader &s, bool msg = true)
{
    s.program = s.vsobj && s.psobj ? glCreateProgram() : 0;
    GLint success = 0;
    if(s.program)
    {
        glAttachShader(s.program, s.vsobj);
        glAttachShader(s.program, s.psobj);
        uint attribs = 0;
        for(int i = 0; i < s.attriblocs.length(); i++)
        {
            AttribLoc &a = s.attriblocs[i];
            glBindAttribLocation(s.program, a.loc, a.name);
            attribs |= 1<<a.loc;
        }
        for(int i = 0; i < gle::Attribute_NumAttributes; ++i)
        {
            if(!(attribs&(1<<i)))
            {
                glBindAttribLocation(s.program, i, gle::attribnames[i]);
            }
        }
        glLinkProgram(s.program);
        glGetProgramiv(s.program, GL_LINK_STATUS, &success);
    }
    if(success)
    {
        glUseProgram(s.program);
        for(int i = 0; i < 16; ++i)
        {
            static const char * const texnames[16] = { "tex0", "tex1", "tex2", "tex3", "tex4", "tex5", "tex6", "tex7", "tex8", "tex9", "tex10", "tex11", "tex12", "tex13", "tex14", "tex15" };
            GLint loc = glGetUniformLocation(s.program, texnames[i]);
            if(loc != -1)
            {
                glUniform1i(loc, i);
            }
        }
        if(s.type & Shader_World)
        {
            bindworldtexlocs(s);
        }
        for(int i = 0; i < s.defaultparams.length(); i++)
        {
            SlotShaderParamState &param = s.defaultparams[i];
            param.loc = glGetUniformLocation(s.program, param.name);
        }
        for(int i = 0; i < s.uniformlocs.length(); i++)
        {
            bindglsluniform(s, s.uniformlocs[i]);
        }
        glUseProgram(0);
    }
    else if(s.program)
    {
        if(msg)
        {
            showglslinfo(GL_FALSE, s.program, s.name);
        }
        glDeleteProgram(s.program);
        s.program = 0;
    }
}

int getlocalparam(const char *name)
{
    return localparams.access(name, static_cast<int>(localparams.numelems));
}

static int addlocalparam(Shader &s, const char *name, int loc, int size, GLenum format)
{
    int idx = getlocalparam(name);
    if(idx >= s.localparamremap.length())
    {
        int n = idx + 1 - s.localparamremap.length();
        memset(s.localparamremap.pad(n), 0xFF, n);
    }
    s.localparamremap[idx] = s.localparams.length();
    LocalShaderParamState &l = s.localparams.add();
    l.name = name;
    l.loc = loc;
    l.size = size;
    l.format = format;
    return idx;
}

GlobalShaderParamState *getglobalparam(const char *name)
{
    GlobalShaderParamState *param = globalparams.access(name);
    if(!param)
    {
        param = &globalparams[name];
        param->name = name;
        memset(param->buf, -1, sizeof(param->buf));
        param->version = -1;
    }
    return param;
}

static void addglobalparam(Shader &s, GlobalShaderParamState *param, int loc, int size, GLenum format)
{
    GlobalShaderParamUse &g = s.globalparams.add();
    g.param = param;
    g.version = -2;
    g.loc = loc;
    g.size = size;
    g.format = format;
}

static void setglsluniformformat(Shader &s, const char *name, GLenum format, int size)
{
    switch(format)
    {
        case GL_FLOAT:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_VEC4:
        case GL_INT:
        case GL_INT_VEC2:
        case GL_INT_VEC3:
        case GL_INT_VEC4:
        case GL_UNSIGNED_INT:
        case GL_UNSIGNED_INT_VEC2:
        case GL_UNSIGNED_INT_VEC3:
        case GL_UNSIGNED_INT_VEC4:
        case GL_BOOL:
        case GL_BOOL_VEC2:
        case GL_BOOL_VEC3:
        case GL_BOOL_VEC4:
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT4:
        {
            break;
        }
        default:
        {
            return;
        }
    }
    if(!std::strncmp(name, "gl_", 3))
    {
        return;
    }
    int loc = glGetUniformLocation(s.program, name);
    if(loc < 0)
    {
        return;
    }
    for(int j = 0; j < s.defaultparams.length(); j++)
    {
        if(s.defaultparams[j].loc == loc)
        {
            s.defaultparams[j].format = format;
            return;
        }
    }
    for(int j = 0; j < s.uniformlocs.length(); j++)
    {
        if(s.uniformlocs[j].loc == loc)
        {
            return;
        }
    }
    for(int j = 0; j < s.globalparams.length(); j++)
    {
        if(s.globalparams[j].loc == loc)
        {
            return;
        }
    }
    for(int j = 0; j < s.localparams.length(); j++)
    {
        if(s.localparams[j].loc == loc)
        {
            return;
        }
    }

    name = getshaderparamname(name);
    GlobalShaderParamState *param = globalparams.access(name);
    if(param)
    {
        addglobalparam(s, param, loc, size, format);
    }
    else
    {
        addlocalparam(s, name, loc, size, format);
    }
}

static void allocglslactiveuniforms(Shader &s)
{
    GLint numactive = 0;
    glGetProgramiv(s.program, GL_ACTIVE_UNIFORMS, &numactive);
    string name;
    for(int i = 0; i < numactive; ++i)
    {
        GLsizei namelen = 0;
        GLint size = 0;
        GLenum format = GL_FLOAT_VEC4;
        name[0] = '\0';
        glGetActiveUniform(s.program, i, sizeof(name)-1, &namelen, &size, &format, name);
        if(namelen <= 0 || size <= 0)
        {
            continue;
        }
        name[std::clamp(static_cast<int>(namelen), 0, static_cast<int>(sizeof(name))-2)] = '\0';
        char *brak = std::strchr(name, '[');
        if(brak)
        {
            *brak = '\0';
        }
        setglsluniformformat(s, name, format, size);
    }
}

void Shader::allocparams(Slot *slot)
{
    allocglslactiveuniforms(*this);
}

int GlobalShaderParamState::nextversion = 0;

void GlobalShaderParamState::resetversions()
{
    ENUMERATE(shaders, Shader, s,
    {
        for(int i = 0; i < s.globalparams.length(); i++)
        {
            GlobalShaderParamUse &u = s.globalparams[i];
            if(u.version != u.param->version)
            {
                u.version = -2;
            }
        }
    });
    nextversion = 0;
    ENUMERATE(globalparams, GlobalShaderParamState, g, { g.version = ++nextversion; });
    ENUMERATE(shaders, Shader, s,
    {
        for(int i = 0; i < s.globalparams.length(); i++)
        {
            GlobalShaderParamUse &u = s.globalparams[i];
            if(u.version >= 0)
            {
                u.version = u.param->version;
            }
        }
    });
}

static float *findslotparam(Slot &s, const char *name, float *noval = nullptr)
{
    for(int i = 0; i < s.params.length(); i++)
    {
        SlotShaderParam &param = s.params[i];
        if(name == param.name)
        {
            return param.val;
        }
    }
    for(int i = 0; i < s.shader->defaultparams.length(); i++)
    {
        SlotShaderParamState &param = s.shader->defaultparams[i];
        if(name == param.name)
        {
            return param.val;
        }
    }
    return noval;
}

static float *findslotparam(VSlot &s, const char *name, float *noval = nullptr)
{
    for(int i = 0; i < s.params.length(); i++)
    {
        SlotShaderParam &param = s.params[i];
        if(name == param.name)
        {
            return param.val;
        }
    }
    return findslotparam(*s.slot, name, noval);
}

static void setslotparam(SlotShaderParamState &l, const float *val)
{
    switch(l.format)
    {
        case GL_BOOL:
        case GL_FLOAT:
        {
            glUniform1fv(l.loc, 1, val);
            break;
        }
        case GL_BOOL_VEC2:
        case GL_FLOAT_VEC2:
        {
            glUniform2fv(l.loc, 1, val);
            break;
        }
        case GL_BOOL_VEC3:
        case GL_FLOAT_VEC3:
        {
            glUniform3fv(l.loc, 1, val);
            break;
        }
        case GL_BOOL_VEC4:
        case GL_FLOAT_VEC4:
        {
            glUniform4fv(l.loc, 1, val);
            break;
        }
        case GL_INT:
        {
            glUniform1i(l.loc, static_cast<int>(val[0]));
            break;
        }
        case GL_INT_VEC2:
        {
            glUniform2i(l.loc, static_cast<int>(val[0]), static_cast<int>(val[1]));
            break;
        }
        case GL_INT_VEC3:
        {
            glUniform3i(l.loc, static_cast<int>(val[0]), static_cast<int>(val[1]), static_cast<int>(val[2]));
            break;
        }
        case GL_INT_VEC4:
        {
            glUniform4i(l.loc, static_cast<int>(val[0]), static_cast<int>(val[1]), static_cast<int>(val[2]), static_cast<int>(val[3]));
            break;
        }
        case GL_UNSIGNED_INT:
        {
            glUniform1ui_(l.loc, static_cast<uint>(val[0]));
            break;
        }
        case GL_UNSIGNED_INT_VEC2:
        {
            glUniform2ui_(l.loc, static_cast<uint>(val[0]), static_cast<uint>(val[1]));
            break;
        }
        case GL_UNSIGNED_INT_VEC3:
        {
            glUniform3ui_(l.loc, static_cast<uint>(val[0]), static_cast<uint>(val[1]), static_cast<uint>(val[2]));
            break;
        }
        case GL_UNSIGNED_INT_VEC4:
        {
            glUniform4ui_(l.loc, static_cast<uint>(val[0]), static_cast<uint>(val[1]), static_cast<uint>(val[2]), static_cast<uint>(val[3]));
            break;
        }
    }
}
//===================================SETSLOTPARAM SETSLOTPARAMS SETDEFAULTPARAMS
#define SETSLOTPARAM(l, mask, i, val) do { \
    if(!(mask&(1<<i))) \
    { \
        mask |= 1<<i; \
        setslotparam(l, val); \
    } \
} while(0)

#define SETSLOTPARAMS(slotparams) \
    for(int i = 0; i < slotparams.length(); i++) \
    { \
        SlotShaderParam &p = slotparams[i]; \
        if(!(static_cast<int>(defaultparams.length()) > p.loc)) \
        { \
            continue; \
        } \
        SlotShaderParamState &l = defaultparams[p.loc]; \
        SETSLOTPARAM(l, unimask, p.loc, p.val); \
    }
#define SETDEFAULTPARAMS \
    for(int i = 0; i < defaultparams.length(); i++) \
    { \
        SlotShaderParamState &l = defaultparams[i]; \
        SETSLOTPARAM(l, unimask, i, l.val); \
    }

void Shader::setslotparams(Slot &slot)
{
    uint unimask = 0;
    SETSLOTPARAMS(slot.params)
    SETDEFAULTPARAMS
}

void Shader::setslotparams(Slot &slot, VSlot &vslot)
{
    static bool thrown = false; //only throw error message once (will spam per frame otherwise)
    uint unimask = 0;
    if(vslot.slot == &slot)
    {
        SETSLOTPARAMS(vslot.params)
        for(int i = 0; i < slot.params.length(); i++)
        {
            SlotShaderParam &p = slot.params[i];
            if(!(static_cast<int>(defaultparams.length()) > p.loc))
            {
                continue;
            }
            SlotShaderParamState &l = defaultparams[p.loc];
            if(p.loc < 0)
            {
                if(!thrown)
                {
                    std::printf("Invalid slot shader param index: some slot shaders may not be in use\n");
                    thrown = true;
                }
            }
            else if(!(unimask&(1<<p.loc)))
            {
                unimask |= 1<<p.loc;
                setslotparam(l, p.val);
            }
        }
        SETDEFAULTPARAMS
    }
    else
    {
        SETSLOTPARAMS(slot.params)
        for(int i = 0; i < defaultparams.length(); i++)
        {
            SlotShaderParamState &l = defaultparams[i];
            SETSLOTPARAM(l, unimask, i, l.flags&SlotShaderParam::REUSE ? findslotparam(vslot, l.name, l.val) : l.val);
        }
    }
}
#undef SETSLOTPARAM
#undef SETSLOTPARAMS
#undef SETDEFAULTPARAMS
//==============================================================================
void Shader::bindprograms()
{
    if(this == lastshader || !loaded())
    {
        return;
    }
    glUseProgram(program);
    lastshader = this;
}

bool Shader::compile()
{
    if(!vsstr)
    {
        vsobj = !reusevs || reusevs->invalid() ? 0 : reusevs->vsobj;
    }
    else
    {
        compileglslshader(*this, GL_VERTEX_SHADER,   vsobj, vsstr, name, debugshader || !variantshader);
    }
    if(!psstr)
    {
        psobj = !reuseps || reuseps->invalid() ? 0 : reuseps->psobj;
    }
    else
    {
        compileglslshader(*this, GL_FRAGMENT_SHADER, psobj, psstr, name, debugshader || !variantshader);
    }
    linkglslprogram(*this, !variantshader);
    return program!=0;
}

void Shader::cleanup(bool full)
{
    used = false;
    if(vsobj)
    {
        if(!reusevs)
        {
            glDeleteShader(vsobj); vsobj = 0;
        }
    }
    if(psobj)
    {
        if(!reuseps)
        {
            glDeleteShader(psobj);
            psobj = 0;
        }
    }
    if(program)
    {
        glDeleteProgram(program);
        program = 0;
    }
    localparams.setsize(0);
    localparamremap.setsize(0);
    globalparams.setsize(0);
    if(standard || full)
    {
        type = Shader_Invalid;

        delete[] vsstr;
        delete[] psstr;
        delete[] defer;

        vsstr = nullptr;
        psstr = nullptr;
        defer = nullptr;

        variants.setsize(0);

        delete[] variantrows;
        variantrows = nullptr;

        defaultparams.setsize(0);
        attriblocs.setsize(0);
        fragdatalocs.setsize(0);
        uniformlocs.setsize(0);
        reusevs = reuseps = nullptr;
    }
    else
    {
        for(int i = 0; i < defaultparams.length(); i++)
        {
            defaultparams[i].loc = -1;
        }
    }
}

static void genattriblocs(Shader &s, const char *vs, const char *ps, Shader *reusevs, Shader *reuseps)
{
    static int len = std::strlen("//:attrib");
    string name;
    int loc;
    if(reusevs)
    {
        s.attriblocs = reusevs->attriblocs;
    }
    else
    {
        while((vs = std::strstr(vs, "//:attrib")))
        {
            if(std::sscanf(vs, "//:attrib %100s %d", name, &loc) == 2)
            {
                s.attriblocs.add(AttribLoc(getshaderparamname(name), loc));
            }
            vs += len;
        }
    }
}

// adds to uniformlocs vector defined uniformlocs
static void genuniformlocs(Shader &s, const char *vs, const char *ps, Shader *reusevs, Shader *reuseps)
{
    static int len = std::strlen("//:uniform");
    string name, blockname;
    int binding, stride;
    if(reusevs)
    {
        s.uniformlocs = reusevs->uniformlocs;
    }
    else
    {
        while((vs = std::strstr(vs, "//:uniform")))
        {
            int numargs = std::sscanf(vs, "//:uniform %100s %100s %d %d", name, blockname, &binding, &stride);
            if(numargs >= 3)
            {
                s.uniformlocs.add(UniformLoc(getshaderparamname(name), getshaderparamname(blockname), binding, numargs >= 4 ? stride : 0));
            }
            else if(numargs >= 1)
            {
                s.uniformlocs.add(UniformLoc(getshaderparamname(name)));
            }
            vs += len;
        }
    }
}

Shader *newshader(int type, const char *name, const char *vs, const char *ps, Shader *variant = nullptr, int row = 0)
{
    if(Shader::lastshader)
    {
        glUseProgram(0);
        Shader::lastshader = nullptr;
    }
    Shader *exists = shaders.access(name);
    char *rname = exists ? exists->name : newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    s.vsstr = newstring(vs);
    s.psstr = newstring(ps);

    delete[] s.defer;
    s.defer = nullptr;

    s.type = type & ~(Shader_Invalid | Shader_Deferred);
    s.variantshader = variant;
    s.standard = standardshaders;
    if(forceshaders)
    {
        s.forced = true;
    }
    s.reusevs = s.reuseps = nullptr;
    if(variant)
    {
        int row = 0,
            col = 0;
        if(!vs[0] || std::sscanf(vs, "%d , %d", &row, &col) >= 1)
        {
            delete[] s.vsstr;
            s.vsstr = nullptr;
            s.reusevs = !vs[0] ? variant : variant->getvariant(col, row);
        }
        row = col = 0;
        if(!ps[0] || std::sscanf(ps, "%d , %d", &row, &col) >= 1)
        {
            delete[] s.psstr;
            s.psstr = nullptr;
            s.reuseps = !ps[0] ? variant : variant->getvariant(col, row);
        }
    }
    if(variant)
    {
        for(int i = 0; i < variant->defaultparams.length(); i++)
        {
            s.defaultparams.add(variant->defaultparams[i]);
        }
    }
    else
    {
        for(int i = 0; i < slotparams.length(); i++)
        {
            s.defaultparams.add(slotparams[i]);
        }
    }
    s.attriblocs.setsize(0);
    s.uniformlocs.setsize(0);
    genattriblocs(s, vs, ps, s.reusevs, s.reuseps);
    genuniformlocs(s, vs, ps, s.reusevs, s.reuseps);
    s.fragdatalocs.setsize(0);
    if(s.reuseps) //probably always true? its else was removed in shader cleanup
    {
        s.fragdatalocs = s.reuseps->fragdatalocs;
    }
    if(!s.compile())
    {
        s.cleanup(true);
        if(variant)
        {
            shaders.remove(rname);
        }
        return nullptr;
    }
    if(variant)
    {
        variant->addvariant(row, &s);
    }
    return &s;
}

static const char *findglslmain(const char *s)
{
    const char *main = std::strstr(s, "main");
    if(!main)
    {
        return nullptr;
    }
    for(; main >= s; main--) //note reverse iteration
    {
        switch(*main)
        {
            case '\r':
            case '\n':
            case ';':
            {
                return main + 1;
            }
        }
    }
    return s;
}

static void gengenericvariant(Shader &s, const char *sname, const char *vs, const char *ps, int row = 0)
{
    int rowoffset = 0;
    bool vschanged = false,
         pschanged = false;
    vector<char> vsv, psv;
    vsv.put(vs, std::strlen(vs)+1);
    psv.put(ps, std::strlen(ps)+1);

    //cannot be constexpr-- strlen is not compile time
    static const int len  = std::strlen("//:variant"),
                     olen = std::strlen("override");
    for(char *vspragma = vsv.getbuf();; vschanged = true)
    {
        vspragma = std::strstr(vspragma, "//:variant");
        if(!vspragma)
        {
            break;
        }
        if(std::sscanf(vspragma + len, "row %d", &rowoffset) == 1)
        {
            continue;
        }
        memset(vspragma, ' ', len);
        vspragma += len;
        if(!std::strncmp(vspragma, "override", olen))
        {
            memset(vspragma, ' ', olen);
            vspragma += olen;
            char *end = vspragma + std::strcspn(vspragma, "\n\r");
            end += std::strspn(end, "\n\r");
            int endlen = std::strcspn(end, "\n\r");
            memset(end, ' ', endlen);
        }
    }
    for(char *pspragma = psv.getbuf();; pschanged = true)
    {
        pspragma = std::strstr(pspragma, "//:variant");
        if(!pspragma)
        {
            break;
        }
        if(std::sscanf(pspragma + len, "row %d", &rowoffset) == 1)
        {
            continue;
        }
        memset(pspragma, ' ', len);
        pspragma += len;
        if(!std::strncmp(pspragma, "override", olen))
        {
            memset(pspragma, ' ', olen);
            pspragma += olen;
            char *end = pspragma + std::strcspn(pspragma, "\n\r");
            end += std::strspn(end, "\n\r");
            int endlen = std::strcspn(end, "\n\r");
            memset(end, ' ', endlen);
        }
    }
    row += rowoffset;
    if(row < 0 || row >= maxvariantrows)
    {
        return;
    }
    int col = s.numvariants(row);
    DEF_FORMAT_STRING(varname, "<variant:%d,%d>%s", col, row, sname);
    string reuse;
    if(col)
    {
        formatstring(reuse, "%d", row);
    }
    else
    {
        copystring(reuse, "");
    }
    newshader(s.type, varname, vschanged ? vsv.getbuf() : reuse, pschanged ? psv.getbuf() : reuse, &s, row);
}

static void genfogshader(vector<char> &vsbuf, vector<char> &psbuf, const char *vs, const char *ps)
{
    const char *vspragma = std::strstr(vs, "//:fog"),
               *pspragma = std::strstr(ps, "//:fog");
    if(!vspragma && !pspragma)
    {
        return;
    }
    //cannot be constexpr -- strlen is not compile time
    static const int pragmalen = std::strlen("//:fog");
    const char *vsmain = findglslmain(vs),
               *vsend  = std::strrchr(vs, '}');
    if(vsmain && vsend)
    {
        if(!std::strstr(vs, "lineardepth"))
        {
            vsbuf.put(vs, vsmain - vs);
            const char *fogparams = "\nuniform vec2 lineardepthscale;\nvarying float lineardepth;\n";
            vsbuf.put(fogparams, std::strlen(fogparams));
            vsbuf.put(vsmain, vsend - vsmain);
            const char *vsfog = "\nlineardepth = dot(lineardepthscale, gl_Position.zw);\n";
            vsbuf.put(vsfog, std::strlen(vsfog));
            vsbuf.put(vsend, std::strlen(vsend)+1);
        }
    }
    const char *psmain = findglslmain(ps),
               *psend  = std::strrchr(ps, '}');
    if(psmain && psend)
    {
        psbuf.put(ps, psmain - ps);
        if(!std::strstr(ps, "lineardepth"))
        {
            const char *foginterp = "\nvarying float lineardepth;\n";
            psbuf.put(foginterp, std::strlen(foginterp));
        }
        const char *fogparams =
            "\nuniform vec3 fogcolor;\n"
            "uniform vec2 fogdensity;\n"
            "uniform vec4 radialfogscale;\n"
            "#define fogcoord lineardepth*length(vec3(gl_FragCoord.xy*radialfogscale.xy + radialfogscale.zw, 1.0))\n";
        psbuf.put(fogparams, std::strlen(fogparams));
        psbuf.put(psmain, psend - psmain);
        const char *psdef = "\n#define FOG_COLOR ",
                   *psfog =
            pspragma && !std::strncmp(pspragma+pragmalen, "rgba", 4) ?
                "\nfragcolor = mix((FOG_COLOR), fragcolor, clamp(exp2(fogcoord*-fogdensity.x)*fogdensity.y, 0.0, 1.0));\n" :
                "\nfragcolor.rgb = mix((FOG_COLOR).rgb, fragcolor.rgb, clamp(exp2(fogcoord*-fogdensity.x)*fogdensity.y, 0.0, 1.0));\n";
        int clen = 0;
        if(pspragma)
        {
            pspragma += pragmalen;
            while(iscubealpha(*pspragma))
            {
                pspragma++;
            }
            while(*pspragma && !iscubespace(*pspragma))
            {
                pspragma++;
            }
            pspragma += std::strspn(pspragma, " \t\v\f");
            clen = std::strcspn(pspragma, "\r\n");
        }
        if(clen <= 0)
        {
            pspragma = "fogcolor";
            clen = std::strlen(pspragma);
        }
        psbuf.put(psdef, std::strlen(psdef));
        psbuf.put(pspragma, clen);
        psbuf.put(psfog, std::strlen(psfog));
        psbuf.put(psend, std::strlen(psend)+1);
    }
}

static void genuniformdefs(vector<char> &vsbuf, vector<char> &psbuf, const char *vs, const char *ps, Shader *variant = nullptr)
{
    if(variant ? variant->defaultparams.empty() : slotparams.empty())
    {
        return;
    }
    const char *vsmain = findglslmain(vs),
               *psmain = findglslmain(ps);
    if(!vsmain || !psmain)
    {
        return;
    }
    vsbuf.put(vs, vsmain - vs);
    psbuf.put(ps, psmain - ps);
    if(variant)
    {
        for(int i = 0; i < variant->defaultparams.length(); i++)
        {
            DEF_FORMAT_STRING(uni, "\nuniform vec4 %s;\n", variant->defaultparams[i].name);
            vsbuf.put(uni, std::strlen(uni));
            psbuf.put(uni, std::strlen(uni));
        }
    }
    else
    {
        for(int i = 0; i < slotparams.length(); i++)
        {
            DEF_FORMAT_STRING(uni, "\nuniform vec4 %s;\n", slotparams[i].name);
            vsbuf.put(uni, std::strlen(uni));
            psbuf.put(uni, std::strlen(uni));
        }
    }
    vsbuf.put(vsmain, std::strlen(vsmain)+1);
    psbuf.put(psmain, std::strlen(psmain)+1);
}

void setupshaders()
{
    GLint val;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &val);
    maxvsuniforms = val/4;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &val);
    maxfsuniforms = val/4;

    glGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &val);
    mintexoffset = val;
    glGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &val);
    maxtexoffset = val;

    mintexrectoffset = mintexoffset;
    maxtexrectoffset = maxtexoffset;


    standardshaders = true;
    nullshader = newshader(0, "<init>null",
        "attribute vec4 vvertex;\n"
        "void main(void) {\n"
        "   gl_Position = vvertex;\n"
        "}\n",
        "fragdata(0) vec4 fragcolor;\n"
        "void main(void) {\n"
        "   fragcolor = vec4(1.0, 0.0, 1.0, 1.0);\n"
        "}\n");
    hudshader = newshader(0, "<init>hud",
        "attribute vec4 vvertex, vcolor;\n"
        "attribute vec2 vtexcoord0;\n"
        "uniform mat4 hudmatrix;\n"
        "varying vec2 texcoord0;\n"
        "varying vec4 colorscale;\n"
        "void main(void) {\n"
        "    gl_Position = hudmatrix * vvertex;\n"
        "    texcoord0 = vtexcoord0;\n"
        "    colorscale = vcolor;\n"
        "}\n",
        "uniform sampler2D tex0;\n"
        "varying vec2 texcoord0;\n"
        "varying vec4 colorscale;\n"
        "fragdata(0) vec4 fragcolor;\n"
        "void main(void) {\n"
        "    vec4 color = texture2D(tex0, texcoord0);\n"
        "    fragcolor = colorscale * color;\n"
        "}\n");
    hudtextshader = newshader(0, "<init>hudtext",
        "attribute vec4 vvertex, vcolor;\n"
        "attribute vec2 vtexcoord0;\n"
        "uniform mat4 hudmatrix;\n"
        "varying vec2 texcoord0;\n"
        "varying vec4 colorscale;\n"
        "void main(void) {\n"
        "    gl_Position = hudmatrix * vvertex;\n"
        "    texcoord0 = vtexcoord0;\n"
        "    colorscale = vcolor;\n"
        "}\n",
        "uniform sampler2D tex0;\n"
        "uniform vec4 textparams;\n"
        "varying vec2 texcoord0;\n"
        "varying vec4 colorscale;\n"
        "fragdata(0) vec4 fragcolor;\n"
        "void main(void) {\n"
        "    float dist = texture2D(tex0, texcoord0).r;\n"
        "    float border = smoothstep(textparams.x, textparams.y, dist);\n"
        "    float outline = smoothstep(textparams.z, textparams.w, dist);\n"
        "    fragcolor = vec4(colorscale.rgb * outline, colorscale.a * border);\n"
        "}\n");
    hudnotextureshader = newshader(0, "<init>hudnotexture",
        "attribute vec4 vvertex, vcolor;\n"
        "uniform mat4 hudmatrix;"
        "varying vec4 color;\n"
        "void main(void) {\n"
        "    gl_Position = hudmatrix * vvertex;\n"
        "    color = vcolor;\n"
        "}\n",
        "varying vec4 color;\n"
        "fragdata(0) vec4 fragcolor;\n"
        "void main(void) {\n"
        "    fragcolor = color;\n"
        "}\n");
    standardshaders = false;
    if(!nullshader || !hudshader || !hudtextshader || !hudnotextureshader)
    {
        fatal("failed to setup shaders");
    }
    dummyslot.shader = nullshader;
}

VAR(defershaders, 0, 1, 1);

void defershader(int *type, const char *name, const char *contents)
{
    Shader *exists = shaders.access(name);
    if(exists && !exists->invalid())
    {
        return;
    }
    if(!defershaders)
    {
        execute(contents);
        return;
    }
    char *rname = exists ? exists->name : newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    delete[] s.defer;
    s.defer = newstring(contents);
    s.type = Shader_Deferred | (*type & ~Shader_Invalid);
    s.standard = standardshaders;
}
COMMAND(defershader, "iss");

void Shader::force()
{
    if(!deferred() || !defer)
    {
        return;
    }
    char *cmd = defer;
    defer = nullptr;
    bool wasstandard = standardshaders,
         wasforcing = forceshaders;
    int oldflags = identflags;
    standardshaders = standard;
    forceshaders = false;
    identflags &= ~Idf_Persist;
    slotparams.shrink(0);
    execute(cmd);
    identflags = oldflags;
    forceshaders = wasforcing;
    standardshaders = wasstandard;
    delete[] cmd;

    if(deferred())
    {
        delete[] defer;
        defer = nullptr;
        type = Shader_Invalid;
    }
}

int Shader::uniformlocversion()
{
    static int version = 0;
    if(++version >= 0)
    {
        return version;
    }
    version = 0;
    ENUMERATE(shaders, Shader, s,
    {
        for(int j = 0; j < s.uniformlocs.length(); j++)
        {
            s.uniformlocs[j].version = -1;
        }
    });
    return version;
}

/* useshaderbyname: get a shader by name string
 *
 * Parameters:
 *  - const char * name: a pointer to a string refering to the name of the shader to use
 * Returns:
 *  - pointer to a Shader object corresponding to the name passed
 * Effects:
 *  - if the shader is deferred, force it to be used
 */
Shader *useshaderbyname(const char *name)
{
    Shader *s = shaders.access(name);
    if(!s)
    {
        return nullptr;
    }
    if(s->deferred())
    {
        s->force();
    }
    s->forced = true;
    return s;
}
COMMANDN(forceshader, useshaderbyname, "s");

//=====================================================================GENSHADER
#define GENSHADER(cond, body) \
    if(cond) \
    { \
        if(vsbuf.length()) \
        { \
            vsbak.setsize(0); \
            vsbak.put(vs, std::strlen(vs)+1); \
            vs = vsbak.getbuf(); \
            vsbuf.setsize(0); \
        } \
        if(psbuf.length()) \
        { \
            psbak.setsize(0); \
            psbak.put(ps, std::strlen(ps)+1); \
            ps = psbak.getbuf(); \
            psbuf.setsize(0); \
        } \
        body; \
        if(vsbuf.length()) \
        { \
            vs = vsbuf.getbuf(); \
        } \
        if(psbuf.length()) \
        { \
            ps = psbuf.getbuf(); \
        } \
    }

void shader(int *type, char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name))
    {
        return;
    }
    DEF_FORMAT_STRING(info, "shader %s", name);
    renderprogress(loadprogress, info);
    vector<char> vsbuf, psbuf, vsbak, psbak;
    GENSHADER(slotparams.length(), genuniformdefs(vsbuf, psbuf, vs, ps));
    GENSHADER(std::strstr(vs, "//:fog") || std::strstr(ps, "//:fog"), genfogshader(vsbuf, psbuf, vs, ps));
    Shader *s = newshader(*type, name, vs, ps);
    if(s)
    {
        if(std::strstr(ps, "//:variant") || std::strstr(vs, "//:variant"))
        {
            gengenericvariant(*s, name, vs, ps);
        }
    }
    slotparams.shrink(0);
}
COMMAND(shader, "isss");

void variantshader(int *type, char *name, int *row, char *vs, char *ps, int *maxvariants)
{
    if(*row < 0)
    {
        shader(type, name, vs, ps);
        return;
    }
    else if(*row >= maxvariantrows)
    {
        return;
    }
    Shader *s = lookupshaderbyname(name);
    if(!s)
    {
        return;
    }
    DEF_FORMAT_STRING(varname, "<variant:%d,%d>%s", s->numvariants(*row), *row, name);
    if(*maxvariants > 0)
    {
        DEF_FORMAT_STRING(info, "shader %s", name);
        renderprogress(std::min(s->variants.length() / static_cast<float>(*maxvariants), 1.0f), info);
    }
    vector<char> vsbuf, psbuf, vsbak, psbak;
    GENSHADER(s->defaultparams.length(), genuniformdefs(vsbuf, psbuf, vs, ps, s));
    GENSHADER(std::strstr(vs, "//:fog") || std::strstr(ps, "//:fog"), genfogshader(vsbuf, psbuf, vs, ps));
    Shader *v = newshader(*type, varname, vs, ps, s, *row);
    if(v)
    {
        if(std::strstr(ps, "//:variant") || std::strstr(vs, "//:variant"))
        {
            gengenericvariant(*s, varname, vs, ps, *row);
        }
    }
}
#undef GENSHADER
//==============================================================================
COMMAND(variantshader, "isissi");

void setshader(char *name)
{
    slotparams.shrink(0);
    Shader *s = shaders.access(name);
    if(!s)
    {
        conoutf(Console_Error, "no such shader: %s", name);
    }
    else
    {
        slotshader = s;
    }
}
COMMAND(setshader, "s");

void resetslotshader()
{
    slotshader = nullptr;
    slotparams.shrink(0);
}

void setslotshader(Slot &s)
{
    s.shader = slotshader;
    if(!s.shader)
    {
        s.shader = stdworldshader;
        return;
    }
    for(int i = 0; i < slotparams.length(); i++)
    {
        s.params.add(slotparams[i]);
    }
}

static void linkslotshaderparams(vector<SlotShaderParam> &params, Shader *sh, bool load)
{
    if(sh->loaded())
    {
        for(int i = 0; i < params.length(); i++)
        {
            int loc = -1;
            SlotShaderParam &param = params[i];
            for(int i = 0; i < sh->defaultparams.length(); i++)
            {
                SlotShaderParamState &dparam = sh->defaultparams[i];
                if(dparam.name==param.name)
                {
                    if(memcmp(param.val, dparam.val, sizeof(param.val)))
                    {
                        loc = i;
                    }
                    break;
                }
            }
            param.loc = loc;
        }
    }
    else if(load)
    {
        for(int i = 0; i < params.length(); i++)
        {
            params[i].loc = -1;
        }
    }
}

void linkslotshader(Slot &s, bool load)
{
    if(!s.shader)
    {
        return;
    }
    if(load && s.shader->deferred())
    {
        s.shader->force();
    }
    linkslotshaderparams(s.params, s.shader, load);
}

void linkvslotshader(VSlot &s, bool load)
{
    if(!s.slot->shader)
    {
        return;
    }
    linkslotshaderparams(s.params, s.slot->shader, load);
    if(!s.slot->shader->loaded())
    {
        return;
    }
    if(s.slot->texmask&(1 << Tex_Glow))
    {
        static const char *paramname = getshaderparamname("glowcolor");
        const float *param = findslotparam(s, paramname);
        if(param)
        {
            s.glowcolor = vec(param).clamp(0, 1);
        }
    }
}

bool shouldreuseparams(Slot &s, VSlot &p)
{
    if(!s.shader)
    {
        return false;
    }
    Shader &sh = *s.shader;
    for(int i = 0; i < sh.defaultparams.length(); i++)
    {
        SlotShaderParamState &param = sh.defaultparams[i];
        if(param.flags & SlotShaderParam::REUSE)
        {
            const float *val = findslotparam(p, param.name);
            if(val && memcmp(param.val, val, sizeof(param.val)))
            {
                for(int j = 0; j < s.params.length(); j++)
                {
                    if(s.params[j].name == param.name)
                    {
                        goto notreused; //bail out of for loop
                    }
                }
                return true;
            notreused:;
            }
        }
    }
    return false;
}


void isshaderdefinedcmd(const char * name)
{
    intret(lookupshaderbyname(name) ? 1 : 0);
}
COMMANDN(isshaderdefined, isshaderdefinedcmd, "s");

static hashset<const char *> shaderparamnames(256);

const char *getshaderparamname(const char *name, bool insert)
{
    const char *exists = shaderparamnames.find(name, nullptr);
    if(exists || !insert)
    {
        return exists;
    }
    return shaderparamnames.add(newstring(name));
}

void addslotparam(const char *name, float x, float y, float z, float w, int flags = 0)
{
    if(name)
    {
        name = getshaderparamname(name);
    }
    for(int i = 0; i < slotparams.length(); i++)
    {
        SlotShaderParam &param = slotparams[i];
        if(param.name==name)
        {
            param.val[0] = x;
            param.val[1] = y;
            param.val[2] = z;
            param.val[3] = w;
            param.flags |= flags;
            return;
        }
    }
    SlotShaderParam param = {name, -1, flags, {x, y, z, w}};
    slotparams.add(param);
}

void setshaderparamcmd(char *name, float *x, float *y, float *z, float *w)
{
    addslotparam(name, *x, *y, *z, *w);
}
COMMANDN(setshaderparam, setshaderparamcmd, "sfFFf");

void reuseuniformparamcmd(char *name, float *x, float *y, float *z, float *w)
{
    addslotparam(name, *x, *y, *z, *w, SlotShaderParam::REUSE);
}
COMMANDN(reuseuniformparam, reuseuniformparamcmd, "sfFFf");

static constexpr int numpostfxbinds = 10;

struct postfxtex
{
    GLuint id;
    int scale, used;
    postfxtex() : id(0), scale(0), used(-1) {}
};
std::vector<postfxtex> postfxtexs;
int postfxbinds[numpostfxbinds];
GLuint postfxfb = 0;
int postfxw = 0,
    postfxh = 0;

struct postfxpass
{
    Shader *shader;
    vec4<float> params;
    uint inputs, freeinputs;
    int outputbind, outputscale;

    postfxpass() : shader(nullptr), inputs(1), freeinputs(1), outputbind(0), outputscale(0) {}
};
std::vector<postfxpass> postfxpasses;

static int allocatepostfxtex(int scale)
{
    for(uint i = 0; i < postfxtexs.size(); i++)
    {
        postfxtex &t = postfxtexs[i];
        if(t.scale==scale && t.used < 0)
        {
            return i;
        }
    }
    postfxtex t;
    t.scale = scale;
    glGenTextures(1, &t.id);
    createtexture(t.id, std::max(postfxw>>scale, 1), std::max(postfxh>>scale, 1), nullptr, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE);
    postfxtexs.push_back(t);
    return postfxtexs.size()-1;
}

void cleanuppostfx(bool fullclean)
{
    if(fullclean && postfxfb)
    {
        glDeleteFramebuffers(1, &postfxfb);
        postfxfb = 0;
    }
    for(uint i = 0; i < postfxtexs.size(); i++)
    {
        glDeleteTextures(1, &postfxtexs[i].id);
    }
    postfxtexs.clear();
    postfxw = 0;
    postfxh = 0;
}

GLuint setuppostfx(int w, int h, GLuint outfbo)
{
    if(postfxpasses.empty())
    {
        return outfbo;
    }
    if(postfxw != w || postfxh != h)
    {
        cleanuppostfx(false);
        postfxw = w;
        postfxh = h;
    }
    for(int i = 0; i < numpostfxbinds; ++i)
    {
        postfxbinds[i] = -1;
    }
    for(uint i = 0; i < postfxtexs.size(); i++)
    {
        postfxtexs[i].used = -1;
    }
    if(!postfxfb)
    {
        glGenFramebuffers(1, &postfxfb);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, postfxfb);
    int tex = allocatepostfxtex(0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, postfxtexs[tex].id, 0);
    gbuf.bindgdepth();

    postfxbinds[0] = tex;
    postfxtexs[tex].used = 0;

    return postfxfb;
}

void renderpostfx(GLuint outfbo)
{
    if(postfxpasses.empty())
    {
        return;
    }
    timer *postfxtimer = begintimer("postfx");
    for(uint i = 0; i < postfxpasses.size(); i++)
    {
        postfxpass &p = postfxpasses[i];

        int tex = -1;
        if(!(postfxpasses.size() < i+1))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, outfbo);
        }
        else
        {
            tex = allocatepostfxtex(p.outputscale);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, postfxtexs[tex].id, 0);
        }
        int w = tex >= 0 ? std::max(postfxw>>postfxtexs[tex].scale, 1) : postfxw,
            h = tex >= 0 ? std::max(postfxh>>postfxtexs[tex].scale, 1) : postfxh;
        glViewport(0, 0, w, h);
        p.shader->set();
        LOCALPARAM(params, p.params);
        int tw = w,
            th = h,
            tmu = 0;
        for(int j = 0; j < numpostfxbinds; ++j)
        {
            if(p.inputs&(1<<j) && postfxbinds[j] >= 0)
            {
                if(!tmu)
                {
                    tw = std::max(postfxw>>postfxtexs[postfxbinds[j]].scale, 1);
                    th = std::max(postfxh>>postfxtexs[postfxbinds[j]].scale, 1);
                }
                else
                {
                    glActiveTexture_(GL_TEXTURE0 + tmu);
                }
                glBindTexture(GL_TEXTURE_RECTANGLE, postfxtexs[postfxbinds[j]].id);
                ++tmu;
            }
        }
        if(tmu)
        {
            glActiveTexture_(GL_TEXTURE0);
        }
        screenquad(tw, th);
        for(int j = 0; j < numpostfxbinds; ++j)
        {
            if(p.freeinputs&(1<<j) && postfxbinds[j] >= 0)
            {
                postfxtexs[postfxbinds[j]].used = -1;
                postfxbinds[j] = -1;
            }
        }
        if(tex >= 0)
        {
            if(postfxbinds[p.outputbind] >= 0)
            {
                postfxtexs[postfxbinds[p.outputbind]].used = -1;
            }
            postfxbinds[p.outputbind] = tex;
            postfxtexs[tex].used = p.outputbind;
        }
    }
    endtimer(postfxtimer);
}

//adds to the global postfxpasses vector a postfx by the given name
static bool addpostfx(const char *name, int outputbind, int outputscale, uint inputs, uint freeinputs, const vec4<float> &params)
{
    if(!*name)
    {
        return false;
    }
    Shader *s = useshaderbyname(name);
    if(!s)
    {
        conoutf(Console_Error, "no such postfx shader: %s", name);
        return false;
    }
    postfxpass p;
    p.shader = s;
    p.outputbind = outputbind;
    p.outputscale = outputscale;
    p.inputs = inputs;
    p.freeinputs = freeinputs;
    p.params = params;
    postfxpasses.push_back(p);
    return true;
}

void clearpostfx()
{
    postfxpasses.clear();
    cleanuppostfx(false);
}
COMMAND(clearpostfx, "");

void addpostfxcmd(char *name, int *bind, int *scale, char *inputs, float *x, float *y, float *z, float *w)
{
    int inputmask = inputs[0] ? 0 : 1,
        freemask = inputs[0] ? 0 : 1;
    bool freeinputs = true;
    for(; *inputs; inputs++)
    {
        if(isdigit(*inputs))
        {
            inputmask |= 1<<(*inputs-'0');
            if(freeinputs)
            {
                freemask |= 1<<(*inputs-'0');
            }
        }
        else if(*inputs=='+')
        {
            freeinputs = false;
        }
        else if(*inputs=='-')
        {
            freeinputs = true;
        }
    }
    inputmask &= (1<<numpostfxbinds)-1;
    freemask &= (1<<numpostfxbinds)-1;
    addpostfx(name, std::clamp(*bind, 0, numpostfxbinds-1), std::max(*scale, 0), inputmask, freemask, vec4<float>(*x, *y, *z, *w));
}
COMMANDN(addpostfx, addpostfxcmd, "siisffff");

void setpostfx(char *name, float *x, float *y, float *z, float *w)
{
    clearpostfx();
    if(name[0])
    {
        addpostfx(name, 0, 0, 1, 1, vec4<float>(*x, *y, *z, *w));
    }
}
COMMAND(setpostfx, "sffff"); //add a postfx shader to the global vector, with name & 4d pos vector

void cleanupshaders()
{
    cleanuppostfx(true);

    loadedshaders = false;
    nullshader = hudshader = hudnotextureshader = nullptr;
    ENUMERATE(shaders, Shader, s, s.cleanup());
    Shader::lastshader = nullptr;
    glUseProgram(0);
}

void reloadshaders()
{
    identflags &= ~Idf_Persist;
    loadshaders();
    identflags |= Idf_Persist;
    linkslotshaders();
    ENUMERATE(shaders, Shader, s,
    {
        if(!s.standard && s.loaded() && !s.variantshader)
        {
            DEF_FORMAT_STRING(info, "shader %s", s.name);
            renderprogress(0.0, info);
            if(!s.compile())
            {
                s.cleanup(true);
            }
            for(int i = 0; i < s.variants.length(); i++)
            {
                Shader *v = s.variants[i];
                if((v->reusevs && v->reusevs->invalid()) ||
                   (v->reuseps && v->reuseps->invalid()) ||
                   !v->compile())
                {
                    v->cleanup(true);
                }
            }
        }
        if(s.forced && s.deferred())
        {
            s.force();
        }
    });
}

void resetshaders()
{
    clearchanges(Change_Shaders);

    cleanuplights();
    cleanupmodels();
    cleanupshaders();
    setupshaders();
    initgbuffer();
    reloadshaders();
    rootworld.allchanged(true);
    glerror();
}
COMMAND(resetshaders, "");

FVAR(blursigma, 0.005f, 0.5f, 2.0f);

void setupblurkernel(int radius, float *weights, float *offsets)
{
    if(radius<1 || radius>maxblurradius)
    {
        return;
    }
    float sigma = blursigma*2*radius,
          total = 1.0f/sigma;
    weights[0] = total;
    offsets[0] = 0;
    // rely on bilinear filtering to sample 2 pixels at once
    // transforms a*X + b*Y into (u+v)*[X*u/(u+v) + Y*(1 - u/(u+v))]
    for(int i = 0; i < radius; ++i)
    {
        float weight1 = std::exp(-((2*i)*(2*i)) / (2*sigma*sigma)) / sigma,
              weight2 = std::exp(-((2*i+1)*(2*i+1)) / (2*sigma*sigma)) / sigma,
              scale = weight1 + weight2,
              offset = 2*i+1 + weight2 / scale;
        weights[i+1] = scale;
        offsets[i+1] = offset;
        total += 2*scale;
    }
    for(int i = 0; i < radius+1; ++i)
    {
        weights[i] /= total;
    }
    for(int i = radius+1; i <= maxblurradius; i++)
    {
        weights[i] = offsets[i] = 0;
    }
}

void setblurshader(int pass, int size, int radius, float *weights, float *offsets, GLenum target)
{
    if(radius<1 || radius>maxblurradius)
    {
        return;
    }
    static Shader *blurshader[7][2] = { { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr } },
                  *blurrectshader[7][2] = { { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr }, { nullptr, nullptr } };
    Shader *&s = (target == GL_TEXTURE_RECTANGLE ? blurrectshader : blurshader)[radius-1][pass];
    if(!s)
    {
        DEF_FORMAT_STRING(name, "blur%c%d%s", 'x'+pass, radius, target == GL_TEXTURE_RECTANGLE ? "rect" : "");
        s = lookupshaderbyname(name);
    }
    s->set();
    LOCALPARAMV(weights, weights, 8);
    float scaledoffsets[8];
    for(int k = 0; k < 8; ++k)
    {
        scaledoffsets[k] = offsets[k]/size;
    }
    LOCALPARAMV(offsets, scaledoffsets, 8);
}


// shader.cpp: OpenGL GLSL shader management

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include "octarender.h"
#include "postfx.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "rendertimers.h"
#include "renderwindow.h"
#include "shaderparam.h"
#include "shader.h"
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

static std::map<std::string, int> localparams;
static std::unordered_map<std::string, Shader> shaders;
static Shader *slotshader = nullptr;
static std::vector<SlotShaderParam> slotparams;
static bool standardshaders = false,
            forceshaders = true,
            loadedshaders = false;
constexpr int maxvariantrows = 32;

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
    auto ss = shaders.find(name);
    if (ss == shaders.end())
    {
        return nullptr;
    }
    Shader *s = &ss->second;
    return s->loaded() ? s : nullptr;
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

static void compileglslshader(const Shader &s, GLenum type, GLuint &obj, const char *def, const char *name, bool msg = true)
{
    if(!glslversion)
    {
        conoutf(Console_Error, "Cannot compile GLSL shader without GLSL initialized");
        return;
    }
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

static void bindglsluniform(const Shader &s, UniformLoc &u)
{
    static VAR(debugubo, 0, 0, 1); //print out to console information about ubos when bindglsluniform is called

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
        glUniformBlockBinding(s.program, bidx, u.binding);
        if(debugubo)
        {
            conoutf(Console_Debug, "UBO: %s:%s:%d, offset: %d, size: %d, stride: %d", u.name, u.blockname, u.binding, offsetval, sizeval, strideval);
        }
    }
}

static void uniformtex(const char * name, int tmu, const Shader &s)
{
    int loc = glGetUniformLocation(s.program, name);
    if(loc != -1)
    {
        glUniform1i(loc, tmu);
    }
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
        for(uint i = 0; i < s.attriblocs.size(); i++)
        {
            Shader::AttribLoc &a = s.attriblocs[i];
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
            uniformtex("diffusemap", Tex_Diffuse, s);
            uniformtex("normalmap", Tex_Normal, s);
            uniformtex("glowmap", Tex_Glow, s);
            uniformtex("blendmap", 7, s);
            uniformtex("refractmask", 7, s);
            uniformtex("refractlight", 8, s);
        }
        for(uint i = 0; i < s.defaultparams.size(); i++)
        {
            SlotShaderParamState &param = s.defaultparams[i];
            param.loc = glGetUniformLocation(s.program, param.name);
        }
        for(uint i = 0; i < s.uniformlocs.size(); i++)
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
    auto findparam = localparams.find(name);
    if (findparam == localparams.end())
    {
        localparams[name] = localparams.size();
    }
    return localparams[name];
}

static int addlocalparam(Shader &s, const char *name, int loc, int size, GLenum format)
{
    int idx = getlocalparam(name);
    if(idx >= static_cast<int>(s.localparamremap.size()))
    {
        int n = idx + 1 - s.localparamremap.size();
        for(int i = 0; i < n; ++i)
        {
            s.localparamremap.push_back(0xFF);
        }
    }
    s.localparamremap[idx] = s.localparams.size();
    LocalShaderParamState l;
    l.name = name;
    l.loc = loc;
    l.size = size;
    l.format = format;
    s.localparams.push_back(l);
    return idx;
}

static void addglobalparam(Shader &s, const GlobalShaderParamState *param, int loc, int size, GLenum format)
{
    GlobalShaderParamUse g;
    g.param = param;
    g.version = -2;
    g.loc = loc;
    g.size = size;
    g.format = format;
    s.globalparams.push_back(g);
}

void Shader::setglsluniformformat(const char *name, GLenum format, int size)
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
    int loc = glGetUniformLocation(program, name);
    if(loc < 0)
    {
        return;
    }
    for(uint j = 0; j < defaultparams.size(); j++)
    {
        if(defaultparams[j].loc == loc)
        {
            defaultparams[j].format = format;
            return;
        }
    }
    for(uint j = 0; j < uniformlocs.size(); j++)
    {
        if(uniformlocs[j].loc == loc)
        {
            return;
        }
    }
    for(uint j = 0; j < globalparams.size(); j++)
    {
        if(globalparams[j].loc == loc)
        {
            return;
        }
    }
    for(uint j = 0; j < localparams.size(); j++)
    {
        if(localparams[j].loc == loc)
        {
            return;
        }
    }

    name = getshaderparamname(name);
    //must explicitly enumerate scope because globalparams is a field & gvar :(
    auto itr = ::globalparams.find(name);
    if(itr != ::globalparams.end())
    {
        addglobalparam(*this, &((*itr).second), loc, size, format);
    }
    else
    {
        addlocalparam(*this, name, loc, size, format);
    }
}

void Shader::allocglslactiveuniforms()
{
    GLint numactive = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numactive);
    string name;
    for(int i = 0; i < numactive; ++i)
    {
        GLsizei namelen = 0;
        GLint size = 0;
        GLenum format = GL_FLOAT_VEC4;
        name[0] = '\0';
        glGetActiveUniform(program, i, sizeof(name)-1, &namelen, &size, &format, name);
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
        setglsluniformformat(name, format, size);
    }
}

void Shader::allocparams()
{
    allocglslactiveuniforms();
}

int GlobalShaderParamState::nextversion = 0;

void GlobalShaderParamState::resetversions()
{
    for(auto &[k, s] : shaders)
    {
        for(uint i = 0; i < s.globalparams.size(); i++)
        {
            GlobalShaderParamUse &u = s.globalparams[i];
            if(u.version != u.param->version)
            {
                u.version = -2;
            }
        }
    };
    nextversion = 0;
    for(auto &[k, g] : globalparams)
    {
        g.version = ++nextversion;
    }
    for(auto &[k, s] : shaders)
    {
        for(uint i = 0; i < s.globalparams.size(); i++)
        {
            GlobalShaderParamUse &u = s.globalparams[i];
            if(u.version >= 0)
            {
                u.version = u.param->version;
            }
        }
    }
}

static const float *findslotparam(const Slot &s, const char *name, const float *noval = nullptr)
{
    for(uint i = 0; i < s.params.size(); i++)
    {
        const SlotShaderParam &param = s.params[i];
        if(name == param.name)
        {
            return param.val;
        }
    }
    for(uint i = 0; i < s.shader->defaultparams.size(); i++)
    {
        const SlotShaderParamState &param = s.shader->defaultparams[i];
        if(name == param.name)
        {
            return param.val;
        }
    }
    return noval;
}

static const float *findslotparam(const VSlot &s, const char *name, const float *noval = nullptr)
{
    for(uint i = 0; i < s.params.size(); i++)
    {
        const SlotShaderParam &param = s.params[i];
        if(name == param.name)
        {
            return param.val;
        }
    }
    return findslotparam(*s.slot, name, noval);
}

static void setslotparam(const SlotShaderParamState &l, const float *val)
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
            glUniform1ui(l.loc, static_cast<uint>(val[0]));
            break;
        }
        case GL_UNSIGNED_INT_VEC2:
        {
            glUniform2ui(l.loc, static_cast<uint>(val[0]), static_cast<uint>(val[1]));
            break;
        }
        case GL_UNSIGNED_INT_VEC3:
        {
            glUniform3ui(l.loc, static_cast<uint>(val[0]), static_cast<uint>(val[1]), static_cast<uint>(val[2]));
            break;
        }
        case GL_UNSIGNED_INT_VEC4:
        {
            glUniform4ui(l.loc, static_cast<uint>(val[0]), static_cast<uint>(val[1]), static_cast<uint>(val[2]), static_cast<uint>(val[3]));
            break;
        }
    }
}

static void setslotparam(const SlotShaderParamState& l, uint& mask, uint i, const float* val)
{
    if(!(mask&(1<<i)))
    {
        mask |= 1<<i;
        setslotparam(l, val);
    }
}

static void setslotparams(const std::vector<SlotShaderParam>& p, uint& unimask, const std::vector<SlotShaderParamState>& defaultparams)
{
    for(const SlotShaderParam &p : slotparams)
    {
        if(!(defaultparams.size() > p.loc))
        {
            continue;
        }
        const SlotShaderParamState &l = defaultparams.at(p.loc);
        setslotparam(l, unimask, p.loc, p.val);
    }
}

static void setdefaultparams(const std::vector<SlotShaderParamState>& defaultparams, uint& unimask)
{
    for(uint i = 0; i < defaultparams.size(); i++)
    {
        const SlotShaderParamState &l = defaultparams.at(i);
        setslotparam(l, unimask, i, l.val);
    }
}

//shader

Shader::Shader() : name(nullptr), vsstr(nullptr), psstr(nullptr), defer(nullptr), type(Shader_Default), program(0), vsobj(0), psobj(0), variantshader(nullptr), standard(false), forced(false), reusevs(nullptr), reuseps(nullptr), owner(nullptr), variantrows(nullptr), used(false)
{
}

Shader::~Shader()
{
    delete[] name;
    delete[] vsstr;
    delete[] psstr;
    delete[] defer;
    delete[] variantrows;
}

void Shader::flushparams()
{
    if(!used)
    {
        allocparams();
        used = true;
    }
    for(uint i = 0; i < globalparams.size(); i++)
    {
        globalparams[i].flush();
    }
}

bool Shader::invalid() const
{
    return (type & Shader_Invalid) != 0;
}
bool Shader::deferred() const
{
    return (type & Shader_Deferred) != 0;
}
bool Shader::loaded() const
{
    return !(type&(Shader_Deferred | Shader_Invalid));
}

bool Shader::isdynamic() const
{
    return (type & Shader_Dynamic) != 0;
}

int Shader::numvariants(int row) const
{
    if(row < 0 || row >= maxvariantrows || !variantrows)
    {
        return 0;
    }
    return variantrows[row+1] - variantrows[row];
}

Shader *Shader::getvariant(int col, int row) const
{
    if(row < 0 || row >= maxvariantrows || col < 0 || !variantrows)
    {
        return nullptr;
    }
    int start = variantrows[row],
        end = variantrows[row+1];
    return col < end - start ? variants[start + col] : nullptr;
}

void Shader::addvariant(int row, Shader *s)
{
    if(row < 0 || row >= maxvariantrows || variants.size() >= USHRT_MAX)
    {
        return;
    }
    if(!variantrows)
    {
        variantrows = new ushort[maxvariantrows+1];
        std::memset(variantrows, 0, (maxvariantrows+1)*sizeof(ushort));
    }
    variants.insert(variants.begin() + variantrows[row+1], s);
    for(int i = row+1; i <= maxvariantrows; ++i)
    {
        ++variantrows[i];
    }
}

void Shader::setvariant_(int col, int row)
{
    Shader *s = this;
    if(variantrows)
    {
        int start = variantrows[row],
            end   = variantrows[row+1];
        for(col = std::min(start + col, end-1); col >= start; --col)
        {
            if(!variants[col]->invalid())
            {
                s = variants[col];
                break;
            }
        }
    }
    if(lastshader!=s)
    {
        s->bindprograms();
    }
}

void Shader::setvariant(int col, int row)
{
    if(!loaded())
    {
        return;
    }
    setvariant_(col, row);
    lastshader->flushparams();
}

void Shader::setvariant(int col, int row, const Slot &slot)
{
    if(!loaded())
    {
        return;
    }
    setvariant_(col, row);
    lastshader->flushparams();
    lastshader->setslotparams(slot);
}

void Shader::setvariant(int col, int row, Slot &slot, const VSlot &vslot)
{
    if(!loaded())
    {
        return;
    }
    setvariant_(col, row);
    lastshader->flushparams();
    lastshader->setslotparams(slot, vslot);
}

void Shader::set_()
{
    if(lastshader!=this)
    {
        bindprograms();
    }
}

void Shader::set()
{
    if(!loaded())
    {
        return;
    }
    set_();
    lastshader->flushparams();
}

void Shader::set(Slot &slot)
{
    if(!loaded())
    {
        return;
    }
    set_();
    lastshader->flushparams();
    lastshader->setslotparams(slot);
}

void Shader::set(Slot &slot, const VSlot &vslot)
{
    if(!loaded())
    {
        return;
    }
    set_();
    lastshader->flushparams();
    lastshader->setslotparams(slot, vslot);
}

void Shader::setslotparams(const Slot &slot)
{
    uint unimask = 0;
    ::setslotparams(slot.params, unimask, defaultparams);
    setdefaultparams(defaultparams, unimask);
}

void Shader::setslotparams(Slot &slot, const VSlot &vslot)
{
    static bool thrown = false; //only throw error message once (will spam per frame otherwise)
    uint unimask = 0;
    if(vslot.slot == &slot)
    {
        ::setslotparams(vslot.params, unimask, defaultparams);
        for(size_t i = 0; i < slot.params.size(); i++)
        {
            SlotShaderParam &p = slot.params.at(i);
            if(!(defaultparams.size() > p.loc))
            {
                continue;
            }
            if(p.loc == SIZE_MAX)
            {
                if(!thrown)
                {
                    std::printf("Invalid slot shader param index: some slot shaders may not be in use\n");
                    thrown = true;
                }
            }
            else if(!(unimask&(1<<p.loc)))
            {
                const SlotShaderParamState &l = defaultparams.at(p.loc);
                unimask |= 1<<p.loc;
                setslotparam(l, p.val);
            }
        }
        setdefaultparams(defaultparams, unimask);
    }
    else
    {
        ::setslotparams(slot.params, unimask, defaultparams);
        for(uint i = 0; i < defaultparams.size(); i++)
        {
            const SlotShaderParamState &l = defaultparams.at(i);
            setslotparam(l, unimask, i, l.flags&SlotShaderParam::REUSE ? findslotparam(vslot, l.name, l.val) : l.val);
        }
    }
}

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
    localparams.clear();
    localparamremap.clear();
    globalparams.clear();
    if(standard || full)
    {
        type = Shader_Invalid;

        delete[] vsstr;
        delete[] psstr;
        delete[] defer;

        vsstr = nullptr;
        psstr = nullptr;
        defer = nullptr;

        variants.clear();

        delete[] variantrows;
        variantrows = nullptr;

        defaultparams.clear();
        attriblocs.clear();
        uniformlocs.clear();
        reusevs = reuseps = nullptr;
    }
    else
    {
        for(uint i = 0; i < defaultparams.size(); i++)
        {
            defaultparams[i].loc = -1;
        }
    }
}

// globalshaderparamuse

void GlobalShaderParamUse::flush()
{
    if(version == param->version)
    {
        return;
    }
    switch(format)
    {
        case GL_BOOL:
        case GL_FLOAT:
        {
            glUniform1fv(loc, size, param->fval);
            break;
        }
        case GL_BOOL_VEC2:
        case GL_FLOAT_VEC2:
        {
            glUniform2fv(loc, size, param->fval);
            break;
        }
        case GL_BOOL_VEC3:
        case GL_FLOAT_VEC3:
        {
            glUniform3fv(loc, size, param->fval);
            break;
        }
        case GL_BOOL_VEC4:
        case GL_FLOAT_VEC4:
        {
            glUniform4fv(loc, size, param->fval);
            break;
        }
        case GL_INT:
        {
            glUniform1iv(loc, size, param->ival);
            break;
        }
        case GL_INT_VEC2:
        {
            glUniform2iv(loc, size, param->ival);
            break;
        }
        case GL_INT_VEC3:
        {
            glUniform3iv(loc, size, param->ival);
            break;
        }
        case GL_INT_VEC4:
        {
            glUniform4iv(loc, size, param->ival);
            break;
        }
        case GL_UNSIGNED_INT:
        {
            glUniform1uiv(loc, size, param->uval);
            break;
        }
        case GL_UNSIGNED_INT_VEC2:
        {
            glUniform2uiv(loc, size, param->uval);
            break;
        }
        case GL_UNSIGNED_INT_VEC3:
        {
            glUniform3uiv(loc, size, param->uval);
            break;
        }
        case GL_UNSIGNED_INT_VEC4:
        {
            glUniform4uiv(loc, size, param->uval);
            break;
        }
        case GL_FLOAT_MAT2:
        {
            glUniformMatrix2fv(loc, 1, GL_FALSE, param->fval);
            break;
        }
        case GL_FLOAT_MAT3:
        {
            glUniformMatrix3fv(loc, 1, GL_FALSE, param->fval);
            break;
        }
        case GL_FLOAT_MAT4:
        {
            glUniformMatrix4fv(loc, 1, GL_FALSE, param->fval);
            break;
        }
    }
    version = param->version;
}

static void genattriblocs(Shader &s, const char *vs, Shader *reusevs)
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
                s.attriblocs.emplace_back(Shader::AttribLoc(getshaderparamname(name), loc));
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
                s.uniformlocs.emplace_back(UniformLoc(getshaderparamname(name), getshaderparamname(blockname), binding, numargs >= 4 ? stride : 0));
            }
            else if(numargs >= 1)
            {
                s.uniformlocs.emplace_back(UniformLoc(getshaderparamname(name)));
            }
            vs += len;
        }
    }
}

static Shader *newshader(int type, const char *name, const char *vs, const char *ps, Shader *variant = nullptr, int row = 0)
{
    if(Shader::lastshader)
    {
        glUseProgram(0);
        Shader::lastshader = nullptr;
    }
    auto existsfind = shaders.find(name);
    Shader *exists = (existsfind == shaders.end()) ? nullptr : &shaders[name];
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
        for(uint i = 0; i < variant->defaultparams.size(); i++)
        {
            s.defaultparams.emplace_back(variant->defaultparams[i]);
        }
    }
    else
    {
        for(uint i = 0; i < slotparams.size(); i++)
        {
            s.defaultparams.emplace_back(slotparams[i]);
        }
    }
    s.attriblocs.clear();
    s.uniformlocs.clear();
    genattriblocs(s, vs, s.reusevs);
    genuniformlocs(s, vs, ps, s.reusevs, s.reuseps);
    if(!s.compile())
    {
        s.cleanup(true);
        if(variant)
        {
            shaders.erase(rname);
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
    std::vector<char> vsv, psv;
    for(uint i = 0; i < std::strlen(vs)+1; ++i)
    {
        vsv.push_back(vs[i]);
    }
    for(uint i = 0; i < std::strlen(ps)+1; ++i)
    {
        psv.push_back(ps[i]);
    }

    //cannot be constexpr-- strlen is not compile time
    static const int len  = std::strlen("//:variant"),
                     olen = std::strlen("override");
    for(char *vspragma = vsv.data();; vschanged = true)
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
        std::memset(vspragma, ' ', len);
        vspragma += len;
        if(!std::strncmp(vspragma, "override", olen))
        {
            std::memset(vspragma, ' ', olen);
            vspragma += olen;
            char *end = vspragma + std::strcspn(vspragma, "\n\r");
            end += std::strspn(end, "\n\r");
            int endlen = std::strcspn(end, "\n\r");
            std::memset(end, ' ', endlen);
        }
    }
    for(char *pspragma = psv.data();; pschanged = true)
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
        std::memset(pspragma, ' ', len);
        pspragma += len;
        if(!std::strncmp(pspragma, "override", olen))
        {
            std::memset(pspragma, ' ', olen);
            pspragma += olen;
            char *end = pspragma + std::strcspn(pspragma, "\n\r");
            end += std::strspn(end, "\n\r");
            int endlen = std::strcspn(end, "\n\r");
            std::memset(end, ' ', endlen);
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
    newshader(s.type, varname, vschanged ? vsv.data() : reuse, pschanged ? psv.data() : reuse, &s, row);
}

static void genfogshader(std::vector<char> &vsbuf, std::vector<char> &psbuf, const char *vs, const char *ps)
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
            for(int i = 0; i < vsmain - vs; ++i)
            {
                vsbuf.push_back(vs[i]);
            }
            const char *fogparams = "\nuniform vec2 lineardepthscale;\nvarying float lineardepth;\n";
            for(uint i = 0; i < std::strlen(fogparams); ++i)
            {
                vsbuf.push_back(fogparams[i]);
            }
            for(int i = 0; i < vsend - vsmain; ++i)
            {
                vsbuf.push_back(vsmain[i]);
            }
            const char *vsfog = "\nlineardepth = dot(lineardepthscale, gl_Position.zw);\n";
            for(uint i = 0; i < std::strlen(vsfog); ++i)
            {
                vsbuf.push_back(vsfog[i]);
            }
            for(uint i = 0; i < std::strlen(vsend)+1; ++i)
            {
                vsbuf.push_back(vsend[i]);
            }
        }
    }
    const char *psmain = findglslmain(ps),
               *psend  = std::strrchr(ps, '}');
    if(psmain && psend)
    {
        for(int i = 0; i < psmain - ps; ++i)
        {
            psbuf.push_back(ps[i]);
        }
        if(!std::strstr(ps, "lineardepth"))
        {
            const char *foginterp = "\nvarying float lineardepth;\n";
            for(uint i = 0; i < std::strlen(foginterp); ++i)
            {
                psbuf.push_back(foginterp[i]);
            }
        }
        const char *fogparams =
            "\nuniform vec3 fogcolor;\n"
            "uniform vec2 fogdensity;\n"
            "uniform vec4 radialfogscale;\n"
            "#define fogcoord lineardepth*length(vec3(gl_FragCoord.xy*radialfogscale.xy + radialfogscale.zw, 1.0))\n";
        for(uint i = 0; i < std::strlen(fogparams); ++i)
        {
            psbuf.push_back(fogparams[i]);
        }
        for(uint i = 0; i < psend - psmain; ++i)
        {
            psbuf.push_back(psmain[i]);
        }
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
        for(uint i = 0; i < std::strlen(psdef); ++i)
        {
            psbuf.push_back(psdef[i]);
        }
        for(int i = 0; i < clen; ++i)
        {
            psbuf.push_back(pspragma[i]);
        }
        for(uint i = 0; i < std::strlen(psfog); ++i)
        {
            psbuf.push_back(psfog[i]);
        }
        for(uint i = 0; i < std::strlen(psend) + 1; ++i)
        {
            psbuf.push_back(psend[i]);
        }
    }
}

static void genuniformdefs(std::vector<char> &vsbuf, std::vector<char> &psbuf, const char *vs, const char *ps, Shader *variant = nullptr)
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
    for(int i = 0; i < vsmain - vs; ++i)
    {
        vsbuf.push_back(vs[i]);
    }
    for(int i = 0; i < psmain - ps; ++i)
    {
        psbuf.push_back(ps[i]);
    }
    if(variant)
    {
        for(uint i = 0; i < variant->defaultparams.size(); i++)
        {
            DEF_FORMAT_STRING(uni, "\nuniform vec4 %s;\n", variant->defaultparams[i].name);
            for(uint j = 0; j < std::strlen(uni); ++j)
            {
                vsbuf.push_back(uni[j]);
                psbuf.push_back(uni[j]);
            }
        }
    }
    else
    {
        for(uint i = 0; i < slotparams.size(); i++)
        {
            DEF_FORMAT_STRING(uni, "\nuniform vec4 %s;\n", slotparams[i].name);
            for(uint j = 0; j < std::strlen(uni); ++j)
            {
                vsbuf.push_back(uni[j]);
                psbuf.push_back(uni[j]);
            }
        }
    }
    for(uint i = 0; i < std::strlen(vsmain)+1; ++i)
    {
        vsbuf.push_back(vsmain[i]);
    }
    for(uint i = 0; i < std::strlen(psmain)+1; ++i)
    {
        psbuf.push_back(psmain[i]);
    }
}

void setupshaders()
{
    if(!glslversion)
    {
        conoutf(Console_Error, "Cannot setup GLSL shaders without GLSL initialized, operation not performed");
        return;
    }
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
    Shader *exists = &shaders[name];
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
    slotparams.clear();
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
    for(auto &[k, s] : shaders)
    {
        for(uint j = 0; j < s.uniformlocs.size(); j++)
        {
            s.uniformlocs[j].version = -1;
        }
    }
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
    auto shaderfind = shaders.find(name);
    if(shaderfind == shaders.end())
    {
        return nullptr;
    }
    Shader *s = &shaderfind->second;
    if(s->deferred())
    {
        s->force();
    }
    s->forced = true;
    return s;
}


//=====================================================================GENSHADER
#define GENSHADER(cond, body) \
    if(cond) \
    { \
        if(vsbuf.size()) \
        { \
            vsbak.clear(); \
            for(uint i = 0; i < std::strlen(vs)+1; ++i) \
            { \
                vsbak.push_back(vs[i]); \
            } \
            vs = vsbak.data(); \
            vsbuf.clear(); \
        } \
        if(psbuf.size()) \
        { \
            psbak.clear(); \
            for(uint i = 0; i < std::strlen(ps)+1; ++i) \
            { \
                psbak.push_back(ps[i]); \
            } \
            ps = psbak.data(); \
            psbuf.clear(); \
        } \
        body; \
        if(vsbuf.size()) \
        { \
            vs = vsbuf.data(); \
        } \
        if(psbuf.size()) \
        { \
            ps = psbuf.data(); \
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
    std::vector<char> vsbuf, psbuf, vsbak, psbak;
    GENSHADER(slotparams.size(), genuniformdefs(vsbuf, psbuf, vs, ps));
    GENSHADER(std::strstr(vs, "//:fog") || std::strstr(ps, "//:fog"), genfogshader(vsbuf, psbuf, vs, ps));
    Shader *s = newshader(*type, name, vs, ps);
    if(s)
    {
        if(std::strstr(ps, "//:variant") || std::strstr(vs, "//:variant"))
        {
            gengenericvariant(*s, name, vs, ps);
        }
    }
    slotparams.clear();
}


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
        renderprogress(std::min(s->variants.size() / static_cast<float>(*maxvariants), 1.0f), info);
    }
    std::vector<char> vsbuf, psbuf, vsbak, psbak;
    GENSHADER(s->defaultparams.size(), genuniformdefs(vsbuf, psbuf, vs, ps, s));
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


void setshader(const char *name)
{
    slotparams.clear();
    auto ss = shaders.find(name);
    if(ss == shaders.end())
    {
        conoutf(Console_Error, "no such shader: %s", name);
    }
    else
    {
        slotshader = &ss->second;
    }
}


void resetslotshader()
{
    slotshader = nullptr;
    slotparams.clear();
}

void setslotshader(Slot &s)
{
    s.shader = slotshader;
    if(!s.shader)
    {
        s.shader = stdworldshader;
        return;
    }
    for(uint i = 0; i < slotparams.size(); i++)
    {
        s.params.push_back(slotparams[i]);
    }
}

static void linkslotshaderparams(std::vector<SlotShaderParam> &params, const Shader &sh, bool load)
{
    if(sh.loaded())
    {
        for(uint i = 0; i < params.size(); i++)
        {
            int loc = -1;
            SlotShaderParam &param = params[i];
            for(uint i = 0; i < sh.defaultparams.size(); i++)
            {
                const SlotShaderParamState &dparam = sh.defaultparams[i];
                if(dparam.name==param.name)
                {
                    if(std::memcmp(param.val, dparam.val, sizeof(param.val)))
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
        for(uint i = 0; i < params.size(); i++)
        {
            params[i].loc = SIZE_MAX;
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
    linkslotshaderparams(s.params, *s.shader, load);
}

void linkvslotshader(VSlot &s, bool load)
{
    if(!s.slot->shader)
    {
        return;
    }
    linkslotshaderparams(s.params, *(s.slot->shader), load);
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

bool shouldreuseparams(const Slot &s, const VSlot &p)
{
    if(!s.shader)
    {
        return false;
    }
    const Shader &sh = *s.shader;
    for(uint i = 0; i < sh.defaultparams.size(); i++)
    {
        const SlotShaderParamState &param = sh.defaultparams[i];
        if(param.flags & SlotShaderParam::REUSE)
        {
            const float *val = findslotparam(p, param.name);
            if(val && std::memcmp(param.val, val, sizeof(param.val)))
            {
                for(uint j = 0; j < s.params.size(); j++)
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


static std::unordered_set<std::string> shaderparamnames;

const char *getshaderparamname(const char *name, bool insert)
{
    auto itr = shaderparamnames.find(name);
    if(itr != shaderparamnames.end() || !insert)
    {
        return (*itr).c_str();
    }
    return (*shaderparamnames.insert(name).first).c_str();
}

void addslotparam(const char *name, float x, float y, float z, float w, int flags = 0)
{
    if(name)
    {
        name = getshaderparamname(name);
    }
    for(uint i = 0; i < slotparams.size(); i++)
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
    SlotShaderParam param = {name, SIZE_MAX, flags, {x, y, z, w}};
    slotparams.push_back(param);
}

void cleanupshaders()
{
    cleanuppostfx(true);

    loadedshaders = false;
    nullshader = hudshader = hudnotextureshader = nullptr;
    for(auto &[k, s] : shaders) 
    {
        s.cleanup();
    }
    Shader::lastshader = nullptr;
    glUseProgram(0);
}

void reloadshaders()
{
    identflags &= ~Idf_Persist;
    loadshaders();
    identflags |= Idf_Persist;
    linkslotshaders();
    for(auto &[k, s] : shaders)
    {
        if(!s.standard && s.loaded() && !s.variantshader)
        {
            DEF_FORMAT_STRING(info, "shader %s", s.name);
            renderprogress(0.0, info);
            if(!s.compile())
            {
                s.cleanup(true);
            }
            for(uint i = 0; i < s.variants.size(); i++)
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
    }
}

void resetshaders()
{
    if(!glslversion)
    {
        conoutf(Console_Error, "Cannot reset GLSL shaders without GLSL initialized, operation not performed");
        return;
    }
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

void setblurshader(int pass, int size, int radius, const float *weights, const float *offsets, GLenum target)
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

void initshadercmds()
{
    addcommand("defershader", reinterpret_cast<identfun>(defershader), "iss", Id_Command);
    addcommand("forceshader", reinterpret_cast<identfun>(useshaderbyname), "s", Id_Command);
    addcommand("shader", reinterpret_cast<identfun>(shader), "isss", Id_Command);
    addcommand("variantshader", reinterpret_cast<identfun>(variantshader), "isissi", Id_Command);
    addcommand("setshader", reinterpret_cast<identfun>(setshader), "s", Id_Command);
    addcommand("isshaderdefined", reinterpret_cast<identfun>(+[](const char* name){intret(lookupshaderbyname(name) ? 1 : 0);}), "s", Id_Command);
    addcommand("setshaderparam", reinterpret_cast<identfun>(+[](char *name, float *x, float *y, float *z, float *w){addslotparam(name, *x, *y, *z, *w);}), "sfFFf", Id_Command);
    addcommand("reuseuniformparam", reinterpret_cast<identfun>(+[](char *name, float *x, float *y, float *z, float *w){addslotparam(name, *x, *y, *z, *w, SlotShaderParam::REUSE);}), "sfFFf", Id_Command);
    addcommand("resetshaders", reinterpret_cast<identfun>(resetshaders), "", Id_Command);
}

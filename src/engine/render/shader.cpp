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

static std::unordered_map<std::string, int> localparams;
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
    auto itr = shaders.find(name);
    if(itr != shaders.end())
    {
        return (*itr).second.loaded() ? &(*itr).second : nullptr;
    }
    return nullptr;
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
        conoutf(Console_Error, "GLSL ERROR (%s:%s)", type == GL_VERTEX_SHADER ? "Vertex shader" : (type == GL_FRAGMENT_SHADER ? "Fragment shader" : "Program"), name);
        FILE *l = getlogfile();
        if(l)
        {
            GLchar *log = new GLchar[length];
            if(type)
            {
                glGetShaderInfoLog(obj, length, &length, log);
            }
            else
            {
                glGetProgramInfoLog(obj, length, &length, log);
            }
            std::fprintf(l, "%s\n", log);
            bool partlines = log[0] != '0';
            int line = 0;
            for(int i = 0; i < numparts; ++i)
            {
                const char *part = parts[i];
                int startline = line;
                while(*part)
                {
                    const char *next = std::strchr(part, '\n');
                    if(++line > 1000)
                    {
                        goto done;
                    }
                    if(partlines)
                    {
                        std::fprintf(l, "%d(%d): ", i, line - startline);
                    }
                    else
                    {
                        std::fprintf(l, "%d: ", line);
                    }
                    std::fwrite(part, 1, next ? next - part + 1 : std::strlen(part), l);
                    if(!next)
                    {
                        std::fputc('\n', l);
                        break;
                    }
                    part = next + 1;
                }
            }
        done:
            delete[] log;
        }
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

void Shader::uniformtex(const char * name, int tmu)
{
    int loc = glGetUniformLocation(program, name);
    if(loc != -1)
    {
        glUniform1i(loc, tmu);
    }
}

void Shader::linkglslprogram(bool msg)
{
    program = vsobj && psobj ? glCreateProgram() : 0;
    GLint success = 0;
    if(program)
    {
        glAttachShader(program, vsobj);
        glAttachShader(program, psobj);
        uint attribs = 0;
        for(const Shader::AttribLoc &a : attriblocs)
        {
            glBindAttribLocation(program, a.loc, a.name);
            attribs |= 1<<a.loc;
        }
        for(int i = 0; i < gle::Attribute_NumAttributes; ++i)
        {
            if(!(attribs&(1<<i)))
            {
                glBindAttribLocation(program, i, gle::attribnames[i]);
            }
        }
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &success);
    }
    if(success)
    {
        glUseProgram(program);
        static std::array<std::string, 16> texnames = { "tex0", "tex1", "tex2", "tex3", "tex4", "tex5", "tex6", "tex7", "tex8", "tex9", "tex10", "tex11", "tex12", "tex13", "tex14", "tex15" };
        for(int i = 0; i < 16; ++i)
        {
            GLint loc = glGetUniformLocation(program, texnames[i].c_str());
            if(loc != -1)
            {
                glUniform1i(loc, i);
            }
        }
        if(type & Shader_World)
        {
            uniformtex("diffusemap", Tex_Diffuse);
            uniformtex("normalmap", Tex_Normal);
            uniformtex("glowmap", Tex_Glow);
            uniformtex("blendmap", 7);
            uniformtex("refractmask", 7);
            uniformtex("refractlight", 8);
        }
        for(SlotShaderParamState &param : defaultparams)
        {
            param.loc = glGetUniformLocation(program, param.name.c_str());
        }
        for(UniformLoc &loc : uniformlocs)
        {
            bindglsluniform(*this, loc);
        }
        glUseProgram(0);
    }
    else if(program)
    {
        if(msg)
        {
            showglslinfo(GL_FALSE, program, name);
        }
        glDeleteProgram(program);
        program = 0;
    }
}

size_t getlocalparam(const std::string &name)
{
    auto itr = localparams.find(name);
    if(itr != localparams.end())
    {
        return (*itr).second;
    }
    size_t size = localparams.size();
    localparams.insert( { name, size } );
    return size;
}

static int addlocalparam(Shader &s, const char *name, int loc, int size, GLenum format)
{
    size_t idx = getlocalparam(name);
    if(idx >= s.localparamremap.size())
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
        for(GlobalShaderParamUse &u : s.globalparams)
        {
            if(u.version != u.param->version)
            {
                u.version = -2;
            }
        }
    }
    nextversion = 0;
    for(auto& [k, g] : globalparams)
    {
        g.version = ++nextversion;
    }
    for(auto &[k, s] : shaders)
    {
        for(GlobalShaderParamUse &u : s.globalparams)
        {
            if(u.version >= 0)
            {
                u.version = u.param->version;
            }
        }
    }
}

static const float *findslotparam(const Slot &s, const char *name, const float *noval = nullptr)
{
    for(const SlotShaderParam &param : s.params)
    {
        if(name == param.name)
        {
            return param.val;
        }
    }
    for(const SlotShaderParamState &param : s.shader->defaultparams)
    {
        if(name == param.name)
        {
            return param.val;
        }
    }
    return noval;
}

static const float *findslotparam(const VSlot &s, const char *name, const float *noval = nullptr)
{
    for(const SlotShaderParam &param : s.params)
    {
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

Shader::Shader() : name(nullptr), defer(nullptr), type(Shader_Default), program(0), variantshader(nullptr), standard(false), forced(false), owner(nullptr), vsstr(nullptr), psstr(nullptr), vsobj(0), psobj(0), reusevs(nullptr), reuseps(nullptr), variantrows(nullptr), used(false)
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
    for(GlobalShaderParamUse &i : globalparams)
    {
        i.flush();
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
            setslotparam(l, unimask, i, l.flags&SlotShaderParam::REUSE ? findslotparam(vslot, l.name.c_str(), l.val) : l.val);
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
    linkglslprogram(!variantshader);
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

void Shader::genattriblocs(const char *vs, const Shader *reusevs)
{
    static int len = std::strlen("//:attrib");
    string name;
    int loc;
    if(reusevs)
    {
        attriblocs = reusevs->attriblocs;
    }
    else
    {
        while((vs = std::strstr(vs, "//:attrib")))
        {
            if(std::sscanf(vs, "//:attrib %100s %d", name, &loc) == 2)
            {
                attriblocs.emplace_back(Shader::AttribLoc(getshaderparamname(name), loc));
            }
            vs += len;
        }
    }
}

// adds to uniformlocs vector defined uniformlocs
void Shader::genuniformlocs(const char *vs, const char *ps, const Shader *reusevs, const Shader *reuseps)
{
    static int len = std::strlen("//:uniform");
    string name, blockname;
    int binding, stride;
    if(reusevs)
    {
        uniformlocs = reusevs->uniformlocs;
    }
    else
    {
        while((vs = std::strstr(vs, "//:uniform")))
        {
            int numargs = std::sscanf(vs, "//:uniform %100s %100s %d %d", name, blockname, &binding, &stride);
            if(numargs >= 3)
            {
                uniformlocs.emplace_back(UniformLoc(getshaderparamname(name), getshaderparamname(blockname), binding, numargs >= 4 ? stride : 0));
            }
            else if(numargs >= 1)
            {
                uniformlocs.emplace_back(UniformLoc(getshaderparamname(name)));
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
    auto itr = shaders.find(name);
    Shader *exists = (itr != shaders.end()) ? &(*itr).second : nullptr;
    char *rname = exists ? exists->name : newstring(name);
    if(!exists)
    {
        itr = shaders.insert( { rname, Shader() } ).first;
    }
    Shader *retval = (*itr).second.setupshader(rname, ps, vs, variant, row);
    return retval; //can be nullptr or s
}

Shader *Shader::setupshader(char *rname, const char *ps, const char *vs, Shader *variant, int row)
{
    name = rname;
    vsstr = newstring(vs);
    psstr = newstring(ps);

    delete[] defer;
    defer = nullptr;

    type = type & ~(Shader_Invalid | Shader_Deferred);
    variantshader = variant;
    standard = standardshaders;
    if(forceshaders)
    {
        forced = true;
    }
    reusevs = reuseps = nullptr;
    if(variant)
    {
        int row = 0,
            col = 0;
        if(!vs[0] || std::sscanf(vs, "%d , %d", &row, &col) >= 1)
        {
            delete[] vsstr;
            vsstr = nullptr;
            reusevs = !vs[0] ? variant : variant->getvariant(col, row);
        }
        row = col = 0;
        if(!ps[0] || std::sscanf(ps, "%d , %d", &row, &col) >= 1)
        {
            delete[] psstr;
            psstr = nullptr;
            reuseps = !ps[0] ? variant : variant->getvariant(col, row);
        }
    }
    if(variant)
    {
        for(uint i = 0; i < variant->defaultparams.size(); i++)
        {
            defaultparams.emplace_back(variant->defaultparams[i]);
        }
    }
    else
    {
        for(uint i = 0; i < slotparams.size(); i++)
        {
            defaultparams.emplace_back(slotparams[i]);
        }
    }
    attriblocs.clear();
    uniformlocs.clear();
    genattriblocs(vs, reusevs);
    genuniformlocs(vs, ps, reusevs, reuseps);
    if(!compile())
    {
        cleanup(true);
        if(variant)
        {
            shaders.erase(rname);
        }
        return nullptr;
    }
    if(variant)
    {
        variant->addvariant(row, this);
    }
    return this;
}

static size_t findglslmain(std::string s)
{
    size_t main = s.find("main");
    if(main == std::string::npos)
    {
        return std::string::npos;
    }
    for(; main >= 0; main--) //note reverse iteration
    {
        switch(s[main])
        {
            case '\r':
            case '\n':
            case ';':
            {
                return main + 1;
            }
        }
    }
    return 0;
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

static void genfogshader(std::string &vs, std::string &ps)
{
    constexpr int PRAGMA_LEN = std::string_view("//:fog").size() + 1;

    size_t vspragma = vs.find("//:fog"),
           pspragma = ps.find("//:fog");

    if(vspragma == std::string::npos && pspragma == std::string::npos)
    {
        return;
    }

    size_t vsmain = findglslmain(vs),
           vsend  = vs.rfind('}');

    if(vsmain != std::string::npos && vsend != std::string::npos)
    {
        if(vs.find("lineardepth") == std::string::npos)
        {
            constexpr std::string_view FOG_PARAMS = "\nuniform vec2 lineardepthscale;\nvarying float lineardepth;\n";
            constexpr std::string_view VS_FOG = "\nlineardepth = dot(lineardepthscale, gl_Position.zw);\n";

            vs.insert(vsend, VS_FOG);
            vs.insert(vsmain, FOG_PARAMS);
        }
    }

    size_t psmain = findglslmain(ps),
           psend  = ps.rfind('}');

    if(psmain != std::string::npos && psend != std::string::npos)
    {
        std::string params;

        if(ps.find("lineardepth") == std::string::npos)
        {
            params = "\nvarying float lineardepth;\n";
        }

        std::string fogparams =
            "\nuniform vec3 fogcolor;\n"
            "uniform vec2 fogdensity;\n"
            "uniform vec4 radialfogscale;\n"
            "#define fogcoord lineardepth*length(vec3(gl_FragCoord.xy*radialfogscale.xy + radialfogscale.zw, 1.0))\n";

        params += fogparams;

        std::string psfog = "\nfragcolor.rgb = mix((fogcolor).rgb, fragcolor.rgb, clamp(exp2(fogcoord*-fogdensity.x)*fogdensity.y, 0.0, 1.0));\n";
        ps.insert(psend, psfog);
        ps.insert(psmain, params);
    }
}

static void genuniformdefs(std::string &vs, std::string &ps, Shader *variant = nullptr)
{
    if(variant ? variant->defaultparams.empty() : slotparams.empty())
    {
        return;
    }

    size_t vsmain = findglslmain(vs),
           psmain = findglslmain(ps);

    if(vsmain == std::string::npos || psmain == std::string::npos)
    {
        return;
    }

    std::string params;
    if(variant)
    {
        for(const auto &param : variant->defaultparams)
        {
            DEF_FORMAT_STRING(uni, "\nuniform vec4 %s;\n", param.name.c_str());
            params += uni;
        }
    }
    else
    {
        for(const auto &param : slotparams)
        {
            DEF_FORMAT_STRING(uni, "\nuniform vec4 %s;\n", param.name);
            params += uni;
        }
    }

    vs.insert(vsmain, params);
    ps.insert(psmain, params);
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

void defershader(const int *type, const char *name, const char *contents)
{
    auto itr = shaders.find(name);
    Shader *exists = (itr != shaders.end()) ? &(*itr).second : nullptr;
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
    if(!exists)
    {
        itr = shaders.insert( { rname, Shader() } ).first;
    }
    Shader &s = (*itr).second;
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
        for(UniformLoc &j : s.uniformlocs)
        {
            j.version = -1;
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
    auto itr = shaders.find(name);
    if(itr == shaders.end())
    {
        return nullptr;
    }
    Shader *s = &(*itr).second;
    if(s->deferred())
    {
        s->force();
    }
    s->forced = true;
    return s;
}

void shader(int *type, const char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name))
    {
        return;
    }
    DEF_FORMAT_STRING(info, "shader %s", name);
    renderprogress(loadprogress, info);
    std::string vs_string(vs), ps_string(ps);

    if(!slotparams.empty())
    {
        genuniformdefs(vs_string, ps_string);
    }

    if(vs_string.find("//:fog") != std::string::npos || ps_string.find("//:fog") != std::string::npos)
    {
        genfogshader(vs_string, ps_string);
    }

    Shader *s = newshader(*type, name, vs_string.c_str(), ps_string.c_str());
    if(s)
    {
        if(vs_string.find("//:variant") != std::string::npos || ps_string.find("//:variant") != std::string::npos)
        {
            gengenericvariant(*s, name, vs_string.c_str(), ps_string.c_str());
        }
    }
    slotparams.clear();
}

static bool adding_shader = false;
static std::vector<std::pair<std::string, std::string>> shader_defines;
static std::vector<std::string> shader_includes_vs, shader_includes_fs;
static std::string shader_path_vs, shader_path_fs;

static std::string shader_make_defines()
{
    std::string defines;

    for(const std::pair<std::string, std::string> &define : shader_defines)
    {
        defines += "#define " + define.first + " " + define.second + "\n";
    }

    return defines;
}

static void shader_clear_defines()
{
    shader_defines.clear();
    shader_includes_vs.clear();
    shader_includes_fs.clear();
    shader_path_vs.clear();
    shader_path_fs.clear();
}

static void shader_assemble(std::string &vs, std::string &ps)
{
    std::string defines;

    defines = shader_make_defines();

    if(!shader_path_vs.empty())
    {
        char *vs_file = loadfile(path(shader_path_vs).c_str(), nullptr);
        if(!vs_file)
        {
            conoutf(Console_Error, "could not load vertex shader %s", shader_path_vs.c_str());
            adding_shader = false;
            return;
        }

        vs = vs_file;

        std::string includes;
        for(const std::string &include : shader_includes_vs)
        {
            char *vs_include = loadfile(path(include).c_str(), nullptr);

            if(!vs_include)
            {
                conoutf(Console_Error, "could not load vertex shader include %s", include.c_str());
                adding_shader = false;
                return;
            }

            includes += std::string(vs_include) + "\n";
        }

        vs = defines + includes + vs;
    }

    if(!shader_path_fs.empty())
    {
        char *ps_file = loadfile(path(shader_path_fs).c_str(), nullptr);
        if(!ps_file)
        {
            conoutf(Console_Error, "could not load fragment shader %s", shader_path_fs.c_str());
            adding_shader = false;
            return;
        }

        ps = ps_file;

        std::string includes;
        for(const std::string &include : shader_includes_fs)
        {
            char *ps_include = loadfile(path(include).c_str(), nullptr);

            if(!ps_include)
            {
                conoutf(Console_Error, "could not load fragment shader include %s", include.c_str());
                adding_shader = false;
                return;
            }

            includes += std::string(ps_include) + "\n";
        }

        ps = defines + includes + ps;
    }
}

static void shader_new(int *type, char *name, uint *code)
{
    if(lookupshaderbyname(name))
    {
        return;
    }

    adding_shader = true;
    shader_clear_defines();

    execute(code);

    std::string vs, ps;
    shader_assemble(vs, ps);

    DEF_FORMAT_STRING(info, "shader %s", name);
    renderprogress(loadprogress, info);

    if(!slotparams.empty())
    {
        genuniformdefs(vs, ps);
    }

    if(vs.find("//:fog") != std::string::npos || ps.find("//:fog") != std::string::npos)
    {
        genfogshader(vs, ps);
    }

    Shader *s = newshader(*type, name, vs.c_str(), ps.c_str());
    if(s)
    {
        if(vs.find("//:variant") != std::string::npos || ps.find("//:variant") != std::string::npos)
        {
            gengenericvariant(*s, name, vs.c_str(), ps.c_str());
        }
    }
    slotparams.clear();

    adding_shader = false;
}

static void shader_define(char *name, char *value)
{
    if(!adding_shader)
    {
        return;
    }

    shader_defines.emplace_back(name, value);
}

static void shader_get_defines()
{
    if(!adding_shader)
    {
        return;
    }

    std::string res;

    for(const std::pair<std::string, std::string> &define : shader_defines)
    {
        res += " [" + define.first + " " + define.second + "]";
    }

    result(res.c_str());
}

static void shader_include_vs(char *path)
{
    if(!adding_shader)
    {
        return;
    }

    shader_includes_vs.emplace_back(path);
}

static void shader_get_includes_vs()
{
    if(!adding_shader)
    {
        return;
    }

    std::string res;

    for(const std::string &include : shader_includes_vs)
    {
        res += " \"" + include + "\"";
    }

    result(res.c_str());
}

static void shader_include_fs(char *path)
{
    if(!adding_shader)
    {
        return;
    }

    shader_includes_fs.emplace_back(path);
}

static void shader_get_includes_fs()
{
    if(!adding_shader)
    {
        return;
    }

    std::string res;

    for(const std::string &include : shader_includes_fs)
    {
        res += " \"" + include + "\"";
    }

    result(res.c_str());
}

static void shader_source(char *vs, char *fs)
{
    if(!adding_shader)
    {
        return;
    }

    shader_path_vs = vs;
    shader_path_fs = fs;
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

    std::string vs_string(vs), ps_string(ps);

    if(!s->defaultparams.empty())
    {
        genuniformdefs(vs_string, ps_string, s);
    }

    if(vs_string.find("//:fog") != std::string::npos || ps_string.find("//:fog") != std::string::npos)
    {
        genfogshader(vs_string, ps_string);
    }

    Shader *v = newshader(*type, varname, vs_string.c_str(), ps_string.c_str(), s, *row);
    if(v)
    {
        if(vs_string.find("//:variant") != std::string::npos || ps_string.find("//:variant") != std::string::npos)
        {
            gengenericvariant(*s, varname, vs_string.c_str(), ps_string.c_str(), *row);
        }
    }
}

void variantshader_new(int *type, char *name, int *row, int *maxvariants, uint *code)
{
    if(*row < 0)
    {
        shader_new(type, name, code);
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

    adding_shader = true;
    shader_clear_defines();

    execute(code);

    std::string vs, ps;
    shader_assemble(vs, ps);

    DEF_FORMAT_STRING(varname, "<variant:%d,%d>%s", s->numvariants(*row), *row, name);
    if(*maxvariants > 0)
    {
        DEF_FORMAT_STRING(info, "shader %s", name);
        renderprogress(std::min(s->variants.size() / static_cast<float>(*maxvariants), 1.0f), info);
    }

    if(!s->defaultparams.empty())
    {
        genuniformdefs(vs, ps, s);
    }

    if(vs.find("//:fog") != std::string::npos || ps.find("//:fog") != std::string::npos)
    {
        genfogshader(vs, ps);
    }

    Shader *v = newshader(*type, varname, vs.c_str(), ps.c_str(), s, *row);
    if(v)
    {
        if(vs.find("//:variant") != std::string::npos || ps.find("//:variant") != std::string::npos)
        {
            gengenericvariant(*s, varname, vs.c_str(), ps.c_str(), *row);
        }
    }

    adding_shader = false;
}

//==============================================================================


void setshader(char *name)
{
    slotparams.clear();
    auto itr = shaders.find(name);
    if(itr == shaders.end())
    {
        conoutf(Console_Error, "no such shader: %s", name);
    }
    else
    {
        slotshader = &(*itr).second;
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
        for(SlotShaderParam &param : params)
        {
            int loc = -1;
            for(uint j = 0; j < sh.defaultparams.size(); j++)
            {
                const SlotShaderParamState &dparam = sh.defaultparams[j];
                if(dparam.name==param.name)
                {
                    if(std::memcmp(param.val, dparam.val, sizeof(param.val)))
                    {
                        loc = j;
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
    for(const SlotShaderParamState &param : sh.defaultparams)
    {
        if(param.flags & SlotShaderParam::REUSE)
        {
            const float *val = findslotparam(p, param.name.c_str());
            if(val && std::memcmp(param.val, val, sizeof(param.val)))
            {
                for(const SlotShaderParam &j : s.params)
                {
                    if(j.name == param.name)
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
    for(SlotShaderParam &i : slotparams)
    {
        if(i.name==name)
        {
            i.val[0] = x;
            i.val[1] = y;
            i.val[2] = z;
            i.val[3] = w;
            i.flags |= flags;
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

void Shader::reusecleanup()
{
    if((reusevs && reusevs->invalid()) ||
       (reuseps && reuseps->invalid()) ||
       !compile())
    {
        cleanup(true);
    }
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
            for(Shader *&v : s.variants)
            {
                v->reusecleanup();
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

/*
 * radius: sets number of weights & offsets for blurring to be made
 * weights: array of length at least radius + 1
 * offsets: array of length at least radius + 1
 */
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
    LOCALPARAMV(weights, weights, maxblurradius+1);
    float scaledoffsets[maxblurradius+1];
    for(int k = 0; k < maxblurradius+1; ++k)
    {
        scaledoffsets[k] = offsets[k]/size;
    }
    LOCALPARAMV(offsets, scaledoffsets, maxblurradius+1);
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

    addcommand("variantshader_new", reinterpret_cast<identfun>(variantshader_new), "isiie", Id_Command);
    addcommand("shader_new", reinterpret_cast<identfun>(shader_new), "ise", Id_Command);
    addcommand("shader_define", reinterpret_cast<identfun>(shader_define), "ss", Id_Command);
    addcommand("shader_source", reinterpret_cast<identfun>(shader_source), "ss", Id_Command);
    addcommand("shader_include_vs", reinterpret_cast<identfun>(shader_include_vs), "s", Id_Command);
    addcommand("shader_include_fs", reinterpret_cast<identfun>(shader_include_fs), "s", Id_Command);
    addcommand("shader_get_defines", reinterpret_cast<identfun>(shader_get_defines), "", Id_Command);
    addcommand("shader_get_includes_vs", reinterpret_cast<identfun>(shader_get_includes_vs), "", Id_Command);
    addcommand("shader_get_includes_fs", reinterpret_cast<identfun>(shader_get_includes_fs), "", Id_Command);
}

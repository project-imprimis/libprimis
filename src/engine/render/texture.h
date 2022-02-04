#ifndef TEXTURE_H_
#define TEXTURE_H_

extern int hwtexsize, hwcubetexsize, hwmaxaniso, maxtexsize, hwtexunits, hwvtexunits;

extern Texture *textureload(const char *name, int clamp = 0, bool mipit = true, bool msg = true);
extern bool floatformat(GLenum format);
extern uchar *loadalphamask(Texture *t);
extern void loadshaders();
extern void createtexture(int tnum, int w, int h, const void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_2D, int pw = 0, int ph = 0, int pitch = 0, bool resize = true, GLenum format = GL_FALSE, bool swizzle = false);
extern void create3dtexture(int tnum, int w, int h, int d, const void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_3D, bool swizzle = false);
extern GLuint setuppostfx(int w, int h, GLuint outfbo = 0);
extern void cleanuppostfx(bool fullclean = false);
extern void renderpostfx(GLuint outfbo = 0);
extern bool reloadtexture(Texture &tex);
extern bool reloadtexture(const char *name);
extern void clearslots();
extern void compacteditvslots();
extern void compactmruvslots();
extern void compactvslots(cube *c, int n = 8);
extern void compactvslot(int &index);
extern void compactvslot(VSlot &vs);
extern void reloadtextures();
extern void cleanuptextures();
extern bool settexture(const char *name, int clamp = 0);

//for imagedata manipulation
extern void scaletexture(uchar * RESTRICT src, uint sw, uint sh, uint bpp, uint pitch, uchar * RESTRICT dst, uint dw, uint dh);
extern void reorientnormals(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy);
extern void reorienttexture(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy);
extern GLenum texformat(int bpp, bool swizzle = false);

struct GlobalShaderParamState
{
    const char *name;
    union
    {
        float fval[32];
        int ival[32];
        uint uval[32];
        uchar buf[32*sizeof(float)];
    };
    int version;

    static int nextversion;

    void resetversions();

    void changed()
    {
        if(++nextversion < 0)
        {
            resetversions();
        }
        version = nextversion;
    }
};

struct ShaderParamBinding
{
    int loc, size;
    GLenum format;
};

struct GlobalShaderParamUse : ShaderParamBinding
{

    GlobalShaderParamState *param;
    int version;

    void flush()
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
                glUniform1fv_(loc, size, param->fval);
                break;
            }
            case GL_BOOL_VEC2:
            case GL_FLOAT_VEC2:
            {
                glUniform2fv_(loc, size, param->fval);
                break;
            }
            case GL_BOOL_VEC3:
            case GL_FLOAT_VEC3:
            {
                glUniform3fv_(loc, size, param->fval);
                break;
            }
            case GL_BOOL_VEC4:
            case GL_FLOAT_VEC4:
            {
                glUniform4fv_(loc, size, param->fval);
                break;
            }
            case GL_INT:
            {
                glUniform1iv_(loc, size, param->ival);
                break;
            }
            case GL_INT_VEC2:
            {
                glUniform2iv_(loc, size, param->ival);
                break;
            }
            case GL_INT_VEC3:
            {
                glUniform3iv_(loc, size, param->ival);
                break;
            }
            case GL_INT_VEC4:
            {
                glUniform4iv_(loc, size, param->ival);
                break;
            }
            case GL_UNSIGNED_INT:
            {
                glUniform1uiv_(loc, size, param->uval);
                break;
            }
            case GL_UNSIGNED_INT_VEC2:
            {
                glUniform2uiv_(loc, size, param->uval);
                break;
            }
            case GL_UNSIGNED_INT_VEC3:
            {
                glUniform3uiv_(loc, size, param->uval);
                break;
            }
            case GL_UNSIGNED_INT_VEC4:
            {
                glUniform4uiv_(loc, size, param->uval);
                break;
            }
            case GL_FLOAT_MAT2:
            {
                glUniformMatrix2fv_(loc, 1, GL_FALSE, param->fval);
                break;
            }
            case GL_FLOAT_MAT3:
            {
                glUniformMatrix3fv_(loc, 1, GL_FALSE, param->fval);
                break;
            }
            case GL_FLOAT_MAT4:
            {
                glUniformMatrix4fv_(loc, 1, GL_FALSE, param->fval);
                break;
            }
        }
        version = param->version;
    }
};

struct LocalShaderParamState : ShaderParamBinding
{
    const char *name;
};

struct SlotShaderParamState : LocalShaderParamState
{
    int flags;
    float val[4];

    SlotShaderParamState() {}
    SlotShaderParamState(const SlotShaderParam &p)
    {
        name = p.name;
        loc = -1;
        size = 1;
        format = GL_FLOAT_VEC4;
        flags = p.flags;
        memcpy(val, p.val, sizeof(val));
    }
};

enum
{
    Shader_Default    = 0,
    Shader_World      = 1 << 0,
    Shader_Refract    = 1 << 2,
    Shader_Option     = 1 << 3,
    Shader_Dynamic    = 1 << 4,
    Shader_Triplanar  = 1 << 5,

    Shader_Invalid    = 1 << 8,
    Shader_Deferred   = 1 << 9
};

const int maxvariantrows = 32;

struct Slot;
struct VSlot;

struct UniformLoc
{
    const char *name, *blockname;
    int loc, version, binding, stride, offset, size;
    void *data;
    UniformLoc(const char *name = nullptr, const char *blockname = nullptr, int binding = -1, int stride = -1) : name(name), blockname(blockname), loc(-1), version(-1), binding(binding), stride(stride), offset(-1), size(-1), data(nullptr) {}
};

struct AttribLoc
{
    const char *name;
    int loc;
    AttribLoc(const char *name = nullptr, int loc = -1) : name(name), loc(loc) {}
};

struct FragDataLoc
{
    const char *name;
    int loc;
    GLenum format;
    int index;
    FragDataLoc(const char *name = nullptr, int loc = -1, GLenum format = GL_FALSE, int index = 0) : name(name), loc(loc), format(format), index(index) {}
};

struct Shader
{
    static Shader *lastshader;

    char *name, *vsstr, *psstr, *defer;
    int type;
    GLuint program, vsobj, psobj;
    vector<SlotShaderParamState> defaultparams;
    vector<GlobalShaderParamUse> globalparams;
    vector<LocalShaderParamState> localparams;
    vector<uchar> localparamremap;
    Shader *variantshader;
    vector<Shader *> variants;
    ushort *variantrows;
    bool standard, forced, used;
    Shader *reusevs, *reuseps;
    vector<UniformLoc> uniformlocs;
    vector<AttribLoc> attriblocs;
    vector<FragDataLoc> fragdatalocs;
    const void *owner;

    Shader() : name(nullptr), vsstr(nullptr), psstr(nullptr), defer(nullptr), type(Shader_Default), program(0), vsobj(0), psobj(0), variantshader(nullptr), variantrows(nullptr), standard(false), forced(false), used(false), reusevs(nullptr), reuseps(nullptr), owner(nullptr)
    {
    }

    ~Shader()
    {
        DELETEA(name);
        DELETEA(vsstr);
        DELETEA(psstr);
        DELETEA(defer);
        DELETEA(variantrows);
    }

    void allocparams(Slot *slot = nullptr);
    void setslotparams(Slot &slot);
    void setslotparams(Slot &slot, VSlot &vslot);
    void bindprograms();

    void flushparams(Slot *slot = nullptr)
    {
        if(!used)
        {
            allocparams(slot);
            used = true;
        }
        for(int i = 0; i < globalparams.length(); i++)
        {
            globalparams[i].flush();
        }
    }

    void force();

    bool invalid() const
    {
        return (type & Shader_Invalid) != 0;
    }
    bool deferred() const
    {
        return (type & Shader_Deferred) != 0;
    }
    bool loaded() const
    {
        return !(type&(Shader_Deferred | Shader_Invalid));
    }

    bool hasoption() const
    {
        return (type & Shader_Option) != 0;
    }

    bool isdynamic() const
    {
        return (type & Shader_Dynamic) != 0;
    }

    static inline bool isnull(const Shader *s)
    {
        return !s;
    }

    bool isnull() const
    {
        return isnull(this);
    }

    int numvariants(int row) const
    {
        if(row < 0 || row >= maxvariantrows || !variantrows)
        {
            return 0;
        }
        return variantrows[row+1] - variantrows[row];
    }

    Shader *getvariant(int col, int row) const
    {
        if(row < 0 || row >= maxvariantrows || col < 0 || !variantrows)
        {
            return nullptr;
        }
        int start = variantrows[row],
            end = variantrows[row+1];
        return col < end - start ? variants[start + col] : nullptr;
    }

    void addvariant(int row, Shader *s)
    {
        if(row < 0 || row >= maxvariantrows || variants.length() >= USHRT_MAX)
        {
            return;
        }
        if(!variantrows)
        {
            variantrows = new ushort[maxvariantrows+1];
            memset(variantrows, 0, (maxvariantrows+1)*sizeof(ushort));
        }
        variants.insert(variantrows[row+1], s);
        for(int i = row+1; i <= maxvariantrows; ++i)
        {
            ++variantrows[i];
        }
    }

    void setvariant_(int col, int row)
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

    void setvariant(int col, int row)
    {
        if(isnull() || !loaded())
        {
            return;
        }
        setvariant_(col, row);
        lastshader->flushparams();
    }

    void setvariant(int col, int row, Slot &slot)
    {
        if(isnull() || !loaded())
        {
            return;
        }
        setvariant_(col, row);
        lastshader->flushparams(&slot);
        lastshader->setslotparams(slot);
    }

    void setvariant(int col, int row, Slot &slot, VSlot &vslot)
    {
        if(isnull() || !loaded())
        {
            return;
        }
        setvariant_(col, row);
        lastshader->flushparams(&slot);
        lastshader->setslotparams(slot, vslot);
    }

    void set_()
    {
        if(lastshader!=this)
        {
            bindprograms();
        }
    }

    void set()
    {
        if(isnull() || !loaded())
        {
            return;
        }
        set_();
        lastshader->flushparams();
    }

    void set(Slot &slot)
    {
        if(isnull() || !loaded())
        {
            return;
        }
        set_();
        lastshader->flushparams(&slot);
        lastshader->setslotparams(slot);
    }

    void set(Slot &slot, VSlot &vslot)
    {
        if(isnull() || !loaded())
        {
            return;
        }
        set_();
        lastshader->flushparams(&slot);
        lastshader->setslotparams(slot, vslot);
    }

    bool compile();
    void cleanup(bool full = false);

    static int uniformlocversion();
};

struct GlobalShaderParam
{
    const char *name;
    GlobalShaderParamState *param;

    GlobalShaderParam(const char *name) : name(name), param(nullptr) {}

    GlobalShaderParamState *resolve()
    {
        extern GlobalShaderParamState *getglobalparam(const char *name);
        if(!param)
        {
            param = getglobalparam(name);
        }
        param->changed();
        return param;
    }

    void setf(float x = 0, float y = 0, float z = 0, float w = 0)
    {
        GlobalShaderParamState *g = resolve();
        g->fval[0] = x;
        g->fval[1] = y;
        g->fval[2] = z;
        g->fval[3] = w;
    }

    void set(const vec &v, float w = 0)
    {
        setf(v.x, v.y, v.z, w);
    }

    void set(const vec2 &v, float z = 0, float w = 0)
    {
        setf(v.x, v.y, z, w);
    }

    void set(const vec4<float> &v)
    {
        setf(v.x, v.y, v.z, v.w);
    }

    void set(const plane &p)
    {
        setf(p.x, p.y, p.z, p.offset);
    }

    void set(const matrix2 &m)
    {
        memcpy(resolve()->fval, m.a.v, sizeof(m));
    }

    void set(const matrix3 &m)
    {
        memcpy(resolve()->fval, m.a.v, sizeof(m));
    }

    void set(const matrix4 &m)
    {
        memcpy(resolve()->fval, m.a.v, sizeof(m));
    }

    template<class T>
    void setv(const T *v, int n = 1)
    {
        memcpy(resolve()->buf, v, n*sizeof(T));
    }

    void seti(int x = 0, int y = 0, int z = 0, int w = 0)
    {
        GlobalShaderParamState *g = resolve();
        g->ival[0] = x;
        g->ival[1] = y;
        g->ival[2] = z;
        g->ival[3] = w;
    }
    void set(const ivec &v, int w = 0)
    {
        seti(v.x, v.y, v.z, w);
    }

    void set(const ivec2 &v, int z = 0, int w = 0)
    {
        seti(v.x, v.y, z, w);
    }

    void set(const vec4<int> &v)
    {
        seti(v.x, v.y, v.z, v.w);
    }

    void setu(uint x = 0, uint y = 0, uint z = 0, uint w = 0)
    {
        GlobalShaderParamState *g = resolve();
        g->uval[0] = x;
        g->uval[1] = y;
        g->uval[2] = z;
        g->uval[3] = w;
    }

    template<class T>
    T *reserve(int n = 1)
    {
        return (T *)resolve()->buf;
    }
};

struct LocalShaderParam
{
    const char *name;
    int loc;

    LocalShaderParam(const char *name) : name(name), loc(-1) {}

    LocalShaderParamState *resolve()
    {
        Shader *s = Shader::lastshader;
        if(!s)
        {
            return nullptr;
        }
        if(!s->localparamremap.inrange(loc))
        {
            extern int getlocalparam(const char *name);
            if(loc == -1)
            {
                loc = getlocalparam(name);
            }
            if(!s->localparamremap.inrange(loc))
            {
                return nullptr;
            }
        }
        uchar remap = s->localparamremap[loc];
        return s->localparams.inrange(remap) ? &s->localparams[remap] : nullptr;
    }

    void setf(float x = 0, float y = 0, float z = 0, float w = 0)
    {
        ShaderParamBinding *b = resolve();
        if(b) switch(b->format)
        {
            case GL_BOOL:
            case GL_FLOAT:
            {
                glUniform1f_(b->loc, x);
                break;
            }
            case GL_BOOL_VEC2:
            case GL_FLOAT_VEC2:
            {
                glUniform2f_(b->loc, x, y);
                break;
            }
            case GL_BOOL_VEC3:
            case GL_FLOAT_VEC3:
            {
                glUniform3f_(b->loc, x, y, z);
                break;
            }
            case GL_BOOL_VEC4:
            case GL_FLOAT_VEC4:
            {
                glUniform4f_(b->loc, x, y, z, w);
                break;
            }
            case GL_INT:
            {
                glUniform1i_(b->loc, static_cast<int>(x));
                break;
            }
            case GL_INT_VEC2:
            {
                glUniform2i_(b->loc, static_cast<int>(x), static_cast<int>(y));
                break;
            }
            case GL_INT_VEC3:
            {
                glUniform3i_(b->loc, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
                break;
            }
            case GL_INT_VEC4:
            {
                glUniform4i_(b->loc, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z), static_cast<int>(w));
                break;
            }
            case GL_UNSIGNED_INT:
            {
                glUniform1ui_(b->loc, static_cast<uint>(x));
                break;
            }
            case GL_UNSIGNED_INT_VEC2:
            {
                glUniform2ui_(b->loc, static_cast<uint>(x), static_cast<uint>(y));
                break;
            }
            case GL_UNSIGNED_INT_VEC3:
            {
                glUniform3ui_(b->loc, static_cast<uint>(x), static_cast<uint>(y), static_cast<uint>(z));
                break;
            }
            case GL_UNSIGNED_INT_VEC4:
            {
                glUniform4ui_(b->loc, static_cast<uint>(x), static_cast<uint>(y), static_cast<uint>(z), static_cast<uint>(w));
                break;
            }
        }
    }

    void set(const vec &v, float w = 0)
    {
        setf(v.x, v.y, v.z, w);
    }

    void set(const vec2 &v, float z = 0, float w = 0)
    {
        setf(v.x, v.y, z, w);
    }

    void set(const vec4<float> &v)
    {
        setf(v.x, v.y, v.z, v.w);
    }

    void set(const plane &p)
    {
        setf(p.x, p.y, p.z, p.offset);
    }

    void setv(const float *f, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform1fv_(b->loc, n, f);
        }
    }

    void setv(const vec *v, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform3fv_(b->loc, n, v->v);
        }
    }

    void setv(const vec2 *v, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform2fv_(b->loc, n, v->v);
        }
    }

    void setv(const vec4<float> *v, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform4fv_(b->loc, n, v->v);
        }
    }

    void setv(const plane *p, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform4fv_(b->loc, n, p->v);
        }
    }

    void setv(const matrix2 *m, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniformMatrix2fv_(b->loc, n, GL_FALSE, m->a.v);
        }
    }

    void setv(const matrix3 *m, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniformMatrix3fv_(b->loc, n, GL_FALSE, m->a.v);
        }
    }

    void setv(const matrix4 *m, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniformMatrix4fv_(b->loc, n, GL_FALSE, m->a.v);
        }
    }

    void set(const matrix2 &m)
    {
        setv(&m);
    }

    void set(const matrix3 &m)
    {
        setv(&m);
    }

    void set(const matrix4 &m)
    {
        setv(&m);
    }

    template<class T>
    void sett(T x, T y, T z, T w)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            switch(b->format)
            {
                case GL_FLOAT:
                {
                    glUniform1f_(b->loc, x);
                    break;
                }
                case GL_FLOAT_VEC2:
                {
                    glUniform2f_(b->loc, x, y);
                    break;
                }
                case GL_FLOAT_VEC3:
                {
                    glUniform3f_(b->loc, x, y, z);
                    break;
                }
                case GL_FLOAT_VEC4:
                {
                    glUniform4f_(b->loc, x, y, z, w);
                    break;
                }
                case GL_BOOL:
                case GL_INT:
                {
                    glUniform1i_(b->loc, x);
                    break;
                }
                case GL_BOOL_VEC2:
                case GL_INT_VEC2:
                {
                    glUniform2i_(b->loc, x, y);
                    break;
                }
                case GL_BOOL_VEC3:
                case GL_INT_VEC3:
                {
                    glUniform3i_(b->loc, x, y, z);
                    break;
                }
                case GL_BOOL_VEC4:
                case GL_INT_VEC4:
                {
                    glUniform4i_(b->loc, x, y, z, w);
                    break;
                }
                case GL_UNSIGNED_INT:
                {
                    glUniform1ui_(b->loc, x);
                    break;
                }
                case GL_UNSIGNED_INT_VEC2:
                {
                    glUniform2ui_(b->loc, x, y);
                    break;
                }
                case GL_UNSIGNED_INT_VEC3:
                {
                    glUniform3ui_(b->loc, x, y, z);
                    break;
                }
                case GL_UNSIGNED_INT_VEC4:
                {
                    glUniform4ui_(b->loc, x, y, z, w);
                    break;
                }
            }
        }
    }
    void seti(int x = 0, int y = 0, int z = 0, int w = 0)
    {
        sett<int>(x, y, z, w);
    }

    void set(const ivec &v, int w = 0)
    {
        seti(v.x, v.y, v.z, w);
    }

    void set(const ivec2 &v, int z = 0, int w = 0)
    {
        seti(v.x, v.y, z, w);
    }

    void set(const vec4<int> &v)
    {
        seti(v.x, v.y, v.z, v.w);
    }

    void setv(const int *i, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform1iv_(b->loc, n, i);
        }
    }
    void setv(const ivec *v, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform3iv_(b->loc, n, v->v);
        }
    }
    void setv(const ivec2 *v, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform2iv_(b->loc, n, v->v);
        }
    }
    void setv(const vec4<int> *v, int n = 1)
    {
        ShaderParamBinding *b = resolve();
        if(b)
        {
            glUniform4iv_(b->loc, n, v->v);
        }
    }

    void setu(uint x = 0, uint y = 0, uint z = 0, uint w = 0)
    {
        sett<uint>(x, y, z, w);
    }
    void setv(const uint *u, int n = 1) { ShaderParamBinding *b = resolve(); if(b) glUniform1uiv_(b->loc, n, u); }
};

#define LOCALPARAM(name, vals) do { static LocalShaderParam param( #name ); param.set(vals); } while(0)
#define LOCALPARAMF(name, ...) do { static LocalShaderParam param( #name ); param.setf(__VA_ARGS__); } while(0)
#define LOCALPARAMI(name, ...) do { static LocalShaderParam param( #name ); param.seti(__VA_ARGS__); } while(0)
#define LOCALPARAMV(name, vals, num) do { static LocalShaderParam param( #name ); param.setv(vals, num); } while(0)
#define GLOBALPARAM(name, vals) do { static GlobalShaderParam param( #name ); param.set(vals); } while(0)
#define GLOBALPARAMF(name, ...) do { static GlobalShaderParam param( #name ); param.setf(__VA_ARGS__); } while(0)
#define GLOBALPARAMI(name, ...) do { static GlobalShaderParam param( #name ); param.seti(__VA_ARGS__); } while(0)
#define GLOBALPARAMV(name, vals, num) do { static GlobalShaderParam param( #name ); param.setv(vals, num); } while(0)

#define SETSHADER(name, ...) \
    do { \
        static Shader *name##shader = nullptr; \
        if(!name##shader) name##shader = lookupshaderbyname(#name); \
        name##shader->set(__VA_ARGS__); \
    } while(0)
#define SETVARIANT(name, ...) \
    do { \
        static Shader *name##shader = nullptr; \
        if(!name##shader) name##shader = lookupshaderbyname(#name); \
        name##shader->setvariant(__VA_ARGS__); \
    } while(0)

// management of texture slots
// each texture slot can have multiple texture frames, of which currently only the first is used
// additional frames can be used for various shaders

struct Texture
{
    enum
    {
        IMAGE      = 0,
        CUBEMAP    = 1,
        TYPE       = 0xFF,

        STUB       = 1<<8,
        TRANSIENT  = 1<<9,
        COMPRESSED = 1<<10,
        ALPHA      = 1<<11,
        MIRROR     = 1<<12,
        FLAGS      = 0xFF00
    };

    char *name;
    int type, w, h, xs, ys, bpp, clamp;
    float ratio;
    bool mipmap, canreduce;
    GLuint id;
    uchar *alphamask;

    Texture() : alphamask(nullptr) {}
};

enum
{
    Tex_Diffuse = 0,
    Tex_Normal,
    Tex_Glow,

    Tex_Spec,
    Tex_Depth,
    Tex_Alpha,
    Tex_Unknown,
};

inline void VSlot::addvariant(Slot *slot)
{
    if(!slot->variants)
    {
        slot->variants = this;
    }
    else
    {
        VSlot *prev = slot->variants;
        while(prev->next)
        {
            prev = prev->next;
        }
        prev->next = this;
    }
}

inline bool VSlot::isdynamic() const
{
    return !scroll.iszero() || slot->shader->isdynamic();
}

struct MatSlot : Slot, VSlot
{
    MatSlot();

    int type() const
    {
        return SlotType_Material;
    }
    const char *name() const;

    VSlot &emptyvslot()
    {
        return *this;
    }

    int cancombine(int) const
    {
        return -1;
    }

    void reset()
    {
        Slot::reset();
        VSlot::reset();
    }

    void cleanup()
    {
        Slot::cleanup();
        VSlot::cleanup();
    }
};

struct texrotation
{
    bool flipx, flipy, swapxy;
};

struct cubemapside
{
    GLenum target;
    const char *name;
    bool flipx, flipy, swapxy;
};

extern const texrotation texrotations[8];
extern const cubemapside cubemapsides[6];
extern Texture *notexture;
extern Shader *nullshader, *hudshader, *hudtextshader, *hudnotextureshader, *nocolorshader, *foggedshader, *foggednotextureshader, *ldrshader, *ldrnotextureshader, *stdworldshader;
extern int maxvsuniforms, maxfsuniforms;

extern Shader *lookupshaderbyname(const char *name);
extern Shader *useshaderbyname(const char *name);
extern Shader *generateshader(const char *name, const char *cmd, ...);
extern void resetslotshader();
extern void setslotshader(Slot &s);
extern void linkslotshader(Slot &s, bool load = true);
extern void linkvslotshader(VSlot &s, bool load = true);
extern void linkslotshaders();
extern bool shouldreuseparams(Slot &s, VSlot &p);
extern void setupshaders();
extern void reloadshaders();
extern void cleanupshaders();

const int maxblurradius = 7;

extern float blursigma;
extern void setupblurkernel(int radius, float *weights, float *offsets);
extern void setblurshader(int pass, int size, int radius, float *weights, float *offsets, GLenum target = GL_TEXTURE_2D);

extern SDL_Surface *loadsurface(const char *name);

extern MatSlot &lookupmaterialslot(int slot, bool load = true);

extern void mergevslot(VSlot &dst, const VSlot &src, const VSlot &delta);
extern void packvslot(vector<uchar> &buf, const VSlot &src);

extern Slot dummyslot;
extern VSlot dummyvslot;
extern DecalSlot dummydecalslot;

#endif

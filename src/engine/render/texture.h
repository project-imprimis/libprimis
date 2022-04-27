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

    void flush();
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
        std::memcpy(val, p.val, sizeof(val));
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

class Shader
{
    public:
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
        bool standard, forced;
        Shader *reusevs, *reuseps;
        vector<UniformLoc> uniformlocs;
        vector<AttribLoc> attriblocs;
        vector<FragDataLoc> fragdatalocs;
        const void *owner;

        Shader();
        ~Shader();

        void flushparams();
        void force();
        bool invalid() const;
        bool deferred() const;
        bool loaded() const;
        bool isdynamic() const;
        int numvariants(int row) const;
        Shader *getvariant(int col, int row) const;
        void addvariant(int row, Shader *s);
        void setvariant(int col, int row);
        void setvariant(int col, int row, Slot &slot);
        void setvariant(int col, int row, Slot &slot, VSlot &vslot);
        void set();
        void set(Slot &slot);
        void set(Slot &slot, VSlot &vslot);
        bool compile();
        void cleanup(bool full = false);

        static int uniformlocversion();
    private:
        ushort *variantrows;
        bool used;
        void allocparams();
        void setslotparams(Slot &slot);
        void setslotparams(Slot &slot, VSlot &vslot);
        void bindprograms();
        void setvariant_(int col, int row);
        void set_();
};

class GlobalShaderParam
{
    public:
        GlobalShaderParam(const char *name);

        GlobalShaderParamState *resolve();
        void setf(float x = 0, float y = 0, float z = 0, float w = 0);
        void set(const vec &v, float w = 0);
        void set(const vec2 &v, float z = 0, float w = 0);
        void set(const vec4<float> &v);
        void set(const plane &p);
        void set(const matrix2 &m);
        void set(const matrix3 &m);
        void set(const matrix4 &m);
        void seti(int x = 0, int y = 0, int z = 0, int w = 0);
        void set(const ivec &v, int w = 0);
        void set(const ivec2 &v, int z = 0, int w = 0);
        void set(const vec4<int> &v);
        void setu(uint x = 0, uint y = 0, uint z = 0, uint w = 0);

        template<class T>
        T *reserve()
        {
            return (T *)resolve()->buf;
        }
    private:
        const char *name;
        GlobalShaderParamState *param;
};

class LocalShaderParam
{
    public:
        LocalShaderParam(const char *name);

        LocalShaderParamState *resolve();

        void setf(float x = 0, float y = 0, float z = 0, float w = 0);
        void set(const vec &v, float w = 0);
        void set(const vec2 &v, float z = 0, float w = 0);
        void set(const vec4<float> &v);
        void set(const plane &p);
        void setv(const vec *v, int n = 1);
        void setv(const vec2 *v, int n = 1);
        void setv(const vec4<float> *v, int n = 1);
        void setv(const plane *p, int n = 1);
        void setv(const float *f, int n);
        void setv(const matrix2 *m, int n = 1);
        void setv(const matrix3 *m, int n = 1);
        void setv(const matrix4 *m, int n = 1);
        void set(const matrix2 &m);
        void set(const matrix3 &m);
        void set(const matrix4 &m);
        void seti(int x = 0, int y = 0, int z = 0, int w = 0);
        void set(const ivec &v, int w = 0);
        void set(const ivec2 &v, int z = 0, int w = 0);
        void set(const vec4<int> &v);
        void setv(const int *i, int n = 1);
        void setv(const ivec *v, int n = 1);
        void setv(const ivec2 *v, int n = 1);
        void setv(const vec4<int> *v, int n = 1);
        void setu(uint x = 0, uint y = 0, uint z = 0, uint w = 0);
        void setv(const uint *u, int n = 1);
    private:
        const char *name;
        int loc;
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

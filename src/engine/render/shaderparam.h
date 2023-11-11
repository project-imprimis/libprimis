#ifndef SHADERPARAM_H_
#define SHADERPARAM_H_

struct UniformLoc
{
    const char *name, *blockname;
    int loc, version, binding, stride, offset, size;
    void *data;
    UniformLoc(const char *name = nullptr, const char *blockname = nullptr, int binding = -1, int stride = -1) : name(name), blockname(blockname), loc(-1), version(-1), binding(binding), stride(stride), offset(-1), size(-1), data(nullptr) {}
};

struct GlobalShaderParamState
{
    union
    {
        float fval[32];
        int ival[32];
        uint uval[32];
        std::array<uchar, 32*sizeof(float)> buf;
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

extern std::map<std::string, GlobalShaderParamState> globalparams;
extern GlobalShaderParamState *getglobalparam(const char *name);

struct ShaderParamBinding
{
    int loc, size;
    GLenum format;
};

struct GlobalShaderParamUse : ShaderParamBinding
{

    const GlobalShaderParamState *param;
    int version;

    void flush();
};

struct LocalShaderParamState : ShaderParamBinding
{
    std::string name;
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

//a container containing a GLSL shader
class Shader
{
    public:
        static Shader *lastshader; //the current shader being used by glUseProgram()

        char *name,
            *defer; //a pointer to a deferred shader
        int type; //type of shader, e.g. world, refractive, deferred, see enum
        GLuint program;
        std::vector<SlotShaderParamState> defaultparams;
        std::vector<GlobalShaderParamUse> globalparams;
        std::vector<LocalShaderParamState> localparams;
        std::vector<uchar> localparamremap;
        Shader *variantshader;
        std::vector<Shader *> variants;
        bool standard, forced;
        std::vector<UniformLoc> uniformlocs;

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
        void setvariant(int col, int row, const Slot &slot);
        void setvariant(int col, int row, Slot &slot, const VSlot &vslot);
        void set();
        void set(Slot &slot);
        void set(Slot &slot, const VSlot &vslot);
        bool compile();
        void cleanup(bool full = false);
        void reusecleanup();

        static int uniformlocversion();
        Shader *setupshader(char *rname, const char *ps, const char *vs, Shader *variant, int row);

    private:
        char *vsstr, //a pointer to a `v`ertex `s`hader `str`ing
             *psstr; //a pointer to a `p`ixel `s`hader `str`ing
        struct AttribLoc
        {
            const char *name;
            int loc;
            AttribLoc(const char *name = nullptr, int loc = -1) : name(name), loc(loc) {}
        };
        std::vector<AttribLoc> attriblocs;
        GLuint vsobj, psobj;
        const Shader *reusevs, *reuseps; //may be equal to variantshader, or its getvariant()
        ushort *variantrows;
        bool used;
        void allocparams();
        void setslotparams(const Slot &slot);
        void setslotparams(Slot &slot, const VSlot &vslot);
        void bindprograms();
        void setvariant_(int col, int row);
        void set_();
        void allocglslactiveuniforms();
        void setglsluniformformat(const char *name, GLenum format, int size);
        void linkglslprogram(bool msg = true);
        void uniformtex(const char * name, int tmu);
        void genattriblocs(const char *vs, const Shader *reusevs);
        void genuniformlocs(const char *vs, const char *ps, const Shader *reusevs, const Shader *reuseps);
};

class GlobalShaderParam
{
    public:
        GlobalShaderParam(const char *name);

        GlobalShaderParamState &resolve();
        void setf(float x = 0, float y = 0, float z = 0, float w = 0);
        void set(const vec &v, float w = 0);
        void set(const vec2 &v, float z = 0, float w = 0);
        void set(const matrix3 &m);
        void set(const matrix4 &m);

        template<class T>
        T *reserve()
        {
            return reinterpret_cast<T *>(resolve().buf.data());
        }
    private:
        const std::string name;
        GlobalShaderParamState *param;
        GlobalShaderParamState &getglobalparam(const std::string &name) const;

};

class LocalShaderParam
{
    public:
        LocalShaderParam(const char *name);
        void setf(float x = 0, float y = 0, float z = 0, float w = 0) const;
        void set(const vec &v, float w = 0) const;
        void set(const vec4<float> &v) const;
        void setv(const vec *v, int n = 1) const;
        void setv(const vec2 *v, int n = 1) const;
        void setv(const vec4<float> *v, int n = 1) const;
        void setv(const float *f, int n) const;
        void set(const matrix3 &m) const;
        void set(const matrix4 &m) const;
    private:
        void setv(const matrix3 *m, int n = 1) const;
        void setv(const matrix4 *m, int n = 1) const;
        const LocalShaderParamState *resolve() const;

        const char * const name;
        mutable int loc;
};

/**
 * @brief Defines a LocalShaderParam with static storage inside the function's scope
 *
 * This macro creates a LocalShaderParam named `param` and inserts it into the function
 *  as a static variable. This variable cannot be accessed later and remains defined
 * for as long as the program runs.
 *
 * @param name a string (or plain text, that will be stringized)
 * @param vals the values to set, must comply with one of the set() functions for LocalShaderParam
 */
#define LOCALPARAM(name, vals) \
    do \
    { \
        static LocalShaderParam param( #name ); \
        param.set(vals); \
    } \
    while(0)

//creates a localshaderparam like above but calls setf() instead
#define LOCALPARAMF(name, ...) \
    do \
    { \
        static LocalShaderParam param( #name ); \
        param.setf(__VA_ARGS__); \
    } while(0)

#define LOCALPARAMV(name, vals, num) \
    do \
    { \
        static LocalShaderParam param( #name ); \
        param.setv(vals, num); \
    } while(0)

//creates a globalshaderparam, either by calling set(), setf() or setv()
//this will create a temp object containing #name, and then attempt to set()
//it to a value in the global shader param map
//overrides exist for set() for various different types
#define GLOBALPARAM(name, vals) do { static GlobalShaderParam param( #name ); param.set(vals); } while(0)

//same as globalparam, but takes up to 4 float args
#define GLOBALPARAMF(name, ...) do { static GlobalShaderParam param( #name ); param.setf(__VA_ARGS__); } while(0)

//creates a new static variable inside the function called <name>setshader
//then sets to it any(if present) args passed to set to the shader
//can only be called once per function, and not in the global scope
//upon calling set(), the shader associated with the name is loaded into OpenGL
#define SETSHADER(name, ...) \
    do { \
        static Shader *name##shader = nullptr; \
        if(!name##shader) \
        { \
            name##shader = lookupshaderbyname(#name); \
        } \
        if(name##shader) \
        { \
            name##shader->set(__VA_ARGS__); \
        } \
    } while(0)
#define SETVARIANT(name, ...) \
    do { \
        static Shader *name##shader = nullptr; \
        if(!name##shader) name##shader = lookupshaderbyname(#name); \
        name##shader->setvariant(__VA_ARGS__); \
    } while(0)

#endif

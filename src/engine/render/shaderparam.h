#ifndef SHADERPARAM_H_
#define SHADERPARAM_H_

struct UniformLoc final
{
    const char *name, *blockname;
    GLint loc;
    int version, binding, stride, offset;
    GLint size;
    const void *data;
    UniformLoc(const char *name = nullptr, const char *blockname = nullptr, int binding = -1, int stride = -1) : name(name), blockname(blockname), loc(-1), version(-1), binding(binding), stride(stride), offset(-1), size(-1), data(nullptr) {}
};

struct GlobalShaderParamState final
{
    union
    {
        GLfloat fval[32];
        GLint ival[32];
        GLuint uval[32];
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
    GLint loc;
    GLsizei size;
    GLenum format;

    ShaderParamBinding(GLint loc, GLsizei size, GLenum format);
    ShaderParamBinding() {};
};

struct GlobalShaderParamUse final : ShaderParamBinding
{

    const GlobalShaderParamState *param;
    int version;

    GlobalShaderParamUse(GLint loc, GLsizei size, GLenum format, const GlobalShaderParamState *param, int version);
    void flush();
};

struct LocalShaderParamState : ShaderParamBinding
{
    std::string name;

    LocalShaderParamState() {}
    LocalShaderParamState(GLint loc, GLsizei size, GLenum format);

};

struct SlotShaderParamState final : LocalShaderParamState
{
    int flags;
    std::array<float, 4> val;

    SlotShaderParamState() {}
    SlotShaderParamState(const SlotShaderParam &p) : LocalShaderParamState(-1, 1, GL_FLOAT_VEC4)
    {
        name = p.name;
        flags = p.flags;
        std::memcpy(val.data(), p.val, sizeof(val));
    }
};

//a container containing a GLSL shader
class Shader final
{
    public:
        static Shader *lastshader; //the current shader being used by glUseProgram()

        char *name, //name of the shader in shaders list
            *defer; //deferred shader contents
        int type; //type of shader, e.g. world, refractive, deferred, see enum
        GLuint program; //handle for GL program object
        std::vector<SlotShaderParamState> defaultparams;
        std::vector<GlobalShaderParamUse> globalparams;
        std::vector<LocalShaderParamState> localparams;
        std::vector<uchar> localparamremap;
        const Shader *variantshader;
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
        void addvariant(int row, Shader *s);
        void setvariant(int col, int row);
        void setvariantandslot(int col, int row);
        void setvariant(int col, int row, const Slot &slot, const VSlot &vslot);
        void set();
        void setslot();
        void set(const Slot &slot, const VSlot &vslot);
        bool compile();
        void cleanup(bool full = false);
        void reusecleanup();

        static int uniformlocversion();
        Shader *setupshader(int newtype, char *rname, const char *ps, const char *vs, Shader *variant, int row);

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
        void setslotparams();
        void setslotparams(const Slot &slot, const VSlot &vslot);
        void bindprograms();
        void setvariant_(int col, int row);
        void set_();
        void allocglslactiveuniforms();
        void setglsluniformformat(const char *name, GLenum format, int size);

        //attaches shaders to the Shader::program handle
        void linkglslprogram(bool msg = true);

        /**
         * @brief Sets an OpenGL int uniform
         * gets the uniform1i with name `name` and sets its value to `tmu`
         * no effect if no uniform with given name found
         */
        void uniformtex(std::string_view name, int tmu) const;

        /**
         * For each `//:attrib <string> <int>` found in the passed vertex shader,
         * adds an AttribLoc to this Shader's attriblocs comprising of a string (also stored in shaderparamnames),
         * and the location specified in the second parameter
         */
        void genattriblocs(const char *vs, const Shader *reusevs);
        void genuniformlocs(const char *vs, const Shader *reusevs);
        const Shader *getvariant(int col, int row) const;
};

class GlobalShaderParam final
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

class LocalShaderParam final
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
#define SETSHADER(name) \
    do { \
        static Shader *name##shader = nullptr; \
        if(!name##shader) \
        { \
            name##shader = lookupshaderbyname(#name); \
        } \
        if(name##shader) \
        { \
            name##shader->set(); \
        } \
    } while(0)

#endif

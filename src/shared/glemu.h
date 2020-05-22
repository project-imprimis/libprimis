namespace gle
{
    enum
    {
        Attribute_Vertex        = 0,
        Attribute_Color         = 1,
        Attribute_TexCoord0     = 2,
        Attribute_TexCoord1     = 3,
        Attribute_Normal        = 4,
        Attribute_Tangent       = 5,
        Attribute_BoneWeight    = 6,
        Attribute_BoneIndex     = 7,
        Attribute_NumAttributes = 8
    };

    extern const char * const attribnames[Attribute_NumAttributes];
    extern ucharbuf attribbuf;

    extern int enabled;
    extern void forcedisable();
    inline void disable() { if(enabled) forcedisable(); }

    extern void begin(GLenum mode);
    extern void begin(GLenum mode, int numverts);
    extern void multidraw();
    extern void defattribs(const char *fmt);
    extern void defattrib(int type, int size, int format);

    #define GLE_DEFATTRIB(name, type, defaultsize, defaultformat) \
        inline void def##name(int size = defaultsize, int format = defaultformat) { defattrib(type, size, format); }

    GLE_DEFATTRIB(vertex, Attribute_Vertex, 3, GL_FLOAT)
    GLE_DEFATTRIB(color, Attribute_Color, 3, GL_FLOAT)
    GLE_DEFATTRIB(texcoord0, Attribute_TexCoord0, 2, GL_FLOAT)
    GLE_DEFATTRIB(texcoord1, Attribute_TexCoord1, 2, GL_FLOAT)
    GLE_DEFATTRIB(normal, Attribute_Normal, 3, GL_FLOAT)
    GLE_DEFATTRIB(tangent, Attribute_Tangent, 4, GL_FLOAT)
    GLE_DEFATTRIB(boneweight, Attribute_BoneWeight, 4, GL_UNSIGNED_BYTE)
    GLE_DEFATTRIB(boneindex, Attribute_BoneIndex, 4, GL_UNSIGNED_BYTE)

    #define GLE_INITATTRIB(name, index, suffix, type) \
        inline void name##suffix(type x) { glVertexAttrib1##suffix##_(index, x); } \
        inline void name##suffix(type x, type y) { glVertexAttrib2##suffix##_(index, x, y); } \
        inline void name##suffix(type x, type y, type z) { glVertexAttrib3##suffix##_(index, x, y, z); } \
        inline void name##suffix(type x, type y, type z, type w) { glVertexAttrib4##suffix##_(index, x, y, z, w); }
    #define GLE_INITATTRIBF(name, index) \
        GLE_INITATTRIB(name, index, f, float) \
        inline void name(const vec &v) { glVertexAttrib3fv_(index, v.v); } \
        inline void name(const vec &v, float w) { glVertexAttrib4f_(index, v.x, v.y, v.z, w); } \
        inline void name(const vec2 &v) { glVertexAttrib2fv_(index, v.v); } \
        inline void name(const vec4 &v) { glVertexAttrib4fv_(index, v.v); }
    #define GLE_INITATTRIBN(name, index, suffix, type, defaultw) \
        inline void name##suffix(type x, type y, type z, type w = defaultw) { glVertexAttrib4N##suffix##_(index, x, y, z, w); }

    GLE_INITATTRIBF(vertex, Attribute_Vertex)
    GLE_INITATTRIBF(color, Attribute_Color)
    GLE_INITATTRIBN(color, Attribute_Color, ub, uchar, 255)
    inline void color(const bvec &v, uchar alpha = 255) { glVertexAttrib4Nub_(Attribute_Color, v.x, v.y, v.z, alpha); }
    inline void color(const bvec4 &v) { glVertexAttrib4Nubv_(Attribute_Color, v.v); }
    GLE_INITATTRIBF(texcoord0, Attribute_TexCoord0)
    GLE_INITATTRIBF(texcoord1, Attribute_TexCoord1)
    inline void normal(float x, float y, float z) { glVertexAttrib4f_(Attribute_Normal, x, y, z, 0.0f); }
    inline void normal(const vec &v) { glVertexAttrib4f_(Attribute_Normal, v.x, v.y, v.z, 0.0f); }
    inline void tangent(float x, float y, float z, float w = 1.0f) { glVertexAttrib4f_(Attribute_Tangent, x, y, z, w); }
    inline void tangent(const vec &v, float w = 1.0f) { glVertexAttrib4f_(Attribute_Tangent, v.x, v.y, v.z, w); }
    inline void tangent(const vec4 &v) { glVertexAttrib4fv_(Attribute_Tangent, v.v); }

    #define GLE_ATTRIBPOINTER(name, index, defaultnormalized, defaultsize, defaulttype, prepare) \
        inline void enable##name() { prepare; glEnableVertexAttribArray_(index); } \
        inline void disable##name() { glDisableVertexAttribArray_(index); } \
        inline void name##pointer(int stride, const void *data, GLenum type = defaulttype, int size = defaultsize, GLenum normalized = defaultnormalized) { \
            prepare; \
            glVertexAttribPointer_(index, size, type, normalized, stride, data); \
        }

    inline void enableattrib(int index) { disable(); glEnableVertexAttribArray_(index); }
    inline void disableattrib(int index) { glDisableVertexAttribArray_(index); }
    GLE_ATTRIBPOINTER(vertex, Attribute_Vertex, GL_FALSE, 3, GL_FLOAT, disable())
    GLE_ATTRIBPOINTER(color, Attribute_Color, GL_TRUE, 4, GL_UNSIGNED_BYTE, )
    GLE_ATTRIBPOINTER(texcoord0, Attribute_TexCoord0, GL_FALSE, 2, GL_FLOAT, )
    GLE_ATTRIBPOINTER(texcoord1, Attribute_TexCoord1, GL_FALSE, 2, GL_FLOAT, )
    GLE_ATTRIBPOINTER(normal, Attribute_Normal, GL_TRUE, 3, GL_FLOAT, )
    GLE_ATTRIBPOINTER(tangent, Attribute_Tangent, GL_TRUE, 4, GL_FLOAT, )
    GLE_ATTRIBPOINTER(boneweight, Attribute_BoneWeight, GL_TRUE, 4, GL_UNSIGNED_BYTE, )
    GLE_ATTRIBPOINTER(boneindex, Attribute_BoneIndex, GL_FALSE, 4, GL_UNSIGNED_BYTE, )

    inline void bindebo(GLuint ebo) { disable(); glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, ebo); }
    inline void clearebo() { glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0); }
    inline void bindvbo(GLuint vbo) { disable(); glBindBuffer_(GL_ARRAY_BUFFER, vbo); }
    inline void clearvbo() { glBindBuffer_(GL_ARRAY_BUFFER, 0); }

    template<class T>
    inline void attrib(T x)
    {
        if(attribbuf.check(sizeof(T)))
        {
            T *buf = (T *)attribbuf.pad(sizeof(T));
            buf[0] = x;
        }
    }

    template<class T>
    inline void attrib(T x, T y)
    {
        if(attribbuf.check(2*sizeof(T)))
        {
            T *buf = (T *)attribbuf.pad(2*sizeof(T));
            buf[0] = x;
            buf[1] = y;
        }
    }

    template<class T>
    inline void attrib(T x, T y, T z)
    {
        if(attribbuf.check(3*sizeof(T)))
        {
            T *buf = (T *)attribbuf.pad(3*sizeof(T));
            buf[0] = x;
            buf[1] = y;
            buf[2] = z;
        }
    }

    template<class T>
    inline void attrib(T x, T y, T z, T w)
    {
        if(attribbuf.check(4*sizeof(T)))
        {
            T *buf = (T *)attribbuf.pad(4*sizeof(T));
            buf[0] = x;
            buf[1] = y;
            buf[2] = z;
            buf[3] = w;
        }
    }

    template<size_t N, class T>
    inline void attribv(const T *v)
    {
        attribbuf.put((const uchar *)v, N*sizeof(T));
    }

    #define GLE_ATTRIB(suffix, type) \
        inline void attrib##suffix(type x) { attrib<type>(x); } \
        inline void attrib##suffix(type x, type y) { attrib<type>(x, y); } \
        inline void attrib##suffix(type x, type y, type z) { attrib<type>(x, y, z); } \
        inline void attrib##suffix(type x, type y, type z, type w) { attrib<type>(x, y, z, w); }

    GLE_ATTRIB(f, float)
    GLE_ATTRIB(d, double)
    GLE_ATTRIB(b, char)
    GLE_ATTRIB(ub, uchar)
    GLE_ATTRIB(s, short)
    GLE_ATTRIB(us, ushort)
    GLE_ATTRIB(i, int)
    GLE_ATTRIB(ui, uint)

    inline void attrib(const vec &v) { attribf(v.x, v.y, v.z); }
    inline void attrib(const vec &v, float w) { attribf(v.x, v.y, v.z, w); }
    inline void attrib(const vec2 &v) { attribf(v.x, v.y); }
    inline void attrib(const vec4 &v) { attribf(v.x, v.y, v.z, v.w); }
    inline void attrib(const ivec &v) { attribi(v.x, v.y, v.z); }
    inline void attrib(const ivec &v, int w) { attribi(v.x, v.y, v.z, w); }
    inline void attrib(const ivec2 &v) { attribi(v.x, v.y); }
    inline void attrib(const ivec4 &v) { attribi(v.x, v.y, v.z, v.w); }
    inline void attrib(const bvec &b) { attribub(b.x, b.y, b.z); }
    inline void attrib(const bvec &b, uchar w) { attribub(b.x, b.y, b.z, w); }
    inline void attrib(const bvec4 &b) { attribub(b.x, b.y, b.z, b.w); }

    extern int end();

    extern void enablequads();
    extern void disablequads();
    extern void drawquads(int offset, int count);

    extern void setup();
    extern void cleanup();
}


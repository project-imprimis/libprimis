#include "../libprimis-headers/cube.h"

#include "glexts.h"

namespace gle
{
    struct attribinfo
    {
        int type, size, formatsize, offset;
        GLenum format;

        attribinfo() : type(0), size(0), formatsize(0), offset(0), format(GL_FALSE) {}

        bool operator==(const attribinfo &a) const
        {
            return type == a.type && size == a.size && format == a.format && offset == a.offset;
        }
        bool operator!=(const attribinfo &a) const
        {
            return type != a.type || size != a.size || format != a.format || offset != a.offset;
        }
    };
    ucharbuf attribbuf;
    static uchar *attribdata;
    static attribinfo attribdefs[Attribute_NumAttributes], lastattribs[Attribute_NumAttributes];
    int enabled = 0;
    static int numattribs = 0,
               attribmask = 0,
               numlastattribs = 0,
               lastattribmask = 0,
               vertexsize = 0,
               lastvertexsize = 0;
    static GLenum primtype = GL_TRIANGLES;
    static uchar *lastbuf = nullptr;
    static bool changedattribs = false;
    static std::vector<GLint> multidrawstart;
    static std::vector<GLsizei> multidrawcount;

    static constexpr int maxquads = (0x10000/4); //65635/4 = 16384
    static GLuint quadindexes = 0;
    static bool quadsenabled = false;

    static constexpr int maxvbosize = (1024*1024*4);
    static GLuint vbo = 0;
    static int vbooffset = maxvbosize;

    static GLuint defaultvao = 0;

    void enablequads()
    {
        quadsenabled = true;
        if(quadindexes)
        {
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, quadindexes);
            return;
        }

        glGenBuffers_(1, &quadindexes);
        ushort *data = new ushort[maxquads*6],
               *dst = data;
        for(int idx = 0; idx < maxquads*4; idx += 4, dst += 6)
        {
            dst[0] = idx;
            dst[1] = idx + 1;
            dst[2] = idx + 2;
            dst[3] = idx + 0;
            dst[4] = idx + 2;
            dst[5] = idx + 3;
        }
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, quadindexes);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, maxquads*6*sizeof(ushort), data, GL_STATIC_DRAW);
        delete[] data;
    }

    void disablequads()
    {
        quadsenabled = false;
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void drawquads(int offset, int count)
    {
        if(count <= 0)
        {
            return;
        }
        if(offset + count > maxquads)
        {
            if(offset >= maxquads)
            {
                return;
            }
            count = maxquads - offset;
        }
        glDrawRangeElements_(GL_TRIANGLES, offset*4, (offset + count)*4-1, count*6, GL_UNSIGNED_SHORT, (ushort *)0 + offset*6);
    }

    void defattrib(int type, int size, int format)
    {
        if(type == Attribute_Vertex)
        {
            numattribs = attribmask = 0;
            vertexsize = 0;
        }
        changedattribs = true;
        attribmask |= 1<<type;
        attribinfo &a = attribdefs[numattribs++];
        a.type = type;
        a.size = size;
        a.format = format;
        switch(format)
        {
            case 'B':
            case GL_UNSIGNED_BYTE:
            {
                a.formatsize = 1;
                a.format = GL_UNSIGNED_BYTE;
                break;
            }
            case 'b':
            case GL_BYTE:
            {
                a.formatsize = 1;
                a.format = GL_BYTE;
                break;
            }
            case 'S':
            case GL_UNSIGNED_SHORT:
            {
                a.formatsize = 2;
                a.format = GL_UNSIGNED_SHORT;
                break;
            }
            case 's':
            case GL_SHORT:
            {
                a.formatsize = 2;
                a.format = GL_SHORT;
                break;
            }
            case 'I':
            case GL_UNSIGNED_INT:
            {
                a.formatsize = 4;
                a.format = GL_UNSIGNED_INT;
                break;
            }
            case 'i':
            case GL_INT:
            {
                a.formatsize = 4;
                a.format = GL_INT;
                break;
            }
            case 'f':
            case GL_FLOAT:
            {
                a.formatsize = 4;
                a.format = GL_FLOAT;
                break;
            }
            case 'd':
            case GL_DOUBLE:
            {
                a.formatsize = 8;
                a.format = GL_DOUBLE;
                break;
            }
            default:
            {
                a.formatsize = 0;
                a.format = GL_FALSE;
                break;
            }
        }
        a.formatsize *= size;
        a.offset = vertexsize;
        vertexsize += a.formatsize;
    }

    void defattribs(const char *fmt)
    {
        for(;; fmt += 3)
        {
            GLenum format;
            switch(fmt[0])
            {
                case 'v':
                {
                    format = Attribute_Vertex;
                    break;
                }
                case 'c':
                {
                    format = Attribute_Color;
                    break;
                }
                case 't':
                {
                    format = Attribute_TexCoord0;
                    break;
                }
                case 'T':
                {
                    format = Attribute_TexCoord1;
                    break;
                }
                case 'n':
                {
                    format = Attribute_Normal;
                    break;
                }
                case 'x':
                {
                    format = Attribute_Tangent;
                    break;
                }
                case 'w':
                {
                    format = Attribute_BoneWeight;
                    break;
                }
                case 'i':
                {
                    format = Attribute_BoneIndex;
                    break;
                }
                default:
                {
                    return;
                }
            }
            defattrib(format, fmt[1]-'0', fmt[2]);
        }
    }

    static void setattrib(const attribinfo &a, uchar *buf)
    {
        switch(a.type)
        {
            case Attribute_Vertex:
            case Attribute_TexCoord0:
            case Attribute_TexCoord1:
            case Attribute_BoneIndex:
            {
                glVertexAttribPointer_(a.type, a.size, a.format, GL_FALSE, vertexsize, buf);
                break;
            }
            case Attribute_Color:
            case Attribute_Normal:
            case Attribute_Tangent:
            case Attribute_BoneWeight:
            {
                glVertexAttribPointer_(a.type, a.size, a.format, GL_TRUE, vertexsize, buf);
                break;
            }
        }
        if(!(enabled&(1<<a.type)))
        {
            glEnableVertexAttribArray_(a.type);
            enabled |= 1<<a.type;
        }
    }

    static void unsetattrib(const attribinfo &a)
    {
        glDisableVertexAttribArray_(a.type);
        enabled &= ~(1<<a.type);
    }

    static void setattribs(uchar *buf)
    {
        bool forceattribs = numattribs != numlastattribs || vertexsize != lastvertexsize || buf != lastbuf;
        if(forceattribs || changedattribs)
        {
            //bitwise AND of attribs
            int diffmask = enabled & lastattribmask & ~attribmask;
            if(diffmask)
            {
                for(int i = 0; i < numlastattribs; ++i)
                {
                    const attribinfo &a = lastattribs[i];
                    if(diffmask & (1<<a.type))
                    {
                        unsetattrib(a);
                    }
                }
            }
            uchar *src = buf;
            for(int i = 0; i < numattribs; ++i)
            {
                const attribinfo &a = attribdefs[i];
                if(forceattribs || a != lastattribs[i])
                {
                    setattrib(a, src);
                    lastattribs[i] = a;
                }
                src += a.formatsize;
            }
            lastbuf = buf;
            numlastattribs = numattribs;
            lastattribmask = attribmask;
            lastvertexsize = vertexsize;
            changedattribs = false;
        }
    }

    void begin(GLenum mode)
    {
        primtype = mode;
    }

    void begin(GLenum mode, int numverts)
    {
        primtype = mode;
        int len = numverts * vertexsize;
        if(vbooffset + len >= maxvbosize)
        {
            len = std::min(len, maxvbosize);
            if(!vbo)
            {
                glGenBuffers_(1, &vbo);
            }
            glBindBuffer_(GL_ARRAY_BUFFER, vbo);
            glBufferData_(GL_ARRAY_BUFFER, maxvbosize, nullptr, GL_STREAM_DRAW);
            vbooffset = 0;
        }
        else if(!lastvertexsize)
        {
            glBindBuffer_(GL_ARRAY_BUFFER, vbo);
        }
        void *buf = glMapBufferRange_(GL_ARRAY_BUFFER, vbooffset, len, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_UNSYNCHRONIZED_BIT);
        if(buf)
        {
            attribbuf.reset(static_cast<uchar *>(buf), len);
        }
    }

    void multidraw()
    {
        int start = multidrawstart.size() ? multidrawstart.back() + multidrawcount.back() : 0,
            count = attribbuf.length()/vertexsize - start;
        if(count > 0)
        {
            multidrawstart.push_back(start);
            multidrawcount.push_back(count);
        }
    }

    int end()
    {
        uchar *buf = attribbuf.getbuf();
        if(attribbuf.empty())
        {
            if(buf != attribdata)
            {
                glUnmapBuffer_(GL_ARRAY_BUFFER);
                attribbuf.reset(attribdata, maxvbosize);
            }
            return 0;
        }
        int start = 0;
        if(buf == attribdata)
        {
            if(vbooffset + attribbuf.length() >= maxvbosize)
            {
                if(!vbo)
                {
                    glGenBuffers_(1, &vbo);
                }
                glBindBuffer_(GL_ARRAY_BUFFER, vbo);
                glBufferData_(GL_ARRAY_BUFFER, maxvbosize, nullptr, GL_STREAM_DRAW);
                vbooffset = 0;
            }
            else if(!lastvertexsize)
            {
                glBindBuffer_(GL_ARRAY_BUFFER, vbo);
            }
            //void pointer warning!
            void *dst = glMapBufferRange_(GL_ARRAY_BUFFER, vbooffset, attribbuf.length(), GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_UNSYNCHRONIZED_BIT);
            memcpy(dst, attribbuf.getbuf(), attribbuf.length());
            glUnmapBuffer_(GL_ARRAY_BUFFER);
        }
        else
        {
            glUnmapBuffer_(GL_ARRAY_BUFFER);
        }
        buf = static_cast<uchar *>(nullptr) + vbooffset;
        if(vertexsize == lastvertexsize && buf >= lastbuf)
        {
            start = static_cast<int>(buf - lastbuf)/vertexsize;
            if(primtype == GL_QUADS && (start%4 || start + attribbuf.length()/vertexsize >= 4*maxquads))
            {
                start = 0;
            }
            else
            {
                buf = lastbuf;
            }
        }
        vbooffset += attribbuf.length();
        setattribs(buf);
        int numvertexes = attribbuf.length()/vertexsize;
        if(primtype == GL_QUADS)
        {
            if(!quadsenabled)
            {
                enablequads();
            }
            drawquads(start/4, numvertexes/4);
        }
        else
        {
            if(multidrawstart.size())
            {
                multidraw();
                if(start)
                {
                    for(uint i = 0; i < multidrawstart.size(); i++)
                    {
                        multidrawstart[i] += start;
                    }
                }
                glMultiDrawArrays_(primtype, multidrawstart.data(), multidrawcount.data(), multidrawstart.size());
                multidrawstart.resize(0);
                multidrawcount.resize(0);
            }
            else
            {
                glDrawArrays(primtype, start, numvertexes);
            }
        }
        attribbuf.reset(attribdata, maxvbosize);
        return numvertexes;
    }

    void forcedisable()
    {
        for(int i = 0; enabled; i++)
        {
            if(enabled&(1<<i))
            {
                glDisableVertexAttribArray_(i);
                enabled &= ~(1<<i);
            }
        }
        numlastattribs = lastattribmask = lastvertexsize = 0;
        lastbuf = nullptr;
        if(quadsenabled)
        {
            disablequads();
        }
        glBindBuffer_(GL_ARRAY_BUFFER, 0);
    }

    void setup()
    {
        if(!defaultvao)
        {
            glGenVertexArrays_(1, &defaultvao);
        }
        glBindVertexArray_(defaultvao);
        attribdata = new uchar[maxvbosize];
        attribbuf.reset(attribdata, maxvbosize);
    }

    void cleanup()
    {
        disable();
        if(quadindexes)
        {
            glDeleteBuffers_(1, &quadindexes);
            quadindexes = 0;
        }
        if(vbo)
        {
            glDeleteBuffers_(1, &vbo);
            vbo = 0;
        }
        vbooffset = maxvbosize;
        if(defaultvao)
        {
            glDeleteVertexArrays_(1, &defaultvao);
            defaultvao = 0;
        }
    }

    void defvertex(int size, int format) { defattrib(Attribute_Vertex, size, format); }
    void defcolor(int size, int format) { defattrib(Attribute_Color, size, format); }
    void deftexcoord0(int size, int format) { defattrib(Attribute_TexCoord0, size, format); }
    void deftexcoord1(int size, int format) { defattrib(Attribute_TexCoord1, size, format); }
    void defnormal(int size, int format) { defattrib(Attribute_Normal, size, format); }
    void deftangent(int size, int format) { defattrib(Attribute_Tangent, size, format); }
    void defboneweight(int size, int format) { defattrib(Attribute_BoneWeight, size, format); }
    void defboneindex(int size, int format) { defattrib(Attribute_BoneIndex, size, format); }

    void vertexf(float x) { glVertexAttrib1f_(Attribute_Vertex, x); }
    void vertexf(float x, float y) { glVertexAttrib2f_(Attribute_Vertex, x, y); }
    void vertexf(float x, float y, float z) { glVertexAttrib3f_(Attribute_Vertex, x, y, z); }
    void vertexf(float x, float y, float z, float w) { glVertexAttrib4f_(Attribute_Vertex, x, y, z, w); }
    void vertex(const vec &v) { glVertexAttrib3fv_(Attribute_Vertex, v.v); }
    void vertex(const vec &v, float w) { glVertexAttrib4f_(Attribute_Vertex, v.x, v.y, v.z, w); }
    void vertex(const vec2 &v) { glVertexAttrib2fv_(Attribute_Vertex, v.v); }
    void vertex(const vec4 &v) { glVertexAttrib4fv_(Attribute_Vertex, v.v); }

    void colorf(float x) { glVertexAttrib1f_(Attribute_Color, x); }
    void colorf(float x, float y) { glVertexAttrib2f_(Attribute_Color, x, y); }
    void colorf(float x, float y, float z) { glVertexAttrib3f_(Attribute_Color, x, y, z); }
    void colorf(float x, float y, float z, float w) { glVertexAttrib4f_(Attribute_Color, x, y, z, w); }

    void color(const vec &v) { glVertexAttrib3fv_(Attribute_Color, v.v); }
    void color(const vec &v, float w) { glVertexAttrib4f_(Attribute_Color, v.x, v.y, v.z, w); }
    void color(const vec2 &v) { glVertexAttrib2fv_(Attribute_Color, v.v); }
    void color(const vec4 &v) { glVertexAttrib4fv_(Attribute_Color, v.v); }

    void colorub(uchar x, uchar y, uchar z, uchar w) { glVertexAttrib4Nub_(Attribute_Color, x, y, z, w); }
    void color(const bvec &v, uchar alpha) { glVertexAttrib4Nub_(Attribute_Color, v.x, v.y, v.z, alpha); }
    void color(const bvec4 &v) { glVertexAttrib4Nubv_(Attribute_Color, v.v); }
    void texcoord0f(float x) { glVertexAttrib1f_(Attribute_TexCoord0, x); }
    void texcoord0f(float x, float y) { glVertexAttrib2f_(Attribute_TexCoord0, x, y); }
    void texcoord0f(float x, float y, float z) { glVertexAttrib3f_(Attribute_TexCoord0, x, y, z); }
    void texcoord0f(float x, float y, float z, float w) { glVertexAttrib4f_(Attribute_TexCoord0, x, y, z, w); }

    void texcoord0(const vec &v) { glVertexAttrib3fv_(Attribute_TexCoord0, v.v); }
    void texcoord0(const vec &v, float w) { glVertexAttrib4f_(Attribute_TexCoord0, v.x, v.y, v.z, w); }
    void texcoord0(const vec2 &v) { glVertexAttrib2fv_(Attribute_TexCoord0, v.v); }
    void texcoord0(const vec4 &v) { glVertexAttrib4fv_(Attribute_TexCoord0, v.v); }
    void texcoord1f(float x) { glVertexAttrib1f_(Attribute_TexCoord1, x); }
    void texcoord1f(float x, float y) { glVertexAttrib2f_(Attribute_TexCoord1, x, y); }
    void texcoord1f(float x, float y, float z) { glVertexAttrib3f_(Attribute_TexCoord1, x, y, z); }
    void texcoord1f(float x, float y, float z, float w) { glVertexAttrib4f_(Attribute_TexCoord1, x, y, z, w); }
    void texcoord1(const vec &v) { glVertexAttrib3fv_(Attribute_TexCoord1, v.v); }
    void texcoord1(const vec &v, float w) { glVertexAttrib4f_(Attribute_TexCoord1, v.x, v.y, v.z, w); }
    void texcoord1(const vec2 &v) { glVertexAttrib2fv_(Attribute_TexCoord1, v.v); }
    void texcoord1(const vec4 &v) { glVertexAttrib4fv_(Attribute_TexCoord1, v.v); }
    void normal(float x, float y, float z) { glVertexAttrib4f_(Attribute_Normal, x, y, z, 0.0f); }
    void normal(const vec &v) { glVertexAttrib4f_(Attribute_Normal, v.x, v.y, v.z, 0.0f); }
    void tangent(float x, float y, float z, float w) { glVertexAttrib4f_(Attribute_Tangent, x, y, z, w); }
    void tangent(const vec &v, float w) { glVertexAttrib4f_(Attribute_Tangent, v.x, v.y, v.z, w); }
    void tangent(const vec4 &v) { glVertexAttrib4fv_(Attribute_Tangent, v.v); }

    void enableattrib(int index) { disable(); glEnableVertexAttribArray_(index); }
    void disableattrib(int index) { glDisableVertexAttribArray_(index); }
    void enablevertex() { disable(); glEnableVertexAttribArray_(Attribute_Vertex); }
    void disablevertex() { glDisableVertexAttribArray_(Attribute_Vertex); }
    void vertexpointer(int stride, const void *data, GLenum type, int size, GLenum normalized) { disable(); glVertexAttribPointer_(Attribute_Vertex, size, type, normalized, stride, data); }
    void enablecolor() { ; glEnableVertexAttribArray_(Attribute_Color); }
    void disablecolor() { glDisableVertexAttribArray_(Attribute_Color); }
    void colorpointer(int stride, const void *data, GLenum type, int size, GLenum normalized) { ; glVertexAttribPointer_(Attribute_Color, size, type, normalized, stride, data); }
    void enabletexcoord0() { ; glEnableVertexAttribArray_(Attribute_TexCoord0); }
    void disabletexcoord0() { glDisableVertexAttribArray_(Attribute_TexCoord0); }
    void texcoord0pointer(int stride, const void *data, GLenum type, int size, GLenum normalized) { ; glVertexAttribPointer_(Attribute_TexCoord0, size, type, normalized, stride, data); }
    void enabletexcoord1() { ; glEnableVertexAttribArray_(Attribute_TexCoord1); }
    void disabletexcoord1() { glDisableVertexAttribArray_(Attribute_TexCoord1); }
    void texcoord1pointer(int stride, const void *data, GLenum type, int size, GLenum normalized) { ; glVertexAttribPointer_(Attribute_TexCoord1, size, type, normalized, stride, data); }
    void enablenormal() { ; glEnableVertexAttribArray_(Attribute_Normal); }
    void disablenormal() { glDisableVertexAttribArray_(Attribute_Normal); }
    void normalpointer(int stride, const void *data, GLenum type, int size, GLenum normalized) { ; glVertexAttribPointer_(Attribute_Normal, size, type, normalized, stride, data); }
    void enabletangent() { ; glEnableVertexAttribArray_(Attribute_Tangent); }
    void disabletangent() { glDisableVertexAttribArray_(Attribute_Tangent); }
    void tangentpointer(int stride, const void *data, GLenum type, int size, GLenum normalized) { ; glVertexAttribPointer_(Attribute_Tangent, size, type, normalized, stride, data); }
    void enableboneweight() { ; glEnableVertexAttribArray_(Attribute_BoneWeight); }
    void disableboneweight() { glDisableVertexAttribArray_(Attribute_BoneWeight); }
    void boneweightpointer(int stride, const void *data, GLenum type, int size, GLenum normalized) { ; glVertexAttribPointer_(Attribute_BoneWeight, size, type, normalized, stride, data); }
    void enableboneindex() { ; glEnableVertexAttribArray_(Attribute_BoneIndex); }
    void disableboneindex() { glDisableVertexAttribArray_(Attribute_BoneIndex); }
    void boneindexpointer(int stride, const void *data, GLenum type, int size, GLenum normalized) { ; glVertexAttribPointer_(Attribute_BoneIndex, size, type, normalized, stride, data); }

    void bindebo(GLuint ebo) { disable(); glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, ebo); }
    void clearebo() { glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0); }
    void bindvbo(GLuint vbo) { disable(); glBindBuffer_(GL_ARRAY_BUFFER, vbo); }
    void clearvbo() { glBindBuffer_(GL_ARRAY_BUFFER, 0); }

    template<class T>
    void attrib(T x)
    {
        if(attribbuf.check(sizeof(T)))
        {
            T *buf = (T *)attribbuf.pad(sizeof(T));
            buf[0] = x;
        }
    }

    template<class T>
    void attrib(T x, T y)
    {
        if(attribbuf.check(2*sizeof(T)))
        {
            T *buf = (T *)attribbuf.pad(2*sizeof(T));
            buf[0] = x;
            buf[1] = y;
        }
    }

    template<class T>
    void attrib(T x, T y, T z)
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
    void attrib(T x, T y, T z, T w)
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
    void attribv(const T *v)
    {
        attribbuf.put((const uchar *)v, N*sizeof(T));
    }

    void attribf(float x) { attrib<float>(x); }
    void attribf(float x, float y) { attrib<float>(x, y); }
    void attribf(float x, float y, float z) { attrib<float>(x, y, z); }
    void attribf(float x, float y, float z, float w) { attrib<float>(x, y, z, w); }
    void attribd(double x) { attrib<double>(x); }
    void attribd(double x, double y) { attrib<double>(x, y); }
    void attribd(double x, double y, double z) { attrib<double>(x, y, z); }
    void attribd(double x, double y, double z, double w) { attrib<double>(x, y, z, w); }
    void attribb(char x) { attrib<char>(x); }
    void attribb(char x, char y) { attrib<char>(x, y); }
    void attribb(char x, char y, char z) { attrib<char>(x, y, z); }
    void attribb(char x, char y, char z, char w) { attrib<char>(x, y, z, w); }
    void attribub(uchar x) { attrib<uchar>(x); }
    void attribub(uchar x, uchar y) { attrib<uchar>(x, y); }
    void attribub(uchar x, uchar y, uchar z) { attrib<uchar>(x, y, z); }
    void attribub(uchar x, uchar y, uchar z, uchar w) { attrib<uchar>(x, y, z, w); }
    void attribs(short x) { attrib<short>(x); }
    void attribs(short x, short y) { attrib<short>(x, y); }
    void attribs(short x, short y, short z) { attrib<short>(x, y, z); }
    void attribs(short x, short y, short z, short w) { attrib<short>(x, y, z, w); }
    void attribus(ushort x) { attrib<ushort>(x); }
    void attribus(ushort x, ushort y) { attrib<ushort>(x, y); }
    void attribus(ushort x, ushort y, ushort z) { attrib<ushort>(x, y, z); }
    void attribus(ushort x, ushort y, ushort z, ushort w) { attrib<ushort>(x, y, z, w); }
    void attribi(int x) { attrib<int>(x); }
    void attribi(int x, int y) { attrib<int>(x, y); }
    void attribi(int x, int y, int z) { attrib<int>(x, y, z); }
    void attribi(int x, int y, int z, int w) { attrib<int>(x, y, z, w); }
    void attribui(uint x) { attrib<uint>(x); }
    void attribui(uint x, uint y) { attrib<uint>(x, y); }
    void attribui(uint x, uint y, uint z) { attrib<uint>(x, y, z); }
    void attribui(uint x, uint y, uint z, uint w) { attrib<uint>(x, y, z, w); }

    void attrib(const vec &v) { attribf(v.x, v.y, v.z); }
    void attrib(const vec &v, float w) { attribf(v.x, v.y, v.z, w); }
    void attrib(const vec2 &v) { attribf(v.x, v.y); }
    void attrib(const vec4 &v) { attribf(v.x, v.y, v.z, v.w); }
    void attrib(const ivec &v) { attribi(v.x, v.y, v.z); }
    void attrib(const ivec &v, int w) { attribi(v.x, v.y, v.z, w); }
    void attrib(const ivec2 &v) { attribi(v.x, v.y); }
    void attrib(const ivec4 &v) { attribi(v.x, v.y, v.z, v.w); }
    void attrib(const bvec &b) { attribub(b.x, b.y, b.z); }
    void attrib(const bvec &b, uchar w) { attribub(b.x, b.y, b.z, w); }
    void attrib(const bvec4 &b) { attribub(b.x, b.y, b.z, b.w); }
}


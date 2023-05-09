/**
 * @brief GL immediate mode EMUlation layer (glemu).
 *
 * This file replicates some of the functionality of the long since removed glBegin/glEnd
 * features from extremely outdated versions of OpenGL.
 */
#include "../libprimis-headers/cube.h"

#include "glemu.h"
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
    static int enabled = 0;
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
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadindexes);
            return;
        }

        glGenBuffers(1, &quadindexes);
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
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadindexes);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, maxquads*6*sizeof(ushort), data, GL_STATIC_DRAW);
        delete[] data;
    }

    void disablequads()
    {
        quadsenabled = false;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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
        glDrawRangeElements(GL_TRIANGLES, offset*4, (offset + count)*4-1, count*6, GL_UNSIGNED_SHORT, (ushort *)0 + offset*6);
    }

    static void defattrib(int type, int size, int format)
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

    static void setattrib(const attribinfo &a, uchar *buf)
    {
        switch(a.type)
        {
            case Attribute_Vertex:
            case Attribute_TexCoord0:
            case Attribute_TexCoord1:
            case Attribute_BoneIndex:
            {
                glVertexAttribPointer(a.type, a.size, a.format, GL_FALSE, vertexsize, buf);
                break;
            }
            case Attribute_Color:
            case Attribute_Normal:
            case Attribute_Tangent:
            case Attribute_BoneWeight:
            {
                glVertexAttribPointer(a.type, a.size, a.format, GL_TRUE, vertexsize, buf);
                break;
            }
        }
        if(!(enabled&(1<<a.type)))
        {
            glEnableVertexAttribArray(a.type);
            enabled |= 1<<a.type;
        }
    }

    static void unsetattrib(const attribinfo &a)
    {
        glDisableVertexAttribArray(a.type);
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
                glGenBuffers(1, &vbo);
            }
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, maxvbosize, nullptr, GL_STREAM_DRAW);
            vbooffset = 0;
        }
        else if(!lastvertexsize)
        {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
        }
        void *buf = glMapBufferRange(GL_ARRAY_BUFFER, vbooffset, len, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_UNSYNCHRONIZED_BIT);
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
                glUnmapBuffer(GL_ARRAY_BUFFER);
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
                    glGenBuffers(1, &vbo);
                }
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, maxvbosize, nullptr, GL_STREAM_DRAW);
                vbooffset = 0;
            }
            else if(!lastvertexsize)
            {
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
            }
            //void pointer warning!
            void *dst = glMapBufferRange(GL_ARRAY_BUFFER, vbooffset, attribbuf.length(), GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_UNSYNCHRONIZED_BIT);
            std::memcpy(dst, attribbuf.getbuf(), attribbuf.length());
            glUnmapBuffer(GL_ARRAY_BUFFER);
        }
        else
        {
            glUnmapBuffer(GL_ARRAY_BUFFER);
        }
        buf = static_cast<uchar *>(nullptr) + vbooffset;
        if(vertexsize == lastvertexsize && buf >= lastbuf)
        {
            start = static_cast<int>(buf - lastbuf)/vertexsize;
            buf = lastbuf;
        }
        vbooffset += attribbuf.length();
        setattribs(buf);
        int numvertexes = attribbuf.length()/vertexsize;
        if(multidrawstart.size())
        {
            multidraw();
            if(start)
            {
                //offset the buffer indices by start point
                for(GLint &i : multidrawstart)
                {
                    i += start;
                }
            }
            glMultiDrawArrays(primtype, multidrawstart.data(), multidrawcount.data(), multidrawstart.size());
            multidrawstart.clear();
            multidrawcount.clear();
        }
        else
        {
            glDrawArrays(primtype, start, numvertexes);
        }
        attribbuf.reset(attribdata, maxvbosize);
        return numvertexes;
    }

    static void forcedisable()
    {
        for(int i = 0; enabled; i++)
        {
            if(enabled&(1<<i))
            {
                glDisableVertexAttribArray(i);
                enabled &= ~(1<<i);
            }
        }
        numlastattribs = lastattribmask = lastvertexsize = 0;
        lastbuf = nullptr;
        if(quadsenabled)
        {
            disablequads();
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void disable()
    {
        if(enabled)
        {
            forcedisable();
        }
    }

    void setup()
    {
        if(!defaultvao)
        {
            glGenVertexArrays(1, &defaultvao);
        }
        glBindVertexArray(defaultvao);
        attribdata = new uchar[maxvbosize];
        attribbuf.reset(attribdata, maxvbosize);
    }

    void cleanup()
    {
        disable();
        if(quadindexes)
        {
            glDeleteBuffers(1, &quadindexes);
            quadindexes = 0;
        }
        if(vbo)
        {
            glDeleteBuffers(1, &vbo);
            vbo = 0;
        }
        vbooffset = maxvbosize;
        if(defaultvao)
        {
            glDeleteVertexArrays(1, &defaultvao);
            defaultvao = 0;
        }
    }

    void defvertex(int size, int format)
    {
        defattrib(Attribute_Vertex, size, format);
    }

    void defcolor(int size, int format)
    {
        defattrib(Attribute_Color, size, format);
    }

    void deftexcoord0(int size, int format)
    {
        defattrib(Attribute_TexCoord0, size, format);
    }

    void defnormal(int size, int format)
    {
        defattrib(Attribute_Normal, size, format);
    }

    void colorf(float x, float y, float z, float w)
    {
        if(w != 0.0f)
        {
            glVertexAttrib4f(Attribute_Color, x, y, z, w);
        }
        else
        {
            glVertexAttrib3f(Attribute_Color, x, y, z);
        }
    }

    void color(const vec &v)
    {
        glVertexAttrib3fv(Attribute_Color, v.v);
    }

    void color(const vec &v, float w)
    {
        glVertexAttrib4f(Attribute_Color, v.x, v.y, v.z, w);
    }

    void colorub(uchar x, uchar y, uchar z, uchar w)
    {
        glVertexAttrib4Nub(Attribute_Color, x, y, z, w);
    }

    void color(const bvec &v, uchar alpha)
    {
        glVertexAttrib4Nub(Attribute_Color, v.x, v.y, v.z, alpha);
    }

    void enablevertex()
    {
        disable();
        glEnableVertexAttribArray(Attribute_Vertex);
    }

    void disablevertex()
    {
        glDisableVertexAttribArray(Attribute_Vertex);
    }
    void vertexpointer(int stride, const void *data, GLenum type, int size, GLenum normalized)
    {
        disable();
        glVertexAttribPointer(Attribute_Vertex, size, type, normalized, stride, data);
    }
    void enablecolor()
    {
        glEnableVertexAttribArray(Attribute_Color);
    }
    void disablecolor()
    {
        glDisableVertexAttribArray(Attribute_Color);
    }
    void colorpointer(int stride, const void *data, GLenum type, int size, GLenum normalized)
    {
        glVertexAttribPointer(Attribute_Color, size, type, normalized, stride, data);
    }

    void enabletexcoord0()
    {
        glEnableVertexAttribArray(Attribute_TexCoord0);
    }

    void disabletexcoord0()
    {
        glDisableVertexAttribArray(Attribute_TexCoord0);
    }

    void texcoord0pointer(int stride, const void *data, GLenum type, int size, GLenum normalized)
    {
        glVertexAttribPointer(Attribute_TexCoord0, size, type, normalized, stride, data);
    }

    void enablenormal()
    {
        glEnableVertexAttribArray(Attribute_Normal);
    }

    void disablenormal()
    {
        glDisableVertexAttribArray(Attribute_Normal);
    }

    void normalpointer(int stride, const void *data, GLenum type, int size, GLenum normalized)
    {
        glVertexAttribPointer(Attribute_Normal, size, type, normalized, stride, data);
    }

    void enabletangent()
    {
        glEnableVertexAttribArray(Attribute_Tangent);
    }

    void disabletangent()
    {
        glDisableVertexAttribArray(Attribute_Tangent);
    }

    void tangentpointer(int stride, const void *data, GLenum type, int size, GLenum normalized)
    {
        glVertexAttribPointer(Attribute_Tangent, size, type, normalized, stride, data);
    }

    void enableboneweight()
    {
        glEnableVertexAttribArray(Attribute_BoneWeight);
    }

    void disableboneweight()
    {
        glDisableVertexAttribArray(Attribute_BoneWeight);
    }

    void boneweightpointer(int stride, const void *data, GLenum type, int size, GLenum normalized)
    {
        glVertexAttribPointer(Attribute_BoneWeight, size, type, normalized, stride, data);
    }

    void enableboneindex()
    {
        glEnableVertexAttribArray(Attribute_BoneIndex);
    }

    void disableboneindex()
    {
        glDisableVertexAttribArray(Attribute_BoneIndex);
    }

    void boneindexpointer(int stride, const void *data, GLenum type, int size, GLenum normalized)
    {
        glVertexAttribPointer(Attribute_BoneIndex, size, type, normalized, stride, data);
    }

    void bindebo(GLuint ebo)
    {
        disable();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    }

    void clearebo()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void bindvbo(GLuint vbo)
    {
        disable();
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
    }

    void clearvbo()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
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

    void attribf(float x, float y)
    {
        attrib<float>(x, y);
    }

    void attribf(float x, float y, float z)
    {
        attrib<float>(x, y, z);
    }

    void attribub(uchar x, uchar y, uchar z, uchar w)
    {
        attrib<uchar>(x, y, z, w);
    }

    void attrib(const vec &v)
    {
        attrib<float>(v.x, v.y, v.z);
    }

    void attrib(const vec &v, float w)
    {
        attrib<float>(v.x, v.y, v.z, w);
    }

    void attrib(const vec2 &v)
    {
        attrib<float>(v.x, v.y);
    }
}


#include "../libprimis-headers/cube.h"

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

    static const int maxquads = (0x10000/4); //65635/4 = 16384
    static GLuint quadindexes = 0;
    static bool quadsenabled = false;

    static const int maxvbosize = (1024*1024*4);
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

    static inline void setattrib(const attribinfo &a, uchar *buf)
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

    static inline void unsetattrib(const attribinfo &a)
    {
        glDisableVertexAttribArray_(a.type);
        enabled &= ~(1<<a.type);
    }

    static inline void setattribs(uchar *buf)
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
            len = min(len, maxvbosize);
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
            attribbuf.reset((uchar *)buf, len);
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
        buf = (uchar *)0 + vbooffset;
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
}


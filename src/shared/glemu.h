/**
 * @file glemu.h
 * @brief
 */

#ifndef GLEMU_H_
#define GLEMU_H_

namespace gle
{
    enum GLAttributes
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

    inline const char * const attribnames[Attribute_NumAttributes] = { "vvertex", "vcolor", "vtexcoord0", "vtexcoord1", "vnormal", "vtangent", "vboneweight", "vboneindex" };
    extern ucharbuf attribbuf;

    extern void disable();

    extern void begin(GLenum mode);
    extern void begin(GLenum mode, int numverts);
    extern void multidraw();

    extern void defvertex(int size = 3, int format = GL_FLOAT);
    extern void defcolor(int size = 3, int format = GL_FLOAT);
    extern void deftexcoord0(int size = 2, int format = GL_FLOAT);
    extern void defnormal(int size = 3, int format = GL_FLOAT);

    /**
     *  if w = 0, then glvertexattrib3f is called, else glvertexattrib4f
     */
    extern void colorf(float x, float y, float z, float w = 0.0f);

    extern void color(const vec &v);
    extern void color(const vec &v, float w);

    extern void colorub(uchar x, uchar y, uchar z, uchar w = 255);
    extern void color(const bvec &v, uchar alpha = 255);

    extern void enablevertex();
    extern void disablevertex();
    extern void vertexpointer(int stride, const void *data, GLenum type = GL_FLOAT, int size = 3, GLenum normalized = GL_FALSE);
    extern void enablecolor();
    extern void disablecolor();
    extern void colorpointer(int stride, const void *data, GLenum type = GL_UNSIGNED_BYTE, int size = 4, GLenum normalized = GL_TRUE);
    extern void enabletexcoord0();
    extern void disabletexcoord0();
    extern void texcoord0pointer(int stride, const void *data, GLenum type = GL_FLOAT, int size = 2, GLenum normalized = GL_FALSE);
    extern void enablenormal();
    extern void disablenormal();
    extern void normalpointer(int stride, const void *data, GLenum type = GL_FLOAT, int size = 3, GLenum normalized = GL_TRUE);
    extern void enabletangent();
    extern void disabletangent();
    extern void tangentpointer(int stride, const void *data, GLenum type = GL_FLOAT, int size = 4, GLenum normalized = GL_TRUE);
    extern void enableboneweight();
    extern void disableboneweight();
    extern void boneweightpointer(int stride, const void *data, GLenum type = GL_UNSIGNED_BYTE, int size = 4, GLenum normalized = GL_TRUE);
    extern void enableboneindex();
    extern void disableboneindex();
    extern void boneindexpointer(int stride, const void *data, GLenum type = GL_UNSIGNED_BYTE, int size = 4, GLenum normalized = GL_FALSE);

    extern void bindebo(GLuint ebo);
    extern void clearebo();
    extern void bindvbo(GLuint vbo);
    extern void clearvbo();

    extern void attribf(float x, float y);
    extern void attribf(float x, float y, float z);

    extern void attribub(uchar x, uchar y, uchar z, uchar w);

    extern void attrib(const vec &v);
    extern void attrib(const vec &v, float w);
    extern void attrib(const vec2 &v);

    extern int end();

    extern void enablequads();
    extern void disablequads();
    extern void drawquads(int offset, int count);

    extern void setup();
    extern void cleanup();
}

#endif /* GLEMU_H_ */

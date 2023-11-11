#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "rendergl.h"
#include "world/octaworld.h"

namespace lightsphere
{
    namespace
    {
        size_t numverts   = 0,
               numindices = 0;
        GLuint vbuf = 0, //the GLuint pointing to the lightsphere vertex buffer obj; bound to a buffer with init(), its texture deleted with cleanup(), bound by enable()
               ebuf = 0; //the GLuint pointing to the lightsphere element buffer obj; bound to a buffer with init(), its texture deleted with cleanup(), bound by enable()

        void init(size_t slices, size_t stacks)
        {
            numverts = (stacks+1)*(slices+1);
            vec * verts = new vec[numverts];
            float ds = 1.0f/slices,
                  dt = 1.0f/stacks,
                  t  = 1.0f;
            for(size_t i = 0; i < stacks+1; ++i)
            {
                float rho    = M_PI*(1-t),
                      s      = 0.0f,
                      sinrho = i && i < stacks ? std::sin(rho) : 0,
                      cosrho = !i ? 1 : (i < stacks ? std::cos(rho) : -1);
                for(size_t j = 0; j < slices+1; ++j)
                {
                    float theta = j==slices ? 0 : 2*M_PI*s;
                    verts[i*(slices+1) + j] = vec(-std::sin(theta)*sinrho, -std::cos(theta)*sinrho, cosrho);
                    s += ds;
                }
                t -= dt;
            }

            numindices = (stacks-1)*slices*3*2;
            GLushort *indices = new ushort[numindices];
            GLushort *curindex = indices;
            for(size_t i = 0; i < stacks; ++i)
            {
                for(size_t k = 0; k < slices; ++k)
                {
                    size_t j = i%2 ? slices-k-1 : k;
                    if(i)
                    {
                        *curindex++ = i*(slices+1)+j;
                        *curindex++ = i*(slices+1)+j+1;
                        *curindex++ = (i+1)*(slices+1)+j;
                    }
                    if(i+1 < stacks)
                    {
                        *curindex++ = i*(slices+1)+j+1;
                        *curindex++ = (i+1)*(slices+1)+j+1;
                        *curindex++ = (i+1)*(slices+1)+j;
                    }
                }
            }
            if(!vbuf)
            {
                glGenBuffers(1, &vbuf);
            }
            gle::bindvbo(vbuf);
            glBufferData(GL_ARRAY_BUFFER, numverts*sizeof(vec), verts, GL_STATIC_DRAW);
            delete[] verts;
            verts = nullptr;
            if(!ebuf)
            {
                glGenBuffers(1, &ebuf);
            }
            gle::bindebo(ebuf);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, numindices*sizeof(GLushort), indices, GL_STATIC_DRAW);
            delete[] indices;
            indices = nullptr;
        }
    }
    void cleanup()
    {
        if(vbuf)
        {
            glDeleteBuffers(1, &vbuf);
            vbuf = 0;
        }
        if(ebuf)
        {
            glDeleteBuffers(1, &ebuf);
            ebuf = 0;
        }
    }

    void enable()
    {
        if(!vbuf)
        {
            init(8, 4);
        }
        gle::bindvbo(vbuf);
        gle::bindebo(ebuf);
        gle::vertexpointer(sizeof(vec), nullptr);
        gle::enablevertex();
    }

    void draw()
    {
        glDrawRangeElements(GL_TRIANGLES, 0, numverts-1, numindices, GL_UNSIGNED_SHORT, nullptr);
        xtraverts += numindices;
        glde++;
    }

    void disable()
    {
        gle::disablevertex();
        gle::clearvbo();
        gle::clearebo();
    }
}

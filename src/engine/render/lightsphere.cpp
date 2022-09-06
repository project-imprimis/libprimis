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
        vec *verts = nullptr;
        GLushort *indices = nullptr;
        int numverts   = 0,
            numindices = 0;
        GLuint vbuf = 0,
               ebuf = 0;

        void init(int slices, int stacks)
        {
            numverts = (stacks+1)*(slices+1);
            verts = new vec[numverts];
            float ds = 1.0f/slices,
                  dt = 1.0f/stacks,
                  t  = 1.0f;
            for(int i = 0; i < stacks+1; ++i)
            {
                float rho    = M_PI*(1-t),
                      s      = 0.0f,
                      sinrho = i && i < stacks ? std::sin(rho) : 0,
                      cosrho = !i ? 1 : (i < stacks ? std::cos(rho) : -1);
                for(int j = 0; j < slices+1; ++j)
                {
                    float theta = j==slices ? 0 : 2*M_PI*s;
                    verts[i*(slices+1) + j] = vec(-std::sin(theta)*sinrho, -std::cos(theta)*sinrho, cosrho);
                    s += ds;
                }
                t -= dt;
            }

            numindices = (stacks-1)*slices*3*2;
            indices = new ushort[numindices];
            GLushort *curindex = indices;
            for(int i = 0; i < stacks; ++i)
            {
                for(int k = 0; k < slices; ++k)
                {
                    int j = i%2 ? slices-k-1 : k;
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
        gle::vertexpointer(sizeof(vec), verts);
        gle::enablevertex();
    }

    void draw()
    {
        glDrawRangeElements(GL_TRIANGLES, 0, numverts-1, numindices, GL_UNSIGNED_SHORT, indices);
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

VARP(softexplosion, 0, 1, 1);
VARP(softexplosionblend, 1, 16, 64);

namespace sphere
{
    struct vert
    {
        vec pos;
        ushort s, t;
    } *verts = NULL;
    GLushort *indices = NULL;
    int numverts = 0,
        numindices = 0;
    GLuint vbuf = 0,
           ebuf = 0;

    void init(int slices, int stacks)
    {
        numverts = (stacks+1)*(slices+1);
        verts = new vert[numverts];
        float ds = 1.0f/slices,
              dt = 1.0f/stacks,
              t  = 1.0f;
        for(int i = 0; i < stacks+1; ++i)
        {
            float rho = M_PI*(1-t),
                  s = 0.0f,
                  sinrho = i && i < stacks ? sin(rho) : 0,
                  cosrho = !i ? 1 : (i < stacks ? cos(rho) : -1);
            for(int j = 0; j < slices+1; ++j)
            {
                float theta = j==slices ? 0 : 2*M_PI*s;
                vert &v = verts[i*(slices+1) + j];
                v.pos = vec(sin(theta)*sinrho, cos(theta)*sinrho, -cosrho);
                v.s = static_cast<ushort>(s*0xFFFF);
                v.t = static_cast<ushort>(t*0xFFFF);
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
                    *curindex++ = (i+1)*(slices+1)+j;
                    *curindex++ = i*(slices+1)+j+1;
                }
                if(i+1 < stacks)
                {
                    *curindex++ = i*(slices+1)+j+1;
                    *curindex++ = (i+1)*(slices+1)+j;
                    *curindex++ = (i+1)*(slices+1)+j+1;
                }
            }
        }
        if(!vbuf)
        {
            glGenBuffers_(1, &vbuf);
        }
        gle::bindvbo(vbuf);
        glBufferData_(GL_ARRAY_BUFFER, numverts*sizeof(vert), verts, GL_STATIC_DRAW);
        DELETEA(verts);
        if(!ebuf)
        {
            glGenBuffers_(1, &ebuf);
        }
        gle::bindebo(ebuf);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, numindices*sizeof(GLushort), indices, GL_STATIC_DRAW);
        DELETEA(indices);
    }

    void cleanup()
    {
        if(vbuf)
        {
            glDeleteBuffers_(1, &vbuf);
            vbuf = 0;
        }
        if(ebuf)
        {
            glDeleteBuffers_(1, &ebuf);
            ebuf = 0;
        }
    }

    void enable()
    {
        if(!vbuf)
        {
            init(12, 6); //12 slices, 6 stacks
        }
        gle::bindvbo(vbuf);
        gle::bindebo(ebuf);

        gle::vertexpointer(sizeof(vert), &verts->pos);
        gle::texcoord0pointer(sizeof(vert), &verts->s, GL_UNSIGNED_SHORT, 2, GL_TRUE);
        gle::enablevertex();
        gle::enabletexcoord0();
    }

    void draw()
    {
        glDrawRangeElements_(GL_TRIANGLES, 0, numverts-1, numindices, GL_UNSIGNED_SHORT, indices);
        xtraverts += numindices;
        glde++;
    }

    void disable()
    {
        gle::disablevertex();
        gle::disabletexcoord0();

        gle::clearvbo();
        gle::clearebo();
    }
}

static const float wobble = 1.25f;

struct fireballrenderer : listrenderer
{
    fireballrenderer(const char *texname)
        : listrenderer(texname, 0, PT_FIREBALL|PT_SHADER)
    {}

    void startrender()
    {
        if(softparticles && softexplosion)
        {
            SETSHADER(explosionsoft);
        }
        else
        {
            SETSHADER(explosion);
        }
        sphere::enable();
    }

    void endrender()
    {
        sphere::disable();
    }

    void cleanup()
    {
        sphere::cleanup();
    }

    void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
    {
        pe.maxfade = max(pe.maxfade, fade);
        pe.extendbb(o, (size+1+pe.ent->attr2)*wobble);
    }

    void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts)
    {
        float pmax = p->val,
              size = p->fade ? static_cast<float>(ts)/p->fade : 1,
              psize = p->size + pmax * size;

        if(isfoggedsphere(psize*wobble, p->o))
        {
            return;
        }
        vec dir = static_cast<vec>(o).sub(camera1->o), s, t;
        float dist = dir.magnitude();
        bool inside = dist <= psize*wobble;
        if(inside)
        {
            s = camright;
            t = camup;
        }
        else
        {
            float mag2 = dir.magnitude2();
            dir.x /= mag2;
            dir.y /= mag2;
            dir.z /= dist;
            s = vec(dir.y, -dir.x, 0);
            t = vec(dir.x*dir.z, dir.y*dir.z, -mag2/dist);
        }

        matrix3 rot(lastmillis/1000.0f*143*RAD, vec(1/SQRT3, 1/SQRT3, 1/SQRT3));
        LOCALPARAM(texgenS, rot.transposedtransform(s));
        LOCALPARAM(texgenT, rot.transposedtransform(t));

        matrix4 m(rot, o);
        m.scale(psize, psize, inside ? -psize : psize);
        m.mul(camprojmatrix, m);
        LOCALPARAM(explosionmatrix, m);

        LOCALPARAM(center, o);
        LOCALPARAMF(blendparams, inside ? 0.5f : 4, inside ? 0.25f : 0);
        if(2*(p->size + pmax)*wobble >= softexplosionblend)
        {
            LOCALPARAMF(softparams, -1.0f/softexplosionblend, 0, inside ? blend/(2*255.0f) : 0);
        }
        else
        {
            LOCALPARAMF(softparams, 0, -1, inside ? blend/(2*255.0f) : 0);
        }

        vec color = p->color.tocolor().mul(ldrscale);
        float alpha = blend/255.0f;

        for(int i = 0; i < (inside ? 2 : 1); ++i)
        {
            gle::color(color, i ? alpha/2 : alpha);
            if(i)
            {
                glDepthFunc(GL_GEQUAL);
            }
            sphere::draw();
            if(i)
            {
                glDepthFunc(GL_LESS);
            }
        }
    }
};
static fireballrenderer fireballs("media/particle/explosion.png"), pulsebursts("media/particle/pulse_burst.png");


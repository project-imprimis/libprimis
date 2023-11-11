// postfx.cpp: screenspace shader post effects

#include "../libprimis-headers/cube.h"

#include "rendergl.h"
#include "renderlights.h"
#include "rendertimers.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/console.h"

class postfx
{
    public:
        void cleanuppostfx(bool fullclean)
        {
            if(fullclean && postfxfb)
            {
                glDeleteFramebuffers(1, &postfxfb);
                postfxfb = 0;
            }
            for(const postfxtex &i : postfxtexs)
            {
                glDeleteTextures(1, &i.id);
            }
            postfxtexs.clear();
            postfxw = 0;
            postfxh = 0;
        }

        void clearpostfx()
        {
            postfxpasses.clear();
            cleanuppostfx(false);
        }

        void addpostfx(const char *name, const int *bind, const int *scale, const char *inputs, const float *x, const float *y, const float *z, const float *w)
        {
            int inputmask = inputs[0] ? 0 : 1,
                freemask = inputs[0] ? 0 : 1;
            bool freeinputs = true;
            for(; *inputs; inputs++)
            {
                if(isdigit(*inputs))
                {
                    inputmask |= 1<<(*inputs-'0');
                    if(freeinputs)
                    {
                        freemask |= 1<<(*inputs-'0');
                    }
                }
                else if(*inputs=='+')
                {
                    freeinputs = false;
                }
                else if(*inputs=='-')
                {
                    freeinputs = true;
                }
            }
            inputmask &= (1<<numpostfxbinds)-1;
            freemask &= (1<<numpostfxbinds)-1;
            addpostfx(name, std::clamp(*bind, 0, numpostfxbinds-1), std::max(*scale, 0), inputmask, freemask, vec4<float>(*x, *y, *z, *w));
        }

        void setpostfx(const char *name, const float *x, const float *y, const float *z, const float *w)
        {
            clearpostfx();
            if(name[0])
            {
                addpostfx(name, 0, 0, 1, 1, vec4<float>(*x, *y, *z, *w));
            }
        } //add a postfx shader to the class field, with name & 4d pos vector

        GLuint setuppostfx(const GBuffer &buf, int w, int h, GLuint outfbo)
        {
            if(postfxpasses.empty())
            {
                return outfbo;
            }
            if(postfxw != w || postfxh != h)
            {
                cleanuppostfx(false);
                postfxw = w;
                postfxh = h;
            }
            for(int i = 0; i < numpostfxbinds; ++i)
            {
                postfxbinds[i] = -1;
            }
            for(postfxtex &i : postfxtexs)
            {
                i.used = -1;
            }
            if(!postfxfb)
            {
                glGenFramebuffers(1, &postfxfb);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, postfxfb);
            int tex = allocatepostfxtex(0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, postfxtexs[tex].id, 0);
            buf.bindgdepth();

            postfxbinds[0] = tex;
            postfxtexs[tex].used = 0;

            return postfxfb;
        }

        void renderpostfx(GLuint outfbo)
        {
            if(postfxpasses.empty())
            {
                return;
            }
            timer *postfxtimer = begintimer("postfx");
            for(uint i = 0; i < postfxpasses.size(); i++)
            {
                postfxpass &p = postfxpasses[i];

                int tex = -1;
                if(!(postfxpasses.size() < i+1))
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, outfbo);
                }
                else
                {
                    tex = allocatepostfxtex(p.outputscale);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, postfxtexs[tex].id, 0);
                }
                int w = tex >= 0 ? std::max(postfxw>>postfxtexs[tex].scale, 1) : postfxw,
                    h = tex >= 0 ? std::max(postfxh>>postfxtexs[tex].scale, 1) : postfxh;
                glViewport(0, 0, w, h);
                p.shader->set();
                LOCALPARAM(params, p.params);
                int tw = w,
                    th = h,
                    tmu = 0;
                for(int j = 0; j < numpostfxbinds; ++j)
                {
                    if(p.inputs&(1<<j) && postfxbinds[j] >= 0)
                    {
                        if(!tmu)
                        {
                            tw = std::max(postfxw>>postfxtexs[postfxbinds[j]].scale, 1);
                            th = std::max(postfxh>>postfxtexs[postfxbinds[j]].scale, 1);
                        }
                        else
                        {
                            glActiveTexture(GL_TEXTURE0 + tmu);
                        }
                        glBindTexture(GL_TEXTURE_RECTANGLE, postfxtexs[postfxbinds[j]].id);
                        ++tmu;
                    }
                }
                if(tmu)
                {
                    glActiveTexture(GL_TEXTURE0);
                }
                screenquad(tw, th);
                for(int j = 0; j < numpostfxbinds; ++j)
                {
                    if(p.freeinputs&(1<<j) && postfxbinds[j] >= 0)
                    {
                        postfxtexs[postfxbinds[j]].used = -1;
                        postfxbinds[j] = -1;
                    }
                }
                if(tex >= 0)
                {
                    if(postfxbinds[p.outputbind] >= 0)
                    {
                        postfxtexs[postfxbinds[p.outputbind]].used = -1;
                    }
                    postfxbinds[p.outputbind] = tex;
                    postfxtexs[tex].used = p.outputbind;
                }
            }
            endtimer(postfxtimer);
        }

    private:
        static constexpr int numpostfxbinds = 10;

        int allocatepostfxtex(int scale)
        {
            for(uint i = 0; i < postfxtexs.size(); i++)
            {
                postfxtex &t = postfxtexs[i];
                if(t.scale==scale && t.used < 0)
                {
                    return i;
                }
            }
            postfxtex t;
            t.scale = scale;
            glGenTextures(1, &t.id);
            createtexture(t.id, std::max(postfxw>>scale, 1), std::max(postfxh>>scale, 1), nullptr, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE);
            postfxtexs.push_back(t);
            return postfxtexs.size()-1;
        }

        //adds to the global postfxpasses vector a postfx by the given name
        bool addpostfx(const char *name, int outputbind, int outputscale, uint inputs, uint freeinputs, const vec4<float> &params)
        {
            if(!*name)
            {
                return false;
            }
            Shader *s = useshaderbyname(name);
            if(!s)
            {
                conoutf(Console_Error, "no such postfx shader: %s", name);
                return false;
            }
            postfxpass p;
            p.shader = s;
            p.outputbind = outputbind;
            p.outputscale = outputscale;
            p.inputs = inputs;
            p.freeinputs = freeinputs;
            p.params = params;
            postfxpasses.push_back(p);
            return true;
        }

        struct postfxtex
        {
            GLuint id;
            int scale, used;
            postfxtex() : id(0), scale(0), used(-1) {}
        };
        std::vector<postfxtex> postfxtexs;
        int postfxbinds[numpostfxbinds];
        GLuint postfxfb = 0;
        int postfxw = 0,
            postfxh = 0;
        struct postfxpass
        {
            Shader *shader;
            vec4<float> params;
            uint inputs, freeinputs;
            int outputbind, outputscale;

            postfxpass() : shader(nullptr), inputs(1), freeinputs(1), outputbind(0), outputscale(0) {}
        };
        std::vector<postfxpass> postfxpasses;
};

postfx pfx;

GLuint setuppostfx(const GBuffer &buf, int w, int h, GLuint outfbo)
{
    return pfx.setuppostfx(buf, w, h, outfbo);
}

void renderpostfx(GLuint outfbo)
{
    pfx.renderpostfx(outfbo);
}

void cleanuppostfx(bool fullclean)
{
    pfx.cleanuppostfx(fullclean);
}

void addpostfxcmd(const char *name, const int *bind, const int *scale, const char *inputs, const float *x, const float *y, const float *z, const float *w)
{
    pfx.addpostfx(name, bind, scale, inputs, x, y, z, w);
}

void clearpostfx()
{
    pfx.clearpostfx();
}

void setpostfx(const char *name, const float *x, const float *y, const float *z, const float *w)
{
    pfx.setpostfx(name, x, y, z, w);
}

void initpostfxcmds()
{
    addcommand("clearpostfx", reinterpret_cast<identfun>(clearpostfx), "", Id_Command);
    addcommand("addpostfx", reinterpret_cast<identfun>(addpostfxcmd), "siisffff", Id_Command);
    addcommand("setpostfx", reinterpret_cast<identfun>(setpostfx), "sffff", Id_Command);
}

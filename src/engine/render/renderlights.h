#ifndef RENDERLIGHTS_H_
#define RENDERLIGHTS_H_

/* gbuffer: a singleton object used to store the graphics buffers
 * (as OpenGL uints) and the functions which act upon the g-buffers.
 */
class GBuffer
{
    public:
        GBuffer()
        {
            //set all of the textures to 0/null
            gfbo = 0;
            gdepthtex = 0;
            gcolortex = 0;
            gnormaltex = 0;
            gglowtex = 0;
            gdepthrb = 0;
            gstencilrb = 0;
            refractfbo = 0;
            refracttex = 0;
        }
        //main g-buffers
        void cleanupgbuffer();
        void preparegbuffer(bool depthclear = true);
        void rendercsmshadowmaps();
        void rendershadowmaps(int offset = 0);
        void renderao();
        void renderradiancehints();
        void rendertransparent();
        void resolvemsaadepth(int w, int h);
        void setupgbuffer();
        void bindgdepth();
        void bindlighttexs(int msaapass, bool transparent);
        void renderparticles(int layer = 0);
        void rendervolumetric();
        void renderwaterfog(int mat, float surface);
        void setaavelocityparams(GLenum tmu = GL_TEXTURE0);
        void shademodelpreview(int x, int y, int w, int h, bool background = true, bool scissor = false);
        void viewdepth();
        //refractive
        void processhdr(GLuint outfbo, int aa);
        void viewrefract();

    private:
        //main g-buffers
        GLuint gfbo,
           gdepthtex,
           gcolortex,
           gnormaltex,
           gglowtex,
           gdepthrb,
           gstencilrb;
        //refractive g-buffers
        GLuint refractfbo,
               refracttex;

};

extern GBuffer gbuf;

#endif

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
            gfbo         = 0;
            gdepthtex    = 0;
            gcolortex    = 0;
            gnormaltex   = 0;
            gglowtex     = 0;
            gdepthrb     = 0;
            gstencilrb   = 0;
            msfbo        = 0;
            msdepthtex   = 0;
            mscolortex   = 0;
            msnormaltex  = 0;
            msglowtex    = 0;
            msdepthrb    = 0;
            msstencilrb  = 0;
            mshdrfbo     = 0;
            mshdrtex     = 0;
            msrefractfbo = 0;
            msrefracttex = 0;
            refractfbo   = 0;
            refracttex   = 0;
            scalefbo[0]  = 0;
            scalefbo[1]  = 0;
            scaletex[0]  = 0;
            scaletex[1]  = 0;
            stencilformat= 0;
            gdepthinit   = false;
            hdrfloat     = false;
            msaadepthblit= false;
        }
        //main g-buffers
        void cleanupgbuffer();
        void preparegbuffer(bool depthclear = true);
        void rendercsmshadowmaps();
        void rendershadowmaps(int offset = 0);
        void renderao();                                    //ao.cpp
        void renderradiancehints();                         //radiancehints.cpp
        void rendertransparent();                           //rendertransparent.cpp
        void resolvemsaadepth(int w, int h);
        void setupgbuffer();
        void bindgdepth();
        void bindlighttexs(int msaapass, bool transparent);
        void renderparticles(int layer = 0);                //renderparticles.cpp
        void rendervolumetric();
        void renderwaterfog(int mat, float surface);        //water.cpp
        void setaavelocityparams(GLenum tmu = GL_TEXTURE0); //aa.cpp
        void shademodelpreview(int x, int y, int w, int h, bool background = true, bool scissor = false);
        void viewdepth();
        //multisample antialiasing specific buffers
        void setupmsbuffer(int w, int h);
        void resolvemsaacolor(int w, int h);
        void shademinimap(const vec &color = vec(-1, -1, -1));
        void shadesky();
        //refractive
        void processhdr(GLuint outfbo, int aa);
        void viewrefract();
        void doscale(GLuint outfbo = 0);
        void setupscale(int sw, int sh, int w, int h);
        GLuint shouldscale();

    private:
        void bindmsdepth();
        void cleanupscale();
        void cleanupmsbuffer();

        bool gdepthinit;
        bool hdrfloat;
        bool msaadepthblit; //no way to change this outside constructor atm

        //main g-buffers
        GLuint gfbo,
               gdepthtex,
               gcolortex,
               gnormaltex,
               gglowtex,
               gdepthrb,
               gstencilrb;
        //multisample antialiasing g-buffers
        GLuint msfbo,
               msdepthtex,
               mscolortex,
               msnormaltex,
               msglowtex,
               msdepthrb,
               msstencilrb,
               mshdrfbo,
               mshdrtex,
               msrefractfbo,
               msrefracttex;
        //refractive g-buffers
        GLuint refractfbo,
               refracttex;
        //rescaling g-buffers
        GLuint scalefbo[2],
               scaletex[2];
        GLenum stencilformat;
        matrix4 eyematrix;
};

extern GBuffer gbuf;

#endif

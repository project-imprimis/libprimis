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
            scalew       = 0;
            scaleh       = 0;
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
            msaatonemapblit = false;
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
        bool msaatonemapblit;

        int scalew,
            scaleh;
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
        matrix4 eyematrix,
                linearworldmatrix;
};

extern GBuffer gbuf;

class PackNode
{
    public:
        ushort w, h;

        PackNode(ushort x, ushort y, ushort w, ushort h) :  w(w), h(h), child1(0), child2(0), x(x), y(y), available(std::min(w, h)) {}

        void reset()
        {
            discardchildren();
            available = std::min(w, h);
        }

        bool resize(int nw, int nh)
        {
            if(w == nw && h == nw)
            {
                return false;
            }
            discardchildren();
            w = nw;
            h = nh;
            available = std::min(w, h);
            return true;
        }

        ~PackNode()
        {
            discardchildren();
        }

        bool insert(ushort &tx, ushort &ty, ushort tw, ushort th);
        void reserve(ushort tx, ushort ty, ushort tw, ushort th);

    private:
        PackNode *child1, *child2;
        ushort x, y;
        int available;

        void discardchildren()
        {
            if(child1)
            {
                delete child1;
                child1 = nullptr;
            }
            if(child2)
            {
                delete child2;
                child2 = nullptr;
            }
        }

        void forceempty()
        {
            discardchildren();
            available = 0;
        }
};

extern PackNode shadowatlaspacker;

struct shadowcacheval;

struct shadowmapinfo
{
    ushort x, y, size, sidemask;
    int light;
    shadowcacheval *cached;
};

extern std::vector<shadowmapinfo> shadowmaps;
extern int smfilter;

extern void addshadowmap(ushort x, ushort y, int size, int &idx, int light = -1, shadowcacheval *cached = nullptr);

constexpr int shadowatlassize = 4096;

const int lighttilemaxwidth  = 16;
const int lighttilemaxheight = 16;

extern int smborder, smborder2;

extern int gdepthstencil, gstencil, glineardepth, msaalineardepth, batchsunlight, smgather, tqaaresolvegather;
extern int lighttilealignw, lighttilealignh, lighttilevieww, lighttileviewh, lighttilew, lighttileh;
extern int spotlights;
extern int volumetriclights;
extern int nospeclights;
extern int debugfullscreen;
extern int msaaedgedetect;
extern int hdrclear;

extern int msaatonemap;
extern int msaatonemapblit;

extern int vieww, viewh;

enum
{
    ShadowMap_None = 0,
    ShadowMap_Reflect,
    ShadowMap_CubeMap,
    ShadowMap_Cascade,
    ShadowMap_Spot,
};

extern int shadowmapping;
extern int smcullside;

extern matrix4 shadowmatrix;

extern void setbilateralshader(int radius, int pass, float depth);
void clearbilateralshaders();
void loadbilateralshaders();

extern void loaddeferredlightshaders();
extern void cleardeferredlightshaders();
extern void clearshadowcache();

extern void cleanupvolumetric();

extern void findshadowvas();
extern void findshadowmms();
extern void renderlights(float bsx1 = -1, float bsy1 = -1, float bsx2 = 1, float bsy2 = 1, const uint *tilemask = nullptr, int stencilmask = 0, int msaapass = 0, bool transparent = false);

extern int calcshadowinfo(const extentity &e, vec &origin, float &radius, vec &spotloc, int &spotangle, float &bias);
extern int dynamicshadowvabounds(int mask, vec &bbmin, vec &bbmax);
extern void rendershadowmapworld();
extern void batchshadowmapmodels(bool skipmesh = false);
extern void rendershadowatlas();
extern void renderrsmgeom(bool dyntex = false);
extern void workinoq();

extern int calcspheresidemask(const vec &p, float radius, float bias);
extern int calcbbrsmsplits(const ivec &bbmin, const ivec &bbmax);
extern int calcspherersmsplits(const vec &center, float radius);

inline bool sphereinsidespot(const vec &dir, int spot, const vec &center, float radius)
{
    const vec2 &sc = sincos360[spot];
    float cdist = dir.dot(center), cradius = radius + sc.y*cdist;
    return sc.x*sc.x*(center.dot(center) - cdist*cdist) <= cradius*cradius;
}
inline bool bbinsidespot(const vec &origin, const vec &dir, int spot, const ivec &bbmin, const ivec &bbmax)
{
    vec radius = vec(ivec(bbmax).sub(bbmin)).mul(0.5f), center = vec(bbmin).add(radius);
    return sphereinsidespot(dir, spot, center.sub(origin), radius.magnitude());
}

extern matrix4 worldmatrix, screenmatrix;

extern int gw, gh, gdepthformat, ghasstencil;
extern int msaasamples, msaalight;
extern std::vector<vec2> msaapositions;

extern bool inoq;
extern int rhinoq;
extern int rsmcull;
extern GLuint rhfbo;

//allows passing nothing to internal uses of gbuffer fxn
//(the parameter is for taking a game function to be rendered onscreen)
inline void dummyfxn()
{
    return;
}

extern bool shouldworkinoq();
extern void initgbuffer();
extern bool usepacknorm();
extern void maskgbuffer(const char *mask);
extern void rendergbuffer(bool depthclear = true, void (*gamefxn)() = dummyfxn);
extern void shadegbuffer();
extern void setuplights();
extern bool debuglights();
extern void cleanuplights();

extern int avatarmask;
extern bool useavatarmask();
extern void enableavatarmask();
extern void disableavatarmask();

template<class T>
inline void calctilebounds(float sx1, float sy1, float sx2, float sy2, T &bx1, T &by1, T &bx2, T &by2)
{
    int tx1 = std::max(static_cast<int>(std::floor(((sx1 + 1)*0.5f*vieww)/lighttilealignw)), 0),
        ty1 = std::max(static_cast<int>(std::floor(((sy1 + 1)*0.5f*viewh)/lighttilealignh)), 0),
        tx2 = std::min(static_cast<int>(std::ceil(((sx2 + 1)*0.5f*vieww)/lighttilealignw)), lighttilevieww),
        ty2 = std::min(static_cast<int>(std::ceil(((sy2 + 1)*0.5f*viewh)/lighttilealignh)), lighttileviewh);
    bx1 = T((tx1 * lighttilew) / lighttilevieww);
    by1 = T((ty1 * lighttileh) / lighttileviewh);
    bx2 = T((tx2 * lighttilew + lighttilevieww - 1) / lighttilevieww);
    by2 = T((ty2 * lighttileh + lighttileviewh - 1) / lighttileviewh);
}

#endif

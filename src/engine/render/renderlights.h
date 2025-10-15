#ifndef RENDERLIGHTS_H_
#define RENDERLIGHTS_H_

const int lighttilemaxwidth  = 16;
const int lighttilemaxheight = 16;

/** @brief Singleton object used to store the graphics buffers
 *
 * Stores the handles for the graphics buffers nd the functions which act upon
 * the g-buffers.
 */
class GBuffer final
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
            stencilformat= 0;
            scalefbo.fill(0);
            scaletex.fill(0);
            gdepthinit   = false;
            hdrfloat     = false;
            msaadepthblit= false;
            msaatonemapblit = false;
            inoq = false;
            transparentlayer = 0;

        }
        static void dummyfxn();
        //main g-buffers
        void cleanupgbuffer();
        void renderao() const;                              //ao.cpp
        void renderradiancehints() const;                   //radiancehints.cpp
        void rendertransparent();                           //renderalpha.cpp
        void resolvemsaadepth(int w, int h) const;
        void setupgbuffer();
        void bindgdepth() const;
        void renderparticles(int layer = 0) const;                //renderparticles.cpp
        void rendervolumetric();
        void renderwaterfog(int mat, float surface);        //water.cpp
        void setaavelocityparams(GLenum tmu = GL_TEXTURE0); //aa.cpp
        void shademodelpreview(int x, int y, int w, int h, bool background = true, bool scissor = false);
        void viewdepth() const;
        void rendershadowatlas();
        //multisample antialiasing specific buffers
        void setupmsbuffer(int w, int h);
        void resolvemsaacolor(int w, int h);
        void shademinimap(const vec &color = vec(-1, -1, -1));
        void shadesky() const;
        //refractive
        void processhdr(GLuint outfbo, int aa);
        void viewrefract();
        void doscale(GLuint outfbo = 0) const;
        void setupscale(int sw, int sh, int w, int h);
        GLuint shouldscale() const;
        void workinoq();

        /**
         * @brief Creates the geometry buffer for the scene
         *
         * Renders and copies a fbo (framebuffer object) to msfbo (multisample framebuffer object)
         * or gfbo (geometry buffer framebuffer object) depending on whether msaa is enabled
         *
         * @param bool depthclear toggles clearing the depth buffer
         * @param gamefxn pointer to a function for game-specific rendering
         */
        void rendergbuffer(bool depthclear = true, void (*gamefxn)() = dummyfxn);
        bool istransparentlayer() const;
        void rendermodelbatches();
        void renderlights(float bsx1 = -1, float bsy1 = -1, float bsx2 = 1, float bsy2 = 1, const uint *tilemask = nullptr, int stencilmask = 0, int msaapass = 0, bool transparent = false);

        /**
         * @brief Returns debug information about lighting.
         *
         * Types:
         * 0: light passes used
         * 1: lights visible
         * 2: lights occluded
         * 3: light batches used
         * 4: light batch rects used
         * 5: light batch stacks used
         *
         * @param type of value to query
         *
         * @return numeric value of appropriate type
         */
        int getlightdebuginfo(uint type) const;
    private:
        void bindmsdepth() const;
        void bindlighttexs(int msaapass, bool transparent) const; //only used in renderlights
        void cleanupscale();
        void cleanupmsbuffer();
        void preparegbuffer(bool depthclear = true);
        void rendercsmshadowmaps() const;
        void rendershadowmaps(int offset = 0) const;
        void rendersunpass(Shader *s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask);
        void renderlightsnobatch(Shader *s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2);
        void renderlightbatches(Shader &s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask);
        void rendergeom();

        void alphaparticles(float allsx1, float allsy1, float allsx2, float allsy2) const;

        void rendermaterialmask() const;
        void renderliquidmaterials() const;
        void packlights();

        struct MaterialInfo final
        {
            float matliquidsx1,
                  matliquidsy1,
                  matliquidsx2,
                  matliquidsy2;
            float matsolidsx1,
                  matsolidsy1,
                  matsolidsx2,
                  matsolidsy2;
            float matrefractsx1,
                  matrefractsy1,
                  matrefractsx2,
                  matrefractsy2;
            uint matliquidtiles[lighttilemaxheight],
                 matsolidtiles[lighttilemaxheight];
            int hasmats;
        };

        struct AlphaInfo final
        {
            float alphafrontsx1, alphafrontsx2,
                  alphafrontsy1, alphafrontsy2,
                  alphabacksx1, alphabacksx2,
                  alphabacksy1, alphabacksy2,
                  alpharefractsx1, alpharefractsx2,
                  alpharefractsy1, alpharefractsy2;
            int hasalphavas;
        };

        MaterialInfo findmaterials() const; //materials.cpp
        AlphaInfo findalphavas();

        struct TransparentModelInfo final
        {
            float mdlsx1, mdlsy1, mdlsx2, mdlsy2;
            std::array<uint, lighttilemaxheight> mdltiles;

            TransparentModelInfo() : mdlsx1(-1), mdlsy1(-1), mdlsx2(1), mdlsy2(1), mdltiles()
            {
            }
        };
        TransparentModelInfo tmodelinfo;

        uint alphatiles[lighttilemaxheight];

        bool transparentlayer;
        bool inoq = false;
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
        std::array<GLuint, 2> scalefbo,
                              scaletex;
        GLenum stencilformat;
        matrix4 eyematrix,
                linearworldmatrix;

    int lightpassesused,
        lightsvisible,
        lightsoccluded,
        lightbatchesused,
        lightbatchrectsused,
        lightbatchstacksused;

    static float refractmargin,
                 refractdepth;
};

extern GBuffer gbuf;

class PackNode final
{
    public:
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

        int availablespace() const
        {
            return available;
        }

        vec2 dimensions() const
        {
            return {static_cast<float>(w), static_cast<float>(h)};
        }

        //debugging printouts, not used in program logic

        //i: recursion depth
        void printchildren(int i = 0) const
        {
            print(i);

            if(child1)
            {
                child1->printchildren(i+1);
            }
            if(child2)
            {
                child2->printchildren(i+1);
            }
        }

        //i: depth to print out
        void print(int i) const
        {
            std::printf("%d: %d %d\n", i, w, h);
        }

    private:
        ushort w, h;
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

struct ShadowMapInfo final
{
    ushort x, y, size, sidemask;
    int light;
    const shadowcacheval *cached;
};

extern std::vector<ShadowMapInfo> shadowmaps;
extern int smfilter;

extern void addshadowmap(ushort x, ushort y, int size, int &idx, int light = -1, const shadowcacheval *cached = nullptr);

constexpr int shadowatlassize = 4096;

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
extern float ldrscale;
extern float ldrscaleb(); //derived from ldrscale

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

/**
 * @brief sets values for one of the bilateralshader[] elements
 *
 * bilateralshader[2] elements' referenced Shader objects have their radius
 * and depth parameters changed
 *
 * @param radius the bilateral filter radius to set
 * @param pass [0-1] the element of the bilateralshader() array to change
 * @param depth the depth of the bilateral filtering to set
 */
extern void setbilateralshader(int radius, int pass, float depth);

/**
 * @brief clears bilateralarray
 *
 * Makes bilateralshader[2]'s elements point to the null pointer..
 */
void clearbilateralshaders();

/**
 * @brief sets bilateralshader array using bilateralshader()
 *
 * Sets bilateralshader[2] elements to point to Shader objects representing the two passes
 */
void loadbilateralshaders();

extern void loaddeferredlightshaders();
extern void cleardeferredlightshaders();
extern void clearshadowcache();

extern void cleanupvolumetric();

extern void findshadowvas();
extern void findshadowmms();

extern int calcshadowinfo(const extentity &e, vec &origin, float &radius, vec &spotloc, int &spotangle, float &bias);
extern void rendershadowmapworld();
extern void batchshadowmapmodels(bool skipmesh = false);
extern void renderrsmgeom(bool dyntex = false);

extern int calcspheresidemask(const vec &p, float radius, float bias);
extern int calcbbrsmsplits(const ivec &bbmin, const ivec &bbmax);
extern int calcspherersmsplits(const vec &center, float radius);

bool sphereinsidespot(const vec &dir, int spot, const vec &center, float radius);
bool bbinsidespot(const vec &origin, const vec &dir, int spot, const ivec &bbmin, const ivec &bbmax);

extern matrix4 worldmatrix, screenmatrix;

extern int gw, gh, gdepthformat, ghasstencil;
extern int msaasamples, msaalight;
extern std::vector<vec2> msaapositions;

extern int rhinoq;

extern bool shouldworkinoq();
extern void initgbuffer();
extern bool usepacknorm();
extern void maskgbuffer(const char *mask);
extern void shadegbuffer();
extern void setuplights(GBuffer &buf);
extern bool debuglights();
extern void cleanuplights();

extern int avatarmask;
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

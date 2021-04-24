#ifndef ENGINE_H_
#define ENGINE_H_

#include "../libprimis-headers/cube.h"

#include "../libprimis-headers/texture.h"

// texture
extern int hwtexsize, hwcubetexsize, hwmaxaniso, maxtexsize, hwtexunits, hwvtexunits;

extern Texture *textureload(const char *name, int clamp = 0, bool mipit = true, bool msg = true);
extern bool floatformat(GLenum format);
extern uchar *loadalphamask(Texture *t);
extern void loadshaders();
extern void createtexture(int tnum, int w, int h, const void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_2D, int pw = 0, int ph = 0, int pitch = 0, bool resize = true, GLenum format = GL_FALSE, bool swizzle = false);
extern void create3dtexture(int tnum, int w, int h, int d, const void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_3D, bool swizzle = false);
extern GLuint setuppostfx(int w, int h, GLuint outfbo = 0);
extern void cleanuppostfx(bool fullclean = false);
extern void renderpostfx(GLuint outfbo = 0);
extern bool reloadtexture(Texture &tex);
extern bool reloadtexture(const char *name);
extern void setuptexcompress();
extern void clearslots();
extern void compacteditvslots();
extern void compactmruvslots();
extern void compactvslots(cube *c, int n = 8);
extern void compactvslot(int &index);
extern void compactvslot(VSlot &vs);
extern int compactvslots(bool cull = false);
extern void reloadtextures();
extern void cleanuptextures();
extern bool settexture(const char *name, int clamp = 0);

// octaworld
extern cube *newcubes(uint face = faceempty, int mat = Mat_Air);
extern cubeext *growcubeext(cubeext *ext, int maxverts);
extern void setcubeext(cube &c, cubeext *ext);
extern cubeext *newcubeext(cube &c, int maxverts = 0, bool init = true);
extern void getcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern void setcubevector(cube &c, int d, int x, int y, int z, const ivec &p);
extern int familysize(const cube &c);
extern void freeocta(cube *c);
extern void discardchildren(cube &c, bool fixtex = false, int depth = 0);
extern void validatec(cube *c, int size = 0);
extern ivec lu;
extern int lusize;
extern cube &lookupcube(const ivec &to, int tsize = 0, ivec &ro = lu, int &rsize = lusize);
extern const cube *neighborstack[32];
extern int neighbordepth;
extern int getmippedtexture(const cube &p, int orient);
extern void forcemip(cube &c, bool fixtex = true);
extern bool subdividecube(cube &c, bool fullcheck=true, bool brighten=true);
extern int faceconvexity(const ivec v[4]);
extern int faceconvexity(const ivec v[4], int &vis);
extern int faceconvexity(const vertinfo *verts, int numverts, int size);
extern int faceconvexity(const cube &c, int orient);
extern uint faceedges(const cube &c, int orient);
extern bool touchingface(const cube &c, int orient);
extern bool flataxisface(const cube &c, int orient);
extern bool collideface(const cube &c, int orient);
extern void genclipbounds(const cube &c, const ivec &co, int size, clipplanes &p);
extern void genclipplanes(const cube &c, const ivec &co, int size, clipplanes &p, bool collide = true, bool noclip = false);
extern bool visibleface(const cube &c, int orient, const ivec &co, int size, ushort mat = Mat_Air, ushort nmat = Mat_Air, ushort matmask = MatFlag_Volume);
extern int classifyface(const cube &c, int orient, const ivec &co, int size);
extern int visibletris(const cube &c, int orient, const ivec &co, int size, ushort vmat = Mat_Air, ushort nmat = Mat_Alpha, ushort matmask = Mat_Alpha);
extern int visibleorient(const cube &c, int orient);
extern void genfaceverts(const cube &c, int orient, ivec v[4]);
extern int calcmergedsize(int orient, const ivec &co, int size, const vertinfo *verts, int numverts);
extern void invalidatemerges(cube &c);
extern void remip();
extern int lookupmaterial(const vec &o);

inline cubeext &ext(cube &c)
{
    return *(c.ext ? c.ext : newcubeext(c));
}

// renderlights

struct PackNode
{
    PackNode *child1, *child2;
    ushort x, y, w, h;
    int available;

    PackNode(ushort x, ushort y, ushort w, ushort h) : child1(0), child2(0), x(x), y(y), w(w), h(h), available(std::min(w, h)) {}

    void discardchildren()
    {
        DELETEP(child1);
        DELETEP(child2);
    }

    void forceempty()
    {
        discardchildren();
        available = 0;
    }

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
extern matrix4 eyematrix;
extern GLuint mshdrtex, mshdrfbo, msrefractfbo;
extern int msaaedgedetect;
extern GLuint refractfbo, refracttex;
extern int hdrclear;

extern int msaatonemap;
extern int msaatonemapblit;

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

extern vec shadoworigin, shadowdir;
extern float shadowradius, shadowbias;
extern int shadowside, shadowspot;
extern matrix4 shadowmatrix, linearworldmatrix;
extern GLuint msrefracttex;

extern void setbilateralshader(int radius, int pass, float depth);
void clearbilateralshaders();
void loadbilateralshaders();

extern void loaddeferredlightshaders();
extern void cleardeferredlightshaders();
extern void clearshadowcache();

extern void rendervolumetric();
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
extern int calctrisidemask(const vec &p1, const vec &p2, const vec &p3, float bias);
extern int cullfrustumsides(const vec &lightpos, float lightradius, float size, float border);
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

extern int transparentlayer;

extern int gw, gh, gdepthformat, ghasstencil;
extern GLuint gdepthtex, gcolortex, gnormaltex, gglowtex, gdepthrb, gstencilrb;
extern int msaasamples, msaalight;
extern GLuint msdepthtex, mscolortex, msnormaltex, msglowtex, msdepthrb, msstencilrb;
extern std::vector<vec2> msaapositions;

extern bool inoq;
extern int rhinoq;
extern int rsmcull;
extern GLuint gfbo, msfbo, rhfbo;

//allows passing nothing to internal uses of gbuffer fxn
//(the parameter is for taking a game function to be rendered onscreen)
inline void dummyfxn()
{
    return;
}

extern void resolvemsaacolor(int w, int h);
extern bool shouldworkinoq();
extern void cleanupgbuffer();
extern void initgbuffer();
extern bool usepacknorm();
extern void maskgbuffer(const char *mask);
extern void bindgdepth();
extern void preparegbuffer(bool depthclear = true);
extern void rendergbuffer(bool depthclear = true, void (*gamefxn)() = dummyfxn);
extern void shadegbuffer();
extern void shademinimap(const vec &color = vec(-1, -1, -1));
extern void shademodelpreview(int x, int y, int w, int h, bool background = true, bool scissor = false);
extern void rendertransparent();
extern void renderao();
extern void setuplights();
extern void setupgbuffer();
extern GLuint shouldscale();
extern void doscale(GLuint outfbo = 0);
extern bool debuglights();
extern void cleanuplights();

extern int avatarmask;
extern bool useavatarmask();
extern void enableavatarmask();
extern void disableavatarmask();

// renderva

extern int outline;
extern int oqfrags;
extern int outline;

extern float alphafrontsx1, alphafrontsx2, alphafrontsy1, alphafrontsy2, alphabacksx1, alphabacksx2, alphabacksy1, alphabacksy2, alpharefractsx1, alpharefractsx2, alpharefractsy1, alpharefractsy2;
extern uint alphatiles[lighttilemaxheight];
extern vtxarray *visibleva;
extern int octaentsize;

extern void visiblecubes(bool cull = true);
extern void setvfcP(const vec &bbmin = vec(-1, -1, -1), const vec &bbmax = vec(1, 1, 1));
extern void rendergeom();
extern int findalphavas();
extern void renderrefractmask();
extern void renderalphageom(int side);
extern void rendermapmodels();
extern void renderoutline();
extern bool renderexplicitsky(bool outline = false);
extern void cleanupva();
extern bvec outlinecolor;

extern bool isfoggedsphere(float rad, const vec &cv);
extern int isvisiblesphere(float rad, const vec &cv);
extern int isvisiblebb(const ivec &bo, const ivec &br);
extern bool bboccluded(const ivec &bo, const ivec &br);

extern int deferquery;
extern void flipqueries();
extern occludequery *newquery(void *owner);
extern void startquery(occludequery *query);
extern void endquery();
extern bool checkquery(occludequery *query, bool nowait = false);
extern void resetqueries();
extern int getnumqueries();
extern void startbb(bool mask = true);
extern void endbb(bool mask = true);
extern void drawbb(const ivec &bo, const ivec &br);

extern void renderdecals();

struct shadowmesh;
extern void clearshadowmeshes();
extern void genshadowmeshes();
extern shadowmesh *findshadowmesh(int idx, extentity &e);
extern void rendershadowmesh(shadowmesh *m);

// command

extern void setvarchecked(ident *id, int val);
extern void setfvarchecked(ident *id, float val);
extern void setsvarchecked(ident *id, const char *val);

extern const char *escapeid(const char *s);
inline const char *escapeid(ident &id) { return escapeid(id.name); }

extern int variable(const char *name, int min, int cur, int max, int *storage, identfun fun, int flags);
extern float fvariable(const char *name, float min, float cur, float max, float *storage, identfun fun, int flags);
extern char *svariable(const char *name, const char *cur, char **storage, identfun fun, int flags);
extern void setvar(const char *name, int i, bool dofunc = true, bool doclamp = true);
extern void setfvar(const char *name, float f, bool dofunc = true, bool doclamp = true);
extern void setsvar(const char *name, const char *str, bool dofunc = true);
extern void touchvar(const char *name);
extern int getvar(const char *name);
extern int getvarmin(const char *name);
extern int getvarmax(const char *name);
extern bool identexists(const char *name);
extern ident *getident(const char *name);
extern ident *newident(const char *name, int flags = 0);
extern ident *readident(const char *name);
extern ident *writeident(const char *name, int flags = 0);
extern bool addcommand(const char *name, identfun fun, const char *narg, int type = Id_Command);
extern void keepcode(uint *p);
extern void freecode(uint *p);
extern void executeret(const uint *code, tagval &result = *commandret);
extern void executeret(const char *p, tagval &result = *commandret);
extern void executeret(ident *id, tagval *args, int numargs, bool lookup = false, tagval &result = *commandret);
extern char *executestr(const uint *code);
extern char *executestr(const char *p);
extern char *executestr(ident *id, tagval *args, int numargs, bool lookup = false);
extern char *execidentstr(const char *name, bool lookup = false);
extern int execute(const uint *code);
extern int execute(const char *p);
extern int execute(ident *id, tagval *args, int numargs, bool lookup = false);
extern int execident(const char *name, int noid = 0, bool lookup = false);
extern float executefloat(const uint *code);
extern float executefloat(const char *p);
extern float executefloat(ident *id, tagval *args, int numargs, bool lookup = false);
extern float execidentfloat(const char *name, float noid = 0, bool lookup = false);
extern bool executebool(const uint *code);
extern bool executebool(const char *p);
extern bool executebool(ident *id, tagval *args, int numargs, bool lookup = false);
extern bool execidentbool(const char *name, bool noid = false, bool lookup = false);
extern bool execfile(const char *cfgfile, bool msg = true);
extern void alias(const char *name, const char *action);
extern void alias(const char *name, tagval &v);
extern const char *getalias(const char *name);
extern const char *escapestring(const char *s);
extern bool validateblock(const char *s);
extern void explodelist(const char *s, vector<char *> &elems, int limit = -1);
extern int listlen(const char *s);
extern void printvar(ident *id);
extern void printvar(ident *id, int i);
extern void printfvar(ident *id, float f);
extern void printsvar(ident *id, const char *s);
extern int clampvar(ident *id, int i, int minval, int maxval);
extern float clampfvar(ident *id, float f, float minval, float maxval);
extern void loopiter(ident *id, identstack &stack, const tagval &v);
extern void loopiter(ident *id, identstack &stack, int i);
extern void loopend(ident *id, identstack &stack);
extern uint *compilecode(const char *p);

extern hashnameset<ident> idents;
extern int identflags;

extern void clearoverrides();

extern void checksleep(int millis);
extern void clearsleep(bool clearoverrides = true);

// world

extern int worldscale, worldsize;
extern int mapversion;
extern char *maptitle;

extern vector<int> entgroup;
extern vector<int> outsideents;

extern void resetmap();
extern void freeoctaentities(cube &c);
extern void entitiesinoctanodes();
extern void entcancel();
extern void entselectionbox(const entity &e, vec &eo, vec &es);

namespace entities
{
    extern extentity *newentity();
    extern void deleteentity(extentity *e);
    extern vector<extentity *> &getents();
}
#endif

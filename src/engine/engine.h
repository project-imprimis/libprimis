#ifndef ENGINE_H_
#define ENGINE_H_

#include "../libprimis-headers/cube.h"

#include "render/texture.h"
#include "world/bih.h"
#include "model/model.h"

extern dynent *player;
extern physent *camera1;                // special ent that acts as camera, same object as player1 in FPS mode

extern int curtime;                     // current frame time
extern int lastmillis;                  // last time
extern int elapsedtime;                 // elapsed frame time
extern int totalmillis;                 // total elapsed time
extern uint totalsecs;
extern int gamespeed, paused;

extern int worldscale, worldsize;
extern int mapversion;
extern char *maptitle;
extern vector<ushort> texmru;
extern int xtraverts, xtravertsva;
extern const ivec cubecoords[8];
extern const ivec facecoords[6][4];
extern const uchar fv[6][4];
extern const uchar fvmasks[64];
extern const uchar faceedgesidx[6][4];
extern bool inbetweenframes, renderedframe;

extern SDL_Window *screen;
extern int screenw, screenh, renderw, renderh, hudw, hudh;

extern vector<int> entgroup;

// rendertext
struct font
{
    struct charinfo
    {
        float x, y, w, h, offsetx, offsety, advance;
        int tex;
    };

    char *name;
    vector<Texture *> texs;
    vector<charinfo> chars;
    int charoffset, defaultw, defaulth, scale;
    float bordermin, bordermax, outlinemin, outlinemax;

    font() : name(NULL) {}
    ~font() { DELETEA(name); }
};

#define FONTH (curfont->scale)
#define FONTW (FONTH/2)
const int minreswidth = 640;
const int minresheight = 480;

extern font *curfont;
extern Shader *textshader;
extern const matrix4x3 *textmatrix;
extern float textscale;

extern font *findfont(const char *name);
extern void reloadfonts();

inline void setfont(font *f) { if(f) curfont = f; }

extern bool setfont(const char *name);
extern void pushfont();
extern bool popfont();
extern void gettextres(int &w, int &h);

extern void draw_text(const char *str, float left, float top, int r = 255, int g = 255, int b = 255, int a = 255, int cursor = -1, int maxwidth = -1);
extern void draw_textf(const char *fstr, float left, float top, ...) PRINTFARGS(1, 4);
extern float text_widthf(const char *str);
extern void text_boundsf(const char *str, float &width, float &height, int maxwidth = -1);
extern int text_visible(const char *str, float hitx, float hity, int maxwidth);
extern void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth);

inline void text_bounds(const char *str, int &width, int &height, int maxwidth = -1)
{
    float widthf, heightf;
    text_boundsf(str, widthf, heightf, maxwidth);
    width = static_cast<int>(ceil(widthf));
    height = static_cast<int>(ceil(heightf));
}

inline void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth)
{
    float cxf, cyf;
    text_posf(str, cursor, cxf, cyf, maxwidth);
    cx = static_cast<int>(cxf);
    cy = static_cast<int>(cyf);
}

inline int text_width(const char *str)
{
    return static_cast<int>(ceil(text_widthf(str)));
}

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

// rendergl
extern vec worldpos, camdir, camright, camup;
extern bool hasS3TC, hasFXT1, hasLATC, hasFBMSBS, hasTQ, hasDBT, hasDBGO, hasES3, hasCI;
extern int glversion, glslversion, glcompat;
extern int maxdrawbufs, maxdualdrawbufs;
extern vec minimapcenter, minimapradius, minimapscale;

enum
{
    Draw_TexNone = 0, //unused
    Draw_TexMinimap,
    Draw_TexModelPreview,
};

extern int vieww, viewh;
extern int fov;
extern float curfov, fovy, aspect, forceaspect;
extern float nearplane;
extern int farplane;
extern bool hdrfloat;
extern float ldrscale, ldrscaleb;
extern int drawtex;
extern const matrix4 viewmatrix, invviewmatrix;
extern matrix4 cammatrix, projmatrix, camprojmatrix, invcammatrix, invcamprojmatrix, invprojmatrix;
extern int fog;
extern bvec fogcolor;
extern vec curfogcolor;
extern int wireframe;

extern int glerr;
extern void glerror(const char *file, int line, GLenum error);

inline void glerror()
{
    if(glerr)
    {
        GLenum error = glGetError();
        if(error != GL_NO_ERROR)
        {
            glerror(__FILE__, __LINE__, error);
        }
    }
}

extern void gl_checkextensions();
extern void gl_init();
extern void gl_resize();
extern void gl_setupframe(bool force = false);
extern void gl_drawframe();
extern void cleanupgl();
extern void drawminimap();
extern void enablepolygonoffset(GLenum type);
extern void disablepolygonoffset(GLenum type);
extern bool calcspherescissor(const vec &center, float size, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2);
extern bool calcbbscissor(const ivec &bbmin, const ivec &bbmax, float &sx1, float &sy1, float &sx2, float &sy2);
extern bool calcspotscissor(const vec &origin, float radius, const vec &dir, int spot, const vec &spotx, const vec &spoty, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2);
extern void screenquad();
extern void screenquad(float sw, float sh);
extern void screenquad(float sw, float sh, float sw2, float sh2);
extern void screenquadoffset(float x, float y, float w, float h);
extern void hudquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1);
extern void debugquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1);
extern float calcfrustumboundsphere(float nearplane, float farplane,  const vec &pos, const vec &view, vec &center);
extern void zerofogcolor();
extern void resetfogcolor();
extern float calcfogdensity(float dist);
extern float calcfogcull();
extern void writecrosshairs(stream *f);
extern vec calcmodelpreviewpos(const vec &radius, float &yaw);

extern matrix4 hudmatrix;
extern void resethudmatrix();
extern void pushhudmatrix();
extern void flushhudmatrix(bool flushparams = true);
extern void pophudmatrix(bool flush = true, bool flushparams = true);
extern void pushhudscale(float sx, float sy = 0);
extern void pushhudtranslate(float tx, float ty, float sx = 0, float sy = 0);
extern void resethudshader();

namespace modelpreview
{
    extern void start(int x, int y, int w, int h, bool background = true, bool scissor = false);
    extern void end();
}

struct timer;
extern timer *begintimer(const char *name, bool gpu = true);
extern void endtimer(timer *t);

// octa
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

const int lighttilemaxwidth  = 16;
const int lighttilemaxheight = 16;

extern int lighttilealignw, lighttilealignh, lighttilevieww, lighttileviewh, lighttilew, lighttileh;
extern int spotlights;
extern int volumetriclights;
extern int nospeclights;
extern int debugfullscreen;

template<class T>
inline void calctilebounds(float sx1, float sy1, float sx2, float sy2, T &bx1, T &by1, T &bx2, T &by2)
{
    int tx1 = max(static_cast<int>(floor(((sx1 + 1)*0.5f*vieww)/lighttilealignw)), 0),
        ty1 = max(static_cast<int>(floor(((sy1 + 1)*0.5f*viewh)/lighttilealignh)), 0),
        tx2 = min(static_cast<int>(ceil(((sx2 + 1)*0.5f*vieww)/lighttilealignw)), lighttilevieww),
        ty2 = min(static_cast<int>(ceil(((sy2 + 1)*0.5f*viewh)/lighttilealignh)), lighttileviewh);
    bx1 = T((tx1 * lighttilew) / lighttilevieww);
    by1 = T((ty1 * lighttileh) / lighttileviewh);
    bx2 = T((tx2 * lighttilew + lighttilevieww - 1) / lighttilevieww);
    by2 = T((ty2 * lighttileh + lighttileviewh - 1) / lighttileviewh);
}

inline void masktiles(uint *tiles, float sx1, float sy1, float sx2, float sy2)
{
    int tx1, ty1, tx2, ty2;
    calctilebounds(sx1, sy1, sx2, sy2, tx1, ty1, tx2, ty2);
    for(int ty = ty1; ty < ty2; ty++) tiles[ty] |= ((1<<(tx2-tx1))-1)<<tx1;
}

enum
{
    ShadowMap_None = 0,
    ShadowMap_Reflect,
    ShadowMap_CubeMap,
    ShadowMap_Cascade,
    ShadowMap_Spot,
};

extern int shadowmapping;

extern vec shadoworigin, shadowdir;
extern float shadowradius, shadowbias;
extern int shadowside, shadowspot;
extern matrix4 shadowmatrix;

extern void loaddeferredlightshaders();
extern void cleardeferredlightshaders();
extern void clearshadowcache();

extern void rendervolumetric();
extern void cleanupvolumetric();

extern void findshadowvas();
extern void findshadowmms();

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
extern int calcbbcsmsplits(const ivec &bbmin, const ivec &bbmax);
extern int calcspherecsmsplits(const vec &center, float radius);
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
extern int csmshadowmap, rhinoq;
extern int rsmcull;
extern GLuint gfbo, msfbo, rhfbo;

enum
{
    AA_Unused = 0,
    AA_Luma,
    AA_Masked,
    AA_Split,
    AA_SplitLuma,
    AA_SplitMasked,
};

//allows passing nothing to internal uses of gbuffer fxn
//(the parameter is for taking a game function to be rendered onscreen)
inline void dummyfxn()
{
    return;
}

extern bool shouldworkinoq();
extern int  gethdrformat(int prec, int fallback = GL_RGB);
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
extern void loadhdrshaders(int aa = AA_Unused);
extern void processhdr(GLuint outfbo = 0, int aa = AA_Unused);
extern void copyhdr(int sw, int sh, GLuint fbo, int dw = 0, int dh = 0, bool flipx = false, bool flipy = false, bool swapxy = false);
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

// aa
extern matrix4 nojittermatrix;

extern void setupaa(int w, int h);
extern void jitteraa();
extern bool multisampledaa();
extern void setaavelocityparams(GLenum tmu = GL_TEXTURE0);
extern void setaamask(bool val);
extern void enableaamask(int stencil = 0);
extern void disableaamask();
extern void doaa(GLuint outfbo, void (*resolve)(GLuint, int));
extern bool debugaa();
extern void cleanupaa();

// octaedit
extern bool allowediting;
extern bool multiplayer;
extern bool editmode;
extern selinfo sel;

extern void multiplayerwarn();
extern void cancelsel();
extern void rendertexturepanel(int w, int h);
extern void commitchanges(bool force = false);
extern void changed(const ivec &bbmin, const ivec &bbmax, bool commit = true);
extern void rendereditcursor();
extern bool noedit(bool view = false, bool msg = true);

extern void previewprefab(const char *name, const vec &color);
extern void cleanupprefabs();

struct editinfo;
extern editinfo *localedit;
extern void pruneundos(int maxremain = 0);
extern bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf);

// octarender
extern ivec worldmin, worldmax;
extern vector<tjoint> tjoints;
extern vector<vtxarray *> varoot, valist;
extern int filltjoints;

extern ushort encodenormal(const vec &n);
extern void guessnormals(const vec *pos, int numverts, vec *normals);
extern void reduceslope(ivec &n);
extern void findtjoints();
extern void octarender();
extern void allchanged(bool load = false);
extern void clearvas(cube *c);
extern void destroyva(vtxarray *va, bool reparent = true);
extern void updatevabb(vtxarray *va, bool force = false);
extern void updatevabbs(bool force = false);

// renderva

extern int oqfrags;
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
extern void cleanupva();

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

// dynlight

extern void updatedynlights();
extern int finddynlights();
extern bool getdynlight(int n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags);

// material

extern float matliquidsx1, matliquidsy1, matliquidsx2, matliquidsy2;
extern float matsolidsx1, matsolidsy1, matsolidsx2, matsolidsy2;
extern float matrefractsx1, matrefractsy1, matrefractsx2, matrefractsy2;
extern uint matliquidtiles[lighttilemaxheight], matsolidtiles[lighttilemaxheight];
extern vector<materialsurface> editsurfs, glasssurfs[4], watersurfs[4], waterfallsurfs[4];
extern const vec matnormals[6];

extern int showmat;

extern int findmaterial(const char *name);
extern const char *findmaterialname(int mat);
extern const char *getmaterialdesc(int mat, const char *prefix = "");
extern void genmatsurfs(const cube &c, const ivec &co, int size, vector<materialsurface> &matsurfs);
extern void calcmatbb(vtxarray *va, const ivec &co, int size, vector<materialsurface> &matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);
extern void setupmaterials(int start = 0, int len = 0);
extern int findmaterials();
extern void rendermaterialmask();
extern void renderliquidmaterials();
extern void rendersolidmaterials();
extern void rendereditmaterials();
extern void renderminimapmaterials();

// water
extern int vertwater, waterreflect, caustics;
extern float watersx1, watersy1, watersx2, watersy2;

#define GETMATIDXVAR(name, var, type) \
    type get##name##var(int mat) \
    { \
        switch(mat&MatFlag_Index) \
        { \
            default: case 0: return name##var; \
            case 1: return name##2##var; \
            case 2: return name##3##var; \
            case 3: return name##4##var; \
        } \
    }

extern const bvec &getwatercolor(int mat);
extern const bvec &getwaterdeepcolor(int mat);
extern const bvec &getwaterdeepfade(int mat);
extern const bvec &getwaterrefractcolor(int mat);
extern const bvec &getwaterfallcolor(int mat);
extern const bvec &getwaterfallrefractcolor(int mat);
extern int getwaterfog(int mat);
extern int getwaterdeep(int mat);
extern int getwaterspec(int mat);
extern float getwaterrefract(int mat);
extern int getwaterfallspec(int mat);
extern float getwaterfallrefract(int mat);

extern const bvec &getglasscolor(int mat);
extern float getglassrefract(int mat);
extern int getglassspec(int mat);

extern void renderwater();
extern void renderwaterfalls();
extern void loadcaustics(bool force = false);
extern void renderwaterfog(int mat, float blend);
extern void preloadwatershaders(bool force = false);

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

// console
extern float conscale;

extern void processkey(int code, bool isdown);
extern void processtextinput(const char *str, int len);
extern float rendercommand(float x, float y, float w);
extern float renderfullconsole(float w, float h);
extern float renderconsole(float w, float h, float abovehud);
extern void conoutf(const char *s, ...) PRINTFARGS(1, 2);
extern void conoutf(int type, const char *s, ...) PRINTFARGS(2, 3);
extern void conoutfv(int type, const char *fmt, va_list args);
extern void logoutf(const char *fmt, ...) PRINTFARGS(1, 2);
extern void logoutfv(const char *fmt, va_list args);
const char *getkeyname(int code);
extern const char *addreleaseaction(char *s);
extern tagval *addreleaseaction(ident *id, int numargs);
extern void writebinds(stream *f);
extern void writecompletions(stream *f);
extern FILE *getlogfile();

// control
extern void fatal(const char *s, ...) PRINTFARGS(1, 2);

extern int initing;

enum
{
    Change_Graphics   = 1<<0,
    Change_Sound      = 1<<1,
    Change_Shaders    = 1<<2,
};
extern bool initwarning(const char *desc, int level = Init_Reset, int type = Change_Graphics);

extern int scr_w, scr_h;

extern float loadprogress;

extern void getfps(int &fps, int &bestdiff, int &worstdiff);
extern int getclockmillis();

// worldio
extern uint getmapcrc();
extern void clearmapcrc();

// physics
extern vec collidewall;
extern int collideinside;
extern physent *collideplayer;

extern void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space);
extern bool movecamera(physent *pl, const vec &dir, float dist, float stepdist);
extern void dropenttofloor(entity *e);
extern bool droptofloor(vec &o, float radius, float height);

extern void resetclipplanes();
extern clipplanes &getclipbounds(const cube &c, const ivec &o, int size, int offset);
extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f, bool playercol = true, bool insideplayercol = false);
extern void modifyorient(float yaw, float pitch);
extern void mousemove(int dx, int dy);
extern void rotatebb(vec &center, vec &radius, int yaw, int pitch, int roll = 0);

extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m);
extern void vectoyawpitch(const vec &v, float &yaw, float &pitch);
extern void updatephysstate(physent *d);
extern void cleardynentcache();
extern void updatedynentcache(physent *d);
extern bool entinmap(dynent *d, bool avoidplayers = false);
extern void findplayerspawn(dynent *d, int forceent = -1, int tag = 0);

extern const float gravity;

// world

extern vector<int> outsideents;

extern void resetmap();
extern void freeoctaentities(cube &c);
extern void entitiesinoctanodes();
extern void entcancel();

namespace entities
{
    extern extentity *newentity();
    extern void deleteentity(extentity *e);
    extern vector<extentity *> &getents();
}

// rendermodel
struct mapmodelinfo { string name; model *m, *collide; };

extern vector<mapmodelinfo> mapmodels;

extern float transmdlsx1, transmdlsy1, transmdlsx2, transmdlsy2;
extern uint transmdltiles[lighttilemaxheight];

extern void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks);
extern model *loadmodel(const char *name, int i = -1, bool msg = false);
extern void resetmodelbatches();
extern void startmodelquery(occludequery *query);
extern void endmodelquery();
extern void rendershadowmodelbatches(bool dynmodel = true);
extern void shadowmaskbatchedmodels(bool dynshadow = true);
extern void rendermapmodelbatches();
extern void rendermodelbatches();
extern void rendertransparentmodelbatches(int stencil = 0);
extern void rendermodel(const char *mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int cull = Model_CullVFC | Model_CullDist | Model_CullOccluded, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float size = 1, const vec4 &color = vec4(1, 1, 1, 1));
extern void rendermapmodel(int idx, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int flags = Model_CullVFC | Model_CullDist, int basetime = 0, float size = 1);
extern void clearbatchedmapmodels();
extern int batcheddynamicmodels();
extern int batcheddynamicmodelbounds(int mask, vec &bbmin, vec &bbmax);
extern void cleanupmodels();
extern model *loadmapmodel(int n);
extern std::vector<int> findanims(const char *pattern);

inline mapmodelinfo *getmminfo(int n) { return mapmodels.inrange(n) ? &mapmodels[n] : NULL; }

// renderparticles
extern int particlelayers;

enum
{
    ParticleLayer_All = 0,
    ParticleLayer_Under,
    ParticleLayer_Over,
    ParticleLayer_NoLayer,
};

extern void clearparticles();
extern void clearparticleemitters();
extern void seedparticles();
extern void debugparticles();
extern void renderparticles(int layer = ParticleLayer_All);
extern void cleanupparticles();

extern void regular_particle_splash(int type, int num, int fade, const vec &p, int color = 0xFFFFFF, float size = 1.0f, int radius = 150, int gravity = 2, int delay = 0);
extern void regular_particle_flame(int type, const vec &p, float radius, float height, int color, int density = 3, float scale = 2.0f, float speed = 200.0f, float fade = 600.0f, int gravity = -15);
extern void particle_splash(int type, int num, int fade, const vec &p, int color = 0xFFFFFF, float size = 1.0f, int radius = 150, int gravity = 2);
extern void particle_trail(int type, int fade, const vec &from, const vec &to, int color = 0xFFFFFF, float size = 1.0f, int gravity = 20);
extern void particle_text(const vec &s, const char *t, int type, int fade = 2000, int color = 0xFFFFFF, float size = 2.0f, int gravity = 0);
extern void particle_textcopy(const vec &s, const char *t, int type, int fade = 2000, int color = 0xFFFFFF, float size = 2.0f, int gravity = 0);
extern void particle_icon(const vec &s, int ix, int iy, int type, int fade = 2000, int color = 0xFFFFFF, float size = 2.0f, int gravity = 0);
extern void particle_meter(const vec &s, float val, int type, int fade = 1, int color = 0xFFFFFF, int color2 = 0xFFFFF, float size = 2.0f);
extern void particle_flare(const vec &p, const vec &dest, int fade, int type, int color = 0xFFFFFF, float size = 0.28f, physent *owner = NULL);
extern void particle_fireball(const vec &dest, float max, int type, int fade = -1, int color = 0xFFFFFF, float size = 4.0f);
extern void removetrackedparticles(physent *owner = NULL);


// stain

extern void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0);

enum
{
    StainBuffer_Opaque = 0,
    StainBuffer_Transparent,
    StainBuffer_Mapmodel,
    StainBuffer_Number,
};

struct stainrenderer;

extern void initstains();
extern void clearstains();
extern bool renderstains(int sbuf, bool gbuf, int layer = 0);
extern void cleanupstains();
extern void genstainmmtri(stainrenderer *s, const vec v[3]);

// rendersky
extern int skytexture, skyshadow, explicitsky;

extern void drawskybox(bool clear = false);
extern bool limitsky();
extern bool renderexplicitsky(bool outline = false);

// ui

namespace UI
{
    bool hascursor();
    void getcursorpos(float &x, float &y);
    void resetcursor();
    bool movecursor(int dx, int dy);
    bool keypress(int code, bool isdown);
    bool textinput(const char *str, int len);
    float abovehud();

    void setup();
    void update();
    void render();
    void cleanup();

    bool showui(const char *name);
    bool hideui(const char *name);
    bool toggleui(const char *name);
    void holdui(const char *name, bool on);
    bool uivisible(const char *name);
}
// menus

extern int mainmenu;

extern void addchange(const char *desc, int type);
extern void clearchanges(int type);
extern void menuprocess();
extern void clearmainmenu();

// sound
extern void clearmapsounds();
extern void checkmapsounds();
extern void updatesounds();

extern int playsound(int n, const vec *loc = NULL, extentity *ent = NULL, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern int playsoundname(const char *s, const vec *loc = NULL, int vol = 0, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern void preloadsound(int n);
extern void preloadmapsound(int n);
extern bool stopsound(int n, int chanid, int fade = 0);
extern void stopsounds();
extern void initsound();

// grass
extern void loadgrassshaders();
extern void generategrass();
extern void rendergrass();
extern void cleanupgrass();

#endif

/* iengine.h
 *
 * the interface the game uses to access the engine
 * this file contains those functions which comprise the API with which to write
 * a game against; this header is not called by the game internally and is the
 * header which should be included in the game code.
 */

extern int curtime;                     // current frame time
extern int lastmillis;                  // last time
extern int elapsedtime;                 // elapsed frame time
extern int totalmillis;                 // total elapsed time
extern uint totalsecs;
extern int gamespeed, paused;
extern vector<int> entgroup;

extern int worldscale, worldsize;


extern bool settexture(const char *name, int clamp = 0);
extern int xtraverts, xtravertsva;
extern SDL_Window *screen;

extern dynent *player;

//forward object declarations (used in some functions below)
struct DecalSlot;
struct VSlot;
struct Texture;
struct editinfo;
struct model;
struct undolist;
struct undoblock;
struct vslotmap;
struct prefab;

// control
extern void fatal(const char *s, ...) PRINTFARGS(1, 2);
extern int getclockmillis();
extern int initing;
extern int scr_w, scr_h;

// ragdoll

extern void moveragdoll(dynent *d);
extern void cleanragdoll(dynent *d);

// crypto
extern void genprivkey(const char *seed, vector<char> &privstr, vector<char> &pubstr);
extern bool calcpubkey(const char *privstr, vector<char> &pubstr);
extern bool hashstring(const char *str, char *result, int maxlen);
extern void answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr);

/*==============================================================================*\
 * Interface Functions & Values                                                 *
 *                                                                              *
 * Input, Scripting, Sound, UI                                                  *
 *                                                                              *
 * command.cpp                                                                  *
 * console.cpp                                                                  *
 * input.cpp                                                                    *
 * menus.cpp                                                                    *
 * sound.cpp                                                                    *
 * ui.cpp                                                                       *
\*==============================================================================*/

// command

extern std::queue<ident *> triggerqueue;
extern int variable(const char *name, int min, int cur, int max, int *storage, identfun fun, int flags);
extern float fvariable(const char *name, float min, float cur, float max, float *storage, identfun fun, int flags);
extern char *svariable(const char *name, const char *cur, char **storage, identfun fun, int flags);
extern void setvar(const char *name, int i, bool dofunc = true, bool doclamp = true);
extern void setfvar(const char *name, float f, bool dofunc = true, bool doclamp = true);
extern void setsvar(const char *name, const char *str, bool dofunc = true);
extern bool identexists(const char *name);
extern ident *getident(const char *name);
extern ident *newident(const char *name, int flags = 0);
extern ident *readident(const char *name);
extern ident *writeident(const char *name, int flags = 0);
extern bool addcommand(const char *name, identfun fun, const char *narg, int type = Id_Command);
extern uint *compilecode(const char *p);
extern void keepcode(uint *p);
extern void freecode(uint *p);
extern int execute(const uint *code);
extern int execute(const char *p);
extern int execute(ident *id, tagval *args, int numargs, bool lookup = false);
extern int execident(const char *name, int noid = 0, bool lookup = false);
extern bool executebool(const uint *code);
extern bool executebool(const char *p);
extern bool executebool(ident *id, tagval *args, int numargs, bool lookup = false);
extern bool execidentbool(const char *name, bool noid = false, bool lookup = false);
extern bool execfile(const char *cfgfile, bool msg = true);
extern void alias(const char *name, const char *action);
extern const char *escapestring(const char *s);
extern void explodelist(const char *s, vector<char *> &elems, int limit = -1);
extern void printvar(ident *id, int i);
extern int clampvar(ident *id, int i, int minval, int maxval);
extern void loopiter(ident *id, identstack &stack, int i);
extern void loopend(ident *id, identstack &stack);
const char *escapeid(const char *s);
extern void writecfg(const char *savedconfig, const char *autoexec = NULL, const char *defaultconfig = NULL, const char *name = NULL);
extern void checksleep(int millis);
extern bool initidents();

extern int identflags;

extern void clear_command();

// console

extern void conoutf(const char *s, ...) PRINTFARGS(1, 2);
extern void conoutf(int type, const char *s, ...) PRINTFARGS(2, 3);
extern void logoutf(const char *fmt, ...) PRINTFARGS(1, 2);
extern void logoutfv(const char *fmt, va_list args);
extern void clear_console();

// input

extern bool grabinput, minimized;

extern bool interceptkey(int sym);
extern void inputgrab(bool on);
extern void checkinput();
extern void ignoremousemotion();
extern void keyrepeat(bool on, int mask = ~0);


// menus

extern void menuprocess();
extern int mainmenu;

// sound

extern int playsound(int n, const vec *loc = NULL, extentity *ent = NULL, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern int playsoundname(const char *s, const vec *loc = NULL, int vol = 0, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern void preloadsound(int n);
extern void preloadmapsound(int n);
extern void preloadmapsounds();
extern bool stopsound(int n, int chanid, int fade = 0);
extern void stopsounds();
extern void initsound();
extern void updatesounds();
extern void clear_sound();


// UI

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


extern void addchange(const char *desc, int type);
/*==============================================================================*\
 * Render Functions & Values                                                    *
 *                                                                              *
 * World, Model, Material, Screenspace FX Rendering                             *
 *                                                                              *
 * aa.cpp                                                                       *
 * grass.cpp                                                                    *
 * normal.cpp                                                                   *
 * octarender.cpp                                                               *
 * radiancehints.cpp                                                            *
 * rendergl.cpp                                                                 *
 * renderlights.cpp                                                             *
 * rendermodel.cpp                                                              *
 * renderparticles.cpp                                                          *
 * rendersky.cpp                                                                *
 * rendertext.cpp                                                               *
 * renderva.cpp                                                                 *
 * renderwindow.cpp                                                             *
 * shader.cpp                                                                   *
 * stain.cpp                                                                    *
 * texture.cpp                                                                  *
 * water.cpp                                                                    *
\*==============================================================================*/

// octarender

extern void allchanged(bool load = false);

// rendergl

extern physent *camera1;
extern vec worldpos, camdir, camright, camup;
extern float curfov, fovy, aspect;
extern bool detachedcamera;

extern void disablezoom();

extern vec calcavatarpos(const vec &pos, float dist);
extern vec calcmodelpreviewpos(const vec &radius, float &yaw);

extern void damageblend(int n);
extern void damagecompass(int n, const vec &loc);

extern vec minimapcenter, minimapradius, minimapscale;
extern void bindminimap();

extern matrix4 hudmatrix;
extern void resethudmatrix();
extern void pushhudmatrix();
extern void flushhudmatrix(bool flushparams = true);
extern void pophudmatrix(bool flush = true, bool flushparams = true);
extern void pushhudscale(float sx, float sy = 0);
extern void resethudshader();
extern void recomputecamera();
extern void initgbuffer();
extern void computezoom();

extern void gl_checkextensions();
extern void gl_init();
extern void gl_resize();
extern void gl_setupframe(bool force = false);
extern void gl_drawframe(int crosshairindex, void (*gamefxn)(), void (*hudfxn)());

// renderlights

extern void clearshadowcache();

extern int spotlights;
extern int volumetriclights;
extern int nospeclights;

// rendermodel
extern int numanims;
extern std::vector<std::string> animnames;

extern void rendermodel(const char *mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int cull = Model_CullVFC | Model_CullDist | Model_CullOccluded, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float size = 1, const vec4 &color = vec4(1, 1, 1, 1));
extern int intersectmodel(const char *mdl, int anim, const vec &pos, float yaw, float pitch, float roll, const vec &o, const vec &ray, float &dist, int mode = 0, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float size = 1);
extern void abovemodel(vec &o, const char *mdl);
extern void renderclient(dynent *d, const char *mdlname, modelattach *attachments, int hold, int attack, int attackdelay, int lastaction, int lastpain, float scale = 1, bool ragdoll = false, float trans = 1);
extern void interpolateorientation(dynent *d, float &interpyaw, float &interppitch);
extern void setbbfrommodel(dynent *d, const char *mdl);
extern const char *mapmodelname(int i);
extern void preloadmodel(const char *name);
extern model *loadmapmodel(int n);
extern model *loadmodel(const char *name, int i = -1, bool msg = false);
extern void flushpreloadedmodels(bool msg = true);
extern void preloadusedmapmodels(bool msg = false, bool bih = false);
extern void clear_models();

// renderparticles

extern std::vector<std::string> entnames;

extern char * getentname(int i);
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
extern void initparticles();
extern void updateparticles();

// rendertext

extern void draw_text(const char *str, float left, float top, int r = 255, int g = 255, int b = 255, int a = 255, int cursor = -1, int maxwidth = -1);
extern void draw_textf(const char *fstr, float left, float top, ...) PRINTFARGS(1, 4);
extern void text_boundsf(const char *str, float &width, float &height, int maxwidth = -1);

extern bool setfont(const char *name);

// renderva

extern int octaentsize;

// renderwindow

extern void swapbuffers(bool overlay = true);
extern int fullscreen;
extern void setupscreen();
extern void restoregamma();
extern void restorevsync();
extern void resetfpshistory();
extern void limitfps(int &millis, int curmillis);
extern void updatefpshistory(int millis);
extern void cleargamma();

extern void renderbackground(const char *caption = NULL, Texture *mapshot = NULL, const char *mapname = NULL, const char *mapinfo = NULL, bool force = false);
extern void renderprogress(float bar, const char *text, bool background = false);

// shader

extern void loadshaders();

// stain
extern void initstains();
extern void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0);

inline void addstain(int type, const vec &center, const vec &surface, float radius, int color, int info = 0)
{
    addstain(type, center, surface, radius, bvec::hexcolor(color), info);
}

// texture

extern Texture *notexture;

extern Texture *textureload(const char *name, int clamp = 0, bool mipit = true, bool msg = true);
extern void packvslot(vector<uchar> &buf, int index);
extern void packvslot(vector<uchar> &buf, const VSlot *vs);

/*==============================================================================*\
 * World Functions & Values                                                     *
 *                                                                              *
 * World Loading, Physics, Collision, Entity Handling                           *
 *                                                                              *
 * bih.cpp                                                                      *
 * dynlight.cpp                                                                 *
 * light.cpp                                                                    *
 * material.cpp                                                                 *
 * octa.cpp                                                                     *
 * octaedit.cpp                                                                 *
 * physics.cpp                                                                  *
 * raycube.cpp                                                                  *
 * world.cpp                                                                    *
 * worldio.cpp                                                                  *
\*==============================================================================*/

// dynlight

extern void adddynlight(const vec &o, float radius, const vec &color, int fade = 0, int peak = 0, int flags = 0, float initradius = 0, const vec &initcolor = vec(0, 0, 0), physent *owner = NULL, const vec &dir = vec(0, 0, 0), int spot = 0);
extern void removetrackeddynlights(physent *owner = NULL);

// light

extern void clearlightcache(int id = -1);
extern void calclight();

// material

extern int findmaterial(const char *name);

// octa
extern ivec lu;
extern int lusize;

extern int lookupmaterial(const vec &o);
extern void freeocta(cube *c);
extern void getcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern void remip();
extern void optiface(uchar *p, cube &c);
extern bool isvalidcube(const cube &c);
extern cube &lookupcube(const ivec &to, int tsize = 0, ivec &ro = lu, int &rsize = lusize);

// octaedit

extern bool allowediting;
extern bool multiplayer;
extern editinfo *localedit;
extern selinfo sel;
extern vector<ushort> texmru;
extern selinfo repsel;
extern int reptex;
extern int lasttex;
extern int lasttexmillis;
extern int curtexindex;
extern int moving, orient;
extern bool editmode;
extern int entediting;
extern selinfo sel, lastsel, savedsel;
extern vector<vslotmap> unpackingvslots;
extern vector<vslotmap> remappedvslots;
extern undolist undos, redos;
extern int nompedit;
extern int hmapedit;
extern bool havesel;
extern vector<editinfo *> editinfos;
extern int texpaneltimer;
extern hashnameset<prefab> prefabs;

extern int shouldpacktex(int index);
extern bool packeditinfo(editinfo *e, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackeditinfo(editinfo *&e, const uchar *inbuf, int inlen, int outlen);
extern void freeeditinfo(editinfo *&e);
extern bool packundo(int op, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackundo(const uchar *inbuf, int inlen, int outlen);
extern bool noedit(bool view = false, bool msg = true);
extern void commitchanges(bool force = false);
extern bool pointinsel(const selinfo &sel, const vec &o);
extern void cancelsel();
extern void addundo(undoblock *u);
extern cube &blockcube(int x, int y, int z, const block3 &b, int rgrid);
extern void changed(const ivec &bbmin, const ivec &bbmax, bool commit);
extern void changed(const block3 &sel, bool commit = true);
extern void discardchildren(cube &c, bool fixtex = false, int depth = 0);
extern void makeundo(selinfo &s);
extern void makeundo();
extern void reorient();
extern int countblock(block3 *b);
extern bool hmapsel;
extern void forcenextundo();
extern void freeundo(undoblock *u);
extern void pasteblock(block3 &b, selinfo &sel, bool local);
extern void pasteundo(undoblock *u);
extern undoblock *newundocube(const selinfo &s);
extern void remapvslots(cube &c, bool delta, const VSlot &ds, int orient, bool &findrep, VSlot *&findedit);
extern void setmat(cube &c, ushort mat, ushort matmask, ushort filtermat, ushort filtermask, int filtergeom);
extern void edittexcube(cube &c, int tex, int orient, bool &findrep);
extern bool haveselent();
extern void pastecube(const cube &src, cube &dst);
extern void pasteundoblock(block3 *b, uchar *g);
extern bool uncompresseditinfo(const uchar *inbuf, int inlen, uchar *&outbuf, int &outlen);
extern void unpackundocube(ucharbuf buf, uchar *outbuf);
extern void multiplayerwarn();

namespace hmap
{
    extern void run(int dir, int mode);
}

// raycube

extern float raycube   (const vec &o, const vec &ray,     float radius = 0, int mode = Ray_ClipMat, int size = 0, extentity *t = 0);
extern float raycubepos(const vec &o, const vec &ray, vec &hit, float radius = 0, int mode = Ray_ClipMat, int size = 0);
extern float rayfloor  (const vec &o, vec &floor, int mode = 0, float radius = 0);
extern bool  raycubelos(const vec &o, const vec &dest, vec &hitpos);
extern bool insideworld(const vec &o);

// physics

extern int floatspeed;
extern const float slopez, wallz, floorz, stairheight;
extern vec collidewall;
extern int collideinside;
extern physent *collideplayer;
extern const float gravity, jumpvel;
extern int numdynents;
extern vector<dynent *> dynents;

extern dynent *iterdynents(int i);
extern void moveplayer(physent *pl, int moveres, bool local);
extern bool moveplayer(physent *pl, int moveres, bool local, int curtime);
extern void crouchplayer(physent *pl, int moveres, bool local);
extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f, bool playercol = true, bool insideplayercol = false);
extern bool bounce(physent *d, float secs, float elasticity, float waterfric, float grav);
extern void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space);
extern bool movecamera(physent *pl, const vec &dir, float dist, float stepdist);
extern void dropenttofloor(entity *e);
extern bool droptofloor(vec &o, float radius, float height);
extern void rotatebb(vec &center, vec &radius, int yaw, int pitch, int roll = 0);

extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m);
extern void vectoyawpitch(const vec &v, float &yaw, float &pitch);
extern void updatephysstate(physent *d);
extern void cleardynentcache();
extern void updatedynentcache(physent *d);
extern bool entinmap(dynent *d, bool avoidplayers = false);
extern void findplayerspawn(dynent *d, int forceent = -1, int tag = 0);

extern void modifygravity(physent *pl, bool water, int curtime);
extern void modifyvelocity(physent *pl, bool local, bool water, bool floating, int curtime);
extern void recalcdir(physent *d, const vec &oldvel, vec &dir);
extern void slideagainst(physent *d, vec &dir, const vec &obstacle, bool foundfloor, bool slidecollide);
extern void freeblock(block3 *b, bool alloced = true);
extern block3 *blockcopy(const block3 &s, int rgrid);
extern ushort getmaterial(cube &c);

// world

extern bool emptymap(int factor, bool force, const char *mname = "", bool usecfg = true);
extern bool enlargemap(bool force);
extern vec getselpos();
extern int getworldsize();
extern void entcancel();

extern void attachentity(extentity &e);
extern void attachentities();
extern void removeentityedit(int id);
extern void addentityedit(int id);
extern void detachentity(extentity &e);
extern void entselectionbox(const entity &e, vec &eo, vec &es);
extern void mmboundbox(const entity &e, model *m, vec &center, vec &radius);
extern float getdecalslotdepth(DecalSlot &s);
extern void entitiesinoctanodes();


namespace entities
{
    extern vector<extentity *> ents;

    extern extentity *newentity();
    extern void deleteentity(extentity *e);
}

// worldio

extern string clientmap;

extern bool load_world(const char *mname, const char *gameident, const char *gameinfo = NULL, const char *cname = NULL);
extern bool save_world(const char *mname, const char *gameident);
extern void fixmapname(char *name);
extern uint getmapcrc();
extern void clearmapcrc();
extern bool loadents(const char *fname, const char *gameident, vector<entity> &ents, uint *crc = NULL);


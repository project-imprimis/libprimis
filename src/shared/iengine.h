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

extern void lightent(extentity &e, float height = 8.0f);
extern void lightreaching(const vec &target, vec &color, vec &dir, bool fast = false, extentity *e = 0, float minambient = 0.4f);

extern int thirdperson;
extern bool isthirdperson();

extern bool settexture(const char *name, int clamp = 0);
extern int xtraverts, xtravertsva;

extern dynent *player;

//raycube

extern float raycube   (const vec &o, const vec &ray,     float radius = 0, int mode = Ray_ClipMat, int size = 0, extentity *t = 0);
extern float raycubepos(const vec &o, const vec &ray, vec &hit, float radius = 0, int mode = Ray_ClipMat, int size = 0);
extern float rayfloor  (const vec &o, vec &floor, int mode = 0, float radius = 0);
extern bool  raycubelos(const vec &o, const vec &dest, vec &hitpos);
extern bool insideworld(const vec &o);
extern bool insideworld(const ivec &o);

// octaedit

struct editinfo;
extern editinfo *localedit;
extern selinfo sel;

extern bool editmode;
extern int entediting;

extern int shouldpacktex(int index);
extern bool packeditinfo(editinfo *e, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackeditinfo(editinfo *&e, const uchar *inbuf, int inlen, int outlen);
extern void freeeditinfo(editinfo *&e);
extern void pruneundos(int maxremain = 0);
extern bool packundo(int op, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackundo(const uchar *inbuf, int inlen, int outlen);
extern bool noedit(bool view = false, bool msg = true);
extern void toggleedit(bool force = true);
extern void mpeditface(int dir, int mode, selinfo &sel, bool local);
extern void mpedittex(int tex, int allfaces, selinfo &sel, bool local);
extern bool mpedittex(int tex, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpeditmat(int matid, int filter, selinfo &sel, bool local);
extern void mpflip(selinfo &sel, bool local);
extern void mpcopy(editinfo *&e, selinfo &sel, bool local);
extern void mppaste(editinfo *&e, selinfo &sel, bool local);
extern void mprotate(int cw, selinfo &sel, bool local);
extern void mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, bool local);
extern bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf);
extern void mpdelcube(selinfo &sel, bool local);
extern void mpremip(bool local);
extern bool mpeditvslot(int delta, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpcalclight(bool local);
extern void commitchanges(bool force = false);
extern void changed(const ivec &bbmin, const ivec &bbmax, bool commit = true);
extern void changed(const block3 &sel, bool commit = true);
extern bool pointinsel(const selinfo &sel, const vec &o);
extern void addundo(undoblock *u);

// command
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
extern uint *compilecode(const char *p);
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

// console

extern void conoutf(const char *s, ...) PRINTFARGS(1, 2);
extern void conoutf(int type, const char *s, ...) PRINTFARGS(2, 3);
extern void conoutfv(int type, const char *fmt, va_list args);

extern FILE *getlogfile();
extern void setlogfile(const char *fname);
extern void closelogfile();
extern void logoutfv(const char *fmt, va_list args);
extern void logoutf(const char *fmt, ...) PRINTFARGS(1, 2);

// octa

extern int lookupmaterial(const vec &o);

// world

struct DecalSlot;
extern bool emptymap(int factor, bool force, const char *mname = "", bool usecfg = true);
extern bool enlargemap(bool force);
extern int findentity(int type, int index = 0, int attr1 = -1, int attr2 = -1);
extern void findents(int low, int high, bool notspawned, const vec &pos, const vec &radius, vector<int> &found);
extern vec getselpos();
extern int getworldsize();
extern int getmapversion();
extern void renderentcone(const extentity &e, const vec &dir, float radius, float angle);
extern void renderentarrow(const extentity &e, const vec &dir, float radius);
extern void renderentattachment(const extentity &e);
extern void renderentsphere(const extentity &e, float radius);
extern void renderentring(const extentity &e, float radius, int axis = 0);
extern void entcancel();

extern void attachentity(extentity &e);
extern void removeentityedit(int id);
extern void addentityedit(int id);
extern void detachentity(extentity &e);
extern void entselectionbox(const entity &e, vec &eo, vec &es);
extern void mmboundbox(const entity &e, model *m, vec &center, vec &radius);
extern float getdecalslotdepth(DecalSlot &s);

// main
extern void fatal(const char *s, ...) PRINTFARGS(1, 2);

// rendertext
extern void draw_text(const char *str, float left, float top, int r = 255, int g = 255, int b = 255, int a = 255, int cursor = -1, int maxwidth = -1);
extern void draw_textf(const char *fstr, float left, float top, ...) PRINTFARGS(1, 4);
extern float text_widthf(const char *str);
extern void text_boundsf(const char *str, float &width, float &height, int maxwidth = -1);
extern int text_visible(const char *str, float hitx, float hity, int maxwidth);
extern void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth);

// texture

struct VSlot;

extern void packvslot(vector<uchar> &buf, int index);
extern void packvslot(vector<uchar> &buf, const VSlot *vs);

extern void adddynlight(const vec &o, float radius, const vec &color, int fade = 0, int peak = 0, int flags = 0, float initradius = 0, const vec &initcolor = vec(0, 0, 0), physent *owner = NULL, const vec &dir = vec(0, 0, 0), int spot = 0);
extern void removetrackeddynlights(physent *owner = NULL);
extern DecalSlot &lookupdecalslot(int slot, bool load = true);

// rendergl
extern physent *camera1;
extern vec worldpos, camdir, camright, camup;
extern float curfov, fovy, aspect;

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
extern void pushhudtranslate(float tx, float ty, float sx = 0, float sy = 0);
extern void resethudshader();

// renderparticles

extern bool canaddparticles();
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
extern void clearparticleemitters();
extern bool printparticles(extentity &e, char *buf, int len);

// renderva
extern int octaentsize;

//octarender
extern void updatevabb(vtxarray *va, bool force = false);

// stain
extern void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0);

inline void addstain(int type, const vec &center, const vec &surface, float radius, int color, int info = 0)
{
    addstain(type, center, surface, radius, bvec::hexcolor(color), info);
}

// worldio

extern bool load_world(const char *mname, const char *cname = NULL);
extern bool save_world(const char *mname);
extern void fixmapname(char *name);
extern uint getmapcrc();
extern void clearmapcrc();
extern bool loadents(const char *fname, vector<entity> &ents, uint *crc = NULL);

// physics

extern void moveplayer(physent *pl, int moveres, bool local);
extern bool moveplayer(physent *pl, int moveres, bool local, int curtime);
extern void crouchplayer(physent *pl, int moveres, bool local);
extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f, bool playercol = true, bool insideplayercol = false);
extern bool bounce(physent *d, float secs, float elasticity, float waterfric, float grav);
extern bool bounce(physent *d, float elasticity, float waterfric, float grav);
extern void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space);
extern bool overlapsdynent(const vec &o, float radius);
extern bool movecamera(physent *pl, const vec &dir, float dist, float stepdist);
extern void physicsframe();
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

// sound

extern int playsound(int n, const vec *loc = NULL, extentity *ent = NULL, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern int playsoundname(const char *s, const vec *loc = NULL, int vol = 0, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern void preloadsound(int n);
extern void preloadmapsound(int n);
extern bool stopsound(int n, int chanid, int fade = 0);
extern void stopsounds();
extern void initsound();

// rendermodel

struct model;

extern void rendermodel(const char *mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int cull = Model_CullVFC | Model_CullDist | Model_CullOccluded, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float size = 1, const vec4 &color = vec4(1, 1, 1, 1));
extern int intersectmodel(const char *mdl, int anim, const vec &pos, float yaw, float pitch, float roll, const vec &o, const vec &ray, float &dist, int mode = 0, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float size = 1);
extern void abovemodel(vec &o, const char *mdl);
extern void renderclient(dynent *d, const char *mdlname, modelattach *attachments, int hold, int attack, int attackdelay, int lastaction, int lastpain, float scale = 1, bool ragdoll = false, float trans = 1);
extern void interpolateorientation(dynent *d, float &interpyaw, float &interppitch);
extern void setbbfrommodel(dynent *d, const char *mdl);
extern const char *mapmodelname(int i);
extern void preloadmodel(const char *name);
extern bool matchanim(const char *name, const char *pattern);
extern model *loadmapmodel(int n);
extern model *loadmodel(const char *name, int i = -1, bool msg = false);

// renderlights

extern void cleardeferredlightshaders();
extern void clearshadowcache();
extern void cleanupvolumetric();

extern int spotlights;
extern int volumetriclights;
extern int nospeclights;

// light

extern void clearlightcache(int id = -1);

// UI

namespace UI
{
    bool showui(const char *name);
    bool hideui(const char *name);
    bool toggleui(const char *name);
    void holdui(const char *name, bool on);
    bool uivisible(const char *name);
}

// ragdoll

extern void moveragdoll(dynent *d);
extern void cleanragdoll(dynent *d);

// server
extern int maxclients;

extern void *getclientinfo(int i);
extern ENetPeer *getclientpeer(int i);
extern ENetPacket *sendf(int cn, int chan, const char *format, ...);
extern ENetPacket *sendfile(int cn, int chan, stream *file, const char *format = "", ...);
extern void sendpacket(int cn, int chan, ENetPacket *packet, int exclude = -1);
extern void flushserver(bool force);
extern int getservermtu();
extern int getnumclients();
extern uint getclientip(int n);
extern void localconnect();
extern const char *disconnectreason(int reason);
extern void disconnect_client(int n, int reason);
extern void kicknonlocalclients(int reason = Discon_None);
extern bool hasnonlocalclients();
extern bool haslocalclients();
extern void sendserverinforeply(ucharbuf &p);
extern bool requestmaster(const char *req);
extern bool requestmasterf(const char *fmt, ...) PRINTFARGS(1, 2);
extern bool isdedicatedserver();

// serverbrowser

extern servinfo *getservinfo(int i);

#define GETSERVINFO(idx, si, body) do { \
    servinfo *si = getservinfo(idx); \
    if(si) \
    { \
        body; \
    } \
} while(0)

#define GETSERVINFOATTR(idx, aidx, aval, body) \
    GETSERVINFO(idx, si, { if(si->attr.inrange(aidx)) { int aval = si->attr[aidx]; body; } })

// client
extern void sendclientpacket(ENetPacket *packet, int chan);

extern void flushclient();
extern void disconnect(bool async = false, bool cleanup = true);
extern bool isconnected(bool attempt = false, bool local = true);
extern const ENetAddress *connectedpeer();
extern bool multiplayer(bool msg = true);
extern void neterr(const char *s, bool disc = true);
extern void gets2c();
extern void notifywelcome();

// crypto
extern void genprivkey(const char *seed, vector<char> &privstr, vector<char> &pubstr);
extern bool calcpubkey(const char *privstr, vector<char> &pubstr);
extern bool hashstring(const char *str, char *result, int maxlen);
extern void answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr);
extern void *parsepubkey(const char *pubstr);
extern void freepubkey(void *pubkey);
extern void *genchallenge(void *pubkey, const void *seed, int seedlen, vector<char> &challengestr);
extern void freechallenge(void *answer);
extern bool checkchallenge(const char *answerstr, void *correct);


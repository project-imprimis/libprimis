// the interface the engine uses to run the gameplay module

struct VSlot;

namespace game
{
    extern bool allowedittoggle();
    extern void toserver(char *text);
    extern void forceedit(const char *name);
    extern int scaletime(int t);

    extern const char *gameident();
    extern const char *savedconfig();
    extern const char *restoreconfig();
    extern const char *defaultconfig();
    extern const char *autoexec();

    extern void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material = 0);
    extern void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0, const VSlot *vs = NULL);
    extern void edittoggled(bool on);
    extern void vartrigger(ident *id);
    extern const char *getclientmap();
    extern const char *getmapinfo();
    extern const char *getscreenshotinfo();
    extern void writeclientinfo(stream *f);

    extern void newmap(int size);
    extern void startmap(const char *name);
    extern bool allowmove(physent *d);
    extern void preload();
    extern dynent *iterdynents(int i);
    extern int numdynents();

    extern void rendergame();
    extern void renderavatar();
    extern void renderplayerpreview(int model, int color, int team, int weap);
    extern int numanims();
    extern void findanims(const char *pattern, vector<int> &anims);
    extern float clipconsole(float w, float h);
    extern const char *defaultcrosshair(int index);
    extern int selectcrosshair(vec &col);
    extern void adddynlights();
    extern void particletrack(physent *owner, vec &o, vec &d);
    extern void dynlighttrack(physent *owner, vec &o, vec &hud);
    extern bool needminimap();

    extern void setupcamera();
    extern bool allowthirdperson();
    extern bool detachcamera();
    extern bool collidecamera();
    extern float abovegameplayhud(int w, int h);

    extern void gameplayhud(int w, int h);
    extern void writegamedata(vector<char> &extras);
    extern void readgamedata(vector<char> &extras);
}

extern bool isconnected(bool attempt = false, bool local = true);
extern bool multiplayer(bool msg = true);

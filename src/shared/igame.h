// the interface the engine uses to run the gameplay module

struct VSlot;

namespace game
{
    extern bool allowedittoggle();
    extern void toserver(char *text);
    extern void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material = 0);
    extern void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0, const VSlot *vs = NULL);
    extern void vartrigger(ident *id);

    extern dynent *iterdynents(int i);
    extern int numdynents();

    extern void rendergame();
    extern void renderavatar();
    extern void renderplayerpreview(int model, int color, int team, int weap);
    extern int numanims();
    extern void findanims(const char *pattern, vector<int> &anims);
}

extern bool isconnected(bool attempt = false, bool local = true);
extern bool multiplayer(bool msg = true);

// the interface the engine uses to run the gameplay module

namespace game
{
    extern bool allowedittoggle();
    extern void vartrigger(ident *id);

    extern dynent *iterdynents(int i);
    extern int numdynents();

    extern void rendergame();
    extern void renderavatar();
    extern void renderplayerpreview(int model, int color, int team, int weap);
    extern int numanims();
    extern void findanims(const char *pattern, vector<int> &anims);
}

extern bool multiplayer(bool msg = true);
extern char *entname(entity &e);

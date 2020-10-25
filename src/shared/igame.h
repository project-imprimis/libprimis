// the interface the engine uses to run the gameplay module

namespace game
{
    extern bool allowedittoggle();
    extern void vartrigger(ident *id);

    extern dynent *iterdynents(int i);

    extern void rendergame();
    extern void renderavatar();
    extern void renderplayerpreview(int model, int color, int team, int weap);
}

extern bool multiplayer(bool msg = true);
extern char *entname(entity &e);

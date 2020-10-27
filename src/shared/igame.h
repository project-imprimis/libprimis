// the interface the engine uses to run the gameplay module

namespace game
{
    extern void vartrigger(ident *id);
    extern dynent *iterdynents(int i);
    extern void renderavatar();
    extern void renderplayerpreview(int model, int color, int team, int weap);
}

extern char *entname(entity &e);

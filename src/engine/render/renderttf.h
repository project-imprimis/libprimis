extern bool initttf();
extern TTF_Font* openfont(const char * path, int size);
extern void renderttf(TTF_Font* f, const char* message, SDL_Color col, int x, int y, float scale = 1.f);

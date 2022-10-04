#ifndef RENDERTTF_H_
#define RENDERTTF_H_

struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;

class TTFRenderer
{
    public:
        bool initttf();
        void openfont(const char * path, int size);
        void renderttf(const char* message, SDL_Color col, int x, int y, float scale = 1.f, uint wrap = 0);
        GLuint renderttfgl(const char* message, SDL_Color col, int x, int y, float scale = 1.f, uint wrap = 0);
        void fontsize(int pts = 12);
    private:
        TTF_Font* f;                         //the current working font
        std::map<int, TTF_Font *> fontcache; //different sizes of the font are cached in a map which maps them to their size in pt
        const char * path;                   //the path which the font was originally found, so it can load other font sizes if needed
};

extern TTFRenderer ttr;

#endif

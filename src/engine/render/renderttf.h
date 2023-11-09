#ifndef RENDERTTF_H_
#define RENDERTTF_H_

struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;

class TTFRenderer
{
    public:
        //starts up SDL2_TTF
        //if the init process did not start properly, returns false
        bool initttf();

        //opens a font with the given path and size in points
        //if fails, returns nullptr to internal value f
        void openfont(const char * path, int size);

        //draws a string to the coordinates x, y in the current hud context at a scale factor `scale`
        //with a (BGRA) SDL_Color value as passed to its third parameter
        void renderttf(const char* message, SDL_Color col, int x, int y, float scale = 1.f, uint wrap = 0) const;

        //sets the current working font renderer to one with the appropriate font size
        //if the size does not exist already, creates a new one with the appropriate size
        void fontsize(int pts = 12);

        void ttfbounds(const char *str, float &width, float &height, int pts);
        //returns the dimensions (x,y) of the rendered text, without paying the full cost of rendering
        ivec2 ttfsize(const char* message);
    private:

        // TTF Surface information
        struct TTFSurface
        {
            GLuint tex;
            int w;
            int h;
        };

        TTF_Font* f;                         //the current working font
        std::map<int, TTF_Font *> fontcache; //different sizes of the font are cached in a map which maps them to their size in pt
        const char * path;                   //the path which the font was originally found, so it can load other font sizes if needed
        TTFSurface renderttfgl(const char* message, SDL_Color col, int x, int y, uint wrap = 0) const;
        std::string trimstring(std::string msg) const; //trims color codes out of a string
};

extern TTFRenderer ttr;

#endif

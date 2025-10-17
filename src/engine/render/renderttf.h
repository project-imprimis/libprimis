#ifndef RENDERTTF_H_
#define RENDERTTF_H_

struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;

class TTFRenderer final
{
    public:
        /**
         * @brief Starts up SDL2_TTF
         *
         * If the init process did not start properly, returns false. Called once
         * per program lifetime.
         *
         * @return true if started correctly, false otherwise
         */
        static bool initttf();

        /**
         * @brief Opens a new object wrapping a font.
         *
         * Opens a font with the given path and size in points. A new point size
         * can be specified later, but the font cannot be changed.
         *
         * If this fails, returns nullptr to internal field f.
         *
         * @param path the path to the font file
         * @param size the size in points of the font
         */
        void openfont(const char * path, int size);

        /**
         * @brief Draws a string.
         *
         * Draws a string to the coordinates x, y in the current hud context at a scale factor `scale`
         * with a (BGRA) SDL_Color value as passed to its third parameter. The font size is implicit
         * to whatever fontsize() has set
         *
         * @param message string to draw
         * @param color color of text to draw
         * @param x x coordinate to draw at
         * @param y y coordinate to draw at
         * @param scale the scale factor of the text
         * @param wrap number of pixels before the text should wrap to more lines
         */
        void renderttf(const char* message, SDL_Color col, int x, int y, float scale = 1.f, uint wrap = 0) const;

        /**
         * @brief Changes this font's working font size
         *
         * Sets the current working font renderer to one with the appropriate font size.
         * If the size does not exist already, creates a new one with the appropriate size.
         *
         * @param pts the point size of the font to add
         */
        void fontsize(int pts = 12);

        /**
         * @brief Returns to parameters size of the text without rendering it.
         *
         * Faster than actually drawing the text, allows checking the dimension
         * in pixels of a sample string.
         *
         * @param str the string to try
         * @param width outputs the width of the rendered text
         * @param height outputs the height of the rendered text
         * @param pts the size of the font to trial
         */
        void ttfbounds(std::string_view str, float &width, float &height, int pts);

        /**
         * @brief returns the dimensions (x,y) of the rendered text, without paying the full cost of rendering
         *
         * @param message the message to determine size of
         *
         * @return ivec2 containing x,y size in pixels
         */
        ivec2 ttfsize(std::string_view message);
    private:

        // TTF Surface information
        struct TTFSurface final
        {
            GLuint tex; /// the texture handle of the drawn ttf object
            int w; /// width of texture in pixels
            int h; /// height of texture in pixels
        };

        TTF_Font* f;                         //the current working font
        std::map<int, TTF_Font *> fontcache; //different sizes of the font are cached in a map which maps them to their size in pt
        const char * path;                   //the path which the font was originally found, so it can load other font sizes if needed
        TTFSurface renderttfgl(const char* message, SDL_Color col, uint wrap = 0) const;

        /**
         * @brief Removes CubeScript color codes from a string
         *
         * Removes any instances of "^f#" from the passed string, and returns a new
         * string with these modifications applied.
         *
         * @param msg the string to parse
         *
         * @return a new string equal to the passed `msg` but with no CS color codes
         */
        static std::string trimstring(std::string msg); //trims color codes out of a string
};

extern TTFRenderer ttr;

#endif

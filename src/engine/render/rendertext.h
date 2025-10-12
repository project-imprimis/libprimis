#ifndef RENDERTEXT_H_
#define RENDERTEXT_H_
struct Font final
{
    struct CharInfo final
    {
        float x, y, w, h, offsetx, offsety, advance;
        int tex;

    };

    std::string name;
    std::vector<Texture *> texs;
    std::vector<CharInfo> chars;
    int charoffset, defaultw, defaulth, scale;
    float bordermin, bordermax, outlinemin, outlinemax;

    Font() {}
};

#define FONTH (curfont->scale)

extern Font *curfont;

inline int fontwidth()
{
    return FONTH/2;
}

extern float textscale;

/**
 * @brief Reloads all fonts in the font map.
 *
 * If any fonts fail to reload, terminates the program with an error message.
 */
extern void reloadfonts();

/**
 * @brief Attempts to set the global curfont variable to the passed object.
 *
 * No effect if the pointer passed is null.
 *
 * @param f the font to attempt to set.
 */
inline void setfont(Font *f)
{
    if(f)
    {
        curfont = f;
    }
}

/**
 * @brief Attempts to set the global curfont variable with a named entry in fonts global
 *
 * If no font exists by the name passed, returns false and curfont remains unchanged.
 *
 * @param name the name of the font to search for
 *
 * @return true if successfully set curfont, false otherwise
 */
extern bool setfont(const char *name);

/**
 * @brief Push whatever font is assigned to curfont to the font stack.
 *
 * fontstack is a std::stack of font pointers. The value in curfont will be
 * added to fontstack even if it is null or duplicated.
 */
extern void pushfont();
extern bool popfont();
extern void gettextres(int &w, int &h);

extern float text_widthf(const char *str);
extern void text_boundsf(const char *str, float &width, float &height, int maxwidth = -1);
extern int text_visible(const char *str, float hitx, float hity, int maxwidth);
extern void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth);

inline int text_width(const char *str)
{
    return static_cast<int>(std::ceil(text_widthf(str)));
}
#endif

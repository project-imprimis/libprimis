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
constexpr int minreswidth = 640,
              minresheight = 480;

extern float textscale;

extern void reloadfonts();

inline void setfont(Font *f)
{
    if(f)
    {
        curfont = f;
    }
}

extern bool setfont(const char *name);
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

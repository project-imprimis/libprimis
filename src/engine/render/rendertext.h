#ifndef RENDERTEXT_H_
#define RENDERTEXT_H_
struct font
{
    struct charinfo
    {
        float x, y, w, h, offsetx, offsety, advance;
        int tex;
    };

    char *name;
    vector<Texture *> texs;
    vector<charinfo> chars;
    int charoffset, defaultw, defaulth, scale;
    float bordermin, bordermax, outlinemin, outlinemax;

    font() : name(nullptr) {}
    ~font()
    {
        delete[] name;
    }
};

#define FONTH (curfont->scale)

extern font *curfont;

inline int fontwidth()
{
    return FONTH/2;
}
constexpr int minreswidth = 640,
              minresheight = 480;

extern Shader *textshader;
extern const matrix4x3 *textmatrix;
extern float textscale;

extern font *findfont(const char *name);
extern void reloadfonts();

inline void setfont(font *f)
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

extern void draw_text(const char *str, float left, float top, int r = 255, int g = 255, int b = 255, int a = 255, int cursor = -1, int maxwidth = -1);
extern void draw_textf(const char *fstr, float left, float top, ...) PRINTFARGS(1, 4);
extern float text_widthf(const char *str);
extern void text_boundsf(const char *str, float &width, float &height, int maxwidth = -1);
extern int text_visible(const char *str, float hitx, float hity, int maxwidth);
extern void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth);

inline void text_bounds(const char *str, int &width, int &height, int maxwidth = -1)
{
    float widthf, heightf;
    text_boundsf(str, widthf, heightf, maxwidth);
    width = static_cast<int>(std::ceil(widthf));
    height = static_cast<int>(std::ceil(heightf));
}

inline void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth)
{
    float cxf, cyf;
    text_posf(str, cursor, cxf, cyf, maxwidth);
    cx = static_cast<int>(cxf);
    cy = static_cast<int>(cyf);
}

inline int text_width(const char *str)
{
    return static_cast<int>(std::ceil(text_widthf(str)));
}
#endif

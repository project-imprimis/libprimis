/* rendertext.cpp: core text functionality for console/ui rendering
 *
 * libprimis uses a font image generated by the `tessfont` utility which has
 * letters sampled from it by reading small sub-rectangles of the overall font
 * image
 *
 * rendertext supports loading, recoloring, resizing, and rendering text to the
 * screen from these font images, which contain raster images of the font to be
 * rendered
 *
 * only a single font can be used at a time, though it can be scaled as needed
 * for various different purposes in UIs (e.g. titles vs body text)
 *
 * fonts are generated by the external `tessfont` utility and consist of a
 * raster image containing the acceptable characters which are sampled as needed
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "rendergl.h"
#include "rendertext.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/control.h"

static hashnameset<font> fonts;
static font *fontdef = nullptr;
static int fontdeftex = 0;

font *curfont = nullptr;

//adds a new font to the hashnameset "fonts" given the parameters passed
static void newfont(char *name, char *tex, int *defaultw, int *defaulth, int *scale)
{
    font *f = &fonts[name];
    if(!f->name)
    {
        f->name = newstring(name);
    }
    f->texs.clear();
    f->texs.push_back(textureload(tex));
    f->chars.clear();
    f->charoffset = '!';
    f->defaultw = *defaultw;
    f->defaulth = *defaulth;
    f->scale = *scale > 0 ? *scale : f->defaulth;
    f->bordermin = 0.49f;
    f->bordermax = 0.5f;
    f->outlinemin = -1;
    f->outlinemax = 0;

    fontdef = f;
    fontdeftex = 0;
}

//sets the fontdef gvar's bordermin/max to the values passed
static void fontborder(float *bordermin, float *bordermax)
{
    if(!fontdef)
    {
        return;
    }
    fontdef->bordermin = *bordermin;
    fontdef->bordermax = std::max(*bordermax, *bordermin+0.01f);
}

//sets the fontdef gvar's outlinemin/max to the values passed
static void fontoutline(float *outlinemin, float *outlinemax)
{
    if(!fontdef)
    {
        return;
    }
    fontdef->outlinemin = std::min(*outlinemin, *outlinemax-0.01f);
    fontdef->outlinemax = *outlinemax;
}

/* fontoffset
 * sets the character offset for the currently loaded font
 *
 * Arguments:
 *    c: a pointer to a char * array representing the character to be offset to
 *
 * Only the first element of the array is accepted, all others are ignored
 *
 */
static void fontoffset(char *c)
{
    if(!fontdef)
    {
        return;
    }
    fontdef->charoffset = c[0];
}

/* fontscale
 * sets the global scale for fonts
 *
 * Arguments:
 *    scale: a pointer to an integer representing the new scale to set the font to
 *
 * If the scale parameter points to the value 0, the font scale is et to its default value.
 *
 */
static void fontscale(int *scale)
{
    if(!fontdef)
    {
        return;
    }

fontdef->scale = *scale > 0 ? *scale : fontdef->defaulth;
}


/* fonttex
 * adds a texture for fonts to be loaded from
 *
 * Arguments:
 *    s: a pointer to a char * array representing the path to a font texture file
 */
static void fonttex(char *s)
{
    if(!fontdef)
    {
        return;
    }
    Texture *t = textureload(s);
    for(uint i = 0; i < fontdef->texs.size(); i++)
    {
        if(fontdef->texs[i] == t)
        {
            fontdeftex = i;
            return;
        }
    }
    fontdeftex = fontdef->texs.size();
    fontdef->texs.push_back(t);
}

/* fontchar
 * adds an entry to the fontdef vector
 * sets the new entry in the vector to have the parameters passed
 *
 */
static void fontchar(float *x, float *y, float *w, float *h, float *offsetx, float *offsety, float *advance)
{
    if(!fontdef)
    {
        return;
    }
    fontdef->chars.emplace_back();
    font::charinfo &c = fontdef->chars.back();
    c.x = *x;
    c.y = *y;
    c.w = *w ? *w : fontdef->defaultw;
    c.h = *h ? *h : fontdef->defaulth;
    c.offsetx = *offsetx;
    c.offsety = *offsety;
    c.advance = *advance ? *advance : c.offsetx + c.w;
    c.tex = fontdeftex;
}

/* fontskip
 * addes an entry to the fontdef vector, which is empty
 */
static void fontskip(int *n)
{
    if(!fontdef)
    {
        return;
    }
    for(int i = 0; i < std::max(*n, 1); ++i)
    {
        fontdef->chars.emplace_back();
        font::charinfo &c = fontdef->chars.back();
        c.x = c.y = c.w = c.h = c.offsetx = c.offsety = c.advance = 0;
        c.tex = 0;
    }
}

/* fontalias
 * copies an entry in the fontdef vector to another one
 * copies the entry at *src to *dst
 */
static void fontalias(const char *dst, const char *src)
{
    font *s = fonts.access(src);
    if(!s)
    {
        return;
    }
    font *d = &fonts[dst];
    if(!d->name)
    {
        d->name = newstring(dst);
    }
    d->texs = s->texs;
    d->chars = s->chars;
    d->charoffset = s->charoffset;
    d->defaultw = s->defaultw;
    d->defaulth = s->defaulth;
    d->scale = s->scale;
    d->bordermin = s->bordermin;
    d->bordermax = s->bordermax;
    d->outlinemin = s->outlinemin;
    d->outlinemax = s->outlinemax;

    fontdef = d;
    fontdeftex = d->texs.size()-1;
}

font *findfont(const char *name)
{
    return fonts.access(name);
}

bool setfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f)
    {
        return false;
    }
    curfont = f;
    return true;
}

static std::stack<font *> fontstack;

void pushfont()
{
    fontstack.push(curfont);
}

bool popfont()
{
    if(fontstack.empty())
    {
        return false;
    }
    curfont = fontstack.top();
    fontstack.pop();
    return true;
}

void gettextres(int &w, int &h)
{
    if(w < minreswidth || h < minresheight)
    {
        if(minreswidth > w*minresheight/h)
        {
            h = h*minreswidth/w;
            w = minreswidth;
        }
        else
        {
            w = w*minresheight/h;
            h = minresheight;
        }
    }
}

float text_widthf(const char *str)
{
    float width, height;
    text_boundsf(str, width, height);
    return width;
}

//returns the size of a tab character, which is hardcoded to 4 spaces
static int fonttab()
{
    return 4*fontwidth();
}

static int texttab(float x)
{
    return (static_cast<int>((x)/fonttab())+1.0f)*fonttab();
}

float textscale = 1;

#define TEXTSKELETON \
    float y = 0, \
          x = 0, \
          scale = curfont->scale/static_cast<float>(curfont->defaulth);\
    int i;\
    for(i = 0; str[i]; i++)\
    {\
        TEXTINDEX(i) /*textindex *must* be defined before runtime, it is not defined above here*/ \
        int c = static_cast<uchar>(str[i]);\
        if(c=='\t')\
        {\
            x = texttab(x);\
            TEXTWHITE(i)\
        }\
        else if(c==' ') \
        { \
            x += scale*curfont->defaultw; \
            TEXTWHITE(i) /*textwhite *must* be defined before runtime, it is not defined above here*/ \
        }\
        else if(c=='\n') \
        { \
            TEXTLINE(i) x = 0; \
            y += FONTH; \
        }\
        else if(c=='\f') \
        { \
            if(str[i+1]) \
            { \
                i++; \
                TEXTCOLOR(i) /*textcolor *must* be defined before runtime, it is not defined above here*/ \
            } \
        }\
        else if(curfont->chars.size() > static_cast<uint>(c-curfont->charoffset))\
        {\
            float cw = scale*curfont->chars[c-curfont->charoffset].advance;\
            if(cw <= 0) \
            { \
                continue; \
            } \
            if(maxwidth >= 0)\
            {\
                int j = i;\
                float w = cw;\
                for(; str[i+1]; i++)\
                {\
                    int c = static_cast<uchar>(str[i+1]);\
                    if(c=='\f') \
                    { \
                        if(str[i+2]) \
                        { \
                            i++; \
                        } \
                        continue; \
                    } \
                    if(!(curfont->chars.size() > static_cast<uint>(c-curfont->charoffset))) \
                    { \
                        break; \
                    } \
                    float cw = scale*curfont->chars[c-curfont->charoffset].advance; \
                    if(cw <= 0 || w + cw > maxwidth) \
                    { \
                        break; \
                    } \
                    w += cw; \
                } \
                if(x + w > maxwidth && x > 0) \
                { \
                    static_cast<void>(j); \
                    TEXTLINE(j-1); \
                    x = 0; \
                    y += FONTH; } \
                TEXTWORD \
            } \
            else \
            { \
                TEXTCHAR(i) \
            }\
        }\
    }

//all the chars are guaranteed to be either drawable or color commands
#define TEXTWORDSKELETON \
    for(; j <= i; j++)\
    {\
        TEXTINDEX(j) /*textindex *must* be defined before runtime, it is not defined above here*/ \
        int c = static_cast<uchar>(str[j]);\
        if(c=='\f') \
        { \
            if(str[j+1]) \
            { \
                j++; \
                TEXTCOLOR(j) /*textcolor *must* be defined before runtime, it is not defined above here*/ \
            } \
        }\
        else \
        { \
            float cw = scale*curfont->chars[c-curfont->charoffset].advance; \
            TEXTCHAR(j); \
        }\
    }

#define TEXTEND(cursor) \
    if(cursor >= i) \
    { \
        do \
        { \
            TEXTINDEX(cursor); /*textindex *must* be defined before runtime, it is not defined above here*/ \
        } while(0); \
    } \

int text_visible(const char *str, float hitx, float hity, int maxwidth)
{
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx) \
    { \
        if(y+FONTH > hity && x >= hitx) \
        { \
            return idx; \
        } \
    }
    #define TEXTLINE(idx) \
    { \
        if(y+FONTH > hity) \
        { \
            return idx; \
        } \
    }
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) \
    { \
        x += cw; \
        TEXTWHITE(idx) \
    }
    #define TEXTWORD TEXTWORDSKELETON
    TEXTSKELETON
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
    return i;
}

//inverse of text_visible
void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth)
{
    #define TEXTINDEX(idx) \
    { \
        if(idx == cursor) \
        { \
            cx = x; \
            cy = y; \
            break; \
        } \
    }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx)
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += cw;
    #define TEXTWORD TEXTWORDSKELETON if(i >= cursor) break;
    cx = cy = 0;
    TEXTSKELETON
    TEXTEND(cursor)
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void text_boundsf(const char *str, float &width, float &height, int maxwidth)
{
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) if(x > width) width = x;
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += cw;
    #define TEXTWORD x += w;
    width = 0;
    TEXTSKELETON
    height = y + FONTH;
    TEXTLINE(_)
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void reloadfonts()
{
    ENUMERATE(fonts, font, f,
        for(uint i = 0; i < f.texs.size(); i++)
        {
            if(!reloadtexture(*f.texs[i]))
            {
                fatal("failed to reload font texture");
            }
        }
    );
}

void initrendertextcmds()
{
    addcommand("fontalias", reinterpret_cast<identfun>(fontalias), "ss", Id_Command);
    addcommand("font", reinterpret_cast<identfun>(newfont), "ssiii", Id_Command);
    addcommand("fontborder", reinterpret_cast<identfun>(fontborder), "ff", Id_Command);
    addcommand("fontoutline", reinterpret_cast<identfun>(fontoutline), "ff", Id_Command);
    addcommand("fontoffset", reinterpret_cast<identfun>(fontoffset), "s", Id_Command);
    addcommand("fontscale", reinterpret_cast<identfun>(fontscale), "i", Id_Command);
    addcommand("fonttex", reinterpret_cast<identfun>(fonttex), "s", Id_Command);
    addcommand("fontchar", reinterpret_cast<identfun>(fontchar), "fffffff", Id_Command);
    addcommand("fontskip", reinterpret_cast<identfun>(fontskip), "i", Id_Command);
}

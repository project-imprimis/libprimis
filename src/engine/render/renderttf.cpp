/** @brief SDL2_TTF text renderer
 *
 * This file implements the wrapper functions needed to use SDL_TTF in the applications
 * where Tessfont/rendertext.cpp would have been used, allowing for any TTF font and
 * any Unicode character to be renderered at any font size, which are all limitations
 * of the legacy Tessfont font sampling system.
 *
 */
#include "SDL_ttf.h"

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "rendergl.h"
#include "rendertext.h"
#include "renderttf.h"
#include "shaderparam.h"
#include "texture.h"
#include "renderwindow.h"

TTFRenderer ttr;

//starts up SDL2_TTF
bool TTFRenderer::initttf()
{
    if(TTF_Init() < 0)
    {
        return false;
    }
    return true;
}

//opens a font with the given path and size in points
//if fails, returns nullptr to internal value f
void TTFRenderer::openfont(const char * inpath, int size)
{
    f = TTF_OpenFont(inpath, size);
    TTF_SetFontStyle(f, TTF_STYLE_NORMAL);
    TTF_SetFontOutline(f, 0);
    TTF_SetFontKerning(f, 1);
    TTF_SetFontHinting(f, TTF_HINTING_NORMAL);
    fontcache[size] = f;
    path = inpath;
}

//draws a string to the coordinates x, y in the current hud context at a scale factor `scale`
//with a (BGRA) SDL_Color value as passed to its third parameter
void TTFRenderer::renderttf(const char* message, SDL_Color col, int x, int y, float scale, uint wrap)
{
    if(!message)
    {
        return;
    }
    SDL_Surface* text = TTF_RenderUTF8_Blended_Wrapped(f, message, col, wrap);
    if(text)
    {
        glEnable(GL_BLEND);
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex);
        //need to load it in reversed because of how SDL_ttf renders
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, text->pitch/4, text->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, text->pixels);

        float w = text->w*scale,
              h = text->h*scale;
        SETSHADER(hudrect);
        gle::colorf(1, 1, 1, 1);
        int tw = text->w,
            th = text->h;
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x+w, y);  gle::attribf(tw, 0);
        gle::attribf(x, y);    gle::attribf(0, 0);
        gle::attribf(x+w,y+h); gle::attribf(tw, th);
        gle::attribf(x,y+h);   gle::attribf(0, th);
        gle::end();
        //clean up
        hudshader->set();
        glDeleteTextures(1, &tex);
    }
    SDL_FreeSurface(text);
}

//returns a GLuint refering to a rectangle texture containing the rendered image of a texture
GLuint TTFRenderer::renderttfgl(const char* message, SDL_Color col, int x, int y, float scale, uint wrap)
{
    if(!message)
    {
        return 0;
    }
        GLuint tex = 0;
    SDL_Surface* text = TTF_RenderUTF8_Blended_Wrapped(f, message, col, wrap);
    if(text)
    {
        glEnable(GL_BLEND);
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex);
        //need to load it in reversed because of how SDL_ttf renders
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, text->pitch/4, text->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, text->pixels);
    }
    SDL_FreeSurface(text);
    return tex;
}

//sets the current working font renderer to one with the appropriate font size
//if the size does not exist already, creates a new one with the appropriate size
void TTFRenderer::fontsize(int pts)
{
    auto itr = fontcache.find(pts);
    if(itr == fontcache.end())
    {
        TTF_Font* newfont = TTF_OpenFont(path, pts);
        TTF_SetFontStyle(newfont, TTF_STYLE_NORMAL);
        TTF_SetFontOutline(newfont, 0);
        TTF_SetFontKerning(newfont, 1);
        TTF_SetFontHinting(newfont, TTF_HINTING_NORMAL);
        fontcache[pts] = newfont;
        f = newfont;
    }
    else
    {
        f = fontcache[pts];
    }
}

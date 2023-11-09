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
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"
#include "renderwindow.h"

TTFRenderer ttr;

bool TTFRenderer::initttf()
{
    if(TTF_Init() < 0)
    {
        return false;
    }
    return true;
}

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

std::string TTFRenderer::trimstring(std::string msg) const
{
    for(;;)
    {
        size_t itr = msg.find("^f");
        if(itr < msg.size())
        {
            msg.erase(itr, 3);
        }
        else
        {
            break;
        }
    }
    return msg;
}

void TTFRenderer::renderttf(const char* message, SDL_Color col, int x, int y, float scale, uint wrap) const
{
    std::string msg = std::string(message);

    if(!msg.size())
    {
        return;
    }

    msg = trimstring(msg);

    TTFSurface tex = renderttfgl(msg.c_str(), col, x, y, wrap);
    if(tex.tex)
    {
        float w = tex.w*scale,
              h = tex.h*scale;
        SETSHADER(hudrect);
        gle::colorf(1, 1, 1, 1);
        int tw = tex.w,
            th = tex.h;
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
        glDeleteTextures(1, &(tex.tex));
    }
    else
    {
        printf("failed to render text:  %s\n", message);
    }
}

void TTFRenderer::ttfbounds(const char *str, float &width, float &height, int pts)
{
    fontsize(pts);
    ivec2 size = ttfsize(str);
    width = size.x;
    height = size.y;
}

ivec2 TTFRenderer::ttfsize(const char* message)
{
    if(!std::strlen(message))
    {
        return ivec2(0,0);
    }
    std::string msg = trimstring(std::string(message));
    ivec2 size;
    TTF_SizeUTF8(f, msg.c_str(), &size.x, &size.y);
    return size;
}

TTFRenderer::TTFSurface TTFRenderer::renderttfgl(const char* message, SDL_Color col, int x, int y, uint wrap) const
{
    if(!message)
    {
        return {0, 0, 0};
    }
    GLuint tex = 0;
    SDL_Color rgbcol = {col.b, col.g, col.r, 0};
    SDL_Surface* text = TTF_RenderUTF8_Blended_Wrapped(f, message, rgbcol, wrap);
    TTFSurface tts;
    if(text)
    {
        glEnable(GL_BLEND);
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex);
        //need to load it in reversed because of how SDL_ttf renders
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, text->pitch/4, text->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, text->pixels);
        tts = {tex, text->w, text->h};
    }
    else //empty string may cause this
    {
        tts = {0, 0, 0};
    }
    SDL_FreeSurface(text);
    return tts;
}

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

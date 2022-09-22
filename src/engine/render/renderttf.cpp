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
#include "shaderparam.h"
#include "texture.h"
#include "renderwindow.h"

//starts up SDL2_TTF
bool initttf()
{
    if(TTF_Init() < 0)
    {
        return false;
    }
    return true;
}

//opens a font with the given path and size in points
//if fails, returns nullptr
TTF_Font* openfont(const char * path, int size)
{
    TTF_Font* f = TTF_OpenFont(path, size);
    TTF_SetFontStyle(f, TTF_STYLE_NORMAL);
    TTF_SetFontOutline(f, 0);
    TTF_SetFontKerning(f, 1);
    TTF_SetFontHinting(f, TTF_HINTING_NORMAL);
    return f;
}

//draws a string to the coordinates x, y in the current hud context at a scale factor `scale`
//with a (BGRA) SDL_Color value as passed to its third parameter
void renderttf(TTF_Font* f, const char* message, SDL_Color col, int x, int y, float scale)
{
    glEnable(GL_BLEND);
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    GLuint tex;
    glGenTextures(1, &tex);
    SDL_Surface* text = TTF_RenderUTF8_Blended(f, message, col);
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
    //need to load it in reversed because of how SDL_ttf renders
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, text->w, text->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, text->pixels);

    float w = text->w*scale, //if debugfullscreen, set to hudw/hudh size; if not, do small size
          h = text->h*scale;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
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
    SDL_FreeSurface(text);
}


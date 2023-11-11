/**
 * @brief texture information class definitions
 *
 * This file implements a class containing the associated date with a texture image.
 * It is only used by texture.cpp and shaderparam.cpp.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"

#include "imagedata.h"
#include "renderwindow.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/control.h"
#include "interface/console.h"
#include "interface/cs.h" //for stringslice

namespace
{
    //helper function for texturedata
    vec parsevec(const char *arg)
    {
        vec v(0, 0, 0);
        uint i = 0;
        for(; arg[0] && (!i || arg[0]=='/') && i<3; arg += std::strcspn(arg, "/,><"), i++)
        {
            if(i)
            {
                arg++;
            }
            v[i] = std::atof(arg);
        }
        if(i==1)
        {
            v.y = v.z = v.x;
        }
        return v;
    }

    //helper function for texturedata
    bool canloadsurface(const char *name)
    {
        stream *f = openfile(name, "rb");
        if(!f)
        {
            return false;
        }
        delete f;
        return true;
    }
}

ImageData::ImageData()
    : data(nullptr), owner(nullptr), freefunc(nullptr)
{}


ImageData::ImageData(int nw, int nh, int nbpp, int nlevels, int nalign, GLenum ncompressed)
{
    setdata(nullptr, nw, nh, nbpp, nlevels, nalign, ncompressed);
}

ImageData::~ImageData()
{
    cleanup();
}

void ImageData::setdata(uchar *ndata, int nw, int nh, int nbpp, int nlevels, int nalign, GLenum ncompressed)
{
    w = nw;
    h = nh;
    bpp = nbpp;
    levels = nlevels;
    align = nalign;
    pitch = align ? 0 : w*bpp;
    compressed = ncompressed;
    data = ndata ? ndata : new uchar[calcsize()];
    if(!ndata)
    {
        owner = this;
        freefunc = nullptr;
    }
}

int ImageData::calclevelsize(int level) const
{
    return ((std::max(w>>level, 1)+align-1)/align)*((std::max(h>>level, 1)+align-1)/align)*bpp;
}

int ImageData::calcsize() const
{
    if(!align)
    {
        return w*h*bpp;
    }
    int lw = w,
        lh = h,
        size = 0;
    for(int i = 0; i < levels; ++i)
    {
        if(lw<=0)
        {
            lw = 1;
        }
        if(lh<=0)
        {
            lh = 1;
        }
        size += ((lw+align-1)/align)*((lh+align-1)/align)*bpp;
        if(lw*lh==1)
        {
            break;
        }
        lw >>= 1;
        lh >>= 1;
    }
    return size;
}

//Clears the objects pointers that the image data points to, to allow them to be exclusively
//pointed to by another object. This is used when calling replace() such that the old object
//does not have references to the new object's data fields.
void ImageData::disown()
{
    data = nullptr;
    owner = nullptr;
    freefunc = nullptr;
}

void ImageData::cleanup()
{
    if(owner==this)
    {
        delete[] data;
    }
    else if(freefunc)
    {
        (*freefunc)(owner);
    }
    disown();
}

// Deletes the data associated with the current ImageData object
//and makes the object point to the one passed by parameter
void ImageData::replace(ImageData &d)
{
    cleanup();
    *this = d;
    if(owner == &d)
    {
        owner = this;
    }
    d.disown();
}

void ImageData::wraptex(SDL_Surface *s)
{
    setdata(static_cast<uchar *>(s->pixels), s->w, s->h, s->format->BytesPerPixel);
    pitch = s->pitch;
    owner = s;
    freefunc = reinterpret_cast<void (*)(void *)>(SDL_FreeSurface);
}

#define WRITE_TEX(t, body) do \
    { \
        uchar *dstrow = t.data; \
        for(int y = 0; y < t.h; ++y) \
        { \
            for(uchar *dst = dstrow, *end = &dstrow[t.w*t.bpp]; dst < end; dst += t.bpp) \
            { \
                body; \
            } \
            dstrow += t.pitch; \
        } \
    } while(0)

#define READ_WRITE_TEX(t, s, body) do \
    { \
        uchar *dstrow = t.data, *srcrow = s.data; \
        for(int y = 0; y < t.h; ++y) \
        { \
            for(uchar *dst = dstrow, *src = srcrow, *end = &srcrow[s.w*s.bpp]; src < end; dst += t.bpp, src += s.bpp) \
            { \
                body; \
            } \
            dstrow += t.pitch; \
            srcrow += s.pitch; \
        } \
    } while(0)

#define READ_2_WRITE_TEX(t, s1, src1, s2, src2, body) do \
    { \
        uchar *dstrow = t.data, *src1row = s1.data, *src2row = s2.data; \
        for(int y = 0; y < t.h; ++y) \
        { \
            for(uchar *dst = dstrow, *end = &dstrow[t.w*t.bpp], *src1 = src1row, *src2 = src2row; dst < end; dst += t.bpp, src1 += s1.bpp, src2 += s2.bpp) \
            { \
                body; \
            } \
            dstrow += t.pitch; \
            src1row += s1.pitch; \
            src2row += s2.pitch; \
        } \
    } while(0)

#define READ_WRITE_RGB_TEX(t, s, body) \
    { \
        if(t.bpp >= 3) \
        { \
            READ_WRITE_TEX(t, s, body); \
        } \
        else \
        { \
            ImageData rgb(t.w, t.h, 3); \
            READ_2_WRITE_TEX(rgb, t, orig, s, src, { dst[0] = dst[1] = dst[2] = orig[0]; body; }); \
            t.replace(rgb); \
        } \
    }

void ImageData::forcergbimage()
{
    if(bpp >= 3)
    {
        return;
    }
    ImageData d(w, h, 3);
    READ_WRITE_TEX(d, ((*this)), { dst[0] = dst[1] = dst[2] = src[0]; });
    replace(d);
}

#define READ_WRITE_RGBA_TEX(t, s, body) \
    { \
        if(t.bpp >= 4) \
        { \
            READ_WRITE_TEX(t, s, body); \
        } \
        else \
        { \
            ImageData rgba(t.w, t.h, 4); \
            if(t.bpp==3) \
            { \
                READ_2_WRITE_TEX(rgba, t, orig, s, src, { dst[0] = orig[0]; dst[1] = orig[1]; dst[2] = orig[2]; body; }); \
            } \
            else \
            { \
                READ_2_WRITE_TEX(rgba, t, orig, s, src, { dst[0] = dst[1] = dst[2] = orig[0]; body; }); \
            } \
            t.replace(rgba); \
        } \
    }

void ImageData::swizzleimage()
{
    if(bpp==2)
    {
        ImageData d(w, h, 4);
        READ_WRITE_TEX(d, (*this), { dst[0] = dst[1] = dst[2] = src[0]; dst[3] = src[1]; });
        replace(d);
    }
    else if(bpp==1)
    {
        ImageData d(w, h, 3);
        READ_WRITE_TEX(d, (*this), { dst[0] = dst[1] = dst[2] = src[0]; });
        replace(d);
    }
}

void ImageData::scaleimage(int w, int h)
{
    ImageData d(w, h, bpp);
    scaletexture(data, w, h, bpp, pitch, d.data, w, h);
    replace(d);
}

void ImageData::texreorient(bool flipx, bool flipy, bool swapxy, int type)
{
    ImageData d(swapxy ? h : w, swapxy ? w : h, bpp, levels, align, compressed);
    switch(compressed)
    {
        default:
            if(type == Tex_Normal && bpp >= 3)
            {
                reorientnormals(data, w, h, bpp, pitch, d.data, flipx, flipy, swapxy);
            }
            else
            {
                reorienttexture(data, w, h, bpp, pitch, d.data, flipx, flipy, swapxy);
            }
            break;
    }
    replace(d);
}

void ImageData::texrotate(int numrots, int type)
{
    if(numrots>=1 && numrots<=7)
    {
        const texrotation &r = texrotations[numrots];
        texreorient(r.flipx, r.flipy, r.swapxy, type);
    }
}

void ImageData::texoffset(int xoffset, int yoffset)
{
    xoffset = std::max(xoffset, 0);
    xoffset %= w;
    yoffset = std::max(yoffset, 0);
    yoffset %= h;
    if(!xoffset && !yoffset)
    {
        return;
    }
    ImageData d(w, h, bpp);
    uchar *src = data;
    for(int y = 0; y < h; ++y)
    {
        uchar *dst = static_cast<uchar *>(d.data)+((y+yoffset)%d.h)*d.pitch;
        std::memcpy(dst+xoffset*bpp, src, (w-xoffset)*bpp);
        std::memcpy(dst, src+(w-xoffset)*bpp, xoffset*bpp);
        src += pitch;
    }
    replace(d);
}

void ImageData::texcrop(int x, int y, int w, int h)
{
    x = std::clamp(x, 0, w);
    y = std::clamp(y, 0, h);
    w = std::min(w < 0 ? w : w, w - x);
    h = std::min(h < 0 ? h : h, h - y);
    if(!w || !h)
    {
        return;
    }
    ImageData d(w, h, bpp);
    uchar *src = data + y*pitch + x*bpp,
          *dst = d.data;
    for(int y = 0; y < h; ++y)
    {
        std::memcpy(dst, src, w*bpp);
        src += pitch;
        dst += d.pitch;
    }
    replace(d);
}

void ImageData::texmad(const vec &mul, const vec &add)
{
    if(bpp < 3 && (mul.x != mul.y || mul.y != mul.z || add.x != add.y || add.y != add.z))
    {
        swizzleimage();
    }
    int maxk = std::min(static_cast<int>(bpp), 3);
    WRITE_TEX((*this),
        for(int k = 0; k < maxk; ++k)
        {
            dst[k] = static_cast<uchar>(std::clamp(dst[k]*mul[k] + 255*add[k], 0.0f, 255.0f));
        }
    );
}

void ImageData::texcolorify(const vec &color, vec weights)
{
    if(bpp < 3)
    {
        return;
    }
    if(weights.iszero())
    {
        weights = vec(0.21f, 0.72f, 0.07f);
    }
    WRITE_TEX((*this),
        float lum = dst[0]*weights.x + dst[1]*weights.y + dst[2]*weights.z;
        for(int k = 0; k < 3; ++k)
        {
            dst[k] = static_cast<uchar>(std::clamp(lum*color[k], 0.0f, 255.0f));
        }
    );
}

void ImageData::texcolormask(const vec &color1, const vec &color2)
{
    if(bpp < 4)
    {
        return;
    }
    ImageData d(w, h, 3);
    READ_WRITE_TEX(d, (*this),
        vec color;
        color.lerp(color2, color1, src[3]/255.0f);
        for(int k = 0; k < 3; ++k)
        {
            dst[k] = static_cast<uchar>(std::clamp(color[k]*src[k], 0.0f, 255.0f));
        }
    );
    replace(d);
}

void ImageData::texdup(int srcchan, int dstchan)
{
    if(srcchan==dstchan || std::max(srcchan, dstchan) >= bpp)
    {
        return;
    }
    WRITE_TEX((*this), dst[dstchan] = dst[srcchan]);
}

void ImageData::texmix(int c1, int c2, int c3, int c4)
{
    int numchans = c1 < 0 ? 0 : (c2 < 0 ? 1 : (c3 < 0 ? 2 : (c4 < 0 ? 3 : 4)));
    if(numchans <= 0)
    {
        return;
    }
    ImageData d(w, h, numchans);
    READ_WRITE_TEX(d, (*this),
        switch(numchans)
        {
            case 4:
            {
                dst[3] = src[c4];
                break;
            }
            case 3:
            {
                dst[2] = src[c3];
                break;
            }
            case 2:
            {
                dst[1] = src[c2];
                break;
            }
            case 1:
            {
                dst[0] = src[c1];
                break;
            }
        }
    );
    replace(d);
}

void ImageData::texgrey()
{
    if(bpp <= 2)
    {
        return;
    }
    ImageData d(w, h, bpp >= 4 ? 2 : 1);
    if(bpp >= 4)
    {
        READ_WRITE_TEX(d, (*this),
            dst[0] = src[0];
            dst[1] = src[3];
        );
    }
    else
    {
        READ_WRITE_TEX(d, (*this), dst[0] = src[0]);
    }
    replace(d);
}

void ImageData::texpremul()
{
    switch(bpp)
    {
        case 2:
            WRITE_TEX((*this),
                dst[0] = static_cast<uchar>((static_cast<uint>(dst[0])*static_cast<uint>(dst[1]))/255);
            );
            break;
        case 4:
            WRITE_TEX((*this),
                uint alpha = dst[3];
                dst[0] = static_cast<uchar>((static_cast<uint>(dst[0])*alpha)/255);
                dst[1] = static_cast<uchar>((static_cast<uint>(dst[1])*alpha)/255);
                dst[2] = static_cast<uchar>((static_cast<uint>(dst[2])*alpha)/255);
            );
            break;
    }
}

void ImageData::texagrad(float x2, float y2, float x1, float y1)
{
    if(bpp != 2 && bpp != 4)
    {
        return;
    }
    y1 = 1 - y1;
    y2 = 1 - y2;
    float minx = 1,
          miny = 1,
          maxx = 1,
          maxy = 1;
    if(x1 != x2)
    {
        minx = (0 - x1) / (x2 - x1);
        maxx = (1 - x1) / (x2 - x1);
    }
    if(y1 != y2)
    {
        miny = (0 - y1) / (y2 - y1);
        maxy = (1 - y1) / (y2 - y1);
    }
    float dx = (maxx - minx)/std::max(w-1, 1),
          dy = (maxy - miny)/std::max(h-1, 1),
          cury = miny;
    for(uchar *dstrow = data + bpp - 1, *endrow = dstrow + h*pitch; dstrow < endrow; dstrow += pitch)
    {
        float curx = minx;
        for(uchar *dst = dstrow, *end = &dstrow[w*bpp]; dst < end; dst += bpp)
        {
            dst[0] = static_cast<uchar>(dst[0]*std::clamp(curx, 0.0f, 1.0f)*std::clamp(cury, 0.0f, 1.0f));
            curx += dx;
        }
        cury += dy;
    }
}

void ImageData::texblend(const ImageData &s0, const ImageData &m0)
{
    ImageData s(s0),
              m(m0);
    if(s.w != w || s.h != h)
    {
        s.scaleimage(w, h);
    }
    if(m.w != w || m.h != h)
    {
        m.scaleimage(w, h);
    }
    if(&s == &m)
    {
        if(s.bpp == 2)
        {
            if(bpp >= 3)
            {
                s.swizzleimage();
            }
        }
        else if(s.bpp == 4)
        {
            if(bpp < 3)
            {
                swizzleimage();
            }
        }
        else
        {
            return;
        }
        //need to declare int for each var because it's inside a macro body
        if(bpp < 3)
        {
            READ_WRITE_TEX((*this), s,
                int srcblend = src[1];
                int dstblend = 255 - srcblend;
                dst[0] = static_cast<uchar>((dst[0]*dstblend + src[0]*srcblend)/255);
            );
        }
        else
        {
            READ_WRITE_TEX((*this), s,
                int srcblend = src[3];
                int dstblend = 255 - srcblend;
                dst[0] = static_cast<uchar>((dst[0]*dstblend + src[0]*srcblend)/255);
                dst[1] = static_cast<uchar>((dst[1]*dstblend + src[1]*srcblend)/255);
                dst[2] = static_cast<uchar>((dst[2]*dstblend + src[2]*srcblend)/255);
            );
        }
    }
    else
    {
        if(s.bpp < 3)
        {
            if(bpp >= 3)
            {
                s.swizzleimage();
            }
        }
        else if(bpp < 3)
        {
            swizzleimage();
        }
        if(bpp < 3)
        {
            READ_2_WRITE_TEX((*this), s, src, m, mask,
                int srcblend = mask[0];
                int dstblend = 255 - srcblend;
                dst[0] = static_cast<uchar>((dst[0]*dstblend + src[0]*srcblend)/255);
            );
        }
        else
        {
            READ_2_WRITE_TEX((*this), s, src, m, mask,
                int srcblend = mask[0];
                int dstblend = 255 - srcblend;
                dst[0] = static_cast<uchar>((dst[0]*dstblend + src[0]*srcblend)/255);
                dst[1] = static_cast<uchar>((dst[1]*dstblend + src[1]*srcblend)/255);
                dst[2] = static_cast<uchar>((dst[2]*dstblend + src[2]*srcblend)/255);
            );
        }
    }
}

void ImageData::addglow(const ImageData &g, const vec &glowcolor)
{
    if(g.bpp < 3)
    {
        READ_WRITE_RGB_TEX((*this), g,
            for(int k = 0; k < 3; ++k)
            {
                dst[k] = std::clamp(static_cast<int>(dst[k]) + static_cast<int>(src[0]*glowcolor[k]), 0, 255);
            }
        );
    }
    else
    {
        READ_WRITE_RGB_TEX((*this), g,
            for(int k = 0; k < 3; ++k)
            {
                dst[k] = std::clamp(static_cast<int>(dst[k]) + static_cast<int>(src[k]*glowcolor[k]), 0, 255);
            }
        );
    }
}

void ImageData::mergespec(const ImageData &s)
{
    if(s.bpp < 3)
    {
        READ_WRITE_RGBA_TEX((*this), s,
            dst[3] = src[0];
        );
    }
    else
    {
        READ_WRITE_RGBA_TEX((*this), s,
            dst[3] = (static_cast<int>(src[0]) + static_cast<int>(src[1]) + static_cast<int>(src[2]))/3;
        );
    }
}

void ImageData::mergedepth(const ImageData &z)
{
    READ_WRITE_RGBA_TEX((*this), z,
        dst[3] = src[0];
    );
}

void ImageData::mergealpha(const ImageData &s)
{
    if(s.bpp < 3)
    {
        READ_WRITE_RGBA_TEX((*this), s,
            dst[3] = src[0];
        );
    }
    else
    {
        READ_WRITE_RGBA_TEX((*this), s,
            dst[3] = src[3];
        );
    }
}

void ImageData::collapsespec()
{
    ImageData d(w, h, 1);
    if(bpp >= 3)
    {
        READ_WRITE_TEX(d, (*this),
        {
            dst[0] = (static_cast<int>(src[0]) + static_cast<int>(src[1]) + static_cast<int>(src[2]))/3;
        });
    }
    else
    {
        READ_WRITE_TEX(d, (*this), { dst[0] = src[0]; });
    }
    replace(d);
}

void ImageData::texnormal(int emphasis)
{
    ImageData d(w, h, 3);
    uchar *src = data,
          *dst = d.data;
    for(int y = 0; y < h; ++y)
    {
        for(int x = 0; x < w; ++x)
        {
            vec normal(0.0f, 0.0f, 255.0f/emphasis);
            normal.x += src[y*pitch + ((x+w-1)%w)*bpp];
            normal.x -= src[y*pitch + ((x+1)%w)*bpp];
            normal.y += src[((y+h-1)%h)*pitch + x*bpp];
            normal.y -= src[((y+1)%h)*pitch + x*bpp];
            normal.normalize();
            *(dst++) = static_cast<uchar>(127.5f + normal.x*127.5f);
            *(dst++) = static_cast<uchar>(127.5f + normal.y*127.5f);
            *(dst++) = static_cast<uchar>(127.5f + normal.z*127.5f);
        }
    }
    replace(d);
}

bool ImageData::texturedata(const char *tname, bool msg, int * const compress, int * const wrap, const char *tdir, int ttype)
{
    auto parsetexcommands = [] (const char *&cmds, const char *&cmd, size_t &len, std::array<const char *, 4> &arg)
    {
        const char *end = nullptr;
        cmd = &cmds[1];
        end = std::strchr(cmd, '>');
        if(!end)
        {
            return true;
        }
        cmds = std::strchr(cmd, '<');
        len = std::strcspn(cmd, ":,><");
        for(int i = 0; i < 4; ++i)
        {
            arg[i] = std::strchr(i ? arg[i-1] : cmd, i ? ',' : ':');
            if(!arg[i] || arg[i] >= end)
            {
                arg[i] = "";
            }
            else
            {
                arg[i]++;
            }
        }
        return false;
    };

    const char *cmds = nullptr,
               *file = tname;
    if(tname[0]=='<')
    {
        cmds = tname;
        file = std::strrchr(tname, '>');
        if(!file)
        {
            if(msg)
            {
                conoutf(Console_Error, "could not load <> modified texture %s", tname);
            }
            return false;
        }
        file++;
    }
    string pname;
    if(tdir)
    {
        formatstring(pname, "%s/%s", tdir, file);
        file = path(pname);
    }
    for(const char *pcmds = cmds; pcmds;)
    {
        const char *cmd = nullptr;
        size_t len = 0;
        std::array<const char *, 4> arg = { nullptr, nullptr, nullptr, nullptr };

        if(parsetexcommands(pcmds, cmd, len, arg))
        {
            break;
        }

        if(matchstring(cmd, len, "stub"))
        {
            return canloadsurface(file);
        }
    }
    if(msg)
    {
        renderprogress(loadprogress, file);
    }
    if(!data)
    {
        SDL_Surface *s = loadsurface(file);
        if(!s)
        {
            if(msg)
            {
                conoutf(Console_Error, "could not load texture %s", file);
            }
            return false;
        }
        int bpp = s->format->BitsPerPixel;
        if(bpp%8 || !texformat(bpp/8))
        {
            SDL_FreeSurface(s); conoutf(Console_Error, "texture must be 8, 16, 24, or 32 bpp: %s", file);
            return false;
        }
        if(std::max(s->w, s->h) > (1<<12))
        {
            SDL_FreeSurface(s); conoutf(Console_Error, "texture size exceeded %dx%d pixels: %s", 1<<12, 1<<12, file);
            return false;
        }
        wraptex(s);
    }

    while(cmds)
    {
        const char *cmd = nullptr;
        size_t len = 0;
        std::array<const char *, 4> arg = { nullptr, nullptr, nullptr, nullptr };

        if(parsetexcommands(cmds, cmd, len, arg))
        {
            break;
        }

        if(compressed)
        {
            goto compressed; //see `compressed` nested between else & if (yes it's ugly)
        }
        if(matchstring(cmd, len, "mad"))
        {
            texmad(parsevec(arg[0]), parsevec(arg[1]));
        }
        else if(matchstring(cmd, len, "colorify"))
        {
            texcolorify(parsevec(arg[0]), parsevec(arg[1]));
        }
        else if(matchstring(cmd, len, "colormask"))
        {
            texcolormask(parsevec(arg[0]), *arg[1] ? parsevec(arg[1]) : vec(1, 1, 1));
        }
        else if(matchstring(cmd, len, "normal"))
        {
            int emphasis = std::atoi(arg[0]);
            texnormal(emphasis > 0 ? emphasis : 3);
        }
        else if(matchstring(cmd, len, "dup"))
        {
            texdup(std::atoi(arg[0]), std::atoi(arg[1]));
        }
        else if(matchstring(cmd, len, "offset"))
        {
            texoffset(std::atoi(arg[0]), std::atoi(arg[1]));
        }
        else if(matchstring(cmd, len, "rotate"))
        {
            texrotate(std::atoi(arg[0]), ttype);
        }
        else if(matchstring(cmd, len, "reorient"))
        {
            texreorient(std::atoi(arg[0])>0, std::atoi(arg[1])>0, std::atoi(arg[2])>0, ttype);
        }
        else if(matchstring(cmd, len, "crop"))
        {
            texcrop(std::atoi(arg[0]), std::atoi(arg[1]), *arg[2] ? std::atoi(arg[2]) : -1, *arg[3] ? std::atoi(arg[3]) : -1);
        }
        else if(matchstring(cmd, len, "mix"))
        {
            texmix(*arg[0] ? std::atoi(arg[0]) : -1, *arg[1] ? std::atoi(arg[1]) : -1, *arg[2] ? std::atoi(arg[2]) : -1, *arg[3] ? std::atoi(arg[3]) : -1);
        }
        else if(matchstring(cmd, len, "grey"))
        {
            texgrey();
        }
        else if(matchstring(cmd, len, "premul"))
        {
            texpremul();
        }
        else if(matchstring(cmd, len, "agrad"))
        {
            texagrad(std::atof(arg[0]), std::atof(arg[1]), std::atof(arg[2]), std::atof(arg[3]));
        }
        else if(matchstring(cmd, len, "blend"))
        {
            ImageData src, mask;
            string srcname, maskname;
            copystring(srcname, stringslice(arg[0], std::strcspn(arg[0], ":,><")));
            copystring(maskname, stringslice(arg[1], std::strcspn(arg[1], ":,><")));
            if(srcname[0] && src.texturedata(srcname, false, nullptr, nullptr, tdir, ttype) && (!maskname[0] || mask.texturedata(maskname, false, nullptr, nullptr, tdir, ttype)))
            {
                texblend(src, maskname[0] ? mask : src);
            }
        }
        else if(matchstring(cmd, len, "thumbnail"))
        {
            int xdim = std::atoi(arg[0]),
                ydim = std::atoi(arg[1]);
            if(xdim <= 0 || xdim > (1<<12))
            {
                xdim = 64;
            }
            if(ydim <= 0 || ydim > (1<<12))
            {
                ydim = xdim; //default 64
            }
            if(w > xdim || h > ydim)
            {
                scaleimage(xdim, ydim);
            }
        }
        else if(matchstring(cmd, len, "compress"))
        {
            int scale = std::atoi(arg[0]);
            if(compress)
            {
                *compress = scale;
            }
        }
        else if(matchstring(cmd, len, "nocompress"))
        {
            if(compress)
            {
                *compress = -1;
            }
        }
        // note that the else/if in else-if is separated by a goto breakpoint
        else
    compressed:
        if(matchstring(cmd, len, "mirror"))
        {
            if(wrap)
            {
                *wrap |= 0x300;
            }
        }
        else if(matchstring(cmd, len, "noswizzle"))
        {
            if(wrap)
            {
                *wrap |= 0x10000;
            }
        }
    }
    return true;
}

bool ImageData::texturedata(Slot &slot, Slot::Tex &tex, bool msg, int *compress, int *wrap)
{
    return texturedata(tex.name, msg, compress, wrap, slot.texturedir(), tex.type);
}

void ImageData::reorientnormals(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
{
    int stridex = bpp,
        stridey = bpp;
    if(swapxy)
    {
        stridex *= sh;
    }
    else
    {
        stridey *= sw;
    }
    if(flipx)
    {
        dst += (sw-1)*stridex;
        stridex = -stridex;
    }
    if(flipy)
    {
        dst += (sh-1)*stridey;
        stridey = -stridey;
    }
    uchar *srcrow = src;
    for(int i = 0; i < sh; ++i)
    {
        for(uchar *curdst = dst, *src = srcrow, *end = &srcrow[sw*bpp]; src < end;)
        {
            uchar nx = *src++, ny = *src++;
            if(flipx)
            {
                nx = 255-nx;
            }
            if(flipy)
            {
                ny = 255-ny;
            }
            if(swapxy)
            {
                std::swap(nx, ny);
            }
            curdst[0] = nx;
            curdst[1] = ny;
            curdst[2] = *src++;
            if(bpp > 3)
            {
                curdst[3] = *src++;
            }
            curdst += stridex;
        }
        srcrow += stride;
        dst += stridey;
    }
}

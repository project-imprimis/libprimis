// texture.cpp: texture slot management

#include "../libprimis-headers/cube.h"
#include "../../shared/glexts.h"

#include "SDL_image.h"

#include "octarender.h"
#include "renderwindow.h"
#include "texture.h"

#include "world/light.h"
#include "world/material.h"
#include "world/octaedit.h"
#include "world/octaworld.h"
#include "world/world.h"

#include "interface/console.h"
#include "interface/control.h"

template<int BPP>
static void halvetexture(uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst)
{
    for(uchar *yend = &src[sh*stride]; src < yend;)
    {
        for(uchar *xend = &src[sw*BPP], *xsrc = src; xsrc < xend; xsrc += 2*BPP, dst += BPP)
        {
            for(int i = 0; i < BPP; ++i)
            {
                dst[i] = (static_cast<uint>(xsrc[i]) + static_cast<uint>(xsrc[i+BPP]) + static_cast<uint>(xsrc[stride+i]) + static_cast<uint>(xsrc[stride+i+BPP]))>>2;
            }
        }
        src += 2*stride;
    }
}

template<int BPP>
static void shifttexture(uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst, uint dw, uint dh)
{
    uint wfrac = sw/dw,
         hfrac = sh/dh,
         wshift = 0,
         hshift = 0;
    while(dw<<wshift < sw)
    {
        wshift++;
    }
    while(dh<<hshift < sh)
    {
        hshift++;
    }
    uint tshift = wshift + hshift;
    for(uchar *yend = &src[sh*stride]; src < yend;)
    {
        for(uchar *xend = &src[sw*BPP], *xsrc = src; xsrc < xend; xsrc += wfrac*BPP, dst += BPP)
        {
            uint t[BPP] = {0};
            for(uchar *ycur = xsrc, *xend = &ycur[wfrac*BPP], *yend = &src[hfrac*stride];
                ycur < yend;
                ycur += stride, xend += stride)
            {
                for(uchar *xcur = ycur; xcur < xend; xcur += BPP)
                    for(int i = 0; i < BPP; ++i)
                    {
                        t[i] += xcur[i];
                    }
            }
            for(int i = 0; i < BPP; ++i)
            {
                dst[i] = t[i] >> tshift;
            }
        }
        src += hfrac*stride;
    }
}

template<int BPP>
static void scaletexture(uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst, uint dw, uint dh)
{
    uint wfrac = (sw<<12)/dw,
         hfrac = (sh<<12)/dh,
         darea = dw*dh,
         sarea = sw*sh;
    int over, under;
    //the for loops here are merely to increment over & under vars which are used later
    for(over = 0; (darea>>over) > sarea; over++)
    {
        //(empty body)
    }
    for(under = 0; (darea<<under) < sarea; under++)
    {
        //(empty body)
    }
    uint cscale = std::clamp(under, over - 12, 12),
         ascale = std::clamp(12 + under - over, 0, 24),
         dscale = ascale + 12 - cscale,
         area = (static_cast<ullong>(darea)<<ascale)/sarea;
    dw *= wfrac;
    dh *= hfrac;
    for(uint y = 0; y < dh; y += hfrac)
    {
        const uint yn = y + hfrac - 1,
                   yi = y>>12, h = (yn>>12) - yi,
                   ylow = ((yn|(-static_cast<int>(h)>>24))&0xFFFU) + 1 - (y&0xFFFU),
                   yhigh = (yn&0xFFFU) + 1;
        const uchar *ysrc = &src[yi*stride];
        for(uint x = 0; x < dw; x += wfrac, dst += BPP)
        {
            const uint xn = x + wfrac - 1,
                       xi = x>>12,
                       w = (xn>>12) - xi,
                       xlow = ((w+0xFFFU)&0x1000U) - (x&0xFFFU),
                       xhigh = (xn&0xFFFU) + 1;
            const uchar *xsrc = &ysrc[xi*BPP],
                        *xend = &xsrc[w*BPP];
            uint t[BPP] = {0};
            for(const uchar *xcur = &xsrc[BPP]; xcur < xend; xcur += BPP)
            {
                for(int i = 0; i < BPP; ++i)
                {
                    t[i] += xcur[i];
                }
            }
            for(int i = 0; i < BPP; ++i)
            {
                t[i] = (ylow*(t[i] + ((xsrc[i]*xlow + xend[i]*xhigh)>>12)))>>cscale;
            }
            if(h)
            {
                xsrc += stride;
                xend += stride;
                for(uint hcur = h; --hcur; xsrc += stride, xend += stride)
                {
                    uint c[BPP] = {0};
                    for(const uchar *xcur = &xsrc[BPP]; xcur < xend; xcur += BPP)
                        for(int i = 0; i < BPP; ++i)
                        {
                            c[i] += xcur[i];
                        }
                    for(int i = 0; i < BPP; ++i)
                    {
                        t[i] += ((c[i]<<12) + xsrc[i]*xlow + xend[i]*xhigh)>>cscale;
                    }
                }
                uint c[BPP] = {0};
                for(const uchar *xcur = &xsrc[BPP]; xcur < xend; xcur += BPP)
                    for(int i = 0; i < BPP; ++i)
                    {
                        c[i] += xcur[i];
                    }
                for(int i = 0; i < BPP; ++i)
                {
                    t[i] += (yhigh*(c[i] + ((xsrc[i]*xlow + xend[i]*xhigh)>>12)))>>cscale;
                }
            }
            for(int i = 0; i < BPP; ++i)
            {
                dst[i] = (t[i] * area)>>dscale;
            }
        }
    }
}

static void scaletexture(uchar * RESTRICT src, uint sw, uint sh, uint bpp, uint pitch, uchar * RESTRICT dst, uint dw, uint dh)
{
    if(sw == dw*2 && sh == dh*2)
    {
        switch(bpp)
        {
            case 1: return halvetexture<1>(src, sw, sh, pitch, dst);
            case 2: return halvetexture<2>(src, sw, sh, pitch, dst);
            case 3: return halvetexture<3>(src, sw, sh, pitch, dst);
            case 4: return halvetexture<4>(src, sw, sh, pitch, dst);
        }
    }
    else if(sw < dw || sh < dh || sw&(sw-1) || sh&(sh-1) || dw&(dw-1) || dh&(dh-1))
    {
        switch(bpp)
        {
            case 1: return scaletexture<1>(src, sw, sh, pitch, dst, dw, dh);
            case 2: return scaletexture<2>(src, sw, sh, pitch, dst, dw, dh);
            case 3: return scaletexture<3>(src, sw, sh, pitch, dst, dw, dh);
            case 4: return scaletexture<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    }
    else
    {
        switch(bpp)
        {
            case 1: return shifttexture<1>(src, sw, sh, pitch, dst, dw, dh);
            case 2: return shifttexture<2>(src, sw, sh, pitch, dst, dw, dh);
            case 3: return shifttexture<3>(src, sw, sh, pitch, dst, dw, dh);
            case 4: return shifttexture<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    }
}

static void reorientnormals(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
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

template<int BPP>
static void reorienttexture(uchar * RESTRICT src, int sw, int sh, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
{
    int stridex = BPP,
        stridey = BPP;
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
        for(uchar *curdst = dst, *src = srcrow, *end = &srcrow[sw*BPP]; src < end;)
        {
            for(int k = 0; k < BPP; ++k)
            {
                curdst[k] = *src++;
            }
            curdst += stridex;
        }
        srcrow += stride;
        dst += stridey;
    }
}

static void reorienttexture(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
{
    switch(bpp)
    {
        case 1: return reorienttexture<1>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 2: return reorienttexture<2>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 3: return reorienttexture<3>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 4: return reorienttexture<4>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
    }
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

static void forcergbimage(ImageData &s)
{
    if(s.bpp >= 3)
    {
        return;
    }
    ImageData d(s.w, s.h, 3);
    READ_WRITE_TEX(d, s, { dst[0] = dst[1] = dst[2] = src[0]; });
    s.replace(d);
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

static void swizzleimage(ImageData &s)
{
    if(s.bpp==2)
    {
        ImageData d(s.w, s.h, 4);
        READ_WRITE_TEX(d, s, { dst[0] = dst[1] = dst[2] = src[0]; dst[3] = src[1]; });
        s.replace(d);
    }
    else if(s.bpp==1)
    {
        ImageData d(s.w, s.h, 3);
        READ_WRITE_TEX(d, s, { dst[0] = dst[1] = dst[2] = src[0]; });
        s.replace(d);
    }
}

static void scaleimage(ImageData &s, int w, int h)
{
    ImageData d(w, h, s.bpp);
    scaletexture(s.data, s.w, s.h, s.bpp, s.pitch, d.data, w, h);
    s.replace(d);
}

static void texreorient(ImageData &s, bool flipx, bool flipy, bool swapxy, int type = Tex_Diffuse)
{
    ImageData d(swapxy ? s.h : s.w, swapxy ? s.w : s.h, s.bpp, s.levels, s.align, s.compressed);
    switch(s.compressed)
    {
        default:
            if(type == Tex_Normal && s.bpp >= 3)
            {
                reorientnormals(s.data, s.w, s.h, s.bpp, s.pitch, d.data, flipx, flipy, swapxy);
            }
            else
            {
                reorienttexture(s.data, s.w, s.h, s.bpp, s.pitch, d.data, flipx, flipy, swapxy);
            }
            break;
    }
    s.replace(d);
}

extern const texrotation texrotations[8] =
{
    { false, false, false }, // 0: default
    { false,  true,  true }, // 1: 90 degrees
    {  true,  true, false }, // 2: 180 degrees
    {  true, false,  true }, // 3: 270 degrees
    {  true, false, false }, // 4: flip X
    { false,  true, false }, // 5: flip Y
    { false, false,  true }, // 6: transpose
    {  true,  true,  true }, // 7: flipped transpose
};

static void texrotate(ImageData &s, int numrots, int type = Tex_Diffuse)
{
    if(numrots>=1 && numrots<=7)
    {
        const texrotation &r = texrotations[numrots];
        texreorient(s, r.flipx, r.flipy, r.swapxy, type);
    }
}

static void texoffset(ImageData &s, int xoffset, int yoffset)
{
    xoffset = std::max(xoffset, 0);
    xoffset %= s.w;
    yoffset = std::max(yoffset, 0);
    yoffset %= s.h;
    if(!xoffset && !yoffset)
    {
        return;
    }
    ImageData d(s.w, s.h, s.bpp);
    uchar *src = s.data;
    for(int y = 0; y < s.h; ++y)
    {
        uchar *dst = static_cast<uchar *>(d.data)+((y+yoffset)%d.h)*d.pitch;
        memcpy(dst+xoffset*s.bpp, src, (s.w-xoffset)*s.bpp);
        memcpy(dst, src+(s.w-xoffset)*s.bpp, xoffset*s.bpp);
        src += s.pitch;
    }
    s.replace(d);
}

static void texcrop(ImageData &s, int x, int y, int w, int h)
{
    x = std::clamp(x, 0, s.w);
    y = std::clamp(y, 0, s.h);
    w = std::min(w < 0 ? s.w : w, s.w - x);
    h = std::min(h < 0 ? s.h : h, s.h - y);
    if(!w || !h)
    {
        return;
    }
    ImageData d(w, h, s.bpp);
    uchar *src = s.data + y*s.pitch + x*s.bpp,
          *dst = d.data;
    for(int y = 0; y < h; ++y)
    {
        memcpy(dst, src, w*s.bpp);
        src += s.pitch;
        dst += d.pitch;
    }
    s.replace(d);
}

void texmad(ImageData &s, const vec &mul, const vec &add)
{
    if(s.bpp < 3 && (mul.x != mul.y || mul.y != mul.z || add.x != add.y || add.y != add.z))
    {
        swizzleimage(s);
    }
    int maxk = std::min(static_cast<int>(s.bpp), 3);
    WRITE_TEX(s,
        for(int k = 0; k < maxk; ++k)
        {
            dst[k] = static_cast<uchar>(std::clamp(dst[k]*mul[k] + 255*add[k], 0.0f, 255.0f));
        }
    );
}

void texcolorify(ImageData &s, const vec &color, vec weights)
{
    if(s.bpp < 3)
    {
        return;
    }
    if(weights.iszero())
    {
        weights = vec(0.21f, 0.72f, 0.07f);
    }
    WRITE_TEX(s,
        float lum = dst[0]*weights.x + dst[1]*weights.y + dst[2]*weights.z;
        for(int k = 0; k < 3; ++k)
        {
            dst[k] = static_cast<uchar>(std::clamp(lum*color[k], 0.0f, 255.0f));
        }
    );
}

void texcolormask(ImageData &s, const vec &color1, const vec &color2)
{
    if(s.bpp < 4)
    {
        return;
    }
    ImageData d(s.w, s.h, 3);
    READ_WRITE_TEX(d, s,
        vec color;
        color.lerp(color2, color1, src[3]/255.0f);
        for(int k = 0; k < 3; ++k)
        {
            dst[k] = static_cast<uchar>(std::clamp(color[k]*src[k], 0.0f, 255.0f));
        }
    );
    s.replace(d);
}

static void texdup(ImageData &s, int srcchan, int dstchan)
{
    if(srcchan==dstchan || std::max(srcchan, dstchan) >= s.bpp)
    {
        return;
    }
    WRITE_TEX(s, dst[dstchan] = dst[srcchan]);
}

static void texmix(ImageData &s, int c1, int c2, int c3, int c4)
{
    int numchans = c1 < 0 ? 0 : (c2 < 0 ? 1 : (c3 < 0 ? 2 : (c4 < 0 ? 3 : 4)));
    if(numchans <= 0)
    {
        return;
    }
    ImageData d(s.w, s.h, numchans);
    READ_WRITE_TEX(d, s,
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
    s.replace(d);
}

static void texgrey(ImageData &s)
{
    if(s.bpp <= 2)
    {
        return;
    }
    ImageData d(s.w, s.h, s.bpp >= 4 ? 2 : 1);
    if(s.bpp >= 4)
    {
        READ_WRITE_TEX(d, s,
            dst[0] = src[0];
            dst[1] = src[3];
        );
    }
    else
    {
        READ_WRITE_TEX(d, s, dst[0] = src[0]);
    }
    s.replace(d);
}

static void texpremul(ImageData &s)
{
    switch(s.bpp)
    {
        case 2:
            WRITE_TEX(s,
                dst[0] = static_cast<uchar>((static_cast<uint>(dst[0])*static_cast<uint>(dst[1]))/255);
            );
            break;
        case 4:
            WRITE_TEX(s,
                uint alpha = dst[3];
                dst[0] = static_cast<uchar>((static_cast<uint>(dst[0])*alpha)/255);
                dst[1] = static_cast<uchar>((static_cast<uint>(dst[1])*alpha)/255);
                dst[2] = static_cast<uchar>((static_cast<uint>(dst[2])*alpha)/255);
            );
            break;
    }
}

static void texagrad(ImageData &s, float x2, float y2, float x1, float y1)
{
    if(s.bpp != 2 && s.bpp != 4)
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
    float dx = (maxx - minx)/std::max(s.w-1, 1),
          dy = (maxy - miny)/std::max(s.h-1, 1),
          cury = miny;
    for(uchar *dstrow = s.data + s.bpp - 1, *endrow = dstrow + s.h*s.pitch; dstrow < endrow; dstrow += s.pitch)
    {
        float curx = minx;
        for(uchar *dst = dstrow, *end = &dstrow[s.w*s.bpp]; dst < end; dst += s.bpp)
        {
            dst[0] = static_cast<uchar>(dst[0]*std::clamp(curx, 0.0f, 1.0f)*std::clamp(cury, 0.0f, 1.0f));
            curx += dx;
        }
        cury += dy;
    }
}

static void texblend(ImageData &d, ImageData &s, ImageData &m)
{
    if(s.w != d.w || s.h != d.h)
    {
        scaleimage(s, d.w, d.h);
    }
    if(m.w != d.w || m.h != d.h)
    {
        scaleimage(m, d.w, d.h);
    }
    if(&s == &m)
    {
        if(s.bpp == 2)
        {
            if(d.bpp >= 3)
            {
                swizzleimage(s);
            }
        }
        else if(s.bpp == 4)
        {
            if(d.bpp < 3)
            {
                swizzleimage(d);
            }
        }
        else
        {
            return;
        }
        //need to declare int for each var because it's inside a macro body
        if(d.bpp < 3) READ_WRITE_TEX(d, s,
            int srcblend = src[1];
            int dstblend = 255 - srcblend;
            dst[0] = static_cast<uchar>((dst[0]*dstblend + src[0]*srcblend)/255);
        );
        else READ_WRITE_TEX(d, s,
            int srcblend = src[3];
            int dstblend = 255 - srcblend;
            dst[0] = static_cast<uchar>((dst[0]*dstblend + src[0]*srcblend)/255);
            dst[1] = static_cast<uchar>((dst[1]*dstblend + src[1]*srcblend)/255);
            dst[2] = static_cast<uchar>((dst[2]*dstblend + src[2]*srcblend)/255);
        );
    }
    else
    {
        if(s.bpp < 3)
        {
            if(d.bpp >= 3)
            {
                swizzleimage(s);
            }
        }
        else if(d.bpp < 3)
        {
            swizzleimage(d);
        }
        if(d.bpp < 3)
        {
            READ_2_WRITE_TEX(d, s, src, m, mask,
                int srcblend = mask[0];
                int dstblend = 255 - srcblend;
                dst[0] = static_cast<uchar>((dst[0]*dstblend + src[0]*srcblend)/255);
            );
        }
        else
        {
            READ_2_WRITE_TEX(d, s, src, m, mask,
                int srcblend = mask[0];
                int dstblend = 255 - srcblend;
                dst[0] = static_cast<uchar>((dst[0]*dstblend + src[0]*srcblend)/255);
                dst[1] = static_cast<uchar>((dst[1]*dstblend + src[1]*srcblend)/255);
                dst[2] = static_cast<uchar>((dst[2]*dstblend + src[2]*srcblend)/255);
            );
        }
    }
}
/*  var             min  default max  */
VAR(hwtexsize,      1,   0,      0);
VAR(hwcubetexsize,  1,   0,      0);
VAR(hwmaxaniso,     1,   0,      0);
VAR(hwtexunits,     1,   0,      0);
VAR(hwvtexunits,    1,   0,      0);
VARFP(maxtexsize,   0,   0,      1<<12, initwarning("texture quality",   Init_Load));
VARFP(reducefilter, 0,   1,      1,     initwarning("texture quality",   Init_Load));
VARF(trilinear,     0,   1,      1,     initwarning("texture filtering", Init_Load));
VARF(bilinear,      0,   1,      1,     initwarning("texture filtering", Init_Load));
VARFP(aniso,        0,   0,      16,    initwarning("texture filtering", Init_Load));

int formatsize(GLenum format)
{
    switch(format)
    {
        case GL_RED:
        case GL_LUMINANCE:
        case GL_ALPHA:
        {
            return 1;
        }
        case GL_RG:
        case GL_LUMINANCE_ALPHA:
        {
            return 2;
        }
        case GL_RGB:
        {
            return 3;
        }
        case GL_RGBA:
        {
            return 4;
        }
        default:
        {
            return 4;
        }
    }
}

static void resizetexture(int w, int h, bool mipmap, bool canreduce, GLenum target, int compress, int &tw, int &th)
{
    int hwlimit = target==GL_TEXTURE_CUBE_MAP ? hwcubetexsize : hwtexsize,
        sizelimit = mipmap && maxtexsize ? std::min(maxtexsize, hwlimit) : hwlimit;
    if(compress > 0)
    {
        w = std::max(w/compress, 1);
        h = std::max(h/compress, 1);
    }
    w = std::min(w, sizelimit);
    h = std::min(h, sizelimit);
    if(target!=GL_TEXTURE_RECTANGLE && (w&(w-1) || h&(h-1)))
    {
        tw = th = 1;
        while(tw < w)
        {
            tw *= 2;
        }
        while(th < h)
        {
            th *= 2;
        }
        if(w < tw - tw/4)
        {
            tw /= 2;
        }
        if(h < th - th/4)
        {
            th /= 2;
        }
    }
    else
    {
        tw = w;
        th = h;
    }
}

static int texalign(const void *data, int w, int bpp)
{
    int stride = w*bpp;
    if(stride&1)
    {
        return 1;
    }
    if(stride&2)
    {
        return 2;
    }
    return 4;
}

static void uploadtexture(GLenum target, GLenum internal, int tw, int th, GLenum format, GLenum type, const void *pixels, int pw, int ph, int pitch, bool mipmap)
{
    int bpp = formatsize(format),
        row = 0,
        rowalign = 0;
    if(!pitch)
    {
        pitch = pw*bpp;
    }
    uchar *buf = nullptr;
    if(pw!=tw || ph!=th)
    {
        buf = new uchar[tw*th*bpp];
        scaletexture(static_cast<uchar *>(const_cast<void *>(pixels)), pw, ph, bpp, pitch, buf, tw, th);
    }
    else if(tw*bpp != pitch)
    {
        row = pitch/bpp;
        rowalign = texalign(pixels, pitch, 1);
        while(rowalign > 0 && ((row*bpp + rowalign - 1)/rowalign)*rowalign != pitch)
        {
            rowalign >>= 1;
        }
        if(!rowalign)
        {
            row = 0;
            buf = new uchar[tw*th*bpp];
            for(int i = 0; i < th; ++i)
            {
                memcpy(&buf[i*tw*bpp], &(const_cast<uchar *>(reinterpret_cast<const uchar *>(pixels)))[i*pitch], tw*bpp);
            }
        }
    }
    for(int level = 0, align = 0;; level++)
    {
        uchar *src = buf ? buf : const_cast<uchar *>(reinterpret_cast<const uchar *>(pixels));
        if(buf)
        {
            pitch = tw*bpp;
        }
        int srcalign = row > 0 ? rowalign : texalign(src, pitch, 1);
        if(align != srcalign)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, align = srcalign);
        }
        if(row > 0)
        {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
        }
        if(target==GL_TEXTURE_1D)
        {
            glTexImage1D(target, level, internal, tw, 0, format, type, src);
        }
        else
        {
            glTexImage2D(target, level, internal, tw, th, 0, format, type, src);
        }
        if(row > 0)
        {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, row = 0);
        }
        if(!mipmap || std::max(tw, th) <= 1)
        {
            break;
        }
        int srcw = tw,
            srch = th;
        if(tw > 1)
        {
            tw /= 2;
        }
        if(th > 1)
        {
            th /= 2;
        }
        if(src)
        {
            if(!buf)
            {
                buf = new uchar[tw*th*bpp];
            }
            scaletexture(src, srcw, srch, bpp, pitch, buf, tw, th);
        }
    }
    if(buf)
    {
        delete[] buf;
    }
}

static void uploadcompressedtexture(GLenum target, GLenum subtarget, GLenum format, int w, int h, const uchar *data, int align, int blocksize, int levels, bool mipmap)
{
    int hwlimit = target==GL_TEXTURE_CUBE_MAP ? hwcubetexsize : hwtexsize,
        sizelimit = levels > 1 && maxtexsize ? std::min(maxtexsize, hwlimit) : hwlimit;
    int level = 0;
    for(int i = 0; i < levels; ++i)
    {
        int size = ((w + align-1)/align) * ((h + align-1)/align) * blocksize;
        if(w <= sizelimit && h <= sizelimit)
        {
            if(target==GL_TEXTURE_1D)
            {
                glCompressedTexImage1D_(subtarget, level, format, w, 0, size, data);
            }
            else
            {
                glCompressedTexImage2D_(subtarget, level, format, w, h, 0, size, data);
            }
            level++;
            if(!mipmap)
            {
                break;
            }
        }
        if(std::max(w, h) <= 1)
        {
            break;
        }
        if(w > 1)
        {
            w /= 2;
        }
        if(h > 1)
        {
            h /= 2;
        }
        data += size;
    }
}

static GLenum textarget(GLenum subtarget)
{
    switch(subtarget)
    {
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        {
            return GL_TEXTURE_CUBE_MAP;
        }
    }
    return subtarget;
}

const GLint *swizzlemask(GLenum format)
{
    static const GLint luminance[4] = { GL_RED, GL_RED, GL_RED, GL_ONE },
                       luminancealpha[4] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
    switch(format)
    {
        case GL_RED:
        {
            return luminance;
        }
        case GL_RG:
        {
            return luminancealpha;
        }
    }
    return nullptr;
}

void setuptexparameters(int tnum, const void *pixels, int clamp, int filter, GLenum format, GLenum target, bool swizzle)
{
    glBindTexture(target, tnum);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : (clamp&0x100 ? GL_MIRRORED_REPEAT : GL_REPEAT));
    if(target!=GL_TEXTURE_1D)
    {
        glTexParameteri(target, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : (clamp&0x200 ? GL_MIRRORED_REPEAT : GL_REPEAT));
    }
    if(target==GL_TEXTURE_3D)
    {
        glTexParameteri(target, GL_TEXTURE_WRAP_R, clamp&4 ? GL_CLAMP_TO_EDGE : (clamp&0x400 ? GL_MIRRORED_REPEAT : GL_REPEAT));
    }
    if(target==GL_TEXTURE_2D && std::min(aniso, hwmaxaniso) > 0 && filter > 1)
    {
        glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(aniso, hwmaxaniso));
    }
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter && bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
        filter > 1 ?
            (trilinear ?
                (bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR) :
                (bilinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST)) :
            (filter && bilinear ? GL_LINEAR : GL_NEAREST));
    if(swizzle)
    {
        const GLint *mask = swizzlemask(format);
        if(mask)
        {
            glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, mask);
        }
    }
}

static GLenum textype(GLenum &component, GLenum &format)
{
    GLenum type = GL_UNSIGNED_BYTE;
    switch(component)
    {
        case GL_R16F:
        case GL_R32F:
        {
            if(!format)
            {
                format = GL_RED;
            }
            type = GL_FLOAT;
            break;
        }
        case GL_RG16F:
        case GL_RG32F:
        {
            if(!format)
            {
                format = GL_RG;
            }
            type = GL_FLOAT;
            break;
        }
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_R11F_G11F_B10F:
        {
            if(!format)
            {
                format = GL_RGB;
            }
            type = GL_FLOAT;
            break;
        }
        case GL_RGBA16F:
        case GL_RGBA32F:
        {
            if(!format)
            {
                format = GL_RGBA;
            }
            type = GL_FLOAT;
            break;
        }
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
        {
            if(!format)
            {
                format = GL_DEPTH_COMPONENT;
            }
            break;
        }
        case GL_DEPTH_STENCIL:
        case GL_DEPTH24_STENCIL8:
        {
            if(!format)
            {
                format = GL_DEPTH_STENCIL;
            }
            type = GL_UNSIGNED_INT_24_8;
            break;
        }
        case GL_R8:
        case GL_R16:
        {
            if(!format)
            {
                format = GL_RED;
            }
            break;
        }
        case GL_RG8:
        case GL_RG16:
        {
            if(!format)
            {
                format = GL_RG;
            }
            break;
        }
        case GL_RGB5:
        case GL_RGB8:
        case GL_RGB16:
        case GL_RGB10:
        {
            if(!format)
            {
                format = GL_RGB;
            }
            break;
        }
        case GL_RGB5_A1:
        case GL_RGBA8:
        case GL_RGBA16:
        case GL_RGB10_A2:
        {
            if(!format)
            {
                format = GL_RGBA;
            }
            break;
        }
        case GL_LUMINANCE8:
        case GL_LUMINANCE16:
        case GL_COMPRESSED_LUMINANCE:
        {
            if(!format)
            {
                format = GL_LUMINANCE;
            }
            break;
        }
        case GL_LUMINANCE8_ALPHA8:
        case GL_LUMINANCE16_ALPHA16:
        case GL_COMPRESSED_LUMINANCE_ALPHA:
        {
            if(!format)
            {
                format = GL_LUMINANCE_ALPHA;
            }
            break;
        }
        case GL_ALPHA8:
        case GL_ALPHA16:
        case GL_COMPRESSED_ALPHA:
        {
            if(!format)
            {
                format = GL_ALPHA;
            }
            break;
        }
        case GL_RGB8UI:
        case GL_RGB16UI:
        case GL_RGB32UI:
        case GL_RGB8I:
        case GL_RGB16I:
        case GL_RGB32I:
        {
            if(!format)
            {
                format = GL_RGB_INTEGER;
            }
            break;
        }
        case GL_RGBA8UI:
        case GL_RGBA16UI:
        case GL_RGBA32UI:
        case GL_RGBA8I:
        case GL_RGBA16I:
        case GL_RGBA32I:
        {
            if(!format)
            {
                format = GL_RGBA_INTEGER;
            }
            break;
        }
        case GL_R8UI:
        case GL_R16UI:
        case GL_R32UI:
        case GL_R8I:
        case GL_R16I:
        case GL_R32I:
        {
            if(!format)
            {
                format = GL_RED_INTEGER;
            }
            break;
        }
        case GL_RG8UI:
        case GL_RG16UI:
        case GL_RG32UI:
        case GL_RG8I:
        case GL_RG16I:
        case GL_RG32I:
        {
            if(!format)
            {
                format = GL_RG_INTEGER;
            }
            break;
        }
    }
    if(!format)
    {
        format = component;
    }
    return type;
}

void createtexture(int tnum, int w, int h, const void *pixels, int clamp, int filter, GLenum component, GLenum subtarget, int pw, int ph, int pitch, bool resize, GLenum format, bool swizzle)
{
    GLenum target = textarget(subtarget),
           type = textype(component, format);
    if(tnum)
    {
        setuptexparameters(tnum, pixels, clamp, filter, format, target, swizzle);
    }
    if(!pw)
    {
        pw = w;
    }
    if(!ph)
    {
        ph = h;
    }
    int tw = w,
        th = h;
    bool mipmap = filter > 1;
    if(resize && pixels)
    {
        resizetexture(w, h, mipmap, false, target, 0, tw, th);
    }
    uploadtexture(subtarget, component, tw, th, format, type, pixels, pw, ph, pitch, mipmap);
}

void createcompressedtexture(int tnum, int w, int h, const uchar *data, int align, int blocksize, int levels, int clamp, int filter, GLenum format, GLenum subtarget, bool swizzle = false)
{
    GLenum target = textarget(subtarget);
    if(tnum)
    {
        setuptexparameters(tnum, data, clamp, filter, format, target, swizzle);
    }
    uploadcompressedtexture(target, subtarget, format, w, h, data, align, blocksize, levels, filter > 1);
}

void create3dtexture(int tnum, int w, int h, int d, const void *pixels, int clamp, int filter, GLenum component, GLenum target, bool swizzle)
{
    GLenum format = GL_FALSE, type = textype(component, format);
    if(tnum)
    {
        setuptexparameters(tnum, pixels, clamp, filter, format, target, swizzle);
    }
    glTexImage3D_(target, 0, component, w, h, d, 0, format, type, pixels);
}


hashnameset<Texture> textures;

Texture *notexture = nullptr; // used as default, ensured to be loaded

static GLenum texformat(int bpp, bool swizzle = false)
{
    switch(bpp)
    {
        case 1:
        {
            return GL_RED;
        }
        case 2:
        {
            return GL_RG;
        }
        case 3:
        {
            return GL_RGB;
        }
        case 4:
        {
            return GL_RGBA;
        }
        default:
        {
            return 0;
        }
    }
}

static bool alphaformat(GLenum format)
{
    switch(format)
    {
        case GL_ALPHA:
        case GL_LUMINANCE_ALPHA:
        case GL_RG:
        case GL_RGBA:
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}

bool floatformat(GLenum format)
{
    switch(format)
    {
        case GL_R16F:
        case GL_R32F:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_R11F_G11F_B10F:
        case GL_RGBA16F:
        case GL_RGBA32F:
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}

static Texture *newtexture(Texture *t, const char *rname, ImageData &s, int clamp = 0, bool mipit = true, bool canreduce = false, bool transient = false, int compress = 0)
{
    if(!t)
    {
        char *key = newstring(rname);
        t = &textures[key];
        t->name = key;
    }

    t->clamp = clamp;
    t->mipmap = mipit;
    t->type = Texture::IMAGE;
    if(transient)
    {
        t->type |= Texture::TRANSIENT;
    }
    if(clamp&0x300)
    {
        t->type |= Texture::MIRROR;
    }
    if(!s.data)
    {
        t->type |= Texture::STUB;
        t->w = t->h = t->xs = t->ys = t->bpp = 0;
        return t;
    }

    bool swizzle = !(clamp&0x10000);
    GLenum format;
    format = texformat(s.bpp, swizzle);
    t->bpp = s.bpp;
    if(alphaformat(format))
    {
        t->type |= Texture::ALPHA;
    }
    t->w = t->xs = s.w;
    t->h = t->ys = s.h;
    t->ratio = t->w / static_cast<float>(t->h);
    int filter = !canreduce || reducefilter ? (mipit ? 2 : 1) : 0;
    glGenTextures(1, &t->id);
    if(s.compressed)
    {
        uchar *data = s.data;
        int levels = s.levels, level = 0;
        int sizelimit = mipit && maxtexsize ? std::min(maxtexsize, hwtexsize) : hwtexsize;
        while(t->w > sizelimit || t->h > sizelimit)
        {
            data += s.calclevelsize(level++);
            levels--;
            if(t->w > 1)
            {
                t->w /= 2;
            }
            if(t->h > 1)
            {
                t->h /= 2;
            }
        }
        createcompressedtexture(t->id, t->w, t->h, data, s.align, s.bpp, levels, clamp, filter, s.compressed, GL_TEXTURE_2D, swizzle);
    }
    else
    {
        resizetexture(t->w, t->h, mipit, canreduce, GL_TEXTURE_2D, compress, t->w, t->h);
        createtexture(t->id, t->w, t->h, s.data, clamp, filter, format, GL_TEXTURE_2D, t->xs, t->ys, s.pitch, false, format, swizzle);
    }
    return t;
}

static SDL_Surface *creatergbsurface(SDL_Surface *os)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, os->w, os->h, 24, 0x0000ff, 0x00ff00, 0xff0000, 0);
    if(ns)
    {
        SDL_BlitSurface(os, nullptr, ns, nullptr);
    }
    SDL_FreeSurface(os);
    return ns;
}

static SDL_Surface *creatergbasurface(SDL_Surface *os)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, os->w, os->h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if(ns)
    {
        SDL_SetSurfaceBlendMode(os, SDL_BLENDMODE_NONE);
        SDL_BlitSurface(os, nullptr, ns, nullptr);
    }
    SDL_FreeSurface(os);
    return ns;
}

static bool checkgrayscale(SDL_Surface *s)
{
    // gray scale images have 256 levels, no colorkey, and the palette is a ramp
    if(s->format->palette)
    {
        if(s->format->palette->ncolors != 256 || SDL_GetColorKey(s, nullptr) >= 0)
        {
            return false;
        }
        const SDL_Color *colors = s->format->palette->colors;
        for(int i = 0; i < 256; ++i)
        {
            if(colors[i].r != i || colors[i].g != i || colors[i].b != i)
            {
                return false;
            }
        }
    }
    return true;
}

static SDL_Surface *fixsurfaceformat(SDL_Surface *s)
{
    if(!s)
    {
        return nullptr;
    }
    if(!s->pixels || std::min(s->w, s->h) <= 0 || s->format->BytesPerPixel <= 0)
    {
        SDL_FreeSurface(s);
        return nullptr;
    }
    static const uint rgbmasks[]  = { 0x0000ff, 0x00ff00, 0xff0000, 0 },
                      rgbamasks[] = { 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };
    switch(s->format->BytesPerPixel)
    {
        case 1:
            if(!checkgrayscale(s))
            {
                return SDL_GetColorKey(s, nullptr) >= 0 ? creatergbasurface(s) : creatergbsurface(s);
            }
            break;
        case 3:
            if(s->format->Rmask != rgbmasks[0] || s->format->Gmask != rgbmasks[1] || s->format->Bmask != rgbmasks[2])
            {
                return creatergbsurface(s);
            }
            break;
        case 4:
            if(s->format->Rmask != rgbamasks[0] || s->format->Gmask != rgbamasks[1] || s->format->Bmask != rgbamasks[2] || s->format->Amask != rgbamasks[3])
            {
                return s->format->Amask ? creatergbasurface(s) : creatergbsurface(s);
            }
            break;
    }
    return s;
}

static void texnormal(ImageData &s, int emphasis)
{
    ImageData d(s.w, s.h, 3);
    uchar *src = s.data,
          *dst = d.data;
    for(int y = 0; y < s.h; ++y)
    {
        for(int x = 0; x < s.w; ++x)
        {
            vec normal(0.0f, 0.0f, 255.0f/emphasis);
            normal.x += src[y*s.pitch + ((x+s.w-1)%s.w)*s.bpp];
            normal.x -= src[y*s.pitch + ((x+1)%s.w)*s.bpp];
            normal.y += src[((y+s.h-1)%s.h)*s.pitch + x*s.bpp];
            normal.y -= src[((y+1)%s.h)*s.pitch + x*s.bpp];
            normal.normalize();
            *dst++ = static_cast<uchar>(127.5f + normal.x*127.5f);
            *dst++ = static_cast<uchar>(127.5f + normal.y*127.5f);
            *dst++ = static_cast<uchar>(127.5f + normal.z*127.5f);
        }
    }
    s.replace(d);
}

static bool canloadsurface(const char *name)
{
    stream *f = openfile(name, "rb");
    if(!f)
    {
        return false;
    }
    delete f;
    return true;
}

SDL_Surface *loadsurface(const char *name)
{
    SDL_Surface *s = nullptr;
    stream *z = openzipfile(name, "rb");
    if(z)
    {
        SDL_RWops *rw = z->rwops();
        if(rw)
        {
            const char *ext = std::strrchr(name, '.');
            if(ext)
            {
                ++ext;
            }
            s = IMG_LoadTyped_RW(rw, 0, ext);
            SDL_FreeRW(rw);
        }
        delete z;
    }
    if(!s)
    {
        s = IMG_Load(findfile(name, "rb"));
    }
    return fixsurfaceformat(s);
}

static vec parsevec(const char *arg)
{
    vec v(0, 0, 0);
    int i = 0;
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

static bool texturedata(ImageData &d, const char *tname, bool msg = true, int *compress = nullptr, int *wrap = nullptr, const char *tdir = nullptr, int ttype = Tex_Diffuse)
{
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
                conoutf(Console_Error, "could not load texture %s", tname);
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
        #define PARSETEXCOMMANDS(cmds) \
            const char *cmd = nullptr, \
                       *end = nullptr, \
                       *arg[4] = { nullptr, nullptr, nullptr, nullptr }; \
            cmd = &cmds[1]; \
            end = std::strchr(cmd, '>'); \
            if(!end) \
            { \
                break; \
            } \
            cmds = std::strchr(cmd, '<'); \
            size_t len = std::strcspn(cmd, ":,><"); \
            for(int i = 0; i < 4; ++i) \
            { \
                arg[i] = std::strchr(i ? arg[i-1] : cmd, i ? ',' : ':'); \
                if(!arg[i] || arg[i] >= end) \
                { \
                    arg[i] = ""; \
                } \
                else \
                { \
                    arg[i]++; \
                } \
            }
        PARSETEXCOMMANDS(pcmds);
        if(matchstring(cmd, len, "stub"))
        {
            return canloadsurface(file);
        }
    }
    if(msg)
    {
        renderprogress(loadprogress, file);
    }
    if(!d.data)
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
        d.wrap(s);
    }

    while(cmds)
    {
        PARSETEXCOMMANDS(cmds);
        if(d.compressed)
        {
            goto compressed; //see `compressed` nested between else & if (yes it's ugly)
        }
        if(matchstring(cmd, len, "mad"))
        {
            texmad(d, parsevec(arg[0]), parsevec(arg[1]));
        }
        else if(matchstring(cmd, len, "colorify"))
        {
            texcolorify(d, parsevec(arg[0]), parsevec(arg[1]));
        }
        else if(matchstring(cmd, len, "colormask"))
        {
            texcolormask(d, parsevec(arg[0]), *arg[1] ? parsevec(arg[1]) : vec(1, 1, 1));
        }
        else if(matchstring(cmd, len, "normal"))
        {
            int emphasis = std::atoi(arg[0]);
            texnormal(d, emphasis > 0 ? emphasis : 3);
        }
        else if(matchstring(cmd, len, "dup"))
        {
            texdup(d, std::atoi(arg[0]), std::atoi(arg[1]));
        }
        else if(matchstring(cmd, len, "offset"))
        {
            texoffset(d, std::atoi(arg[0]), std::atoi(arg[1]));
        }
        else if(matchstring(cmd, len, "rotate"))
        {
            texrotate(d, std::atoi(arg[0]), ttype);
        }
        else if(matchstring(cmd, len, "reorient"))
        {
            texreorient(d, std::atoi(arg[0])>0, std::atoi(arg[1])>0, std::atoi(arg[2])>0, ttype);
        }
        else if(matchstring(cmd, len, "crop"))
        {
            texcrop(d, std::atoi(arg[0]), std::atoi(arg[1]), *arg[2] ? std::atoi(arg[2]) : -1, *arg[3] ? std::atoi(arg[3]) : -1);
        }
        else if(matchstring(cmd, len, "mix"))
        {
            texmix(d, *arg[0] ? std::atoi(arg[0]) : -1, *arg[1] ? std::atoi(arg[1]) : -1, *arg[2] ? std::atoi(arg[2]) : -1, *arg[3] ? std::atoi(arg[3]) : -1);
        }
        else if(matchstring(cmd, len, "grey"))
        {
            texgrey(d);
        }
        else if(matchstring(cmd, len, "premul"))
        {
            texpremul(d);
        }
        else if(matchstring(cmd, len, "agrad"))
        {
            texagrad(d, std::atof(arg[0]), std::atof(arg[1]), std::atof(arg[2]), std::atof(arg[3]));
        }
        else if(matchstring(cmd, len, "blend"))
        {
            ImageData src, mask;
            string srcname, maskname;
            copystring(srcname, stringslice(arg[0], std::strcspn(arg[0], ":,><")));
            copystring(maskname, stringslice(arg[1], std::strcspn(arg[1], ":,><")));
            if(srcname[0] && texturedata(src, srcname, false, nullptr, nullptr, tdir, ttype) && (!maskname[0] || texturedata(mask, maskname, false, nullptr, nullptr, tdir, ttype)))
            {
                texblend(d, src, maskname[0] ? mask : src);
            }
        }
        else if(matchstring(cmd, len, "thumbnail"))
        {
            int w = std::atoi(arg[0]),
                h = std::atoi(arg[1]);
            if(w <= 0 || w > (1<<12))
            {
                w = 64;
            }
            if(h <= 0 || h > (1<<12))
            {
                h = w;
            }
            if(d.w > w || d.h > h)
            {
                scaleimage(d, w, h);
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

static bool texturedata(ImageData &d, Slot &slot, Slot::Tex &tex, bool msg = true, int *compress = nullptr, int *wrap = nullptr)
{
    return texturedata(d, tex.name, msg, compress, wrap, slot.texturedir(), tex.type);
}

uchar *loadalphamask(Texture *t)
{
    if(t->alphamask)
    {
        return t->alphamask;
    }
    if(!(t->type&Texture::ALPHA))
    {
        return nullptr;
    }
    ImageData s;
    if(!texturedata(s, t->name, false) || !s.data || s.compressed)
    {
        return nullptr;
    }
    t->alphamask = new uchar[s.h * ((s.w+7)/8)];
    uchar *srcrow = s.data,
          *dst = t->alphamask-1;
    for(int y = 0; y < s.h; ++y)
    {
        uchar *src = srcrow+s.bpp-1;
        for(int x = 0; x < s.w; ++x)
        {
            int offset = x%8;
            if(!offset)
            {
                *++dst = 0;
            }
            if(*src)
            {
                *dst |= 1<<offset;
            }
            src += s.bpp;
        }
        srcrow += s.pitch;
    }
    return t->alphamask;
}

Texture *textureload(const char *name, int clamp, bool mipit, bool msg)
{
    string tname;
    copystring(tname, name);
    Texture *t = textures.access(path(tname));
    if(t)
    {
        return t;
    }
    int compress = 0;
    ImageData s;
    if(texturedata(s, tname, msg, &compress, &clamp))
    {
        return newtexture(nullptr, tname, s, clamp, mipit, false, false, compress);
    }
    return notexture;
}

bool settexture(const char *name, int clamp)
{
    Texture *t = textureload(name, clamp, true, false);
    glBindTexture(GL_TEXTURE_2D, t->id);
    return t != notexture;
}

vector<VSlot *> vslots;
vector<Slot *> slots;
MatSlot materialslots[(MatFlag_Volume|MatFlag_Index)+1];
Slot dummyslot;
VSlot dummyvslot(&dummyslot);
vector<DecalSlot *> decalslots;
DecalSlot dummydecalslot;
Slot *defslot = nullptr;

const char *Slot::name() const
{
    return tempformatstring("slot %d", index);
}

MatSlot::MatSlot() : Slot(static_cast<int>(this - materialslots)), VSlot(this) {}
const char *MatSlot::name() const
{
    return tempformatstring("material slot %s", findmaterialname(Slot::index));
}

const char *DecalSlot::name() const
{
    return tempformatstring("decal slot %d", Slot::index);
}

void texturereset(int *n)
{
    if(!(identflags&Idf_Overridden) && !allowediting)
    {
        return;
    }
    defslot = nullptr;
    resetslotshader();
    int limit = std::clamp(*n, 0, slots.length());
    for(int i = limit; i < slots.length(); i++)
    {
        Slot *s = slots[i];
        for(VSlot *vs = s->variants; vs; vs = vs->next)
        {
            vs->slot = &dummyslot;
        }
        delete s;
    }
    slots.setsize(limit);
    while(vslots.length())
    {
        VSlot *vs = vslots.last();
        if(vs->slot != &dummyslot || vs->changed)
        {
            break;
        }
        delete vslots.pop();
    }
}

COMMAND(texturereset, "i");

void materialreset()
{
    if(!(identflags&Idf_Overridden) && !allowediting)
    {
        return;
    }
    defslot = nullptr;
    for(int i = 0; i < (MatFlag_Volume|MatFlag_Index)+1; ++i)
    {
        materialslots[i].reset();
    }
}

COMMAND(materialreset, "");

void decalreset(int *n)
{
    if(!(identflags&Idf_Overridden) && !allowediting)
    {
        return;
    }
    defslot = nullptr;
    resetslotshader();
    decalslots.deletecontents(*n);
}

COMMAND(decalreset, "i");

static int compactedvslots = 0,
           compactvslotsprogress = 0,
           clonedvslots = 0;
static bool markingvslots = false;

void clearslots()
{
    defslot = nullptr;
    resetslotshader();
    slots.deletecontents();
    vslots.deletecontents();
    for(int i = 0; i < (MatFlag_Volume|MatFlag_Index)+1; ++i)
    {
        materialslots[i].reset();
    }
    decalslots.deletecontents();
    clonedvslots = 0;
}

static void assignvslot(VSlot &vs)
{
    vs.index = compactedvslots++;
}

void compactvslot(int &index)
{
    if(vslots.inrange(index))
    {
        VSlot &vs = *vslots[index];
        if(vs.index < 0)
        {
            assignvslot(vs);
        }
        if(!markingvslots)
        {
            index = vs.index;
        }
    }
}

void compactvslot(VSlot &vs)
{
    if(vs.index < 0)
    {
        assignvslot(vs);
    }
}

void compactvslots(cube *c, int n)
{
    if((compactvslotsprogress++&0xFFF)==0)
    {
        renderprogress(std::min(static_cast<float>(compactvslotsprogress)/allocnodes, 1.0f), markingvslots ? "marking slots..." : "compacting slots...");
    }
    for(int i = 0; i < n; ++i)
    {
        if(c[i].children)
        {
            compactvslots(c[i].children);
        }
        else
        {
            for(int j = 0; j < 6; ++j)
            {
                if(vslots.inrange(c[i].texture[j]))
                {
                    VSlot &vs = *vslots[c[i].texture[j]];
                    if(vs.index < 0)
                    {
                        assignvslot(vs);
                    }
                    if(!markingvslots)
                    {
                        c[i].texture[j] = vs.index;
                    }
                }
            }
        }
    }
}

int cubeworld::compactvslots(bool cull)
{
    defslot = nullptr;
    clonedvslots = 0;
    markingvslots = cull;
    compactedvslots = 0;
    compactvslotsprogress = 0;
    for(int i = 0; i < vslots.length(); i++)
    {
        vslots[i]->index = -1;
    }
    if(cull)
    {
        int numdefaults = std::min(static_cast<int>(Default_NumDefaults), slots.length());
        for(int i = 0; i < numdefaults; ++i)
        {
            slots[i]->variants->index = compactedvslots++;
        }
    }
    else
    {
        for(int i = 0; i < slots.length(); i++)
        {
            slots[i]->variants->index = compactedvslots++;
        }
        for(int i = 0; i < vslots.length(); i++)
        {
            VSlot &vs = *vslots[i];
            if(!vs.changed && vs.index < 0)
            {
                markingvslots = true;
                break;
            }
        }
    }
    compactvslots(worldroot);
    int total = compactedvslots;
    compacteditvslots();
    for(int i = 0; i < vslots.length(); i++)
    {
        VSlot *vs = vslots[i];
        if(vs->changed)
        {
            continue;
        }
        while(vs->next)
        {
            if(vs->next->index < 0)
            {
                vs->next = vs->next->next;
            }
            else
            {
                vs = vs->next;
            }
        }
    }
    if(markingvslots)
    {
        markingvslots = false;
        compactedvslots = 0;
        compactvslotsprogress = 0;
        int lastdiscard = 0;
        for(int i = 0; i < vslots.length(); i++)
        {
            VSlot &vs = *vslots[i];
            if(vs.changed || (vs.index < 0 && !vs.next))
            {
                vs.index = -1;
            }
            else
            {
                if(!cull)
                {
                    while(lastdiscard < i)
                    {
                        VSlot &ds = *vslots[lastdiscard++];
                        if(!ds.changed && ds.index < 0)
                        {
                            ds.index = compactedvslots++;
                        }
                    }
                }
                vs.index = compactedvslots++;
            }
        }
        compactvslots(worldroot);
        total = compactedvslots;
        compacteditvslots();
    }
    compactmruvslots();
    if(cull)
    {
        for(int i = slots.length(); --i >=0;) //note reverse iteration
        {
            if(slots[i]->variants->index < 0)
            {
                delete slots.remove(i);
            }
        }
        for(int i = 0; i < slots.length(); i++)
        {
            slots[i]->index = i;
        }
    }
    for(int i = 0; i < vslots.length(); i++)
    {
        while(vslots[i]->index >= 0 && vslots[i]->index != i)
        {
            std::swap(vslots[i], vslots[vslots[i]->index]);
        }
    }
    for(int i = compactedvslots; i < vslots.length(); i++)
    {
        delete vslots[i];
    }
    vslots.setsize(compactedvslots);
    return total;
}

void compactvslotscmd(int *cull)
{
    multiplayerwarn();
    rootworld.compactvslots(*cull!=0);
    rootworld.allchanged();
}
COMMANDN(compactvslots, compactvslotscmd, "i");

static void clampvslotoffset(VSlot &dst, Slot *slot = nullptr)
{
    if(!slot)
    {
        slot = dst.slot;
    }
    if(slot && slot->sts.inrange(0))
    {
        if(!slot->loaded)
        {
            slot->load();
        }
        Texture *t = slot->sts[0].t;
        int xs = t->xs,
            ys = t->ys;
        if(t->type & Texture::MIRROR)
        {
            xs *= 2;
            ys *= 2;
        }
        if(texrotations[dst.rotation].swapxy)
        {
            std::swap(xs, ys);
        }
        dst.offset.x %= xs;
        if(dst.offset.x < 0)
        {
            dst.offset.x += xs;
        }
        dst.offset.y %= ys;
        if(dst.offset.y < 0)
        {
            dst.offset.y += ys;
        }
    }
    else
    {
        dst.offset.max(0);
    }
}

static void propagatevslot(VSlot &dst, const VSlot &src, int diff, bool edit = false)
{
    if(diff & (1 << VSlot_ShParam))
    {
        for(int i = 0; i < src.params.length(); i++)
        {
            dst.params.add(src.params[i]);
        }
    }
    if(diff & (1 << VSlot_Scale))
    {
        dst.scale = src.scale;
    }
    if(diff & (1 << VSlot_Rotation))
    {
        dst.rotation = src.rotation;
        if(edit && !dst.offset.iszero())
        {
            clampvslotoffset(dst);
        }
    }
    if(diff & (1 << VSlot_Angle))
    {
        dst.angle = src.angle;
    }
    if(diff & (1 << VSlot_Offset))
    {
        dst.offset = src.offset;
        if(edit)
        {
            clampvslotoffset(dst);
        }
    }
    if(diff & (1 << VSlot_Scroll))
    {
        dst.scroll = src.scroll;
    }
    if(diff & (1 << VSlot_Alpha))
    {
        dst.alphafront = src.alphafront;
        dst.alphaback = src.alphaback;
    }
    if(diff & (1 << VSlot_Color))
    {
        dst.colorscale = src.colorscale;
    }
    if(diff & (1 << VSlot_Refract))
    {
        dst.refractscale = src.refractscale;
        dst.refractcolor = src.refractcolor;
    }
}

static void propagatevslot(VSlot *root, int changed)
{
    for(VSlot *vs = root->next; vs; vs = vs->next)
    {
        int diff = changed & ~vs->changed;
        if(diff)
        {
            propagatevslot(*vs, *root, diff);
        }
    }
}

static void mergevslot(VSlot &dst, const VSlot &src, int diff, Slot *slot = nullptr)
{
    if(diff & (1 << VSlot_ShParam))
    {
        for(int i = 0; i < src.params.length(); i++)
        {
            const SlotShaderParam &sp = src.params[i];
            for(int j = 0; j < dst.params.length(); j++)
            {
                SlotShaderParam &dp = dst.params[j];
                if(sp.name == dp.name)
                {
                    memcpy(dp.val, sp.val, sizeof(dp.val));
                    goto nextparam; //bail out of loop
                }
            }
            dst.params.add(sp);
        nextparam:;
        }
    }
    if(diff & (1 << VSlot_Scale))
    {
        dst.scale = std::clamp(dst.scale*src.scale, 1/8.0f, 8.0f);
    }
    if(diff & (1 << VSlot_Rotation))
    {
        dst.rotation = std::clamp(dst.rotation + src.rotation, 0, 7);
        if(!dst.offset.iszero())
        {
            clampvslotoffset(dst, slot);
        }
    }
    if(diff & (1 << VSlot_Angle))
    {
        dst.angle.add(src.angle);
    }
    if(diff & (1 << VSlot_Offset))
    {
        dst.offset.add(src.offset);
        clampvslotoffset(dst, slot);
    }
    if(diff & (1 << VSlot_Scroll))
    {
        dst.scroll.add(src.scroll);
    }
    if(diff & (1 << VSlot_Alpha))
    {
        dst.alphafront = src.alphafront;
        dst.alphaback = src.alphaback;
    }
    if(diff & (1 << VSlot_Color))
    {
        dst.colorscale.mul(src.colorscale);
    }
    if(diff & (1 << VSlot_Refract))
    {
        dst.refractscale *= src.refractscale;
        dst.refractcolor.mul(src.refractcolor);
    }
}

void mergevslot(VSlot &dst, const VSlot &src, const VSlot &delta)
{
    dst.changed = src.changed | delta.changed;
    propagatevslot(dst, src, (1 << VSlot_Num) - 1);
    mergevslot(dst, delta, delta.changed, src.slot);
}

static VSlot *reassignvslot(Slot &owner, VSlot *vs)
{
    vs->reset();
    owner.variants = vs;
    while(vs)
    {
        vs->slot = &owner;
        vs->linked = false;
        vs = vs->next;
    }
    return owner.variants;
}

static VSlot *emptyvslot(Slot &owner)
{
    int offset = 0;
    for(int i = slots.length(); --i >=0;) //note reverse iteration
    {
        if(slots[i]->variants)
        {
            offset = slots[i]->variants->index + 1;
            break;
        }
    }
    for(int i = offset; i < vslots.length(); i++)
    {
        if(!vslots[i]->changed)
        {
            return reassignvslot(owner, vslots[i]);
        }
    }
    return vslots.add(new VSlot(&owner, vslots.length()));
}

static bool comparevslot(const VSlot &dst, const VSlot &src, int diff)
{
    if(diff & (1 << VSlot_ShParam))
    {
        if(src.params.length() != dst.params.length())
        {
            return false;
        }
        for(int i = 0; i < src.params.length(); i++)
        {
            const SlotShaderParam &sp = src.params[i], &dp = dst.params[i];
            if(sp.name != dp.name || memcmp(sp.val, dp.val, sizeof(sp.val)))
            {
                return false;
            }
        }
    }
    if(diff & (1 << VSlot_Scale)   && dst.scale != src.scale)       return false;
    if(diff & (1 << VSlot_Rotation)&& dst.rotation != src.rotation) return false;
    if(diff & (1 << VSlot_Angle)   && dst.angle != src.angle)       return false;
    if(diff & (1 << VSlot_Offset)  && dst.offset != src.offset)     return false;
    if(diff & (1 << VSlot_Scroll)  && dst.scroll != src.scroll)     return false;
    if(diff & (1 << VSlot_Alpha)   && (dst.alphafront != src.alphafront || dst.alphaback != src.alphaback)) return false;
    if(diff & (1 << VSlot_Color)   && dst.colorscale != src.colorscale) return false;
    if(diff & (1 << VSlot_Refract) && (dst.refractscale != src.refractscale || dst.refractcolor != src.refractcolor)) return false;
    return true;
}

void packvslot(vector<uchar> &buf, const VSlot &src)
{
    if(src.changed & (1 << VSlot_ShParam))
    {
        for(int i = 0; i < src.params.length(); i++)
        {
            const SlotShaderParam &p = src.params[i];
            buf.put(VSlot_ShParam);
            sendstring(p.name, buf);
            for(int j = 0; j < 4; ++j)
            {
                putfloat(buf, p.val[j]);
            }
        }
    }
    if(src.changed & (1 << VSlot_Scale))
    {
        buf.put(VSlot_Scale);
        putfloat(buf, src.scale);
    }
    if(src.changed & (1 << VSlot_Rotation))
    {
        buf.put(VSlot_Rotation);
        putint(buf, src.rotation);
    }
    if(src.changed & (1 << VSlot_Angle))
    {
        buf.put(VSlot_Angle);
        putfloat(buf, src.angle.x);
        putfloat(buf, src.angle.y);
        putfloat(buf, src.angle.z);
    }
    if(src.changed & (1 << VSlot_Offset))
    {
        buf.put(VSlot_Offset);
        putint(buf, src.offset.x);
        putint(buf, src.offset.y);
    }
    if(src.changed & (1 << VSlot_Scroll))
    {
        buf.put(VSlot_Scroll);
        putfloat(buf, src.scroll.x);
        putfloat(buf, src.scroll.y);
    }
    if(src.changed & (1 << VSlot_Alpha))
    {
        buf.put(VSlot_Alpha);
        putfloat(buf, src.alphafront);
        putfloat(buf, src.alphaback);
    }
    if(src.changed & (1 << VSlot_Color))
    {
        buf.put(VSlot_Color);
        putfloat(buf, src.colorscale.r);
        putfloat(buf, src.colorscale.g);
        putfloat(buf, src.colorscale.b);
    }
    if(src.changed & (1 << VSlot_Refract))
    {
        buf.put(VSlot_Refract);
        putfloat(buf, src.refractscale);
        putfloat(buf, src.refractcolor.r);
        putfloat(buf, src.refractcolor.g);
        putfloat(buf, src.refractcolor.b);
    }
    buf.put(0xFF);
}

void packvslot(vector<uchar> &buf, int index)
{
    if(vslots.inrange(index))
    {
        packvslot(buf, *vslots[index]);
    }
    else
    {
        buf.put(0xFF);
    }
}

void packvslot(vector<uchar> &buf, const VSlot *vs)
{
    if(vs)
    {
        packvslot(buf, *vs);
    }
    else
    {
        buf.put(0xFF);
    }
}

bool unpackvslot(ucharbuf &buf, VSlot &dst, bool delta)
{
    while(buf.remaining())
    {
        int changed = buf.get();
        if(changed >= 0x80)
        {
            break;
        }
        switch(changed)
        {
            case VSlot_ShParam:
            {
                string name;
                getstring(name, buf);
                SlotShaderParam p = { name[0] ? getshaderparamname(name) : nullptr, -1, 0, { 0, 0, 0, 0 } };
                for(int i = 0; i < 4; ++i)
                {
                    p.val[i] = getfloat(buf);
                }
                if(p.name)
                {
                    dst.params.add(p);
                }
                break;
            }
            case VSlot_Scale:
            {
                dst.scale = getfloat(buf);
                if(dst.scale <= 0)
                {
                    dst.scale = 1;
                }
                else if(!delta)
                {
                    dst.scale = std::clamp(dst.scale, 1/8.0f, 8.0f);
                }
                break;
            }
            case VSlot_Rotation:
                dst.rotation = getint(buf);
                if(!delta)
                {
                    dst.rotation = std::clamp(dst.rotation, 0, 7);
                }
                break;
            case VSlot_Angle:
            {
                dst.angle.x = getfloat(buf);
                dst.angle.y = getfloat(buf);
                dst.angle.z = getfloat(buf);
                break;
            }
            case VSlot_Offset:
            {
                dst.offset.x = getint(buf);
                dst.offset.y = getint(buf);
                if(!delta)
                {
                    dst.offset.max(0);
                }
                break;
            }
            case VSlot_Scroll:
            {
                dst.scroll.x = getfloat(buf);
                dst.scroll.y = getfloat(buf);
                break;
            }
            case VSlot_Alpha:
            {
                dst.alphafront = std::clamp(getfloat(buf), 0.0f, 1.0f);
                dst.alphaback = std::clamp(getfloat(buf), 0.0f, 1.0f);
                break;
            }
            case VSlot_Color:
            {
                dst.colorscale.r = std::clamp(getfloat(buf), 0.0f, 2.0f);
                dst.colorscale.g = std::clamp(getfloat(buf), 0.0f, 2.0f);
                dst.colorscale.b = std::clamp(getfloat(buf), 0.0f, 2.0f);
                break;
            }
            case VSlot_Refract:
            {
                dst.refractscale = std::clamp(getfloat(buf), 0.0f, 1.0f);
                dst.refractcolor.r = std::clamp(getfloat(buf), 0.0f, 1.0f);
                dst.refractcolor.g = std::clamp(getfloat(buf), 0.0f, 1.0f);
                dst.refractcolor.b = std::clamp(getfloat(buf), 0.0f, 1.0f);
                break;
            }
            default:
            {
                return false;
            }
        }
        dst.changed |= 1<<changed;
    }
    if(buf.overread())
    {
        return false;
    }
    return true;
}

VSlot *findvslot(Slot &slot, const VSlot &src, const VSlot &delta)
{
    for(VSlot *dst = slot.variants; dst; dst = dst->next)
    {
        if((!dst->changed || dst->changed == (src.changed | delta.changed)) &&
           comparevslot(*dst, src, src.changed & ~delta.changed) &&
           comparevslot(*dst, delta, delta.changed))
        {
            return dst;
        }
    }
    return nullptr;
}

static VSlot *clonevslot(const VSlot &src, const VSlot &delta)
{
    VSlot *dst = vslots.add(new VSlot(src.slot, vslots.length()));
    dst->changed = src.changed | delta.changed;
    propagatevslot(*dst, src, ((1 << VSlot_Num) - 1) & ~delta.changed);
    propagatevslot(*dst, delta, delta.changed, true);
    return dst;
}

VARP(autocompactvslots, 0, 256, 0x10000);

VSlot *editvslot(const VSlot &src, const VSlot &delta)
{
    VSlot *exists = findvslot(*src.slot, src, delta);
    if(exists)
    {
        return exists;
    }
    if(vslots.length()>=0x10000)
    {
        ::rootworld.compactvslots();
        rootworld.allchanged();
        if(vslots.length()>=0x10000)
        {
            return nullptr;
        }
    }
    if(autocompactvslots && ++clonedvslots >= autocompactvslots)
    {
        ::rootworld.compactvslots();
        rootworld.allchanged();
    }
    return clonevslot(src, delta);
}

static void fixinsidefaces(cube *c, const ivec &o, int size, int tex)
{
    for(int i = 0; i < 8; ++i)
    {
        ivec co(i, o, size);
        if(c[i].children)
        {
            fixinsidefaces(c[i].children, co, size>>1, tex);
        }
        else
        {
            for(int j = 0; j < 6; ++j)
            {
                if(!visibletris(c[i], j, co, size))
                {
                    c[i].texture[j] = tex;
                }
            }
        }
    }
}

const struct slottex
{
    const char *name;
    int id;
} slottexs[] =
{
    {"0", Tex_Diffuse},
    {"1", Tex_Unknown},

    {"c", Tex_Diffuse},
    {"u", Tex_Unknown},
    {"n", Tex_Normal},
    {"g", Tex_Glow},
    {"s", Tex_Spec},
    {"z", Tex_Depth},
    {"a", Tex_Alpha}
};

int findslottex(const char *name)
{
    for(int i = 0; i < static_cast<int>(sizeof(slottexs)/sizeof(slottex)); ++i)
    {
        if(!std::strcmp(slottexs[i].name, name))
        {
            return slottexs[i].id;
        }
    }
    return -1;
}

static void texture(char *type, char *name, int *rot, int *xoffset, int *yoffset, float *scale)
{
    int tnum = findslottex(type), matslot;
    if(tnum == Tex_Diffuse)
    {
        if(slots.length() >= 0x10000)
        {
            return;
        }
        defslot = slots.add(new Slot(slots.length()));
    }
    else if(!std::strcmp(type, "decal"))
    {
        if(decalslots.length() >= 0x10000)
        {
            return;
        }
        tnum = Tex_Diffuse;
        defslot = decalslots.add(new DecalSlot(decalslots.length()));
    }
    else if((matslot = findmaterial(type)) >= 0)
    {
        tnum = Tex_Diffuse;
        defslot = &materialslots[matslot];
        defslot->reset();
    }
    else if(!defslot)
    {
        return;
    }
    else if(tnum < 0)
    {
        tnum = Tex_Unknown;
    }
    Slot &s = *defslot;
    s.loaded = false;
    s.texmask |= 1<<tnum;
    if(s.sts.length()>=8)
    {
        conoutf(Console_Warn, "warning: too many textures in %s", s.name());
    }
    Slot::Tex &st = s.sts.add();
    st.type = tnum;
    copystring(st.name, name);
    path(st.name);
    if(tnum == Tex_Diffuse)
    {
        setslotshader(s);
        VSlot &vs = s.emptyvslot();
        vs.rotation = std::clamp(*rot, 0, 7);
        vs.offset = ivec2(*xoffset, *yoffset).max(0);
        vs.scale = *scale <= 0 ? 1 : *scale;
        propagatevslot(&vs, (1 << VSlot_Num) - 1);
    }
}
COMMAND(texture, "ssiiif");

void texgrass(char *name)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    DELETEA(s.grass);
    s.grass = name[0] ? newstring(makerelpath("media/texture", name)) : nullptr;
}
COMMAND(texgrass, "s");

void texscroll(float *scrollS, float *scrollT)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.variants->scroll = vec2(*scrollS/1000.0f, *scrollT/1000.0f);
    propagatevslot(s.variants, 1 << VSlot_Scroll);
}
COMMAND(texscroll, "ff");

void texoffset_(int *xoffset, int *yoffset)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.variants->offset = ivec2(*xoffset, *yoffset).max(0);
    propagatevslot(s.variants, 1 << VSlot_Offset);
}
COMMANDN(texoffset, texoffset_, "ii");

void texrotate_(int *rot)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.variants->rotation = std::clamp(*rot, 0, 7);
    propagatevslot(s.variants, 1 << VSlot_Rotation);
}
COMMANDN(texrotate, texrotate_, "i");


void texangle_(float *a)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.variants->angle = vec(*a, std::sin(RAD**a), std::cos(RAD**a));
    propagatevslot(s.variants, 1 << VSlot_Angle);
}
COMMANDN(texangle, texangle_, "f");


void texscale(float *scale)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.variants->scale = *scale <= 0 ? 1 : *scale;
    propagatevslot(s.variants, 1 << VSlot_Scale);
}
COMMAND(texscale, "f");

void texalpha(float *front, float *back)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.variants->alphafront = std::clamp(*front, 0.0f, 1.0f);
    s.variants->alphaback = std::clamp(*back, 0.0f, 1.0f);
    propagatevslot(s.variants, 1 << VSlot_Alpha);
}
COMMAND(texalpha, "ff");

void texcolor(float *r, float *g, float *b)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.variants->colorscale = vec(std::clamp(*r, 0.0f, 2.0f), std::clamp(*g, 0.0f, 2.0f), std::clamp(*b, 0.0f, 2.0f));
    propagatevslot(s.variants, 1 << VSlot_Color);
}
COMMAND(texcolor, "fff");

void texrefract(float *k, float *r, float *g, float *b)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.variants->refractscale = std::clamp(*k, 0.0f, 1.0f);
    if(s.variants->refractscale > 0 && (*r > 0 || *g > 0 || *b > 0))
    {
        s.variants->refractcolor = vec(std::clamp(*r, 0.0f, 1.0f), std::clamp(*g, 0.0f, 1.0f), std::clamp(*b, 0.0f, 1.0f));
    }
    else
    {
        s.variants->refractcolor = vec(1, 1, 1);
    }
    propagatevslot(s.variants, 1 << VSlot_Refract);
}
COMMAND(texrefract, "ffff");

void texsmooth(int *id, int *angle)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.smooth = smoothangle(*id, *angle);
}
COMMAND(texsmooth, "ib");

void decaldepth(float *depth, float *fade)
{
    if(!defslot || defslot->type() != Slot::SlotType_Decal)
    {
        return;
    }
    DecalSlot &s = *static_cast<DecalSlot *>(defslot);
    s.depth = std::clamp(*depth, 1e-3f, 1e3f);
    s.fade = std::clamp(*fade, 0.0f, s.depth);
}
COMMAND(decaldepth, "ff");

static void addglow(ImageData &c, ImageData &g, const vec &glowcolor)
{
    if(g.bpp < 3)
    {
        READ_WRITE_RGB_TEX(c, g,
            for(int k = 0; k < 3; ++k)
            {
                dst[k] = std::clamp(static_cast<int>(dst[k]) + static_cast<int>(src[0]*glowcolor[k]), 0, 255);
            }
        );
    }
    else
    {
        READ_WRITE_RGB_TEX(c, g,
            for(int k = 0; k < 3; ++k)
            {
                dst[k] = std::clamp(static_cast<int>(dst[k]) + static_cast<int>(src[k]*glowcolor[k]), 0, 255);
            }
        );
    }
}

static void mergespec(ImageData &c, ImageData &s)
{
    if(s.bpp < 3)
    {
        READ_WRITE_RGBA_TEX(c, s,
            dst[3] = src[0];
        );
    }
    else
    {
        READ_WRITE_RGBA_TEX(c, s,
            dst[3] = (static_cast<int>(src[0]) + static_cast<int>(src[1]) + static_cast<int>(src[2]))/3;
        );
    }
}

static void mergedepth(ImageData &c, ImageData &z)
{
    READ_WRITE_RGBA_TEX(c, z,
        dst[3] = src[0];
    );
}

static void mergealpha(ImageData &c, ImageData &s)
{
    if(s.bpp < 3)
    {
        READ_WRITE_RGBA_TEX(c, s,
            dst[3] = src[0];
        );
    }
    else
    {
        READ_WRITE_RGBA_TEX(c, s,
            dst[3] = src[3];
        );
    }
}

static void collapsespec(ImageData &s)
{
    ImageData d(s.w, s.h, 1);
    if(s.bpp >= 3)
    {
        READ_WRITE_TEX(d, s,
        {
            dst[0] = (static_cast<int>(src[0]) + static_cast<int>(src[1]) + static_cast<int>(src[2]))/3;
        });
    }
    else
    {
        READ_WRITE_TEX(d, s, { dst[0] = src[0]; });
    }
    s.replace(d);
}

int DecalSlot::cancombine(int type) const
{
    switch(type)
    {
        case Tex_Glow:
        {
            return Tex_Spec;
        }
        case Tex_Normal:
        {
            return texmask&(1 << Tex_Depth) ? Tex_Depth : (texmask & (1 << Tex_Glow) ? -1 : Tex_Spec);
        }
        default:
        {
            return -1;
        }
    }
}

bool DecalSlot::shouldpremul(int type) const
{
    switch(type)
    {
        case Tex_Diffuse:
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}

static void addname(vector<char> &key, Slot &slot, Slot::Tex &t, bool combined = false, const char *prefix = nullptr)
{
    if(combined)
    {
        key.add('&');
    }
    if(prefix)
    {
        while(*prefix)
        {
            key.add(*prefix++);
        }
    }
    DEF_FORMAT_STRING(tname, "%s/%s", slot.texturedir(), t.name);
    for(const char *s = path(tname); *s; key.add(*s++))
    {
        //(empty body)
    }
}

// Slot object

VSlot &Slot::emptyvslot()
{
    return *::emptyvslot(*this);
}

int Slot::findtextype(int type, int last) const
{
    for(int i = last+1; i<sts.length(); i++)
    {
        if((type&(1<<sts[i].type)) && sts[i].combined<0)
        {
            return i;
        }
    }
    return -1;
}

int Slot::cancombine(int type) const
{
    switch(type)
    {
        case Tex_Diffuse:
        {
            return texmask&((1 << Tex_Spec) | (1 << Tex_Normal)) ? Tex_Spec : Tex_Alpha;
        }
        case Tex_Normal:
        {
            return texmask&(1 << Tex_Depth) ? Tex_Depth : Tex_Alpha;
        }
        default:
        {
            return -1;
        }
    }
}

void Slot::load(int index, Slot::Tex &t)
{
    vector<char> key;
    addname(key, *this, t, false, shouldpremul(t.type) ? "<premul>" : nullptr);
    Slot::Tex *combine = nullptr;
    for(int i = 0; i < sts.length(); i++)
    {
        Slot::Tex &c = sts[i];
        if(c.combined == index)
        {
            combine = &c;
            addname(key, *this, c, true);
            break;
        }
    }
    key.add('\0');
    t.t = textures.access(key.getbuf());
    if(t.t)
    {
        return;
    }
    int compress = 0,
        wrap = 0;
    ImageData ts;
    if(!texturedata(ts, *this, t, true, &compress, &wrap))
    {
        t.t = notexture;
        return;
    }
    if(!ts.compressed)
    {
        switch(t.type)
        {
            case Tex_Spec:
            {
                if(ts.bpp > 1)
                {
                    collapsespec(ts);
                }
                break;
            }
            case Tex_Glow:
            case Tex_Diffuse:
            case Tex_Normal:
                if(combine)
                {
                    ImageData cs;
                    if(texturedata(cs, *this, *combine))
                    {
                        if(cs.w!=ts.w || cs.h!=ts.h)
                        {
                            scaleimage(cs, ts.w, ts.h);
                        }
                        switch(combine->type)
                        {
                            case Tex_Spec:
                            {
                                mergespec(ts, cs);
                                break;
                            }
                            case Tex_Depth:
                            {
                                mergedepth(ts, cs);
                                break;
                            }
                            case Tex_Alpha:
                            {
                                mergealpha(ts, cs);
                                break;
                            }
                        }
                    }
                }
                if(ts.bpp < 3)
                {
                    swizzleimage(ts);
                }
                break;
        }
    }
    if(!ts.compressed && shouldpremul(t.type))
    {
        texpremul(ts);
    }
    t.t = newtexture(nullptr, key.getbuf(), ts, wrap, true, true, true, compress);
}

void Slot::load()
{
    linkslotshader(*this);
    for(int i = 0; i < sts.length(); i++)
    {
        Slot::Tex &t = sts[i];
        if(t.combined >= 0)
        {
            continue;
        }
        int combine = cancombine(t.type);
        if(combine >= 0 && (combine = findtextype(1<<combine)) >= 0)
        {
            Slot::Tex &c = sts[combine];
            c.combined = i;
        }
    }
    for(int i = 0; i < sts.length(); i++)
    {
        Slot::Tex &t = sts[i];
        if(t.combined >= 0)
        {
            continue;
        }
        switch(t.type)
        {
            default:
            {
                load(i, t);
                break;
            }
        }
    }
    loaded = true;
}

// end of Slot

MatSlot &lookupmaterialslot(int index, bool load)
{
    MatSlot &s = materialslots[index];
    if(load && !s.linked)
    {
        if(!s.loaded)
        {
            s.load();
        }
        linkvslotshader(s);
        s.linked = true;
    }
    return s;
}

Slot &lookupslot(int index, bool load)
{
    Slot &s = slots.inrange(index) ? *slots[index] : (slots.inrange(Default_Geom) ? *slots[Default_Geom] : dummyslot);
    if(!s.loaded && load)
    {
        s.load();
    }
    return s;
}

VSlot &lookupvslot(int index, bool load)
{
    VSlot &s = vslots.inrange(index) && vslots[index]->slot ? *vslots[index] : (slots.inrange(Default_Geom) && slots[Default_Geom]->variants ? *slots[Default_Geom]->variants : dummyvslot);
    if(load && !s.linked)
    {
        if(!s.slot->loaded)
        {
            s.slot->load();
        }
        linkvslotshader(s);
        s.linked = true;
    }
    return s;
}

DecalSlot &lookupdecalslot(int index, bool load)
{
    DecalSlot &s = decalslots.inrange(index) ? *decalslots[index] : dummydecalslot;
    if(load && !s.linked)
    {
        if(!s.loaded)
        {
            s.load();
        }
        linkvslotshader(s);
        s.linked = true;
    }
    return s;
}

void linkslotshaders()
{
    for(int i = 0; i < slots.length(); i++)
    {
        if(slots[i]->loaded)
        {
            linkslotshader(*slots[i]);
        }
    }
    for(int i = 0; i < vslots.length(); i++)
    {
        if(vslots[i]->linked)
        {
            linkvslotshader(*vslots[i]);
        }
    }
    for(int i = 0; i < (MatFlag_Volume|MatFlag_Index)+1; ++i)
    {
        if(materialslots[i].loaded)
        {
            linkslotshader(materialslots[i]);
            linkvslotshader(materialslots[i]);
        }
    }
    for(int i = 0; i < decalslots.length(); i++)
    {
        if(decalslots[i]->loaded)
        {
            linkslotshader(*decalslots[i]);
            linkvslotshader(*decalslots[i]);
        }
    }
}

static void blitthumbnail(ImageData &d, ImageData &s, int x, int y)
{
    forcergbimage(d);
    forcergbimage(s);
    uchar *dstrow = &d.data[d.pitch*y + d.bpp*x],
          *srcrow = s.data;
    for(int y = 0; y < s.h; ++y)
    {
        for(uchar *dst = dstrow, *src = srcrow, *end = &srcrow[s.w*s.bpp]; src < end; dst += d.bpp, src += s.bpp)
        {
            for(int k = 0; k < 3; ++k)
            {
                dst[k] = src[k];
            }
        }
        dstrow += d.pitch;
        srcrow += s.pitch;
    }
}

Texture *Slot::loadthumbnail()
{
    if(thumbnail)
    {
        return thumbnail;
    }
    if(!variants)
    {
        thumbnail = notexture;
        return thumbnail;
    }
    VSlot &vslot = *variants;
    linkslotshader(*this, false);
    linkvslotshader(vslot, false);
    vector<char> name;
    if(vslot.colorscale == vec(1, 1, 1))
    {
        addname(name, *this, sts[0], false, "<thumbnail>");
    }
    else
    {
        DEF_FORMAT_STRING(prefix, "<thumbnail:%.2f/%.2f/%.2f>", vslot.colorscale.x, vslot.colorscale.y, vslot.colorscale.z);
        addname(name, *this, sts[0], false, prefix);
    }
    int glow = -1;
    if(texmask&(1 << Tex_Glow))
    {
        for(int j = 0; j < sts.length(); j++)
        {
            if(sts[j].type == Tex_Glow)
            {
                glow = j;
                break;
            }
        }
        if(glow >= 0)
        {
            DEF_FORMAT_STRING(prefix, "<glow:%.2f/%.2f/%.2f>", vslot.glowcolor.x, vslot.glowcolor.y, vslot.glowcolor.z);
            addname(name, *this, sts[glow], true, prefix);
        }
    }
    name.add('\0');
    Texture *t = textures.access(path(name.getbuf()));
    if(t)
    {
        thumbnail = t;
    }
    else
    {
        ImageData s, g, l, d;
        texturedata(s, *this, sts[0], false);
        if(glow >= 0)
        {
            texturedata(g, *this, sts[glow], false);
        }
        if(!s.data)
        {
            t = thumbnail = notexture;
        }
        else
        {
            if(vslot.colorscale != vec(1, 1, 1))
            {
                texmad(s, vslot.colorscale, vec(0, 0, 0));
            }
            int xs = s.w, ys = s.h;
            if(s.w > 128 || s.h > 128)
            {
                scaleimage(s, std::min(s.w, 128), std::min(s.h, 128));
            }
            if(g.data)
            {
                if(g.w != s.w || g.h != s.h) scaleimage(g, s.w, s.h);
                addglow(s, g, vslot.glowcolor);
            }
            if(l.data)
            {
                if(l.w != s.w/2 || l.h != s.h/2)
                {
                    scaleimage(l, s.w/2, s.h/2);
                }
                blitthumbnail(s, l, s.w-l.w, s.h-l.h);
            }
            if(d.data)
            {
                if(vslot.colorscale != vec(1, 1, 1))
                {
                    texmad(d, vslot.colorscale, vec(0, 0, 0));
                }
                if(d.w != s.w/2 || d.h != s.h/2)
                {
                    scaleimage(d, s.w/2, s.h/2);
                }
                blitthumbnail(s, d, 0, 0);
            }
            if(s.bpp < 3)
            {
                forcergbimage(s);
            }
            t = newtexture(nullptr, name.getbuf(), s, 0, false, false, true);
            t->xs = xs;
            t->ys = ys;
            thumbnail = t;
        }
    }
    return t;
}

// environment mapped reflections

extern const cubemapside cubemapsides[6] =
{
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "lf", false, true,  true  },
    { GL_TEXTURE_CUBE_MAP_POSITIVE_X, "rt", true,  false, true  },
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "bk", false, false, false },
    { GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "ft", true,  true,  false },
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "dn", true,  false, true  },
    { GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "up", true,  false, true  },
};

void cleanuptexture(Texture *t)
{
    DELETEA(t->alphamask);
    if(t->id)
    {
        glDeleteTextures(1, &t->id);
        t->id = 0;
    }
    if(t->type&Texture::TRANSIENT)
    {
        textures.remove(t->name);
    }
}

void cleanuptextures()
{
    for(int i = 0; i < slots.length(); i++)
    {
        slots[i]->cleanup();
    }
    for(int i = 0; i < vslots.length(); i++)
    {
        vslots[i]->cleanup();
    }
    for(int i = 0; i < (MatFlag_Volume|MatFlag_Index)+1; ++i)
    {
        materialslots[i].cleanup();
    }
    for(int i = 0; i < decalslots.length(); i++)
    {
        decalslots[i]->cleanup();
    }
    ENUMERATE(textures, Texture, tex, cleanuptexture(&tex));
}

bool reloadtexture(const char *name)
{
    Texture *t = textures.access(copypath(name));
    if(t)
    {
        return reloadtexture(*t);
    }
    return true;
}

bool reloadtexture(Texture &tex)
{
    if(tex.id)
    {
        return true;
    }
    switch(tex.type&Texture::TYPE)
    {
        case Texture::IMAGE:
        {
            int compress = 0;
            ImageData s;
            if(!texturedata(s, tex.name, true, &compress) || !newtexture(&tex, nullptr, s, tex.clamp, tex.mipmap, false, false, compress))
            {
                return false;
            }
            break;
        }
    }
    return true;
}

void reloadtex(char *name)
{
    Texture *t = textures.access(copypath(name));
    if(!t)
    {
        conoutf(Console_Error, "texture %s is not loaded", name);
        return;
    }
    if(t->type&Texture::TRANSIENT)
    {
        conoutf(Console_Error, "can't reload transient texture %s", name);
        return;
    }
    DELETEA(t->alphamask);
    Texture oldtex = *t;
    t->id = 0;
    if(!reloadtexture(*t))
    {
        if(t->id)
        {
            glDeleteTextures(1, &t->id);
        }
        *t = oldtex;
        conoutf(Console_Error, "failed to reload texture %s", name);
    }
}

COMMAND(reloadtex, "s");

void reloadtextures()
{
    int reloaded = 0;
    ENUMERATE(textures, Texture, tex,
    {
        loadprogress = static_cast<float>(++reloaded)/textures.numelems;
        reloadtexture(tex);
    });
    loadprogress = 0;
}

static void writepngchunk(stream *f, const char *type, uchar *data = nullptr, uint len = 0)
{
    f->putbig<uint>(len);
    f->write(type, 4);
    f->write(data, len);

    uint crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef *>(type), 4);
    if(data)
    {
        crc = crc32(crc, data, len);
    }
    f->putbig<uint>(crc);
}

VARP(compresspng, 0, 9, 9);

static void savepng(const char *filename, ImageData &image, bool flip)
{
    uchar ctype = 0;
    switch(image.bpp)
    {
        case 1:
        {
            ctype = 0;
            break;
        }
        case 2:
        {
            ctype = 4;
            break;
        }
        case 3:
        {
            ctype = 2;
            break;
        }
        case 4:
        {
            ctype = 6;
            break;
        }
        default:
        {
            conoutf(Console_Error, "failed saving png to %s", filename);
            return;
        }
    }
    stream *f = openfile(filename, "wb");
    if(!f)
    {
        conoutf(Console_Error, "could not write to %s", filename);
        return;
    }
    uchar signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    f->write(signature, sizeof(signature));
    struct pngihdr
    {
        uint width, height;
        uchar bitdepth, colortype, compress, filter, interlace;
    } ihdr = { static_cast<uint>(endianswap(image.w)), static_cast<uint>(endianswap(image.h)), 8, ctype, 0, 0, 0 };
    writepngchunk(f, "IHDR", reinterpret_cast<uchar *>(&ihdr), 13);
    stream::offset idat = f->tell();
    uint len = 0;
    f->write("\0\0\0\0IDAT", 8);
    uint crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef *>("IDAT"), 4);
    z_stream z;
    z.zalloc = nullptr;
    z.zfree = nullptr;
    z.opaque = nullptr;
    if(deflateInit(&z, compresspng) != Z_OK)
    {
        goto error; //goto is beneath FLUSHZ macro
    }
    uchar buf[1<<12];
    z.next_out = static_cast<Bytef *>(buf);
    z.avail_out = sizeof(buf);
    for(int i = 0; i < image.h; ++i)
    {
        uchar filter = 0;
        for(int j = 0; j < 2; ++j)
        {
            z.next_in = j ? static_cast<Bytef *>(image.data) + (flip ? image.h-i-1 : i)*image.pitch : static_cast<Bytef *>(&filter);
            z.avail_in = j ? image.w*image.bpp : 1;
            while(z.avail_in > 0)
            {
                if(deflate(&z, Z_NO_FLUSH) != Z_OK)
                {
                    goto cleanuperror; //goto is beneath FLUSHZ macro
                }
//========================================================================FLUSHZ
                #define FLUSHZ do { \
                    int flush = sizeof(buf) - z.avail_out; \
                    crc = crc32(crc, buf, flush); \
                    len += flush; \
                    f->write(buf, flush); \
                    z.next_out = static_cast<Bytef *>(buf); \
                    z.avail_out = sizeof(buf); \
                } while(0)
                FLUSHZ;
            }
        }
    }

    for(;;)
    {
        int err = deflate(&z, Z_FINISH);
        if(err != Z_OK && err != Z_STREAM_END)
        {
            goto cleanuperror;
        }
        FLUSHZ;
        if(err == Z_STREAM_END)
        {
            break;
        }
    }
#undef FLUSHZ
//==============================================================================
    deflateEnd(&z);

    f->seek(idat, SEEK_SET);
    f->putbig<uint>(len);
    f->seek(0, SEEK_END);
    f->putbig<uint>(crc);
    writepngchunk(f, "IEND");
    delete f;
    return;

cleanuperror:
    deflateEnd(&z);

error:
    delete f;
    conoutf(Console_Error, "failed saving png to %s", filename);
}

SVARP(screenshotdir, "screenshot");

void screenshot(char *filename)
{
    static string buf;
    int dirlen = 0;
    copystring(buf, screenshotdir);
    if(screenshotdir[0])
    {
        dirlen = std::strlen(buf);
        if(buf[dirlen] != '/' && buf[dirlen] != '\\' && dirlen+1 < static_cast<int>(sizeof(buf)))
        {
            buf[dirlen++] = '/';
            buf[dirlen] = '\0';
        }
        const char *dir = findfile(buf, "w");
        if(!fileexists(dir, "w"))
        {
            createdir(dir);
        }
    }
    if(filename[0])
    {
        concatstring(buf, filename);
    }
    else
    {
        string sstime;
        time_t t = std::time(nullptr);
        size_t len = std::strftime(sstime, sizeof(sstime), "%Y-%m-%d_%H.%M.%S.png", std::localtime(&t));
        sstime[std::min(len, sizeof(sstime)-1)] = '\0';
        concatstring(buf, sstime);
        for(char *s = &buf[dirlen]; *s; s++)
        {
            if(iscubespace(*s) || *s == '/' || *s == '\\')
            {
                *s = '-';
            }
        }
    }

    ImageData image(screenw, screenh, 3);
    glPixelStorei(GL_PACK_ALIGNMENT, texalign(image.data, screenw, 3));
    glReadPixels(0, 0, screenw, screenh, GL_RGB, GL_UNSIGNED_BYTE, image.data);
    savepng(path(buf), image, true);
}

COMMAND(screenshot, "s");

//used in libprimis api, avoids having to provide entire Shader iface
void setldrnotexture()
{
    ldrnotextureshader->set();
}

// texture.cpp: texture slot management

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include "SDL_image.h"

#include "imagedata.h"
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

void scaletexture(uchar * RESTRICT src, uint sw, uint sh, uint bpp, uint pitch, uchar * RESTRICT dst, uint dw, uint dh)
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

void reorientnormals(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
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

void reorienttexture(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
{
    switch(bpp)
    {
        case 1: return reorienttexture<1>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 2: return reorienttexture<2>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 3: return reorienttexture<3>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 4: return reorienttexture<4>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
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

static void resizetexture(int w, int h, bool mipmap, GLenum target, int compress, int &tw, int &th)
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

static int texalign(int w, int bpp)
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
        rowalign = texalign(pitch, 1);
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
                std::memcpy(&buf[i*tw*bpp], &(const_cast<uchar *>(reinterpret_cast<const uchar *>(pixels)))[i*pitch], tw*bpp);
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
        int srcalign = row > 0 ? rowalign : texalign(pitch, 1);
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

static void setuptexparameters(int tnum, int clamp, int filter, GLenum format, GLenum target, bool swizzle)
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
        setuptexparameters(tnum, clamp, filter, format, target, swizzle);
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
        resizetexture(w, h, mipmap, target, 0, tw, th);
    }
    uploadtexture(subtarget, component, tw, th, format, type, pixels, pw, ph, pitch, mipmap);
}

void createcompressedtexture(int tnum, int w, int h, const uchar *data, int align, int blocksize, int levels, int clamp, int filter, GLenum format, GLenum subtarget, bool swizzle = false)
{
    GLenum target = textarget(subtarget);
    if(tnum)
    {
        setuptexparameters(tnum, clamp, filter, format, target, swizzle);
    }
    uploadcompressedtexture(target, subtarget, format, w, h, data, align, blocksize, levels, filter > 1);
}

void create3dtexture(int tnum, int w, int h, int d, const void *pixels, int clamp, int filter, GLenum component, GLenum target, bool swizzle)
{
    GLenum format = GL_FALSE, type = textype(component, format);
    if(tnum)
    {
        setuptexparameters(tnum, clamp, filter, format, target, swizzle);
    }
    glTexImage3D_(target, 0, component, w, h, d, 0, format, type, pixels);
}


hashnameset<Texture> textures;

Texture *notexture = nullptr; // used as default, ensured to be loaded

GLenum texformat(int bpp)
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
    format = texformat(s.bpp);
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
        resizetexture(t->w, t->h, mipit, GL_TEXTURE_2D, compress, t->w, t->h);
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
    if(!s.texturedata(t->name, false) || !s.data || s.compressed)
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
    if(s.texturedata(tname, msg, &compress, &clamp))
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
                    std::memcpy(dp.val, sp.val, sizeof(dp.val));
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
            if(sp.name != dp.name || std::memcmp(sp.val, dp.val, sizeof(sp.val)))
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

//used in iengine.h
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

//used in iengine.h
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

void texgrass(char *name)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    delete[] s.grass;
    s.grass = name[0] ? newstring(makerelpath("media/texture", name)) : nullptr;
}

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

void texsmooth(int *id, int *angle)
{
    if(!defslot)
    {
        return;
    }
    Slot &s = *defslot;
    s.smooth = smoothangle(*id, *angle);
}

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
    if(!ts.texturedata(*this, t, true, &compress, &wrap))
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
                    ts.collapsespec();
                }
                break;
            }
            case Tex_Glow:
            case Tex_Diffuse:
            case Tex_Normal:
                if(combine)
                {
                    ImageData cs;
                    if(cs.texturedata(*this, *combine))
                    {
                        if(cs.w!=ts.w || cs.h!=ts.h)
                        {
                            cs.scaleimage(ts.w, ts.h);
                        }
                        switch(combine->type)
                        {
                            case Tex_Spec:
                            {
                                ts.mergespec(cs);
                                break;
                            }
                            case Tex_Depth:
                            {
                                ts.mergedepth(cs);
                                break;
                            }
                            case Tex_Alpha:
                            {
                                ts.mergealpha(cs);
                                break;
                            }
                        }
                    }
                }
                if(ts.bpp < 3)
                {
                    ts.swizzleimage();
                }
                break;
        }
    }
    if(!ts.compressed && shouldpremul(t.type))
    {
        ts.texpremul();
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
    d.forcergbimage();
    s.forcergbimage();
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
        s.texturedata(*this, sts[0], false);
        if(glow >= 0)
        {
            g.texturedata(*this, sts[glow], false);
        }
        if(!s.data)
        {
            t = thumbnail = notexture;
        }
        else
        {
            if(vslot.colorscale != vec(1, 1, 1))
            {
                s.texmad(vslot.colorscale, vec(0, 0, 0));
            }
            int xs = s.w, ys = s.h;
            if(s.w > 128 || s.h > 128)
            {
                s.scaleimage(std::min(s.w, 128), std::min(s.h, 128));
            }
            if(g.data)
            {
                if(g.w != s.w || g.h != s.h) g.scaleimage(s.w, s.h);
                s.addglow(g, vslot.glowcolor);
            }
            if(l.data)
            {
                if(l.w != s.w/2 || l.h != s.h/2)
                {
                    l.scaleimage(s.w/2, s.h/2);
                }
                blitthumbnail(s, l, s.w-l.w, s.h-l.h);
            }
            if(d.data)
            {
                if(vslot.colorscale != vec(1, 1, 1))
                {
                    d.texmad(vslot.colorscale, vec(0, 0, 0));
                }
                if(d.w != s.w/2 || d.h != s.h/2)
                {
                    d.scaleimage(s.w/2, s.h/2);
                }
                blitthumbnail(s, d, 0, 0);
            }
            if(s.bpp < 3)
            {
                s.forcergbimage();
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
    delete[] t->alphamask;
    t->alphamask = nullptr;
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
            if(!s.texturedata(tex.name, true, &compress) || !newtexture(&tex, nullptr, s, tex.clamp, tex.mipmap, false, false, compress))
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
    delete[] t->alphamask;
    t->alphamask = nullptr;
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
    glPixelStorei(GL_PACK_ALIGNMENT, texalign(screenw, 3));
    glReadPixels(0, 0, screenw, screenh, GL_RGB, GL_UNSIGNED_BYTE, image.data);
    savepng(path(buf), image, true);
}

//used in libprimis api, avoids having to provide entire Shader iface
void setldrnotexture()
{
    ldrnotextureshader->set();
}

//globalshaderparam

GlobalShaderParam::GlobalShaderParam(const char *name) : name(name), param(nullptr) {}

GlobalShaderParamState *GlobalShaderParam::resolve()
{
    extern GlobalShaderParamState *getglobalparam(const char *name);
    if(!param)
    {
        param = getglobalparam(name);
    }
    param->changed();
    return param;
}

void GlobalShaderParam::setf(float x, float y, float z, float w)
{
    GlobalShaderParamState *g = resolve();
    g->fval[0] = x;
    g->fval[1] = y;
    g->fval[2] = z;
    g->fval[3] = w;
}

void GlobalShaderParam::set(const vec &v, float w)
{
    setf(v.x, v.y, v.z, w);
}

void GlobalShaderParam::set(const vec2 &v, float z, float w)
{
    setf(v.x, v.y, z, w);
}

void GlobalShaderParam::set(const vec4<float> &v)
{
    setf(v.x, v.y, v.z, v.w);
}

void GlobalShaderParam::set(const plane &p)
{
    setf(p.x, p.y, p.z, p.offset);
}

void GlobalShaderParam::set(const matrix2 &m)
{
    std::memcpy(resolve()->fval, m.a.v, sizeof(m));
}

void GlobalShaderParam::set(const matrix3 &m)
{
    std::memcpy(resolve()->fval, m.a.v, sizeof(m));
}

void GlobalShaderParam::set(const matrix4 &m)
{
    std::memcpy(resolve()->fval, m.a.v, sizeof(m));
}

void GlobalShaderParam::seti(int x, int y, int z, int w)
{
    GlobalShaderParamState *g = resolve();
    g->ival[0] = x;
    g->ival[1] = y;
    g->ival[2] = z;
    g->ival[3] = w;
}
void GlobalShaderParam::set(const ivec &v, int w)
{
    seti(v.x, v.y, v.z, w);
}

void GlobalShaderParam::set(const ivec2 &v, int z, int w)
{
    seti(v.x, v.y, z, w);
}

void GlobalShaderParam::set(const vec4<int> &v)
{
    seti(v.x, v.y, v.z, v.w);
}

void GlobalShaderParam::setu(uint x, uint y, uint z, uint w)
{
    GlobalShaderParamState *g = resolve();
    g->uval[0] = x;
    g->uval[1] = y;
    g->uval[2] = z;
    g->uval[3] = w;
}

//localshaderparam

LocalShaderParam::LocalShaderParam(const char *name) : name(name), loc(-1) {}

LocalShaderParamState *LocalShaderParam::resolve()
{
    Shader *s = Shader::lastshader;
    if(!s)
    {
        return nullptr;
    }
    if(!s->localparamremap.inrange(loc))
    {
        extern int getlocalparam(const char *name);
        if(loc == -1)
        {
            loc = getlocalparam(name);
        }
        if(!s->localparamremap.inrange(loc))
        {
            return nullptr;
        }
    }
    uchar remap = s->localparamremap[loc];
    return s->localparams.inrange(remap) ? &s->localparams[remap] : nullptr;
}

void LocalShaderParam::setf(float x, float y, float z, float w)
{
    ShaderParamBinding *b = resolve();
    if(b) switch(b->format)
    {
        case GL_BOOL:
        case GL_FLOAT:
        {
            glUniform1f(b->loc, x);
            break;
        }
        case GL_BOOL_VEC2:
        case GL_FLOAT_VEC2:
        {
            glUniform2f(b->loc, x, y);
            break;
        }
        case GL_BOOL_VEC3:
        case GL_FLOAT_VEC3:
        {
            glUniform3f(b->loc, x, y, z);
            break;
        }
        case GL_BOOL_VEC4:
        case GL_FLOAT_VEC4:
        {
            glUniform4f(b->loc, x, y, z, w);
            break;
        }
        case GL_INT:
        {
            glUniform1i(b->loc, static_cast<int>(x));
            break;
        }
        case GL_INT_VEC2:
        {
            glUniform2i(b->loc, static_cast<int>(x), static_cast<int>(y));
            break;
        }
        case GL_INT_VEC3:
        {
            glUniform3i(b->loc, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
            break;
        }
        case GL_INT_VEC4:
        {
            glUniform4i(b->loc, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z), static_cast<int>(w));
            break;
        }
        case GL_UNSIGNED_INT:
        {
            glUniform1ui_(b->loc, static_cast<uint>(x));
            break;
        }
        case GL_UNSIGNED_INT_VEC2:
        {
            glUniform2ui_(b->loc, static_cast<uint>(x), static_cast<uint>(y));
            break;
        }
        case GL_UNSIGNED_INT_VEC3:
        {
            glUniform3ui_(b->loc, static_cast<uint>(x), static_cast<uint>(y), static_cast<uint>(z));
            break;
        }
        case GL_UNSIGNED_INT_VEC4:
        {
            glUniform4ui_(b->loc, static_cast<uint>(x), static_cast<uint>(y), static_cast<uint>(z), static_cast<uint>(w));
            break;
        }
    }
}

void LocalShaderParam::set(const vec &v, float w)
{
    setf(v.x, v.y, v.z, w);
}

void LocalShaderParam::set(const vec2 &v, float z, float w)
{
    setf(v.x, v.y, z, w);
}

void LocalShaderParam::set(const vec4<float> &v)
{
    setf(v.x, v.y, v.z, v.w);
}

void LocalShaderParam::set(const plane &p)
{
    setf(p.x, p.y, p.z, p.offset);
}

void LocalShaderParam::setv(const float *f, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform1fv(b->loc, n, f);
    }
}

void LocalShaderParam::setv(const vec *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform3fv(b->loc, n, v->v);
    }
}

void LocalShaderParam::setv(const vec2 *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform2fv(b->loc, n, v->v);
    }
}

void LocalShaderParam::setv(const vec4<float> *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform4fv(b->loc, n, v->v);
    }
}

void LocalShaderParam::setv(const plane *p, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform4fv(b->loc, n, p->v);
    }
}

void LocalShaderParam::setv(const matrix2 *m, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniformMatrix2fv(b->loc, n, GL_FALSE, m->a.v);
    }
}

void LocalShaderParam::setv(const matrix3 *m, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniformMatrix3fv(b->loc, n, GL_FALSE, m->a.v);
    }
}

void LocalShaderParam::setv(const matrix4 *m, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniformMatrix4fv(b->loc, n, GL_FALSE, m->a.v);
    }
}

void LocalShaderParam::set(const matrix2 &m)
{
    setv(&m);
}

void LocalShaderParam::set(const matrix3 &m)
{
    setv(&m);
}

void LocalShaderParam::set(const matrix4 &m)
{
    setv(&m);
}

template<class T>
void sett(T x, T y, T z, T w, LocalShaderParam & sh)
{
    ShaderParamBinding *b = sh.resolve();
    if(b)
    {
        switch(b->format)
        {
            case GL_FLOAT:
            {
                glUniform1f(b->loc, x);
                break;
            }
            case GL_FLOAT_VEC2:
            {
                glUniform2f(b->loc, x, y);
                break;
            }
            case GL_FLOAT_VEC3:
            {
                glUniform3f(b->loc, x, y, z);
                break;
            }
            case GL_FLOAT_VEC4:
            {
                glUniform4f(b->loc, x, y, z, w);
                break;
            }
            case GL_BOOL:
            case GL_INT:
            {
                glUniform1i(b->loc, x);
                break;
            }
            case GL_BOOL_VEC2:
            case GL_INT_VEC2:
            {
                glUniform2i(b->loc, x, y);
                break;
            }
            case GL_BOOL_VEC3:
            case GL_INT_VEC3:
            {
                glUniform3i(b->loc, x, y, z);
                break;
            }
            case GL_BOOL_VEC4:
            case GL_INT_VEC4:
            {
                glUniform4i(b->loc, x, y, z, w);
                break;
            }
            case GL_UNSIGNED_INT:
            {
                glUniform1ui_(b->loc, x);
                break;
            }
            case GL_UNSIGNED_INT_VEC2:
            {
                glUniform2ui_(b->loc, x, y);
                break;
            }
            case GL_UNSIGNED_INT_VEC3:
            {
                glUniform3ui_(b->loc, x, y, z);
                break;
            }
            case GL_UNSIGNED_INT_VEC4:
            {
                glUniform4ui_(b->loc, x, y, z, w);
                break;
            }
        }
    }
}
void LocalShaderParam::seti(int x, int y, int z, int w)
{
    sett<int>(x, y, z, w, *this);
}

void LocalShaderParam::set(const ivec &v, int w)
{
    seti(v.x, v.y, v.z, w);
}

void LocalShaderParam::set(const ivec2 &v, int z, int w)
{
    seti(v.x, v.y, z, w);
}

void LocalShaderParam::set(const vec4<int> &v)
{
    seti(v.x, v.y, v.z, v.w);
}

void LocalShaderParam::setv(const int *i, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform1iv(b->loc, n, i);
    }
}
void LocalShaderParam::setv(const ivec *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform3iv(b->loc, n, v->v);
    }
}
void LocalShaderParam::setv(const ivec2 *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform2iv(b->loc, n, v->v);
    }
}
void LocalShaderParam::setv(const vec4<int> *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform4iv(b->loc, n, v->v);
    }
}

void LocalShaderParam::setu(uint x, uint y, uint z, uint w)
{
    sett<uint>(x, y, z, w, *this);
}

void LocalShaderParam::setv(const uint *u, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform1uiv_(b->loc, n, u);
    }
}
void inittexturecmds() {
    addcommand("texturereset", reinterpret_cast<identfun>(texturereset), "i", Id_Command);
    addcommand("materialreset", reinterpret_cast<identfun>(materialreset), "", Id_Command);
    addcommand("decalreset", reinterpret_cast<identfun>(decalreset), "i", Id_Command);
    addcommand("compactvslots", reinterpret_cast<identfun>(+[](int *cull){multiplayerwarn();rootworld.compactvslots(*cull!=0);rootworld.allchanged();}), "i", Id_Command);
    addcommand("texture", reinterpret_cast<identfun>(texture), "ssiiif", Id_Command);
    addcommand("texgrass", reinterpret_cast<identfun>(texgrass), "s", Id_Command);
    addcommand("texscroll", reinterpret_cast<identfun>(texscroll), "ff", Id_Command);
    addcommand("texoffset", reinterpret_cast<identfun>(texoffset_), "ii", Id_Command);
    addcommand("texrotate", reinterpret_cast<identfun>(texrotate_), "i", Id_Command);
    addcommand("texangle", reinterpret_cast<identfun>(texangle_), "f", Id_Command);
    addcommand("texscale", reinterpret_cast<identfun>(texscale), "f", Id_Command);
    addcommand("texalpha", reinterpret_cast<identfun>(texalpha), "ff", Id_Command);
    addcommand("texcolor", reinterpret_cast<identfun>(texcolor), "fff", Id_Command);
    addcommand("texrefract", reinterpret_cast<identfun>(texrefract), "ffff", Id_Command);
    addcommand("texsmooth", reinterpret_cast<identfun>(texsmooth), "ib", Id_Command);
    addcommand("decaldepth", reinterpret_cast<identfun>(decaldepth), "ff", Id_Command);
    addcommand("reloadtex", reinterpret_cast<identfun>(reloadtex), "s", Id_Command);
    addcommand("screenshot", reinterpret_cast<identfun>(screenshot), "s", Id_Command);
}


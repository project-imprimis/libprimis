// texture.cpp: texture slot management

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include "SDL_image.h"

#include "imagedata.h"
#include "octarender.h"
#include "renderwindow.h"
#include "shader.h"
#include "shaderparam.h"
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

//copies every other pixel into a destination buffer
//sw,sh are source width/height
template<int BPP>
static void halvetexture(const uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst)
{
    for(const uchar *yend = &src[sh*stride]; src < yend;)
    {
        for(const uchar *xend = &src[sw*BPP], *xsrc = src; xsrc < xend; xsrc += 2*BPP, dst += BPP)
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
static void shifttexture(const uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst, uint dw, uint dh)
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
    for(const uchar *yend = &src[sh*stride]; src < yend;)
    {
        for(const uchar *xend = &src[sw*BPP], *xsrc = src; xsrc < xend; xsrc += wfrac*BPP, dst += BPP)
        {

            uint t[BPP] = {0};
            for(const uchar *ycur = xsrc, *xend = &ycur[wfrac*BPP], *yend = &src[hfrac*stride];
                ycur < yend;
                ycur += stride, xend += stride)
            {
                //going to (xend - 1) seems to be necessary to avoid buffer overrun
                for(const uchar *xcur = ycur; xcur < xend -1; xcur += BPP)
                {
                    for(int i = 0; i < BPP; ++i)
                    {
                        t[i] += xcur[i];
                    }
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
static void scaletexture(const uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst, uint dw, uint dh)
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

void scaletexture(const uchar * RESTRICT src, uint sw, uint sh, uint bpp, uint pitch, uchar * RESTRICT dst, uint dw, uint dh)
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

template<int BPP>
static void reorienttexture(const uchar * RESTRICT src, int sw, int sh, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
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
    const uchar *srcrow = src;
    for(int i = 0; i < sh; ++i)
    {
        uchar* curdst;
        const uchar* src;
        const uchar* end;
        for(curdst = dst, src = srcrow, end = &srcrow[sw*BPP]; src < end;)
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

void reorienttexture(const uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
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

static int formatsize(GLenum format)
{
    switch(format)
    {
        case GL_RED:
        {
            return 1;
        }
        case GL_RG:
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
    tw = w;
    th = h;
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
                glCompressedTexImage1D(subtarget, level, format, w, 0, size, data);
            }
            else
            {
                glCompressedTexImage2D(subtarget, level, format, w, h, 0, size, data);
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

static void setuptexparameters(int tnum, const void *pixels, int clamp, int filter, GLenum format, GLenum target, bool swizzle)
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

static void createcompressedtexture(int tnum, int w, int h, const uchar *data, int align, int blocksize, int levels, int clamp, int filter, GLenum format, GLenum subtarget, bool swizzle = false)
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
    glTexImage3D(target, 0, component, w, h, d, 0, format, type, pixels);
}

std::unordered_map<std::string, Texture> textures;

Texture *notexture = nullptr; // used as default, ensured to be loaded

GLenum texformat(int bpp, bool swizzle)
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
        auto itr = textures.insert_or_assign(key, Texture()).first;
        t = &(*itr).second;
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

const uchar * Texture::loadalphamask()
{
    if(alphamask)
    {
        return alphamask;
    }
    if(!(type&Texture::ALPHA))
    {
        return nullptr;
    }
    ImageData s;
    if(!s.texturedata(name, false) || !s.data || s.compressed)
    {
        return nullptr;
    }
    alphamask = new uchar[s.h * ((s.w+7)/8)];
    uchar *srcrow = s.data,
          *dst = alphamask-1;
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
    return alphamask;
}

float Texture::ratio() const
{
    return (w / static_cast<float>(h));
}

Texture *textureload(const char *name, int clamp, bool mipit, bool msg)
{
    std::string tname(name);
    auto itr = textures.find(path(tname));
    if(itr != textures.end())
    {
        return &(*itr).second;
    }
    int compress = 0;
    ImageData s;
    if(s.texturedata(tname.c_str(), msg, &compress, &clamp))
    {
        return newtexture(nullptr, tname.c_str(), s, clamp, mipit, false, false, compress);
    }
    return notexture;
}

bool settexture(const char *name, int clamp)
{
    Texture *t = textureload(name, clamp, true, false);
    glBindTexture(GL_TEXTURE_2D, t->id);
    return t != notexture;
}

std::vector<VSlot *> vslots;
std::vector<Slot *> slots;
MatSlot materialslots[(MatFlag_Volume|MatFlag_Index)+1];
Slot dummyslot;
VSlot dummyvslot(&dummyslot);
static std::vector<DecalSlot *> decalslots;
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
    int limit = std::clamp(*n, 0, static_cast<int>(slots.size()));
    for(size_t i = limit; i < slots.size(); i++)
    {
        Slot *s = slots[i];
        for(VSlot *vs = s->variants; vs; vs = vs->next)
        {
            vs->slot = &dummyslot;
        }
        delete s;
    }
    slots.resize(limit);
    while(vslots.size())
    {
        VSlot *vs = vslots.back();
        if(vs->slot != &dummyslot || vs->changed)
        {
            break;
        }
        delete vslots.back();
        vslots.pop_back();
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

void decalreset(const int *n)
{
    if(!(identflags&Idf_Overridden) && !allowediting)
    {
        return;
    }
    defslot = nullptr;
    resetslotshader();
    for(uint i = *n; i < decalslots.size(); ++i)
    {
        delete decalslots.at(i);
    }
    decalslots.resize(*n);
}

static int compactedvslots = 0,
           compactvslotsprogress = 0,
           clonedvslots = 0;
static bool markingvslots = false;

void clearslots()
{
    defslot = nullptr;
    resetslotshader();
    for(Slot * i : slots)
    {
        delete i;
    }
    slots.clear();
    for(VSlot * i : vslots)
    {
        delete i;
    }
    vslots.clear();
    for(int i = 0; i < (MatFlag_Volume|MatFlag_Index)+1; ++i)
    {
        materialslots[i].reset();
    }
    decalslots.clear();
    clonedvslots = 0;
}

static void assignvslot(VSlot &vs)
{
    vs.index = compactedvslots++;
}

void compactvslot(int &index)
{
    if(static_cast<long>(vslots.size()) > index)
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

//n will be capped at 8
void compactvslots(cube * const c, int n)
{
    if((compactvslotsprogress++&0xFFF)==0)
    {
        renderprogress(std::min(static_cast<float>(compactvslotsprogress)/allocnodes, 1.0f), markingvslots ? "marking slots..." : "compacting slots...");
    }
    for(int i = 0; i < std::min(n, 8); ++i)
    {
        if(c[i].children)
        {
            compactvslots(c[i].children->data());
        }
        else
        {
            for(int j = 0; j < 6; ++j)
            {
                if(vslots.size() > c[i].texture[j])
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
    if(!worldroot)
    {
        conoutf(Console_Error, "no cube to compact");
        return 0;
    }
    defslot = nullptr;
    clonedvslots = 0;
    markingvslots = cull;
    compactedvslots = 0;
    compactvslotsprogress = 0;
    for(uint i = 0; i < vslots.size(); i++)
    {
        vslots[i]->index = -1;
    }
    if(cull)
    {
        uint numdefaults = std::min(static_cast<uint>(Default_NumDefaults), static_cast<uint>(slots.size()));
        for(uint i = 0; i < numdefaults; ++i)
        {
            slots[i]->variants->index = compactedvslots++;
        }
    }
    else
    {
        for(const Slot *i : slots)
        {
            i->variants->index = compactedvslots++;
        }
        for(const VSlot *i : vslots)
        {
            if(!i->changed && i->index < 0)
            {
                markingvslots = true;
                break;
            }
        }
    }
    ::compactvslots(worldroot->data());
    int total = compactedvslots;
    compacteditvslots();
    for(VSlot *i : vslots)
    {
        if(i->changed)
        {
            continue;
        }
        while(i->next)
        {
            if(i->next->index < 0)
            {
                i->next = i->next->next;
            }
            else
            {
                i = i->next;
            }
        }
    }
    if(markingvslots)
    {
        markingvslots = false;
        compactedvslots = 0;
        compactvslotsprogress = 0;
        int lastdiscard = 0;
        for(uint i = 0; i < vslots.size(); i++)
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
                    while(lastdiscard < static_cast<int>(i))
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
        ::compactvslots(worldroot->data());
        total = compactedvslots;
        compacteditvslots();
    }
    compactmruvslots();
    if(cull)
    {
        for(int i = static_cast<int>(slots.size()); --i >=0;) //note reverse iteration
        {
            if(slots[i]->variants->index < 0)
            {
                delete slots.at(i);
                slots.erase(slots.begin() + i);
            }
        }
        for(uint i = 0; i < slots.size(); i++)
        {
            slots[i]->index = i;
        }
    }
    for(uint i = 0; i < vslots.size(); i++)
    {
        while(vslots[i]->index >= 0 && vslots[i]->index != static_cast<int>(i))
        {
            std::swap(vslots[i], vslots[vslots[i]->index]);
        }
    }
    for(uint i = compactedvslots; i < vslots.size(); i++)
    {
        delete vslots[i];
    }
    vslots.resize(compactedvslots);
    return total;
}

static void clampvslotoffset(VSlot &dst, Slot *slot = nullptr)
{
    if(!slot)
    {
        slot = dst.slot;
    }
    if(slot && slot->sts.size())
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
        for(uint i = 0; i < src.params.size(); i++)
        {
            dst.params.push_back(src.params[i]);
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

static void propagatevslot(const VSlot *root, int changed)
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
        for(uint i = 0; i < src.params.size(); i++)
        {
            const SlotShaderParam &sp = src.params[i];
            for(uint j = 0; j < dst.params.size(); j++)
            {
                SlotShaderParam &dp = dst.params[j];
                if(sp.name == dp.name)
                {
                    std::memcpy(dp.val, sp.val, sizeof(dp.val));
                    goto nextparam; //bail out of loop
                }
            }
            dst.params.push_back(sp);
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
    for(int i = static_cast<int>(slots.size()); --i >=0;) //note reverse iteration
    {
        if(slots[i]->variants)
        {
            offset = slots[i]->variants->index + 1;
            break;
        }
    }
    for(uint i = offset; i < vslots.size(); i++)
    {
        if(!vslots[i]->changed)
        {
            return reassignvslot(owner, vslots[i]);
        }
    }
    vslots.push_back(new VSlot(&owner, vslots.size()));
    return vslots.back();
}

static bool comparevslot(const VSlot &dst, const VSlot &src, int diff)
{
    if(diff & (1 << VSlot_ShParam))
    {
        if(src.params.size() != dst.params.size())
        {
            return false;
        }
        for(uint i = 0; i < src.params.size(); i++)
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

void packvslot(std::vector<uchar> &buf, const VSlot &src)
{
    if(src.changed & (1 << VSlot_ShParam))
    {
        for(uint i = 0; i < src.params.size(); i++)
        {
            const SlotShaderParam &p = src.params[i];
            buf.push_back(VSlot_ShParam);
            sendstring(p.name, buf);
            for(int j = 0; j < 4; ++j)
            {
                putfloat(buf, p.val[j]);
            }
        }
    }
    if(src.changed & (1 << VSlot_Scale))
    {
        buf.push_back(VSlot_Scale);
        putfloat(buf, src.scale);
    }
    if(src.changed & (1 << VSlot_Rotation))
    {
        buf.push_back(VSlot_Rotation);
        putint(buf, src.rotation);
    }
    if(src.changed & (1 << VSlot_Angle))
    {
        buf.push_back(VSlot_Angle);
        putfloat(buf, src.angle.x);
        putfloat(buf, src.angle.y);
        putfloat(buf, src.angle.z);
    }
    if(src.changed & (1 << VSlot_Offset))
    {
        buf.push_back(VSlot_Offset);
        putint(buf, src.offset.x);
        putint(buf, src.offset.y);
    }
    if(src.changed & (1 << VSlot_Scroll))
    {
        buf.push_back(VSlot_Scroll);
        putfloat(buf, src.scroll.x);
        putfloat(buf, src.scroll.y);
    }
    if(src.changed & (1 << VSlot_Alpha))
    {
        buf.push_back(VSlot_Alpha);
        putfloat(buf, src.alphafront);
        putfloat(buf, src.alphaback);
    }
    if(src.changed & (1 << VSlot_Color))
    {
        buf.push_back(VSlot_Color);
        putfloat(buf, src.colorscale.r);
        putfloat(buf, src.colorscale.g);
        putfloat(buf, src.colorscale.b);
    }
    if(src.changed & (1 << VSlot_Refract))
    {
        buf.push_back(VSlot_Refract);
        putfloat(buf, src.refractscale);
        putfloat(buf, src.refractcolor.r);
        putfloat(buf, src.refractcolor.g);
        putfloat(buf, src.refractcolor.b);
    }
    buf.push_back(0xFF);
}

//used in iengine.h
void packvslot(std::vector<uchar> &buf, int index)
{
    if(static_cast<long>(vslots.size()) > index)
    {
        packvslot(buf, *vslots[index]);
    }
    else
    {
        buf.push_back(0xFF);
    }
}

//used in iengine.h
void packvslot(std::vector<uchar> &buf, const VSlot *vs)
{
    if(vs)
    {
        packvslot(buf, *vs);
    }
    else
    {
        buf.push_back(0xFF);
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
                SlotShaderParam p = { name[0] ? getshaderparamname(name) : nullptr, SIZE_MAX, 0, { 0, 0, 0, 0 } };
                for(int i = 0; i < 4; ++i)
                {
                    p.val[i] = getfloat(buf);
                }
                if(p.name)
                {
                    dst.params.push_back(p);
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

VSlot *findvslot(const Slot &slot, const VSlot &src, const VSlot &delta)
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
    vslots.push_back(new VSlot(src.slot, vslots.size()));
    VSlot *dst = vslots.back();
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
    if(vslots.size()>=0x10000)
    {
        ::rootworld.compactvslots();
        rootworld.allchanged();
        if(vslots.size()>=0x10000)
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

static void fixinsidefaces(std::array<cube, 8> &c, const ivec &o, int size, int tex)
{
    for(int i = 0; i < 8; ++i)
    {
        ivec co(i, o, size);
        if(c[i].children)
        {
            fixinsidefaces(*(c[i].children), co, size>>1, tex);
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

int findslottex(const char *name)
{

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
        if(slots.size() >= 0x10000)
        {
            return;
        }
        slots.push_back(new Slot(slots.size()));
        defslot = slots.back();
    }
    else if(!std::strcmp(type, "decal"))
    {
        if(decalslots.size() >= 0x10000)
        {
            return;
        }
        tnum = Tex_Diffuse;
        decalslots.push_back(new DecalSlot(decalslots.size()));
        defslot = decalslots.back();
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
    if(s.sts.size()>=8)
    {
        conoutf(Console_Warn, "warning: too many textures in %s", s.name());
    }
    s.sts.emplace_back();
    Slot::Tex &st = s.sts.back();
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
    s.variants->angle = vec(*a, std::sin(*a/RAD), std::cos(*a/RAD));
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

static void addname(std::vector<char> &key, Slot &slot, Slot::Tex &t, bool combined = false, const char *prefix = nullptr)
{
    if(combined)
    {
        key.push_back('&');
    }
    if(prefix)
    {
        while(*prefix)
        {
            key.push_back(*prefix++);
        }
    }
    DEF_FORMAT_STRING(tname, "%s/%s", slot.texturedir(), t.name);
    for(const char *s = path(tname); *s; key.push_back(*s++))
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
    for(uint i = last+1; i<sts.size(); i++)
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
    std::vector<char> key;
    addname(key, *this, t, false, shouldpremul(t.type) ? "<premul>" : nullptr);
    Slot::Tex *combine = nullptr;
    for(uint i = 0; i < sts.size(); i++)
    {
        Slot::Tex &c = sts[i];
        if(c.combined == index)
        {
            combine = &c;
            addname(key, *this, c, true);
            break;
        }
    }
    key.push_back('\0');
    auto itr = textures.find(key.data());
    if(itr != textures.end())
    {
        t.t = &(*itr).second;
        return;
    }
    t.t = nullptr;
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
    t.t = newtexture(nullptr, key.data(), ts, wrap, true, true, true, compress);
}

void Slot::load()
{
    linkslotshader(*this);
    for(uint i = 0; i < sts.size(); i++)
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
    for(uint i = 0; i < sts.size(); i++)
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

// VSlot

void VSlot::addvariant(Slot *slot)
{
    if(!slot->variants)
    {
        slot->variants = this;
    }
    else
    {
        VSlot *prev = slot->variants;
        while(prev->next)
        {
            prev = prev->next;
        }
        prev->next = this;
    }
}

bool VSlot::isdynamic() const
{
    return !scroll.iszero() || slot->shader->isdynamic();
}

// end of Slot/VSlot
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
    Slot &s = (static_cast<long>(slots.size()) > index) ? *slots[index] : ((slots.size() > Default_Geom) ? *slots[Default_Geom] : dummyslot);
    if(!s.loaded && load)
    {
        s.load();
    }
    return s;
}

VSlot &lookupvslot(int index, bool load)
{
    VSlot &s = (static_cast<long>(vslots.size()) > index) && vslots[index]->slot ? *vslots[index] : ((slots.size() > Default_Geom) && slots[Default_Geom]->variants ? *slots[Default_Geom]->variants : dummyvslot);
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
    DecalSlot &s = (static_cast<int>(decalslots.size()) > index) ? *decalslots[index] : dummydecalslot;
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
    for(Slot * const &i : slots)
    {
        if(i->loaded)
        {
            linkslotshader(*i);
        }
    }
    for(VSlot * const &i : vslots)
    {
        if(i->linked)
        {
            linkvslotshader(*i);
        }
    }
    for(uint i = 0; i < (MatFlag_Volume|MatFlag_Index)+1; ++i)
    {
        if(materialslots[i].loaded)
        {
            linkslotshader(materialslots[i]);
            linkvslotshader(materialslots[i]);
        }
    }
    for(DecalSlot * const &i : decalslots)
    {
        if(i->loaded)
        {
            linkslotshader(*i);
            linkvslotshader(*i);
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
    std::vector<char> name;
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
        for(uint j = 0; j < sts.size(); j++)
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
    name.push_back('\0');
    auto itr = textures.find(path(name.data()));
    if(itr != textures.end())
    {
        thumbnail = &(*itr).second;
        return &(*itr).second;
    }
    else
    {
        auto insert = textures.insert( { std::string(name.data()), Texture() } ).first;
        Texture *t = &(*insert).second;
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
            t = newtexture(nullptr, name.data(), s, 0, false, false, true);
            t->xs = xs;
            t->ys = ys;
            thumbnail = t;
        }
        return t;
    }
}

// environment mapped reflections

void cleanuptextures()
{
    for(Slot * const &i : slots)
    {
        i->cleanup();
    }
    for(VSlot * const &i : vslots)
    {
        i->cleanup();
    }
    for(uint i = 0; i < (MatFlag_Volume|MatFlag_Index)+1; ++i)
    {
        materialslots[i].cleanup();
    }
    for(DecalSlot * const &i : decalslots)
    {
        i->cleanup();
    }
    for(auto itr = textures.begin(); itr != textures.end(); ++itr)
    {
        Texture &t = (*itr).second;
        delete[] t.alphamask;
        t.alphamask = nullptr;
        if(t.id)
        {
            glDeleteTextures(1, &t.id);
            t.id = 0;
        }
        if(t.type&Texture::TRANSIENT)
        {
            itr = textures.erase(itr);
        }
    }
}

bool reloadtexture(const char *name)
{
    auto itr = textures.find(path(std::string(name)));
    if(itr != textures.end())
    {
        return (*itr).second.reload();
    }
    return true;
}

bool Texture::reload()
{
    if(id)
    {
        return true;
    }
    switch(type&TYPE)
    {
        case IMAGE:
        {
            int compress = 0;
            ImageData s;
            if(!s.texturedata(name, true, &compress) || !newtexture(this, nullptr, s, clamp, mipmap, false, false, compress))
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
    auto itr = textures.find(path(std::string(name)));
    if(itr == textures.end())
    {
        conoutf(Console_Error, "texture %s is not loaded", name);
        return;
    }
    Texture *t = &(*itr).second;
    if(t->type&Texture::TRANSIENT)
    {
        conoutf(Console_Error, "can't reload transient texture %s", name);
        return;
    }
    delete[] t->alphamask;
    t->alphamask = nullptr;
    Texture oldtex = *t;
    t->id = 0;
    if(!t->reload())
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
    for(auto &[k, t] : textures)
    {
        loadprogress = static_cast<float>(++reloaded)/textures.size();
        t.reload();
    }
    loadprogress = 0;
}

static void writepngchunk(stream *f, const char *type, const uchar *data = nullptr, uint len = 0)
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

static void flushzip(z_stream &z, uchar *buf, const uint &buflen, uint &len, stream *f, uint &crc)
{
    int flush = buflen- z.avail_out;
    crc = crc32(crc, buf, flush);
    len += flush;
    f->write(buf, flush);
    z.next_out = static_cast<Bytef *>(buf);
    z.avail_out = buflen;
}

static void savepng(const char *filename, const ImageData &image, bool flip)
{
    if(!image.h || !image.w)
    {
        conoutf(Console_Error, "cannot save 0-size png");
        return;
    }
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
        uint width,
             height;
        uchar bitdepth,
              colortype,
              compress,
              filter,
              interlace;
    };
    pngihdr ihdr =
    {
        static_cast<uint>(endianswap(image.w)),
        static_cast<uint>(endianswap(image.h)),
        8,
        ctype,
        0,
        0,
        0
    };
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
                flushzip(z, buf, sizeof(buf), len, f, crc);
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
        flushzip(z, buf, sizeof(buf), len, f, crc);
        if(err == Z_STREAM_END)
        {
            break;
        }
    }
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

void inittexturecmds()
{
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


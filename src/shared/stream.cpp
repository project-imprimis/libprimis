/* stream.cpp: utilities for character streams
 *
 * stream.cpp defines character handling to enable character streams to be written
 * into and out of files
 * also included is utilities for gz archive support
 *
 */
#include "../libprimis-headers/cube.h"
#include "stream.h"

#include "../engine/interface/console.h"

///////////////////////// character conversion /////////////////////////////////

#define CUBECTYPE(s, p, d, a, A, u, U) \
    0, U, U, U, U, U, U, U, U, s, s, s, s, s, U, U, \
    U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, \
    s, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, \
    d, d, d, d, d, d, d, d, d, d, p, p, p, p, p, p, \
    p, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, \
    A, A, A, A, A, A, A, A, A, A, A, p, p, p, p, p, \
    p, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, \
    a, a, a, a, a, a, a, a, a, a, a, p, p, p, p, U, \
    U, u, u, u, u, u, u, u, u, u, u, u, u, u, u, u, \
    u, u, u, u, u, u, u, u, u, u, u, u, u, u, u, U, \
    u, U, u, U, u, U, u, U, u, U, u, U, u, U, u, U, \
    u, U, u, U, u, U, u, U, u, U, u, U, u, U, u, U, \
    u, U, u, U, u, U, u, U, U, u, U, u, U, u, U, U, \
    U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, \
    U, U, U, U, u, u, u, u, u, u, u, u, u, u, u, u, \
    u, u, u, u, u, u, u, u, u, u, u, u, u, u, U, u

/* note here:
 * these vars are declared extern inline to allow a `const` (implicitly also
 * `static`) to be linked to other files as a `const`.
 *
 * these vars cannot be `constexpr` due to it not being legal to define constexpr
 * prototypes in headers
 */

extern const uchar cubectype[256] =
{
    CUBECTYPE(CubeType_Space,
              CubeType_Print,
              CubeType_Print | CubeType_Digit,
              CubeType_Print | CubeType_Alpha | CubeType_Lower,
              CubeType_Print | CubeType_Alpha | CubeType_Upper,
              CubeType_Print | CubeType_Unicode | CubeType_Alpha | CubeType_Lower,
              CubeType_Print | CubeType_Unicode | CubeType_Alpha | CubeType_Upper)
};
extern const int cube2unichars[256] =
{
    0, 192, 193, 194, 195, 196, 197, 198, 199, 9, 10, 11, 12, 13, 200, 201,
    202, 203, 204, 205, 206, 207, 209, 210, 211, 212, 213, 214, 216, 217, 218, 219,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 220,
    221, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237,
    238, 239, 241, 242, 243, 244, 245, 246, 248, 249, 250, 251, 252, 253, 255, 0x104,
    0x105, 0x106, 0x107, 0x10C, 0x10D, 0x10E, 0x10F, 0x118, 0x119, 0x11A, 0x11B, 0x11E, 0x11F, 0x130, 0x131, 0x141,
    0x142, 0x143, 0x144, 0x147, 0x148, 0x150, 0x151, 0x152, 0x153, 0x158, 0x159, 0x15A, 0x15B, 0x15E, 0x15F, 0x160,
    0x161, 0x164, 0x165, 0x16E, 0x16F, 0x170, 0x171, 0x178, 0x179, 0x17A, 0x17B, 0x17C, 0x17D, 0x17E, 0x404, 0x411,
    0x413, 0x414, 0x416, 0x417, 0x418, 0x419, 0x41B, 0x41F, 0x423, 0x424, 0x426, 0x427, 0x428, 0x429, 0x42A, 0x42B,
    0x42C, 0x42D, 0x42E, 0x42F, 0x431, 0x432, 0x433, 0x434, 0x436, 0x437, 0x438, 0x439, 0x43A, 0x43B, 0x43C, 0x43D,
    0x43F, 0x442, 0x444, 0x446, 0x447, 0x448, 0x449, 0x44A, 0x44B, 0x44C, 0x44D, 0x44E, 0x44F, 0x454, 0x490, 0x491
};
extern const int uni2cubeoffsets[8] =
{
    0, 256, 658, 658, 512, 658, 658, 658
};

//only 255 chars can be aliased by the text renderer
extern const uchar uni2cubechars[878] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 10, 11, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 14, 15, 16, 17, 18, 19, 20, 21, 0, 22, 23, 24, 25, 26, 27, 0, 28, 29, 30, 31, 127, 128, 0, 129,
    130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 0, 146, 147, 148, 149, 150, 151, 0, 152, 153, 154, 155, 156, 157, 0, 158,
    0, 0, 0, 0, 159, 160, 161, 162, 0, 0, 0, 0, 163, 164, 165, 166, 0, 0, 0, 0, 0, 0, 0, 0, 167, 168, 169, 170, 0, 0, 171, 172,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 173, 174, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 175, 176, 177, 178, 0, 0, 179, 180, 0, 0, 0, 0, 0, 0, 0, 181, 182, 183, 184, 0, 0, 0, 0, 185, 186, 187, 188, 0, 0, 189, 190,
    191, 192, 0, 0, 193, 194, 0, 0, 0, 0, 0, 0, 0, 0, 195, 196, 197, 198, 0, 0, 0, 0, 0, 0, 199, 200, 201, 202, 203, 204, 205, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 17, 0, 0, 206, 83, 73, 21, 74, 0, 0, 0, 0, 0, 0, 0, 65, 207, 66, 208, 209, 69, 210, 211, 212, 213, 75, 214, 77, 72, 79, 215,
    80, 67, 84, 216, 217, 88, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 97, 228, 229, 230, 231, 101, 232, 233, 234, 235, 236, 237, 238, 239, 111, 240,
    112, 99, 241, 121, 242, 120, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 0, 141, 0, 0, 253, 115, 105, 145, 106, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 254, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//lowercase chars occupy mostly slots 128+
//upercase chars are mutually exclusive from lowercase chars in the char map
extern const uchar cubelowerchars[256] =
{
    0, 130, 131, 132, 133, 134, 135, 136, 137, 9, 10, 11, 12, 13, 138, 139,
    140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 156,
    157, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 160,
    160, 162, 162, 164, 164, 166, 166, 168, 168, 170, 170, 172, 172, 105, 174, 176,
    176, 178, 178, 180, 180, 182, 182, 184, 184, 186, 186, 188, 188, 190, 190, 192,
    192, 194, 194, 196, 196, 198, 198, 158, 201, 201, 203, 203, 205, 205, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};
extern const uchar cubeupperchars[256] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 123, 124, 125, 126, 127,
    128, 129, 1, 2, 3, 4, 5, 6, 7, 8, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 127, 128, 199, 159,
    159, 161, 161, 163, 163, 165, 165, 167, 167, 169, 169, 171, 171, 173, 73, 175,
    175, 177, 177, 179, 179, 181, 181, 183, 183, 185, 185, 187, 187, 189, 189, 191,
    191, 193, 193, 195, 195, 197, 197, 199, 200, 200, 202, 202, 204, 204, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};

size_t decodeutf8(uchar *dstbuf, size_t dstlen, const uchar *srcbuf, size_t srclen, size_t *carry)
{
    uchar *dst = dstbuf,
          *dstend = &dstbuf[dstlen];
    const uchar *src = srcbuf,
                *srcend = &srcbuf[srclen];
    if(dstbuf == srcbuf)
    {
        int len = std::min(dstlen, srclen);
        for(const uchar *end4 = &srcbuf[len&~3]; src < end4; src += 4)
        {
            if(*reinterpret_cast<const int *>(src) & 0x80808080)
            {
                goto decode;
            }
        }
        for(const uchar *end = &srcbuf[len]; src < end; src++)
        {
            if(*src & 0x80)
            {
                goto decode;
            }
        }
        if(carry)
        {
            *carry += len;
        }
        return len;
    }

decode:
    dst += src - srcbuf;
    while(src < srcend && dst < dstend)
    {
        int c = *src++;
        if(c < 0x80)
        {
            *dst++ = c;
        }
        else if(c >= 0xC0)
        {
            int uni;
            if(c >= 0xE0)
            {
                if(c >= 0xF0)
                {
                    if(c >= 0xF8)
                    {
                        if(c >= 0xFC)
                        {
                            if(c >= 0xFE)
                            {
                                continue;
                            }
                            uni = c&1;
                            if(srcend - src < 5)
                            {
                                break;
                            }
                            c = *src;
                            if((c&0xC0) != 0x80)
                            {
                                continue;
                            }
                            src++;
                            uni = (uni<<6) | (c&0x3F);
                        }
                        else
                        {
                            uni = c&3;
                            if(srcend - src < 4)
                            {
                                break;
                            }
                        }
                        c = *src;
                        if((c&0xC0) != 0x80)
                        {
                            continue;
                        }
                        src++;
                        uni = (uni<<6) | (c&0x3F);
                    }
                    else
                    {
                        uni = c&7;
                        if(srcend - src < 3)
                        {
                            break;
                        }
                    }
                    c = *src;
                    if((c&0xC0) != 0x80)
                    {
                        continue;
                    }
                    src++;
                    uni = (uni<<6) | (c&0x3F);
                }
                else
                {
                    uni = c&0xF;
                    if(srcend - src < 2)
                    {
                        break;
                    }
                }
                c = *src;
                if((c&0xC0) != 0x80)
                {
                    continue;
                }
                src++;
                uni = (uni<<6) | (c&0x3F);
            }
            else
            {
                uni = c&0x1F;
                if(srcend - src < 1)
                {
                    break;
                }
            }
            c = *src;
            {
                if((c&0xC0) != 0x80)
                {
                    continue;
                }
                src++;
                uni = (uni<<6) | (c&0x3F);
            }
            c = uni2cube(uni);
            if(!c)
            {
                continue;
            }
            *dst++ = c;
        }
    }
    if(carry)
    {
        *carry += src - srcbuf;
    }
    return dst - dstbuf;
}

size_t encodeutf8(uchar *dstbuf, size_t dstlen, const uchar *srcbuf, size_t srclen, size_t *carry)
{
    uchar *dst = dstbuf,
          *dstend = &dstbuf[dstlen];
    const uchar *src = srcbuf,
                *srcend = &srcbuf[srclen];
    if(src < srcend && dst < dstend)
    {
        do
        {
            int uni = cube2uni(*src);
            if(uni <= 0x7F)
            {
                if(dst >= dstend)
                {
                    goto done;
                }
                const uchar *end = std::min(srcend, &src[dstend-dst]);
                do
                {
                    if(uni == '\f')
                    {
                        if(++src >= srcend)
                        {
                            goto done;
                        }
                        goto uni1;
                    }
                    *dst++ = uni;
                    if(++src >= end)
                    {
                        goto done;
                    }
                    uni = cube2uni(*src);
                } while(uni <= 0x7F);
            }
            if(uni <= 0x7FF)
            {
                if(dst + 2 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xC0 | (uni>>6);
                goto uni2;
            }
            else if(uni <= 0xFFFF)
            {
                if(dst + 3 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xE0 | (uni>>12);
                goto uni3;
            }
            else if(uni <= 0x1FFFFF)
            {
                if(dst + 4 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xF0 | (uni>>18);
                goto uni4;
            }
            else if(uni <= 0x3FFFFFF)
            {
                if(dst + 5 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xF8 | (uni>>24);
                goto uni5;
            }
            else if(uni <= 0x7FFFFFFF)
            {
                if(dst + 6 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xFC | (uni>>30);
                goto uni6;
            }
            else
            {
                goto uni1;
            }
        uni6: *dst++ = 0x80 | ((uni>>24)&0x3F);
        uni5: *dst++ = 0x80 | ((uni>>18)&0x3F);
        uni4: *dst++ = 0x80 | ((uni>>12)&0x3F);
        uni3: *dst++ = 0x80 | ((uni>>6)&0x3F);
        uni2: *dst++ = 0x80 | (uni&0x3F);
        uni1:;
        } while(++src < srcend);
    }
done:
    if(carry)
    {
        *carry += src - srcbuf;
    }
    return dst - dstbuf;
}

///////////////////////// file system ///////////////////////

#ifdef WIN32
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

string homedir = "";
struct packagedir
{
    char *dir, *filter;
    size_t dirlen, filterlen;
};
static std::vector<packagedir> packagedirs;

char *makerelpath(const char *dir, const char *file, const char *prefix, const char *cmd)
{
    static string tmp;
    if(prefix)
    {
        copystring(tmp, prefix);
    }
    else
    {
        tmp[0] = '\0';
    }
    if(file[0]=='<')
    {
        const char *end = std::strrchr(file, '>');
        if(end)
        {
            size_t len = std::strlen(tmp);
            copystring(&tmp[len], file, std::min(sizeof(tmp)-len, static_cast<size_t>(end+2-file)));
            file = end+1;
        }
    }
    if(cmd)
    {
        concatstring(tmp, cmd);
    }
    if(dir)
    {
        DEF_FORMAT_STRING(pname, "%s/%s", dir, file);
        concatstring(tmp, pname);
    }
    else
    {
        concatstring(tmp, file);
    }
    return tmp;
}


char *path(char *s)
{
    for(char *curpart = s;;)
    {
        char *endpart = std::strchr(curpart, '&');
        if(endpart)
        {
            *endpart = '\0';
        }
        if(curpart[0]=='<')
        {
            char *file = std::strrchr(curpart, '>');
            if(!file)
            {
                return s;
            }
            curpart = file+1;
        }
        for(char *t = curpart; (t = strpbrk(t, "/\\")); *t++ = PATHDIV)
        {
            //(empty body)
        }
        for(char *prevdir = nullptr, *curdir = curpart;;)
        {
            prevdir = curdir[0]==PATHDIV ? curdir+1 : curdir;
            curdir = std::strchr(prevdir, PATHDIV);
            if(!curdir)
            {
                break;
            }
            if(prevdir+1==curdir && prevdir[0]=='.')
            {
                memmove(prevdir, curdir+1, std::strlen(curdir+1)+1);
                curdir = prevdir;
            }
            else if(curdir[1]=='.' && curdir[2]=='.' && curdir[3]==PATHDIV)
            {
                if(prevdir+2==curdir && prevdir[0]=='.' && prevdir[1]=='.')
                {
                    continue;
                }
                memmove(prevdir, curdir+4, std::strlen(curdir+4)+1);
                if(prevdir-2 >= curpart && prevdir[-1]==PATHDIV)
                {
                    prevdir -= 2;
                    while(prevdir-1 >= curpart && prevdir[-1] != PATHDIV)
                    {
                        --prevdir;
                    }
                }
                curdir = prevdir;
            }
        }
        if(endpart)
        {
            *endpart = '&';
            curpart = endpart+1;
        }
        else
        {
            break;
        }
    }
    return s;
}

char *copypath(const char *s)
{
    static string tmp;
    copystring(tmp, s);
    path(tmp);
    return tmp;
}

const char *parentdir(const char *directory)
{
    const char *p = directory + std::strlen(directory);
    while(p > directory && *p != '/' && *p != '\\')
    {
        p--;
    }
    static string parent;
    size_t len = p-directory+1;
    copystring(parent, directory, len);
    return parent;
}

bool fileexists(const char *path, const char *mode)
{
    bool exists = true;
    if(mode[0]=='w' || mode[0]=='a')
    {
        path = parentdir(path);
    }
#ifdef WIN32
    if(GetFileAttributes(path[0] ? path : ".\\") == INVALID_FILE_ATTRIBUTES)
    {
        exists = false;
    }
#else
    if(access(path[0] ? path : ".", mode[0]=='w' || mode[0]=='a' ? W_OK : (mode[0]=='d' ? X_OK : R_OK)) == -1)
    {
        exists = false;
    }
#endif
    return exists;
}

bool createdir(const char *path)
{
    size_t len = std::strlen(path);
    if(path[len-1] == PATHDIV)
    {
        static string strip;
        path = copystring(strip, path, len);
    }
#ifdef WIN32
    return CreateDirectory(path, nullptr) != 0;
#else
    return mkdir(path, 0777) == 0;
#endif
}

size_t fixpackagedir(char *dir)
{
    path(dir);
    size_t len = std::strlen(dir);
    if(len > 0 && dir[len-1] != PATHDIV)
    {
        dir[len] = PATHDIV;
        dir[len+1] = '\0';
    }
    return len;
}

bool subhomedir(char *dst, int len, const char *src)
{
    const char *sub = std::strstr(src, "$HOME");
    if(!sub)
    {
        sub = std::strchr(src, '~');
    }
    if(sub && sub-src < len)
    {
#ifdef WIN32
        char home[MAX_PATH+1];
        home[0] = '\0';
        if(SHGetFolderPath(nullptr, CSIDL_PERSONAL, nullptr, 0, home) != S_OK || !home[0])
        {
            return false;
        }
#else
        const char *home = getenv("HOME");
        if(!home || !home[0])
        {
            return false;
        }
#endif
        dst[sub-src] = '\0';
        concatstring(dst, home, len);
        concatstring(dst, sub+(*sub == '~' ? 1 : std::strlen("$HOME")), len);
    }
    return true;
}

const char *sethomedir(const char *dir)
{
    string pdir;
    copystring(pdir, dir);
    if(!subhomedir(pdir, sizeof(pdir), dir) || !fixpackagedir(pdir))
    {
        return nullptr;
    }
    copystring(homedir, pdir);
    return homedir;
}

const char *addpackagedir(const char *dir)
{
    string pdir;
    copystring(pdir, dir);
    if(!subhomedir(pdir, sizeof(pdir), dir) || !fixpackagedir(pdir))
    {
        return nullptr;
    }
    char *filter = pdir;
    for(;;)
    {
        static int len = std::strlen("media");
        filter = std::strstr(filter, "media");
        if(!filter)
        {
            break;
        }
        if(filter > pdir && filter[-1] == PATHDIV && filter[len] == PATHDIV)
        {
            break;
        }
        filter += len;
    }
    packagedir pf;
    pf.dir = filter ? newstring(pdir, filter-pdir) : newstring(pdir);
    pf.dirlen = filter ? filter-pdir : std::strlen(pdir);
    pf.filter = filter ? newstring(filter) : nullptr;
    pf.filterlen = filter ? std::strlen(filter) : 0;
    packagedirs.push_back(pf);
    return pf.dir;
}

const char *findfile(const char *filename, const char *mode)
{
    static string s;
    if(homedir[0])
    {
        formatstring(s, "%s%s", homedir, filename);
        if(fileexists(s, mode))
        {
            return s;
        }
        if(mode[0] == 'w' || mode[0] == 'a')
        {
            string dirs;
            copystring(dirs, s);
            char *dir = std::strchr(dirs[0] == PATHDIV ? dirs+1 : dirs, PATHDIV);
            while(dir)
            {
                *dir = '\0';
                if(!fileexists(dirs, "d") && !createdir(dirs))
                {
                    return s;
                }
                *dir = PATHDIV;
                dir = std::strchr(dir+1, PATHDIV);
            }
            return s;
        }
    }
    if(mode[0] == 'w' || mode[0] == 'a')
    {
        return filename;
    }
    for(uint i = 0; i < packagedirs.size(); i++)
    {
        packagedir &pf = packagedirs[i];
        if(pf.filter && strncmp(filename, pf.filter, pf.filterlen))
        {
            continue;
        }
        formatstring(s, "%s%s", pf.dir, filename);
        if(fileexists(s, mode))
        {
            return s;
        }
    }
    if(mode[0]=='e')
    {
        return nullptr;
    }
    return filename;
}

bool listdir(const char *dirname, bool rel, const char *ext, vector<char *> &files)
{
    size_t extsize = ext ? std::strlen(ext)+1 : 0;
#ifdef WIN32
    DEF_FORMAT_STRING(pathname, rel ? ".\\%s\\*.%s" : "%s\\*.%s", dirname, ext ? ext : "*");
    WIN32_FIND_DATA FindFileData;
    HANDLE Find = FindFirstFile(pathname, &FindFileData);
    if(Find != INVALID_HANDLE_VALUE)
    {
        do {
            if(!ext)
            {
                files.add(newstring(FindFileData.cFileName));
            }
            else
            {
                size_t namelen = std::strlen(FindFileData.cFileName);
                if(namelen > extsize)
                {
                    namelen -= extsize;
                    if(FindFileData.cFileName[namelen] == '.' && strncmp(FindFileData.cFileName+namelen+1, ext, extsize-1)==0)
                    {
                        files.add(newstring(FindFileData.cFileName, namelen));
                    }
                }
            }
        } while(FindNextFile(Find, &FindFileData));
        FindClose(Find);
        return true;
    }
#else
    DEF_FORMAT_STRING(pathname, rel ? "./%s" : "%s", dirname);
    DIR *d = opendir(pathname);
    if(d)
    {
        struct dirent *de;
        while((de = readdir(d)) != nullptr)
        {
            if(!ext)
            {
                files.add(newstring(de->d_name));
            }
            else
            {
                size_t namelen = std::strlen(de->d_name);
                if(namelen > extsize)
                {
                    namelen -= extsize;
                    if(de->d_name[namelen] == '.' && strncmp(de->d_name+namelen+1, ext, extsize-1)==0)
                    {
                        files.add(newstring(de->d_name, namelen));
                    }
                }
            }
        }
        closedir(d);
        return true;
    }
#endif
    else
    {
        return false;
    }
}

int listfiles(const char *dir, const char *ext, vector<char *> &files)
{
    string dirname;
    copystring(dirname, dir);
    path(dirname);
    size_t dirlen = std::strlen(dirname);
    while(dirlen > 1 && dirname[dirlen-1] == PATHDIV)
    {
        dirname[--dirlen] = '\0';
    }
    int dirs = 0;
    if(listdir(dirname, true, ext, files))
    {
        dirs++;
    }
    string s;
    if(homedir[0])
    {
        formatstring(s, "%s%s", homedir, dirname);
        if(listdir(s, false, ext, files))
        {
            dirs++;
        }
    }
    for(uint i = 0; i < packagedirs.size(); i++)
    {
        packagedir &pf = packagedirs[i];
        if(pf.filter && strncmp(dirname, pf.filter, dirlen == pf.filterlen-1 ? dirlen : pf.filterlen))
        {
            continue;
        }
        formatstring(s, "%s%s", pf.dir, dirname);
        if(listdir(s, false, ext, files))
        {
            dirs++;
        }
    }
    dirs += listzipfiles(dirname, ext, files);
    return dirs;
}

static Sint64 rwopsseek(SDL_RWops *rw, Sint64 pos, int whence)
{
    stream *f = static_cast<stream *>(rw->hidden.unknown.data1);
    if((!pos && whence==SEEK_CUR) || f->seek(pos, whence))
    {
        return static_cast<int>(f->tell());
    }
    return -1;
}

static size_t rwopsread(SDL_RWops *rw, void *buf, size_t size, size_t nmemb)
{
    stream *f = static_cast<stream *>(rw->hidden.unknown.data1);
    return f->read(buf, size*nmemb)/size;
}

static size_t rwopswrite(SDL_RWops *rw, const void *buf, size_t size, size_t nmemb)
{
    stream *f = static_cast<stream *>(rw->hidden.unknown.data1);
    return f->write(buf, size*nmemb)/size;
}

SDL_RWops *stream::rwops()
{
    SDL_RWops *rw = SDL_AllocRW();
    if(!rw)
    {
        return nullptr;
    }
    rw->hidden.unknown.data1 = this;
    rw->seek = rwopsseek;
    rw->read = rwopsread;
    rw->write = rwopswrite;
    rw->close = 0;
    return rw;
}

stream::offset stream::size()
{
    offset pos = tell(), endpos;
    if(pos < 0 || !seek(0, SEEK_END))
    {
        return -1;
    }
    endpos = tell();
    return pos == endpos || seek(pos, SEEK_SET) ? endpos : -1;
}

bool stream::getline(char *str, size_t len)
{
    for(int i = 0; i < static_cast<int>(len-1); ++i)
    {
        if(read(&str[i], 1) != 1)
        {
            str[i] = '\0';
            return i > 0;
        }
        else if(str[i] == '\n')
        {
            str[i+1] = '\0';
            return true;
        }
    }
    if(len > 0)
    {
        str[len-1] = '\0';
    }
    return true;
}

size_t stream::printf(const char *fmt, ...)
{
    char buf[512];
    char *str = buf;
    va_list args;
#if defined(WIN32) && !defined(__GNUC__)
    va_start(args, fmt);
    int len = _vscprintf(fmt, args);
    if(len <= 0)
    {
        va_end(args);
        return 0;
    }
    if(len >= static_cast<int>(sizeof(buf)))
    {
        str = new char[len+1];
    }
    _vsnprintf(str, len+1, fmt, args);
    va_end(args);
#else
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if(len <= 0)
    {
        return 0;
    }
    if(len >= static_cast<int>(sizeof(buf)))
    {
        str = new char[len+1];
        va_start(args, fmt);
        vsnprintf(str, len+1, fmt, args);
        va_end(args);
    }
#endif
    size_t n = write(str, len);
    if(str != buf)
    {
        delete[] str;
    }
    return n;
}

struct filestream : stream
{
    FILE *file;

    filestream() : file(nullptr) {}
    ~filestream() { close(); }

    bool open(const char *name, const char *mode)
    {
        if(file)
        {
            return false;
        }
        file = fopen(name, mode);
        return file!=nullptr;
    }
    #ifdef WIN32
        bool opentemp(const char *name, const char *mode)
        {
            if(file)
            {
                return false;
            }
            file = fopen(name, mode);
            return file!=nullptr;
        }
    #else
        bool opentemp(const char *, const char *)
        {
            if(file)
            {
                return false;
            }
            file = tmpfile();
            return file!=nullptr;
        }
    #endif
    void close()
    {
        if(file)
        {
            fclose(file);
            file = nullptr;
        }
    }

    bool end()
    {
        return feof(file)!=0;
    }
    offset tell()
    {
#ifdef WIN32
#if defined(__GNUC__) && !defined(__MINGW32__)
        offset off = ftello64(file);
#else
        offset off = _ftelli64(file);
#endif
#else
        offset off = ftello(file);
#endif
        // ftello returns LONG_MAX for directories on some platforms
        return off + 1 >= 0 ? off : -1;
    }
    bool seek(offset pos, int whence)
    {
#ifdef WIN32
#if defined(__GNUC__) && !defined(__MINGW32__)
        return fseeko64(file, pos, whence) >= 0;
#else
        return _fseeki64(file, pos, whence) >= 0;
#endif
#else
        return fseeko(file, pos, whence) >= 0;
#endif
    }

    size_t read(void *buf, size_t len)
    {
        return fread(buf, 1, len, file);
    }

    size_t write(const void *buf, size_t len)
    {
        return fwrite(buf, 1, len, file);
    }

    bool flush()
    {
        return !fflush(file);
    }

    int getchar()
    {
        return fgetc(file);
    }

    bool putchar(int c)
    {
        return fputc(c, file)!=EOF;
    }

    bool getline(char *str, size_t len)
    {
        return fgets(str, len, file)!=nullptr;
    }

    bool putstring(const char *str)
    {
        return fputs(str, file)!=EOF;
    }

    size_t printf(const char *fmt, ...)
    {
        va_list v;
        va_start(v, fmt);
        int result = std::vfprintf(file, fmt, v);
        va_end(v);
        return std::max(result, 0);
    }
};

VAR(debuggz, 0, 0, 1); //toggles gz checking routines

struct gzstream : stream
{
    enum
    {
        MAGIC1   = 0x1F,
        MAGIC2   = 0x8B,
        BUFSIZE  = 16384,
        OS_UNIX  = 0x03
    };

    enum
    {
        F_ASCII    = 0x01,
        F_CRC      = 0x02,
        F_EXTRA    = 0x04,
        F_NAME     = 0x08,
        F_COMMENT  = 0x10,
        F_RESERVED = 0xE0
    };

    stream *file;
    z_stream zfile;
    uchar *buf;
    bool reading, writing, autoclose;
    uint crc;
    size_t headersize;

    gzstream() : file(nullptr), buf(nullptr), reading(false), writing(false), autoclose(false), crc(0), headersize(0)
    {
        zfile.zalloc = nullptr;
        zfile.zfree = nullptr;
        zfile.opaque = nullptr;
        zfile.next_in = zfile.next_out = nullptr;
        zfile.avail_in = zfile.avail_out = 0;
    }

    ~gzstream()
    {
        close();
    }

    void writeheader()
    {
        uchar header[] = { MAGIC1, MAGIC2, Z_DEFLATED, 0, 0, 0, 0, 0, 0, OS_UNIX };
        file->write(header, sizeof(header));
    }

    void readbuf(size_t size = BUFSIZE)
    {
        if(!zfile.avail_in)
        {
            zfile.next_in = static_cast<Bytef *>(buf);
        }
        size = std::min(size, static_cast<size_t>(&buf[BUFSIZE] - &zfile.next_in[zfile.avail_in]));
        size_t n = file->read(zfile.next_in + zfile.avail_in, size);
        if(n > 0)
        {
            zfile.avail_in += n;
        }
    }

    uchar readbyte(size_t size = BUFSIZE)
    {
        if(!zfile.avail_in)
        {
            readbuf(size);
        }
        if(!zfile.avail_in)
        {
            return 0;
        }
        zfile.avail_in--;
        return *static_cast<uchar *>(zfile.next_in++);
    }

    void skipbytes(size_t n)
    {
        while(n > 0 && zfile.avail_in > 0)
        {
            size_t skipped = std::min(n, static_cast<size_t>(zfile.avail_in));
            zfile.avail_in -= skipped;
            zfile.next_in += skipped;
            n -= skipped;
        }
        if(n <= 0)
        {
            return;
        }
        file->seek(n, SEEK_CUR);
    }

    bool checkheader()
    {
        readbuf(10);
        if(readbyte() != MAGIC1 || readbyte() != MAGIC2 || readbyte() != Z_DEFLATED)
        {
            return false;
        }
        uchar flags = readbyte();
        if(flags & F_RESERVED)
        {
            return false;
        }
        skipbytes(6);
        if(flags & F_EXTRA)
        {
            size_t len = readbyte(512);
            len |= static_cast<size_t>(readbyte(512))<<8;
            skipbytes(len);
        }
        if(flags & F_NAME)
        {
            while(readbyte(512))
            {
                //(empty body)
            }
        }
        if(flags & F_COMMENT)
        {
            while(readbyte(512))
            {
                //(empty body)
            }
        }
        if(flags & F_CRC)
        {
            skipbytes(2);
        }
        headersize = static_cast<size_t>(file->tell() - zfile.avail_in);
        return zfile.avail_in > 0 || !file->end();
    }

    bool open(stream *f, const char *mode, bool needclose, int level)
    {
        if(file)
        {
            return false;
        }
        for(; *mode; mode++)
        {
            if(*mode=='r')
            {
                reading = true;
                break;
            }
            else if(*mode=='w')
            {
                writing = true;
                break;
            }
        }
        if(reading)
        {
            if(inflateInit2(&zfile, -MAX_WBITS) != Z_OK)
            {
                reading = false;
            }
        }
        else if(writing && deflateInit2(&zfile, level, Z_DEFLATED, -MAX_WBITS, std::min(MAX_MEM_LEVEL, 8), Z_DEFAULT_STRATEGY) != Z_OK)
        {
            writing = false;
        }
        if(!reading && !writing)
        {
            return false;
        }
        file = f;
        crc = crc32(0, nullptr, 0);
        buf = new uchar[BUFSIZE];
        if(reading)
        {
            if(!checkheader())
            {
                stopreading();
                return false;
            }
        }
        else if(writing)
        {
            writeheader();
        }
        autoclose = needclose;
        return true;
    }

    uint getcrc()
    {
        return crc;
    }

    void finishreading()
    {
        if(!reading)
        {
            return;
        }
        if(debuggz)
        {
            uint checkcrc = 0,
                 checksize = 0;
            for(int i = 0; i < 4; ++i)
            {
                checkcrc |= static_cast<uint>(readbyte()) << (i*8);
            }
            for(int i = 0; i < 4; ++i)
            {
                checksize |= static_cast<uint>(readbyte()) << (i*8);
            }
            if(checkcrc != crc)
            {
                conoutf(Console_Debug, "gzip crc check failed: read %X, calculated %X", checkcrc, crc);
            }
            if(checksize != zfile.total_out)
            {
                conoutf(Console_Debug, "gzip size check failed: read %u, calculated %u", checksize, static_cast<uint>(zfile.total_out));
            }
        }
    }

    void stopreading()
    {
        if(!reading)
        {
            return;
        }
        inflateEnd(&zfile);
        reading = false;
    }

    void finishwriting()
    {
        if(!writing)
        {
            return;
        }
        for(;;)
        {
            int err = zfile.avail_out > 0 ? deflate(&zfile, Z_FINISH) : Z_OK;
            if(err != Z_OK && err != Z_STREAM_END)
            {
                break;
            }
            flushbuf();
            if(err == Z_STREAM_END)
            {
                break;
            }
        }
        uchar trailer[8] =
        {
            static_cast<uchar>(crc&0xFF), static_cast<uchar>((crc>>8)&0xFF), static_cast<uchar>((crc>>16)&0xFF), static_cast<uchar>((crc>>24)&0xFF),
            static_cast<uchar>(zfile.total_in&0xFF), static_cast<uchar>((zfile.total_in>>8)&0xFF), static_cast<uchar>((zfile.total_in>>16)&0xFF), static_cast<uchar>((zfile.total_in>>24)&0xFF)
        };
        file->write(trailer, sizeof(trailer));
    }

    void stopwriting()
    {
        if(!writing)
        {
            return;
        }
        deflateEnd(&zfile);
        writing = false;
    }

    void close()
    {
        if(reading)
        {
            finishreading();
        }
        stopreading();
        if(writing)
        {
            finishwriting();
        }
        stopwriting();
        DELETEA(buf);
        if(autoclose)
        {
            if(file)
            {
                delete file;
                file = nullptr;
            }
        }
    }

    bool end()
    {
        return !reading && !writing;
    }

    offset tell()
    {
        return reading ? zfile.total_out : (writing ? zfile.total_in : offset(-1));
    }

    offset rawtell()
    {
        return file ? file->tell() : offset(-1);
    }

    offset size()
    {
        if(!file)
        {
            return -1;
        }
        offset pos = tell();
        if(!file->seek(-4, SEEK_END))
        {
            return -1;
        }
        uint isize = file->get<uint>();
        return file->seek(pos, SEEK_SET) ? isize : offset(-1);
    }

    offset rawsize()
    {
        return file ? file->size() : offset(-1);
    }

    bool seek(offset pos, int whence)
    {
        if(writing || !reading)
        {
            return false;
        }

        if(whence == SEEK_END)
        {
            uchar skip[512];
            while(read(skip, sizeof(skip)) == sizeof(skip))
            {
                //(empty body)
            }
            return !pos;
        }
        else if(whence == SEEK_CUR)
        {
            pos += zfile.total_out;
        }
        if(pos >= static_cast<offset>(zfile.total_out))
        {
            pos -= zfile.total_out;
        }
        else if(pos < 0 || !file->seek(headersize, SEEK_SET))
        {
            return false;
        }
        else
        {
            if(zfile.next_in && zfile.total_in <= static_cast<uint>(zfile.next_in - buf))
            {
                zfile.avail_in += zfile.total_in;
                zfile.next_in -= zfile.total_in;
            }
            else
            {
                zfile.avail_in = 0;
                zfile.next_in = nullptr;
            }
            inflateReset(&zfile);
            crc = crc32(0, nullptr, 0);
        }

        uchar skip[512];
        while(pos > 0)
        {
            size_t skipped = static_cast<size_t>(std::min(pos, static_cast<offset>(sizeof(skip))));
            if(read(skip, skipped) != skipped)
            {
                stopreading();
                return false;
            }
            pos -= skipped;
        }

        return true;
    }

    size_t read(void *buf, size_t len)
    {
        if(!reading || !buf || !len)
        {
            return 0;
        }
        zfile.next_out = static_cast<Bytef *>(buf);
        zfile.avail_out = len;
        while(zfile.avail_out > 0)
        {
            if(!zfile.avail_in)
            {
                readbuf(BUFSIZE);
                if(!zfile.avail_in)
                {
                    stopreading();
                    break;
                }
            }
            int err = inflate(&zfile, Z_NO_FLUSH);
            if(err == Z_STREAM_END)
            {
                crc = crc32(crc, static_cast<Bytef *>(buf), len - zfile.avail_out);
                finishreading();
                stopreading();
                return len - zfile.avail_out;
            }
            else if(err != Z_OK)
            {
                stopreading();
                break;
            }
        }
        crc = crc32(crc, reinterpret_cast<Bytef *>(buf), len - zfile.avail_out);
        return len - zfile.avail_out;
    }

    bool flushbuf(bool full = false)
    {
        if(full)
        {
            deflate(&zfile, Z_SYNC_FLUSH);
        }
        if(zfile.next_out && zfile.avail_out < BUFSIZE)
        {
            if(file->write(buf, BUFSIZE - zfile.avail_out) != BUFSIZE - zfile.avail_out || (full && !file->flush()))
            {
                return false;
            }
        }
        zfile.next_out = buf;
        zfile.avail_out = BUFSIZE;
        return true;
    }

    bool flush()
    {
        return flushbuf(true);
    }

    size_t write(const void *buf, size_t len)
    {
        if(!writing || !buf || !len)
        {
            return 0;
        }
        zfile.next_in = static_cast<Bytef *>(const_cast<void *>(buf)); //cast away constness, then to Bytef
        zfile.avail_in = len;
        while(zfile.avail_in > 0)
        {
            if(!zfile.avail_out && !flushbuf())
            {
                stopwriting();
                break;
            }
            int err = deflate(&zfile, Z_NO_FLUSH);
            if(err != Z_OK)
            {
                stopwriting();
                break;
            }
        }
        crc = crc32(crc, static_cast<Bytef *>(const_cast<void *>(buf)), len - zfile.avail_in);
        return len - zfile.avail_in;
    }
};

struct utf8stream : stream
{
    enum
    {
        Buffer_Size = 4096
    };
    stream *file;
    offset pos;
    size_t bufread, bufcarry, buflen;
    bool reading, writing, autoclose;
    uchar buf[Buffer_Size];

    utf8stream() : file(nullptr), pos(0), bufread(0), bufcarry(0), buflen(0), reading(false), writing(false), autoclose(false)
    {
    }

    ~utf8stream()
    {
        close();
    }

    bool readbuf(size_t size = Buffer_Size)
    {
        if(bufread >= bufcarry)
        {
            if(bufcarry > 0 && bufcarry < buflen)
            {
                memmove(buf, &buf[bufcarry], buflen - bufcarry);
                buflen -= bufcarry;
                bufread = bufcarry = 0;
            }
        }
        size_t n = file->read(&buf[buflen], std::min(size, Buffer_Size - buflen));
        if(n <= 0)
        {
            return false;
        }
        buflen += n;
        size_t carry = bufcarry;
        bufcarry += decodeutf8(&buf[bufcarry], Buffer_Size-bufcarry, &buf[bufcarry], buflen-bufcarry, &carry);
        if(carry > bufcarry && carry < buflen)
        {
            memmove(&buf[bufcarry], &buf[carry], buflen - carry);
            buflen -= carry - bufcarry;
        }
        return true;
    }

    bool checkheader()
    {
        size_t n = file->read(buf, 3);
        if(n == 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF)
        {
            return true;
        }
        buflen = n;
        return false;
    }

    bool open(stream *f, const char *mode, bool needclose)
    {
        if(file)
        {
            return false;
        }
        for(; *mode; mode++)
        {
            if(*mode=='r')
            {
                reading = true;
                break;
            }
            else if(*mode=='w')
            {
                writing = true;
                break;
            }
        }
        if(!reading && !writing)
        {
            return false;
        }
        file = f;
        if(reading)
        {
            checkheader();
        }
        autoclose = needclose;
        return true;
    }

    void finishreading()
    {
        if(!reading)
        {
            return;
        }
    }

    void stopreading()
    {
        if(!reading)
        {
            return;
        }
        reading = false;
    }

    void stopwriting()
    {
        if(!writing)
        {
            return;
        }
        writing = false;
    }

    void close()
    {
        stopreading();
        stopwriting();
        if(autoclose)
        {
            if(file)
            {
                delete file;
                file = nullptr;
            }
        }
    }

    bool end()
    {
        return !reading && !writing;
    }

    offset tell()
    {
        return reading || writing ? pos : offset(-1);
    }

    bool seek(offset off, int whence)
    {
        if(writing || !reading)
        {
            return false;
        }

        if(whence == SEEK_END)
        {
            uchar skip[512];
            while(read(skip, sizeof(skip)) == sizeof(skip))
            {
                //(empty body)
            }
            return !off;
        }
        else if(whence == SEEK_CUR) off += pos;

        if(off >= pos)
        {
            off -= pos;
        }
        else if(off < 0 || !file->seek(0, SEEK_SET))
        {
            return false;
        }
        else
        {
            bufread = bufcarry = buflen = 0;
            pos = 0;
            checkheader();
        }

        uchar skip[512];
        while(off > 0)
        {
            size_t skipped = static_cast<size_t>(std::min(off, static_cast<offset>(sizeof(skip))));
            if(read(skip, skipped) != skipped)
            {
                stopreading();
                return false;
            }
            off -= skipped;
        }

        return true;
    }

    size_t read(void *dst, size_t len)
    {
        if(!reading || !dst || !len)
        {
            return 0;
        }
        size_t next = 0;
        while(next < len)
        {
            if(bufread >= bufcarry)
            {
                if(readbuf(Buffer_Size))
                {
                    continue;
                }
                stopreading();
                break;
            }
            size_t n = std::min(len - next, bufcarry - bufread);
            memcpy(&(static_cast<uchar *>(dst))[next], &buf[bufread], n);
            next += n;
            bufread += n;
        }
        pos += next;
        return next;
    }

    bool getline(char *dst, size_t len)
    {
        if(!reading || !dst || !len)
        {
            return false;
        }
        --len;
        size_t next = 0;
        while(next < len)
        {
            if(bufread >= bufcarry)
            {
                if(readbuf(Buffer_Size))
                {
                    continue;
                }
                stopreading();
                if(!next)
                {
                    return false;
                }
                break;
            }
            size_t n = std::min(len - next, bufcarry - bufread);
            uchar *endline = static_cast<uchar *>(memchr(&buf[bufread], '\n', n));
            if(endline)
            {
                n = endline+1 - &buf[bufread];
                len = next + n;
            }
            memcpy(&(reinterpret_cast<uchar *>(dst))[next], &buf[bufread], n);
            next += n;
            bufread += n;
        }
        dst[next] = '\0';
        pos += next;
        return true;
    }

    size_t write(const void *src, size_t len)
    {
        if(!writing || !src || !len)
        {
            return 0;
        }
        uchar dst[512];
        size_t next = 0;
        while(next < len)
        {
            size_t carry = 0,
                   n = encodeutf8(dst, sizeof(dst), &(static_cast<uchar *>(const_cast<void *>(src)))[next], len - next, &carry);
            if(n > 0 && file->write(dst, n) != n)
            {
                stopwriting();
                break;
            }
            next += carry;
        }
        pos += next;
        return next;
    }

    bool flush()
    {
        return file->flush();
    }
};

stream *openrawfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
    if(!found)
    {
        return nullptr;
    }
    filestream *file = new filestream;
    if(!file->open(found, mode))
    {
        delete file;
        return nullptr;
    }
    return file;
}

stream *openfile(const char *filename, const char *mode)
{
    stream *s = openzipfile(filename, mode);
    if(s)
    {
        return s;
    }
    return openrawfile(filename, mode);
}

stream *opentempfile(const char *name, const char *mode)
{
    const char *found = findfile(name, mode);
    filestream *file = new filestream;
    if(!file->opentemp(found ? found : name, mode))
    {
        delete file;
        return nullptr;
    }
    return file;
}

stream *opengzfile(const char *filename, const char *mode, stream *file, int level)
{
    stream *source = file ? file : openfile(filename, mode);
    if(!source)
    {
        return nullptr;
    }
    gzstream *gz = new gzstream;
    if(!gz->open(source, mode, !file, level))
    {
        if(!file)
        {
            delete source;
        }
        delete gz;
        return nullptr;
    }
    return gz;
}

stream *openutf8file(const char *filename, const char *mode, stream *file)
{
    stream *source = file ? file : openfile(filename, mode);
    if(!source)
    {
        return nullptr;
    }
    utf8stream *utf8 = new utf8stream;
    if(!utf8->open(source, mode, !file))
    {
        if(!file)
        {
            delete source;
        }
        delete utf8;
        return nullptr;
    }
    return utf8;
}

char *loadfile(const char *fn, size_t *size, bool utf8)
{
    stream *f = openfile(fn, "rb");
    if(!f)
    {
        return nullptr;
    }
    stream::offset fsize = f->size();
    if(fsize <= 0)
    {
        delete f;
        return nullptr;
    }
    size_t len = fsize;
    char *buf = new char[len+1];
    if(!buf)
    {
        delete f;
        return nullptr;
    }
    size_t offset = 0;
    if(utf8 && len >= 3)
    {
        if(f->read(buf, 3) != 3)
        {
            delete f;
            delete[] buf;
            return nullptr;
        }
        if((reinterpret_cast<uchar*>(buf))[0] == 0xEF &&
           (reinterpret_cast<uchar*>(buf))[1] == 0xBB &&
           (reinterpret_cast<uchar*>(buf))[2] == 0xBF)
        {
            len -= 3;
        }
        else
        {
            offset += 3;
        }
    }
    size_t rlen = f->read(&buf[offset], len-offset);
    delete f;
    if(rlen != len-offset)
    {
        delete[] buf;
        return nullptr;
    }
    if(utf8)
    {
        len = decodeutf8(reinterpret_cast<uchar *>(buf), len, reinterpret_cast<uchar *>(buf), len);
    }
    buf[len] = '\0';
    if(size!=nullptr)
    {
        *size = len;
    }
    return buf;
}


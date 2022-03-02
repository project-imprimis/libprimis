// implementation of generic tools

#include "../libprimis-headers/cube.h"
#include "stream.h"

////////////////////////// strings ////////////////////////////////////////

char *tempformatstring(const char *fmt, ...)
{
    static string tmpstr[4];
    static int tmpidx = 0;
    tmpidx = (tmpidx+1)%4;

    va_list v;
    va_start(v, fmt);
    vformatstring(tmpstr[tmpidx], fmt, v);
    va_end(v);

    return tmpstr[tmpidx];
}

////////////////////////// bit packing ////////////////////////////////////

// these functions are useful for packing ints/chars into smaller chunks, which
// is useful for reducing file save sizes

// note that the parent put*_ template functions is not defined here but in tools.h

//*             signed integers            *//
void putint(ucharbuf &p, int n)
{
    putint_(p, n);
}

void putint(vector<uchar> &p, int n)
{
    putint_(p, n);
}

int getint(ucharbuf &p)
{
    int c = static_cast<char>(p.get());
    if(c == -128)
    {
        int n = p.get();
        n |= static_cast<char>(p.get()) << 8;
        return n;
    }
    else if(c == -127)
    {
        int n = p.get();
        n |= p.get() << 8;
        n |= p.get() << 16;
        return n|(p.get()<<24);
    }
    else
    {
        return c;
    }
}

//*             unisigned integers            *//
// much smaller encoding for unsigned integers up to 28 bits, but can handle signed

void putuint(ucharbuf &p, int n)
{
    putuint_(p, n);
}

void putuint(vector<uchar> &p, int n)
{
    putuint_(p, n);
}

int getuint(ucharbuf &p)
{
    int n = p.get();
    if(n & 0x80)
    {
        n += (p.get() << 7) - 0x80;
        if(n & (1<<14))
        {
            n += (p.get() << 14) - (1<<14);
        }
        if(n & (1<<21))
        {
            n += (p.get() << 21) - (1<<21);
        }
        if(n & (1<<28))
        {
            n |= ~0U<<28;
        }
    }
    return n;
}

//*             floats            *//
void putfloat(ucharbuf &p, float f)
{
    putfloat_(p, f);
}

void putfloat(vector<uchar> &p, float f)
{
    putfloat_(p, f);
}

float getfloat(ucharbuf &p)
{
    float f;
    p.get(reinterpret_cast<uchar *>(&f), sizeof(float));
    return f;
}

//*             strings            *//
void sendstring(const char *t, ucharbuf &p)
{
    sendstring_(t, p);
}
void sendstring(const char *t, vector<uchar> &p)
{
    sendstring_(t, p);
}

void getstring(char *text, ucharbuf &p, size_t len)
{
    char *t = text;
    do
    {
        if(t >= &text[len])
        {
            text[len-1] = 0;
            return;
        }
        if(!p.remaining())
        {
            *t = 0;
            return;
        }
        *t = getint(p);
    } while(*t++);
}

void filtertext(char *dst, const char *src, bool whitespace, bool forcespace, size_t len)
{
    for(int c = static_cast<uchar>(*src); c; c = static_cast<uchar>(*++src))
    {
        if(c == '\f')
        {
            if(!*++src)
            {
                break;
            }
            continue;
        }
        if(!iscubeprint(c))
        {
            if(!iscubespace(c) || !whitespace)
            {
                continue;
            }
            if(forcespace)
            {
                c = ' ';
            }
        }
        *dst++ = c;
        if(!--len)
        {
            break;
        }
    }
    *dst = '\0';
}

#include "../libprimis-headers/cube.h"

///////////////////////// cryptography /////////////////////////////////

/* Based off the reference implementation of Tiger, a cryptographically
 * secure 192 bit hash function by Ross Anderson and Eli Biham. More info at:
 * http://www.cs.technion.ac.il/~biham/Reports/Tiger/
 */

static constexpr int tigerpasses = 3;

namespace tiger
{
    typedef unsigned long long int chunk;

    union hashval
    {
        uchar bytes[3*8];
        chunk chunks[3];
    };

    chunk sboxes[4*256];

    void compress(const chunk *str, chunk state[3])
    {
        chunk a, b, c;
        chunk aa, bb, cc;
        chunk x0, x1, x2, x3, x4, x5, x6, x7;

        a = state[0];
        b = state[1];
        c = state[2];

        x0=str[0];
        x1=str[1];
        x2=str[2];
        x3=str[3];
        x4=str[4];
        x5=str[5];
        x6=str[6];
        x7=str[7];

        aa = a;
        bb = b;
        cc = c;

        for(int pass_no = 0; pass_no < tigerpasses; ++pass_no)
        {
            if(pass_no)
            {
                x0 -= x7 ^ 0xA5A5A5A5A5A5A5A5ULL;
                x1 ^= x0;
                x2 += x1;
                x3 -= x2 ^ ((~x1)<<19);
                x4 ^= x3;
                x5 += x4;
                x6 -= x5 ^ ((~x4)>>23);
                x7 ^= x6;
                x0 += x7;
                x1 -= x0 ^ ((~x7)<<19);
                x2 ^= x1;
                x3 += x2;
                x4 -= x3 ^ ((~x2)>>23);
                x5 ^= x4;
                x6 += x5;
                x7 -= x6 ^ 0x0123456789ABCDEFULL;
            }

//============================================================= SB_1 2 3 4 ROUND
#define SB_1 (sboxes)
#define SB_2 (sboxes+256)
#define SB_3 (sboxes+256*2)
#define SB_4 (sboxes+256*3)
#define ROUND(a, b, c, x) \
      c ^= x; \
      a -= SB_1[((c)>>(0*8))&0xFF] ^ SB_2[((c)>>(2*8))&0xFF] ^ \
           SB_3[((c)>>(4*8))&0xFF] ^ SB_4[((c)>>(6*8))&0xFF] ; \
      b += SB_4[((c)>>(1*8))&0xFF] ^ SB_3[((c)>>(3*8))&0xFF] ^ \
           SB_2[((c)>>(5*8))&0xFF] ^ SB_1[((c)>>(7*8))&0xFF] ; \
      b *= mul;

            uint mul = !pass_no ? 5 : (pass_no==1 ? 7 : 9);
            ROUND(a, b, c, x0)
            ROUND(b, c, a, x1)
            ROUND(c, a, b, x2)
            ROUND(a, b, c, x3)
            ROUND(b, c, a, x4)
            ROUND(c, a, b, x5)
            ROUND(a, b, c, x6)
            ROUND(b, c, a, x7)

            chunk tmp = a;
            a = c;
            c = b;
            b = tmp;
        }

        a ^= aa;
        b -= bb;
        c += cc;

        state[0] = a;
        state[1] = b;
        state[2] = c;
    }
#undef ROUND
#undef SB_1
#undef SB_2
#undef SB_3
#undef SB_4
//==============================================================================
    void gensboxes()
    {
        const char *str = "Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham";
        chunk state[3] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, 0xF096A5B4C3B2E187ULL };
        uchar temp[64];
        for(int j = 0; j < 64; ++j)
        {
            temp[j] = str[j];
        }
        for(int i = 0; i < 1024; ++i)
        {
            for(int col = 0; col < 2; ++col)
            {
                reinterpret_cast<uchar *>(&sboxes[i])[col] = i&0xFF;
            }
        }

        int abc = 2;
        for(int pass = 0; pass < 5; ++pass)
        {
            for(int i = 0; i < 256; ++i)
            {
                for(int sb = 0; sb < 1024; sb += 256)
                {
                    abc++;
                    if(abc >= 3)
                    {
                        abc = 0; compress((chunk *)temp, state);
                    }
                    for(int col = 0; col < 8; ++col)
                    {
                        uchar val = reinterpret_cast<uchar *>(&sboxes[sb+i])[col];
                        reinterpret_cast<uchar *>(&sboxes[sb+i])[col] = (reinterpret_cast<uchar *>(&sboxes[sb + (reinterpret_cast<uchar *>(&state[abc]))[col]]))[col];
                        reinterpret_cast<uchar *>(&sboxes[sb + (reinterpret_cast<uchar *>(&state[abc]))[col]])[col] = val;
                    }
                }
            }
        }
    }

    void hash(const uchar *str, int length, hashval &val)
    {
        static bool init = false;
        if(!init)
        {
            gensboxes();
            init = true;
        }

        uchar temp[64];

        val.chunks[0] = 0x0123456789ABCDEFULL;
        val.chunks[1] = 0xFEDCBA9876543210ULL;
        val.chunks[2] = 0xF096A5B4C3B2E187ULL;

        int i = length;
        for(; i >= 64; i -= 64, str += 64)
        {
            compress((chunk *)str, val.chunks);
        }
        int j;
        for(j = 0; j < i; j++)
        {
            temp[j] = str[j];
        }
        temp[j] = 0x01;
        while(++j&7) temp[j] = 0;
        if(j > 56)
        {
            while(j < 64)
            {
                temp[j++] = 0;
            }
            compress(reinterpret_cast<chunk *>(temp), val.chunks);
            j = 0;
        }
        while(j < 56) temp[j++] = 0;
        *reinterpret_cast<chunk *>(temp+56) = static_cast<chunk>(length << 3);
        compress(reinterpret_cast<chunk *>(temp), val.chunks);
    }
}

/* Elliptic curve cryptography based on NIST DSS prime curves. */


constexpr int bidigitbits = 16;

template<int BI_DIGITS>
struct bigint
{
    typedef ushort digit;
    typedef uint dbldigit;

    int len;
    digit digits[BI_DIGITS];

    bigint() {}
    bigint(digit n)
    {
        if(n)
        {
            len = 1;
            digits[0] = n;
        }
        else
        {
            len = 0;
        }
    }
    bigint(const char *s)
    {
        parse(s);
    }
    template<int ydigits>
    bigint(const bigint<ydigits> &y)
    {
        *this = y;
    }

    static int parsedigits(ushort *digits, int maxlen, const char *s)
    {
        int slen = 0;
        while(isxdigit(s[slen]))
        {
            slen++;
        }
        int len = (slen+2*sizeof(ushort)-1)/(2*sizeof(ushort));
        if(len>maxlen)
        {
            return 0;
        }
        memset(digits, 0, len*sizeof(ushort));
        for(int i = 0; i < slen; ++i)
        {
            int c = s[slen-i-1];
            if(isalpha(c))
            {
                c = toupper(c) - 'A' + 10;
            }
            else if(isdigit(c))
            {
                c -= '0';
            }
            else
            {
                return 0;
            }
            digits[i/(2*sizeof(ushort))] |= c<<(4*(i%(2*sizeof(ushort))));
        }
        return len;
    }

    void parse(const char *s)
    {
        len = parsedigits(digits, BI_DIGITS, s);
        shrink();
    }

    void zero() { len = 0; }

    void print(stream *out) const
    {
        vector<char> buf;
        printdigits(buf);
        out->write(buf.getbuf(), buf.length());
    }

    void printdigits(vector<char> &buf) const
    {
        for(int i = 0; i < len; ++i)
        {
            digit d = digits[len-i-1];
            for(int j = 0; j < bidigitbits/4; ++j)
            {
                uint shift = bidigitbits - (j+1)*4;
                int val = (d >> shift) & 0xF;
                if(val < 10)
                {
                    buf.add('0' + val);
                }
                else
                {
                    buf.add('a' + val - 10);
                }
            }
        }
    }

    template<int ydigits>
    bigint &operator=(const bigint<ydigits> &y)
    {
        len = y.len;
        memcpy(digits, y.digits, len*sizeof(digit));
        return *this;
    }

    bool iszero() const
    {
        return !len;
    }

    bool isone() const
    {
        return len==1 && digits[0]==1;
    }

    int numbits() const
    {
        if(!len)
        {
            return 0;
        }
        int bits = len*bidigitbits;
        digit last = digits[len-1], mask = 1<<(bidigitbits-1);
        while(mask)
        {
            if(last&mask)
            {
                return bits;
            }
            bits--;
            mask >>= 1;
        }
        return 0;
    }

    bool hasbit(int n) const
    {
        return n/bidigitbits < len && ((digits[n/bidigitbits]>>(n%bidigitbits))&1);
    }

    bool morebits(int n) const
    {
        return len > n/bidigitbits;
    }

    template<int xdigits, int ydigits>
    bigint &add(const bigint<xdigits> &x, const bigint<ydigits> &y)
    {
        dbldigit carry = 0;
        int maxlen = std::max(x.len, y.len), i;
        for(i = 0; i < y.len || carry; i++)
        {
             carry += (i < x.len ? static_cast<dbldigit>(x.digits[i]) : 0) + (i < y.len ? static_cast<dbldigit>(y.digits[i]) : 0);
             digits[i] = static_cast<digit>(carry);
             carry >>= bidigitbits;
        }
        if(i < x.len && this != &x)
        {
            memcpy(&digits[i], &x.digits[i], (x.len - i)*sizeof(digit));
        }
        len = std::max(i, maxlen);
        return *this;
    }
    template<int ydigits>
    bigint &add(const bigint<ydigits> &y) { return add(*this, y); }

    template<int xdigits, int ydigits>
    bigint &sub(const bigint<xdigits> &x, const bigint<ydigits> &y)
    {
        dbldigit borrow = 0;
        int i;
        for(i = 0; i < y.len || borrow; i++)
        {
             borrow = (1<<bidigitbits) + static_cast<dbldigit>(x.digits[i]) - (i<y.len ? static_cast<dbldigit>(y.digits[i]) : 0) - borrow;
             digits[i] = static_cast<digit>(borrow);
             borrow = (borrow>>bidigitbits)^1;
        }
        if(i < x.len && this != &x)
        {
            memcpy(&digits[i], &x.digits[i], (x.len - i)*sizeof(digit));
        }
        len = x.len;
        shrink();
        return *this;
    }

    template<int ydigits>
    bigint &sub(const bigint<ydigits> &y)
    {
        return sub(*this, y);
    }

    void shrink()
    {
        while(len && !digits[len-1])
        {
            len--;
        }
    }
    void shrinkdigits(int n)
    {
        len = n; shrink();
    }

    void shrinkbits(int n)
    {
        shrinkdigits(n/bidigitbits);
    }

    template<int ydigits>
    void copyshrinkdigits(const bigint<ydigits> &y, int n)
    {
        len = std::min(y.len, n);
        memcpy(digits, y.digits, len*sizeof(digit));
        shrink();
    }
    template<int ydigits>
    void copyshrinkbits(const bigint<ydigits> &y, int n)
    {
        copyshrinkdigits(y, n/bidigitbits);
    }

    template<int xdigits, int ydigits>
    bigint &mul(const bigint<xdigits> &x, const bigint<ydigits> &y)
    {
        if(!x.len || !y.len)
        {
            len = 0;
            return *this;
        }
        memset(digits, 0, y.len*sizeof(digit));
        for(int i = 0; i < x.len; ++i)
        {
            dbldigit carry = 0;
            for(int j = 0; j < y.len; ++j)
            {
                carry += static_cast<dbldigit>(x.digits[i]) * static_cast<dbldigit>(y.digits[j]) + static_cast<dbldigit>(digits[i+j]);
                digits[i+j] = (digit)carry;
                carry >>= bidigitbits;
            }
            digits[i+y.len] = carry;
        }
        len = x.len + y.len;
        shrink();
        return *this;
    }

    bigint &rshift(int n)
    {
        assert(len <= BI_DIGITS);
        if(!len || n<=0)
        {
            return *this;
        }
        if(n >= len*bidigitbits)
        {
            len = 0;
            return *this;
        }
        int dig = (n-1)/bidigitbits;
        n = ((n-1) % bidigitbits)+1;
        digit carry = digit(digits[dig]>>n);
        for(int i = dig+1; i < len; i++)
        {
            digit tmp = digits[i];
            digits[i-dig-1] = digit((tmp<<(bidigitbits-n)) | carry);
            carry = digit(tmp>>n);
        }
        digits[len-dig-1] = carry;
        len -= dig + (n/bidigitbits);
        shrink();
        return *this;
    }

    bigint &lshift(int n)
    {
        if(!len || n<=0)
        {
            return *this;
        }
        int dig = n/bidigitbits;
        n %= bidigitbits;
        digit carry = 0;
        for(int i = len; --i >= 0;) //note reverse iteration
        {
            digit tmp = digits[i];
            digits[i+dig] = digit((tmp<<n) | carry);
            carry = digit(tmp>>(bidigitbits-n));
        }
        len += dig;
        if(carry)
        {
            digits[len++] = carry;
        }
        if(dig)
        {
            memset(digits, 0, dig*sizeof(digit));
        }
        return *this;
    }

    void zerodigits(int i, int n)
    {
        memset(&digits[i], 0, n*sizeof(digit));
    }
    void zerobits(int i, int n)
    {
        zerodigits(i/bidigitbits, n/bidigitbits);
    }

    template<int ydigits>
    void copydigits(int to, const bigint<ydigits> &y, int from, int n)
    {
        int avail = std::min(y.len-from, n);
        memcpy(&digits[to], &y.digits[from], avail*sizeof(digit));
        if(avail < n)
        {
            memset(&digits[to+avail], 0, (n-avail)*sizeof(digit));
        }
    }

    template<int ydigits>
    void copybits(int to, const bigint<ydigits> &y, int from, int n)
    {
        copydigits(to/bidigitbits, y, from/bidigitbits, n/bidigitbits);
    }

    void dupdigits(int to, int from, int n)
    {
        memcpy(&digits[to], &digits[from], n*sizeof(digit));
    }
    void dupbits(int to, int from, int n)
    {
        dupdigits(to/bidigitbits, from/bidigitbits, n/bidigitbits);
    }

    template<int ydigits>
    bool operator==(const bigint<ydigits> &y) const
    {
        if(len!=y.len)
        {
            return false;
        }
        for(int i = len; --i >= 0;) //note reverse iteration
        {
            if(digits[i]!=y.digits[i])
            {
                return false;
            }
        }
        return true;
    }

    template<int ydigits>
    bool operator!=(const bigint<ydigits> &y) const { return !(*this==y); }

    template<int ydigits>
    bool operator<(const bigint<ydigits> &y) const
    {
        if(len<y.len)
        {
            return true;
        }
        if(len>y.len)
        {
            return false;
        }
        for(int i = len; --i >= 0;) //note reverse iteration
        {
            if(digits[i]<y.digits[i])
            {
                return true;
            }
            if(digits[i]>y.digits[i])
            {
                return false;
            }
        }
        return false;
    }

    template<int ydigits>
    bool operator>(const bigint<ydigits> &y) const { return y<*this; }

    template<int ydigits>
    bool operator<=(const bigint<ydigits> &y) const { return !(y<*this); }

    template<int ydigits>
    bool operator>=(const bigint<ydigits> &y) const { return !(*this<y); }
};

constexpr int gfdigits = (192+bidigitbits-1)/bidigitbits;

typedef bigint<gfdigits+1> gfint;

/* NIST prime Galois fields.
 * Currently only supports NIST P-192, where P=2^192-2^64-1
 */
struct gfield : gfint
{
    static const gfield P;

    gfield() {}
    gfield(digit n) : gfint(n) {}
    gfield(const char *s) : gfint(s) {}

    template<int ydigits>
    gfield(const bigint<ydigits> &y) : gfint(y) {}

    template<int ydigits>
    gfield &operator=(const bigint<ydigits> &y)
    {
        gfint::operator=(y);
        return *this;
    }

    template<int xdigits, int ydigits>
    gfield &add(const bigint<xdigits> &x, const bigint<ydigits> &y)
    {
        gfint::add(x, y);
        if(*this >= P) gfint::sub(*this, P);
        return *this;
    }
    template<int ydigits>
    gfield &add(const bigint<ydigits> &y) { return add(*this, y); }

    template<int xdigits>
    gfield &mul2(const bigint<xdigits> &x) { return add(x, x); }

    gfield &mul2() { return mul2(*this); }

    gfield &div2()
    {
        if(hasbit(0))
        {
            gfint::add(*this, P);
        }
        rshift(1);
        return *this;
    }

    template<int xdigits, int ydigits>
    gfield &sub(const bigint<xdigits> &x, const bigint<ydigits> &y)
    {
        if(x < y)
        {
            gfint tmp; /* necessary if this==&y, using this instead would clobber y */
            tmp.add(x, P);
            gfint::sub(tmp, y);
        }
        else
        {
            gfint::sub(x, y);
        }
        return *this;
    }
    template<int ydigits>
    gfield &sub(const bigint<ydigits> &y)
    {
        return sub(*this, y);
    }

    template<int xdigits>
    gfield &neg(const bigint<xdigits> &x)
    {
        gfint::sub(P, x);
        return *this;
    }
    gfield &neg()
    {
        return neg(*this);
    }

    template<int xdigits>
    gfield &square(const bigint<xdigits> &x)
    {
        return mul(x, x);
    }

    gfield &square()
    {
        return square(*this);
    }

    template<int xdigits, int ydigits>
    gfield &mul(const bigint<xdigits> &x, const bigint<ydigits> &y)
    {
        bigint<xdigits+ydigits> result;
        result.mul(x, y);
        reduce(result);
        return *this;
    }

    template<int ydigits>
    gfield &mul(const bigint<ydigits> &y)
    {
        return mul(*this, y);
    }

    template<int RESULT_DIGITS>
    void reduce(const bigint<RESULT_DIGITS> &result)
    {
        // B = T + S1 + S2 + S3 mod p
        copyshrinkdigits(result, gfdigits); // T
        if(result.morebits(192))
        {
            gfield s;
            s.copybits(0, result, 192, 64);
            s.dupbits(64, 0, 64);
            s.shrinkbits(128);
            add(s); // S1
            if(result.morebits(256))
            {
                s.zerobits(0, 64);
                s.copybits(64, result, 256, 64);
                s.dupbits(128, 64, 64);
                s.shrinkdigits(gfdigits);
                add(s); // S2
                if(result.morebits(320))
                {
                    s.copybits(0, result, 320, 64);
                    s.dupbits(64, 0, 64);
                    s.dupbits(128, 0, 64);
                    s.shrinkdigits(gfdigits);
                    add(s); // S3
                }
            }
        }
        else if(*this >= P)
        {
            gfint::sub(*this, P);
        }
    }

    template<int xdigits, int ydigits>
    gfield &pow(const bigint<xdigits> &x, const bigint<ydigits> &y)
    {
        gfield a(x);
        if(y.hasbit(0))
        {
            *this = a;
        }
        else
        {
            len = 1;
            digits[0] = 1;
            if(!y.len)
            {
                return *this;
            }
        }
        for(int i = 1, j = y.numbits(); i < j; i++)
        {
            a.square();
            if(y.hasbit(i))
            {
                mul(a);
            }
        }
        return *this;
    }

    template<int ydigits>
    gfield &pow(const bigint<ydigits> &y)
    {
        return pow(*this, y);
    }

    bool invert(const gfield &x)
    {
        if(!x.len)
        {
            return false;
        }
        gfint u(x), v(P), A((gfint::digit)1), C((gfint::digit)0);
        while(!u.iszero())
        {
            int ushift = 0, ashift = 0;
            while(!u.hasbit(ushift))
            {
                ushift++;
                if(A.hasbit(ashift))
                {
                    if(ashift)\
                    {
                        A.rshift(ashift);
                        ashift = 0;
                    }
                    A.add(P);
                }
                ashift++;
            }
            if(ushift)
            {
                u.rshift(ushift);
            }
            if(ashift)
            {
                A.rshift(ashift);
            }
            int vshift = 0,
                cshift = 0;
            while(!v.hasbit(vshift))
            {
                vshift++;
                if(C.hasbit(cshift))
                {
                    if(cshift)
                    {
                        C.rshift(cshift);
                        cshift = 0;
                    }
                    C.add(P);
                }
                cshift++;
            }
            if(vshift)
            {
                v.rshift(vshift);
            }
            if(cshift)
            {
                C.rshift(cshift);
            }
            if(u >= v)
            {
                u.sub(v);
                if(A < C)
                {
                    A.add(P);
                }
                A.sub(C);
            }
            else
            {
                v.sub(v, u);
                if(C < A)
                {
                    C.add(P);
                }
                C.sub(A);
            }
        }
        if(C >= P)
        {
            gfint::sub(C, P);
        }
        else
        {
            len = C.len;
            memcpy(digits, C.digits, len*sizeof(digit));
        }
        return true;
    }

    void invert()
    {
        invert(*this);
    }

    template<int xdigits>
    static int legendre(const bigint<xdigits> &x)
    {
        static const gfint Psub1div2(gfint(P).sub(bigint<1>(1)).rshift(1));
        gfield L;
        L.pow(x, Psub1div2);
        if(!L.len)
        {
            return 0;
        }
        if(L.len==1)
        {
            return 1;
        }
        return -1;
    }

    int legendre() const
    {
        return legendre(*this);
    }

    bool sqrt(const gfield &x)
    {
        if(!x.len)
        {
            len = 0;
            return true;
        }
        static const gfint Padd1div4(gfint(P).add(bigint<1>(1)).rshift(2));
        switch(legendre(x))
        {
            case 0:
            {
                len = 0;
                return true;
            }
            case -1:
            {
                return false;
            }
            default:
            {
                pow(x, Padd1div4);
                return true;
            }
        }
    }
    bool sqrt()
    {
        return sqrt(*this);
    }
};

struct ecjacobian
{
    static const gfield B;
    static const ecjacobian base;
    static const ecjacobian origin;

    gfield x, y, z;

    ecjacobian() {}
    ecjacobian(const gfield &x, const gfield &y) : x(x), y(y), z(bigint<1>(1)) {}
    ecjacobian(const gfield &x, const gfield &y, const gfield &z) : x(x), y(y), z(z) {}

    void mul2()
    {
        if(z.iszero())
        {
            return;
        }
        else if(y.iszero())
        {
            *this = origin;
            return;
        }
        gfield a, b, c, d;
        d.sub(x, c.square(z));
        d.mul(c.add(x));
        c.mul2(d).add(d);
        z.mul(y).add(z);
        a.square(y);
        b.mul2(a);
        d.mul2(x).mul(b);
        x.square(c).sub(d).sub(d);
        a.square(b).add(a);
        y.sub(d, x).mul(c).sub(a);
    }

    void add(const ecjacobian &q)
    {
        if(q.z.iszero())
        {
            return;
        }
        else if(z.iszero())
        {
            *this = q;
            return;
        }
        gfield a, b, c, d, e, f;
        a.square(z);
        b.mul(q.y, a).mul(z);
        a.mul(q.x);
        if(q.z.isone())
        {
            c.add(x, a);
            d.add(y, b);
            a.sub(x, a);
            b.sub(y, b);
        }
        else
        {
            f.mul(y, e.square(q.z)).mul(q.z);
            e.mul(x);
            c.add(e, a);
            d.add(f, b);
            a.sub(e, a);
            b.sub(f, b);
        }
        if(a.iszero())
        {
            if(b.iszero())
            {
                mul2();
            }
            else
            {
                *this = origin;
            }
            return;
        }
        if(!q.z.isone())
        {
            z.mul(q.z);
        }
        z.mul(a);
        x.square(b).sub(f.mul(c, e.square(a)));
        y.sub(f, x).sub(x).mul(b).sub(e.mul(a).mul(d)).div2();
    }

    template<int Q_DIGITS>
    void mul(const ecjacobian &p, const bigint<Q_DIGITS> &q)
    {
        *this = origin;
        for(int i = q.numbits(); --i >= 0;) //note reverse iteration
        {
            mul2();
            if(q.hasbit(i))
            {
                add(p);
            }
        }
    }
    template<int Q_DIGITS>
    void mul(const bigint<Q_DIGITS> &q)
    {
        ecjacobian tmp(*this);
        mul(tmp, q);
    }

    void normalize()
    {
        if(z.iszero() || z.isone())
        {
            return;
        }
        gfield tmp;
        z.invert();
        tmp.square(z);
        x.mul(tmp);
        y.mul(tmp).mul(z);
        z = bigint<1>(1);
    }

    bool calcy(bool ybit)
    {
        gfield y2, tmp;
        y2.square(x).mul(x).sub(tmp.add(x, x).add(x)).add(B);
        if(!y.sqrt(y2))
        {
            y.zero();
            return false;
        }
        if(y.hasbit(0) != ybit)
        {
            y.neg();
        }
        return true;
    }

    void print(vector<char> &buf)
    {
        normalize();
        buf.add(y.hasbit(0) ? '-' : '+');
        x.printdigits(buf);
    }

    void parse(const char *s)
    {
        bool ybit = *s++ == '-';
        x.parse(s);
        calcy(ybit);
        z = bigint<1>(1);
    }
};

const ecjacobian ecjacobian::origin(gfield((gfield::digit)1), gfield((gfield::digit)1), gfield((gfield::digit)0));

const gfield gfield::P("fffffffffffffffffffffffffffffffeffffffffffffffff");
const gfield ecjacobian::B("64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1");
const ecjacobian ecjacobian::base(
    gfield("188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012"),
    gfield("07192b95ffc8da78631011ed6b24cdd573f977a11e794811")
);

void calcpubkey(gfint privkey, vector<char> &pubstr)
{
    ecjacobian c(ecjacobian::base);
    c.mul(privkey);
    c.normalize();
    c.print(pubstr);
    pubstr.add('\0');
}

bool calcpubkey(const char *privstr, vector<char> &pubstr)
{
    if(!privstr[0])
    {
        return false;
    }
    gfint privkey;
    privkey.parse(privstr);
    calcpubkey(privkey, pubstr);
    return true;
}

void genprivkey(const char *seed, vector<char> &privstr, vector<char> &pubstr)
{
    tiger::hashval hash;
    tiger::hash(reinterpret_cast<const uchar *>(seed), static_cast<int>(std::strlen(seed)), hash);
    bigint<8*sizeof(hash.bytes)/bidigitbits> privkey;
    memcpy(privkey.digits, hash.bytes, sizeof(hash.bytes));
    privkey.len = 8*sizeof(hash.bytes)/bidigitbits;
    privkey.shrink();
    privkey.printdigits(privstr);
    privstr.add('\0');

    calcpubkey(privkey, pubstr);
}

bool hashstring(const char *str, char *result, int maxlen)
{
    tiger::hashval hv;
    if(maxlen < 2*static_cast<int>(sizeof(hv.bytes) + 1))
    {
        return false;
    }
    tiger::hash(const_cast<uchar *>(reinterpret_cast<const uchar *>(str)), std::strlen(str), hv);
    for(int i = 0; i < static_cast<int>(sizeof(hv.bytes)); ++i)
    {
        uchar c = hv.bytes[i];
        *result++ = "0123456789abcdef"[c>>4];
        *result++ = "0123456789abcdef"[c&0xF];
    }
    *result = '\0';
    return true;
}

void answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr)
{
    gfint privkey;
    privkey.parse(privstr);
    ecjacobian answer;
    answer.parse(challenge);
    answer.mul(privkey);
    answer.normalize();
    answer.x.printdigits(answerstr);
    answerstr.add('\0');
}

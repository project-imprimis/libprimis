#include "../libprimis-headers/cube.h"
#include "geomexts.h"

/* This file defines matrix3, matrix4, and matrix4x3's member functions.
 * The definitions of the classes themselves are in geom.h (one of the shared
 * header files)
 *
 */

//needed for det3
/* det2x2()
 *
 * returns the determinant of a 2x2 matrix, provided a set of four doubles
 * (rather than as a matrix2 object)
 */
static double det2x2(double a, double b, double c, double d)
{
    return a*d - b*c;
}

//needed to invert matrix below
/* det3x3()
 *
 * returns the determinant of a 3x3 matrix, provided a set of nine doubles
 * (rather than as a matrix3 object)
 */
static double det3x3(double a1, double a2, double a3,
                            double b1, double b2, double b3,
                            double c1, double c2, double c3)
{
    return a1 * det2x2(b2, b3, c2, c3)
         - b1 * det2x2(a2, a3, c2, c3)
         + c1 * det2x2(a2, a3, b2, b3);
}

// =============================================================================
//  matrix3 (3x3) object
// =============================================================================

matrix3::matrix3() : a(0,0,0), b(0,0,0), c(0,0,0)
{
}

matrix3::matrix3(const vec &a, const vec &b, const vec &c) : a(a), b(b), c(c)
{
}

matrix3::matrix3(const quat &q)
{
    float x   = q.x,   y  = q.y,    z = q.z, w = q.w,
          tx  = 2*x,   ty = 2*y,   tz = 2*z,
          txx = tx*x, tyy = ty*y, tzz = tz*z,
          txy = tx*y, txz = tx*z, tyz = ty*z,
          twx = w*tx, twy = w*ty, twz = w*tz;
    a = vec(1 - (tyy + tzz), txy + twz, txz - twy);
    b = vec(txy - twz, 1 - (txx + tzz), tyz + twx);
    c = vec(txz + twy, tyz - twx, 1 - (txx + tyy));
}

matrix3::matrix3(float angle, const vec &axis)
{
    rotate(angle, axis);
}

void matrix3::mul(const matrix3 &m, const matrix3 &n)
{
    a = vec(m.a).mul(n.a.x).madd(m.b, n.a.y).madd(m.c, n.a.z);
    b = vec(m.a).mul(n.b.x).madd(m.b, n.b.y).madd(m.c, n.b.z);
    c = vec(m.a).mul(n.c.x).madd(m.b, n.c.y).madd(m.c, n.c.z);
}

void matrix3::mul(const matrix3 &n)
{
    mul(matrix3(*this), n);
}

void matrix3::multranspose(const matrix3 &m, const matrix3 &n)
{
    a = vec(m.a).mul(n.a.x).madd(m.b, n.b.x).madd(m.c, n.c.x);
    b = vec(m.a).mul(n.a.y).madd(m.b, m.b.y).madd(m.c, n.c.y);
    c = vec(m.a).mul(n.a.z).madd(m.b, n.b.z).madd(m.c, n.c.z);
}
void matrix3::multranspose(const matrix3 &n)
{
    multranspose(matrix3(*this), n);
}

void matrix3::transposemul(const matrix3 &m, const matrix3 &n)
{
    a = vec(m.a.dot(n.a), m.b.dot(n.a), m.c.dot(n.a));
    b = vec(m.a.dot(n.b), m.b.dot(n.b), m.c.dot(n.b));
    c = vec(m.a.dot(n.c), m.b.dot(n.c), m.c.dot(n.c));
}

void matrix3::transposemul(const matrix3 &n)
{
    transposemul(matrix3(*this), n);
}

void matrix3::transpose()
{
    std::swap(a.y, b.x); std::swap(a.z, c.x);
    std::swap(b.z, c.y);
}

void matrix3::transpose(const matrix3 &m)
{
    a = vec(m.a.x, m.b.x, m.c.x);
    b = vec(m.a.y, m.b.y, m.c.y);
    c = vec(m.a.z, m.b.z, m.c.z);
}

void matrix3::invert(const matrix3 &o)
{
    vec unscale(1/o.a.squaredlen(), 1/o.b.squaredlen(), 1/o.c.squaredlen());
    transpose(o);
    a.mul(unscale);
    b.mul(unscale);
    c.mul(unscale);
}

void matrix3::invert()
{
    invert(matrix3(*this));
}

void matrix3::normalize()
{
    a.normalize();
    b.normalize();
    c.normalize();
}

void matrix3::scale(float k)
{
    a.mul(k);
    b.mul(k);
    c.mul(k);
}

void matrix3::rotate(float angle, const vec &axis)
{
    rotate(cosf(angle), std::sin(angle), axis);
}

void matrix3::rotate(float ck, float sk, const vec &axis)
{
    a = vec(axis.x*axis.x*(1-ck)+ck, axis.x*axis.y*(1-ck)+axis.z*sk, axis.x*axis.z*(1-ck)-axis.y*sk);
    b = vec(axis.x*axis.y*(1-ck)-axis.z*sk, axis.y*axis.y*(1-ck)+ck, axis.y*axis.z*(1-ck)+axis.x*sk);
    c = vec(axis.x*axis.z*(1-ck)+axis.y*sk, axis.y*axis.z*(1-ck)-axis.x*sk, axis.z*axis.z*(1-ck)+ck);
}

void matrix3::setyaw(float ck, float sk)
{
    a = vec(ck, sk, 0);
    b = vec(-sk, ck, 0);
    c = vec(0, 0, 1);
}

void matrix3::setyaw(float angle)
{
    setyaw(cosf(angle), std::sin(angle));
}

float matrix3::trace() const
{
    return a.x + b.y + c.z;
}

bool matrix3::calcangleaxis(float tr, float &angle, vec &axis, float threshold) const
{
    if(tr <= -1)
    {
        if(a.x >= b.y && a.x >= c.z)
        {
            float r = 1 + a.x - b.y - c.z;
            if(r <= threshold)
            {
                return false;
            }
            r = sqrtf(r);
            axis.x = 0.5f*r;
            axis.y = b.x/r;
            axis.z = c.x/r;
        }
        else if(b.y >= c.z)
        {
            float r = 1 + b.y - a.x - c.z;
            if(r <= threshold)
            {
                return false;
            }
            r = sqrtf(r);
            axis.y = 0.5f*r;
            axis.x = b.x/r;
            axis.z = c.y/r;
        }
        else
        {
            float r = 1 + b.y - a.x - c.z;
            if(r <= threshold)
            {
                return false;
            }
            r = sqrtf(r);
            axis.z = 0.5f*r;
            axis.x = c.x/r;
            axis.y = c.y/r;
        }
        angle = M_PI;
    }
    else if(tr >= 3)
    {
        axis = vec(0, 0, 1);
        angle = 0;
    }
    else
    {
        axis = vec(b.z - c.y, c.x - a.z, a.y - b.x);
        float r = axis.squaredlen();
        if(r <= threshold)
        {
            return false;
        }
        axis.mul(1/sqrtf(r));
        angle = acosf(0.5f*(tr - 1));
    }
    return true;
}

bool matrix3::calcangleaxis(float &angle, vec &axis, float threshold) const
{
    return calcangleaxis(trace(), angle, axis, threshold);
}

vec matrix3::transform(const vec &o) const
{
    return vec(a).mul(o.x).madd(b, o.y).madd(c, o.z);
}

vec matrix3::transposedtransform(const vec &o) const
{
    return vec(a.dot(o), b.dot(o), c.dot(o));
}

vec matrix3::abstransform(const vec &o) const
{
    return vec(a).mul(o.x).abs().add(vec(b).mul(o.y).abs()).add(vec(c).mul(o.z).abs());
}

vec matrix3::abstransposedtransform(const vec &o) const
{
    return vec(a.absdot(o), b.absdot(o), c.absdot(o));
}

void matrix3::identity()
{
    a = vec(1, 0, 0);
    b = vec(0, 1, 0);
    c = vec(0, 0, 1);
}

void matrix3::rotate_around_x(float ck, float sk)
{
    vec rb = vec(b).mul(ck).madd(c, sk),
        rc = vec(c).mul(ck).msub(b, sk);
    b = rb;
    c = rc;
}
void matrix3::rotate_around_x(float angle)
{
    rotate_around_x(cosf(angle), std::sin(angle));
}

void matrix3::rotate_around_x(const vec2 &sc)
{
    rotate_around_x(sc.x, sc.y);
}

void matrix3::rotate_around_y(float ck, float sk)
{
    vec rc = vec(c).mul(ck).madd(a, sk),
        ra = vec(a).mul(ck).msub(c, sk);
    c = rc;
    a = ra;
}

void matrix3::rotate_around_y(float angle)
{
    rotate_around_y(cosf(angle), std::sin(angle));
}

void matrix3::rotate_around_y(const vec2 &sc)
{
    rotate_around_y(sc.x, sc.y);
}

void matrix3::rotate_around_z(float ck, float sk)
{
    vec ra = vec(a).mul(ck).madd(b, sk),
        rb = vec(b).mul(ck).msub(a, sk);
    a = ra;
    b = rb;
}

void matrix3::rotate_around_z(float angle)
{
    rotate_around_z(cosf(angle), std::sin(angle));
}

void matrix3::rotate_around_z(const vec2 &sc)
{
    rotate_around_z(sc.x, sc.y);
}

vec matrix3::transform(const vec2 &o)
{
    return vec(a).mul(o.x).madd(b, o.y);
}

vec matrix3::transposedtransform(const vec2 &o) const
{
    return vec(a.dot2(o), b.dot2(o), c.dot2(o));
}

vec matrix3::rowx() const
{
    return vec(a.x, b.x, c.x);
}

vec matrix3::rowy() const
{
    return vec(a.y, b.y, c.y);
}

vec matrix3::rowz() const
{
    return vec(a.z, b.z, c.z);
}

// =============================================================================
//  matrix4 (4x4) object
// =============================================================================

/* invert()
 *
 * sets the matrix values to the inverse of the provided matrix A*A^-1 = I
 * returns false if singular (or nearly singular to within tolerance of mindet)
 * or true if matrix was inverted successfully
 *
 * &m: a matrix4 object to be inverted and assigned to the object
 * mindet: the minimum value at which matrices are considered
 */
bool matrix4::invert(const matrix4 &m, double mindet)
{
    double a1 = m.a.x, a2 = m.a.y, a3 = m.a.z, a4 = m.a.w,
           b1 = m.b.x, b2 = m.b.y, b3 = m.b.z, b4 = m.b.w,
           c1 = m.c.x, c2 = m.c.y, c3 = m.c.z, c4 = m.c.w,
           d1 = m.d.x, d2 = m.d.y, d3 = m.d.z, d4 = m.d.w,
           det1 =  det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4),
           det2 = -det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4),
           det3 =  det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4),
           det4 = -det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4),
           det = a1*det1 + b1*det2 + c1*det3 + d1*det4;

    if(std::fabs(det) < mindet)
    {
        return false;
    }

    double invdet = 1/det;

    a.x = det1 * invdet;
    a.y = det2 * invdet;
    a.z = det3 * invdet;
    a.w = det4 * invdet;

    b.x = -det3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4) * invdet;
    b.y =  det3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4) * invdet;
    b.z = -det3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4) * invdet;
    b.w =  det3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4) * invdet;

    c.x =  det3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4) * invdet;
    c.y = -det3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4) * invdet;
    c.z =  det3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4) * invdet;
    c.w = -det3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4) * invdet;

    d.x = -det3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3) * invdet;
    d.y =  det3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3) * invdet;
    d.z = -det3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3) * invdet;
    d.w =  det3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3) * invdet;

    return true;
}

matrix4 matrix4::inverse(double mindet) const
{
    matrix4 ret;
    if(ret.invert(*this, mindet))
    {
        return ret;
    }
    else
    {
        return { vec4<float>(0,0,0,0), vec4<float>(0,0,0,0), vec4<float>(0,0,0,0), vec4<float>(0,0,0,0) };
    }
}

matrix4::matrix4() : a(0,0,0,0), b(0,0,0,0), c(0,0,0,0), d(0,0,0,0)
{
}

matrix4::matrix4(const float *m) : a(m), b(m+4), c(m+8), d(m+12)
{
}

matrix4::matrix4(const vec &a, const vec &b, const vec &c)
    : a(a.x, b.x, c.x, 0), b(a.y, b.y, c.y, 0), c(a.z, b.z, c.z, 0), d(0, 0, 0, 1)
{
}

matrix4::matrix4(const vec4<float> &a, const vec4<float> &b, const vec4<float> &c, const vec4<float> &d)
    : a(a), b(b), c(c), d(d)
{
}

matrix4::matrix4(const matrix4x3 &m)
    : a(m.a, 0), b(m.b, 0), c(m.c, 0), d(m.d, 1)
{
}

matrix4::matrix4(const matrix3 &rot, const vec &trans)
    : a(rot.a, 0), b(rot.b, 0), c(rot.c, 0), d(trans, 1)
{
}

void matrix4::transposedtransform(const plane &in, plane &out) const
{
    out.x = in.dist(a);
    out.y = in.dist(b);
    out.z = in.dist(c);
    out.offset = in.dist(d);
}

void matrix4::mul(const matrix4 &x, const matrix3 &y)
{
    a = vec4<float>(x.a).mul(y.a.x).madd(x.b, y.a.y).madd(x.c, y.a.z);
    b = vec4<float>(x.a).mul(y.b.x).madd(x.b, y.b.y).madd(x.c, y.b.z);
    c = vec4<float>(x.a).mul(y.c.x).madd(x.b, y.c.y).madd(x.c, y.c.z);
    d = x.d;
}

void matrix4::mul(const matrix3 &y)
{
    mul(matrix4(*this), y);
}

void matrix4::mul(const matrix4 &x, const matrix4 &y)
{
    mult<vec4<float>>(x, y);
}

void matrix4::mul(const matrix4 &y)
{
    mult<vec4<float>>(matrix4(*this), y);
}

void matrix4::muld(const matrix4 &x, const matrix4 &y)
{
    mult<vec4<float>>(x, y);
}

void matrix4::muld(const matrix4 &y)
{
    mult<vec4<float>>(matrix4(*this), y);
}

void matrix4::rotate_around_x(float ck, float sk)
{
    vec4<float> rb = vec4<float>(b).mul(ck).madd(c, sk),
                rc = vec4<float>(c).mul(ck).msub(b, sk);
    b = rb;
    c = rc;
}

void matrix4::rotate_around_x(float angle)
{
    rotate_around_x(cosf(angle), std::sin(angle));
}

void matrix4::rotate_around_x(const vec2 &sc)
{
    rotate_around_x(sc.x, sc.y);
}

void matrix4::rotate_around_y(float ck, float sk)
{
    vec4<float> rc = vec4<float>(c).mul(ck).madd(a, sk),
                ra = vec4<float>(a).mul(ck).msub(c, sk);
    c = rc;
    a = ra;
}

void matrix4::rotate_around_y(float angle)
{
    rotate_around_y(cosf(angle), std::sin(angle));
}

void matrix4::rotate_around_y(const vec2 &sc)
{
    rotate_around_y(sc.x, sc.y);
}

void matrix4::rotate_around_z(float ck, float sk)
{
    vec4<float> ra = vec4<float>(a).mul(ck).madd(b, sk),
                rb = vec4<float>(b).mul(ck).msub(a, sk);
    a = ra;
    b = rb;
}

void matrix4::rotate_around_z(float angle)
{
    rotate_around_z(cosf(angle), std::sin(angle));
}

void matrix4::rotate_around_z(const vec2 &sc)
{
    rotate_around_z(sc.x, sc.y);
}

void matrix4::rotate(float ck, float sk, const vec &axis)
{
    matrix3 m;
    m.rotate(ck, sk, axis);
    mul(m);
}

void matrix4::rotate(float angle, const vec &dir)
{
    rotate(cosf(angle), std::sin(angle), dir);
}

void matrix4::rotate(const vec2 &sc, const vec &dir)
{
    rotate(sc.x, sc.y, dir);
}

void matrix4::identity()
{
    a = vec4<float>(1, 0, 0, 0);
    b = vec4<float>(0, 1, 0, 0);
    c = vec4<float>(0, 0, 1, 0);
    d = vec4<float>(0, 0, 0, 1);
}

void matrix4::settranslation(const vec &v)
{
    d.setxyz(v);
}

void matrix4::settranslation(float x, float y, float z)
{
    d.x = x;
    d.y = y;
    d.z = z;
}

void matrix4::translate(const vec &p)
{
    d.madd(a, p.x).madd(b, p.y).madd(c, p.z);
}

void matrix4::translate(float x, float y, float z)
{
    translate(vec(x, y, z));
}

void matrix4::translate(const vec &p, float scale)
{
    translate(vec(p).mul(scale));
}

void matrix4::setscale(float x, float y, float z)
{
    a.x = x;
    b.y = y;
    c.z = z;
}

void matrix4::setscale(const vec &v)
{
    setscale(v.x, v.y, v.z);
}

void matrix4::setscale(float n)
{
    setscale(n, n, n);
}

void matrix4::scale(float x, float y, float z)
{
    a.mul(x);
    b.mul(y);
    c.mul(z);
}

void matrix4::scale(const vec &v)
{
    scale(v.x, v.y, v.z);
}

void matrix4::scale(float n)
{
    scale(n, n, n);
}

void matrix4::scalez(float k)
{
    a.z *= k;
    b.z *= k;
    c.z *= k;
    d.z *= k;
}

void matrix4::jitter(float x, float y)
{
    a.x += x * a.w;
    a.y += y * a.w;
    b.x += x * b.w;
    b.y += y * b.w;
    c.x += x * c.w;
    c.y += y * c.w;
    d.x += x * d.w;
    d.y += y * d.w;
}

void matrix4::transpose()
{
    //swap upper triangular elements of row 'a'
    std::swap(a.y, b.x);
    std::swap(a.z, c.x);
    std::swap(a.w, d.x);
    //swap upper triangular elements of row 'b'
    std::swap(b.z, c.y);
    std::swap(b.w, d.y);
    //swap upper triangular element of row 'c'
    std::swap(c.w, d.z);
    //no upper triangular elements on row 'd'
}

void matrix4::transpose(const matrix4 &m)
{
    a = vec4<float>(m.a.x, m.b.x, m.c.x, m.d.x);
    b = vec4<float>(m.a.y, m.b.y, m.c.y, m.d.y);
    c = vec4<float>(m.a.z, m.b.z, m.c.z, m.d.z);
    d = vec4<float>(m.a.w, m.b.w, m.c.w, m.d.w);
}

void matrix4::frustum(float left, float right, float bottom, float top, float znear, float zfar)
{
    float width = right - left,
          height = top - bottom,
          zrange = znear - zfar;
    a = vec4<float>(2*znear/width, 0, 0, 0);
    b = vec4<float>(0, 2*znear/height, 0, 0);
    c = vec4<float>((right + left)/width, (top + bottom)/height, (zfar + znear)/zrange, -1);
    d = vec4<float>(0, 0, 2*znear*zfar/zrange, 0);
}

void matrix4::perspective(float fovy, float aspect, float znear, float zfar)
{
    float ydist = znear * std::tan(fovy/(2*RAD)),
          xdist = ydist * aspect;
    frustum(-xdist, xdist, -ydist, ydist, znear, zfar);
}

void matrix4::ortho(float left, float right, float bottom, float top, float znear, float zfar)
{
    float width = right - left,
          height = top - bottom,
          zrange = znear - zfar;
    a = vec4<float>(2/width, 0, 0, 0);
    b = vec4<float>(0, 2/height, 0, 0);
    c = vec4<float>(0, 0, 2/zrange, 0);
    d = vec4<float>(-(right+left)/width, -(top+bottom)/height, (zfar+znear)/zrange, 1);
}

void matrix4::transform(const vec &in, vec &out) const
{
    out = vec(a).mul(in.x).add(vec(b).mul(in.y)).add(vec(c).mul(in.z)).add(vec(d));
}

void matrix4::transform(const vec4<float> &in, vec &out) const
{
    out = vec(a).mul(in.x).add(vec(b).mul(in.y)).add(vec(c).mul(in.z)).add(vec(d).mul(in.w));
}

void matrix4::transform(const vec &in, vec4<float> &out) const
{
    out = vec4<float>(a).mul(in.x).madd(b, in.y).madd(c, in.z).add(d);
}

void matrix4::transform(const vec4<float> &in, vec4<float> &out) const
{
    out = vec4<float>(a).mul(in.x).madd(b, in.y).madd(c, in.z).madd(d, in.w);
}

void matrix4::transformnormal(const vec &in, vec &out) const
{
    out = vec(a).mul(in.x).add(vec(b).mul(in.y)).add(vec(c).mul(in.z));
}

void matrix4::transformnormal(const vec &in, vec4<float> &out) const
{
    out = vec4<float>(a).mul(in.x).madd(b, in.y).madd(c, in.z);
}

void matrix4::transposedtransform(const vec &in, vec &out) const
{
    vec p = vec(in).sub(vec(d));
    out.x = a.dot3(p);
    out.y = b.dot3(p);
    out.z = c.dot3(p);
}

void matrix4::transposedtransformnormal(const vec &in, vec &out) const
{
    out.x = a.dot3(in);
    out.y = b.dot3(in);
    out.z = c.dot3(in);
}

vec matrix4::gettranslation() const
{
    return vec(d);
}

vec4<float> matrix4::rowx() const
{
    return vec4<float>(a.x, b.x, c.x, d.x);
}

vec4<float> matrix4::rowy() const
{
    return vec4<float>(a.y, b.y, c.y, d.y);
}

vec4<float> matrix4::rowz() const
{
    return vec4<float>(a.z, b.z, c.z, d.z);
}

vec4<float> matrix4::roww() const
{
    return vec4<float>(a.w, b.w, c.w, d.w);
}

vec2 matrix4::lineardepthscale() const
{
    return vec2(d.w, -d.z).div(c.z*d.w - d.z*c.w);
}

// =============================================================================
//  matrix4x3 object
// =============================================================================

matrix4x3::matrix4x3() : a(0,0,0), b(0,0,0), c(0,0,0), d(0,0,0)
{
}

matrix4x3::matrix4x3(const vec &a, const vec &b, const vec &c, const vec &d) : a(a), b(b), c(c), d(d)
{
}

matrix4x3::matrix4x3(const matrix3 &rot, const vec &trans) : a(rot.a), b(rot.b), c(rot.c), d(trans)
{
}

matrix4x3::matrix4x3(const dualquat &dq)
{
    vec4<float> r = vec4<float>(dq.real).mul(1/dq.real.squaredlen()), rr = vec4<float>(r).mul(dq.real);
    r.mul(2);
    float xy = r.x*dq.real.y, xz = r.x*dq.real.z, yz = r.y*dq.real.z,
          wx = r.w*dq.real.x, wy = r.w*dq.real.y, wz = r.w*dq.real.z;
    a = vec(rr.w + rr.x - rr.y - rr.z, xy + wz, xz - wy);
    b = vec(xy - wz, rr.w + rr.y - rr.x - rr.z, yz + wx);
    c = vec(xz + wy, yz - wx, rr.w + rr.z - rr.x - rr.y);
    d = vec(-(dq.dual.w*r.x - dq.dual.x*r.w + dq.dual.y*r.z - dq.dual.z*r.y),
            -(dq.dual.w*r.y - dq.dual.x*r.z - dq.dual.y*r.w + dq.dual.z*r.x),
            -(dq.dual.w*r.z + dq.dual.x*r.y - dq.dual.y*r.x - dq.dual.z*r.w));
}

void matrix4x3::mul(float k)
{
    a.mul(k);
    b.mul(k);
    c.mul(k);
    d.mul(k);
}

void matrix4x3::setscale(float x, float y, float z)
{
    a.x = x;
    b.y = y;
    c.z = z;
}

void matrix4x3::setscale(const vec &v)
{
    setscale(v.x, v.y, v.z);
}

void matrix4x3::setscale(float n)
{
    setscale(n, n, n);
}

void matrix4x3::scale(float x, float y, float z)
{
    a.mul(x);
    b.mul(y);
    c.mul(z);
}

void matrix4x3::scale(const vec &v)
{
    scale(v.x, v.y, v.z);
}

void matrix4x3::scale(float n)
{
    scale(n, n, n);
}

void matrix4x3::settranslation(const vec &p)
{
    d = p;
}

void matrix4x3::settranslation(float x, float y, float z)
{
    d = vec(x, y, z);
}

void matrix4x3::translate(const vec &p)
{
    d.madd(a, p.x).madd(b, p.y).madd(c, p.z);
}

void matrix4x3::translate(float x, float y, float z)
{
    translate(vec(x, y, z));
}

void matrix4x3::translate(const vec &p, float scale)
{
    translate(vec(p).mul(scale));
}

void matrix4x3::accumulate(const matrix4x3 &m, float k)
{
    a.madd(m.a, k);
    b.madd(m.b, k);
    c.madd(m.c, k);
    d.madd(m.d, k);
}

void matrix4x3::normalize()
{
    a.normalize();
    b.normalize();
    c.normalize();
}

void matrix4x3::lerp(const matrix4x3 &to, float t)
{
    a.lerp(to.a, t);
    b.lerp(to.b, t);
    c.lerp(to.c, t);
    d.lerp(to.d, t);
}
void matrix4x3::lerp(const matrix4x3 &from, const matrix4x3 &to, float t)
{
    a.lerp(from.a, to.a, t);
    b.lerp(from.b, to.b, t);
    c.lerp(from.c, to.c, t);
    d.lerp(from.d, to.d, t);
}

void matrix4x3::identity()
{
    a = vec(1, 0, 0);
    b = vec(0, 1, 0);
    c = vec(0, 0, 1);
    d = vec(0, 0, 0);
}

void matrix4x3::mul(const matrix4x3 &m, const matrix4x3 &n)
{
    a = vec(m.a).mul(n.a.x).madd(m.b, n.a.y).madd(m.c, n.a.z);
    b = vec(m.a).mul(n.b.x).madd(m.b, n.b.y).madd(m.c, n.b.z);
    c = vec(m.a).mul(n.c.x).madd(m.b, n.c.y).madd(m.c, n.c.z);
    d = vec(m.d).madd(m.a, n.d.x).madd(m.b, n.d.y).madd(m.c, n.d.z);
}

void matrix4x3::mul(const matrix4x3 &n)
{
    mul(matrix4x3(*this), n);
}

void matrix4x3::mul(const matrix3 &m, const matrix4x3 &n)
{
    a = vec(m.a).mul(n.a.x).madd(m.b, n.a.y).madd(m.c, n.a.z);
    b = vec(m.a).mul(n.b.x).madd(m.b, n.b.y).madd(m.c, n.b.z);
    c = vec(m.a).mul(n.c.x).madd(m.b, n.c.y).madd(m.c, n.c.z);
    d = vec(m.a).mul(n.d.x).madd(m.b, n.d.y).madd(m.c, n.d.z);
}

void matrix4x3::mul(const matrix3 &rot, const vec &trans, const matrix4x3 &n)
{
    mul(rot, n);
    d.add(trans);
}

void matrix4x3::transpose()
{
    d = vec(a.dot(d), b.dot(d), c.dot(d)).neg();
    std::swap(a.y, b.x);
    std::swap(a.z, c.x);
    std::swap(b.z, c.y);
}

void matrix4x3::transpose(const matrix4x3 &o)
{
    a = vec(o.a.x, o.b.x, o.c.x);
    b = vec(o.a.y, o.b.y, o.c.y);
    c = vec(o.a.z, o.b.z, o.c.z);
    d = vec(o.a.dot(o.d), o.b.dot(o.d), o.c.dot(o.d)).neg();
}

void matrix4x3::transposemul(const matrix4x3 &m, const matrix4x3 &n)
{
    vec t(m.a.dot(m.d), m.b.dot(m.d), m.c.dot(m.d));
    a = vec(m.a.dot(n.a), m.b.dot(n.a), m.c.dot(n.a));
    b = vec(m.a.dot(n.b), m.b.dot(n.b), m.c.dot(n.b));
    c = vec(m.a.dot(n.c), m.b.dot(n.c), m.c.dot(n.c));
    d = vec(m.a.dot(n.d), m.b.dot(n.d), m.c.dot(n.d)).sub(t);
}

void matrix4x3::multranspose(const matrix4x3 &m, const matrix4x3 &n)
{
    vec t(n.a.dot(n.d), n.b.dot(n.d), n.c.dot(n.d));
    a = vec(m.a).mul(n.a.x).madd(m.b, n.b.x).madd(m.c, n.c.x);
    b = vec(m.a).mul(n.a.y).madd(m.b, m.b.y).madd(m.c, n.c.y);
    c = vec(m.a).mul(n.a.z).madd(m.b, n.b.z).madd(m.c, n.c.z);
    d = vec(m.d).msub(m.a, t.x).msub(m.b, t.y).msub(m.c, t.z);
}

void matrix4x3::invert(const matrix4x3 &o)
{
    vec unscale(1/o.a.squaredlen(), 1/o.b.squaredlen(), 1/o.c.squaredlen());
    transpose(o);
    a.mul(unscale);
    b.mul(unscale);
    c.mul(unscale);
    d.mul(unscale);
}

void matrix4x3::invert()
{
    invert(matrix4x3(*this));
}

void matrix4x3::rotate(float angle, const vec &d)
{
    rotate(cosf(angle), std::sin(angle), d);
}

void matrix4x3::rotate(float ck, float sk, const vec &axis)
{
    matrix3 m;
    m.rotate(ck, sk, axis);
    *this = matrix4x3(m, vec(0, 0, 0));
}

void matrix4x3::rotate_around_x(float ck, float sk)
{
    vec rb = vec(b).mul(ck).madd(c, sk),
        rc = vec(c).mul(ck).msub(b, sk);
    b = rb;
    c = rc;
}
void matrix4x3::rotate_around_x(float angle)
{
    rotate_around_x(cosf(angle), std::sin(angle));
}

void matrix4x3::rotate_around_x(const vec2 &sc)
{
    rotate_around_x(sc.x, sc.y);
}

void matrix4x3::rotate_around_y(float ck, float sk)
{
    vec rc = vec(c).mul(ck).madd(a, sk),
        ra = vec(a).mul(ck).msub(c, sk);
    c = rc;
    a = ra;
}
void matrix4x3::rotate_around_y(float angle)
{
    rotate_around_y(cosf(angle), std::sin(angle));
}

void matrix4x3::rotate_around_y(const vec2 &sc)
{
    rotate_around_y(sc.x, sc.y);
}

void matrix4x3::rotate_around_z(float ck, float sk)
{
    vec ra = vec(a).mul(ck).madd(b, sk),
        rb = vec(b).mul(ck).msub(a, sk);
    a = ra;
    b = rb;
}

void matrix4x3::rotate_around_z(float angle)
{
    rotate_around_z(cosf(angle), std::sin(angle));
}

void matrix4x3::rotate_around_z(const vec2 &sc)
{
    rotate_around_z(sc.x, sc.y);
}

vec matrix4x3::transposedtransform(const vec &o) const
{
    vec p = vec(o).sub(d);
    return vec(a.dot(p), b.dot(p), c.dot(p));
}

vec matrix4x3::transformnormal(const vec &o) const
{
    return vec(a).mul(o.x).madd(b, o.y).madd(c, o.z);
}

vec matrix4x3::transposedtransformnormal(const vec &o) const
{
    return vec(a.dot(o), b.dot(o), c.dot(o));
}

vec matrix4x3::transform(const vec &o) const
{
    return vec(d).madd(a, o.x).madd(b, o.y).madd(c, o.z);
}

vec matrix4x3::transform(const vec2 &o) const
{
    return vec(d).madd(a, o.x).madd(b, o.y);
}

vec4<float> matrix4x3::rowx() const
{
    return vec4<float>(a.x, b.x, c.x, d.x);
}

vec4<float> matrix4x3::rowy() const
{
    return vec4<float>(a.y, b.y, c.y, d.y);
}

vec4<float> matrix4x3::rowz() const
{
    return vec4<float>(a.z, b.z, c.z, d.z);
}

//end matrix4x3

/**
 * @file geomexts.h
 * @brief Internal engine geometry structures not needed in the API
 */
#ifndef GEOMEXTS_H_
#define GEOMEXTS_H_

/**
 * @brief half precision floating point object, similar to `float` but with less data bits
 */
struct half
{
    ushort val;

    half() {}
    half(float f) //truncates a 32 bit float to a 16 bit half
    {
        union
        {
            int i;
            float f;
        } conv;
        conv.f = f;
        ushort signbit = (conv.i>>(31-15)) & (1<<15),
               mantissa = (conv.i>>(23-10)) & 0x3FF;
        int exponent = ((conv.i>>23)&0xFF) - 127 + 15;
        if(exponent <= 0)
        {
            mantissa |= 0x400;
            mantissa >>= min(1-exponent, 10+1);
            exponent = 0;
        }
        else if(exponent >= 0x1F)
        {
            mantissa = 0;
            exponent = 0x1F;
        }
        val = signbit | (static_cast<ushort>(exponent)<<10) | mantissa;
    }

    bool operator==(const half &h) const { return val == h.val; }
    bool operator!=(const half &h) const { return val != h.val; }
};

struct triangle
{
    vec a, b, c;

    triangle(const vec &a, const vec &b, const vec &c) : a(a), b(b), c(c) {}
    triangle() {}

    triangle &add(const vec &o)
    {
        a.add(o);
        b.add(o);
        c.add(o);
        return *this;
    }

    triangle &sub(const vec &o)
    {
        a.sub(o);
        b.sub(o);
        c.sub(o);
        return *this;
    }

    bool operator==(const triangle &t) const
    {
        return a == t.a && b == t.b && c == t.c;
    }
};

/**
 * @brief An object in four-dimensional complex space.
 *
 * This object is a four component "vector" with three imaginary components and
 * one scalar component used for object rotations: quats have 6 DoF among their
 * four terms, avoiding gimbal lock inherent to Euler angles.
 *
 * Follows the Hamiltonian convention (i*j = k) for dimensional relationships.
 *
 * x represents the i component
 * y represents the j component
 * z represents the k component
 * w represents the real (scalar) component
 */
struct quat : vec4<float>
{
    public:
        quat() {}
        quat(float x, float y, float z, float w) : vec4<float>(x, y, z, w) {}
        quat(const vec &axis, float angle)
        {
            w = cosf(angle/2);
            float s = std::sin(angle/2);
            x = s*axis.x;
            y = s*axis.y;
            z = s*axis.z;
        }
        quat(const vec &u, const vec &v)
        {
            w = sqrtf(u.squaredlen() * v.squaredlen()) + u.dot(v);
            cross(u, v);
            normalize();
        }
        explicit quat(const vec &v)
        {
            x = v.x;
            y = v.y;
            z = v.z;
            restorew();
        }
        explicit quat(const matrix3 &m) { convertmatrix(m); }
        explicit quat(const matrix4x3 &m) { convertmatrix(m); }
        explicit quat(const matrix4 &m) { convertmatrix(m); }

        void restorew() { w = 1.0f-x*x-y*y-z*z; w = w<0 ? 0 : -sqrtf(w); }

        quat &add(const vec4<float> &o) { vec4<float>::add(o); return *this; }
        quat &sub(const vec4<float> &o) { vec4<float>::sub(o); return *this; }
        quat &mul(float k) { vec4<float>::mul(k); return *this; }
        quat &madd(const vec4<float> &a, const float &b) { return add(vec4<float>(a).mul(b)); }
        quat &msub(const vec4<float> &a, const float &b) { return sub(vec4<float>(a).mul(b)); }

        quat &mul(const quat &p, const quat &o)
        {
            x = p.w*o.x + p.x*o.w + p.y*o.z - p.z*o.y;
            y = p.w*o.y - p.x*o.z + p.y*o.w + p.z*o.x;
            z = p.w*o.z + p.x*o.y - p.y*o.x + p.z*o.w;
            w = p.w*o.w - p.x*o.x - p.y*o.y - p.z*o.z;
            return *this;
        }
        quat &mul(const quat &o) { return mul(quat(*this), o); }

        /**
         * @brief Turns this quaternion into its inverse.
         *
         * The inverse q⁻¹ of the quaternion (used in the rotation formula q*a*q⁻¹)
         * is one with the imaginary components equal to their additive inverse, with
         * no change to the real (scalar) component.
         *
         * @return a reference to `this` object
         */
        quat &invert() { neg3(); return *this; }

        quat &normalize() { vec4<float>::safenormalize(); return *this; }

        vec rotate(const vec &v) const
        {
            return vec().cross(*this, vec().cross(*this, v).madd(v, w)).mul(2).add(v);
        }

        vec invertedrotate(const vec &v) const
        {
            return vec().cross(*this, vec().cross(*this, v).msub(v, w)).mul(2).add(v);
        }

    private:
        template<class M>
        void convertmatrix(const M &m)
        {
            float trace = m.a.x + m.b.y + m.c.z;
            if(trace>0)
            {
                float r = sqrtf(1 + trace), inv = 0.5f/r;
                w = 0.5f*r;
                x = (m.b.z - m.c.y)*inv;
                y = (m.c.x - m.a.z)*inv;
                z = (m.a.y - m.b.x)*inv;
            }
            else if(m.a.x > m.b.y && m.a.x > m.c.z)
            {
                float r = sqrtf(1 + m.a.x - m.b.y - m.c.z), inv = 0.5f/r;
                x = 0.5f*r;
                y = (m.a.y + m.b.x)*inv;
                z = (m.c.x + m.a.z)*inv;
                w = (m.b.z - m.c.y)*inv;
            }
            else if(m.b.y > m.c.z)
            {
                float r = sqrtf(1 + m.b.y - m.a.x - m.c.z), inv = 0.5f/r;
                x = (m.a.y + m.b.x)*inv;
                y = 0.5f*r;
                z = (m.b.z + m.c.y)*inv;
                w = (m.c.x - m.a.z)*inv;
            }
            else
            {
                float r = sqrtf(1 + m.c.z - m.a.x - m.b.y), inv = 0.5f/r;
                x = (m.c.x + m.a.z)*inv;
                y = (m.b.z + m.c.y)*inv;
                z = 0.5f*r;
                w = (m.a.y - m.b.x)*inv;
            }
        }
};

/**
 * @brief dual quaternion object
 *
 * 8 dimensional numbers that are the product of dual numbers and quaternions
 * used for rigid body transformations (like animations) (dualquats have 6 DoF)
 */
struct dualquat
{
    quat real, dual;

    dualquat() {}
    dualquat(const quat &q, const vec &p)
        : real(q),
          dual(0.5f*( p.x*q.w + p.y*q.z - p.z*q.y),
               0.5f*(-p.x*q.z + p.y*q.w + p.z*q.x),
               0.5f*( p.x*q.y - p.y*q.x + p.z*q.w),
              -0.5f*( p.x*q.x + p.y*q.y + p.z*q.z))
    {
    }
    explicit dualquat(const quat &q) : real(q), dual(0, 0, 0, 0) {}
    explicit dualquat(const matrix4x3 &m);

    dualquat &mul(float k) { real.mul(k); dual.mul(k); return *this; }

    dualquat &invert()
    {
        real.invert();
        dual.invert();
        dual.msub(real, 2*real.dot(dual));
        return *this;
    }

    void mul(const dualquat &p, const dualquat &o)
    {
        real.mul(p.real, o.real);
        dual.mul(p.real, o.dual).add(quat().mul(p.dual, o.real));
    }
    void mul(const dualquat &o) { mul(dualquat(*this), o); }

    void mulorient(const quat &q)
    {
        real.mul(q, quat(real));
        dual.mul(quat(q).invert(), quat(dual));
    }

    void mulorient(const quat &q, const dualquat &base)
    {
        quat trans;
        trans.mul(base.dual, quat(base.real).invert());
        dual.mul(quat(q).invert(), quat().mul(real, trans).add(dual));

        real.mul(q, quat(real));
        dual.add(quat().mul(real, trans.invert())).msub(real, 2*base.real.dot(base.dual));
    }

    void normalize()
    {
        float invlen = 1/real.magnitude();
        real.mul(invlen);
        dual.mul(invlen);
    }

    void translate(const vec &p)
    {
        dual.x +=  0.5f*( p.x*real.w + p.y*real.z - p.z*real.y);
        dual.y +=  0.5f*(-p.x*real.z + p.y*real.w + p.z*real.x);
        dual.z +=  0.5f*( p.x*real.y - p.y*real.x + p.z*real.w);
        dual.w += -0.5f*( p.x*real.x + p.y*real.y + p.z*real.z);
    }

    void fixantipodal(const dualquat &d)
    {
        if(real.dot(d.real) < 0)
        {
            real.neg();
            dual.neg();
        }
    }

    void accumulate(const dualquat &d, float k)
    {
        if(real.dot(d.real) < 0)
        {
            k = -k;
        }
        real.madd(d.real, k);
        dual.madd(d.dual, k);
    }

    vec transform(const vec &v) const
    {
        return vec().cross(real, vec().cross(real, v).madd(v, real.w).add(vec(dual))).madd(vec(dual), real.w).msub(vec(real), dual.w).mul(2).add(v);
    }

    quat transform(const quat &q) const
    {
        return quat().mul(real, q);
    }

    vec transformnormal(const vec &v) const
    {
        return real.rotate(v);
    }

};

inline dualquat::dualquat(const matrix4x3 &m) : real(m)
{
    dual.x =  0.5f*( m.d.x*real.w + m.d.y*real.z - m.d.z*real.y);
    dual.y =  0.5f*(-m.d.x*real.z + m.d.y*real.w + m.d.z*real.x);
    dual.z =  0.5f*( m.d.x*real.y - m.d.y*real.x + m.d.z*real.w);
    dual.w = -0.5f*( m.d.x*real.x + m.d.y*real.y + m.d.z*real.z);
}

struct plane : vec
{
    float offset;

    float dist(const vec &p) const { return dot(p)+offset; }
    float dist(const vec4<float> &p) const { return p.dot3(*this) + p.w*offset; }
    bool operator==(const plane &p) const { return x==p.x && y==p.y && z==p.z && offset==p.offset; }
    bool operator!=(const plane &p) const { return x!=p.x || y!=p.y || z!=p.z || offset!=p.offset; }

    plane() : vec(0,0,0), offset(0)
    {
    }

    plane(const vec &c, float off) : vec(c), offset(off)
    {
        if(x == 0 && y == 0 && z == 0)
        {
            throw std::invalid_argument("cannot create plane with no normal vector");
        }
    }
    plane(const vec4<float> &p) : vec(p), offset(p.w)
    {
        if(x == 0 && y == 0 && z == 0)
        {
            throw std::invalid_argument("cannot create plane with no normal vector");
        }
    }
    plane(int d, float off)
    {
        if(d < 0 || d > 2)
        {
            throw std::invalid_argument("cannot specify plane index outside 0..2");
        }
        x = y = z = 0.0f;
        v[d] = 1.0f;
        offset = -off;
    }
    plane(float a, float b, float c, float d) : vec(a, b, c), offset(d)
    {
        if(x == 0 && y == 0 && z == 0)
        {
            throw std::invalid_argument("cannot create plane with no normal vector");
        }
    }

    void toplane(const vec &n, const vec &p)
    {
        x = n.x;
        y = n.y;
        z = n.z;
        offset = -dot(p);
    }

    bool toplane(const vec &a, const vec &b, const vec &c)
    {
        cross(vec(b).sub(a), vec(c).sub(a));
        float mag = magnitude();
        if(!mag)
        {
            return false;
        }
        div(mag);
        offset = -dot(a);
        return true;
    }

    bool rayintersect(const vec &o, const vec &ray, float &dist) const
    {
        float cosalpha = dot(ray);
        if(cosalpha==0)
        {
            return false;
        }
        float deltac = offset+dot(o);
        dist -= deltac/cosalpha;
        return true;
    }

    plane &reflectz(float rz)
    {
        offset += 2*rz*z;
        z = -z;
        return *this;
    }

    plane &invert()
    {
        neg();
        offset = -offset;
        return *this;
    }

    plane &scale(float k)
    {
        mul(k);
        return *this;
    }

    plane &translate(const vec &p)
    {
        offset += dot(p);
        return *this;
    }

    plane &normalize()
    {
        float mag = magnitude();
        div(mag);
        offset /= mag;
        return *this;
    }

    float zdelta(const vec &p) const
    {
        return -(x*p.x+y*p.y)/z;
    }

};

//short integer quaternion
class squat
{
    public:
        short x, y, z, w;

        squat() {}
        //all dimensions of `q` should be <= 1 (normalized)
        squat(const vec4<float> &q)
        {
            convert(q);
        }

        void lerp(const vec4<float> &a, const vec4<float> &b, float t)
        {
            vec4<float> q;
            q.lerp(a, b, t);
            convert(q);
        }
    private:
        void convert(const vec4<float> &q)
        {
            x = static_cast<short>(q.x*32767.5f-0.5f);
            y = static_cast<short>(q.y*32767.5f-0.5f);
            z = static_cast<short>(q.z*32767.5f-0.5f);
            w = static_cast<short>(q.w*32767.5f-0.5f);
        }
};

struct matrix2
{
    vec2 a, b;

    matrix2() {}
    matrix2(const vec2 &a, const vec2 &b) : a(a), b(b) {}
    explicit matrix2(const matrix4 &m) : a(m.a), b(m.b) {}
    explicit matrix2(const matrix3 &m) : a(m.a), b(m.b) {}
};

#endif

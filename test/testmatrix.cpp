
#include "libprimis.h"
#include "../src/shared/geomexts.h"

constexpr float tolerance = 0.001;

////////////////////////////////////////////////////////////////////////////////
// matrix 3x3 tests
////////////////////////////////////////////////////////////////////////////////

void test_matrix3_ctor()
{
    std::printf("testing matrix3 constructor\n");
    //matrix3()
    {
        matrix3 m;
        assert(m.a == vec(0,0,0));
        assert(m.b == vec(0,0,0));
        assert(m.c == vec(0,0,0));
    }
    //matrix3(vec,vec,vec)
    {
        vec a(1,2,3),
            b(4,5,6),
            c(7,8,9);
        matrix3 m(a,b,c);
        assert(m.a == a);
        assert(m.b == b);
        assert(m.c == c);
    }
    //matrix3(quat)
    {
        //test use of normalized quat -> normalized matrix
        quat q(0.5,0.5,0.5,0.5);
        matrix3 m(q);
        assert(m.a.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,0,1)).magnitude() < tolerance);
        assert(m.c.sub(vec(1,0,0)).magnitude() < tolerance);
    }
    {
        //test identity quat -> identity matrix
        quat q(0,0,0,1);
        matrix3 m(q);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
    //matrix3(float,vec)
    {
        //test identity generation
        vec axis(1,0,0);
        float angle = 0;
        matrix3 m(angle, axis);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
    {
        //test 360 degree rotation about x
        vec axis(1,0,0);
        float angle = 2*M_PI;
        matrix3 m(angle, axis);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
    {
        //test 180 degree rotation about x
        vec axis(1,0,0);
        float angle = M_PI;
        matrix3 m(angle, axis);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,-1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,-1)).magnitude() < tolerance);
    }
    {
        //test 180 degree rotation about y
        vec axis(0,1,0);
        float angle = M_PI;
        matrix3 m(angle, axis);
        assert(m.a.sub(vec(-1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,-1)).magnitude() < tolerance);
    }
    //matrix3(matrix4x3)
    {
        matrix4x3 m1(vec(1,2,3), vec(4,5,6), vec(7,8,9), vec(10,11,12));
        matrix3 m2(m1);
        assert(m2.a == vec(1,2,3));
        assert(m2.b == vec(4,5,6));
        assert(m2.c == vec(7,8,9));
    }
    //matrix3(matrix4)
    {
        matrix4 m1(vec4<float>(1,2,3,4),
                   vec4<float>(5,6,7,8),
                   vec4<float>(9,10,11,12),
                   vec4<float>(13,14,15,16));
        matrix3 m2(m1);
        assert(m2.a == vec(1,2,3));
        assert(m2.b == vec(5,6,7));
        assert(m2.c == vec(9,10,11));
    }
}

void test_matrix3_normalize()
{
    std::printf("testing matrix3 normalize\n");
    {
        //test identity -> identity
        matrix3 m;
        m.identity();
        m.normalize();
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
    {
        //test normal normalization case
        matrix3 m;
        m.a = vec(3,4,0);
        m.b = vec(0,4,3);
        m.c = vec(4,0,3);
        m.normalize();
        assert(m.a.sub(vec(0.6,0.8,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,0.8,0.6)).magnitude() < tolerance);
        assert(m.c.sub(vec(0.8,0,0.6)).magnitude() < tolerance);
    }
}

void test_matrix3_scale()
{
    std::printf("testing matrix3 scale\n");

    matrix3 m;
    m.identity();
    m.scale(2);
    assert(m.a.sub(vec(2,0,0)).magnitude() < tolerance);
    assert(m.b.sub(vec(0,2,0)).magnitude() < tolerance);
    assert(m.c.sub(vec(0,0,2)).magnitude() < tolerance);
}

void test_matrix3_setyaw()
{
    std::printf("testing matrix3 setyaw\n");
    {
        //90 degrees
        matrix3 m;
        m.setyaw(M_PI/2);
        assert(m.a.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(-1,0,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
    {
        //180 degrees
        matrix3 m;
        m.setyaw(M_PI);
        assert(m.a.sub(vec(-1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,-1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
}

void test_matrix3_trace()
{
    std::printf("testing matrix3 trace\n");
    {
        matrix3 m;
        assert(m.trace() == 0);
    }
    {
        matrix3 m;
        m.identity();
        assert(m.trace() == 3);
    }
    {
        vec a(1,2,3),
            b(4,5,6),
            c(7,8,9);
        matrix3 m(a,b,c);
        assert(m.trace() == 15);
    }
}

void test_matrix3_identity()
{
    std::printf("testing matrix3 identity\n");
    matrix3 m;
    m.identity();
    assert(m.a == vec(1,0,0));
    assert(m.b == vec(0,1,0));
    assert(m.c == vec(0,0,1));
}

void test_matrix3_transpose()
{
    std::printf("testing matrix3 transpose\n");
    matrix3 m({1,2,3}, {4,5,6}, {7,8,9});
    m.transpose();
    assert(m.a == vec(1,4,7));
    assert(m.b == vec(2,5,8));
    assert(m.c == vec(3,6,9));
}

void test_matrix3_invert()
{
    std::printf("testing matrix3 invert\n");
    //invert()
    {
        matrix3 m;
        m.identity();
        m.invert();
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
    {
        matrix3 m({1,1,1}, {2,2,2}, {3,3,3});
        m.invert();
        vec inv(1.f/3, 1.f/6, 1.f/9);
        assert(m.a.sub(inv).magnitude() < tolerance);
        assert(m.b.sub(inv).magnitude() < tolerance);
        assert(m.c.sub(inv).magnitude() < tolerance);
    }
    //invert(matrix3&)
    {
        matrix3 m,
                m2;
        m.identity();
        m2.identity();
        m.invert(m2);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
    {
        matrix3 m({1,2,3}, {4,5,6}, {7,8,9}),
                m2;
        m2.identity();
        m.invert(m2);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
    }
}

void test_matrix3_row()
{
    std::printf("testing matrix3 rowx/rowy/rowz\n");

    matrix3 m;
    m.identity();
    assert(m.rowx() == vec(1,0,0));
    assert(m.rowy() == vec(0,1,0));
    assert(m.rowz() == vec(0,0,1));
}

////////////////////////////////////////////////////////////////////////////////
// matrix 4x3 tests
////////////////////////////////////////////////////////////////////////////////

void test_matrix4x3_ctor()
{
    std::printf("testing matrix4x3 ctor\n");

    //matrix4x3()
    {
        matrix4x3 m;
        assert(m.a == vec(0,0,0));
        assert(m.b == vec(0,0,0));
        assert(m.c == vec(0,0,0));
        assert(m.d == vec(0,0,0));
    }
    //matrix4x3(vec,vec,vec,vec)
    {
        matrix4x3 m({1,0,0}, {0,1,0}, {0,0,1}, {0,0,0});
        assert(m.a == vec(1,0,0));
        assert(m.b == vec(0,1,0));
        assert(m.c == vec(0,0,1));
        assert(m.d == vec(0,0,0));
    }
    //matrix4x3(matrix3, vec)
    {
        matrix3 m0;
        m0.identity();
        matrix4x3 m(m0, {1,1,1});
        assert(m.a == vec(1,0,0));
        assert(m.b == vec(0,1,0));
        assert(m.c == vec(0,0,1));
        assert(m.d == vec(1,1,1));
    }
    //matrix4x3(dualquat)
    {
        //test trivial case: identity dualquat
        quat a(0,0,0,1),
             b(0,0,0,0);
        dualquat dq(a);
        dq.dual = b;
        matrix4x3 m(dq);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
        assert(m.d.sub(vec(0,0,0)).magnitude() < tolerance);
    }
    {
        //test rotation and translation
        quat a(0.5,0.5,0.5,0.5),
             b(0,0,0,1);
        dualquat dq(a);
        dq.dual = b;
        matrix4x3 m(dq);
        assert(m.a.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,0,1)).magnitude() < tolerance);
        assert(m.c.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.d.sub(vec(-1,-1,-1)).magnitude() < tolerance);
    }
}

void test_matrix4x3_accumulate()
{
    std::printf("testing matrix4x3 accumulate\n");
    matrix4x3 m;
    m.identity();
    m.accumulate(m, 2);
    assert(m.a == vec(3,0,0));
    assert(m.b == vec(0,3,0));
    assert(m.c == vec(0,0,3));
    assert(m.d == vec(0,0,0));
}

void test_matrix4x3_mul()
{
    std::printf("testing matrix4x3 mul\n");
    {
        matrix4x3 m;
        m.identity();
        m.mul(2);
        assert(m.a == vec(2,0,0));
        assert(m.b == vec(0,2,0));
        assert(m.c == vec(0,0,2));
        assert(m.d == vec(0,0,0));
    }
}

void test_matrix4x3_setscale()
{
    std::printf("testing matrix4x3 setscale\n");
    matrix4x3 output(vec(1,0,0), vec(0,2,0), vec(0,0,3), vec(0,0,0)),
              output2(vec(2,0,0), vec(0,2,0), vec(0,0,2), vec(0,0,0));
    //setscale(float, float, float)
    {
        matrix4x3 m;
        m.setscale(1,2,3);
        assert(m.a == output.a);
        assert(m.b == output.b);
        assert(m.c == output.c);
    }
    //setscale(vec)
    {
        matrix4x3 m;
        m.setscale(vec(1,2,3));
        assert(m.a == output.a);
        assert(m.b == output.b);
        assert(m.c == output.c);
    }
    //setscale(float)
    {
        matrix4x3 m;
        m.setscale(2);
        assert(m.a == output2.a);
        assert(m.b == output2.b);
        assert(m.c == output2.c);
    }
}

void test_matrix4x3_scale()
{
    std::printf("testing matrix4x3 scale\n");
    matrix4x3 output(vec(1,0,0), vec(0,2,0), vec(0,0,3), vec(0,0,0)),
              output2(vec(2,0,0), vec(0,2,0), vec(0,0,2), vec(0,0,0));
    //setscale(float, float, float)
    {
        matrix4x3 m;
        m.identity();
        m.scale(1,2,3);
        assert(m.a == output.a);
        assert(m.b == output.b);
        assert(m.c == output.c);
    }
    //setscale(vec)
    {
        matrix4x3 m;
        m.identity();
        m.scale(vec(1,2,3));
        assert(m.a == output.a);
        assert(m.b == output.b);
        assert(m.c == output.c);
    }
    //setscale(float)
    {
        matrix4x3 m;
        m.identity();
        m.scale(2);
        assert(m.a == output2.a);
        assert(m.b == output2.b);
        assert(m.c == output2.c);
    }
}

void test_matrix4x3_settranslation()
{
    std::printf("testing matrix4x3 settranslation\n");
    //settranslation(vec)
    {
        matrix4x3 m;
        m.identity();
        m.settranslation(vec(1,2,3));
        assert(m.d == vec(1,2,3));
    }
    //settranslation(float,float,float)
    {
        matrix4x3 m;
        m.identity();
        m.settranslation(1,2,3);
        assert(m.d == vec(1,2,3));
    }
}

void test_matrix4x3_normalize()
{
    std::printf("testing matrix4x3 normalize\n");
    {
        matrix4x3 m;
        m.identity();
        m.normalize();
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
        assert(m.d.magnitude() < tolerance);
    }
    {
        matrix4x3 m({3,4,0}, {3,0,4}, {4,3,0}, {4,0,3});
        m.normalize();
        assert(m.a.sub(vec( 0.6, 0.8,   0)).magnitude() < tolerance);
        assert(m.b.sub(vec( 0.6,   0, 0.8)).magnitude() < tolerance);
        assert(m.c.sub(vec( 0.8, 0.6,   0)).magnitude() < tolerance);
        assert(m.d.sub(vec(   4,   0,   3)).magnitude() < tolerance);
    }
}

void test_matrix4x3_lerp()
{
    std::printf("testing matrix4x3 lerp\n");
    //lerp(matrix4x3,float)
    {
        matrix4x3 m1,
                  m2({1,2,3}, {4,5,6}, {7,8,9}, {10,11,12});
        m1.lerp(m2, 0.5);
        assert(m1.a.sub(vec( 0.5, 1.0, 1.5)).magnitude() < tolerance);
        assert(m1.b.sub(vec( 2.0, 2.5, 3.0)).magnitude() < tolerance);
        assert(m1.c.sub(vec( 3.5, 4.0, 4.5)).magnitude() < tolerance);
        assert(m1.d.sub(vec( 5.0, 5.5, 6.0)).magnitude() < tolerance);
    }
    //lerp(matrix4x3,matrix4x3,float)
    {
        matrix4x3 m1,
                  m2({1,2,3}, {4,5,6}, {7,8,9}, {10,11,12}),
                  m3;
        m3.lerp(m1, m2, 0.5);
        assert(m3.a.sub(vec( 0.5, 1.0, 1.5)).magnitude() < tolerance);
        assert(m3.b.sub(vec( 2.0, 2.5, 3.0)).magnitude() < tolerance);
        assert(m3.c.sub(vec( 3.5, 4.0, 4.5)).magnitude() < tolerance);
        assert(m3.d.sub(vec( 5.0, 5.5, 6.0)).magnitude() < tolerance);
    }

}

void test_matrix4x3_identity()
{
    std::printf("testing matrix4x3 identity\n");
    matrix4x3 m;
    m.identity();
    assert(m.a == vec(1,0,0));
    assert(m.b == vec(0,1,0));
    assert(m.c == vec(0,0,1));
    assert(m.d == vec(0,0,0));
}

void test_matrix4x3_transpose()
{
    std::printf("testing matrix4x3 transpose\n");
    {
        matrix4x3 m;
        m.identity();
        m.transpose();
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
        assert(m.d.magnitude() < tolerance);
    }
    {
        matrix4x3 m({1,2,3}, {4,5,6}, {7,8,9}, {10,11,12});
        m.transpose();
        assert(m.a.sub(vec(1,4,7)).magnitude() < tolerance);
        assert(m.b.sub(vec(2,5,8)).magnitude() < tolerance);
        assert(m.c.sub(vec(3,6,9)).magnitude() < tolerance);
        //                 a*d  b*d  c*d
        assert(m.d.sub(vec(-68,-167,-266)).magnitude() < tolerance);
    }
}

void test_matrix4x3_invert()
{
    std::printf("testing matrix4x3 invert\n");
    //invert()
    {
        matrix4x3 m;
        m.identity();
        m.invert();
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
        assert(m.d.magnitude() < tolerance);
    }
    {
        matrix4x3 m({1,1,1}, {2,2,2}, {3,3,3}, {0,0,0});
        m.invert();
        vec inv(1.f/3, 1.f/6, 1.f/9);
        assert(m.a.sub(inv).magnitude() < tolerance);
        assert(m.b.sub(inv).magnitude() < tolerance);
        assert(m.c.sub(inv).magnitude() < tolerance);
        assert(m.d.magnitude() < tolerance);
    }
    {
        //show that normalize-invert operations do not commute
        matrix4x3 m({3,4,0}, {3,0,4}, {4,3,0}, {4,0,3}),
                  m2(m);
        //normalize-invert
        m.normalize();
        m.invert();

        assert(m.a.sub(vec( 0.6, 0.6, 0.8)).magnitude() < tolerance);
        assert(m.b.sub(vec( 0.8,   0, 0.6)).magnitude() < tolerance);
        assert(m.c.sub(vec(   0, 0.8,   0)).magnitude() < tolerance);
        assert(m.d.sub(vec(-2.4,-4.8,-3.2)).magnitude() < tolerance);
        //invert-normalize
        m2.invert();
        m2.normalize();

        float a = 0.514496,
              b = 0.685994;
        assert(m2.a.sub(vec(    a,    a,    b)).magnitude() < tolerance);
        assert(m2.b.sub(vec(  0.8,    0,  0.6)).magnitude() < tolerance);
        assert(m2.c.sub(vec(    0,    1,    0)).magnitude() < tolerance);
        assert(m2.d.sub(vec(-0.48,-0.96,-0.64)).magnitude() < tolerance);
    }
    //invert(matrix4x3&)
    {
        matrix4x3 m,
                  m2;
        m.identity();
        m2.identity();
        m.invert(m2);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
        assert(m.d.magnitude() < tolerance);
    }
    {
        //test that m2 is inverted and not m
        matrix4x3 m({1,2,3}, {4,5,6}, {7,8,9}, {10,11,12}),
                  m2;
        m2.identity();
        m.invert(m2);
        assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
        assert(m.d.magnitude() < tolerance);
    }
}

void test_matrix4x3_transposedtransformnormal()
{
    std::printf("testing matrix4x3 transposedtransformnormal\n");

    {
        matrix4x3 m;
        m.identity();
        assert(m.transposedtransformnormal(vec(1,1,1)) == vec(1,1,1));
    }
    {
        matrix4x3 m;
        m.identity();
        assert(m.transposedtransformnormal(vec(1,0,0)) == vec(1,0,0));
    }
    {
        matrix4x3 m;
        m.identity();
        m.mul(2);
        assert(m.transposedtransformnormal(vec(2,0,0)) == vec(4,0,0));
    }
}

void test_matrix4x3_transformnormal()
{
    std::printf("testing matrix4x3 transformnormal\n");

    {
        matrix4x3 m;
        m.identity();
        assert(m.transformnormal(vec(1,1,1)) == vec(1,1,1));
    }
    {
        matrix4x3 m;
        m.identity();
        assert(m.transformnormal(vec(1,0,0)) == vec(1,0,0));
    }
    {
        matrix4x3 m;
        m.identity();
        m.mul(2);
        assert(m.transformnormal(vec(2,0,0)) == vec(4,0,0));
    }
}

void test_matrix4x3_transform()
{
    std::printf("testing matrix4x3 transform\n");

    //transform(vec)
    {
        matrix4x3 m;
        m.identity();
        vec v = m.transform(vec(1,1,1));
        assert(v.sub(vec(1,1,1)).magnitude() < tolerance);
    }
    {
        matrix4x3 m(vec(1,2,3), vec(4,5,6), vec(7,8,9), vec(1,2,3));
        vec v = m.transform(vec(1,1,1));
        assert(v.sub(vec(13,17,21)).magnitude() < tolerance);
    }
    {
        matrix4x3 m(vec(1,0,0), vec(0,5,0), vec(0,0,9), vec(1,2,3));
        vec v = m.transform(vec(1,1,1));
        assert(v.sub(vec(2,7,12)).magnitude() < tolerance);
    }
    //transform(vec2)
    {
        matrix4x3 m;
        m.identity();
        vec v = m.transform(vec2(1,1));
        m.transform(v);
        assert(v.sub(vec(1,1,0)).magnitude() < tolerance);
    }
    {
        matrix4x3 m(vec(1,2,3), vec(4,5,6), vec(7,8,9), vec(1,2,3));
        vec v = m.transform(vec2(1,1));
        assert(v.sub(vec(6,9,12)).magnitude() < tolerance);
    }
    {
        matrix4x3 m(vec(1,0,0), vec(0,5,0), vec(0,0,9), vec(1,2,3));
        vec v = m.transform(vec2(1,1));
        assert(v.sub(vec(2,7,3)).magnitude() < tolerance);
    }
}

void test_matrix4x3_row()
{
    std::printf("testing matrix4x3 rowx/rowy/rowz\n");

    matrix4x3 m;
    m.identity();
    assert(m.rowx() == vec4<float>(1,0,0,0));
    assert(m.rowy() == vec4<float>(0,1,0,0));
    assert(m.rowz() == vec4<float>(0,0,1,0));
}

////////////////////////////////////////////////////////////////////////////////
// matrix 4x4 tests
////////////////////////////////////////////////////////////////////////////////

void test_matrix4_ctor()
{
    std::printf("testing matrix4 ctor\n");

    //matrix4(float*)
    {
        matrix4 m;
        assert(m.a == vec4<float>(0,0,0,0));
        assert(m.b == vec4<float>(0,0,0,0));
        assert(m.c == vec4<float>(0,0,0,0));
        assert(m.d == vec4<float>(0,0,0,0));
    }
    //matrix4(float*)
    {
        std::array<float, 16> f{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
        matrix4 m(f.data());
        assert(m.a == vec4<float>(1,2,3,4));
        assert(m.b == vec4<float>(5,6,7,8));
        assert(m.c == vec4<float>(9,10,11,12));
        assert(m.d == vec4<float>(13,14,15,16));
    }
    //matrix4(vec,vec,vec)
    {
        matrix4 m(vec(1,0,0), vec(0,1,0), vec(0,0,1));
        assert(m.a == vec4<float>(1,0,0,0));
        assert(m.b == vec4<float>(0,1,0,0));
        assert(m.c == vec4<float>(0,0,1,0));
        assert(m.d == vec4<float>(0,0,0,1));
    }
    //matrix4(vec4<float>,vec4<float>,vec4<float>,vec4<float>)
    {
        matrix4 m({1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1});
        assert(m.a == vec4<float>(1,0,0,0));
        assert(m.b == vec4<float>(0,1,0,0));
        assert(m.c == vec4<float>(0,0,1,0));
        assert(m.d == vec4<float>(0,0,0,1));
    }
    //matrix4(matrix3, vec)
    {
        matrix3 m0;
        m0.identity();
        matrix4 m(m0, {1,1,1});
        assert(m.a == vec4<float>(1,0,0,0));
        assert(m.b == vec4<float>(0,1,0,0));
        assert(m.c == vec4<float>(0,0,1,0));
        assert(m.d == vec4<float>(1,1,1,1));
    }
    //matrix4(matrix4x3)
    {
        matrix4x3 m0({1,0,0}, {0,1,0}, {0,0,1}, {0,0,0});
        matrix4 m(m0);
        assert(m.a == vec4<float>(1,0,0,0));
        assert(m.b == vec4<float>(0,1,0,0));
        assert(m.c == vec4<float>(0,0,1,0));
        assert(m.d == vec4<float>(0,0,0,1));
    }
}

void test_matrix4_identity()
{
    std::printf("testing matrix4 identity\n");
    matrix4 m;
    m.identity();
    assert(m.a == vec4<float>(1,0,0,0));
    assert(m.b == vec4<float>(0,1,0,0));
    assert(m.c == vec4<float>(0,0,1,0));
    assert(m.d == vec4<float>(0,0,0,1));
}

void test_matrix4_settranslation()
{
    std::printf("testing matrix4 settranslation\n");
    //settranslation(vec)
    {
        matrix4 m;
        m.settranslation(vec(1,2,3));
        assert(m.d == vec4<float>(1,2,3,0));
    }
    //settranslation(float, float, float)
    {
        matrix4 m;
        m.settranslation(1,2,3);
        assert(m.d == vec4<float>(1,2,3,0));
    }
}

void test_matrix4_setscale()
{
    //setscale(float)
    std::printf("testing matrix4 setscale methods\n");
    {
        matrix4 m;
        m.setscale(2);
        assert(m.a == vec4<float>(2,0,0,0));
        assert(m.b == vec4<float>(0,2,0,0));
        assert(m.c == vec4<float>(0,0,2,0));
        assert(m.d == vec4<float>(0,0,0,0));
    }
    //setscale(float,float,float)
    {
        matrix4 m;
        m.setscale(2,3,4);
        assert(m.a == vec4<float>(2,0,0,0));
        assert(m.b == vec4<float>(0,3,0,0));
        assert(m.c == vec4<float>(0,0,4,0));
        assert(m.d == vec4<float>(0,0,0,0));
    }
    //setscale(vec)
    {
        matrix4 m;
        m.setscale(vec(2,3,4));
        assert(m.a == vec4<float>(2,0,0,0));
        assert(m.b == vec4<float>(0,3,0,0));
        assert(m.c == vec4<float>(0,0,4,0));
        assert(m.d == vec4<float>(0,0,0,0));
    }
}

void test_matrix4_scale()
{
    //setscale(float)
    std::printf("testing matrix4 scale methods\n");
    {
        matrix4 m;
        m.identity();
        m.scale(2);
        assert(m.a == vec4<float>(2,0,0,0));
        assert(m.b == vec4<float>(0,2,0,0));
        assert(m.c == vec4<float>(0,0,2,0));
        assert(m.d == vec4<float>(0,0,0,1));
    }
    //setscale(float,float,float)
    {
        matrix4 m;
        m.identity();
        m.scale(2,3,4);
        assert(m.a == vec4<float>(2,0,0,0));
        assert(m.b == vec4<float>(0,3,0,0));
        assert(m.c == vec4<float>(0,0,4,0));
        assert(m.d == vec4<float>(0,0,0,1));
    }
    //setscale(vec)
    {
        matrix4 m;
        m.identity();
        m.scale(vec(2,3,4));
        assert(m.a == vec4<float>(2,0,0,0));
        assert(m.b == vec4<float>(0,3,0,0));
        assert(m.c == vec4<float>(0,0,4,0));
        assert(m.d == vec4<float>(0,0,0,1));
    }
}

void test_matrix4_scalez()
{
    std::printf("testing matrix4 scalez\n");

    {
        matrix4 m;
        m.identity();
        m.scalez(2);
        assert(m.c.z == 2);
    }
    {
        matrix4 m({1,0,1,0}, {0,1,2,0}, {0,0,3,0}, {0,0,4,1});
        m.scalez(2);
        assert(m.a.z == 2);
        assert(m.b.z == 4);
        assert(m.c.z == 6);
        assert(m.d.z == 8);
    }


}
void test_matrix4_jitter()
{
    std::printf("testing matrix4 jitter\n");
    {
        //test jitter with nonzero w values
        matrix4 m({1,1,1,1},{1,1,1,2}, {1,1,1,3}, {1,1,1,4});
        m.jitter(1,2);
        assert(m.a == vec4<float>(2,3,1,1));
        assert(m.b == vec4<float>(3,5,1,2));
        assert(m.c == vec4<float>(4,7,1,3));
        assert(m.d == vec4<float>(5,9,1,4));
    }
    {
        //test jitter with zero w values
        matrix4 m({1,1,1,0},{1,1,1,0}, {1,1,1,0}, {1,1,1,0});
        m.jitter(1,2);
        assert(m.a == vec4<float>(1,1,1,0));
        assert(m.b == vec4<float>(1,1,1,0));
        assert(m.c == vec4<float>(1,1,1,0));
        assert(m.d == vec4<float>(1,1,1,0));
    }
}

void test_matrix4_transpose()
{
    std::printf("testing matrix4 transpose\n");
    //transpose()
    {
        matrix4 m({1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16});
        m.transpose();
        assert(m.a == vec4<float>(1,5,9,13));
        assert(m.b == vec4<float>(2,6,10,14));
        assert(m.c == vec4<float>(3,7,11,15));
        assert(m.d == vec4<float>(4,8,12,16));
    }
    //transpose(matrix4)
    {
        matrix4 m({1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16}),
                m2;
        m2.transpose(m);
        assert(m2.a == vec4<float>(1,5,9,13));
        assert(m2.b == vec4<float>(2,6,10,14));
        assert(m2.c == vec4<float>(3,7,11,15));
        assert(m2.d == vec4<float>(4,8,12,16));
    }
}

void test_matrix4_ortho()
{
    std::printf("testing matrix4 ortho\n");
    {
        matrix4 m;
        m.ortho(0,1,0,1,1,0);
        assert(m.a == vec4<float>(2,0,0,0));
        assert(m.b == vec4<float>(0,2,0,0));
        assert(m.c == vec4<float>(0,0,2,0));
        assert(m.d == vec4<float>(-1,-1,1,1));
    }
    {
        matrix4 m;
        m.ortho(1,2,1,2,2,1);
        assert(m.a == vec4<float>(2,0,0,0));
        assert(m.b == vec4<float>(0,2,0,0));
        assert(m.c == vec4<float>(0,0,2,0));
        assert(m.d == vec4<float>(-3,-3,3,1));
    }
}

void test_matrix4_transformnormal()
{
    std::printf("testing matrix4 transformnormal\n");

    //transformnormal(vec,vec)
    {
        //trivial identity test
        matrix4 m;
        m.identity();
        vec v1(1,1,1),
            v2;
        m.transformnormal(v1,v2);
        assert(v2 == vec(1,1,1));
    }
    {
        //test scaling input vec
        matrix4 m;
        m.identity();
        vec v1(2,2,2),
            v2;
        m.transformnormal(v1,v2);
        assert(v2 == vec(2,2,2));
    }
    {
        //test non identity matrix
        matrix4 m({1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16});
        vec v1(1,1,1),
            v2;
        m.transformnormal(v1,v2);
        assert(v2 == vec(15,18,21));
    }
    {
        //test that w elements are irrelevant
        matrix4 m({1,2,3,0}, {5,6,7,0}, {9,10,11,0}, {13,14,15,0});
        vec v1(1,1,1),
            v2;
        m.transformnormal(v1,v2);
        assert(v2 == vec(15,18,21));
    }
    //transformnormal(vec,vec4<float>)
    {
        //trivial identity test
        matrix4 m;
        m.identity();
        vec v1(1,1,1);
        vec4<float> v2;
        m.transformnormal(v1,v2);
        assert(v2 == vec4<float>(1,1,1,0));
    }
    {
        //test scaling input vec
        matrix4 m;
        m.identity();
        vec v1(2,2,2);
        vec4<float> v2;
        m.transformnormal(v1,v2);
        assert(v2 == vec4<float>(2,2,2,0));
    }
    {
        //test non identity matrix
        matrix4 m({1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16});
        vec v1(1,1,1);
        vec4<float> v2;
        m.transformnormal(v1,v2);
        assert(v2 == vec4<float>(15,18,21,24));
    }
}

void test_matrix4_transposedtransform()
{
    std::printf("testing matrix4 transposedtransform\n");

    {
        matrix4 m;
        m.identity();
        vec v1(1,1,1),
            v2;
        m.transposedtransform(v1, v2);
        assert(v2 == vec(1,1,1));
    }
    {
        matrix4 m;
        m.identity();
        vec v1(2,2,2),
            v2;
        m.transposedtransform(v1, v2);
        assert(v2 == vec(2,2,2));
    }
    {
        matrix4 m({1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {1,1,1,1});
        vec v1(2,2,2),
            v2;
        m.transposedtransform(v1, v2);
        assert(v2 == vec(6,18,30));
    }
}

void test_matrix4_transposedtransformnormal()
{
    std::printf("testing matrix4 transposedtransformnormal\n");

    {
        matrix4 m;
        m.identity();
        vec v1(1,1,1),
            v2;
        m.transposedtransformnormal(v1, v2);
        assert(v2 == vec(1,1,1));
    }
    {
        matrix4 m;
        m.identity();
        vec v1(2,2,2),
            v2;
        m.transposedtransformnormal(v1, v2);
        assert(v2 == vec(2,2,2));
    }
    {
        matrix4 m({1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16});
        vec v1(1,1,1),
            v2;
        m.transposedtransformnormal(v1, v2);
        assert(v2 == vec(6,18,30));
    }
}

void test_matrix4_gettranslation()
{
    std::printf("testing matrix4 gettranslation\n");
    matrix4 m({1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16});
    assert(m.gettranslation() == vec(13,14,15));
}

void test_matrix4_row()
{
    std::printf("testing matrix4 rowx/rowy/rowz/roww\n");

    matrix4 m({1,2,0,0},
              {3,1,2,0},
              {0,3,1,2},
              {0,0,3,1});
    assert(m.rowx() == vec4<float>(1,3,0,0));
    assert(m.rowy() == vec4<float>(2,1,3,0));
    assert(m.rowz() == vec4<float>(0,2,1,3));
    assert(m.roww() == vec4<float>(0,0,2,1));
}

void test_matrix4_invert()
{
    std::printf("testing matrix4 invert\n");
    {
        //test I^-1 == I
        matrix4 m;
        m.identity();
        bool notsingular = m.invert(m);
        assert(m.a.sub(vec4<float>(1,0,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec4<float>(0,1,0,0)).magnitude() < tolerance);
        assert(m.c.sub(vec4<float>(0,0,1,0)).magnitude() < tolerance);
        assert(m.d.sub(vec4<float>(0,0,0,1)).magnitude() < tolerance);
        assert(notsingular == true);
    }
    {
        //test trivially singular matrix
        matrix4 m({1,0,0,0},{1,0,0,0}, {1,0,0,0}, {1,0,0,0});
        bool notsingular = m.invert(m);
        assert(m.a.sub(vec4<float>(1,0,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec4<float>(1,0,0,0)).magnitude() < tolerance);
        assert(m.c.sub(vec4<float>(1,0,0,0)).magnitude() < tolerance);
        assert(m.d.sub(vec4<float>(1,0,0,0)).magnitude() < tolerance);
        assert(notsingular == false);
    }
}

void test_matrix4_inverse()
{
    std::printf("testing matrix4 inverse\n");
    {
        //test I^-1 == I
        matrix4 m;
        m.identity();
        m = m.inverse();
        assert(m.a.sub(vec4<float>(1,0,0,0)).magnitude() < tolerance);
        assert(m.b.sub(vec4<float>(0,1,0,0)).magnitude() < tolerance);
        assert(m.c.sub(vec4<float>(0,0,1,0)).magnitude() < tolerance);
        assert(m.d.sub(vec4<float>(0,0,0,1)).magnitude() < tolerance);
    }
    {
        //test trivially singular matrix
        matrix4 m({1,0,0,0},{1,0,0,0}, {1,0,0,0}, {1,0,0,0});
        m = m.inverse();
        assert(m.rowx() == vec4<float>(0,0,0,0));
        assert(m.rowy() == vec4<float>(0,0,0,0));
        assert(m.rowz() == vec4<float>(0,0,0,0));
        assert(m.roww() == vec4<float>(0,0,0,0));
    }
}

void test_matrix4_lineardepthscale()
{
    std::printf("testing matrix4 lineardepthscale\n");

    {
        matrix4 m;
        m.identity();
        assert(m.lineardepthscale() == vec2(1,0));
    }
    {
        matrix4 m({0,0,0,0},{0,0,0,0}, {0,0,1,0}, {0,0,0,1});
        assert(m.lineardepthscale() == vec2(1,0));
    }
    {
        matrix4 m({0,0,0,0},{0,0,0,0}, {0,0,1,1}, {0,0,1,2});
        assert(m.lineardepthscale() == vec2(2,-1));
    }
}

void test_matrix()
{
    std::printf(
"===============================================================\n\
testing matrices\n\
===============================================================\n"
    );

    test_matrix3_ctor();
    test_matrix3_normalize();
    test_matrix3_scale();
    test_matrix3_setyaw();
    test_matrix3_trace();
    test_matrix3_identity();
    test_matrix3_transpose();
    test_matrix3_invert();
    test_matrix3_row();

    test_matrix4x3_ctor();
    test_matrix4x3_mul();
    test_matrix4x3_setscale();
    test_matrix4x3_scale();
    test_matrix4x3_settranslation();
    test_matrix4x3_accumulate();
    test_matrix4x3_normalize();
    test_matrix4x3_lerp();
    test_matrix4x3_identity();
    test_matrix4x3_transpose();
    test_matrix4x3_invert();
    test_matrix4x3_transposedtransformnormal();
    test_matrix4x3_transformnormal();
    test_matrix4x3_transform();
    test_matrix4x3_row();

    test_matrix4_ctor();
    test_matrix4_identity();
    test_matrix4_settranslation();
    test_matrix4_scale();
    test_matrix4_setscale();
    test_matrix4_scalez();
    test_matrix4_jitter();
    test_matrix4_transpose();
    test_matrix4_ortho();
    test_matrix4_transposedtransformnormal();
    test_matrix4_transposedtransform();
    test_matrix4_transformnormal();
    test_matrix4_gettranslation();
    test_matrix4_row();
    test_matrix4_invert();
    test_matrix4_inverse();
    test_matrix4_lineardepthscale();
}

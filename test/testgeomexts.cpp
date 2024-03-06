
#include "libprimis.h"
#include "../src/shared/geomexts.h"

constexpr float tolerance = 0.001;

////////////////////////////////////////////////////////////////////////////////
// half float tests
////////////////////////////////////////////////////////////////////////////////

void test_half_ctor()
{
    std::printf("testing half float ctor\n");

    {
        half h(0.f);
        assert(h.val == 0);
    }
    {
        half h(1.f);
        assert(h.val == 15360);
    }
    {
        half h(10.f);
        assert(h.val == 18688);
    }
    {
        //test values near/above exponent max
        half h0(32767.f);
        half h1(32768.f);
        half h2(32769.f);
        assert(h1.val == h2.val); //both => overflow value, so are set to 65635
        assert(h0.val != h1.val); //32767 is below overflow limit
    }
}

void test_half_equals()
{
    std::printf("testing half float operator==\n");
    half h1(5.f);
    half h2(5.f);
    assert(h1 == h2);
    assert(h1 == h1);
}

void test_half_nequals()
{
    std::printf("testing half float operator!=\n");
    half h1(0.f);
    half h2(1.f);

    assert(h1 != h2);
}

////////////////////////////////////////////////////////////////////////////////
// triangle tests
////////////////////////////////////////////////////////////////////////////////

void test_triangle_add()
{
    std::printf("testing triangle add method\n");
    triangle t1 = triangle({1,1,1}, {2,2,2}, {3,3,3});
    assert(t1.add({2,2,2}) == triangle({3,3,3}, {4,4,4}, {5,5,5}));
}

void test_triangle_sub()
{
    std::printf("testing triangle subtract method\n");
    triangle t1 = triangle({1,1,1}, {2,2,2}, {3,3,3});
    assert(t1.sub({2,2,2}) == triangle({-1,-1,-1}, {0,0,0}, {1,1,1}));
}

void test_triangle_equals()
{
    std::printf("testing triangle equality operator\n");
    triangle t1 = triangle({1,1,1}, {2,2,2}, {3,3,3});
    triangle t2 = triangle({1,1,1}, {2,2,2}, {3,3,3});
    triangle t3 = triangle({0,0,0}, {2,2,2}, {3,3,3});
    triangle t4;
    t4.a = vec(0,0,0);
    t4.b = vec(2,2,2);
    t4.c = vec(3,3,3);
    assert(!(t1 == t3));
    assert(t1 == t2);
    assert(t1 == t1);
    assert(t4 == t3);
}

////////////////////////////////////////////////////////////////////////////////
// plane tests
////////////////////////////////////////////////////////////////////////////////

void test_plane_rayintersect()
{
    std::printf("testing plane intersection method\n");

    plane p = plane({0,0,1}, 0); // xy plane
    float dist = 0;
    assert(p.rayintersect({0,0,-1}, {0,-1,0}, dist) == false);
    assert(p.rayintersect({0,0,1}, {0,-1,0}, dist) == false);
    assert(p.rayintersect({0,0,1}, {0,0,-1}, dist));
    assert(p.rayintersect({0,0,1}, {0,0,-1}, dist));
}

void test_plane_zdelta()
{
    std::printf("testing plane zdelta method\n");

    {
        plane p = plane({1,0,0}, 0);
        assert(p.zdelta({0,0,0}) == 0);
        assert(p.zdelta({1,0,0}) == -std::numeric_limits<float>::infinity());
        assert(p.zdelta({-1,0,0}) == std::numeric_limits<float>::infinity());
    }
    {
        plane p = plane({1,1,1}, 0);
        assert(p.zdelta({-1,0,0}) == 1);
        assert(p.zdelta({-1,-1,0}) == 2);
    }
}

void test_plane_normalize()
{
    std::printf("testing plane normalization\n");

    {
        plane p = plane({1,0,0}, 0);
        plane p2 = plane({1,0,0}, 0).normalize();
        assert(p == p2);
    }
    {
        plane p = plane({3,4,0}, 5).normalize();
        plane p2 = plane({0.6,0.8,0}, 1);
        assert(p == p2);
    }
}

void test_plane_ctor()
{
    std::printf("testing plane constructors\n");
    //test throwing on invalid plane
    {
        bool exceptioncaught = false;
        try
        {
            plane p({0,0,0}, 0);
        }
        catch(const std::invalid_argument &e)
        {
            exceptioncaught = true;
        }
        assert(exceptioncaught);
    }
    {
        bool exceptioncaught = false;
        try
        {
            plane p(0,0,0,0);
        }
        catch(const std::invalid_argument &e)
        {
            exceptioncaught = true;
        }
        assert(exceptioncaught);
    }
    {
        bool exceptioncaught = false;
        try
        {
            plane p(vec4(0,0,0,0));
        }
        catch(const std::invalid_argument &e)
        {
            exceptioncaught = true;
        }
        assert(exceptioncaught);
    }
    //test equivalence of constructors
    {
        plane p(1,2,3,4);
        plane p2({1,2,3}, 4);
        plane p3(vec4(1,2,3,4));
        assert(p == p2);
        assert(p2 == p3);
    }
    //test xyz plane ctor
    {
        plane p(0,1.f);
        assert(p == plane(1.f,0,0,-1.f));
    }
    {
        plane p(1,1.f);
        assert(p == plane(0,1.f,0,-1.f));
    }
    {
        plane p(2,1.f);
        assert(p == plane(0,0,1.f,-1.f));
    }
    {
        bool exceptioncaught = false;
        try
        {
            plane p(3,1.f);
        }
        catch(const std::invalid_argument &e)
        {
            exceptioncaught = true;
        }
        assert(exceptioncaught);
    }
    {
        bool exceptioncaught = false;
        try
        {
            plane p(-1,1.f);
        }
        catch(const std::invalid_argument &e)
        {
            exceptioncaught = true;
        }
        assert(exceptioncaught);
    }
}

void test_plane_scale()
{
    std::printf("testing plane scale\n");

    plane p(1,2,3,4);
    p.scale(2);
    assert(p == plane(2,4,6,4));
    p.scale(-1);
    assert(p == plane(-2,-4,-6,4));
}

void test_plane_invert()
{
    std::printf("testing plane inversion\n");

    plane p(1,2,3,4);
    p.invert();
    assert(p == plane(-1,-2,-3,-4));
    p.invert();
    assert(p == plane(1,2,3,4));
}

void test_plane_reflectz()
{
    std::printf("testing plane reflectz\n");

    {
        plane p(0,0,1,0);
        p.reflectz(0);
        assert(p == plane(0,0,-1,0));
    }
    {
        plane p(0,0,1,1);
        p.reflectz(0);
        assert(p == plane(0,0,-1,1));
    }
}

void test_plane_toplane()
{
    std::printf("testing plane toplane methods\n");
    //toplane(vec,vec)
    {
        plane p;
        vec a(0,0,1);
        vec b(1,0,0);
        p.toplane(a,b);
        assert(p == plane(0,0,1,0));
    }
    //toplane(vec,vec,vec)
    {
        plane p;
        vec a(1,0,0);
        vec b(0,1,0);
        vec c(0,0,1);
        assert(p.toplane(a,b,c));
        assert(p == plane(sqrtf(3)/3, sqrtf(3)/3, sqrtf(3)/3, -sqrtf(3)/3));
    }
    {
        plane p;
        vec a(1,0,0);
        vec b(0,1,0);
        assert(p.toplane(a,a,a) == false);
        assert(p.toplane(a,a,b) == false);
        assert(p.toplane(a,b,a) == false);
        assert(p.toplane(b,a,a) == false);
    }
}

void test_plane_dist()
{
    std::printf("testing plane dist methods\n");

    //dist(vec)
    {
        plane p(0,0,1,0);
        vec o(0,0,1);
        assert(p.dist(o) == 1);
    }
    {
        plane p(0,0,1,0);
        vec o(0,0,-1);
        assert(p.dist(o) == -1);
    }
    //dist(vec4<float>)
    {
        plane p(0,0,1,0);
        vec4<float> o(0,0,1,1);
        assert(p.dist(o) == 1);
    }
    {
        plane p(0,0,1,0);
        vec4<float> o(0,0,1,2);
        assert(p.dist(o) == 1);
    }
    {
        plane p(0,0,1,1);
        vec4<float> o(0,0,1,2);
        assert(p.dist(o) == 3);
    }
}

////////////////////////////////////////////////////////////////////////////////
// quaternion tests
////////////////////////////////////////////////////////////////////////////////

void test_quat_ctor()
{
    std::printf("testing quaternion ctor\n");
    //quat(vec, float)
    {
        quat q({1,0,0}, 0);
        assert(q.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
    {
        quat q({1,0,0}, M_PI/2);
        assert(q.sub(quat(sqrt(2)/2, 0, 0, sqrt(2)/2)).magnitude() < tolerance);
    }
    {
        quat q({1,0,0}, M_PI);
        assert(q.sub(vec4<float>(1,0,0,0)).magnitude() < tolerance);
    }
    {
        quat q({1,0,0}, 3*M_PI/2);
        assert(q.sub(quat(sqrt(2)/2, 0, 0, -sqrt(2)/2)).magnitude() < tolerance);
    }
    //quat(vec, vec)
    {
        vec a(1,0,0);
        quat q(a,a);
        assert(q.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
    {
        vec a(1,0,0);
        vec b(0,1,0);
        quat q(a,b);
        assert(q.sub(quat(0, 0, sqrt(2)/2, sqrt(2)/2)).magnitude() < tolerance);
    }
    {
        vec a(1,0,0);
        vec b(0,1,0);
        quat q(b,a);
        assert(q.sub(quat(0, 0, -sqrt(2)/2, sqrt(2)/2)).magnitude() < tolerance);
    }
    //quat(vec)
    {
        vec a(0,0,0);
        quat q(a);
        assert(q.sub(quat(0,0,0,-1)).magnitude() < tolerance);
    }
    {
        vec a(1,2,3);
        quat q(a);
        assert(q.sub(quat(1,2,3,0)).magnitude() < tolerance);
    }
    {
        vec a(0.5,0,0);
        quat q(a);
        assert(q.sub(quat(0.5, 0, 0, -sqrt(3)/2)).magnitude() < tolerance);
    }
    //quat(matrix3)
    {
        matrix3 a;
        a.identity();
        quat q(a);
        assert(q.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
    {
        matrix3 a;
        //rotate 180 about x axis
        a.a = {1,0,0};
        a.b = {0,-1,0};
        a.c = {0,0,-1};
        quat q(a);
        assert(q.sub(quat(1,0,0,0)).magnitude() < tolerance);
    }
    {
        matrix3 a;
        //rotate 180 about y axis
        a.a = {-1,0,0};
        a.b = {0,1,0};
        a.c = {0,0,-1};
        quat q(a);
        assert(q.sub(quat(0,1,0,0)).magnitude() < tolerance);
    }
    {
        matrix3 a;
        //rotate 180 about z axis
        a.a = {-1,0,0};
        a.b = {0,-1,0};
        a.c = {0,0,1};
        quat q(a);
        assert(q.sub(quat(0,0,1,0)).magnitude() < tolerance);
    }
    //quat(matrix4x3)
    {
        matrix4x3 a;
        a.identity();
        quat q(a);
        assert(q.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
    //quat(matrix4)
    {
        matrix4 a;
        a.identity();
        quat q(a);
        assert(q.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
}

void test_quat_add()
{
    std::printf("testing quaternion addition\n");

    {
        quat a(0,0,0,1);
        a.add(a);
        assert(a.sub(quat(0,0,0,2)).magnitude() < tolerance);
    }
    {
        quat a(0,0,0,1);
        quat b(0,0,0,-1);
        a.add(b);
        assert(a.magnitude() < tolerance);
    }
}

void test_quat_sub()
{
    std::printf("testing quaternion subtraction\n");
    {
        quat a(0,0,0,1);
        a.sub(a);
        assert(a.magnitude() < tolerance);
    }
    {
        quat a(0,0,0,1);
        quat b(0,0,0,-1);
        a.sub(b);
        assert(a.sub(quat(0,0,0,2)).magnitude() < tolerance);
    }

}

void test_quat_mul()
{
    std::printf("testing quaternion multiplication\n");
    //mul(float)
    {
        quat a(0,0,0,1);
        a.mul(2);
        assert(a.sub(quat(0,0,0,2)).magnitude() < tolerance);
    }
    {
        quat a(0,0,0,1);
        a.mul(0);
        assert(a.sub(quat(0,0,0,0)).magnitude() < tolerance);
    }
    //mul(quat)
    {
        quat a(0,0,0,1);
        a.mul(a);
        assert(a.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
    {
        quat a(1,2,3,4),
             b(4,3,2,1);
        a.mul(b);
        assert(a.sub(quat(12,24,6,-12)).magnitude() < tolerance);
    }
    {
        // 1 * i = i
        quat a(0,0,0,1),
             b(1,0,0,0);
        a.mul(b);
        assert(a.sub(quat(1,0,0,0)).magnitude() < tolerance);
    }
    {
        // i * j = k
        quat a(1,0,0,0),
             b(0,1,0,0);
        a.mul(b);
        assert(a.sub(quat(0,0,1,0)).magnitude() < tolerance);
    }
    {
        // j * i = -k
        quat a(0,1,0,0),
             b(1,0,0,0);
        a.mul(b);
        assert(a.sub(quat(0,0,-1,0)).magnitude() < tolerance);
    }
    {
        // j * k = i
        quat a(0,1,0,0),
             b(0,0,1,0);
        a.mul(b);
        assert(a.sub(quat(1,0,0,0)).magnitude() < tolerance);
    }
    {
        // k * j = -i
        quat a(0,0,1,0),
             b(0,1,0,0);
        a.mul(b);
        assert(a.sub(quat(-1,0,0,0)).magnitude() < tolerance);
    }
    {
        // k * i = j
        quat a(0,0,1,0),
             b(1,0,0,0);
        a.mul(b);
        assert(a.sub(quat(0,1,0,0)).magnitude() < tolerance);
    }
    {
        // i * k = -j
        quat a(1,0,0,0),
             b(0,0,1,0);
        a.mul(b);
        assert(a.sub(quat(0,-1,0,0)).magnitude() < tolerance);
    }
    //mul(quat, quat)
    {
        quat a,
             b(0,0,0,1),
             c(0,0,0,1);
        a.mul(b,c);
        assert(a.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
    {
        quat a,
             b(0,0,0,1),
             c(1,0,0,0);
        a.mul(b,c);
        assert(a.sub(quat(1,0,0,0)).magnitude() < tolerance);
    }
    {
        quat a,
             b(1,2,3,0),
             c(4,0,2,1);
        a.mul(b,c);
        assert(a.sub(quat(5,12,-5,-10)).magnitude() < tolerance);
    }
}

void test_quat_madd()
{
    std::printf("testing quaternion multiply-add\n");

    {
        quat q(0,0,0,1);
        vec4<float> b(0,0,0,1);
        q.madd(b, 1);
        assert(q.sub(quat(0,0,0,2)).magnitude() < tolerance);
    }
    {
        quat q(0,0,0,1);
        vec4<float> b(1,1,1,1);
        q.madd(b, 2);
        assert(q.sub(quat(2,2,2,3)).magnitude() < tolerance);
    }
}

void test_quat_msub()
{
    std::printf("testing quaternion multiply-subtract\n");

    {
        quat q(0,0,0,1);
        vec4<float> b(0,0,0,1);
        q.msub(b, 1);
        assert(q.magnitude() < tolerance);
    }
    {
        quat q(0,0,0,1);
        vec4<float> b(1,1,1,1);
        q.msub(b, 2);
        assert(q.sub(quat(-2,-2,-2,-1)).magnitude() < tolerance);
    }
}

void test_quat_invert()
{
    std::printf("testing quaternion inversion\n");

    {
        quat q(0,0,0,1);
        q.invert();
        assert(q.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
    {
        quat q(0,0,0,0);
        q.invert();
        assert(q.magnitude() < tolerance);
    }
    {
        quat q(-1,1,-1,1);
        q.invert();
        assert(q.sub(quat(1,-1,1,1)).magnitude() < tolerance);
    }
}

void test_quat_normalize()
{
    std::printf("testing quaternion normalization\n");

    {
        quat q(0,0,0,1);
        q.normalize();
        assert(q.sub(quat(0,0,0,1)).magnitude() < tolerance);
    }
    {
        //attempt normalizing zero quaternion
        quat q(0,0,0,0);
        q.normalize();
        assert(q == quat(0,0,0,0));
    }
    {
        quat q(0,0,1,1);
        q.normalize();
        assert(q.sub(quat(0,0,std::sqrt(2)/2,std::sqrt(2)/2)).magnitude() < tolerance);
    }
}

void test_quat_rotate()
{
    std::printf("testing quaternion rotation\n");

    {
        //identity transformation
        quat q(0,0,0,1);
        vec v = q.rotate(vec(1,0,0));
        assert(v.sub(vec(1,0,0)).magnitude() < tolerance);
    }
    {
        //90 degree rotation about z axis
        quat q(0,0,std::sqrt(2)/2,std::sqrt(2)/2);
        vec v = q.rotate(vec(1,0,0));
        assert(v.sub(vec(0,1,0)).magnitude() < tolerance);
    }
    {
        //test rotating 90 degrees around (1,1,1) axis
        quat q(sqrt(3)/3,std::sqrt(3)/3,std::sqrt(3)/3,0);
        vec v = q.rotate(vec(1,0,0));
        assert(v.sub(vec(-1.f/3,2.f/3,2.f/3)).magnitude() < tolerance);
    }
    {
        //rotate does not preserve behavior if not normalized
        quat q(0,0,1,1);
        vec v = q.rotate(vec(1,0,0));
        assert(v.sub(vec(-1,2,0)).magnitude() < tolerance);
    }

}

void test_quat_invertedrotate()
{
    std::printf("testing quaternion inverse rotation\n");
    {
        //identity transformation
        quat q(0,0,0,1);
        vec v = q.invertedrotate(vec(1,0,0));
        assert(v.sub(vec(1,0,0)).magnitude() < tolerance);
    }
    {
        quat q(0,0,std::sqrt(2)/2,std::sqrt(2)/2);
        vec v = q.invertedrotate(vec(1,0,0));
        assert(v.sub(vec(0,-1,0)).magnitude() < tolerance);
    }
    {
        //inverted rotate does not preserve behavior if not normalized
        quat q(0,0,1,1);
        vec v = q.invertedrotate(vec(1,0,0));
        assert(v.sub(vec(-1,-2,0)).magnitude() < tolerance);
    }
}

////////////////////////////////////////////////////////////////////////////////
// dual quaternion tests
////////////////////////////////////////////////////////////////////////////////

void test_dualquat_ctor()
{
    std::printf("testing dual quaternion ctors\n");

    //dualquat(quat,vec)
    {
        quat q(0,0,0,1);
        vec v(1,0,0);
        dualquat dq(q, v);
        assert(dq.real.sub(quat(0,0,0,1)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0.5,0,0,0)).magnitude() < tolerance);
    }
    {
        quat q(1,0,0,1);
        vec v(1,2,3);
        dualquat dq(q, v);
        assert(dq.real.sub(quat(1,0,0,1)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0.5,2.5,0.5,-0.5)).magnitude() < tolerance);
    }
    //dualquat(quat)
    {
        quat q(1,2,3,1);
        dualquat dq(q);
        assert(dq.real == q);
        assert(dq.dual == quat(0,0,0,0));
    }
    //dualquat(matrix4x3)
    {
        matrix4x3 m;
        m.identity();
        dualquat dq(m);
        assert(dq.real.sub(quat(0,0,0,1)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,0,0,0)).magnitude() < tolerance);
    }
    {
        matrix4x3 m;
        m.identity();
        m.b.y = -1;
        m.a.x = -1;
        dualquat dq(m);
        assert(dq.real.sub(quat(0,0,1,0)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,0,0,0)).magnitude() < tolerance);
    }
}

void test_dualquat_invert()
{
    std::printf("testing dual quaternion inversion\n");

    {
        //test ordinary case
        dualquat dq(quat(0,1,0,1));
        dq.dual = quat(0,1,0,1);
        dq.invert();
        assert(dq.real.sub(quat(0,-1,0,1)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,3,0,-3)).magnitude() < tolerance);
    }
    {
        //test zero dual case
        dualquat dq(quat(1,1,1,1));
        dq.dual = quat(0,0,0,0);
        dq.invert();
        assert(dq.real.sub(quat(-1,-1,-1,1)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,0,0,0)).magnitude() < tolerance);
    }
    {
        //test zero real/dual case
        dualquat dq(quat(0,0,0,0));
        dq.dual = quat(0,0,0,0);
        dq.invert();
        assert(dq.real.sub(quat(0,0,0,0)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,0,0,0)).magnitude() < tolerance);
    }
}

void test_dualquat_mul()
{
    std::printf("testing dual quaternion multiplication\n");

    //mul(float)
    {
        //multipy by zero
        quat q(1,0,0,1);
        vec v(1,2,3);
        dualquat dq(q, v);
        dq.mul(0);
        assert(dq.real.sub(quat(0,0,0,0)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,0,0,0)).magnitude() < tolerance);
    }
    {
        quat q(1,0,0,1);
        vec v(1,2,3);
        dualquat dq(q, v);
        dq.mul(2);
        assert(dq.real.sub(quat(2,0,0,2)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(1,5,1,-1)).magnitude() < tolerance);
    }
    //mul(dualquat, dualquat)
    {
        quat a(1,2,3,4),
             b(4,3,2,1);
        dualquat dq,
                 dqa(a),
                 dqb(b);
        dq.mul(dqa, dqb);
        assert(dq.real.sub(quat(12,24,6,-12)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,0,0,0)).magnitude() < tolerance);
    }
    {
        quat a(1,2,3,4),
             b(4,3,2,1);
        dualquat dq,
                 dqa(a),
                 dqb(b);
        dqa.dual = b;
        dqb.dual = a;
        dq.mul(dqa, dqb);
        assert(dq.real.sub(quat(12,24,6,-12)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(16,22,28,-26)).magnitude() < tolerance);
    }
    //mul(dualquat)
    {
        quat a(1,2,3,4),
             b(4,3,2,1);
        dualquat dqa(a),
                 dqb(b);
        dqa.mul(dqb);
        assert(dqa.real.sub(quat(12,24,6,-12)).magnitude() < tolerance);
        assert(dqa.dual.sub(quat(0,0,0,0)).magnitude() < tolerance);
    }
    {
        quat a(1,2,3,4),
             b(4,3,2,1);
        dualquat dqa(a),
                 dqb(b);
        dqa.dual = b;
        dqb.dual = a;
        dqa.mul(dqb);
        assert(dqa.real.sub(quat(12,24,6,-12)).magnitude() < tolerance);
        assert(dqa.dual.sub(quat(16,22,28,-26)).magnitude() < tolerance);
    }
}

void test_dualquat_mulorient()
{
    std::printf("testing dual quaternion mulorient\n");

    quat a(1,2,3,4),
         b(4,3,2,1);
    //mulorient(quat)
    {
        dualquat dqa(a);
        dqa.dual = b;
        dqa.mulorient(a);
        assert(dqa.real.sub(quat(8,16,24,2)).magnitude() < tolerance);
        assert(dqa.dual.sub(quat(20,0,10,20)).magnitude() < tolerance);
    }
    {
        dualquat dqa(a);
        dqa.dual = a;
        dqa.mulorient(a);
        assert(dqa.real.sub(quat(8,16,24,2)).magnitude() < tolerance);
        assert(dqa.dual.sub(quat(0,0,0,30)).magnitude() < tolerance);
    }
    {
        dualquat dqb(b);
        dqb.dual = a;
        dqb.mulorient(a);
        assert(dqb.real.sub(quat(12,24,6,-12)).magnitude() < tolerance);
        assert(dqb.dual.sub(quat(0,0,0,30)).magnitude() < tolerance);
    }
    //mulorient(quat,dualquat)
    {
        dualquat dqa(a);
        dqa.dual = b;
        dualquat dqc = dqa;
        dqa.mulorient(a, dqc);
        assert(dqa.real.sub(quat(8,16,24,2)).magnitude() < tolerance);
        assert(dqa.dual.sub(quat(620,0,-470,980)).magnitude() < tolerance);
    }
    {
        dualquat dqb(b);
        dqb.dual = a;
        dualquat dqc = dqb;
        dqb.mulorient(a, dqc);
        assert(dqb.real.sub(quat(12,24,6,-12)).magnitude() < tolerance);
        assert(dqb.dual.sub(quat(-80,-1160,-320,270)).magnitude() < tolerance);
    }
}

void test_dualquat_normalize()
{
    std::printf("testing dual quaternion normalize\n");

    {
        //test trivial case (no scaling)
        quat a(1,0,0,0),
             b(2,2,2,2);
        dualquat dq;
        dq.real = a;
        dq.dual = b;
        dq.normalize();
        assert(dq.real.sub(quat(1,0,0,0)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(2,2,2,2)).magnitude() < tolerance);
    }
    {
        //scale by sqrt(2)/2
        quat a(1,1,0,0),
             b(2,2,2,2);
        dualquat dq;
        dq.real = a;
        dq.dual = b;
        dq.normalize();
        assert(dq.real.sub(quat(std::sqrt(2)/2,std::sqrt(2)/2,0,0)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(std::sqrt(2),std::sqrt(2),std::sqrt(2),std::sqrt(2))).magnitude() < tolerance);
    }
}

void test_dualquat_translate()
{
    std::printf("testing dual quaternion translate\n");

    {
        //test no translation
        dualquat dq(quat(0,1,0,1));
        dq.dual = quat(0,1,0,1);
        dq.translate(vec(0,0,0));
        assert(dq.real.sub(quat(0,1,0,1)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,1,0,1)).magnitude() < tolerance);
    }
    {
        //test translation 1,1,1
        dualquat dq(quat(0,1,0,1));
        dq.dual = quat(0,1,0,1);
        dq.translate(vec(1,1,1));
        assert(dq.real.sub(quat(0,1,0,1)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(0,1.5,1,0.5)).magnitude() < tolerance);
    }
}

void test_dualquat_fixantipodal()
{
    std::printf("testing dual quaternion fixantipodal\n");

    {
        //test case where no action (dot >= 0)
        quat a(1,0,0,0),
             b(1,1,1,1);
        dualquat dq;
        dq.real = a;
        dq.dual = b;
        dualquat dq2 = dq;
        dq.fixantipodal(dq);
        assert(dq.real == dq2.real);
        assert(dq.dual == dq2.dual);
    }
    {
        //test case where negation action taken(dot < 0)
        quat a(-1,0,0,0),
             b(2,2,2,2);
        dualquat dq;
        dq.real = a;
        dq.dual = b;
        //make dot product negative
        dualquat dq2 = dq;
        dq2.real.x = 1;

        dq.fixantipodal(dq2);
        assert(dq.real.sub(quat(1,0,0,0)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(-2,-2,-2,-2)).magnitude() < tolerance);
    }

}

void test_dualquat_accumulate()
{
    std::printf("testing dual quaternion accumulate\n");

    {
        //test case where k > 0
        quat a(1,1,1,1),
             b(1,1,1,1);
        dualquat dq;
        dq.real = a;
        dq.dual = b;
        dq.accumulate(dq, 2);
        assert(dq.real.sub(quat(3,3,3,3)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(3,3,3,3)).magnitude() < tolerance);
    }
    {
        //test case where k > 0
        quat a(1,1,1,1),
             b(1,1,1,1),
             c(-1,-1,-1,-1);
        dualquat dq;
        dq.real = a;
        dq.dual = b;
        //make dot product negative
        dualquat dq2 = dq;
        dq2.real = c;
        dq.accumulate(dq2, 2);
        assert(dq.real.sub(quat(3,3,3,3)).magnitude() < tolerance);
        assert(dq.dual.sub(quat(-1,-1,-1,-1)).magnitude() < tolerance);
    }
}

void test_dualquat_transform()
{
    std::printf("testing dual quaternion transform\n");

    //transform(quat)
    {
        quat a(1,2,3,4),
             b(4,3,2,1);
        dualquat dq(a);
        quat qout = dq.transform(b);
        assert(qout.sub(quat(12,24,6,-12)).magnitude() < tolerance);
    }
    {
        //test that setting dual does not affect result
        quat a(1,2,3,4),
             b(4,3,2,1);
        dualquat dq(a);
        dq.dual = b;
        quat qout = dq.transform(b);
        assert(qout.sub(quat(12,24,6,-12)).magnitude() < tolerance);
    }
    //transform(vec)
    {
        //test identity operation
        quat a(0,0,0,1),
             b(0,0,0,0);
        dualquat dq(a);
        dq.dual = b;
        vec vout = dq.transform(vec(1,0,0));
        assert(vout.sub(vec(1,0,0)).magnitude() < tolerance);
    }
    {
        //test simple transformation
        quat a(0.5,0.5,0.5,0.5),
             b(1,0,0,0);
        dualquat dq(a);
        dq.dual = b;
        vec vout = dq.transform(vec(1,0,0));
        assert(vout.sub(vec(1,2,-1)).magnitude() < tolerance);
    }

}

void test_dualquat_transformnormal()
{
    std::printf("testing dual quaternion transform normal\n");

    {
        //identity transformation
        quat q(0,0,0,1);
        dualquat dq(q);
        vec v = dq.transformnormal(vec(1,0,0));
        assert(v.sub(vec(1,0,0)).magnitude() < tolerance);
    }
    {
        quat q(0,0,std::sqrt(2)/2,std::sqrt(2)/2);
        dualquat dq(q);
        vec v = dq.transformnormal(vec(1,0,0));
        assert(v.sub(vec(0,1,0)).magnitude() < tolerance);
    }
}

////////////////////////////////////////////////////////////////////////////////
// short quaternion tests
////////////////////////////////////////////////////////////////////////////////

void test_squat_ctor()
{
    std::printf("testing short quaternion ctor\n");
    {
        quat q(0,0,0,0.1);
        squat s(q);
        assert(s.x == 0 && s.y == 0 && s.z == 0 && s.w == 3276);
    }
    {
        quat q(0,0,0,1);
        squat s(q);
        assert(s.x == 0 && s.y == 0 && s.z == 0 && s.w == 32767);
    }
}

void test_squat_lerp()
{
    std::printf("testing short quaternion lerp\n");
    {
        vec4<float> a(0,0,0,0);
        squat s;
        s.lerp(a,a,1);
        assert(s.x == 0 && s.y == 0 && s.z == 0 && s.w == 0);
    }
    {
        vec4<float> a(0,0,0,1),
                    b(0,0,0,1);
        squat s;
        s.lerp(a,b,1);
        assert(s.x == 0 && s.y == 0 && s.z == 0 && s.w == 32767);
    }
    {
        vec4<float> a(0,0,0,1),
                    b(0.5,0,0,0);
        squat s;
        s.lerp(a,b,2);
        assert(s.x == 32767 && s.y == 0 && s.z == 0 && s.w == -32768);
    }
}

void test_geomexts()
{
    std::printf(
"===============================================================\n\
testing geometry extensions\n\
===============================================================\n"
    );

    test_half_ctor();
    test_half_equals();
    test_half_nequals();

    test_triangle_add();
    test_triangle_sub();
    test_triangle_equals();

    test_plane_rayintersect();
    test_plane_zdelta();
    test_plane_normalize();
    test_plane_ctor();
    test_plane_scale();
    test_plane_invert();
    test_plane_reflectz();
    test_plane_toplane();
    test_plane_dist();

    test_quat_ctor();
    test_quat_add();
    test_quat_sub();
    test_quat_mul();
    test_quat_madd();
    test_quat_msub();
    test_quat_invert();
    test_quat_normalize();
    test_quat_rotate();
    test_quat_invertedrotate();

    test_dualquat_ctor();
    test_dualquat_invert();
    test_dualquat_mul();
    test_dualquat_mulorient();
    test_dualquat_normalize();
    test_dualquat_translate();
    test_dualquat_fixantipodal();
    test_dualquat_accumulate();
    test_dualquat_transform();
    test_dualquat_transformnormal();

    test_squat_ctor();
    test_squat_lerp();
}

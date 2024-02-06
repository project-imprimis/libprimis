
#include "libprimis.h"
#include "../src/shared/geomexts.h"

constexpr float tolerance = 0.001;

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
    assert(!(t1 == t3));
    assert(t1 == t2);
    assert(t1 == t1);
}

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

void test_geomexts()
{
    std::printf(
"===============================================================\n\
testing geometry extensions\n\
===============================================================\n"
    );
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
}

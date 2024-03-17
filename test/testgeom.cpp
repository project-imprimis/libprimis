#include "libprimis.h"
#include "../src/shared/geomexts.h"

constexpr float tolerance = 0.001;

void test_raysphereintersect()
{
    std::printf("testing raysphereintersect\n");

    //intersection tests
    {
        //ray to ~infinity
        vec center(0,0,0);
        float radius = 1;
        vec o(0,0,-4),
            ray(0,0,9999999);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected);
        assert(std::abs(dist) < tolerance);
    }
    {
        //ray starting inside sphere
        vec center(0,0,0);
        float radius = 1;
        vec o(0,0,0),
            ray(0,0,2);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected);
        assert(std::abs(1 + dist) < tolerance);
    }
    {
        //ray starting at tangent
        vec center(0,0,0);
        float radius = 1;
        vec o(0,1,0),
            ray(0,0,2);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected);
        assert(std::abs(dist) < tolerance);
    }
    {
        //ray ending at tangent
        vec center(0,0,0);
        float radius = 1;
        vec o(0,-1,-1),
            ray(0,0,1);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected);
        assert(std::abs(1 - dist) < tolerance);
    }
    //non-intersection tests
    {
        //ray too far in +y
        vec center(0,0,0);
        float radius = 1;
        vec o(0,2,0),
            ray(0,0,1);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected == false);
        assert(std::abs(dist) < tolerance);
    }
}

void test_linecylinderintersect()
{
    std::printf("testing linecylinderintersect\n");
    //intersection tests
    {
        //test line from origin to z=10 intersecting cylinder r=1 halfway through
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,5),
              end(1,0,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.4 - dist) < tolerance);
    }
    {
        //test line from origin to z=10 intersecting cylinder centered at z=10 and not leaving
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,10),
              end(1,0,10);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.9 - dist) < tolerance);
    }
    {
        //test line from origin to z=10 entirely inside intersecting cylinder
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,5),
              end(1,0,5);
        float radius = 6,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test line from origin to z=10 entirely inside intersecting cylinder, with dist set
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,5),
              end(1,0,5);
        float radius = 6,
              dist = 10;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test line intersecting 0-size cylinder through middle
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,5),
              end(1,0,5);
        float radius = 0,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.5 - dist) < tolerance);
    }
    {
        //test line intersecting cylinder at tangent point
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(1,1,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.5 - dist) < tolerance);
    }
    {
        //test line intersecting cylinder at tangent point, at endpoint of line
        vec  from(0,0,0),
               to(0,0,5),
            start(-1,1,5),
              end(1,1,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(1 - dist) < tolerance);
    }
    {
        //test line intersecting cylinder at tangent, at edge
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(0,1,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.5 - dist) < tolerance);
    }
    {
        //test diagonal line intersecting cylinder through edge
        vec  from(0,0,0),
               to(10,0,10),
            start(3,0,5),
              end(7,0,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.4 - dist) < tolerance);
    }
    {
        //test diagonal line intersecting cylinder through cylinder caps
        vec  from(0,0,0),
               to(10,0,10),
            start(4.5,0,5),
              end(5.5,0,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.45 - dist) < tolerance);
    }
    {
        //test line intersecting cylinder of negative radius
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(1,1,5);
        float radius = -1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.5 - dist) < tolerance);
    }
    //non-intersection tests
    {
        //test line which nearly intersects cylinder along cylinder tangent
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(0,1,5);
        float radius = 0.99,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected == false);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test line which nearly intersects cylinder edge
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(-0.01,1,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected == false);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test line which nearly intersects cylinder opposite edge
        vec  from(0,0,0),
               to(0,0,10),
            start(1,1,5),
              end(0.01,1,5);
        float radius = 0.1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected == false);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test case where dist is already set
        vec  from(0,0,0),
               to(0,0,10),
            start(1,1,5),
              end(0.01,1,5);
        float radius = 0.1,
              dist = 10;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected == false);
        assert(std::abs(10 - dist) < tolerance);
    }
}

void test_polyclip()
{
    std::printf("testing polyclip\n");
    {
        //test no directionality
        std::array<vec, 5> in = {{vec(0,0,0), vec(0,0,1), vec(0,1,1), vec(0,1,0), vec(0,2,0)}};
        std::array<vec, 5> out;
        out.fill(vec(0,0,0));

        polyclip(in.data(), 5, vec(0,0,0), 0, 0, out.data());
        assert(out[0] == vec(0,0,0));
        assert(out[1] == vec(0,0,1));
        assert(out[2] == vec(0,1,1));
        assert(out[3] == vec(0,1,0));
        assert(out[4] == vec(0,2,0));
    }
    {
        //test clipping +z at 0
        std::array<vec, 5> in = {{vec(0,0,0), vec(0,0,1), vec(0,1,1), vec(0,1,0), vec(0,2,0)}};
        std::array<vec, 5> out;
        out.fill(vec(0,0,0));

        polyclip(in.data(), 5, vec(0,0,1), 0, 0, out.data());
        assert(out[0] == vec(0,0,0));
        assert(out[1] == vec(0,1,0));
        assert(out[2] == vec(0,2,0));
        assert(out[3] == vec(0,0,0));
        assert(out[4] == vec(0,0,0));
    }
    {
        //test clipping +z 1..2
        std::array<vec, 5> in = {{vec(0,0,0), vec(0,0,1), vec(0,1,3), vec(0,1,1), vec(0,2,0)}};
        std::array<vec, 5> out;
        out.fill(vec(0,0,0));

        polyclip(in.data(), 5, vec(0,0,1), 1, 2, out.data());
        assert(out[0] == vec(0,0,1));
        assert(out[1] == vec(0,0.5,2));
        assert(out[2] == vec(0,1,2));
        assert(out[3] == vec(0,1,1));
        assert(out[4] == vec(0,0,0));
    }
}

void test_ivec_dist()
{
    std::printf("testing ivec dist\n");

    {
        plane p(vec(0,0,1), 0);
        ivec i(1,1,1);
        assert(i.dist(p) == 1);
    }
    {
        plane p(vec(0,0,1), 0);
        ivec i(1,1,0);
        assert(i.dist(p) == 0);
    }
}

void test_mod360()
{
    std::printf("testing mod360\n");
    assert(mod360(0) == 0);
    assert(mod360(360) == 0);
    assert(mod360(540) == 180);
    assert(mod360(-180) == 180);
    assert(mod360(900) == 180);
}

void test_sin360()
{
    std::printf("testing sin360\n");

    assert(sin360(0) == 0);
    assert(sin360(90) == 1);
    assert(sin360(180) == 0);
    assert(sin360(270) == -1);
    assert(sin360(360) == 0);

    assert(std::abs(sin360(90) - std::sin(90/RAD)) < tolerance);
    assert(std::abs(sin360(60) - std::sin(60/RAD)) < tolerance);
    assert(std::abs(sin360(52) - std::sin(52/RAD)) < tolerance);
    assert(std::abs(sin360(700) - std::sin(700/RAD)) < tolerance);
    assert(std::abs(sin360(720) - std::sin(720/RAD)) < tolerance);
}

void test_cos360()
{
    std::printf("testing cos360\n");

    assert(cos360(0) == 1);
    assert(cos360(90) == 0);
    assert(cos360(180) == -1);
    assert(cos360(270) == 0);
    assert(cos360(360) == 1);

    assert(std::abs(cos360(95) - std::cos(95/RAD)) < tolerance);
    assert(std::abs(cos360(69) - std::cos(69/RAD)) < tolerance);
    assert(std::abs(cos360(52) - std::cos(52/RAD)) < tolerance);
    assert(std::abs(cos360(700) - std::cos(700/RAD)) < tolerance);
    assert(std::abs(cos360(720) - std::cos(720/RAD)) < tolerance);
}

void test_geom()
{
    std::printf(
"===============================================================\n\
testing geometry\n\
===============================================================\n"
    );

    test_raysphereintersect();
    test_linecylinderintersect();
    test_polyclip();
    test_ivec_dist();
    test_mod360();
    test_sin360();
    test_cos360();
}


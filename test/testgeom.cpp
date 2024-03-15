#include "libprimis.h"
#include "../src/shared/geomexts.h"

constexpr float tolerance = 0.001;

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

void test_geom()
{
    std::printf(
"===============================================================\n\
testing geometry\n\
===============================================================\n"
    );

    test_linecylinderintersect();
    test_ivec_dist();
}


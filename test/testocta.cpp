
#include "libprimis.h"
#include "../shared/geomexts.h"
#include "../src/engine/world/octaworld.h"

namespace
{
    void testoctaboxoverlap()
    {
        std::printf("Testing octaboxoverlap, pure octants\n");
        uchar expected = 1;
        uchar test = octaboxoverlap({0,0,0}, 0, {-2,-2,-2}, {-1, -1, -1});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {2, -2,-2}, {3, -1, -1});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {-2, 2,-2}, {-1, 3, -1});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {2, 2,-2}, {3, 3, -1});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {-2, -2, 2}, {-1, -1, 3});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {2, -2, 2}, {3, -1, 3});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {-2, 2, 2}, {-1, -1, 3});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {2,2,2}, {3, 3, 3});
        assert(test == expected);
        expected *= 2;

        std::printf("Testing octaboxoverlap, multiple octants\n");
        test = octaboxoverlap({0,0,0}, 0, {-2,-2,-2}, {1, -1, -1});
        assert(test == 3); //octants 0 and 1
        test = octaboxoverlap({0,0,0}, 0, {-2,-2,-2}, {1, 1, -1});
        assert(test == 15); //octants 0,1,2,3
        test = octaboxoverlap({0,0,0}, 0, {-2,-2,-2}, {1, 1, 1});
        assert(test == 255); //all octants
    }

    void testfamilysize()
    {
        std::printf("Testing cube familysize\n");
        cube c1;
        c1.children = newcubes();
        assert(familysize(c1) == 9);
        (*c1.children)[0].children = newcubes();
        assert(familysize(c1) == 17);
        assert(familysize((*c1.children)[0]) == 9);
        freeocta(c1.children);
    }

    void testgetcubevector()
    {
        std::printf("Testing getcubevector\n");
        cube c1;
        ivec p = {0,0,0};
        getcubevector(c1, 0, 1, 1, 1, p);
        assert(p == ivec(15, 0, 0));
    }

    void test_octadim()
    {
        std::printf("Testing octadim\n");

        assert(octadim(10) == 1024);
        assert(octadim(5) == 32);
        assert(octadim(1) == 2);
        assert(octadim(0) == 1);
    }
}

void test_octa()
{
    testoctaboxoverlap();
    testfamilysize();
    testgetcubevector();
    test_octadim();
}

#include "libprimis.h"
#include "../src/shared/geomexts.h"

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

    test_ivec_dist();
}


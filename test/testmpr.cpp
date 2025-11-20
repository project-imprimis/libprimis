
#include "libprimis.h"
#include "../src/shared/geomexts.h"
#include "../src/engine/world/mpr.h"
#include "../src/engine/world/octaworld.h"

namespace
{
    void test_cubeplanes_ctor()
    {
        std::printf("testing cubeplanes::cubeplanes\n");
        clipplanes clip;
        mpr::CubePlanes p(clip);
        assert(&p.p == &clip);
    }
}

void test_mpr()
{
    std::printf(
"===============================================================\n\
testing mpr functionality\n\
===============================================================\n"
    );
    test_cubeplanes_ctor();
}

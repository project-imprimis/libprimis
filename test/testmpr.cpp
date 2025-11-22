
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

    void test_cubeplanes_center()
    {
        std::printf("testing cubeplanes::center\n");
        clipplanes clip;
        clip.clear();
        mpr::CubePlanes p(clip);
        assert(p.center() == vec(0,0,0));
    }

    void test_cubeplanes_supportpoint()
    {
        std::printf("testing cubeplanes::supportpoint\n");
        {
            clipplanes clip;
            clip.v = {{
                {0,0,0},
                {0,0,0},
                {0,0,0},
                {0,0,0},
                {0,0,0},
                {0,0,0},
                {0,0,0},
                {0,0,0}
            }};
            mpr::CubePlanes p(clip);
            assert(p.supportpoint(vec(0,0,0)) == vec(0,0,0));
        }
        {
            clipplanes clip;
            clip.v = {{
                {0,0,0},
                {0,0,0},
                {0,0,0},
                {0,1,0},
                {0,0,0},
                {0,0,0},
                {0,0,0},
                {0,0,0}
            }};
            mpr::CubePlanes p(clip);
            assert(p.supportpoint(vec(0,1,0)) == vec(0,1,0));
        }
        {
            clipplanes clip;
            clip.v = {{
                {0,0,0},
                {0,0,0},
                {0,0,0},
                {0,1,0},
                {0,0,0},
                {0,0,0},
                {0,2,2},
                {0,0,0}
            }};
            mpr::CubePlanes p(clip);
            assert(p.supportpoint(vec(0,1,0)) == vec(0,2,2));
        }
    }

    void test_solidcube_ctor()
    {
        std::printf("testing solidcube::solidcube\n");
        {
            mpr::SolidCube s(1,2,3,4);
            assert(s.o == vec(1,2,3));
            assert(s.size == 4);
        }
        {
            mpr::SolidCube s(vec(1,2,3),4);
            assert(s.o == vec(1,2,3));
            assert(s.size == 4);
        }
        {
            mpr::SolidCube s(ivec(1,2,3),4);
            assert(s.o == vec(1,2,3));
            assert(s.size == 4);
        }
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
    test_cubeplanes_center();
    test_cubeplanes_supportpoint();
    test_solidcube_ctor();
}

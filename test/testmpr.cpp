
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
        assert(&p.p != nullptr);
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

    void test_solidcube_center()
    {
        std::printf("testing solidcube::center\n");
        {
            mpr::SolidCube s(1,2,3,0);
            assert(s.center() == vec(1,2,3));
        }
        {
            mpr::SolidCube s(1,2,3,4);
            assert(s.center() == vec(3,4,5));
        }
    }

    void test_solidcube_supportpoint()
    {
        std::printf("testing solidcube::supportpoint\n");
        {
            mpr::SolidCube s(1,2,3,0);
            assert(s.supportpoint(vec(1,1,1)) == vec(1,2,3));
        }
        {
            mpr::SolidCube s(1,2,3,1);
            assert(s.supportpoint(vec(1,1,1)) == vec(2,3,4));
        }
        {
            mpr::SolidCube s(1,2,3,1);
            assert(s.supportpoint(vec(0,1,1)) == vec(1,3,4));
        }
    }

    void test_ent_ctor()
    {
        std::printf("testing ent::ent\n");
        physent p;
        mpr::Ent e(&p);
        assert(e.ent == &p);
        assert(e.ent != nullptr);
    }

    void test_ent_center()
    {
        std::printf("testing ent::center\n");
        {
            physent p;
            p.o = vec(1,2,3);
            p.aboveeye = 0;
            p.eyeheight = 0;
            mpr::Ent e(&p);
            assert(e.center() == vec(1,2,3));
        }
        {
            physent p;
            p.o = vec(1,2,3);
            p.aboveeye = 2;
            p.eyeheight = 0;
            mpr::Ent e(&p);
            assert(e.center() == vec(1,2,4));
        }
        {
            physent p;
            p.o = vec(1,2,3);
            p.aboveeye = 4;
            p.eyeheight = 2;
            mpr::Ent e(&p);
            assert(e.center() == vec(1,2,4));
        }
    }

    void test_entobb_supportpoint()
    {
        std::printf("testing entobb::supportpoint\n");
        physent p;
        p.xradius = 1;
        p.yradius = 1;
        p.eyeheight = 1;
        p.o = vec(1,1,1);
        p.yaw = 0;
        mpr::EntOBB e(&p);
        vec v = e.supportpoint(vec(1,1,1));
        assert(v == vec(2,2,3));
    }

    void test_entobb_left_right()
    {
        std::printf("testing entobb::left and entobb::right\n");
        {
            physent p;
            p.xradius = 1;
            p.yradius = 1;
            p.eyeheight = 1;
            p.o = vec(0,0,0);
            p.yaw = 0;
            mpr::EntOBB e(&p);
            float l = e.left();
            float r = e.right();
            assert(l == -r);
            assert(l == -1.f);
        }
        {
            physent p;
            p.xradius = 2;
            p.yradius = 1;
            p.eyeheight = 1;
            p.o = vec(0,0,0);
            p.yaw = 0;
            mpr::EntOBB e(&p);
            float l = e.left();
            float r = e.right();
            assert(l == -r);
            assert(l == -2.f);
        }
    }

    void test_entobb_front_back()
    {
        std::printf("testing entobb::front and entobb::back\n");
        {
            physent p;
            p.xradius = 1;
            p.yradius = 1;
            p.eyeheight = 1;
            p.o = vec(0,0,0);
            p.yaw = 0;
            mpr::EntOBB e(&p);
            float f = e.front();
            float b = e.back();
            assert(f == -b);
            assert(f == 1.f);
        }
        {
            physent p;
            p.xradius = 1;
            p.yradius = 2;
            p.eyeheight = 1;
            p.o = vec(0,0,0);
            p.yaw = 0;
            mpr::EntOBB e(&p);
            float f = e.front();
            float b = e.back();
            assert(f == -b);
            assert(f == 2.f);
        }
    }

    void test_entobb_top()
    {
        std::printf("testing entobb::top\n");
        {
            physent p;
            p.aboveeye = 1;
            p.o = vec(0,0,0);
            p.yaw = 0;
            mpr::EntOBB e(&p);
            float f = e.top();
            assert(f == 1.f);
        }
    }

    void test_entobb_bottom()
    {
        std::printf("testing entobb::bottom\n");
        {
            physent p;
            p.eyeheight = 1;
            p.o = vec(0,0,0);
            p.yaw = 0;
            mpr::EntOBB e(&p);
            float f = e.bottom();
            assert(f == -1.f);
        }
    }

    void test_entfuzzy_ctor()
    {
        std::printf("testing entfuzzy::entfuzzy\n");
        physent p;
        mpr::EntFuzzy e(&p);
        assert(e.ent == &p);
        assert(e.ent != nullptr);
    }

    void test_entfuzzy_left()
    {
        std::printf("testing entfuzzy::left\n");
        {
            physent p;
            p.radius = 1;
            p.o = vec(0,0,0);
            mpr::EntFuzzy e(&p);
            float f = e.left();
            assert(f == -1.f);
        }
    }

    void test_entfuzzy_right()
    {
        std::printf("testing entfuzzy::right\n");
        {
            physent p;
            p.radius = 1;
            p.o = vec(0,0,0);
            mpr::EntFuzzy e(&p);
            float f = e.right();
            assert(f == 1.f);
        }
    }

    void test_entfuzzy_front()
    {
        std::printf("testing entfuzzy::front\n");
        {
            physent p;
            p.radius = 1;
            p.o = vec(0,0,0);
            mpr::EntFuzzy e(&p);
            float f = e.front();
            assert(f == 1.f);
        }
    }

    void test_entfuzzy_back()
    {
        std::printf("testing entfuzzy::back\n");
        {
            physent p;
            p.radius = 1;
            p.o = vec(0,0,0);
            mpr::EntFuzzy e(&p);
            float f = e.back();
            assert(f == -1.f);
        }
    }

    void test_entfuzzy_bottom()
    {
        std::printf("testing entfuzzy::bottom\n");
        {
            physent p;
            p.eyeheight = 1;
            p.o = vec(0,0,0);
            mpr::EntFuzzy e(&p);
            float f = e.bottom();
            assert(f == -1.f);
        }
        {
            physent p;
            p.eyeheight = 1;
            p.o = vec(0,0,2);
            mpr::EntFuzzy e(&p);
            float f = e.bottom();
            assert(f == 1.f);
        }
    }

    void test_entfuzzy_top()
    {
        std::printf("testing entfuzzy::top\n");
        {
            physent p;
            p.aboveeye = 1;
            p.o = vec(0,0,0);
            mpr::EntFuzzy e(&p);
            float f = e.top();
            assert(f == 1.f);
        }
        {
            physent p;
            p.aboveeye = 1;
            p.o = vec(0,0,1);
            mpr::EntFuzzy e(&p);
            float f = e.top();
            assert(f == 2.f);
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
    test_solidcube_center();
    test_solidcube_supportpoint();
    test_ent_ctor();
    test_ent_center();
    test_entobb_supportpoint();
    test_entobb_left_right();
    test_entobb_front_back();
    test_entobb_top();
    test_entobb_bottom();
    test_entfuzzy_ctor();
    test_entfuzzy_left();
    test_entfuzzy_right();
    test_entfuzzy_front();
    test_entfuzzy_back();
    test_entfuzzy_bottom();
    test_entfuzzy_top();
}


#include "../src/libprimis-headers/cube.h"
#include "../src/shared/geomexts.h"

#include <memory>
#include <optional>

#include "../src/engine/interface/console.h"
#include "../src/engine/interface/control.h"

#include "../src/engine/render/rendergl.h"

#include "../src/engine/world/entities.h"
#include "../src/engine/world/octaworld.h"
#include "../src/engine/world/physics.h"
#include "../src/engine/world/bih.h"

#include "../src/engine/model/model.h"
#include "../src/engine/model/ragdoll.h"

namespace
{
    constexpr float tolerance = 0.001;

    void test_ragdollskel_tri_shareverts()
    {
        std::printf("testing ragdollskel::tri::shareverts\n");

        ragdollskel::tri t1 = {0,1,2},
                         t2 = {2,3,4},
                         t3 = {10,11,12};
        assert(t1.shareverts(t1));
        assert(t1.shareverts(t2));
        assert(t1.shareverts(t3) == false);
        assert(t2.shareverts(t3) == false);
    }

    void test_ragdollskel_setup()
    {
        std::printf("testing ragdollskel::setup\n");

        ragdollskel s;
        s.tris.push_back({0,1,2});
        s.tris.push_back({2,3,4});

        s.verts.push_back({vec(0,0,0), 0, 0});
        s.verts.push_back({vec(1,0,0), 0, 0});
        s.verts.push_back({vec(0,1,0), 0, 0});

        s.joints.push_back({0, 0, {0,1,2}, 0.f, matrix4x3()});
        s.setup();

        //setupjoints check
        assert(s.joints.at(0).orient.a.sub(vec(1,0,0)).magnitude() < tolerance);
        assert(s.joints.at(0).orient.b.sub(vec(0,1,0)).magnitude() < tolerance);
        assert(s.joints.at(0).orient.c.sub(vec(0,0,1)).magnitude() < tolerance);

        //setuprotfrictions check
        assert(s.rotfrictions.size() == 1);
        assert(s.rotfrictions.at(0).tri[0] == 0);
        assert(s.rotfrictions.at(0).tri[1] == 1);

        assert(s.loaded == true);
    }

    void test_ragdollskel_addreljoint()
    {
        std::printf("testing ragdollskel::addreljoint\n");

        ragdollskel s;

        s.addreljoint(1,2);
        assert(s.reljoints.size() == 1);
        assert(s.reljoints.at(0).bone == 1);
        assert(s.reljoints.at(0).parent == 2);
    }

    void test_ragdolldata_calcanimjoint()
    {
        std::printf("testing ragdolldata::calcanimjoint\n");

        ragdollskel s;
        s.tris.push_back({0,1,2});
        s.tris.push_back({2,3,4});

        s.verts.push_back({vec(0,0,0), 0, 0});
        s.verts.push_back({vec(1,0,0), 0, 0});
        s.verts.push_back({vec(0,1,0), 0, 0});

        s.joints.push_back({0, 0, {0,1,2}, 0.f, matrix4x3()});

        ragdolldata r(&s);

        r.calcanimjoint(0, matrix4x3());
        r.animjoints = new matrix4x3[1];

        assert(r.animjoints[0].a.magnitude() < tolerance);
        assert(r.animjoints[0].b.magnitude() < tolerance);
        assert(r.animjoints[0].c.magnitude() < tolerance);
        assert(r.animjoints[0].d.magnitude() < tolerance);
    }

    void test_ragdolldata_init()
    {
        std::printf("testing ragdoll init\n");
        ragdollskel s;
        ragdolldata r(&s);
        r.verts.push_back(ragdolldata::vert());
        r.verts.back().pos = vec(0,0,0);
        r.verts.push_back(ragdolldata::vert());
        r.verts.back().pos = vec(1,0,0);

        dynent d;
        d.o = vec(0,0,0);
        d.vel = vec(0,0,0);
        d.falling = vec(0,0,0);
        d.eyeheight = 0;
        d.aboveeye = 0;

        r.init(&d);

        assert(r.center.sub(vec(0.5,0,0)).magnitude() < tolerance);
        assert(std::abs(r.radius - 0.5) < tolerance);
    }

    void test_cleanragdoll()
    {
        std::printf("testing cleanragdoll\n");
        dynent d;
        ragdollskel s;
        ragdolldata *r = new ragdolldata(&s);

        d.ragdoll = r;
        cleanragdoll(&d);
        assert(d.ragdoll == nullptr);
    }
}

void test_ragdoll()
{
    std::printf(
"===============================================================\n\
testing ragdoll functionality\n\
===============================================================\n"
    );

    test_ragdollskel_tri_shareverts();
    test_ragdollskel_setup();
    test_ragdollskel_addreljoint();
    test_ragdolldata_calcanimjoint();
    test_ragdolldata_init();
    test_cleanragdoll();
}


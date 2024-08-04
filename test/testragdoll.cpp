
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

void test_init()
{
    std::printf("testing ragdoll init\n");
    ragdollskel s;
    s.verts.push_back({vec(0,0,0), 0, 0});
    s.verts.push_back({vec(1,0,0), 0, 0});
    ragdolldata r(&s);

    dynent d;
    d.o = vec(0,0,0);
    d.vel = vec(0,0,0);
    d.falling = vec(0,0,0);
    d.eyeheight = 0;
    d.aboveeye = 0;

    r.init(&d);

    assert(r.center == vec(0.5, 0, 0));
    assert(r.radius == 0.5);
}

void test_ragdoll()
{
    std::printf(
"===============================================================\n\
testing ragdoll functionality\n\
===============================================================\n"
    );

    test_init();
}


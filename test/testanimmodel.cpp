
#include "../src/libprimis-headers/cube.h"

#include "../src/shared/geomexts.h"
#include "../src/shared/glexts.h"

#include <optional>
#include <memory>

#include "../src/engine/interface/console.h"
#include "../src/engine/interface/cs.h"

#include "../src/engine/render/rendergl.h"
#include "../src/engine/render/rendermodel.h"
#include "../src/engine/render/shader.h"
#include "../src/engine/render/shaderparam.h"
#include "../src/engine/render/texture.h"

#include "../src/engine/world/entities.h"
#include "../src/engine/world/bih.h"

#include "../src/engine/model/model.h"
#include "../src/engine/model/ragdoll.h"
#include "../src/engine/model/animmodel.h"

namespace
{
    void test_animstate_equals()
    {
        std::printf("testing animstate operator==\n");
        animmodel::AnimPos p{1,1,1,1.f};
        animmodel::AnimState s1{nullptr, p, p, 1.f};
        animmodel::AnimState s2{nullptr, p, p, 0.f};
        assert(s1 == s1);
        assert(!(s2 == s1));
        assert(!(s1 == s2));
    }

    void test_animstate_nequals()
    {
        std::printf("testing animstate operator!=\n");
        animmodel::AnimPos p{1,1,1,1.f};
        animmodel::AnimState s1{nullptr, p, p, 1.f};
        animmodel::AnimState s2{nullptr, p, p, 0.f};
        assert(!(s1 != s1));
        assert(s2 != s1);
        assert(s1 != s2);
    }

    void test_animpos_equals()
    {
        std::printf("testing animpos operator==\n");
        animmodel::AnimPos p1{1,1,1,1.f};
        animmodel::AnimPos p2{1,1,1,1.f};
        animmodel::AnimPos p3{1,2,3,4.f};
        assert(p1 == p1);
        assert(p1 == p2);
        assert(!(p1 == p3));
    }

    void test_animpos_nequals()
    {
        std::printf("testing animpos operator==\n");
        animmodel::AnimPos p1{1,1,1,1.f};
        animmodel::AnimPos p2{1,1,1,1.f};
        animmodel::AnimPos p3{1,2,3,4.f};
        assert(!(p1 != p1));
        assert(!(p1 != p2));
        assert((p1 != p3));
    }
}

void test_animmodel()
{
    std::printf(
"===============================================================\n\
testing animmodel functionality\n\
===============================================================\n"
    );
    test_animstate_equals();
    test_animstate_nequals();
    test_animpos_equals();
    test_animpos_nequals();
};

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
#include "../src/engine/model/skelmodel.h"

constexpr float tolerance = 0.001;

struct MinimalSkelModel : skelmodel
{
    MinimalSkelModel(std::string name) : skelmodel(name)
    {
    }

    int type() const final
    {
        return 0;
    }

    bool flipy() const final
    {
        return false;
    }

    bool loadconfig(const std::string &) final
    {
        return false;
    }

    bool loaddefaultparts() final
    {
        return false;
    }

    void startload() final
    {
    }

    void endload() final
    {
    }

    skelmeshgroup *newmeshes() final
    {
        return nullptr;
    }
};

void test_skel_ctor()
{
    std::printf("constructing a skelmodel object\n");
    MinimalSkelModel s = MinimalSkelModel(std::string("test"));
    assert(s.modelname() == "test");
}

void test_blendcombo_equals()
{
    std::printf("testing blendcombo operator==\n");

    skelmodel::blendcombo::BoneData b1 = { 1.f, 1, 2 },
                                    b2 = { 2.f, 3, 4 },
                                    b3 = { 2.f, 3, 5 };

    {
        //test trivial inequality
        skelmodel::blendcombo a,
                              b;
        a.bonedata.fill(b1);
        b.bonedata.fill(b2);
        assert(!(a == b));
    }
    {
        //test trivial equality
        skelmodel::blendcombo a,
                              b;
        a.bonedata.fill(b1);
        b.bonedata.fill(b1);
        assert(a == b);
    }
    {
        //test that interpbones value does not matter
        skelmodel::blendcombo a,
                              b;
        a.bonedata.fill(b2);
        b.bonedata.fill(b3);
        assert(a == b);
    }
    {
        //test that uses/interpindex does not matter
        skelmodel::blendcombo a,
                              b;
        a.bonedata.fill(b3);
        a.uses = 2;
        a.interpindex = 3;
        b.bonedata.fill(b3);
        a.uses = 4;
        a.interpindex = 5;
        assert(a == b);
    }
}

void test_skelmesh_assignvert()
{
    std::printf("testing skelmesh assignvert\n");

    //skelmesh::assignvert(vvertg, vert)
    {
        skelmodel::vert v = { {0,0,0}, {1,0,0}, {1,1}, {0,0,0,1}, 1, 2 };
        skelmodel::vvertg vv;
        skelmodel::skelmesh::assignvert(vv, v);
        assert(vv.pos.x.val == 0);
        assert(vv.pos.y.val == 0);
        assert(vv.pos.z.val == 0);
        assert(vv.pos.w.val == 15360); // == 1
        assert(vv.tc.x.val == 15360);
        assert(vv.tc.y.val == 15360);
        squat s = squat(quat(0,0,0,1));
        assert(vv.tangent.x == s.x);
        assert(vv.tangent.y == s.y);
        assert(vv.tangent.z == s.z);
        assert(vv.tangent.w == s.w);
    }
    //skelmesh::assignvert(vvertgw, vert, blendcombo)
    {
        skelmodel::blendcombo c;
        skelmodel::vert v = { {0,0,0}, {1,0,0}, {1,1}, {0,0,0,1}, 1, 2 };
        skelmodel::vvertgw vv;
        skelmodel::skelmesh::assignvert(vv, v, c);
        assert(vv.pos.x.val == 0);
        assert(vv.pos.y.val == 0);
        assert(vv.pos.z.val == 0);
        assert(vv.pos.w.val == 15360); // == 1
        assert(vv.tc.x.val == 15360);
        assert(vv.tc.y.val == 15360);
        squat s = squat(quat(0,0,0,1));
        assert(vv.tangent.x == s.x);
        assert(vv.tangent.y == s.y);
        assert(vv.tangent.z == s.z);
        assert(vv.tangent.w == s.w);
    }
}

void test_skelmesh_fillvert()
{
    std::printf("testing skelmesh fillvert\n");

    skelmodel::vert v = { {0,0,0}, {1,0,0}, {1,1}, {0,0,0,1}, 1, 2 };
    skelmodel::vvert vv;
    skelmodel::skelmesh::fillvert(vv, v);
    assert(vv.tc.x.val == 15360);
    assert(vv.tc.y.val == 15360);
}

void test_skel()
{
    std::printf(
"===============================================================\n\
testing skelmodel functionality\n\
===============================================================\n"
    );

    test_skel_ctor();

    test_blendcombo_equals();

    test_skelmesh_assignvert();
    test_skelmesh_fillvert();
}

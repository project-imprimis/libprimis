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

void test_blendcombo_size()
{
    std::printf("testing blendcombo size\n");

    skelmodel::blendcombo::BoneData b1 = { 1.f, 0, 0 },
                                    b2 = { 0.f, 0, 0 };
    {
        //all weights
        skelmodel::blendcombo a;
        a.bonedata.fill(b1);
        assert(a.size() == 4);
    }
    {
        //three weights
        skelmodel::blendcombo a;
        a.bonedata.fill(b1);
        a.bonedata[3] = b2;
        assert(a.size() == 3);
    }
    {
        //test that size only counts to first null
        skelmodel::blendcombo a;
        a.bonedata.fill(b1);
        a.bonedata[2] = b2;
        assert(a.size() == 2);
    }
    {
        //one weight
        skelmodel::blendcombo a;
        a.bonedata.fill(b2);
        a.bonedata[0] = b1;
        assert(a.size() == 1);
    }
    {
        //empty weights, test returning min of 1
        skelmodel::blendcombo a;
        a.bonedata.fill(b2);
        assert(a.size() == 1);
    }
}

void test_blendcombo_sortcmp()
{
    std::printf("testing blendcombo sortcmp\n");

    skelmodel::blendcombo::BoneData b1 = { 1.f, 0, 0 },
                                    b2 = { 0.f, 0, 0 };
    {
        //compare self, ensure false
        skelmodel::blendcombo a,
                              b;
        a.bonedata.fill(b1);
        b.bonedata.fill(b1);
        assert(skelmodel::blendcombo::sortcmp(a,b) == false);
    }
    {
        //make a[3] zero
        skelmodel::blendcombo a,
                              b;
        a.bonedata.fill(b1);
        a.bonedata[3] = b2;
        b.bonedata.fill(b1);
        assert(skelmodel::blendcombo::sortcmp(a,b) == false);
    }
    {
        //make b[3] zero
        skelmodel::blendcombo a,
                              b;
        a.bonedata.fill(b1);
        b.bonedata.fill(b1);
        b.bonedata[3] = b2;
        assert(skelmodel::blendcombo::sortcmp(a,b));
    }
}

/**
 * @brief Attempts to assign a weight to one of the bonedata slots.
 *
 * Attempts to add the passed weight/bone combination to this blendcombo. If the
 * weight passed is less than 1e-3, the weight will not be added regardless of the
 * status of the object.
 *
 * If a weight/bone combo with a weight larger than any of the existing members of the
 * object are stored, the smaller weights are shifted (and the smallest one removed) to
 * make space for it.
 *
 * The returned value sorted indicates the depth into the object at which the object
 * should attempt to add an element. If an element is successfully added, and if the
 * bone data is not filled, then sorted is returned incremented by one. Otherwise, the
 * same value passed as sorted will be returned.
 *
 * @param sorted the depth to add a weight in this object
 * @param weight the weight to attempt to add
 * @param bone the corresponding bone index
 *
 * @return the resulting number of allocated weights
 */
void test_blendcombo_addweight()
{
    std::printf("testing blendcombo addweight\n");

    {
        //test value below minimum weight
        skelmodel::blendcombo a;
        int sorted = 0;
        sorted = a.addweight(sorted, 0.9e-3, 1);
        assert(sorted == 0);
        assert(a.bonedata[0].weights == 0.f);
        assert(a.bonedata[0].bones == 0);
    }
    {
        //test adding single value
        skelmodel::blendcombo a;
        int sorted = 0;
        sorted = a.addweight(sorted, 1.f, 1);
        assert(sorted == 1);
        assert(a.bonedata[0].weights == 1.f);
        assert(a.bonedata[0].bones == 1);
    }
    {
        //test failing to add value, all existing bones larger
        skelmodel::blendcombo a;
        a.bonedata.fill({2.f, 1, 2});
        int sorted = 4;
        sorted = a.addweight(sorted, 1.f, 1);
        assert(sorted == 4);
        assert(a.bonedata[0].weights == 2.f);
        assert(a.bonedata[0].bones == 1);
    }
    {
        //test adding new entry to appropriate spot
        skelmodel::blendcombo a;
        a.bonedata[0] = {4.f, 4, 0};
        a.bonedata[1] = {3.f, 3, 0};
        a.bonedata[2] = {2.f, 2, 0};
        a.bonedata[3] = {1.f, 1, 0};
        int sorted = 4;
        sorted = a.addweight(sorted, 2.5f, 5);
        assert(sorted == 4);
        assert(a.bonedata[2].weights == 2.5f);
        assert(a.bonedata[2].bones == 5);
    }
    {
        //test adding enty with small sorted value
        skelmodel::blendcombo a;
        a.bonedata[0] = {4.f, 4, 0};
        a.bonedata[1] = {3.f, 3, 0};
        a.bonedata[2] = {2.f, 2, 0};
        a.bonedata[3] = {1.f, 1, 0};
        int sorted = 0;
        sorted = a.addweight(sorted, 2.5f, 5);
        assert(sorted == 1);
        //check that weight was added to bonedata[sorted]
        assert(a.bonedata[sorted-1].weights == 2.5f);
        assert(a.bonedata[sorted-1].bones == 5);
        //check that ordinary position if sorted >2 was not affected
        assert(a.bonedata[2].weights == 2.f);
        assert(a.bonedata[2].bones == 2);
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

    test_blendcombo_addweight();
    test_blendcombo_equals();
    test_blendcombo_sortcmp();
    test_blendcombo_size();

    test_skelmesh_assignvert();
    test_skelmesh_fillvert();
}

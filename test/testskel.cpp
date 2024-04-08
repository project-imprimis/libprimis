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
    {
        //pass two empty bonedatas
        skelmodel::blendcombo a,
                              b;
        a.bonedata.fill(b2);
        b.bonedata.fill(b2);
        assert(skelmodel::blendcombo::sortcmp(a,b) == false);
    }
}

void test_blendcombo_addweight()
{
    std::printf("testing blendcombo addweight\n");

    {
        //test value below minimum weight
        skelmodel::blendcombo a;
        int sorted = 0;
        sorted = a.addweight(sorted, 0.9e-3, 1);
        assert(sorted == 0);
        assert(a.bonedata[0].weight == 0.f);
        assert(a.getbone(0) == 0);
    }
    {
        //test adding single value
        skelmodel::blendcombo a;
        int sorted = 0;
        sorted = a.addweight(sorted, 1.f, 1);
        assert(sorted == 1);
        assert(a.bonedata[0].weight == 1.f);
        assert(a.getbone(0) == 1);
    }
    {
        //test failing to add value, all existing bones larger
        skelmodel::blendcombo a;
        a.bonedata.fill({2.f, 1, 2});
        int sorted = 4;
        sorted = a.addweight(sorted, 1.f, 1);
        assert(sorted == 4);
        assert(a.bonedata[0].weight == 2.f);
        assert(a.getbone(0) == 1);
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
        assert(a.bonedata[2].weight == 2.5f);
        assert(a.getbone(2) == 5);
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
        assert(a.bonedata[sorted-1].weight == 2.5f);
        assert(a.bonedata[sorted-1].bone == 5);
        //check that ordinary position if sorted >2 was not affected
        assert(a.bonedata[2].weight == 2.f);
        assert(a.getbone(2) == 2);
    }
}

void test_blendcombo_finalize()
{
    std::printf("testing blendcombo finalize\n");

    skelmodel::blendcombo::BoneData b1 = { 0.5f, 0, 0 },
                                    b2 = { 0.f, 0, 0 };

    {
        //test normalization of same value in all four channels
        skelmodel::blendcombo a;
        a.bonedata.fill(b1);
        a.finalize(4);
        assert(a.bonedata[0].weight == 0.25f);
    }
    {
        //test normalization of single value
        skelmodel::blendcombo a;
        a.bonedata.fill(b2);
        a.bonedata[0] = b1;
        a.finalize(4);
        assert(a.bonedata[0].weight == 1.f);
    }
    {
        //test normalization of finalize with sorted < size of array
        skelmodel::blendcombo a;
        a.bonedata.fill(b1);
        a.finalize(2);
        assert(a.bonedata[0].weight - 0.5f < tolerance);
    }
    {
        //test no effect if sorted = 0
        skelmodel::blendcombo a;
        a.bonedata.fill(b1);
        a.finalize(0);
        assert(a.bonedata[0].weight - 0.5f < tolerance);
    }
}

void test_blendcombo_serialize()
{
    std::printf("testing blendcombo serialize\n");

    skelmodel::blendcombo::BoneData b1 = { 0.5f, 0, 0 },
                                    b2 = { 0.1f, 0, 1 },
                                    b3 = { 0.f,  0, 0 };

    {
        //test behavior when interpindex >=0
        skelmodel::vvertgw vv;
        skelmodel::blendcombo a;
        a.interpindex = 1;
        a.serialize(vv);
        assert(vv.weights[0] == 255);
        assert(vv.weights[1] == 0);
        assert(vv.weights[2] == 0);
        assert(vv.weights[3] == 0);
        for(size_t i = 0; i < 4; ++i)
        {
            assert(vv.bones[i] == 2);
        }
    }
    {
        //test values that sum >1
        skelmodel::vvertgw vv;
        skelmodel::blendcombo a;
        a.bonedata.fill(b1);
        a.interpindex = -1;
        a.serialize(vv);
        for(size_t i = 0; i < 4; ++i)
        {
            assert(vv.weights[i] == 63 || vv.weights[i] == 64); //either is okay and ~=0.25, +/-1 to sum to 255
        }
    }
    {
        //test array of differing values { 0.1, 0.5, 0.5, 0.5 }
        skelmodel::vvertgw vv;
        skelmodel::blendcombo a;
        a.bonedata.fill(b1);
        a.bonedata[0] = b2;
        a.interpindex = -1;
        a.serialize(vv);
        assert(vv.weights[0] == 0);
        for(size_t i = 1; i < 4; ++i)
        {
            assert(vv.weights[i] == 85);
        }
    }
    {
        //test array of differing values { 0.5, 0, 0, 0 }
        skelmodel::vvertgw vv;
        skelmodel::blendcombo a;
        a.bonedata.fill(b3);
        a.bonedata[0] = b1;
        a.interpindex = -1;
        a.serialize(vv);
        assert(vv.weights[0] == 160);
        for(size_t i = 1; i < 4; ++i)
        {
            assert(vv.weights[i] == 32 || vv.weights[i] == 31); //either is okay and ~=0.25, +/-1 to sum to 255
        }
    }
    {
        //test values that sum <1
        skelmodel::vvertgw vv;
        skelmodel::blendcombo a;
        a.bonedata.fill(b2);
        a.interpindex = -1;
        a.serialize(vv);
        for(size_t i = 0; i < 4; ++i)
        {
            assert(vv.weights[i] == 63 || vv.weights[i] == 64); //either is okay and ~=0.25, +/-1 to sum to 255
        }
    }
    {
        //test values assigned from interpbones
        skelmodel::vvertgw vv;
        skelmodel::blendcombo a;
        a.bonedata.fill(b2);
        a.interpindex = -1;
        a.serialize(vv);
        for(size_t i = 0; i < 4; ++i)
        {
            assert(vv.bones[i] == 2);
        }
    }
}

void test_blendcombo_blendbones()
{
    std::printf("testing blendcombo blendbones\n");

    std::array<skelmodel::blendcombo::BoneData, 4> bd = {{ { 0.4f, 0, 0 },
                                                           { 0.3f, 0, 1 },
                                                           { 0.2f, 0, 2 },
                                                           { 0.1f, 0, 3 }
                                                        }};
    std::array<dualquat, 4> dqs = { dualquat({0,0,0,1}), dualquat({1,0,0,0}), dualquat({0,1,0,0}), dualquat({0,0,1,0}) };
    {
        //test blending independent vectors
        skelmodel::blendcombo a;
        a.bonedata = bd;
        dualquat d = a.blendbones(dqs.data());
        assert(d.real.sub(vec4<float>(0.3,0.2,0.1,0.4)).magnitude() < tolerance);
        assert(d.dual.magnitude() < tolerance);
    }
    {
        //test blending <4 vectors, after normalization
        skelmodel::blendcombo a;
        a.bonedata = bd;
        a.bonedata[0] = {0,0,0};
        a.bonedata[3] = {0,0,3};
        a.finalize(4);
        dualquat d = a.blendbones(dqs.data());
        assert(d.real.sub(vec4<float>(0.6,0.4,0,0)).magnitude() < tolerance);
        assert(d.dual.magnitude() < tolerance);
    }
}

void test_blendcombo_remapblend()
{
    std::printf("testing blendcombo remapblend\n");

    std::array<skelmodel::blendcombo::BoneData, 4> bd = {{ { 0.4f, 0, 0 },
                                                           { 0.3f, 0, 1 },
                                                           { 0.2f, 0, 2 },
                                                           { 0.1f, 0, 3 }
                                                        }};

    {
        //test if multiple bonedata, bonedata::interpindex returned
        skelmodel::blendcombo a;
        a.bonedata = bd;
        a.interpindex = 5;
        assert(a.remapblend() == 5);
    }
    {
        //test if only one bonedata, that bonedata's interpindex returned
        skelmodel::blendcombo a;
        a.bonedata.fill({0,0,0});
        a.bonedata[0] = {0.4f, 0, 1};
        a.interpindex = 5;
        assert(a.remapblend() == 1);
    }
}

void test_blendcombo_setinterpindex()
{
    std::printf("testing blendcombo setinterpindex\n");

    std::array<skelmodel::blendcombo::BoneData, 4> bd = {{ { 0.4f, 0, 0 },
                                                           { 0.3f, 0, 1 },
                                                           { 0.2f, 0, 2 },
                                                           { 0.1f, 0, 3 }
                                                        }};

    skelmodel::blendcombo a;
    a.bonedata = bd;
    a.setinterpindex(1);
    assert(a.remapblend() == 1);
}

void test_blendcombo_getbone()
{
    std::printf("testing blendcombo getbone\n");

    skelmodel::blendcombo a;
    a.addweight(4, 0.5f, 1);
    assert(a.getbone(0) == 1);
    a.addweight(4, 0.7f, 2);
    assert(a.getbone(0) == 2);
    assert(a.getbone(1) == 1);
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

void test_skelmesh_buildnorms()
{
    std::printf("testing skelmesh buildnorms\n");

    //test buildnorms(true)
    {
        skelmodel::vert *verts = new skelmodel::vert[4];

        verts[0].pos = vec(1,0,0);
        verts[1].pos = vec(0,1,1);
        verts[2].pos = vec(0,-1,1);
        verts[3].pos = vec(-1,0,0);

        skelmodel::tri *tris = new skelmodel::tri[2];
        tris[0].vert[0] = 0;
        tris[0].vert[1] = 1;
        tris[0].vert[2] = 2;

        //vertex order so this tri faces up as well
        tris[1].vert[0] = 3;
        tris[1].vert[1] = 2;
        tris[1].vert[2] = 1;

        skelmodel::skelmesh mesh("test", verts, 4, tris, 2);

        mesh.buildnorms(true);

        assert(mesh.verts[0].norm.sub(vec(std::sqrt(2)/2, 0, sqrt(2)/2)).magnitude() < tolerance);
        assert(mesh.verts[1].norm.sub(vec(0, 0, 1)).magnitude() < tolerance);
        assert(mesh.verts[2].norm.sub(vec(0, 0, 1)).magnitude() < tolerance);
        assert(mesh.verts[3].norm.sub(vec(-std::sqrt(2)/2, 0, sqrt(2)/2)).magnitude() < tolerance);
    }
    //test buildnorms(false), also switch vertex order
    {
        skelmodel::vert *verts = new skelmodel::vert[4];

        verts[0].pos = vec(1,0,0);
        verts[1].pos = vec(0,1,1);
        verts[2].pos = vec(0,-1,1);
        verts[3].pos = vec(-1,0,0);

        skelmodel::tri *tris = new skelmodel::tri[2];
        tris[0].vert[0] = 0;
        tris[0].vert[1] = 1;
        tris[0].vert[2] = 2;

        //vertex order so this tri faces up as well
        tris[1].vert[0] = 1;
        tris[1].vert[1] = 2;
        tris[1].vert[2] = 3;

        skelmodel::skelmesh mesh("test", verts, 4, tris, 2);

        mesh.buildnorms(false);

        assert(mesh.verts[0].norm.sub(vec(std::sqrt(2)/2, 0, sqrt(2)/2)).magnitude() < tolerance);
        assert(mesh.verts[1].norm.sub(vec(1, 0, 0)).magnitude() < tolerance);
        assert(mesh.verts[2].norm.sub(vec(1, 0, 0)).magnitude() < tolerance);
        assert(mesh.verts[3].norm.sub(vec(std::sqrt(2)/2, 0, -sqrt(2)/2)).magnitude() < tolerance);
    }
}

void test_skelmesh_calcbb()
{
    std::printf("testing skelmesh calcbb\n");

    skelmodel::vert *verts = new skelmodel::vert[4];

    verts[0].pos = vec(1,0,0);
    verts[1].pos = vec(0,1,1);
    verts[2].pos = vec(0,-1,1);
    verts[3].pos = vec(-1,0,0);
    skelmodel::skelmesh mesh("test", verts, 4, nullptr, 0);

    {
        matrix4x3 m;
        m.identity();

        vec min(0,0,0),
            max(0,0,0);

        mesh.calcbb(min, max, m);

        assert(min == vec(-1,-1,0));
        assert(max == vec(1,1,1));
    }
    {
        matrix4x3 m(vec(1,0,0), vec(0,5,0), vec(0,0,9), vec(1,2,3));

        vec min(99,99,99),
            max(-99,-99,-99);

        mesh.calcbb(min, max, m);

        assert(min == vec(0,-3,3));
        assert(max == vec(2,7,12));
    }
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
    test_blendcombo_finalize();
    test_blendcombo_serialize();
    test_blendcombo_blendbones();
    test_blendcombo_remapblend();
    test_blendcombo_setinterpindex();
    test_blendcombo_getbone();

    test_skelmesh_assignvert();
    test_skelmesh_fillvert();
    test_skelmesh_buildnorms();
    test_skelmesh_calcbb();
}

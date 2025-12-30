
#include "../src/libprimis-headers/cube.h"

#include "../src/shared/geomexts.h"
#include "../src/shared/glexts.h"

#include "../src/engine/world/bih.h"


namespace
{
    constexpr float tolerance = 0.001;

    void test_bih_mesh_ctor()
    {
        std::printf("test bih::mesh ctor\n");

        BIH::mesh m;
        assert(m.numnodes == 0);
        assert(m.numtris == 0);
        assert(m.tex == nullptr);
        assert(m.flags == 0);
    }

    void test_bih_mesh_setmesh()
    {
        std::printf("test bih::mesh setmesh\n");

        BIH::mesh m;

        m.setmesh(nullptr, 1, nullptr, 1, nullptr, 1);

        assert(m.tris == nullptr);
        assert(m.numtris == 1);

        m.setmesh(nullptr, 2, nullptr, 1, nullptr, 1);

        assert(m.numtris == 2);
    }

    void test_bih_mesh_invxform()
    {
        std::printf("test bih::mesh invxform\n");
        {
            BIH::mesh bm;
            matrix4x3 m = bm.xform;
            bm.xform.identity();
            m = bm.invxform();
            assert(m.a.sub(vec(1,0,0)).magnitude() < tolerance);
            assert(m.b.sub(vec(0,1,0)).magnitude() < tolerance);
            assert(m.c.sub(vec(0,0,1)).magnitude() < tolerance);
            assert(m.d.magnitude() < tolerance);
        }
        {
            BIH::mesh bm;
            matrix4x3 m = bm.xform;
            bm.xform = matrix4x3({1,1,1}, {2,2,2}, {3,3,3}, {0,0,0});
            m = bm.invxform();
            vec inv(1.f/3, 1.f/6, 1.f/9);
            assert(m.a.sub(inv).magnitude() < tolerance);
            assert(m.b.sub(inv).magnitude() < tolerance);
            assert(m.c.sub(inv).magnitude() < tolerance);
            assert(m.d.magnitude() < tolerance);
        }
    }

    void test_bih_mesh_tribb_outside()
    {
        std::printf("test bih::mesh::tribb outside");
        {
            BIH::mesh::tribb b;
            b.center = svec(0,0,0);
            b.radius = svec(0,0,0);
            assert(b.outside(ivec(0,0,0), ivec(0,0,0)) == false);
        }
        {
            BIH::mesh::tribb b;
            b.center = svec(0,0,0);
            b.radius = svec(1,1,1);
            assert(b.outside(ivec(0,0,0), ivec(1,1,1)) == false);
        }
        {
            BIH::mesh::tribb b;
            b.center = svec(0,0,0);
            b.radius = svec(1,1,1);
            assert(b.outside(ivec(0,0,0), ivec(0,0,1)) == false);
        }
        {
            BIH::mesh::tribb b;
            b.center = svec(5,5,5);
            b.radius = svec(1,1,1);
            assert(b.outside(ivec(0,0,0), ivec(0,0,1)) == true);
        }
        {
            BIH::mesh::tribb b;
            b.center = svec(2,2,2);
            b.radius = svec(1,1,1);
            assert(b.outside(ivec(0,0,0), ivec(0,0,1)) == true);
        }
    }

    void test_bih_node_axis()
    {
        std::printf("test bih::node axis\n");

        {
            BIH::Node n;
            n.child[0] = 0b0010'0000'0000'0000;
            assert(n.axis() == 0);
        }
        {
            BIH::Node n;
            n.child[0] = 0b0100'0000'0000'0000;
            assert(n.axis() == 1);
        }
        {
            BIH::Node n;
            n.child[0] = 0b1100'0000'0000'0000;
            assert(n.axis() == 0b11);
            assert(n.axis() == 3);
        }
    }

    void test_bih_node_childindex()
    {
        std::printf("test bih::node childindex\n");

        {
            BIH::Node n;
            n.child[0] = 1;
            assert(n.childindex(0) == 1);
        }
        {
            BIH::Node n;
            n.child[0] = 3;
            assert(n.childindex(0) == 3);
        }
        {
            BIH::Node n;
            n.child[0] = 31;
            assert(n.childindex(0) == 31);
        }
        {
            BIH::Node n;
            n.child[0] = 0x3FFF;
            assert(n.childindex(0) == 0x3FFF);
            assert(n.childindex(0) == 0b11'1111'1111'1111);
            assert(n.childindex(0) == 16383);
        }
        {
            BIH::Node n;
            n.child[0] = 0x4000;
            assert(n.childindex(0) == 0x0);
            assert(n.childindex(0) == 0b0);
            assert(n.childindex(0) == 0);
        }
    }

    void test_bih_node_isleaf()
    {
        std::printf("test bih::node isleaf\n");
        {
            BIH::Node n;
            n.child[1] = 0b0010'0000'0000'0000;
            assert(n.isleaf(0) == false);
            assert(n.isleaf(1) == false);
        }
        {
            BIH::Node n;
            n.child[1] = 0b0100'0000'0000'0000;
            assert(n.isleaf(0) == true);
            assert(n.isleaf(1) == false);
        }
        {
            BIH::Node n;
            n.child[1] = 0b1000'0000'0000'0000;
            assert(n.isleaf(0) == false);
            assert(n.isleaf(1) == true);
        }
        {
            BIH::Node n;
            n.child[1] = 0b1100'0000'0000'0000;
            assert(n.isleaf(0) == true);
            assert(n.isleaf(1) == true);
        }
    }

}

void test_bih()
{
    std::printf(
"===============================================================\n\
testing bih functionality\n\
===============================================================\n"
    );
    test_bih_mesh_ctor();
    test_bih_mesh_setmesh();
    test_bih_mesh_invxform();
    test_bih_mesh_tribb_outside();
    test_bih_node_axis();
    test_bih_node_childindex();
    test_bih_node_isleaf();
};

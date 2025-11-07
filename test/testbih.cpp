
#include "../src/libprimis-headers/cube.h"

#include "../src/shared/geomexts.h"
#include "../src/shared/glexts.h"

#include "../src/engine/world/bih.h"


namespace
{

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
    test_bih_node_axis();
    test_bih_node_childindex();
    test_bih_node_isleaf();
};

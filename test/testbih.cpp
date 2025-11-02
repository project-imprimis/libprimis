
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

    void test_bih_node_axis()
    {
        std::printf("test bih::node axis\n");

        BIH::Node n;

        n.child[0] = 1;

        assert(n.axis() == 1>>14);
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
    test_bih_node_axis();
};

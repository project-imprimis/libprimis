
#include "libprimis.h"

#include "../src/engine/render/renderlights.h"

void testpacknode()
{
    std::printf("Testing packnode construction\n");
    PackNode satest(0, 0, 128, 128);
    {
        vec2 size = satest.dimensions();
        assert(size.x == 128 && size.y == 128);
    }
    assert(satest.availablespace() == 128);

    //allocated texture locations
    ushort tx = 0;
    ushort ty = 0;
    std::vector<std::pair<ushort, ushort>> allocated;
    for(int i = 0; i < 6; ++i)
    {
        satest.insert(tx, ty, 32, 64);
        allocated.push_back({tx, ty});
    }
    std::printf("Testing packnode insertion\n");
    satest.printchildren();
    //check that all generated shadowatlas allocations are at unique coordinates
    std::sort(allocated.begin(), allocated.end());
    assert(std::adjacent_find(allocated.begin(), allocated.end()) == allocated.end());

    std::printf("Testing packnode clear\n");
    satest.reset();
    assert(satest.availablespace() == 128);

    std::printf("Testing packnode resize functionality\n");
    satest.resize(256, 256);
    {
        vec2 size = satest.dimensions();
        assert(size.x == 256 && size.y == 256);
    }
    assert(satest.availablespace() == 256);
}

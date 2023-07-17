
#include "libprimis.h"

#include "../src/engine/render/renderlights.h"

void testpacknode()
{
    PackNode satest(0, 0, 128, 128);
    //allocated texture locations
    ushort tx = 0;
    ushort ty = 0;
    std::vector<std::pair<ushort, ushort>> allocated;
    for(int i = 0; i < 6; ++i)
    {
        satest.insert(tx, ty, 32, 64);
        allocated.push_back({tx, ty});
    }
    std::printf("tree 1:\n");
    satest.printchildren();
    //check that all generated shadowatlas allocations are at unique coordinates
    std::sort(allocated.begin(), allocated.end());
    assert(std::adjacent_find(allocated.begin(), allocated.end()) == allocated.end());
}


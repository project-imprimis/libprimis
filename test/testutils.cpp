#include "libprimis.h"

namespace header_tools
{
}
namespace header_geom
{
    void testgenericvec3()
    {
        GenericVec3<float> a(5,5,5);
        GenericVec3<float> b(5,5,5);
        GenericVec3<float> c = a+b;

        c = a-b;
        assert(c == vec(0,0,0));
    }

    void testvec3()
    {
        vec a(5,5,5);
        vec b(5,5,5);
        a.mul(2);
        a.mul(vec(2,2,3));
        a.add(vec(1,1,1));
        a.sub(vec(1,1,1));
        a.mul(2);
    }

    void testmod360()
    {
        int a = 460;
        a = mod360(a);
        assert(a == 100);

        a = -100;
        a = mod360(a);
        assert(a == 260);
    }

}

void testutils()
{
    header_geom::testgenericvec3();
    header_geom::testvec3();
    header_geom::testmod360();
}

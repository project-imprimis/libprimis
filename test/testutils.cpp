#include "libprimis.h"

namespace header_tools
{
    void testvector()
    {
        vector<int> a;
        assert(a.length() == 0);
        assert(a.empty());
        //check adding and removing a member
        a.add(1);
        assert(a.length() == 1);
        a.remove(0);
        assert(a.length() == 0);

        //check adding a large member (vector.put())
        const char * testtext = "large text",
                   * testtextstart = "l",
                   * testtextend   = "t";

        vector<char> b;
        b.put(testtext, 10);
        assert(b.length() == 10);
        assert(b[0] == *testtextstart);
        assert(b[9] == *testtextend);

        //setsize
        b.setsize(5);
        assert(b.length() == 5);
    }
}
namespace header_geom
{
    void testgenericvec3()
    {
        GenericVec3<float> a(5,5,5);
        GenericVec3<float> b(5,5,5);
        GenericVec3<float> c = a+b;

        c = a-b;

        bool d = a > b;
        d = a < b;
        d = a >= b;
        d = a <= b;
        d = a == b;
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

    void testcrc()
    {
        clearmapcrc();
        entitiesinoctanodes();
        getworldsize();
    }
}

void testutils()
{
    header_tools::testvector();
    header_geom::testgenericvec3();
    header_geom::testvec3();
    header_geom::testmod360();
    header_geom::testcrc();
}

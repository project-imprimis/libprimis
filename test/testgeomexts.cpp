
#include "libprimis.h"
#include "../src/shared/geomexts.h"

constexpr float tolerance = 0.001;

void test_triangle_add()
{
    std::printf("testing triangle add method\n");
    triangle t1 = triangle({1,1,1}, {2,2,2}, {3,3,3});
    assert(t1.add({2,2,2}) == triangle({3,3,3}, {4,4,4}, {5,5,5})); 
}

void test_triangle_sub()
{
    std::printf("testing triangle subtract method\n");
    triangle t1 = triangle({1,1,1}, {2,2,2}, {3,3,3});
    assert(t1.sub({2,2,2}) == triangle({-1,-1,-1}, {0,0,0}, {1,1,1})); 
}

void test_triangle_equals()
{
    std::printf("testing triangle equality operator\n");
    triangle t1 = triangle({1,1,1}, {2,2,2}, {3,3,3});
    triangle t2 = triangle({1,1,1}, {2,2,2}, {3,3,3});
    triangle t3 = triangle({0,0,0}, {2,2,2}, {3,3,3});
    assert(!(t1 == t3));
    assert(t1 == t2);
    assert(t1 == t1);
}

void test_geomexts()
{
    std::printf(
"===============================================================\n\
testing geometry extensions\n\
===============================================================\n"
    );
    test_triangle_add();
    test_triangle_sub();
    test_triangle_equals();
}

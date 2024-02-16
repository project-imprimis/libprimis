
#include "libprimis.h"

void test_matrix3_ctor()
{
    std::printf("testing matrix3 constructor\n");
    matrix3 m;
    assert(m.a == vec(0,0,0));
    assert(m.b == vec(0,0,0));
    assert(m.c == vec(0,0,0));
}

void test_matrix3_identity()
{
    std::printf("testing matrix3 identity\n");
    matrix3 m;
    m.identity();
    assert(m.a == vec(1,0,0));
    assert(m.b == vec(0,1,0));
    assert(m.c == vec(0,0,1));
}

void test_matrix4x3_identity()
{
    std::printf("testing matrix4x3 identity\n");
    matrix4x3 m;
    m.identity();
    assert(m.a == vec(1,0,0));
    assert(m.b == vec(0,1,0));
    assert(m.c == vec(0,0,1));
    assert(m.d == vec(0,0,0));
}

void test_matrix4_identity()
{
    std::printf("testing matrix4 identity\n");
    matrix4 m;
    m.identity();
    assert(m.a == vec4<float>(1,0,0,0));
    assert(m.b == vec4<float>(0,1,0,0));
    assert(m.c == vec4<float>(0,0,1,0));
    assert(m.d == vec4<float>(0,0,0,1));
}

void test_matrix()
{
    std::printf(
"===============================================================\n\
testing matrices\n\
===============================================================\n"
    );

    test_matrix3_ctor();
    test_matrix3_identity();

    test_matrix4x3_identity();

    test_matrix4_identity();
}

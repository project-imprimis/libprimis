
#include "libprimis.h"
#include "../shared/geomexts.h"
#include "../src/engine/world/octaworld.h"
#include "../src/engine/world/octacube.h"

namespace
{
    void testoctaboxoverlap()
    {
        std::printf("Testing octaboxoverlap, pure octants\n");
        uchar expected = 1;
        uchar test = octaboxoverlap({0,0,0}, 0, {-2,-2,-2}, {-1, -1, -1});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {2, -2,-2}, {3, -1, -1});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {-2, 2,-2}, {-1, 3, -1});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {2, 2,-2}, {3, 3, -1});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {-2, -2, 2}, {-1, -1, 3});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {2, -2, 2}, {3, -1, 3});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {-2, 2, 2}, {-1, -1, 3});
        assert(test == expected);
        expected *= 2;
        test = octaboxoverlap({0,0,0}, 0, {2,2,2}, {3, 3, 3});
        assert(test == expected);
        expected *= 2;

        std::printf("Testing octaboxoverlap, multiple octants\n");
        test = octaboxoverlap({0,0,0}, 0, {-2,-2,-2}, {1, -1, -1});
        assert(test == 3); //octants 0 and 1
        test = octaboxoverlap({0,0,0}, 0, {-2,-2,-2}, {1, 1, -1});
        assert(test == 15); //octants 0,1,2,3
        test = octaboxoverlap({0,0,0}, 0, {-2,-2,-2}, {1, 1, 1});
        assert(test == 255); //all octants
    }

    void testfamilysize()
    {
        std::printf("Testing cube familysize\n");
        cube c1;
        c1.children = newcubes();
        assert(familysize(c1) == 9);
        (*c1.children)[0].children = newcubes();
        assert(familysize(c1) == 17);
        assert(familysize((*c1.children)[0]) == 9);
        freeocta(c1.children);
    }

    void testgetcubevector()
    {
        std::printf("Testing getcubevector\n");
        cube c1;
        ivec p = {0,0,0};
        getcubevector(c1, 0, 1, 1, 1, p);
        assert(p == ivec(15, 0, 0));
    }

    void test_octadim()
    {
        std::printf("Testing octadim\n");

        assert(octadim(10) == 1024);
        assert(octadim(5) == 32);
        assert(octadim(1) == 2);
        assert(octadim(0) == 1);
    }

    void test_cube_isempty()
    {
        std::printf("Testing cube::isempty\n");

        cube c;
        c.faces[0] = 0;
        c.faces[1] = 0;
        c.faces[2] = 0;
        assert(c.isempty());
        c.faces[0] = 1;
        assert(!c.isempty());
    }

    void test_cube_issolid()
    {
        std::printf("Testing cube::issolid\n");
        cube c;
        c.faces[0] = facesolid;
        c.faces[1] = facesolid;
        c.faces[2] = facesolid;
        assert(c.issolid());
        c.faces[0] = 1;
        assert(!c.issolid());
    }

    void test_cube_calcmerges()
    {
        std::printf("Testing cube::calcmerges\n");
        {
            std::array<cube,8> c = newcubes(facesolid, 0)[0];
            c[0].children = newcubes(facesolid, 0);
            c[0].calcmerges();
            assert(c[0].faces[0] == facesolid);
            assert(c[0].faces[1] == facesolid);
            assert(c[0].faces[2] == facesolid);
        }
        {
            std::array<cube,8> c = newcubes(faceempty, 0)[0];
            c[0].children = newcubes(faceempty, 0);
            c[0].calcmerges();
            assert(c[0].faces[0] == faceempty);
            assert(c[0].faces[1] == faceempty);
            assert(c[0].faces[2] == faceempty);
        }
    }

    void test_selinfo_size()
    {
        std::printf("Testing selinfo::size\n");
        {
            selinfo sel;
            sel.s = ivec(1,2,3);
            assert(sel.size() == 6);
        }
        {
            selinfo sel;
            sel.s = ivec(1,1,1);
            assert(sel.size() == 1);
        }
        {
            selinfo sel;
            sel.s = ivec(0,1,1);
            assert(sel.size() == 0);
        }
    }

    void test_selinfo_us()
    {
        std::printf("Testing selinfo::us\n");

        selinfo sel;
        sel.s = ivec(1,2,3);
        sel.grid = 2;
        assert(sel.us(0) == 2);
        assert(sel.us(1) == 4);
        assert(sel.us(2) == 6);
        sel.grid = 5;
        assert(sel.us(0) == 5);
        assert(sel.us(1) == 10);
        assert(sel.us(2) == 15);
    }

    void test_selinfo_equals()
    {
        std::printf("Testing selinfo::operator==\n");
        selinfo s;
        s.o = ivec(1,1,1);
        s.s = s.o;
        s.grid = 1;
        s.orient = 1;
        selinfo s2 = s;
        selinfo s3;
        s3.o = ivec(0,0,0);

        assert(s == s2);
        assert(!(s3 == s2));
    }

    void test_block3_ctor()
    {
        std::printf("testing block3::blcok3\n");
        selinfo s;
        s.o = ivec(1,1,1);
        s.s = s.o;
        s.grid = 1;
        s.orient = 1;
        block3 b(s);
        assert(b.o == ivec(1,1,1));
        assert(b.s == ivec(1,1,1));
        assert(b.grid == 1);
        assert(b.orient == 1);
    }

    void test_block3_getcube()
    {
        block3 block;
        std::printf("testing block3::getcube\n");
        assert(std::distance(const_cast<const block3 *>(&block), reinterpret_cast<const block3 *>(block.getcube())) == 1);
    }

    void test_block3_c()
    {
        block3 block;
        std::printf("testing block3::c \n");
        assert(std::distance(&block, reinterpret_cast<block3 *>(block.c())) == 1);
    }

    void test_block3_size()
    {
        std::printf("Testing block3::size\n");
        {
            block3 block;
            block.s = ivec(1,2,3);
            assert(block.size() == 6);
        }
        {
            block3 block;
            block.s = ivec(1,1,1);
            assert(block.size() == 1);
        }
        {
            block3 block;
            block.s = ivec(0,1,1);
            assert(block.size() == 0);
        }
    }

    void test_editinfo_ctor()
    {
        std::printf("Testing editinfo ctor\n");
        editinfo e;
        assert(e.copy == nullptr);
    }

    void test_undoblock_block()
    {
        undoblock block;
        std::printf("testing undoblock::block\n");
        assert(std::distance(&block, reinterpret_cast<undoblock *>(block.block())) == 1);
    }

    void test_undoblock_ents()
    {
        undoblock block;
        std::printf("testing undoblock::block\n");
        assert(std::distance(&block, reinterpret_cast<undoblock *>(block.ents())) == 1);
    }

    void test_touchingface()
    {
        std::printf("testing touchingface\n");
        cube c;
        setcubefaces(c, 1);
        assert(!touchingface(c, 0));
        assert(!touchingface(c, 1));
        assert(!touchingface(c, 2));
        setcubefaces(c, 0);
        assert(touchingface(c, 0));
        assert(!touchingface(c, 1));
        assert(touchingface(c, 2));
    }

    void test_notouchingface()
    {
        std::printf("testing notouchingface\n");
        cube c;
        setcubefaces(c, 1);
        assert(!notouchingface(c, 0));
        assert(notouchingface(c, 1));
        assert(!notouchingface(c, 2));
        setcubefaces(c, 0);
        assert(!notouchingface(c, 0));
        assert(notouchingface(c, 1));
        assert(!notouchingface(c, 2));
    }
}

void test_octa()
{
    std::printf(
"===============================================================\n\
testing octa functionality\n\
===============================================================\n"
    );
    testoctaboxoverlap();
    testfamilysize();
    testgetcubevector();
    test_octadim();
    test_cube_isempty();
    test_cube_issolid();
    test_cube_calcmerges();
    test_selinfo_size();
    test_selinfo_us();
    test_selinfo_equals();
    test_block3_ctor();
    test_block3_getcube();
    test_block3_c();
    test_block3_size();
    test_editinfo_ctor();
    test_undoblock_block();
    test_undoblock_ents();
    test_touchingface();
    test_notouchingface();
}

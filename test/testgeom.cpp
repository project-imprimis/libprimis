#include "libprimis.h"
#include "../src/shared/geomexts.h"

constexpr float tolerance = 0.001;

////////////////////////////////////////////////////////////////////////////////
// float vec2 tests
////////////////////////////////////////////////////////////////////////////////

void test_vec2_ctor()
{
    std::printf("testing vec2 ctor\n");

    //vec2(float,float)
    {
        vec2 v(1,2);
        assert(v.x == 1);
        assert(v.y == 2);
    }
    //vec2(vec)
    {
        vec v(1,2,3);
        vec2 v2(v);
        assert(v2 == vec2(1,2));
    }
    //vec2(vec4<float>)
    {
        vec4<float> v(1,2,3,4);
        vec2 v2(v);
        assert(v2 == vec2(1,2));
    }
}

void test_vec2_bracket()
{
    std::printf("testing vec2 operator[]\n");
    {
        vec2 v(1,2);

        float &f1 = v[0],
              &f2 = v[1];
        assert(f1 == 1);
        assert(f2 == 2);
    }
    {
        const vec2 v(1,2);

        float f3 = v[0],
              f4 = v[1];
        assert(f3 == 1);
        assert(f4 == 2);
    }
}

void test_vec2_nequal()
{
    std::printf("testing vec2 operator!=\n");

    vec2 v(1,2),
         v2(2,2);

    assert( (v != v) == false);
    assert( v != v2);
    assert( v2 != v);
}

void test_vec2_iszero()
{
    std::printf("testing vec2 iszero\n");

    vec2 v1(0,0),
         v2(1,1);
    assert(v1.iszero() == true);
    assert(v2.iszero() == false);
}

void test_vec2_dot()
{
    std::printf("testing vec2 dot\n");
    vec2 v1(1,2),
         v2(3,4);
    assert(v1.dot(v2) == 11);
}

void test_vec2_squaredlen()
{
    std::printf("testing vec2 squaredlen\n");
    vec2 v1(0,0),
         v2(1,1),
         v3(1,2);

    assert(v1.squaredlen() == 0);
    assert(v2.squaredlen() == 2);
    assert(v3.squaredlen() == 5);
}

void test_vec2_magnitude()
{
    std::printf("testing vec2 magnitude\n");

    vec2 v1(0,0),
         v2(3,4),
         v3(5,12);

    assert(v1.magnitude() == 0);
    assert(v2.magnitude() == 5);
    assert(v3.magnitude() == 13);
}

void test_vec2_normalize()
{
    std::printf("testing vec2 normalize\n");

    vec2 v1(1,0),
         v2(3,4),
         v3(5,12);

    assert(std::abs(v1.normalize().magnitude() - 1) < tolerance);
    assert(std::abs(v2.normalize().magnitude() - 1) < tolerance);
    assert(std::abs(v3.normalize().magnitude() - 1) < tolerance);
}

void test_vec2_safenormalize()
{
    std::printf("testing vec2 safenormalize\n");

    vec2 v1(0,0),
         v2(3,4),
         v3(5,12);

    assert(v1.safenormalize().magnitude() < tolerance);
    assert(std::abs(v2.safenormalize().magnitude() - 1) < tolerance);
    assert(std::abs(v3.safenormalize().magnitude() - 1) < tolerance);
}

void test_vec2_cross()
{
    std::printf("testing vec2 cross\n");

    vec2 v1(0,0),
         v2(-1,1),
         v3(-1,-2);

    assert(v1.cross(v1) == 0);
    assert(v2.cross(v2) == 0);
    assert(v2.cross(v3) == 3);
}

void test_vec2_squaredist()
{
    std::printf("testing vec2 squaredist\n");

    vec2 v1(0,0),
         v2(-1,1),
         v3(-1,-2);

    assert(v1.squaredist(v1) == 0);
    assert(v1.squaredist(v2) == 2);
    assert(v2.squaredist(v3) == 9);
}

void test_vec2_dist()
{
    std::printf("testing vec2 dist\n");

    vec2 v1(0,0),
         v2(3,4),
         v3(6,8);

    assert(v1.dist(v1) == 0);
    assert(v1.dist(v2) == 5);
    assert(v2.dist(v3) == 5);
}

void test_vec2_abs()
{
    std::printf("testing vec2 abs\n");
    vec2 v1(0,0),
         v2(-1,1),
         v3(-1,-2);

    assert(v1.abs() == vec2(0,0));
    assert(v2.abs() == vec2(1,1));
    assert(v3.abs() == vec2(1,2));
}

void test_vec2_mul()
{
    std::printf("testing vec2 mul\n");

    //mul(float)
    {
        vec2 v1(0,0),
             v2(-1,1);
        v1.mul(1);
        v2.mul(-2);

        assert(v1 == vec2(0,0));
        assert(v2 == vec2(2,-2));
    }
    //mul(vec2)
    {
        vec2 v1(0,0),
             v2(-1,1),
             v3(1,-2);
        v1.mul(v2);
        v2.mul(v3);

        assert(v1 == vec2(0,0));
        assert(v2 == vec2(-1,-2));
    }
}

void test_vec2_square()
{
    std::printf("testing vec2 square\n");

    vec2 v1(0,0),
         v2(1,-2);

    v1.square();
    v2.square();
    assert(v1 == vec2(0,0));
    assert(v2 == vec2(1,4));
}

void test_vec2_clamp()
{
    std::printf("testing vec2 clamp\n");

    vec2 v1(0,0),
         v2(0,2);

    v1.clamp(-2,-1);
    v2.clamp(-2,1);
    assert(v1 == vec2(-1,-1));
    assert(v2 == vec2(0,1));
}

void test_vec2_lerp()
{
    std::printf("testing vec2 lerp\n");
    //lerp(vec,float)
    {
        vec2 v1(0,0),
             v2(1,1);
        assert(v1.lerp(v2,1) == vec2(1,1));
    }
    {
        vec2 v1(0,0),
             v2(1,1);
        assert(v1.lerp(v2,-1) == vec2(-1,-1));
    }
    {
        vec2 v1(0,0),
             v2(1,1);
        assert(v1.lerp(v2,0.5) == vec2(0.5,0.5));
    }
    {
        vec2 v1(0,0),
             v2(1,2);
        assert(v1.lerp(v2,0.5) == vec2(0.5,1));
    }
    {
        vec2 v1(0,0),
             v2(1,2);
        assert(v1.lerp(v2,1.5) == vec2(1.5,3));
    }
    //lerp(vec,vec,float)
    {
        vec2 v1(0,0),
             v2(1,1),
             v3;
        assert(v3.lerp(v1, v2,1) == vec2(1,1));
    }
    {
        vec2 v1(0,0),
             v2(1,1),
             v3;
        assert(v3.lerp(v1, v2,-1) == vec2(-1,-1));
    }
    {
        vec2 v1(0,0),
             v2(1,2),
             v3;
        assert(v3.lerp(v1, v2,1.5) == vec2(1.5,3));
    }
}

void test_vec2_avg()
{
    std::printf("testing vec2 avg\n");

    vec2 v1(0,0),
         v2(2,4);
    assert(v1.avg(v1) == vec2(0,0));
    assert(v2.avg(v2) == vec2(2,4));
    assert(v1.avg(v2) == vec2(1,2));
}

////////////////////////////////////////////////////////////////////////////////
// float vec tests
////////////////////////////////////////////////////////////////////////////////

void test_vec_ctor()
{
    std::printf("testing vec ctor\n");

    //vec(int)
    {
        vec v(1);
        assert(v == vec(1,1,1));
    }
    //vec(float)
    {
        vec v(1.f);
        assert(v == vec(1,1,1));
    }
    //vec(int*)
    {
        std::array<int, 3> arr{1,2,3};
        vec v(arr.data());
        assert(v == vec(1,2,3));
    }
    //vec(float*)
    {
        std::array<float, 3> arr{1.5f,2.5f,3.5f};
        vec v(arr.data());
        assert(v == vec(1.5f,2.5f,3.5f));
    }
    //vec(vec2, float)
    {
        vec2 v2(1,2);
        vec v(v2, 3);
        assert(v == vec(1,2,3));
    }
}

void test_vec_set()
{
    std::printf("testing vec set\n");

    vec v(0,0,0);
    v.set(0, 1);
    v.set(1, 2);
    v.set(2, 3);
    assert(v == vec(1,2,3));
}

void test_vec_bracket()
{
    std::printf("testing vec operator[]\n");

    vec v(1,2,3);

    float &f1 = v[0],
          &f2 = v[1],
          &f3 = v[2];
    float f4 = v[0],
          f5 = v[1],
          f6 = v[2];
    assert(f1 == 1);
    assert(f2 == 2);
    assert(f3 == 3);
    assert(f4 == 1);
    assert(f5 == 2);
    assert(f6 == 3);
}

void test_vec_nequal()
{
    std::printf("testing vec operator!=\n");

    vec v(1,2,3),
        v2(2,2,3);

    assert( (v != v) == false);
    assert( v != v2);
    assert( v2 != v);
}

void test_vec_iszero()
{
    std::printf("testing vec iszero\n");

    vec v1(0,0,0),
        v2(1,1,1);
    assert(v1.iszero() == true);
    assert(v2.iszero() == false);
}

void test_vec_squaredlen()
{
    std::printf("testing vec squaredlen\n");
    vec v1(0,0,0),
        v2(1,1,1),
        v3(1,2,3);

    assert(v1.squaredlen() == 0);
    assert(v2.squaredlen() == 3);
    assert(v3.squaredlen() == 14);
}

void test_vec_square()
{
    std::printf("testing vec square\n");
    vec v1(0,0,0),
        v2(1,1,1),
        v3(1,2,3);

    assert(v1.square() == vec(0,0,0));
    assert(v2.square() == vec(1,1,1));
    assert(v3.square() == vec(1,4,9));
}

void test_vec_neg2()
{
    std::printf("testing vec neg2\n");
    vec v1(0,0,0),
        v2(1,1,1),
        v3(1,2,3);

    assert(v1.neg2() == vec(0,0,0));
    assert(v2.neg2() == vec(-1,-1,1));
    assert(v3.neg2() == vec(-1,-2,3));
}

void test_vec_neg()
{
    std::printf("testing vec neg\n");
    vec v1(0,0,0),
        v2(1,1,1),
        v3(1,2,3);

    assert(v1.neg() == vec(0,0,0));
    assert(v2.neg() == vec(-1,-1,-1));
    assert(v3.neg() == vec(-1,-2,-3));
}

void test_vec_abs()
{
    std::printf("testing vec abs\n");
    vec v1(0,0,0),
        v2(-1,1,-1),
        v3(-1,-2,-3);

    assert(v1.abs() == vec(0,0,0));
    assert(v2.abs() == vec(1,1,1));
    assert(v3.abs() == vec(1,2,3));
}

void test_vec_recip()
{
    std::printf("testing vec recip\n");
    vec v1(1,2,-4);
    assert(v1.recip() == vec(1,0.5,-0.25));
}

void test_vec_magnitude2()
{
    std::printf("testing vec magnitude2\n");

    vec v1(0,0,1),
        v2(3,4,1),
        v3(5,12,1);

    assert(v1.magnitude2() == 0);
    assert(v2.magnitude2() == 5);
    assert(v3.magnitude2() == 13);
}

void test_vec_magnitude()
{
    std::printf("testing vec magnitude\n");

    vec v1(0,0,0),
        v2(3,4,12),
        v3(5,12,0);

    assert(v1.magnitude() == 0);
    assert(v2.magnitude() == 13);
    assert(v3.magnitude() == 13);
}

void test_vec_normalize()
{
    std::printf("testing vec normalize\n");

    vec v1(1,0,0),
        v2(3,4,0),
        v3(3,4,12);

    assert(std::abs(v1.normalize().magnitude() - 1) < tolerance);
    assert(std::abs(v2.normalize().magnitude() - 1) < tolerance);
    assert(std::abs(v3.normalize().magnitude() - 1) < tolerance);
}

void test_vec_safenormalize()
{
    std::printf("testing vec safenormalize\n");

    vec v1(0,0,0),
        v2(3,4,0),
        v3(3,4,12);

    assert(v1.safenormalize().magnitude() < tolerance);
    assert(std::abs(v2.safenormalize().magnitude() - 1) < tolerance);
    assert(std::abs(v3.safenormalize().magnitude() - 1) < tolerance);
}

void test_vec_isnormalized()
{
    std::printf("testing vec isnormalized\n");

    vec v1(0.6,0.8,0),
        v2(1,0,0),
        v3(3,0,0),
        v4(0,0,0);

    assert(v1.isnormalized() == true);
    assert(v2.isnormalized() == true);
    assert(v3.isnormalized() == false);
    assert(v3.isnormalized() == false);
}

void test_vec_mul()
{
    std::printf("testing vec mul\n");

    //mul(float)
    {
        vec v1(0,0,0),
            v2(-1,1,2);
        v1.mul(1);
        v2.mul(-2);

        assert(v1 == vec(0,0,0));
        assert(v2 == vec(2,-2,-4));
    }
    //mul(vec2)
    {
        vec v1(0,0,0),
            v2(-1,1,2),
            v3(1,-2,3);
        v1.mul(v2);
        v2.mul(v3);

        assert(v1 == vec(0,0,0));
        assert(v2 == vec(-1,-2,6));
    }
}

void test_vec_mul2()
{
    std::printf("testing vec mul2\n");

    vec v1(0,0,0),
        v2(-1,1,2);
    v1.mul2(2);
    v2.mul2(2);

    assert(v1 == vec(0,0,0));
    assert(v2 == vec(-2,2,2));
}

void test_vec_min()
{
    std::printf("testing vec min\n");

    vec v1(0,0,0),
        v2(0,2,-3);

    v1.min(-1);
    v2.min(1);
    assert(v1 == vec(-1,-1,-1));
    assert(v2 == vec(0,1,-3));
}

void test_vec_max()
{
    std::printf("testing vec max\n");

    vec v1(0,0,0),
        v2(0,2,-3);

    v1.max(1);
    v2.max(1);
    assert(v1 == vec(1,1,1));
    assert(v2 == vec(1,2,1));
}

void test_vec_clamp()
{
    std::printf("testing vec clamp\n");

    vec v1(0,0,0),
        v2(0,2,-3);

    v1.clamp(-2,-1);
    v2.clamp(-2,1);
    assert(v1 == vec(-1,-1,-1));
    assert(v2 == vec(0,1,-2));
}

void test_vec_dot2()
{
    std::printf("testing vec dot2\n");
    //dot2(vec)
    {
        vec v1(1,2,3),
            v2(4,5,6);
        assert(v1.dot2(v2) == 14);
    }
    //dot2(vec2)
    {
        vec v1(1,2,3);
        vec2 v2(4,5);
        assert(v1.dot2(v2) == 14);
    }
}

void test_vec_dot()
{
    std::printf("testing vec dot\n");
    vec v1(1,2,3),
        v2(4,5,6);
    assert(v1.dot(v2) == 32);
}

void test_vec_absdot()
{
    std::printf("testing vec absdot\n");
    vec v1(1,2,3),
        v2(-4,-5,-6);
    assert(v1.absdot(v2) == 32);
}

void test_vec_squaredist()
{
    std::printf("testing vec squaredist\n");

    vec v1(0,0,0),
        v2(3,4,0),
        v3(6,8,0);

    assert(v1.dist(v1) == 0);
    assert(v1.squaredist(v2) == 25);
    assert(v2.squaredist(v1) == 25);
    assert(v2.squaredist(v3) == 25);

}

void test_vec_dist()
{
    std::printf("testing vec dist\n");

    //dist(vec)
    {
        vec v1(0,0,0),
            v2(3,4,0),
            v3(6,8,0);

        assert(v1.dist(v1) == 0);
        assert(v1.dist(v2) == 5);
        assert(v2.dist(v3) == 5);
    }
    //dist(vec,vec)
    {
        vec v1(0,0,0),
            v2(3,4,0),
            v3(6,8,0);

        assert(v1.dist(v1, v1) == 0);
        assert(v1 == vec(0,0,0));
        assert(v1.dist(v2, v1) == 5);
        assert(v1 == vec(-3,-4,0));
        assert(v1.dist(v3, v2) == 15);
        assert(v2 == vec(-9,-12,0));
    }
}

void test_vec_dist2()
{
    std::printf("testing vec dist2\n");

    vec v1(0,0,0),
        v2(3,4,1),
        v3(6,8,2);

    assert(v1.dist2(v1) == 0);
    assert(v1.dist2(v2) == 5);
    assert(v2.dist2(v3) == 5);
}

void test_vec_reject()
{
    std::printf("testing vec reject\n");

    vec v1(0,0,0),
        v2(2,3,0),
        v3(-2,-3,0);

    assert(v1.reject(v2, 3) == false);
    assert(v1.reject(v2, 1) == true);
    assert(v1.reject(v3, 3) == false);
    assert(v1.reject(v3, 1) == true);
    assert(v1.reject(v1, 0) == false);
}

void test_vec_lerp()
{
    std::printf("testing vec lerp\n");
    //lerp(vec,float)
    {
        vec v1(0,0,0),
            v2(1,1,1);
        assert(v1.lerp(v2,1) == vec(1,1,1));
    }
    {
        vec v1(0,0,0),
            v2(1,1,1);
        assert(v1.lerp(v2,-1) == vec(-1,-1,-1));
    }
    {
        vec v1(0,0,0),
            v2(1,1,1);
        assert(v1.lerp(v2,0.5) == vec(0.5,0.5,0.5));
    }
    {
        vec v1(0,0,0),
            v2(1,2,3);
        assert(v1.lerp(v2,0.5) == vec(0.5,1,1.5));
    }
    {
        vec v1(0,0,0),
            v2(1,2,3);
        assert(v1.lerp(v2,1.5) == vec(1.5,3,4.5));
    }
    //lerp(vec,vec,float)
    {
        vec v1(0,0,0),
            v2(1,1,1),
            v3;
        assert(v3.lerp(v1, v2,1) == vec(1,1,1));
    }
    {
        vec v1(0,0,0),
            v2(1,1,1),
            v3;
        assert(v3.lerp(v1, v2,-1) == vec(-1,-1,-1));
    }
    {
        vec v1(0,0,0),
            v2(1,2,3),
            v3;
        assert(v3.lerp(v1, v2,1.5) == vec(1.5,3,4.5));
    }
}

void test_vec_avg()
{
    std::printf("testing vec avg\n");

    vec v1(0,0,0),
        v2(2,4,6);
    assert(v1.avg(v1) == vec(0,0,0));
    assert(v2.avg(v2) == vec(2,4,6));
    assert(v1.avg(v2) == vec(1,2,3));
}

void test_vec_rescale()
{
    std::printf("testing vec rescale\n");

    vec v1(1,0,0),
        v2(3,4,0);
    assert(v1.rescale(2) == vec(2,0,0));
    assert(v1.rescale(0) == vec(0,0,0));
    assert(v2.rescale(0.5) == vec(0.3,0.4,0));
}

void test_vec_orthogonal()
{
    std::printf("testing vec orthogonal\n");

    vec v1(1,0,0),
        v2(0,1,0),
        v3(1,1,0),
        v4(1,0,1);
    vec v5,
        v6,
        v7,
        v8;

    v5.orthogonal(v1);
    assert(v5 == vec(0,1,0));
    v6.orthogonal(v2);
    assert(v6 == vec(0,0,1));
    v7.orthogonal(v3);
    assert(v7 == vec(-1,1,0));
    v8.orthogonal(v4);
    assert(v8 == vec(0,-1,0));

    //test orthogonality condition
    assert(!v1.dot(v5));
    assert(!v2.dot(v6));
    assert(!v3.dot(v7));
    assert(!v4.dot(v8));
}

void test_vec_insidebb()
{
    std::printf("testing vec insidebb\n");

    vec v1(1,2,3);
    ivec iv1(2,3,4),
         iv2(5,6,7),
         iv3(-1,0,1);
    //insidebb(ivec, int)
    {
        assert(v1.insidebb(iv1, 1) == false);
        assert(v1.insidebb(iv1, -1) == false);
        assert(v1.insidebb(iv2, 1) == false);
        assert(v1.insidebb(iv3, 1) == false);
        assert(v1.insidebb(iv3, 2) == true);
        assert(v1.insidebb(iv3, 3) == true);
    }
    //insidebb(ivec,int,int)
    {
        assert(v1.insidebb(iv1, 1, 0) == false);
        assert(v1.insidebb(iv1, 1, 1) == true);
        assert(v1.insidebb(iv2, 1, 0) == false);
        assert(v1.insidebb(iv2, 1, 4) == true);
        assert(v1.insidebb(iv3, 1, 0) == false);
        assert(v1.insidebb(iv3, 1, 1) == true);
        assert(v1.insidebb(iv3, 2, 1) == true);
    }
}

void test_vec_dist_to_bb()
{
    std::printf("testing vec dist_to_bb");

    vec v1(0,0,0),
        v2(3,4,1);
    ivec min(0,0,1),
         max(3,4,12);
    assert(v1.dist_to_bb(min,max) == 1);  //vec at 0,0,-1 from bb
    assert(v1.dist_to_bb(max,max) == 13); //bb at point
    assert(v2.dist_to_bb(min,min) == 5);  //vec +x+y+z from bb
    assert(v2.dist_to_bb(min,max) == 0);  //position inside bb
}

void test_vec_project_bb()
{
    std::printf("testing vec project_bb\n");

    vec v1(-1,1,-1),
        v2(-1,0,-1);
    ivec min(1,2,3),
         max(4,5,6);

    assert(v1.project_bb(min, max) == -4 +2 -6);
    assert(v2.project_bb(min, max) == -4 +0 -6);
}

////////////////////////////////////////////////////////////////////////////////
// integer vec tests
////////////////////////////////////////////////////////////////////////////////

void test_ivec_ctor()
{
    std::printf("testing ivec ctor\n");
    //ivec(vec)
    {
        vec v1(1.5,1.3,2.7),
            v2(-1.5,-3.7,-5.2);

        assert(ivec(v1) == ivec(1,1,2));
        assert(ivec(v2) == ivec(-1,-3,-5));
    }
    //ivec(int, ivec, int)
    {
        ivec v1(0,0,0),
             v2(1,2,3);

        assert(ivec(1,v1,1) == ivec(1,0,0));
        assert(ivec(2,v1,1) == ivec(0,1,0));
        assert(ivec(3,v1,1) == ivec(1,1,0));
        assert(ivec(4,v1,1) == ivec(0,0,1));
        assert(ivec(3,v2,2) == ivec(3,4,3));
    }
}

void test_ivec_iszero()
{
    std::printf("testing ivec iszero\n");

    ivec v1(0,0,0),
         v2(1,1,1),
         v3(1,0,0);
    assert(v1.iszero() == true);
    assert(v2.iszero() == false);
    assert(v3.iszero() == false);
}

void test_ivec_shl()
{
    std::printf("testing ivec shl (shift left)\n");

    ivec v(1,2,3);
    v.shl(1);
    assert(v == ivec(2,4,6));
    v.shl(0);
    assert(v == ivec(2,4,6));
}

void test_ivec_shr()
{
    std::printf("testing ivec shr (shift right)\n");

    ivec v(2,4,6);
    v.shr(1);
    assert(v == ivec(1,2,3));
    v.shr(0);
    assert(v == ivec(1,2,3));
    v.shr(1);
    assert(v == ivec(0,1,1));

}

void test_ivec_mask()
{
    std::printf("testing ivec mask\n");

    ivec v1(1,2,3);
    assert(v1.mask(1) == ivec(1,0,1));
}

void test_ivec_neg()
{
    std::printf("testing ivec neg\n");
    ivec v1(0,0,0),
         v2(1,1,1),
         v3(1,2,3);

    assert(v1.neg() == ivec(0,0,0));
    assert(v2.neg() == ivec(-1,-1,-1));
    assert(v3.neg() == ivec(-1,-2,-3));
}

void test_ivec_dot()
{
    std::printf("testing ivec dot\n");
    ivec v1(1,2,3),
         v2(4,5,6);
    assert(v1.dot(v2) == 32);
}

void test_ivec_clamp()
{
    std::printf("testing ivec clamp\n");

    ivec v1(0,0,0),
         v2(0,2,-3);

    v1.clamp(-2,-1);
    v2.clamp(-2,1);
    assert(v1 == ivec(-1,-1,-1));
    assert(v2 == ivec(0,1,-2));
}

void test_ivec_dist()
{
    std::printf("testing ivec dist\n");
    {
        plane p(vec(0,0,1), 0);
        ivec i(1,1,1);
        assert(i.dist(p) == 1);
    }
    {
        plane p(vec(0,0,1), 0);
        ivec i(1,1,0);
        assert(i.dist(p) == 0);
    }
}

////////////////////////////////////////////////////////////////////////////////
// integer vec2 tests
////////////////////////////////////////////////////////////////////////////////

void test_ivec2_iszero()
{
    std::printf("testing ivec2 iszero\n");

    ivec2 v1(0,0),
         v2(1,1),
         v3(1,0);
    assert(v1.iszero() == true);
    assert(v2.iszero() == false);
    assert(v3.iszero() == false);
}

void test_raysphereintersect()
{
    std::printf("testing raysphereintersect\n");

    //intersection tests
    {
        //ray to ~infinity
        vec center(0,0,0);
        float radius = 1;
        vec o(0,0,-4),
            ray(0,0,9999999);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected);
        assert(std::abs(dist) < tolerance);
    }
    {
        //ray starting inside sphere
        vec center(0,0,0);
        float radius = 1;
        vec o(0,0,0),
            ray(0,0,2);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected);
        assert(std::abs(1 + dist) < tolerance);
    }
    {
        //ray starting at tangent
        vec center(0,0,0);
        float radius = 1;
        vec o(0,1,0),
            ray(0,0,2);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected);
        assert(std::abs(dist) < tolerance);
    }
    {
        //ray ending at tangent
        vec center(0,0,0);
        float radius = 1;
        vec o(0,-1,-1),
            ray(0,0,1);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected);
        assert(std::abs(1 - dist) < tolerance);
    }
    //non-intersection tests
    {
        //ray too far in +y
        vec center(0,0,0);
        float radius = 1;
        vec o(0,2,0),
            ray(0,0,1);
        float dist = 0;
        bool intersected = raysphereintersect(center, radius, o, ray, dist);
        assert(intersected == false);
        assert(std::abs(dist) < tolerance);
    }
}

void test_rayboxintersect()
{
    std::printf("testing rayboxintersect\n");
    //intersection tests
    {
        vec b(0,0,0),
            s(1,1,1),
            o(-1,-1,-1),
            ray(2,2,2);
        float dist = 0;
        int orient = 0;
        bool intersected = rayboxintersect(b, s, o, ray, dist, orient);
        assert(intersected);
        assert(std::abs(dist - 0.5) < tolerance);
        assert(orient == 0);
    }
    {
        vec b(0,0,0),
            s(1,1,1),
            o(-1,-1,-1),
            ray(1,1,1);
        float dist = 0;
        int orient = 0;
        bool intersected = rayboxintersect(b, s, o, ray, dist, orient);
        assert(intersected);
        assert(std::abs(dist - 1) < tolerance);
        assert(orient == 0);
    }
    {
        vec b(0,0,0),
            s(2,2,2),
            o(2,3,2),
            ray(-2,-2,-2);
        float dist = 0;
        int orient = 0;
        bool intersected = rayboxintersect(b, s, o, ray, dist, orient);
        assert(intersected);
        assert(std::abs(dist - 0.5) < tolerance);
        assert(orient == 3);
    }
    {
        vec b(0,0,0),
            s(2,2,2),
            o(2,3,2),
            ray(-3,-3,-3);
        float dist = 0;
        int orient = 0;
        bool intersected = rayboxintersect(b, s, o, ray, dist, orient);
        assert(intersected);
        assert(std::abs(dist - 0.333) < tolerance);
        assert(orient == 3);
    }
    //non-intersection tests
    {
        vec b(0,0,0),
            s(1,1,1),
            o(0,0,0),
            ray(0,0,0);
        float dist = 0;
        int orient = 0;
        bool intersected = rayboxintersect(b, s, o, ray, dist, orient);
        assert(intersected == false);
    }
    {
        vec b(0,0,0),
            s(-1,-1,-1),
            o(-2,-2,-2),
            ray(-3,-3,-3);
        float dist = 0;
        int orient = 0;
        bool intersected = rayboxintersect(b, s, o, ray, dist, orient);
        assert(intersected == false);
    }
}

void test_linecylinderintersect()
{
    std::printf("testing linecylinderintersect\n");
    //intersection tests
    {
        //test line from origin to z=10 intersecting cylinder r=1 halfway through
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,5),
              end(1,0,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.4 - dist) < tolerance);
    }
    {
        //test line from origin to z=10 intersecting cylinder centered at z=10 and not leaving
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,10),
              end(1,0,10);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.9 - dist) < tolerance);
    }
    {
        //test line from origin to z=10 entirely inside intersecting cylinder
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,5),
              end(1,0,5);
        float radius = 6,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test line from origin to z=10 entirely inside intersecting cylinder, with dist set
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,5),
              end(1,0,5);
        float radius = 6,
              dist = 10;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test line intersecting 0-size cylinder through middle
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,0,5),
              end(1,0,5);
        float radius = 0,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.5 - dist) < tolerance);
    }
    {
        //test line intersecting cylinder at tangent point
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(1,1,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.5 - dist) < tolerance);
    }
    {
        //test line intersecting cylinder at tangent point, at endpoint of line
        vec  from(0,0,0),
               to(0,0,5),
            start(-1,1,5),
              end(1,1,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(1 - dist) < tolerance);
    }
    {
        //test line intersecting cylinder at tangent, at edge
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(0,1,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.5 - dist) < tolerance);
    }
    {
        //test diagonal line intersecting cylinder through edge
        vec  from(0,0,0),
               to(10,0,10),
            start(3,0,5),
              end(7,0,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.4 - dist) < tolerance);
    }
    {
        //test diagonal line intersecting cylinder through cylinder caps
        vec  from(0,0,0),
               to(10,0,10),
            start(4.5,0,5),
              end(5.5,0,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.45 - dist) < tolerance);
    }
    {
        //test line intersecting cylinder of negative radius
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(1,1,5);
        float radius = -1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected);
        assert(std::abs(0.5 - dist) < tolerance);
    }
    //non-intersection tests
    {
        //test line which nearly intersects cylinder along cylinder tangent
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(0,1,5);
        float radius = 0.99,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected == false);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test line which nearly intersects cylinder edge
        vec  from(0,0,0),
               to(0,0,10),
            start(-1,1,5),
              end(-0.01,1,5);
        float radius = 1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected == false);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test line which nearly intersects cylinder opposite edge
        vec  from(0,0,0),
               to(0,0,10),
            start(1,1,5),
              end(0.01,1,5);
        float radius = 0.1,
              dist = 0;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected == false);
        assert(std::abs(dist) < tolerance);
    }
    {
        //test case where dist is already set
        vec  from(0,0,0),
               to(0,0,10),
            start(1,1,5),
              end(0.01,1,5);
        float radius = 0.1,
              dist = 10;
        bool intersected = linecylinderintersect(from, to, start, end, radius, dist);
        assert(intersected == false);
        assert(std::abs(10 - dist) < tolerance);
    }
}

void test_polyclip()
{
    std::printf("testing polyclip\n");
    {
        //test no directionality
        std::array<vec, 5> in = {{vec(0,0,0), vec(0,0,1), vec(0,1,1), vec(0,1,0), vec(0,2,0)}};
        std::array<vec, 5> out;
        out.fill(vec(0,0,0));

        polyclip(in.data(), 5, vec(0,0,0), 0, 0, out.data());
        assert(out[0] == vec(0,0,0));
        assert(out[1] == vec(0,0,1));
        assert(out[2] == vec(0,1,1));
        assert(out[3] == vec(0,1,0));
        assert(out[4] == vec(0,2,0));
    }
    {
        //test clipping +z at 0
        std::array<vec, 5> in = {{vec(0,0,0), vec(0,0,1), vec(0,1,1), vec(0,1,0), vec(0,2,0)}};
        std::array<vec, 5> out;
        out.fill(vec(0,0,0));

        polyclip(in.data(), 5, vec(0,0,1), 0, 0, out.data());
        assert(out[0] == vec(0,0,0));
        assert(out[1] == vec(0,1,0));
        assert(out[2] == vec(0,2,0));
        assert(out[3] == vec(0,0,0));
        assert(out[4] == vec(0,0,0));
    }
    {
        //test clipping +z 1..2
        std::array<vec, 5> in = {{vec(0,0,0), vec(0,0,1), vec(0,1,3), vec(0,1,1), vec(0,2,0)}};
        std::array<vec, 5> out;
        out.fill(vec(0,0,0));

        polyclip(in.data(), 5, vec(0,0,1), 1, 2, out.data());
        assert(out[0] == vec(0,0,1));
        assert(out[1] == vec(0,0.5,2));
        assert(out[2] == vec(0,1,2));
        assert(out[3] == vec(0,1,1));
        assert(out[4] == vec(0,0,0));
    }
}

void test_mod360()
{
    std::printf("testing mod360\n");
    assert(mod360(0) == 0);
    assert(mod360(360) == 0);
    assert(mod360(540) == 180);
    assert(mod360(-180) == 180);
    assert(mod360(900) == 180);
}

void test_sin360()
{
    std::printf("testing sin360\n");

    assert(sin360(0) == 0);
    assert(sin360(90) == 1);
    assert(sin360(180) == 0);
    assert(sin360(270) == -1);
    assert(sin360(360) == 0);

    assert(std::abs(sin360(90) - std::sin(90/RAD)) < tolerance);
    assert(std::abs(sin360(60) - std::sin(60/RAD)) < tolerance);
    assert(std::abs(sin360(52) - std::sin(52/RAD)) < tolerance);
    assert(std::abs(sin360(700) - std::sin(700/RAD)) < tolerance);
    assert(std::abs(sin360(720) - std::sin(720/RAD)) < tolerance);
}

void test_cos360()
{
    std::printf("testing cos360\n");

    assert(cos360(0) == 1);
    assert(cos360(90) == 0);
    assert(cos360(180) == -1);
    assert(cos360(270) == 0);
    assert(cos360(360) == 1);

    assert(std::abs(cos360(95) - std::cos(95/RAD)) < tolerance);
    assert(std::abs(cos360(69) - std::cos(69/RAD)) < tolerance);
    assert(std::abs(cos360(52) - std::cos(52/RAD)) < tolerance);
    assert(std::abs(cos360(700) - std::cos(700/RAD)) < tolerance);
    assert(std::abs(cos360(720) - std::cos(720/RAD)) < tolerance);
}

void test_geom()
{
    std::printf(
"===============================================================\n\
testing geometry\n\
===============================================================\n"
    );

    test_vec2_ctor();
    test_vec2_bracket();
    test_vec2_nequal();
    test_vec2_iszero();
    test_vec2_dot();
    test_vec2_squaredlen();
    test_vec2_magnitude();
    test_vec2_normalize();
    test_vec2_safenormalize();
    test_vec2_cross();
    test_vec2_squaredist();
    test_vec2_dist();
    test_vec2_abs();
    test_vec2_mul();
    test_vec2_square();
    test_vec2_clamp();
    test_vec2_lerp();
    test_vec2_avg();

    test_vec_ctor();
    test_vec_set();
    test_vec_bracket();
    test_vec_nequal();
    test_vec_iszero();
    test_vec_squaredlen();
    test_vec_square();
    test_vec_neg2();
    test_vec_neg();
    test_vec_abs();
    test_vec_recip();
    test_vec_magnitude2();
    test_vec_magnitude();
    test_vec_normalize();
    test_vec_safenormalize();
    test_vec_isnormalized();
    test_vec_mul();
    test_vec_mul2();
    test_vec_min();
    test_vec_max();
    test_vec_clamp();
    test_vec_dot2();
    test_vec_dot();
    test_vec_absdot();
    test_vec_squaredist();
    test_vec_dist();
    test_vec_dist2();
    test_vec_reject();
    test_vec_lerp();
    test_vec_avg();
    test_vec_rescale();
    test_vec_orthogonal();
    test_vec_insidebb();
    test_vec_dist_to_bb();
    test_vec_project_bb();

    test_ivec_ctor();
    test_ivec_iszero();
    test_ivec_shl();
    test_ivec_shr();
    test_ivec_mask();
    test_ivec_neg();
    test_ivec_clamp();
    test_ivec_dot();
    test_ivec_dist();

    test_ivec_iszero();

    test_raysphereintersect();
    test_rayboxintersect();
    test_linecylinderintersect();
    test_polyclip();
    test_mod360();
    test_sin360();
    test_cos360();
}


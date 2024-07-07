#include "libprimis.h"
#include "../src/shared/geomexts.h"

namespace
{
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

    template<class T, class U>
    void test_2d_bracket(std::string_view type)
    {
        std::printf("testing %s bracket\n", type.data());

        {
            T v(1,2);

            U &f1 = v[0],
              &f2 = v[1];
            assert(f1 == 1);
            assert(f2 == 2);
        }
        {
            const T v(1,2);

            U f3 = v[0],
              f4 = v[1];
            assert(f3 == 1);
            assert(f4 == 2);
        }
    }

    void test_vec2_bracket()
    {
        test_2d_bracket<vec2, float>("vec2");
    }

    void test_vec2_nequal()
    {
        std::printf("testing vec2 operator!=\n");

        vec2 v(1,2),
             v2(2,2);

        assert((v != v) == false);
        assert(v != v2);
        assert(v2 != v);
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

    template<class T>
    void test_2d_mul(std::string_view type)
    {
        std::printf("testing %s mul\n", type.data());

        //mul(float)
        {
            T v1(0,0),
              v2(-1,1);
            v1.mul(1);
            v2.mul(-2);

            assert(v1 == T(0,0));
            assert(v2 == T(2,-2));
        }
        //mul(vec2)
        {
            T v1(0,0),
              v2(-1,1),
              v3(1,-2);
            v1.mul(v2);
            v2.mul(v3);

            assert(v1 == T(0,0));
            assert(v2 == T(-1,-2));
        }
    }

    void test_vec2_mul()
    {
        test_2d_mul<vec2>("vec2");
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

    template<class T>
    void test_2d_add(std::string_view type)
    {
        std::printf("testing %s add\n", type.data());

        //T::add(float)
        {
            T v1(0,0),
              v2(-1,1);
            v1.add(1);
            v2.add(-2);

            assert(v1 == T(1,1));
            assert(v2 == T(-3,-1));
        }
        //T::add(vec2)
        {
            T v1(0,0),
              v2(-1,1),
              v3(1,-2);
            v1.add(v2);
            v2.add(v3);

            assert(v1 == T(-1,1));
            assert(v2 == T(0,-1));
        }
    }

    void test_vec2_add()
    {
        test_2d_add<vec2>("vec2");
    }

    template<class T>
    void test_2d_sub(std::string_view type)
    {
        std::printf("testing %s sub\n", type.data());

        //T::sub(float)
        {
            T v1(0,0),
              v2(-1,1);
            v1.sub(1);
            v2.sub(-2);

            assert(v1 == T(-1,-1));
            assert(v2 == T(1,3));
        }
        //T::sub(vec2)
        {
            T v1(0,0),
              v2(-1,1),
              v3(1,-2);
            v1.sub(v2);
            v2.sub(v3);

            assert(v1 == T(1,-1));
            assert(v2 == T(-2,3));
        }
    }

    void test_vec2_sub()
    {
        test_2d_sub<vec2>("vec2");
    }

    template<class T>
    void test_2d_min(std::string_view type)
    {
        std::printf("testing %s min\n", type.data());

        {
            T v1(0,0),
              v2(-1,1);
            v1.min(v2);
            assert(v1 == T(-1,0));
        }
        {
            T v1(1,-1);
            v1.min(0);
            assert(v1 == T(0,-1));
        }
    }

    void test_vec2_min()
    {
        test_2d_min<vec2>("vec2");
    }

    template<class T>
    void test_2d_max(std::string_view type)
    {
        std::printf("testing %s max\n", type.data());

        {
            T v1(0,0),
              v2(-1,1);
            v1.max(v2);
            assert(v1 == T(0,1));
        }
        {
            T v1(1,-1);
            v1.max(0);
            assert(v1 == T(1,0));
        }
    }

    void test_vec2_max()
    {
        test_2d_max<vec2>("vec2");
    }

    template<class T>
    void test_2d_neg(std::string_view type)
    {
        std::printf("testing %s neg\n", type.data());
        T v1(0,0),
          v2(1,1),
          v3(-1,2);

        assert(v1.neg() == T(0,0));
        assert(v2.neg() == T(-1,-1));
        assert(v3.neg() == T(1,-2));
    }

    void test_vec2_neg()
    {
        test_2d_neg<vec2>("vec2");
    }

    template<class T>
    void test_2d_abs(std::string_view type)
    {
        std::printf("testing %s abs\n", type.data());
        T v1(0,0),
          v2(-1,1),
          v3(-1,-2);

        assert(v1.abs() == T(0,0));
        assert(v2.abs() == T(1,1));
        assert(v3.abs() == T(1,2));
    }

    void test_vec2_abs()
    {
        test_2d_abs<vec2>("vec2");
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

    template<class T>
    void test_3d_plus(std::string_view type)
    {
        std::printf("testing %s operator+\n", type.data());
        T v1(vec(0,0,0)),
          v2(vec(2,4,6));
        assert(v1 + v1 == T(vec(0,0,0)));
        assert(v2 + v2 == T(vec(4,8,12)));
        assert(v1 + v2 == T(vec(2,4,6)));
    }

    void test_vec2_plus()
    {
        test_3d_plus<vec2>("vec2");
    }

    template<class T>
    void test_3d_minus(std::string_view type)
    {
        std::printf("testing %s operator-\n", type.data());

        //operator-(vec2)
        {
            T v1(vec(1,1,1)),
              v2(vec(2,4,6));
            assert(v1 - v1 == T(vec(0,0,0)));
            assert(v2 - v1 == T(vec(1,3,5)));
            assert(v1 - v2 == T(vec(-1,-3,-5)));
        }
        //operator-()
        {
            T v1(vec(1,1,1)),
              v2(vec(2,4,6));
            assert(-v1 == T(vec(-1,-1,-1)));
            assert(-v2 == T(vec(-2,-4,-6)));
        }
    }

    void test_vec2_minus()
    {
        test_3d_minus<vec2>("vec2");
    }

    template<class T>
    void test_3d_star(std::string_view type)
    {
        std::printf("testing %s operator*\n", type.data());

        //operator*(T)
        {
            T v1(vec(1,1,1)),
              v2(vec(2,4,6));
            assert(v1*3 == T(vec(3,3,3)));
            assert(v2*2 == T(vec(4,8,12)));
            assert(v1*0 == T(vec(0,0,0)));
        }
        //operator*(vec2)
        {
            T v1(vec(1,1,1)),
              v2(vec(2,4,6));
            assert(v1*v1 == T(vec(1,1,1)));
            assert(v2*v1 == T(vec(2,4,6)));
            assert(v2*v2 == T(vec(4,16,36)));
        }
    }

    void test_vec2_star()
    {
        test_3d_star<vec2>("vec2");
    }

    template<class T>
    void test_3d_slash(std::string_view type)
    {
        std::printf("testing %s operator/\n", type.data());

        //operator/(T)
        {
            T v1(vec(4,4,4)),
              v2(vec(2,4,6));
            assert(v1/2 == T(vec(2,2,2)));
            assert(v2/2 == T(vec(1,2,3)));
            assert(v1/1 == T(vec(4,4,4)));
        }
        //operator/(vec2)
        {
            T v1(vec(2,2,2)),
              v2(vec(2,4,6));
            assert(v1/v1 == T(vec(1,1,1)));
            assert(v2/v1 == T(vec(1,2,3)));
            assert(v2/v2 == T(vec(1,1,1)));
        }
    }

    void test_vec2_slash()
    {
        test_3d_slash<vec2>("vec2");
    }

    //3d test can be narrowed to test 2d case
    template<class T, class U>
    void test_3d_rotate_around_z(std::string_view type)
    {
        std::printf("testing %s rotation around z\n", type.data());

        //rotate_around_z(T,T)
        {
            //rotate 180 degrees
            T v1 = std::cos(M_PI),
              v2 = std::sin(M_PI);
            U v3(vec(0,1,1));
            v3.rotate_around_z(v1, v2);
            assert(v3.sub(U(vec(0,-1,1))).magnitude() < tolerance);
        }
        {
            //rotate 360
            T v1 = std::cos(2*M_PI),
              v2 = std::sin(2*M_PI);
            U v3(vec(0,1,1));
            v3.rotate_around_z(v1, v2);
            assert(v3.sub(U(vec(0,1,1))).magnitude() < tolerance);
        }
        {
            //rotate 90 (CCW)
            T v1 = std::cos(M_PI/2),
              v2 = std::sin(M_PI/2);
            U v3(vec(0,1,1));
            v3.rotate_around_z(v1, v2);
            assert(v3.sub(U(vec(-1,0,1))).magnitude() < tolerance);
        }
        //rotate_around_z(T)
        {
            //rotate 180 degrees
            U v3(vec(0,1,1));
            v3.rotate_around_z(M_PI);
            assert(v3.sub(U(vec(0,-1,1))).magnitude() < tolerance);
        }
        {
            //rotate 360 degrees
            U v3(vec(0,1,1));
            v3.rotate_around_z(2*M_PI);
            assert(v3.sub(U(vec(0,1,1))).magnitude() < tolerance);
        }
        {
            //rotate 90 degrees
            U v3(vec(0,1,1));
            v3.rotate_around_z(M_PI/2);
            assert(v3.sub(U(vec(-1,0,1))).magnitude() < tolerance);
        }
        //rotate_around_z(vec2)
        {
            //rotate 180 degrees
            vec2 v{std::cos(M_PI), std::sin(M_PI)};
            U v3(vec(0,1,1));
            v3.rotate_around_z(v);
            assert(v3.sub(U(vec(0,-1,1))).magnitude() < tolerance);
        }
        {
            //rotate 360
            vec2 v{std::cos(2*M_PI), std::sin(2*M_PI)};
            U v3(vec(0,1,1));
            v3.rotate_around_z(v);
            assert(v3.sub(U(vec(0,1,1))).magnitude() < tolerance);
        }
        {
            //rotate 90 (CCW)
            vec2 v{std::cos(M_PI/2), std::sin(M_PI/2)};
            U v3(vec(0,1,1));
            v3.rotate_around_z(v);
            assert(v3.sub(U(vec(-1,0,1))).magnitude() < tolerance);
        }
    }

    void test_vec2_rotate_around_z()
    {
        test_3d_rotate_around_z<float, vec2>("vec2");
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

    template<class T, class U>
    void test_3d_bracket(std::string_view type)
    {
        std::printf("testing %s operator[]\n", type.data());

        T v(1,2,3);
        const T v2(3,4,5);

        U &f1 = v[0],
          &f2 = v[1];
        const U &f3 = v2[2];
        U f4 = v[0],
          f5 = v[1];
        const U f6 = v2[2];
        assert(f1 == 1);
        assert(f2 == 2);
        assert(f3 == 5);
        assert(f4 == 1);
        assert(f5 == 2);
        assert(f6 == 5);
    }

    void test_vec_bracket()
    {
        test_3d_bracket<vec, float>("vec");
    }

    template<class T>
    void test_3d_nequal(std::string_view type)
    {
        std::printf("testing %s operator!=\n", type.data());

        T v(1,2,3),
          v2(2,2,3);

        assert( (v != v) == false);
        assert( v != v2);
        assert( v2 != v);
    }

    void test_vec_nequal()
    {
        test_3d_nequal<vec>("vec");
    }

    void test_vec_plus()
    {
        test_3d_plus<vec>("vec");
    }

    void test_vec_minus()
    {
        test_3d_minus<vec>("vec");
    }

    void test_vec_star()
    {
        test_3d_star<vec>("vec");
    }

    void test_vec_slash()
    {
        test_3d_star<vec>("vec");
    }

    template<class T>
    void test_3d_iszero(std::string_view type)
    {
        std::printf("testing %s iszero\n", type.data());

        T v1(0,0,0),
          v2(1,1,1);
        assert(v1.iszero() == true);
        assert(v2.iszero() == false);
    }

    template<class T>
    void test_3d_to_bool(std::string_view type)
    {
        std::printf("testing %s operator bool()\n", type.data());

        T v1(0,0,0),
          v2(1,1,1);
        assert(static_cast<bool>(v1) == false);
        assert(static_cast<bool>(v2) == true);
    }

    void test_vec_iszero()
    {
        test_3d_iszero<vec>("vec");
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

    template<class T>
    void test_3d_mul(std::string_view type)
    {
        std::printf("testing %s mul\n", type.data());

        //mul(float)
        {
            T v1(0,0,0),
              v2(-1,1,2);
            v1.mul(1);
            v2.mul(-2);

            assert(v1 == T(0,0,0));
            assert(v2 == T(2,-2,-4));
        }
        //mul(vec2)
        {
            T v1(0,0,0),
              v2(-1,1,2),
              v3(1,-2,3);
            v1.mul(v2);
            v2.mul(v3);

            assert(v1 == T(0,0,0));
            assert(v2 == T(-1,-2,6));
        }
    }

    void test_vec_mul()
    {
        test_3d_mul<vec>("vec");
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

    template<class T>
    void test_3d_add(std::string_view type)
    {
        std::printf("testing %s add\n", type.data());

        //T::sub(float)
        {
            T v1(0,0,0);
            v1.add(1);
            assert(v1 == T(1,1,1));
        }
        //T::sub(T)
        {
            T v1(1,2,3),
              v2(-1,2,-3);
            v1.add(v2);
            assert(v1 == T(0,4,0));
        }
    }

    void test_vec_add()
    {
        test_3d_add<vec>("vec");
    }

    void test_vec_addz()
    {
        std::printf("testing vec add2\n");

        vec v1(0,0,0),
            v2(-1,1,2);
        v1.add2(2);
        v2.add2(2);

        assert(v1 == vec(2,2,0));
        assert(v2 == vec(1,3,2));
    }

    void test_vec_add2()
    {
        std::printf("testing vec add2\n");

        vec v1(0,0,0),
            v2(-1,1,2);
        v1.addz(2);
        v2.addz(2);

        assert(v1 == vec(0,0,2));
        assert(v2 == vec(-1,1,4));
    }

    template<class T>
    void test_3d_sub(std::string_view type)
    {
        std::printf("testing %s sub\n", type.data());

        //T::sub(float)
        {
            T v1(0,0,0);
            v1.sub(1);
            assert(v1 == T(-1,-1,-1));
        }
        //T::sub(T)
        {
            T v1(1,2,3),
              v2(-1,2,-3);
            v1.sub(v2);
            assert(v1 == T(2,0,6));
        }
    }

    void test_vec_sub()
    {
        test_3d_sub<vec>("vec");
    }

    template<class T>
    void test_3d_min(std::string_view type)
    {
        std::printf("testing %s min\n", type.data());

        {
            T v1(0,0,0),
              v2(0,2,-3);

            v1.min(-1);
            v2.min(1);
            assert(v1 == T(-1,-1,-1));
            assert(v2 == T(0,1,-3));
        }
        {
            T v1(0,0,0),
              v2(0,2,-3);

            v1.min(v2);
            assert(v1 == T(0,0,-3));
        }
    }

    void test_vec_min()
    {
        test_3d_min<vec>("vec");
    }

    template<class T>
    void test_3d_max(std::string_view type)
    {
        std::printf("testing %s max\n", type.data());

        {
            T v1(0,0,0),
              v2(0,2,-3);

            v1.max(1);
            v2.max(1);
            assert(v1 == T(1,1,1));
            assert(v2 == T(1,2,1));
        }
        {
            T v1(0,0,0),
              v2(0,2,-3);

            v1.max(v2);
            assert(v1 == T(0,2,0));
        }
    }

    void test_vec_max()
    {
        test_3d_max<vec>("vec");
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

    template<class T>
    void test_3d_dot(std::string_view type)
    {
        std::printf("testing %s max\n", type.data());
        T v1(1,2,3),
          v2(4,5,6);
        assert(v1.dot(v2) == 32);
    }

    void test_vec_dot()
    {
        test_3d_dot<vec>("vec");
    }

    void test_vec_squaredot()
    {
        std::printf("testing vec squaredot\n");
        vec v1(1,1,1),
            v2(1,2,3);
        assert(v1.squaredot(v2) == 36);
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

    template<class T, class U>
    void test_3d_cross(std::string_view type)
    {
        std::printf("testing %s cross\n", type.data());
        //test axes
        {
            T v1(0,0,1),
              v2(0,1,0);
            U v3;

            v3.cross(v1, v2);
            assert(v3 == U(-1,0,0));
            v3.cross(v2, v1);
            assert(v3 == U(1,0,0));
        }
        //test parallelogram
        {
            T v1(0,0,1),
              v2(0,1,1);
            U v3;

            v3.cross(v1, v2);
            assert(v3 == U(-1,0,0));
            v3.cross(v2, v1);
            assert(v3 == U(1,0,0));
        }
        //test colinear
        {
            T v1(0,0,1),
              v2(0,0,2);
            U v3;

            v3.cross(v1, v2);
            assert(v3 == U(0,0,0));
            v3.cross(v2, v1);
            assert(v3 == U(0,0,0));
        }
    }

    void test_vec_cross()
    {
        test_3d_cross<vec, vec>("vec");
    }

    void test_vec_scalartriple()
    {
        std::printf("testing vec scalartriple\n");

        vec v1(0,0,1),
            v2(0,1,0),
            v3(1,0,0);

        float st1 = v3.scalartriple(v1,v2);
        assert(st1 == -1);
        float st2 = v2.scalartriple(v1,v2);
        assert(st2 == 0);
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

    template<class T, class U>
    void test_3d_rotate_around_x(std::string_view type)
    {
        std::printf("testing %s rotation around x\n", type.data());

        //rotate_around_x(T,T)
        {
            //rotate 180 degrees
            T v1 = std::cos(M_PI),
              v2 = std::sin(M_PI);
            U v3(1,0,1);
            v3.rotate_around_x(v1, v2);
            assert(v3.sub(U(1,0,-1)).magnitude() < tolerance);
        }
        {
            //rotate 360
            T v1 = std::cos(2*M_PI),
              v2 = std::sin(2*M_PI);
            U v3(1,0,1);
            v3.rotate_around_x(v1, v2);
            assert(v3.sub(U(1,0,1)).magnitude() < tolerance);
        }
        {
            //rotate 90 (CCW)
            T v1 = std::cos(M_PI/2),
              v2 = std::sin(M_PI/2);
            U v3(1,0,1);
            v3.rotate_around_x(v1, v2);
            assert(v3.sub(U(1,-1,0)).magnitude() < tolerance);
        }
        //rotate_around_x(T)
        {
            //rotate 180 degrees
            U v3(1,0,1);
            v3.rotate_around_x(M_PI);
            assert(v3.sub(U(1,0,-1)).magnitude() < tolerance);
        }
        {
            //rotate 360 degrees
            U v3(1,0,1);
            v3.rotate_around_x(2*M_PI);
            assert(v3.sub(U(1,0,1)).magnitude() < tolerance);
        }
        {
            //rotate 90 degrees
            U v3(1,0,1);
            v3.rotate_around_x(M_PI/2);
            assert(v3.sub(U(1,-1,0)).magnitude() < tolerance);
        }
        //rotate_around_x(vec2)
        {
            //rotate 180 degrees
            vec2 v{std::cos(M_PI), std::sin(M_PI)};
            U v3(1,0,1);
            v3.rotate_around_x(v);
            assert(v3.sub(U(1,0,-1)).magnitude() < tolerance);
        }
        {
            //rotate 360
            vec2 v{std::cos(2*M_PI), std::sin(2*M_PI)};
            U v3(1,0,1);
            v3.rotate_around_x(v);
            assert(v3.sub(U(1,0,1)).magnitude() < tolerance);
        }
        {
            //rotate 90 (CCW)
            vec2 v{std::cos(M_PI/2), std::sin(M_PI/2)};
            U v3(1,0,1);
            v3.rotate_around_x(v);
            assert(v3.sub(U(1,-1,0)).magnitude() < tolerance);
        }
    }

    template<class T, class U>
    void test_3d_rotate_around_y(std::string_view type)
    {
        std::printf("testing %s rotation around y\n", type.data());

        //rotate_around_y(T,T)
        {
            //rotate 180 degrees
            T v1 = std::cos(M_PI),
              v2 = std::sin(M_PI);
            U v3(0,1,1);
            v3.rotate_around_y(v1, v2);
            assert(v3.sub(U(0,1,-1)).magnitude() < tolerance);
        }
        {
            //rotate 360
            T v1 = std::cos(2*M_PI),
              v2 = std::sin(2*M_PI);
            U v3(0,1,1);
            v3.rotate_around_y(v1, v2);
            assert(v3.sub(U(0,1,1)).magnitude() < tolerance);
        }
        {
            //rotate 90 (CCW)
            T v1 = std::cos(M_PI/2),
              v2 = std::sin(M_PI/2);
            U v3(0,1,1);
            v3.rotate_around_y(v1, v2);
            assert(v3.sub(U(-1,1,0)).magnitude() < tolerance);
        }
        //rotate_around_y(T)
        {
            //rotate 180 degrees
            U v3(0,1,1);
            v3.rotate_around_y(M_PI);
            assert(v3.sub(U(0,1,-1)).magnitude() < tolerance);
        }
        {
            //rotate 360 degrees
            U v3(0,1,1);
            v3.rotate_around_y(2*M_PI);
            assert(v3.sub(U(0,1,1)).magnitude() < tolerance);
        }
        {
            //rotate 90 degrees
            U v3(0,1,1);
            v3.rotate_around_y(M_PI/2);
            assert(v3.sub(U(-1,1,0)).magnitude() < tolerance);
        }
        //rotate_around_y(vec2)
        {
            //rotate 180 degrees
            vec2 v{std::cos(M_PI), std::sin(M_PI)};
            U v3(0,1,1);
            v3.rotate_around_y(v);
            assert(v3.sub(U(0,1,-1)).magnitude() < tolerance);
        }
        {
            //rotate 360
            vec2 v{std::cos(2*M_PI), std::sin(2*M_PI)};
            U v3(0,1,1);
            v3.rotate_around_y(v);
            assert(v3.sub(U(0,1,1)).magnitude() < tolerance);
        }
        {
            //rotate 90 (CCW)
            vec2 v{std::cos(M_PI/2), std::sin(M_PI/2)};
            U v3(0,1,1);
            v3.rotate_around_y(v);
            assert(v3.sub(U(-1,1,0)).magnitude() < tolerance);
        }
    }

    void test_vec_rotate_around_z()
    {
        test_3d_rotate_around_z<float, vec>("vec");
    }

    void test_vec_rotate_around_x()
    {
        test_3d_rotate_around_x<float, vec>("vec");
    }

    void test_vec_rotate_around_y()
    {
        test_3d_rotate_around_y<float, vec>("vec");
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
        std::printf("testing vec dist_to_bb\n");

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
    // byte vec tests
    ////////////////////////////////////////////////////////////////////////////////

    void test_bvec_ctor()
    {
        std::printf("testing bvec ctor\n");
        //bvec(uchar,uchar,uchar)
        {
            bvec b1(255,255,255);
            assert(b1.x == 255);
            assert(b1.y == 255);
            assert(b1.z == 255);
        }
        {
            bvec b1(1,2,3);
            assert(b1.x == 1);
            assert(b1.y == 2);
            assert(b1.z == 3);
        }
        //bvec(vec)
        {
            bvec b1(vec(1,1,1));
            assert(b1 == bvec(255,255,255));
        }
        {
            bvec b1(vec(0,0,0));
            assert(b1 == bvec(127,127,127));
        }
        {
            bvec b1(vec(-1,-1,-1));
            assert(b1 == bvec(0,0,0));
        }
    }

    void test_bvec_bracket()
    {
        test_3d_bracket<bvec, uchar>("bvec");
    }

    void test_bvec_nequal()
    {
        test_3d_nequal<bvec>("bvec");
    }

    void test_bvec_iszero()
    {
        test_3d_iszero<bvec>("bvec");
    }

    void test_bvec_flip()
    {
        std::printf("testing bvec flip\n");

        {
            bvec b1(255,255,255);
            b1.flip();
            assert(b1 == bvec(127,127,127));
        }
        {
            bvec b1(0,0,0);
            b1.flip();
            assert(b1 == bvec(128,128,128));
        }
        {
            bvec b1(1,2,3);
            b1.flip();
            assert(b1 == bvec(129,130,131));
        }
        {
            bvec b1(127,128,129);
            b1.flip();
            assert(b1 == bvec(255,0,1));
        }
    }

    void test_bvec_scale()
    {
        std::printf("testing bvec scale\n");

        {
            bvec b1(1,1,1);
            b1.scale(9,3);
            assert(b1 == bvec(3,3,3));
        }
        {
            bvec b1(128,64,32);
            b1.scale(3,1);
            assert(b1 == bvec(128,192,96));
        }
        {
            bvec b1(128,64,32);
            b1.scale(1,2);
            assert(b1 == bvec(64,32,16));
        }
    }

    template<class T>
    void test_3d_shl(std::string_view type)
    {
        std::printf("testing %s shl (shift left)\n", type.data());

        T v(1,2,3);
        v.shl(1);
        assert(v == T(2,4,6));
        v.shl(0);
        assert(v == T(2,4,6));
    }

    void test_bvec_shl()
    {
        test_3d_shl<bvec>("bvec");
    }

    template<class T>
    void test_3d_shr(std::string_view type)
    {
        std::printf("testing %s shr (shift right)\n", type.data());

        T v(2,4,6);
        v.shr(1);
        assert(v == T(1,2,3));
        v.shr(0);
        assert(v == T(1,2,3));
        v.shr(1);
        assert(v == T(0,1,1));
    }

    void test_bvec_shr()
    {
        test_3d_shr<bvec>("bvec");
    }

    void test_bvec_fromcolor()
    {
        std::printf("testing bvec fromcolor\n");

        assert(bvec::fromcolor(vec(1,1,1)) == bvec(255,255,255));
        assert(bvec::fromcolor(vec(0.25,0.5,0.75)) == bvec(63,127,191));
        assert(bvec::fromcolor(vec(0,0,0)) == bvec(0,0,0));
    }

    void test_bvec_tocolor()
    {
        std::printf("testing bvec tocolor\n");

        assert(bvec(255,255,255).tocolor() == vec(1,1,1));
        assert(bvec(63,127,191).tocolor().sub(vec(0.25,0.5,0.75)).magnitude() < 1.f/255);
        assert(bvec(0,0,0).tocolor() == vec(0,0,0));
    }

    void test_bvec_from565()
    {
        std::printf("testing bvec from565\n");
        //                     rrrrrggggggbbbbb
        assert(bvec::from565(0b0000000000000000) == bvec(0,0,0));
        assert(bvec::from565(0b1111100000011111) == bvec(255,0,255));
        assert(bvec::from565(0b1111111111111111) == bvec(255,255,255));
        assert(bvec::from565(0b1111011111011110) == bvec(247,251,247));
        assert(bvec::from565(0b1000010000010000) == bvec(131,130,131));
    }

    void test_bvec_hexcolor()
    {
        std::printf("testing bvec hexcolor\n");

        assert(bvec::hexcolor(0xFFFFFF) == bvec(255,255,255));
        assert(bvec::hexcolor(0x0010FF) == bvec(0,16,255));
        assert(bvec::hexcolor(0x000000) == bvec(0,0,0));
        assert(bvec::hexcolor(0x010203) == bvec(1,2,3));
    }

    void test_bvec_tohexcolor()
    {
        std::printf("testing bvec tohexcolor\n");

        assert(bvec(255,255,255).tohexcolor() == 0xFFFFFF);
        assert(bvec(0,16,255).tohexcolor() == 0x0010FF);
        assert(bvec(0,0,0).tohexcolor() == 0x000000);
        assert(bvec(1,2,3).tohexcolor() == 0x010203);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // vec4 tests
    ////////////////////////////////////////////////////////////////////////////////

    void test_vec4_ctor()
    {
        std::printf("testing vec4 constructor\n");

        //vec4(vec, T)
        {
            assert(vec4<float>(1,1,1,1) == vec4<float>(vec(1,1,1), 1));
            assert(vec4<float>(1,1,1,2) == vec4<float>(vec(1), 2));
        }
        //vec4(vec2, T, T)
        {
            assert(vec4<float>(1,1,1,1) == vec4<float>(vec2(1,1), 1, 1));
            assert(vec4<float>(1,1,1,2) == vec4<float>(vec2(1,1), 1, 2));
        }
        //vec4(bvec, uchar)
        {
            assert(vec4<float>(255,255,255,255) == vec4<float>(bvec(255,255,255), 255));
        }
        //vec4(bvec)
        {
            assert(vec4<float>(255,255,255,0) == vec4<float>(bvec(255,255,255)));
        }
    }

    void test_vec4_bracket()
    {
        std::printf("testing vec4 operator[]\n");

        //const operator[]
        {
            const vec4<int> v(1,2,3,4);
            assert(v[0] == 1);
            assert(v[1] == 2);
            assert(v[2] == 3);
            assert(v[3] == 4);
        }
        //operator[]
        {
            vec4<int> v(1,2,3,4);
            assert(v[0] == 1);
            assert(v[1] == 2);
            assert(v[2] == 3);
            assert(v[3] == 4);
        }
    }

    void test_vec4_equal()
    {
        std::printf("testing vec4 operator==\n");

        assert(vec4<int>(1,1,1,1) == vec4<float>(1,1,1,1));
        assert(vec4<bool>(true,true,true,true) == vec4<float>(1,1,1,1));
        assert(vec4<double>(1,1,1,1) == vec4<float>(1,1,1,1));
    }

    void test_vec4_nequal()
    {
        std::printf("testing vec4 operator!=\n");

        assert(vec4<int>(1,1,1,1) != vec4<float>(1,1,1,0));
        assert(vec4<bool>(true,true,true,true) != vec4<float>(0,0,0,0));
        assert(vec4<double>(1,1,1,2) != vec4<float>(1,1,1,1));
    }

    void test_vec4_dot3()
    {
        std::printf("testing vec4 dot3\n");

        {
            assert(vec4<int>(1,1,1,1).dot3(vec4<int>(1,1,1,1)) == 3);
            assert(vec4<int>(1,2,3,1).dot3(vec4<int>(1,1,1,1)) == 6);
            assert(vec4<float>(1,1,1,1).dot3(vec4<float>(1,1,1,1)) == 3.f);
            assert(vec4<float>(1,2,3,1).dot3(vec4<float>(1,1,1,1)) == 6);
        }
        {
            assert(vec4<int>(1,1,1,1).dot3(vec(1,1,1)) == 3);
            assert(vec4<int>(1,2,3,1).dot3(vec(1,1,1)) == 6);
            assert(vec4<float>(1,1,1,1).dot3(vec(1,1,1)) == 3.f);
            assert(vec4<float>(1,2,3,1).dot3(vec(1,1,1)) == 6);
        }
    }

    void test_vec4_dot()
    {
        std::printf("testing vec4 dot\n");

        {
            assert(vec4<int>(1,1,1,1).dot(vec4<int>(1,1,1,1)) == 4);
            assert(vec4<int>(1,2,3,1).dot(vec4<int>(1,1,1,1)) == 7);
            assert(vec4<float>(1,1,1,1).dot(vec4<float>(1,1,1,1)) == 4.f);
            assert(vec4<float>(1,2,3,1).dot(vec4<float>(1,1,1,1)) == 7.f);
        }
        {
            assert(vec4<int>(1,1,1,1).dot(vec(1,1,1)) == 4);
            assert(vec4<int>(1,2,3,1).dot(vec(1,1,1)) == 7);
            assert(vec4<float>(1,1,1,1).dot(vec(1,1,1)) == 4.f);
            assert(vec4<float>(1,2,3,1).dot(vec(1,1,1)) == 7.f);
        }
    }

    void test_vec4_squaredlen()
    {
        std::printf("testing vec4 squared len\n");

        {
            assert(vec4<int>(1,1,1,1).squaredlen() == 4);
            assert(vec4<int>(1,2,3,4).squaredlen() == 30);
            assert(vec4<float>(1,1,1,1).squaredlen() == 4.f);
            assert(vec4<float>(1,2,3,4).squaredlen() == 30.f);
        }
    }

    void test_vec4_magnitude()
    {
        std::printf("testing vec4 magnitude\n");

        {
            assert(vec4<int>(3,4,0,0).magnitude() == 5);
            assert(vec4<int>(0,0,5,12).magnitude() == 13);
            assert(vec4<float>(3,4,0,0).magnitude() == 5.f);
            assert(vec4<float>(0,0,5,12).magnitude() == 13.f);
        }
    }

    void test_vec4_magnitude3()
    {
        std::printf("testing vec4 magnitude3\n");

        {
            assert(vec4<int>(3,4,0,0).magnitude3() == 5);
            assert(vec4<int>(0,0,5,12).magnitude3() == 5);
            assert(vec4<float>(3,4,0,0).magnitude3() == 5.f);
            assert(vec4<float>(0,0,5,12).magnitude3() == 5.f);
        }
    }

    void test_vec4_normalize()
    {
        std::printf("testing vec4 normalize\n");

        {
            assert(vec4<float>(3,4,0,0).normalize() == vec4<float>(0.6,0.8,0,0));
            assert(vec4<float>(6,0,8,0).normalize() == vec4<float>(0.6,0,0.8,0));
        }
    }

    void test_vec4_safenormalize()
    {
        std::printf("testing vec4 safenormalize\n");

        {
            assert(vec4<float>(3,4,0,0).safenormalize() == vec4<float>(0.6,0.8,0,0));
            assert(vec4<float>(6,0,8,0).safenormalize() == vec4<float>(0.6,0,0.8,0));
            assert(vec4<float>(0,0,0,0).safenormalize() == vec4<float>(0,0,0,0));
        }
    }

    void test_vec4_lerp()
    {
        std::printf("testing vec4 lerp\n");
        //lerp(vec4<uchar>, vec4<uchar>, float)
        {
            vec4<uchar> v1(0,0,0,0),
                        v2(1,1,1,1);
            v1.lerp(v1, v2,1.f);
            assert(v1 == vec4<uchar>(1,1,1,0));
        }
        {
            vec4<uchar> v1(0,0,0,0),
                        v2(10,10,10,10);
            v1.lerp(v1, v2,2.5f);
            assert(v1 == vec4<uchar>(25,25,25,0));
        }
        //lerp(vec4<uchar>, vec4<uchar>, vec4<uchar>, float, float, float)
        {
            vec4<uchar> v1(1,1,1,1),
                        v2(10,10,10,10),
                        v3(100,100,100,100);
            v1.lerp(v1, v2, v3, 1, 2, 3);
            assert(v1 == vec4<uchar>(65,65,65,65));
        }
        {
            vec4<uchar> v1(1,1,1,1),
                        v2(10,10,10,10),
                        v3(100,100,100,100);
            v1.lerp(v1, v2, v3, 3, 2, 1);
            assert(v1 == vec4<uchar>(123,123,123,123));
        }
        {
            vec4<uchar> v1(1,1,1,1),
                        v2(10,10,10,10),
                        v3(100,100,100,100);
            v1.lerp(v1, v2, v3, 1, 1, 1);
            assert(v1 == vec4<uchar>(111,111,111,111));
        }
        //lerp(vec4,float)
        {
            vec4<float> v1(0,0,0,0),
                        v2(1,1,1,1);
            assert(v1.lerp(v2,1) == vec4<float>(1,1,1,1));
        }
        {
            vec4<float> v1(0,0,0,0),
                        v2(1,1,1,1);
            assert(v1.lerp(v2,-1) == vec4<float>(-1,-1,-1,-1));
        }
        {
            vec4<float> v1(0,0,0,0),
                        v2(1,1,1,1);
            assert(v1.lerp(v2,0.5) == vec4<float>(0.5,0.5,0.5,0.5));
        }
        {
            vec4<float> v1(0,0,0,0),
                        v2(1,2,3,4);
            assert(v1.lerp(v2,0.5) == vec4<float>(0.5,1,1.5,2));
        }
        {
            vec4<float> v1(0,0,0,0),
                        v2(1,2,3,4);
            assert(v1.lerp(v2,1.5) == vec4<float>(1.5,3,4.5,6));
        }
        //lerp(vec4<float>,vec4<float>,float)
        {
            vec4<float> v1(0,0,0,0),
                        v2(1,1,1,1),
                        v3;
            assert(v3.lerp(v1, v2,1) == vec4<float>(1,1,1,1));
        }
        {
            vec4<float> v1(0,0,0,0),
                        v2(1,1,1,1),
                        v3;
            assert(v3.lerp(v1, v2,-1) == vec4<float>(-1,-1,-1,-1));
        }
        {
            vec4<float> v1(0,0,0,0),
                        v2(1,2,3,4),
                        v3;
            assert(v3.lerp(v1, v2,1.5) == vec4<float>(1.5,3,4.5,6));
        }
    }

    void test_vec4_flip()
    {
        std::printf("testing vec4 flip");

        {
            vec4<uchar> v1(0,0,0,0);
            v1.flip();
            assert(v1 == vec4<uchar>(128,128,128,128));
            v1.flip();
            assert(v1 == vec4<uchar>(0,0,0,0));
        }
        {
            vec4<uchar> v1(10,10,10,10);
            v1.flip();
            assert(v1 == vec4<uchar>(138,138,138,138));
            v1.flip();
            assert(v1 == vec4<uchar>(10,10,10,10));
        }
    }

    void test_vec4_avg()
    {
        std::printf("testing vec4 avg\n");

        {
            assert(vec4<float>(3,4,0,0).avg(vec4<float>(9,12,0,0)) == vec4<float>(6,8,0,0));
            assert(vec4<float>(2,2,0,0).avg(vec4<float>(0,0,0,0)) == vec4<float>(1,1,0,0));
            assert(vec4<float>(-2,2,0,0).avg(vec4<float>(2,0,0,0)) == vec4<float>(0,1,0,0));
        }
    }

    void test_vec4_madd()
    {
        std::printf("testing vec4 madd\n");

        {
            assert(vec4<float>(3,4,0,0).madd(vec4<float>(9,12,0,0), vec4<float>(1,1,1,1)) == vec4<float>(12,16,0,0));
            assert(vec4<float>(3,4,0,0).madd(vec4<float>(9,12,0,0), vec4<float>(-1,1,1,1)) == vec4<float>(-6,16,0,0));
            assert(vec4<float>(1,2,3,4).madd(vec4<float>(9,12,0,0), vec4<float>(-1,1,1,1)) == vec4<float>(-8,14,3,4));
        }
    }

    void test_vec4_msub()
    {
        std::printf("testing vec4 msub\n");

        {
            assert(vec4<float>(3,4,0,0).msub(vec4<float>(9,12,0,0), vec4<float>(1,1,1,1)) == vec4<float>(-6,-8,0,0));
            assert(vec4<float>(3,4,0,0).msub(vec4<float>(9,12,0,0), vec4<float>(-1,1,1,1)) == vec4<float>(12,-8,0,0));
            assert(vec4<float>(1,2,3,4).msub(vec4<float>(9,12,0,0), vec4<float>(-1,1,1,1)) == vec4<float>(10,-10,3,4));
        }
    }

    void test_vec4_mul3()
    {
        std::printf("testing vec4 mul3\n");

        {
            assert(vec4<float>(3,4,0,0).mul3(0) == vec4<float>(0,0,0,0));
            assert(vec4<float>(3,4,0,0).mul3(1) == vec4<float>(3,4,0,0));
            assert(vec4<float>(6,0,8,0).mul3(2) == vec4<float>(12,0,16,0));
            assert(vec4<float>(1,2,3,4).mul3(3) == vec4<float>(3,6,9,4));
            assert(vec4<float>(1,2,3,4).mul3(0) == vec4<float>(0,0,0,4));
        }
    }

    void test_vec4_mul()
    {
        std::printf("testing vec4 mul\n");
        //mul(T)
        {
            assert(vec4<float>(3,4,0,0).mul(0) == vec4<float>(0,0,0,0));
            assert(vec4<float>(3,4,0,0).mul(1) == vec4<float>(3,4,0,0));
            assert(vec4<float>(6,0,8,0).mul(2) == vec4<float>(12,0,16,0));
            assert(vec4<float>(1,2,3,4).mul(3) == vec4<float>(3,6,9,12));
            assert(vec4<float>(1,2,3,4).mul(0) == vec4<float>(0,0,0,0));
        }
        //mul(vec4)
        {
            assert(vec4<float>(-2,2,0,0).mul(vec4<float>(2,0,0,0)) == vec4<float>(-4,0,0,0));
            assert(vec4<float>(-1,-2,-3,-4).mul(vec4<float>(1,1,1,1)) == vec4<float>(-1,-2,-3,-4));
        }
        //mul(vec)
        {
            assert(vec4<float>(-2,2,0,0).mul(vec(2,0,0)) == vec4<float>(-4,0,0,0));
            assert(vec4<float>(1,2,3,4).mul(vec(2,2,2)) == vec4<float>(2,4,6,4));
            assert(vec4<float>(1,2,3,4).mul(vec(2,-2,2)) == vec4<float>(2,-4,6,4));
        }
    }

    void test_vec4_square()
    {
        std::printf("testing vec4 square\n");

        {
            assert(vec4<float>(0,0,0,0).square() == vec4<float>(0,0,0,0));
            assert(vec4<float>(-1,-2,-3,-4).square() == vec4<float>(1,4,9,16));
            assert(vec4<float>(3,4,0,0).square() == vec4<float>(9,16,0,0));
        }
    }

    void test_vec4_div3()
    {
        std::printf("testing vec4 div3\n");

        {
            assert(vec4<float>(3,4,0,0).div3(1) == vec4<float>(3,4,0,0));
            assert(vec4<float>(3,6,0,0).div3(3) == vec4<float>(1,2,0,0));
            assert(vec4<float>(6,0,8,0).div3(2) == vec4<float>(3,0,4,0));
            assert(vec4<float>(2,4,6,8).div3(2) == vec4<float>(1,2,3,8));
        }
    }

    void test_vec4_div()
    {
        std::printf("testing vec4 div\n");
        //div(T)
        {
            assert(vec4<float>(3,4,0,0).div(1) == vec4<float>(3,4,0,0));
            assert(vec4<float>(3,6,0,0).div(3) == vec4<float>(1,2,0,0));
            assert(vec4<float>(6,0,8,0).div(2) == vec4<float>(3,0,4,0));
            assert(vec4<float>(2,4,6,8).div(2) == vec4<float>(1,2,3,4));
        }
        //div(vec4)
        {
            assert(vec4<float>(-2,2,0,0).div(vec4<float>(2,1,1,1)) == vec4<float>(-1,2,0,0));
            assert(vec4<float>(-1,-2,-3,-4).div(vec4<float>(1,-1,1,-2)) == vec4<float>(-1,2,-3,2));
        }
        //div(vec)
        {
            assert(vec4<float>(-2,2,0,0).div(vec(2,1,1)) == vec4<float>(-1,2,0,0));
            assert(vec4<float>(2,4,6,4).div(vec(2,2,2)) == vec4<float>(1,2,3,4));
            assert(vec4<float>(2,-4,6,4).div(vec(2,-2,2)) == vec4<float>(1,2,3,4));
        }
    }


    void test_vec4_recip()
    {
        std::printf("testing vec4 recip\n");

        {
            assert(vec4<float>(2,4,1,0.25).recip() == vec4<float>(0.5,0.25,1,4));
            assert(vec4<float>(4,8,5,2).recip() == vec4<float>(0.25,0.125,0.2,0.5));
        }
    }

    void test_vec4_add3()
    {
        std::printf("testing vec4 add3\n");

        {
            assert(vec4<float>(3,4,0,0).add3(1) == vec4<float>(4,5,1,0));
            assert(vec4<float>(3,6,0,0).add3(3) == vec4<float>(6,9,3,0));
            assert(vec4<float>(6,0,8,0).add3(2) == vec4<float>(8,2,10,0));
            assert(vec4<float>(2,4,6,8).add3(2) == vec4<float>(4,6,8,8));
        }
    }

    void test_vec4_add()
    {
        std::printf("testing vec4 add\n");
        //add(T)
        {
            assert(vec4<float>(3,4,0,0).add(1) == vec4<float>(4,5,1,1));
            assert(vec4<float>(3,6,0,0).add(3) == vec4<float>(6,9,3,3));
            assert(vec4<float>(6,0,8,0).add(2) == vec4<float>(8,2,10,2));
            assert(vec4<float>(2,4,6,8).add(2) == vec4<float>(4,6,8,10));
        }
        //add(vec4)
        {
            assert(vec4<float>(-2,2,0,0).add(vec4<float>(2,1,1,1)) == vec4<float>(0,3,1,1));
            assert(vec4<float>(-1,-2,-3,-4).add(vec4<float>(1,-1,1,-2)) == vec4<float>(0,-3,-2,-6));
        }
        //add(vec)
        {
            assert(vec4<float>(-2,2,0,0).add(vec(2,1,1)) == vec4<float>(0,3,1,0));
            assert(vec4<float>(2,4,6,4).add(vec(2,2,2)) == vec4<float>(4,6,8,4));
            assert(vec4<float>(2,-4,6,4).add(vec(2,-2,2)) == vec4<float>(4,-6,8,4));
        }
    }

    void test_vec4_addw()
    {
        std::printf("testing vec4 addw\n");

        {
            assert(vec4<float>(3,4,0,0).addw(1) == vec4<float>(3,4,0,1));
            assert(vec4<float>(3,6,0,0).addw(3) == vec4<float>(3,6,0,3));
            assert(vec4<float>(6,0,8,0).addw(2) == vec4<float>(6,0,8,2));
            assert(vec4<float>(2,4,6,8).addw(2) == vec4<float>(2,4,6,10));
        }
    }

    void test_vec4_sub3()
    {
        std::printf("testing vec4 sub3\n");

        {
            assert(vec4<float>(3,4,0,0).sub3(1) == vec4<float>(2,3,-1,0));
            assert(vec4<float>(3,6,0,0).sub3(3) == vec4<float>(0,3,-3,0));
            assert(vec4<float>(6,0,8,0).sub3(2) == vec4<float>(4,-2,6,0));
            assert(vec4<float>(2,4,6,8).sub3(2) == vec4<float>(0,2,4,8));
        }
    }

    void test_vec4_sub()
    {
        std::printf("testing vec4 sub\n");
        //sub(T)
        {
            assert(vec4<float>(3,4,0,0).sub(1) == vec4<float>(2,3,-1,-1));
            assert(vec4<float>(3,6,0,0).sub(3) == vec4<float>(0,3,-3,-3));
            assert(vec4<float>(6,0,8,0).sub(2) == vec4<float>(4,-2,6,-2));
            assert(vec4<float>(2,4,6,8).sub(2) == vec4<float>(0,2,4,6));
        }
        //sub(vec4)
        {
            assert(vec4<float>(-2,2,0,0).sub(vec4<float>(2,1,1,1)) == vec4<float>(-4,1,-1,-1));
            assert(vec4<float>(-1,-2,-3,-4).sub(vec4<float>(1,-1,1,-2)) == vec4<float>(-2,-1,-4,-2));
        }
        //sub(vec)
        {
            assert(vec4<float>(-2,2,0,0).sub(vec(2,1,1)) == vec4<float>(-4,1,-1,0));
            assert(vec4<float>(2,4,6,4).sub(vec(2,2,2)) == vec4<float>(0,2,4,4));
            assert(vec4<float>(2,-4,6,4).sub(vec(2,-2,2)) == vec4<float>(0,-2,4,4));
        }
    }

    void test_vec4_subw()
    {
        std::printf("testing vec4 subw\n");

        {
            assert(vec4<float>(3,4,0,0).subw(1) == vec4<float>(3,4,0,-1));
            assert(vec4<float>(3,6,0,0).subw(3) == vec4<float>(3,6,0,-3));
            assert(vec4<float>(6,0,8,0).subw(2) == vec4<float>(6,0,8,-2));
            assert(vec4<float>(2,4,6,8).subw(2) == vec4<float>(2,4,6,6));
        }
    }

    void test_vec4_neg3()
    {
        std::printf("testing vec4 neg3\n");

        {
            assert(vec4<float>(3,4,0,0).neg3() == vec4<float>(-3,-4,0,0));
            assert(vec4<float>(3,6,0,0).neg3() == vec4<float>(-3,-6,0,0));
            assert(vec4<float>(6,0,8,0).neg3() == vec4<float>(-6,0,-8,0));
            assert(vec4<float>(2,4,6,8).neg3() == vec4<float>(-2,-4,-6,8));
        }
    }

    void test_vec4_neg()
    {
        std::printf("testing vec4 neg\n");

        {
            assert(vec4<float>(3,4,0,0).neg() == vec4<float>(-3,-4,0,0));
            assert(vec4<float>(3,6,0,0).neg() == vec4<float>(-3,-6,0,0));
            assert(vec4<float>(6,0,8,0).neg() == vec4<float>(-6,0,-8,0));
            assert(vec4<float>(2,4,6,8).neg() == vec4<float>(-2,-4,-6,-8));
        }
    }

    void test_vec4_clamp()
    {
        std::printf("testing vec4 clamp\n");

        {
            assert(vec4<float>(3,4,0,0).clamp(1,4) == vec4<float>(3,4,1,1));
            assert(vec4<float>(3,6,0,0).clamp(1,4) == vec4<float>(3,4,1,1));
            assert(vec4<float>(6,0,8,0).clamp(-1, 2) == vec4<float>(2,0,2,0));
            assert(vec4<float>(2,4,6,8).clamp(0,0) == vec4<float>(0,0,0,0));
        }
    }

    void test_vec4_plus()
    {
        std::printf("testing vec4 operator+\n");

        {
            assert(vec4<float>(3,4,0,0) + vec4<float>(1,1,1,1) == vec4<float>(4,5,1,1));
            assert(vec4<float>(3,6,0,0) + vec4<float>(1,2,3,4) == vec4<float>(4,8,3,4));
            assert(vec4<float>(6,0,8,0) + vec4<float>(1,2,3,4) == vec4<float>(7,2,11,4));
            assert(vec4<float>(2,4,6,8) + vec4<float>(-1,-1,-1,-1) == vec4<float>(1,3,5,7));
        }
    }

    void test_vec4_minus()
    {
        std::printf("testing vec4 operator-\n");

        //operator-(vec4)
        {
            assert(vec4<float>(3,4,0,0) - vec4<float>(1,1,1,1) == vec4<float>(2,3,-1,-1));
            assert(vec4<float>(3,6,0,0) - vec4<float>(1,2,3,4) == vec4<float>(2,4,-3,-4));
            assert(vec4<float>(6,0,8,0) - vec4<float>(1,2,3,4) == vec4<float>(5,-2,5,-4));
            assert(vec4<float>(2,4,6,8) - vec4<float>(-1,-1,-1,-1) == vec4<float>(3,5,7,9));
        }
        //operator-()
        {
            assert(-vec4<float>(3,4,0,0) == vec4<float>(-3,-4,0,0));
            assert(-vec4<float>(3,6,0,0) == vec4<float>(-3,-6,0,0));
            assert(-vec4<float>(6,0,8,0) == vec4<float>(-6,0,-8,0));
            assert(-vec4<float>(2,4,6,8) == vec4<float>(-2,-4,-6,-8));
        }
    }

    void test_vec4_star()
    {
        std::printf("testing vec4 operator*\n");
        //operator*(vec4)
        {
            assert(vec4<float>(3,4,0,0) * vec4<float>(1,1,1,1) == vec4<float>(3,4,0,0));
            assert(vec4<float>(3,6,0,0) * vec4<float>(1,2,3,4) == vec4<float>(3,12,0,0));
            assert(vec4<float>(6,0,8,0) * vec4<float>(1,2,3,4) == vec4<float>(6,0,24,0));
            assert(vec4<float>(2,4,6,8) * vec4<float>(-1,-1,-1,-1) == vec4<float>(-2,-4,-6,-8));
        }
        //operator*(U)
        {
            assert(vec4<float>(1,2,3,4) * 2 == vec4<float>(2,4,6,8));
            assert(vec4<float>(1,2,3,4) * 0 == vec4<float>(0,0,0,0));
            assert(vec4<float>(1,2,3,4) * -1 == vec4<float>(-1,-2,-3,-4));
        }
    }

    void test_vec4_slash()
    {
        std::printf("testing vec4 operator/\n");
        //operator*(vec4)
        {
            assert(vec4<float>(3,4,0,0) / vec4<float>(1,1,1,1) == vec4<float>(3,4,0,0));
            assert(vec4<float>(3,6,0,0) / vec4<float>(1,2,3,4) == vec4<float>(3,3,0,0));
            assert(vec4<float>(6,0,8,0) / vec4<float>(1,1,2,2) == vec4<float>(6,0,4,0));
            assert(vec4<float>(2,4,6,8) / vec4<float>(-1,-1,-1,-1) == vec4<float>(-2,-4,-6,-8));
        }
        //operator*(U)
        {
            assert(vec4<float>(2,4,6,8) / 2 == vec4<float>(1,2,3,4));
            assert(vec4<float>(1,2,3,4) / 1 == vec4<float>(1,2,3,4));
            assert(vec4<float>(1,2,3,4) / -1 == vec4<float>(-1,-2,-3,-4));
        }
    }

    void test_vec4_cross()
    {
        test_3d_cross<vec, vec4<float>>("vec4");
    }

    void test_vec4_setxyz()
    {
        std::printf("testing vec4 setxyz\n");

        {
            vec4<float> f = {0,0,0,0};
            f.setxyz(vec(1,2,3));
            assert(f == vec4<float>(1,2,3,0));
        }
        {
            vec4<float> f = {2,3,4,0};
            f.setxyz(vec(1,2,3));
            assert(f == vec4<float>(1,2,3,0));
        }
        {
            vec4<float> f = {2,3,4,1};
            f.setxyz(vec(0,0,0));
            assert(f == vec4<float>(0,0,0,1));
        }
    }

    void test_vec4_rotate_around_z()
    {
        test_3d_rotate_around_z<float, vec4<float>>("vec4");
    }

    void test_vec4_rotate_around_x()
    {
        test_3d_rotate_around_x<float, vec4<float>>("vec4");
    }

    void test_vec4_rotate_around_y()
    {
        test_3d_rotate_around_y<float, vec4<float>>("vec4");
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

    void test_ivec_bracket()
    {
        test_3d_bracket<ivec, int>("ivec");
    }

    void test_ivec_nequal()
    {
        test_3d_nequal<ivec>("ivec");
    }

    void test_ivec_to_bool()
    {
        test_3d_to_bool<ivec>("ivec");
    }

    void test_ivec_shl()
    {
        test_3d_shl<ivec>("ivec");
    }

    void test_ivec_shr()
    {
        test_3d_shr<ivec>("ivec");
    }

    void test_ivec_mul()
    {
        test_3d_mul<ivec>("ivec");
    }

    void test_ivec_add()
    {
        test_3d_add<ivec>("ivec");
    }

    void test_ivec_sub()
    {
        test_3d_sub<ivec>("ivec");
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

    void test_ivec_min()
    {
        test_3d_min<ivec>("ivec");
    }

    void test_ivec_max()
    {
        test_3d_max<ivec>("ivec");
    }

    void test_ivec_dot()
    {
        test_3d_dot<ivec>("ivec");
    }

    void test_ivec_abs()
    {
        std::printf("testing ivec abs\n");
        ivec v1(0,0,0),
            v2(-1,1,-1),
            v3(-1,-2,-3);

        assert(v1.abs() == ivec(0,0,0));
        assert(v2.abs() == ivec(1,1,1));
        assert(v3.abs() == ivec(1,2,3));
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

    void test_ivec_cross()
    {
        test_3d_cross<ivec, ivec>("ivec");
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

    void test_ivec_floor()
    {
        std::printf("testing ivec floor\n");
        {
            ivec i = ivec::floor(vec(1.5,0,2.5)),
                 i2 = ivec::floor(vec(-1.5,-1,-0));
            assert(i == ivec(1,0,2));
            assert(i2 == ivec(-2,-1,0));
        }
    }

    void test_ivec_ceil()
    {
        std::printf("testing ivec ceil\n");
        {
            ivec i = ivec::ceil(vec(1.5,0,2.5)),
                 i2 = ivec::ceil(vec(-1.5,-1,-0));
            assert(i == ivec(2,0,3));
            assert(i2 == ivec(-1,-1,0));
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // integer vec2 tests
    ////////////////////////////////////////////////////////////////////////////////

    void test_ivec2_bracket()
    {
        test_2d_bracket<ivec2, int>("ivec2");
    }

    void test_ivec2_nequal()
    {
        std::printf("testing ivec2 operator!=\n");

        ivec2 v(1,2),
              v2(2,2);

        assert( (v != v) == false);
        assert( v != v2);
        assert( v2 != v);
    }

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

    void test_ivec2_shl()
    {
        std::printf("testing ivec2 shl (shift left)\n");

        ivec2 v(1,2);
        v.shl(1);
        assert(v == ivec2(2,4));
        v.shl(0);
        assert(v == ivec2(2,4));
    }

    void test_ivec2_shr()
    {
        std::printf("testing ivec2 shr (shift right)\n");

        ivec2 v(2,4);
        v.shr(1);
        assert(v == ivec2(1,2));
        v.shr(0);
        assert(v == ivec2(1,2));
        v.shr(1);
        assert(v == ivec2(0,1));
    }

    void test_ivec2_mul()
    {
        test_2d_mul<ivec2>("ivec2");
    }

    void test_ivec2_add()
    {
        test_2d_add<ivec2>("ivec2");
    }

    void test_ivec2_sub()
    {
        test_2d_sub<ivec2>("ivec2");
    }

    void test_ivec2_mask()
    {
        std::printf("testing ivec2 mask\n");

        ivec2 v1(1,2),
              v2(0,1),
              v3(4,12);
        assert(v1.mask(2) == ivec2(0,2));
        assert(v2.mask(1) == ivec2(0,1));
        assert(v3.mask(4) == ivec2(4,4));
    }

    void test_ivec2_neg()
    {
        test_2d_neg<ivec2>("ivec2");
    }

    void test_ivec2_min()
    {
        test_2d_min<ivec2>("ivec2");
    }

    void test_ivec2_max()
    {
        test_2d_max<ivec2>("ivec2");
    }

    void test_ivec2_abs()
    {
        test_2d_abs<ivec2>("ivec2");
    }

    void test_ivec2_dot()
    {
        std::printf("testing ivec2 dot\n");
        ivec2 v1(1,2),
              v2(4,5);
        assert(v1.dot(v2) == 14);
    }

    void test_ivec2_cross()
    {
        std::printf("testing ivec2 cross\n");
        ivec2 v1(1,2),
              v2(4,5),
              v3(7,8);
        assert(v1.cross(v2) == -3);
        assert(v2.cross(v3) == -3);
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
    test_vec2_mul();
    test_vec2_square();
    test_vec2_add();
    test_vec2_sub();
    test_vec2_min();
    test_vec2_max();
    test_vec2_neg();
    test_vec2_abs();
    test_vec2_clamp();
    test_vec2_lerp();
    test_vec2_avg();
    test_vec2_plus();
    test_vec2_minus();
    test_vec2_star();
    test_vec2_slash();
    test_vec2_rotate_around_z();

    test_vec_ctor();
    test_vec_set();
    test_vec_bracket();
    test_vec_nequal();
    test_vec_plus();
    test_vec_minus();
    test_vec_star();
    test_vec_slash();
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
    test_vec_add();
    test_vec_add2();
    test_vec_addz();
    test_vec_sub();
    test_vec_min();
    test_vec_max();
    test_vec_clamp();
    test_vec_dot2();
    test_vec_dot();
    test_vec_squaredot();
    test_vec_absdot();
    test_vec_squaredist();
    test_vec_dist();
    test_vec_dist2();
    test_vec_reject();
    test_vec_scalartriple();
    test_vec_cross();
    test_vec_lerp();
    test_vec_avg();
    test_vec_rescale();
    test_vec_rotate_around_z();
    test_vec_rotate_around_x();
    test_vec_rotate_around_y();
    test_vec_orthogonal();
    test_vec_insidebb();
    test_vec_dist_to_bb();
    test_vec_project_bb();

    test_bvec_ctor();
    test_bvec_bracket();
    test_bvec_nequal();
    test_bvec_iszero();
    test_bvec_flip();
    test_bvec_scale();
    test_bvec_shl();
    test_bvec_shr();
    test_bvec_fromcolor();
    test_bvec_tocolor();
    test_bvec_from565();
    test_bvec_hexcolor();
    test_bvec_tohexcolor();

    test_vec4_ctor();
    test_vec4_bracket();
    test_vec4_equal();
    test_vec4_nequal();
    test_vec4_dot3();
    test_vec4_dot();
    test_vec4_squaredlen();
    test_vec4_magnitude();
    test_vec4_magnitude3();
    test_vec4_normalize();
    test_vec4_safenormalize();
    test_vec4_lerp();
    test_vec4_flip();
    test_vec4_avg();
    test_vec4_madd();
    test_vec4_msub();
    test_vec4_mul3();
    test_vec4_mul();
    test_vec4_square();
    test_vec4_div3();
    test_vec4_div();
    test_vec4_recip();
    test_vec4_add3();
    test_vec4_add();
    test_vec4_addw();
    test_vec4_sub3();
    test_vec4_sub();
    test_vec4_subw();
    test_vec4_neg3();
    test_vec4_neg();
    test_vec4_clamp();
    test_vec4_plus();
    test_vec4_minus();
    test_vec4_star();
    test_vec4_slash();
    test_vec4_cross();
    test_vec4_setxyz();
    test_vec4_rotate_around_z();
    test_vec4_rotate_around_x();
    test_vec4_rotate_around_y();

    test_ivec_ctor();
    test_ivec_bracket();
    test_ivec_nequal();
    test_ivec_to_bool();
    test_ivec_shl();
    test_ivec_shr();
    test_ivec_mul();
    test_ivec_add();
    test_ivec_sub();
    test_ivec_mask();
    test_ivec_neg();
    test_ivec_min();
    test_ivec_max();
    test_ivec_abs();
    test_ivec_clamp();
    test_ivec_cross();
    test_ivec_dot();
    test_ivec_dist();
    test_ivec_floor();
    test_ivec_ceil();

    test_ivec2_bracket();
    test_ivec2_nequal();
    test_ivec2_iszero();
    test_ivec2_shl();
    test_ivec2_shr();
    test_ivec2_mul();
    test_ivec2_add();
    test_ivec2_sub();
    test_ivec2_mask();
    test_ivec2_neg();
    test_ivec2_min();
    test_ivec2_max();
    test_ivec2_abs();
    test_ivec2_dot();
    test_ivec2_cross();

    test_raysphereintersect();
    test_rayboxintersect();
    test_linecylinderintersect();
    test_polyclip();
    test_mod360();
    test_sin360();
    test_cos360();
}

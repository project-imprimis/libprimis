
#include "../src/libprimis-headers/cube.h"

#include "../src/shared/geomexts.h"
#include "../src/shared/glexts.h"

#include <optional>
#include <memory>

#include "../src/engine/interface/console.h"
#include "../src/engine/interface/cs.h"

#include "../src/engine/render/rendergl.h"
#include "../src/engine/render/rendermodel.h"
#include "../src/engine/render/shader.h"
#include "../src/engine/render/shaderparam.h"
#include "../src/engine/render/texture.h"

#include "../src/engine/world/entities.h"
#include "../src/engine/world/bih.h"

#include "../src/engine/model/model.h"
#include "../src/engine/model/ragdoll.h"
#include "../src/engine/model/animmodel.h"

namespace
{
    void test_animstate_equals()
    {
        std::printf("testing animstate operator==\n");
        animmodel::AnimPos p{1,1,1,1.f};
        animmodel::AnimState s1{nullptr, p, p, 1.f};
        animmodel::AnimState s2{nullptr, p, p, 0.f};
        assert(s1 == s1);
        assert(!(s2 == s1));
        assert(!(s1 == s2));
    }

    void test_animstate_nequals()
    {
        std::printf("testing animstate operator!=\n");
        animmodel::AnimPos p{1,1,1,1.f};
        animmodel::AnimState s1{nullptr, p, p, 1.f};
        animmodel::AnimState s2{nullptr, p, p, 0.f};
        assert(!(s1 != s1));
        assert(s2 != s1);
        assert(s1 != s2);
    }

    void test_animpos_equals()
    {
        std::printf("testing animpos operator==\n");
        animmodel::AnimPos p1{1,1,1,1.f};
        animmodel::AnimPos p2{1,1,1,1.f};
        animmodel::AnimPos p3{1,2,3,4.f};
        assert(p1 == p1);
        assert(p1 == p2);
        assert(!(p1 == p3));
    }

    void test_animpos_nequals()
    {
        std::printf("testing animpos operator==\n");
        animmodel::AnimPos p1{1,1,1,1.f};
        animmodel::AnimPos p2{1,1,1,1.f};
        animmodel::AnimPos p3{1,2,3,4.f};
        assert(!(p1 != p1));
        assert(!(p1 != p2));
        assert((p1 != p3));
    }

    void test_shaderparams_ctor()
    {
        std::printf("testing shaderparams ctor\n");
        animmodel::shaderparams s;

        assert(s.spec == 1.f);
        assert(s.gloss == 1.f);
        assert(s.glow == 3.f);
        assert(s.glowdelta == 0.f);
        assert(s.glowpulse == 0.f);
        assert(s.fullbright == 0.f);
        assert(s.scrollu == 0.f);
        assert(s.scrollv == 0.f);
        assert(s.alphatest == 0.9f);
        assert(s.color == vec(1.f, 1.f, 1.f));
    }

    void test_shaderparams_equals()
    {
        std::printf("testing shaderparams operator==\n");
        {
            animmodel::shaderparams s1;
            animmodel::shaderparams s2;

            assert(s1.spec == 1.f);
            assert(s1.gloss == 1.f);
            assert(s1.glow == 3.f);
            assert(s1.glowdelta == 0.f);
            assert(s1.glowpulse == 0.f);
            assert(s1.fullbright == 0.f);
            assert(s1.scrollu == 0.f);
            assert(s1.scrollv == 0.f);
            assert(s1.alphatest == 0.9f);
            assert(s1.color == vec(1.f, 1.f, 1.f));

            assert(s2.spec == 1.f);
            assert(s2.gloss == 1.f);
            assert(s2.glow == 3.f);
            assert(s2.glowdelta == 0.f);
            assert(s2.glowpulse == 0.f);
            assert(s2.fullbright == 0.f);
            assert(s2.scrollu == 0.f);
            assert(s2.scrollv == 0.f);
            assert(s2.alphatest == 0.9f);
            assert(s2.color == vec(1.f, 1.f, 1.f));

            assert(s2.spec == s1.spec);
            assert(s2.gloss == s1.gloss);
            assert(s2.glow == s1.glow);
            assert(s2.glowdelta == s1.glowdelta);
            assert(s2.glowpulse == s1.glowpulse);
            assert(s2.fullbright == s1.fullbright);
            assert(s2.scrollu == s1.scrollu);
            assert(s2.scrollv == s1.scrollv);
            assert(s2.alphatest == s1.alphatest);
            assert(s2.color == s1.color);
            assert(s1 == s2);
        }
        {
            animmodel::shaderparams s1, s2;
            s1.spec = 2.f;
            assert(!(s1 == s2));
        }
        {
            animmodel::shaderparams s1, s2;
            s2.fullbright = 2.f;
            assert(!(s1 == s2));
        }
        {
            animmodel::shaderparams s1, s2;
            s1.alphatest = 2.f;
            assert(!(s1 == s2));
        }
        {
            animmodel::shaderparams s1, s2;
            s2.color = vec(2.f);
            assert(!(s1 == s2));
        }
    }

    void test_skin_ctor()
    {
        std::printf("testing skin ctor\n");
        animmodel::skin s(nullptr, nullptr, nullptr);

        assert(s.tex == nullptr);
        assert(s.decal == nullptr);
        assert(s.masks == nullptr);
        assert(s.normalmap == nullptr);
        assert(s.shader == nullptr);
        assert(s.cullface == 1);
    }

    void test_skin_cleanup()
    {
        std::printf("testing skin cleanup\n");
        {
            animmodel::skin s(nullptr, nullptr, nullptr);
            s.cleanup();
            assert(s.shader == nullptr);
        }
        {
            Shader t;
            animmodel::skin s(nullptr, nullptr, nullptr);
            s.shader = &t;
            s.cleanup();
            assert(s.shader != nullptr);
        }
    }

    //animmodel related objects

    void test_animinfo_ctor()
    {
        std::printf("testing animinfo ctor\n");
        animinfo a;
        assert(a.anim == 0);
        assert(a.frame == 0);
        assert(a.range == 0);
        assert(a.basetime == 0);
        assert(a.speed == 100.f);
        assert(a.varseed == 0);
    }

    void test_animinfo_equals()
    {
        std::printf("testing animinfo operator==\n");
        {
            animinfo a;
            assert(a == a);
        }
        //test anim which has specific conditions
        {
            animinfo a;
            animinfo b;
            b.anim = Anim_SetTime;
            assert((a == b) == false);
            assert((b == a) == false);
        }
        {
            animinfo a;
            animinfo b;
            b.anim = Anim_Dir;
            assert((a == b) == false);
            assert((b == a) == false);
        }
        {
            animinfo a;
            animinfo b;
            b.anim = 1;
            assert((a == b) == true);
            assert((b == a) == true);
        }
        //test other params
        {
            animinfo a;
            animinfo b;
            b.frame = 1;
            assert((a == b) == false);
            assert((b == a) == false);
        }
        {
            animinfo a;
            animinfo b;
            b.range = 1;
            assert((a == b) == false);
            assert((b == a) == false);
        }
        {
            animinfo a;
            animinfo b;
            b.basetime = 1.f;
            assert((a == b) == false);
            assert((b == a) == false);
        }
        {
            animinfo a;
            animinfo b;
            b.speed = 1;
            assert((a == b) == false);
            assert((b == a) == false);
        }
        //note: varseed not used in comparison
        {
            animinfo a;
            animinfo b;
            b.varseed = 1;
            assert((a == b) == true);
            assert((b == a) == true);
        }
    }
}

void test_animmodel()
{
    std::printf(
"===============================================================\n\
testing animmodel functionality\n\
===============================================================\n"
    );
    test_animstate_equals();
    test_animstate_nequals();
    test_animpos_equals();
    test_animpos_nequals();
    test_shaderparams_ctor();
    test_shaderparams_equals();
    test_skin_ctor();
    test_skin_cleanup();
    test_animinfo_ctor();
    test_animinfo_equals();
};

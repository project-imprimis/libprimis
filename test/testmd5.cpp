#include "../src/libprimis-headers/cube.h"

#include "../src/shared/geomexts.h"
#include "../src/shared/glexts.h"
#include "../src/shared/stream.h"

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
#include "../src/engine/model/skelmodel.h"
#include "../src/engine/model/md5.h"

namespace
{

    //helper that creates a new md5 from media/model/pulserifle.md5mesh
    md5 *generate_md5_model()
    {
        md5 *m = new md5("pulserifle");
        m->startload();
        assert(md5::loading == m);

        skelcommands<md5>::setdir(std::string("pulserifle").data());
        float smooth = 0;
        skelcommands<md5>::loadpart("pulserifle.md5mesh", nullptr, &smooth);

        return m;
    }

    void test_md5_ctor()
    {
        std::printf("testing md5 ctor\n");

        md5 m("test");
        assert(m.modelname() == "test");
    }

    void test_md5_formatname()
    {
        std::printf("testing md5 formatname\n");

        assert(std::string(md5::formatname()) == std::string("md5"));
    }

    void test_md5_flipy()
    {
        std::printf("testing md5 flipy\n");

        md5 m("test");
        assert(m.flipy() == false);
    }

    void test_md5_type()
    {
        std::printf("testing md5 type\n");

        md5 m("test");
        assert(m.type() == MDL_MD5);
    }

    void test_md5_newmeshes()
    {
        std::printf("testing md5 newmeshes\n");

        md5 m("test");
        assert(m.newmeshes() != nullptr);
    }

    void test_md5_loaddefaultparts()
    {
        std::printf("testing md5 loaddefaultparts\n");

        //load model that exists
        {
            md5 m("pulserifle");

            assert(m.loaddefaultparts() == true);

            assert(m.modelname() == "pulserifle");
            assert(m.parts.size() == 1);
            assert(m.parts[0]->meshes != nullptr);
            skelmodel::skelmesh *s = static_cast<skelmodel::skelmesh *>(m.parts[0]->meshes[0].meshes.at(0));
            assert(s != nullptr);
            assert(s->vertcount() == 438);
            assert(s->tricount() == 462);
        }
        //(fail) to load nonexistent model
        {
            md5 m("pulserifle2");

            assert(m.loaddefaultparts() == false);
        }
    }

    void test_md5_load()
    {
        std::printf("testing md5 load\n");

        //load model that exists
        {
            md5 m("pulserifle");

            assert(m.load() == true);

            assert(m.modelname() == "pulserifle");
            assert(m.parts.size() == 1);
            assert(m.parts[0]->meshes != nullptr);
            skelmodel::skelmesh *s = static_cast<skelmodel::skelmesh *>(m.parts[0]->meshes[0].meshes.at(0));
            assert(s != nullptr);
            assert(s->vertcount() == 438);
            assert(s->tricount() == 462);
        }
        //(fail) to load nonexistent model
        {
            md5 m("pulserifle2");

            assert(m.load() == false);
        }
    }

    void test_md5_loadpart()
    {
        std::printf("testing md5 loadpart\n");

        md5 *m = generate_md5_model();

        assert(m->modelname() == "pulserifle");
        assert(m->parts.size() == 1);
        assert(m->parts[0]->meshes != nullptr);
        skelmodel::skelmesh *s = static_cast<skelmodel::skelmesh *>(m->parts[0]->meshes[0].meshes.at(0));
        assert(s != nullptr);
        assert(s->vertcount() == 438);
        assert(s->tricount() == 462);

        m->endload();
        delete m;
    }

    void test_md5_settag()
    {
        std::printf("testing md5 settag\n");

        md5 *m = generate_md5_model();

        float pos = 0;

        skelcommands<md5>::settag("X_pulse_muzzle", "tag_muzzle", &pos, &pos, &pos, &pos, &pos, &pos);
        skelmodel::skeleton *s = static_cast<skelmodel::skelmeshgroup *>(&(m->parts[0]->meshes[0]))->skel;
        assert(s->findtag("tag_muzzle") == 0);

        skelcommands<md5>::settag("X_pulse_base", "base", &pos, &pos, &pos, &pos, &pos, &pos);
        s = static_cast<skelmodel::skelmeshgroup *>(&(m->parts[0]->meshes[0]))->skel;
        assert(s->findtag("base") == 1);

        m->endload();
        delete m;
    }

    void test_md5_loadanim()
    {
        std::printf("testing md5 loadanim\n");

        md5 *m = generate_md5_model();

        //anims must be registered in this global first
        animnames.emplace_back("pulserifle");

        float speed = 30;
        int priority = 0;
        int offsets = 0;
        skelcommands<md5>::setanim("pulserifle", "pulserifle.md5anim", &speed, &priority, &offsets, &offsets);
        skelmodel::skelpart *p = static_cast<skelmodel::skelpart *>(m->parts[0]);
        assert(m->animated() == true);
        assert(p->animated() == true);

        m->loaded();
        m->endload();
        animnames.clear();
        delete m;
    }

    void test_md5_setpitchtarget()
    {
        std::printf("testing md5 setpitchtarget\n");

        md5 *m = generate_md5_model();

        //anims must be registered in this global first
        animnames.emplace_back("pulserifle");

        float speed = 30;
        int priority = 0,
            offsets = 0;

        int frameoffset = 1;
        float pitchmin = 1.f,
              pitchmax = 2.f;
        skelcommands<md5>::setanim("pulserifle", "pulserifle.md5anim", &speed, &priority, &offsets, &offsets);
        skelcommands<md5>::setpitchtarget("X_pulse_muzzle", "pulserifle.md5anim", &frameoffset, &pitchmin, &pitchmax);
        m->loaded();
        m->endload();
        animnames.clear();
        delete m;
    }

    void test_md5_setanimpart()
    {
        std::printf("testing md5 setanimpart\n");

        md5 *m = generate_md5_model();

        skelmodel::skelpart *p = static_cast<skelmodel::skelpart *>(m->parts[0]);

        assert(p->numanimparts == 1);
        assert(p->partmask.size() == 0);

        skelcommands<md5>::setanimpart("X_pulse_base");

        assert(p->numanimparts == 2);
        assert(p->partmask.size() == 0);

        m->load();

        assert(p->partmask.size() == 4);
        delete m;
    }

    void test_md5_setskin()
    {
        std::printf("testing md5 setskin\n");

        //mock existence of valid opengl context
        hwcubetexsize = 1024;
        hwtexsize = 1024;

        md5 *m = generate_md5_model();
        skelmodel::skelpart *p = static_cast<skelmodel::skelpart *>(m->parts[0]);
        p->initskins();
        std::printf("1\n");
        skelcommands<md5>::setskin("*", "blank.png", "blank.png");

        assert(p->skins.size() == 1);
        auto skinlist = skelcommands<md5>::getskins("*");
        assert(skinlist.size() == 1);
        assert((*skinlist[0]).tex != nullptr);
        assert((*skinlist[0]).masks != nullptr);

        m->load(); //if load called earlier, skinlist is empty
        m->endload();
        delete m;
        hwcubetexsize = 0;
        hwtexsize = 0;
    }

    void test_md5_setbumpmap()
    {
        std::printf("testing md5 setbumpmap\n");

        //mock existence of valid opengl context
        hwcubetexsize = 1024;
        hwtexsize = 1024;

        md5 *m = generate_md5_model();

        skelcommands<md5>::setbumpmap("*", "blank.png");

        auto skinlist = skelcommands<md5>::getskins("*");
        assert(skinlist.size() == 1);
        assert((*skinlist[0]).normalmap != nullptr);

        m->load();
        m->endload();

        delete m;
        hwcubetexsize = 0;
        hwtexsize = 0;
    }
}

void test_md5()
{
    std::printf(
"===============================================================\n\
testing md5 functionality\n\
===============================================================\n"
    );

    test_md5_ctor();
    test_md5_formatname();
    test_md5_flipy();
    test_md5_type();
    test_md5_newmeshes();
    test_md5_loaddefaultparts();
    test_md5_load();
    test_md5_loadpart();
    test_md5_settag();
    test_md5_loadanim();
    test_md5_setpitchtarget();
    test_md5_setanimpart();
    test_md5_setskin();
    test_md5_setbumpmap();
}

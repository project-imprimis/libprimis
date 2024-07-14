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
#include "../src/engine/model/skelmodel.h"
#include "../src/engine/model/md5.h"

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

void test_md5_loadpart()
{
    std::printf("testing md5 loadpart\n");

    md5 m("md5");
    m.startload();
    assert(md5::loading == &m);

    skelcommands<md5>::setdir(std::string("md5").data());
    float smooth = 0;
    skelcommands<md5>::loadpart("pulserifle.md5mesh", nullptr, &smooth);

    assert(m.modelname() == "md5");
    assert(m.parts.size() == 1);
    assert(m.parts[0]->meshes != nullptr);
    skelmodel::skelmesh *s = static_cast<skelmodel::skelmesh *>(m.parts[0]->meshes[0].meshes.at(0));
    assert(s != nullptr);
    assert(s->vertcount() == 438);
    assert(s->tricount() == 462);

    m.endload();
}

void test_md5_loadanim()
{
    std::printf("testing md5 loadanim\n");

    md5 m("md5");
    m.startload();
    assert(md5::loading == &m);

    skelcommands<md5>::setdir(std::string("md5").data());
    float smooth = 0;
    skelcommands<md5>::loadpart("pulserifle.md5mesh", nullptr, &smooth);

    //anims must be registered in this global first
    animnames.emplace_back("idle");

    float speed = 30;
    int priority = 0;
    int offsets = 0;
    skelcommands<md5>::setanim("idle", "idle.md5anim", &speed, &priority, &offsets, &offsets);
    skelmodel::skelpart *p = static_cast<skelmodel::skelpart *>(m.parts[0]);
    assert(m.animated() == true);
    assert(p->animated() == true);

    m.endload();
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
    test_md5_loadpart();
    test_md5_loadanim();
}

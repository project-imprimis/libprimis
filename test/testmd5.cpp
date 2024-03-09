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
}

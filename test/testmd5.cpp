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

void test_md5()
{
    std::printf(
"===============================================================\n\
testing md5 functionality\n\
===============================================================\n"
    );

    test_md5_ctor();
}

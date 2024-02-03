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

struct MinimalSkelModel : skelmodel
{
    MinimalSkelModel(std::string name) : skelmodel(name)
    {
    }

    int type() const final
    {
        return 0;
    }

    bool flipy() const final
    {
        return false;
    }

    bool loadconfig(const std::string &) final
    {
        return false;
    }

    bool loaddefaultparts() final
    {
        return false;
    }

    void startload() final
    {
    }

    void endload() final
    {
    }

    skelmeshgroup *newmeshes() final
    {
        return nullptr;
    }
};

void constructskel()
{
    std::printf("constructing a skelmodel object\n");
    MinimalSkelModel s = MinimalSkelModel(std::string("test"));
    assert(s.modelname() == "test");
}

void test_skel()
{
    std::printf(
"===============================================================\n\
testing skelmodel functionality\n\
===============================================================\n"
    );

    constructskel();
}

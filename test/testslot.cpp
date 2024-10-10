
#include "libprimis.h"
#include "../shared/geomexts.h"

namespace
{
    void test_slot_type()
    {
        std::printf("testing slot::type\n");
        Slot s;
        assert(s.type() == Slot::SlotType_Octa);
    }

    void test_slot_texturedir()
    {
        std::printf("testing slot::texturedir\n");
        Slot s;
        assert(s.texturedir() == std::string("media/texture"));
    }

    void test_slot_name()
    {
        std::printf("testing slot::name\n");
        Slot s;
        s.index = 1;
        assert(s.name() == std::string("slot 1"));
        s.index = 123;
        assert(s.name() == std::string("slot 123"));
    }

    void test_decalslot_type()
    {
        std::printf("testing decalslot::type\n");
        DecalSlot s;
        assert(s.type() == Slot::SlotType_Decal);
    }
}

void test_slot()
{
    std::printf(
"===============================================================\n\
testing slot functionality\n\
===============================================================\n"
    );
    test_slot_type();
    test_slot_texturedir();
    test_slot_name();
    test_decalslot_type();
}

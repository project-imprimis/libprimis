
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

    void test_slot_name()
    {
        std::printf("testing slot::name\n");
        Slot s;
        s.index = 1;
        assert(s.name() == std::string("slot 1"));
        s.index = 123;
        assert(s.name() == std::string("slot 123"));
    }

    void test_slot_texturedir()
    {
        std::printf("testing slot::texturedir\n");
        Slot s;
        assert(s.texturedir() == std::string("media/texture"));
    }

    void test_slot_cleanup()
    {
        std::printf("testing slot::cleanup\n");
        Slot s;
        s.sts.emplace_back();
        s.cleanup();
        assert(s.loaded == false);
        assert(s.grasstex == nullptr);
        assert(s.thumbnail == nullptr);
        assert(s.sts[0].t == nullptr);
        assert(s.sts[0].combined == -1);
    }

    void test_slot_shouldpremul()
    {
        std::printf("testing slot::shouldpremul\n");
        Slot s;
        assert(s.shouldpremul(0) == false);
        assert(s.shouldpremul(1) == false);
        assert(s.shouldpremul(99) == false);
    }

    void test_vslot_cleanup()
    {
        std::printf("testing vslot::cleanup\n");
        VSlot s;
        s.cleanup();
        assert(s.linked == false);
    }

    void test_decalslot_type()
    {
        std::printf("testing decalslot::type\n");
        DecalSlot s;
        assert(s.type() == Slot::SlotType_Decal);
    }

    void test_decalslot_name()
    {
        std::printf("testing decalslot::name\n");
        DecalSlot s;
        s.Slot::index = 1;
        assert(s.name() == std::string("decal slot 1"));
        s.Slot::index = 123;
        assert(s.name() == std::string("decal slot 123"));
    }

    void test_decalslot_texturedir()
    {
        std::printf("testing decalslot::texturedir\n");
        DecalSlot s;
        assert(s.texturedir() == std::string("media/decal"));
    }

    void test_decalslot_cleanup()
    {
        std::printf("testing decalslot::cleanup\n");
        DecalSlot s;
        s.cleanup();
        assert(s.loaded == false);
        assert(s.grasstex == nullptr);
        assert(s.thumbnail == nullptr);
        assert(s.linked == false);
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
    test_slot_name();
    test_slot_texturedir();
    test_slot_cleanup();
    test_slot_shouldpremul();
    test_vslot_cleanup();
    test_decalslot_type();
    test_decalslot_name();
    test_decalslot_texturedir();
    test_decalslot_cleanup();
}

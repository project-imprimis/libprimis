#include "libprimis.h"

#include "../src/libprimis-headers/prop.h"
#include "../src/engine/interface/cs.h"
#include <type_traits>

enum
{
    PropTest0 = 0,
    PropTest1,
    PropTest2,
    PropTest3,
    PropTest4,
    PropTest5,

    PropTestCount
};

const prop::PropertyMeta prop_meta[PropTestCount] =
{
    prop::PropertyMeta
    (
        "prop_test_0",
        prop::PropertyType::Int,
        -10, 0, 10
    ),
    prop::PropertyMeta
    (
        "prop_test_1",
        prop::PropertyType::Float,
        -5.0f, 0.0f, 5.0f
    ),
    prop::PropertyMeta
    (
        "prop_test_2",
        prop::PropertyType::Color,
        bvec(255, 255, 255)
    ),
    prop::PropertyMeta
    (
        "prop_test_3",
        prop::PropertyType::IntVec,
        ivec(-10, -10, -10), ivec(0, 0, 0), ivec(10, 10, 10)
    ),
    prop::PropertyMeta
    (
        "prop_test_4",
        prop::PropertyType::FloatVec,
        vec(-20.0f, -20.0f, -20.0f), vec(1.0f, 2.0f, 3.0f), vec(20.0f, 20.0f, 20.0f)
    ),
    prop::PropertyMeta
    (
        "prop_test_5",
        prop::PropertyType::String,
        "default"
    )
};

std::array<prop::Property<>, PropTestCount> props =
    prop::make_props_array<PropTestCount, prop::Property<>>(prop_meta);

static void try_find_props()
{
    std::printf("===============================================================\n");
    std::printf("Testing finding props:\n");
    std::printf("===============================================================\n");
    for(int i = 0; i < PropTestCount; i++)
    {
        std::printf("Finding prop %s...\n", prop_meta[i].get_name().c_str());
        prop::Property<>* prop = find_prop(prop_meta[i].get_name(), props);
        assert(prop);
    }
    std::printf("===============================================================\n");
}

static void try_defaults()
{
    std::printf("===============================================================\n");
    std::printf("Testing defaults:\n");
    std::printf("===============================================================\n");
    for(int i = 0; i < PropTestCount; i++)
    {
        std::printf("Checking prop %s...\n", prop_meta[i].get_name().c_str());
        prop::Property<>* prop = find_prop(prop_meta[i].get_name(), props);
        assert(prop);
        switch(i)
        {
            case 0:
                assert(prop->get_int() == 0);
                break;

            case 1:
                assert(prop->get_float() == 0.0f);
                break;

            case 2:
                assert(prop->get_color() == bvec(255, 255, 255));
                break;

            case 3:
                assert(prop->get_ivec() == ivec(0, 0, 0));
                break;

            case 4:
                assert(prop->get_fvec() == vec(1.0f, 2.0f, 3.0f));
                break;

            case 5:
                assert(prop->get_string() == "default");
                break;
        }
    }
    std::printf("===============================================================\n");
}

static void try_assignment()
{
    std::printf("===============================================================\n");
    std::printf("Testing assignment:\n");
    std::printf("===============================================================\n");
    for(int i = 0; i < PropTestCount; i++)
    {
        std::printf("Checking prop %s...\n", prop_meta[i].get_name().c_str());
        prop::Property<>* prop = find_prop(prop_meta[i].get_name(), props);
        assert(prop);
        switch(i)
        {
            case 0:
                set_prop(prop_meta[i].get_name(), 5, props);
                assert(prop->get_int() == 5);
                break;

            case 1:
                set_prop(prop_meta[i].get_name(), 5.0f, props);
                assert(prop->get_float() == 5.0f);
                break;

            case 2:
                set_prop(prop_meta[i].get_name(), bvec(0, 0, 0), props);
                assert(prop->get_color() == bvec(0, 0, 0));
                break;

            case 3:
                set_prop(prop_meta[i].get_name(), ivec(5, 5, 5), props);
                assert(prop->get_ivec() == ivec(5, 5, 5));
                break;

            case 4:
                set_prop(prop_meta[i].get_name(), vec(5.0f, 5.0f, 5.0f), props);
                assert(prop->get_fvec() == vec(5.0f, 5.0f, 5.0f));
                break;

            case 5:
                set_prop(prop_meta[i].get_name(), "test", props);
                assert(prop->get_string() == "test");
                break;
        }
    }
    std::printf("===============================================================\n");
}

static void try_assignment_clamp()
{
    std::printf("===============================================================\n");
    std::printf("Testing assignment clamping:\n");
    std::printf("===============================================================\n");
    for(int i = 0; i < PropTestCount; i++)
    {
        std::printf("Checking prop %s...\n", prop_meta[i].get_name().c_str());
        prop::Property<>* prop = find_prop(prop_meta[i].get_name(), props);
        assert(prop);
        switch(i)
        {
            case 0:
                set_prop(prop_meta[i].get_name(), -11, props);
                assert(prop->get_int() == -10);
                set_prop(prop_meta[i].get_name(), 11, props);
                assert(prop->get_int() == 10);
                break;

            case 1:
                set_prop(prop_meta[i].get_name(), -6.0f, props);
                assert(prop->get_float() == -5.0f);
                set_prop(prop_meta[i].get_name(), 6.0f, props);
                assert(prop->get_float() == 5.0f);
                break;

            case 2:
                // Clamping not supported for colors
                continue;

            case 3:
                set_prop(prop_meta[i].get_name(), ivec(-11, 5, 11), props);
                assert(prop->get_ivec() == ivec(-10, 5, 10));
                break;

            case 4:
                set_prop(prop_meta[i].get_name(), vec(-21, 5, 21), props);
                assert(prop->get_fvec() == vec(-20, 5, 20));
                break;

            case 5:
                // Tis a string, sir. No clamping here.
                continue;
        }
    }
    std::printf("===============================================================\n");
}

static void try_pack_unpack()
{
    std::array<prop::Property<>, PropTestCount> unpacked_props =
        prop::make_props_array<PropTestCount, prop::Property<>>(prop_meta);

    std::printf("===============================================================\n");
    std::printf("Testing prop pack/unpack:\n");
    std::printf("===============================================================\n");

    set_prop("prop_test_0", 3,                       props);
    set_prop("prop_test_1", -2.0f,                   props);
    set_prop("prop_test_2", bvec(128, 64, 32),       props);
    set_prop("prop_test_3", ivec(5, 4, 3),           props);
    set_prop("prop_test_4", vec(12.0f, -5.0f, 0.0f), props);
    set_prop("prop_test_5", "foobar",                props);

    std::vector<uint8_t> packed_props;

    pack_props(props, packed_props);
    unpack_props(packed_props, unpacked_props);

    for(int i = 0; i < PropTestCount; i++)
    {
        std::printf("Checking prop %s...\n", prop_meta[i].get_name().c_str());

        switch(i)
        {
            case 0:
                assert(unpacked_props[i].get_int() == 3);
                break;

            case 1:
                assert(unpacked_props[i].get_float() == -2.0f);
                break;

            case 2:
                assert(unpacked_props[i].get_color() == bvec(128, 64, 32));
                break;

            case 3:
                assert(unpacked_props[i].get_ivec() == ivec(5, 4, 3));
                break;

            case 4:
                assert(unpacked_props[i].get_fvec() == vec(12.0f, -5.0f, 0.0f));
                break;

            case 5:
                assert(unpacked_props[i].get_string() == "foobar");
                break;
        }
    }
}

static void try_callback()
{
    bool called = false;

    static prop::PropertyMeta prop_meta_with_cb[1] =
    {
        prop::PropertyMeta
        (
            "prop_test_cb_0",
            prop::PropertyType::Int,
            -10, 0, 10,
            [&called](std::any argument)
            {
                std::printf("Callback called! %s\n", std::any_cast<const char*>(argument));
                called = true;
            }
        )
    };

    std::array<prop::Property<>, 1> props_with_cb =
        prop::make_props_array<1, prop::Property<>>(prop_meta_with_cb);

    std::printf("===============================================================\n");
    std::printf("Testing prop callback:\n");
    std::printf("===============================================================\n");

    set_prop("prop_test_cb_0", 3, props_with_cb, "hello world");
    assert(called);
}

static void try_to_string()
{
    std::printf("Testing prop to_string\n");

    set_prop("prop_test_0", 1,                     props);
    set_prop("prop_test_1", -3.0f,                 props);
    set_prop("prop_test_2", bvec(1, 2, 4),         props);
    set_prop("prop_test_3", ivec(4, 2, 1),         props);
    set_prop("prop_test_4", vec(3.0f, 4.0f, 0.0f), props);
    set_prop("prop_test_5", "baz",                 props);

    std::array<std::string, 6> propstrings = {
        "1",
        "-3.000000",
        "1 2 4",
        "4 2 1",
        "3.000000 4.000000 0.000000",
        "baz"
    };

    for(size_t i = 0; i < PropTestCount; ++i)
    {
        assert(props[i].to_string() == propstrings[i]);
    }
}

static void try_cmd_result()
{
    std::printf("Testing prop cmd_result\n");

    set_prop("prop_test_0", 1,                     props);
    set_prop("prop_test_1", -3.0f,                 props);
    set_prop("prop_test_2", bvec(1, 2, 4),         props);
    set_prop("prop_test_3", ivec(4, 2, 1),         props);
    set_prop("prop_test_4", vec(3.0f, 4.0f, 0.0f), props);
    set_prop("prop_test_5", "baz",                 props);

    std::array<std::string, 3> propstrings = {
        "4 2 1",
        "3.000000 4.000000 0.000000",
        "baz"
    };

    {
        props[0].cmd_result();
        assert(commandret->getint() == 1);
    }
    {
        props[1].cmd_result();
        assert(commandret->getfloat() == -3.0f);
    }
    {
        props[2].cmd_result();
        assert(commandret->getint() == 0x010204);
    }
    {
        props[3].cmd_result();
        assert(std::strcmp(commandret->getstr(), propstrings[0].c_str()) == 0);
    }
    {
        props[4].cmd_result();
        assert(std::strcmp(commandret->getstr(), propstrings[1].c_str()) == 0);
    }
    {
        props[5].cmd_result();
        assert(std::strcmp(commandret->getstr(), propstrings[2].c_str()) == 0);
    }
}

void test_props()
{
    try_find_props();
    try_defaults();
    try_assignment();
    try_assignment_clamp();
    try_pack_unpack();
    try_callback();
    try_to_string();
    try_cmd_result();
}

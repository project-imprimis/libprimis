#include "libprimis.h"

#include "../src/libprimis-headers/prop.h"
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
    printf("===============================================================\n");
    printf("Testing finding props:\n");
    printf("===============================================================\n");
    for(int i = 0; i < PropTestCount; i++)
    {
        printf("Finding prop %s...\n", prop_meta[i].get_name().c_str());
        prop::Property<>* prop = find_prop(prop_meta[i].get_name(), props);
        assert(prop);
    }
    printf("===============================================================\n");
}

static void try_defaults()
{
    printf("===============================================================\n");
    printf("Testing defaults:\n");
    printf("===============================================================\n");
    for(int i = 0; i < PropTestCount; i++)
    {
        printf("Checking prop %s...\n", prop_meta[i].get_name().c_str());
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
    printf("===============================================================\n");
}

static void try_assignment()
{
    printf("===============================================================\n");
    printf("Testing assignment:\n");
    printf("===============================================================\n");
    for(int i = 0; i < PropTestCount; i++)
    {
        printf("Checking prop %s...\n", prop_meta[i].get_name().c_str());
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
    printf("===============================================================\n");
}

static void try_assignment_clamp()
{
    printf("===============================================================\n");
    printf("Testing assignment clamping:\n");
    printf("===============================================================\n");
    for(int i = 0; i < PropTestCount; i++)
    {
        printf("Checking prop %s...\n", prop_meta[i].get_name().c_str());
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
    printf("===============================================================\n");
}

static void try_pack_unpack()
{
    std::array<prop::Property<>, PropTestCount> unpacked_props =
        prop::make_props_array<PropTestCount, prop::Property<>>(prop_meta);

    printf("===============================================================\n");
    printf("Testing prop pack/unpack:\n");
    printf("===============================================================\n");

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
        printf("Checking prop %s...\n", prop_meta[i].get_name().c_str());

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
                printf("Callback called! %s\n", std::any_cast<const char*>(argument));
                called = true;
            }
        )
    };

    std::array<prop::Property<>, 1> props_with_cb =
        prop::make_props_array<1, prop::Property<>>(prop_meta_with_cb);

    printf("===============================================================\n");
    printf("Testing prop callback:\n");
    printf("===============================================================\n");

    set_prop("prop_test_cb_0", 3, props_with_cb, "hello world");
    assert(called);
}

void test_props()
{
    try_find_props();
    try_defaults();
    try_assignment();
    try_assignment_clamp();
    try_pack_unpack();
    try_callback();
}

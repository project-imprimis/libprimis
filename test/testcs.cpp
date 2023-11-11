
// test CS functions

#include "libprimis.h"
#include "../src/engine/interface/cs.h"

void testparsefloat()
{
    const char *s = "3.2 test";
    float conout = parsefloat(s);
    std::printf("parsefloat output: %f\n", conout);
    assert(conout == 3.2f);

    const char *s2 = "test 3.2";
    float conout2 = parsefloat(s2);
    std::printf("parsefloat output: %f\n", conout2);
    assert(conout2 == 0.f);
}

void testparsenumber()
{
    const char *s = "3.2 test";
    double conout = parsenumber(s);
    std::printf("parsenumber output: %f\n", conout);
    assert(conout == 3.2d);

    const char *s2 = "test 3.2";
    double conout2 = parsenumber(s2);
    std::printf("parsenumber output: %f\n", conout2);
    assert(conout2 == 0.d);
}

void testfloatformat()
{
    char str[260] = "";
    floatformat(str, 3.3, 260);
    std::printf("floatformat output: %s\n", str);
    assert(std::strcmp(str, "3.3") == 0);
}

void testintformat()
{
    char str[260] = "";
    floatformat(str, 3, 260);
    std::printf("intformat output: %s\n", str);
    assert(std::strcmp(str, "3.0") == 0);
}

void testparseword()
{
    const char *p = "test; test2";
    const char *conout = parseword(p);
    std::printf("parseword output: %s\n", conout);
    assert(std::strcmp(conout, "; test2") == 0);

    //test buffer underrun of bracket buffer
    const char *p2 = "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]";
    const char *conout2 = parseword(p2);
    std::printf("parseword output: %s\n", conout2);
    assert(std::strcmp(conout2, "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]") == 0);

    //test buffer overrun of bracket buffer
    const char *p3 = "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[";
    const char *conout3 = parseword(p3);
    std::printf("parseword output: %s\n", conout3);
    assert(std::strcmp(conout3, "[[[[[[[[") == 0);
}

void testconc()
{
    tagval tagvalargs[3];
    tagvalargs[0].type = Value_Integer;
    tagvalargs[0].i = 1;
    tagvalargs[1].type = Value_Integer;
    tagvalargs[1].i = 2;
    tagvalargs[2].type = Value_Integer;
    tagvalargs[2].i = 3;

    const char * conout = conc(tagvalargs, 3, true, "test");
    std::printf("conc output: %s\n", conout);
    assert(std::strcmp(conout, "test 1 2 3") == 0);

    const char * conout2 = conc(tagvalargs, 3, false, "test");
    std::printf("conc output: %s\n", conout2);
    assert(std::strcmp(conout2, "test123") == 0);

    tagval tagvalargmix[4];
    char tagvalstring[11] = "teststring";
    tagvalargmix[0].type = Value_Integer;
    tagvalargmix[0].i = 1;
    tagvalargmix[1].type = Value_Float;
    tagvalargmix[1].f = 1.1;
    tagvalargmix[2].type = Value_String;
    tagvalargmix[2].s = tagvalstring;

    const char * conout3 = conc(tagvalargmix, 3, true, "test");
    std::printf("conc output: %s\n", conout3);
    assert(std::strcmp(conout3, "test 1 1.1 teststring") == 0);
    const char * conout4 = conc(tagvalargmix, 3, false, "test");
    std::printf("conc output: %s\n", conout4);
    assert(std::strcmp(conout4, "test11.1teststring") == 0);
}

//command.h functions

void testescapestring()
{
    const char * teststr = "escapestring output: \n \f \t";
    const char * conout = escapestring(teststr);
    std::printf("%s\n", conout);
    assert(std::strcmp(conout, "\"escapestring output: ^n ^f ^t\"") == 0);
}

void testescapeid()
{
    const char * teststr = "escapeid output: \n \f \t";
    const char * conout = escapeid(teststr);
    std::printf("%s\n", conout);
    assert(std::strcmp(conout, "\"escapeid output: ^n ^f ^t\"") == 0);

    const char * teststr2 = "";
    const char * conout2 = escapeid(teststr2);
    std::printf("%s\n", conout2);
    assert(std::strcmp(conout2, "") == 0);
}

//run tests
void testcs()
{
    testparsefloat();
    testparsenumber();
    testfloatformat();
    testintformat();
    testparseword();
    testconc();
    //command.h
    testescapestring();
    testescapeid();
}

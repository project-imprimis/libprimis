
// test CS functions

#include "libprimis.h"
#include "../src/engine/interface/cs.h"

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

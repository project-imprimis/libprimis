
// test CS functions

#include "libprimis.h"
#include "../src/engine/interface/cs.h"


namespace
{
    constexpr float tolerance = 0.001;

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
        assert(conout == 3.2);

        const char *s2 = "test 3.2";
        double conout2 = parsenumber(s2);
        std::printf("parsenumber output: %f\n", conout2);
        assert(conout2 == 0.0);
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

    void test_validateblock()
    {
        std::printf("testing validateblock\n");

        assert(validateblock("[[[") == false);
        assert(validateblock("]]]") == false);
        assert(validateblock("[[[]]]") == true);
        assert(validateblock("[[[]]]][") == false);
        assert(validateblock("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[\
                              [[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[\
                              ]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]\
                              ]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]") == false);

        assert(validateblock("(((") == false);
        assert(validateblock(")))") == false);
        assert(validateblock("((()))") == true);
        assert(validateblock("((())))(") == false);
        assert(validateblock("((((((((((((((((((((((((((((((((((((((((((((((((((\
                              (((((((((((((((((((((((((((((((((((((((((((((((((((\
                              )))))))))))))))))))))))))))))))))))))))))))))))))))\
                              ))))))))))))))))))))))))))))))))))))))))))))))))))") == false);

        assert(validateblock("\"") == false);
        assert(validateblock("\"\"") == true);

        assert(validateblock("@") == false);
        assert(validateblock("\f") == false);
        assert(validateblock("/") == true);
        assert(validateblock("//") == false);

    }

    void test_cs_command(const std::vector<std::pair<std::string, int>> &inputs)
    {
        for(const std::pair<std::string, int> &i : inputs)
        {
            int val = execute(i.first.c_str());
            std::printf("executing %s -> %d\n", i.first.c_str(), val);
            assert(val == i.second);
        }
    }

    /**
     * @brief Returns a vector of tagval results from a std::vector of input strings to execute
     */
    std::vector<tagval> generate_command_tagvals(const std::vector<std::string> &inputs)
    {
        std::vector<tagval> outputs;
        for(const std::string_view i : inputs)
        {
            tagval t;
            executeret(i.data(), t);
            outputs.push_back(t);
        }
        return outputs;
    }

    /**
     * @brief Checks whether a vector of CS commands and float results match
     *
     * If a comparison check fails, an assertion will fail and terminate the tests.
     *
     * @param inputs a vector of pairs of inputs and expected results
     */
    void test_cs_command_float(const std::vector<std::pair<std::string, float>> &inputs)
    {
        std::vector<std::string> inputstrings;
        for(const std::pair<std::string, float> &i : inputs)
        {
            inputstrings.push_back(i.first);
        }
        std::vector<tagval> outputs = generate_command_tagvals(inputstrings);
        //create vector of tagval generated outputs, expected outputs
        std::vector<std::pair<tagval, float>> outputcombo;
        for(size_t i = 0; i < outputs.size(); ++i)
        {
            outputcombo.emplace_back(outputs[i], inputs[i].second);
        }

        for(const std::pair<tagval, float> &i : outputcombo)
        {
            float f = i.first.getfloat();
            std::printf("float result: %f\n", f);
            assert(std::abs(f - i.second) < tolerance);
        }
    }

    /**
     * @brief Checks whether a vector of CS commands and string results match
     *
     * If a comparison check fails, an assertion will fail and terminate the tests.
     *
     * @param inputs a vector of pairs of inputs and expected results
     */
    void test_cs_command_string(const std::vector<std::pair<std::string, std::string>> &inputs)
    {
        std::vector<std::string> inputstrings;
        for(const std::pair<std::string, std::string> &i : inputs)
        {
            inputstrings.push_back(i.first);
        }
        std::vector<tagval> outputs = generate_command_tagvals(inputstrings);
        //create vector of tagval generated outputs, expected outputs
        std::vector<std::pair<tagval, std::string>> outputcombo;
        for(size_t i = 0; i < outputs.size(); ++i)
        {
            outputcombo.emplace_back(outputs[i], inputs[i].second);
        }

        for(const std::pair<tagval, std::string> &i : outputcombo)
        {
            std::string s = i.first.getstr();
            std::printf("string result: %s\n", s.c_str());
            assert(s == i.second);
        }
    }

    void test_cs_plus()
    {
        std::printf("testing CS + command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"+ 1", 1},
            {"+ 1 2", 3},
            {"+ 1 2 3", 6},
            {"+f 1", 1},
            {"+f 1 2", 3},
            {"+f 1 2 3", 6}
        };

        test_cs_command(inputs);
    }

    void test_cs_mul()
    {
        std::printf("testing CS * command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"* 1", 1},
            {"* 1 2", 2},
            {"* 1 2 3", 6},
            {"*f 1", 1},
            {"*f 1 2", 2},
            {"*f 1 2 3", 6}
        };

        test_cs_command(inputs);
    }

    void test_cs_minus()
    {
        std::printf("testing CS - command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"- 1", -1},
            {"- 1 2", -1},
            {"- 1 2 3", -4},
            {"-f 1", -1},
            {"-f 1 2", -1},
            {"-f 1 2 3", -4}
        };

        test_cs_command(inputs);
    }

    void test_cs_equals()
    {
        std::printf("testing CS = command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"= 1", 0},
            {"= 0", 1},
            {"= 1 1", 1},
            {"= 1 1 1", 1},
            {"= 1 0", 0},
            {"=f 1", 0},
            {"=f 0", 1},
            {"=f 1 1", 1},
            {"=f 1 1 1", 1},
            {"=f 1 0", 0},
            {"=s test test", 1},
            {"=s test test2", 0},
            {"=s test test test test", 1}
        };

        test_cs_command(inputs);
    }

    void test_cs_nequals()
    {
        std::printf("testing CS != command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"!= 1", 1},
            {"!= 0", 0},
            {"!= 1 1", 0},
            {"!= 1 1 1", 0},
            {"!= 1 0", 1},
            {"!=f 1", 1},
            {"!=f 0", 0},
            {"!=f 1 1", 0},
            {"!=f 1 1 1", 0},
            {"!=f 1 0", 1},
            {"!=s test test", 0},
            {"!=s test test2", 1},
            {"!=s test test test test", 0}
        };

        test_cs_command(inputs);
    }

    void test_cs_lessthan()
    {
        std::printf("testing CS < command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"< 1", 0},
            {"< 0", 0},
            {"< 1 1", 0},
            {"< 0 1 2", 1},
            {"< 0 1 0", 0},
            {"< 1 0", 0},
            {"<f 1", 0},
            {"<f 0", 0},
            {"<f 1 1", 0},
            {"<f 0 1 2", 1},
            {"<f 0 1 0", 0},
            {"<f 1 0", 0},
            {"<s test test2", 1},
            {"<s test2 test", 0},
            {"<s test test2 test3", 1},
            {"<s test test test", 0}
        };

        test_cs_command(inputs);
    }

    void test_cs_greaterthan()
    {
        std::printf("testing CS > command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"> 1", 1},
            {"> 0", 0},
            {"> 1 1", 0},
            {"> 2 1 0", 1},
            {"> 0 1 0", 0},
            {"> 0 1", 0},
            {">f 1", 1},
            {">f 0", 0},
            {">f 1 1", 0},
            {">f 2 1 0", 1},
            {">f 0 1 0", 0},
            {">f 0 1", 0},
            {">s test test2", 0},
            {">s test2 test", 1},
            {">s test3 test2 test", 1},
            {">s test test test", 0}
        };

        test_cs_command(inputs);
    }

    void test_cs_lessequalthan()
    {
        std::printf("testing CS <= command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"<= 1", 0},
            {"<= 0", 1},
            {"<= 1 1", 1},
            {"<= 0 1 2", 1},
            {"<= 0 1 0", 0},
            {"<= 1 0", 0},
            {"<=f 1", 0},
            {"<=f 0", 1},
            {"<=f 1 1", 1},
            {"<=f 0 1 2", 1},
            {"<=f 0 1 0", 0},
            {"<=f 1 0", 0},
            {"<=s test test2", 1},
            {"<=s test2 test", 0},
            {"<=s test test2 test3", 1},
            {"<=s test test test", 1}
        };

        test_cs_command(inputs);
    }

    void test_cs_greaterequalthan()
    {
        std::printf("testing CS >= command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {">= 1", 1},
            {">= 0", 1},
            {">= 1 1", 1},
            {">= 2 1 0", 1},
            {">= 0 1 0", 0},
            {">= 0 1", 0},
            {">=f 1", 1},
            {">=f 0", 1},
            {">=f 1 1", 1},
            {">=f 2 1 0", 1},
            {">=f 0 1 0", 0},
            {">=f 0 1", 0},
            {">=s test test2", 0},
            {">=s test2 test", 1},
            {">=s test3 test2 test", 1},
            {">=s test test test", 1}
        };

        test_cs_command(inputs);
    }

    void test_cs_bitwise_xor()
    {
        std::printf("testing CS ^ command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"^ 1 1", 0},
            {"^ 2 1", 3},
            {"^ 2 2 1", 1},
            {"^ 4 2 1", 7},
            {"^ -1 -2 -4", -3},
            {"^ -1 4 2 1", -8},
            {"^ -10 4 2 1", -15},
        };

        test_cs_command(inputs);
    }

    void test_cs_bitwise_and()
    {
        std::printf("testing CS & command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"&", 0},
            {"& 1", 1},
            {"& 1 1", 1},
            {"& 2 1", 0},
            {"& 2 2 1", 0},
            {"& 2 2", 2},
            {"& 4 2 1", 0},
            {"& -1 -2 -4", -4},
            {"& -1 4 2 1", 0},
            {"& -10 4 2 1", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_bitwise_or()
    {
        std::printf("testing CS | command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"| 1 1", 1},
            {"| 2 1", 3},
            {"| 2 2 1", 3},
            {"| 4 2 1", 7},
            {"| -1 -2 -4", -1},
            {"| -1 4 2 1", -1},
            {"| -10 4 2 1", -9},
        };

        test_cs_command(inputs);
    }

    void test_cs_bitwise_xnor()
    {
        std::printf("testing CS ^~ command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"^~ ", 0},
            {"^~ 1 1", -1},
            {"^~ 2 1", -4},
            {"^~ 2 2 1", 1},
            {"^~ 4 2 1", 7},
            {"^~ -1 -2 -4", -3},
            {"^~ -1 4 2 1", 7},
            {"^~ -10 4 2 1", 14},
        };

        test_cs_command(inputs);
    }

    void test_cs_bitwise_nand()
    {
        std::printf("testing CS &~ command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"&~", 0},
            {"&~ 1", 1},
            {"&~ 1 1", 0},
            {"&~ 2 1", 2},
            {"&~ 2 2 1", 0},
            {"&~ 2 2", 0},
            {"&~ 4 2 1", 4},
            {"&~ -1 -2 -4", 1},
            {"&~ -1 4 2 1", -8},
            {"&~ -10 4 2 1", -16},
        };

        test_cs_command(inputs);
    }

    void test_cs_bitwise_nor()
    {
        std::printf("testing CS |~ command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"|~ ", 0},
            {"|~ 1 1", -1},
            {"|~ 2 1", -2},
            {"|~ 2 2 1", -1},
            {"|~ 4 2 1", -1},
            {"|~ -1 -2 -4", -1},
            {"|~ -1 4 2 1", -1},
            {"|~ -10 4 2 1", -1},
        };

        test_cs_command(inputs);
    }

    void test_cs_shl()
    {
        std::printf("testing CS << command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"<< 1 1", 2},
            {"<< 1 1 1", 4},
            {"<< 3 2", 12},
            {"<< 16 -1", 16},
            {"<< 1 33", 0},
            {"<< 1 32", 0},
            {"<< 1 31", -2147483648},
            {"<< 1 30", 1073741824},
            {"<< 1", 1},
            {"<<", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_shr()
    {
        std::printf("testing CS >> command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {">> 1 1", 0},
            {">> 8 1 1", 2},
            {">> 3 1", 1},
            {">> -8 1", -4},
            {">> -8 4", -1},
            {">> -8 5", -1},
            {">> 16 -1", 16},
            {">> 2147483648 33", -1},
            {">> 2147483648 32", -1},
            {">> 2147483648 31", -1},
            {">> 2147483648 30", -2},
            {">> 1", 1},
            {">>", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_not()
    {
        std::printf("testing CS ! command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"! 1", 0},
            {"! 0", 1},
            {"! -1", 0},
            {"! a", 0},
            {"! 0.1", 0},
            {"! 0.0", 1},
        };

        test_cs_command(inputs);
    }

    void test_cs_and()
    {
        std::printf("testing CS && command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"&&  1 1", 1},
            {"&& 1", 1},
            {"&&", 1},
            {"&& 1 1 1", 1},
            {"&& 1 1 1 1 1 0", 0}
        };

        test_cs_command(inputs);
    }

    void test_cs_or()
    {
        std::printf("testing CS || command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"||  1 1", 1},
            {"|| 1", 1},
            {"|| 1 1 1", 1},
            {"|| 1 1 1 1 1 0", 1},
            {"||", 0},
            {"|| 0", 0},
            {"|| 0 0", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_div()
    {
        std::printf("testing CS div(f) command\n");

        std::vector<std::pair<std::string, int>> intinputs = {
            {"div 0 1", 0},
            {"div 1 1", 1},
            {"div 1 10", 0.1},
            {"div 1 3", 0},
            {"div 25 100", 0},
            {"div 125 100", 1}
        };

        test_cs_command(intinputs);

        std::vector<std::pair<std::string, float>> floatinputs = {
            {"divf 0 1", 0.f},
            {"divf 1 1", 1.f},
            {"divf 1 10", 0.1f},
            {"divf 1 3", 0.33333f},
            {"divf 25 100", 0.25f},
            {"divf 125 100", 1.25f}
        };

        test_cs_command_float(floatinputs);
    }

    void test_cs_mod()
    {
        std::printf("testing CS mod(f) command\n");

        std::vector<std::pair<std::string, int>> intinputs = {
            {"mod 0 1", 0},
            {"mod 1 1", 0},
            {"mod 1 10", 1},
            {"mod 1 3", 1},
            {"mod 25 100", 25},
            {"mod 25 100 6", 1},
            {"mod 25 100 18 3", 1}
        };
        test_cs_command(intinputs);

        std::vector<std::pair<std::string, float>> floatinputs = {
            {"modf 0 1", 0.f},
            {"modf 1 1", 0.f},
            {"modf 1 10", 1.f},
            {"modf 1 3", 1.f},
            {"modf 25 100", 25.f},
            {"modf 25 100 6", 1.f},
            {"modf 25 100 18 3", 1.f}
        };

        test_cs_command_float(floatinputs);
    }

    void test_cs_pow()
    {
        std::printf("testing CS pow command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"pow", 0},
            {"pow 0", 0},
            {"pow 1", 1},
            {"pow 2 2", 4},
            {"pow 2 2 2", 16},
            {"pow 2 3 2", 64},
            {"pow 2 2 2 2", 256},
        };

        test_cs_command_float(inputs);
    }

    //trancendental tests

    void test_cs_sin()
    {
        std::printf("testing CS sin command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"sin 0", 0},
            {"sin 90", 1},
            {"sin 180", 0},
            {"sin 270", -1},
            {"sin 360", 0}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_cos()
    {
        std::printf("testing CS cos command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"cos 0", 1},
            {"cos 90", 0},
            {"cos 180", -1},
            {"cos 270", 0},
            {"cos 360", 1}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_tan()
    {
        std::printf("testing CS tan command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"tan 0", 0},
            {"tan 80", 5.6713f},
            {"tan 180", 0},
            {"tan 260", 5.6713f},
            {"tan 360", 0}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_asin()
    {
        std::printf("testing CS asin command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"asin -1", -90.f},
            {"asin -0.5", -30.f},
            {"asin 0", 0},
            {"asin 0.5", 30.f},
            {"asin 1", 90.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_acos()
    {
        std::printf("testing CS acos command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"acos -1", 180.f},
            {"acos -0.5", 120.f},
            {"acos 0", 90.f},
            {"acos 0.5", 60.f},
            {"acos 1", 0.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_atan()
    {
        std::printf("testing CS atan command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"atan -1", -45.f},
            {"atan -0.5", -26.565f},
            {"atan 0", 0.f},
            {"atan 0.5", 26.565f},
            {"atan 1", 45.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_atan2()
    {
        std::printf("testing CS atan command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"atan2 -1 1", -45.f},
            {"atan2 -1 2", -26.565f},
            {"atan2 0 1", 0.f},
            {"atan2 2 4", 26.565f},
            {"atan2 6 6", 45.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_sqrt()
    {
        std::printf("testing CS sqrt command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"sqrt 0.25", 0.5f},
            {"sqrt 1", 1.f},
            {"sqrt 16", 4.f},
            {"sqrt 625", 25.f},
            {"sqrt 2", 1.4142f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_loge()
    {
        std::printf("testing CS loge command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"loge 0.3679", -1.f},
            {"loge 1", 0.f},
            {"loge 2.7183", 1.f},
            {"loge 7.3891", 2.f},
            {"loge 20.086", 3.f},
            {"loge 148.41", 5.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_log2()
    {
        std::printf("testing CS log2 command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"log2 0.5", -1.f},
            {"log2 1", 0.f},
            {"log2 2", 1.f},
            {"log2 4", 2.f},
            {"log2 1024", 10.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_log10()
    {
        std::printf("testing CS log10 command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"log10 0.1", -1.f},
            {"log10 1", 0.f},
            {"log10 10", 1.f},
            {"log10 100", 2.f},
            {"log10 1000", 3.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_exp()
    {
        std::printf("testing CS exp command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"exp -1", 0.3679f},
            {"exp 0", 1.f},
            {"exp 1", 2.7183f},
            {"exp 2", 7.3891f},
            {"exp 5", 148.4132f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_min()
    {
        std::printf("testing CS min(f) command\n");

        std::vector<std::pair<std::string, float>> floatinputs = {
            {"minf -1", -1.f},
            {"minf", 0.f},
            {"minf 1 2 3 4", 1.f},
            {"minf -1 -1 -1", -1.f}
        };

        test_cs_command_float(floatinputs);

        std::vector<std::pair<std::string, int>> intinputs = {
            {"min -1 0 ", -1},
            {"min", 0},
            {"min 1 2 3 4", 1},
            {"min -1 -1 -1", -1},
        };

        test_cs_command(intinputs);
    }

    void test_cs_max()
    {
        std::printf("testing CS max(f) command\n");

        std::vector<std::pair<std::string, float>> floatinputs = {
            {"maxf -1", -1.f},
            {"maxf", 0.f},
            {"maxf 1 2 3 4", 4.f},
            {"maxf -1 -1 -1", -1.f}
        };

        test_cs_command_float(floatinputs);

        std::vector<std::pair<std::string, int>> intinputs = {
            {"max -1 0 ", 0},
            {"max", 0},
            {"max 1 2 3 4", 4},
            {"max -1 -1 -1", -1},
        };

        test_cs_command(intinputs);
    }

    void test_cs_bitscan()
    {
        std::printf("testing CS bitscan command\n");

        std::vector<std::pair<std::string, int>> intinputs = {
            {"bitscan", -1},
            {"bitscan 1 ", 0},
            {"bitscan 4", 2},
            {"bitscan 3", 0},
            {"bitscan -1", 0},
        };

        test_cs_command(intinputs);
    }


    void test_cs_abs()
    {
        std::printf("testing CS abs(f) command\n");

        std::vector<std::pair<std::string, float>> floatinputs = {
            {"absf", 0.f},
            {"absf -1", 1.f},
            {"absf 0", 0.f},
            {"absf 1", 1.f},
            {"absf 2", 2.f}
        };

        test_cs_command_float(floatinputs);

        std::vector<std::pair<std::string, int>> intinputs = {
            {"abs", 0},
            {"abs -1", 1},
            {"abs 0", 0},
            {"abs 1", 1},
            {"abs 2", 2}
        };

        test_cs_command(intinputs);
    }

    void test_cs_floor()
    {
        std::printf("testing CS floor command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"floor", 0.f},
            {"floor -1", -1.f},
            {"floor -1.5", -2.f},
            {"floor 0", 0.f},
            {"floor 1.5", 1.f},
            {"floor 3", 3.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_ceil()
    {
        std::printf("testing CS ceil command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"ceil", 0.f},
            {"ceil -1", -1.f},
            {"ceil -1.5", -1.f},
            {"ceil 0", 0.f},
            {"ceil 1.5", 2.f},
            {"ceil 3", 3.f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_round()
    {
        std::printf("testing CS round command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"round", 0.f},
            {"round 1", 1.f},
            {"round -1", -1.f},
            {"round -1 1", -1.f},
            {"round -1.5 0.5", -1.5f},
            {"round -1.5 1", -2.f},
            {"round -1.5 0", -2.f},
            {"round 1.5 0.5", 1.5f},
            {"round 1.5 1", 2.f},
            {"round 1.5 0", 2.f},
            {"round 0 1", 0.f},
            {"round 1.111 0.1", 1.1f},
            {"round 3.333 0.3", 3.3f},
            {"round 2.333 0.3", 2.4f}
        };

        test_cs_command_float(inputs);
    }

    void test_cs_cond()
    {
        std::printf("testing CS cond command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"cond", 0},
            {"cond 1", 0},
            {"cond 1 1", 1},
            {"cond 1 1 0", 1},
            {"cond 0 1 0", 0},
            {"cond 0 1 1 2 0", 2},
            {"cond 0 1 0 2 3", 3},
        };

        test_cs_command(inputs);
    }

    void test_cs_tohex()
    {
        std::printf("testing CS tohex command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"tohex 1", "0x1"},
            {"tohex 0", "0x0"},
            {"tohex 255", "0xFF"},
            {"tohex 256", "0x100"},
            {"tohex -1", "0xFFFFFFFF"},
            {"tohex -255", "0xFFFFFF01"},
            {"tohex 1 0", "0x1"},
            {"tohex 1 1", "0x1"},
            {"tohex 1 2", "0x01"},
            {"tohex 1 5", "0x00001"},
            {"tohex 32 5", "0x00020"},
            {"tohex 32 0", "0x20"},
            {"tohex 32 1", "0x20"},
            {"tohex 32 -1", "0x20"},
            {"tohex", "0x0"},
        };

        test_cs_command_string(inputs);
    }

    //initstrcmds

    void test_cs_strstr()
    {
        std::printf("testing CS strstr command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"strstr test e", 1},
            {"strstr", 0},
            {"strstr testing ing", 4},
            {"strstr testingng ng", 5}
        };

        test_cs_command(inputs);
    }

    void test_cs_strlen()
    {
        std::printf("testing CS strlen command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"strlen test", 4},
            {"strlen test test", 9},
            {"strlen test, test", 10},
            {"strlen", 0}
        };

        test_cs_command(inputs);
    }

    void test_cs_strsplice()
    {
        std::printf("testing CS strsplice command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"strsplice teststring test 1 1", "ttestststring"},
            {"strsplice teststring test 2 1", "tetesttstring"},
            {"strsplice teststring test 0 4", "teststring"},
            {"strsplice teststring t 0 4", "tstring"},
            {"strsplice teststring t 0 30", "t"},
            {"strsplice teststring test 7 3", "teststrtest"},
            {"strsplice teststring test 7 4", "teststrtest"},
            {"strsplice teststring test 7 2", "teststrtestg"},
            {"strsplice teststring test 9 2", "teststrintest"},
            {"strsplice teststring t", "tteststring"},
            {"strsplice teststring t", "tteststring"},
            {"strsplice teststring", "teststring"},
            {"strsplice", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_strreplace()
    {
        std::printf("testing CS strreplace command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"strreplace teststring test t", "tstring"},
            {"strreplace teststring t t tt", "testtstring"},
            {"strreplace ababababab s t", "ababababab"},
            {"strreplace ababababab a c", "cbcbcbcbcb"},
            {"strreplace ababababab a c d", "cbdbcbdbcb"},
            {"strreplace", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_stripcolors()
    {
        std::printf("testing CS stripcolors command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"stripcolors teststring", "teststring"},
            {"stripcolors \f0teststring", "teststring"},
            {"stripcolors \f2\f4teststring", "teststring"},
            {"stripcolors", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_concat()
    {
        std::printf("testing CS concat command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"concat test test2", "test test2"},
            {"concat test", "test"},
            {"concat test test test test", "test test test test"},
            {"concat test, test, test, test", "test, test, test, test"},
            {"concat t t", "t t"},
            {"concat", ""}
        };

        test_cs_command_string(inputs);
    }

    void test_cs_concatword()
    {
        std::printf("testing CS concatword command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"concatword test test2", "testtest2"},
            {"concatword test", "test"},
            {"concatword test test test test", "testtesttesttest"},
            {"concatword test, test, test, test", "test,test,test,test"},
            {"concatword t t", "tt"},
            {"concatword", ""}
        };

        test_cs_command_string(inputs);
    }

    void test_cs_if()
    {
        std::printf("testing CS if command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"if 1 [result 2] [result 3]", 2},
            {"if 1 2 3", 2},
            {"if 0 [result 2] [result 3]", 3},
            {"if 0 2 3", 3},
            {"if 1", 0},
            {"if 0", 0},
            {"if", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_ternary()
    {
        std::printf("testing CS ? command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"? 1 2 3", 2},
            {"? 0 2 3", 3},
            {"? 1", 0},
            {"? 0", 0},
            {"?", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_result()
    {
        std::printf("testing CS result command\n");

        std::vector<std::pair<std::string, std::string>> stringinputs = {
            {"test = \"teststring\"; result $test", "teststring"},
            {"test = \"teststring\"; result test", "test"},
            {"result 1", "1"},
            {"result  ", ""},
            {"result", ""},
        };

        test_cs_command_string(stringinputs);

        std::vector<std::pair<std::string, float>> floatinputs = {
            {"test = 4.0; result $test", 4.f},
            {"test = 4.0; result test", 0.f},
            {"result 1.000", 1.f},
            {"result  ", 0.f},
            {"result", 0.f},
        };

        test_cs_command_float(floatinputs);

        std::vector<std::pair<std::string, int>> intinputs = {
            {"test = 4; result $test", 4},
            {"result 4", 4},
            {"test = 4; result test", 0},
            {"result  ", 0},
            {"result", 0},
        };

        test_cs_command(intinputs);
    }

    void test_cs_listlen()
    {
        std::printf("testing CS listlen command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"listlen test", 1},
            {"listlen", 0},
            {"listlen test test test", 3},
        };

        test_cs_command(inputs);
    }

    void test_cs_at()
    {
        std::printf("testing CS at command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"at test", "test"},
            {"at test 0", "test"},
            {"at [test1 test2 test3] 2", "test3"},
            {"at \"test1 test2 test3\" 2", "test3"},
            {"at [test1 test2 test3] -1", "test1"},
            {"at", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_sublist()
    {
        std::printf("testing CS sublist command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"sublist test 0 1", "test"},
            {"sublist \"test1 test2 test3\" 1 1", "test2"},
            {"sublist \"test1 test2 test3\" 1 3", "test2 test3"},
            {"sublist \"test1 test2 test3\" -1 3", "test1 test2 test3"},
            {"sublist \"test1 [test2 test3] test4\" -1 3", "test1 [test2 test3] test4"},
            {"sublist \"test1 test2 test3\" 1 0", ""},
            {"sublist \"test1 test2 test3\" 1 -1", ""},
            {"sublist \"test1 test2 test3\" 1", "test2 test3"},
            {"sublist \"test1 test2 test3\"", "test1 test2 test3"},
            {"sublist", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_listcount()
    {
        std::printf("testing CS listcount command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"listcount i \"test\" [=s $i test]", 1},
            {"listcount i \"test test\" [=s $i test]", 2},
            {"listcount i \"test test test2\" [=s $i test]", 2},
            {"listcount i [test test test2] [=s $i test]", 2},
            {"listcount i \"test test test2\" [result 1]", 3},
            {"listcount i \"\" [=s $i test]", 0},
            {"listcount i \"\" []", 0},
            {"listcount i", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_listfind()
    {
        std::printf("testing CS listfind command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"listfind i \"test\" [=s $i test]", 0},
            {"listfind i \"test\" [=s $i test2]", -1},
            {"listfind i \"alpha bravo charlie\" [=s $i bravo]", 1},
            {"listfind i [alpha bravo charlie] [=s $i bravo]", 1},
            {"listfind i \"alpha bravo charlie bravo\" [=s $i bravo]", 1},
            {"listfind i \"alpha bravo \0charlie bravo\" [=s $i charlie]", -1},
            {"listfind i \"alpha bravo [\0charlie] bravo\" [=s $i charlie]", -1},
            {"listfind", -1},
            {"listfind i", -1},
            {"listfind listfind", -1},
            {"listfind listfind \"test\" [=s $listfind test]", -1},
            {"listfind i \"test test test\" [=s $i test]", 0},
            {"listfind i [test [test] test] [=s $i test]", 0},
            {"listfind i \"(test) [test] test\" [=s $i test]", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_loop()
    {
        std::printf("testing CS loop command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"testval = 0; loop i 10 [testval = (+ $testval 1)]; result $testval", 10},
            {"testval = 0; loop i 10 [testval = (+ $testval $i)]; result $testval", 45},
            {"testval = 0; loop i 10 [testval = (+ $testval $i)]; result $i", 0},
            {"testval = 0; loop i 0 [testval = (+ $testval 1)]; result $testval", 0},
            {"testval = 0; loop i 'test' [testval = (+ $testval 1)]; result $testval", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_loopplus()
    {
        std::printf("testing CS loop+ command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"testval = 0; loop+ i 2 10 [testval = (+ $testval 1)]; result $testval", 10},
            {"testval = 0; loop+ i 2 10 [testval = (+ $testval $i);]; result $testval", 65},
            {"testval = 0; loop+ i 2 10 [testval = $i]; result $testval", 11},
            {"testval = 0; loop+ i 5 10 [testval = $i]; result $testval", 14},
            {"testval = 0; loop+ i 2 10 [testval = (+ $testval $i)]; result $i", 0},
            {"testval = 0; loop+ i 2 0 [testval = (+ $testval 1)]; result $testval", 0},
            {"testval = 0; loop+ i 2 'test' [testval = (+ $testval 1)]; result $testval", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_loopmul()
    {
        std::printf("testing CS loop* command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"testval = 0; loop* i 2 8 [testval = (+ $testval 1)]; result $testval", 8},
            {"testval = 0; loop* i 2 6 [testval = (+ $testval $i);]; result $testval", 30},
            {"testval = 0; loop* i 2 4 [testval = $i]; result $testval", 6},
            {"testval = 0; loop* i 2 10 [testval = (+ $testval $i)]; result $i", 0},
            {"testval = 0; loop* i 2 0 [testval = (+ $testval 1)]; result $testval", 0},
            {"testval = 0; loop* i 2 'test' [testval = (+ $testval 1)]; result $testval", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_loopplusmul()
    {
        std::printf("testing CS loop+* command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"testval = 0; loop+* i 2 2 8 [testval = (+ $testval 1)]; result $testval", 8},
            {"testval = 0; loop+* i 2 2 6 [testval = (+ $testval $i);]; result $testval", 42},
            {"testval = 0; loop+* i 2 2 4 [testval = $i]; result $testval", 8},
            {"testval = 0; loop+* i 2 0 4 [testval = $i]; result $testval", 2},
            {"testval = 0; loop+* i 0 2 4 [testval = $i]; result $testval", 6},
            {"testval = 0; loop+* i 2 2 10 [testval = (+ $testval $i)]; result $i", 0},
            {"testval = 0; loop+* i 2 2 0 [testval = (+ $testval 1)]; result $testval", 0},
            {"testval = 0; loop+* i 2 2 'test' [testval = (+ $testval 1)]; result $testval", 0},
        };

        test_cs_command(inputs);
    }

    void test_cs_loopconcat()
    {
        std::printf("testing CS loopconcat command\n");

        std::vector<std::pair<std::string, std::string>> strinputs = {
            {"loopconcat i 3 [result 1]", "1 1 1"},
            {"loopconcat i 10 [result $i]", "0 1 2 3 4 5 6 7 8 9"},
            {"loopconcat i 10 [loopconcat j 1 [result 1]]", "1 1 1 1 1 1 1 1 1 1"},
            {"loopconcat i 3 [format test]", "test test test"},
            {"loopconcat i 3 [concatword test test2]", "testtest2 testtest2 testtest2"},
            {"loopconcat i 2", " "},
            {"loopconcat i", ""},
            {"loopconcat", ""},
        };

        test_cs_command_string(strinputs);
    }

    void test_cs_loopconcatplus()
    {
        std::printf("testing CS loopconcat+ command\n");

        std::vector<std::pair<std::string, std::string>> strinputs = {
            {"loopconcat+ i 3 3 [result 1]", "1 1 1"},
            {"loopconcat+ i 5 5 [result $i]", "5 6 7 8 9"},
            {"loopconcat+ i 3 10 [loopconcat j 1 [result 1]]", "1 1 1 1 1 1 1 1 1 1"},
            {"loopconcat+ i 2 3 [format test]", "test test test"},
            {"loopconcat+ i 5 3 [concatword test test2]", "testtest2 testtest2 testtest2"},
            {"loopconcat+ i 2 2", " "},
            {"loopconcat+ i 2", ""},
            {"loopconcat+ i", ""},
            {"loopconcat+", ""},
        };

        test_cs_command_string(strinputs);
    }

    void test_cs_listassoceq()
    {
        std::printf("testing CS listassoc= command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"listassoc= \"1 2 3\" 1", "2"},
            {"listassoc= \"1 2 3 4\" 1", "2"},
            {"listassoc= \"2 3 4\" 1", ""},
            {"listassoc= \"1 3 4\" 1", "3"},
            {"listassoc= \"2 3 1 4\" 1", "4"},
            {"listassoc= \"2 1 2 3 2 4 2 5\" 2", "1"},
            {"listassoc= \"1 2 1 4\" 1", "2"},
            {"listassoc= \"0 1 2\" 1", ""},
            {"listassoc= \"1\" 1", ""},
            {"listassoc=", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_prettylist()
    {
        std::printf("testing CS prettylist command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"prettylist \"alpha bravo charlie\" and", "alpha, bravo, and charlie"},
            {"prettylist \"alpha bravo charlie delta echo\" and", "alpha, bravo, charlie, delta, and echo"},
            {"prettylist \"\" and", ""},
            {"prettylist \"1 2 3 4 5\" or", "1, 2, 3, 4, or 5"},
            {"prettylist [1 2 3 4 5] or", "1, 2, 3, 4, or 5"},
            {"prettylist \"1 2 3 4 5\"", "1, 2, 3, 4, 5"},
            {"prettylist \"1 2\" or", "1 or 2"},
            {"prettylist \"1 2\"", "1, 2"},
            {"prettylist \"1\" or", "1"},
            {"prettylist", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_indexof()
    {
        std::printf("testing CS indexof command\n");

        std::vector<std::pair<std::string, int>> inputs = {
            {"indexof \"alpha bravo charlie\" alpha", 0},
            {"indexof \"alpha bravo charlie delta\" delta", 3},
            {"indexof [alpha bravo charlie delta] delta", 3},
            {"indexof \"alpha alpha charlie delta\" charlie", 2},
            {"indexof \"alpha alpha charlie delta\" echo", -1},
            {"indexof \"1 2 3 4\" 2", 1},
            {"indexof \"1 1.0 3 4\" 1.0", 1},
            {"indexof \"1 1.0 3 4\"", -1},
            {"indexof \"1 0 3 4\"", -1},
            {"indexof \"1 \"\" 3 4\"", -1},
            {"indexof", -1},
        };

        test_cs_command(inputs);
    }

    void test_cs_listdel()
    {
        std::printf("testing CS listdel command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"listdel \"alpha\" \"alpha\"", ""},
            {"listdel \"alpha alpha\" \"alpha\"", ""},
            {"listdel \"alpha alpha bravo\" \"bravo\"", "alpha alpha"},
            {"listdel \"alpha alpha bravo\" \"alpha\"", "bravo"},
            {"listdel \"alpha bravo charlie alpha\" \"alpha bravo\"", "charlie"},
            {"listdel", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_listintersect()
    {
        std::printf("testing CS listintersect command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"listintersect \"alpha\" \"alpha\"", "alpha"},
            {"listintersect \"alpha alpha\" \"alpha\"", "alpha alpha"},
            {"listintersect \"alpha\" \"alpha alpha\"", "alpha"},
            {"listintersect \"alpha bravo\" \"alpha charlie\"", "alpha"},
            {"listintersect \"alpha bravo charlie\" \"alpha charlie delta\"", "alpha charlie"},
            {"listintersect \"alpha charlie\" \"charlie alpha\"", "alpha charlie"},
            {"listintersect", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_listunion()
    {
        std::printf("testing CS listunion command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"listunion \"alpha\" \"alpha\"", "alpha"},
            {"listunion \"alpha alpha\" \"alpha\"", "alpha alpha"},
            {"listunion \"alpha bravo\" \"alpha alpha\"", "alpha bravo"},
            {"listunion \"alpha bravo\" \"alpha charlie\"", "alpha bravo charlie"},
            {"listunion \"alpha bravo charlie\" \"charlie alpha delta\"", "alpha bravo charlie delta"},
            {"listunion \"alpha bravo\" \"bravo alpha\"", "alpha bravo"},
            {"listunion", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_listsplice()
    {
        std::printf("testing CS listsplice command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"listsplice \"alpha\" \"bravo\" 1 1", "alpha bravo"},
            {"listsplice \"alpha\" \"bravo\" 0 0", "bravo alpha"},
            {"listsplice \"alpha\" \"bravo\" 0 1", "bravo"},
            {"listsplice \"alpha bravo charlie\" \"delta echo\" 2 1", "alpha bravo delta echo"},
            {"listsplice \"alpha bravo charlie\" \"delta echo\" 2 0", "alpha bravo delta echo charlie"},
            {"listsplice \"alpha bravo charlie\" \"delta echo\" 1 2", "alpha delta echo"},
            {"listsplice", ""},
        };

        test_cs_command_string(inputs);
    }

    void test_cs_sortlist()
    {
        std::printf("testing CS sortlist command\n");

        std::vector<std::pair<std::string, std::string>> stringinputs = {
            {"testval = \"alpha bravo charlie\"; sortlist $testval a b [<s $a $b]", "alpha bravo charlie"},
            {"testval = \"alpha bravo charlie\"; sortlist $testval a b [>s $a $b]", "charlie bravo alpha"},
            {"testval = \"charlie bravo alpha\"; sortlist $testval a b [<s $a $b]", "alpha bravo charlie"},
            {"testval = \"3 2 1\"; sortlist $testval a b [< $a $b]", "1 2 3"},
            {"testval = \"3.0 2.0 1.0\"; sortlist $testval a b [<f $a $b]", "1.0 2.0 3.0"},
            //test unique sortlist
            {"testval = \"charlie charlie bravo alpha\"; sortlist $testval a b [<s $a $b] [=s $a $b]", "alpha bravo charlie"},
            {"testval = \"3 3 2 1\"; sortlist $testval a b [< $a $b] [= $a $b]", "1 2 3"},
            {"testval = \"3.0 3.0 2.0 1.0\"; sortlist $testval a b [<f $a $b] [=f $a $b]", "1.0 2.0 3.0"},
            {"sortlist", ""},
        };

        test_cs_command_string(stringinputs);
    }

    void test_cs_uniquelist()
    {
        std::printf("testing CS uniquelist command\n");

        std::vector<std::pair<std::string, std::string>> stringinputs = {
            {"testval = \"alpha alpha alpha\"; uniquelist $testval a b [=s $a $b]", "alpha"},
            {"testval = \"alpha bravo charlie\"; uniquelist $testval a b [=s $a $b]", "alpha bravo charlie"},
            {"testval = \"charlie bravo alpha\"; uniquelist $testval a b [=s $a $b]", "charlie bravo alpha"},
            {"testval = \"alpha bravo alpha\"; uniquelist $testval a b [=s $a $b]", "alpha bravo"},
            {"testval = \"1 2 1\"; uniquelist $testval a b [= $a $b]", "1 2"},
            {"testval = \"1.0 2.0 1.0\"; uniquelist $testval a b [=f $a $b]", "1.0 2.0"},
            {"uniquelist", ""}
        };

        test_cs_command_string(stringinputs);
    }
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
    test_cs_plus();
    test_cs_mul();
    test_cs_minus();
    test_cs_equals();
    test_cs_nequals();
    test_cs_lessthan();
    test_cs_greaterthan();
    test_cs_lessequalthan();
    test_cs_greaterequalthan();
    test_cs_bitwise_or();
    test_cs_bitwise_and();
    test_cs_bitwise_xor();
    test_cs_bitwise_nor();
    test_cs_bitwise_nand();
    test_cs_bitwise_xnor();
    test_cs_shl();
    test_cs_shr();
    test_cs_not();
    test_cs_and();
    test_cs_or();
    test_cs_div();
    test_cs_mod();
    test_cs_pow();
    test_cs_sin();
    test_cs_cos();
    test_cs_tan();
    test_cs_asin();
    test_cs_acos();
    test_cs_atan();
    test_cs_atan2();
    test_cs_sqrt();
    test_cs_loge();
    test_cs_log2();
    test_cs_log10();
    test_cs_exp();
    test_cs_min();
    test_cs_max();
    test_cs_bitscan();
    test_cs_abs();
    test_cs_floor();
    test_cs_ceil();
    test_cs_round();
    test_cs_cond();
    test_cs_tohex();
    //strcmds
    test_cs_strstr();
    test_cs_strlen();
    test_cs_strsplice();
    test_cs_strreplace();
    test_cs_stripcolors();
    test_cs_concat();
    test_cs_concatword();
    //controlcmds
    test_cs_if();
    test_cs_ternary();
    test_cs_result();
    test_cs_listlen();
    test_cs_at();
    test_cs_sublist();
    test_cs_listcount();
    test_cs_listfind();
    test_cs_loop();
    test_cs_loopplus();
    test_cs_loopmul();
    test_cs_loopplusmul();
    test_cs_loopconcat();
    test_cs_loopconcatplus();
    test_cs_listassoceq();
    test_cs_prettylist();
    test_cs_indexof();
    test_cs_listdel();
    test_cs_listintersect();
    test_cs_listunion();
    test_cs_listsplice();
    test_cs_sortlist();
    test_cs_uniquelist();
    //command.h
    testescapestring();
    testescapeid();
    test_validateblock();
}

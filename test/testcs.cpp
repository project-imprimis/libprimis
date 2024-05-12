
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

    void test_cs_command(const std::vector<std::pair<std::string, int>> &inputs)
    {
        for(const std::pair<std::string, int> &i : inputs)
        {
            int val = execute(i.first.c_str());
            std::printf("executing %s\n", i.first.c_str());
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

    void test_cs_divf()
    {
        std::printf("testing CS divf command\n");

        std::vector<std::pair<std::string, float>> inputs = {
            {"divf 0 1", 0},
            {"divf 1 1", 1},
            {"divf 1 10", 0.1},
            {"divf 1 3", 0.33333},
            {"divf 25 100", 0.25}
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

    void test_cs_concat()
    {
        std::printf("testing CS concat command\n");

        std::vector<std::pair<std::string, std::string>> inputs = {
            {"concat test test2", "test test2"},
            {"concat test", "test"},
            {"concat test test test test", "test test test test"},
            {"concat test, test, test, test", "test, test, test, test"},
            {"concat t t", "t t"}
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
            {"concatword t t", "tt"}
        };

        test_cs_command_string(inputs);
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
    test_cs_divf();
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
    //strcmds
    test_cs_strstr();
    test_cs_strlen();
    test_cs_concat();
    test_cs_concatword();
    //command.h
    testescapestring();
    testescapeid();
}

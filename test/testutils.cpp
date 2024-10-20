#include "libprimis.h"
#include "../shared/stream.h"

namespace
{
    void testpath()
    {
        static std::string_view test_cases[][2] =
        {
            {
                "data/textures/image.png",
                "data/textures/image.png"
            },
            {
                "data/../data/textures/image.png",
                "data/textures/image.png"
            },
            {
                "../data/../data/textures/image.png",
                "../data/textures/image.png"
            },
            {
                "data/.././data/textures/other_image.png",
                "data/textures/other_image.png"
            },
            {
                "data/textures/image.png&data/textures/other_image.png",
                "data/textures/image.png"
            },
            {
                "data/../data/textures/image.png&data/../data/textures/other_image.png",
                "data/textures/image.png"
            },
            {
                "<command:0.5f,0.25f/1.0f,0.33f>data/textures/image.png",
                "data/textures/image.png"
            },
            {
                "<command:0.5f,0.25f/1.0f,0.33f>data/../data/./textures/image.png",
                "data/textures/image.png"
            },
            {
                "<command:0.5f,0.25f/1.0f,0.33f>data/textures/image.png&data/textures/other_image.png",
                "data/textures/image.png"
            },
            {
                "<command:0.5f,0.25f/1.0f,0.33f>data/../data/./textures/image.png&data/../data/textures/other_image.png",
                "data/textures/image.png"
            },
            {
                "./data/sounds/music.ogg",
                "data/sounds/music.ogg"
            },
            {
                "../data/other/file.dat",
                "../data/other/file.dat"
            }
        };

        for(const auto &test_case : test_cases)
        {
            std::string before = std::string(test_case[0]);
            std::string after  = path(before);

            std::printf("Testing path %s -> %s\n", before.c_str(), after.c_str());

            assert(after == test_case[1]);
        }
    }

    void testcpath()
    {
        //note: <command> tests do not behave the same way as std::string path
        static const char * test_cases[][2] =
        {
            {
                "data/textures/image.png",
                "data/textures/image.png"
            },
            {
                "data/../data/textures/image.png",
                "data/textures/image.png"
            },
            {
                "../data/../data/textures/image.png",
                "../data/textures/image.png"
            },
            {
                "data/.././data/textures/other_image.png",
                "data/textures/other_image.png"
            },
            {
                "data/textures/image.png&data/textures/other_image.png",
                "data/textures/image.png&data/textures/other_image.png"
            },
            {
                "data/../data/textures/image.png&data/../data/textures/other_image.png",
                "data/textures/image.png"
            },
            {
                "<command:0.5f,0.25f/1.0f,0.33f>data/textures/image.png",
                "<command:0.5f,0.25f/1.0f,0.33f>data/textures/image.png"
            },
            {
                "<command:0.5f,0.25f/1.0f,0.33f>data/../data/./textures/image.png",
                "<command:0.5f,0.25f/1.0f,0.33f>data/textures/image.png"
            },
            {
                "<command:0.5f,0.25f/1.0f,0.33f>data/textures/image.png&data/textures/other_image.png",
                "<command:0.5f,0.25f/1.0f,0.33f>data/textures/image.png&data/textures/other_image.png"
            },
            {
                "<command:0.5f,0.25f/1.0f,0.33f>data/../data/./textures/image.png&data/../data/textures/other_image.png",
                "<command:0.5f,0.25f/1.0f,0.33f>data/textures/image.png"
            },
            {
                "./data/sounds/music.ogg",
                "data/sounds/music.ogg"
            },
            {
                "../data/other/file.dat",
                "../data/other/file.dat"
            }
        };

        for(const auto &test_case : test_cases)
        {
            const char * before = test_case[0];
            char s[500];
            char * after = copystring(s, before, 500);
            path(after);

            std::printf("Testing cpath %s -> %s\n", before, after);

            assert(std::string(after) == std::string(test_case[1]));
        }
    }

    void testparentdir()
    {
        static const char * test_cases[][2] =
        {
            {
                "data/textures/image.png",
                "data/textures"
            },
            {
                "./data/sounds/music.ogg",
                "./data/sounds"
            },
            {
                "../data/other/file.dat",
                "../data/other"
            },
            {
                "/data/other//file.dat",
                "/data/other/"
            },
            {
                "/data/other/",
                "/data/other"
            },
            {
                "/data/other//",
                "/data/other/"
            },
        };

        for(const auto &test_case : test_cases)
        {
            const char * before = test_case[0];
            const char * after  = parentdir(before);

            std::printf("Testing parentdir %s -> %s\n", before, after);

            assert(std::string(after) == std::string(test_case[1]));
        }
    }

    void testfixpackagedir()
    {
        static const char * test_cases[][2] =
        {
            {
                "data/textures",
                "data/textures/"
            },
            {
                "config/ui/",
                "config/ui/"
            },
            {
                "data/ui\\",
                "data/ui/"
            },
        };

        for(const auto &test_case : test_cases)
        {
            const char * before = test_case[0];
            char s[260];
            char * after = copystring(s, before, 260);
            fixpackagedir(after);

            std::printf("Testing fixpackagedir %s -> %s\n", before, after);

            assert(std::string(after) == std::string(test_case[1]));
        }
    }

    void testcopystring()
    {
        //test copy with enough chars
        std::printf("Testing string copy\n");
        char s[260];
        const char *s1 = "test string";
        copystring(s, s1, 260);
        assert(std::strcmp(s1, s) == 0);

        //test copy with not enough chars
        std::printf("Testing string copy, short string\n");
        const int len = 7;
        char s2[len];
        const char *s3 = "test string";
        copystring(s2, s3, len);
        assert(std::strcmp(s2, std::string(s3).substr(0, len-1).c_str()) == 0);
    }

    void testconcatstring()
    {
        std::printf("Testing concat string\n");
        char s[260];
        const char *s1 = "test string";
        std::strcpy(s, s1);
        const char *s2 = "append";

        concatstring(s, s2, 260);
        std::printf("test string length: %lu\n" , std::strlen(s));
        assert(std::strlen(s) == std::strlen(s1) + std::strlen(s2));
    }

    void test_matchstring()
    {
        std::printf("Testing match string\n");
        std::string a("test1");
        std::string b("test2");
        assert(matchstring(a,5,a));
        assert(matchstring(a,5,b) == false);
        assert(matchstring(a,1,a) == false);
    }

    void test_vector_putint()
    {
        std::printf("Testing putint/getint (std::vector)\n");

        std::vector<uchar> v;
        putint(v, 3);
        assert(getint(v) == 3);
        assert(v.size() == 0);
        putint(v, -999);
        assert(getint(v) == -999);
        assert(v.size() == 0);
    }

    void test_vector_putfloat()
    {
        std::printf("Testing putfloat/getfloat (std::vector)\n");

        std::vector<uchar> v;
        putfloat(v, 3.3f);
        assert(getfloat(v) == 3.3f);
        assert(v.size() == 0);
        putfloat(v, -999.99f);
        assert(getfloat(v) == -999.99f);
        assert(v.size() == 0);
    }

    void test_databuf_put()
    {
        std::printf("Testing databuf<>::put\n");
        std::array<int, 4> buf;
        databuf<int> d(buf.data(),4);

        d.put(3);
        assert(d.length() == 1);
        assert(d.buf[0] == 3);
    }

    void test_databuf_get()
    {
        std::printf("Testing databuf<>::get\n");
        std::array<int, 4> buf{1,2,3,4};
        databuf<int> d(buf.data(),4);
        int out = d.get();
        assert(out == 1);
    }

    void test_databuf_reset()
    {
        std::printf("Testing databuf<>::reset\n");
        std::array<int, 4> buf;
        buf.fill(0);
        databuf<int> d(buf.data(),4);

        d.put(3);
        assert(d.length() == 1);
        d.reset();
        assert(d.length() == 0);
    }

    void test_databuf_empty()
    {
        std::printf("Testing databuf<>::empty\n");
        std::array<int, 4> buf;
        databuf<int> d(buf.data(),4);
        assert(d.empty());
        d.put(3);
        assert(d.empty() == false);
    }

    void test_databuf_remaining()
    {
        std::printf("Testing databuf<>::remaining\n");
        std::array<int, 4> buf;
        databuf<int> d(buf.data(),4);
        assert(d.remaining() == 4);
        d.put(3);
        assert(d.remaining() == 3);
    }
}

void testutils()
{
    std::printf(
"===============================================================\n\
testing tools functionality\n\
===============================================================\n"
    );

    testpath();
    testcpath();
    testcopystring();
    testconcatstring();
    test_matchstring();
    testparentdir();
    testfixpackagedir();
    test_vector_putint();
    test_vector_putfloat();
    test_databuf_put();
    test_databuf_get();
    test_databuf_reset();
    test_databuf_empty();
    test_databuf_remaining();
}

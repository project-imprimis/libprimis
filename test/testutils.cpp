#include "libprimis.h"
#include "../shared/stream.h"

namespace header_tools
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
}

void testutils()
{
    header_tools::testpath();
    header_tools::testcpath();
    header_tools::testcopystring();
    header_tools::testconcatstring();
    header_tools::testparentdir();
    header_tools::testfixpackagedir();
}

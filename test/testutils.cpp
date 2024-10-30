#include "libprimis.h"
#include "../shared/stream.h"

namespace
{
    constexpr float tolerance = 0.001;

    void test_min()
    {
        std::printf("Testing min\n");

        assert(min(1,2,3) == 1);
        assert(min(1,1,1) == 1);
        assert(min(1.f,2.f,3.f) == 1.f);
    }

    void test_max()
    {
        std::printf("Testing max\n");

        assert(max(1,2,3) == 3);
        assert(max(1,1,1) == 1);
        assert(max(1.f,2.f,3.f) == 3.f);
    }

    void test_bitscan()
    {
        std::printf("Testing bitscan\n");

        assert(BITSCAN(0) == -1);
        assert(BITSCAN(1) == 0);
        assert(BITSCAN(128) == 7);
    }

    void test_detrnd()
    {
        std::printf("Testing detrnd\n");

        assert(std::abs(detrnd(0, 8) - 0) < tolerance);
        assert(std::abs(detrnd(1, 8) - 6) < tolerance);
        assert(std::abs(detrnd(2, 8) - 4) < tolerance);
    }

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

    void test_newconcatstring()
    {
        std::printf("Testing new concat string\n");
        const char *s1 = "test string";
        const char *s2 = "second test string";
        char *c = newconcatstring(s1, s2);
        assert(std::string(c) == "test stringsecond test string");
        delete[] c;
    }

    void test_endianswap()
    {
        std::printf("Testing endianswap\n");
        //int
        {
            int i = -1;
            assert(endianswap(i) == -1);
        }
        {
            int i = 0;
            assert(endianswap(i) == 0);
        }
        {
            int i = 32768;
            assert(endianswap(i) == 8388608);
        }
        //uint
        {
            uint i = 1;
            assert(endianswap(i) == 16777216);
        }
        {
            uint i = 0;
            assert(endianswap(i) == 0);
        }
        {
            uint i = 32768;
            assert(endianswap(i) == 8388608);
        }
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

    void test_databuf_putint()
    {
        std::printf("Testing putint/getint (databuf)\n");

        std::array<uchar, 100> buf;
        ucharbuf v(buf.data(),100);

        putint(v, 3);
        v.reset();
        assert(getint(v) == 3);
        v.reset();
        putint(v, -999);
        v.reset();
        assert(getint(v) == -999);
        v.reset();
        putint(v, 32769);
        v.reset();
        assert(getint(v) == 32769);
    }

    void test_databuf_putuint()
    {
        std::printf("Testing putuint/getuint (databuf)\n");

        std::array<uchar, 100> buf;
        ucharbuf v(buf.data(),100);

        putuint(v, 3);
        v.reset();
        assert(getuint(v) == 3);
        v.reset();
        putuint(v, -1);
        v.reset();
        assert(getuint(v) == -1);
        v.reset();
        putuint(v, 999);
        v.reset();
        assert(getuint(v) == 999);
        v.reset();
        putuint(v, 65636);
        v.reset();
        assert(getuint(v) == 65636);
    }

    void test_vector_putfloat()
    {
        std::printf("Testing putfloat/getfloat (std::vector)\n");

        std::vector<uchar> v;
        putfloat(v, 3.3f);
        assert(std::abs(getfloat(v) - 3.3f) < tolerance);
        assert(v.size() == 0);
        putfloat(v, -999.99f);
        assert(getfloat(v) == -999.99f);
        assert(v.size() == 0);
    }

    void test_databuf_putfloat()
    {
        std::printf("Testing putfloat/getfloat (databuf)\n");

        std::array<uchar, 100> buf;
        ucharbuf v(buf.data(),100);

        putfloat(v, 3.3f);
        v.reset();
        assert(std::abs(getfloat(v) - 3.3f) < tolerance);
        v.reset();
        putfloat(v, -999.99f);
        v.reset();
        assert(getfloat(v) == -999.99f);
    }

    void test_databuf_sendstring()
    {
        std::printf("Testing sendstring/getstring (databuf)\n");

        {
            std::array<uchar, 100> buf;
            ucharbuf v(buf.data(),100);

            sendstring("test", v);
            v.reset();
            std::array<char, 100> buf2;
            getstring(buf2.data(), v, 5);
            assert(std::string(buf2.data()) == std::string("test"));
        }
        {
            std::array<uchar, 100> buf;
            ucharbuf v(buf.data(),100);

            sendstring("test", v);
            v.reset();
            char buf2[5];
            getstring(buf2, v);
            assert(std::string(buf2) == std::string("test"));
        }
    }

    void test_databuf_subbuf()
    {
        std::printf("Testing databuf<>::subbuf\n");

        std::array<int, 4> buf{1,2,3,4};
        databuf<int> d(buf.data(),4);
        databuf<int> d2 = d.subbuf(2);
        assert(d2.remaining() == 2);
        assert(d2.getbuf()[0] == 1);
        assert(d2.getbuf()[1] == 2);
    }

    void test_databuf_pad()
    {
        std::printf("Testing databuf<>::pad\n");
        std::array<int, 4> buf;
        databuf<int> d(buf.data(),4);
        d.pad(1);
        assert(d.length() == 1);
        d.pad(2);
        assert(d.length() == 3);
    }

    void test_databuf_put()
    {
        std::printf("Testing databuf<>::put\n");
        {
            std::array<int, 4> buf;
            databuf<int> d(buf.data(), 4);

            d.put(3);
            assert(d.length() == 1);
            assert(d.buf[0] == 3);
        }
        {
            std::array<int, 1> buf;
            databuf<int> d(buf.data(), 1);

            d.put(3);
            d.put(3);
            assert(d.length() == 1);
            assert(d.overwrote());
        }
        {
            std::array<int, 4> buf;
            databuf<int> d(buf.data(), 4);
            std::array<int, 2> buf2{1,2};
            d.put(buf2.data(),2);
            assert(d.length() == 2);
            assert(d.buf[0] == 1);
            assert(d.buf[1] == 2);
        }
        {
            std::array<int, 2> buf;
            databuf<int> d(buf.data(), 2);
            std::array<int, 4> buf2{1,2,3,4};
            d.put(buf2.data(),4);
            assert(d.overwrote() == true);
            assert(d.length() == 2);
            assert(d.buf[0] == 1);
            assert(d.buf[1] == 2);
        }
    }

    void test_databuf_get()
    {
        std::printf("Testing databuf<>::get\n");
        {
            std::array<int, 4> buf{1,2,3,4};
            databuf<int> d(buf.data(),4);
            int out = d.get();
            assert(out == 1);
            assert(d.overread() == false);
        }
        {
            std::array<int, 1> buf{1};
            databuf<int> d(buf.data(),1);
            int out = d.get();
            assert(out == 1);
            assert(d.overread() == false);
            out = d.get();
            assert(d.overread() == true);
        }
    }

    void test_databuf_offset()
    {
        std::printf("Testing databuf<>::offset\n");
        std::array<int, 4> buf{1,2,3,4};
        databuf<int> d(buf.data(),4);
        d.offset(1);
        int out = d.get();
        assert(out == 2);
    }

    void test_databuf_reset()
    {
        std::printf("Testing databuf<>::reset\n");
        {
            std::array<int, 4> buf;
            buf.fill(0);
            databuf<int> d(buf.data(),4);

            d.put(3);
            assert(d.length() == 1);
            d.reset();
            assert(d.length() == 0);
        }
        {
            std::array<int, 4> buf;
            buf.fill(0);
            databuf<int> d(buf.data(),4);
            std::array<int, 3> buf2;
            assert(d.buf == buf.data());
            d.reset(buf2.data(), 3);
            assert(d.buf == buf2.data());
            assert(d.maxlen == 3);
        }
    }

    void test_databuf_getbuf()
    {
        std::printf("Testing databuf<>::getbuf\n");
        std::array<int, 4> buf;
        databuf<int> d(buf.data(),4);
        int *dbuf = d.getbuf();
        assert(dbuf == buf.data());
        assert(d.overread() == false);
    }

    void test_databuf_empty()
    {
        std::printf("Testing databuf<>::empty\n");
        std::array<int, 4> buf;
        databuf<int> d(buf.data(),4);
        assert(d.empty());
        d.put(3);
        assert(d.empty() == false);
        assert(d.overread() == false);
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

    void test_databuf_check()
    {
        std::printf("Testing databuf<>::check\n");
        std::array<int, 4> buf;
        databuf<int> d(buf.data(),4);
        assert(d.check(4) == true);
        assert(d.check(5) == false);
        d.put(3);
        assert(d.check(4) == false);
        assert(d.check(3) == true);
    }

    void test_stream_overloadable()
    {
        std::printf("Testing virtual overloadable std::stream methods:\ntell\nrawtell\nseek\nread\nwrite\nflush\ngetcrc\n");
        struct basic_stream : stream
        {
            void close() override
            {
            }
            bool end() override
            {
                return false;
            }
        };
        basic_stream s;

        assert(s.tell() == -1);
        assert(s.rawtell() == -1);
        assert(s.seek(0,0) == false);
        assert(s.read(nullptr, 0) == 0);
        assert(s.write(nullptr, 0) == 0);
        assert(s.flush() == true);
        assert(s.getcrc() == 0);
    }
}

void testutils()
{
    std::printf(
"===============================================================\n\
testing tools functionality\n\
===============================================================\n"
    );

    test_max();
    test_min();
    test_bitscan();
    test_detrnd();
    testpath();
    testcpath();
    testcopystring();
    testconcatstring();
    test_newconcatstring();
    test_matchstring();
    testparentdir();
    testfixpackagedir();
    test_endianswap();
    test_vector_putint();
    test_databuf_putint();
    test_databuf_putuint();
    test_vector_putfloat();
    test_databuf_putfloat();
    test_databuf_sendstring();
    test_databuf_subbuf();
    test_databuf_pad();
    test_databuf_put();
    test_databuf_get();
    test_databuf_offset();
    test_databuf_reset();
    test_databuf_getbuf();
    test_databuf_empty();
    test_databuf_remaining();
    test_databuf_check();
    test_stream_overloadable();
}

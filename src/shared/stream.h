
#ifndef STREAM_H_
#define STREAM_H_

template<class T>
struct streambuf
{
    stream *s;

    streambuf(stream *s) : s(s) {}

    T get() { return s->get<T>(); }
    size_t get(T *vals, size_t numvals) { return s->get(vals, numvals); }
    void put(const T &val) { s->put(&val, 1); }
    void put(const T *vals, size_t numvals) { s->put(vals, numvals); }
    void push_back(const T &val) { put(val); }
    size_t length() { return s->size(); }
};

enum
{
    CubeType_Print   = 1 << 0,
    CubeType_Space   = 1 << 1,
    CubeType_Digit   = 1 << 2,
    CubeType_Alpha   = 1 << 3,
    CubeType_Lower   = 1 << 4,
    CubeType_Upper   = 1 << 5,
    CubeType_Unicode = 1 << 6
};

extern const uchar cubectype[256];
inline int iscubeprint(uchar c) { return cubectype[c] & CubeType_Print; }
inline int iscubespace(uchar c) { return cubectype[c] & CubeType_Space; }
inline int iscubealpha(uchar c) { return cubectype[c] & CubeType_Alpha; }
inline int iscubealnum(uchar c) { return cubectype[c]&(CubeType_Alpha | CubeType_Digit); }
inline int iscubepunct(uchar c) { return cubectype[c] == CubeType_Print; }
inline int cube2uni(uchar c)
{
    return c;
}

inline uchar cubelower(uchar c)
{
    return c;
}
inline uchar cubeupper(uchar c)
{
    return c;
}
extern size_t encodeutf8(uchar *dstbuf, size_t dstlen, const uchar *srcbuf, size_t srclen, size_t *carry = nullptr);

extern char *loadfile(const char *fn, size_t *size, bool utf8 = true);
extern int listfiles(const char *dir, const char *ext, std::vector<char *> &files);
extern int listzipfiles(const char *dir, const char *ext, std::vector<char *> &files);
extern bool findzipfile(const char *filename);
extern const char *addpackagedir(const char *dir);
extern const char *parentdir(const char *directory);
extern bool fileexists(const char *path, const char *mode);
extern bool createdir(const char *path);
extern size_t fixpackagedir(char *dir);
extern char *makerelpath(const char *dir, const char *file, const char *prefix = nullptr, const char *cmd = nullptr);

extern void sendstring(const char *t, std::vector<uchar> &p);

extern void putint(std::vector<uchar> &p, int n);
extern int getint(std::vector<uchar> &p);
extern void putfloat(std::vector<uchar> &p, float n);
extern float getfloat(std::vector<uchar> &p);
#endif

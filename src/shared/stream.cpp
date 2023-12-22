/* stream.cpp: utilities for character streams
 *
 * stream.cpp defines character handling to enable character streams to be written
 * into and out of files
 * also included is utilities for gz archive support
 *
 */
#include <sstream>

#include "../libprimis-headers/cube.h"
#include "stream.h"

#include "../engine/interface/console.h"

///////////////////////// character conversion /////////////////////////////////

#define CUBECTYPE(s, p, d, a, A, u, U) \
    0, U, U, U, U, U, U, U, U, s, s, s, s, s, U, U, \
    U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, \
    s, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, \
    d, d, d, d, d, d, d, d, d, d, p, p, p, p, p, p, \
    p, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, \
    A, A, A, A, A, A, A, A, A, A, A, p, p, p, p, p, \
    p, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, \
    a, a, a, a, a, a, a, a, a, a, a, p, p, p, p, U, \
    U, u, u, u, u, u, u, u, u, u, u, u, u, u, u, u, \
    u, u, u, u, u, u, u, u, u, u, u, u, u, u, u, U, \
    u, U, u, U, u, U, u, U, u, U, u, U, u, U, u, U, \
    u, U, u, U, u, U, u, U, u, U, u, U, u, U, u, U, \
    u, U, u, U, u, U, u, U, U, u, U, u, U, u, U, U, \
    U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, \
    U, U, U, U, u, u, u, u, u, u, u, u, u, u, u, u, \
    u, u, u, u, u, u, u, u, u, u, u, u, u, u, U, u

/* note here:
 * these vars are declared extern inline to allow a `const` (implicitly also
 * `static`) to be linked to other files as a `const`.
 *
 * these vars cannot be `constexpr` due to it not being legal to define constexpr
 * prototypes in headers
 */

extern const uchar cubectype[256] =
{
    CUBECTYPE(CubeType_Space,
              CubeType_Print,
              CubeType_Print | CubeType_Digit,
              CubeType_Print | CubeType_Alpha | CubeType_Lower,
              CubeType_Print | CubeType_Alpha | CubeType_Upper,
              CubeType_Print | CubeType_Unicode | CubeType_Alpha | CubeType_Lower,
              CubeType_Print | CubeType_Unicode | CubeType_Alpha | CubeType_Upper)
};

size_t encodeutf8(uchar *dstbuf, size_t dstlen, const uchar *srcbuf, size_t srclen, size_t *carry)
{
    uchar *dst = dstbuf,
          *dstend = &dstbuf[dstlen];
    const uchar *src = srcbuf,
                *srcend = &srcbuf[srclen];
    if(src < srcend && dst < dstend)
    {
        do
        {
            int uni = cube2uni(*src);
            if(uni <= 0x7F)
            {
                if(dst >= dstend)
                {
                    goto done;
                }
                const uchar *end = std::min(srcend, &src[dstend-dst]);
                do
                {
                    if(uni == '\f')
                    {
                        if(++src >= srcend)
                        {
                            goto done;
                        }
                        goto uni1;
                    }
                    *dst++ = uni;
                    if(++src >= end)
                    {
                        goto done;
                    }
                    uni = cube2uni(*src);
                } while(uni <= 0x7F);
            }
            if(uni <= 0x7FF)
            {
                if(dst + 2 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xC0 | (uni>>6);
                goto uni2;
            }
            else if(uni <= 0xFFFF)
            {
                if(dst + 3 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xE0 | (uni>>12);
                goto uni3;
            }
            else if(uni <= 0x1FFFFF)
            {
                if(dst + 4 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xF0 | (uni>>18);
                goto uni4;
            }
            else if(uni <= 0x3FFFFFF)
            {
                if(dst + 5 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xF8 | (uni>>24);
                goto uni5;
            }
            else if(uni <= 0x7FFFFFFF)
            {
                if(dst + 6 > dstend)
                {
                    goto done;
                }
                *dst++ = 0xFC | (uni>>30);
                goto uni6;
            }
            else
            {
                goto uni1;
            }
        uni6: *dst++ = 0x80 | ((uni>>24)&0x3F);
        uni5: *dst++ = 0x80 | ((uni>>18)&0x3F);
        uni4: *dst++ = 0x80 | ((uni>>12)&0x3F);
        uni3: *dst++ = 0x80 | ((uni>>6)&0x3F);
        uni2: *dst++ = 0x80 | (uni&0x3F);
        uni1:;
        } while(++src < srcend);
    }
done:
    if(carry)
    {
        *carry += src - srcbuf;
    }
    return dst - dstbuf;
}

///////////////////////// file system ///////////////////////

#ifdef WIN32
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

string homedir = "";
struct packagedir
{
    std::string dir;
    std::string filter;
};
static std::vector<packagedir> packagedirs;

char *makerelpath(const char *dir, const char *file, const char *prefix, const char *cmd)
{
    static string tmp;
    if(prefix)
    {
        copystring(tmp, prefix);
    }
    else
    {
        tmp[0] = '\0';
    }
    if(file[0]=='<')
    {
        const char *end = std::strrchr(file, '>');
        if(end)
        {
            size_t len = std::strlen(tmp);
            copystring(&tmp[len], file, std::min(sizeof(tmp)-len, static_cast<size_t>(end+2-file)));
            file = end+1;
        }
    }
    if(cmd)
    {
        concatstring(tmp, cmd);
    }
    if(dir)
    {
        DEF_FORMAT_STRING(pname, "%s/%s", dir, file);
        concatstring(tmp, pname);
    }
    else
    {
        concatstring(tmp, file);
    }
    return tmp;
}


char *path(char *s)
{
    for(char *curpart = s;;)
    {
        char *endpart = std::strchr(curpart, '&');
        if(endpart)
        {
            *endpart = '\0';
        }
        if(curpart[0]=='<')
        {
            char *file = std::strrchr(curpart, '>');
            if(!file)
            {
                return s;
            }
            curpart = file+1;
        }
        for(char *t = curpart; (t = std::strpbrk(t, "/\\")); *t++ = PATHDIV)
        {
            //(empty body)
        }
        for(char *prevdir = nullptr, *curdir = curpart;;)
        {
            prevdir = curdir[0]==PATHDIV ? curdir+1 : curdir;
            curdir = std::strchr(prevdir, PATHDIV);
            if(!curdir)
            {
                break;
            }
            if(prevdir+1==curdir && prevdir[0]=='.')
            {
                std::memmove(prevdir, curdir+1, std::strlen(curdir+1)+1);
                curdir = prevdir;
            }
            else if(curdir[1]=='.' && curdir[2]=='.' && curdir[3]==PATHDIV)
            {
                if(prevdir+2==curdir && prevdir[0]=='.' && prevdir[1]=='.')
                {
                    continue;
                }
                std::memmove(prevdir, curdir+4, std::strlen(curdir+4)+1);
                if(prevdir-2 >= curpart && prevdir[-1]==PATHDIV)
                {
                    prevdir -= 2;
                    while(prevdir-1 >= curpart && prevdir[-1] != PATHDIV)
                    {
                        --prevdir;
                    }
                }
                curdir = prevdir;
            }
        }
        if(endpart)
        {
            *endpart = '&';
            curpart = endpart+1;
        }
        else
        {
            break;
        }
    }
    return s;
}

std::string path(std::string s)
{
    std::string truncated_path, processed_path;

    size_t path_begin = 0,
           path_end = s.length();

    // Find the in-line command segment and skip it
    if(s.find('<') != std::string::npos)
    {
        size_t command_end = s.rfind('>');
        if(command_end != std::string::npos)
        {
            path_begin = command_end + 1;
        }
    }

    // Find the conjugated path and cut it
    if(s.find('&') != std::string::npos)
    {
        path_end = s.find('&');
    }

    truncated_path = s.substr(path_begin, path_end - path_begin);

    // Handle "."" and ".."" in the path
    std::istringstream path_stream(truncated_path);
    std::stack<std::string> path_stack;
    std::string token;

    // Construct a stack of path tokens
    while(std::getline(path_stream, token, '/'))
    {
        if(token == "..")
        {
            if(!path_stack.empty())
            {
                path_stack.pop();
            }
            else
            {
                path_stack.push(token);
            }
        }
        else if(!token.empty() && token != ".")
        {
            path_stack.push(token);
        }
    }

    // Re-construct the processed path from the stack
    while(!path_stack.empty())
    {
        if(path_stack.size() > 1)
        {
            processed_path = "/" + path_stack.top() + processed_path;
        }
        else
        {
            processed_path = path_stack.top() + processed_path;
        }
        path_stack.pop();
    }

    return processed_path;
}

char *copypath(const char *s)
{
    static string tmp;
    copystring(tmp, s);
    path(tmp);
    return tmp;
}

const char *parentdir(const char *directory)
{
    const char *p = directory + std::strlen(directory);
    while(p > directory && *p != '/' && *p != '\\')
    {
        p--;
    }
    static string parent;
    size_t len = p-directory+1;
    copystring(parent, directory, len);
    return parent;
}

bool fileexists(const char *path, const char *mode)
{
    bool exists = true;
    if(mode[0]=='w' || mode[0]=='a')
    {
        path = parentdir(path);
    }
#ifdef WIN32
    if(GetFileAttributes(path[0] ? path : ".\\") == INVALID_FILE_ATTRIBUTES)
    {
        exists = false;
    }
#else
    if(access(path[0] ? path : ".", mode[0]=='w' || mode[0]=='a' ? W_OK : (mode[0]=='d' ? X_OK : R_OK)) == -1)
    {
        exists = false;
    }
#endif
    return exists;
}

bool createdir(const char *path)
{
    size_t len = std::strlen(path);
    if(path[len-1] == PATHDIV)
    {
        static string strip;
        path = copystring(strip, path, len);
    }
#ifdef WIN32
    return CreateDirectory(path, nullptr) != 0;
#else
    return mkdir(path, 0777) == 0;
#endif
}

size_t fixpackagedir(char *dir)
{
    path(dir);
    size_t len = std::strlen(dir);
    if(len > 0 && dir[len-1] != PATHDIV)
    {
        dir[len] = PATHDIV;
        dir[len+1] = '\0';
    }
    return len;
}

bool subhomedir(char *dst, int len, const char *src)
{
    const char *sub = std::strstr(src, "$HOME");
    if(!sub)
    {
        sub = std::strchr(src, '~');
    }
    if(sub && sub-src < len)
    {
#ifdef WIN32
        char home[MAX_PATH+1];
        home[0] = '\0';
        if(SHGetFolderPath(nullptr, CSIDL_PERSONAL, nullptr, 0, home) != S_OK || !home[0])
        {
            return false;
        }
#else
        const char *home = getenv("HOME");
        if(!home || !home[0])
        {
            return false;
        }
#endif
        dst[sub-src] = '\0';
        concatstring(dst, home, len);
        concatstring(dst, sub+(*sub == '~' ? 1 : std::strlen("$HOME")), len);
    }
    return true;
}

const char *sethomedir(const char *dir)
{
    string pdir;
    copystring(pdir, dir);
    if(!subhomedir(pdir, sizeof(pdir), dir) || !fixpackagedir(pdir))
    {
        return nullptr;
    }
    copystring(homedir, pdir);
    return homedir;
}

const char *addpackagedir(const char *dir)
{
    string pdir;
    copystring(pdir, dir);
    if(!subhomedir(pdir, sizeof(pdir), dir) || !fixpackagedir(pdir))
    {
        return nullptr;
    }
    char *filter = pdir;
    for(;;)
    {
        static int len = std::strlen("media");
        filter = std::strstr(filter, "media");
        if(!filter)
        {
            break;
        }
        if(filter > pdir && filter[-1] == PATHDIV && filter[len] == PATHDIV)
        {
            break;
        }
        filter += len;
    }
    packagedir pf;
    pf.dir = filter ? std::string(pdir, filter-pdir) : std::string(pdir);
    pf.filter = filter ? std::string(filter) : "";
    packagedirs.push_back(pf);
    return pf.dir.c_str();
}

const char *findfile(const char *filename, const char *mode)
{
    static string s;
    if(homedir[0])
    {
        formatstring(s, "%s%s", homedir, filename);
        if(fileexists(s, mode))
        {
            return s;
        }
        if(mode[0] == 'w' || mode[0] == 'a')
        {
            string dirs;
            copystring(dirs, s);
            char *dir = std::strchr(dirs[0] == PATHDIV ? dirs+1 : dirs, PATHDIV);
            while(dir)
            {
                *dir = '\0';
                if(!fileexists(dirs, "d") && !createdir(dirs))
                {
                    return s;
                }
                *dir = PATHDIV;
                dir = std::strchr(dir+1, PATHDIV);
            }
            return s;
        }
    }
    if(mode[0] == 'w' || mode[0] == 'a')
    {
        return filename;
    }
    for(const packagedir &pf : packagedirs)
    {
        if(pf.filter.size() > 0 && std::strncmp(filename, pf.filter.c_str(), pf.filter.size()))
        {
            continue;
        }
        formatstring(s, "%s%s", pf.dir.c_str(), filename);
        if(fileexists(s, mode))
        {
            return s;
        }
    }
    if(mode[0]=='e')
    {
        return nullptr;
    }
    return filename;
}

bool listdir(const char *dirname, bool rel, const char *ext, std::vector<char *> &files)
{
    size_t extsize = ext ? std::strlen(ext)+1 : 0;
#ifdef WIN32
    DEF_FORMAT_STRING(pathname, rel ? ".\\%s\\*.%s" : "%s\\*.%s", dirname, ext ? ext : "*");
    WIN32_FIND_DATA FindFileData;
    HANDLE Find = FindFirstFile(pathname, &FindFileData);
    if(Find != INVALID_HANDLE_VALUE)
    {
        do {
            if(!ext)
            {
                files.push_back(newstring(FindFileData.cFileName));
            }
            else
            {
                size_t namelen = std::strlen(FindFileData.cFileName);
                if(namelen > extsize)
                {
                    namelen -= extsize;
                    if(FindFileData.cFileName[namelen] == '.' && std::strncmp(FindFileData.cFileName+namelen+1, ext, extsize-1)==0)
                    {
                        files.push_back(newstring(FindFileData.cFileName, namelen));
                    }
                }
            }
        } while(FindNextFile(Find, &FindFileData));
        FindClose(Find);
        return true;
    }
#else
    DEF_FORMAT_STRING(pathname, rel ? "./%s" : "%s", dirname);
    DIR *d = opendir(pathname);
    if(d)
    {
        struct dirent *de;
        while((de = readdir(d)) != nullptr)
        {
            if(!ext)
            {
                files.push_back(newstring(de->d_name));
            }
            else
            {
                size_t namelen = std::strlen(de->d_name);
                if(namelen > extsize)
                {
                    namelen -= extsize;
                    if(de->d_name[namelen] == '.' && std::strncmp(de->d_name+namelen+1, ext, extsize-1)==0)
                    {
                        files.push_back(newstring(de->d_name, namelen));
                    }
                }
            }
        }
        closedir(d);
        return true;
    }
#endif
    else
    {
        return false;
    }
}

int listfiles(const char *dir, const char *ext, std::vector<char *> &files)
{
    string dirname;
    copystring(dirname, dir);
    path(dirname);
    size_t dirlen = std::strlen(dirname);
    while(dirlen > 1 && dirname[dirlen-1] == PATHDIV)
    {
        dirname[--dirlen] = '\0';
    }
    int dirs = 0;
    if(listdir(dirname, true, ext, files))
    {
        dirs++;
    }
    string s;
    if(homedir[0])
    {
        formatstring(s, "%s%s", homedir, dirname);
        if(listdir(s, false, ext, files))
        {
            dirs++;
        }
    }
    for(const packagedir &pf : packagedirs)
    {
        if(pf.filter.size() && std::strncmp(dirname, pf.filter.c_str(), dirlen == pf.filter.size()-1 ? dirlen : pf.filter.size()))
        {
            continue;
        }
        formatstring(s, "%s%s", pf.dir.c_str(), dirname);
        if(listdir(s, false, ext, files))
        {
            dirs++;
        }
    }
    dirs += listzipfiles(dirname, ext, files);
    return dirs;
}

static Sint64 rwopsseek(SDL_RWops *rw, Sint64 pos, int whence)
{
    stream *f = static_cast<stream *>(rw->hidden.unknown.data1);
    if((!pos && whence==SEEK_CUR) || f->seek(pos, whence))
    {
        return static_cast<int>(f->tell());
    }
    return -1;
}

static size_t rwopsread(SDL_RWops *rw, void *buf, size_t size, size_t nmemb)
{
    stream *f = static_cast<stream *>(rw->hidden.unknown.data1);
    return f->read(buf, size*nmemb)/size;
}

static size_t rwopswrite(SDL_RWops *rw, const void *buf, size_t size, size_t nmemb)
{
    stream *f = static_cast<stream *>(rw->hidden.unknown.data1);
    return f->write(buf, size*nmemb)/size;
}

SDL_RWops *stream::rwops()
{
    SDL_RWops *rw = SDL_AllocRW();
    if(!rw)
    {
        return nullptr;
    }
    rw->hidden.unknown.data1 = this;
    rw->seek = rwopsseek;
    rw->read = rwopsread;
    rw->write = rwopswrite;
    rw->close = 0;
    return rw;
}

stream::offset stream::size()
{
    offset pos = tell(), endpos;
    if(pos < 0 || !seek(0, SEEK_END))
    {
        return -1;
    }
    endpos = tell();
    return pos == endpos || seek(pos, SEEK_SET) ? endpos : -1;
}

bool stream::getline(char *str, size_t len)
{
    for(int i = 0; i < static_cast<int>(len-1); ++i)
    {
        if(read(&str[i], 1) != 1)
        {
            str[i] = '\0';
            return i > 0;
        }
        else if(str[i] == '\n')
        {
            str[i+1] = '\0';
            return true;
        }
    }
    if(len > 0)
    {
        str[len-1] = '\0';
    }
    return true;
}

size_t stream::printf(const char *fmt, ...)
{
    char buf[512];
    char *str = buf;
    va_list args;
#if defined(WIN32) && !defined(__GNUC__)
    va_start(args, fmt);
    int len = _vscprintf(fmt, args);
    if(len <= 0)
    {
        va_end(args);
        return 0;
    }
    if(len >= static_cast<int>(sizeof(buf)))
    {
        str = new char[len+1];
    }
    _vsnprintf(str, len+1, fmt, args);
    va_end(args);
#else
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if(len <= 0)
    {
        return 0;
    }
    if(len >= static_cast<int>(sizeof(buf)))
    {
        str = new char[len+1];
        va_start(args, fmt);
        vsnprintf(str, len+1, fmt, args);
        va_end(args);
    }
#endif
    size_t n = write(str, len);
    if(str != buf)
    {
        delete[] str;
    }
    return n;
}

struct filestream final : stream
{
    FILE *file;

    filestream() : file(nullptr) {}
    ~filestream() { close(); }

    bool open(const char *name, const char *mode)
    {
        if(file)
        {
            return false;
        }
        file = fopen(name, mode);
        return file!=nullptr;
    }
    #ifdef WIN32
        bool opentemp(const char *name, const char *mode)
        {
            if(file)
            {
                return false;
            }
            file = fopen(name, mode);
            return file!=nullptr;
        }
    #else
        bool opentemp(const char *, const char *)
        {
            if(file)
            {
                return false;
            }
            file = tmpfile();
            return file!=nullptr;
        }
    #endif
    void close() override final
    {
        if(file)
        {
            fclose(file);
            file = nullptr;
        }
    }

    bool end() override final
    {
        return feof(file)!=0;
    }

    offset tell() override final
    {
#ifdef WIN32
#if defined(__GNUC__) && !defined(__MINGW32__)
        offset off = ftello64(file);
#else
        offset off = _ftelli64(file);
#endif
#else
        offset off = ftello(file);
#endif
        // ftello returns LONG_MAX for directories on some platforms
        return off + 1 >= 0 ? off : -1;
    }

    bool seek(offset pos, int whence) override final
    {
#ifdef WIN32
#if defined(__GNUC__) && !defined(__MINGW32__)
        return fseeko64(file, pos, whence) >= 0;
#else
        return _fseeki64(file, pos, whence) >= 0;
#endif
#else
        return fseeko(file, pos, whence) >= 0;
#endif
    }

    size_t read(void *buf, size_t len) override final
    {
        return fread(buf, 1, len, file);
    }

    size_t write(const void *buf, size_t len) override final
    {
        return fwrite(buf, 1, len, file);
    }

    bool flush() override final
    {
        return !fflush(file);
    }

    int getchar() override final
    {
        return fgetc(file);
    }

    bool putchar(int c) override final
    {
        return fputc(c, file)!=EOF;
    }

    bool getline(char *str, size_t len) override final
    {
        return fgets(str, len, file)!=nullptr;
    }

    bool putstring(const char *str) override final
    {
        return fputs(str, file)!=EOF;
    }

    size_t printf(const char *fmt, ...) override final
    {
        va_list v;
        va_start(v, fmt);
        int result = std::vfprintf(file, fmt, v);
        va_end(v);
        return std::max(result, 0);
    }
};

VAR(debuggz, 0, 0, 1); //toggles gz checking routines

class gzstream final : public stream
{
    public:
        gzstream() : file(nullptr), buf(nullptr), reading(false), writing(false), autoclose(false), crc(0), headersize(0)
        {
            zfile.zalloc = nullptr;
            zfile.zfree = nullptr;
            zfile.opaque = nullptr;
            zfile.next_in = zfile.next_out = nullptr;
            zfile.avail_in = zfile.avail_out = 0;
        }

        ~gzstream()
        {
            close();
        }

        void writeheader()
        {
            uchar header[] = { MAGIC1, MAGIC2, Z_DEFLATED, 0, 0, 0, 0, 0, 0, OS_UNIX };
            file->write(header, sizeof(header));
        }

        void readbuf(size_t size = BUFSIZE)
        {
            if(!zfile.avail_in)
            {
                zfile.next_in = static_cast<Bytef *>(buf);
            }
            size = std::min(size, static_cast<size_t>(&buf[BUFSIZE] - &zfile.next_in[zfile.avail_in]));
            size_t n = file->read(zfile.next_in + zfile.avail_in, size);
            if(n > 0)
            {
                zfile.avail_in += n;
            }
        }

        uchar readbyte(size_t size = BUFSIZE)
        {
            if(!zfile.avail_in)
            {
                readbuf(size);
            }
            if(!zfile.avail_in)
            {
                return 0;
            }
            zfile.avail_in--;
            return *static_cast<uchar *>(zfile.next_in++);
        }

        void skipbytes(size_t n)
        {
            while(n > 0 && zfile.avail_in > 0)
            {
                size_t skipped = std::min(n, static_cast<size_t>(zfile.avail_in));
                zfile.avail_in -= skipped;
                zfile.next_in += skipped;
                n -= skipped;
            }
            if(n <= 0)
            {
                return;
            }
            file->seek(n, SEEK_CUR);
        }

        bool checkheader()
        {
            readbuf(10);
            if(readbyte() != MAGIC1 || readbyte() != MAGIC2 || readbyte() != Z_DEFLATED)
            {
                return false;
            }
            uchar flags = readbyte();
            if(flags & F_RESERVED)
            {
                return false;
            }
            skipbytes(6);
            if(flags & F_EXTRA)
            {
                size_t len = readbyte(512);
                len |= static_cast<size_t>(readbyte(512))<<8;
                skipbytes(len);
            }
            if(flags & F_NAME)
            {
                while(readbyte(512))
                {
                    //(empty body)
                }
            }
            if(flags & F_COMMENT)
            {
                while(readbyte(512))
                {
                    //(empty body)
                }
            }
            if(flags & F_CRC)
            {
                skipbytes(2);
            }
            headersize = static_cast<size_t>(file->tell() - zfile.avail_in);
            return zfile.avail_in > 0 || !file->end();
        }

        bool open(stream *f, const char *mode, bool needclose, int level)
        {
            if(file)
            {
                return false;
            }
            for(; *mode; mode++)
            {
                if(*mode=='r')
                {
                    reading = true;
                    break;
                }
                else if(*mode=='w')
                {
                    writing = true;
                    break;
                }
            }
            if(reading)
            {
                if(inflateInit2(&zfile, -MAX_WBITS) != Z_OK)
                {
                    reading = false;
                }
            }
            else if(writing && deflateInit2(&zfile, level, Z_DEFLATED, -MAX_WBITS, std::min(MAX_MEM_LEVEL, 8), Z_DEFAULT_STRATEGY) != Z_OK)
            {
                writing = false;
            }
            if(!reading && !writing)
            {
                return false;
            }
            file = f;
            crc = crc32(0, nullptr, 0);
            buf = new uchar[BUFSIZE];
            if(reading)
            {
                if(!checkheader())
                {
                    stopreading();
                    return false;
                }
            }
            else if(writing)
            {
                writeheader();
            }
            autoclose = needclose;
            return true;
        }

        uint getcrc() override final
        {
            return crc;
        }

        void finishreading()
        {
            if(!reading)
            {
                return;
            }
            if(debuggz)
            {
                uint checkcrc = 0,
                     checksize = 0;
                for(int i = 0; i < 4; ++i)
                {
                    checkcrc |= static_cast<uint>(readbyte()) << (i*8);
                }
                for(int i = 0; i < 4; ++i)
                {
                    checksize |= static_cast<uint>(readbyte()) << (i*8);
                }
                if(checkcrc != crc)
                {
                    conoutf(Console_Debug, "gzip crc check failed: read %X, calculated %X", checkcrc, crc);
                }
                if(checksize != zfile.total_out)
                {
                    conoutf(Console_Debug, "gzip size check failed: read %u, calculated %u", checksize, static_cast<uint>(zfile.total_out));
                }
            }
        }

        void stopreading()
        {
            if(!reading)
            {
                return;
            }
            inflateEnd(&zfile);
            reading = false;
        }

        void finishwriting()
        {
            if(!writing)
            {
                return;
            }
            for(;;)
            {
                int err = zfile.avail_out > 0 ? deflate(&zfile, Z_FINISH) : Z_OK;
                if(err != Z_OK && err != Z_STREAM_END)
                {
                    break;
                }
                flushbuf();
                if(err == Z_STREAM_END)
                {
                    break;
                }
            }
            uchar trailer[8] =
            {
                static_cast<uchar>(crc&0xFF), static_cast<uchar>((crc>>8)&0xFF), static_cast<uchar>((crc>>16)&0xFF), static_cast<uchar>((crc>>24)&0xFF),
                static_cast<uchar>(zfile.total_in&0xFF), static_cast<uchar>((zfile.total_in>>8)&0xFF), static_cast<uchar>((zfile.total_in>>16)&0xFF), static_cast<uchar>((zfile.total_in>>24)&0xFF)
            };
            file->write(trailer, sizeof(trailer));
        }

        void stopwriting()
        {
            if(!writing)
            {
                return;
            }
            deflateEnd(&zfile);
            writing = false;
        }

        void close() override final
        {
            if(reading)
            {
                finishreading();
            }
            stopreading();
            if(writing)
            {
                finishwriting();
            }
            stopwriting();
            delete[] buf;
            buf = nullptr;
            if(autoclose)
            {
                if(file)
                {
                    delete file;
                    file = nullptr;
                }
            }
        }

        bool end() override final
        {
            return !reading && !writing;
        }

        offset tell() override final
        {
            return reading ? zfile.total_out : (writing ? zfile.total_in : offset(-1));
        }

        offset rawtell() override final
        {
            return file ? file->tell() : offset(-1);
        }

        offset size() override final
        {
            if(!file)
            {
                return -1;
            }
            offset pos = tell();
            if(!file->seek(-4, SEEK_END))
            {
                return -1;
            }
            uint isize = file->get<uint>();
            return file->seek(pos, SEEK_SET) ? isize : offset(-1);
        }

        offset rawsize() override final
        {
            return file ? file->size() : offset(-1);
        }

        bool seek(offset pos, int whence) override final
        {
            if(writing || !reading)
            {
                return false;
            }

            if(whence == SEEK_END)
            {
                uchar skip[512];
                while(read(skip, sizeof(skip)) == sizeof(skip))
                {
                    //(empty body)
                }
                return !pos;
            }
            else if(whence == SEEK_CUR)
            {
                pos += zfile.total_out;
            }
            if(pos >= static_cast<offset>(zfile.total_out))
            {
                pos -= zfile.total_out;
            }
            else if(pos < 0 || !file->seek(headersize, SEEK_SET))
            {
                return false;
            }
            else
            {
                if(zfile.next_in && zfile.total_in <= static_cast<uint>(zfile.next_in - buf))
                {
                    zfile.avail_in += zfile.total_in;
                    zfile.next_in -= zfile.total_in;
                }
                else
                {
                    zfile.avail_in = 0;
                    zfile.next_in = nullptr;
                }
                inflateReset(&zfile);
                crc = crc32(0, nullptr, 0);
            }

            uchar skip[512];
            while(pos > 0)
            {
                size_t skipped = static_cast<size_t>(std::min(pos, static_cast<offset>(sizeof(skip))));
                if(read(skip, skipped) != skipped)
                {
                    stopreading();
                    return false;
                }
                pos -= skipped;
            }

            return true;
        }

        size_t read(void *buf, size_t len) override final
        {
            if(!reading || !buf || !len)
            {
                return 0;
            }
            zfile.next_out = static_cast<Bytef *>(buf);
            zfile.avail_out = len;
            while(zfile.avail_out > 0)
            {
                if(!zfile.avail_in)
                {
                    readbuf(BUFSIZE);
                    if(!zfile.avail_in)
                    {
                        stopreading();
                        break;
                    }
                }
                int err = inflate(&zfile, Z_NO_FLUSH);
                if(err == Z_STREAM_END)
                {
                    crc = crc32(crc, static_cast<Bytef *>(buf), len - zfile.avail_out);
                    finishreading();
                    stopreading();
                    return len - zfile.avail_out;
                }
                else if(err != Z_OK)
                {
                    stopreading();
                    break;
                }
            }
            crc = crc32(crc, reinterpret_cast<Bytef *>(buf), len - zfile.avail_out);
            return len - zfile.avail_out;
        }

        bool flushbuf(bool full = false)
        {
            if(full)
            {
                deflate(&zfile, Z_SYNC_FLUSH);
            }
            if(zfile.next_out && zfile.avail_out < BUFSIZE)
            {
                if(file->write(buf, BUFSIZE - zfile.avail_out) != BUFSIZE - zfile.avail_out || (full && !file->flush()))
                {
                    return false;
                }
            }
            zfile.next_out = buf;
            zfile.avail_out = BUFSIZE;
            return true;
        }

        bool flush() override final
        {
            return flushbuf(true);
        }

        size_t write(const void *buf, size_t len) override final
        {
            if(!writing || !buf || !len)
            {
                return 0;
            }
            zfile.next_in = static_cast<Bytef *>(const_cast<void *>(buf)); //cast away constness, then to Bytef
            zfile.avail_in = len;
            while(zfile.avail_in > 0)
            {
                if(!zfile.avail_out && !flushbuf())
                {
                    stopwriting();
                    break;
                }
                int err = deflate(&zfile, Z_NO_FLUSH);
                if(err != Z_OK)
                {
                    stopwriting();
                    break;
                }
            }
            crc = crc32(crc, static_cast<Bytef *>(const_cast<void *>(buf)), len - zfile.avail_in);
            return len - zfile.avail_in;
        }
    private:
        enum GzHeader
        {
            MAGIC1   = 0x1F,
            MAGIC2   = 0x8B,
            BUFSIZE  = 16384,
            OS_UNIX  = 0x03
        };

        enum GzFlags
        {
            F_ASCII    = 0x01,
            F_CRC      = 0x02,
            F_EXTRA    = 0x04,
            F_NAME     = 0x08,
            F_COMMENT  = 0x10,
            F_RESERVED = 0xE0
        };

        stream *file;
        z_stream zfile;
        uchar *buf;
        bool reading, writing, autoclose;
        uint crc;
        size_t headersize;
};

stream *openrawfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
    if(!found)
    {
        return nullptr;
    }
    filestream *file = new filestream;
    if(!file->open(found, mode))
    {
        delete file;
        return nullptr;
    }
    return file;
}

stream *openfile(const char *filename, const char *mode)
{
    stream *s = openzipfile(filename, mode);
    if(s)
    {
        return s;
    }
    return openrawfile(filename, mode);
}

stream *opengzfile(const char *filename, const char *mode, stream *file, int level)
{
    stream *source = file ? file : openfile(filename, mode);
    if(!source)
    {
        return nullptr;
    }
    gzstream *gz = new gzstream;
    if(!gz->open(source, mode, !file, level))
    {
        if(!file)
        {
            delete source;
        }
        delete gz;
        return nullptr;
    }
    return gz;
}

char *loadfile(const char *fn, size_t *size, bool utf8)
{
    stream *f = openfile(fn, "rb");
    if(!f)
    {
        return nullptr;
    }
    stream::offset fsize = f->size();
    if(fsize <= 0)
    {
        delete f;
        return nullptr;
    }
    size_t len = fsize;
    char *buf = new char[len+1];
    if(!buf)
    {
        delete f;
        return nullptr;
    }
    size_t offset = 0;
    if(utf8 && len >= 3)
    {
        if(f->read(buf, 3) != 3)
        {
            delete f;
            delete[] buf;
            return nullptr;
        }
        if((reinterpret_cast<uchar*>(buf))[0] == 0xEF &&
           (reinterpret_cast<uchar*>(buf))[1] == 0xBB &&
           (reinterpret_cast<uchar*>(buf))[2] == 0xBF)
        {
            len -= 3;
        }
        else
        {
            offset += 3;
        }
    }
    size_t rlen = f->read(&buf[offset], len-offset);
    delete f;
    if(rlen != len-offset)
    {
        delete[] buf;
        return nullptr;
    }
    buf[len] = '\0';
    if(size!=nullptr)
    {
        *size = len;
    }
    return buf;
}


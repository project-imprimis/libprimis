#include "../libprimis-headers/cube.h"
#include "stream.h"

#include "../engine/interface/console.h"

enum ZipFlags
{
    Zip_LocalFileSignature  = 0x04034B50,
    Zip_LocalFileSize       = 30,
    Zip_FileSignature       = 0x02014B50,
    Zip_FileSize            = 46,
    Zip_DirectorySignature  = 0x06054B50,
    Zip_DirectorySize       = 22
};

struct ziplocalfileheader
{
    uint signature;
    ushort version, flags, compression, modtime, moddate;
    uint crc32, compressedsize, uncompressedsize;
    ushort namelength, extralength;
};

struct zipfileheader
{
    uint signature;
    ushort version, needversion, flags, compression, modtime, moddate;
    uint crc32, compressedsize, uncompressedsize;
    ushort namelength, extralength, commentlength, disknumber, internalattribs;
    uint externalattribs, offset;
};

struct zipdirectoryheader
{
    uint signature;
    ushort disknumber, directorydisk, diskentries, entries;
    uint size, offset;
    ushort commentlength;
};

struct zipfile
{
    char *name;
    uint header, offset, size, compressedsize;

    zipfile() : name(nullptr), header(0), offset(~0U), size(0), compressedsize(0)
    {
    }
    ~zipfile()
    {
        delete[] name;
    }
};

class zipstream;

struct ziparchive
{
    char *name;
    FILE *data;
    std::map<std::string, zipfile> files;
    int openfiles;
    zipstream *owner;

    ziparchive() : name(nullptr), data(nullptr), files(), openfiles(0), owner(nullptr)
    {
    }
    ~ziparchive()
    {
        delete[] name;
        if(data)
        {
            std::fclose(data);
            data = nullptr;
        }
    }
};

static bool findzipdirectory(FILE *f, zipdirectoryheader &hdr)
{
    if(fseek(f, 0, SEEK_END) < 0)
    {
        return false;
    }
    long offset = ftell(f);
    if(offset < 0)
    {
        return false;
    }
    uchar buf[1024],
          *src = nullptr;
    long end = std::max(offset - 0xFFFFL - Zip_DirectorySize, 0L);
    size_t len = 0;
    const uint signature = static_cast<uint>(Zip_DirectorySignature);
    while(offset > end)
    {
        size_t carry = std::min(len, static_cast<size_t>(Zip_DirectorySize-1)), next = std::min(sizeof(buf) - carry, static_cast<size_t>(offset - end));
        offset -= next;
        std::memmove(&buf[next], buf, carry);
        if(next + carry < Zip_DirectorySize || fseek(f, offset, SEEK_SET) < 0 || std::fread(buf, 1, next, f) != next)
        {
            return false;
        }
        len = next + carry;
        uchar *search = &buf[next-1];
        for(; search >= buf; search--)
        {
            if(*(uint *)search == signature)
            {
                break;
            }
        }
        if(search >= buf)
        {
            src = search;
            break;
        }
    }
    if(!src || &buf[len] - src < Zip_DirectorySize)
    {
        return false;
    }
    hdr.signature = *(uint *)src; src += 4; //src is incremented by the size of the field (int is 4 bytes)
    hdr.disknumber = *(ushort *)src; src += 2;
    hdr.directorydisk = *(ushort *)src; src += 2;
    hdr.diskentries = *(ushort *)src; src += 2;
    hdr.entries = *(ushort *)src; src += 2;
    hdr.size = *(uint *)src; src += 4;
    hdr.offset = *(uint *)src; src += 4;
    hdr.commentlength = *(ushort *)src; src += 2;
    if(hdr.signature != Zip_DirectorySignature || hdr.disknumber != hdr.directorydisk || hdr.diskentries != hdr.entries)
    {
        return false;
    }
    return true;
}

VAR(debugzip, 0, 0, 1);

static bool readzipdirectory(const char *archname, FILE *f, int entries, int offset, uint size, std::vector<zipfile> &files)
{
    uchar *buf = new uchar[size],
          *src = buf;
    if(!buf || fseek(f, offset, SEEK_SET) < 0 || std::fread(buf, 1, size, f) != size)
    {
        delete[] buf;
        return false;
    }
    for(int i = 0; i < entries; ++i)
    {
        if(src + Zip_FileSize > &buf[size])
        {
            break;
        }
        zipfileheader hdr;
        hdr.signature   = *(uint *)src; src += 4; //src is incremented by the size of the field (int is 4 bytes)
        hdr.version     = *(ushort *)src; src += 2;
        hdr.needversion = *(ushort *)src; src += 2;
        hdr.flags       = *(ushort *)src; src += 2;
        hdr.compression = *(ushort *)src; src += 2;
        hdr.modtime     = *(ushort *)src; src += 2;
        hdr.moddate     = *(ushort *)src; src += 2;
        hdr.crc32            = *(uint *)src; src += 4;
        hdr.compressedsize   = *(uint *)src; src += 4;
        hdr.uncompressedsize = *(uint *)src; src += 4;
        hdr.namelength       = *(ushort *)src; src += 2;
        hdr.extralength      = *(ushort *)src; src += 2;
        hdr.commentlength    = *(ushort *)src; src += 2;
        hdr.disknumber       = *(ushort *)src; src += 2;
        hdr.internalattribs  = *(ushort *)src; src += 2;
        hdr.externalattribs  = *(uint *)src; src += 4;
        hdr.offset           = *(uint *)src; src += 4;
        if(hdr.signature != Zip_FileSignature)
        {
            break;
        }
        if(!hdr.namelength || !hdr.uncompressedsize || (hdr.compression && (hdr.compression != Z_DEFLATED || !hdr.compressedsize)))
        {
            src += hdr.namelength + hdr.extralength + hdr.commentlength;
            continue;
        }
        if(src + hdr.namelength > &buf[size])
        {
            break;
        }
        string pname;
        int namelen = std::min(static_cast<int>(hdr.namelength), static_cast<int>(sizeof(pname)-1));
        std::memcpy(pname, src, namelen);
        pname[namelen] = '\0';
        path(pname);
        char *name = newstring(pname);
        zipfile f;
        f.name = name;
        f.header = hdr.offset;
        f.size = hdr.uncompressedsize;
        files.push_back(f);
        f.compressedsize = hdr.compression ? hdr.compressedsize : 0;
        if(debugzip)
        {
            conoutf(Console_Debug, "%s: file %s, size %d, compress %d, flags %x", archname, name, hdr.uncompressedsize, hdr.compression, hdr.flags);
        }
        src += hdr.namelength + hdr.extralength + hdr.commentlength;
    }
    delete[] buf;
    return files.size() > 0;
}

static bool readlocalfileheader(FILE *f, ziplocalfileheader &h, uint offset)
{
    uchar buf[Zip_LocalFileSize];
    if(fseek(f, offset, SEEK_SET) < 0 || std::fread(buf, 1, Zip_LocalFileSize, f) != Zip_LocalFileSize)
    {
        return false;
    }
    uchar *src = buf;
    h.signature = *(uint *)src; src += 4; //src is incremented by the size of the field (int is 4 bytes e.g)
    h.version = *(ushort *)src; src += 2;
    h.flags = *(ushort *)src; src += 2;
    h.compression = *(ushort *)src; src += 2;
    h.modtime = *(ushort *)src; src += 2;
    h.moddate = *(ushort *)src; src += 2;
    h.crc32 = *(uint *)src; src += 4;
    h.compressedsize = *(uint *)src; src += 4;
    h.uncompressedsize = *(uint *)src; src += 4;
    h.namelength = *(ushort *)src; src += 2;
    h.extralength = *(ushort *)src; src += 2;
    if(h.signature != Zip_LocalFileSignature)
    {
        return false;
    }
    // h.uncompressedsize or h.compressedsize may be zero - so don't validate
    return true;
}

static std::vector<ziparchive *> archives;

ziparchive *findzip(const char *name)
{
    for(uint i = 0; i < archives.size(); i++)
    {
        if(!std::strcmp(name, archives[i]->name))
        {
            return archives[i];
        }
    }
    return nullptr;
}

static bool checkprefix(std::vector<zipfile> &files, const char *prefix, int prefixlen)
{
    for(uint i = 0; i < files.size(); i++)
    {
        if(!std::strncmp(files[i].name, prefix, prefixlen))
        {
            return false;
        }
    }
    return true;
}

static void mountzip(ziparchive &arch, std::vector<zipfile> &files, const char *mountdir, const char *stripdir)
{
    string packagesdir = "media/";
    path(packagesdir);
    size_t striplen = stripdir ? std::strlen(stripdir) : 0;
    if(!mountdir && !stripdir)
    {
        for(uint i = 0; i < files.size(); i++)
        {
            zipfile &f = files[i];
            const char *foundpackages = std::strstr(f.name, packagesdir);
            if(foundpackages)
            {
                if(foundpackages > f.name)
                {
                    stripdir = f.name;
                    striplen = foundpackages - f.name;
                }
                break;
            }
            const char *foundogz = std::strstr(f.name, ".ogz");
            if(foundogz)
            {
                const char *ogzdir = foundogz;
                while(--ogzdir >= f.name && *ogzdir != PATHDIV)
                {
                    //(empty body)
                }
                if(ogzdir < f.name || checkprefix(files, f.name, ogzdir + 1 - f.name))
                {
                    if(ogzdir >= f.name)
                    {
                        stripdir = f.name;
                        striplen = ogzdir + 1 - f.name;
                    }
                    if(!mountdir)
                    {
                        mountdir = "media/map/";
                    }
                    break;
                }
            }
        }
    }
    string mdir = "", fname;
    if(mountdir)
    {
        copystring(mdir, mountdir);
        if(fixpackagedir(mdir) <= 1)
        {
            mdir[0] = '\0';
        }
    }
    for(uint i = 0; i < files.size(); i++)
    {
        zipfile &f = files[i];
        formatstring(fname, "%s%s", mdir, striplen && !std::strncmp(f.name, stripdir, striplen) ? &f.name[striplen] : f.name);
        if(arch.files.find(fname) != arch.files.end())
        {
            continue;
        }
        char *mname = newstring(fname);
        zipfile &mf = arch.files[mname];
        mf = f;
        mf.name = mname;
    }
}

bool addzip(const char *name, const char *mount = nullptr, const char *strip = nullptr)
{
    string pname;
    copystring(pname, name);
    path(pname);
    size_t plen = std::strlen(pname);
    if(plen < 4 || !std::strchr(&pname[plen-4], '.'))
    {
        concatstring(pname, ".zip");
    }
    ziparchive *exists = findzip(pname);
    if(exists)
    {
        conoutf(Console_Error, "already added zip %s", pname);
        return true;
    }

    FILE *f = fopen(findfile(pname, "rb"), "rb");
    if(!f)
    {
        conoutf(Console_Error, "could not open file %s", pname);
        return false;
    }
    zipdirectoryheader h;
    std::vector<zipfile> files;
    if(!findzipdirectory(f, h) || !readzipdirectory(pname, f, h.entries, h.offset, h.size, files))
    {
        conoutf(Console_Error, "could not read directory in zip %s", pname);
        std::fclose(f);
        return false;
    }
    ziparchive *arch = new ziparchive;
    arch->name = newstring(pname);
    arch->data = f;
    mountzip(*arch, files, mount, strip);
    archives.push_back(arch);
    conoutf("added zip %s", pname);
    return true;
}

bool removezip(const char *name)
{
    string pname;
    copystring(pname, name);
    path(pname);
    int plen = (int)std::strlen(pname);
    if(plen < 4 || !std::strchr(&pname[plen-4], '.'))
    {
        concatstring(pname, ".zip");
    }
    ziparchive *exists = findzip(pname);
    if(!exists)
    {
        conoutf(Console_Error, "zip %s is not loaded", pname);
        return false;
    }
    if(exists->openfiles)
    {
        conoutf(Console_Error, "zip %s has open files", pname);
        return false;
    }
    conoutf("removed zip %s", exists->name);
    archives.erase(std::find(archives.begin(), archives.end(), exists));
    delete exists;
    return true;
}

class zipstream final : public stream
{
    public:
        enum
        {
            Buffer_Size  = 16384
        };
        zipstream() : arch(nullptr), info(nullptr), buf(nullptr), reading(~0U), ended(false)
        {
            zfile.zalloc = nullptr;
            zfile.zfree = nullptr;
            zfile.opaque = nullptr;
            zfile.next_in = zfile.next_out = nullptr;
            zfile.avail_in = zfile.avail_out = 0;
        }
        ~zipstream()
        {
            close();
        }
        void readbuf(uint size = Buffer_Size)
        {
            if(!zfile.avail_in)
            {
                zfile.next_in = (Bytef *)buf;
            }
            size = std::min(size, static_cast<uint>(&buf[Buffer_Size] - &zfile.next_in[zfile.avail_in]));
            if(arch->owner != this)
            {
                arch->owner = nullptr;
                if(fseek(arch->data, reading, SEEK_SET) >= 0)
                {
                    arch->owner = this;
                }
                else
                {
                    return;
                }
            }
            uint remaining = info->offset + info->compressedsize - reading,
                 n = arch->owner == this ? std::fread(zfile.next_in + zfile.avail_in, 1, std::min(size, remaining), arch->data) : 0U;
            zfile.avail_in += n;
            reading += n;
        }

        bool open(ziparchive *a, zipfile *f)
        {
            if(f->offset == ~0U)
            {
                ziplocalfileheader h;
                a->owner = nullptr;
                if(!readlocalfileheader(a->data, h, f->header))
                {
                    return false;
                }
                f->offset = f->header + Zip_LocalFileSize + h.namelength + h.extralength;
            }
            if(f->compressedsize && inflateInit2(&zfile, -MAX_WBITS) != Z_OK)
            {
                return false;
            }
            a->openfiles++;
            arch = a;
            info = f;
            reading = f->offset;
            ended = false;
            if(f->compressedsize)
            {
                buf = new uchar[Buffer_Size];
            }
            return true;
        }

        void stopreading()
        {
            if(reading == ~0U)
            {
                return;
            }
            if(debugzip)
            {
                conoutf(Console_Debug, info->compressedsize ? "%s: zfile.total_out %u, info->size %u" : "%s: reading %u, info->size %u", info->name, info->compressedsize ? static_cast<uint>(zfile.total_out) : reading - info->offset, info->size);
            }
            if(info->compressedsize)
            {
                inflateEnd(&zfile);
            }
            reading = ~0U;
        }

        void close() override final
        {
            stopreading();
            delete[] buf;
            buf = nullptr;
            if(arch)
            {
                arch->owner = nullptr;
                arch->openfiles--;
                arch = nullptr;
            }
        }

        offset size() override final
        {
            return info->size;
        }
        bool end() override final
        {
            return reading == ~0U || ended;
        }
        offset tell() override final
        {
            return reading != ~0U ? (info->compressedsize ? zfile.total_out : reading - info->offset) : offset(-1);
        }
        bool seek(offset pos, int whence) override final
        {
            if(reading == ~0U)
            {
                return false;
            }
            if(!info->compressedsize)
            {
                switch(whence)
                {
                    case SEEK_END:
                    {
                        pos += info->offset + info->size;
                        break;
                    }
                    case SEEK_CUR:
                    {
                        pos += reading;
                        break;
                    }
                    case SEEK_SET:
                    {
                        pos += info->offset;
                        break;
                    }
                    default:
                    {
                        return false;
                    }
                }
                pos = std::clamp(pos, offset(info->offset), offset(info->offset + info->size));
                arch->owner = nullptr;
                if(fseek(arch->data, static_cast<int>(pos), SEEK_SET) < 0)
                {
                    return false;
                }
                arch->owner = this;
                reading = pos;
                ended = false;
                return true;
            }
            switch(whence)
            {
                case SEEK_END:
                {
                    pos += info->size;
                    break;
                }
                case SEEK_CUR:
                {
                    pos += zfile.total_out;
                    break;
                }
                case SEEK_SET:
                {
                    break;
                }
                default:
                {
                    return false;
                }
            }
            if(pos >= (offset)info->size)
            {
                reading = info->offset + info->compressedsize;
                zfile.next_in += zfile.avail_in;
                zfile.avail_in = 0;
                zfile.total_in = info->compressedsize;
                zfile.total_out = info->size;
                arch->owner = nullptr;
                ended = false;
                return true;
            }
            if(pos < 0)
            {
                return false;
            }
            if(pos >= (offset)zfile.total_out)
            {
                pos -= zfile.total_out;
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
                    arch->owner = nullptr;
                    zfile.avail_in = 0;
                    zfile.next_in = nullptr;
                    reading = info->offset;
                }
                inflateReset(&zfile);
            }
            uchar skip[512];
            while(pos > 0)
            {
                size_t skipped = static_cast<size_t>(std::min(pos, (offset)sizeof(skip)));
                if(read(skip, skipped) != skipped)
                {
                    return false;
                }
                pos -= skipped;
            }

            ended = false;
            return true;
        }
        size_t read(void *buf, size_t len) override final
        {
            if(reading == ~0U || !buf || !len)
            {
                return 0;
            }
            if(!info->compressedsize)
            {
                if(arch->owner != this)
                {
                    arch->owner = nullptr;
                    if(fseek(arch->data, reading, SEEK_SET) < 0)
                    {
                        stopreading();
                        return 0;
                    }
                    arch->owner = this;
                }
                size_t n = std::fread(buf, 1, std::min(len, static_cast<size_t>(info->size + info->offset - reading)), arch->data);
                reading += n;
                if(n < len)
                {
                    ended = true;
                }
                return n;
            }
            zfile.next_out = (Bytef *)buf;
            zfile.avail_out = len;
            while(zfile.avail_out > 0)
            {
                if(!zfile.avail_in)
                {
                    readbuf(Buffer_Size);
                }
                int err = inflate(&zfile, Z_NO_FLUSH);
                if(err != Z_OK)
                {
                    if(err == Z_STREAM_END)
                    {
                        ended = true;
                    }
                    else
                    {
                        if(debugzip)
                        {
                            conoutf(Console_Debug, "inflate error: %s", zError(err));
                        }
                        stopreading();
                    }
                    break;
                }
            }
            return len - zfile.avail_out;
        }
    private:
        ziparchive *arch;
        zipfile *info;
        z_stream zfile;
        uchar *buf;
        uint reading;
        bool ended;
};

stream *openzipfile(const char *name, const char *mode)
{
    for(; *mode; mode++)
    {
        if(*mode=='w' || *mode=='a')
        {
            return nullptr;
        }
    }
    for(int i = archives.size(); --i >=0;) //note reverse iteration
    {
        ziparchive *arch = archives[i];
        auto itr = arch->files.find(name);
        zipfile *f = nullptr;
        if(itr == arch->files.end())
        {
            continue;
        }
        f = &(*(itr)).second;
        zipstream *s = new zipstream;
        if(s->open(arch, f))
        {
            return s;
        }
        delete s;
    }
    return nullptr;
}

bool findzipfile(const char *name)
{
    for(int i = archives.size(); --i >=0;) //note reverse iteration
    {
        ziparchive *arch = archives[i];
        if(arch->files.find(name) != arch->files.end())
        {
            return true;
        }
    }
    return false;
}

int listzipfiles(const char *dir, const char *ext, std::vector<char *> &files)
{
    size_t extsize = ext ? std::strlen(ext)+1 : 0,
           dirsize = std::strlen(dir);
    int dirs = 0;
    for(int i = archives.size(); --i >=0;) //note reverse iteration
    {
        ziparchive *arch = archives[i];
        uint oldsize = files.size();
        for(const auto& [k, f] : arch->files)
        {
            if(std::strncmp(k.c_str(), dir, dirsize))
            {
                continue;
            }
            const char *name = k.c_str() + dirsize;
            if(name[0] == PATHDIV)
            {
                name++;
            }
            if(std::strchr(name, PATHDIV))
            {
                continue;
            }
            if(!ext)
            {
                files.push_back(newstring(name));
            }
            else
            {
                size_t namelen = std::strlen(name);
                if(namelen > extsize)
                {
                    namelen -= extsize;
                    if(name[namelen] == '.' && std::strncmp(name+namelen+1, ext, extsize-1)==0)
                    {
                        files.push_back(newstring(name, namelen));
                    }
                }
            }
        }
        if(files.size() > oldsize)
        {
            dirs++;
        }
    }
    return dirs;
}

void initzipcmds()
{
    addcommand("addzip", reinterpret_cast<identfun>(+[](const char *name, const char *mount, const char *strip){addzip(name, mount[0] ? mount : nullptr, strip[0] ? strip : nullptr);}), "sss", Id_Command);
    addcommand("removezip", reinterpret_cast<identfun>(removezip), "s", Id_Command);
}


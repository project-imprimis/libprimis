
/**
 * @brief GLTF 2.0 loading functionality
 *
 * This file handles the loading of GLTF 2.0 files, converting them into data
 * structures readable by the program.
 *
 * The various get() functions *generate* the output vectors of arrays on demand;
 * they are not stored inside the object.
 *
 * This file, and gltfloader.h along with it, are explicitly designed not to rely
 * on the dependencies of the rest of the engine. It should be possible to compile
 * this file without the build system of the engine at large.
 */
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <optional>
#include <cstdint>

typedef unsigned int uint;
typedef unsigned short ushort;

#include "gltfloader.h"

//for GL types (GL_FLOAT, GL_UNSIGNED_INT, etc.)
#include <GL/gl.h>

//these values should be loaded by gl.h, fallback if not defined
#ifndef GL_UNSIGNED_INT
    #define GL_UNSIGNED_INT 5125
#endif
#ifndef GL_UNSIGNED_SHORT
    #define GL_UNSIGNED_SHORT 5123
#endif
#ifndef GL_UNSIGNED_BYTE
    #define GL_UNSIGNED_SHORT 5121
#endif
#ifndef GL_FLOAT
    #define GL_FLOAT 5126
#endif

//populates the object vectors with the data in the gltf file
GLTFModelInfo::GLTFModelInfo(std::string_view path, bool messages) : messages(messages)
{
    std::ifstream infile;
    infile.exceptions(std::ifstream::failbit);
    infile.open(path.data());
    std::vector<std::string> output;

    findmeshes(path);
    findaccessors(path);
    findbufferviews(path);
    findbuffers(path);
    findanimations(path);
}

std::vector<std::string> GLTFModelInfo::getmeshnames() const
{
    std::vector<std::string> meshnames;
    for(const Mesh &m : meshes)
    {
        meshnames.push_back(m.name);
    }
    return meshnames;
}

//getter functions generate vectors of arrays of the appropriate type
//given the mesh name
std::vector<std::array<float, 3>> GLTFModelInfo::getpositions(std::string name) const
{
    std::vector<std::array<float, 3>> positions;
    for(const Mesh &m : meshes)
    {
        if(m.name == name && m.positions)
        {
            if(!m.positions) //bail out if optional is nullopt
            {
                return positions;
            }
            const Accessor &a = accessors[m.positions.value()];
            const BufferView &bv = bufferviews[a.bufferview];
            if(a.componenttype == GL_FLOAT)
            {
                std::vector<float> floatbuf = gettypeblock<float>(bv.buffer, bv.bytelength, bv.byteoffset);
                positions = fillvector<float, float, 3>(floatbuf);
            }
            else
            {
                std::printf("invalid component type %u\n (want: %u)", a.componenttype, GL_FLOAT);
                throw std::logic_error("invalid vertex position component type");
            }
            return positions;
        }
    }
    return positions; //empty fallback
}

std::vector<std::array<float, 3>> GLTFModelInfo::getnormals(std::string name) const
{
    std::vector<std::array<float, 3>> normals;
    for(const Mesh &m : meshes)
    {
        if(m.name == name && m.normals)
        {
            if(!m.normals) //bail out if optional is nullopt
            {
                return normals;
            }
            const Accessor &a = accessors[m.normals.value()];
            const BufferView &bv = bufferviews[a.bufferview];
            if(a.componenttype == GL_FLOAT)
            {
                std::vector<float> floatbuf = gettypeblock<float>(bv.buffer, bv.bytelength, bv.byteoffset);
                normals = fillvector<float, float, 3>(floatbuf);
            }
            else
            {
                std::printf("invalid component type %u\n (want: %u)", a.componenttype, GL_FLOAT);
                throw std::logic_error("invalid vertex normal component type");
            }
            return normals;
        }
    }
    return normals; //empty fallback
}

std::vector<std::array<float, 2>> GLTFModelInfo::gettexcoords(std::string name) const
{
    std::vector<std::array<float, 2>> texcoords;
    for(const Mesh &m : meshes)
    {
        if(m.name == name && m.texcoords)
        {
            if(!m.texcoords) //bail out if optional is nullopt
            {
                return texcoords;
            }
            const Accessor &a = accessors[m.texcoords.value()];
            const BufferView &bv = bufferviews[a.bufferview];
            if(a.componenttype == GL_FLOAT)
            {
                std::vector<float> floatbuf = gettypeblock<float>(bv.buffer, bv.bytelength, bv.byteoffset);
                texcoords = fillvector<float, float, 2>(floatbuf);
            }
            else
            {
                std::printf("invalid component type %u\n (want: %u)", a.componenttype, GL_FLOAT);
                throw std::logic_error("invalid vertex texture coordinate component type");
            }
            return texcoords;
        }
    }
    return texcoords; //empty fallback
}

std::vector<std::array<uint, 4>> GLTFModelInfo::getjoints(std::string name) const
{
    std::vector<std::array<uint, 4>> joints;
    for(const Mesh &m : meshes)
    {
        if(m.name == name && m.indices)
        {
            if(!m.joints) //bail out if optional is nullopt
            {
                return joints;
            }
            const Accessor &a = accessors[m.joints.value()];
            const BufferView &bv = bufferviews[a.bufferview];
            if(a.componenttype == GL_UNSIGNED_BYTE)
            {
                std::vector<uint8_t> scalarbuf = gettypeblock<uint8_t>(bv.buffer, bv.bytelength, bv.byteoffset);
                joints = fillvector<uint, uint8_t, 4>(scalarbuf);
            }
            else if(a.componenttype == GL_UNSIGNED_SHORT)
            {
                std::vector<ushort> scalarbuf = gettypeblock<ushort>(bv.buffer, bv.bytelength, bv.byteoffset);
                joints = fillvector<uint, ushort, 4>(scalarbuf);
            }
            else
            {
                std::printf("invalid component type %u\n (want: %u %u)", a.componenttype, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT);
                throw std::logic_error("invalid vertex joint component type");
            }
            return joints;
        }
    }
    return joints; //empty fallback
}

std::vector<std::array<float, 4>> GLTFModelInfo::getweights(std::string name) const
{
    std::vector<std::array<float, 4>> weights;
    for(const Mesh &m : meshes)
    {
        if(m.name == name && m.indices)
        {
            if(!m.weights) //bail out if optional is nullopt
            {
                return weights;
            }
            const Accessor &a = accessors[m.weights.value()];
            const BufferView &bv = bufferviews[a.bufferview];
            if(a.componenttype == GL_FLOAT)
            {
                std::vector<float> floatbuf = gettypeblock<float>(bv.buffer, bv.bytelength, bv.byteoffset);
                weights = fillvector<float, float, 4>(floatbuf);
            }
            else
            {
                std::printf("invalid component type %u\n (want: %u)", a.componenttype, GL_FLOAT);
                throw std::logic_error("invalid vertex weight component type");
            }
            return weights;
        }
    }
    return weights; //empty fallback
}

std::vector<std::array<uint, 3>> GLTFModelInfo::getindices(std::string name) const
{
    std::vector<std::array<uint, 3>> indices;
    for(const Mesh &m : meshes)
    {
        if(m.name == name && m.indices)
        {
            if(!m.indices) //bail out if optional is nullopt
            {
                return indices;
            }
            const Accessor &a = accessors[m.indices.value()];
            const BufferView &bv = bufferviews[a.bufferview];
            if(a.componenttype == GL_UNSIGNED_SHORT)
            {
                std::vector<ushort> scalarbuf = gettypeblock<ushort>(bv.buffer, bv.bytelength, bv.byteoffset);
                indices = fillvector<uint, ushort, 3>(scalarbuf);
            }
            else if(a.componenttype == GL_UNSIGNED_INT)
            {
                std::vector<uint> uintbuf = gettypeblock<uint>(bv.buffer, bv.bytelength, bv.byteoffset);
                indices = fillvector<uint, uint, 3>(uintbuf);
            }
            else
            {
                std::printf("invalid component type %u\n (want: %u %u)", a.componenttype, GL_UNSIGNED_INT, GL_UNSIGNED_SHORT);
                throw std::logic_error("invalid vertex index component type");
            }
            //check that all indices are <= size of that mesh's texture coordinate list
            for(std::array<uint,3> i : indices)
            {
                for(size_t j = 0; j < 3; ++j)
                {
                    if(i[j] >= accessors[m.positions.value()].count)
                    {
                        throw std::logic_error("invalid texture index");
                    }
                }
            }
            return indices;
        }
    }
    return indices; //empty fallback
}

bool GLTFModelInfo::operator==(const GLTFModelInfo &m) const
{
    std::vector<std::string> names = getmeshnames();
    std::vector<std::string> mnames = m.getmeshnames();
    if(names != mnames)
    {
        return false;
    }
    for(std::string s : names)
    {
        if( !( getpositions(s) == m.getpositions(s)
            && getnormals(s) == m.getnormals(s)
            && gettexcoords(s) == m.gettexcoords(s)
            && getjoints(s) == m.getjoints(s)
            && getweights(s) == m.getweights(s)
            && getindices(s) == m.getindices(s)))
        {
            return false;
        }
    }
    return true;
}

////////////////////////////////////////
//private methods
////////////////////////////////////////
/* loadjsonfile: loads a (gltf) json file to a std::vector
 *
 * Loads a JSON file and creates a new line for each bracket level and entry.
 *
 * Parameters:
 *  - std::string name: path to the file to load
 * Returns:
 *  - std::vector<std::string> of file
 */
std::vector<std::string> GLTFModelInfo::loadjsonfile(std::string_view name)
{
    std::string wholefile;
    std::string line;
    std::ifstream infile;
    infile.open(name.data());
    std::vector<std::string> output;

    if(!infile.good())
    {
        perror("Error opening GLTF file");
        return output;//empty vector
    }
    while(getline(infile, line))
    {
        bool stringliteral = false;
        //strip all whitespace not inside string literals
        for(size_t i = 0; i < line.size(); ++i)
        {
            if((i == 0 && line[i] == '"') || (line[i] == '"' && line[i-1] != '\\'))
            {
                stringliteral = !stringliteral;
            }
            if(!stringliteral)
            {
                if(line[i] == ' ' || line[i] == '\t')
                {
                    line.erase(i, 1);
                    i--;
                }
            }
        }
        stringliteral = false;
        wholefile.append(line);
    }
    //line break after every { } ] ,

    //helper lambda
    auto pushstring = [] (size_t depth, std::vector<std::string> &out, const std::string &wholefile, size_t lastbreak, size_t cur)
    {
        std::string line(depth, ' ');
        line.append(wholefile.substr(lastbreak, cur - lastbreak));
        out.push_back(line);
    };
    size_t lastbreak = 0,
           bracketdepth = 0;
    for(size_t i = 0; i < wholefile.size(); ++i)
    {
        if(wholefile[i] == ']' || wholefile[i] == '}')
        {
            if(bracketdepth == 0)
            {
                throw std::logic_error("GLTF loader error: too many closing } or ]");
            }
            pushstring(bracketdepth, output, wholefile, lastbreak, i);
            bracketdepth--;
            lastbreak = i;
        }
        else if(wholefile[i] =='{')
        {
            i++;
            pushstring(bracketdepth, output, wholefile, lastbreak, i);
            bracketdepth++;
            lastbreak = i;
            i--;
        }
        else if(wholefile[i] =='[')
        {
            i++;
            pushstring(bracketdepth, output, wholefile, lastbreak, i);
            bracketdepth++;
            lastbreak = i;
            i--;
        }
        else if(wholefile[i] ==',')
        {
            i++;
            pushstring(bracketdepth, output, wholefile, lastbreak, i);
            lastbreak = i;
            i--;
        }
    }
    if(bracketdepth != 0)
    {
        throw std::logic_error("GLTF loader error: too few } or ]");
    }

    infile.close();
    return output;
}

//helper for find<object> functions
std::vector<std::string> GLTFModelInfo::getblockbyname(std::string_view path, std::string blockname)
{
    std::vector<std::string> file = loadjsonfile(path);
    size_t blockstart = 0;
    for(size_t i = 0; i < file.size(); ++i)
    {
        if(file[i].find(blockname) != std::string::npos)
        {
            blockstart = i;
            break;
        }
    }
    std::vector<std::string> block = getblock(file, blockstart);
    return block;
}

//helper to sanitize things dropped by sscanf
//removes ", from strings, part of json syntax
void GLTFModelInfo::cleanstring(std::string &s)
{
    for(size_t i = 0; i < s.size(); ++i)
    {
        if(s[i] == '\"' || s[i] == ',')
        {
            s.erase(i, 1);
            i--;
        }
    }
}
//return a list of strings corresponding to the named objects in the gltf file
size_t GLTFModelInfo::findmeshes(std::string_view path)
{
    accessors.clear();
    std::vector<std::string> accessorblock = getblockbyname(path, "\"meshes\"");
    size_t numaccessors = 0;
    //get indices by parsing sub-blocks
    for(size_t i = 0; i < accessorblock.size(); ++i)
    {
        std::vector<std::string> block = getblock(accessorblock, i);
        if(!block.size())
        {
            continue;
        }
        Mesh m{"",std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
        for(std::string j : block)
        {
            if(j.find(" \"name\":") != std::string::npos)
            {
                std::array<char, 256> s;
                s.fill(0);
                std::sscanf(j.c_str(), " \"name\":\%s", s.data());
                m.name = s.data();
                cleanstring(m.name);
            }
            else if(j.find(" \"POSITION\":") != std::string::npos)
            {
                uint positions = 0;
                std::sscanf(j.c_str(), " \"POSITION\":%u", &positions);
                m.positions = positions; //assign to optional with operator=
            }
            else if(j.find("\"NORMAL\"") != std::string::npos)
            {
                uint normals = 0;
                std::sscanf(j.c_str(), " \"NORMAL\":%u", &normals);
                m.normals = normals;
            }
            else if(j.find("\"TEXCOORD_0\"") != std::string::npos)
            {
                uint texcoords = 0;
                std::sscanf( j.c_str(), " \"TEXCOORD_0\":%u", &texcoords);
                m.texcoords = texcoords;
            }
            else if(j.find("\"JOINTS_0\"") != std::string::npos)
            {
                uint joints = 0;
                std::sscanf( j.c_str(), " \"JOINTS_0\":%u", &joints);
                m.joints = joints;
            }
            else if(j.find("\"WEIGHTS_0\"") != std::string::npos)
            {
                uint weights = 0;
                std::sscanf( j.c_str(), " \"WEIGHTS_0\":%u", &weights);
                m.weights = weights;
            }
            else if(j.find("\"indices\"") != std::string::npos)
            {
                uint indices = 0;
                std::sscanf( j.c_str(), " \"indices\":%u", &indices);
                m.indices = indices;
            }
        }
        if(messages)
        {
            std::printf("new mesh created: %s %u %u %u %u %u %u\n",
                m.name.c_str(),
                m.positions ? m.positions.value() : -1,
                m.normals ? m.normals.value() : -1,
                m.texcoords ? m.texcoords.value() : -1,
                m.joints ? m.joints.value() : -1,
                m.weights ? m.weights.value() : -1,
                m.indices ? m.indices.value() : -1
            );
        }
        meshes.push_back(m);
        i += block.size();
        numaccessors++;
    }
    return numaccessors;
}

//clears accessors vector, assigns to it ones found in the given math, returns number of accessors
size_t GLTFModelInfo::findaccessors(std::string_view path)
{
    accessors.clear();
    std::vector<std::string> accessorblock = getblockbyname(path, "\"accessors\"");
    size_t numaccessors = 0;
    //get indices by parsing sub-blocks
    for(size_t i = 0; i < accessorblock.size(); ++i)
    {
        std::vector<std::string> block = getblock(accessorblock, i);
        if(!block.size())
        {
            continue;
        }
        Accessor a{0,0,0,0,""};
        a.index = accessors.size();
        for(std::string j : block)
        {
            if(j.find(" \"bufferView\":") != std::string::npos)
            {
                std::sscanf(j.c_str(), " \"bufferView\":%u", &a.bufferview);
            }
            else if(j.find("\"componentType\"") != std::string::npos)
            {
                std::sscanf(j.c_str(), " \"componentType\":%u", &a.componenttype);
            }
            else if(j.find("\"count\"") != std::string::npos)
            {
                std::sscanf( j.c_str(), " \"count\":%u", &a.count);
            }
            else if(j.find("\"type\"") != std::string::npos)
            {
                std::array<char, 32> s;
                s.fill(0);
                std::sscanf(j.c_str(), " \"type\":%s", s.data());
                a.type = s.data();
            }
        }
        if(messages)
        {
            std::printf("new accessor created: %lu %u %u %u %s\n", a.index, a.bufferview, a.componenttype, a.count, a.type.c_str());
        }
        accessors.push_back(a);
        i += block.size();
        numaccessors++;
    }
    return numaccessors;
}

//clears buffer views vector, assigns to it buffers found in file, returns number of buffer views
uint GLTFModelInfo::findbufferviews(std::string_view path)
{
    bufferviews.clear();
    std::vector<std::string> bufferviewblock = getblockbyname(path, "\"bufferViews\"");
    size_t numbufferviews = 0;
    for(size_t i = 0; i < bufferviewblock.size(); ++i)
    {
        std::vector<std::string> block = getblock(bufferviewblock, i);
        if(!block.size())
        {
            continue;
        }
        BufferView b{0,0,0};
        b.index = bufferviews.size();
        for(std::string j : block)
        {
            if(j.find(" \"buffer\":") != std::string::npos)
            {
                std::sscanf(j.c_str(), " \"buffer\":%u", &b.buffer);
            }
            else if(j.find("\"byteLength\"") != std::string::npos)
            {
                std::sscanf(j.c_str(), " \"byteLength\":%u", &b.bytelength);
            }
            else if(j.find("\"byteOffset\"") != std::string::npos)
            {
                std::sscanf( j.c_str(), " \"byteOffset\":%u", &b.byteoffset);
            }
        }
        if(messages)
        {
            std::printf("new bufferview created: %lu %u %u %u\n", b.index, b.buffer, b.bytelength, b.byteoffset);
        }
        bufferviews.push_back(b);
        i += block.size();
        numbufferviews++;
    }
    return numbufferviews;
}

uint GLTFModelInfo::findbuffers(std::string_view path)
{
    buffers.clear();
    std::vector<std::string> bufferblock = getblockbyname(path, "\"buffers\"");
    size_t numbuffers = 0;
    for(size_t i = 0; i < bufferblock.size(); ++i)
    {
        std::vector<std::string> block = getblock(bufferblock, i);
        if(!block.size())
        {
            continue;
        }
        Buffer b{0,0,""};
        b.index = buffers.size();
        for(std::string j : block)
        {
            if(j.find(" \"byteLength\":") != std::string::npos)
            {
                std::sscanf(j.c_str(), " \"byteLength\":%u", &b.bytelength);
            }
            else if(j.find("\"uri\"") != std::string::npos)
            {
                std::array<char, 256> s;
                s.fill(0);
                std::sscanf(j.c_str(), " \"uri\":\"%s", s.data());
                b.uri = s.data();
                cleanstring(b.uri);
            }
        }
        std::ifstream binary(b.uri, std::ios::binary);
        std::vector<char> buffer(std::istreambuf_iterator<char>(binary), {});
        b.buf = buffer;
        if(messages)
        {
            std::printf("new buffer created: %lu %u %s %lu\n", b.index, b.bytelength, b.uri.c_str(), buffer.size());
        }
        buffers.push_back(b);
        i += block.size();
        numbuffers++;
        binary.close();
    }
    return numbuffers;
}

//clears buffer views vector, assigns to it buffers found in file, returns number of buffer views
uint GLTFModelInfo::findanimations(std::string_view path)
{
    animations.clear();
    std::vector<std::string> animationsblock = getblockbyname(path, "\"animations\""); //all of the animations section
    size_t numanimations = 0;
    for(size_t i = 0; i < animationsblock.size(); ++i)
    {
        std::vector<std::string> block = getblock(animationsblock, i); //a single animation data block
        if(!block.size())
        {
            continue;
        }
        Animation a;
        for(size_t j = 0; j < block.size(); ++j)
        {
            if(block[j].find(" \"name\":") != std::string::npos)
            {
                std::array<char, 256> s;
                s.fill(0);
                std::sscanf(block[j].c_str(), " \"name\":\"%s", s.data());
                a.name = s.data();
                cleanstring(a.name);
            }
            if(block[j].find(" \"channels\":") != std::string::npos)
            {
                std::vector<std::string> channelblock = getblock(animationsblock, j+1); // all of the channel information of a single anim
                for(size_t k = 0; k < channelblock.size(); ++k)
                {
                    std::vector<std::string> channeldata = getblock(channelblock, k); // a single channel data block
                    if(!channeldata.size())
                    {
                        continue;
                    }
                    Animation::Channel c{a.channels.size(),0,0,""};
                    for(std::string_view l : channeldata)
                    {
                        if(l.find(" \"sampler\":") != std::string::npos)
                        {
                            std::sscanf(l.data(), " \"sampler\":%lu", &c.sampler);
                        }
                        else if(l.find("\"node\"") != std::string::npos)
                        {
                            std::sscanf(l.data(), " \"node\":%lu", &c.targetnode);
                        }
                        else if(l.find("\"path\"") != std::string::npos)
                        {
                            std::array<char, 256> s;
                            s.fill(0);
                            std::sscanf(l.data(), " \"path\":%s", s.data());
                            c.targetpath = s.data();
                            cleanstring(c.targetpath);
                        }
                    }
                    if(messages)
                    {
                        std::printf("new channel (animation %lu) added: %lu %lu %s\n", animations.size(), c.sampler, c.targetnode, c.targetpath.c_str());
                    }
                    a.channels.push_back(c);
                    k += channeldata.size();
                }
            }
            if(block[j].find(" \"samplers\":") != std::string::npos)
            {
                std::vector<std::string> channelblock = getblock(animationsblock, j+1); // all of the channel information of a single anim
                for(size_t k = 0; k < channelblock.size(); ++k)
                {
                    std::vector<std::string> channeldata = getblock(channelblock, k); // a single channel data block
                    if(!channeldata.size())
                    {
                        continue;
                    }
                    Animation::Sampler s{a.samplers.size(),0,"", 0};
                    for(std::string_view l : channeldata)
                    {
                        if(l.find(" \"input\":") != std::string::npos)
                        {
                            std::sscanf(l.data(), " \"input\":%lu", &s.input);
                        }
                        else if(l.find("\"output\"") != std::string::npos)
                        {
                            std::sscanf(l.data(), " \"output\":%lu", &s.output);
                        }
                        else if(l.find("\"interpolation\"") != std::string::npos)
                        {
                            std::array<char, 256> str;
                            str.fill(0);
                            std::sscanf(l.data(), " \"interpolation\":%s", str.data());
                            s.interpolation = str.data();
                            cleanstring(s.interpolation);
                        }
                    }
                    if(messages)
                    {
                        std::printf("new sampler (animation %lu) added: %lu %lu %s %lu\n", animations.size(), s.index, s.input, s.interpolation.c_str(), s.output);
                    }
                    a.samplers.push_back(s);
                    k += channeldata.size();
                }
            }
        }
        if(messages)
        {
            std::printf("new animation (index %lu) created: %s\n", animations.size(), a.name.c_str());
        }
        animations.push_back(a);
        i += animationsblock.size();
        numanimations++;
    }
    return numanimations;
}

//get the indented block starting at the specified line
std::vector<std::string> GLTFModelInfo::getblock(const std::vector<std::string> &file, uint line)
{
    auto getindentationdepth = [] (const std::string &r)
    {
        uint indentationdepth = 0;
        for(const char &c : r)
        {
            if(c != ' ')
            {
                break;
            }
            indentationdepth++;
        }
        return indentationdepth;
    };

    std::string line0 = std::string(file[line]);
    uint line0depth = getindentationdepth(line0);

    std::vector<std::string> block;
    for(uint i = line+1; i < file.size(); ++i)
    {
        if(getindentationdepth(file[i]) <= line0depth)
        {
            break;
        }
        block.push_back(file[i]);
    }
    return block;
}

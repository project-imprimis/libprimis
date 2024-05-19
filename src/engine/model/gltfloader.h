
#ifndef GLTFLOADER_H_
#define GLTFLOADER_H_

class GLTFModelInfo
{
    public:
        //populates the object vectors with the data in the gltf file
        //throws std::ios_base::failure if unable to load file
        //throws std::logic_error if invalid bracketing
        //throws std::logic_error if invalid geometry data type (e.g. float vertex indices)
        GLTFModelInfo(std::string_view path, bool messages = false);
        //return list of mesh names, using one of the NodeTypes
        std::vector<std::string> getnodenames(int type) const;
        //getter functions generate vectors of arrays of the appropriate type
        //given the node name
        std::vector<std::array<float, 3>> getpositions(std::string name) const;
        std::vector<std::array<float, 3>> getnormals(std::string name) const;
        std::vector<std::array<float, 2>> gettexcoords(std::string name) const;
        std::vector<std::array<uint, 4>> getjoints(std::string name) const;
        std::vector<std::array<float, 4>> getweights(std::string name) const;
        std::vector<std::array<uint, 3>> getindices(std::string name) const;

        //two models are equal if their getters return the same (slow, requires loading data pointed to by file)
        bool operator==(const GLTFModelInfo &m) const;


        //Enum for selecting node types with getnodenames
        //"All" selects every node name, "Mesh", "Armature", "Bone" selects only those subsets
        enum NodeTypes
        {
            NodeType_All,
            NodeType_Mesh,
            NodeType_Armature,
            NodeType_Bone
        };
    private:

        struct Node
        {
            std::string name;
            std::optional<std::array<float, 3>> translation;
            std::optional<std::array<float, 4>> rotation;
            std::optional<size_t> mesh;
            std::vector<size_t> children;
        };

        struct Mesh
        {
            std::string name;
            //indices of accessors
            std::optional<uint> positions,
                                normals,
                                texcoords,
                                joints,
                                weights,
                                indices;
        };

        struct Accessor
        {
            size_t index; //index of this accessor
            uint bufferview; //index in the binary buffer this points to
            uint componenttype; //type of individual elements
            uint count; //number of elements
            std::string type; //type of data structure (vec2/vec3/scalar/etc)
        };

        struct BufferView
        {
            size_t index;
            uint buffer,
                 byteoffset,
                 bytelength;
        };

        struct Buffer
        {
            size_t index;
            uint bytelength;
            std::string uri;
            std::vector<char> buf;
        };

        struct Animation
        {
            struct Channel
            {
                size_t index;
                size_t sampler;
                size_t targetnode;
                std::string targetpath;
            };
            std::vector<Channel> channels;
            struct Sampler
            {
                size_t index;
                size_t input;
                std::string interpolation;
                size_t output;
            };
            std::vector<Sampler> samplers;
            std::string name;
        };

        //T: the type of the output arrays in the vector
        //U: the type of the vector of input data
        //N: the number of elements in output arrays
        template<class T, class U, int N>
        static std::vector<std::array<T, N>> fillvector(const std::vector<U> &block)
        {
            std::vector<std::array<T, N>> output;
            for(size_t i = 0; i < block.size(); i+=N)
            {
                std::array<T, N> v;
                for(size_t j = 0; j < N; ++j)
                {
                    v[j] = block[i+j];
                };
                output.push_back(v);
            }
            return output;
        }


        //index and length in bytes
        //returns a new copy of the buffer, as a vector, in the appropriate type
        template<class T>
        std::vector<T> gettypeblock(size_t bufindex, uint length, uint index) const
        {
            if(length + index > buffers[bufindex].buf.size())
            {
                std::printf("indices specified out of range: %u + %u > %lu\n", length, index, buffers[bufindex].buf.size());
            }
            std::vector<T> outbuf;
            for(uint i = index; i < index + length; i+=sizeof(T))
            {
                //type pun the char array to T
                char *data = new char[sizeof(T)];
                for(uint j = 0; j < sizeof(T); ++j)
                {
                    data[j] = buffers[bufindex].buf[i+j];
                }
                T *outdata = reinterpret_cast<T*>(data);
                outbuf.push_back(*outdata);
                delete[] data;
            }
            return outbuf;
        }

        /* loadjsonfile: loads a (gltf) json file to a std::vector
         *
         * Loads a JSON file and creates a new line for each bracket level and entry.
         *
         * Parameters:
         *  - std::string name: path to the file to load
         * Returns:
         *  - std::vector<std::string> of file
         */
        std::vector<std::string> loadjsonfile(std::string_view name);
        std::vector<std::string> getblockbyname(std::string_view path, std::string blockname, size_t maxdepth = 0);
        void cleanstring(std::string &s);
        size_t findnodes(std::string_view path);
        size_t findmeshes(std::string_view path);
        size_t findaccessors(std::string_view path);
        uint findbufferviews(std::string_view path);
        uint findbuffers(std::string_view path);
        uint findanimations(std::string_view path);
        std::vector<std::string> getblock(const std::vector<std::string> &file, uint line);

        const bool messages;
        std::vector<Node> nodes;
        std::vector<Mesh> meshes;
        std::vector<Accessor> accessors;
        std::vector<BufferView> bufferviews;
        std::vector<Buffer> buffers;
        std::vector<Animation> animations;

};

#endif

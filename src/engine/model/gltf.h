#ifndef GLTF_H_
#define GLTF_H_

class gltf final : public SkelLoader<gltf>
{
    public:
        //ordinary methods
        gltf(std::string name);

        //method overrides

        /**
         * @brief Returns that this model type does not have a flipped y coord (false)
         *
         * @return false
         */
        bool flipy() const final;

        /**
         * @brief Returns that this model is an MDL_GLTF
         *
         * Returns the enum entry MDL_GLTF from the enum in model.h
         *
         * @return enum value MDL_GLTF
         */
        int type() const final;
        bool loaddefaultparts() final;

        //static methods

        /**
         * @brief Returns name of this model format, "gltf".
         *
         * @return string array containing "gltf".
         */
        static const char *formatname();

    private:
        struct GLTFJoint final
        {
            vec pos;
            quat orient;
        };

        struct GLTFWeight final
        {
            int joint;
            float bias;
            vec pos;
        };

        struct GLTFVert final
        {
            vec2 tc;
            uint start, count;
        };

        struct GLTFHierarchy final
        {
            string name;
            int parent, flags, start;
        };

        class GLTFMeshGroup final : public skelmeshgroup
        {
            public:
                GLTFMeshGroup();
                //main anim loading functionality
                const skelanimspec * loadanim(const std::string &) final
                {
                    return nullptr;
                };

            private:
                bool loadmesh(const char *filename, float smooth, part &p);
                bool load(std::string_view meshname, float smooth, part &p) final;
        };


        //extensions to skelmesh objects for gltf specifically
        class GLTFMesh final : public skelmesh
        {
            public:
                GLTFMesh(std::string_view initname, vert *initverts, uint initnumverts, tri *inittris, uint initnumtris, meshgroup *initm);
                ~GLTFMesh();
                void cleanup();
                void buildverts(const std::vector<GLTFJoint> &joints);
                //gltf model loader
                void load(stream *f, char *buf, size_t bufsize, part &p, const std::string &modeldir);
        };

        static SkelCommands<gltf> gltfcommands;

        skelmeshgroup *newmeshes() final;
};

#endif

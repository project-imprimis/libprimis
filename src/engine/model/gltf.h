#ifndef GLTF_H_
#define GLTF_H_

class gltf final : public skelloader<gltf>
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
        int type() const final;
        bool loaddefaultparts() final;

        //static methods
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
                const skelanimspec * loadanim(const std::string &) final { return nullptr;};

            private:
                bool loadmesh(const char *filename, float smooth, part &p);
                bool load(std::string_view meshname, float smooth, part &p) final;
        };


        //extensions to skelmesh objects for gltf specifically
        class gltfmesh final : public skelmesh
        {
            public:
                gltfmesh(std::string_view name, vert *verts, uint numverts, tri *tris, uint numtris, meshgroup *m);
                ~gltfmesh();
                void cleanup();
                void buildverts(const std::vector<GLTFJoint> &joints);
                //gltf model loader
                void load(stream *f, char *buf, size_t bufsize, part &p, const std::string &modeldir);
        };

        static skelcommands<gltf> gltfcommands;

        skelmeshgroup *newmeshes() final;
};

#endif

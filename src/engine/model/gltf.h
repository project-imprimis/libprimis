#ifndef GLTF_H_
#define GLTF_H_

class gltf final : public skelloader<gltf>
{
    public:
        //ordinary methods
        gltf(std::string name);

        //method overrides
        bool flipy() const final;
        int type() const final;
        skelmeshgroup *newmeshes() final;
        bool loaddefaultparts() final;

        //static methods
        static const char *formatname();

    private:
        struct gltfjoint final
        {
            vec pos;
            quat orient;
        };

        struct gltfweight final
        {
            int joint;
            float bias;
            vec pos;
        };

        struct gltfvert final
        {
            vec2 tc;
            uint start, count;
        };

        struct gltfhierarchy final
        {
            string name;
            int parent, flags, start;
        };

        class gltfmeshgroup final : public skelmeshgroup
        {
            public:
                gltfmeshgroup();
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
                void buildverts(const std::vector<gltfjoint> &joints);
                //gltf model loader
                void load(stream *f, char *buf, size_t bufsize, part &p, const std::string &modeldir);
        };

        static skelcommands<gltf> gltfcommands;
};

#endif

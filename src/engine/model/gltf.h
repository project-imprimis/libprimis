
class gltf final : public skelloader<gltf>
{
    public:
        //ordinary methods
        gltf(std::string name);

        //method overrides
        bool flipy() const override final;
        int type() const override final;
        skelmeshgroup *newmeshes() override final;
        bool loaddefaultparts() override final;

        //static methods
        static const char *formatname();

    private:
        struct gltfjoint
        {
            vec pos;
            quat orient;
        };

        struct gltfweight
        {
            int joint;
            float bias;
            vec pos;
        };

        struct gltfvert
        {
            vec2 tc;
            uint start, count;
        };

        struct gltfhierarchy
        {
            string name;
            int parent, flags, start;
        };

        class gltfmeshgroup : public skelmeshgroup
        {
            public:
                gltfmeshgroup();
                //main anim loading functionality
                const skelanimspec * loadanim(const std::string &filename) override final { return nullptr;};

            private:
                bool loadmesh(const char *filename, float smooth, part &p);
                bool load(std::string_view meshname, float smooth, part &p) override final;
        };


        //extensions to skelmesh objects for gltf specifically
        class gltfmesh : public skelmesh
        {
            public:
                gltfmesh(std::string_view name, vert *verts, uint numverts, tri *tris, uint numtris);
                ~gltfmesh();
                void cleanup();
                void buildverts(const std::vector<gltfjoint> &joints);
                //gltf model loader
                void load(stream *f, char *buf, size_t bufsize, part &p, const std::string &modeldir);
        };

        static skelcommands<gltf> gltfcommands;
};

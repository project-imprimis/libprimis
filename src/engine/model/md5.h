
class md5 final : public skelloader<md5>
{
    public:
        //ordinary methods
        md5(std::string name);

        //method overrides
        bool flipy() const override final;
        int type() const override final;
        skelmeshgroup *newmeshes() override final;
        bool loaddefaultparts() override final;

        //static methods
        static const char *formatname();

    private:
        struct md5joint
        {
            vec pos;
            quat orient;
        };

        struct md5weight
        {
            int joint;
            float bias;
            vec pos;
        };

        struct md5vert
        {
            vec2 tc;
            uint start, count;
        };

        class md5meshgroup : public skelmeshgroup
        {
            public:
                md5meshgroup();
                //main anim loading functionality
                const skelanimspec * loadanim(const std::string &filename) override final;

            private:
                bool loadmesh(const char *filename, float smooth, part &p);
                bool load(std::string_view meshfile, float smooth, part &p) override final;
        };


        //extensions to skelmesh objects for md5 specifically
        class md5mesh : public skelmesh
        {
            public:
                md5mesh();
                ~md5mesh();
                void cleanup();
                void buildverts(const std::vector<md5joint> &joints);
                //md5 model loader
                void load(stream *f, char *buf, size_t bufsize, part &p, const std::string &modeldir);

            private:
                md5weight *weightinfo;
                int numweights;
                md5vert *vertinfo;
        };
        static skelcommands<md5> md5commands;
};


class md5 final : public skelloader<md5>
{
    public:
        md5(std::string name);
        static const char *formatname();
        bool flipy() const override final;
        int type() const override final;
        skelmeshgroup *newmeshes() override final;

        bool loaddefaultparts() override final;

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

        struct md5hierarchy
        {
            string name;
            int parent, flags, start;
        };

        class md5meshgroup : public skelmeshgroup
        {
            public:
                md5meshgroup();
                //main anim loading functionality
                const skelanimspec * loadanim(const char *filename);

            private:
                bool loadmesh(const char *filename, float smooth);
                bool load(const char *meshfile, float smooth);
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
                void load(stream *f, char *buf, size_t bufsize);

            private:
                md5weight *weightinfo;
                int numweights;
                md5vert *vertinfo;
        };
};

extern skelcommands<md5> md5commands; //see skelmodel.h for these commands


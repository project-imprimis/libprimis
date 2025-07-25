#ifndef MD5_H_
#define MD5_H_

class md5 final : public skelloader<md5>
{
    public:
        //ordinary methods
        md5(std::string name);

        //method overrides
        bool flipy() const final;
        int type() const final;
        skelmeshgroup *newmeshes() final;

        /**
         * @brief Attempts to load an md5mesh with default parameters.
         *
         * - adds a skelpart to the current model
         * - attempts to add the md5mesh file at the path indicated by the model's name
         * - the path and md5mesh file name should be the same (e.g. foo/foo.md5mesh)
         *
         * @return true if the mesh was successfully added
         * @return false if no mesh was added (such as if no file found)
         */
        bool loaddefaultparts() final;

        //static methods
        static const char *formatname();

    private:
        struct md5joint final
        {
            vec pos;
            quat orient;
        };

        struct md5weight final
        {
            int joint;
            float bias;
            vec pos;
        };

        struct md5vert final
        {
            vec2 tc;
            uint start, count;
        };

        class md5meshgroup final : public skelmeshgroup
        {
            public:
                md5meshgroup();
                //main anim loading functionality
                const skelanimspec * loadanim(const std::string &filename) final;

            private:
                bool loadmesh(std::string_view filename, float smooth, part &p);
                bool load(std::string_view meshfile, float smooth, part &p) final;
        };


        //extensions to skelmesh objects for md5 specifically
        class md5mesh final : public skelmesh
        {
            public:
                md5mesh(std::string_view name, meshgroup *m);
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

#endif

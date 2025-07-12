#ifndef OBJ_H_
#define OBJ_H_

struct obj final : vertloader<obj>
{
    obj(std::string name);

    static const char *formatname();
    static bool cananimate();
    bool flipy() const final;
    int type() const final;
    bool skeletal() const final;

    struct objmeshgroup final : vertmeshgroup
    {
        public:
            bool load(const char *filename, float smooth) final;

        private:
            static void parsevert(char *s, std::vector<vec> &out);
            static void flushmesh(vertmesh &curmesh,
                           const std::vector<vert> &verts,
                           const std::vector<tcvert> &tcverts,
                           const std::vector<tri> &tris,
                           const std::vector<vec> &attrib,
                           float smooth);
    };

    vertmeshgroup *newmeshes() final
    {
        return new objmeshgroup;
    }

    bool loaddefaultparts() final;

    private:
        /* note about objcommands variable:
         *
         * this variable is never used anywhere at all in the codebase
         * it only exists to call its constructor which adds commands to the cubescript
         * ident hash table of the given template type (obj)
         */
        static vertcommands<obj> objcommands;
};

#endif


#ifndef OBJ_H_
#define OBJ_H_

struct obj final : vertloader<obj>
{
    obj(std::string name);

    /**
     * @brief Returns the file type of this obj file, "obj"
     *
     * @return the C string "obj"
     */
    static const char *formatname();

    /**
     * @brief Returns that this model type cannot be animated (false)
     *
     * @return false
     */
    static bool cananimate();

    /**
     * @brief Returns that this model type has a flipped y axis (true)
     *
     * @return true
     */
    bool flipy() const final;

    /**
     * @brief Returns the enum type of this model
     *
     * Returns MDL_OBJ in model.h
     *
     * @return MDL_OBJ (1)
     */
    int type() const final;

    /**
     * @brief Returns that this model is not skeletal (false)
     *
     * @return false
     */
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


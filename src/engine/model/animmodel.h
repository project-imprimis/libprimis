#ifndef ANIMMODEL_H_
#define ANIMMODEL_H_

extern int testtags, debugcolmesh;

const std::string modelpath = "media/model/";

/**
 * @brief Animated model object/
 *
 * Generic class for an animated model object, derived from the very
 * general model structure
 *
 * animmodel provides functionality to implement an animated model consisting of
 * multiple mesh groups which are treated as a single model
 *
 * animmodel is extended by skelmodel to allow multipart objects to be rigged by
 * a skeleton/bone type animation methodology
 */
class animmodel : public model
{
    public:

        struct AnimPos final
        {
            int anim, fr1, fr2;
            float t;

            void setframes(const animinfo &info);

            bool operator==(const AnimPos &a) const;
            bool operator!=(const AnimPos &a) const;
        };

        class part;

        struct AnimState final
        {
            const part *owner;
            AnimPos cur, prev;
            float interp;

            bool operator==(const AnimState &a) const;
            bool operator!=(const AnimState &a) const;
        };

        class Mesh;

        struct shaderparams
        {
            float spec, gloss, glow, glowdelta, glowpulse, fullbright, scrollu, scrollv, alphatest;
            vec color;


            bool operator==(const animmodel::shaderparams &y) const;
            shaderparams();
        };

        //An object used to store texture data for a mesh, contained inside a `part` object.
        //A `meshgroup` object is paired with a `skin` object to provide texture & geometry
        //data for a mesh
        class skin final : public shaderparams
        {
            public:
                Texture *tex, *decal;
                const Texture *masks, *normalmap;
                Shader *shader;
                int cullface;

                skin(const part *owner, Texture *tex, const Texture *masks) :
                    tex(tex),
                    decal(nullptr),
                    masks(masks),
                    normalmap(nullptr),
                    shader(nullptr),
                    cullface(1),
                    owner(owner),
                    rsmshader(nullptr),
                    key(nullptr)
                {
                }

                bool alphatested() const;
                void setkey();
                void cleanup();
                void preloadBIH() const;
                void preloadshader();
                void bind(Mesh &b, const AnimState *as, bool usegpuskel = false, int vweights = 0);
                static void invalidateshaderparams();

            private:
                const part *owner;
                Shader *rsmshader;
                class ShaderParamsKey
                {
                    public:
                        static std::unordered_map<shaderparams, ShaderParamsKey> keys;

                        ShaderParamsKey() : version(-1) {}

                        bool checkversion();

                        static void invalidate()
                        {
                            firstversion = lastversion;
                        }
                    private:
                        static int firstversion, lastversion;

                        int version;
                };
                ShaderParamsKey *key;

                bool masked() const;
                bool bumpmapped() const;
                bool decaled() const;
                void setshaderparams(const AnimState *as, bool skinned = true);
                Shader *loadshader();
                void setshader(Mesh &m, bool usegpuskel, int vweights);

        };

        class meshgroup;

        //An object used to store a single geometry mesh inside a `meshgroup` object.
        //This object is to be extended to contain the actual geometry of a type of
        //model, such as `skelmesh` or `vertmesh`. In its base class form, the object
        //contains no geometry data.
        class Mesh
        {
            public:
                std::string name;
                bool cancollide, canrender, noclip;

                virtual ~Mesh()
                {
                }

                virtual void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) const = 0;

                virtual void genBIH(BIH::mesh &m) const = 0;

                void genBIH(const skin &s, std::vector<BIH::mesh> &bih, const matrix4x3 &t) const;

                virtual void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m) const = 0;

                virtual void setshader(Shader *s, bool, int, int row = 0) const
                {
                    if(row)
                    {
                        s->setvariant(0, row);
                    }
                    else
                    {
                        s->set();
                    }
                }

            protected:
                meshgroup *group;

                Mesh() : cancollide(true), canrender(true), noclip(false), group(nullptr)
                {
                }

                Mesh(std::string_view name, meshgroup *m) :
                    name(name),
                    cancollide(true),
                    canrender(true),
                    noclip(false),
                    group(m)
                {
                }

                template<class T>
                static void smoothnorms(typename T::vert *verts, int numverts, const typename T::tri *tris, int numtris, float limit, bool areaweight)
                {
                    if(!numverts)
                    {
                        return;
                    }
                    smoothdata *smooth = new smoothdata[numverts];
                    std::unordered_map<vec, int> share;
                    for(int i = 0; i < numverts; ++i)
                    {
                        const typename T::vert &v = verts[i];
                        const auto itr = share.find(v.pos);
                        if(itr == share.end())
                        {
                            share[v.pos] = i;
                        }
                        else
                        {
                            smooth[i].next = (*itr).second;
                            (*itr).second = i;
                        }
                    }
                    for(int i = 0; i < numtris; ++i)
                    {
                        const typename T::tri &t = tris[i];
                        const uint v1 = t.vert[0],
                                   v2 = t.vert[1],
                                   v3 = t.vert[2];
                        vec norm;
                        norm.cross(verts[v1].pos, verts[v2].pos, verts[v3].pos);
                        if(!areaweight)
                        {
                            norm.normalize();
                        }
                        smooth[v1].norm.add(norm);
                        smooth[v2].norm.add(norm);
                        smooth[v3].norm.add(norm);
                    }
                    for(int i = 0; i < numverts; ++i)
                    {
                        verts[i].norm = vec(0, 0, 0);
                    }
                    for(int i = 0; i < numverts; ++i)
                    {
                        const smoothdata &n = smooth[i];
                        verts[i].norm.add(n.norm);
                        if(n.next >= 0)
                        {
                            const float vlimit = limit*n.norm.magnitude();
                            for(int j = n.next; j >= 0;)
                            {
                                const smoothdata &o = smooth[j];
                                if(n.norm.dot(o.norm) >= vlimit*o.norm.magnitude())
                                {
                                    verts[i].norm.add(o.norm);
                                    verts[j].norm.add(n.norm);
                                }
                                j = o.next;
                            }
                        }
                    }
                    for(int i = 0; i < numverts; ++i)
                    {
                        verts[i].norm.normalize();
                    }
                    delete[] smooth;
                }

                /**
                 * @brief Generates normal data for an array of vert objects.
                 *
                 * If there are no verts in the verts array, returns with no changes.
                 *
                 * Sets all norm fields in the verts array to the zero vector.
                 * Accesses three elements in verts using a set of indices saved in the tri object.
                 *
                 * Each triangle's normal is the cross product of the three elements' positions, normalized if areaweight == false.
                 * (If areaweight is true, since the cross product's magnitude encodes its area, we don't want to
                 * normalize because we want to weight normals by area)
                 * This normal is added to each of the three vertices accessed (individual vertices can have multiple tris).
                 *
                 * Then, all vertices are normalized, creating normalized normals using all triangles sharing each vertex.
                 *
                 * @param verts An array of vertices
                 * @param numverts The size of the vertex array
                 * @param tris An array of triangles, containing indices in verts
                 * @param numtris The size of the tris array
                 * @param areaweight If true, weights normals by area of associated triangle
                 */
                template<class T>
                static void buildnorms(typename T::vert *verts, int numverts, const typename T::tri *tris, int numtris, bool areaweight)
                {
                    if(!numverts)
                    {
                        return;
                    }
                    for(int i = 0; i < numverts; ++i)
                    {
                        verts[i].norm = vec(0, 0, 0);
                    }
                    for(int i = 0; i < numtris; ++i)
                    {
                        const typename T::tri &t = tris[i];
                        typename T::vert &v1 = verts[t.vert[0]],
                                        &v2 = verts[t.vert[1]],
                                        &v3 = verts[t.vert[2]];
                        vec norm;
                        norm.cross(v1.pos, v2.pos, v3.pos);
                        if(!areaweight)
                        {
                            norm.normalize();
                        }
                        v1.norm.add(norm);
                        v2.norm.add(norm);
                        v3.norm.add(norm);
                    }
                    for(int i = 0; i < numverts; ++i)
                    {
                        verts[i].norm.normalize();
                    }
                }

                template<class T>
                static void buildnorms(typename T::vert *verts, int numverts, const typename T::tri *tris, int numtris, bool areaweight, int numframes)
                {
                    if(!numverts)
                    {
                        return;
                    }
                    for(int i = 0; i < numframes; ++i)
                    {
                        buildnorms<T>(&verts[i*numverts], numverts, tris, numtris, areaweight);
                    }
                }

                static void fixqtangent(quat &q, float bt);

                template<class T, class TC>
                void calctangents(typename T::vert *verts, const TC *tcverts, int numverts, const typename T::tri *tris, int numtris, bool areaweight) const
                {
                    vec *tangent = new vec[2*numverts],
                        *bitangent = tangent+numverts;
                    std::memset(static_cast<void *>(tangent), 0, 2*numverts*sizeof(vec));

                    for(int i = 0; i < numtris; ++i)
                    {
                        const typename T::tri &t = tris[i];
                        const vec &e0 = verts[t.vert[0]].pos;
                        vec e1 = vec(verts[t.vert[1]].pos).sub(e0),
                            e2 = vec(verts[t.vert[2]].pos).sub(e0);

                        const vec2 &tc0 = tcverts[t.vert[0]].tc,
                                   &tc1 = tcverts[t.vert[1]].tc,
                                   &tc2 = tcverts[t.vert[2]].tc;
                        float u1 = tc1.x - tc0.x,
                              v1 = tc1.y - tc0.y,
                              u2 = tc2.x - tc0.x,
                              v2 = tc2.y - tc0.y;
                        vec u(e2), v(e2);
                        u.mul(v1).sub(vec(e1).mul(v2));
                        v.mul(u1).sub(vec(e1).mul(u2));

                        if(vec().cross(e2, e1).dot(vec().cross(v, u)) >= 0)
                        {
                            u.neg();
                            v.neg();
                        }

                        if(!areaweight)
                        {
                            u.normalize();
                            v.normalize();
                        }

                        for(int j = 0; j < 3; ++j)
                        {
                            tangent[t.vert[j]].sub(u);
                            bitangent[t.vert[j]].add(v);
                        }
                    }
                    for(int i = 0; i < numverts; ++i)
                    {
                        typename T::vert &v = verts[i];
                        const vec &t = tangent[i],
                                  &bt = bitangent[i];
                        matrix3 m;
                        m.c = v.norm;
                        (m.a = t).project(m.c).normalize();
                        m.b.cross(m.c, m.a);
                        quat q(m);
                        fixqtangent(q, m.b.dot(bt));
                        v.tangent = q;
                    }
                    delete[] tangent;
                }

                template<class T, class TC>
                void calctangents(typename T::vert *verts, const TC *tcverts, int numverts, const typename T::tri *tris, int numtris, bool areaweight, int numframes) const
                {
                    for(int i = 0; i < numframes; ++i)
                    {
                        calctangents<T, TC>(&verts[i*numverts], tcverts, numverts, tris, numtris, areaweight);
                    }
                }

            private:
                struct smoothdata final
                {
                    vec norm;
                    int next;

                    smoothdata() : norm(0, 0, 0), next(-1) {}
                };
        };

        //A group of one or more meshes, which are used by a `part` to represent its contained geometry.
        //A global (static field) map of meshgroups are kept to cache model geometry; the actual `part`'s
        //refered mesh group object is kept as a pointer to a member of the static meshgroup map.
        class meshgroup
        {
            public:
                std::vector<Mesh *> meshes;

                virtual ~meshgroup();

                virtual void concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const = 0;
                virtual std::optional<size_t> findtag(std::string_view name) = 0;

                /**
                 * @brief Returns the number of animation frames this meshgroup contains
                 *
                 * For skelmodels, always returns at least 1. May be 0 or greater for vertmodels.
                 */
                virtual int totalframes() const = 0;
                virtual void *animkey() = 0;
                virtual void cleanup() = 0;
                virtual void render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p) = 0;
                virtual void preload() = 0;

                /**
                 * Returns a list of Mesh iterators corresponding to a given name
                 * The iterators may be invalidated by other method calls.
                 */
                std::vector<std::vector<animmodel::Mesh *>::const_iterator> getmeshes(std::string_view meshname) const;
                /**
                 * Returns a list of indices corresponding to locations in animmodel::part::skins.
                 * These indices are invalidated if animmodel::skins is modified after calling.
                 */
                std::vector<size_t> getskins(std::string_view meshname) const;

                void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &t) const;
                void genBIH(const std::vector<skin> &skins, std::vector<BIH::mesh> &bih, const matrix4x3 &t) const;
                void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &t) const;

                /**
                 * @brief Returns true if i is a valid index in the list of frames this meshgroup contains
                 *
                 * That is, returns true if the index i is at least 0 and is no larger than the last index
                 * in the frame list for this meshgroup
                 *
                 * @param index to query
                 *
                 * @return true if the passed index is a valid index
                 */
                bool hasframe(int i) const;

                /**
                 * @brief Returns true if the range i...n is a valid index of frames this meshgroup contains
                 *
                 * Returns true if the indices i through n begins with i at 0 or greater and i+n no larger
                 * than the last index in the frame list for this meshgroup
                 *
                 * @param i the first index to query
                 * @param n the number of indices thereafter to query
                 *
                 * @return true if the passed index range are all valid
                 */
                bool hasframes(int i, int n) const;

                /**
                 * @brief Returns the lesser of n and the number of remaining frames between i and n
                 *
                 * Returns n if there are existing frames between i and i+n, and returns the largest
                 * value that would satisfy this condition if not.
                 *
                 * @param i the first index to query
                 * @param n the number of indices thereafter to query
                 *
                 * @return n, or the maximum number of entries after i if n is too large
                 */
                int clipframes(int i, int n) const;

                /**
                 * @brief Returns the name of this meshgroup
                 *
                 * This is created by the constructor for the object.
                 *
                 * @return the name of the meshgroup
                 */
                const std::string &groupname() const;

                /**
                 * @brief Returns a list of valid renderable meshes contained within this object.
                 *
                 * Returns a vector of std::vector<>::iterator objects which point to valid
                 * elements of the object's mesh list which can be rendered (the relevant flag
                 * field is true). Alternatively if the global debugcolmesh is enabled, all
                 * meshes will be returned.
                 *
                 * @return a vector of std::vector iterators pointing to renderable meshes
                 */
                std::vector<std::vector<Mesh *>::const_iterator> getrendermeshes() const;
                std::vector<std::vector<Mesh *>::iterator> getrendermeshes();

                #define LOOP_RENDER_MESHES(type, name, body) do { \
                    for(uint i = 0; i < meshes.size(); i++) \
                    { \
                        type &name = *static_cast<type *>(meshes[i]); \
                        if(name.canrender || debugcolmesh) \
                        { \
                            body; \
                        } \
                    } \
                } while(0)

            protected:
                meshgroup();

                std::string name;

                void bindpos(GLuint ebuf, GLuint vbuf, const void *v, int stride, int type, int size);
                void bindpos(GLuint ebuf, GLuint vbuf, const vec *v, int stride);
                void bindpos(GLuint ebuf, GLuint vbuf, const vec4<half> *v, int stride);

                /**
                 * @brief Binds to TexCoord0 the provided data with a specified stridelength
                 *
                 * Binds (via glVertexAttribPointer) the provided array pointed to by v
                 * with a stride length (offset between attributes) of `stride`.
                 *
                 * @param v the array of texture coordinate data to bind
                 * @param stride the offset between coodinates within the array
                 */
                void bindtc(const void *v, int stride);
                void bindtangents(const void *v, int stride);
                void bindbones(const void *wv, const void *bv, int stride);
            //no private-able members
        };

        /**
         * @brief Global map to share mesh froups across models.
         *
         * The meshgroups map stores meshgroups which may be shared between different
         * model instantiations (e.g. skeletal models which have other models attached
         * as parts).
         *
         * The animmodel is comprised of parts which each represent instance-specific
         * metadata applied to a meshgroup in this map.
         */
        static std::unordered_map<std::string, meshgroup *> meshgroups;

        /**
         * @brief High level component of a model instance.
         *
         * The `part` object is the highest level of organization in a model object.
         * Each `part` is a logically separate part of an overall model, containing
         * its own skin(s) (`skin` objects), mesh(es) (`meshgroup` objects), and
         * model rendering parameters.
         *
         * Parts may contain a list of linked parts, which are other `part` objects
         * logically dependent on another `part`. An example is an object being held
         * in the hand of another model type, which is a separate mesh.
         */
        class part
        {
            public:
                const animmodel *model;
                int index;
                meshgroup *meshes; //pointer to a single meshgroup in animmodel::meshgroups

                //a pointer to another part object dependent on this part.
                //pointed part may be modified in case it is necessary to link/unlink
                //the pointed part to other dependent parts
                struct linkedpart
                {
                    part *p;
                    int tag, anim, basetime;
                    vec translate;
                    vec *pos; //a pos pointer from a modelattach object, which is set from within game
                    matrix4 matrix;

                    linkedpart() : p(nullptr), tag(-1), anim(-1), basetime(0), translate(0, 0, 0), pos(nullptr) {}
                    linkedpart(part *p, int tag, int anim, int basetime, vec translate, vec *post, matrix4 matrix) :
                        p(p), tag(tag), anim(anim), basetime(basetime), translate(translate), pos(post), matrix(matrix) {}
                };
                std::vector<linkedpart> links;
                std::vector<skin> skins;
                int numanimparts;
                float pitchscale, pitchoffset, pitchmin, pitchmax;

                /**
                 * @brief Creates a part object pointing to the given animmodel
                 *
                 * Sets this part's animation interpolation index to the value passed.
                 *
                 * @param model pointer to a single animmodel
                 * @param index animation interpolation index to assign
                 */
                part(const animmodel *model, int index = 0);
                virtual ~part();
                part(const part& a) = delete;
                part &operator=(const part &a) = delete;

                /**
                 * @brief Cleans up the meshgroup and attached skins.
                 *
                 * Cleans up the single meshgroup at part::meshes, and cleans up
                 * every skin in the skin vector. Does not delete the skins nor
                 * the meshgroup but merely leaves them in a cleaned up state
                 */
                void cleanup();

                /**
                 * @brief Zeros out the pitch fields in this part.
                 *
                 * Sets part::pitchscale, part::pitchoffset, part::pitchmin, and part::pitchmax
                 * to zero. This means that the part after this is called will have zero pitch
                 * component (rotation).
                 */
                void disablepitch();

                /**
                 * @brief Returns to bbmin and bbmax the bounding area of this model
                 *
                 * Returns the rectangular bounding box of the model, given its scale
                 * and transformation.
                 *
                 * @param bbmin the -x-y-z corner of the rectangular bounding box
                 * @param bbmax the +x+y+z cornder of the rectangular bounding box
                 * @param m the transformation matrix of the model
                 * @param modelscale the scale factor for the model
                 */
                void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m, float modelscale) const;

                /**
                 * @brief Generates a new BIH for this model and appends it to the back of the passed vector.
                 *
                 * Creates a new BIH at the end of the passed vector of BIH meshes and
                 * sets its parameters to that of this model part with the specified scale
                 * factor and scale.
                 *
                 * @param bih the bounded interval hierarchy for the model
                 * @param m the transformation matrix of this model
                 * @param modelscale the scale factor for the model
                 */
                void genBIH(std::vector<BIH::mesh> &bih, const matrix4x3 &m, float modelscale) const;
                void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m, float modelscale) const;
                bool link(part *p, std::string_view tag, const vec &translate = vec(0, 0, 0), int anim = -1, int basetime = 0, vec *pos = nullptr);

                /**
                 * @brief Removes from this part's linkedpart list the linkedpart corresponding to the passed part
                 *
                 * `part::links` is the vector containing the list of linkedparts.
                 *
                 * @param p the part to remove the corresponding linkedpart from
                 */
                bool unlink(const part *p);
                void initskins(Texture *tex = notexture, Texture *masks = notexture, uint limit = 0);
                bool alphatested() const;
                void preloadBIH() const;
                void preloadshaders();

                /**
                 * @brief Causes the attached meshgroup to run its preload routine
                 *
                 * The (single) meshgroup pointed to by this object will preload itself.
                 * This process includes generating the vertex buffer objects required
                 * for rendering the meshes.
                 */
                void preloadmeshes();

                void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray);
                void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray, AnimState *as);
                void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d);
                void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, AnimState *as);
                void setanim(int animpart, int num, int frame, int range, float speed, int priority = 0);

                /**
                 * @brief Returns true of all animparts for this part are set
                 *
                 * Checks that all animspec vectors in part::anims exist and have
                 * been instantiated. No values must be in the vectors for this to return true;
                 * they only need to exist.
                 *
                 * @return true if all animparts are set, false otherwise
                 */
                bool animated() const;
                virtual void loaded();

            private:
                struct animspec
                {
                    int frame, range;
                    float speed;
                    int priority;
                };

                std::vector<animspec> *anims[maxanimparts]; //pointer to array of std::vector<animspec>

                virtual void getdefaultanim(animinfo &info) const;
                bool calcanim(int animpart, int anim, int basetime, int basetime2, dynent *d, int interp, animinfo &info, int &animinterptime) const;
        };

        std::vector<part *> parts; //vector of part objects heap-allocated by skelmodel::addpart or part::addpart

        //ordinary methods
        ~animmodel();
        animmodel(const animmodel& a) = delete;
        animmodel &operator=(const animmodel &a) = delete;

        /**
         * @brief Creates a heap-allocated part object.
         *
         * This must be `delete`'d or it will cause a memory leak.
         *
         * @return a reference to the newly created part
         */
        part &addpart();
        /**
         * @brief Sets up a transformation matrix based on this model's rotation, translation, and scale
         *
         * @return a transformation matrix corresponding to the model's transformations
         */
        matrix4x3 initmatrix() const;
        void genBIH(std::vector<BIH::mesh> &bih);
        bool link(part *p, std::string_view tag, const vec &translate = vec(0, 0, 0), int anim = -1, int basetime = 0, vec *pos = nullptr) const;
        void loaded();
        bool unlink(const part *p) const;
        void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a) const;

        //virtual methods
        /**
         * @brief Returns whether this model type requires a flipped Y axis
         *
         * This function will return true if the model type needs its coordinate
         * system transformed to properly render in the engine. GLTF and MD5 do not,
         * while OBJ does.
         *
         * @return true if the model format requires transformation, false otherwise
         */
        virtual bool flipy() const = 0;
        virtual bool loadconfig(const std::string &mdlname) = 0;
        virtual bool loaddefaultparts() = 0;
        virtual void startload() = 0;
        virtual void endload() = 0;

        //model object overrides
        void render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec4<float> &color) const final;
        void cleanup() final;
        void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &orient) const final;
        void preloadBIH() final;
        void setBIH() final;
        bool animated() const final;
        bool pitched() const final;
        bool alphatested() const final;
        bool load() final;
        void preloadshaders() final;

        /**
         * @brief Preloades every entry in the part::meshes array.
         *
         * No effect if there are no meshes to be loaded.
         */
        void preloadmeshes() final;

        /**
         * @brief Sets the shader to be used by the model
         *
         * Clears any pre-existing slot parameters related to an existing shader.
         *
         * If no such shader was found, prints an error message to the console.
         *
         * @param shader the shader to set
         */
        void setshader(Shader *shader) final;
        void setspec(float spec) final;
        void setgloss(int gloss) final;
        void setglow(float glow, float delta, float pulse) final;
        /**
         * @brief Sets the alphatest value for each skin in each part of this model
         *
         * alphatest is one of the shader parameters for the skin shader. Each skin
         * in this model will recieve the same alphatest value.
         *
         * @param alphatest the alphatest value to set
         */
        void setalphatest(float alphatest) final;
        void setfullbright(float fullbright) final;
        void setcullface(int cullface) final;
        void setcolor(const vec &color) final;
        void settransformation(const std::optional<vec> pos,
                               const std::optional<vec> rotate,
                               const std::optional<vec> orient,
                               const std::optional<float> size) final;

        /**
         * @brief Returns a 4-vector consisting of the translation and scale of the model.
         *
         * @return translation coordinates in x,y,z; scale factor in w
         */
        vec4<float> locationsize() const final;
        void calcbb(vec &center, vec &radius) const final;
        void startrender() const final;
        void endrender() const final;

        /**
         * @brief Recalculates the bounding box parameters and returns them to the parameters passed
         *
         * Sets the parent `model` object's fields `bbcenter` and `bbradius`.
         *
         * @param center returns the value of this model's bounding box
         * @param radius returns the radius of this model's bounding box
         */
        void boundbox(vec &center, vec &radius) final;
        float collisionbox(vec &center, vec &radius) final;
        const std::string &modelname() const final;
        //static methods
        static void disablebones();
        static void disabletangents();
        static void disabletc();
        static void disablevbo();

    protected:
        enum
        {
            Link_Tag = 0,
            Link_Reuse
        };

        animmodel(std::string name);

        /**
         * @brief Returns the linkage type of the specified model part.
         *
         * For animmodels, always returns Link_Tag (see above anim).
         * Skeletal models may return Link_Reuse if the passed part shares the same mesh as this part.
         *
         * @return the linkage type of this part
         */
        virtual int linktype(const animmodel *, const part *) const;
        int intersect(int anim, int basetime, int basetime2, const vec &pos, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec &o, const vec &ray) const final;

        static bool enabletc, enablebones, enabletangents;

        /**
         * @brief A stack of transformation matrices used for model intersection math
         */
        static std::stack<matrix4> matrixstack;
        static float sizescale;

    private:
        void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a, const vec &o, const vec &ray) const;

        static bool enablecullface, enabledepthoffset;
        static vec4<float> colorscale;
        static GLuint lastvbuf, lasttcbuf, lastxbuf, lastbbuf, lastebuf;
        static const Texture *lasttex,
                             *lastdecal,
                             *lastmasks,
                             *lastnormalmap;
};

/**
 * @brief wrapper to load specific format into model object
 *
 * modelloader is a template for a wrapper to load a model into a model/animmodel
 * object from a transactional format, it is intended to be a child template class
 * of an animmodel derivative (the BASE template parameter)
 *
 * skelloader is a specialization of this class which uses modelloader to load
 * a skeletal model
 *
 */
template<class MDL, class BASE>
struct modelloader : BASE
{
    static MDL *loading;
    static std::string dir;

    modelloader(std::string name) : BASE(name)
    {
    }

    static bool cananimate()
    {
        return true;
    }

    static bool multiparted()
    {
        return true;
    }

    static bool multimeshed()
    {
        return true;
    }

    void startload() final
    {
        loading = static_cast<MDL *>(this);
    }

    void endload() final
    {
        loading = nullptr;
    }

    bool loadconfig(const std::string &mdlname) final
    {
        dir.clear();
        dir.append(modelpath).append(mdlname);
        std::string cfgname;
        cfgname.append(modelpath).append(mdlname).append("/").append(MDL::formatname()).append(".cfg");

        identflags &= ~Idf_Persist;
        bool success = execfile(cfgname.c_str(), false);
        identflags |= Idf_Persist;
        return success;
    }
};

template<class MDL, class BASE>
MDL *modelloader<MDL, BASE>::loading = nullptr;

template<class MDL, class BASE>
std::string modelloader<MDL, BASE>::dir = "";

/* modelcommands
 *
 * this template class adds a series of commands to the cubescript binding
 * adaptable to a specific model type
 *
 * this template class generates unique command names for each separate model type
 * such as objcolor for obj, or md5color for md5 models; they are static and the
 * same for any given MDL template parameter
 *
 * the intended MDL template parameter is one of the model formats (md5, obj, etc)
 */
template<class MDL>
struct modelcommands
{
    typedef class MDL::part part;
    typedef class MDL::skin skin;

    static bool checkmdl()
    {
        if(!MDL::loading)
        {
            conoutf(Console_Error, "not loading a model");
            return false;
        }
        else
        {
            return true;
        }
    }

    static void mdlcullface(const int *cullface)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->setcullface(*cullface);
    }

    static void mdlcolor(const float *r, const float *g, const float *b)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->setcolor(vec(*r, *g, *b));
    }

    static void mdlcollide(const int *collide)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->collide = *collide!=0 ? (MDL::loading->collide ? MDL::loading->collide : Collide_OrientedBoundingBox) : Collide_None;
    }

    static void mdlellipsecollide(const int *collide)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->collide = *collide!=0 ? Collide_Ellipse : Collide_None;
    }

    static void mdltricollide(const char *collide)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->collidemodel.clear();
        char *end = nullptr;
        int val = std::strtol(collide, &end, 0);
        if(*end)
        {
            val = 1;
            MDL::loading->collidemodel = std::string(collide);
        }
        MDL::loading->collide = val ? Collide_TRI : Collide_None;
    }

    static void mdlspec(const float *percent)
    {
        if(!checkmdl())
        {
            return;
        }
        float spec = *percent > 0 ? *percent/100.0f : 0.0f;
        MDL::loading->setspec(spec);
    }

    static void mdlgloss(const int *gloss)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->setgloss(std::clamp(*gloss, 0, 2));
    }

    static void mdlalphatest(const float *cutoff)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->setalphatest(std::max(0.0f, std::min(1.0f, *cutoff)));
    }

    static void mdldepthoffset(const int *offset)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->depthoffset = *offset!=0;
    }

    static void mdlglow(const float *percent, const float *delta, const float *pulse)
    {
        if(!checkmdl())
        {
            return;
        }
        float glow = *percent > 0 ? *percent/100.0f : 0.0f,
              glowdelta = *delta/100.0f,
              glowpulse = *pulse > 0 ? *pulse/1000.0f : 0;
        glowdelta -= glow;
        MDL::loading->setglow(glow, glowdelta, glowpulse);
    }

    static void mdlfullbright(const float *fullbright)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->setfullbright(*fullbright);
    }


    static void mdlshader(const char *shader)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->setshader(lookupshaderbyname(shader));
    }

    //assigns a new spin speed in three euler angles for the model object currently being loaded
    static void mdlspin(const float *yaw, const float *pitch, const float *roll)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->settransformation(std::nullopt, vec(*yaw, *pitch, *roll), std::nullopt, std::nullopt);
    }

    //assigns a new scale factor in % for the model object currently being loaded
    static void mdlscale(const float *percent)
    {
        if(!checkmdl())
        {
            return;
        }
        float scale = *percent > 0 ? *percent/100.0f : 1.0f;
        MDL::loading->settransformation(std::nullopt, std::nullopt, std::nullopt, scale);
    }

    //assigns translation in x,y,z in cube units for the model object currently being loaded
    static void mdltrans(const float *x, const float *y, const float *z)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->settransformation(vec(*x, *y, *z), std::nullopt, std::nullopt, std::nullopt);
    }

    //assigns angle to the offsetyaw field of the model object currently being loaded
    static void mdlyaw(const float *angle)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->orientation.x = *angle;
    }


    //assigns angle to the offsetpitch field of the model object currently being loaded
    static void mdlpitch(const float *angle)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->orientation.y = *angle;
    }

    //assigns angle to the offsetroll field of the model object currently being loaded
    static void mdlroll(const float *angle)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->orientation.z = *angle;
    }

    //assigns shadow to the shadow field of the model object currently being loaded
    static void mdlshadow(const int *shadow)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->shadow = *shadow!=0;
    }

    //assigns alphashadow to the alphashadow field of the model object currently being loaded
    static void mdlalphashadow(const int *alphashadow)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->alphashadow = *alphashadow!=0;
    }

    //assigns rad, h, eyeheight to the fields of the model object currently being loaded
    static void mdlbb(const float *rad, const float *h, const float *eyeheight)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->collidexyradius = *rad;
        MDL::loading->collideheight = *h;
        MDL::loading->eyeheight = *eyeheight;
    }

    static void mdlextendbb(const float *x, const float *y, const float *z)
    {
        if(!checkmdl())
        {
            return;
        }
        MDL::loading->bbextend = vec(*x, *y, *z);
    }

    /* mdlname
     *
     * returns the name of the model currently loaded [most recently]
     */
    static void mdlname()
    {
        if(!checkmdl())
        {
            return;
        }
        result(MDL::loading->modelname().c_str());
    }

    static void setdir(const char *name)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        MDL::dir = std::string(modelpath).append(name ? name : "");
    }

    /**
     * @brief Returns an iterator vector of meshes with the given name
     *
     * Returns a vector of MDL::loading->parts.back()'s meshgroup's mesh vector
     * iterators where those iterator's contents' name field compares equal to the
     * passed string. If the wildcard "*" is passed as `meshname` then all elements
     * will be added regardless of name. If no such mesh vector exists (or there is
     * no loading model) then an empty vector is returned.
     *
     * @param meshname the mesh name to select from the mesh vector
     *
     * @return vector of iterators corresponding to meshes with the given name
     */
    static std::vector<std::vector<animmodel::Mesh *>::const_iterator> getmeshes(std::string_view meshname)
    {
        std::vector<std::vector<animmodel::Mesh *>::const_iterator> meshlist;
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return meshlist; //empty vector
        }
        const part &mdl = *MDL::loading->parts.back();
        if(!mdl.meshes)
        {
            return meshlist; //empty vector
        }
        meshlist = mdl.meshes->getmeshes(meshname);
        return meshlist;
    }

    /**
     * @brief Returns an iterator vector of skins associated with the given name
     *
     * Returns a vector of MDL::loading->parts.back()'s meshgroup's skin vector
     * iterators where those iterator's contents' name field compares equal to the
     * passed string. If the wildcard "*" is passed as `meshname` then all elements
     * will be added regardless of name. If no such mesh vector exists (or there is
     * no loading model) then an empty vector is returned.
     *
     * @param meshname the mesh name to select from the skin vector
     *
     * @return vector of iterators corresponding to skins with the given name
     */
    static std::vector<std::vector<animmodel::skin>::iterator> getskins(std::string_view meshname)
    {
        std::vector<std::vector<animmodel::skin>::iterator> skinlist;
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return skinlist;
        }
        part &mdl = *MDL::loading->parts.back();
        if(!mdl.meshes)
        {
            return skinlist;
        }
        std::vector<size_t> skinindices = mdl.meshes->getskins(meshname);
        for(size_t i : skinindices)
        {
            skinlist.push_back(mdl.skins.begin() + i);
        }
        return skinlist;
    }

    static void setskin(const char *meshname, const char *tex, const char *masks)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).tex = textureload(makerelpath(MDL::dir.c_str(), tex), 0, true, false);
            if(*masks)
            {
                (*s).masks = textureload(makerelpath(MDL::dir.c_str(), masks), 0, true, false);
            }
        }
    }

    static void setspec(const char *meshname, const float *percent)
    {
        float spec = *percent > 0 ? *percent/100.0f : 0.0f;
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).spec = spec;
        }
    }

    static void setgloss(const char *meshname, const int *gloss)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).gloss = std::clamp(*gloss, 0, 2);
        }
    }

    static void setglow(const char *meshname, const float *percent, const float *delta, const float *pulse)
    {
        float glow = *percent > 0 ? *percent/100.0f : 0.0f,
              glowdelta = *delta/100.0f,
              glowpulse = *pulse > 0 ? *pulse/1000.0f : 0;
        glowdelta -= glow;
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).glow = glow;
            (*s).glowdelta = glowdelta;
            (*s).glowpulse = glowpulse;
        }
    }

    static void setalphatest(const char *meshname, const float *cutoff)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).alphatest = std::max(0.0f, std::min(1.0f, *cutoff));
        }
    }

    static void setcullface(const char *meshname, const int *cullface)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).cullface = *cullface;
        }
    }

    static void setcolor(const char *meshname, const float *r, const float *g, const float *b)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).color = vec(*r, *g, *b);
        }
    }

    static void setbumpmap(const char *meshname, const char *normalmapfile)
    {
        Texture *normalmaptex = textureload(makerelpath(MDL::dir.c_str(), normalmapfile), 0, true, false);
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).normalmap = normalmaptex;
        }
    }

    static void setdecal(const char *meshname, const char *decal)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).decal = textureload(makerelpath(MDL::dir.c_str(), decal), 0, true, false);
        }
    }

    static void setfullbright(const char *meshname, const float *fullbright)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).fullbright = *fullbright;
        }
    }

    static void setshader(const char *meshname, const char *shader)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).shader = lookupshaderbyname(shader);
        }
    }

    static void setscroll(const char *meshname, const float *scrollu, const float *scrollv)
    {
        auto skinlist = getskins(meshname);
        for(auto s : skinlist)
        {
            (*s).scrollu = *scrollu;
            (*s).scrollv = *scrollv;
        }
    }

    static void setnoclip(const char *meshname, const int *noclip)
    {
        auto meshlist = getmeshes(std::string(meshname));
        if(meshlist.empty())
        {
            return;
        }
        for(auto &i : meshlist)
        {
            (*i)->noclip = *noclip!=0;
        }
    }

    static void settricollide(const char *meshname)
    {
        bool init = true;
        auto meshlist = getmeshes(std::string(meshname));
        if(!meshlist.empty())
        {
            return;
        }
        else
        {
            for(auto &i : meshlist)
            {
                if(!(*i)->cancollide)
                {
                    init = false;
                }
            }
            if(init)
            {
                for(auto &i : meshlist)
                {
                    (*i)->cancollide = false;
                }
            }
            for(auto &i : meshlist)
            {
                (*i)->cancollide = true;
                (*i)->canrender = false;
            }
        }
    }

    static void setlink(const int *parent, const int *child, const char *tagname, const float *x, const float *y, const float *z)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        if(!(static_cast<int>(MDL::loading->parts.size()) > *parent) || !(static_cast<int>(MDL::loading->parts.size()) > *child))
        {
            conoutf("no models loaded to link");
            return;
        }
        if(!MDL::loading->parts[*parent]->link(MDL::loading->parts[*child], tagname, vec(*x, *y, *z)))
        {
            conoutf("could not link model %s", MDL::loading->modelname().c_str());
        }
    }

    template<class F>
    void modelcommand(F *fun, std::string_view suffix, std::string_view args)
    {
        std::string name;
        name.append(MDL::formatname()).append(suffix);
        addcommand(newstring(name.c_str()), reinterpret_cast<identfun>(fun), args.data());
    }

    modelcommands()
    {
        modelcommand(setdir, "dir", "s");//<fmt>dir [name]
        if(MDL::multimeshed())
        {
            modelcommand(mdlcullface, "cullface", "i");
            modelcommand(mdlcolor, "color", "fff");
            modelcommand(mdlcollide, "collide", "i");
            modelcommand(mdlellipsecollide, "ellipsecollide", "i");
            modelcommand(mdltricollide, "tricollide", "s");
            modelcommand(mdlspec, "spec", "f");
            modelcommand(mdlgloss, "gloss", "i");
            modelcommand(mdlalphatest, "alphatest", "f");
            modelcommand(mdldepthoffset, "depthoffset", "i");
            modelcommand(mdlglow, "glow", "fff");
            modelcommand(mdlfullbright, "fullbright", "f");
            modelcommand(mdlshader, "shader", "s");
            modelcommand(mdlspin, "spin", "fff");
            modelcommand(mdlscale, "scale", "f");
            modelcommand(mdltrans, "trans", "fff");
            modelcommand(mdlyaw, "yaw", "f");
            modelcommand(mdlpitch, "pitch", "f");
            modelcommand(mdlroll, "roll", "f");
            modelcommand(mdlshadow, "shadow", "i");
            modelcommand(mdlalphashadow, "alphashadow", "i");
            modelcommand(mdlbb, "bb", "fff");
            modelcommand(mdlextendbb, "extendbb", "fff");
            modelcommand(mdlname, "name", "");

            modelcommand(setskin, "skin", "sss");               //<fmt>skin [meshname] [tex] [masks]
            modelcommand(setspec, "texspec", "sf");             //<fmt>texspec [mesh] [scale]
            modelcommand(setgloss, "texgloss", "si");           //<fmt>texgloss [mesh] [type] type ranges 0..2
            modelcommand(setglow, "texglow", "sfff");           //<fmt>texglow [mesh] [pct] [del] [pulse]
            modelcommand(setalphatest, "meshalphatest", "sf");  //<fmt>meshalphatest [mesh] [cutoff]
            modelcommand(setcullface, "meshcullface", "si");    //<fmt>cullface [mesh] [cullface]
            modelcommand(setcolor, "meshcolor", "sfff");        //<fmt>meshcolor [mesh] [r] [g] [b]
            modelcommand(setbumpmap, "bumpmap", "ss");          //<fmt>bumpmap [mesh] [tex]
            modelcommand(setdecal, "decal", "ss");              //<fmt>decal [mesh] [tex]
            modelcommand(setfullbright, "meshfullbright", "sf");//<fmt>meshfullbright [mesh] [bright]
            modelcommand(setshader, "meshshader", "ss");        //<fmt>meshshader [mesh] [shader]
            modelcommand(setscroll, "scroll", "sff");           //<fmt>scroll [mesh] [x] [y]
            modelcommand(setnoclip, "noclip", "si");            //<fmt>noclip [mesh] [bool]
            modelcommand(settricollide, "tricollide", "s");     //<fmt>settricollide [mesh]
        }
        if(MDL::multiparted())
        {
            modelcommand(setlink, "link", "iisfff");//<mdl>link [parent] [child] [tag] [x] [y] [z]
        }
    }
};

#endif

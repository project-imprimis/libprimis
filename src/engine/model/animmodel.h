#ifndef ANIMMODEL_H_
#define ANIMMODEL_H_

extern int fullbrightmodels, testtags, debugcolmesh;

const std::string modelpath = "media/model/";

/* animmodel: generic class for an animated model object, derived from the very
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


            bool operator==(const animmodel::shaderparams &y) const
            {
                return spec == y.spec
                    && gloss == y.glow
                    && glow == y.glow
                    && glowdelta == y.glowdelta
                    && glowpulse == y.glowpulse
                    && fullbright == y.fullbright
                    && scrollu == y.scrollu
                    && scrollv == y.scrollv
                    && alphatest == y.alphatest
                    && color == y.color;
            }
            shaderparams() : spec(1.0f), gloss(1), glow(3.0f), glowdelta(0), glowpulse(0), fullbright(0), scrollu(0), scrollv(0), alphatest(0.9f), color(1, 1, 1) {}
        };

        class skin final : public shaderparams
        {
            public:
                const part *owner;
                Texture *tex, *decal, *masks, *normalmap;
                Shader *shader, *rsmshader;
                int cullface;

                skin() : owner(0), tex(notexture), decal(nullptr), masks(notexture), normalmap(nullptr), shader(nullptr), rsmshader(nullptr), cullface(1), key(nullptr) {}


                bool alphatested() const;
                void setkey();
                void cleanup();
                void preloadBIH() const;
                void preloadshader();
                void bind(Mesh &b, const AnimState *as);
                static void invalidateshaderparams();
            private:
                struct ShaderParamsKey
                {
                    static std::unordered_map<shaderparams, ShaderParamsKey> keys;

                    static int firstversion, lastversion;

                    int version;

                    ShaderParamsKey() : version(-1) {}

                    bool checkversion();

                    static void invalidate()
                    {
                        firstversion = lastversion;
                    }
                };
                ShaderParamsKey *key;

                bool masked() const;
                bool bumpmapped() const;
                bool decaled() const;
                void setshaderparams(Mesh &m, const AnimState *as, bool skinned = true);
                Shader *loadshader();
                void setshader(Mesh &m, const AnimState *as);

        };

        class meshgroup;

        class Mesh
        {
            public:
                meshgroup *group;
                char *name;
                bool cancollide, canrender, noclip;

                Mesh() : group(nullptr), name(nullptr), cancollide(true), canrender(true), noclip(false)
                {
                }

                virtual ~Mesh()
                {
                    delete[] name;
                }

                virtual void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) = 0;

                virtual void genBIH(BIH::mesh &m) {}

                void genBIH(const skin &s, std::vector<BIH::mesh> &bih, const matrix4x3 &t);

                virtual void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m) const
                {
                }

                virtual void setshader(Shader *s, int row = 0)
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

                template<class V, class T>
                static void smoothnorms(V *verts, int numverts, const T *tris, int numtris, float limit, bool areaweight)
                {
                    if(!numverts)
                    {
                        return;
                    }
                    smoothdata *smooth = new smoothdata[numverts];
                    std::unordered_map<vec, int> share;
                    for(int i = 0; i < numverts; ++i)
                    {
                        const V &v = verts[i];
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
                        const T &t = tris[i];
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

                template<class V, class T>
                static void buildnorms(V *verts, int numverts, const T *tris, int numtris, bool areaweight)
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
                        const T &t = tris[i];
                        V &v1 = verts[t.vert[0]],
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

                template<class V, class T>
                static void buildnorms(V *verts, int numverts, const T *tris, int numtris, bool areaweight, int numframes)
                {
                    if(!numverts)
                    {
                        return;
                    }
                    for(int i = 0; i < numframes; ++i)
                    {
                        buildnorms(&verts[i*numverts], numverts, tris, numtris, areaweight);
                    }
                }

                static void fixqtangent(quat &q, float bt);

                template<class V, class TC, class T>
                void calctangents(V *verts, TC *tcverts, int numverts, T *tris, int numtris, bool areaweight)
                {
                    vec *tangent = new vec[2*numverts],
                        *bitangent = tangent+numverts;
                    std::memset(static_cast<void *>(tangent), 0, 2*numverts*sizeof(vec));

                    for(int i = 0; i < numtris; ++i)
                    {
                        const T &t = tris[i];
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
                        V &v = verts[i];
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

                template<class V, class TC, class T>
                void calctangents(V *verts, TC *tcverts, int numverts, T *tris, int numtris, bool areaweight, int numframes)
                {
                    for(int i = 0; i < numframes; ++i)
                    {
                        calctangents(&verts[i*numverts], tcverts, numverts, tris, numtris, areaweight);
                    }
                }
            private:
                struct smoothdata
                {
                    vec norm;
                    int next;

                    smoothdata() : norm(0, 0, 0), next(-1) {}
                };
        };

        class meshgroup
        {
            public:
                std::vector<Mesh *> meshes;

                virtual ~meshgroup();

                virtual void concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const = 0;
                virtual int findtag(const char *name) = 0;
                virtual int totalframes() const = 0;
                virtual void *animkey() = 0;
                virtual void cleanup() = 0;
                virtual void render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p) = 0;
                virtual void preload() = 0;

                void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &t) const;
                void genBIH(const std::vector<skin> &skins, std::vector<BIH::mesh> &bih, const matrix4x3 &t);
                void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &t) const;
                bool hasframe(int i) const;
                bool hasframes(int i, int n) const;
                int clipframes(int i, int n) const;
                const std::string &groupname() const;

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

                void bindpos(GLuint ebuf, GLuint vbuf, void *v, int stride, int type, int size);
                void bindpos(GLuint ebuf, GLuint vbuf, vec *v, int stride);
                void bindpos(GLuint ebuf, GLuint vbuf, vec4<half> *v, int stride);
                void bindtc(void *v, int stride);
                void bindtangents(void *v, int stride);
                void bindbones(void *wv, void *bv, int stride);
            //no private-able members
        };

        static std::unordered_map<std::string, meshgroup *> meshgroups;

        class part
        {
            public:
                animmodel *model;
                int index;
                meshgroup *meshes;

                struct linkedpart
                {
                    part *p;
                    int tag, anim, basetime;
                    vec translate;
                    vec *pos;
                    matrix4 matrix;

                    linkedpart() : p(nullptr), tag(-1), anim(-1), basetime(0), translate(0, 0, 0), pos(nullptr) {}
                };
                std::vector<linkedpart> links;
                std::vector<skin> skins;
                int numanimparts;
                float pitchscale, pitchoffset, pitchmin, pitchmax;

                part(animmodel *model, int index = 0);
                virtual ~part();

                void cleanup();
                void disablepitch();
                void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m, float modelscale) const;
                void genBIH(std::vector<BIH::mesh> &bih, const matrix4x3 &m, float modelscale) const;
                void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m, float modelscale) const;
                bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), int anim = -1, int basetime = 0, vec *pos = nullptr);
                bool unlink(const part *p);
                void initskins(Texture *tex = notexture, Texture *masks = notexture, uint limit = 0);
                bool alphatested() const;
                void preloadBIH() const;
                void preloadshaders();
                void preloadmeshes();

                void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray);
                void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray, AnimState *as);
                void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d);
                void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, AnimState *as);
                void setanim(int animpart, int num, int frame, int range, float speed, int priority = 0);
                bool animated() const;
                virtual void loaded();

            protected:
                virtual void getdefaultanim(animinfo &info) const;
                bool calcanim(int animpart, int anim, int basetime, int basetime2, dynent *d, int interp, animinfo &info, int &animinterptime) const;

            private:
                struct animspec
                {
                    int frame, range;
                    float speed;
                    int priority;
                };

                std::vector<animspec> *anims[maxanimparts]; //pointer to array of std::vector<animspec>
        };

        void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a) const;
        void render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec4<float> &color) const;

        std::vector<part *> parts;

        ~animmodel();

        void cleanup() override final;

        virtual void flushpart() {}

        part& addpart()
        {
            flushpart();
            part *p = new part(this, parts.size());
            parts.push_back(p);
            return *p;
        }

        void initmatrix(matrix4x3 &m) const;
        void genBIH(std::vector<BIH::mesh> &bih);
        void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &orient);
        void preloadBIH();
        bool setBIH();
        bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), int anim = -1, int basetime = 0, vec *pos = nullptr) const;

        bool unlink(const part *p) const
        {
            if(parts.empty())
            {
                return false;
            }
            return parts[0]->unlink(p);
        }

        bool animated() const override final;

        bool pitched() const override final
        {
            return parts[0]->pitchscale != 0;
        }

        bool alphatested() const;

        virtual bool flipy() const = 0;
        virtual bool loadconfig() = 0;
        virtual bool loaddefaultparts() = 0;

        virtual void startload() = 0;
        virtual void endload() = 0;

        bool load() override;

        void preloadshaders() override final;
        void preloadmeshes() override final;

        void setshader(Shader *shader) override final;
        void setspec(float spec) override final;
        void setgloss(int gloss) override final;
        void setglow(float glow, float delta, float pulse) override final;
        void setalphatest(float alphatest) override final;
        void setfullbright(float fullbright) override final;
        void setcullface(int cullface) override final;
        void setcolor(const vec &color) override final;

        void calcbb(vec &center, vec &radius) const;
        void calctransform(matrix4x3 &m) const;

        virtual void loaded()
        {
            for(part *p : parts)
            {
                p->loaded();
            }
        }

        void startrender() const override final;
        static void disablebones();
        static void disabletangents();
        static void disabletc();
        static void disablevbo();
        void endrender() const override final;
    protected:
        enum
        {
            Link_Tag = 0,
            Link_Coop,
            Link_Reuse
        };

        animmodel(std::string name);

        virtual int linktype(const animmodel *, const part *) const
        {
            return Link_Tag;
        }
        int intersect(int anim, int basetime, int basetime2, const vec &pos, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec &o, const vec &ray, float &dist, int mode) const override final;

        static bool enabletc, enablebones, enabletangents;
        static std::stack<matrix4> matrixstack;
        static float sizescale;

    private:
        void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a, const vec &o, const vec &ray) const;

        static bool enablecullface, enabledepthoffset;
        static vec4<float> colorscale;
        static GLuint lastvbuf, lasttcbuf, lastxbuf, lastbbuf, lastebuf;
        static Texture *lasttex, *lastdecal, *lastmasks, *lastnormalmap;
};

/* modelloader
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

    modelloader(std::string name) : BASE(name) {}

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

    void startload()
    {
        loading = static_cast<MDL *>(this);
    }

    void endload()
    {
        loading = nullptr;
    }

    bool loadconfig()
    {
        dir.clear();
        dir.append(modelpath).append(BASE::modelname());
        DEF_FORMAT_STRING(cfgname, "media/model/%s/%s.cfg", BASE::modelname().c_str(), MDL::formatname());

        identflags &= ~Idf_Persist;
        bool success = execfile(cfgname, false);
        identflags |= Idf_Persist;
        return success;
    }
};

template<class MDL, class BASE>
MDL *modelloader<MDL, BASE>::loading = nullptr;

template<class MDL, class BASE>
std::string modelloader<MDL, BASE>::dir = {""}; // crashes clang if "" is used here

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
    typedef struct MDL::skin skin;

    static void setdir(char *name)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        MDL::dir.clear();
        MDL::dir.append(modelpath).append(name);
    }
//======================================================= LOOP_MESHES LOOP_SKINS

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
    static std::vector<std::vector<animmodel::Mesh *>::iterator> getmeshes(std::string meshname)
    {
        std::vector<std::vector<animmodel::Mesh *>::iterator> meshlist;
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return meshlist; //empty vector
        }
        part &mdl = *MDL::loading->parts.back();
        if(!mdl.meshes)
        {
            return meshlist; //empty vector
        }
        for(std::vector<animmodel::Mesh *>::iterator i = mdl.meshes->meshes.begin(); i != mdl.meshes->meshes.end(); ++i)
        {
            animmodel::Mesh &tempmesh = **i;
            if(!std::strcmp(meshname.c_str(), "*") || (tempmesh.name && !std::strcmp(tempmesh.name, meshname.c_str())))
            {
                meshlist.push_back(i);
            }
        }
        return meshlist;
    }

    #define LOOP_SKINS(meshname, s, body) do { \
        if(!MDL::loading || MDL::loading->parts.empty()) \
        { \
            conoutf("not loading an %s", MDL::formatname()); \
            return; \
        } \
        part &mdl = *MDL::loading->parts.back(); \
        if(!mdl.meshes) \
        { \
            return; \
        } \
        for(uint i = 0; i < mdl.meshes->meshes.size(); i++) \
        { \
            auto &m = *(mdl.meshes->meshes[i]); \
            if(!std::strcmp(meshname, "*") || (m.name && !std::strcmp(m.name, meshname))) \
            { \
                skin &s = mdl.skins[i]; \
                body; \
            } \
        } \
    } while(0)

    static void setskin(char *meshname, char *tex, char *masks)
    {
        LOOP_SKINS(meshname, s,
            s.tex = textureload(makerelpath(MDL::dir.c_str(), tex), 0, true, false);
            if(*masks)
            {
                s.masks = textureload(makerelpath(MDL::dir.c_str(), masks), 0, true, false);
            }
        );
    }

    static void setspec(char *meshname, float *percent)
    {
        float spec = *percent > 0 ? *percent/100.0f : 0.0f;
        LOOP_SKINS(meshname, s, s.spec = spec);
    }

    static void setgloss(char *meshname, int *gloss)
    {
        LOOP_SKINS(meshname, s, s.gloss = std::clamp(*gloss, 0, 2));
    }

    static void setglow(char *meshname, float *percent, float *delta, float *pulse)
    {
        float glow = *percent > 0 ? *percent/100.0f : 0.0f,
              glowdelta = *delta/100.0f,
              glowpulse = *pulse > 0 ? *pulse/1000.0f : 0;
        glowdelta -= glow;
        LOOP_SKINS(meshname, s, { s.glow = glow; s.glowdelta = glowdelta; s.glowpulse = glowpulse; });
    }

    static void setalphatest(char *meshname, float *cutoff)
    {
        LOOP_SKINS(meshname, s, s.alphatest = std::max(0.0f, std::min(1.0f, *cutoff)));
    }

    static void setcullface(char *meshname, int *cullface)
    {
        LOOP_SKINS(meshname, s, s.cullface = *cullface);
    }

    static void setcolor(char *meshname, float *r, float *g, float *b)
    {
        LOOP_SKINS(meshname, s, s.color = vec(*r, *g, *b));
    }

    static void setbumpmap(char *meshname, char *normalmapfile)
    {
        Texture *normalmaptex = textureload(makerelpath(MDL::dir.c_str(), normalmapfile), 0, true, false);
        LOOP_SKINS(meshname, s, s.normalmap = normalmaptex);
    }

    static void setdecal(char *meshname, char *decal)
    {
        LOOP_SKINS(meshname, s,
            s.decal = textureload(makerelpath(MDL::dir.c_str(), decal), 0, true, false);
        );
    }

    static void setfullbright(char *meshname, float *fullbright)
    {
        LOOP_SKINS(meshname, s, s.fullbright = *fullbright);
    }

    static void setshader(char *meshname, char *shader)
    {
        LOOP_SKINS(meshname, s, s.shader = lookupshaderbyname(shader));
    }

    static void setscroll(char *meshname, float *scrollu, float *scrollv)
    {
        LOOP_SKINS(meshname, s, { s.scrollu = *scrollu; s.scrollv = *scrollv; });
    }

    static void setnoclip(char *meshname, int *noclip)
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

    static void settricollide(char *meshname)
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

#undef LOOP_SKINS
//==============================================================================

    static void setlink(int *parent, int *child, char *tagname, float *x, float *y, float *z)
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
    void modelcommand(F *fun, const char *suffix, const char *args)
    {
        DEF_FORMAT_STRING(name, "%s%s", MDL::formatname(), suffix);
        addcommand(newstring(name), (identfun)fun, args);
    }

    modelcommands()
    {
        modelcommand(setdir, "dir", "s");//<fmt>dir [name]
        if(MDL::multimeshed())
        {
            modelcommand(setskin, "skin", "sss");           //<fmt>skin [meshname] [tex] [masks]
            modelcommand(setspec, "spec", "sf");            //<fmt>spec [tex] [scale]
            modelcommand(setgloss, "gloss", "si");          //<fmt>gloss [tex] [type] type ranges 0..2
            modelcommand(setglow, "glow", "sfff");          //<fmt>glow [tex] [pct] [del] [pulse]
            modelcommand(setalphatest, "alphatest", "sf");  //<fmt>alphatest [mesh] [cutoff]
            modelcommand(setcullface, "cullface", "si");    //<fmt>cullface [mesh] [cullface]
            modelcommand(setcolor, "color", "sfff");        //<fmt>color [mesh] [r] [g] [b]
            modelcommand(setbumpmap, "bumpmap", "ss");      //<fmt>bumpmap [mesh] [tex]
            modelcommand(setdecal, "decal", "ss");          //<fmt>decal [mesh] [tex]
            modelcommand(setfullbright, "fullbright", "sf");//<fmt>fullbright [mesh] [bright]
            modelcommand(setshader, "shader", "ss");        //<fmt>shader [mesh] [shader]
            modelcommand(setscroll, "scroll", "sff");       //<fmt>scroll [mesh] [x] [y]
            modelcommand(setnoclip, "noclip", "si");        //<fmt>noclip [mesh] [bool]
            modelcommand(settricollide, "tricollide", "s"); //<fmt>settricollide [mesh]
        }
        if(MDL::multiparted())
        {
            modelcommand(setlink, "link", "iisfff");//<mdl>link [parent] [child] [tag] [x] [y] [z]
        }
    }
};

#endif

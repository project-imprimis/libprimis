#ifndef ANIMMODEL_H_
#define ANIMMODEL_H_

extern int fullbrightmodels, testtags, debugcolmesh;

class animmodel : public model
{
    public:
        struct animspec
        {
            int frame, range;
            float speed;
            int priority;
        };

        struct AnimPos
        {
            int anim, fr1, fr2;
            float t;

            void setframes(const animinfo &info);

            bool operator==(const AnimPos &a) const
            {
                return fr1==a.fr1 && fr2==a.fr2 && (fr1==fr2 || t==a.t);
            }
            bool operator!=(const AnimPos &a) const
            {
                return fr1!=a.fr1 || fr2!=a.fr2 || (fr1!=fr2 && t!=a.t);
            }
        };

        struct part;

        struct AnimState
        {
            part *owner;
            AnimPos cur, prev;
            float interp;

            bool operator==(const AnimState &a) const
            {
                return cur==a.cur && (interp<1 ? interp==a.interp && prev==a.prev : a.interp>=1);
            }
            bool operator!=(const AnimState &a) const
            {
                return cur!=a.cur || (interp<1 ? interp!=a.interp || prev!=a.prev : a.interp<1);
            }
        };

        struct linkedpart;
        class Mesh;

        struct shaderparams
        {
            float spec, gloss, glow, glowdelta, glowpulse, fullbright, scrollu, scrollv, alphatest;
            vec color;

            shaderparams() : spec(1.0f), gloss(1), glow(3.0f), glowdelta(0), glowpulse(0), fullbright(0), scrollu(0), scrollv(0), alphatest(0.9f), color(1, 1, 1) {}
        };

        struct ShaderParamsKey
        {
            static hashtable<shaderparams, ShaderParamsKey> keys;
            static int firstversion, lastversion;

            int version;

            ShaderParamsKey() : version(-1) {}

            bool checkversion();

            static inline void invalidate()
            {
                firstversion = lastversion;
            }
        };

        struct skin : shaderparams
        {
            part *owner;
            Texture *tex, *decal, *masks, *normalmap;
            Shader *shader, *rsmshader;
            int cullface;
            ShaderParamsKey *key;

            skin() : owner(0), tex(notexture), decal(nullptr), masks(notexture), normalmap(nullptr), shader(nullptr), rsmshader(nullptr), cullface(1), key(nullptr) {}

            bool masked() const;
            bool bumpmapped() const;
            bool alphatested() const;
            bool decaled() const;
            void setkey();
            void setshaderparams(Mesh &m, const AnimState *as, bool skinned = true);
            Shader *loadshader();
            void cleanup();
            void preloadBIH();
            void preloadshader();
            void setshader(Mesh &m, const AnimState *as);
            void bind(Mesh &b, const AnimState *as);
        };

        struct meshgroup;

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
                    DELETEA(name);
                }

                virtual void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) {}

                virtual void genBIH(BIH::mesh &m) {}

                void genBIH(skin &s, vector<BIH::mesh> &bih, const matrix4x3 &t);

                virtual void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m)
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

                struct smoothdata
                {
                    vec norm;
                    int next;

                    smoothdata() : norm(0, 0, 0), next(-1) {}
                };

                template<class V, class T>
                void smoothnorms(V *verts, int numverts, T *tris, int numtris, float limit, bool areaweight)
                {
                    if(!numverts)
                    {
                        return;
                    }
                    smoothdata *smooth = new smoothdata[numverts];
                    hashtable<vec, int> share;
                    for(int i = 0; i < numverts; ++i)
                    {
                        V &v = verts[i];
                        int &idx = share.access(v.pos, i);
                        if(idx != i)
                        {
                            smooth[i].next = idx;
                            idx = i;
                        }
                    }
                    for(int i = 0; i < numtris; ++i)
                    {
                        T &t = tris[i];
                        int v1 = t.vert[0],
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
                            float vlimit = limit*n.norm.magnitude();
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
                void buildnorms(V *verts, int numverts, T *tris, int numtris, bool areaweight)
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
                        T &t = tris[i];
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
                void buildnorms(V *verts, int numverts, T *tris, int numtris, bool areaweight, int numframes)
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
                    memset(static_cast<void *>(tangent), 0, 2*numverts*sizeof(vec));

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
        };

        struct meshgroup
        {
            meshgroup *next;
            int shared;
            char *name;
            vector<Mesh *> meshes;

            meshgroup() : next(nullptr), shared(0), name(nullptr)
            {
            }

            virtual ~meshgroup()
            {
                DELETEA(name);
                meshes.deletecontents();
                DELETEP(next);
            }

            virtual int findtag(const char *name)
            {
                return -1;
            }

            virtual void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n) {}

            #define LOOP_RENDER_MESHES(type, name, body) do { \
                for(int i = 0; i < meshes.length(); i++) \
                { \
                    type &name = *static_cast<type *>(meshes[i]); \
                    if(name.canrender || debugcolmesh) \
                    { \
                        body; \
                    } \
                } \
            } while(0)

            void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &t);
            void genBIH(vector<skin> &skins, vector<BIH::mesh> &bih, const matrix4x3 &t);
            void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &t);

            virtual void *animkey()
            {
                return this;
            }

            virtual int totalframes() const
            {
                return 1;
            }

            bool hasframe(int i) const;
            bool hasframes(int i, int n) const;
            int clipframes(int i, int n) const;

            virtual void cleanup() {}
            virtual void preload(part *p) {}
            virtual void render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p) {}
            virtual void intersect(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p, const vec &o, const vec &ray) {}

            void bindpos(GLuint ebuf, GLuint vbuf, void *v, int stride, int type, int size);
            void bindpos(GLuint ebuf, GLuint vbuf, vec *v, int stride);
            void bindpos(GLuint ebuf, GLuint vbuf, GenericVec4<half> *v, int stride);
            void bindtc(void *v, int stride);
            void bindtangents(void *v, int stride);
            void bindbones(void *wv, void *bv, int stride);
        };

        static hashnameset<meshgroup *> meshgroups;

        struct linkedpart
        {
            part *p;
            int tag, anim, basetime;
            vec translate;
            vec *pos;
            matrix4 matrix;

            linkedpart() : p(nullptr), tag(-1), anim(-1), basetime(0), translate(0, 0, 0), pos(nullptr) {}
        };

        struct part
        {
            animmodel *model;
            int index;
            meshgroup *meshes;
            vector<linkedpart> links;
            vector<skin> skins;
            vector<animspec> *anims[maxanimparts];
            int numanimparts;
            float pitchscale, pitchoffset, pitchmin, pitchmax;

            part(animmodel *model, int index = 0) : model(model), index(index), meshes(nullptr), numanimparts(1), pitchscale(1), pitchoffset(0), pitchmin(0), pitchmax(0)
            {
                for(int k = 0; k < maxanimparts; ++k)
                {
                    anims[k] = nullptr;
                }
            }
            virtual ~part()
            {
                for(int k = 0; k < maxanimparts; ++k)
                {
                    DELETEA(anims[k]);
                }
            }

            virtual void cleanup();
            void disablepitch();
            void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m);
            void genBIH(vector<BIH::mesh> &bih, const matrix4x3 &m);
            void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m);
            bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), int anim = -1, int basetime = 0, vec *pos = nullptr);
            bool unlink(part *p);
            void initskins(Texture *tex = notexture, Texture *masks = notexture, int limit = 0);
            bool alphatested() const;
            void preloadBIH();
            void preloadshaders();
            void preloadmeshes();
            virtual void getdefaultanim(animinfo &info, int anim, uint varseed, dynent *d);
            bool calcanim(int animpart, int anim, int basetime, int basetime2, dynent *d, int interp, animinfo &info, int &animinterptime);
            void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray);
            void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray, AnimState *as);
            void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d);
            void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, AnimState *as);
            void setanim(int animpart, int num, int frame, int range, float speed, int priority = 0);
            bool animated() const;
            virtual void loaded();
        };

        enum
        {
            Link_Tag = 0,
            Link_Coop,
            Link_Reuse
        };

        static int intersectresult, intersectmode;
        static float intersectdist, intersectscale;

        void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a);
        void render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec4 &color);

        vector<part *> parts;

        animmodel(const char *name) : model(name)
        {
        }

        ~animmodel()
        {
            parts.deletecontents();
        }

        void cleanup();

        virtual void flushpart() {}

        part& addpart()
        {
            flushpart();
            part *p = new part(this, parts.length());
            parts.add(p);
            return *p;
        }

        void initmatrix(matrix4x3 &m);
        void genBIH(vector<BIH::mesh> &bih);
        void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &orient);
        void preloadBIH();
        BIH *setBIH();
        bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), int anim = -1, int basetime = 0, vec *pos = nullptr);

        bool unlink(part *p)
        {
            if(parts.empty())
            {
                return false;
            }
            return parts[0]->unlink(p);
        }

        bool animated() const;

        bool pitched() const
        {
            return parts[0]->pitchscale != 0;
        }

        bool alphatested() const;

        virtual bool flipy() const
        {
            return false;
        }

        virtual bool loadconfig()
        {
            return false;
        }

        virtual bool loaddefaultparts()
        {
            return false;
        }

        virtual void startload()
        {
        }

        virtual void endload()
        {
        }

        bool load();

        void preloadshaders();
        void preloadmeshes();

        void setshader(Shader *shader);
        void setspec(float spec);
        void setgloss(int gloss);
        void setglow(float glow, float delta, float pulse);
        void setalphatest(float alphatest);
        void setfullbright(float fullbright);
        void setcullface(int cullface);
        void setcolor(const vec &color);

        void calcbb(vec &center, vec &radius);
        void calctransform(matrix4x3 &m);

        virtual void loaded()
        {
            for(int i = 0; i < parts.length(); i++)
            {
                parts[i]->loaded();
            }
        }

        static bool enabletc, enablebones, enabletangents;
        static float sizescale;
        static int matrixpos;
        static matrix4 matrixstack[64];

        void startrender();
        static void disablebones();
        static void disabletangents();
        static void disabletc();
        static void disablevbo();
        void endrender();
    protected:
        virtual int linktype(animmodel *m, part *p) const
        {
            return Link_Tag;
        }
        int intersect(int anim, int basetime, int basetime2, const vec &pos, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec &o, const vec &ray, float &dist, int mode);

    private:
        void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a, const vec &o, const vec &ray);

        static bool enablecullface, enabledepthoffset;
        static vec4 colorscale;
        static GLuint lastvbuf, lasttcbuf, lastxbuf, lastbbuf, lastebuf;
        static Texture *lasttex, *lastdecal, *lastmasks, *lastnormalmap;
};

static inline uint hthash(const animmodel::shaderparams &k)
{
    return memhash(&k, sizeof(k));
}

static inline bool htcmp(const animmodel::shaderparams &x, const animmodel::shaderparams &y)
{
    return !memcmp(&x, &y, sizeof(animmodel::shaderparams));
}

template<class MDL, class BASE>
struct modelloader : BASE
{
    static MDL *loading;
    static string dir;

    modelloader(const char *name) : BASE(name) {}

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
        formatstring(dir, "media/model/%s", BASE::name);
        DEF_FORMAT_STRING(cfgname, "media/model/%s/%s.cfg", BASE::name, MDL::formatname());

        identflags &= ~Idf_Persist;
        bool success = execfile(cfgname, false);
        identflags |= Idf_Persist;
        return success;
    }
};

template<class MDL, class BASE>
MDL *modelloader<MDL, BASE>::loading = nullptr;

template<class MDL, class BASE>
string modelloader<MDL, BASE>::dir = {'\0'}; // crashes clang if "" is used here

template<class MDL, class MESH>
struct modelcommands
{
    typedef struct MDL::part part;
    typedef struct MDL::skin skin;

    static void setdir(char *name)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        formatstring(MDL::dir, "media/model/%s", name);
    }

    #define LOOP_MESHES(meshname, m, body) do { \
        if(!MDL::loading || MDL::loading->parts.empty()) \
        { \
            conoutf("not loading an %s", MDL::formatname()); \
            return; \
        } \
        part &mdl = *MDL::loading->parts.last(); \
        if(!mdl.meshes) \
        { \
            return; \
        } \
        for(int i = 0; i < mdl.meshes->meshes.length(); i++) \
        { \
            MESH &m = *static_cast<MESH *>(mdl.meshes->meshes[i]); \
            if(!strcmp(meshname, "*") || (m.name && !strcmp(m.name, meshname))) \
            { \
                body; \
            } \
        } \
    } while(0)

    #define LOOP_SKINS(meshname, s, body) do { \
        LOOP_MESHES(meshname, m, \
        { \
            skin &s = mdl.skins[i]; \
            body; \
        }); \
    } while(0)

    static void setskin(char *meshname, char *tex, char *masks)
    {
        LOOP_SKINS(meshname, s,
            s.tex = textureload(makerelpath(MDL::dir, tex), 0, true, false);
            if(*masks)
            {
                s.masks = textureload(makerelpath(MDL::dir, masks), 0, true, false);
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
        Texture *normalmaptex = textureload(makerelpath(MDL::dir, normalmapfile), 0, true, false);
        LOOP_SKINS(meshname, s, s.normalmap = normalmaptex);
    }

    static void setdecal(char *meshname, char *decal)
    {
        LOOP_SKINS(meshname, s,
            s.decal = textureload(makerelpath(MDL::dir, decal), 0, true, false);
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
        LOOP_MESHES(meshname, m, m.noclip = *noclip!=0);
    }

    static void settricollide(char *meshname)
    {
        bool init = true;
        LOOP_MESHES("*", m,
        {
            if(!m.cancollide)
            {
                init = false;
            }
        });
        if(init)
        {
            LOOP_MESHES("*", m, m.cancollide = false);
        }
        LOOP_MESHES(meshname, m,
        {
            m.cancollide = true;
            m.canrender = false;
        });
        MDL::loading->collide = Collide_TRI;
    }

#undef LOOP_MESHES
#undef LOOP_SKINS

    static void setlink(int *parent, int *child, char *tagname, float *x, float *y, float *z)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        if(!MDL::loading->parts.inrange(*parent) || !MDL::loading->parts.inrange(*child))
        {
            conoutf("no models loaded to link");
            return;
        }
        if(!MDL::loading->parts[*parent]->link(MDL::loading->parts[*child], tagname, vec(*x, *y, *z)))
        {
            conoutf("could not link model %s", MDL::loading->name);
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

#ifndef SKELMODEL_H_
#define SKELMODEL_H_

enum Bonemask
{
    Bonemask_Not  = 0x8000,
    Bonemask_End  = 0xFFFF,
    Bonemask_Bone = 0x7FFF
};

class skelhitdata; //defined in hitzone.h

/* skelmodel: implementation of model object for a skeletally rigged model
 *
 * skelmodel implements most of what is required to render a skeletally rigged
 * and animated model, using animmodel's specialization of model to implement
 * animations.
 *
 * extending skelmodel for a specific file format allows a program to use these
 * formats with skeletal animation support
 */
struct skelmodel : animmodel
{
    struct vert
    {
        vec pos, norm;
        vec2 tc;
        quat tangent;
        int blend, interpindex;
    };

    struct vvert
    {
        vec pos;
        GenericVec2<half> tc;
        squat tangent;
    };

    struct vvertg
    {
        vec4<half> pos;
        GenericVec2<half> tc;
        squat tangent;
    };

    struct vvertgw : vvertg
    {
        uchar weights[4];
        uchar bones[4];
    };

    struct tri
    {
        ushort vert[3];
    };

    class blendcombo
    {
        public:
            int uses, interpindex;
            float weights[4];
            uchar bones[4], interpbones[4];

            blendcombo();

            bool operator==(const blendcombo &c) const;

            int size() const;
            static bool sortcmp(const blendcombo &x, const blendcombo &y);
            int addweight(int sorted, float weight, int bone);
            void finalize(int sorted);

            void serialize(skelmodel::vvertgw &v);
    };


    struct animcacheentry
    {
        AnimState as[maxanimparts];
        float pitch;
        int millis;
        uchar *partmask;
        const ragdolldata *ragdoll;

        animcacheentry();

        bool operator==(const animcacheentry &c) const;
        bool operator!=(const animcacheentry &c) const;
    };

    struct vbocacheentry : animcacheentry
    {
        GLuint vbuf;
        int owner;

        vbocacheentry() : vbuf(0), owner(-1) {}
    };

    struct skelcacheentry : animcacheentry
    {
        dualquat *bdata;
        int version;

        skelcacheentry() : bdata(nullptr), version(-1) {}

        void nextversion()
        {
            version = Shader::uniformlocversion();
        }
    };

    struct blendcacheentry : skelcacheentry
    {
        int owner;

        blendcacheentry() : owner(-1) {}
    };

    struct skelmeshgroup;

    struct skelmesh : Mesh
    {
        vert *verts;
        tri *tris;
        int numverts, numtris, maxweights;

        int voffset, eoffset, elen;
        ushort minvert, maxvert;

        skelmesh() : verts(nullptr), tris(nullptr), numverts(0), numtris(0), maxweights(0)
        {
        }

        virtual ~skelmesh()
        {
            delete[] verts;
            delete[] tris;
        }

        int addblendcombo(const blendcombo &c);
        void smoothnorms(float limit = 0, bool areaweight = true);
        void buildnorms(bool areaweight = true);
        void calctangents(bool areaweight = true);
        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m);
        void genBIH(BIH::mesh &m);
        void genshadowmesh(std::vector<triangle> &out, const matrix4x3 &m);
        static void assignvert(vvertg &vv, int j, vert &v, blendcombo &c);
        static void assignvert(vvertgw &vv, int j, vert &v, blendcombo &c);

        template<class T>
        int genvbo(std::vector<ushort> &idxs, int offset, std::vector<T> &vverts)
        {
            voffset = offset;
            eoffset = idxs.size();
            for(int i = 0; i < numverts; ++i)
            {
                vert &v = verts[i];
                vverts.emplace_back(T());
                assignvert(vverts.back(), i, v, (static_cast<skelmeshgroup *>(group))->blendcombos[v.blend]);
            }
            for(int i = 0; i < numtris; ++i)
            {
                for(int j = 0; j < 3; ++j)
                {
                    idxs.push_back(voffset + tris[i].vert[j]);
                }
            }
            elen = idxs.size()-eoffset;
            minvert = voffset;
            maxvert = voffset + numverts-1;
            return numverts;
        }

        template<class T>
        int genvbo(std::vector<ushort> &idxs, int offset, std::vector<T> &vverts, int *htdata, int htlen)
        {
            voffset = offset;
            eoffset = idxs.size();
            minvert = 0xFFFF;
            for(int i = 0; i < numtris; ++i)
            {
                tri &t = tris[i];
                for(int j = 0; j < 3; ++j)
                {
                    int index = t.vert[j];
                    vert &v = verts[index];
                    T vv;
                    assignvert(vv, index, v, (static_cast<skelmeshgroup *>(group))->blendcombos[v.blend]);
                    int htidx = hthash(v.pos)&(htlen-1);
                    for(int k = 0; k < htlen; ++k)
                    {
                        int &vidx = htdata[(htidx+k)&(htlen-1)];
                        if(vidx < 0)
                        {
                            vidx = idxs.emplace_back(static_cast<ushort>(vverts.size()));
                            vverts.push_back(vv);
                            break;
                        }
                        else if(!memcmp(&vverts[vidx], &vv, sizeof(vv)))
                        {
                            minvert = std::min(minvert, idxs.emplace_back(static_cast<ushort>(vidx)));
                            break;
                        }
                    }
                }
            }
            elen = idxs.size()-eoffset;
            minvert = std::min(minvert, static_cast<ushort>(voffset));
            maxvert = std::max(minvert, static_cast<ushort>(vverts.size()-1));
            return vverts.size()-voffset;
        }

        int genvbo(std::vector<ushort> &idxs, int offset);

        template<class T>
        static inline void fillvert(T &vv, int j, vert &v)
        {
            vv.tc = v.tc;
        }

        template<class T>
        void fillverts(T *vdata)
        {
            vdata += voffset;
            for(int i = 0; i < numverts; ++i)
            {
                fillvert(vdata[i], i, verts[i]);
            }
        }

        template<class T>
        void interpverts(const dualquat * RESTRICT bdata1, const dualquat * RESTRICT bdata2, T * RESTRICT vdata, skin &s)
        {
            const int blendoffset = (static_cast<skelmeshgroup *>(group))->skel->numgpubones;
            bdata2 -= blendoffset;
            vdata += voffset;
            for(int i = 0; i < numverts; ++i)
            {
                const vert &src = verts[i];
                T &dst = vdata[i];
                const dualquat &b = (src.interpindex < blendoffset ? bdata1 : bdata2)[src.interpindex];
                dst.pos = b.transform(src.pos);
                quat q = b.transform(src.tangent);
                fixqtangent(q, src.tangent.w);
                dst.tangent = q;
            }
        }

        void setshader(Shader *s, int row);
        void render(const AnimState *as, skin &s, vbocacheentry &vc);
    };

    struct tag
    {
        std::string name;
        int bone;
        matrix4x3 matrix;

    };

    struct skelanimspec
    {
        std::string name;
        int frame, range;
    };

    struct boneinfo
    {
        const char *name;
        int parent, children, next, group, scheduled, interpindex, interpparent, ragdollindex, correctindex;
        float pitchscale, pitchoffset, pitchmin, pitchmax;
        dualquat base, invbase;

        boneinfo() : name(nullptr), parent(-1), children(-1), next(-1), group(INT_MAX), scheduled(-1), interpindex(-1), interpparent(-1), ragdollindex(-1), correctindex(-1), pitchscale(0), pitchoffset(0), pitchmin(0), pitchmax(0) {}
        ~boneinfo()
        {
            delete[] name;
        }
    };

    struct antipode
    {
        int parent, child;

        antipode(int parent, int child) : parent(parent), child(child) {}
    };

    struct pitchdep
    {
        int bone, parent;
        dualquat pose;
    };

    struct pitchtarget
    {
        int bone, frame, corrects, deps;
        float pitchmin, pitchmax, deviated;
        dualquat pose;
    };

    struct pitchcorrect
    {
        int bone, target, parent;
        float pitchmin, pitchmax, pitchscale, pitchangle, pitchtotal;

        pitchcorrect() : parent(-1), pitchangle(0), pitchtotal(0) {}
    };

    struct skeleton
    {
        char *name;
        int shared;
        std::vector<skelmeshgroup *> users;
        boneinfo *bones;
        int numbones, numinterpbones, numgpubones, numframes;
        dualquat *framebones;
        std::vector<skelanimspec> skelanims;
        std::vector<tag> tags;
        std::vector<antipode> antipodes;
        ragdollskel *ragdoll;
        std::vector<pitchdep> pitchdeps;
        std::vector<pitchtarget> pitchtargets;
        std::vector<pitchcorrect> pitchcorrects;

        bool usegpuskel;
        std::vector<skelcacheentry> skelcache;
        std::unordered_map<GLuint, int> blendoffsets;

        skeleton() : name(nullptr), shared(0), bones(nullptr), numbones(0), numinterpbones(0), numgpubones(0), numframes(0), framebones(nullptr), ragdoll(nullptr), usegpuskel(false), blendoffsets()
        {
        }

        ~skeleton()
        {
            delete[] name;
            delete[] bones;
            delete[] framebones;
            if(ragdoll)
            {
                delete ragdoll;
                ragdoll = nullptr;
            }
            for(uint i = 0; i < skelcache.size(); i++)
            {
                delete[] skelcache[i].bdata;
            }
        }

        const skelanimspec *findskelanim(const char *name, char sep = '\0') const;
        skelanimspec &addskelanim(const char *name);
        int findbone(const char *name) const;
        int findtag(const char *name) const;
        bool addtag(const char *name, int bone, const matrix4x3 &matrix);
        void addpitchdep(int bone, int frame);
        int findpitchdep(int bone) const;
        int findpitchcorrect(int bone) const;
        void initpitchdeps();
        void optimize();
        void expandbonemask(uchar *expansion, int bone, int val);
        void applybonemask(ushort *mask, uchar *partmask, int partindex);
        void linkchildren();
        int availgpubones() const;
        float calcdeviation(const vec &axis, const vec &forward, const dualquat &pose1, const dualquat &pose2);
        void calcpitchcorrects(float pitch, const vec &axis, const vec &forward);
        void interpbones(const AnimState *as, float pitch, const vec &axis, const vec &forward, int numanimparts, const uchar *partmask, skelcacheentry &sc);
        void initragdoll(ragdolldata &d, const skelcacheentry &sc, const part * const p);
        void genragdollbones(const ragdolldata &d, skelcacheentry &sc, const part * const p);
        void concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n);
        void calctags(part *p, const skelcacheentry *sc = nullptr);
        void cleanup(bool full = true);
        bool canpreload() const;
        void preload();
        skelcacheentry &checkskelcache(const part * const p, const AnimState *as, float pitch, const vec &axis, const vec &forward, const ragdolldata * const rdata);
        int getblendoffset(const UniformLoc &u);
        void setgpubones(skelcacheentry &sc, blendcacheentry *bc, int count);
        bool shouldcleanup() const;

        private:

            void calcantipodes();
            void remapbones();

            struct framedata
            {
                const dualquat *fr1, *fr2, *pfr1, *pfr2;
            };

            void setglslbones(UniformLoc &u, const skelcacheentry &sc, const skelcacheentry &bc, int count);
            bool gpuaccelerate() const;
            dualquat interpbone(int bone, framedata partframes[maxanimparts], const AnimState *as, const uchar *partmask);
    };

    static std::map<std::string, skeleton *> skeletons;

    struct skelmeshgroup : meshgroup
    {
        skeleton *skel;

        std::vector<blendcombo> blendcombos;
        int numblends[4];

        static constexpr int maxblendcache = 16; //number of entries in the blendcache entry array
        static constexpr int maxvbocache = 16;   //number of entries in the vertex buffer object array

        blendcacheentry blendcache[maxblendcache];

        vbocacheentry vbocache[maxvbocache];

        ushort *edata;
        GLuint ebuf;
        int vlen, vertsize, vblends, vweights;
        uchar *vdata;

        skelhitdata *hitdata;

        skelmeshgroup() : skel(nullptr), edata(nullptr), ebuf(0), vlen(0), vertsize(0), vblends(0), vweights(0), vdata(nullptr), hitdata(nullptr)
        {
            memset(numblends, 0, sizeof(numblends));
        }

        virtual ~skelmeshgroup();

        void shareskeleton(const char *name);
        int findtag(const char *name);
        void *animkey();
        int totalframes() const;

        virtual const skelanimspec *loadanim(const char *filename)
        {
            return nullptr;
        }

        void genvbo(vbocacheentry &vc);

        template<class T>
        void bindbones(T *vverts)
        {
            if(enablebones)
            {
                disablebones();
            }
        }
        void bindbones(vvertgw *vverts)
        {
            meshgroup::bindbones(vverts->weights, vverts->bones, vertsize);
        }

        template<class T>
        void bindvbo(const AnimState *as, part *p, vbocacheentry &vc)
        {
            T *vverts = 0;
            bindpos(ebuf, vc.vbuf, &vverts->pos, vertsize);
            if(as->cur.anim & Anim_NoSkin)
            {
                if(enabletangents)
                {
                    disabletangents();
                }
                if(p->alphatested())
                {
                    bindtc(&vverts->tc, vertsize);
                }
                else if(enabletc)
                {
                    disabletc();
                }
            }
            else
            {
                bindtangents(&vverts->tangent, vertsize);

                bindtc(&vverts->tc, vertsize);
            }
            bindbones(vverts);
        }

        void bindvbo(const AnimState *as, part *p, vbocacheentry &vc, skelcacheentry *sc = nullptr, blendcacheentry *bc = nullptr);
        void concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n);
        int addblendcombo(const blendcombo &c);
        void sortblendcombos();
        int remapblend(int blend);
        static void blendbones(dualquat &d, const dualquat *bdata, const blendcombo &c);
        void blendbones(const skelcacheentry &sc, blendcacheentry &bc);
        static void blendbones(const dualquat *bdata, dualquat *dst, const blendcombo *c, int numblends);
        void cleanup();
        vbocacheentry &checkvbocache(const skelcacheentry &sc, int owner);
        blendcacheentry &checkblendcache(const skelcacheentry &sc, int owner);
        //hitzone
        void cleanuphitdata();
        void deletehitdata();
        void buildhitdata(const uchar *hitzones);
        void intersect(skelhitdata *z, part *p, const skelmodel::skelcacheentry &sc, const vec &o, const vec &ray) const;
        //end hitzone.h
        void intersect(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p, const vec &o, const vec &ray);
        void preload();

        void render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p);

        virtual bool load(const char *name, float smooth) = 0;
    };

    virtual skelmeshgroup *newmeshes() = 0;

    meshgroup *loadmeshes(const char *name, const char *skelname = nullptr, float smooth = 2);
    meshgroup *sharemeshes(const char *name, const char *skelname = nullptr, float smooth = 2);

    struct animpartmask
    {
        animpartmask *next;
        int numbones;
        uchar bones[1];
    };

    class skelpart : public part
    {
        public:
            animpartmask *buildingpartmask;

            uchar *partmask;

            skelpart(animmodel *model, int index = 0) : part(model, index), buildingpartmask(nullptr), partmask(nullptr)
            {
            }

            virtual ~skelpart()
            {
                delete[] buildingpartmask;
            }

            void initanimparts();
            bool addanimpart(ushort *bonemask);
            void loaded();
        private:
            uchar *sharepartmask(animpartmask *o);
            animpartmask *newpartmask();
            void endanimparts();
    };

    skelmodel(const char *name) : animmodel(name)
    {
    }

    int linktype(animmodel *m, part *p) const
    {
        return type()==m->type() &&
            (static_cast<skelmeshgroup *>(parts[0]->meshes))->skel == (static_cast<skelmeshgroup *>(p->meshes))->skel ?
                Link_Reuse :
                Link_Tag;
    }

    bool skeletal() const
    {
        return true;
    }

    skelpart &addpart()
    {
        flushpart();
        skelpart *p = new skelpart(this, parts.size());
        parts.push_back(p);
        return *p;
    }
};

class skeladjustment
{
    public:
        skeladjustment(float yaw, float pitch, float roll, const vec &translate) : yaw(yaw), pitch(pitch), roll(roll), translate(translate) {}
        void adjust(dualquat &dq);

    private:
        float yaw, pitch, roll;
        vec translate;
};

template<class MDL>
struct skelloader : modelloader<MDL, skelmodel>
{
    static std::vector<skeladjustment> adjustments;
    static std::vector<uchar> hitzones;

    skelloader(const char *name) : modelloader<MDL, skelmodel>(name) {}

    void flushpart()
    {
        if(hitzones.size() && skelmodel::parts.size())
        {
            skelmodel::skelpart *p = static_cast<skelmodel::skelpart *>(skelmodel::parts.back());
            skelmodel::skelmeshgroup *m = static_cast<skelmodel::skelmeshgroup *>(p->meshes);
            if(m)
            {
                m->buildhitdata(hitzones.data());
            }
        }

        adjustments.clear();
        hitzones.clear();
    }
};

template<class MDL>
std::vector<skeladjustment> skelloader<MDL>::adjustments;

template<class MDL>
std::vector<uchar> skelloader<MDL>::hitzones;

/*
 * this template structure defines a series of commands for a model object (or
 * child of the model object) which can be used to set its dynamically modifiable
 * properties
 *
 */
template<class MDL>
struct skelcommands : modelcommands<MDL, struct MDL::skelmesh>
{
    typedef modelcommands<MDL, struct MDL::skelmesh> commands;
    typedef struct MDL::skeleton skeleton;
    typedef struct MDL::skelmeshgroup meshgroup;
    typedef class  MDL::skelpart part;
    typedef struct MDL::skin skin;
    typedef struct MDL::boneinfo boneinfo;
    typedef struct MDL::skelanimspec animspec;
    typedef struct MDL::pitchdep pitchdep;
    typedef struct MDL::pitchtarget pitchtarget;
    typedef struct MDL::pitchcorrect pitchcorrect;

    static void loadpart(char *meshfile, char *skelname, float *smooth)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir, meshfile);
        part &mdl = MDL::loading->addpart();
        mdl.meshes = MDL::loading->sharemeshes(path(filename), skelname[0] ? skelname : nullptr, *smooth > 0 ? std::cos(std::clamp(*smooth, 0.0f, 180.0f)/RAD) : 2);
        if(!mdl.meshes)
        {
            conoutf("could not load %s", filename);
        }
        else
        {
            if(mdl.meshes && static_cast<meshgroup *>(mdl.meshes)->skel->numbones > 0)
            {
                mdl.disablepitch();
            }
            mdl.initanimparts();
            mdl.initskins();
        }
    }

    static void settag(char *name, char *tagname, float *tx, float *ty, float *tz, float *rx, float *ry, float *rz)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *static_cast<part *>(MDL::loading->parts.back());
        int i = mdl.meshes ? static_cast<meshgroup *>(mdl.meshes)->skel->findbone(name) : -1;
        if(i >= 0)
        {
            float cx = *rx ? std::cos(*rx/(2*RAD)) : 1, sx = *rx ? std::sin(*rx/(2*RAD)) : 0,
                  cy = *ry ? std::cos(*ry/(2*RAD)) : 1, sy = *ry ? std::sin(*ry/(2*RAD)) : 0,
                  cz = *rz ? std::cos(*rz/(2*RAD)) : 1, sz = *rz ? std::sin(*rz/(2*RAD)) : 0;
            matrix4x3 m(matrix3(quat(sx*cy*cz - cx*sy*sz, cx*sy*cz + sx*cy*sz, cx*cy*sz - sx*sy*cz, cx*cy*cz + sx*sy*sz)),
                        vec(*tx, *ty, *tz));
            static_cast<meshgroup *>(mdl.meshes)->skel->addtag(tagname, i, m);
            return;
        }
        conoutf("could not find bone %s for tag %s", name, tagname);
    }

    //attempts to set the pitch of a named bone within a MDL object, within the bounds set
    //prints to console failure messages if no model or no bone with name passed
    static void setpitch(char *name, float *pitchscale, float *pitchoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *static_cast<part *>(MDL::loading->parts.back());

        if(name[0])
        {
            int i = mdl.meshes ? static_cast<meshgroup *>(mdl.meshes)->skel->findbone(name) : -1;
            if(i>=0)
            {
                boneinfo &b = static_cast<meshgroup *>(mdl.meshes)->skel->bones[i];
                b.pitchscale = *pitchscale;
                b.pitchoffset = *pitchoffset;
                if(*pitchmin || *pitchmax)
                {
                    b.pitchmin = *pitchmin;
                    b.pitchmax = *pitchmax;
                }
                else
                {
                    b.pitchmin = -360*std::fabs(b.pitchscale) + b.pitchoffset;
                    b.pitchmax = 360*std::fabs(b.pitchscale) + b.pitchoffset;
                }
                return;
            }
            conoutf("could not find bone %s to pitch", name);
            return;
        }

        mdl.pitchscale = *pitchscale;
        mdl.pitchoffset = *pitchoffset;
        if(*pitchmin || *pitchmax)
        {
            mdl.pitchmin = *pitchmin;
            mdl.pitchmax = *pitchmax;
        }
        else
        {
            mdl.pitchmin = -360*std::fabs(mdl.pitchscale) + mdl.pitchoffset;
            mdl.pitchmax = 360*std::fabs(mdl.pitchscale) + mdl.pitchoffset;
        }
    }

    static void setpitchtarget(char *name, char *animfile, int *frameoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *static_cast<part *>(MDL::loading->parts.back());
        if(!mdl.meshes)
        {
            return;
        }
        DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir, animfile);
        const animspec *sa = static_cast<meshgroup *>(mdl.meshes)->loadanim(path(filename));
        if(!sa)
        {
            conoutf("could not load %s anim file %s", MDL::formatname(), filename);
            return;
        }
        skeleton *skel = static_cast<meshgroup *>(mdl.meshes)->skel;
        int bone = skel ? skel->findbone(name) : -1;
        if(bone < 0)
        {
            conoutf("could not find bone %s to pitch target", name);
            return;
        }
        for(uint i = 0; i < skel->pitchtargets.size(); i++)
        {
            if(skel->pitchtargets[i].bone == bone)
            {
                return;
            }
        }
        pitchtarget t;
        t.bone = bone;
        t.frame = sa->frame + std::clamp(*frameoffset, 0, sa->range-1);
        t.pitchmin = *pitchmin;
        t.pitchmax = *pitchmax;
        skel->pitchtargets.push_back(t);
    }

    static void setpitchcorrect(char *name, char *targetname, float *scale, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *static_cast<part *>(MDL::loading->parts.back());
        if(!mdl.meshes)
        {
            return;
        }
        skeleton *skel = static_cast<meshgroup *>(mdl.meshes)->skel;
        int bone = skel ? skel->findbone(name) : -1;
        if(bone < 0)
        {
            conoutf("could not find bone %s to pitch correct", name);
            return;
        }
        if(skel->findpitchcorrect(bone) >= 0)
        {
            return;
        }
        int targetbone = skel->findbone(targetname),
            target = -1;
        if(targetbone >= 0)
        {
            for(uint i = 0; i < skel->pitchtargets.size(); i++)
            {
                if(skel->pitchtargets[i].bone == targetbone)
                {
                    target = i;
                    break;
                }
            }
        }
        if(target < 0)
        {
            conoutf("could not find pitch target %s to pitch correct %s", targetname, name);
            return;
        }
        pitchcorrect c;
        c.bone = bone;
        c.target = target;
        c.pitchmin = *pitchmin;
        c.pitchmax = *pitchmax;
        c.pitchscale = *scale;
        uint pos = skel->pitchcorrects.size();
        for(uint i = 0; i < skel->pitchcorrects.size(); i++)
        {
            if(bone <= skel->pitchcorrects[i].bone)
            {
                pos = i;
                break;
            }
        }
        skel->pitchcorrects.insert(skel->pitchcorrects.begin() + pos, c);
    }

    //attempts to give a model object an animation by the name of *anim parameter
    //loaded from *animfile with speed/priority/offsets to determine how fast
    //and what frames play
    static void setanim(char *anim, char *animfile, float *speed, int *priority, int *startoffset, int *endoffset)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        std::vector<int> anims = findanims(anim);
        if(anims.empty())
        {
            conoutf("could not find animation %s", anim);
        }
        else
        {
            part *p = static_cast<part *>(MDL::loading->parts.back());
            if(!p->meshes)
            {
                return;
            }
            DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir, animfile);
            const animspec *sa = static_cast<meshgroup *>(p->meshes)->loadanim(path(filename));
            if(!sa)
            {
                conoutf("could not load %s anim file %s", MDL::formatname(), filename);
            }
            else
            {
                for(int i = 0; i < static_cast<int>(anims.size()); i++)
                {
                    int start = sa->frame,
                        end = sa->range;
                    if(*startoffset > 0)
                    {
                        start += std::min(*startoffset, end-1);
                    }
                    else if(*startoffset < 0)
                    {
                        start += std::max(end + *startoffset, 0);
                    }
                    end -= start - sa->frame;
                    if(*endoffset > 0)
                    {
                        end = std::min(end, *endoffset);
                    }
                    else if(*endoffset < 0)
                    {
                        end = std::max(end + *endoffset, 1);
                    }
                    MDL::loading->parts.back()->setanim(p->numanimparts-1, anims[i], start, end, *speed, *priority);
                }
            }
        }
    }

    static void setanimpart(char *maskstr)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part *p = static_cast<part *>(MDL::loading->parts.back());

        std::vector<char *> bonestrs;
        explodelist(maskstr, bonestrs);
        std::vector<ushort> bonemask;
        for(uint i = 0; i < bonestrs.size(); i++)
        {
            char *bonestr = bonestrs[i];
            int bone = p->meshes ? static_cast<meshgroup *>(p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr+1 : bonestr) : -1;
            if(bone<0)
            {
                conoutf("could not find bone %s for anim part mask [%s]", bonestr, maskstr);
                for(char* j : bonestrs)
                {
                    delete[] j;
                }
                return;
            }
            bonemask.push_back(bone | (bonestr[0]=='!' ? Bonemask_Not : 0));
        }
        for(char* i : bonestrs)
        {
            delete[] i;
        }
        std::sort(bonemask.begin(), bonemask.end());
        if(bonemask.size())
        {
            bonemask.push_back(Bonemask_End);
        }
        if(!p->addanimpart(bonemask.data()))
        {
            conoutf("too many animation parts");
        }
    }

    static void setadjust(char *name, float *yaw, float *pitch, float *roll, float *tx, float *ty, float *tz)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *static_cast<part *>(MDL::loading->parts.back());
        if(!name[0])
        {
            return;
        }
        int i = mdl.meshes ? static_cast<meshgroup *>(mdl.meshes)->skel->findbone(name) : -1;
        if(i < 0)
        {
            conoutf("could not find bone %s to adjust", name);
            return;
        }
        while(!(static_cast<int>(MDL::adjustments.size()) > i))
        {
            MDL::adjustments.push_back(skeladjustment(0, 0, 0, vec(0, 0, 0)));
        }
        MDL::adjustments[i] = skeladjustment(*yaw, *pitch, *roll, vec(*tx/4, *ty/4, *tz/4));
    }

    static void sethitzone(int *id, char *maskstr)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        if(*id >= 0x80)
        {
            conoutf("invalid hit zone id %d", *id);
            return;
        }
        part *p = static_cast<part *>(MDL::loading->parts.back());
        meshgroup *m = static_cast<meshgroup *>(p->meshes);
        if(!m || m->hitdata)
        {
            return;
        }
        std::vector<char *> bonestrs;
        explodelist(maskstr, bonestrs);
        std::vector<ushort> bonemask;
        for(uint i = 0; i < bonestrs.size(); i++)
        {
            char *bonestr = bonestrs[i];
            int bone = p->meshes ? static_cast<meshgroup *>(p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr+1 : bonestr) : -1;
            if(bone<0)
            {
                conoutf("could not find bone %s for hit zone mask [%s]", bonestr, maskstr);
                for(char* j : bonestrs)
                {
                    delete[] j;
                }
                return;
            }
            bonemask.push_back(bone | (bonestr[0]=='!' ? Bonemask_Not : 0));
        }
        for(char* i : bonestrs)
        {
            delete[] i;
        }
        if(bonemask.empty())
        {
            return;
        }
        std::sort(bonemask.begin(), bonemask.end());
        bonemask.push_back(Bonemask_End);

        while(static_cast<int>(MDL::hitzones.size()) < m->skel->numbones)
        {
            MDL::hitzones.emplace_back(0xFF);
        }
        m->skel->applybonemask(bonemask.data(), MDL::hitzones.data(), *id < 0 ? 0xFF : *id);
    }

    skelcommands()
    {
        if(MDL::multiparted())
        {
            this->modelcommand(loadpart, "load", "ssf"); //<fmt>load [mesh] [skel] [smooth]
        }
        this->modelcommand(settag, "tag", "ssffffff"); //<fmt>tag [name] [tag] [tx] [ty] [tz] [rx] [ry] [rz]
        this->modelcommand(setpitch, "pitch", "sffff"); //<fmt>pitch [name] [target] [scale] [min] [max]
        this->modelcommand(setpitchtarget, "pitchtarget", "ssiff"); //<fmt>pitchtarget [name] [anim] [offset] [min] [max]
        this->modelcommand(setpitchcorrect, "pitchcorrect", "ssfff"); //<fmt>pitchcorrect [name] [target] [scale] [min] [max]
        this->modelcommand(sethitzone, "hitzone", "is"); //<fmt>hitzone [id] [mask]
        if(MDL::cananimate())
        {
            this->modelcommand(setanim, "anim", "ssfiii"); //<fmt>anim [anim] [animfile] [speed] [priority] [startoffset] [endoffset]
            this->modelcommand(setanimpart, "animpart", "s"); //<fmt>animpart [maskstr]
            this->modelcommand(setadjust, "adjust", "sffffff"); //<fmt>adjust [name] [yaw] [pitch] [tx] [ty] [tz]
        }
    }
};

#endif

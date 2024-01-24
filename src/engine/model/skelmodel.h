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
    struct vert final
    {
        vec pos, norm;
        vec2 tc;
        quat tangent;
        int blend, interpindex;
    };

    struct vvert final
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

    struct vvertgw final : vvertg
    {
        uchar weights[4];
        uchar bones[4];
    };

    struct tri final
    {
        uint vert[3];
    };

    class blendcombo final
    {
        public:
            int uses, interpindex;

            struct BoneData
            {
                float weights;
                uchar bones;
                uchar interpbones;
            };
            std::array<BoneData, 4> bonedata;

            blendcombo();

            bool operator==(const blendcombo &c) const;

            size_t size() const;
            static bool sortcmp(const blendcombo &x, const blendcombo &y);
            int addweight(int sorted, float weight, int bone);
            void finalize(int sorted);

            void serialize(skelmodel::vvertgw &v) const;
    };

    struct animcacheentry
    {
        std::array<AnimState, maxanimparts> as;
        float pitch;
        int millis;
        const std::vector<uchar> * partmask;
        const ragdolldata *ragdoll;

        animcacheentry();

        bool operator==(const animcacheentry &c) const;
        bool operator!=(const animcacheentry &c) const;
    };

    struct vbocacheentry final : animcacheentry
    {
        GLuint vbuf;
        int owner;

        bool check() const;
        vbocacheentry();
    };

    struct skelcacheentry : animcacheentry
    {
        dualquat *bdata;
        int version;

        skelcacheentry();
        void nextversion();
    };

    struct blendcacheentry final : skelcacheentry
    {
        int owner;

        blendcacheentry();
        bool check() const;
    };

    struct skelmeshgroup;

    struct skelmesh : Mesh
    {
        vert *verts;
        tri *tris;
        int numverts, numtris, maxweights;

        int voffset, eoffset, elen;
        GLuint minvert, maxvert;

        skelmesh();

        virtual ~skelmesh();

        int addblendcombo(const blendcombo &c);
        void smoothnorms(float limit = 0, bool areaweight = true);
        void buildnorms(bool areaweight = true);
        void calctangents(bool areaweight = true);
        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) override final;
        void genBIH(BIH::mesh &m) const override final;
        void genshadowmesh(std::vector<triangle> &out, const matrix4x3 &m) const override final;
        static void assignvert(vvertg &vv, int j, const vert &v);
        static void assignvert(vvertgw &vv, int j, const vert &v, const blendcombo &c);

        int genvbo(std::vector<GLuint> &idxs, int offset, std::vector<vvertgw> &vverts);
        int genvbo(std::vector<GLuint> &idxs, int offset, std::vector<vvertg> &vverts, int *htdata, int htlen);
        int genvbo(std::vector<GLuint> &idxs, int offset);

        static void fillvert(vvert &vv, int j, vert &v);
        void fillverts(vvert *vdata);
        void interpverts(const dualquat * RESTRICT bdata1, const dualquat * RESTRICT bdata2, vvert * RESTRICT vdata, skin &s);
        void setshader(Shader *s, int row) override final;
        void render(const AnimState *as, skin &s, vbocacheentry &vc);
    };

    struct skelanimspec final
    {
        std::string name;
        int frame, range;
    };

    struct boneinfo final
    {
        const char *name;
        int parent, children, next, group, scheduled, interpindex, interpparent, ragdollindex, correctindex;
        float pitchscale, pitchoffset, pitchmin, pitchmax;
        dualquat base;

        boneinfo();
        ~boneinfo();
    };

    struct pitchtarget final
    {
        int bone, frame, corrects, deps;
        float pitchmin, pitchmax, deviated;
        dualquat pose;
    };

    struct pitchcorrect final
    {
        int bone, target, parent;
        float pitchmin, pitchmax, pitchscale, pitchangle, pitchtotal;

        pitchcorrect(int bone, int target, float pitchscale, float pitchmin, float pitchmax);
        pitchcorrect();
    };

    class skeleton
    {
        public:
            std::string name;
            skelmeshgroup * const owner;
            boneinfo *bones; //array of boneinfo, size equal to numbones
            int numbones, numinterpbones, numgpubones, numframes;
            dualquat *framebones; //array of quats, size equal to anim frames * bones in model
            std::vector<skelanimspec> skelanims;
            ragdollskel *ragdoll; //optional ragdoll object if ragdoll is in effect
            std::vector<pitchtarget> pitchtargets;
            std::vector<pitchcorrect> pitchcorrects;

            bool usegpuskel;
            std::vector<skelcacheentry> skelcache;
            std::unordered_map<GLuint, int> blendoffsets;

            skeleton(skelmeshgroup * const group);
            ~skeleton();

            const skelanimspec *findskelanim(std::string_view name, char sep = '\0') const;
            skelanimspec &addskelanim(const char *name);
            std::optional<int> findbone(const std::string &name) const;
            int findtag(std::string_view name) const;
            bool addtag(std::string_view name, int bone, const matrix4x3 &matrix);
            void addpitchdep(int bone, int frame);
            int findpitchdep(int bone) const;
            int findpitchcorrect(int bone) const;
            void initpitchdeps();
            void optimize();
            void expandbonemask(uchar *expansion, int bone, int val) const;
            void applybonemask(const std::vector<uint> &mask, std::vector<uchar> &partmask, int partindex) const;
            void linkchildren();
            int availgpubones() const;
            float calcdeviation(const vec &axis, const vec &forward, const dualquat &pose1, const dualquat &pose2) const;
            void calcpitchcorrects(float pitch, const vec &axis, const vec &forward);
            void interpbones(const AnimState *as, float pitch, const vec &axis, const vec &forward, int numanimparts, const uchar *partmask, skelcacheentry &sc);
            void initragdoll(ragdolldata &d, const skelcacheentry &sc, const part * const p);
            void genragdollbones(const ragdolldata &d, skelcacheentry &sc, const part * const p);
            void concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const;
            void calctags(part *p, const skelcacheentry *sc = nullptr) const;
            void cleanup(bool full = true);
            bool canpreload() const;
            void preload();
            const skelcacheentry &checkskelcache(const part * const p, const AnimState *as, float pitch, const vec &axis, const vec &forward, const ragdolldata * const rdata);
            void setgpubones(const skelcacheentry &sc, blendcacheentry *bc, int count);
            bool shouldcleanup() const;

        private:
            struct pitchdep
            {
                int bone, parent;
                dualquat pose;
            };
            std::vector<pitchdep> pitchdeps;

            struct antipode
            {
                int parent, child;

                antipode(int parent, int child) : parent(parent), child(child) {}
            };
            std::vector<antipode> antipodes;

            struct tag
            {
                std::string name;
                int bone;
                matrix4x3 matrix;
            };
            std::vector<tag> tags;

            void calcantipodes();
            void remapbones();

            struct framedata
            {
                const dualquat *fr1, *fr2, *pfr1, *pfr2;
            };

            void setglslbones(UniformLoc &u, const skelcacheentry &sc, const skelcacheentry &bc, int count);
            int getblendoffset(const UniformLoc &u);
            bool gpuaccelerate() const;
            dualquat interpbone(int bone, const std::array<framedata, maxanimparts> &partframes, const AnimState *as, const uchar *partmask);
    };

    class skelmeshgroup : public meshgroup
    {
        public:
            skeleton *skel;

            std::vector<blendcombo> blendcombos;

            GLuint *edata;
            int vweights;

            skelmeshgroup() : skel(nullptr), edata(nullptr), vweights(0), ebuf(0), vlen(0), vertsize(0), vblends(0), vdata(nullptr)
            {
                numblends.fill(0);
            }

            virtual ~skelmeshgroup();

            int findtag(std::string_view) override final;
            void *animkey() override final;
            int totalframes() const override final;
            void concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const override final;
            void preload() override final;
            void render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p) override final;

            //for vvert, vvertg (vvertgw see below function), disable bones if active
            template<class T>
            void bindbones(T *)
            {
                if(enablebones)
                {
                    disablebones();
                }
            }
            //for vvertgw only, call parent bindbones function
            void bindbones(const vvertgw *vverts)
            {
                meshgroup::bindbones(vverts->weights, vverts->bones, vertsize);
            }

            template<class T>
            void bindvbo(const AnimState *as, part *p, const vbocacheentry &vc)
            {
                T *vverts = nullptr;
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

            void makeskeleton();
            void genvbo(vbocacheentry &vc);
            void bindvbo(const AnimState *as, part *p, vbocacheentry &vc, const skelcacheentry *sc = nullptr);
            int addblendcombo(const blendcombo &c);
            void sortblendcombos();
            int remapblend(int blend);
            static void blendbones(dualquat &d, const dualquat *bdata, const blendcombo &c);
            void blendbones(const skelcacheentry &sc, blendcacheentry &bc);
            static void blendbones(const dualquat *bdata, dualquat *dst, const blendcombo *c, int numblends);
            void cleanup() override final;
            vbocacheentry &checkvbocache(const skelcacheentry &sc, int owner);
            blendcacheentry &checkblendcache(const skelcacheentry &sc, int owner);

            virtual bool load(const char *name, float smooth, part &p) = 0;
            virtual const skelanimspec *loadanim(const char *filename) = 0;
        private:
            std::array<int, 4> numblends;

            static constexpr int maxblendcache = 16; //number of entries in the blendcache entry array
            static constexpr int maxvbocache = 16;   //number of entries in the vertex buffer object array

            blendcacheentry blendcache[maxblendcache];
            vbocacheentry vbocache[maxvbocache];
            GLuint ebuf;
            int vlen, vertsize, vblends;
            uchar *vdata;

    };

    class skelpart : public part
    {
        public:
            std::vector<uchar> partmask;

            skelpart(animmodel *model, int index = 0);
            virtual ~skelpart();

            void initanimparts();
            bool addanimpart(const std::vector<uint> &bonemask);
            void loaded() override final;
        private:
            std::vector<uchar> buildingpartmask;

            std::vector<uchar> &sharepartmask(std::vector<uchar> &o);
            std::vector<uchar> newpartmask();
            void endanimparts();
    };

    //ordinary methods
    skelmodel(std::string name);
    skelpart &addpart();
    meshgroup *loadmeshes(const char *name, float smooth = 2);
    meshgroup *sharemeshes(const char *name, float smooth = 2);
    //virtual methods
    virtual skelmeshgroup *newmeshes() = 0;
    //override methods
    int linktype(const animmodel *m, const part *p) const override final;
    bool skeletal() const override final;

};

class skeladjustment final
{
    public:
        skeladjustment(float yaw, float pitch, float roll, const vec &translate) : yaw(yaw), pitch(pitch), roll(roll), translate(translate) {}
        void adjust(dualquat &dq) const;

    private:
        float yaw, pitch, roll;
        vec translate;
};

template<class MDL>
struct skelloader : modelloader<MDL, skelmodel>
{
    static std::vector<skeladjustment> adjustments;
    static std::vector<uchar> hitzones;

    skelloader(std::string name) : modelloader<MDL, skelmodel>(name) {}
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
struct skelcommands : modelcommands<MDL>
{
    typedef modelcommands<MDL> commands;
    typedef class  MDL::skeleton skeleton;
    typedef struct MDL::skelmeshgroup meshgroup;
    typedef class  MDL::skelpart part;
    typedef struct MDL::boneinfo boneinfo;
    typedef struct MDL::skelanimspec animspec;
    typedef struct MDL::pitchtarget pitchtarget;
    typedef struct MDL::pitchcorrect pitchcorrect;

    //unused second param
    static void loadpart(const char *meshfile, const char *, const float *smooth)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        std::string filename;
        filename.append(MDL::dir).append("/").append(meshfile);
        part &mdl = MDL::loading->addpart();
        mdl.meshes = MDL::loading->sharemeshes(path(filename).c_str(), *smooth > 0 ? std::cos(std::clamp(*smooth, 0.0f, 180.0f)/RAD) : 2);
        if(!mdl.meshes)
        {
            conoutf("could not load %s", filename.c_str());
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

    static void settag(const char *name, const char *tagname,
                       const float *tx, const float *ty, const float *tz,
                       const float *rx, const float *ry, const float *rz)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *static_cast<part *>(MDL::loading->parts.back());
        std::optional<int> i = mdl.meshes ? static_cast<meshgroup *>(mdl.meshes)->skel->findbone(name) : std::nullopt;
        if(i)
        {
            float cx = *rx ? std::cos(*rx/(2*RAD)) : 1, sx = *rx ? std::sin(*rx/(2*RAD)) : 0,
                  cy = *ry ? std::cos(*ry/(2*RAD)) : 1, sy = *ry ? std::sin(*ry/(2*RAD)) : 0,
                  cz = *rz ? std::cos(*rz/(2*RAD)) : 1, sz = *rz ? std::sin(*rz/(2*RAD)) : 0;
            matrix4x3 m(matrix3(quat(sx*cy*cz - cx*sy*sz, cx*sy*cz + sx*cy*sz, cx*cy*sz - sx*sy*cz, cx*cy*cz + sx*sy*sz)),
                        vec(*tx, *ty, *tz));
            static_cast<meshgroup *>(mdl.meshes)->skel->addtag(tagname, *i, m);
            return;
        }
        conoutf("could not find bone %s for tag %s", name, tagname);
    }

    //attempts to set the pitch of a named bone within a MDL object, within the bounds set
    //prints to console failure messages if no model or no bone with name passed
    static void setpitch(const char *name, const float *pitchscale,
                         const float *pitchoffset, const float *pitchmin, const float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *static_cast<part *>(MDL::loading->parts.back());

        if(name[0])
        {
            std::optional<int> i = mdl.meshes ? static_cast<meshgroup *>(mdl.meshes)->skel->findbone(name) : std::nullopt;
            if(i)
            {
                boneinfo &b = static_cast<meshgroup *>(mdl.meshes)->skel->bones[*i];
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

    static void setpitchtarget(const char *name, const char *animfile, const int *frameoffset,
                               const float *pitchmin, const float *pitchmax)
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
        DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir.c_str(), animfile);
        const animspec *sa = static_cast<meshgroup *>(mdl.meshes)->loadanim(path(filename));
        if(!sa)
        {
            conoutf("could not load %s anim file %s", MDL::formatname(), filename);
            return;
        }
        skeleton *skel = static_cast<meshgroup *>(mdl.meshes)->skel;
        std::optional<int> bone = skel ? skel->findbone(name) : std::nullopt;
        if(!bone)
        {
            conoutf("could not find bone %s to pitch target", name);
            return;
        }
        for(const pitchtarget &i : skel->pitchtargets)
        {
            if(i.bone == *bone)
            {
                return;
            }
        }
        pitchtarget t;
        t.bone = *bone;
        t.frame = sa->frame + std::clamp(*frameoffset, 0, sa->range-1);
        t.pitchmin = *pitchmin;
        t.pitchmax = *pitchmax;
        skel->pitchtargets.push_back(t);
    }

    static void setpitchcorrect(const char *name, const char *targetname,
                                const float *scale, const float *pitchmin, const float *pitchmax)
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
        std::optional<int> bone = skel ? skel->findbone(name) : std::nullopt;
        if(!bone)
        {
            conoutf("could not find bone %s to pitch correct", name);
            return;
        }
        if(skel->findpitchcorrect(*bone) >= 0)
        {
            return;
        }
        std::optional<int> targetbone = skel->findbone(targetname),
                           target = std::nullopt;
        if(targetbone)
        {
            for(uint i = 0; i < skel->pitchtargets.size(); i++)
            {
                if(skel->pitchtargets[i].bone == *targetbone)
                {
                    target = i;
                    break;
                }
            }
        }
        if(!target)
        {
            conoutf("could not find pitch target %s to pitch correct %s", targetname, name);
            return;
        }
        pitchcorrect c(*bone, *target, *pitchmin, *pitchmax, *scale);
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
    static void setanim(const char *anim, const char *animfile, const float *speed,
                        const int *priority, const int *startoffset, const int *endoffset)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        std::vector<size_t> anims = findanims(anim);
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
            DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir.c_str(), animfile);
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

    static void setanimpart(const char *maskstr)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part *p = static_cast<part *>(MDL::loading->parts.back());

        std::vector<char *> bonestrs;
        explodelist(maskstr, bonestrs);
        std::vector<uint> bonemask;
        for(uint i = 0; i < bonestrs.size(); i++)
        {
            char *bonestr = bonestrs[i];
            std::optional<int> bone = p->meshes ? static_cast<meshgroup *>(p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr+1 : bonestr) : std::nullopt;
            if(!bone)
            {
                conoutf("could not find bone %s for anim part mask [%s]", bonestr, maskstr);
                for(char* j : bonestrs)
                {
                    delete[] j;
                }
                return;
            }
            bonemask.push_back(*bone | (bonestr[0]=='!' ? Bonemask_Not : 0));
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
        if(!p->addanimpart(bonemask))
        {
            conoutf("too many animation parts");
        }
    }

    static void setadjust(const char *name, const float *yaw, const float *pitch, const float *roll,
                                            const float *tx,  const float *ty,    const float *tz)
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
        std::optional<int> i = mdl.meshes ? static_cast<meshgroup *>(mdl.meshes)->skel->findbone(name) : std::nullopt;
        if(!i)
        {
            conoutf("could not find bone %s to adjust", name);
            return;
        }
        while(!(static_cast<int>(MDL::adjustments.size()) > *i))
        {
            MDL::adjustments.push_back(skeladjustment(0, 0, 0, vec(0, 0, 0)));
        }
        MDL::adjustments[*i] = skeladjustment(*yaw, *pitch, *roll, vec(*tx/4, *ty/4, *tz/4));
    }

    static void sethitzone(const int *id, const char *maskstr)
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
        if(!m)
        {
            return;
        }
        std::vector<char *> bonestrs;
        explodelist(maskstr, bonestrs);
        std::vector<uint> bonemask;
        for(uint i = 0; i < bonestrs.size(); i++)
        {
            char *bonestr = bonestrs[i];
            std::optional<int> bone = p->meshes ? static_cast<meshgroup *>(p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr+1 : bonestr) : std::nullopt;
            if(!bone)
            {
                conoutf("could not find bone %s for hit zone mask [%s]", bonestr, maskstr);
                for(char* j : bonestrs)
                {
                    delete[] j;
                }
                return;
            }
            bonemask.push_back(*bone | (bonestr[0]=='!' ? Bonemask_Not : 0));
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
        m->skel->applybonemask(bonemask, MDL::hitzones, *id < 0 ? 0xFF : *id);
    }

    // if a skeletal model is being loaded, and meets the criteria for a ragdoll,
    // returns the pointer to that ragdollskel (new one made if necessary), returns nullptr otherwise
    static ragdollskel *checkragdoll()
    {
        if(!MDL::loading)
        {
            conoutf(Console_Error, "not loading a model");
            return nullptr;
        }
        if(!MDL::loading->skeletal())
        {
            conoutf(Console_Error, "not loading a skeletal model");
            return nullptr;
        }
        skelmodel *m = static_cast<skelmodel *>(MDL::loading);
        if(m->parts.empty())
        {
            return nullptr;
        }
        skelmodel::skelmeshgroup *meshes = static_cast<skelmodel::skelmeshgroup *>(m->parts.back()->meshes);
        if(!meshes)
        {
            return nullptr;
        }
        skelmodel::skeleton *skel = meshes->skel;
        if(!skel->ragdoll)
        {
            skel->ragdoll = new ragdollskel;
        }
        ragdollskel *ragdoll = skel->ragdoll;
        if(ragdoll->loaded)
        {
            return nullptr;
        }
        return ragdoll;
    }

    static void rdvert(float *x, float *y, float *z, float *radius)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->verts.push_back({vec(*x, *y, *z), *radius > 0 ? *radius : 1});
    }

    /* ragdoll eye level: sets the ragdoll's eye point to the level passed
     * implicitly modifies the ragdoll selected by CHECK_RAGDOLL
     */
    static void rdeye(int *v)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->eye = *v;
    }

    static void rdtri(int *v1, int *v2, int *v3)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->tris.push_back({*v1, *v2, *v3});
    }

    static void rdjoint(int *n, int *t, int *v1, int *v2, int *v3)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        const skelmodel *m = static_cast<skelmodel *>(MDL::loading);
        const skelmodel::skelmeshgroup *meshes = static_cast<const skelmodel::skelmeshgroup *>(m->parts.back()->meshes);
        const skelmodel::skeleton *skel = meshes->skel;
        if(*n < 0 || *n >= skel->numbones)
        {
            return;
        }
        ragdollskel::joint j;
        j.bone = *n;
        j.tri = *t;
        j.vert[0] = *v1;
        j.vert[1] = *v2;
        j.vert[2] = *v3;
        ragdoll->joints.push_back(j);
    }

    static void rdlimitdist(int *v1, int *v2, float *mindist, float *maxdist)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->distlimits.push_back({*v1, *v2, *mindist, std::max(*maxdist, *mindist)});
    }

    static void rdlimitrot(int *t1, int *t2, float *maxangle, float *qx, float *qy, float *qz, float *qw)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        float rmaxangle = *maxangle / RAD;
        ragdoll->rotlimits.push_back({*t1,
                                      *t2,
                                      rmaxangle,
                                      1 + 2*std::cos(rmaxangle),
                                      matrix3(quat(*qx, *qy, *qz, *qw))});
    }

    static void rdanimjoints(int *on)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->animjoints = *on!=0;
    }

    skelcommands()
    {
        if(MDL::multiparted())
        {
            this->modelcommand(loadpart, "load", "ssf"); //<fmt>load [mesh] [skel] [smooth]
        }
        this->modelcommand(settag, "tag", "ssffffff"); //<fmt>tag [name] [tag] [tx] [ty] [tz] [rx] [ry] [rz]
        this->modelcommand(setpitch, "pitchbone", "sffff"); //<fmt>pitchbone [name] [target] [scale] [min] [max]
        this->modelcommand(setpitchtarget, "pitchtarget", "ssiff"); //<fmt>pitchtarget [name] [anim] [offset] [min] [max]
        this->modelcommand(setpitchcorrect, "pitchcorrect", "ssfff"); //<fmt>pitchcorrect [name] [target] [scale] [min] [max]
        this->modelcommand(sethitzone, "hitzone", "is"); //<fmt>hitzone [id] [mask]
        if(MDL::cananimate())
        {
            this->modelcommand(setanim, "anim", "ssfiii"); //<fmt>anim [anim] [animfile] [speed] [priority] [startoffset] [endoffset]
            this->modelcommand(setanimpart, "animpart", "s"); //<fmt>animpart [maskstr]
            this->modelcommand(setadjust, "adjust", "sffffff"); //<fmt>adjust [name] [yaw] [pitch] [tx] [ty] [tz]
        }

        this->modelcommand(rdvert, "rdvert", "ffff");
        this->modelcommand(rdeye, "rdeye", "i");
        this->modelcommand(rdtri, "rdtri", "iii");
        this->modelcommand(rdjoint, "rdjoint", "iibbb");
        this->modelcommand(rdlimitdist, "rdlimitdist", "iiff");
        this->modelcommand(rdlimitrot, "rdlimitrot", "iifffff");
        this->modelcommand(rdanimjoints, "rdanimjoints", "i");
    }
};

#endif

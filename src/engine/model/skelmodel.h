#ifndef SKELMODEL_H_
#define SKELMODEL_H_

extern int gpuskel, maxskelanimdata;

enum
{
    BONEMASK_NOT  = 0x8000,
    BONEMASK_END  = 0xFFFF,
    BONEMASK_BONE = 0x7FFF
};

struct skelhitdata;

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
        GenericVec4<half> pos;
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

    struct blendcombo
    {
        int uses, interpindex;
        float weights[4];
        uchar bones[4], interpbones[4];

        blendcombo() : uses(1)
        {
        }

        bool operator==(const blendcombo &c) const
        {
            for(int k = 0; k < 4; ++k)
            {
                if(bones[k] != c.bones[k])
                {
                    return false;
                }
            }
            for(int k = 0; k < 4; ++k)
            {
                if(weights[k] != c.weights[k])
                {
                    return false;
                }
            }
            return true;
        }

        int size() const;
        static bool sortcmp(const blendcombo &x, const blendcombo &y);
        int addweight(int sorted, float weight, int bone);
        void finalize(int sorted);

        template<class T>
        void serialize(T &v)
        {
            if(interpindex >= 0)
            {
                v.weights[0] = 255;
                for(int k = 0; k < 3; ++k)
                {
                    v.weights[k+1] = 0;
                }
                v.bones[0] = 2*interpindex;
                for(int k = 0; k < 3; ++k)
                {
                    v.bones[k+1] = v.bones[0];
                }
            }
            else
            {
                int total = 0;
                for(int k = 0; k < 4; ++k)
                {
                    total += (v.weights[k] = static_cast<uchar>(0.5f + weights[k]*255));
                }
                while(total > 255)
                {
                    for(int k = 0; k < 4; ++k)
                    {
                        if(v.weights[k] > 0 && total > 255)
                        {
                            v.weights[k]--;
                            total--;
                        }
                    }
                }
                while(total < 255)
                {
                    for(int k = 0; k < 4; ++k)
                    {
                        if(v.weights[k] < 255 && total < 255)
                        {
                            v.weights[k]++;
                            total++;
                        }
                    }
                }
                for(int k = 0; k < 4; ++k)
                {
                    v.bones[k] = 2*interpbones[k];
                }
            }
        }
    };


    struct animcacheentry
    {
        animstate as[maxanimparts];
        float pitch;
        int millis;
        uchar *partmask;
        ragdolldata *ragdoll;

        animcacheentry() : ragdoll(NULL)
        {
            for(int k = 0; k < maxanimparts; ++k)
            {
                as[k].cur.fr1 = as[k].prev.fr1 = -1;
            }
        }

        bool operator==(const animcacheentry &c) const
        {
            for(int i = 0; i < maxanimparts; ++i)
            {
                if(as[i]!=c.as[i])
                {
                    return false;
                }
            }
            return pitch==c.pitch && partmask==c.partmask && ragdoll==c.ragdoll && (!ragdoll || min(millis, c.millis) >= ragdoll->lastmove);
        }

        bool operator!=(const animcacheentry &c) const
        {
            return !operator==(c);
        }
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

        skelcacheentry() : bdata(NULL), version(-1) {}

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

    struct skelmesh : mesh
    {
        vert *verts;
        tri *tris;
        int numverts, numtris, maxweights;

        int voffset, eoffset, elen;
        ushort minvert, maxvert;

        skelmesh() : verts(NULL), tris(NULL), numverts(0), numtris(0), maxweights(0)
        {
        }

        virtual ~skelmesh()
        {
            DELETEA(verts);
            DELETEA(tris);
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
        int genvbo(vector<ushort> &idxs, int offset, vector<T> &vverts)
        {
            voffset = offset;
            eoffset = idxs.length();
            for(int i = 0; i < numverts; ++i)
            {
                vert &v = verts[i];
                assignvert(vverts.add(), i, v, ((skelmeshgroup *)group)->blendcombos[v.blend]);
            }
            for(int i = 0; i < numtris; ++i)
            {
                for(int j = 0; j < 3; ++j)
                {
                    idxs.add(voffset + tris[i].vert[j]);
                }
            }
            elen = idxs.length()-eoffset;
            minvert = voffset;
            maxvert = voffset + numverts-1;
            return numverts;
        }

        template<class T>
        int genvbo(vector<ushort> &idxs, int offset, vector<T> &vverts, int *htdata, int htlen)
        {
            voffset = offset;
            eoffset = idxs.length();
            minvert = 0xFFFF;
            for(int i = 0; i < numtris; ++i)
            {
                tri &t = tris[i];
                for(int j = 0; j < 3; ++j)
                {
                    int index = t.vert[j];
                    vert &v = verts[index];
                    T vv;
                    assignvert(vv, index, v, ((skelmeshgroup *)group)->blendcombos[v.blend]);
                    int htidx = hthash(v.pos)&(htlen-1);
                    for(int k = 0; k < htlen; ++k)
                    {
                        int &vidx = htdata[(htidx+k)&(htlen-1)];
                        if(vidx < 0)
                        {
                            vidx = idxs.add(static_cast<ushort>(vverts.length()));
                            vverts.add(vv);
                            break;
                        }
                        else if(!memcmp(&vverts[vidx], &vv, sizeof(vv)))
                        {
                            minvert = min(minvert, idxs.add(static_cast<ushort>(vidx)));
                            break;
                        }
                    }
                }
            }
            elen = idxs.length()-eoffset;
            minvert = min(minvert, static_cast<ushort>(voffset));
            maxvert = max(minvert, static_cast<ushort>(vverts.length()-1));
            return vverts.length()-voffset;
        }

        int genvbo(vector<ushort> &idxs, int offset);

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
            const int blendoffset = ((skelmeshgroup *)group)->skel->numgpubones;
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
        void render(const animstate *as, skin &s, vbocacheentry &vc);
    };

    struct tag
    {
        char *name;
        int bone;
        matrix4x3 matrix;

        tag() : name(NULL) {}
        ~tag() { DELETEA(name); }
    };

    struct skelanimspec
    {
        char *name;
        int frame, range;

        skelanimspec() : name(NULL), frame(0), range(0) {}
        ~skelanimspec()
        {
            DELETEA(name);
        }
    };

    struct boneinfo
    {
        const char *name;
        int parent, children, next, group, scheduled, interpindex, interpparent, ragdollindex, correctindex;
        float pitchscale, pitchoffset, pitchmin, pitchmax;
        dualquat base, invbase;

        boneinfo() : name(NULL), parent(-1), children(-1), next(-1), group(INT_MAX), scheduled(-1), interpindex(-1), interpparent(-1), ragdollindex(-1), correctindex(-1), pitchscale(0), pitchoffset(0), pitchmin(0), pitchmax(0) {}
        ~boneinfo()
        {
            DELETEA(name);
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
        vector<skelmeshgroup *> users;
        boneinfo *bones;
        int numbones, numinterpbones, numgpubones, numframes;
        dualquat *framebones;
        vector<skelanimspec> skelanims;
        vector<tag> tags;
        vector<antipode> antipodes;
        ragdollskel *ragdoll;
        vector<pitchdep> pitchdeps;
        vector<pitchtarget> pitchtargets;
        vector<pitchcorrect> pitchcorrects;

        bool usegpuskel;
        vector<skelcacheentry> skelcache;
        hashtable<GLuint, int> blendoffsets;

        skeleton() : name(NULL), shared(0), bones(NULL), numbones(0), numinterpbones(0), numgpubones(0), numframes(0), framebones(NULL), ragdoll(NULL), usegpuskel(false), blendoffsets(32)
        {
        }

        ~skeleton()
        {
            DELETEA(name);
            DELETEA(bones);
            DELETEA(framebones);
            DELETEP(ragdoll);
            for(int i = 0; i < skelcache.length(); i++)
            {
                DELETEA(skelcache[i].bdata);
            }
        }

        skelanimspec *findskelanim(const char *name, char sep = '\0');
        skelanimspec &addskelanim(const char *name);
        int findbone(const char *name);
        int findtag(const char *name);
        bool addtag(const char *name, int bone, const matrix4x3 &matrix);
        void calcantipodes();
        void remapbones();
        void addpitchdep(int bone, int frame);
        int findpitchdep(int bone);
        int findpitchcorrect(int bone);
        void initpitchdeps();
        void optimize();
        void expandbonemask(uchar *expansion, int bone, int val);
        void applybonemask(ushort *mask, uchar *partmask, int partindex);
        void linkchildren();
        int availgpubones() const;
        bool gpuaccelerate() const;
        float calcdeviation(const vec &axis, const vec &forward, const dualquat &pose1, const dualquat &pose2);
        void calcpitchcorrects(float pitch, const vec &axis, const vec &forward);
        void interpbones(const animstate *as, float pitch, const vec &axis, const vec &forward, int numanimparts, const uchar *partmask, skelcacheentry &sc);
        void initragdoll(ragdolldata &d, skelcacheentry &sc, part *p);
        void genragdollbones(ragdolldata &d, skelcacheentry &sc, part *p);
        void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n);
        void calctags(part *p, skelcacheentry *sc = NULL);
        void cleanup(bool full = true);
        bool canpreload();
        void preload();
        skelcacheentry &checkskelcache(part *p, const animstate *as, float pitch, const vec &axis, const vec &forward, ragdolldata *rdata);
        int getblendoffset(UniformLoc &u);
        void setglslbones(UniformLoc &u, skelcacheentry &sc, skelcacheentry &bc, int count);
        void setgpubones(skelcacheentry &sc, blendcacheentry *bc, int count);
        bool shouldcleanup() const;
    };

    static hashnameset<skeleton *> skeletons;

    struct skelmeshgroup : meshgroup
    {
        skeleton *skel;

        vector<blendcombo> blendcombos;
        int numblends[4];

        static constexpr int maxblendcache = 16;
        static constexpr int maxvbocache = 16;

        blendcacheentry blendcache[maxblendcache];

        vbocacheentry vbocache[maxvbocache];

        ushort *edata;
        GLuint ebuf;
        int vlen, vertsize, vblends, vweights;
        uchar *vdata;

        skelhitdata *hitdata;

        skelmeshgroup() : skel(NULL), edata(NULL), ebuf(0), vlen(0), vertsize(0), vblends(0), vweights(0), vdata(NULL), hitdata(NULL)
        {
            memset(numblends, 0, sizeof(numblends));
        }

        virtual ~skelmeshgroup()
        {
            if(skel)
            {
                if(skel->shared)
                {
                    skel->users.removeobj(this);
                }
                else
                {
                    DELETEP(skel);
                }
            }
            if(ebuf)
            {
                glDeleteBuffers_(1, &ebuf);
            }
            for(int i = 0; i < maxblendcache; ++i)
            {
                DELETEA(blendcache[i].bdata);
            }
            for(int i = 0; i < maxvbocache; ++i)
            {
                if(vbocache[i].vbuf)
                {
                    glDeleteBuffers_(1, &vbocache[i].vbuf);
                }
            }
            DELETEA(vdata);
            deletehitdata();
        }

        void shareskeleton(const char *name);
        int findtag(const char *name);
        void *animkey();
        int totalframes() const;

        virtual skelanimspec *loadanim(const char *filename)
        {
            return NULL;
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
        void bindvbo(const animstate *as, part *p, vbocacheentry &vc)
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

        void bindvbo(const animstate *as, part *p, vbocacheentry &vc, skelcacheentry *sc = NULL, blendcacheentry *bc = NULL);
        void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n);
        int addblendcombo(const blendcombo &c);
        void sortblendcombos();
        int remapblend(int blend);
        static void blendbones(dualquat &d, const dualquat *bdata, const blendcombo &c);
        void blendbones(const skelcacheentry &sc, blendcacheentry &bc);
        static void blendbones(const dualquat *bdata, dualquat *dst, const blendcombo *c, int numblends);
        void cleanup();
        vbocacheentry &checkvbocache(skelcacheentry &sc, int owner);
        blendcacheentry &checkblendcache(skelcacheentry &sc, int owner);
        //hitzone
        void cleanuphitdata();
        void deletehitdata();
        void buildhitdata(const uchar *hitzones);
        void intersect(skelhitdata *z, part *p, const skelmodel::skelcacheentry &sc, const vec &o, const vec &ray);
        //end hitzone.h
        void intersect(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p, const vec &o, const vec &ray);
        void preload(part *p);

        void render(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p);

        virtual bool load(const char *name, float smooth) = 0;
    };

    virtual skelmeshgroup *newmeshes() = 0;

    meshgroup *loadmeshes(const char *name, const char *skelname = NULL, float smooth = 2)
    {
        skelmeshgroup *group = newmeshes();
        group->shareskeleton(skelname);
        if(!group->load(name, smooth))
        {
            delete group;
            return NULL;
        }
        return group;
    }
    meshgroup *sharemeshes(const char *name, const char *skelname = NULL, float smooth = 2)
    {
        if(!meshgroups.access(name))
        {
            meshgroup *group = loadmeshes(name, skelname, smooth);
            if(!group)
            {
                return NULL;
            }
            meshgroups.add(group);
        }
        return meshgroups[name];
    }

    struct animpartmask
    {
        animpartmask *next;
        int numbones;
        uchar bones[1];
    };

    struct skelpart : part
    {
        animpartmask *buildingpartmask;

        uchar *partmask;

        skelpart(animmodel *model, int index = 0) : part(model, index), buildingpartmask(NULL), partmask(NULL)
        {
        }

        virtual ~skelpart()
        {
            DELETEA(buildingpartmask);
        }

        uchar *sharepartmask(animpartmask *o);
        animpartmask *newpartmask();
        void initanimparts();
        bool addanimpart(ushort *bonemask);
        void endanimparts();
        void loaded();
    };

    skelmodel(const char *name) : animmodel(name)
    {
    }

    int linktype(animmodel *m, part *p) const
    {
        return type()==m->type() &&
            ((skelmeshgroup *)parts[0]->meshes)->skel == ((skelmeshgroup *)p->meshes)->skel ?
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
        skelpart *p = new skelpart(this, parts.length());
        parts.add(p);
        return *p;
    }
};

struct skeladjustment
{
    float yaw, pitch, roll;
    vec translate;

    skeladjustment(float yaw, float pitch, float roll, const vec &translate) : yaw(yaw), pitch(pitch), roll(roll), translate(translate) {}

    void adjust(dualquat &dq)
    {
        if(yaw)
        {
            dq.mulorient(quat(vec(0, 0, 1), yaw*RAD));
        }
        if(pitch)
        {
            dq.mulorient(quat(vec(0, -1, 0), pitch*RAD));
        }
        if(roll)
        {
            dq.mulorient(quat(vec(-1, 0, 0), roll*RAD));
        }
        if(!translate.iszero())
        {
            dq.translate(translate);
        }
    }
};

template<class MDL>
struct skelloader : modelloader<MDL, skelmodel>
{
    static vector<skeladjustment> adjustments;
    static vector<uchar> hitzones;

    skelloader(const char *name) : modelloader<MDL, skelmodel>(name) {}

    void flushpart()
    {
        if(hitzones.length() && skelmodel::parts.length())
        {
            skelmodel::skelpart *p = (skelmodel::skelpart *)skelmodel::parts.last();
            skelmodel::skelmeshgroup *m = (skelmodel::skelmeshgroup *)p->meshes;
            if(m)
            {
                m->buildhitdata(hitzones.getbuf());
            }
        }

        adjustments.setsize(0);
        hitzones.setsize(0);
    }
};

template<class MDL>
vector<skeladjustment> skelloader<MDL>::adjustments;

template<class MDL>
vector<uchar> skelloader<MDL>::hitzones;

template<class MDL>
struct skelcommands : modelcommands<MDL, struct MDL::skelmesh>
{
    typedef modelcommands<MDL, struct MDL::skelmesh> commands;
    typedef struct MDL::skeleton skeleton;
    typedef struct MDL::skelmeshgroup meshgroup;
    typedef struct MDL::skelpart part;
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
        mdl.meshes = MDL::loading->sharemeshes(path(filename), skelname[0] ? skelname : NULL, *smooth > 0 ? cosf(std::clamp(*smooth, 0.0f, 180.0f)*RAD) : 2);
        if(!mdl.meshes)
        {
            conoutf("could not load %s", filename);
        }
        else
        {
            if(mdl.meshes && ((meshgroup *)mdl.meshes)->skel->numbones > 0)
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
        part &mdl = *(part *)MDL::loading->parts.last();
        int i = mdl.meshes ? ((meshgroup *)mdl.meshes)->skel->findbone(name) : -1;
        if(i >= 0)
        {
            float cx = *rx ? cosf(*rx/2*RAD) : 1, sx = *rx ? sinf(*rx/2*RAD) : 0,
                  cy = *ry ? cosf(*ry/2*RAD) : 1, sy = *ry ? sinf(*ry/2*RAD) : 0,
                  cz = *rz ? cosf(*rz/2*RAD) : 1, sz = *rz ? sinf(*rz/2*RAD) : 0;
            matrix4x3 m(matrix3(quat(sx*cy*cz - cx*sy*sz, cx*sy*cz + sx*cy*sz, cx*cy*sz - sx*sy*cz, cx*cy*cz + sx*sy*sz)),
                        vec(*tx, *ty, *tz));
            ((meshgroup *)mdl.meshes)->skel->addtag(tagname, i, m);
            return;
        }
        conoutf("could not find bone %s for tag %s", name, tagname);
    }

    static void setpitch(char *name, float *pitchscale, float *pitchoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *(part *)MDL::loading->parts.last();

        if(name[0])
        {
            int i = mdl.meshes ? ((meshgroup *)mdl.meshes)->skel->findbone(name) : -1;
            if(i>=0)
            {
                boneinfo &b = ((meshgroup *)mdl.meshes)->skel->bones[i];
                b.pitchscale = *pitchscale;
                b.pitchoffset = *pitchoffset;
                if(*pitchmin || *pitchmax)
                {
                    b.pitchmin = *pitchmin;
                    b.pitchmax = *pitchmax;
                }
                else
                {
                    b.pitchmin = -360*fabs(b.pitchscale) + b.pitchoffset;
                    b.pitchmax = 360*fabs(b.pitchscale) + b.pitchoffset;
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
            mdl.pitchmin = -360*fabs(mdl.pitchscale) + mdl.pitchoffset;
            mdl.pitchmax = 360*fabs(mdl.pitchscale) + mdl.pitchoffset;
        }
    }

    static void setpitchtarget(char *name, char *animfile, int *frameoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("\frnot loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *(part *)MDL::loading->parts.last();
        if(!mdl.meshes)
        {
            return;
        }
        DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir, animfile);
        animspec *sa = ((meshgroup *)mdl.meshes)->loadanim(path(filename));
        if(!sa)
        {
            conoutf("\frcould not load %s anim file %s", MDL::formatname(), filename);
            return;
        }
        skeleton *skel = ((meshgroup *)mdl.meshes)->skel;
        int bone = skel ? skel->findbone(name) : -1;
        if(bone < 0)
        {
            conoutf("\frcould not find bone %s to pitch target", name);
            return;
        }
        for(int i = 0; i < skel->pitchtargets.length(); i++)
        {
            if(skel->pitchtargets[i].bone == bone)
            {
                return;
            }
        }
        pitchtarget &t = skel->pitchtargets.add();
        t.bone = bone;
        t.frame = sa->frame + std::clamp(*frameoffset, 0, sa->range-1);
        t.pitchmin = *pitchmin;
        t.pitchmax = *pitchmax;
    }

    static void setpitchcorrect(char *name, char *targetname, float *scale, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("\frnot loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *(part *)MDL::loading->parts.last();
        if(!mdl.meshes)
        {
            return;
        }
        skeleton *skel = ((meshgroup *)mdl.meshes)->skel;
        int bone = skel ? skel->findbone(name) : -1;
        if(bone < 0)
        {
            conoutf("\frcould not find bone %s to pitch correct", name);
            return;
        }
        if(skel->findpitchcorrect(bone) >= 0)
        {
            return;
        }
        int targetbone = skel->findbone(targetname), target = -1;
        if(targetbone >= 0)
        {
            for(int i = 0; i < skel->pitchtargets.length(); i++)
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
            conoutf("\frcould not find pitch target %s to pitch correct %s", targetname, name);
            return;
        }
        pitchcorrect c;
        c.bone = bone;
        c.target = target;
        c.pitchmin = *pitchmin;
        c.pitchmax = *pitchmax;
        c.pitchscale = *scale;
        int pos = skel->pitchcorrects.length();
        for(int i = 0; i < skel->pitchcorrects.length(); i++)
        {
            if(bone <= skel->pitchcorrects[i].bone)
            {
                pos = i;
                break;
            }
        }
        skel->pitchcorrects.insert(pos, c);
    }

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
            part *p = (part *)MDL::loading->parts.last();
            if(!p->meshes)
            {
                return;
            }
            DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir, animfile);
            animspec *sa = ((meshgroup *)p->meshes)->loadanim(path(filename));
            if(!sa)
            {
                conoutf("could not load %s anim file %s", MDL::formatname(), filename);
            }
            else
            {
                for(int i = 0; i < static_cast<int>(anims.size()); i++)
                {
                    int start = sa->frame, end = sa->range;
                    if(*startoffset > 0)
                    {
                        start += min(*startoffset, end-1);
                    }
                    else if(*startoffset < 0)
                    {
                        start += max(end + *startoffset, 0);
                    }
                    end -= start - sa->frame;
                    if(*endoffset > 0)
                    {
                        end = min(end, *endoffset);
                    }
                    else if(*endoffset < 0)
                    {
                        end = max(end + *endoffset, 1);
                    }
                    MDL::loading->parts.last()->setanim(p->numanimparts-1, anims[i], start, end, *speed, *priority);
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
        part *p = (part *)MDL::loading->parts.last();

        vector<char *> bonestrs;
        explodelist(maskstr, bonestrs);
        vector<ushort> bonemask;
        for(int i = 0; i < bonestrs.length(); i++)
        {
            char *bonestr = bonestrs[i];
            int bone = p->meshes ? ((meshgroup *)p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr+1 : bonestr) : -1;
            if(bone<0)
            {
                conoutf("could not find bone %s for anim part mask [%s]", bonestr, maskstr); bonestrs.deletearrays();
                return;
            }
            bonemask.add(bone | (bonestr[0]=='!' ? BONEMASK_NOT : 0));
        }
        bonestrs.deletearrays();
        bonemask.sort();
        if(bonemask.length())
        {
            bonemask.add(BONEMASK_END);
        }
        if(!p->addanimpart(bonemask.getbuf()))
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
        part &mdl = *(part *)MDL::loading->parts.last();
        if(!name[0])
        {
            return;
        }
        int i = mdl.meshes ? ((meshgroup *)mdl.meshes)->skel->findbone(name) : -1;
        if(i < 0)
        {
            conoutf("could not find bone %s to adjust", name);
            return;
        }
        while(!MDL::adjustments.inrange(i))
        {
            MDL::adjustments.add(skeladjustment(0, 0, 0, vec(0, 0, 0)));
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
        part *p = (part *)MDL::loading->parts.last();
        meshgroup *m = (meshgroup *)p->meshes;
        if(!m || m->hitdata)
        {
            return;
        }
        vector<char *> bonestrs;
        explodelist(maskstr, bonestrs);
        vector<ushort> bonemask;
        for(int i = 0; i < bonestrs.length(); i++)
        {
            char *bonestr = bonestrs[i];
            int bone = p->meshes ? ((meshgroup *)p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr+1 : bonestr) : -1;
            if(bone<0)
            {
                conoutf("could not find bone %s for hit zone mask [%s]", bonestr, maskstr);
                bonestrs.deletearrays();
                return;
            }
            bonemask.add(bone | (bonestr[0]=='!' ? BONEMASK_NOT : 0));
        }
        bonestrs.deletearrays();
        if(bonemask.empty())
        {
            return;
        }
        bonemask.sort();
        bonemask.add(BONEMASK_END);

        while(MDL::hitzones.length() < m->skel->numbones)
        {
            MDL::hitzones.add(0xFF);
        }
        m->skel->applybonemask(bonemask.getbuf(), MDL::hitzones.getbuf(), *id < 0 ? 0xFF : *id);
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

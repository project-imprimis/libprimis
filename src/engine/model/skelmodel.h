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
 *
 * arrows indicate pointer fields pointing towards an object of the pointed type
 * arrows point to base or derived type specifically (which share a box)
 *
 *              /-------------------------------\
 *              | skelmodel : animmodel : model |
 *              \--------------|---Λ------------/
 *                             |   \_______________
 *                             |                   \   ____________
 *                             |                   |  /            \
 *                             |      /------------V--V-\     /----|-------\
 *                             |      | skelpart : part-------->linkedpart |
 *                             |      \-------------|-Λ-/     \------------/
 *                              \      _____________/ \___
 *                               |    /                   \
 *            /------------------V----V---\           /---V----\
 *            | skelmeshgroup : meshgroup |           |  skin  |
 *            \-|------|--|-Λ----------Λ--/           \--|---|-/
 *              |      |  | |          |                 |   |
 *  /-----------V-\    |  | |          |                 |   |
 *  |vbocacheentry|    |  | |          |                 |   |
 *  \-|-----------/    |  | |          |                 |   |
 *    |               /   | |          |                 |   |
 *    |               |   | |          |                 |   |
 *    | /-------------V-\ | |          |                 |   \_____
 *    | |blendcacheentry| | |          \_____________    |         \
 *    | \--|------------/ |  \                       \ /-V----\     |
 *    |    |             /    \                      | |shader|     |
 *    |    |            /      \                     | \------/ /---V---\
 *    |    |            |       |                    |          |texture|
 *    |    |   /--------V-\  /--V-----\ /------------V----\     \-------/
 *    |    |   |blendcombo|  |skeleton| | skelmesh : Mesh |
 *    |    |   \----------/  \-|---|--/ \--|----|---------/
 *    \____\_________          |   |       |    |
 *                   \         |   |       |    |
 * /----------------\ | /------V-\ |    /--V-\  |
 * |dynent : physent| | |boneinfo| |    |vert|  |
 * \---|------------/ | \--------/ |    \----/  |
 *     \____________  |           /             |
 *                  \ |          /            /-V-\
 *               /--V-V------\  |             |tri|
 *               |ragdolldata|  |             \---/
 *               \-|-|-----|-/  |
 *                 | |     |    |
 *            ____/  |  /--V----V---\
 *           /       |  |ragdollskel|
 *           |       |  \-----------/
 *         /-V-\  /--V-\
 *         |tri|  |vert|
 *         \---/  \----/
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

    /**
     * @brief An object representing a set of weights for a vertex.
     *
     * A blendcombo object stores a set of weights, which when finalized should total
     * to a total weight quantity of 1. The weights are stored in descending order,
     * and should only assume to be normalized once finalize() is called.
     */
    class blendcombo final
    {
        public:
            int uses, interpindex;

            struct BoneData
            {
                float weight;
                uchar bone;
                uchar interpbone;
            };
            std::array<BoneData, 4> bonedata;

            blendcombo();

            bool operator==(const blendcombo &c) const;

            /**
             * @brief Returns the number of assigned weights in the blendcomb object.
             *
             * Assumes that all weights assigned are in the order created by addweight().
             *
             * @return the number of weights assigned in the bonedata
             */
            size_t size() const;
            static bool sortcmp(const blendcombo &x, const blendcombo &y);

            /**
             * @brief Attempts to assign a weight to one of the bonedata slots.
             *
             * Attempts to add the passed weight/bone combination to this blendcombo.
             * If the weight passed is less than 1e-3, the weight will not be added
             * regardless of the status of the object.
             *
             * If a weight/bone combo with a weight larger than any of the existing
             * members of the object are stored, the smaller weights are shifted
             * (and the smallest one removed) to make space for it. The stored
             * blends are assumed to be stored in descending order, and if added,
             * the inserted object will also be inserted to preserve descending
             * order.
             *
             * The inserted object will only be inserted at a depth inside the
             * weights buffer as deep as `sorted`. If this occurs, the descending
             * order of the buffer may not be maintained, and future operations
             * depending on this may not function properly.
             *
             * The returned value sorted indicates the depth into the object at
             * which the object should attempt to add an element. If an element
             * is successfully added, and if the bone data is not filled, then
             * sorted is returned incremented by one. Otherwise, the same value
             * passed as sorted will be returned.
             *
             * @param sorted the depth to add a weight in this object
             * @param weight the weight to attempt to add
             * @param bone the corresponding bone index
             *
             * @return the resulting number of allocated weights
             */
            int addweight(int sorted, float weight, int bone);

            /**
             * @brief Normalizes elements in the bonedata array.
             * Normalizes the elements in the weights part of the bonedata array
             * (up to `output` number of bones to normalize).
             *
             * The normalization critera for the weight data is the condition where
             * the sum of all the weights (up to the number sorted) adds to 1.
             *
             * @param sorted number of elements to normalize (must be <= 4)
             */
            void finalize(int sorted);

            /**
             * @brief Assigns unsigned character values to a vvertgw using the data in the blendcombo object
             *
             * If interpindex >=0:
             *  Sets the zeroth weight to 255 (1.f) and the others to zero
             *  Sets all of the bone values to 2*interpindex
             *  Note that none of the blendcombo's saved weights/bones values impact this operation
             *
             * If interpindex <0:
             *  Sets the passed vvertgw's weight values using floating point values ranging from 0..1
             *  converted to an unsigned character value ranging from 0..255
             *
             *  While the sum of the weights is greater than 1 (255), for each nonzero weight, remove
             *  1/255 from that weight, until the sum of weights is 1
             *
             *  Otherwise, while the sum of the weights is less than 1 (255), for each weight < 1, add
             *  1/255 from that weight, until the sum of the weights is 1
             *
             *  Assigns the passed object's bones to be equal to two times this objects' respective interpbones index
             *
             * @param v the vvertgw object to set weight/bone values to
             */
            void serialize(skelmodel::vvertgw &v) const;

            /**
             * @brief Creates a dual quaternion representation from the bone data of a blendcombo.
             *
             * Accumulates the set of dual quaternions pointed to by the bonedata object, scaled
             * by their respective weights. The resulting dual quaternion should be normalized,
             * if the blendcombo was normalized (by calling finalize()).
             *
             * @param bdata an array of dualquats, to which the BoneData.interpbones field points to
             *
             * @return a dual quaternion created from the blendcombo object's bone data
             */
            dualquat blendbones(const dualquat *bdata) const;

            /**
             * @brief Returns an appropriate interpolation index for this bone.
             *
             * If there is more than one bone, returns the object's interpindex;
             * if there is only one bone, returns that bone's interpindex
             *
             * @return an appropriate interpolation index for the blendcombo
             */
            int remapblend() const;

            /**
             * @brief Sets the interpolation index of the object.
             *
             * If there is more than one bone, such that a shared interpolation index
             * for all bones is necessary, sets this object's interpolation index
             * (interpindex) to `val`. If there are not multiple bones, sets the
             * interpolation index to -1.
             *
             * @param val the value to optionally set to the object's shared interpindex
             */
            void setinterpindex(int val);

            /**
             * @brief Assigns interpbones values to the specified bonedata.
             *
             * @param val value to set to the indicated bone
             * @param i the index of the bonedata to set
             */
            void setinterpbones(int val, size_t i);

            /**
             * @brief Gets the bone stored at index.
             *
             * Gets the bone index associated with one of the bonedata's stored
             * bones. This index represents an index of the skeleton's boneinfo array.
             *
             * @param index the index of the bonedata to access
             */
            int getbone(size_t index);

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

        /**
         * @brief Constructs a skelmesh object.
         *
         * @param name name of the underlying Mesh object
         * @param verts a heap-allocated array of vertices
         * @param numverts size of verts array
         * @param tris a heap-allocated array of tris
         * @param numtris size of tris array
         */
        skelmesh(std::string_view name, vert *verts, uint numverts, tri *tris, uint numtris);

        virtual ~skelmesh();

        int addblendcombo(const blendcombo &c);

        void smoothnorms(float limit = 0, bool areaweight = true);
        void buildnorms(bool areaweight = true);
        void calctangents(bool areaweight = true);
        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) const override final;
        void genBIH(BIH::mesh &m) const override final;
        void genshadowmesh(std::vector<triangle> &out, const matrix4x3 &m) const override final;
        static void assignvert(vvertg &vv, const vert &v);
        static void assignvert(vvertgw &vv, const vert &v, const blendcombo &c);

        int genvbo(const std::vector<blendcombo> &bcs, std::vector<GLuint> &idxs, int offset, std::vector<vvertgw> &vverts);
        int genvbo(std::vector<GLuint> &idxs, int offset, std::vector<vvertg> &vverts, int *htdata, int htlen);
        int genvbo(const std::vector<blendcombo> &bcs, std::vector<GLuint> &idxs, int offset);

        static void fillvert(vvert &vv, const vert &v);
        void fillverts(vvert *vdata);
        void interpverts(int numgpubones, const dualquat * RESTRICT bdata1, const dualquat * RESTRICT bdata2, vvert * RESTRICT vdata, skin &s);
        void setshader(Shader *s, bool usegpuskel, int vweights, int row) override final;
        void render();
    };

    struct skelanimspec final
    {
        std::string name;
        int frame, range;
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
            size_t numbones;
            int numinterpbones, numgpubones, numframes;
            dualquat *framebones; //array of quats, size equal to anim frames * bones in model
            std::vector<skelanimspec> skelanims;
            ragdollskel *ragdoll; //optional ragdoll object if ragdoll is in effect
            std::vector<pitchtarget> pitchtargets;
            std::vector<pitchcorrect> pitchcorrects;

            bool usegpuskel;
            std::vector<skelcacheentry> skelcache;

            skeleton(skelmeshgroup * const group);
            ~skeleton();

            const skelanimspec *findskelanim(std::string_view name, char sep = '\0') const;
            skelanimspec &addskelanim(const std::string &name, int numframes, int animframes);
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
            /**
             * @brief Sets the pitch information for the index'th bone in the skeleton's bones
             *
             * If no bone exists at `index` returns false; otherwise returns true
             */
            bool setbonepitch(size_t index, float scale, float offset, float min, float max);

            //Returns nullopt if index out of bounds
            std::optional<dualquat> getbonebase(size_t index) const;

            bool setbonebases(const std::vector<dualquat> &bases);

            //Only sets name if no name present (nullptr)
            bool setbonename(size_t index, std::string_view name);

            //Only sets parent if within boundaries of bone array
            bool setboneparent(size_t index, size_t parent);

            //creates boneinfo array of size num, also sets numbones to that size
            void createbones(size_t num);


        private:
            skelmeshgroup * const owner;

            struct boneinfo final
            {
                const char *name;
                int parent, children, next, group, scheduled, interpindex, interpparent, ragdollindex, correctindex;
                float pitchscale, pitchoffset, pitchmin, pitchmax;
                dualquat base;

                boneinfo();
                ~boneinfo();
            };
            boneinfo *bones; //array of boneinfo, size equal to numbones

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

            std::unordered_map<GLuint, int> blendoffsets;

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

            //for vvert, vvertg and vvertgw (also for vvertgw see below function),
            //disable bones if active
            //must have same const-qualification to properly interact with bindbones() below
            template<class T>
            void bindbones(const T *)
            {
                if(enablebones)
                {
                    disablebones();
                }
            }

            /* this function is only called if `bindbones(vvertgw *)` is used to call it;
             * if you call bindbones<vvertgw>(), that will call the above template
             * (this function can be called if no <> specifier is provided, because
             * of partial ordering rules -- see C++20 N4849 13.10.2.4)
             */
            void bindbones(const vvertgw *vverts);

            template<class T>
            void bindvbo(const AnimState *as, const part *p, const vbocacheentry &vc)
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
            void bindvbo(const AnimState *as, const part *p, const vbocacheentry &vc, const skelcacheentry *sc = nullptr);
            int addblendcombo(const blendcombo &c);
            void sortblendcombos();
            void blendbones(const skelcacheentry &sc, blendcacheentry &bc);
            void cleanup() override final;
            vbocacheentry &checkvbocache(const skelcacheentry &sc, int owner);
            blendcacheentry &checkblendcache(const skelcacheentry &sc, int owner);

            virtual bool load(std::string_view meshfile, float smooth, part &p) = 0;
            virtual const skelanimspec *loadanim(const std::string &filename) = 0;
        private:
            int vweights;
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

    /* Returns the link type of an animmodel relative to a part
     *
     * If `this` model's zeroth part's mesh's skel is the same as the passed part's
     * mesh's skel, returns Link_Reuse
     * If the passed model parameter is not linkable, or does not meet the criteria above,
     * returns Link_Tag.
     *
     * m *must* point to a valid animmodel object
     * p *must* point to a valid part object which points to a valid skeleton.
     */
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
                float newpitchmin = 0.f,
                      newpitchmax = 0.f;
                if(*pitchmin || *pitchmax)
                {
                    newpitchmin = *pitchmin;
                    newpitchmax = *pitchmax;
                }
                else
                {
                    newpitchmin = -360*std::fabs(*pitchscale) + *pitchoffset;
                    newpitchmax = 360*std::fabs(*pitchscale) + *pitchoffset;
                }
                static_cast<meshgroup *>(mdl.meshes)->skel->setbonepitch(*i, *pitchscale, *pitchoffset, newpitchmin, newpitchmax);
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

        while(MDL::hitzones.size() < m->skel->numbones)
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
        if(*n < 0 || *n >= static_cast<int>(skel->numbones))
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

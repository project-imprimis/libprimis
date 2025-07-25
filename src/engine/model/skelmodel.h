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

            /**
             * @brief Compares two blendcombos' weights and bones.
             *
             * Returns true if all bone indices and weight values in the Bone Data
             * array match. Checks no other values in the BoneData or other fields in
             * the blendcombo object
             *
             * @param c the blendcombo to compare
             *
             * @return true if the bones and weights match, false otherwise
             */
            bool operator==(const blendcombo &c) const;

            /**
             * @brief Returns the number of assigned weights in the blendcomb object.
             *
             * Assumes that all weights assigned are in the order created by addweight().
             *
             * @return the number of weights assigned in the bonedata
             */
            size_t size() const;

            /**
             * @brief Returns whether the first blendcombo has more weights than the second
             *
             * Returns true if `x` has a weight set at an index which `y` does not (reading from
             * left to right). Does not compare the actual values of the weights.
             * If both blendcombos have the same number of weights, returns false
             *
             * @param x the first blendcombo to compare
             * @param y the second blendcombo to compare
             *
             * @return true if x has more weights than y set, false if equal or less weights
             */
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

        /**
         * @brief Returns whether two animcacheentries compare equal
         *
         * Checks that all AnimStates in the animcacheentry::as field compare equal, as
         * well as the pitch, partmask, and ragdoll fields.
         *
         * If there are ragdolls in both objects, checks that neither
         * object's timestamp is less than this object's lastmove timestamp
         *
         * @param c the animcacheentry to compare
         *
         * @return true if the animcacheentries compare equal
         */
        bool operator==(const animcacheentry &c) const;

        /**
         * @brief Returns the opposite of animcacheentry::operator==
         *
         * @param c the animcacheentry to compare
         *
         * @return true if the animcacheentries compare unequal
         */
        bool operator!=(const animcacheentry &c) const;
    };

    struct vbocacheentry final : animcacheentry
    {
        GLuint vbuf; //GL_ARRAY_BUFFER (gle::bindvbo)
        int owner;

        bool check() const;
        vbocacheentry();
    };

    struct skelcacheentry : animcacheentry
    {
        dualquat *bdata; //array of size numinterpbones
        int version; //caching version

        skelcacheentry();
        void nextversion();
    };

    struct blendcacheentry final : skelcacheentry
    {
        int owner;

        blendcacheentry();
    };

    class skelmeshgroup;

    class skelmesh : public Mesh
    {
        public:
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
            skelmesh(std::string_view name, vert *verts, uint numverts, tri *tris, uint numtris, meshgroup *m);

            virtual ~skelmesh();

            int addblendcombo(const blendcombo &c);

            void smoothnorms(float limit = 0, bool areaweight = true);
            void buildnorms(bool areaweight = true);
            void calctangents(bool areaweight = true);
            void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) const final;
            void genBIH(BIH::mesh &m) const final;
            void genshadowmesh(std::vector<triangle> &out, const matrix4x3 &m) const final;
            //assignvert() functions are used externally in test code
            static void assignvert(vvertg &vv, const vert &v);
            static void assignvert(vvertgw &vv, const vert &v, const blendcombo &c);

            /*
             * these two genvbo() functions are used for different cases
             * of skelmodel rendering paths:
             *
             * genvbo(const std::vector<blendcombo>&, std::vector<GLuint>&, int, std::vector<vvertgw>) is for skeleton with animation frames
             * genvbo(std::vector<GLuint>&, int, std::vector<vvertg>&, int, int) is for no animation frames
             */
            int genvbo(const std::vector<blendcombo> &bcs, std::vector<GLuint> &idxs, int offset, std::vector<vvertgw> &vverts);
            int genvbo(std::vector<GLuint> &idxs, int offset, std::vector<vvertg> &vverts, int *htdata, int htlen);

            void setshader(Shader *s, bool usegpuskel, int vweights, int row) const final;
            void render() const;
            /**
             * @brief Assigns indices from the remap parameter to the object's verts
             *
             * Assigns the vector of remap blend indices to the verts array. Assumes
             * that the vector passed is at least as large as the verts array.
             *
             * @param remap a vector of new indices to assign
             */
            void remapverts(const std::vector<int> remap);
            /**
             * @brief Returns the number of verts represented by the object.
             *
             * This function is used by the testing code.
             *
             * @return the number of vertices represented
             */
            int vertcount() const;

            /**
             * @brief Returns the number of tris represented by the object.
             *
             * This function is used by the testing code.
             *
             * @return the number of triangles represented
             */
            int tricount() const;

            /**
             * @brief Returns a const reference to a vert object inside this skelmesh
             *
             * This function is intended for testing and not to be used in other parts
             * of the model code.
             *
             * @param index the index of the verts array to get
             *
             * @return a reference to a skelmodel::vert corresponding to the index
             */
            const vert &getvert(size_t index) const;

        protected:
            tri *tris;
            int numtris;
            vert *verts;
            int numverts;

        private:
            int maxweights;
            int voffset, eoffset, elen;
            GLuint minvert, maxvert;
    };

    struct skelanimspec final
    {
        std::string name;
        int frame, range;
    };

    class skeleton
    {
        public:
            size_t numbones;
            int numgpubones;
            size_t numframes;
            dualquat *framebones; //array of quats, size equal to anim frames * bones in model
            std::vector<skelanimspec> skelanims;
            ragdollskel *ragdoll; //optional ragdoll object if ragdoll is in effect

            struct pitchtarget final
            {
                size_t bone; //an index in skeleton::bones
                int frame, corrects, deps;
                float pitchmin, pitchmax, deviated;
                dualquat pose;
            };
            std::vector<pitchtarget> pitchtargets; //vector of pitch target objects, added to models via pitchtarget command

            struct pitchcorrect final
            {
                int bone, parent;
                size_t target; //an index in skeleton::pitchtargets vector
                float pitchmin, pitchmax, pitchscale, pitchangle, pitchtotal;

                pitchcorrect(int bone, size_t target, float pitchscale, float pitchmin, float pitchmax);
                pitchcorrect();
            };
            std::vector<pitchcorrect> pitchcorrects; //vector pitch correct objects, added to models via pitchcorrect command

            std::vector<skelcacheentry> skelcache;

            skeleton(skelmeshgroup * const group);
            ~skeleton();

            /**
             * @brief Finds a skelanimspec in skeleton::skelanims
             *
             * Searches for the first skelanimspec in skeleton::skelanims with a
             * nonnull name field equalling the name passed. The first such entry
             * is returned by const pointer (or nullptr if none is found)
             *
             * @param name the skelanimspec::name to query
             *
             * @return pointer to element of skeleton::skelanims
             */
            const skelanimspec *findskelanim(std::string_view name) const;

            /**
             * @brief Adds a skelanimspec to the end of skelanims()
             *
             * @param name the name to set in the skelanimspec
             * @param numframes the number of frames to set in the skelanimspec
             * @param amimframes the number of animation frames to set in the skelanimspec
             *
             * @return a reference to the added skelanimspec
             */
            skelanimspec &addskelanim(std::string_view name, int numframes, int animframes);

            /**
             * @brief Returns the first bone index in skeleton::bones with matching name field
             *
             * @param name the name to search for
             *
             * @return the index in skeleton::bones if found, nullopt if not
             */
            std::optional<size_t> findbone(std::string_view name) const;

            /**
             * @brief Returns the first tag index in skeleton::tags with matching name field
             *
             * @param name the name to search for
             *
             * @return the index in skeleton::tags if found, nullopt if not
             */
            std::optional<size_t> findtag(std::string_view name) const;

            /**
             * @brief Modifies or sets a tag in the `skeleton::tags`
             *
             * If there is a tag in `skeleton::tags` with a matching name to `name`,
             * sets the value of `bone` and `matrix` in that object.
             *
             * If there is no such tag, creates a new one at the end of `skeleton:tags`
             * with `name`, `bone` and `matrix` set
             *
             * @param name name string to search for
             * @param bone bone value to set
             * @param matrix matrix value to set
             */
            bool addtag(std::string_view name, int bone, const matrix4x3 &matrix);

            /**
             * @brief Returns the first pitchcorrect index in skeleton::pitchcorrects with matching bone
             *
             * @param bone the bone to search for
             *
             * @return the index in skeleton::pitchcorrects if found, nullopt if not
             */
            std::optional<size_t> findpitchcorrect(int bone) const;
            void optimize();

            /**
             * @brief Applies bone mask values to the given partmask.
             *
             * No effect if mask is empty or starts with end enum value (Bonemask_End).
             * The partmask will not be changed in size, and is implied to be of size
             * `numbones`.
             * Sets `partindex` value to elements in `partmask` according to `expandbonemask()`,
             * applied to a copy of `mask`.
             * Partindex will be downcast from int -> unsigned char.
             *
             * @param mask vector of mask values to determine setting with, size numbones
             * @param partmask vector of values to conditionally set
             * @param partindex value to conditionally set to partmask
             */
            void applybonemask(const std::vector<uint> &mask, std::vector<uchar> &partmask, int partindex) const;

            /**
             * @brief Links this skeleton's children (boneinfo elements)
             *
             * Invalidates each element's child index and then resets it, if another boneinfo
             * read later (later implies position lower on tree) indicates it as a parent. The
             * value previously pointed to as the child in that parent object is set to the child's
             * `next` field.
             *
             * Invalidates the `next` element in the boneinfo's linkedlist if there is no valid parent
             * for a bone (is the top of the tree).
             */
            void linkchildren();

            /**
             * @brief Returns the number of bones available to be accelerated
             *
             * Returns the lesser of the `maxvsuniforms` (max vertex shader uniforms)
             * and `maxskelanimdata` (divided by two).
             *
             * @return number of bones that can be accelerated
             */
            static int availgpubones();

            /**
             * @brief Sets up the ragdolldata passed using the metadata of this skeleton.
             *
             * The ragdolldata, which is an object that acts as an instatiation of the ragdollskel
             * contained in skeleton->ragdoll, has its joints, animjoints, verts, and reljoints
             * set up by the ragdollskel associated with this model's skeleton.
             *
             * These values are modified by the array of dualquat transformations stored in the
             * skelcacheentry pointed to by `sc`.
             *
             * @param d the ragdolldata to set up
             * @param sc the location of the dualquat transformations to apply
             * @param scale scale factor for the vertex coordinates
             */
            void initragdoll(ragdolldata &d, const skelcacheentry &sc, float scale) const;

            /**
             * @brief Sets n to the product of m, the i'th bone's base matrix, and the i'th tag's matrix
             *
             * The contents of the matrix n have no effect on the resulting value stored in n.
             *
             * @param i the tag index in skeleton::tags to use
             * @param m the matrix to start the transform with
             * @param n the matrix to set
             */
            void concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const;
            void calctags(part *p, const skelcacheentry *sc = nullptr) const;
            void cleanup(bool full = true);

            /**
             * @brief Gets a skelcacheentry from skeleton::skelcache
             *
             * Returns the first skelcacheentry matching the specified pitch, partmask (from `as` parent part),
             * ragdolldata. If no such element exists, creates one and adds it to the back of the skelcache,
             * modifying the ragdollbones and calling interpbones() to update the model's bones
             *
             * @param pos position to set in interpbones() if entry added
             * @param scale scale to add to new skelcache entry if added
             * @param as array of animstates, size numanimparts, from which metadata is queried
             * @param pitch pitch value to check against and conditionally set
             * @param axis value to pass to interpbones() if new entry added
             * @param forward value to pass to interpbones() if new entry added
             * @param rdata ragdoll data to check against and conditionally set
             *
             * @return the skelcache entry which was either found or added
             */
            const skelcacheentry &checkskelcache(const vec &pos, float scale, const AnimState *as, float pitch, const vec &axis, const vec &forward, const ragdolldata * const rdata);
            void setgpubones(const skelcacheentry &sc, const blendcacheentry *bc, int count);
            bool shouldcleanup() const;

            /**
             * @brief Sets the pitch information for the index'th bone in the skeleton's bones
             *
             * If no bone exists at `index` returns false; otherwise returns true
             * If no bone exists, only effect is to return false.
             *
             * @param index the index in `skeleton::bones`
             * @param scale scale factor to set
             * @param offset pitch offset value to set
             * @param min pitch minimum value to set
             * @param max pitch maximum value to set
             *
             * @return true if index within bounds, false if outside
             */
            bool setbonepitch(size_t index, float scale, float offset, float min, float max);

            /**
             * @brief Returns the dualquaternion base transform from the specified bone
             *
             * Returns the dualquat base field from skeleton::bones; if index is out
             * of bounds, returns nullopt.
             *
             * @param index index in skeleton::bones
             *
             * @return value of bone's dualquat base
             */
            std::optional<dualquat> getbonebase(size_t index) const;

            /**
             * @brief Assigns the vector of dual quaternion bases to skeleton:bones
             *
             * Assigns the vector of dual quaternion bases to the skeleton::bones
             * field. If skeleton::bones and bases are not the same length, returns
             * false and performs no operation; returns true otherwise.
             *
             * @param bases the vector of bases to assign
             */
            bool setbonebases(const std::vector<dualquat> &bases);

            /**
             * @brief Sets a boneinfo's name in skeleton::bones
             *
             * Only sets name if no name is present (null string). Does not apply
             * any effect if the index is out of bounds.
             *
             * @param index the element of skeleton::bones to modify
             * @param name the new name to set
             *
             * @return true if the name was set, false if a name already existed or invalid index
             */
            bool setbonename(size_t index, std::string_view name);

            /**
             * @brief Sets a boneinfo's parent in skeleton::bones
             *
             * Does not apply any effect if the index or parent is out of bounds
             * (if either value is larger than numbones)
             *
             * @param index the element of skeleton::bones to modify
             * @param name the new name to set
             *
             * @return true if the name was set, false if either value was an invalid index
             */
            bool setboneparent(size_t index, size_t parent);

            /**
             * @brief Creates a boneinfo array and assigns it to skeleton::bones
             *
             * Also sets the value of numbones to the size passed.
             * Will cause a memory leak if skeleton::bones is already heap-allocated.
             *
             * @param num the number of array elements in the new array
             */
            void createbones(size_t num);

            /**
             * @brief Creates a ragdoll if none is defined; returns the skeleton's ragdoll
             *
             * @return a pointer to this model's ragdoll
             */
            ragdollskel *trycreateragdoll();

        private:
            skelmeshgroup * const owner;
            size_t numinterpbones;

            struct boneinfo final
            {
                std::string name;
                int parent, //parent node in boneinfo
                    children, //first index of child bone list in boneinfo
                    next, //next adjacent sibling bone in boneinfo, last bone in sibling list has next = 0
                    group,
                    scheduled,
                    interpindex,
                    interpparent,
                    ragdollindex,
                    correctindex;
                float pitchscale, pitchoffset, pitchmin, pitchmax;
                dualquat base;

                boneinfo();
            };
            /**
             * nodes in boneinfo tree, node relations in the tree are indicated
             * by boneinfo's fields
             *
             * n-leaf tree (nodes can have 0...INT_MAX children)
             *
             * size equal to numbones
             */
            boneinfo *bones;

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

                tag(std::string_view name, int bone, matrix4x3 matrix) : name(name), bone(bone), matrix(matrix) {}
            };
            std::vector<tag> tags;

            /**
             * @brief Cache used by skeleton::getblendoffset() to cache glGetUniformLocation queries
             */
            std::unordered_map<GLuint, GLint> blendoffsets;

            /**
             * @brief Creates a new antipode array
             *
             * Clears the existing skeleton::antipode vector and adds new bones
             * corresponding to boneinfo objects from the skeleton::bones array.
             *
             * The number of antipodes in the created array is no larger than the
             * number of values in the skeleton::bones array (skeleton::numbones)
             * multiplied by the number of bones with their group field set to
             * a value larger than numbones.
             */
            void calcantipodes();
            void remapbones();

            struct framedata
            {
                const dualquat *fr1, *fr2, //frame data
                               *pfr1, *pfr2; //part frame data
            };

            /**
             * @brief Gets the location of the uniform specified
             *
             * Helper function for setglslbones().
             *
             * Gets the uniform location of the uniform in `u`, at index
             * 2*`skeleton::numgpubones`. Adds the shader program in `shader::lastshader`
             * to `blendoffsets` if it is not already there.
             *
             * Once a shader program has been added to the `skeleton::blendoffsets`
             * map, further calls of this function while that shader program is
             * assigned to `shader::lastshader` will return the first uniformloc
             * location query value associated with that program, and the parameter
             * will be ignored.
             *
             * Returns the array element at 2*skeleton::numgpubones, skipping
             * the values used in `setglslbones()` to set the `sc` skelcacheentry values.
             *
             * @param u the uniformloc to query
             *
             * @return a GLint location of the uniform array at a position skipping the "sc" elements
             */
            GLint getblendoffset(const UniformLoc &u);

            /**
             * @brief Sets uniform values from skelcacheentries to the uniform at the specified UniformLoc
             *
             * Uses glUniform4fv to copy 4 dimensional quaternion values from the specified
             * skelcacheentries into the GL uniform array pointed to by the UniformLoc u.
             * The number of values copied from sc will be skeleton::numgpubones*2, and
             * the number of values copied from bc will be `count`. Only the real component
             * of the dual quaternions are copied.
             *
             * Sets the version and data values of the UniformLoc to that of the bc parameter,
             * to cache this operation only to occur when there is a mismatch between those
             * two objects.
             *
             * @param u the uniform location object to modify corresponding uniforms of
             * @param sc the skelcacheentry from which to set
             * @param bc the skelcacheentry from which to set the trailing values from
             * @param count the number of entries from bc to place in
             */
            void setglslbones(UniformLoc &u, const skelcacheentry &sc, const skelcacheentry &bc, int count);
            dualquat interpbone(int bone, const std::array<framedata, maxanimparts> &partframes, const AnimState *as, const uchar *partmask) const;
            void addpitchdep(int bone, int frame);
            static float calcdeviation(const vec &axis, const vec &forward, const dualquat &pose1, const dualquat &pose2);

            /**
             * @brief Searches for a pitchdep in the pitchdeps field
             *
             * Searches the pitchdeps vector field in ascending order. For each pitchdep
             * in the pitchdeps vector, if the bone stored in that pitchdep is at least as
             * high of an index (lower on the tree) then searching is stopped and the
             * function will either return that index (if the pitchdep's bone field exactly
             * matches) or nullopt if the bone at that pitchdep is lower on the tree.
             *
             * If the bone passed is larger than any bone data in any pitchep in the
             * pitchdeps vector, returns nullopt.
             *
             * @param bone the bone to search for
             *
             * @return nullopt if no such valid pitchdep exists
             * @return index of pitchdep if found
             */
            std::optional<size_t> findpitchdep(int bone) const;
            void initpitchdeps();

            /**
             * @brief Recursively applies the specified mask value to the bone mask array passed.
             *
             * The expansion array should be equal to the number of bones (which may be greater
             * than the bone parameter).
             *
             * Assigns the value val to the bone'th element in the expansion array, then calls
             * this function recursively for all children pointed to by the bone's index in
             * the boneinfo array `bones`.
             *
             * Applies the value val to children in boneinfo depth-first. All nodes traversed
             * will have the same value assigned (in the expansion array).
             *
             * @param expansion mask array to assign values to
             * @param bone the root bone to assign values to
             * @param val the value to set
             */
            void expandbonemask(uchar *expansion, int bone, int val) const;
            void calcpitchcorrects(float pitch, const vec &axis, const vec &forward);
            void interpbones(const AnimState *as, float pitch, const vec &axis, const vec &forward, int numanimparts, const uchar *partmask, skelcacheentry &sc);

            /**
             * @brief Sets up a skelcacheentry's bone transformations.
             *
             * Uses the model data at `d` and the translation/positon information passed
             * to set up the dual quaternion transformation array in the passed skelcacheentry.
             * Creates a new array of size `skeleton::numinterpbones` if no array exists, allocated
             * on the heap.
             *
             * @param d the ragdolldata to use vertex/tri information from
             * @param sc the skelcacheentry to set up
             * @param translate the position of the model
             * @param scale the scale factor of the model's vertices
             */
            void genragdollbones(const ragdolldata &d, skelcacheentry &sc, const vec &translate, float scale) const;

    };

    class skelmeshgroup : public meshgroup
    {
        public:
            skeleton *skel;

            std::vector<blendcombo> blendcombos;

            GLuint *edata;

            skelmeshgroup() : skel(nullptr), edata(nullptr), ebuf(0), vweights(0), vlen(0), vertsize(0), vblends(0), vdata(nullptr)
            {
                numblends.fill(0);
            }

            virtual ~skelmeshgroup();

            std::optional<size_t> findtag(std::string_view) final;

            /**
             * @brief Returns the skelmodel::skeleton object this skelmeshgroup points to.
             *
             * Returns the pointer to the skeleton object associated with this object.
             */
            void *animkey() final;
            int totalframes() const final;
            void concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const final;
            void preload() final;
            void render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p) final;

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
            /**
             * @brief Generates a vertex buffer object for an associated vbocache entry
             * the vbocacheentry passed will have its vbuf assigned to a GL buffer,
             * and if there is no ebuf (element array buffer) the following will
             * occur (summarized):
             *
             * - vweights will be set depending animation presence and gpuskel
             * - vlen will be set to the sum of all the encapsulated meshes' vertices
             * - vdata will be deleted and re-allocated as an array of size vlen*sizeof(vert object)
             * - vdata will be filled with values using fillverts() (which gets data from skelmesh::verts array)
             * - ebuf will be filled with data from skelmesh::genvbo, with ebuf size being equal to all of the respective meshes' tri counts summed
             *
             * @param vc the vbocacheentry to modify
             */
            void genvbo(vbocacheentry &vc);
            void bindvbo(const AnimState *as, const part *p, const vbocacheentry &vc);
            int addblendcombo(const blendcombo &c);
            //sorts the blendcombos by its comparison function, then applies this new order to associated skelmesh verts
            void sortblendcombos();
            void blendbones(const skelcacheentry &sc, blendcacheentry &bc) const;
            void cleanup() final;

            virtual bool load(std::string_view meshfile, float smooth, part &p) = 0;
            virtual const skelanimspec *loadanim(const std::string &filename) = 0;
        private:
            std::array<int, 4> numblends;

            static constexpr size_t maxblendcache = 16; //number of entries in the blendcache entry array
            static constexpr size_t maxvbocache = 16;   //number of entries in the vertex buffer object array

            std::array<blendcacheentry, maxblendcache> blendcache;
            std::array<vbocacheentry, maxvbocache> vbocache;
            /*
             * ebuf, vbo variables are all initialized by genvbo(vbocacheentry), if render() has an ebuf
             * present then vbo variables will not be modified in render()
             */
            GLuint ebuf; //GL_ELEMENT_ARRAY_BUFFER gluint handle
            int vweights, //number of vbo weights, values 0...4
                vlen, //sum of this skelmeshgroup's renderable meshes' vertex counts
                vertsize, //sizeof vvert, if skeleton has animation frames & gpuskel; sizeof vvertgw if animation frames and no gpuskel, sizeof vvertg if neither
                vblends; //number of blendcombos (= number of verts in e.g. md5)
            uchar *vdata; //vertex data drawn in the render() stage. It is filled by genvbo() and then used as a GL_ARRAY_BUFFER in the render() stage.

            blendcacheentry &checkblendcache(const skelcacheentry &sc, int owner);
    };

    class skelpart : public part
    {
        public:
            std::vector<uchar> partmask;

            skelpart(animmodel *model, int index = 0);
            virtual ~skelpart();

            void initanimparts();
            bool addanimpart(const std::vector<uint> &bonemask);
            void loaded() final;
        private:
            std::vector<uchar> buildingpartmask;

            /**
             * @brief Manages caching of part masking data.
             *
             * Attempts to match the passed vector of uchar with one in the internal static vector.
             * If a matching entry is found, empties the passed vector returns the entry in the cache.
             *
             * @param o a vector of uchar to delete or insert into the internal cache
             * @return the passed value o, or the equivalent entry in the internal cache
             */
            std::vector<uchar> &sharepartmask(std::vector<uchar> &o);
            std::vector<uchar> newpartmask();
            void endanimparts();
    };

    //ordinary methods
    skelmodel(std::string name);
    skelpart &addpart();
    meshgroup *loadmeshes(const std::string &name, float smooth = 2);
    meshgroup *sharemeshes(const std::string &name, float smooth = 2);
    //virtual methods
    virtual skelmeshgroup *newmeshes() = 0;
    //override methods

    /**
     * @brief Returns the link type of an animmodel relative to a part
     *
     * If `this` model's zeroth part's mesh's skel is the same as the passed part's
     * mesh's skel, returns Link_Reuse
     * If the passed model parameter is not linkable, or does not meet the criteria above,
     * returns Link_Tag.
     *
     * @param m must point to a valid animmodel object
     * @param p must point to a valid part object which points to a valid skeleton.
     */
    int linktype(const animmodel *m, const part *p) const final;
    bool skeletal() const final;

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

/**
 * @brief Defines skeletal commands for a chosen type of skeletal model format.
 *
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
    typedef struct MDL::skeleton::pitchtarget pitchtarget;
    typedef struct MDL::skeleton::pitchcorrect pitchcorrect;

    //unused second param
    static void loadpart(const char *meshfile, const char *, const float *smooth)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        std::string filename;
        filename.append(MDL::dir).append("/").append(meshfile ? meshfile : "");
        part &mdl = MDL::loading->addpart();
        mdl.meshes = MDL::loading->sharemeshes(path(filename), *smooth > 0 ? std::cos(std::clamp(*smooth, 0.0f, 180.0f)/RAD) : 2);
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

    /**
     * @brief Adds a tag corresponding to a bone
     *
     * @param name the name of the bone in the skeletal model
     * @param name the new name to assign the associated tag
     *
     * @param tx/ty/tz translation parameters
     * @param rx/ry/rz rotation parameters
     */
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
        std::optional<size_t> i = mdl.meshes ? static_cast<meshgroup *>(mdl.meshes)->skel->findbone(name) : std::nullopt;
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
            std::optional<size_t> i = mdl.meshes ? static_cast<meshgroup *>(mdl.meshes)->skel->findbone(name) : std::nullopt;
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
        std::string filename = std::format("{}/{}", MDL::dir, animfile);
        const animspec *sa = static_cast<meshgroup *>(mdl.meshes)->loadanim(path(filename));
        if(!sa)
        {
            conoutf("could not load %s anim file %s", MDL::formatname(), filename.c_str());
            return;
        }
        skeleton *skel = static_cast<meshgroup *>(mdl.meshes)->skel;
        std::optional<size_t> bone = skel ? skel->findbone(name) : std::nullopt;
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

    /**
     * @brief Adds a pitch correction to this model's skeleton
     *
     * @param name the name of the bone to pitch correct
     * @param targetname the name of the bone to target
     * @param scale the scale to apply to the pitchcorrect
     * @param pitchmin the minimum pitch to apply
     * @param pitchmax the maximum pitch to apply
     */
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
        std::optional<size_t> targetbone = skel->findbone(targetname),
                              target = std::nullopt;
        if(targetbone)
        {
            for(size_t i = 0; i < skel->pitchtargets.size(); i++)
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
        size_t pos = skel->pitchcorrects.size();
        for(size_t i = 0; i < skel->pitchcorrects.size(); i++)
        {
            if(bone <= skel->pitchcorrects[i].bone)
            {
                pos = i;
                break;
            }
        }
        skel->pitchcorrects.insert(skel->pitchcorrects.begin() + pos, c);
    }

    /**
     * @param Assigns an animation to the currently loaded model.
     *
     * Attempts to give a model object an animation by the name of anim parameter
     * loaded from animfile with speed/priority/offsets to determine how fast
     * and what frames play.
     *
     * The name of the anim being loaded (anim param) must be in the global animnames vector.
     *
     * The MDL::loading static field must be set by calling startload() for the
     * relevant model object.
     *
     * The animation will be applied to the most recent loaded part (with loadpart()).
     */
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
            std::string filename = std::format("{}/{}", MDL::dir, animfile);
            const animspec *sa = static_cast<meshgroup *>(p->meshes)->loadanim(path(filename));
            if(!sa)
            {
                conoutf("could not load %s anim file %s", MDL::formatname(), filename.c_str());
            }
            else
            {
                for(size_t i = 0; i < anims.size(); i++)
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

    /**
     * @brief Assigns a subtree of bones to a bone mask.
     *
     * This bone mask is used to separate a part into two (and no more than two)
     * distinct groups of subtrees which can be animated independently.
     *
     * These bones which make up subtree(s) under the specified bones are saved
     * in the partmask vector of the relevant skelpart.
     *
     */
    static void setanimpart(const char *maskstr)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part *p = static_cast<part *>(MDL::loading->parts.back());

        std::vector<std::string> bonestrs;
        explodelist(maskstr, bonestrs);
        std::vector<uint> bonemask;
        for(size_t i = 0; i < bonestrs.size(); i++)
        {
            const std::string &bonestr = bonestrs[i];
            std::optional<int> bone = p->meshes ? static_cast<meshgroup *>(p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr.substr(1) : bonestr) : std::nullopt;
            if(!bone)
            {
                conoutf("could not find bone %s for anim part mask [%s]", bonestr.c_str(), maskstr);
                return;
            }
            bonemask.push_back(*bone | (bonestr[0]=='!' ? Bonemask_Not : 0));
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
        std::vector<std::string> bonestrs;
        explodelist(maskstr, bonestrs);
        std::vector<uint> bonemask;
        for(size_t i = 0; i < bonestrs.size(); i++)
        {
            const std::string &bonestr = bonestrs[i];
            std::optional<int> bone = p->meshes ? static_cast<meshgroup *>(p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr.substr(1) : bonestr) : std::nullopt;
            if(!bone)
            {
                conoutf("could not find bone %s for hit zone mask [%s]", bonestr.c_str(), maskstr);
                return;
            }
            bonemask.push_back(*bone | (bonestr[0]=='!' ? Bonemask_Not : 0));
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

    /**
     * @brief Checks for an available ragdoll to modify.
     *
     * If a skeletal model is being loaded, and meets the criteria for a ragdoll,
     * returns the pointer to that ragdollskel (new one made if necessary), returns nullptr otherwise
     *
     * @return pointer to the loading skelmodel's ragdoll, or nullptr if no such object available
     */
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
        ragdollskel *ragdoll = skel->trycreateragdoll();
        if(ragdoll->loaded)
        {
            return nullptr;
        }
        return ragdoll;
    }

    /**
     * @brief Adds a vertex to the working ragdoll vert list
     *
     * @param x the x position of the new vert
     * @param y the y position of the new vert
     * @param z the z position of the new vert
     * @param radius the effect radius of the ragdoll vert
     */
    static void rdvert(const float *x, const float *y, const float *z, const float *radius)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->verts.push_back({vec(*x, *y, *z), *radius > 0 ? *radius : 1, 0.f});
    }

    /**
     *  @brief sets the ragdoll eye level
     *
     * Sets the ragdoll's eye point to the level passed
     * implicitly modifies the ragdoll selected by CHECK_RAGDOLL
     *
     * @param v the level to set the eye at
     */
    static void rdeye(const int *v)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->eye = *v;
    }


    /**
     * @brief adds a ragdoll tri
     *
     * Adds a triangle to the current ragdoll with the specified indices.
     * No effect if there is no current ragdoll.
     *
     * @param v1 first vertex index
     * @param v2 second vertex index
     * @param v3 third vertex index.
     */
    static void rdtri(const int *v1, const int *v2, const int *v3)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->tris.push_back({*v1, *v2, *v3});
    }

    static void rdjoint(const int *n, const int *t, const int *v1, const int *v2, const int *v3)
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
        ragdoll->joints.push_back({*n, *t, {*v1, *v2, *v3}, 0.f, matrix4x3()});
    }

    static void rdlimitdist(const int *v1, const int *v2, const float *mindist, const float *maxdist)
    {
        ragdollskel *ragdoll = checkragdoll();
        if(!ragdoll)
        {
            return;
        }
        ragdoll->distlimits.push_back({*v1, *v2, *mindist, std::max(*maxdist, *mindist)});
    }

    static void rdlimitrot(const int *t1, const int *t2, const float *maxangle, const float *qx, const float *qy, const float *qz, const float *qw)
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

    static void rdanimjoints(const int *on)
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

#ifndef MODEL_H_
#define MODEL_H_

enum
{
    MDL_MD5 = 0,
    MDL_OBJ,
    MDL_GLTF,
    MDL_NumMDLTypes
};

/* model: the base class for an ingame model, with only pure virtual methods,
 * all methods are defined in animmodel or one of its defined classes
 *
 * extended by animmodel (animated model) which is itself extended by skelmodel
 * and vertmodel
 *
 * a model format loader (e.g. md5 or obj) extends model or one of its children
 * and assigns the data from the file format into the object (by setting its
 * fields and overriding its methods)
 *
 * model class hierarchy (all are objects except bottom gvars)
 *
 *      /-------\
 *      | model |
 *      \-------/
 *          |
 *          v
 *      /-----------\
 *      | animmodel |
 *      \-----------/
 *          |     \__________
 *          v                \
 *      /-----------\         v
 *      | skelmodel |       /-----------\
 *      \-----------/       | vertmodel |
 *        |                 \-----------/
 *        |     /-------------\       |
 *        |     | modelloader |       |
 *        |     \-------------/       |
 *        |            |              |
 *        \__________/ \_____________/  <-- multiple inheritance via template class
 *              |                 |
 *              v                 v
 *          /------------\        /------------\
 *          | skelloader |        | vertloader |
 *          \------------/        \------------/
 *     _______|    |                           |
 *    |            v     /---------------\     v
 * /----\     /-----\    | modelcommands |    /-----\
 * |gltf|  -->| md5 |    \---------------/    | obj |<-
 * \----/  |  \-----/        |        |       \-----/ |
 *  ^ |    |   |             v        v            |  |
 *  | |    |   | /--------------\ /--------------\ |  |
 *  | |    |   | | skelcommands | | vertcommands | |  |
 *  | |    |   | \--------------/ \--------------/ |  |
 *  | |    |   |   |           |               |   |  |
 *  | |    |   v   v           |               v   v  |
 *  | | /-------------------\  | /-------------------\
 *  | | | skelcommands<md5> |  | | vertcommands<obj> |
 *  | | | md5::md5commands  |  | | obj::objcommands  |
 *  | | \---static field----/  | \---static field----/
 *  | v                       /
 *  /--------------------\   /
 *  | skelcommands<gltf> |<-/
 *  | gltf::gltfcommands |
 *  \----static field----/
 */
class model
{
    public:
        //for spin, orientation: x = yaw, y = pitch, z = roll
        vec orientation;
        bool shadow, alphashadow, depthoffset;
        std::unique_ptr<BIH> bih;
        vec bbextend;
        float eyeheight, collidexyradius, collideheight;
        std::string collidemodel;
        int collide, batch;

        virtual ~model() = default;
        virtual void calcbb(vec &, vec &) const = 0;
        virtual void calctransform(matrix4x3 &) const = 0;
        virtual int intersect(int, int, int, const vec &, float, float, float, dynent *, modelattach *, float, const vec &, const vec &, float &) const = 0;
        virtual void render(int, int, int, const vec&, float, float, float , dynent *, modelattach * = nullptr, float = 1, const vec4<float>& = vec4<float>(1, 1, 1, 1)) const = 0;
        virtual bool load() = 0;
        virtual int type() const = 0;
        virtual bool setBIH() = 0;
        virtual bool skeletal() const = 0;
        virtual bool animated() const = 0;
        virtual bool pitched() const = 0;
        virtual bool alphatested() const = 0;

        virtual void setshader(Shader *) = 0;
        virtual void setspec(float) = 0;
        virtual void setgloss(int) = 0;
        virtual void setglow(float, float, float) = 0;
        virtual void setalphatest(float) = 0;
        virtual void setfullbright(float) = 0;
        virtual void setcullface(int) = 0;
        virtual void setcolor(const vec &) = 0;
        virtual void settransformation(const std::optional<vec>,
                                       const std::optional<vec>,
                                       const std::optional<vec>,
                                       const std::optional<float>) = 0;
        //returns the location and size of the model
        virtual vec4<float> locationsize() const = 0;
        virtual void genshadowmesh(std::vector<triangle> &, const matrix4x3 &) = 0;

        virtual void preloadBIH() = 0;
        virtual void preloadshaders() = 0;
        virtual void preloadmeshes() = 0;
        virtual void cleanup() = 0;
        virtual void startrender() const = 0;
        virtual void endrender() const = 0;
        virtual void boundbox(vec &center, vec &radius) = 0;
        virtual float collisionbox(vec &center, vec &radius) = 0;
        virtual float above() = 0;
        virtual const std::string &modelname() const = 0;

    protected:
        vec translate,
            spin;
        float scale;
        std::string name;
        vec bbcenter, bbradius, collidecenter, collideradius;
        float rejectradius;

        model(std::string name) : orientation(0, 0, 0),
                                  shadow(true),
                                  alphashadow(true),
                                  depthoffset(false),
                                  bbextend(0, 0, 0),
                                  eyeheight(0.9f),
                                  collidexyradius(0),
                                  collideheight(0),
                                  collidemodel(""),
                                  collide(Collide_OrientedBoundingBox),
                                  batch(-1),
                                  translate(0, 0, 0),
                                  spin(0, 0, 0),
                                  scale(1.0f),
                                  name(name),
                                  bbcenter(0, 0, 0),
                                  bbradius(-1, -1, -1),
                                  collidecenter(0, 0, 0),
                                  collideradius(-1, -1, -1),
                                  rejectradius(-1)
                                  {}
};

#endif

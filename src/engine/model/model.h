#ifndef MODEL_H_
#define MODEL_H_

enum
{
    MDL_MD5 = 0,
    MDL_OBJ,
    MDL_NumMDLTypes
};

/* model: the base class for an ingame model
 *
 * extended by animmodel (animated model) which is itself extended by skelmodel
 *
 * a model format loader (e.g. md5 or obj) extends model or one of its children
 * and assigns the data from the file format into the object (by setting its
 * fields and overriding its methods)
 */
class model
{
    public:
        //for spin, orientation: x = yaw, y = pitch, z = roll
        vec spin,
            orientation;
        bool shadow, alphashadow, depthoffset;
        float scale;
        vec translate;
        std::unique_ptr<BIH> bih;
        vec bbextend;
        float eyeheight, collidexyradius, collideheight;
        const char *collidemodel;
        int collide, batch;

        virtual ~model()
        {
        }

        virtual void calcbb(vec &center, vec &radius) const = 0;
        virtual void calctransform(matrix4x3 &m) const = 0;
        virtual int intersect(int anim, int basetime, int basetime2, const vec &pos, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec &o, const vec &ray, float &dist, int mode) const = 0;
        virtual void render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a = nullptr, float size = 1, const vec4<float> &color = vec4<float>(1, 1, 1, 1)) const = 0;
        virtual bool load() = 0;
        virtual int type() const = 0;
        virtual bool setBIH() { return false; }
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

        virtual void settransformation(const std::optional<vec> pos,
                                       const std::optional<vec> rotate,
                                       const std::optional<vec> orient,
                                       const std::optional<float> size)
        {
            if(pos)
            {
                translate = pos.value();
            }
            if(rotate)
            {
                spin = rotate.value();
            }
            if(orient)
            {
                orientation = orient.value();
            }
            if(size)
            {
                scale = size.value();
            }
        }

        //returns the location and size of the model
        virtual vec4<float> locationsize() const
        {
            return vec4<float>(translate.x, translate.y, translate.z, scale);
        }

        virtual void genshadowmesh(std::vector<triangle> &, const matrix4x3 &) {}

        virtual void preloadBIH()
        {
            if(!bih)
            {
                setBIH();
            }
        }

        virtual void preloadshaders() = 0;
        virtual void preloadmeshes() = 0;
        virtual void cleanup() {}

        virtual void startrender() const = 0;
        virtual void endrender() const = 0;

        void boundbox(vec &center, vec &radius)
        {
            if(bbradius.x < 0)
            {
                calcbb(bbcenter, bbradius);
                bbradius.add(bbextend);
            }
            center = bbcenter;
            radius = bbradius;
        }

        float collisionbox(vec &center, vec &radius)
        {
            if(collideradius.x < 0)
            {
                boundbox(collidecenter, collideradius);
                if(collidexyradius)
                {
                    collidecenter.x = collidecenter.y = 0;
                    collideradius.x = collideradius.y = collidexyradius;
                }
                if(collideheight)
                {
                    collidecenter.z = collideradius.z = collideheight/2;
                }
                rejectradius = collideradius.magnitude();
            }
            center = collidecenter;
            radius = collideradius;
            return rejectradius;
        }

        float above()
        {
            vec center, radius;
            boundbox(center, radius);
            return center.z+radius.z;
        }

        const std::string &modelname() const
        {
            return name;
        }

    protected:
        model(const char *name) : spin(0, 0, 0),
                                  orientation(0, 0, 0),
                                  shadow(true),
                                  alphashadow(true),
                                  depthoffset(false),
                                  scale(1.0f),
                                  translate(0, 0, 0),
                                  bbextend(0, 0, 0),
                                  eyeheight(0.9f),
                                  collidexyradius(0),
                                  collideheight(0),
                                  collidemodel(nullptr),
                                  collide(Collide_OrientedBoundingBox),
                                  batch(-1),
                                  name(name),
                                  bbcenter(0, 0, 0),
                                  bbradius(-1, -1, -1),
                                  collidecenter(0, 0, 0),
                                  collideradius(-1, -1, -1),
                                  rejectradius(-1)
                                  {}
    private:
        std::string name;
        vec bbcenter, bbradius, collidecenter, collideradius;
        float rejectradius;
};

#endif

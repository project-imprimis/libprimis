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
        char *name;
        float spinyaw, spinpitch, spinroll, offsetyaw, offsetpitch, offsetroll;
        bool shadow, alphashadow, depthoffset;
        float scale;
        vec translate;
        BIH *bih;
        vec bbextend;
        float eyeheight, collidexyradius, collideheight;
        char *collidemodel;
        int collide, batch;

        model(const char *name) : name(name ? newstring(name) : nullptr),
                                  spinyaw(0),
                                  spinpitch(0),
                                  spinroll(0),
                                  offsetyaw(0),
                                  offsetpitch(0),
                                  offsetroll(0),
                                  shadow(true),
                                  alphashadow(true),
                                  depthoffset(false),
                                  scale(1.0f),
                                  translate(0, 0, 0),
                                  bih(0),
                                  bbextend(0, 0, 0),
                                  eyeheight(0.9f),
                                  collidexyradius(0),
                                  collideheight(0),
                                  collidemodel(nullptr),
                                  collide(Collide_OrientedBoundingBox),
                                  batch(-1), bbcenter(0, 0, 0),
                                  bbradius(-1, -1, -1),
                                  collidecenter(0, 0, 0),
                                  collideradius(-1, -1, -1),
                                  rejectradius(-1) {}

        virtual ~model()
        {
            delete[] name;
            name = nullptr;
            if(bih)
            {
                delete bih;
                bih = nullptr;
            }
        }
        virtual void calcbb(vec &center, vec &radius) = 0;
        virtual void calctransform(matrix4x3 &m) = 0;
        virtual int intersect(int anim, int basetime, int basetime2, const vec &pos, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec &o, const vec &ray, float &dist, int mode) = 0;
        virtual void render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a = nullptr, float size = 1, const vec4<float> &color = vec4<float>(1, 1, 1, 1)) = 0;
        virtual bool load() = 0;
        virtual int type() const = 0;
        virtual BIH *setBIH() { return nullptr; }
        virtual bool skeletal() const { return false; }
        virtual bool animated() const { return false; }
        virtual bool pitched() const { return true; }
        virtual bool alphatested() const { return false; }

        virtual void setshader(Shader *) {}
        virtual void setspec(float) {}
        virtual void setgloss(int) {}
        virtual void setglow(float, float, float) {}
        virtual void setalphatest(float) {}
        virtual void setfullbright(float) {}
        virtual void setcullface(int) {}
        virtual void setcolor(const vec &) {}

        virtual void genshadowmesh(std::vector<triangle> &, const matrix4x3 &) {}
        virtual void preloadBIH() { if(!bih) setBIH(); }
        virtual void preloadshaders() {}
        virtual void preloadmeshes() {}
        virtual void cleanup() {}

        virtual void startrender() {}
        virtual void endrender() {}

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

        float boundsphere(vec &center)
        {
            vec radius;
            boundbox(center, radius);
            return radius.magnitude();
        }

        float above()
        {
            vec center, radius;
            boundbox(center, radius);
            return center.z+radius.z;
        }
    private:
        vec bbcenter, bbradius, collidecenter, collideradius;
        float rejectradius;
};

#endif

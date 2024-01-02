#ifndef RADIANCEHINTS_H_
#define RADIANCEHINTS_H_

//note that radiance hints is the term for the mechanism by which global illumination is done
constexpr int rhmaxsplits = 4, //maximum number of times that radiance hints can split to increase resolution (note exponential increase in nodes)
              rhmaxgrid = 64; //subdivision count for radiance hints

extern int rhrect, rhgrid, rhsplits, rhborder, rhprec, rhtaps, rhcache, rhforce, rsmprec, rsmdepthprec, rsmsize;
extern int gi, gidist;
extern float giscale, giaoscale;
extern int debugrsm, debugrh;
extern std::array<GLuint, 8> rhtex;
extern Shader *rsmworldshader;

//defines the size, position & projection info for a reflective shadow map
// the reflective shadow map is then used to calculate global illumination
class reflectiveshadowmap
{
    public:
        plane cull[4];
        matrix4 model, proj;
        vec lightview;
        vec scale, offset;
        void setup();
    private:
        vec center, bounds;
        void getmodelmatrix();
        void getprojmatrix();
        void gencullplanes();
};

extern reflectiveshadowmap rsm;

class radiancehints
{
    public:
        radiancehints() : dynmin(1e16f, 1e16f, 1e16f), dynmax(-1e16f, -1e16f, -1e16f), prevdynmin(1e16f, 1e16f, 1e16f), prevdynmax(-1e16f, -1e16f, -1e16f) {}

        vec dynmin, dynmax;
        void setup();
        void renderslices();
        void bindparams() const;
        void clearcache();
        bool allcached() const;
        //copies dynmin/max to prevdynmin/max
        void rotatedynlimits();
        //checks if prevmin's z value is less than prevmax
        bool checkprevbounds();
    private:
        vec prevdynmin, prevdynmax;
        //splits are used to LOD global illumination (more detail near camera)
        struct splitinfo
        {
            float nearplane, farplane;
            vec offset, scale;
            vec center; float bounds;
            vec cached; bool copied;

            splitinfo() : center(-1e16f, -1e16f, -1e16f), bounds(-1e16f), cached(-1e16f, -1e16f, -1e16f), copied(false)
            {
            }

            void clearcache()
            {
                bounds = -1e16f;
            }
        };
        std::array<splitinfo, rhmaxsplits> splits;

        void updatesplitdist();
};

extern radiancehints rh;

extern void clearradiancehintscache();
extern bool useradiancehints();
extern void setupradiancehints();
extern void cleanupradiancehints();

extern void viewrh();
extern void viewrsm();

#endif

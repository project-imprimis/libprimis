#ifndef RADIANCEHINTS_H_
#define RADIANCEHINTS_H_

//note that radiance hints is the term for the mechanism by which global illumination is done
const int rhmaxsplits = 4; //maximum number of times that radiance hints can split to increase resolution (note exponential increase in nodes)
const int rhmaxgrid = 64; //subdivision count for radiance hints

extern int rhrect, rhgrid, rhsplits, rhborder, rhprec, rhtaps, rhcache, rhforce, rsmprec, rsmdepthprec, rsmsize;
extern int gi, gidist;
extern float giscale, giaoscale;
extern int debugrsm, debugrh;
extern GLuint rhtex[8];
extern Shader *rsmworldshader;

//defines the size, position & projection info for a reflective shadow map
// the reflective shadow map is then used to calculate global illumination
struct reflectiveshadowmap
{
    matrix4 model, proj;
    vec lightview;
    plane cull[4];
    vec scale, offset;
    vec center, bounds;
    void setup();
    void getmodelmatrix();
    void getprojmatrix();
    void gencullplanes();
};

extern reflectiveshadowmap rsm;

struct radiancehints
{
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
    } splits[rhmaxsplits];

    vec dynmin, dynmax, prevdynmin, prevdynmax;

    radiancehints() : dynmin(1e16f, 1e16f, 1e16f), dynmax(-1e16f, -1e16f, -1e16f), prevdynmin(1e16f, 1e16f, 1e16f), prevdynmax(-1e16f, -1e16f, -1e16f) {}

    void setup();
    void updatesplitdist();
    void bindparams();
    void renderslices();

    void clearcache();
    bool allcached() const;
};

extern radiancehints rh;

extern void clearradiancehintscache();
extern bool useradiancehints();
extern void renderradiancehints();
extern void setupradiancehints();
extern void cleanupradiancehints();

extern void viewrh();
extern void viewrsm();

#endif

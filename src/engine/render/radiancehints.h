
//note that radiance hints is the term for the mechanism by which global illumination is done
#define RH_MAXSPLITS 4 //maximum number of times that radiance hints can split to increase resolution (note exponential increase in nodes)
#define RH_MAXGRID 64 //subdivision count for radiance hints

extern int rhrect, rhgrid, rhsplits, rhborder, rhprec, rhtaps, rhcache, rhforce, rsmprec, rsmdepthprec, rsmsize;
extern int gi, gidist;
extern float giscale, giaoscale;
extern int debugrsm, debugrh;
extern GLuint rhtex[8]; 

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
    struct splitinfo
    {
        float nearplane, farplane;
        vec offset, scale;
        vec center; float bounds;
        vec cached; bool copied;

        splitinfo() : center(-1e16f, -1e16f, -1e16f), bounds(-1e16f), cached(-1e16f, -1e16f, -1e16f), copied(false) {}

        void clearcache() { bounds = -1e16f; }
    } splits[RH_MAXSPLITS];

    vec dynmin, dynmax, prevdynmin, prevdynmax;

    radiancehints() : dynmin(1e16f, 1e16f, 1e16f), dynmax(-1e16f, -1e16f, -1e16f), prevdynmin(1e16f, 1e16f, 1e16f), prevdynmax(-1e16f, -1e16f, -1e16f) {}

    void setup();
    void updatesplitdist();
    void bindparams();
    void renderslices();

    void clearcache()
    {
        for(int i = 0; i < RH_MAXSPLITS; ++i)
        {
            splits[i].clearcache();
        }
    }
    bool allcached() const {
        for(int i = 0; i < rhsplits; ++i)
        {
            if(splits[i].cached != splits[i].center)
            {
                return false;
            }
        }
        return true;
    }
};

extern radiancehints rh;

extern void clearradiancehintscache();
extern bool useradiancehints();
extern void renderradiancehints();
extern void setupradiancehints();
extern void cleanupradiancehints();

extern void viewrh();
extern void viewrsm();

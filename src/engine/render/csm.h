#ifndef CSM_H_
#define CSM_H_

static const int csmmaxsplits = 8;

class cascadedshadowmap
{
    public:
        struct splitinfo
        {
            float nearplane;     // split distance to near plane
            float farplane;      // split distance to farplane
            matrix4 proj;        // one projection per split
            vec scale, offset;   // scale and offset of the projection
            int idx;             // shadowmapinfo indices
            vec center, bounds;  // max extents of shadowmap in sunlight model space
            plane cull[4];       // world space culling planes of the split's projected sides
        };
        matrix4 model;                  // model view is shared by all splits
        splitinfo splits[csmmaxsplits]; // per-split parameters
        vec lightview;                  // view vector for light
        void setup();                   // insert shadowmaps for each split frustum if there is sunlight
        void bindparams();              // bind any shader params necessary for lighting
        int calcbbcsmsplits(const ivec &bbmin, const ivec &bbmax);
        int calcspherecsmsplits(const vec &center, float radius);

    private:
        void updatesplitdist();         // compute split frustum distances
        void getmodelmatrix();          // compute the shared model matrix
        void getprojmatrix();           // compute each cropped projection matrix
        void gencullplanes();           // generate culling planes for the mvp matrix
};

extern cascadedshadowmap csm;

extern int csmsplits, csmshadowmap;
extern float csmpolyoffset, csmpolyoffset2;
extern float csmpolyfactor, csmpolyfactor2;

#endif

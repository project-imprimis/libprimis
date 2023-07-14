#ifndef CSM_H_
#define CSM_H_

class cascadedshadowmap
{
    public:
        static const int csmmaxsplits = 8;

        enum CSMProp : int
        {
            MaxSize = 0,
            NearPlane,
            FarPlane,
            Cull,
            SplitWeight,
            PRadiusTweak,
            DepthRange,
            DepthMargin,
            Bias,
            Bias2,
            Splits,
            ShadowMap,
            PolyOffset,
            PolyOffset2,
            PolyFactor,
            PolyFactor2
        };

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
        int calcspherecsmsplits(const vec &center, float radius) const;
        bool setcsmproperty(int index, float value);
        float getcsmproperty(int index) const;

        cascadedshadowmap();

    private:
        void updatesplitdist();         // compute split frustum distances
        void getmodelmatrix();          // compute the shared model matrix
        void getprojmatrix();           // compute each cropped projection matrix
        void gencullplanes();           // generate culling planes for the mvp matrix

        int csmmaxsize,
            csmnearplane,
            csmfarplane,
            csmsplits;
        bool csmcull,
             csmshadowmap;
        float csmsplitweight,
              csmpradiustweak,
              csmdepthrange,
              csmdepthmargin,
              csmbias,
              csmbias2,
              csmpolyfactor,
              csmpolyfactor2,
              csmpolyoffset,
              csmpolyoffset2;
};

extern cascadedshadowmap csm;

#endif

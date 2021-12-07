#ifndef RENDERVA_H_
#define RENDERVA_H_

class shadowmesh
{
    public:
        vec origin;
        float radius;
        vec spotloc;
        int spotangle;
        int type;
        int draws[6];

        void rendershadowmesh();
};


class vfc
{
    public:
        int isfoggedcube(const ivec &o, int size);
        int isvisiblecube(const ivec &o, int size);
        void visiblecubes(bool cull = true);
        bool isfoggedsphere(float rad, const vec &cv);
        int isvisiblesphere(float rad, const vec &cv);
        int isvisiblebb(const ivec &bo, const ivec &br);
        int cullfrustumsides(const vec &lightpos, float lightradius, float size, float border);
    private:
        void calcvfcD();
        void setvfcP(const vec &bbmin = vec(-1, -1, -1), const vec &bbmax = vec(1, 1, 1));

        plane vfcP[5];  // perpindictular vectors to view frustrum bounding planes
        float vfcDfog;  // far plane culling distance (fog limit).
        float vfcDnear[5], //near plane culling
              vfcDfar[5];  //far plane culling
};

extern vfc view;

#endif

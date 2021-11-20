// renderva.cpp: handles the occlusion and rendering of vertex arrays

#include "engine.h"

#include "csm.h"
#include "grass.h"
#include "octarender.h"
#include "radiancehints.h"
#include "rendergl.h"
#include "rendermodel.h"
#include "renderwindow.h"
#include "rendersky.h"

#include "interface/control.h"
#include "world/octaedit.h"
#include "world/raycube.h"
#include "world/bih.h"

#include "model/model.h"

VAR(outline, 0, 0, 1); //vertex/edge highlighting in edit mode
VAR(oqfrags, 0, 8, 64); //occlusion query fragments
CVARP(outlinecolor, 0); //color of edit mode outlines
float alphafrontsx1   = -1,
      alphafrontsx2   =  1,
      alphafrontsy1   = -1,
      alphafrontsy2   = -1,
      alphabacksx1    = -1,
      alphabacksx2    =  1,
      alphabacksy1    = -1,
      alphabacksy2    = -1,
      alpharefractsx1 = -1,
      alpharefractsx2 =  1,
      alpharefractsy1 = -1,
      alpharefractsy2 =  1;

float shadowradius = 0,
      shadowbias = 0;
int shadowside = 0,
    shadowspot = 0;
vec shadoworigin(0, 0, 0),
    shadowdir(0, 0, 0);

uint alphatiles[lighttilemaxheight];
vtxarray *visibleva = nullptr;

int deferquery = 0;

struct shadowmesh
{
    vec origin;
    float radius;
    vec spotloc;
    int spotangle;
    int type;
    int draws[6];
};

/* internally relevant functionality */
///////////////////////////////////////

void findshadowvas(vector<vtxarray *> &vas);

namespace
{
    void drawtris(GLsizei numindices, const GLvoid *indices, ushort minvert, ushort maxvert)
    {
        glDrawRangeElements_(GL_TRIANGLES, minvert, maxvert, numindices, GL_UNSIGNED_SHORT, indices);
        glde++;
    }

    void drawvatris(vtxarray *va, GLsizei numindices, int offset)
    {
        drawtris(numindices, (ushort *)0 + va->eoffset + offset, va->minvert, va->maxvert);
    }

    void drawvaskytris(vtxarray *va)
    {
        drawtris(va->sky, (ushort *)0 + va->skyoffset, va->minvert, va->maxvert);
    }

    ///////// view frustrum culling ///////////////////////

    plane vfcP[5];  // perpindictular vectors to view frustrum bounding planes
    float vfcDfog;  // far plane culling distance (fog limit).
    float vfcDnear[5], //near plane culling
          vfcDfar[5];  //far plane culling


    int isfoggedcube(const ivec &o, int size)
    {
        for(int i = 0; i < 4; ++i)
        {
            if(o.dist(vfcP[i]) < -vfcDfar[i]*size)
            {
                return true;
            }
        }
        float dist = o.dist(vfcP[4]);
        return dist < -vfcDfar[4]*size || dist > vfcDfog - vfcDnear[4]*size;
    }

    int isvisiblecube(const ivec &o, int size)
    {
        int v = ViewFrustumCull_FullyVisible;
        float dist;

        for(int i = 0; i < 5; ++i)
        {
            dist = o.dist(vfcP[i]);
            if(dist < -vfcDfar[i]*size)
            {
                return ViewFrustumCull_NotVisible;
            }
            if(dist < -vfcDnear[i]*size)
            {
                v = ViewFrustumCull_PartlyVisible;
            }
        }

        dist -= vfcDfog;
        if(dist > -vfcDnear[4]*size)
        {
            return ViewFrustumCull_Fogged;
        }
        if(dist > -vfcDfar[4]*size)
        {
            v = ViewFrustumCull_PartlyVisible;
        }

        return v;
    }

    float vadist(vtxarray *va, const vec &p)
    {
        return p.dist_to_bb(va->bbmin, va->bbmax);
    }

    constexpr int vasortsize = 64;

    vtxarray *vasort[vasortsize];

    void addvisibleva(vtxarray *va)
    {
        float dist = vadist(va, camera1->o);
        va->distance = static_cast<int>(dist); /*cv.dist(camera1->o) - va->size*SQRT3/2*/

        int hash = std::clamp(static_cast<int>(dist*vasortsize/worldsize), 0, vasortsize-1);
        vtxarray **prev = &vasort[hash],
                  *cur = vasort[hash];

        while(cur && va->distance >= cur->distance)
        {
            prev = &cur->next;
            cur = cur->next;
        }

        va->next = cur;
        *prev = va;
    }

    void sortvisiblevas()
    {
        visibleva = nullptr;
        vtxarray **last = &visibleva;
        for(int i = 0; i < vasortsize; ++i)
        {
            if(vasort[i])
            {
                vtxarray *va = vasort[i];
                *last = va;
                while(va->next)
                {
                    va = va->next;
                }
                last = &va->next;
            }
        }
    }

    template<bool fullvis, bool resetocclude>
    void findvisiblevas(vector<vtxarray *> &vas)
    {
        for(int i = 0; i < vas.length(); i++)
        {
            vtxarray &v = *vas[i];
            int prevvfc = v.curvfc;
            v.curvfc = fullvis ? ViewFrustumCull_FullyVisible : isvisiblecube(v.o, v.size);
            if(v.curvfc != ViewFrustumCull_NotVisible)
            {
                bool resetchildren = prevvfc >= ViewFrustumCull_NotVisible || resetocclude;
                if(resetchildren)
                {
                    v.occluded = !v.texs ? Occlude_Geom : Occlude_Nothing;
                    v.query = nullptr;
                }
                addvisibleva(&v);
                if(v.children.length())
                {
                    if(fullvis || v.curvfc == ViewFrustumCull_FullyVisible)
                    {
                        if(resetchildren)
                        {
                            findvisiblevas<true, true>(v.children);
                        }
                        else
                        {
                            findvisiblevas<true, false>(v.children);
                        }
                    }
                    else if(resetchildren)
                    {
                        findvisiblevas<false, true>(v.children);
                    }
                    else
                    {
                        findvisiblevas<false, false>(v.children);
                    }
                }
            }
        }
    }

    void findvisiblevas()
    {
        memset(vasort, 0, sizeof(vasort));
        findvisiblevas<false, false>(varoot);
        sortvisiblevas();
    }

    void calcvfcD()
    {
        for(int i = 0; i < 5; ++i)
        {
            plane &p = vfcP[i];
            vfcDnear[i] = vfcDfar[i] = 0;
            for(int k = 0; k < 3; ++k)
            {
                if(p[k] > 0)
                {
                    vfcDfar[i] += p[k];
                }
                else
                {
                    vfcDnear[i] += p[k];
                }
            }
        }
    }

    plane oldvfcP[5];

    ///////// occlusion queries /////////////

    constexpr int maxquery = 2048,
                  maxqueryframes = 2;

    //all members of this struct are used elsewhere (must be public)
    struct queryframe
    {
        public:
            int cur;

            queryframe() : cur(0), max(0), defer(0) {}

            void flip()
            {
                for(int i = 0; i < cur; ++i)
                {
                    queries[i].owner = nullptr;
                }
                for(; defer > 0 && max < maxquery; defer--)
                {
                    queries[max].owner = nullptr;
                    queries[max].fragments = -1;
                    glGenQueries_(1, &queries[max++].id);
                }
                cur = defer = 0;
            }

            occludequery *newquery(void *owner)
            {
                if(cur >= max)
                {
                    if(max >= maxquery)
                    {
                        return nullptr;
                    }
                    if(deferquery)
                    {
                        if(max + defer < maxquery)
                        {
                            defer++;
                        }
                        return nullptr;
                    }
                    glGenQueries_(1, &queries[max++].id);
                }
                occludequery *query = &queries[cur++];
                query->owner = owner;
                query->fragments = -1;
                return query;
            }

            void reset()
            {
                for(int i = 0; i < max; ++i)
                {
                    queries[i].owner = nullptr;
                }
            }

            void cleanup()
            {
                for(int i = 0; i < max; ++i)
                {
                    glDeleteQueries_(1, &queries[i].id);
                    queries[i].owner = nullptr;
                }
                cur = max = defer = 0;
            }
        private:
            int max, defer;
            occludequery queries[maxquery];
    };

    queryframe queryframes[maxqueryframes];
    uint flipquery = 0;

    void clearqueries()
    {
        for(int i = 0; i < maxqueryframes; ++i)
        {
            queryframes[i].cleanup();
        }
    }

    VARF(oqany, 0, 0, 2, clearqueries()); //occlusion query settings: 0: GL_SAMPLES_PASSED, 1: GL_ANY_SAMPLES_PASSED, 2: GL_ANY_SAMPLES_PASSED_CONSERVATIVE
    VAR(oqwait, 0, 1, 1);

    GLenum querytarget()
    {
        return oqany ? (oqany > 1 && hasES3 ? GL_ANY_SAMPLES_PASSED_CONSERVATIVE : GL_ANY_SAMPLES_PASSED) : GL_SAMPLES_PASSED;
    }

    GLuint bbvbo = 0,
           bbebo = 0;

    void setupbb()
    {
        if(!bbvbo)
        {
            glGenBuffers_(1, &bbvbo);
            gle::bindvbo(bbvbo);
            vec verts[8];
            for(int i = 0; i < 8; ++i)
            {
                verts[i] = vec(i&1, (i>>1)&1, (i>>2)&1);
            }
            glBufferData_(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
            gle::clearvbo();
        }
        if(!bbebo)
        {
            glGenBuffers_(1, &bbebo);
            gle::bindebo(bbebo);
            GLushort tris[3*2*6];
            //======================================== GENFACEVERT GENFACEORIENT
            #define GENFACEORIENT(orient, v0, v1, v2, v3) do { \
                int offset = orient*3*2; \
                tris[offset + 0] = v0; \
                tris[offset + 1] = v1; \
                tris[offset + 2] = v2; \
                tris[offset + 3] = v0; \
                tris[offset + 4] = v2; \
                tris[offset + 5] = v3; \
            } while(0);
            #define GENFACEVERT(orient, vert, ox,oy,oz, rx,ry,rz) (ox | oy | oz)
            GENFACEVERTS(0, 1, 0, 2, 0, 4, , , , , , )
            #undef GENFACEORIENT
            #undef GENFACEVERT
            //==================================================================
            glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(tris), tris, GL_STATIC_DRAW);
            gle::clearebo();
        }
    }

    void cleanupbb()
    {
        if(bbvbo)
        {
            glDeleteBuffers_(1, &bbvbo);
            bbvbo = 0;
        }
        if(bbebo)
        {
            glDeleteBuffers_(1, &bbebo);
            bbebo = 0;
        }
    }

    octaentities *visiblemms,
                       **lastvisiblemms;

    void findvisiblemms(const vector<extentity *> &ents, bool doquery)
    {
        visiblemms = nullptr;
        lastvisiblemms = &visiblemms;
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(va->occluded < Occlude_BB && va->curvfc < ViewFrustumCull_Fogged)
            {
                for(int i = 0; i < va->mapmodels.length(); i++)
                {
                    octaentities *oe = va->mapmodels[i];
                    if(isfoggedcube(oe->o, oe->size))
                    {
                        continue;
                    }
                    bool occluded = doquery && oe->query && oe->query->owner == oe && checkquery(oe->query);
                    if(occluded)
                    {
                        oe->distance = -1;
                        oe->next = nullptr;
                        *lastvisiblemms = oe;
                        lastvisiblemms = &oe->next;
                    }
                    else
                    {
                        int visible = 0;
                        for(int i = 0; i < oe->mapmodels.length(); i++)
                        {
                            extentity &e = *ents[oe->mapmodels[i]];
                            if(e.flags&EntFlag_NoVis)
                            {
                                continue;
                            }
                            e.flags |= EntFlag_Render;
                            ++visible;
                        }
                        if(!visible)
                        {
                            continue;
                        }
                        oe->distance = static_cast<int>(camera1->o.dist_to_bb(oe->o, oe->size));

                        octaentities **prev = &visiblemms, *cur = visiblemms;
                        while(cur && cur->distance >= 0 && oe->distance > cur->distance)
                        {
                            prev = &cur->next;
                            cur = cur->next;
                        }

                        if(*prev == nullptr)
                        {
                            lastvisiblemms = &oe->next;
                        }
                        oe->next = *prev;
                        *prev = oe;
                    }
                }
            }
        }
    }

    VAR(oqmm, 0, 4, 8); //`o`cclusion `q`uery `m`ap `m`odel

    void rendermapmodel(extentity &e)
    {
        int anim = Anim_Mapmodel | Anim_Loop, basetime = 0;
        rendermapmodel(e.attr1, anim, e.o, e.attr2, e.attr3, e.attr4, Model_CullVFC | Model_CullDist, basetime, e.attr5 > 0 ? e.attr5/100.0f : 1.0f);
    }

    bool bbinsideva(const ivec &bo, const ivec &br, vtxarray *va)
    {
        return bo.x >= va->bbmin.x && bo.y >= va->bbmin.y && bo.z >= va->bbmin.z &&
            br.x <= va->bbmax.x && br.y <= va->bbmax.y && br.z <= va->bbmax.z;
    }

    bool bboccluded(const ivec &bo, const ivec &br, cube *c, const ivec &o, int size)
    {
        LOOP_OCTA_BOX(o, size, bo, br)
        {
            ivec co(i, o, size);
            if(c[i].ext && c[i].ext->va)
            {
                vtxarray *va = c[i].ext->va;
                if(va->curvfc >= ViewFrustumCull_Fogged || (va->occluded >= Occlude_BB && bbinsideva(bo, br, va)))
                {
                    continue;
                }
            }
            if(c[i].children && bboccluded(bo, br, c[i].children, co, size>>1))
            {
                continue;
            }
            return false;
        }
        return true;
    }

    VAR(dtoutline, 0, 1, 1); //`d`epth `t`est `outline`s

    int calcbbsidemask(const ivec &bbmin, const ivec &bbmax, const vec &lightpos, float lightradius, float bias)
    {
        vec pmin = vec(bbmin).sub(lightpos).div(lightradius),
            pmax = vec(bbmax).sub(lightpos).div(lightradius);
        int mask = 0x3F;
        float dp1 = pmax.x + pmax.y,
              dn1 = pmax.x - pmin.y,
              ap1 = std::fabs(dp1),
              an1 = std::fabs(dn1),
              dp2 = pmin.x + pmin.y,
              dn2 = pmin.x - pmax.y,
              ap2 = std::fabs(dp2),
              an2 = std::fabs(dn2);
        if(ap1 > bias*an1 && ap2 > bias*an2)
        {
            mask &= (3<<4)
                | (dp1 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
                | (dp2 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
        }
        if(an1 > bias*ap1 && an2 > bias*ap2)
        {
            mask &= (3<<4)
                | (dn1 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
                | (dn2 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));
        }
        dp1 = pmax.y + pmax.z,
        dn1 = pmax.y - pmin.z,
        ap1 = std::fabs(dp1),
        an1 = std::fabs(dn1),
        dp2 = pmin.y + pmin.z,
        dn2 = pmin.y - pmax.z,
        ap2 = std::fabs(dp2),
        an2 = std::fabs(dn2);
        if(ap1 > bias*an1 && ap2 > bias*an2)
        {
            mask &= (3<<0)
                | (dp1 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
                | (dp2 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
        }
        if(an1 > bias*ap1 && an2 > bias*ap2)
        {
            mask &= (3<<0)
                | (dn1 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
                | (dn2 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));
        }
        dp1 = pmax.z + pmax.x,
        dn1 = pmax.z - pmin.x,
        ap1 = std::fabs(dp1),
        an1 = std::fabs(dn1),
        dp2 = pmin.z + pmin.x,
        dn2 = pmin.z - pmax.x,
        ap2 = std::fabs(dp2),
        an2 = std::fabs(dn2);
        if(ap1 > bias*an1 && ap2 > bias*an2)
        {
            mask &= (3<<2)
                | (dp1 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
                | (dp2 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
        }
        if(an1 > bias*ap1 && an2 > bias*ap2)
        {
            mask &= (3<<2)
                | (dn1 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
                | (dn2 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));
        }
        return mask;
    }

    VAR(smbbcull, 0, 1, 1);
    VAR(smdistcull, 0, 1, 1);
    VAR(smnodraw, 0, 0, 1);

    vtxarray *shadowva = nullptr;

    void addshadowva(vtxarray *va, float dist)
    {
        va->rdistance = static_cast<int>(dist);

        int hash = std::clamp(static_cast<int>(dist*vasortsize/shadowradius), 0, vasortsize-1);
        vtxarray **prev = &vasort[hash], *cur = vasort[hash];

        while(cur && va->rdistance > cur->rdistance)
        {
            prev = &cur->rnext;
            cur = cur->rnext;
        }

        va->rnext = cur;
        *prev = va;
    }

    void sortshadowvas()
    {
        shadowva = nullptr;
        vtxarray **last = &shadowva;
        for(int i = 0; i < vasortsize; ++i)
        {
            if(vasort[i])
            {
                vtxarray *va = vasort[i];
                *last = va;
                while(va->rnext)
                {
                    va = va->rnext;
                }
                last = &va->rnext;
            }
        }
    }

    void findcsmshadowvas(vtxarray * v)
    {
        ivec bbmin, bbmax;
        if(v->children.length() || v->mapmodels.length())
        {
            bbmin = v->bbmin;
            bbmax = v->bbmax;
        }
        else
        {
            bbmin = v->geommin;
            bbmax = v->geommax;
        }
        v->shadowmask = calcbbcsmsplits(bbmin, bbmax);
        if(v->shadowmask)
        {
            float dist = shadowdir.project_bb(bbmin, bbmax) - shadowbias;
            addshadowva(v, dist);
            for(int i = 0; i < v->children.length(); ++i)
            {
                findcsmshadowvas(v->children[i]);
            }
        }
    }

    void findrsmshadowvas(vtxarray * v)
    {
        ivec bbmin, bbmax;
        if(v->children.length() || v->mapmodels.length())
        {
            bbmin = v->bbmin;
            bbmax = v->bbmax;
        }
        else
        {
            bbmin = v->geommin;
            bbmax = v->geommax;
        }
        v->shadowmask = calcbbrsmsplits(bbmin, bbmax);
        if(v->shadowmask)
        {
            float dist = shadowdir.project_bb(bbmin, bbmax) - shadowbias;
            addshadowva(v, dist);
            for(int i = 0; i < v->children.length(); ++i)
            {
                findrsmshadowvas(v->children[i]);
            }
        }
    }

    void findspotshadowvas(vtxarray * v)
    {
        float dist = vadist(v, shadoworigin);
        if(dist < shadowradius || !smdistcull)
        {
            v->shadowmask = !smbbcull || (v->children.length() || v->mapmodels.length() ?
                                bbinsidespot(shadoworigin, shadowdir, shadowspot, v->bbmin, v->bbmax) :
                                bbinsidespot(shadoworigin, shadowdir, shadowspot, v->geommin, v->geommax)) ? 1 : 0;
            addshadowva(v, dist);
            for(int i = 0; i < v->children.length(); ++i)
            {
                findspotshadowvas(v->children[i]);
            }
        }
    }

    octaentities *shadowmms = nullptr;

    struct renderstate
    {
        bool colormask, depthmask;
        int alphaing;
        GLuint vbuf;
        bool vattribs, vquery;
        vec colorscale;
        float alphascale;
        float refractscale;
        vec refractcolor;
        int globals, tmu;
        GLuint textures[7];
        Slot *slot, *texgenslot;
        VSlot *vslot, *texgenvslot;
        vec2 texgenscroll;
        int texgenorient, texgenmillis;

        renderstate() : colormask(true), depthmask(true), alphaing(0), vbuf(0), vattribs(false),
                        vquery(false), colorscale(1, 1, 1), alphascale(0), refractscale(0),
                        refractcolor(1, 1, 1), globals(-1), tmu(-1), slot(nullptr),
                        texgenslot(nullptr), vslot(nullptr), texgenvslot(nullptr),
                        texgenscroll(0, 0), texgenorient(-1), texgenmillis(lastmillis)
        {
            for(int k = 0; k < 7; ++k)
            {
                textures[k] = 0;
            }
        }
    };

    void disablevbuf(renderstate &cur)
    {
        gle::clearvbo();
        gle::clearebo();
        cur.vbuf = 0;
    }

    void enablevquery(renderstate &cur)
    {
        if(cur.colormask)
        {
            cur.colormask = false;
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }
        if(cur.depthmask)
        {
            cur.depthmask = false;
            glDepthMask(GL_FALSE);
        }
        startbb(false);
        cur.vquery = true;
    }

    void disablevquery(renderstate &cur)
    {
        endbb(false);
        cur.vquery = false;
    }

    void renderquery(renderstate &cur, occludequery *query, vtxarray *va, bool full = true)
    {
        if(!cur.vquery)
        {
            enablevquery(cur);
        }
        startquery(query);
        if(full)
        {
            drawbb(ivec(va->bbmin).sub(1), ivec(va->bbmax).sub(va->bbmin).add(2));
        }
        else
        {
            drawbb(va->geommin, ivec(va->geommax).sub(va->geommin));
        }
        endquery();
    }

    enum
    {
        RenderPass_GBuffer = 0,
        RenderPass_Z,
        RenderPass_Caustics,
        RenderPass_GBufferBlend,
        RenderPass_ReflectiveShadowMap,
        RenderPass_ReflectiveShadowMapBlend
    };

    struct geombatch
    {
        const elementset &es;
        VSlot &vslot;
        int offset;
        vtxarray *va;
        int next, batch;

        geombatch(const elementset &es, int offset, vtxarray *va)
          : es(es), vslot(lookupvslot(es.texture)), offset(offset), va(va),
            next(-1), batch(-1)
        {}

        int compare(const geombatch &b) const
        {
            if(va->vbuf < b.va->vbuf)
            {
                return -1;
            }
            if(va->vbuf > b.va->vbuf)
            {
                return 1;
            }
            if(es.layer&BlendLayer_Bottom)
            {
                if(!(b.es.layer&BlendLayer_Bottom))
                {
                    return 1;
                }
                int x1 = va->o.x&~0xFFF,
                    x2 = b.va->o.x&~0xFFF;
                if(x1 < x2)
                {
                    return -1;
                }
                if(x1 > x2)
                {
                    return 1;
                }
                int y1 = va->o.y&~0xFFF,
                    y2 = b.va->o.y&~0xFFF;
                if(y1 < y2)
                {
                    return -1;
                }
                if(y1 > y2)
                {
                    return 1;
                }
            }
            else if(b.es.layer&BlendLayer_Bottom)
            {
                return -1;
            }
            if(vslot.slot->shader < b.vslot.slot->shader)
            {
                return -1;
            }
            if(vslot.slot->shader > b.vslot.slot->shader)
            {
                return 1;
            }
            if(es.texture < b.es.texture)
            {
                return -1;
            }
            if(es.texture > b.es.texture)
            {
                return 1;
            }
            if(vslot.slot->params.length() < b.vslot.slot->params.length())
            {
                return -1;
            }
            if(vslot.slot->params.length() > b.vslot.slot->params.length())
            {
                return 1;
            }
            if(es.orient < b.es.orient)
            {
                return -1;
            }
            if(es.orient > b.es.orient)
            {
                return 1;
            }
            return 0;
        }
    };

    vector<geombatch> geombatches;
    int firstbatch = -1,
               numbatches = 0;

    void mergetexs(renderstate &cur, vtxarray *va, elementset *texs = nullptr, int offset = 0)
    {
        int numtexs;
        if(!texs)
        {
            texs = va->texelems;
            numtexs = va->texs;
            if(cur.alphaing)
            {
                texs += va->texs;
                offset += 3*(va->tris);
                numtexs = va->alphaback;
                if(cur.alphaing > 1)
                {
                    numtexs += va->alphafront + va->refract;
                }
            }
        }

        if(firstbatch < 0)
        {
            firstbatch = geombatches.length();
            numbatches = numtexs;
            for(int i = 0; i < numtexs-1; ++i)
            {
                geombatches.add(geombatch(texs[i], offset, va)).next = i+1;
                offset += texs[i].length;
            }
            geombatches.add(geombatch(texs[numtexs-1], offset, va));
            return;
        }

        int prevbatch = -1,
            curbatch = firstbatch,
            curtex = 0;
        do
        {
            geombatch &b = geombatches.add(geombatch(texs[curtex], offset, va));
            offset += texs[curtex].length;
            int dir = -1;
            while(curbatch >= 0)
            {
                dir = b.compare(geombatches[curbatch]);
                if(dir <= 0)
                {
                    break;
                }
                prevbatch = curbatch;
                curbatch = geombatches[curbatch].next;
            }
            if(!dir)
            {
                int last = curbatch, next;
                for(;;)
                {
                    next = geombatches[last].batch;
                    if(next < 0)
                    {
                        break;
                    }
                    last = next;
                }
                if(last==curbatch)
                {
                    b.batch = curbatch;
                    b.next = geombatches[curbatch].next;
                    if(prevbatch < 0)
                    {
                        firstbatch = geombatches.length()-1;
                    }
                    else
                    {
                        geombatches[prevbatch].next = geombatches.length()-1;
                    }
                    curbatch = geombatches.length()-1;
                }
                else
                {
                    b.batch = next;
                    geombatches[last].batch = geombatches.length()-1;
                }
            }
            else
            {
                numbatches++;
                b.next = curbatch;
                if(prevbatch < 0)
                {
                    firstbatch = geombatches.length()-1;
                }
                else
                {
                    geombatches[prevbatch].next = geombatches.length()-1;
                }
                prevbatch = geombatches.length()-1;
            }
        } while(++curtex < numtexs);
    }

    void enablevattribs(renderstate &cur, bool all = true)
    {
        gle::enablevertex();
        if(all)
        {
            gle::enabletexcoord0();
            gle::enablenormal();
            gle::enabletangent();
        }
        cur.vattribs = true;
    }

    void disablevattribs(renderstate &cur, bool all = true)
    {
        gle::disablevertex();
        if(all)
        {
            gle::disabletexcoord0();
            gle::disablenormal();
            gle::disabletangent();
        }
        cur.vattribs = false;
    }

    void changevbuf(renderstate &cur, int pass, vtxarray *va)
    {
        gle::bindvbo(va->vbuf);
        gle::bindebo(va->ebuf);
        cur.vbuf = va->vbuf;

        vertex *vdata = nullptr;
        gle::vertexpointer(sizeof(vertex), vdata->pos.v);

        if(pass==RenderPass_GBuffer || pass==RenderPass_ReflectiveShadowMap)
        {
            gle::normalpointer(sizeof(vertex), vdata->norm.v, GL_BYTE);
            gle::texcoord0pointer(sizeof(vertex), vdata->tc.v);
            gle::tangentpointer(sizeof(vertex), vdata->tangent.v, GL_BYTE);
        }
    }

    void changebatchtmus(renderstate &cur)
    {
        if(cur.tmu != 0)
        {
            cur.tmu = 0;
            glActiveTexture_(GL_TEXTURE0);
        }
    }

    void bindslottex(renderstate &cur, int type, Texture *tex, GLenum target = GL_TEXTURE_2D)
    {
        if(cur.textures[type] != tex->id)
        {
            if(cur.tmu != type)
            {
                cur.tmu = type;
                glActiveTexture_(GL_TEXTURE0 + type);
            }
            glBindTexture(target, cur.textures[type] = tex->id);
        }
    }

    void changeslottmus(renderstate &cur, int pass, Slot &slot, VSlot &vslot)
    {
        Texture *diffuse = slot.sts.empty() ? notexture : slot.sts[0].t;
        if(pass==RenderPass_GBuffer || pass==RenderPass_ReflectiveShadowMap)
        {
            bindslottex(cur, Tex_Diffuse, diffuse);

            if(pass == RenderPass_GBuffer)
            {
                if(msaasamples)
                {
                    GLOBALPARAMF(hashid, vslot.index);
                }
                if(slot.shader->type & Shader_Triplanar)
                {
                    float scale = defaulttexscale/vslot.scale;
                    GLOBALPARAMF(texgenscale, scale/diffuse->xs, scale/diffuse->ys);
                }
            }
        }

        if(cur.alphaing)
        {
            float alpha = cur.alphaing > 1 ? vslot.alphafront : vslot.alphaback;
            if(cur.alphascale != alpha)
            {
                cur.alphascale = alpha;
                cur.refractscale = 0;
                goto changecolorparams; //also run next if statement
            }
            if(cur.colorscale != vslot.colorscale)
            {
            changecolorparams:
                cur.colorscale = vslot.colorscale;
                GLOBALPARAMF(colorparams,
                             alpha*vslot.colorscale.x,
                             alpha*vslot.colorscale.y,
                             alpha*vslot.colorscale.z,
                             alpha);
            }
            if(cur.alphaing > 1 && vslot.refractscale > 0 &&
                  (cur.refractscale != vslot.refractscale || cur.refractcolor != vslot.refractcolor))
            {
                cur.refractscale = vslot.refractscale;
                cur.refractcolor = vslot.refractcolor;
                float refractscale = 0.5f/ldrscale*(1-alpha);
                GLOBALPARAMF(refractparams,
                             vslot.refractcolor.x*refractscale,
                             vslot.refractcolor.y*refractscale,
                             vslot.refractcolor.z*refractscale,
                             vslot.refractscale*viewh);
            }
        }
        else if(cur.colorscale != vslot.colorscale)
        {
            cur.colorscale = vslot.colorscale;
            GLOBALPARAMF(colorparams, vslot.colorscale.x, vslot.colorscale.y, vslot.colorscale.z, 1);
        }

        for(int j = 0; j < slot.sts.length(); j++)
        {
            Slot::Tex &t = slot.sts[j];
            switch(t.type)
            {
                case Tex_Normal:
                case Tex_Glow:
                {
                    bindslottex(cur, t.type, t.t);
                    break;
                }
            }
        }
        GLOBALPARAM(rotate, vec(vslot.angle.y, vslot.angle.z, diffuse->ratio));
        if(cur.tmu != 0)
        {
            cur.tmu = 0;
            glActiveTexture_(GL_TEXTURE0);
        }

        cur.slot = &slot;
        cur.vslot = &vslot;
    }

    void changetexgen(renderstate &cur, int orient, Slot &slot, VSlot &vslot)
    {
        if(cur.texgenslot != &slot || cur.texgenvslot != &vslot)
        {
            Texture *curtex = !cur.texgenslot || cur.texgenslot->sts.empty() ? notexture : cur.texgenslot->sts[0].t,
                    *tex = slot.sts.empty() ? notexture : slot.sts[0].t;
            if(!cur.texgenvslot || slot.sts.empty() ||
                (curtex->xs != tex->xs || curtex->ys != tex->ys ||
                 cur.texgenvslot->rotation != vslot.rotation || cur.texgenvslot->scale != vslot.scale ||
                 cur.texgenvslot->offset != vslot.offset || cur.texgenvslot->scroll != vslot.scroll) ||
                 cur.texgenvslot->angle != vslot.angle)
            {
                const texrotation &r = texrotations[vslot.rotation];
                float xs = r.flipx ? -tex->xs : tex->xs,
                      ys = r.flipy ? -tex->ys : tex->ys;
                vec2 scroll(vslot.scroll);
                if(r.swapxy)
                {
                    std::swap(scroll.x, scroll.y);
                }
                scroll.x *= cur.texgenmillis*tex->xs/xs;
                scroll.y *= cur.texgenmillis*tex->ys/ys;
                if(cur.texgenscroll != scroll)
                {
                    cur.texgenscroll = scroll;
                    cur.texgenorient = -1;
                }
            }
            cur.texgenslot = &slot;
            cur.texgenvslot = &vslot;
        }

        if(cur.texgenorient == orient)
        {
            return;
        }
        GLOBALPARAM(texgenscroll, cur.texgenscroll);

        cur.texgenorient = orient;
    }

    void changeshader(renderstate &cur, int pass, geombatch &b)
    {
        VSlot &vslot = b.vslot;
        Slot &slot = *vslot.slot;
        if(pass == RenderPass_ReflectiveShadowMap)
        {
            if(b.es.layer&BlendLayer_Bottom)
            {
                rsmworldshader->setvariant(0, 0, slot, vslot);
            }
            else
            {
                rsmworldshader->set(slot, vslot);
            }
        }
        else if(cur.alphaing)
        {
            slot.shader->setvariant(cur.alphaing > 1 && vslot.refractscale > 0 ? 1 : 0, 1, slot, vslot);
        }
        else if(b.es.layer&BlendLayer_Bottom)
        {
            slot.shader->setvariant(0, 0, slot, vslot);
        }
        else
        {
            slot.shader->set(slot, vslot);
        }
        cur.globals = GlobalShaderParamState::nextversion;
    }

    template<class T>
    void updateshader(T &cur)
    {
        if(cur.globals != GlobalShaderParamState::nextversion)
        {
            if(Shader::lastshader)
            {
                Shader::lastshader->flushparams();
            }
            cur.globals = GlobalShaderParamState::nextversion;
        }
    }

    void renderbatch(geombatch &b)
    {
        gbatches++;
        for(geombatch *curbatch = &b;; curbatch = &geombatches[curbatch->batch])
        {
            ushort len = curbatch->es.length;
            if(len)
            {
                drawtris(len, (ushort *)0 + curbatch->va->eoffset + curbatch->offset, curbatch->es.minvert, curbatch->es.maxvert);
                vtris += len/3;
            }
            if(curbatch->batch < 0)
            {
                break;
            }
        }
    }

    void resetbatches()
    {
        geombatches.setsize(0);
        firstbatch = -1;
        numbatches = 0;
    }

    void renderbatches(renderstate &cur, int pass)
    {
        cur.slot = nullptr;
        cur.vslot = nullptr;
        int curbatch = firstbatch;
        if(curbatch >= 0)
        {
            if(!cur.depthmask)
            {
                cur.depthmask = true;
                glDepthMask(GL_TRUE);
            }
            if(!cur.colormask)
            {
                cur.colormask = true;
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            }
            if(!cur.vattribs)
            {
                if(cur.vquery)
                {
                    disablevquery(cur);
                }
                enablevattribs(cur);
            }
        }
        while(curbatch >= 0)
        {
            geombatch &b = geombatches[curbatch];
            curbatch = b.next;

            if(cur.vbuf != b.va->vbuf)
            {
                changevbuf(cur, pass, b.va);
            }
            if(pass == RenderPass_GBuffer || pass == RenderPass_ReflectiveShadowMap)
            {
                changebatchtmus(cur);
            }
            if(cur.vslot != &b.vslot)
            {
                changeslottmus(cur, pass, *b.vslot.slot, b.vslot);
                if(cur.texgenorient != b.es.orient || (cur.texgenorient < Orient_Any && cur.texgenvslot != &b.vslot))
                {
                    changetexgen(cur, b.es.orient, *b.vslot.slot, b.vslot);
                }
                changeshader(cur, pass, b);
            }
            else
            {
                if(cur.texgenorient != b.es.orient)
                {
                    changetexgen(cur, b.es.orient, *b.vslot.slot, b.vslot);
                }
                updateshader(cur);
            }

            renderbatch(b);
        }

        resetbatches();
    }

    void renderzpass(renderstate &cur, vtxarray *va)
    {
        if(!cur.vattribs)
        {
            if(cur.vquery)
            {
                disablevquery(cur);
            }
            enablevattribs(cur, false);
        }
        if(cur.vbuf!=va->vbuf)
        {
            changevbuf(cur, RenderPass_Z, va);
        }
        if(!cur.depthmask)
        {
            cur.depthmask = true;
            glDepthMask(GL_TRUE);
        }
        if(cur.colormask)
        {
            cur.colormask = false;
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }
        int firsttex = 0,
            numtris = va->tris,
            offset = 0;
        if(cur.alphaing)
        {
            firsttex += va->texs;
            offset += 3*(va->tris);
            numtris = va->alphabacktris + va->alphafronttris + va->refracttris;
            xtravertsva += 3*numtris;
        }
        else
        {
            xtravertsva += va->verts;
        }
        nocolorshader->set();
        drawvatris(va, 3*numtris, offset);
    }
//====================================================== STARTVAQUERY ENDVAQUERY
    #define STARTVAQUERY(va, flush) \
        do { \
            if(va->query) \
            { \
                flush; \
                startquery(va->query); \
            } \
        } while(0)


    #define ENDVAQUERY(va, flush) \
        do { \
            if(va->query) \
            { \
                flush; \
                endquery(); \
            } \
        } while(0)

    VAR(batchgeom, 0, 1, 1);

    void renderva(renderstate &cur, vtxarray *va, int pass = RenderPass_GBuffer, bool doquery = false)
    {
        switch(pass)
        {
            case RenderPass_GBuffer:
                if(!cur.alphaing)
                {
                    vverts += va->verts;
                }
                if(doquery)
                {
                    STARTVAQUERY(va, { if(geombatches.length()) renderbatches(cur, pass); });
                }
                mergetexs(cur, va);
                if(doquery)
                {
                    ENDVAQUERY(va, { if(geombatches.length()) renderbatches(cur, pass); });
                }
                else if(!batchgeom && geombatches.length())
                {
                    renderbatches(cur, pass);
                }
                break;

            case RenderPass_GBufferBlend:
                if(doquery)
                {
                    STARTVAQUERY(va, { if(geombatches.length()) renderbatches(cur, RenderPass_GBuffer); });
                }
                mergetexs(cur, va, &va->texelems[va->texs], 3*va->tris);
                if(doquery)
                {
                    ENDVAQUERY(va, { if(geombatches.length()) renderbatches(cur, RenderPass_GBuffer); });
                }
                else if(!batchgeom && geombatches.length())
                {
                    renderbatches(cur, RenderPass_GBuffer);
                }
                break;

            case RenderPass_Caustics:
                if(!cur.vattribs)
                {
                    enablevattribs(cur, false);
                }
                if(cur.vbuf!=va->vbuf)
                {
                    changevbuf(cur, pass, va);
                }
                drawvatris(va, 3*va->tris, 0);
                xtravertsva += va->verts;
                break;

            case RenderPass_Z:
                if(doquery)
                {
                    STARTVAQUERY(va, );
                }
                renderzpass(cur, va);
                if(doquery)
                {
                    ENDVAQUERY(va, );
                }
                break;

            case RenderPass_ReflectiveShadowMap:
                mergetexs(cur, va);
                if(!batchgeom && geombatches.length())
                {
                    renderbatches(cur, pass);
                }
                break;

            case RenderPass_ReflectiveShadowMapBlend:
                mergetexs(cur, va, &va->texelems[va->texs], 3*va->tris);
                if(!batchgeom && geombatches.length())
                {
                    renderbatches(cur, RenderPass_ReflectiveShadowMap);
                }
                break;
        }
    }

    #undef STARTVAQUERY
    #undef ENDVAQUERY
//==============================================================================
    void setupgeom()
    {
        glActiveTexture_(GL_TEXTURE0);
        GLOBALPARAMF(colorparams, 1, 1, 1, 1);
    }

    void cleanupgeom(renderstate &cur)
    {
        if(cur.vattribs)
        {
            disablevattribs(cur);
        }
        if(cur.vbuf)
        {
            disablevbuf(cur);
        }
    }

    VAR(oqgeom, 0, 1, 1); //occlusion query geometry

    std::vector<vtxarray *> alphavas;
    int alphabackvas    = 0,
        alpharefractvas = 0;

    CVARP(explicitskycolor, 0x800080);

    struct decalrenderer
    {
        GLuint vbuf;
        vec colorscale;
        int globals, tmu;
        GLuint textures[7];
        DecalSlot *slot;

        decalrenderer() : vbuf(0), colorscale(1, 1, 1), globals(-1), tmu(-1), slot(nullptr)
        {
            for(int i = 0; i < 7; ++i)
            {
                textures[i] = 0;
            }
        }
    };

    struct decalbatch
    {
        const elementset &es;
        DecalSlot &slot;
        int offset;
        vtxarray *va;
        int next, batch;

        decalbatch(const elementset &es, int offset, vtxarray *va)
          : es(es), slot(lookupdecalslot(es.texture)), offset(offset), va(va),
            next(-1), batch(-1)
        {}

        int compare(const decalbatch &b) const
        {
            if(va->vbuf < b.va->vbuf)
            {
                return -1;
            }
            if(va->vbuf > b.va->vbuf)
            {
                return 1;
            }
            if(slot.shader < b.slot.shader)
            {
                return -1;
            }
            if(slot.shader > b.slot.shader)
            {
                return 1;
            }
            if(es.texture < b.es.texture)
            {
                return -1;
            }
            if(es.texture > b.es.texture)
            {
                return 1;
            }
            if(slot.Slot::params.length() < b.slot.Slot::params.length())
            {
                return -1;
            }
            if(slot.Slot::params.length() > b.slot.Slot::params.length())
            {
                return 1;
            }
            if(es.reuse < b.es.reuse)
            {
                return -1;
            }
            if(es.reuse > b.es.reuse)
            {
                return 1;
            }
            return 0;
        }
    };

    std::vector<decalbatch> decalbatches;

    void mergedecals(vtxarray *va)
    {
        elementset *texs = va->decalelems;
        int numtexs = va->decaltexs,
            offset  = 0;

        if(firstbatch < 0)
        {
            firstbatch = decalbatches.size();
            numbatches = numtexs;
            for(int i = 0; i < numtexs-1; ++i)
            {
                decalbatches.emplace_back(decalbatch(texs[i], offset, va));
                decalbatches.back().next = i+1;
                offset += texs[i].length;
            }
            decalbatches.emplace_back(decalbatch(texs[numtexs-1], offset, va));
            return;
        }

        int prevbatch = -1,
            curbatch = firstbatch,
            curtex = 0;
        do
        {
            decalbatch b = decalbatch(texs[curtex], offset, va);
            offset += texs[curtex].length;
            int dir = -1;
            while(curbatch >= 0)
            {
                dir = b.compare(decalbatches[curbatch]);
                if(dir <= 0)
                {
                    break;
                }
                prevbatch = curbatch;
                curbatch = decalbatches[curbatch].next;
            }
            if(!dir)
            {
                int last = curbatch, next;
                for(;;)
                {
                    next = decalbatches[last].batch;
                    if(next < 0)
                    {
                        break;
                    }
                    last = next;
                }
                if(last==curbatch)
                {
                    b.batch = curbatch;
                    b.next = decalbatches[curbatch].next;
                    if(prevbatch < 0)
                    {
                        firstbatch = decalbatches.size()-1;
                    }
                    else
                    {
                        decalbatches[prevbatch].next = decalbatches.size()-1;
                    }
                    curbatch = decalbatches.size()-1;
                }
                else
                {
                    b.batch = next;
                    decalbatches[last].batch = decalbatches.size()-1;
                }
            }
            else
            {
                numbatches++;
                b.next = curbatch;
                if(prevbatch < 0)
                {
                    firstbatch = decalbatches.size()-1;
                }
                else
                {
                    decalbatches[prevbatch].next = decalbatches.size()-1;
                }
                prevbatch = decalbatches.size()-1;
            }
            decalbatches.push_back(b);
        } while(++curtex < numtexs);
    }

    void resetdecalbatches()
    {
        decalbatches.clear();
        firstbatch = -1;
        numbatches = 0;
    }

    void changevbuf(decalrenderer &cur, vtxarray *va)
    {
        gle::bindvbo(va->vbuf);
        gle::bindebo(va->decalbuf);
        cur.vbuf = va->vbuf;
        vertex *vdata = nullptr;
        gle::vertexpointer(sizeof(vertex), vdata->pos.v);
        gle::normalpointer(sizeof(vertex), vdata->norm.v, GL_BYTE, 4);
        gle::texcoord0pointer(sizeof(vertex), vdata->tc.v, GL_FLOAT, 3);
        gle::tangentpointer(sizeof(vertex), vdata->tangent.v, GL_BYTE);
    }

    void changebatchtmus(decalrenderer &cur)
    {
        if(cur.tmu != 0)
        {
            cur.tmu = 0;
            glActiveTexture_(GL_TEXTURE0);
        }
    }

    void bindslottex(decalrenderer &cur, int type, Texture *tex, GLenum target = GL_TEXTURE_2D)
    {
        if(cur.textures[type] != tex->id)
        {
            if(cur.tmu != type)
            {
                cur.tmu = type;
                glActiveTexture_(GL_TEXTURE0 + type);
            }
            glBindTexture(target, cur.textures[type] = tex->id);
        }
    }

    void changeslottmus(decalrenderer &cur, DecalSlot &slot)
    {
        Texture *diffuse = slot.sts.empty() ? notexture : slot.sts[0].t;
        bindslottex(cur, Tex_Diffuse, diffuse);
        for(int i = 0; i < slot.sts.length(); i++)
        {
            Slot::Tex &t = slot.sts[i];
            switch(t.type)
            {
                case Tex_Normal:
                case Tex_Glow:
                {
                    bindslottex(cur, t.type, t.t);
                    break;
                }
                case Tex_Spec:
                {
                    if(t.combined < 0)
                    {
                        bindslottex(cur, Tex_Glow, t.t);
                    }
                    break;
                }
            }
        }
        if(cur.tmu != 0)
        {
            cur.tmu = 0;
            glActiveTexture_(GL_TEXTURE0);
        }
        if(cur.colorscale != slot.colorscale)
        {
            cur.colorscale = slot.colorscale;
            GLOBALPARAMF(colorparams, slot.colorscale.x, slot.colorscale.y, slot.colorscale.z, 1);
        }
        cur.slot = &slot;
    }

    void changeshader(decalrenderer &cur, int pass, decalbatch &b)
    {
        DecalSlot &slot = b.slot;
        if(b.es.reuse)
        {
            VSlot &reuse = lookupvslot(b.es.reuse);
            if(pass)
            {
                slot.shader->setvariant(0, 0, slot, reuse);
            }
            else
            {
                slot.shader->set(slot, reuse);
            }
        }
        else if(pass)
        {
            slot.shader->setvariant(0, 0, slot);
        }
        else
        {
            slot.shader->set(slot);
        }
        cur.globals = GlobalShaderParamState::nextversion;
    }

    void renderdecalbatch(decalbatch &b)
    {
        gbatches++;
        for(decalbatch *curbatch = &b;; curbatch = &decalbatches[curbatch->batch])
        {
            ushort len = curbatch->es.length;
            if(len)
            {
                drawtris(len, reinterpret_cast<ushort *>(curbatch->va->decaloffset) + curbatch->offset, curbatch->es.minvert, curbatch->es.maxvert);
                vtris += len/3;
            }
            if(curbatch->batch < 0)
            {
                break;
            }
        }
    }

    void renderdecalbatches(decalrenderer &cur, int pass)
    {
        cur.slot = nullptr;
        int curbatch = firstbatch;
        while(curbatch >= 0)
        {
            decalbatch &b = decalbatches[curbatch];
            curbatch = b.next;

            if(pass && !b.slot.shader->numvariants(0))
            {
                continue;
            }
            if(cur.vbuf != b.va->vbuf)
            {
                changevbuf(cur, b.va);
            }
            changebatchtmus(cur);
            if(cur.slot != &b.slot)
            {
                changeslottmus(cur, b.slot);
                changeshader(cur, pass, b);
            }
            else
            {
                updateshader(cur);
            }

            renderdecalbatch(b);
        }

        resetdecalbatches();
    }

    void setupdecals()
    {
        gle::enablevertex();
        gle::enablenormal();
        gle::enabletexcoord0();
        gle::enabletangent();

        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        enablepolygonoffset(GL_POLYGON_OFFSET_FILL);

        GLOBALPARAMF(colorparams, 1, 1, 1, 1);
    }

    void cleanupdecals()
    {
        disablepolygonoffset(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        maskgbuffer("cnd");

        gle::disablevertex();
        gle::disablenormal();
        gle::disabletexcoord0();
        gle::disabletangent();

        gle::clearvbo();
        gle::clearebo();
    }

    VAR(batchdecals, 0, 1, 1);

    struct shadowdraw
    {
        GLuint ebuf, vbuf;
        int offset, tris, next;
        ushort minvert, maxvert;
    };

    struct shadowverts
    {
        static const int tablesize = 1<<13;
        int table[tablesize];
        std::vector<vec> verts;
        std::vector<int> chain;

        shadowverts() { clear(); }

        void clear()
        {
            memset(table, -1, sizeof(table));
            chain.clear();
            verts.clear();
        }

        int add(const vec &v)
        {
            uint h = hthash(v)&(tablesize-1);
            for(int i = table[h]; i>=0; i = chain[i])
            {
                if(verts[i] == v)
                {
                    return i;
                }
            }
            if(verts.size() >= USHRT_MAX)
            {
                return -1;
            }
            verts.push_back(v);
            chain.emplace_back(table[h]);
            return table[h] = verts.size()-1;
        }
    } shadowverts;
    std::vector<ushort> shadowtris[6];
    std::vector<GLuint> shadowvbos;
    hashtable<int, shadowmesh> shadowmeshes;
    std::vector<shadowdraw> shadowdraws;

    struct shadowdrawinfo
    {
        int last;
        ushort minvert, maxvert;

        shadowdrawinfo() : last(-1)
        {
            reset();
        }

        void reset()
        {
            minvert = USHRT_MAX;
            maxvert = 0;
        }
    };

    void flushshadowmeshdraws(shadowmesh &m, int sides, shadowdrawinfo draws[6])
    {
        int numindexes = 0;
        for(int i = 0; i < sides; ++i)
        {
            numindexes += shadowtris[i].size();
        }
        if(!numindexes)
        {
            return;
        }

        GLuint ebuf = 0, vbuf = 0;
        glGenBuffers_(1, &ebuf);
        glGenBuffers_(1, &vbuf);
        ushort *indexes = new ushort[numindexes];
        int offset = 0;
        for(int i = 0; i < sides; ++i)
        {
            if(shadowtris[i].size())
            {
                if(draws[i].last < 0)
                {
                    m.draws[i] = shadowdraws.size();
                }
                else
                {
                    shadowdraws[draws[i].last].next = shadowdraws.size();
                }
                draws[i].last = shadowdraws.size();

                shadowdraw d;
                d.ebuf = ebuf;
                d.vbuf = vbuf;
                d.offset = offset;
                d.tris = shadowtris[i].size()/3;
                d.minvert = draws[i].minvert;
                d.maxvert = draws[i].maxvert;
                d.next = -1;
                shadowdraws.push_back(d);

                memcpy(indexes + offset, shadowtris[i].data(), shadowtris[i].size()*sizeof(ushort));
                offset += shadowtris[i].size();

                shadowtris[i].clear();
                draws[i].reset();
            }
        }

        gle::bindebo(ebuf);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, numindexes*sizeof(ushort), indexes, GL_STATIC_DRAW);
        gle::clearebo();
        delete[] indexes;

        gle::bindvbo(vbuf);
        glBufferData_(GL_ARRAY_BUFFER, shadowverts.verts.size()*sizeof(vec), shadowverts.verts.data(), GL_STATIC_DRAW);
        gle::clearvbo();
        shadowverts.clear();

        shadowvbos.push_back(ebuf);
        shadowvbos.push_back(vbuf);
    }

    void addshadowmeshtri(shadowmesh &m, int sides, shadowdrawinfo draws[6], const vec &v0, const vec &v1, const vec &v2)
    {
        vec l0 = vec(v0).sub(shadoworigin);
        float side = l0.scalartriple(vec(v1).sub(v0), vec(v2).sub(v0));
        if(smcullside ? side > 0 : side < 0)
        {
            return;
        }
        vec l1 = vec(v1).sub(shadoworigin),
            l2 = vec(v2).sub(shadoworigin);
        if(l0.squaredlen() > shadowradius*shadowradius && l1.squaredlen() > shadowradius*shadowradius && l2.squaredlen() > shadowradius*shadowradius)
        {
            return;
        }
        int sidemask = 0;
        switch(m.type)
        {
            case ShadowMap_Spot:
            {
                sidemask = bbinsidespot(shadoworigin, shadowdir, shadowspot, ivec(vec(v0).min(v1).min(v2)), ivec(vec(v0).max(v1).max(v2).add(1))) ? 1 : 0;
                break;
            }
            case ShadowMap_CubeMap:
            {
                sidemask = calctrisidemask(l0.div(shadowradius), l1.div(shadowradius), l2.div(shadowradius), shadowbias);
                break;
            }
        }
        if(!sidemask)
        {
            return;
        }
        if(shadowverts.verts.size() + 3 >= USHRT_MAX)
        {
            flushshadowmeshdraws(m, sides, draws);
        }
        int i0 = shadowverts.add(v0),
            i1 = shadowverts.add(v1),
            i2 = shadowverts.add(v2);
        ushort minvert = std::min(i0, std::min(i1, i2)),
               maxvert = std::max(i0, std::max(i1, i2));
        for(int k = 0; k < sides; ++k)
        {
            if(sidemask&(1<<k))
            {
                shadowdrawinfo &d = draws[k];
                d.minvert = std::min(d.minvert, minvert);
                d.maxvert = std::max(d.maxvert, maxvert);
                shadowtris[k].push_back(i0);
                shadowtris[k].push_back(i1);
                shadowtris[k].push_back(i2);
            }
        }
    }

    void genshadowmeshtris(shadowmesh &m, int sides, shadowdrawinfo draws[6], ushort *edata, int numtris, vertex *vdata)
    {
        for(int j = 0; j < 3*numtris; j += 3)
        {
            addshadowmeshtri(m, sides, draws, vdata[edata[j]].pos, vdata[edata[j+1]].pos, vdata[edata[j+2]].pos);
        }
    }

    void genshadowmeshmapmodels(shadowmesh &m, int sides, shadowdrawinfo draws[6])
    {
        const vector<extentity *> &ents = entities::getents();
        for(octaentities *oe = shadowmms; oe; oe = oe->rnext)
        {
            for(int k = 0; k < oe->mapmodels.length(); k++)
            {
                extentity &e = *ents[oe->mapmodels[k]];
                if(e.flags&(EntFlag_NoVis|EntFlag_NoShadow))
                {
                    continue;
                }
                e.flags |= EntFlag_Render;
            }
        }
        std::vector<triangle> tris;
        for(octaentities *oe = shadowmms; oe; oe = oe->rnext)
        {
            for(int j = 0; j < oe->mapmodels.length(); j++)
            {
                extentity &e = *ents[oe->mapmodels[j]];
                if(!(e.flags&EntFlag_Render))
                {
                    continue;
                }
                e.flags &= ~EntFlag_Render;
                model *mm = loadmapmodel(e.attr1);
                if(!mm || !mm->shadow || mm->animated() || (mm->alphashadow && mm->alphatested()))
                {
                    continue;
                }
                matrix4x3 orient;
                orient.identity();
                if(e.attr2)
                {
                    orient.rotate_around_z(sincosmod360(e.attr2));
                }
                if(e.attr3)
                {
                    orient.rotate_around_x(sincosmod360(e.attr3));
                }
                if(e.attr4)
                {
                    orient.rotate_around_y(sincosmod360(-e.attr4));
                }
                if(e.attr5 > 0)
                {
                    orient.scale(e.attr5/100.0f);
                }
                orient.settranslation(e.o);
                tris.clear();
                mm->genshadowmesh(tris, orient);

                for(uint i = 0; i < tris.size(); i++)
                {
                    triangle &t = tris[i];
                    addshadowmeshtri(m, sides, draws, t.a, t.b, t.c);
                }

                e.flags |= EntFlag_ShadowMesh;
            }
        }
    }

    void genshadowmesh(int idx, extentity &e)
    {
        shadowmesh m;
        m.type = calcshadowinfo(e, m.origin, m.radius, m.spotloc, m.spotangle, shadowbias);
        if(!m.type)
        {
            return;
        }
        memset(m.draws, -1, sizeof(m.draws));

        shadowmapping = m.type;
        shadoworigin = m.origin;
        shadowradius = m.radius;
        shadowdir = m.type == ShadowMap_Spot ? vec(m.spotloc).sub(m.origin).normalize() : vec(0, 0, 0);
        shadowspot = m.spotangle;

        findshadowvas();
        findshadowmms();

        int sides = m.type == ShadowMap_Spot ? 1 : 6;
        shadowdrawinfo draws[6];
        for(vtxarray *va = shadowva; va; va = va->rnext)
        {
            if(va->shadowmask)
            {
                if(va->tris)
                {
                    genshadowmeshtris(m, sides, draws, va->edata + va->eoffset, va->tris, va->vdata);
                }
                if(skyshadow && va->sky)
                {
                    genshadowmeshtris(m, sides, draws, va->skydata + va->skyoffset, va->sky/3, va->vdata);
                }
            }
        }
        if(shadowmms)
        {
            genshadowmeshmapmodels(m, sides, draws);
        }
        flushshadowmeshdraws(m, sides, draws);

        shadowmeshes[idx] = m;

        shadowmapping = 0;
    }

    VARF(smmesh, 0, 1, 1, { if(!smmesh) clearshadowmeshes(); });
}

/* externally relevant functionality */
///////////////////////////////////////

// vfc - view frustum culling

bool isfoggedsphere(float rad, const vec &cv)
{
    for(int i = 0; i < 4; ++i)
    {
        if(vfcP[i].dist(cv) < -rad)
        {
            return true;
        }
    }
    float dist = vfcP[4].dist(cv);
    return dist < -rad || dist > vfcDfog + rad; // true if abs(dist) is large
}

int isvisiblesphere(float rad, const vec &cv)
{
    int v = ViewFrustumCull_FullyVisible;
    float dist;

    for(int i = 0; i < 5; ++i)
    {
        dist = vfcP[i].dist(cv);
        if(dist < -rad)
        {
            return ViewFrustumCull_NotVisible;
        }
        if(dist < rad)
        {
            v = ViewFrustumCull_PartlyVisible;
        }
    }

    dist -= vfcDfog;
    if(dist > rad)
    {
        return ViewFrustumCull_Fogged;  //ViewFrustumCull_NotVisible;    // culling when fog is closer than size of world results in HOM
    }
    if(dist > -rad)
    {
        v = ViewFrustumCull_PartlyVisible;
    }
    return v;
}

void visiblecubes(bool cull)
{
    if(cull)
    {
        setvfcP();
        findvisiblevas();
    }
    else
    {
        memset(vfcP, 0, sizeof(vfcP));
        vfcDfog = farplane;
        memset(vfcDnear, 0, sizeof(vfcDnear));
        memset(vfcDfar, 0, sizeof(vfcDfar));
        visibleva = nullptr;
        for(int i = 0; i < valist.length(); i++)
        {
            vtxarray *va = valist[i];
            va->distance = 0;
            va->curvfc = ViewFrustumCull_FullyVisible;
            va->occluded = !va->texs ? Occlude_Geom : Occlude_Nothing;
            va->query = nullptr;
            va->next = visibleva;
            visibleva = va;
        }
    }
}

int isvisiblebb(const ivec &bo, const ivec &br)
{
    int v = ViewFrustumCull_FullyVisible;
    float dnear, dfar;

    for(int i = 0; i < 5; ++i)
    {
        const plane &p = vfcP[i];
        dnear = dfar = bo.dist(p);
        if(p.x > 0)
        {
            dfar += p.x*br.x;
        }
        else
        {
            dnear += p.x*br.x;
        }
        if(p.y > 0)
        {
            dfar += p.y*br.y;
        }
        else
        {
            dnear += p.y*br.y;
        }
        if(p.z > 0)
        {
            dfar += p.z*br.z;
        }
        else
        {
            dnear += p.z*br.z;
        }
        if(dfar < 0)
        {
            return ViewFrustumCull_NotVisible;
        }
        if(dnear < 0)
        {
            v = ViewFrustumCull_PartlyVisible;
        }
    }

    if(dnear > vfcDfog)
    {
        return ViewFrustumCull_Fogged;
    }
    if(dfar > vfcDfog)
    {
        v = ViewFrustumCull_PartlyVisible;
    }
    return v;
}

bool bboccluded(const ivec &bo, const ivec &br)
{
    int diff = (bo.x^br.x) | (bo.y^br.y) | (bo.z^br.z);
    if(diff&~((1<<worldscale)-1))
    {
        return false;
    }
    int scale = worldscale-1;
    if(diff&(1<<scale))
    {
        return bboccluded(bo, br, worldroot, ivec(0, 0, 0), 1<<scale);
    }
    cube *c = &worldroot[OCTA_STEP(bo.x, bo.y, bo.z, scale)];
    if(c->ext && c->ext->va)
    {
        vtxarray *va = c->ext->va;
        if(va->curvfc >= ViewFrustumCull_Fogged || (va->occluded >= Occlude_BB && bbinsideva(bo, br, va)))
        {
            return true;
        }
    }
    scale--;
    while(c->children && !(diff&(1<<scale)))
    {
        c = &c->children[OCTA_STEP(bo.x, bo.y, bo.z, scale)];
        if(c->ext && c->ext->va)
        {
            vtxarray *va = c->ext->va;
            if(va->curvfc >= ViewFrustumCull_Fogged || (va->occluded >= Occlude_BB && bbinsideva(bo, br, va)))
            {
                return true;
            }
        }
        scale--;
    }
    if(c->children)
    {
        return bboccluded(bo, br, c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale);
    }
    return false;
}

void flipqueries()
{
    flipquery = (flipquery + 1) % maxqueryframes;
    queryframes[flipquery].flip();
}

occludequery *newquery(void *owner)
{
    return queryframes[flipquery].newquery(owner);
}

void startquery(occludequery *query)
{
    glBeginQuery_(querytarget(), query->id);
}

void endquery()
{
    glEndQuery_(querytarget());
}

bool checkquery(occludequery *query, bool nowait)
{
    if(query->fragments < 0)
    {
        if(nowait || !oqwait)
        {
            GLint avail;
            glGetQueryObjectiv_(query->id, GL_QUERY_RESULT_AVAILABLE, &avail);
            if(!avail)
            {
                return false;
            }
        }

        GLuint fragments;
        glGetQueryObjectuiv_(query->id, GL_QUERY_RESULT, &fragments);
        query->fragments = querytarget() == GL_SAMPLES_PASSED || !fragments ? static_cast<int>(fragments) : oqfrags;
    }
    return query->fragments < oqfrags;
}


void resetqueries()
{
    for(int i = 0; i < maxqueryframes; ++i)
    {
        queryframes[i].reset();
    }
}

int getnumqueries()
{
    return queryframes[flipquery].cur;
}

void startbb(bool mask)
{
    setupbb();
    gle::bindvbo(bbvbo);
    gle::bindebo(bbebo);
    gle::vertexpointer(sizeof(vec), (const vec *)0);
    gle::enablevertex();
    SETSHADER(bbquery);
    if(mask)
    {
        glDepthMask(GL_FALSE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }
}

void endbb(bool mask)
{
    gle::disablevertex();
    gle::clearvbo();
    gle::clearebo();
    if(mask)
    {
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
}

void drawbb(const ivec &bo, const ivec &br)
{
    LOCALPARAMF(bborigin, bo.x, bo.y, bo.z);
    LOCALPARAMF(bbsize, br.x, br.y, br.z);
    glDrawRangeElements_(GL_TRIANGLES, 0, 8-1, 3*2*6, GL_UNSIGNED_SHORT, (ushort *)0);
    xtraverts += 8;
}

void setvfcP(const vec &bbmin, const vec &bbmax)
{
    vec4 px = camprojmatrix.rowx(), py = camprojmatrix.rowy(), pz = camprojmatrix.rowz(), pw = camprojmatrix.roww();
    vfcP[0] = plane(vec4(pw).mul(-bbmin.x).add(px)).normalize(); // left plane
    vfcP[1] = plane(vec4(pw).mul(bbmax.x).sub(px)).normalize(); // right plane
    vfcP[2] = plane(vec4(pw).mul(-bbmin.y).add(py)).normalize(); // bottom plane
    vfcP[3] = plane(vec4(pw).mul(bbmax.y).sub(py)).normalize(); // top plane
    vfcP[4] = plane(vec4(pw).add(pz)).normalize(); // near/far planes

    vfcDfog = std::min(calcfogcull(), static_cast<float>(farplane));
    calcvfcD();
}

//oq

void rendermapmodels()
{
    static int skipoq = 0;
    bool doquery = !drawtex && oqfrags && oqmm;
    const vector<extentity *> &ents = entities::getents();
    findvisiblemms(ents, doquery);

    for(octaentities *oe = visiblemms; oe; oe = oe->next)
    {
        if(oe->distance>=0)
        {
            bool rendered = false;
            for(int i = 0; i < oe->mapmodels.length(); i++)
            {
                extentity &e = *ents[oe->mapmodels[i]];
                if(!(e.flags&EntFlag_Render))
                {
                    continue;
                }
                if(!rendered)
                {
                    rendered = true;
                    oe->query = doquery && oe->distance>0 && !(++skipoq%oqmm) ? newquery(oe) : nullptr;
                    if(oe->query)
                    {
                        startmodelquery(oe->query);
                    }
                }
                rendermapmodel(e);
                e.flags &= ~EntFlag_Render;
            }
            if(rendered && oe->query)
            {
                endmodelquery();
            }
        }
    }
    rendermapmodelbatches();
    clearbatchedmapmodels();

    bool queried = false;
    for(octaentities *oe = visiblemms; oe; oe = oe->next)
    {
        if(oe->distance<0)
        {
            oe->query = doquery && !camera1->o.insidebb(oe->bbmin, oe->bbmax, 1) ? newquery(oe) : nullptr;
            if(!oe->query)
            {
                continue;
            }
            if(!queried)
            {
                startbb();
                queried = true;
            }
            startquery(oe->query);
            drawbb(oe->bbmin, ivec(oe->bbmax).sub(oe->bbmin));
            endquery();
        }
    }
    if(queried)
    {
        endbb();
    }
}

void renderoutline()
{
    ldrnotextureshader->set();

    gle::enablevertex();

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    gle::color(outlinecolor);

    enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    if(!dtoutline)
    {
        glDisable(GL_DEPTH_TEST);
    }
    vtxarray *prev = nullptr;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->occluded < Occlude_BB)
        {
            if((!va->texs || va->occluded >= Occlude_Geom) && !va->alphaback && !va->alphafront && !va->refracttris)
            {
                continue;
            }
            if(!prev || va->vbuf != prev->vbuf)
            {
                gle::bindvbo(va->vbuf);
                gle::bindebo(va->ebuf);
                const vertex *ptr = 0;
                gle::vertexpointer(sizeof(vertex), ptr->pos.v);
            }
            if(va->texs && va->occluded < Occlude_Geom)
            {
                drawvatris(va, 3*va->tris, 0);
                xtravertsva += va->verts;
            }
            if(va->alphaback || va->alphafront || va->refract)
            {
                drawvatris(va, 3*(va->alphabacktris + va->alphafronttris + va->refracttris), 3*(va->tris));
                xtravertsva += 3*(va->alphabacktris + va->alphafronttris + va->refracttris);
            }
            prev = va;
        }
    }
    if(!dtoutline)
    {
        glEnable(GL_DEPTH_TEST);
    }
    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gle::clearvbo();
    gle::clearebo();
    gle::disablevertex();
}

bool renderexplicitsky(bool outline)
{
    vtxarray *prev = nullptr;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->sky && va->occluded < Occlude_BB &&
            ((va->skymax.x >= 0 && isvisiblebb(va->skymin, ivec(va->skymax).sub(va->skymin)) != ViewFrustumCull_NotVisible) ||
            !insideworld(camera1->o)))
        {
            if(!prev || va->vbuf != prev->vbuf)
            {
                if(!prev)
                {
                    gle::enablevertex();
                    if(outline)
                    {
                        ldrnotextureshader->set();
                        gle::color(explicitskycolor);
                        glDepthMask(GL_FALSE);
                        enablepolygonoffset(GL_POLYGON_OFFSET_LINE);
                        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    }
                    else if(editmode)
                    {
                        maskgbuffer("d");
                        SETSHADER(depth);
                    }
                    else
                    {
                        nocolorshader->set();
                        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    }
                }
                gle::bindvbo(va->vbuf);
                gle::bindebo(va->skybuf);
                const vertex *ptr = 0;
                gle::vertexpointer(sizeof(vertex), ptr->pos.v);
            }
            drawvaskytris(va);
            xtraverts += va->sky;
            prev = va;
        }
    }
    if(!prev)
    {
        return false;
    }
    if(outline)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
        glDepthMask(GL_TRUE);
    }
    else if(editmode)
    {
        maskgbuffer("cnd");
    }
    else
    {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    gle::disablevertex();
    gle::clearvbo();
    gle::clearebo();
    return true;
}

void cleanupva()
{
    clearvas(worldroot);
    clearqueries();
    cleanupbb();
    cleanupgrass();
}

int calctrisidemask(const vec &p1, const vec &p2, const vec &p3, float bias)
{
    // p1, p2, p3 are in the cubemap's local coordinate system
    // bias = border/(size - border)
    int mask = 0x3F;
    float dp1 = p1.x + p1.y,
          dn1 = p1.x - p1.y,
          ap1 = std::fabs(dp1),
          an1 = std::fabs(dn1),
          dp2 = p2.x + p2.y,
          dn2 = p2.x - p2.y,
          ap2 = std::fabs(dp2),
          an2 = std::fabs(dn2),
          dp3 = p3.x + p3.y,
          dn3 = p3.x - p3.y,
          ap3 = std::fabs(dp3),
          an3 = std::fabs(dn3);
    if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
    {
        mask &=  (3<<4)
               | (dp1 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
               | (dp2 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
               | (dp3 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    }
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &=  (3<<4)
               | (dn1 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
               | (dn2 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
               | (dn3 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));
    dp1 = p1.y + p1.z,
    dn1 = p1.y - p1.z,
    ap1 = std::fabs(dp1),
    an1 = std::fabs(dn1),
    dp2 = p2.y + p2.z,
    dn2 = p2.y - p2.z,
    ap2 = std::fabs(dp2),
    an2 = std::fabs(dn2),
    dp3 = p3.y + p3.z,
    dn3 = p3.y - p3.z,
    ap3 = std::fabs(dp3),
    an3 = std::fabs(dn3);
    if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
    {
        mask &= (3<<0)
            | (dp1 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp2 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp3 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    }
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
    {
        mask &= (3<<0)
            | (dn1 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn2 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn3 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));
    }
    dp1 = p1.z + p1.x,
    dn1 = p1.z - p1.x,
    ap1 = std::fabs(dp1),
    an1 = std::fabs(dn1),
    dp2 = p2.z + p2.x,
    dn2 = p2.z - p2.x,
    ap2 = std::fabs(dp2),
    an2 = std::fabs(dn2),
    dp3 = p3.z + p3.x,
    dn3 = p3.z - p3.x,
    ap3 = std::fabs(dp3),
    an3 = std::fabs(dn3);
    if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
    {
        mask &= (3<<2)
            | (dp1 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp2 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp3 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    }
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
    {
        mask &= (3<<2)
            | (dn1 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn2 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn3 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));
    }
    return mask;
}

int findalphavas()
{
    alphavas.clear();
    alphafrontsx1 = alphafrontsy1 = alphabacksx1 = alphabacksy1 = alpharefractsx1 = alpharefractsy1 = 1;
    alphafrontsx2 = alphafrontsy2 = alphabacksx2 = alphabacksy2 = alpharefractsx2 = alpharefractsy2 = -1;
    alphabackvas = alpharefractvas = 0;
    memset(alphatiles, 0, sizeof(alphatiles));
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->alphabacktris || va->alphafronttris || va->refracttris)
        {
            if(va->occluded >= Occlude_BB)
            {
                continue;
            }
            if(va->curvfc==ViewFrustumCull_Fogged)
            {
                continue;
            }
            float sx1 = -1,
                  sx2 =  1,
                  sy1 = -1,
                  sy2 =  1;
            if(!calcbbscissor(va->alphamin, va->alphamax, sx1, sy1, sx2, sy2))
            {
                continue;
            }
            alphavas.push_back(va);
            masktiles(alphatiles, sx1, sy1, sx2, sy2);
            alphafrontsx1 = std::min(alphafrontsx1, sx1);
            alphafrontsy1 = std::min(alphafrontsy1, sy1);
            alphafrontsx2 = std::max(alphafrontsx2, sx2);
            alphafrontsy2 = std::max(alphafrontsy2, sy2);
            if(va->alphabacktris)
            {
                alphabackvas++;
                alphabacksx1 = std::min(alphabacksx1, sx1);
                alphabacksy1 = std::min(alphabacksy1, sy1);
                alphabacksx2 = std::max(alphabacksx2, sx2);
                alphabacksy2 = std::max(alphabacksy2, sy2);
            }
            if(va->refracttris)
            {
                if(!calcbbscissor(va->refractmin, va->refractmax, sx1, sy1, sx2, sy2))
                {
                    continue;
                }
                alpharefractvas++;
                alpharefractsx1 = std::min(alpharefractsx1, sx1);
                alpharefractsy1 = std::min(alpharefractsy1, sy1);
                alpharefractsx2 = std::max(alpharefractsx2, sx2);
                alpharefractsy2 = std::max(alpharefractsy2, sy2);
            }
        }
    }
    return (alpharefractvas ? 4 : 0) | (alphavas.size() ? 2 : 0) | (alphabackvas ? 1 : 0);
}

void renderrefractmask()
{
    gle::enablevertex();

    vtxarray *prev = nullptr;
    for(uint i = 0; i < alphavas.size(); i++)
    {
        vtxarray *va = alphavas[i];
        if(!va->refracttris)
        {
            continue;
        }
        if(!prev || va->vbuf != prev->vbuf)
        {
            gle::bindvbo(va->vbuf);
            gle::bindebo(va->ebuf);
            const vertex *ptr = 0;
            gle::vertexpointer(sizeof(vertex), ptr->pos.v);
        }
        drawvatris(va, 3*va->refracttris, 3*(va->tris + va->alphabacktris + va->alphafronttris));
        xtravertsva += 3*va->refracttris;
        prev = va;
    }

    gle::clearvbo();
    gle::clearebo();
    gle::disablevertex();
}

void renderalphageom(int side)
{
    resetbatches();

    renderstate cur;
    cur.alphaing = side;
    cur.alphascale = -1;

    setupgeom();

    if(side == 2)
    {
        for(uint i = 0; i < alphavas.size(); i++)
        {
            renderva(cur, alphavas[i], RenderPass_GBuffer);
        }
        if(geombatches.length())
        {
            renderbatches(cur, RenderPass_GBuffer);
        }
    }
    else
    {
        glCullFace(GL_FRONT);
        for(uint i = 0; i < alphavas.size(); i++)
        {
            if(alphavas[i]->alphabacktris)
            {
                renderva(cur, alphavas[i], RenderPass_GBuffer);
            }
        }
        if(geombatches.length())
        {
            renderbatches(cur, RenderPass_GBuffer);
        }
        glCullFace(GL_BACK);
    }

    cleanupgeom(cur);
}

void rendergeom()
{
    bool doOQ = oqfrags && oqgeom && !drawtex,
         multipassing = false;
    renderstate cur;

    if(doOQ)
    {
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(va->texs)
            {
                if(!camera1->o.insidebb(va->o, va->size, 2))
                {
                    if(va->parent && va->parent->occluded >= Occlude_BB)
                    {
                        va->query = nullptr;
                        va->occluded = Occlude_Parent;
                        continue;
                    }
                    va->occluded = va->query && va->query->owner == va &&
                                       checkquery(va->query) ? std::min(va->occluded+1, static_cast<int>(Occlude_BB)) : Occlude_Nothing;
                    va->query = newquery(va);
                    if(!va->query || !va->occluded)
                    {
                        va->occluded = Occlude_Nothing;
                    }
                    if(va->occluded >= Occlude_Geom)
                    {
                        if(va->query)
                        {
                            if(cur.vattribs)
                            {
                                disablevattribs(cur, false);
                            }
                            if(cur.vbuf)
                            {
                                disablevbuf(cur);
                            }
                            renderquery(cur, va->query, va);
                        }
                        continue;
                    }
                }
                else
                {
                    va->query = nullptr;
                    va->occluded = Occlude_Nothing;
                    if(va->occluded >= Occlude_Geom)
                    {
                        continue;
                    }
                }
                renderva(cur, va, RenderPass_Z, true);
            }
        }

        if(cur.vquery)
        {
            disablevquery(cur);
        }
        if(cur.vattribs)
        {
            disablevattribs(cur, false);
        }
        if(cur.vbuf)
        {
            disablevbuf(cur);
        }
        glFlush();
        if(cur.colormask)
        {
            cur.colormask = false;
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }
        if(cur.depthmask)
        {
            cur.depthmask = false;
            glDepthMask(GL_FALSE);
        }
        workinoq();
        if(!cur.colormask)
        {
            cur.colormask = true;
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        if(!cur.depthmask)
        {
            cur.depthmask = true;
            glDepthMask(GL_TRUE);
        }
        if(!multipassing)
        {
            multipassing = true;
            glDepthFunc(GL_LEQUAL);
        }
        cur.texgenorient = -1;
        setupgeom();
        resetbatches();
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(va->texs && va->occluded < Occlude_Geom)
            {
                renderva(cur, va, RenderPass_GBuffer);
            }
        }
        if(geombatches.length())
        {
            renderbatches(cur, RenderPass_GBuffer);
            glFlush();
        }
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(va->texs && va->occluded >= Occlude_Geom)
            {
                if((va->parent && va->parent->occluded >= Occlude_BB) || (va->query && checkquery(va->query)))
                {
                    va->occluded = Occlude_BB;
                    continue;
                }
                else
                {
                    va->occluded = Occlude_Nothing;
                    if(va->occluded >= Occlude_Geom)
                    {
                        continue;
                    }
                }

                renderva(cur, va, RenderPass_GBuffer);
            }
        }
        if(geombatches.length())
        {
            renderbatches(cur, RenderPass_GBuffer);
        }
    }
    else
    {
        setupgeom();
        resetbatches();
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(va->texs)
            {
                va->query = nullptr;
                va->occluded = Occlude_Nothing;
                if(va->occluded >= Occlude_Geom)
                {
                    continue;
                }
                renderva(cur, va, RenderPass_GBuffer);
            }
        }
        if(geombatches.length())
        {
            renderbatches(cur, RenderPass_GBuffer);
        }
    }
    if(multipassing)
    {
        glDepthFunc(GL_LESS);
    }
    cleanupgeom(cur);
    if(!doOQ)
    {
        glFlush();
        if(cur.colormask)
        {
            cur.colormask = false;
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }
        if(cur.depthmask)
        {
            cur.depthmask = false;
            glDepthMask(GL_FALSE);
        }
        workinoq();
        if(!cur.colormask)
        {
            cur.colormask = true;
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        if(!cur.depthmask)
        {
            cur.depthmask = true;
            glDepthMask(GL_TRUE);
        }
    }
}

void renderdecals()
{
    vtxarray *decalva;
    for(decalva = visibleva; decalva; decalva = decalva->next)
    {
        if(decalva->decaltris && decalva->occluded < Occlude_BB)
        {
            break;
        }
    }
    if(!decalva)
    {
        return;
    }
    decalrenderer cur;

    setupdecals();
    resetdecalbatches();

    if(maxdualdrawbufs)
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC1_ALPHA);
        maskgbuffer("c");
        for(vtxarray *va = decalva; va; va = va->next)
        {
            if(va->decaltris && va->occluded < Occlude_BB)
            {
                mergedecals(va);
                if(!batchdecals && decalbatches.size())
                {
                    renderdecalbatches(cur, 0);
                }
            }
        }
        if(decalbatches.size())
        {
            renderdecalbatches(cur, 0);
        }
        if(usepacknorm())
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        }
        else
        {
            glBlendFunc(GL_SRC1_ALPHA, GL_ONE_MINUS_SRC1_ALPHA);
        }
        maskgbuffer("n");
        cur.vbuf = 0;
        for(vtxarray *va = decalva; va; va = va->next)
        {
            if(va->decaltris && va->occluded < Occlude_BB)
            {
                mergedecals(va);
                if(!batchdecals && decalbatches.size())
                {
                    renderdecalbatches(cur, 1);
                }
            }
        }
        if(decalbatches.size())
        {
            renderdecalbatches(cur, 1);
        }
    }
    else
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        maskgbuffer("cn");
        for(vtxarray *va = decalva; va; va = va->next)
        {
            if(va->decaltris && va->occluded < Occlude_BB)
            {
                mergedecals(va);
                if(!batchdecals && decalbatches.size())
                {
                    renderdecalbatches(cur, 0);
                }
            }
        }
        if(decalbatches.size())
        {
            renderdecalbatches(cur, 0);
        }
    }
    cleanupdecals();
}

//shadowmeshes

void clearshadowmeshes()
{
    if(shadowvbos.size())
    {
        glDeleteBuffers_(shadowvbos.size(), shadowvbos.data());
        shadowvbos.clear();
    }
    if(shadowmeshes.numelems)
    {
        vector<extentity *> &ents = entities::getents();
        for(int i = 0; i < ents.length(); i++)
        {
            extentity &e = *ents[i];
            if(e.flags&EntFlag_ShadowMesh)
            {
                e.flags &= ~EntFlag_ShadowMesh;
            }
        }
    }
    shadowmeshes.clear();
    shadowdraws.clear();
}

void genshadowmeshes()
{
    clearshadowmeshes();

    if(!smmesh)
    {
        return;
    }
    renderprogress(0, "generating shadow meshes..");

    vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < ents.length(); i++)
    {
        extentity &e = *ents[i];
        if(e.type != EngineEnt_Light)
        {
            continue;
        }
        genshadowmesh(i, e);
    }
}

shadowmesh *findshadowmesh(int idx, extentity &e)
{
    shadowmesh *m = shadowmeshes.access(idx);
    if(!m || m->type != shadowmapping || m->origin != shadoworigin || m->radius < shadowradius)
    {
        return nullptr;
    }
    switch(m->type)
    {
        case ShadowMap_Spot:
        {
            if(!e.attached || e.attached->type != EngineEnt_Spotlight || m->spotloc != e.attached->o || m->spotangle < std::clamp(static_cast<int>(e.attached->attr1), 1, 89))
            {
                return nullptr;
            }
            break;
        }
    }
    return m;
}

void rendershadowmesh(shadowmesh *m)
{
    int draw = m->draws[shadowside];
    if(draw < 0)
    {
        return;
    }
    SETSHADER(shadowmapworld);
    gle::enablevertex();
    GLuint ebuf = 0,
           vbuf = 0;
    while(draw >= 0)
    {
        shadowdraw &d = shadowdraws[draw];
        if(ebuf != d.ebuf)
        {
            gle::bindebo(d.ebuf);
            ebuf = d.ebuf;
        }
        if(vbuf != d.vbuf)
        {
            gle::bindvbo(d.vbuf);
            vbuf = d.vbuf; gle::vertexpointer(sizeof(vec), 0);
        }
        drawtris(3*d.tris, (ushort *)0 + d.offset, d.minvert, d.maxvert);
        xtravertsva += 3*d.tris;
        draw = d.next;
    }
    gle::disablevertex();
    gle::clearebo();
    gle::clearvbo();
}

//external api functions
int calcspheresidemask(const vec &p, float radius, float bias)
{
    // p is in the cubemap's local coordinate system
    // bias = border/(size - border)
    float dxyp = p.x + p.y,
          dxyn = p.x - p.y,
          axyp = std::fabs(dxyp),
          axyn = std::fabs(dxyn),
          dyzp = p.y + p.z,
          dyzn = p.y - p.z,
          ayzp = std::fabs(dyzp),
          ayzn = std::fabs(dyzn),
          dzxp = p.z + p.x,
          dzxn = p.z - p.x,
          azxp = std::fabs(dzxp),
          azxn = std::fabs(dzxn);
    int mask = 0x3F;
    radius *= SQRT2;
    if(axyp > bias*axyn + radius)
    {
        mask &= dxyp < 0 ? ~((1<<0)|(1<<2)) : ~((2<<0)|(2<<2));
    }
    if(axyn > bias*axyp + radius)
    {
        mask &= dxyn < 0 ? ~((1<<0)|(2<<2)) : ~((2<<0)|(1<<2));
    }
    if(ayzp > bias*ayzn + radius)
    {
        mask &= dyzp < 0 ? ~((1<<2)|(1<<4)) : ~((2<<2)|(2<<4));
    }
    if(ayzn > bias*ayzp + radius)
    {
        mask &= dyzn < 0 ? ~((1<<2)|(2<<4)) : ~((2<<2)|(1<<4));
    }
    if(azxp > bias*azxn + radius)
    {
        mask &= dzxp < 0 ? ~((1<<4)|(1<<0)) : ~((2<<4)|(2<<0));
    }
    if(azxn > bias*azxp + radius)
    {
        mask &= dzxn < 0 ? ~((1<<4)|(2<<0)) : ~((2<<4)|(1<<0));
    }
    return mask;
}

int cullfrustumsides(const vec &lightpos, float lightradius, float size, float border)
{
    int sides = 0x3F,
        masks[6] = { 3<<4, 3<<4, 3<<0, 3<<0, 3<<2, 3<<2 };
    float scale = (size - 2*border)/size,
          bias = border / static_cast<float>(size - border);
    // check if cone enclosing side would cross frustum plane
    scale = 2 / (scale*scale + 2);
    for(int i = 0; i < 5; ++i)
    {
        if(vfcP[i].dist(lightpos) <= -0.03125f)
        {
            vec n = vec(vfcP[i]).div(lightradius);
            float len = scale*n.squaredlen();
            if(n.x*n.x > len)
            {
                sides &= n.x < 0 ? ~(1<<0) : ~(2 << 0);
            }
            if(n.y*n.y > len)
            {
                sides &= n.y < 0 ? ~(1<<2) : ~(2 << 2);
            }
            if(n.z*n.z > len)
            {
                sides &= n.z < 0 ? ~(1<<4) : ~(2 << 4);
            }
        }
    }
    if (vfcP[4].dist(lightpos) >= vfcDfog + 0.03125f)
    {
        vec n = vec(vfcP[4]).div(lightradius);
        float len = scale*n.squaredlen();
        if(n.x*n.x > len)
        {
            sides &= n.x >= 0 ? ~(1<<0) : ~(2 << 0);
        }
        if(n.y*n.y > len)
        {
            sides &= n.y >= 0 ? ~(1<<2) : ~(2 << 2);
        }
        if(n.z*n.z > len)
        {
            sides &= n.z >= 0 ? ~(1<<4) : ~(2 << 4);
        }
    }
    // this next test usually clips off more sides than the former, but occasionally clips fewer/different ones, so do both and combine results
    // check if frustum corners/origin cross plane sides
    // infinite version, assumes frustum corners merely give direction and extend to infinite distance
    vec p = vec(camera1->o).sub(lightpos).div(lightradius);
    float dp = p.x + p.y,
          dn = p.x - p.y,
          ap = std::fabs(dp),
          an = std::fabs(dn);
    masks[0] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    masks[1] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));
    dp = p.y + p.z, dn = p.y - p.z,
    ap = std::fabs(dp),
    an = std::fabs(dn);
    masks[2] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    masks[3] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));
    dp = p.z + p.x,
    dn = p.z - p.x,
    ap = std::fabs(dp),
    an = std::fabs(dn);
    masks[4] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    masks[5] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));
    for(int i = 0; i < 4; ++i)
    {
        vec n;
        switch(i)
        {
            case 0:
            {
                n.cross(vfcP[0], vfcP[2]);
                break;
            }
            case 1:
            {
                n.cross(vfcP[3], vfcP[0]);
                break;
            }
            case 2:
            {
                n.cross(vfcP[2], vfcP[1]);
                break;
            }
            case 3:
            {
                n.cross(vfcP[1], vfcP[3]);
                break;
            }
        }
        dp = n.x + n.y,
        dn = n.x - n.y,
        ap = std::fabs(dp),
        an = std::fabs(dn);
        if(ap > 0)
        {
            masks[0] |= dp >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2);
        }
        if(an > 0)
        {
            masks[1] |= dn >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2);
        }
        dp = n.y + n.z,
        dn = n.y - n.z,
        ap = std::fabs(dp),
        an = std::fabs(dn);
        if(ap > 0)
        {
            masks[2] |= dp >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4);
        }
        if(an > 0)
        {
            masks[3] |= dn >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4);
        }
        dp = n.z + n.x,
        dn = n.z - n.x,
        ap = std::fabs(dp),
        an = std::fabs(dn);
        if(ap > 0)
        {
            masks[4] |= dp >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0);
        }
        if(an > 0)
        {
            masks[5] |= dn >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0);
        }
    }
    return sides & masks[0] & masks[1] & masks[2] & masks[3] & masks[4] & masks[5];
}

void findshadowvas(vector<vtxarray *> &vas)
{
    for(int i = 0; i < vas.length(); i++)
    {
        vtxarray &v = *vas[i];
        float dist = vadist(&v, shadoworigin);
        if(dist < shadowradius || !smdistcull)
        {
            v.shadowmask = !smbbcull ? 0x3F : (v.children.length() || v.mapmodels.length() ?
                                calcbbsidemask(v.bbmin, v.bbmax, shadoworigin, shadowradius, shadowbias) :
                                calcbbsidemask(v.geommin, v.geommax, shadoworigin, shadowradius, shadowbias));
            addshadowva(&v, dist);
            if(v.children.length())
            {
                findshadowvas(v.children);
            }
        }
    }
}

void renderrsmgeom(bool dyntex)
{
    renderstate cur;
    if(!dyntex)
    {
        cur.texgenmillis = 0;
    }
    setupgeom();
    if(skyshadow)
    {
        enablevattribs(cur, false);
        SETSHADER(rsmsky);
        vtxarray *prev = nullptr;
        for(vtxarray *va = shadowva; va; va = va->rnext)
        {
            if(va->sky)
            {
                if(!prev || va->vbuf != prev->vbuf)
                {
                    gle::bindvbo(va->vbuf);
                    gle::bindebo(va->skybuf);
                    const vertex *ptr = 0;
                    gle::vertexpointer(sizeof(vertex), ptr->pos.v);
                }
                drawvaskytris(va);
                xtravertsva += va->sky/3;
                prev = va;
            }
        }
        if(cur.vattribs)
        {
            disablevattribs(cur, false);
        }
    }
    resetbatches();
    for(vtxarray *va = shadowva; va; va = va->rnext)
    {
        if(va->texs)
        {
            renderva(cur, va, RenderPass_ReflectiveShadowMap);
        }
    }
    if(geombatches.length())
    {
        renderbatches(cur, RenderPass_ReflectiveShadowMap);
    }
    cleanupgeom(cur);
}

int dynamicshadowvabounds(int mask, vec &bbmin, vec &bbmax)
{
    int vis = 0;
    for(vtxarray *va = shadowva; va; va = va->rnext)
    {
        if(va->shadowmask&mask && va->dyntexs)
        {
            bbmin.min(vec(va->geommin));
            bbmax.max(vec(va->geommax));
            vis++;
        }
    }
    return vis;
}

void findshadowmms()
{
    shadowmms = nullptr;
    octaentities **lastmms = &shadowmms;
    for(vtxarray *va = shadowva; va; va = va->rnext)
    {
        for(int j = 0; j < va->mapmodels.length(); j++)
        {
            octaentities *oe = va->mapmodels[j];
            switch(shadowmapping)
            {
                case ShadowMap_Reflect:
                {
                    break;
                }
                case ShadowMap_Cascade:
                {
                    if(!calcbbcsmsplits(oe->bbmin, oe->bbmax))
                    {
                        continue;
                    }
                    break;
                }
                case ShadowMap_CubeMap:
                {
                    if(smdistcull && shadoworigin.dist_to_bb(oe->bbmin, oe->bbmax) >= shadowradius)
                    {
                        continue;
                    }
                    break;
                }
                case ShadowMap_Spot:
                {
                    if(smdistcull && shadoworigin.dist_to_bb(oe->bbmin, oe->bbmax) >= shadowradius)
                    {
                        continue;
                    }
                    if(smbbcull && !bbinsidespot(shadoworigin, shadowdir, shadowspot, oe->bbmin, oe->bbmax))
                    {
                        continue;
                    }
                    break;
                }
            }
            oe->rnext = nullptr;
            *lastmms = oe;
            lastmms = &oe->rnext;
        }
    }
}

void rendershadowmapworld()
{
    SETSHADER(shadowmapworld);

    gle::enablevertex();

    vtxarray *prev = nullptr;
    for(vtxarray *va = shadowva; va; va = va->rnext)
    {
        if(va->tris && va->shadowmask&(1<<shadowside))
        {
            if(!prev || va->vbuf != prev->vbuf)
            {
                gle::bindvbo(va->vbuf);
                gle::bindebo(va->ebuf);
                const vertex *ptr = 0;
                gle::vertexpointer(sizeof(vertex), ptr->pos.v);
            }
            if(!smnodraw)
            {
                drawvatris(va, 3*va->tris, 0);
            }
            xtravertsva += va->verts;
            prev = va;
        }
    }
    if(skyshadow)
    {
        prev = nullptr;
        for(vtxarray *va = shadowva; va; va = va->rnext)
        {
            if(va->sky && va->shadowmask&(1<<shadowside))
            {
                if(!prev || va->vbuf != prev->vbuf)
                {
                    gle::bindvbo(va->vbuf);
                    gle::bindebo(va->skybuf);
                    const vertex *ptr = 0;
                    gle::vertexpointer(sizeof(vertex), ptr->pos.v);
                }
                if(!smnodraw)
                {
                    drawvaskytris(va);
                }
                xtravertsva += va->sky/3;
                prev = va;
            }
        }
    }

    gle::clearvbo();
    gle::clearebo();
    gle::disablevertex();
}

void batchshadowmapmodels(bool skipmesh)
{
    if(!shadowmms)
    {
        return;
    }
    int nflags = EntFlag_NoVis|EntFlag_NoShadow;
    if(skipmesh)
    {
        nflags |= EntFlag_ShadowMesh;
    }
    const vector<extentity *> &ents = entities::getents();
    for(octaentities *oe = shadowmms; oe; oe = oe->rnext)
    {
        for(int k = 0; k < oe->mapmodels.length(); k++)
        {
            extentity &e = *ents[oe->mapmodels[k]];
            if(e.flags&nflags)
            {
                continue;
            }
            e.flags |= EntFlag_Render;
        }
    }
    for(octaentities *oe = shadowmms; oe; oe = oe->rnext)
    {
        for(int j = 0; j < oe->mapmodels.length(); j++)
        {
            extentity &e = *ents[oe->mapmodels[j]];
            if(!(e.flags&EntFlag_Render))
            {
                continue;
            }
            rendermapmodel(e);
            e.flags &= ~EntFlag_Render;
        }
    }
}

void findshadowvas()
{
    memset(vasort, 0, sizeof(vasort));
    switch(shadowmapping)
    {
        case ShadowMap_Reflect:
        {
            for(int i = 0; i < varoot.length(); ++i)
            {
                findrsmshadowvas(varoot[i]);
            }
            break;
        }
        case ShadowMap_CubeMap:
        {
            findshadowvas(varoot);
            break;
        }
        case ShadowMap_Cascade:
        {
            for(int i = 0; i < varoot.length(); ++i)
            {
                findcsmshadowvas(varoot[i]);
            }
            break;
        }
        case ShadowMap_Spot:
        {
            for(int i = 0; i < varoot.length(); ++i)
            {
                findspotshadowvas(varoot[i]);
            }
            break;
        }
    }
    sortshadowvas();
}

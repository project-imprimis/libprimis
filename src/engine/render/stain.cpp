/* stain.cpp: dynamic world geometry decals
 *
 * Stains are mostly useful for player-created effects, mainly those left behind
 * by weapons. They fade at a fixed period (not controllable by individual stains)
 * and can be culled if there are too many (`maxstaintris`).
 *
 * Stains apply to world (octree) geometry only and cannot be applied to models
 * (players, mapmodels, or otherwise).
 *
 * The performance of stains is generally high enough that many thousands of stain
 * particles must be present at once for there to be a noticable performance drop.
 */

#include "../libprimis-headers/cube.h"
#include "../../shared/glexts.h"

#include "octarender.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendermodel.h"
#include "stain.h"
#include "texture.h"

#include "interface/console.h"
#include "interface/control.h"

#include "world/bih.h"
#include "world/octaworld.h"
#include "world/world.h"

#include "model/model.h"

struct stainvert
{
    vec pos;
    bvec4 color;
    vec2 tc;
};

struct staininfo
{
    int millis;
    bvec color;
    uchar owner;
    ushort startvert, endvert;
};

//stainflag enum is local to this file
enum
{
    StainFlag_Rnd4       = 1<<0,
    StainFlag_Rotate     = 1<<1,
    StainFlag_InvMod     = 1<<2,
    StainFlag_Overbright = 1<<3,
    StainFlag_Glow       = 1<<4,
    StainFlag_Saturate   = 1<<5
};

VARFP(maxstaintris, 1, 2048, 16384, initstains());  //need to call initstains to potentially cull extra stain tris
VARP(stainfade, 1000, 15000, 60000);                //number of milliseconds before stain geom fades
VAR(debugstain, 0, 0, 1);                           //toggles printout of stain information to console

class stainbuffer
{
    public:
        int maxverts, endvert, lastvert, availverts;
        stainbuffer() : maxverts(0), endvert(0), lastvert(0), availverts(0), verts(nullptr), startvert(0), vbo(0), dirty(false)
        {}

        ~stainbuffer()
        {
            DELETEA(verts);
        }

        void init(int tris)
        {
            if(verts)
            {
                DELETEA(verts);
                maxverts = startvert = endvert = lastvert = availverts = 0;
            }
            if(tris)
            {
                maxverts = tris*3 + 3;
                availverts = maxverts - 3;
                verts = new stainvert[maxverts];
            }
        }

        void cleanup()
        {
            if(vbo)
            {
                glDeleteBuffers_(1, &vbo);
                vbo = 0;
            }
        }

        void clear()
        {
            startvert = endvert = lastvert = 0;
            availverts = std::max(maxverts - 3, 0);
            dirty = true;
        }

        int freestain(const staininfo &d)
        {
            int removed = d.endvert < d.startvert ? maxverts - (d.startvert - d.endvert) : d.endvert - d.startvert;
            startvert = d.endvert;
            if(startvert==endvert)
            {
                startvert = endvert = lastvert = 0;
            }
            availverts += removed;
            return removed;
        }

        void clearstains(const staininfo &d)
        {
            startvert = d.endvert;
            availverts = endvert < startvert ? startvert - endvert - 3 : maxverts - 3 - (endvert - startvert);
            dirty = true;
        }

        bool faded(const staininfo &d) const
        {
            return verts[d.startvert].color.a < 255;
        }

        void fadestain(const staininfo &d, const bvec4 &color)
        {
            stainvert *vert = &verts[d.startvert],
                      *end = &verts[d.endvert < d.startvert ? maxverts : d.endvert];
            while(vert < end)
            {
                vert->color = color;
                vert++;
            }
            if(d.endvert < d.startvert)
            {
                vert = verts;
                end = &verts[d.endvert];
                while(vert < end)
                {
                    vert->color = color;
                    vert++;
                }
            }
            dirty = true;
        }

        void render()
        {
            if(startvert == endvert)
            {
                return;
            }
            if(!vbo)
            {
                glGenBuffers_(1, &vbo);
                dirty = true;
            }
            gle::bindvbo(vbo);
            int count = endvert < startvert ? maxverts - startvert : endvert - startvert;
            if(dirty)
            {
                glBufferData_(GL_ARRAY_BUFFER, maxverts*sizeof(stainvert), nullptr, GL_STREAM_DRAW);
                glBufferSubData_(GL_ARRAY_BUFFER, 0, count*sizeof(stainvert), &verts[startvert]);
                if(endvert < startvert)
                {
                    glBufferSubData_(GL_ARRAY_BUFFER, count*sizeof(stainvert), endvert*sizeof(stainvert), verts);
                    count += endvert;
                }
                dirty = false;
            }
            else if(endvert < startvert)
            {
                count += endvert;
            }
            const stainvert *ptr = 0;
            gle::vertexpointer(sizeof(stainvert), ptr->pos.v);
            gle::texcoord0pointer(sizeof(stainvert), ptr->tc.v);
            gle::colorpointer(sizeof(stainvert), ptr->color.v);

            glDrawArrays(GL_TRIANGLES, 0, count);
            xtravertsva += count;
        }

        stainvert *addtri()
        {
            stainvert *tri = &verts[endvert];
            availverts -= 3;
            endvert += 3;
            if(endvert >= maxverts)
            {
                endvert = 0;
            }
            return tri;
        }

        void addstain(staininfo &d)
        {
            dirty = true;
        }

        bool hasverts() const
        {
            return startvert != endvert;
        }

        int nextverts() const
        {
            return endvert < lastvert ? endvert + maxverts - lastvert : endvert - lastvert;
        }

        int totaltris() const
        {
            return (maxverts - 3 - availverts)/3;
        }
    private:
        stainvert *verts;
        int startvert;
        GLuint vbo;
        bool dirty;

        //debug functions, not used by any of the code
        int totalverts() const
        {
            return endvert < startvert ? maxverts - (startvert - endvert) : endvert - startvert;
        }

};

class stainrenderer
{
    public:
        int flags, fadeintime, fadeouttime, timetolive;
        Texture *tex;
        staininfo *stains;
        int maxstains, startstain, endstain;
        stainbuffer verts[StainBuffer_Number];

        stainrenderer(const char *texname, int flags = 0, int fadeintime = 0, int fadeouttime = 1000, int timetolive = -1)
            : flags(flags),
              fadeintime(fadeintime), fadeouttime(fadeouttime), timetolive(timetolive),
              tex(nullptr),
              stains(nullptr), maxstains(0), startstain(0), endstain(0),
              stainu(0), stainv(0), texname(texname)
        {
        }

        ~stainrenderer()
        {
            DELETEA(stains);
        }

        bool usegbuffer() const
        {
            return !(flags&(StainFlag_InvMod|StainFlag_Glow));
        }

        void init(int tris)
        {
            if(stains)
            {
                DELETEA(stains);
                maxstains = startstain = endstain = 0;
            }
            stains = new staininfo[tris];
            maxstains = tris;
            for(int i = 0; i < StainBuffer_Number; ++i)
            {
                verts[i].init(i == StainBuffer_Transparent ? tris/2 : tris);
            }
        }

        void preload()
        {
            tex = textureload(texname, 3);
        }

        int totalstains()
        {
            return endstain < startstain ? maxstains - (startstain - endstain) : endstain - startstain;
        }

        bool hasstains(int sbuf)
        {
            return verts[sbuf].hasverts();
        }

        void clearstains()
        {
            startstain = endstain = 0;
            for(int i = 0; i < StainBuffer_Number; ++i)
            {
                verts[i].clear();
            }
        }

        int freestain()
        {
            if(startstain==endstain)
            {
                return 0;
            }
            staininfo &d = stains[startstain];
            startstain++;
            if(startstain >= maxstains)
            {
                startstain = 0;
            }
            return verts[d.owner].freestain(d);
        }

        bool faded(const staininfo &d) const
        {
            return verts[d.owner].faded(d);
        }

        void fadestain(const staininfo &d, uchar alpha)
        {
            bvec color = d.color;
            if(flags&(StainFlag_Overbright|StainFlag_Glow|StainFlag_InvMod))
            {
                color.scale(alpha, 255);
            }
            verts[d.owner].fadestain(d, bvec4(color, alpha));
        }

        void clearfadedstains()
        {
            int threshold = lastmillis - (timetolive>=0 ? timetolive : stainfade) - fadeouttime;
            staininfo *d = &stains[startstain],
                      *end = &stains[endstain < startstain ? maxstains : endstain],
                      *cleared[StainBuffer_Number] = {nullptr};
            for(; d < end && d->millis <= threshold; d++)
                cleared[d->owner] = d;
            if(d >= end && endstain < startstain)
            {
                for(d = stains, end = &stains[endstain]; d < end && d->millis <= threshold; d++)
                {
                    cleared[d->owner] = d;
                }
            }
            startstain = d - stains;
            if(startstain == endstain)
            {
                for(int i = 0; i < StainBuffer_Number; ++i)
                {
                    verts[i].clear();
                }
            }
            else
            {
                for(int i = 0; i < StainBuffer_Number; ++i)
                {
                    if(cleared[i])
                    {
                        verts[i].clearstains(*cleared[i]);
                    }
                }
            }
        }

        void fadeinstains()
        {
            if(!fadeintime)
            {
                return;
            }
            staininfo *d = &stains[endstain],
                      *end = &stains[endstain < startstain ? 0 : startstain];
            while(d > end)
            {
                d--;
                int fade = lastmillis - d->millis;
                if(fade < fadeintime)
                {
                    fadestain(*d, (fade<<8)/fadeintime);
                }
                else if(faded(*d))
                {
                    fadestain(*d, 255);
                }
                else
                {
                    return;
                }
            }
            if(endstain < startstain)
            {
                d = &stains[maxstains];
                end = &stains[startstain];
                while(d > end)
                {
                    d--;
                    int fade = lastmillis - d->millis;
                    if(fade < fadeintime)
                    {
                        fadestain(*d, (fade<<8)/fadeintime);
                    }
                    else if(faded(*d))
                    {
                        fadestain(*d, 255);
                    }
                    else
                    {
                        return;
                    }
                }
            }
        }

        void fadeoutstains()
        {
            staininfo *d = &stains[startstain],
                      *end = &stains[endstain < startstain ? maxstains : endstain];
            int offset = (timetolive>=0 ? timetolive : stainfade) + fadeouttime - lastmillis;
            while(d < end)
            {
                int fade = d->millis + offset;
                if(fade >= fadeouttime)
                {
                    return;
                }
                fadestain(*d, (fade<<8)/fadeouttime);
                d++;
            }
            if(endstain < startstain)
            {
                d = stains;
                end = &stains[endstain];
                while(d < end)
                {
                    int fade = d->millis + offset;
                    if(fade >= fadeouttime)
                    {
                        return;
                    }
                    fadestain(*d, (fade<<8)/fadeouttime);
                    d++;
                }
            }
        }

        static void setuprenderstate(int sbuf, bool gbuf, int layer)
        {
            if(gbuf)
            {
                maskgbuffer(sbuf == StainBuffer_Transparent ? "cg" : "c");
            }
            else
            {
                zerofogcolor();
            }

            if(layer && ghasstencil)
            {
                glStencilFunc(GL_EQUAL, layer, 0x07);
                glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            }

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

            enablepolygonoffset(GL_POLYGON_OFFSET_FILL);

            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);

            gle::enablevertex();
            gle::enabletexcoord0();
            gle::enablecolor();
        }

        static void cleanuprenderstate(int sbuf, bool gbuf, int layer)
        {
            gle::clearvbo();

            gle::disablevertex();
            gle::disabletexcoord0();
            gle::disablecolor();

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);

            disablepolygonoffset(GL_POLYGON_OFFSET_FILL);

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            if(gbuf)
            {
                maskgbuffer(sbuf == StainBuffer_Transparent ? "cndg" : "cnd");
            }
            else
            {
                resetfogcolor();
            }
        }

        void cleanup()
        {
            for(int i = 0; i < StainBuffer_Number; ++i)
            {
                verts[i].cleanup();
            }
        }

        void render(int sbuf)
        {
            float colorscale = 1,
                  alphascale = 1;
            if(flags&StainFlag_Overbright)
            {
                glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
                SETVARIANT(overbrightstain, sbuf == StainBuffer_Transparent ? 0 : -1, 0);
            }
            else if(flags&StainFlag_Glow)
            {
                glBlendFunc(GL_ONE, GL_ONE);
                colorscale = ldrscale;
                if(flags&StainFlag_Saturate)
                {
                    colorscale *= 2;
                }
                alphascale = 0;
                SETSHADER(foggedstain);
            }
            else if(flags&StainFlag_InvMod)
            {
                glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                alphascale = 0;
                SETSHADER(foggedstain);
            }
            else
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                colorscale = ldrscale;
                if(flags&StainFlag_Saturate)
                {
                    colorscale *= 2;
                }
                SETVARIANT(stain, sbuf == StainBuffer_Transparent ? 0 : -1, 0);
            }
            LOCALPARAMF(colorscale, colorscale, colorscale, colorscale, alphascale);

            glBindTexture(GL_TEXTURE_2D, tex->id);

            verts[sbuf].render();
        }

        staininfo &newstain()
        {
            staininfo &d = stains[endstain];
            int next = endstain + 1;
            if(next>=maxstains)
            {
                next = 0;
            }
            if(next==startstain)
            {
                freestain();
            }
            endstain = next;
            return d;
        }

        ivec bbmin, bbmax;
        vec staincenter, stainnormal, staintangent, stainbitangent;
        float stainradius, stainu, stainv;
        bvec4 staincolor;

        void addstain(const vec &center, const vec &dir, float radius, const bvec &color, int info)
        {
            if(dir.iszero())
            {
                return;
            }
            bbmin = ivec(center).sub(radius);
            bbmax = ivec(center).add(radius).add(1);

            staincolor = bvec4(color, 255);
            staincenter = center;
            stainradius = radius;
            stainnormal = dir;

            staintangent = vec(dir.z, -dir.x, dir.y);
            staintangent.project(dir);

            if(flags&StainFlag_Rotate)
            {
                staintangent.rotate(sincos360[randomint(360)], dir);
            }
            staintangent.normalize();
            stainbitangent.cross(staintangent, dir);
            if(flags&StainFlag_Rnd4)
            {
                stainu = 0.5f*(info&1);
                stainv = 0.5f*((info>>1)&1);
            }

            for(int i = 0; i < StainBuffer_Number; ++i)
            {
                verts[i].lastvert = verts[i].endvert;
            }
            gentris(worldroot, ivec(0, 0, 0), worldsize>>1);
            for(int i = 0; i < StainBuffer_Number; ++i)
            {
                stainbuffer &buf = verts[i];
                if(buf.endvert == buf.lastvert)
                {
                    continue;
                }
                if(debugstain)
                {
                    int nverts = buf.nextverts();
                    static const char * const sbufname[StainBuffer_Number] = { "opaque", "transparent", "mapmodel" };
                    conoutf(Console_Debug, "tris = %d, verts = %d, total tris = %d, %s", nverts/3, nverts, buf.totaltris(), sbufname[i]);
                }

                staininfo &d = newstain();
                d.owner = i;
                d.color = color;
                d.millis = lastmillis;
                d.startvert = buf.lastvert;
                d.endvert = buf.endvert;
                buf.addstain(d);
            }
        }

        void genmmtri(const vec v[3]) // gen map model triangles
        {
            vec n;
            n.cross(v[0], v[1], v[2]).normalize();
            float facing = n.dot(stainnormal);
            if(facing <= 0)
            {
                return;
            }
            vec p = vec(v[0]).sub(staincenter);
            float dist = n.dot(p);
            if(std::fabs(dist) > stainradius)
            {
                return;
            }
            vec pcenter = vec(n).mul(dist).add(staincenter);
            vec ft, fb;
            ft.orthogonal(n);
            ft.normalize();
            fb.cross(ft, n);
            vec pt = vec(ft).mul(ft.dot(staintangent)).add(vec(fb).mul(fb.dot(staintangent))).normalize(),
                pb = vec(ft).mul(ft.dot(stainbitangent)).add(vec(fb).mul(fb.dot(stainbitangent))).project(pt).normalize();
            vec v1[3+4],
                v2[3+4];
            float ptc = pt.dot(pcenter),
                  pbc = pb.dot(pcenter);
            int numv = polyclip(v, 3, pt, ptc - stainradius, ptc + stainradius, v1);
            if(numv<3) //check with v1
            {
                return;
            }
            numv = polyclip(v1, numv, pb, pbc - stainradius, pbc + stainradius, v2);
            if(numv<3) //check again with v2
            {
                return;
            }
            float tsz = flags&StainFlag_Rnd4 ? 0.5f : 1.0f,
                  scale = tsz*0.5f/stainradius,
                  tu = stainu + tsz*0.5f - ptc*scale,
                  tv = stainv + tsz*0.5f - pbc*scale;
            pt.mul(scale); pb.mul(scale);
            stainvert dv1 = { v2[0], staincolor, vec2(pt.dot(v2[0]) + tu, pb.dot(v2[0]) + tv) },
                      dv2 = { v2[1], staincolor, vec2(pt.dot(v2[1]) + tu, pb.dot(v2[1]) + tv) };
            int totalverts = 3*(numv-2);
            stainbuffer &buf = verts[StainBuffer_Mapmodel];
            if(totalverts > buf.maxverts-3)
            {
                return;
            }
            while(buf.availverts < totalverts)
            {
                if(!freestain())
                {
                    return;
                }
            }
            for(int k = 0; k < numv-2; ++k)
            {
                stainvert *tri = buf.addtri();
                tri[0] = dv1;
                tri[1] = dv2;
                dv2.pos = v2[k+2];
                dv2.tc = vec2(pt.dot(v2[k+2]) + tu, pb.dot(v2[k+2]) + tv);
                tri[2] = dv2;
            }
        }

    private:
        const char *texname;

        void findmaterials(vtxarray *va)
        {
            materialsurface *matbuf = va->matbuf;
            int matsurfs = va->matsurfs;
            for(int i = 0; i < matsurfs; ++i)
            {
                materialsurface &m = matbuf[i];
                if(!IS_CLIPPED(m.material&MatFlag_Volume))
                {
                    i += m.skip;
                    continue;
                }
                int dim = DIMENSION(m.orient),
                    dc = DIM_COORD(m.orient);
                if(dc ? stainnormal[dim] <= 0 : stainnormal[dim] >= 0)
                {
                    i += m.skip;
                    continue;
                }
                int c = C[dim],
                    r = R[dim];
                for(;;)
                {
                    materialsurface &m = matbuf[i];
                    if(m.o[dim] >= bbmin[dim] && m.o[dim] <= bbmax[dim] &&
                       m.o[c] + m.csize >= bbmin[c] && m.o[c] <= bbmax[c] &&
                       m.o[r] + m.rsize >= bbmin[r] && m.o[r] <= bbmax[r])
                    {
                        static cube dummy;
                        gentris(dummy, m.orient, m.o, std::max(m.csize, m.rsize), &m);
                    }
                    if(i+1 >= matsurfs)
                    {
                        break;
                    }
                    materialsurface &n = matbuf[i+1];
                    if(n.material != m.material || n.orient != m.orient)
                    {
                        break;
                    }
                    i++;
                }
            }
        }

        void findescaped(cube *c, const ivec &o, int size, int escaped)
        {
            for(int i = 0; i < 8; ++i)
            {
                cube &cu = c[i];
                if(escaped&(1<<i))
                {
                    ivec co(i, o, size);
                    if(cu.children)
                    {
                        findescaped(cu.children, co, size>>1, cu.escaped);
                    }
                    else
                    {
                        int vismask = cu.merged;
                        if(vismask)
                        {
                            for(int j = 0; j < 6; ++j)
                            {
                                if(vismask&(1<<j))
                                {
                                    gentris(cu, j, co, size);
                                }
                            }
                        }
                    }
                }
            }
        }

        void gentris(cube *c, const ivec &o, int size, int escaped = 0)
        {
            int overlap = octaboxoverlap(o, size, bbmin, bbmax);
            for(int i = 0; i < 8; ++i)
            {
                cube &cu = c[i];
                if(overlap&(1<<i))
                {
                    ivec co(i, o, size);
                    if(cu.ext)
                    {
                        if(cu.ext->va && cu.ext->va->matsurfs)
                        {
                            findmaterials(cu.ext->va);
                        }
                        if(cu.ext->ents && cu.ext->ents->mapmodels.length())
                        {
                            genmmtris(*cu.ext->ents);
                        }
                    }
                    if(cu.children)
                    {
                        gentris(cu.children, co, size>>1, cu.escaped);
                    }
                    else
                    {
                        int vismask = cu.visible; //visibility mask
                        if(vismask&0xC0)
                        {
                            if(vismask&0x80)
                            {
                                for(int j = 0; j < 6; ++j)
                                {
                                    gentris(cu, j, co, size, nullptr, vismask);
                                }
                            }
                            else
                            {
                                for(int j = 0; j < 6; ++j)
                                {
                                    if(vismask&(1<<j))
                                    {
                                        gentris(cu, j, co, size);
                                    }
                                }
                            }
                        }
                    }
                }
                else if(escaped&(1<<i))
                {
                    ivec co(i, o, size);
                    if(cu.children)
                    {
                        findescaped(cu.children, co, size>>1, cu.escaped);
                    }
                    else
                    {
                        int vismask = cu.merged; //visibility mask
                        if(vismask)
                        {
                            for(int j = 0; j < 6; ++j)
                            {
                                if(vismask&(1<<j))
                                {
                                    gentris(cu, j, co, size);
                                }
                            }
                        }
                    }
                }
            }
        }

        void genmmtris(octaentities &oe)
        {
            const vector<extentity *> &ents = entities::getents();
            for(int i = 0; i < oe.mapmodels.length(); i++)
            {
                extentity &e = *ents[oe.mapmodels[i]];
                model *m = loadmapmodel(e.attr1);
                if(!m)
                {
                    continue;
                }
                vec center, radius;
                float rejectradius = m->collisionbox(center, radius),
                      scale = e.attr5 > 0 ? e.attr5/100.0f : 1;
                center.mul(scale);
                if(staincenter.reject(vec(e.o).add(center), stainradius + rejectradius*scale))
                {
                    continue;
                }
                if(m->animated() || (!m->bih && !m->setBIH()))
                {
                    continue;
                }
                int yaw = e.attr2,
                    pitch = e.attr3,
                    roll = e.attr4;
                m->bih->genstaintris(this, staincenter, stainradius, e.o, yaw, pitch, roll, scale);
            }
        }

        void gentris(cube &cu, int orient, const ivec &o, int size, materialsurface *mat = nullptr, int vismask = 0)
        {
            vec pos[Face_MaxVerts+4];
            int numverts = 0,
                numplanes = 1;
            vec planes[2];
            if(mat)
            {
                planes[0] = vec(0, 0, 0);
                switch(orient)
                {
                //want to define GENFACEORIENT and GENFACEVERT to pass the appropriate code to GENFACEVERTS
                //GENFACEVERTS has different GENFACEORIENT and GENFACEVERT for many different calls in other files
                #define GENFACEORIENT(orient, v0, v1, v2, v3) \
                    case orient: \
                        planes[0][DIMENSION(orient)] = DIM_COORD(orient) ? 1 : -1; \
                        v0 v1 v2 v3 \
                        break;
                #define GENFACEVERT(orient, vert, x,y,z, xv,yv,zv) \
                        pos[numverts++] = vec(x xv, y yv, z zv);
                    GENFACEVERTS(o.x, o.x, o.y, o.y, o.z, o.z, , + mat->csize, , + mat->rsize, + 0.1f, - 0.1f);
                #undef GENFACEORIENT
                #undef GENFACEVERT
                }
            }
            else if(cu.texture[orient] == Default_Sky)
            {
                return;
            }
            else if(cu.ext && (numverts = cu.ext->surfaces[orient].numverts&Face_MaxVerts))
            {
                vertinfo *verts = cu.ext->verts() + cu.ext->surfaces[orient].verts;
                ivec vo = ivec(o).mask(~0xFFF).shl(3);
                for(int j = 0; j < numverts; ++j)
                {
                    pos[j] = vec(verts[j].getxyz().add(vo)).mul(1/8.0f);
                }
                planes[0].cross(pos[0], pos[1], pos[2]).normalize();
                if(numverts >= 4 && !(cu.merged&(1<<orient)) && !flataxisface(cu, orient) && faceconvexity(verts, numverts, size))
                {
                    planes[1].cross(pos[0], pos[2], pos[3]).normalize();
                    numplanes++;
                }
            }
            else if(cu.merged&(1<<orient))
            {
                return;
            }
            else if(!vismask || (vismask&0x40 && visibleface(cu, orient, o, size, Mat_Air, (cu.material&Mat_Alpha)^Mat_Alpha, Mat_Alpha)))
            {
                ivec v[4];
                genfaceverts(cu, orient, v);
                int vis = 3,
                    convex = faceconvexity(v, vis),
                    order = convex < 0 ? 1 : 0;
                vec vo(o);
                pos[numverts++] = vec(v[order]).mul(size/8.0f).add(vo);
                if(vis&1)
                {
                    pos[numverts++] = vec(v[order+1]).mul(size/8.0f).add(vo);
                }
                pos[numverts++] = vec(v[order+2]).mul(size/8.0f).add(vo);
                if(vis&2)
                {
                    pos[numverts++] = vec(v[(order+3)&3]).mul(size/8.0f).add(vo);
                }
                planes[0].cross(pos[0], pos[1], pos[2]).normalize();
                if(convex)
                {
                    planes[1].cross(pos[0], pos[2], pos[3]).normalize();
                    numplanes++;
                }
            }
            else
            {
                return;
            }

            stainbuffer &buf = verts[mat || cu.material&Mat_Alpha ? StainBuffer_Transparent : StainBuffer_Opaque];
            for(int l = 0; l < numplanes; ++l) //note this is a loop l (level 4)
            {
                const vec &n = planes[l];
                float facing = n.dot(stainnormal);
                if(facing <= 0)
                {
                    continue;
                }
                vec p = vec(pos[0]).sub(staincenter);
                // travel back along plane normal from the stain center
                float dist = n.dot(p);
                if(std::fabs(dist) > stainradius)
                {
                    continue;
                }
                vec pcenter = vec(n).mul(dist).add(staincenter);
                vec ft, fb;
                ft.orthogonal(n);
                ft.normalize();
                fb.cross(ft, n);
                vec pt = vec(ft).mul(ft.dot(staintangent)).add(vec(fb).mul(fb.dot(staintangent))).normalize(),
                    pb = vec(ft).mul(ft.dot(stainbitangent)).add(vec(fb).mul(fb.dot(stainbitangent))).project(pt).normalize();
                vec v1[Face_MaxVerts+4],
                    v2[Face_MaxVerts+4];
                float ptc = pt.dot(pcenter),
                      pbc = pb.dot(pcenter);
                int numv;
                if(numplanes >= 2)
                {
                    if(l)
                    {
                        pos[1] = pos[2];
                        pos[2] = pos[3];
                    }
                    numv = polyclip(pos, 3, pt, ptc - stainradius, ptc + stainradius, v1);
                    if(numv<3)
                    {
                        continue;
                    }
                }
                else
                {
                    numv = polyclip(pos, numverts, pt, ptc - stainradius, ptc + stainradius, v1);
                    if(numv<3)
                    {
                        continue;
                    }
                }
                numv = polyclip(v1, numv, pb, pbc - stainradius, pbc + stainradius, v2);
                if(numv<3)
                {
                    continue;
                }
                float tsz = flags&StainFlag_Rnd4 ? 0.5f : 1.0f,
                      scale = tsz*0.5f/stainradius,
                      tu = stainu + tsz*0.5f - ptc*scale,
                      tv = stainv + tsz*0.5f - pbc*scale;
                pt.mul(scale); pb.mul(scale);
                stainvert dv1 = { v2[0], staincolor, vec2(pt.dot(v2[0]) + tu, pb.dot(v2[0]) + tv) },
                          dv2 = { v2[1], staincolor, vec2(pt.dot(v2[1]) + tu, pb.dot(v2[1]) + tv) };
                int totalverts = 3*(numv-2);
                if(totalverts > buf.maxverts-3)
                {
                    return;
                }
                while(buf.availverts < totalverts)
                {
                    if(!freestain())
                    {
                        return;
                    }
                }
                for(int k = 0; k < numv-2; ++k)
                {
                    stainvert *tri = buf.addtri();
                    tri[0] = dv1;
                    tri[1] = dv2;
                    dv2.pos = v2[k+2];
                    dv2.tc = vec2(pt.dot(v2[k+2]) + tu, pb.dot(v2[k+2]) + tv);
                    tri[2] = dv2;
                }
            }
        }
};

stainrenderer stains[] =
{
    stainrenderer("<grey>media/particle/blood.png", StainFlag_Rnd4|StainFlag_Rotate|StainFlag_InvMod),
    stainrenderer("<grey>media/particle/pulse_scorch.png", StainFlag_Rotate, 500),
    stainrenderer("<grey>media/particle/rail_hole.png", StainFlag_Rotate|StainFlag_Overbright),
    stainrenderer("<grey>media/particle/pulse_glow.png", StainFlag_Rotate|StainFlag_Glow|StainFlag_Saturate, 250, 1500, 250),
    stainrenderer("<grey>media/particle/rail_glow.png",  StainFlag_Rotate|StainFlag_Glow|StainFlag_Saturate, 100, 1100, 100)
};

/* initstains: sets up each entry in the stains global variable array using init() method
 * and then preloads them
 *
 * fails to do anything if initing is set (early game loading time)
 */
void initstains()
{
    if(initing)
    {
        return;
    }
    for(int i = 0; i < static_cast<int>(sizeof(stains)/sizeof(stains[0])); ++i)
    {
        stains[i].init(maxstaintris);
    }
    for(int i = 0; i < static_cast<int>(sizeof(stains)/sizeof(stains[0])); ++i)
    {
        loadprogress = static_cast<float>(i+1)/(sizeof(stains)/sizeof(stains[0]));
        stains[i].preload();
    }
    loadprogress = 0;
}

/* clearstains: loops through the stains[] global variable array and runs clearstains for each entry
 */
void clearstains()
{
    for(int i = 0; i < static_cast<int>(sizeof(stains)/sizeof(stains[0])); ++i)
    {
        stains[i].clearstains();
    }
}

VARNP(stains, showstains, 0, 1, 1); // toggles rendering stains at all

bool renderstains(int sbuf, bool gbuf, int layer)
{
    bool rendered = false;
    for(int i = 0; i < static_cast<int>(sizeof(stains)/sizeof(stains[0])); ++i)
    {
        stainrenderer &d = stains[i];
        if(d.usegbuffer() != gbuf)
        {
            continue;
        }
        if(sbuf == StainBuffer_Opaque)
        {
            d.clearfadedstains();
            d.fadeinstains();
            d.fadeoutstains();
        }
        if(!showstains || !d.hasstains(sbuf))
        {
            continue;
        }
        if(!rendered)
        {
            rendered = true;
            stainrenderer::setuprenderstate(sbuf, gbuf, layer);
        }
        d.render(sbuf);
    }
    if(!rendered)
    {
        return false;
    }
    stainrenderer::cleanuprenderstate(sbuf, gbuf, layer);
    return true;
}

void cleanupstains()
{
    for(int i = 0; i < static_cast<int>(sizeof(stains)/sizeof(stains[0])); ++i)
    {
        stains[i].cleanup();
    }
}

VARP(maxstaindistance, 1, 512, 10000); //distance in cubes before stains stop rendering

void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color, int info)
{
    if(!showstains || type<0 || static_cast<size_t>(type) >= sizeof(stains)/sizeof(stains[0]) || center.dist(camera1->o) - radius > maxstaindistance)
    {
        return;
    }
    stainrenderer &d = stains[type];
    d.addstain(center, surface, radius, color, info);
}

void genstainmmtri(stainrenderer *s, const vec v[3])
{
    s->genmmtri(v);
}


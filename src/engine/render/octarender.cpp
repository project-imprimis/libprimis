// octarender.cpp: fill vertex arrays with different cube surfaces.
#include "../libprimis-headers/cube.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "grass.h"
#include "octarender.h"
#include "rendergl.h"
#include "renderlights.h"
#include "renderparticles.h"
#include "rendersky.h"
#include "renderva.h"
#include "texture.h"

#include "interface/menus.h"

#include "world/light.h"
#include "world/material.h"
#include "world/octaworld.h"
#include "world/physics.h"
#include "world/world.h"


/* global variables */
//////////////////////

int allocva  = 0,
    wtris    = 0,
    wverts   = 0,
    vtris    = 0,
    vverts   = 0,
    glde     = 0,
    gbatches = 0;

vector<vtxarray *> valist, varoot;

ivec worldmin(0, 0, 0),
     worldmax(0, 0, 0);

std::vector<tjoint> tjoints;

VARFP(filltjoints, 0, 1, 1, rootworld.allchanged()); //eliminate "sparklies" by filling in geom t-joints

/* internally relevant functionality */
///////////////////////////////////////

namespace
{
    struct vboinfo
    {
        int uses;
        uchar *data;
    };

    hashtable<GLuint, vboinfo> vbos;

    VARFN(vbosize, maxvbosize, 0, 1<<14, 1<<16, rootworld.allchanged());

    //vbo (vertex buffer object) enum is local to this file
    enum
    {
        VBO_VBuf = 0,
        VBO_EBuf,
        VBO_SkyBuf,
        VBO_DecalBuf,
        VBO_NumVBOs
    };

    vector<uchar> vbodata[VBO_NumVBOs];
    vector<vtxarray *> vbovas[VBO_NumVBOs];
    int vbosize[VBO_NumVBOs];

    void destroyvbo(GLuint vbo)
    {
        vboinfo *exists = vbos.access(vbo);
        if(!exists)
        {
            return;
        }
        vboinfo &vbi = *exists;
        if(vbi.uses <= 0)
        {
            return;
        }
        vbi.uses--;
        if(!vbi.uses)
        {
            glDeleteBuffers_(1, &vbo);
            if(vbi.data)
            {
                delete[] vbi.data;
            }
            vbos.remove(vbo);
        }
    }

    //sets up vbos (vertex buffer objects) for the pointer-to-array-of-vtxarray **vas
    //by setting up each vertex array's vbuf and vdata
    void genvbo(int type, void *buf, int len, vtxarray **vas, int numva)
    {
        gle::disable();

        GLuint vbo;
        glGenBuffers_(1, &vbo);
        GLenum target = type==VBO_VBuf ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;
        glBindBuffer_(target, vbo);
        glBufferData_(target, len, buf, GL_STATIC_DRAW);
        glBindBuffer_(target, 0);

        vboinfo &vbi = vbos[vbo];
        vbi.uses = numva;
        vbi.data = new uchar[len];
        memcpy(vbi.data, buf, len);

        for(int i = 0; i < numva; ++i)
        {
            vtxarray *va = vas[i];
            switch(type)
            {
                case VBO_VBuf:
                {
                    va->vbuf = vbo;
                    va->vdata = reinterpret_cast<vertex *>(vbi.data);
                    break;
                }
                case VBO_EBuf:
                {
                    va->ebuf = vbo;
                    va->edata = reinterpret_cast<ushort *>(vbi.data);
                    break;
                }
                case VBO_SkyBuf:
                {
                    va->skybuf = vbo;
                    va->skydata = reinterpret_cast<ushort *>(vbi.data);
                    break;
                }
                case VBO_DecalBuf:
                {
                    va->decalbuf = vbo;
                    va->decaldata = reinterpret_cast<ushort *>(vbi.data);
                    break;
                }
            }
        }
    }

    void flushvbo(int type = -1)
    {
        if(type < 0)
        {
            for(int i = 0; i < VBO_NumVBOs; ++i)
            {
                flushvbo(i);
            }
            return;
        }

        vector<uchar> &data = vbodata[type];
        if(data.empty())
        {
            return;
        }
        vector<vtxarray *> &vas = vbovas[type];
        genvbo(type, data.getbuf(), data.length(), vas.getbuf(), vas.length());
        data.setsize(0);
        vas.setsize(0);
        vbosize[type] = 0;
    }

    uchar *addvbo(vtxarray *va, int type, int numelems, int elemsize)
    {
        switch(type)
        {
            case VBO_VBuf:
            {
                va->voffset = vbosize[type];
                break;
            }
            case VBO_EBuf:
            {
                va->eoffset = vbosize[type];
                break;
            }
            case VBO_SkyBuf:
            {
                va->skyoffset = vbosize[type];
                break;
            }
            case VBO_DecalBuf:
            {
                va->decaloffset = vbosize[type];
                break;
            }
        }
        vbosize[type] += numelems;
        vector<uchar> &data = vbodata[type];
        vector<vtxarray *> &vas = vbovas[type];
        vas.add(va);
        int len = numelems*elemsize;
        uchar *buf = data.reserve(len).buf;
        data.advance(len);
        return buf;
    }

    class verthash
    {
        public:
            std::vector<vertex> verts;

            verthash() { clearverts(); }

            int addvert(const vertex &v)
            {
                uint h = hthash(v.pos)&(hashsize-1);
                for(int i = table[h]; i>=0; i = chain[i])
                {
                    const vertex &c = verts[i];
                    if(c.pos==v.pos && c.tc==v.tc && c.norm==v.norm && c.tangent==v.tangent)
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

            void clearverts()
            {
                memset(table, -1, sizeof(table));
                chain.clear();
                verts.clear();
            }
        private:
            static const int hashsize = 1<<13;
            int table[hashsize];

            std::vector<int> chain;

            int addvert(const vec &pos, const vec &tc = vec(0, 0, 0), const bvec &norm = bvec(128, 128, 128), const vec4<uchar> &tangent = vec4<uchar>(128, 128, 128, 128))
            {
                vertex vtx;
                vtx.pos = pos;
                vtx.tc = tc;
                vtx.norm = norm;
                vtx.tangent = tangent;
                return addvert(vtx);
            }
    };

    //alpha enum local to this file
    enum
    {
        Alpha_None = 0,
        Alpha_Back,
        Alpha_Front,
        Alpha_Refract
    };

    class sortkey
    {
        public:
            ushort tex;
            uchar orient, layer, alpha;

            sortkey() {}
            sortkey(ushort tex, uchar orient, uchar layer = BlendLayer_Top, uchar alpha = Alpha_None)
             : tex(tex), orient(orient), layer(layer), alpha(alpha)
            {}

            bool operator==(const sortkey &o) const
            {
                return tex==o.tex && orient==o.orient && layer==o.layer && alpha==o.alpha;
            }

            static bool sort(const sortkey &x, const sortkey &y)
            {
                if(x.alpha < y.alpha)
                {
                    return true;
                }
                if(x.alpha > y.alpha)
                {
                    return false;
                }
                if(x.layer < y.layer)
                {
                    return true;
                }
                if(x.layer > y.layer)
                {
                    return false;
                }
                if(x.tex == y.tex)
                {
                    if(x.orient < y.orient)
                    {
                        return true;
                    }
                    if(x.orient > y.orient)
                    {
                        return false;
                    }
                    return false;
                }
                VSlot &xs = lookupvslot(x.tex, false),
                      &ys = lookupvslot(y.tex, false);
                if(xs.slot->shader < ys.slot->shader)
                {
                    return true;
                }
                if(xs.slot->shader > ys.slot->shader)
                {
                    return false;
                }
                if(xs.slot->params.length() < ys.slot->params.length())
                {
                    return true;
                }
                if(xs.slot->params.length() > ys.slot->params.length())
                {
                    return false;
                }
                if(x.tex < y.tex)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
    };

    inline bool htcmp(const sortkey &x, const sortkey &y)
    {
        return x == y;
    }

    inline uint hthash(const sortkey &k)
    {
        return k.tex;
    }

    struct decalkey
    {
        ushort tex, reuse;

        decalkey() {}
        decalkey(ushort tex, ushort reuse = 0)
         : tex(tex), reuse(reuse)
        {}

        bool operator==(const decalkey &o) const
        {
            return tex==o.tex && reuse==o.reuse;
        }

        static bool sort(const decalkey &x, const decalkey &y)
        {
            if(x.tex == y.tex)
            {
                if(x.reuse < y.reuse)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            DecalSlot &xs = lookupdecalslot(x.tex, false),
                      &ys = lookupdecalslot(y.tex, false);
            if(xs.slot->shader < ys.slot->shader)
            {
                return true;
            }
            if(xs.slot->shader > ys.slot->shader)
            {
                return false;
            }
            if(xs.slot->params.length() < ys.slot->params.length())
            {
                return true;
            }
            if(xs.slot->params.length() > ys.slot->params.length())
            {
                return false;
            }
            if(x.tex < y.tex)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    };

    inline bool htcmp(const decalkey &x, const decalkey &y)
    {
        return x == y;
    }

    inline uint hthash(const decalkey &k)
    {
        return k.tex;
    }

    struct sortval
    {
         vector<ushort> tris;

         sortval() {}
    };

    class vacollect : public verthash
    {
        public:
            int size;
            ivec origin;
            std::vector<materialsurface> matsurfs;
            vector<octaentities *> mapmodels, decals, extdecals;
            vec skymin, skymax;
            vec alphamin, alphamax;
            vec refractmin, refractmax;
            vector<grasstri> grasstris;
            int worldtris, skytris;
            vector<ushort> skyindices;
            hashtable<sortkey, sortval> indices;

            void clear()
            {
                clearverts();
                worldtris = skytris = decaltris = 0;
                indices.clear();
                decalindices.clear();
                skyindices.setsize(0);
                matsurfs.clear();
                mapmodels.setsize(0);
                decals.setsize(0);
                extdecals.setsize(0);
                grasstris.setsize(0);
                texs.setsize(0);
                decaltexs.setsize(0);
                alphamin = refractmin = skymin = vec(1e16f, 1e16f, 1e16f);
                alphamax = refractmax = skymax = vec(-1e16f, -1e16f, -1e16f);
            }

            void setupdata(vtxarray *va)
            {
                optimize();
                gendecals();

                va->verts = verts.size();
                va->tris = worldtris/3;
                va->vbuf = 0;
                va->vdata = 0;
                va->minvert = 0;
                va->maxvert = va->verts-1;
                va->voffset = 0;
                if(va->verts)
                {
                    if(vbosize[VBO_VBuf] + static_cast<int>(verts.size()) > maxvbosize ||
                       vbosize[VBO_EBuf] + worldtris > USHRT_MAX ||
                       vbosize[VBO_SkyBuf] + skytris > USHRT_MAX ||
                       vbosize[VBO_DecalBuf] + decaltris > USHRT_MAX)
                    {
                        flushvbo();
                    }
                    uchar *vdata = addvbo(va, VBO_VBuf, va->verts, sizeof(vertex));
                    genverts(vdata);
                    va->minvert += va->voffset;
                    va->maxvert += va->voffset;
                }

                va->matbuf = nullptr;
                va->matsurfs = matsurfs.size();
                va->matmask = 0;
                if(va->matsurfs)
                {
                    va->matbuf = new materialsurface[matsurfs.size()];
                    memcpy(va->matbuf, matsurfs.data(), matsurfs.size()*sizeof(materialsurface));
                    for(uint i = 0; i < matsurfs.size(); i++)
                    {
                        materialsurface &m = matsurfs[i];
                        if(m.visible == MatSurf_EditOnly)
                        {
                            continue;
                        }
                        switch(m.material)
                        {
                            case Mat_Glass:
                            case Mat_Water:
                            {
                                break;
                            }
                            default:
                            {
                                continue;
                            }
                        }
                        va->matmask |= 1<<m.material;
                    }
                }

                va->skybuf = 0;
                va->skydata = 0;
                va->skyoffset = 0;
                va->sky = skyindices.length();
                if(va->sky)
                {
                    ushort *skydata = reinterpret_cast<ushort *>(addvbo(va, VBO_SkyBuf, va->sky, sizeof(ushort)));
                    memcpy(skydata, skyindices.getbuf(), va->sky*sizeof(ushort));
                    if(va->voffset)
                    {
                        for(int i = 0; i < va->sky; ++i)
                        {
                            skydata[i] += va->voffset;
                        }
                    }
                }

                va->texelems = nullptr;
                va->texs = texs.length();
                va->alphabacktris = 0;
                va->alphaback = 0;
                va->alphafronttris = 0;
                va->alphafront = 0;
                va->refracttris = 0;
                va->refract = 0;
                va->ebuf = 0;
                va->edata = 0;
                va->eoffset = 0;
                if(va->texs)
                {
                    va->texelems = new elementset[va->texs];
                    ushort *edata = reinterpret_cast<ushort *>(addvbo(va, VBO_EBuf, worldtris, sizeof(ushort))),
                           *curbuf = edata;
                    for(int i = 0; i < texs.length(); i++)
                    {
                        const sortkey &k = texs[i];
                        const sortval &t = indices[k];
                        elementset &e = va->texelems[i];
                        e.texture = k.tex;
                        e.orient = k.orient;
                        e.layer = k.layer;
                        ushort *startbuf = curbuf;
                        e.minvert = USHRT_MAX;
                        e.maxvert = 0;

                        if(t.tris.length())
                        {
                            memcpy(curbuf, t.tris.getbuf(), t.tris.length() * sizeof(ushort));
                            for(int j = 0; j < t.tris.length(); j++)
                            {
                                curbuf[j] += va->voffset;
                                e.minvert = std::min(e.minvert, curbuf[j]);
                                e.maxvert = std::max(e.maxvert, curbuf[j]);
                            }
                            curbuf += t.tris.length();
                        }
                        e.length = curbuf-startbuf;
                        if(k.alpha==Alpha_Back)
                        {
                            va->texs--;
                            va->tris -= e.length/3;
                            va->alphaback++;
                            va->alphabacktris += e.length/3;
                        }
                        else if(k.alpha==Alpha_Front)
                        {
                            va->texs--;
                            va->tris -= e.length/3;
                            va->alphafront++;
                            va->alphafronttris += e.length/3;
                        }
                        else if(k.alpha==Alpha_Refract)
                        {
                            va->texs--;
                            va->tris -= e.length/3;
                            va->refract++;
                            va->refracttris += e.length/3;
                        }
                    }
                }

                va->texmask = 0;
                va->dyntexs = 0;
                for(int i = 0; i < (va->texs+va->alphaback+va->alphafront+va->refract); ++i)
                {
                    VSlot &vslot = lookupvslot(va->texelems[i].texture, false);
                    if(vslot.isdynamic())
                    {
                        va->dyntexs++;
                    }
                    Slot &slot = *vslot.slot;
                    for(int j = 0; j < slot.sts.length(); j++)
                    {
                        va->texmask |= 1<<slot.sts[j].type;
                    }
                }

                va->decalbuf = 0;
                va->decaldata = 0;
                va->decaloffset = 0;
                va->decalelems = nullptr;
                va->decaltexs = decaltexs.length();
                va->decaltris = decaltris/3;
                if(va->decaltexs)
                {
                    va->decalelems = new elementset[va->decaltexs];
                    ushort *edata = reinterpret_cast<ushort *>(addvbo(va, VBO_DecalBuf, decaltris, sizeof(ushort))),
                           *curbuf = edata;
                    for(int i = 0; i < decaltexs.length(); i++)
                    {
                        const decalkey &k = decaltexs[i];
                        const sortval &t = decalindices[k];
                        elementset &e = va->decalelems[i];
                        e.texture = k.tex;
                        e.reuse = k.reuse;
                        ushort *startbuf = curbuf;
                        e.minvert = USHRT_MAX;
                        e.maxvert = 0;
                        if(t.tris.length())
                        {
                            memcpy(curbuf, t.tris.getbuf(), t.tris.length() * sizeof(ushort));
                            for(int j = 0; j < t.tris.length(); j++)
                            {
                                curbuf[j] += va->voffset;
                                e.minvert = std::min(e.minvert, curbuf[j]);
                                e.maxvert = std::max(e.maxvert, curbuf[j]);
                            }
                            curbuf += t.tris.length();
                        }
                        e.length = curbuf-startbuf;
                    }
                }
                if(grasstris.length())
                {
                    va->grasstris.move(grasstris);
                    loadgrassshaders();
                }
                if(mapmodels.length())
                {
                    va->mapmodels.put(mapmodels.getbuf(), mapmodels.length());
                }
                if(decals.length())
                {
                    va->decals.put(decals.getbuf(), decals.length());
                }
            }

            bool emptyva()
            {
                return verts.empty() && matsurfs.empty() && skyindices.empty() && grasstris.empty() && mapmodels.empty() && decals.empty();
            }

        private:
            hashtable<decalkey, sortval> decalindices;
            vector<sortkey> texs;
            vector<decalkey> decaltexs;
            int decaltris;

            void optimize()
            {
                ENUMERATE_KT(indices, sortkey, k, sortval, t,
                {
                    if(t.tris.length())
                    {
                        texs.add(k);
                    }
                });
                texs.sort(sortkey::sort);

                matsurfs.resize(optimizematsurfs(matsurfs.data(), matsurfs.size()));
            }

            void genverts(void *buf)
            {
                vertex *f = reinterpret_cast<vertex *>(buf);
                for(vertex i : verts)
                {
                    const vertex &v = i;
                    *f = v;
                    f->norm.flip();
                    f->tangent.flip();
                    f++;
                }
            }

            void gendecal(const extentity &e, DecalSlot &s, const decalkey &key)
            {
                matrix3 orient;
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
                vec size(std::max(static_cast<float>(e.attr5), 1.0f));
                size.y *= s.depth;
                if(!s.sts.empty())
                {
                    Texture *t = s.sts[0].t;
                    if(t->xs < t->ys)
                    {
                        size.x *= t->xs / static_cast<float>(t->ys);
                    }
                    else if(t->xs > t->ys)
                    {
                        size.z *= t->ys / static_cast<float>(t->xs);
                    }
                }
                vec center = orient.transform(vec(0, size.y*0.5f, 0)).add(e.o),
                    radius = orient.abstransform(vec(size).mul(0.5f)),
                    bbmin = vec(center).sub(radius),
                    bbmax = vec(center).add(radius),
                    clipoffset = orient.transposedtransform(center).msub(size, 0.5f);
                for(int i = 0; i < texs.length(); i++)
                {
                    const sortkey &k = texs[i];
                    if(k.layer == BlendLayer_Blend || k.alpha != Alpha_None)
                    {
                        continue;
                    }
                    const sortval &t = indices[k];
                    if(t.tris.empty())
                    {
                        continue;
                    }
                    decalkey tkey(key);
                    if(shouldreuseparams(s, lookupvslot(k.tex, false)))
                    {
                        tkey.reuse = k.tex;
                    }
                    for(int j = 0; j < t.tris.length(); j += 3)
                    {
                        const vertex &t0 = verts[t.tris[j]],
                                     &t1 = verts[t.tris[j+1]],
                                     &t2 = verts[t.tris[j+2]];
                        vec v0 = t0.pos,
                            v1 = t1.pos,
                            v2 = t2.pos,
                            tmin = vec(v0).min(v1).min(v2),
                            tmax = vec(v0).max(v1).max(v2);
                        if(tmin.x >= bbmax.x || tmin.y >= bbmax.y || tmin.z >= bbmax.z ||
                           tmax.x <= bbmin.x || tmax.y <= bbmin.y || tmax.z <= bbmin.z)
                        {
                            continue;
                        }
                        float f0 = t0.norm.tonormal().dot(orient.b),
                              f1 = t1.norm.tonormal().dot(orient.b),
                              f2 = t2.norm.tonormal().dot(orient.b);
                        if(f0 >= 0 && f1 >= 0 && f2 >= 0)
                        {
                            continue;
                        }
                        vec p1[9], p2[9];
                        p1[0] = v0;
                        p1[1] = v1;
                        p1[2] = v2;
                        int nump = polyclip(p1, 3, orient.b, clipoffset.y, clipoffset.y + size.y, p2);
                        if(nump < 3)
                        {
                            continue;
                        }
                        nump = polyclip(p2, nump, orient.a, clipoffset.x, clipoffset.x + size.x, p1);
                        if(nump < 3)
                        {
                            continue;
                        }
                        nump = polyclip(p1, nump, orient.c, clipoffset.z, clipoffset.z + size.z, p2);
                        if(nump < 3)
                        {
                            continue;
                        }
                        vec4<uchar> n0 = t0.norm,
                              n1 = t1.norm,
                              n2 = t2.norm,
                              x0 = t0.tangent,
                              x1 = t1.tangent,
                              x2 = t2.tangent;
                        vec e1 = vec(v1).sub(v0),
                            e2 = vec(v2).sub(v0);
                        float d11 = e1.dot(e1),
                              d12 = e1.dot(e2),
                              d22 = e2.dot(e2);
                        int idx[9];
                        for(int k = 0; k < nump; ++k)
                        {
                            vertex v;
                            v.pos = p2[k];
                            vec ep = vec(v.pos).sub(v0);
                            float dp1 = ep.dot(e1),
                                  dp2 = ep.dot(e2),
                                  denom = d11*d22 - d12*d12,
                                  b1 = (d22*dp1 - d12*dp2) / denom,
                                  b2 = (d11*dp2 - d12*dp1) / denom,
                                  b0 = 1 - b1 - b2;
                            v.norm.lerp(n0, n1, n2, b0, b1, b2);
                            v.norm.w = static_cast<uchar>(127.5f - 127.5f*(f0*b0 + f1*b1 + f2*b2));
                            vec tc = orient.transposedtransform(vec(center).sub(v.pos)).div(size).add(0.5f);
                            v.tc = vec(tc.x, tc.z, s.fade ? tc.y * s.depth / s.fade : 1.0f);
                            v.tangent.lerp(x0, x1, x2, b0, b1, b2);
                            idx[k] = addvert(v);
                        }
                        vector<ushort> &tris = decalindices[tkey].tris;
                        for(int k = 0; k < nump-2; ++k)
                        {
                            if(idx[0] != idx[k+1] && idx[k+1] != idx[k+2] && idx[k+2] != idx[0])
                            {
                                tris.add(idx[0]);
                                tris.add(idx[k+1]);
                                tris.add(idx[k+2]);
                                decaltris += 3;
                            }
                        }
                    }
                }
            }

            void gendecals()
            {
                if(decals.length())
                {
                    extdecals.put(decals.getbuf(), decals.length());
                }
                if(extdecals.empty())
                {
                    return;
                }
                vector<extentity *> &ents = entities::getents();
                for(int i = 0; i < extdecals.length(); i++)
                {
                    octaentities *oe = extdecals[i];
                    for(int j = 0; j < oe->decals.length(); j++)
                    {
                        extentity &e = *ents[oe->decals[j]];
                        if(e.flags&EntFlag_Render)
                        {
                            continue;
                        }
                        e.flags |= EntFlag_Render;
                        DecalSlot &s = lookupdecalslot(e.attr1, true);
                        if(!s.shader)
                        {
                            continue;
                        }
                        decalkey k(e.attr1);
                        gendecal(e, s, k);
                    }
                }
                for(int i = 0; i < extdecals.length(); i++)
                {
                    octaentities *oe = extdecals[i];
                    for(int j = 0; j < oe->decals.length(); j++)
                    {
                        extentity &e = *ents[oe->decals[j]];
                        if(e.flags&EntFlag_Render)
                        {
                            e.flags &= ~EntFlag_Render;
                        }
                    }
                }
                ENUMERATE_KT(decalindices, decalkey, k, sortval, t,
                {
                    if(t.tris.length())
                    {
                        decaltexs.add(k);
                    }
                });
                decaltexs.sort(decalkey::sort);
            }
    } vc;

    int recalcprogress = 0;

    // [rotation][orient]
    const vec orientation_tangent[8][6] =
    {
        { vec( 0,  1,  0), vec( 0, -1,  0), vec(-1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0) },
        { vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0, -1,  0), vec( 0,  1,  0) },
        { vec( 0, -1,  0), vec( 0,  1,  0), vec( 1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0) },
        { vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  1,  0), vec( 0, -1,  0) },
        { vec( 0, -1,  0), vec( 0,  1,  0), vec( 1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0) },
        { vec( 0,  1,  0), vec( 0, -1,  0), vec(-1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0) },
        { vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0, -1,  0), vec( 0,  1,  0) },
        { vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  1,  0), vec( 0, -1,  0) },
    };

    const vec orientation_bitangent[8][6] =
    {
        { vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0, -1,  0), vec( 0,  1,  0) },
        { vec( 0, -1,  0), vec( 0,  1,  0), vec( 1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0) },
        { vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  1,  0), vec( 0, -1,  0) },
        { vec( 0,  1,  0), vec( 0, -1,  0), vec(-1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0) },
        { vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0, -1,  0), vec( 0,  1,  0) },
        { vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  1,  0), vec( 0, -1,  0) },
        { vec( 0,  1,  0), vec( 0, -1,  0), vec(-1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0) },
        { vec( 0, -1,  0), vec( 0,  1,  0), vec( 1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0) },
    };

    void addtris(VSlot &vslot, int orient, const sortkey &key, vertex *verts, int *index, int numverts, int tj)
    {
        int &total = key.tex == Default_Sky ? vc.skytris : vc.worldtris,
             edge  = orient*(Face_MaxVerts+1);
        for(int i = 0; i < numverts-2; ++i)
        {
            if(index[0]!=index[i+1] && index[i+1]!=index[i+2] && index[i+2]!=index[0])
            {
                vector<ushort> &idxs = key.tex == Default_Sky ? vc.skyindices : vc.indices[key].tris;
                int left  = index[0],
                    mid   = index[i+1],
                    right = index[i+2],
                    start = left,
                    i0    = left,
                    i1    = -1;
                for(int k = 0; k < 4; ++k)
                {
                    int i2    = -1,
                        ctj   = -1,
                        cedge = -1;
                    switch(k)
                    {
                        case 1:
                        {
                            i1 = i2 = mid;
                            cedge = edge+i+1;
                            break;
                        }
                        case 2:
                        {
                            if(i1 != mid || i0 == left)
                            {
                                i0 = i1;
                                i1 = right;
                            }
                            i2 = right;
                            if(i+1 == numverts-2)
                            {
                                cedge = edge+i+2;
                            }
                            break;
                        }
                        case 3:
                        {
                            if(i0 == start)
                            {
                                i0 = i1;
                                i1 = left;
                            }
                            i2 = left;
                        }
                        [[fallthrough]];
                        default:
                        {
                            if(!i)
                            {
                                cedge = edge;
                            }
                            break;
                        }
                    }
                    if(i1 != i2)
                    {
                        if(total + 3 > USHRT_MAX)
                        {
                            return;
                        }
                        total += 3;
                        idxs.add(i0);
                        idxs.add(i1);
                        idxs.add(i2);
                        i1 = i2;
                    }
                    if(cedge >= 0)
                    {
                        for(ctj = tj;;)
                        {
                            if(ctj < 0)
                            {
                                break;
                            }
                            if(tjoints[ctj].edge < cedge)
                            {
                                ctj = tjoints[ctj].next;
                                continue;
                            }
                            if(tjoints[ctj].edge != cedge)
                            {
                                ctj = -1;
                            }
                            break;
                        }
                    }
                    if(ctj >= 0)
                    {
                        int e1 = cedge%(Face_MaxVerts+1),
                            e2 = (e1+1)%numverts;
                        vertex &v1 = verts[e1],
                               &v2 = verts[e2];
                        ivec d(vec(v2.pos).sub(v1.pos).mul(8));
                        int axis = std::abs(d.x) > std::abs(d.y) ? (std::abs(d.x) > std::abs(d.z) ? 0 : 2) : (std::abs(d.y) > std::abs(d.z) ? 1 : 2);
                        if(d[axis] < 0)
                        {
                            d.neg();
                        }
                        reduceslope(d);
                        int origin =  static_cast<int>(std::min(v1.pos[axis], v2.pos[axis])*8)&~0x7FFF,
                            offset1 = (static_cast<int>(v1.pos[axis]*8) - origin) / d[axis],
                            offset2 = (static_cast<int>(v2.pos[axis]*8) - origin) / d[axis];
                        vec o = vec(v1.pos).sub(vec(d).mul(offset1/8.0f));
                        float doffset = 1.0f / (offset2 - offset1);

                        if(i1 < 0)
                        {
                            for(;;)
                            {
                                tjoint &t = tjoints[ctj];
                                if(t.next < 0 || tjoints[t.next].edge != cedge)
                                {
                                    break;
                                }
                                ctj = t.next;
                            }
                        }
                        while(ctj >= 0)
                        {
                            tjoint &t = tjoints[ctj];
                            if(t.edge != cedge)
                            {
                                break;
                            }
                            float offset = (t.offset - offset1) * doffset;
                            vertex vt;
                            vt.pos = vec(d).mul(t.offset/8.0f).add(o);
                            vt.tc.lerp(v1.tc, v2.tc, offset);
                            vt.norm.lerp(v1.norm, v2.norm, offset);
                            vt.tangent.lerp(v1.tangent, v2.tangent, offset);
                            if(v1.tangent.w != v2.tangent.w)
                            {
                                vt.tangent.w = orientation_bitangent[vslot.rotation][orient].scalartriple(vt.norm.tonormal(), vt.tangent.tonormal()) < 0 ? 0 : 255;
                            }
                            int i2 = vc.addvert(vt);
                            if(i2 < 0)
                            {
                                return;
                            }
                            if(i1 >= 0)
                            {
                                if(total + 3 > USHRT_MAX)
                                {
                                    return;
                                }
                                total += 3;
                                idxs.add(i0);
                                idxs.add(i1);
                                idxs.add(i2);
                                i1 = i2;
                            }
                            else
                            {
                                start = i0 = i2;
                            }
                            ctj = t.next;
                        }
                    }
                }
            }
        }
    }

    //face: index of which face to cover
    //verts: information about grass texs' face
    //numv: number of grass vertices
    //texture: index for the grass texture to use
    void addgrasstri(int face, vertex *verts, int numv, ushort texture)
    {
        grasstri &g = vc.grasstris.add();
        int i1, i2, i3, i4;
        if(numv <= 3 && face%2)
        {
            i1 = face+1;
            i2 = face+2;
            i3 = i4 = 0;
        }
        else
        {
            i1 = 0;
            i2 = face+1;
            i3 = face+2;
            i4 = numv > 3 ? face+3 : i3;
        }
        g.v[0] = verts[i1].pos;
        g.v[1] = verts[i2].pos;
        g.v[2] = verts[i3].pos;
        g.v[3] = verts[i4].pos;
        g.numv = numv;
        g.surface.toplane(g.v[0], g.v[1], g.v[2]);
        if(g.surface.z <= 0)
        {
            vc.grasstris.pop();
            return;
        }
        g.minz = std::min(std::min(g.v[0].z, g.v[1].z), std::min(g.v[2].z, g.v[3].z));
        g.maxz = std::max(std::max(g.v[0].z, g.v[1].z), std::max(g.v[2].z, g.v[3].z));
        g.center = vec(0, 0, 0);
        for(int k = 0; k < numv; ++k)
        {
            g.center.add(g.v[k]);
        }
        g.center.div(numv);
        g.radius = 0;
        for(int k = 0; k < numv; ++k)
        {
            g.radius = std::max(g.radius, g.v[k].dist(g.center));
        }
        g.texture = texture;
    }

    void calctexgen(VSlot &vslot, int orient, vec4<float> &sgen, vec4<float> &tgen)
    {
        Texture *tex = vslot.slot->sts.empty() ? notexture : vslot.slot->sts[0].t;
        const texrotation &r = texrotations[vslot.rotation];
        float k = defaulttexscale/vslot.scale,
              xs = r.flipx ? -tex->xs : tex->xs,
              ys = r.flipy ? -tex->ys : tex->ys,
              sk = k/xs, tk = k/ys,
              soff = -(r.swapxy ? vslot.offset.y : vslot.offset.x)/xs,
              toff = -(r.swapxy ? vslot.offset.x : vslot.offset.y)/ys;
        sgen = vec4<float>(0, 0, 0, soff);
        tgen = vec4<float>(0, 0, 0, toff);
        if(r.swapxy)
        {
            switch(orient)
            {
                case 0:
                {
                    sgen.z = -sk;
                    tgen.y =  tk;
                    break;
                }
                case 1:
                {
                    sgen.z = -sk;
                    tgen.y = -tk;
                    break;
                }
                case 2:
                {
                    sgen.z = -sk;
                    tgen.x = -tk;
                    break;
                }
                case 3:
                {
                    sgen.z = -sk;
                    tgen.x =  tk;
                    break;
                }
                case 4:
                {
                    sgen.y = -sk;
                    tgen.x =  tk;
                    break;
                }
                case 5:
                {
                    sgen.y =  sk;
                    tgen.x =  tk;
                    break;
                }
            }
        }
        else
        {
            switch(orient)
            {
                case 0:
                {
                    sgen.y =  sk;
                    tgen.z = -tk;
                    break;
                }
                case 1:
                {
                    sgen.y = -sk;
                    tgen.z = -tk;
                    break;
                }
                case 2:
                {
                    sgen.x = -sk;
                    tgen.z = -tk;
                    break;
                }
                case 3:
                {
                    sgen.x =  sk;
                    tgen.z = -tk;
                    break;
                }
                case 4:
                {
                    sgen.x =  sk;
                    tgen.y = -tk;
                    break;
                }
                case 5:
                {
                    sgen.x =  sk;
                    tgen.y =  tk;
                    break;
                }
            }
        }
    }

    //takes a packed ushort vector and turns it into a vec3 vector object
    vec decodenormal(ushort norm)
    {
        if(!norm)
        {
            return vec(0, 0, 1);
        }
        norm--;
        const vec2 &yaw = sincos360[norm%360],
                   &pitch = sincos360[norm/360+270];
        return vec(-yaw.y*pitch.x, yaw.x*pitch.x, pitch.y);
    }

    void addcubeverts(VSlot &vslot, int orient, vec *pos, ushort texture, vertinfo *vinfo, int numverts, int tj = -1, int grassy = 0, bool alpha = false, int layer = BlendLayer_Top)
    {
        vec4<float> sgen, tgen;
        calctexgen(vslot, orient, sgen, tgen);
        vertex verts[Face_MaxVerts];
        int index[Face_MaxVerts];
        vec normals[Face_MaxVerts];
        for(int k = 0; k < numverts; ++k)
        {
            vertex &v = verts[k];
            v.pos = pos[k];
            v.tc = vec(sgen.dot(v.pos), tgen.dot(v.pos), 0);
            if(vinfo && vinfo[k].norm)
            {
                vec n = decodenormal(vinfo[k].norm),
                    t = orientation_tangent[vslot.rotation][orient];
                t.project(n).normalize();
                v.norm = bvec(n);
                v.tangent = vec4<uchar>(bvec(t), orientation_bitangent[vslot.rotation][orient].scalartriple(n, t) < 0 ? 0 : 255);
            }
            else if(texture != Default_Sky)
            {
                if(!k)
                {
                    guessnormals(pos, numverts, normals);
                }
                const vec &n = normals[k];
                vec t = orientation_tangent[vslot.rotation][orient];
                t.project(n).normalize();
                v.norm = bvec(n);
                v.tangent = vec4<uchar>(bvec(t), orientation_bitangent[vslot.rotation][orient].scalartriple(n, t) < 0 ? 0 : 255);
            }
            else
            {
                v.norm = bvec(128, 128, 255);
                v.tangent = vec4<uchar>(255, 128, 128, 255);
            }
            index[k] = vc.addvert(v);
            if(index[k] < 0)
            {
                return;
            }
        }

        if(alpha)
        {
            for(int k = 0; k < numverts; ++k)
            {
                vc.alphamin.min(pos[k]);
                vc.alphamax.max(pos[k]);
            }
            if(vslot.refractscale > 0)
            {
                for(int k = 0; k < numverts; ++k)
                {
                    vc.refractmin.min(pos[k]);
                    vc.refractmax.max(pos[k]);
                }
            }
        }
        if(texture == Default_Sky)
        {
            for(int i = 0; i < numverts; ++i)
            {
                if(pos[i][orient>>1] != ((orient&1)<<worldscale))
                {
                    for(int k = 0; k < numverts; ++k)
                    {
                        vc.skymin.min(pos[k]);
                        vc.skymax.max(pos[k]);
                    }
                    break;
                }
            }
        }
        sortkey key(texture, vslot.scroll.iszero() ? Orient_Any : orient, layer&BlendLayer_Bottom ? layer : BlendLayer_Top, alpha ? (vslot.refractscale > 0 ? Alpha_Refract : (vslot.alphaback ? Alpha_Back : Alpha_Front)) : Alpha_None);
        addtris(vslot, orient, key, verts, index, numverts, tj);
        if(grassy)
        {
            for(int i = 0; i < numverts-2; i += 2)
            {
                int faces = 0;
                if(index[0]!=index[i+1] && index[i+1]!=index[i+2] && index[i+2]!=index[0])
                {
                    faces |= 1;
                }
                if(i+3 < numverts && index[0]!=index[i+2] && index[i+2]!=index[i+3] && index[i+3]!=index[0])
                {
                    faces |= 2;
                }
                if(grassy > 1 && faces==3)
                {
                    addgrasstri(i, verts, 4, texture);
                }
                else
                {
                    if(faces&1)
                    {
                        addgrasstri(i, verts, 3, texture);
                    }
                    if(faces&2)
                    {
                        addgrasstri(i+1, verts, 3, texture);
                    }
                }
            }
        }
    }

    //edgegroup: struct used for tjoint joining (to reduce sparklies between geom faces)
    struct edgegroup
    {
        ivec slope, origin;
        int axis;
    };

    inline uint hthash(const edgegroup &g)
    {
        return g.slope.x^g.slope.y^g.slope.z^g.origin.x^g.origin.y^g.origin.z;
    }

    inline bool htcmp(const edgegroup &x, const edgegroup &y)
    {
        return x.slope==y.slope && x.origin==y.origin;
    }

    enum
    {
        CubeEdge_Start = 1<<0,
        CubeEdge_End   = 1<<1,
        CubeEdge_Flip  = 1<<2,
        CubeEdge_Dup   = 1<<3
    };

    struct cubeedge
    {
        cube *c;
        int next, offset;
        ushort size;
        uchar index, flags;
    };

    std::vector<cubeedge> cubeedges;
    hashtable<edgegroup, int> edgegroups(1<<13);

    void gencubeedges(cube &c, const ivec &co, int size)
    {
        ivec pos[Face_MaxVerts];
        int vis;
        for(int i = 0; i < 6; ++i)
        {
            if((vis = visibletris(c, i, co, size)))
            {
                int numverts = c.ext ? c.ext->surfaces[i].numverts&Face_MaxVerts : 0;
                if(numverts)
                {
                    vertinfo *verts = c.ext->verts() + c.ext->surfaces[i].verts;
                    ivec vo = ivec(co).mask(~0xFFF).shl(3);
                    for(int j = 0; j < numverts; ++j)
                    {
                        vertinfo &v = verts[j];
                        pos[j] = ivec(v.x, v.y, v.z).add(vo);
                    }
                }
                else if(c.merged&(1<<i))
                {
                    continue;
                }
                else
                {
                    ivec v[4];
                    genfaceverts(c, i, v);
                    int order = vis&4 || (!flataxisface(c, i) && faceconvexity(v) < 0) ? 1 : 0;
                    ivec vo = ivec(co).shl(3);
                    pos[numverts++] = v[order].mul(size).add(vo);
                    if(vis&1)
                    {
                        pos[numverts++] = v[order+1].mul(size).add(vo);
                    }
                    pos[numverts++] = v[order+2].mul(size).add(vo);
                    if(vis&2)
                    {
                        pos[numverts++] = v[(order+3)&3].mul(size).add(vo);
                    }
                }
                for(int j = 0; j < numverts; ++j)
                {
                    int e1 = j,
                        e2 = j+1 < numverts ? j+1 : 0;
                    ivec d = pos[e2];
                    d.sub(pos[e1]);
                    if(d.iszero())
                    {
                        continue;
                    }
                    //axistemp1/2 used in `axis` only
                    // x = 0, y = 1, z = 2, `int axis` is the largest component of `d` using
                    // axistemp1/2 to determine if x,y > z and then if x > y
                    int axistemp1 = std::abs(d.x) > std::abs(d.z) ? 0 : 2,
                        axistemp2 = std::abs(d.y) > std::abs(d.z) ? 1 : 2,
                        axis = std::abs(d.x) > std::abs(d.y) ? axistemp1 : axistemp2;
                    if(d[axis] < 0)
                    {
                        d.neg();
                        std::swap(e1, e2);
                    }
                    reduceslope(d);
                    int t1 = pos[e1][axis]/d[axis],
                        t2 = pos[e2][axis]/d[axis];
                    edgegroup g;
                    g.origin = ivec(pos[e1]).sub(ivec(d).mul(t1));
                    g.slope = d;
                    g.axis = axis;
                    cubeedge ce;
                    ce.c = &c;
                    ce.offset = t1;
                    ce.size = t2 - t1;
                    ce.index = i*(Face_MaxVerts+1)+j;
                    ce.flags = CubeEdge_Start | CubeEdge_End | (e1!=j ? CubeEdge_Flip : 0);
                    ce.next = -1;
                    bool insert = true;
                    int *exists = edgegroups.access(g);
                    if(exists)
                    {
                        int prev = -1,
                            cur  = *exists;
                        while(cur >= 0)
                        {
                            cubeedge &p = cubeedges[cur];
                            if(p.flags&CubeEdge_Dup ?
                                ce.offset>=p.offset && ce.offset+ce.size<=p.offset+p.size :
                                ce.offset==p.offset && ce.size==p.size)
                            {
                                p.flags |= CubeEdge_Dup;
                                insert = false;
                                break;
                            }
                            else if(ce.offset >= p.offset)
                            {
                                if(ce.offset == p.offset+p.size)
                                {
                                    ce.flags &= ~CubeEdge_Start;
                                }
                                prev = cur;
                                cur = p.next;
                            }
                            else
                            {
                                break;
                            }
                        }
                        if(insert)
                        {
                            ce.next = cur;
                            while(cur >= 0)
                            {
                                cubeedge &p = cubeedges[cur];
                                if(ce.offset+ce.size==p.offset)
                                {
                                    ce.flags &= ~CubeEdge_End;
                                    break;
                                }
                                cur = p.next;
                            }
                            if(prev>=0)
                            {
                                cubeedges[prev].next = cubeedges.size();
                            }
                            else
                            {
                                *exists = cubeedges.size();
                            }
                        }
                    }
                    else
                    {
                        edgegroups[g] = cubeedges.size();
                    }
                    if(insert)
                    {
                        cubeedges.push_back(ce);
                    }
                }
            }
        }
    }

    void gencubeedges(cube *c, const ivec &co = ivec(0, 0, 0), int size = worldsize>>1)
    {
        neighborstack[++neighbordepth] = c;
        for(int i = 0; i < 8; ++i)
        {
            ivec o(i, co, size);
            if(c[i].ext)
            {
                c[i].ext->tjoints = -1;
            }
            if(c[i].children)
            {
                gencubeedges(c[i].children, o, size>>1);
            }
            else if(!(c[i].isempty()))
            {
                gencubeedges(c[i], o, size);
            }
        }
        --neighbordepth;
    }

    void gencubeverts(cube &c, const ivec &co, int size)
    {
        if(!(c.visible&0xC0))
        {
            return;
        }
        int vismask = ~c.merged & 0x3F;
        if(!(c.visible&0x80))
        {
            vismask &= c.visible;
        }
        if(!vismask)
        {
            return;
        }
        int tj = filltjoints && c.ext ? c.ext->tjoints : -1, vis;
        for(int i = 0; i < 6; ++i)
        {
            if(vismask&(1<<i) && (vis = visibletris(c, i, co, size)))
            {
                vec pos[Face_MaxVerts];
                vertinfo *verts = nullptr;
                int numverts = c.ext ? c.ext->surfaces[i].numverts&Face_MaxVerts : 0,
                    convex   = 0;
                if(numverts)
                {
                    verts = c.ext->verts() + c.ext->surfaces[i].verts;
                    vec vo(ivec(co).mask(~0xFFF));
                    for(int j = 0; j < numverts; ++j)
                    {
                        pos[j] = vec(verts[j].getxyz()).mul(1.0f/8).add(vo);
                    }
                    if(!flataxisface(c, i))
                    {
                        convex = faceconvexity(verts, numverts, size);
                    }
                }
                else
                {
                    ivec v[4];
                    genfaceverts(c, i, v);
                    if(!flataxisface(c, i))
                    {
                        convex = faceconvexity(v);
                    }
                    int order = vis&4 || convex < 0 ? 1 : 0;
                    vec vo(co);
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
                }

                VSlot &vslot = lookupvslot(c.texture[i], true);
                while(tj >= 0 && tjoints[tj].edge < i*(Face_MaxVerts+1))
                {
                    tj = tjoints[tj].next;
                }
                int hastj = tj >= 0 && tjoints[tj].edge < (i+1)*(Face_MaxVerts+1) ? tj : -1,
                    grassy = vslot.slot->grass && i!=Orient_Bottom ? (vis!=3 || convex ? 1 : 2) : 0;
                if(!c.ext)
                {
                    addcubeverts(vslot, i, pos, c.texture[i], nullptr, numverts, hastj, grassy, (c.material&Mat_Alpha)!=0);
                }
                else
                {
                    const surfaceinfo &surf = c.ext->surfaces[i];
                    if(!surf.numverts || surf.numverts&BlendLayer_Top)
                    {
                        addcubeverts(vslot, i, pos, c.texture[i], verts, numverts, hastj, grassy, (c.material&Mat_Alpha)!=0, surf.numverts&BlendLayer_Blend);
                    }
                    if(surf.numverts&BlendLayer_Bottom)
                    {
                        addcubeverts(vslot, i, pos, 0, verts, numverts, hastj, 0, false, surf.numverts&BlendLayer_Top ? BlendLayer_Bottom : BlendLayer_Top);
                    }
                }
            }
        }
    }

    ////////// Vertex Arrays //////////////

    vtxarray *newva(const ivec &o, int size)
    {
        vtxarray *va = new vtxarray;
        va->parent = nullptr;
        va->o = o;
        va->size = size;
        va->curvfc = ViewFrustumCull_NotVisible;
        va->occluded = Occlude_Nothing;
        va->query = nullptr;
        va->bbmin = va->alphamin = va->refractmin = va->skymin = ivec(-1, -1, -1);
        va->bbmax = va->alphamax = va->refractmax = va->skymax = ivec(-1, -1, -1);
        va->hasmerges = 0;
        va->mergelevel = -1;

        vc.setupdata(va);

        if(va->alphafronttris || va->alphabacktris || va->refracttris)
        {
            va->alphamin = ivec(vec(vc.alphamin).mul(8)).shr(3);
            va->alphamax = ivec(vec(vc.alphamax).mul(8)).add(7).shr(3);
        }
        if(va->refracttris)
        {
            va->refractmin = ivec(vec(vc.refractmin).mul(8)).shr(3);
            va->refractmax = ivec(vec(vc.refractmax).mul(8)).add(7).shr(3);
        }
        if(va->sky && vc.skymax.x >= 0)
        {
            va->skymin = ivec(vec(vc.skymin).mul(8)).shr(3);
            va->skymax = ivec(vec(vc.skymax).mul(8)).add(7).shr(3);
        }

        wverts += va->verts;
        wtris  += va->tris + va->alphabacktris + va->alphafronttris + va->refracttris + va->decaltris;
        allocva++;
        valist.add(va);
        return va;
    }

    struct mergedface
    {
        uchar orient, numverts;
        ushort mat, tex;
        vertinfo *verts;
        int tjoints;
    };

    const int maxmergelevel = 12;
    int vahasmerges = 0,
        vamergemax = 0;
    vector<mergedface> vamerges[maxmergelevel+1];

    int genmergedfaces(cube &c, const ivec &co, int size, int minlevel = -1)
    {
        if(!c.ext || c.isempty())
        {
            return -1;
        }
        int tj = c.ext->tjoints,
            maxlevel = -1;
        for(int i = 0; i < 6; ++i)
        {
            if(c.merged&(1<<i))
            {
                surfaceinfo &surf = c.ext->surfaces[i];
                int numverts = surf.numverts&Face_MaxVerts;
                if(!numverts)
                {
                    if(minlevel < 0)
                    {
                        vahasmerges |= Merge_Part;
                    }
                    continue;
                }
                mergedface mf;
                mf.orient = i;
                mf.mat = c.material;
                mf.tex = c.texture[i];
                mf.numverts = surf.numverts;
                mf.verts = c.ext->verts() + surf.verts;
                mf.tjoints = -1;
                int level = calcmergedsize(i, co, size, mf.verts, mf.numverts&Face_MaxVerts);
                if(level > minlevel)
                {
                    maxlevel = std::max(maxlevel, level);

                    while(tj >= 0 && tjoints[tj].edge < i*(Face_MaxVerts+1))
                    {
                        tj = tjoints[tj].next;
                    }
                    if(tj >= 0 && tjoints[tj].edge < (i+1)*(Face_MaxVerts+1))
                    {
                        mf.tjoints = tj;
                    }
                    if(surf.numverts&BlendLayer_Top)
                    {
                        vamerges[level].add(mf);
                    }
                    if(surf.numverts&BlendLayer_Bottom)
                    {
                        mf.numverts &= ~BlendLayer_Blend;
                        mf.numverts |= surf.numverts&BlendLayer_Top ? BlendLayer_Bottom : BlendLayer_Top;
                        vamerges[level].add(mf);
                    }
                }
            }
        }
        if(maxlevel >= 0)
        {
            vamergemax = std::max(vamergemax, maxlevel);
            vahasmerges |= Merge_Origin;
        }
        return maxlevel;
    }

    int findmergedfaces(cube &c, const ivec &co, int size, int csi, int minlevel)
    {
        if(c.ext && c.ext->va && !(c.ext->va->hasmerges&Merge_Origin))
        {
            return c.ext->va->mergelevel;
        }
        else if(c.children)
        {
            int maxlevel = -1;
            for(int i = 0; i < 8; ++i)
            {
                ivec o(i, co, size/2);
                int level = findmergedfaces(c.children[i], o, size/2, csi-1, minlevel);
                maxlevel = std::max(maxlevel, level);
            }
            return maxlevel;
        }
        else if(c.ext && c.merged)
        {
            return genmergedfaces(c, co, size, minlevel);
        }
        else
        {
            return -1;
        }
    }

    void addmergedverts(int level, const ivec &o)
    {
        vector<mergedface> &mfl = vamerges[level];
        if(mfl.empty())
        {
            return;
        }
        vec vo(ivec(o).mask(~0xFFF));
        vec pos[Face_MaxVerts];
        for(int i = 0; i < mfl.length(); i++)
        {
            mergedface &mf = mfl[i];
            int numverts = mf.numverts&Face_MaxVerts;
            for(int i = 0; i < numverts; ++i)
            {
                vertinfo &v = mf.verts[i];
                pos[i] = vec(v.x, v.y, v.z).mul(1.0f/8).add(vo);
            }
            VSlot &vslot = lookupvslot(mf.tex, true);
            int grassy = vslot.slot->grass && mf.orient!=Orient_Bottom && mf.numverts&BlendLayer_Top ? 2 : 0;
            addcubeverts(vslot, mf.orient, pos, mf.tex, mf.verts, numverts, mf.tjoints, grassy, (mf.mat&Mat_Alpha)!=0, mf.numverts&BlendLayer_Blend);
            vahasmerges |= Merge_Use;
        }
        mfl.setsize(0);
    }

    //recursively finds and adds decals to vacollect object vc
    void finddecals(vtxarray *va)
    {
        if(va->hasmerges&(Merge_Origin|Merge_Part))
        {
            for(int i = 0; i < va->decals.length(); i++)
            {
                vc.extdecals.add(va->decals[i]);
            }
            for(int i = 0; i < va->children.length(); i++)
            {
                finddecals(va->children[i]);
            }
        }
    }

    void rendercube(cube &c, const ivec &co, int size, int csi, int &maxlevel) // creates vertices and indices ready to be put into a va
    {
        if(c.ext && c.ext->va)
        {
            maxlevel = std::max(maxlevel, c.ext->va->mergelevel);
            finddecals(c.ext->va);
            return; // don't re-render
        }

        if(c.children)
        {
            neighborstack[++neighbordepth] = c.children;
            c.escaped = 0;
            for(int i = 0; i < 8; ++i)
            {
                ivec o(i, co, size/2);
                int level = -1;
                rendercube(c.children[i], o, size/2, csi-1, level);
                if(level >= csi)
                {
                    c.escaped |= 1<<i;
                }
                maxlevel = std::max(maxlevel, level);
            }
            --neighbordepth;

            if(csi <= maxmergelevel && vamerges[csi].length())
            {
                addmergedverts(csi, co);
            }
            if(c.ext && c.ext->ents)
            {
                if(c.ext->ents->mapmodels.length())
                {
                    vc.mapmodels.add(c.ext->ents);
                }
                if(c.ext->ents->decals.length())
                {
                    vc.decals.add(c.ext->ents);
                }
            }
            return;
        }

        if(!(c.isempty()))
        {
            gencubeverts(c, co, size);
            if(c.merged)
            {
                maxlevel = std::max(maxlevel, genmergedfaces(c, co, size));
            }
        }
        if(c.material != Mat_Air)
        {
            genmatsurfs(c, co, size, vc.matsurfs);
        }
        if(c.ext && c.ext->ents)
        {
            if(c.ext->ents->mapmodels.length())
            {
                vc.mapmodels.add(c.ext->ents);
            }
            if(c.ext->ents->decals.length())
            {
                vc.decals.add(c.ext->ents);
            }
        }

        if(csi <= maxmergelevel && vamerges[csi].length())
        {
            addmergedverts(csi, co);
        }
    }

    void calcgeombb(const ivec &co, int size, ivec &bbmin, ivec &bbmax)
    {
        vec vmin(co),
            vmax = vmin;
        vmin.add(size);

        for(uint i = 0; i < vc.verts.size(); i++)
        {
            const vec &v = vc.verts[i].pos;
            vmin.min(v);
            vmax.max(v);
        }

        bbmin = ivec(vmin.mul(8)).shr(3);
        bbmax = ivec(vmax.mul(8)).add(7).shr(3);
    }

    int entdepth = -1;
    octaentities *entstack[32];

    void setva(cube &c, const ivec &co, int size, int csi)
    {
        int vamergeoffset[maxmergelevel+1];
        for(int i = 0; i < maxmergelevel+1; ++i)
        {
            vamergeoffset[i] = vamerges[i].length();
        }
        vc.origin = co;
        vc.size = size;
        for(int i = 0; i < entdepth+1; ++i)
        {
            octaentities *oe = entstack[i];
            if(oe->decals.length())
            {
                vc.extdecals.add(oe);
            }
        }
        int maxlevel = -1;
        rendercube(c, co, size, csi, maxlevel);
        if(size == std::min(0x1000, worldsize/2) || !vc.emptyva())
        {
            vtxarray *va = newva(co, size);
            ext(c).va = va;
            calcgeombb(co, size, va->geommin, va->geommax);
            calcmatbb(va, co, size, vc.matsurfs);
            va->hasmerges = vahasmerges;
            va->mergelevel = vamergemax;
        }
        else
        {
            for(int i = 0; i < maxmergelevel+1; ++i)
            {
                vamerges[i].setsize(vamergeoffset[i]);
            }
        }
        vc.clear();
    }

    int setcubevisibility(cube &c, const ivec &co, int size)
    {
        if(c.isempty() && (c.material&MatFlag_Clip) != Mat_Clip)
        {
            return 0;
        }
        int numvis = 0,
            vismask = 0,
            collidemask = 0,
            checkmask = 0;
        for(int i = 0; i < 6; ++i)
        {
            int facemask = classifyface(c, i, co, size);
            if(facemask&1)
            {
                vismask |= 1<<i;
                if(c.merged&(1<<i))
                {
                    if(c.ext && c.ext->surfaces[i].numverts&Face_MaxVerts)
                    {
                        numvis++;
                    }
                }
                else
                {
                    numvis++;
                    if(c.texture[i] != Default_Sky && !(c.ext && c.ext->surfaces[i].numverts & Face_MaxVerts))
                    {
                        checkmask |= 1<<i;
                    }
                }
            }
            if(facemask&2)
            {
                collidemask |= 1<<i;
            }
        }
        c.visible = collidemask | (vismask ? (vismask != collidemask ? (checkmask ? 0x80|0x40 : 0x80) : 0x40) : 0);
        return numvis;
    }

    //va settings, used in updateva below
    VARF(vafacemax, 64, 384, 256*256, rootworld.allchanged());
    VARF(vafacemin, 0, 96, 256*256, rootworld.allchanged());
    VARF(vacubesize, 32, 128, 0x1000, rootworld.allchanged()); //note that performance drops off at low values -> large numbers of VAs

    //updates the va that contains the cube c
    int updateva(cube *c, const ivec &co, int size, int csi)
    {
        int ccount = 0,
            cmergemax  = vamergemax,
            chasmerges = vahasmerges;
        neighborstack[++neighbordepth] = c;
        for(int i = 0; i < 8; ++i)                                  // counting number of semi-solid/solid children cubes
        {
            int count = 0,
                childpos = varoot.length();
            ivec o(i, co, size);                                    //translate cube vector to world vector
            vamergemax = 0;
            vahasmerges = 0;
            if(c[i].ext && c[i].ext->va)
            {
                varoot.add(c[i].ext->va);
                if(c[i].ext->va->hasmerges&Merge_Origin)
                {
                    findmergedfaces(c[i], o, size, csi, csi);
                }
            }
            else
            {
                if(c[i].children)
                {
                    if(c[i].ext && c[i].ext->ents)
                    {
                        entstack[++entdepth] = c[i].ext->ents;
                    }
                    count += updateva(c[i].children, o, size/2, csi-1);
                    if(c[i].ext && c[i].ext->ents)
                    {
                        --entdepth;
                    }
                }
                else
                {
                    count += setcubevisibility(c[i], o, size);
                }
                int tcount = count + (csi <= maxmergelevel ? vamerges[csi].length() : 0);
                if(tcount > vafacemax || (tcount >= vafacemin && size >= vacubesize) || size == std::min(0x1000, worldsize/2))
                {
                    setva(c[i], o, size, csi);
                    if(c[i].ext && c[i].ext->va)
                    {
                        while(varoot.length() > childpos)
                        {
                            vtxarray *child = varoot.pop();
                            c[i].ext->va->children.add(child);
                            child->parent = c[i].ext->va;
                        }
                        varoot.add(c[i].ext->va);
                        if(vamergemax > size)
                        {
                            cmergemax = std::max(cmergemax, vamergemax);
                            chasmerges |= vahasmerges&~Merge_Use;
                        }
                        continue;
                    }
                    else
                    {
                        count = 0;
                    }
                }
            }
            if(csi+1 <= maxmergelevel && vamerges[csi].length())
            {
                vamerges[csi+1].move(vamerges[csi]);
            }
            cmergemax = std::max(cmergemax, vamergemax);
            chasmerges |= vahasmerges;
            ccount += count;
        }
        --neighbordepth;
        vamergemax = cmergemax;
        vahasmerges = chasmerges;

        return ccount;
    }

    void addtjoint(const edgegroup &g, const cubeedge &e, int offset)
    {
        int vcoord = (g.slope[g.axis]*offset + g.origin[g.axis]) & 0x7FFF;
        tjoint tj = tjoint();
        tj.offset = vcoord / g.slope[g.axis];
        tj.edge = e.index;
        int prev = -1,
            cur  = ext(*e.c).tjoints;
        while(cur >= 0)
        {
            tjoint &o = tjoints[cur];
            if(tj.edge < o.edge || (tj.edge==o.edge && (e.flags&CubeEdge_Flip ? tj.offset > o.offset : tj.offset < o.offset)))
            {
                break;
            }
            prev = cur;
            cur = o.next;
        }
        tj.next = cur;
        tjoints.push_back(tj);
        if(prev < 0)
        {
            e.c->ext->tjoints = tjoints.size()-1;
        }
        else
        {
            tjoints[prev].next = tjoints.size()-1;
        }
    }

    void precachetextures()
    {
        vector<int> texs;
        for(int i = 0; i < valist.length(); i++)
        {
            vtxarray *va = valist[i];
            for(int j = 0; j < va->texs; ++j)
            {
                int tex = va->texelems[j].texture;
                if(texs.find(tex) < 0)
                {
                    texs.add(tex);
                }
            }
        }
        for(int i = 0; i < texs.length(); i++)
        {
            lookupvslot(texs[i]);
        }
    }
}

/* externally relevant functionality */
///////////////////////////////////////

void findtjoints(int cur, const edgegroup &g)
{
    int active = -1;
    while(cur >= 0)
    {
        cubeedge &e = cubeedges[cur];
        int prevactive = -1,
            curactive  = active;
        while(curactive >= 0)
        {
            cubeedge &a = cubeedges[curactive];
            if(a.offset+a.size <= e.offset)
            {
                if(prevactive >= 0)
                {
                    cubeedges[prevactive].next = a.next;
                }
                else
                {
                    active = a.next;
                }
            }
            else
            {
                prevactive = curactive;
                if(!(a.flags&CubeEdge_Dup))
                {
                    if(e.flags&CubeEdge_Start && e.offset > a.offset && e.offset < a.offset+a.size)
                    {
                        addtjoint(g, a, e.offset);
                    }
                    if(e.flags&CubeEdge_End && e.offset+e.size > a.offset && e.offset+e.size < a.offset+a.size)
                    {
                        addtjoint(g, a, e.offset+e.size);
                    }
                }
                if(!(e.flags&CubeEdge_Dup))
                {
                    if(a.flags&CubeEdge_Start && a.offset > e.offset && a.offset < e.offset+e.size)
                    {
                        addtjoint(g, e, a.offset);
                    }
                    if(a.flags&CubeEdge_End && a.offset+a.size > e.offset && a.offset+a.size < e.offset+e.size)
                    {
                        addtjoint(g, e, a.offset+a.size);
                    }
                }
            }
            curactive = a.next;
        }
        int next = e.next;
        e.next = active;
        active = cur;
        cur = next;
    }
}

//takes a 3d vec3 and transforms it into a packed ushort vector
//the output ushort is in base 360 and has yaw in the first place and pitch in the second place
//the second place has pitch as a range from 0 to 90
//since this is a normal vector, no magnitude needed
ushort encodenormal(const vec &n)
{
    if(n.iszero())
    {
        return 0;
    }
    int yaw = static_cast<int>(-std::atan2(n.x, n.y)/RAD), //arctangent in degrees
        pitch = static_cast<int>(std::asin(n.z)/RAD); //arcsin in degrees
    return static_cast<ushort>(std::clamp(pitch + 90, 0, 180)*360 + (yaw < 0 ? yaw%360 + 360 : yaw%360) + 1);
}

void reduceslope(ivec &n)
{
    int mindim = -1,
        minval = 64;
    for(int i = 0; i < 3; ++i)
    {
        if(n[i])
        {
            int val = std::abs(n[i]);
            if(mindim < 0 || val < minval)
            {
                mindim = i;
                minval = val;
            }
        }
    }
    if(!(n[R[mindim]]%minval) && !(n[C[mindim]]%minval))
    {
        n.div(minval);
    }
    while(!((n.x|n.y|n.z)&1))
    {
        n.shr(1); //shift right 1 to reduce slope
    }
}

void guessnormals(const vec *pos, int numverts, vec *normals)
{
    vec n1, n2;
    n1.cross(pos[0], pos[1], pos[2]);
    if(numverts != 4)
    {
        n1.normalize();
        for(int k = 0; k < numverts; ++k)
        {
            normals[k] = n1;
        }
        return;
    }
    n2.cross(pos[0], pos[2], pos[3]);
    if(n1.iszero())
    {
        n2.normalize();
        for(int k = 0; k < 4; ++k)
        {
            normals[k] = n2;
        }
        return;
    }
    else
    {
        n1.normalize();
    }
    if(n2.iszero())
    {
        for(int k = 0; k < 4; ++k)
        {
            normals[k] = n1;
        }
        return;
    }
    else
    {
        n2.normalize();
    }
    vec avg = vec(n1).add(n2).normalize();
    normals[0] = avg;
    normals[1] = n1;
    normals[2] = avg;
    normals[3] = n2;
}

//va external fxns

/* destroyva
 * destroys the vertex array object, its various buffer objects and information from
 * the valist object
 *
 * if reparent is set to true, assigns child vertex arrays to the parent of the selected va
 */
void destroyva(vtxarray *va, bool reparent)
{
    wverts -= va->verts;
    wtris -= va->tris + va->alphabacktris + va->alphafronttris + va->refracttris + va->decaltris;
    allocva--;
    valist.removeobj(va);
    if(!va->parent)
    {
        varoot.removeobj(va);
    }
    if(reparent)
    {
        if(va->parent)
        {
            va->parent->children.removeobj(va);
        }
        for(int i = 0; i < va->children.length(); i++)
        {
            vtxarray *child = va->children[i];
            child->parent = va->parent;
            if(child->parent)
            {
                child->parent->children.add(child);
            }
        }
    }
    if(va->vbuf)
    {
        destroyvbo(va->vbuf);
    }
    if(va->ebuf)
    {
        destroyvbo(va->ebuf);
    }
    if(va->skybuf)
    {
        destroyvbo(va->skybuf);
    }
    if(va->decalbuf)
    {
        destroyvbo(va->decalbuf);
    }
    if(va->texelems)
    {
        delete[] va->texelems;
    }
    if(va->decalelems)
    {
        delete[] va->decalelems;
    }
    if(va->matbuf)
    {
        delete[] va->matbuf;
    }
    delete va;
}

//recursively clear vertex arrays for a cube object and its children
void clearvas(cube *c)
{
    for(int i = 0; i < 8; ++i)
    {
        if(c[i].ext)
        {
            if(c[i].ext->va)
            {
                destroyva(c[i].ext->va, false);
            }
            c[i].ext->va = nullptr;
            c[i].ext->tjoints = -1;
        }
        if(c[i].children)
        {
            clearvas(c[i].children);
        }
    }
}

void updatevabb(vtxarray *va, bool force)
{
    if(!force && va->bbmin.x >= 0)
    {
        return;
    }
    va->bbmin = va->geommin;
    va->bbmax = va->geommax;
    va->bbmin.min(va->watermin);
    va->bbmax.max(va->watermax);
    va->bbmin.min(va->glassmin);
    va->bbmax.max(va->glassmax);
    for(int i = 0; i < va->children.length(); i++)
    {
        vtxarray *child = va->children[i];
        updatevabb(child, force);
        va->bbmin.min(child->bbmin);
        va->bbmax.max(child->bbmax);
    }
    for(int i = 0; i < va->mapmodels.length(); i++)
    {
        octaentities *oe = va->mapmodels[i];
        va->bbmin.min(oe->bbmin);
        va->bbmax.max(oe->bbmax);
    }
    for(int i = 0; i < va->decals.length(); i++)
    {
        octaentities *oe = va->decals[i];
        va->bbmin.min(oe->bbmin);
        va->bbmax.max(oe->bbmax);
    }
    va->bbmin.max(va->o);
    va->bbmax.min(ivec(va->o).add(va->size));
    worldmin.min(va->bbmin);
    worldmax.max(va->bbmax);
}

//update vertex array bounding boxes recursively from the root va object down to all children
void updatevabbs(bool force)
{
    if(force)
    {
        worldmin = ivec(worldsize, worldsize, worldsize);
        worldmax = ivec(0, 0, 0);
        for(int i = 0; i < varoot.length(); i++)
        {
            updatevabb(varoot[i], true);
        }
        if(worldmin.x >= worldmax.x)
        {
            worldmin = ivec(0, 0, 0);
            worldmax = ivec(worldsize, worldsize, worldsize);
        }
    }
    else
    {
        for(int i = 0; i < varoot.length(); i++)
        {
            updatevabb(varoot[i]);
        }
    }
}

void cubeworld::findtjoints()
{
    recalcprogress = 0;
    gencubeedges(worldroot);
    tjoints.clear();
    ENUMERATE_KT(edgegroups, edgegroup, g, int, e, ::findtjoints(e, g));
    cubeedges.clear();
    edgegroups.clear();
}

void cubeworld::octarender()                               // creates va s for all leaf cubes that don't already have them
{
    int csi = 0;
    while(1<<csi < worldsize)
    {
        csi++;
    }
    recalcprogress = 0;
    varoot.setsize(0);
    updateva(worldroot, ivec(0, 0, 0), worldsize/2, csi-1);
    flushvbo();
    explicitsky = 0;
    for(int i = 0; i < valist.length(); i++)
    {
        vtxarray *va = valist[i];
        explicitsky += va->sky;
    }
    visibleva = nullptr;
}

void cubeworld::allchanged(bool load)
{
    if(mainmenu)
    {
        load = false;
    }
    if(load)
    {
        initlights();
    }
    clearvas(worldroot);
    resetqueries();
    resetclipplanes();
    entitiesinoctanodes();
    tjoints.clear();
    if(filltjoints)
    {
        findtjoints();
    }
    octarender();
    if(load)
    {
        precachetextures();
    }
    setupmaterials();
    clearshadowcache();
    updatevabbs(true);
    if(load)
    {
        genshadowmeshes();
        seedparticles();
    }
}

void recalc()
{
    rootworld.allchanged(true);
}
COMMAND(recalc, "");

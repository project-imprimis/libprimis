/* vertmodel.cpp: vertex model support
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include <memory>
#include <optional>

#include "interface/console.h"
#include "interface/control.h"

#include "render/rendergl.h"
#include "render/renderlights.h"
#include "render/rendermodel.h"
#include "render/shader.h"
#include "render/shaderparam.h"
#include "render/texture.h"

#include "world/entities.h"
#include "world/octaworld.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"
#include "animmodel.h"
#include "vertmodel.h"

//==============================================================================
// vertmodel object
//==============================================================================

vertmodel::meshgroup * vertmodel::loadmeshes(const char *name, float smooth)
{
    vertmeshgroup *group = newmeshes();
    if(!group->load(name, smooth))
    {
        delete group;
        return nullptr;
    }
    return group;
}

vertmodel::meshgroup * vertmodel::sharemeshes(const char *name, float smooth)
{
    if(meshgroups.find(name) == meshgroups.end())
    {
        meshgroup *group = loadmeshes(name, smooth);
        if(!group)
        {
            return nullptr;
        }
        meshgroups[group->groupname()] = group;
    }
    return meshgroups[name];
}

vertmodel::vertmodel(std::string name) : animmodel(name)
{
}

//==============================================================================
// vertmodel::vbocacheentry object
//==============================================================================

vertmodel::vbocacheentry::vbocacheentry() : vbuf(0)
{
    as.cur.fr1 = as.prev.fr1 = -1;
}

//==============================================================================
// vertmodel::vertmesh object
//==============================================================================

vertmodel::vertmesh::vertmesh() : verts(0), tcverts(0), tris(0)
{
}

vertmodel::vertmesh::~vertmesh()
{
    delete[] verts;
    delete[] tcverts;
    delete[] tris;
}

void vertmodel::vertmesh::smoothnorms(float limit, bool areaweight)
{
    if((static_cast<vertmeshgroup *>(group))->numframes == 1)
    {
        Mesh::smoothnorms<vertmodel>(verts, numverts, tris, numtris, limit, areaweight);
    }
    else
    {
        buildnorms(areaweight);
    }
}

void vertmodel::vertmesh::buildnorms(bool areaweight)
{
    Mesh::buildnorms<vertmodel>(verts, numverts, tris, numtris, areaweight, (static_cast<vertmeshgroup *>(group))->numframes);
}

void vertmodel::vertmesh::calctangents(bool areaweight)
{
    Mesh::calctangents<vertmodel, vertmodel::tcvert>(verts, tcverts, numverts, tris, numtris, areaweight, (static_cast<vertmeshgroup *>(group))->numframes);
}

void vertmodel::vertmesh::calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) const
{
    for(int j = 0; j < numverts; ++j)
    {
        vec v = m.transform(verts[j].pos);
        bbmin.min(v);
        bbmax.max(v);
    }
}

void vertmodel::vertmesh::genBIH(BIH::mesh &m) const
{
    m.setmesh(reinterpret_cast<const BIH::mesh::tri *>(tris), numtris,
              reinterpret_cast<const uchar *>(&verts->pos), sizeof(vert),
              reinterpret_cast<const uchar *>(&tcverts->tc), sizeof(tcvert));
}

void vertmodel::vertmesh::genshadowmesh(std::vector<triangle> &out, const matrix4x3 &m) const
{
    for(int j = 0; j < numtris; ++j)
    {
        out.emplace_back(m.transform(verts[tris[j].vert[0]].pos),
                         m.transform(verts[tris[j].vert[1]].pos),
                         m.transform(verts[tris[j].vert[2]].pos));
    }
}

void vertmodel::vertmesh::assignvert(vvertg &vv, int j, const tcvert &tc, const vert &v)
{
    vv.pos = vec4<half>(v.pos, 1);
    vv.tc = tc.tc;
    vv.tangent = v.tangent;
}

int vertmodel::vertmesh::genvbo(std::vector<uint> &idxs, int offset)
{
    voffset = offset;
    for(int i = 0; i < numtris; ++i)
    {
        const tri &t = tris[i];
        for(int j = 0; j < 3; ++j)
        {
            idxs.push_back(voffset+t.vert[j]);
        }
    }
    minvert = voffset;
    maxvert = voffset + numverts-1;
    elen = idxs.size();
    return numverts;
}

void vertmodel::vertmesh::render()
{
    if(!Shader::lastshader)
    {
        return;
    }
    glDrawRangeElements(GL_TRIANGLES, minvert, maxvert, elen, GL_UNSIGNED_INT, nullptr);
    glde++;
    xtravertsva += numverts;
}

//==============================================================================
// vertmodel::vertmeshgroup object
//==============================================================================

vertmodel::vertmeshgroup::vertmeshgroup()
    : numframes(0), tags(nullptr), numtags(0), ebuf(0), vlen(0), vertsize(0), vdata(nullptr)
{
}

vertmodel::vertmeshgroup::~vertmeshgroup()
{
    delete[] tags;
    if(ebuf)
    {
        glDeleteBuffers(1, &ebuf);
    }
    for(int i = 0; i < maxvbocache; ++i)
    {
        if(vbocache[i].vbuf)
        {
            glDeleteBuffers(1, &vbocache[i].vbuf);
        }
    }
    delete[] vdata;
}

void vertmodel::vertmeshgroup::concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const
{
}

int vertmodel::vertmeshgroup::findtag(std::string_view name)
{
    for(int i = 0; i < numtags; ++i)
    {
        if(!std::strcmp(tags[i].name, name.data()))
        {
            return i;
        }
    }
    return -1;
}

bool vertmodel::vertmeshgroup::addtag(std::string_view name, const matrix4x3 &matrix)
{
    int idx = findtag(name);
    if(idx >= 0)
    {
        if(!testtags)
        {
            return false;
        }
        for(int i = 0; i < numframes; ++i)
        {
            tag &t = tags[i*numtags + idx];
            t.matrix = matrix;
        }
    }
    else
    {
        tag *newtags = new tag[(numtags+1)*numframes];
        for(int i = 0; i < numframes; ++i)
        {
            tag *dst = &newtags[(numtags+1)*i],
                *src = &tags[numtags*i];
            if(!i)
            {
                for(int j = 0; j < numtags; ++j)
                {
                    std::swap(dst[j].name, src[j].name);
                }
                dst[numtags].name = newstring(name.data());
            }
            for(int j = 0; j < numtags; ++j)
            {
                dst[j].matrix = src[j].matrix;
            }
            dst[numtags].matrix = matrix;
        }
        if(tags)
        {
            delete[] tags;
        }
        tags = newtags;
        numtags++;
    }
    return true;
}

int vertmodel::vertmeshgroup::totalframes() const
{
    return numframes;
}

void vertmodel::vertmeshgroup::calctagmatrix(const part *p, int i, const AnimState &as, matrix4 &matrix) const
{
    const matrix4x3 &tag1 = tags[as.cur.fr1*numtags + i].matrix,
                    &tag2 = tags[as.cur.fr2*numtags + i].matrix;
    matrix4x3 tag;
    tag.lerp(tag1, tag2, as.cur.t);
    if(as.interp<1)
    {
        const matrix4x3 &tag1p = tags[as.prev.fr1*numtags + i].matrix,
                        &tag2p = tags[as.prev.fr2*numtags + i].matrix;
        matrix4x3 tagp;
        tagp.lerp(tag1p, tag2p, as.prev.t);
        tag.lerp(tagp, tag, as.interp);
    }
    tag.d.mul(p->model->locationsize().w * sizescale);
    matrix = matrix4(tag);
}

void vertmodel::vertmeshgroup::genvbo(vbocacheentry &vc)
{
    if(!vc.vbuf)
    {
        glGenBuffers(1, &vc.vbuf);
    }
    if(ebuf)
    {
        return;
    }

    std::vector<uint> idxs;
    auto rendermeshes = getrendermeshes();
    vlen = 0;
    if(numframes>1)
    {
        vertsize = sizeof(vvert);

        for(auto i : rendermeshes)
        {
            vlen += static_cast<vertmesh *>(*i)->genvbo(idxs, vlen);
        }
        delete[] vdata;
        vdata = new uchar[vlen*vertsize];
        for(auto i : rendermeshes)
        {
            static_cast<vertmesh *>(*i)->fillverts(reinterpret_cast<vvert *>(vdata));
        }
    }
    else
    {
        vertsize = sizeof(vvertg);
        gle::bindvbo(vc.vbuf);
        size_t numverts = 0,
               htlen = 128;
        for(auto i : rendermeshes)
        {
            numverts += static_cast<vertmesh *>(*i)->numverts;
        }
        while(htlen < numverts)
        {
            htlen *= 2;
        }
        if(numverts*4 > htlen*3)
        {
            htlen *= 2;
        }
        int *htdata = new int[htlen];
        std::memset(htdata, -1, htlen*sizeof(int));
        std::vector<vvertg> vverts;
        for(auto i : rendermeshes)
        {
            vlen += static_cast<vertmesh *>(*i)->genvbo(idxs, vlen, vverts, htdata, htlen);
        }
        glBufferData(GL_ARRAY_BUFFER, vverts.size()*sizeof(vvertg), vverts.data(), GL_STATIC_DRAW);
        delete[] htdata;
        htdata = nullptr;
        gle::clearvbo();
    }

    glGenBuffers(1, &ebuf);
    gle::bindebo(ebuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxs.size()*sizeof(uint), idxs.data(), GL_STATIC_DRAW);
    gle::clearebo();
}

void vertmodel::vertmeshgroup::bindvbo(const AnimState *as, const part *p, const vbocacheentry &vc)
{
    if(numframes>1)
    {
        bindvbo<vvert>(as, p, vc);
    }
    else
    {
        bindvbo<vvertg>(as, p, vc);
    }
}

void *vertmodel::vertmeshgroup::animkey()
{
    return this;
}

void vertmodel::vertmeshgroup::cleanup()
{
    for(int i = 0; i < maxvbocache; ++i)
    {
        vbocacheentry &c = vbocache[i];
        if(c.vbuf)
        {
            glDeleteBuffers(1, &c.vbuf);
            c.vbuf = 0;
        }
        c.as.cur.fr1 = -1;
    }
    if(ebuf)
    {
        glDeleteBuffers(1, &ebuf);
        ebuf = 0;
    }
}

void vertmodel::vertmeshgroup::preload()
{
    if(numframes > 1)
    {
        return;
    }
    if(!vbocache->vbuf)
    {
        genvbo(*vbocache);
    }
}

void vertmodel::vertmeshgroup::render(const AnimState *as, float, const vec &, const vec &, dynent *, part *p)
{
    if(as->cur.anim & Anim_NoRender)
    {
        for(part::linkedpart &l : p->links)
        {
            calctagmatrix(p, l.tag, *as, l.matrix);
        }
        return;
    }
    vbocacheentry *vc = nullptr;
    if(numframes<=1)
    {
        vc = vbocache;
    }
    else
    {
        for(int i = 0; i < maxvbocache; ++i)
        {
            vbocacheentry &c = vbocache[i];
            if(!c.vbuf)
            {
                continue;
            }
            if(c.as==*as)
            {
                vc = &c;
                break;
            }
        }
        if(!vc)
        {
            for(int i = 0; i < maxvbocache; ++i)
            {
                vc = &vbocache[i];
                if(!vc->vbuf || vc->millis < lastmillis)
                {
                    break;
                }
            }
        }
    }
    if(!vc->vbuf)
    {
        genvbo(*vc);
    }
    if(numframes>1)
    {
        if(vc->as!=*as)
        {
            vc->as = *as;
            vc->millis = lastmillis;
            auto rendermeshes = getrendermeshes();
            for(auto i : rendermeshes)
            {
                static_cast<vertmesh *>(*i)->interpverts(*as, reinterpret_cast<vvert *>(vdata));
            }
            gle::bindvbo(vc->vbuf);
            glBufferData(GL_ARRAY_BUFFER, vlen*vertsize, vdata, GL_STREAM_DRAW);
        }
        vc->millis = lastmillis;
    }
    bindvbo(as, p, *vc);
    LOOP_RENDER_MESHES(vertmesh, m,
    {
        p->skins[i].bind(m, as); //not a skeletal model, do not need to include trailing params
        m.render();
    });
    for(part::linkedpart &l : p->links)
    {
        calctagmatrix(p, l.tag, *as, l.matrix);
    }
}

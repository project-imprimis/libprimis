/* vertmodel.cpp: vertex model support
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"


#include "interface/console.h"
#include "interface/control.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"
#include "render/renderlights.h"
#include "render/rendermodel.h"
#include "render/texture.h"

#include "world/entities.h"
#include "world/octaworld.h"
#include "world/physics.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"
#include "animmodel.h"
#include "skelmodel.h"
#include "vertmodel.h"


//==============================================================================
// vertmodel::vertmeshgroup object
//==============================================================================
vertmodel::vertmeshgroup::vertmeshgroup()
    : numframes(0), tags(nullptr), numtags(0), edata(nullptr), ebuf(0), vlen(0), vertsize(0), vdata(nullptr)
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

int vertmodel::vertmeshgroup::findtag(const char *name)
{
    for(int i = 0; i < numtags; ++i)
    {
        if(!std::strcmp(tags[i].name, name))
        {
            return i;
        }
    }
    return -1;
}

bool vertmodel::vertmeshgroup::addtag(const char *name, const matrix4x3 &matrix)
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
                dst[numtags].name = newstring(name);
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

void vertmodel::vertmeshgroup::concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n)
{
    n.mul(m, tags[i].matrix);
}

void vertmodel::vertmeshgroup::calctagmatrix(part *p, int i, const AnimState &as, matrix4 &matrix)
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
    tag.d.mul(p->model->scale * sizescale);
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

    vector<ushort> idxs;

    vlen = 0;
    if(numframes>1)
    {
        vertsize = sizeof(vvert);
        LOOP_RENDER_MESHES(vertmesh, m, vlen += m.genvbo(idxs, vlen));
        delete[] vdata;
        vdata = new uchar[vlen*vertsize];
        LOOP_RENDER_MESHES(vertmesh, m,
        {
            m.fillverts(reinterpret_cast<vvert *>(vdata));
        });
    }
    else
    {
        vertsize = sizeof(vvertg);
        gle::bindvbo(vc.vbuf);

        #define GENVBO(type) \
            do \
            { \
                vector<type> vverts; \
                LOOP_RENDER_MESHES(vertmesh, m, vlen += m.genvbo(idxs, vlen, vverts, htdata, htlen)); \
                glBufferData(GL_ARRAY_BUFFER, vverts.length()*sizeof(type), vverts.getbuf(), GL_STATIC_DRAW); \
            } while(0)

        int numverts = 0,
            htlen = 128;
        LOOP_RENDER_MESHES(vertmesh, m, numverts += m.numverts);
        while(htlen < numverts)
        {
            htlen *= 2;
        }
        if(numverts*4 > htlen*3)
        {
            htlen *= 2;
        }
        int *htdata = new int[htlen];
        memset(htdata, -1, htlen*sizeof(int));
        GENVBO(vvertg);
        delete[] htdata;
        htdata = nullptr;
        #undef GENVBO

        gle::clearvbo();
    }

    glGenBuffers(1, &ebuf);
    gle::bindebo(ebuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW);
    gle::clearebo();
}

void vertmodel::vertmeshgroup::bindvbo(const AnimState *as, part *p, vbocacheentry &vc)
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

void vertmodel::vertmeshgroup::preload(part *p)
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

void vertmodel::vertmeshgroup::render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p)
{
    if(as->cur.anim & Anim_NoRender)
    {
        for(int i = 0; i < p->links.length(); i++)
        {
            calctagmatrix(p, p->links[i].tag, *as, p->links[i].matrix);
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
            LOOP_RENDER_MESHES(vertmesh, m,
            {
                m.interpverts(*as, reinterpret_cast<vvert *>(vdata), p->skins[i]);
            });
            gle::bindvbo(vc->vbuf);
            glBufferData(GL_ARRAY_BUFFER, vlen*vertsize, vdata, GL_STREAM_DRAW);
        }
        vc->millis = lastmillis;
    }
    bindvbo(as, p, *vc);
    LOOP_RENDER_MESHES(vertmesh, m,
    {
        p->skins[i].bind(m, as);
        m.render(as, p->skins[i], *vc);
    });
    for(int i = 0; i < p->links.length(); i++)
    {
        calctagmatrix(p, p->links[i].tag, *as, p->links[i].matrix);
    }
}

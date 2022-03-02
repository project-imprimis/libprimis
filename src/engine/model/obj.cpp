/* obj.cpp: wavefront model support
 *
 * Libprimis supports the Wavefront (obj) model format for simple static models.
 * This file contains the implementation functions, while the class for the obj
 * model type is located in obj.h.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"
#include "render/rendermodel.h"
#include "render/renderwindow.h"
#include "render/texture.h"

#include "interface/console.h"
#include "interface/control.h"

#include "world/entities.h"
#include "world/octaworld.h"
#include "world/physics.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"
#include "animmodel.h"
#include "vertmodel.h"
#include "skelmodel.h"

#include "obj.h"

#include "interface/console.h"

const char *obj::formatname()
{
    return "obj";
}

bool obj::cananimate()
{
    return false;
}

bool obj::flipy() const
{
    return true;
}

int obj::type() const
{
    return MDL_OBJ;
}

bool obj::objmeshgroup::load(const char *filename, float smooth)
{
    int len = std::strlen(filename);
    if(len < 4 || strcasecmp(&filename[len-4], ".obj"))
    {
        return false;
    }
    stream *file = openfile(filename, "rb");
    if(!file)
    {
        return false;
    }
    name = newstring(filename);
    numframes = 1;
    std::vector<vec> attrib[3];
    char buf[512];
    hashtable<ivec, int> verthash(1<<11);
    vector<vert> verts;
    vector<tcvert> tcverts;
    vector<tri> tris;

    string meshname = "";
    vertmesh *curmesh = nullptr;
    while(file->getline(buf, sizeof(buf)))
    {
        char *c = buf;
        while(isspace(*c))
        {
            c++;
        }
        switch(*c)
        {
            case '#':
            {
                continue;
            }
            case 'v':
            {
                if(isspace(c[1]))
                {
                    parsevert(c, attrib[0]);
                }
                else if(c[1]=='t')
                {
                    parsevert(c, attrib[1]);
                }
                else if(c[1]=='n')
                {
                    parsevert(c, attrib[2]);
                }
                break;
            }
            case 'g':
            {
                while(isalpha(*c))
                {
                    c++;
                }
                while(isspace(*c))
                {
                    c++;
                }
                char *name = c;
                size_t namelen = std::strlen(name);
                while(namelen > 0 && isspace(name[namelen-1]))
                {
                    namelen--;
                }
                copystring(meshname, name, std::min(namelen+1, sizeof(meshname)));
                if(curmesh)
                {
                    flushmesh(meshname, curmesh, verts, tcverts, tris, attrib[2], smooth);
                }
                curmesh = nullptr;
                break;
            }
            case 'f':
            {
                if(!curmesh)
                {
                    //startmesh
                    vertmesh &m = *new vertmesh;
                    m.group = this;
                    m.name = meshname[0] ? newstring(meshname) : nullptr;
                    meshes.add(&m);
                    curmesh = &m;
                    verthash.clear();
                    verts.setsize(0);
                    tcverts.setsize(0);
                    tris.setsize(0);
                }
                int v0 = -1,
                    v1 = -1;
                while(isalpha(*c))
                {
                    c++;
                }
                for(;;)
                {
                    while(isspace(*c))
                    {
                        c++;
                    }
                    if(!*c)
                    {
                        break;
                    }
                    ivec vkey(-1, -1, -1);
                    for(int i = 0; i < 3; ++i)
                    {
                        vkey[i] = strtol(c, &c, 10);
                        if(vkey[i] < 0)
                        {
                            vkey[i] = attrib[i].size() + vkey[i];
                        }
                        else
                        {
                            vkey[i]--;
                        }
                        if(!(attrib[i].size() > static_cast<uint>(vkey[i])))
                        {
                            vkey[i] = -1;
                        }
                        if(*c!='/')
                        {
                            break;
                        }
                        c++;
                    }
                    int *index = verthash.access(vkey);
                    if(!index)
                    {
                        index = &verthash[vkey];
                        *index = verts.length();
                        vert &v = verts.add();
                        v.pos = vkey.x < 0 ? vec(0, 0, 0) : attrib[0][vkey.x];
                        v.pos = vec(v.pos.z, -v.pos.x, v.pos.y);
                        v.norm = vkey.z < 0 ? vec(0, 0, 0) : attrib[2][vkey.z];
                        v.norm = vec(v.norm.z, -v.norm.x, v.norm.y);
                        tcvert &tcv = tcverts.add();
                        tcv.tc = vkey.y < 0 ? vec2(0, 0) : vec2(attrib[1][vkey.y].x, 1-attrib[1][vkey.y].y);
                    }
                    if(v0 < 0)
                    {
                        v0 = *index;
                    }
                    else if(v1 < 0)
                    {
                        v1 = *index;
                    }
                    else
                    {
                        tri &t = tris.add();
                        t.vert[0] = static_cast<ushort>(*index);
                        t.vert[1] = static_cast<ushort>(v1);
                        t.vert[2] = static_cast<ushort>(v0);
                        v1 = *index;
                    }
                }
                break;
            }
        }
    }
    if(curmesh)
    {
        flushmesh(meshname, curmesh, verts, tcverts, tris, attrib[2], smooth);
    }
    delete file;
    return true;
}

void obj::objmeshgroup::parsevert(char *s, std::vector<vec> &out)
{
    out.emplace_back(vec(0, 0, 0));
    vec &v = out.back();
    while(isalpha(*s))
    {
        s++;
    }
    for(int i = 0; i < 3; ++i)
    {
        v[i] = strtod(s, &s);
        while(isspace(*s))
        {
            s++;
        }
        if(!*s)
        {
            break;
        }
    }
}

const void obj::objmeshgroup::flushmesh(string meshname, vertmesh *curmesh, vector<vert> verts, vector<tcvert> tcverts,
                                   vector<tri> tris, std::vector<vec> attrib, float smooth)
{
    curmesh->numverts = verts.length();
    if(verts.length())
    {
        curmesh->verts = new vert[verts.length()];
        memcpy(curmesh->verts, verts.getbuf(), verts.length()*sizeof(vert));
        curmesh->tcverts = new tcvert[verts.length()];
        memcpy(curmesh->tcverts, tcverts.getbuf(), tcverts.length()*sizeof(tcvert));
    }
    curmesh->numtris = tris.length();
    if(tris.length())
    {
        curmesh->tris = new tri[tris.length()];
        memcpy(curmesh->tris, tris.getbuf(), tris.length()*sizeof(tri));
    }
    if(attrib.empty())
    {
        if(smooth <= 1)
        {
            curmesh->smoothnorms(smooth);
        }
        else curmesh->buildnorms();
    }
    curmesh->calctangents();
}

bool obj::loaddefaultparts()
{
    part &mdl = addpart();
    const char *pname = parentdir(name);
    DEF_FORMAT_STRING(name1, "media/model/%s/tris.obj", name);
    mdl.meshes = sharemeshes(path(name1));
    if(!mdl.meshes)
    {
        DEF_FORMAT_STRING(name2, "media/model/%s/tris.obj", pname);    // try obj in parent folder (vert sharing)
        mdl.meshes = sharemeshes(path(name2));
        if(!mdl.meshes)
        {
            return false;
        }
    }
    Texture *tex, *masks;
    loadskin(name, pname, tex, masks);
    mdl.initskins(tex, masks);
    if(tex==notexture)
    {
        conoutf("could not load model skin for %s", name1);
    }
    return true;
}

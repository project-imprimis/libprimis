
/* gltf.cpp: Khronos GL Transmission Format
 * 
 * This is the implementation file for the GL Transmission Format (GLTF) model
 * format, version 2.
 */

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include <optional>
#include <memory>

#include "render/rendergl.h"
#include "render/rendermodel.h"
#include "render/renderwindow.h"
#include "render/shader.h"
#include "render/shaderparam.h"
#include "render/texture.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/cs.h"

#include "world/entities.h"
#include "world/octaworld.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"
#include "animmodel.h"
#include "skelmodel.h"

#include "gltf.h"
#include "gltfloader.h"

skelcommands<gltf> gltf::gltfcommands;

gltf::gltf(std::string name) : skelloader(name)
{
}

bool gltf::flipy() const
{
    return false;
}

int gltf::type() const
{
    return MDL_GLTF;
}

const char *gltf::formatname()
{
    return "gltf";
}

gltf::skelmeshgroup *gltf::newmeshes()
{
    return new gltfmeshgroup;
}

bool gltf::loaddefaultparts()
{
    skelpart &mdl = addpart();
    const char *fname = modelname().c_str() + std::strlen(modelname().c_str());
    do
    {
        --fname;
    } while(fname >= modelname() && *fname!='/' && *fname!='\\');
    fname++;
    std::string meshname = std::string(modelpath);
    meshname.append(modelname()).append("/").append(fname).append(".gltfmesh");
    mdl.meshes = sharemeshes(path(meshname).c_str());
    if(!mdl.meshes)
    {
        return false;
    }
    mdl.initanimparts();
    mdl.initskins();
    return true;
}


gltf::gltfmeshgroup::gltfmeshgroup()
{
}

bool gltf::gltfmeshgroup::loadmesh(const char *filename, float smooth, part &p)
{
    GLTFModelInfo mi(filename);
    
    gltfmesh *m = new gltfmesh;
    m->group = this;
    meshes.push_back(m);
    p.initskins(notexture, notexture, m->group->meshes.size());
    //get GLTF data from file/binary
    std::string meshname = mi.getmeshnames()[0];
    std::vector<std::array<float, 3>> positions = mi.getpositions(meshname);
    std::vector<std::array<float, 3>> normals = mi.getnormals(meshname);
    std::vector<std::array<float, 2>> texcoords = mi.gettexcoords(meshname);
    std::vector<std::array<uint, 4>> joints = mi.getjoints(meshname);
    std::vector<std::array<float, 4>> weights = mi.getweights(meshname);
    std::vector<std::array<uint, 3>> indices = mi.getindices(meshname);
    //set skelmodel::vert and skelmodel::tris
    if(positions.size() == normals.size() && normals.size() == texcoords.size())
    {
        m->verts = new vert[positions.size()];
        m->numverts = positions.size();
        for(size_t i = 0; i < positions.size(); ++i)
        {
            m->verts[i].pos = vec(positions[i][0], positions[i][1], positions[i][2]);
            m->verts[i].norm = vec(normals[i][0], normals[i][1], normals[i][2]);
            m->verts[i].tc = vec2(texcoords[i][0], texcoords[i][1]);
            blendcombo c;
            c.addweight(0,0,0);
            c.finalize(0);
            m->verts[i].blend = addblendcombo(c);
        }
    }
    else
    {
        conoutf("gltf sizes %lu %lu %lu", positions.size(), normals.size(), texcoords.size());
    }
    m->tris = new tri[indices.size()];
    m->numtris = indices.size();
    for(size_t i = 0; i < indices.size(); ++i)
    {
        m->tris[i].vert[0] = indices[i][0];
        m->tris[i].vert[1] = indices[i][1];
        m->tris[i].vert[2] = indices[i][2];
    }
    
    for(uint i = 0; i < meshes.size(); i++)
    {
        gltfmesh &m = *static_cast<gltfmesh *>(meshes[i]);
        m.buildnorms();
        m.calctangents();
        m.cleanup();
    }
    sortblendcombos();
    //spoof a single bone into existence
    skel->numbones = 1;
    skel->bones = new boneinfo[1];
    skel->bones->base = dualquat(quat(0,0,0,0), vec(1,0,0));
    skel->bones->name = newstring("test");
    skel->bones->parent = 0;
    return true;
}

bool gltf::gltfmeshgroup::load(const char *meshfile, float smooth, part &p)
{
    name = meshfile;

    if(!loadmesh(meshfile, smooth, p))
    {
        return false;
    }
    return true;
}

gltf::gltfmesh::gltfmesh()
{
}

gltf::gltfmesh::~gltfmesh()
{
    cleanup();
}

void gltf::gltfmesh::cleanup()
{
}

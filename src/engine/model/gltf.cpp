
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
    try
    {
        GLTFModelInfo mi(filename);

        std::vector<std::string> nodenames = mi.getnodenames(GLTFModelInfo::NodeType_Mesh);
        for(std::string meshname : nodenames)
        {
            //get GLTF data from file/binary
            std::vector<std::array<float, 3>> positions = mi.getpositions(meshname);
            std::vector<std::array<float, 3>> normals = mi.getnormals(meshname);
            std::vector<std::array<float, 2>> texcoords = mi.gettexcoords(meshname);
            std::vector<std::array<uint, 4>> joints = mi.getjoints(meshname);
            std::vector<std::array<float, 4>> weights = mi.getweights(meshname);
            std::vector<std::array<uint, 3>> indices = mi.getindices(meshname);
            //set skelmodel::vert and skelmodel::tris
            vert *verts;
            size_t numverts;
            if(positions.size() == normals.size() && normals.size() == texcoords.size())
            {
                verts = new vert[positions.size()];
                numverts = positions.size();
                for(size_t i = 0; i < positions.size(); ++i)
                {
                    //pos, normals are transformed (-z->y, y->z, x->x) from GLTF to Cube coord system
                    verts[i].pos = vec(positions[i][0], -positions[i][2], positions[i][1]);
                    verts[i].norm = vec(normals[i][0], -normals[i][2], normals[i][1]);
                    verts[i].tc = vec2(texcoords[i][0], texcoords[i][1]);
                    blendcombo c;
                    c.addweight(0,0,0);
                    c.finalize(0);
                    verts[i].blend = addblendcombo(c);
                }
            }
            else
            {
                throw std::logic_error("index mismatch: positions/normals/texcoords different sizes");
            }
            tri *tris = new tri[indices.size()];
            size_t numtris = indices.size();
            for(size_t i = 0; i < indices.size(); ++i)
            {
                tris[i].vert[0] = indices[i][0];
                tris[i].vert[1] = indices[i][1];
                tris[i].vert[2] = indices[i][2];
            }
            //if able to create the verts/tris arrays without throwing, create new gltfmesh
            gltfmesh *m = new gltfmesh(newstring(meshname.c_str()), verts, numverts, tris, numtris);
            m->group = this;
            meshes.push_back(m);
            p.initskins(notexture, notexture, m->group->meshes.size());

        }
        for(uint i = 0; i < meshes.size(); i++)
        {
            gltfmesh &m = *static_cast<gltfmesh *>(meshes[i]);
            m.buildnorms();
            m.calctangents();
            m.cleanup();
        }
        sortblendcombos();
        return true;
    }
    //catch errors thrown by GLTFModelInfo
    catch(const std::ios_base::failure &e)
    {
        conoutf("model loading failed: caught %s\n", e.what());
        return false;
    }
    catch(const std::logic_error &e)
    {
        conoutf("invalid model file contents: caught %s\n", e.what());
        return false;
    }
}

bool gltf::gltfmeshgroup::load(std::string_view meshfile, float smooth, part &p)
{
    name = meshfile;

    if(!loadmesh(meshfile.data(), smooth, p))
    {
        return false;
    }
    return true;
}

gltf::gltfmesh::gltfmesh(std::string_view name, vert *verts, uint numverts, tri *tris, uint numtris) : skelmesh(name, verts, numverts, tris, numtris)
{
}

gltf::gltfmesh::~gltfmesh()
{
    cleanup();
}

void gltf::gltfmesh::cleanup()
{
}

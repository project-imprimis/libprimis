
/* md5.cpp: md5 (id tech 4) model support
 *
 * Libprimis supports the md5 (id Tech 4) skeletal model format for animated
 * models. Ragdolls and animated models (such as players) use this model format.
 * The implmentation for the md5 class is in this file, while the object is defined
 * inside md5.h.
 */

//includes copied from obj.cpp
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

#include "md5.h"

static constexpr int md5version = 10;

skelcommands<md5> md5::md5commands;

md5::md5(std::string name) : skelloader(name) {}

const char *md5::formatname()
{
    return "md5";
}

bool md5::flipy() const
{
    return false;
}

int md5::type() const
{
    return MDL_MD5;
}

md5::skelmeshgroup *md5::newmeshes()
{
    return new md5meshgroup;
}

bool md5::loaddefaultparts()
{
    skelpart &mdl = addpart();
    const char *fname = modelname().c_str() + std::strlen(modelname().c_str());
    do
    {
        --fname;
    } while(fname >= modelname() && *fname!='/' && *fname!='\\');
    fname++;
    std::string meshname = modelpath;
    meshname.append(modelname()).append("/").append(fname).append(".md5mesh");
    mdl.meshes = sharemeshes(path(meshname).c_str());
    if(!mdl.meshes)
    {
        return false;
    }
    mdl.initanimparts();
    mdl.initskins();
    std::string animname = modelpath;
    animname.append(modelname()).append("/").append(fname).append(".md5anim");
    static_cast<md5meshgroup *>(mdl.meshes)->loadanim(path(animname));
    return true;
}


md5::md5meshgroup::md5meshgroup()
{
}

//main anim loading functionality
const md5::skelanimspec *md5::md5meshgroup::loadanim(const std::string &filename)
{
    {
        const skelanimspec *sa = skel->findskelanim(filename);
        if(sa)
        {
            return sa;
        }
    }
    stream *f = openfile(filename.c_str(), "r");
    if(!f)
    {
        return nullptr;
    }
    //hierarchy, basejoints vectors are to have correlating indices with
    // skel->bones, this->adjustments, this->frame, skel->framebones
    struct md5hierarchy
    {
        string name;
        int parent, flags, start;
    };
    std::vector<md5hierarchy> hierarchy; //metadata used only within this function
    std::vector<md5joint> basejoints;
    int animdatalen = 0,
        animframes = 0;
    float *animdata = nullptr;
    dualquat *animbones = nullptr;
    char buf[512]; //presumably lines over 512 char long will break this loader
    //for each line in the opened file
    skelanimspec *sas = nullptr;
    while(f->getline(buf, sizeof(buf)))
    {
        int tmp;
        if(std::sscanf(buf, " MD5Version %d", &tmp) == 1)
        {
            if(tmp != md5version)
            {
                delete f; //bail out if md5version is not what we want
                return nullptr;
            }
        }
        else if(std::sscanf(buf, " numJoints %d", &tmp) == 1)
        {
            if(tmp != static_cast<int>(skel->numbones))
            {
                delete f; //bail out if numbones is not consistent
                return nullptr;
            }
        }
        else if(std::sscanf(buf, " numFrames %d", &animframes) == 1)
        {
            if(animframes < 1) //if there are no animated frames, don't do animated frame stuff
            {
                delete f;
                return nullptr;
            }
        }
        //apparently, do nothing with respect to framerate
        else if(std::sscanf(buf, " frameRate %d", &tmp) == 1)
        {
            //(empty body)
        }
        //create animdata if there is some relevant info in file
        else if(std::sscanf(buf, " numAnimatedComponents %d", &animdatalen)==1)
        {
            if(animdatalen > 0)
            {
                animdata = new float[animdatalen];
            }
        }
        else if(std::strstr(buf, "bounds {"))
        {
            while(f->getline(buf, sizeof(buf)) && buf[0]!='}') //loop until end of {} block
            {
                //(empty body)
            }
        }
        else if(std::strstr(buf, "hierarchy {"))
        {
            while(f->getline(buf, sizeof(buf)) && buf[0]!='}') //loop until end of {} block
            {
                md5hierarchy h;
                if(std::sscanf(buf, " %100s %d %d %d", h.name, &h.parent, &h.flags, &h.start)==4)
                {
                    hierarchy.push_back(h);
                }
            }
        }
        else if(std::strstr(buf, "baseframe {"))
        {
            while(f->getline(buf, sizeof(buf)) && buf[0]!='}') //loop until end of {} block
            {
                md5joint j;
                //pick up pos/orient 3-vectors within
                if(std::sscanf(buf, " ( %f %f %f ) ( %f %f %f )", &j.pos.x, &j.pos.y, &j.pos.z, &j.orient.x, &j.orient.y, &j.orient.z)==6)
                {
                    j.pos.y = -j.pos.y;
                    j.orient.x = -j.orient.x;
                    j.orient.z = -j.orient.z;
                    j.orient.restorew();
                    basejoints.push_back(j);
                }
            }
            if(basejoints.size() != skel->numbones)
            {
                delete f;
                if(animdata)
                {
                    delete[] animdata;
                }
                return nullptr;
            }
            animbones = new dualquat[(skel->numframes+animframes)*skel->numbones];
            if(skel->framebones)
            {
                std::memcpy(animbones, skel->framebones, skel->numframes*skel->numbones*sizeof(dualquat));
                delete[] skel->framebones;
            }
            skel->framebones = animbones;
            animbones += skel->numframes*skel->numbones;

            sas = &skel->addskelanim(filename, skel->numframes, animframes);

            skel->numframes += animframes;
        }
        else if(std::sscanf(buf, " frame %d", &tmp)==1)
        {
            for(int numdata = 0; f->getline(buf, sizeof(buf)) && buf[0]!='}';)
            {
                for(char *src = buf, *next = src; numdata < animdatalen; numdata++, src = next)
                {
                    animdata[numdata] = std::strtod(src, &next);
                    if(next <= src)
                    {
                        break;
                    }
                }
            }
            dualquat *frame = &animbones[tmp*skel->numbones];
            if(basejoints.size() != hierarchy.size())
            {
                conoutf("Invalid model data: hierarchy (%lu) and baseframe (%lu) size mismatch", hierarchy.size(), basejoints.size());
                return nullptr;
            }
            for(uint i = 0; i < basejoints.size(); i++)
            {
                const md5hierarchy &h = hierarchy[i];
                md5joint j = basejoints[i]; //intentionally getting by value to modify temp copy
                if(h.start < animdatalen && h.flags)
                {
                    const float *jdata = &animdata[h.start];
                    //bitwise AND against bits 0...5
                    if(h.flags & 1)
                    {
                        j.pos.x = *jdata++;
                    }
                    if(h.flags & 2)
                    {
                        j.pos.y = -*jdata++;
                    }
                    if(h.flags & 4)
                    {
                        j.pos.z = *jdata++;
                    }
                    if(h.flags & 8)
                    {
                        j.orient.x = -*jdata++;
                    }
                    if(h.flags & 16)
                    {
                        j.orient.y = *jdata++;
                    }
                    if(h.flags & 32)
                    {
                        j.orient.z = -*jdata++;
                    }
                    j.orient.restorew();
                }
                dualquat dq(j.orient, j.pos);
                if(adjustments.size() > i)
                {
                    adjustments[i].adjust(dq);
                }
                //assume nullopt cannot happen
                dq.mul(skel->getbonebase(i).value().invert());
                dualquat &dst = frame[i];
                if(h.parent < 0)
                {
                    dst = dq;
                }
                else
                {
                    dst.mul(skel->getbonebase(h.parent).value(), dq);
                }
                dst.fixantipodal(skel->framebones[i]);
            }
        }
    }

    if(animdata)
    {
        delete[] animdata;
    }
    delete f;

    return sas;
}

bool md5::md5meshgroup::loadmesh(const char *filename, float smooth, part &p)
{
    stream *f = openfile(filename, "r");
    if(!f) //immediately bail if no file present
    {
        return false;
    }
    char buf[512]; //presumably this will fail with lines over 512 char long
    std::vector<md5joint> basejoints;
    while(f->getline(buf, sizeof(buf)))
    {
        int tmp;
        if(std::sscanf(buf, " MD5Version %d", &tmp)==1)
        {
            if(tmp!=10)
            {
                delete f; //bail out if md5version is not what we want
                return false;
            }
        }
        else if(std::sscanf(buf, " numJoints %d", &tmp)==1)
        {
            if(tmp<1)
            {
                delete f; //bail out if no joints found
                return false;
            }
            if(skel->numbones>0) //if we have bones, keep going
            {
                continue;
            }
            skel->createbones(tmp);
        }
        else if(std::sscanf(buf, " numMeshes %d", &tmp)==1)
        {
            if(tmp<1)
            {
                delete f; //if there's no meshes, nothing to be done
                return false;
            }
        }
        else if(std::strstr(buf, "joints {"))
        {
            string name;
            int parent;
            md5joint j;
            while(f->getline(buf, sizeof(buf)) && buf[0]!='}')
            {
                const char *curbuf = buf;
                      char *curname = name;
                bool allowspace = false;
                while(*curbuf && std::isspace(*curbuf))
                {
                    curbuf++;
                }
                if(*curbuf == '"')
                {
                    curbuf++;
                    allowspace = true;
                }
                while(*curbuf && curname < &name[sizeof(name)-1])
                {
                    char c = *curbuf++;
                    if(c == '"')
                    {
                        break;
                    }
                    if(std::isspace(c) && !allowspace)
                    {
                        break;
                    }
                    *curname++ = c;
                }
                *curname = '\0';
                //pickup parent, pos/orient 3-vectors
                if(std::sscanf(curbuf, " %d ( %f %f %f ) ( %f %f %f )",
                    &parent, &j.pos.x, &j.pos.y, &j.pos.z,
                    &j.orient.x, &j.orient.y, &j.orient.z)==7)
                {
                    j.pos.y = -j.pos.y;
                    j.orient.x = -j.orient.x;
                    j.orient.z = -j.orient.z;
                    if(basejoints.size() < skel->numbones)
                    {
                        skel->setbonename(basejoints.size(), name);
                        skel->setboneparent(basejoints.size(), parent);
                    }
                    j.orient.restorew();
                    basejoints.push_back(j);
                }
            }
            if(basejoints.size() != skel->numbones)
            {
                delete f;
                return false;
            }
        }
        //load up meshes
        else if(std::strstr(buf, "mesh {"))
        {
            md5mesh *m = new md5mesh;
            m->group = this;
            meshes.push_back(m);

            std::string modeldir = filename;
            modeldir.resize(modeldir.rfind("/")); //truncate to file's directory
            m->load(f, buf, sizeof(buf), p, modeldir);
            if(!m->numtris || !m->numverts) //if no content in the mesh
            {
                conoutf("empty mesh in %s", filename);
                //double std::find of the same thing not the most efficient
                if(std::find(meshes.begin(), meshes.end(), m) != meshes.end())
                {
                    meshes.erase(std::find(meshes.begin(), meshes.end(), m));
                }
                delete m;
            }
        }
    }

    skel->linkchildren();
    //assign basejoints accumulated above
    {
        std::vector<dualquat> bases;
        for(const md5joint &m : basejoints)
        {
            bases.emplace_back(m.orient, m.pos);
        }
        skel->setbonebases(bases);
    }
    for(uint i = 0; i < meshes.size(); i++)
    {
        md5mesh &m = *static_cast<md5mesh *>(meshes[i]);
        m.buildverts(basejoints);
        if(smooth <= 1)
        {
            m.smoothnorms(smooth);
        }
        else
        {
            m.buildnorms();
        }
        m.calctangents();
        m.cleanup();
    }

    sortblendcombos();

    delete f;
    return true;
}

bool md5::md5meshgroup::load(std::string_view meshfile, float smooth, part &p)
{
    name = meshfile;

    if(!loadmesh(meshfile.data(), smooth, p))
    {
        return false;
    }
    return true;
}

md5::md5mesh::md5mesh() : weightinfo(nullptr), numweights(0), vertinfo(nullptr)
{
}

md5::md5mesh::~md5mesh()
{
    cleanup();
}

void md5::md5mesh::cleanup()
{
    delete[] weightinfo;
    delete[] vertinfo;
    vertinfo = nullptr;
    weightinfo = nullptr;
}

void md5::md5mesh::buildverts(const std::vector<md5joint> &joints)
{
    for(int i = 0; i < numverts; ++i)
    {
        md5vert &v = vertinfo[i];
        vec pos(0, 0, 0);
        for(uint k = 0; k < v.count; ++k)
        {
            const md5weight &w = weightinfo[v.start+k];
            const md5joint &j = joints[w.joint];
            vec wpos = j.orient.rotate(w.pos);
            wpos.add(j.pos);
            wpos.mul(w.bias);
            pos.add(wpos);
        }
        vert &vv = verts[i];
        vv.pos = pos;
        vv.tc = v.tc;

        blendcombo c;
        int sorted = 0;
        for(uint j = 0; j < v.count; ++j)
        {
            const md5weight &w = weightinfo[v.start+j];
            sorted = c.addweight(sorted, w.bias, w.joint);
        }
        c.finalize(sorted);
        vv.blend = addblendcombo(c);
    }
}

//md5 model loader
void  md5::md5mesh::load(stream *f, char *buf, size_t bufsize, part &p, const std::string &modeldir)
{
    md5weight w;
    md5vert v;
    tri t;
    int index;

    while(f->getline(buf, bufsize) && buf[0]!='}')
    {
        if(std::strstr(buf, "// meshes:"))
        {
            char *start = std::strchr(buf, ':')+1;
            if(*start==' ')
            {
                start++;
            }
            char *end = start + std::strlen(start)-1;
            while(end >= start && std::isspace(*end))
            {
                end--;
            }
            name = newstring(start, end+1-start);
        }
        else if(std::strstr(buf, "shader"))
        {
            char *start = std::strchr(buf, '"'),
                 *end = start ? std::strchr(start+1, '"') : nullptr;
            if(start && end)
            {
                char *texname = newstring(start+1, end-(start+1));
                p.initskins(notexture, notexture, group->meshes.size());
                skin &s = p.skins.back();
                s.tex = textureload(makerelpath(modeldir.c_str(), texname), 0, true, false);
                delete[] texname;
            }
        }
        //create the vert arrays
        else if(std::sscanf(buf, " numverts %d", &numverts)==1)
        {
            numverts = std::max(numverts, 0);
            if(numverts)
            {
                vertinfo = new md5vert[numverts];
                verts = new vert[numverts];
            }
        }
        //create tri array
        else if(std::sscanf(buf, " numtris %d", &numtris)==1)
        {
            numtris = std::max(numtris, 0);
            if(numtris)
            {
                tris = new tri[numtris];
            }
        }
        //create md5weight array
        else if(std::sscanf(buf, " numweights %d", &numweights)==1)
        {
            numweights = std::max(numweights, 0);
            if(numweights)
            {
                weightinfo = new md5weight[numweights];
            }
        }
        //assign md5verts to vertinfo array
        else if(std::sscanf(buf, " vert %d ( %f %f ) %u %u", &index, &v.tc.x, &v.tc.y, &v.start, &v.count)==5)
        {
            if(index>=0 && index<numverts)
            {
                vertinfo[index] = v;
            }
        }
        // assign tris to tri array
        else if(std::sscanf(buf, " tri %d %u %u %u", &index, &t.vert[0], &t.vert[1], &t.vert[2])==4)
        {
            if(index>=0 && index<numtris)
            {
                tris[index] = t;
            }
        }
        //assign md5weights to weights array
        else if(std::sscanf(buf, " weight %d %d %f ( %f %f %f ) ", &index, &w.joint, &w.bias, &w.pos.x, &w.pos.y, &w.pos.z)==6)
        {
            w.pos.y = -w.pos.y;
            if(index>=0 && index<numweights)
            {
                weightinfo[index] = w;
            }
        }
    }
}

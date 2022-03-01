
class md5 : public skelloader<md5>
{
    public:
        md5(const char *name) : skelloader(name) {}

        static const char *formatname()
        {
            return "md5";
        }

        int type() const
        {
            return MDL_MD5;
        }

        skelmeshgroup *newmeshes()
        {
            return new md5meshgroup;
        }

        bool loaddefaultparts()
        {
            skelpart &mdl = addpart();
            const char *fname = name + std::strlen(name);
            do
            {
                --fname;
            } while(fname >= name && *fname!='/' && *fname!='\\');
            fname++;
            DEF_FORMAT_STRING(meshname, "media/model/%s/%s.md5mesh", name, fname);
            mdl.meshes = sharemeshes(path(meshname));
            if(!mdl.meshes)
            {
                return false;
            }
            mdl.initanimparts();
            mdl.initskins();
            DEF_FORMAT_STRING(animname, "media/model/%s/%s.md5anim", name, fname);
            static_cast<md5meshgroup *>(mdl.meshes)->loadanim(path(animname));
            return true;
        }

    private:
        struct md5joint
        {
            vec pos;
            quat orient;
        };

        struct md5weight
        {
            int joint;
            float bias;
            vec pos;
        };

        struct md5vert
        {
            vec2 tc;
            ushort start, count;
        };

        struct md5hierarchy
        {
            string name;
            int parent, flags, start;
        };

        class md5meshgroup : public skelmeshgroup
        {
            public:
                md5meshgroup()
                {
                }

                //main anim loading functionality
                skelanimspec *loadanim(const char *filename)
                {
                    skelanimspec *sa = skel->findskelanim(filename);
                    if(sa)
                    {
                        return sa;
                    }
                    stream *f = openfile(filename, "r");
                    if(!f)
                    {
                        return nullptr;
                    }
                    vector<md5hierarchy> hierarchy;
                    vector<md5joint> basejoints;
                    int animdatalen = 0,
                        animframes = 0;
                    float *animdata = nullptr;
                    dualquat *animbones = nullptr;
                    char buf[512]; //presumably lines over 512 char long will break this loader
                    //for each line in the opened file
                    while(f->getline(buf, sizeof(buf)))
                    {
                        int tmp;
                        if(std::sscanf(buf, " MD5Version %d", &tmp) == 1)
                        {
                            if(tmp != 10)
                            {
                                delete f; //bail out if md5version is not what we want
                                return nullptr;
                            }
                        }
                        else if(std::sscanf(buf, " numJoints %d", &tmp) == 1)
                        {
                            if(tmp != skel->numbones)
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
                                    hierarchy.add(h);
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
                                    basejoints.add(j);
                                }
                            }
                            if(basejoints.length()!=skel->numbones)
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
                                memcpy(animbones, skel->framebones, skel->numframes*skel->numbones*sizeof(dualquat));
                                delete[] skel->framebones;
                            }
                            skel->framebones = animbones;
                            animbones += skel->numframes*skel->numbones;

                            sa = &skel->addskelanim(filename);
                            sa->frame = skel->numframes;
                            sa->range = animframes;

                            skel->numframes += animframes;
                        }
                        else if(std::sscanf(buf, " frame %d", &tmp)==1)
                        {
                            for(int numdata = 0; f->getline(buf, sizeof(buf)) && buf[0]!='}';)
                            {
                                for(char *src = buf, *next = src; numdata < animdatalen; numdata++, src = next)
                                {
                                    animdata[numdata] = strtod(src, &next);
                                    if(next <= src)
                                    {
                                        break;
                                    }
                                }
                            }
                            dualquat *frame = &animbones[tmp*skel->numbones];
                            for(int i = 0; i < basejoints.length(); i++)
                            {
                                md5hierarchy &h = hierarchy[i];
                                md5joint j = basejoints[i];
                                if(h.start < animdatalen && h.flags)
                                {
                                    float *jdata = &animdata[h.start];
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
                                if(static_cast<int>(adjustments.size()) > i)
                                {
                                    adjustments[i].adjust(dq);
                                }
                                boneinfo &b = skel->bones[i];
                                dq.mul(b.invbase);
                                dualquat &dst = frame[i];
                                if(h.parent < 0)
                                {
                                    dst = dq;
                                }
                                else
                                {
                                    dst.mul(skel->bones[h.parent].base, dq);
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

                    return sa;
                }

            private:
                bool loadmesh(const char *filename, float smooth)
                {
                    stream *f = openfile(filename, "r");
                    if(!f) //immediately bail if no file present
                    {
                        return false;
                    }
                    char buf[512]; //presumably this will fail with lines over 512 char long
                    vector<md5joint> basejoints;
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
                            skel->numbones = tmp;
                            skel->bones = new boneinfo[skel->numbones];
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
                                char *curbuf = buf,
                                     *curname = name;
                                bool allowspace = false;
                                while(*curbuf && isspace(*curbuf))
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
                                    if(isspace(c) && !allowspace)
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
                                    if(basejoints.length()<skel->numbones)
                                    {
                                        if(!skel->bones[basejoints.length()].name)
                                        {
                                            skel->bones[basejoints.length()].name = newstring(name);
                                        }
                                        skel->bones[basejoints.length()].parent = parent;
                                    }
                                    j.orient.restorew();
                                    basejoints.add(j);
                                }
                            }
                            if(basejoints.length()!=skel->numbones)
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
                            meshes.add(m);
                            m->load(f, buf, sizeof(buf));
                            if(!m->numtris || !m->numverts) //if no content in the mesh
                            {
                                conoutf("empty mesh in %s", filename);
                                meshes.removeobj(m);
                                delete m;
                            }
                        }
                    }

                    if(skel->shared <= 1)
                    {
                        skel->linkchildren();
                        for(int i = 0; i < basejoints.length(); i++)
                        {
                            boneinfo &b = skel->bones[i];
                            b.base = dualquat(basejoints[i].orient, basejoints[i].pos);
                            (b.invbase = b.base).invert();
                        }
                    }

                    for(int i = 0; i < meshes.length(); i++)
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

                bool load(const char *meshfile, float smooth)
                {
                    name = newstring(meshfile);

                    if(!loadmesh(meshfile, smooth))
                    {
                        return false;
                    }
                    return true;
                }
        };


        //extensions to skelmesh objects for md5 specifically
        class md5mesh : public skelmesh
        {
            public:
                md5mesh() : weightinfo(nullptr), numweights(0), vertinfo(nullptr)
                {
                }

                ~md5mesh()
                {
                    cleanup();
                }

                void cleanup()
                {
                    delete[] weightinfo;
                    delete[] vertinfo;
                    vertinfo = nullptr;
                    weightinfo = nullptr;
                }

                void buildverts(vector<md5joint> &joints)
                {
                    for(int i = 0; i < numverts; ++i)
                    {
                        md5vert &v = vertinfo[i];
                        vec pos(0, 0, 0);
                        for(int k = 0; k < v.count; ++k)
                        {
                            md5weight &w = weightinfo[v.start+k];
                            md5joint &j = joints[w.joint];
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
                        for(int j = 0; j < v.count; ++j)
                        {
                            md5weight &w = weightinfo[v.start+j];
                            sorted = c.addweight(sorted, w.bias, w.joint);
                        }
                        c.finalize(sorted);
                        vv.blend = addblendcombo(c);
                    }
                }

                //md5 model loader
                void load(stream *f, char *buf, size_t bufsize)
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
                            while(end >= start && isspace(*end))
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
                                part *p = loading->parts.last();
                                p->initskins(notexture, notexture, group->meshes.length());
                                skin &s = p->skins.back();
                                s.tex = textureload(makerelpath(dir, texname), 0, true, false);
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
                        else if(std::sscanf(buf, " vert %d ( %f %f ) %hu %hu", &index, &v.tc.x, &v.tc.y, &v.start, &v.count)==5)
                        {
                            if(index>=0 && index<numverts)
                            {
                                vertinfo[index] = v;
                            }
                        }
                        // assign tris to tri array
                        else if(std::sscanf(buf, " tri %d %hu %hu %hu", &index, &t.vert[0], &t.vert[1], &t.vert[2])==4)
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
            private:
                md5weight *weightinfo;
                int numweights;
                md5vert *vertinfo;
        };
};

skelcommands<md5> md5commands; //see skelmodel.h for these commands


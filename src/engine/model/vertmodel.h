struct vertmodel : animmodel
{
    struct vert
    {
        vec pos, norm;
        vec4<float> tangent;
    };

    struct vvert
    {
        vec pos;
        GenericVec2<half> tc;
        squat tangent;
    };

    struct vvertg
    {
        vec4<half> pos;
        GenericVec2<half> tc;
        squat tangent;
    };

    struct tcvert
    {
        vec2 tc;
    };

    struct tri
    {
        ushort vert[3];
    };

    struct vbocacheentry
    {
        GLuint vbuf;
        AnimState as;
        int millis;

        vbocacheentry() : vbuf(0) { as.cur.fr1 = as.prev.fr1 = -1; }
    };

    struct vertmesh : Mesh
    {
        vert *verts;
        tcvert *tcverts;
        tri *tris;
        int numverts, numtris;

        int voffset, eoffset, elen;
        ushort minvert, maxvert;

        vertmesh() : verts(0), tcverts(0), tris(0)
        {
        }

        virtual ~vertmesh()
        {
            delete[] verts;
            delete[] tcverts;
            delete[] tris;
        }

        void smoothnorms(float limit = 0, bool areaweight = true)
        {
            if((static_cast<vertmeshgroup *>(group))->numframes == 1)
            {
                Mesh::smoothnorms(verts, numverts, tris, numtris, limit, areaweight);
            }
            else
            {
                buildnorms(areaweight);
            }
        }

        void buildnorms(bool areaweight = true)
        {
            Mesh::buildnorms(verts, numverts, tris, numtris, areaweight, (static_cast<vertmeshgroup *>(group))->numframes);
        }

        void calctangents(bool areaweight = true)
        {
            Mesh::calctangents(verts, tcverts, numverts, tris, numtris, areaweight, (static_cast<vertmeshgroup *>(group))->numframes);
        }

        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m)
        {
            for(int j = 0; j < numverts; ++j)
            {
                vec v = m.transform(verts[j].pos);
                bbmin.min(v);
                bbmax.max(v);
            }
        }

        void genBIH(BIH::mesh &m)
        {
            m.tris = reinterpret_cast<const BIH::tri *>(tris);
            m.numtris = numtris;
            m.pos = reinterpret_cast<const uchar *>(&verts->pos);
            m.posstride = sizeof(vert);
            m.tc = reinterpret_cast<const uchar *>(&tcverts->tc);
            m.tcstride = sizeof(tcvert);
        }

        void genshadowmesh(std::vector<triangle> &out, const matrix4x3 &m)
        {
            for(int j = 0; j < numtris; ++j)
            {
                triangle t;
                t.a = m.transform(verts[tris[j].vert[0]].pos);
                t.b = m.transform(verts[tris[j].vert[1]].pos);
                t.c = m.transform(verts[tris[j].vert[2]].pos);
                out.push_back(t);
            }
        }

        static void assignvert(vvertg &vv, int j, tcvert &tc, vert &v)
        {
            vv.pos = vec4<half>(v.pos, 1);
            vv.tc = tc.tc;
            vv.tangent = v.tangent;
        }

        template<class T>
        int genvbo(vector<ushort> &idxs, int offset, vector<T> &vverts, int *htdata, int htlen)
        {
            voffset = offset;
            eoffset = idxs.length();
            minvert = 0xFFFF;
            for(int i = 0; i < numtris; ++i)
            {
                tri &t = tris[i];
                for(int j = 0; j < 3; ++j)
                {
                    int index = t.vert[j];
                    vert &v = verts[index];
                    tcvert &tc = tcverts[index];
                    T vv;
                    assignvert(vv, index, tc, v);
                    int htidx = hthash(v.pos)&(htlen-1);
                    for(int k = 0; k < htlen; ++k)
                    {
                        int &vidx = htdata[(htidx+k)&(htlen-1)];
                        if(vidx < 0)
                        {
                            vidx = idxs.add(static_cast<ushort>(vverts.length()));
                            vverts.add(vv);
                            break;
                        }
                        else if(!memcmp(&vverts[vidx], &vv, sizeof(vv)))
                        {
                            minvert = std::min(minvert, idxs.add(static_cast<ushort>(vidx)));
                            break;
                        }
                    }
                }
            }
            minvert = std::min(minvert, static_cast<ushort>(voffset));
            maxvert = std::max(minvert, static_cast<ushort>(vverts.length()-1));
            elen = idxs.length()-eoffset;
            return vverts.length()-voffset;
        }

        int genvbo(vector<ushort> &idxs, int offset)
        {
            voffset = offset;
            eoffset = idxs.length();
            for(int i = 0; i < numtris; ++i)
            {
                tri &t = tris[i];
                for(int j = 0; j < 3; ++j)
                {
                    idxs.add(voffset+t.vert[j]);
                }
            }
            minvert = voffset;
            maxvert = voffset + numverts-1;
            elen = idxs.length()-eoffset;
            return numverts;
        }

        template<class T>
        static void fillvert(T &vv, int j, tcvert &tc, vert &v)
        {
            vv.tc = tc.tc;
        }

        template<class T>
        void fillverts(T *vdata)
        {
            vdata += voffset;
            for(int i = 0; i < numverts; ++i)
            {
                fillvert(vdata[i], i, tcverts[i], verts[i]);
            }
        }

        template<class T>
        void interpverts(const AnimState &as, T * RESTRICT vdata, skin &s)
        {
            vdata += voffset;
            const vert * RESTRICT vert1 = &verts[as.cur.fr1 * numverts],
                       * RESTRICT vert2 = &verts[as.cur.fr2 * numverts],
                       * RESTRICT pvert1 = as.interp<1 ? &verts[as.prev.fr1 * numverts] : nullptr,
                       * RESTRICT pvert2 = as.interp<1 ? &verts[as.prev.fr2 * numverts] : nullptr;
                       //lerp: Linear intERPolation
            //========================================================== IP_VERT
            //InterPolate_VERTex
            #define IP_VERT(attrib, type) v.attrib.lerp(vert1[i].attrib, vert2[i].attrib, as.cur.t)
            #define IP_VERT_P(attrib, type) v.attrib.lerp(type().lerp(pvert1[i].attrib, pvert2[i].attrib, as.prev.t), type().lerp(vert1[i].attrib, vert2[i].attrib, as.cur.t), as.interp)
            if(as.interp<1)
            {
                for(int i = 0; i < numverts; ++i)
                {
                    T &v = vdata[i];
                    IP_VERT_P(pos, vec);
                    IP_VERT_P(tangent, vec4<float>);
                }
            }
            else
            {
                for(int i = 0; i < numverts; ++i)
                {
                    T &v = vdata[i];
                    IP_VERT(pos, vec);
                    IP_VERT(tangent, vec4<float>);
                }
            }
            #undef IP_VERT
            #undef IP_VERT_P
            //==================================================================
        }

        void render(const AnimState *as, skin &s, vbocacheentry &vc)
        {
            if(!Shader::lastshader)
            {
                return;
            }
            glDrawRangeElements_(GL_TRIANGLES, minvert, maxvert, elen, GL_UNSIGNED_SHORT, &(static_cast<vertmeshgroup *>(group))->edata[eoffset]);
            glde++;
            xtravertsva += numverts;
        }
    };

    struct tag
    {
        char *name;
        matrix4x3 matrix;

        tag() : name(nullptr) {}
        ~tag()
        {
            delete[] name;
        }
    };

    struct vertmeshgroup : meshgroup
    {
        int numframes;
        tag *tags;
        int numtags;

        static constexpr int maxvbocache = 16;
        vbocacheentry vbocache[maxvbocache];

        ushort *edata;
        GLuint ebuf;
        int vlen, vertsize;
        uchar *vdata;

        vertmeshgroup() : numframes(0), tags(nullptr), numtags(0), edata(nullptr), ebuf(0), vlen(0), vertsize(0), vdata(nullptr)
        {
        }

        virtual ~vertmeshgroup()
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

        int findtag(const char *name)
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

        bool addtag(const char *name, const matrix4x3 &matrix)
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

        int totalframes() const
        {
            return numframes;
        }

        void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n)
        {
            n.mul(m, tags[i].matrix);
        }

        void calctagmatrix(part *p, int i, const AnimState &as, matrix4 &matrix)
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

        void genvbo(vbocacheentry &vc)
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

        template<class T>
        void bindvbo(const AnimState *as, part *p, vbocacheentry &vc)
        {
            T *vverts = 0;
            bindpos(ebuf, vc.vbuf, &vverts->pos, vertsize);
            if(as->cur.anim & Anim_NoSkin)
            {
                if(enabletangents)
                {
                    disabletangents();
                }
                if(p->alphatested())
                {
                    bindtc(&vverts->tc, vertsize);
                }
                else if(enabletc)
                {
                    disabletc();
                }
            }
            else
            {
                bindtangents(&vverts->tangent, vertsize);
                bindtc(&vverts->tc, vertsize);
            }
            if(enablebones)
            {
                disablebones();
            }
        }

        void bindvbo(const AnimState *as, part *p, vbocacheentry &vc)
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

        void cleanup()
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

        void preload(part *p)
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

        void render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p)
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

        virtual bool load(const char *name, float smooth) = 0;
    };

    virtual vertmeshgroup *newmeshes() = 0;

    meshgroup *loadmeshes(const char *name, float smooth = 2)
    {
        vertmeshgroup *group = newmeshes();
        if(!group->load(name, smooth))
        {
            delete group;
            return nullptr;
        }
        return group;
    }

    meshgroup *sharemeshes(const char *name, float smooth = 2)
    {
        if(!meshgroups.access(name))
        {
            meshgroup *group = loadmeshes(name, smooth);
            if(!group)
            {
                return nullptr;
            }
            meshgroups.add(group);
        }
        return meshgroups[name];
    }

    vertmodel(const char *name) : animmodel(name)
    {
    }
};

template<class MDL>
struct vertloader : modelloader<MDL, vertmodel>
{
    vertloader(const char *name) : modelloader<MDL, vertmodel>(name) {}
};

template<class MDL>
struct vertcommands : modelcommands<MDL, struct MDL::vertmesh>
{
    typedef struct MDL::vertmeshgroup meshgroup;
    typedef struct MDL::part part;
    typedef struct MDL::skin skin;

    static void loadpart(char *model, float *smooth)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir, model);
        part &mdl = MDL::loading->addpart();
        if(mdl.index)
        {
            mdl.disablepitch();
        }
        mdl.meshes = MDL::loading->sharemeshes(path(filename), *smooth > 0 ? std::cos(std::clamp(*smooth, 0.0f, 180.0f)*RAD) : 2);
        if(!mdl.meshes)
        {
            conoutf("could not load %s", filename);
        }
        else
        {
            mdl.initskins();
        }
    }

    static void settag(char *tagname, float *tx, float *ty, float *tz, float *rx, float *ry, float *rz)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *static_cast<part *>(MDL::loading->parts.last());
        float cx = *rx ? std::cos(*rx/2*RAD) : 1,
              sx = *rx ? std::sin(*rx/2*RAD) : 0,
              cy = *ry ? std::cos(*ry/2*RAD) : 1,
              sy = *ry ? std::sin(*ry/2*RAD) : 0,
              cz = *rz ? std::cos(*rz/2*RAD) : 1,
              sz = *rz ? std::sin(*rz/2*RAD) : 0;
        //matrix m created from (matrix3 created from quat) + (vec) appended afterwards
        matrix4x3 m(static_cast<matrix3>(quat(sx*cy*cz - cx*sy*sz,
                                              cx*sy*cz + sx*cy*sz,
                                              cx*cy*sz - sx*sy*cz,
                                              cx*cy*cz + sx*sy*sz)),
                    vec(*tx, *ty, *tz));

        static_cast<meshgroup *>(mdl.meshes)->addtag(tagname, m);
    }

    static void setpitch(float *pitchscale, float *pitchoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        part &mdl = *MDL::loading->parts.last();
        mdl.pitchscale = *pitchscale;
        mdl.pitchoffset = *pitchoffset;
        if(*pitchmin || *pitchmax)
        {
            mdl.pitchmin = *pitchmin;
            mdl.pitchmax = *pitchmax;
        }
        else
        {
            mdl.pitchmin = -360*std::fabs(mdl.pitchscale) + mdl.pitchoffset;
            mdl.pitchmax =  360*std::fabs(mdl.pitchscale) + mdl.pitchoffset;
        }
    }

    static void setanim(char *anim, int *frame, int *range, float *speed, int *priority)
    {
        if(!MDL::loading || MDL::loading->parts.empty())
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        std::vector<int> anims = findanims(anim);
        if(anims.empty())
        {
            conoutf("could not find animation %s", anim);
        }
        else
        {
            for(int i = 0; i < static_cast<int>(anims.size()); i++)
            {
                MDL::loading->parts.last()->setanim(0, anims[i], *frame, *range, *speed, *priority);
            }
        }
    }

    vertcommands()
    {
        if(MDL::multiparted())
        {
            this->modelcommand(loadpart, "load", "sf"); //<fmt>load [model] [smooth]
        }
        this->modelcommand(settag, "tag", "sffffff"); //<fmt>tag [tagname] [tx] [ty] [tz] [rx] [ry] [rz]
        this->modelcommand(setpitch, "pitch", "ffff"); //<fmt>pitch [scale] [offset] [min] [max]
        if(MDL::cananimate())
        {
            this->modelcommand(setanim, "anim", "siiff"); //<fmt>anim [anim] [frame] [range] [speed] [priority]
        }
    }
};

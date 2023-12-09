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
        uint vert[3];
    };

    struct vbocacheentry
    {
        GLuint vbuf;
        AnimState as;
        int millis;

        vbocacheentry();
    };

    struct vertmesh : Mesh
    {
        vert *verts;
        tcvert *tcverts;
        tri *tris;
        int numverts, numtris;

        int voffset, elen;
        uint minvert, maxvert;

        vertmesh();
        virtual ~vertmesh();

        void smoothnorms(float limit = 0, bool areaweight = true);
        void buildnorms(bool areaweight = true);
        void calctangents(bool areaweight = true);
        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m);
        void genBIH(BIH::mesh &m) const;
        void genshadowmesh(std::vector<triangle> &out, const matrix4x3 &m) const;

        static void assignvert(vvertg &vv, int j, const tcvert &tc, const vert &v);

        template<class T>
        int genvbo(std::vector<uint> &idxs, int offset, std::vector<T> &vverts, int *htdata, int htlen)
        {
            voffset = offset;
            minvert = UINT_MAX;
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
                    auto hashfn = std::hash<vec>();
                    int htidx = hashfn(v.pos)&(htlen-1);
                    for(int k = 0; k < htlen; ++k)
                    {
                        int &vidx = htdata[(htidx+k)&(htlen-1)];
                        if(vidx < 0)
                        {
                            idxs.push_back(vverts.size());
                            vidx = idxs.back();
                            vverts.push_back(vv);
                            break;
                        }
                        else if(!std::memcmp(&vverts[vidx], &vv, sizeof(vv)))
                        {
                            idxs.push_back(static_cast<uint>(vidx));
                            minvert = std::min(minvert, idxs.back());
                            break;
                        }
                    }
                }
            }
            minvert = std::min(minvert, static_cast<uint>(voffset));
            maxvert = std::max(minvert, static_cast<uint>(vverts.size()-1));
            elen = idxs.size();
            return vverts.size()-voffset;
        }

        int genvbo(std::vector<uint> &idxs, int offset);

        template<class T>
        static void fillvert(T &vv, tcvert &tc, vert &v)
        {
            vv.tc = tc.tc;
        }

        template<class T>
        void fillverts(T *vdata)
        {
            vdata += voffset;
            for(int i = 0; i < numverts; ++i)
            {
                fillvert(vdata[i], tcverts[i], verts[i]);
            }
        }

        template<class T>
        void interpverts(const AnimState &as, T * RESTRICT vdata)
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

        void render();
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

        GLuint ebuf;
        int vlen, vertsize;
        uchar *vdata;

        vertmeshgroup();
        virtual ~vertmeshgroup();

        bool addtag(const char *name, const matrix4x3 &matrix);
        int findtag(const char *name);

        int totalframes() const;
        void calctagmatrix(const part *p, int i, const AnimState &as, matrix4 &matrix) const;

        void genvbo(vbocacheentry &vc);

        template<class T>
        void bindvbo(const AnimState *as, const part *p, const vbocacheentry &vc)
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

        void bindvbo(const AnimState *as, const part *p, const vbocacheentry &vc);
        void cleanup();
        void preload();
        void render(const AnimState *as, float, const vec &, const vec &, dynent *, part *p);

        virtual bool load(const char *name, float smooth) = 0;
    };

    virtual vertmeshgroup *newmeshes() = 0;

    meshgroup *loadmeshes(const char *name, float smooth = 2);
    meshgroup *sharemeshes(const char *name, float smooth = 2);

    vertmodel(const char *name);
};

template<class MDL>
struct vertloader : modelloader<MDL, vertmodel>
{
    vertloader(const char *name) : modelloader<MDL, vertmodel>(name) {}
};

template<class MDL>
struct vertcommands : modelcommands<MDL>
{
    typedef struct MDL::vertmeshgroup meshgroup;
    typedef class  MDL::part part;
    typedef struct MDL::skin skin;

    static void loadpart(char *model, float *smooth)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        DEF_FORMAT_STRING(filename, "%s/%s", MDL::dir.c_str(), model);
        part &mdl = MDL::loading->addpart();
        if(mdl.index)
        {
            mdl.disablepitch();
        }
        mdl.meshes = MDL::loading->sharemeshes(path(filename), *smooth > 0 ? std::cos(std::clamp(*smooth, 0.0f, 180.0f)/RAD) : 2);
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
        const part &mdl = *static_cast<part *>(MDL::loading->parts.back());
        float cx = *rx ? std::cos(*rx/(2*RAD)) : 1,
              sx = *rx ? std::sin(*rx/(2*RAD)) : 0,
              cy = *ry ? std::cos(*ry/(2*RAD)) : 1,
              sy = *ry ? std::sin(*ry/(2*RAD)) : 0,
              cz = *rz ? std::cos(*rz/(2*RAD)) : 1,
              sz = *rz ? std::sin(*rz/(2*RAD)) : 0;
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
        part &mdl = *MDL::loading->parts.back();
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
            for(const int &i : anims)
            {
                MDL::loading->parts.back()->setanim(0, i, *frame, *range, *speed, *priority);
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

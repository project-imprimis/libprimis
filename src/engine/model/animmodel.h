#ifndef ANIMMODEL_H_
#define ANIMMODEL_H_

extern int fullbrightmodels, testtags, dbgcolmesh;

struct animmodel : model
{
    struct animspec
    {
        int frame, range;
        float speed;
        int priority;
    };

    struct animpos
    {
        int anim, fr1, fr2;
        float t;

        void setframes(const animinfo &info);

        bool operator==(const animpos &a) const
        {
            return fr1==a.fr1 && fr2==a.fr2 && (fr1==fr2 || t==a.t);
        }
        bool operator!=(const animpos &a) const
        {
            return fr1!=a.fr1 || fr2!=a.fr2 || (fr1!=fr2 && t!=a.t);
        }
    };

    struct part;

    struct animstate
    {
        part *owner;
        animpos cur, prev;
        float interp;

        bool operator==(const animstate &a) const
        {
            return cur==a.cur && (interp<1 ? interp==a.interp && prev==a.prev : a.interp>=1);
        }
        bool operator!=(const animstate &a) const
        {
            return cur!=a.cur || (interp<1 ? interp!=a.interp || prev!=a.prev : a.interp<1);
        }
    };

    struct linkedpart;
    struct mesh;

    struct shaderparams
    {
        float spec, gloss, glow, glowdelta, glowpulse, fullbright, scrollu, scrollv, alphatest;
        vec color;

        shaderparams() : spec(1.0f), gloss(1), glow(3.0f), glowdelta(0), glowpulse(0), fullbright(0), scrollu(0), scrollv(0), alphatest(0.9f), color(1, 1, 1) {}
    };

    struct shaderparamskey
    {
        static hashtable<shaderparams, shaderparamskey> keys;
        static int firstversion, lastversion;

        int version;

        shaderparamskey() : version(-1) {}

        bool checkversion();

        static inline void invalidate()
        {
            firstversion = lastversion;
        }
    };

    struct skin : shaderparams
    {
        part *owner;
        Texture *tex, *decal, *masks, *normalmap;
        Shader *shader, *rsmshader;
        int cullface;
        shaderparamskey *key;

        skin() : owner(0), tex(notexture), decal(NULL), masks(notexture), normalmap(NULL), shader(NULL), rsmshader(NULL), cullface(1), key(NULL) {}

        bool masked() const;
        bool bumpmapped() const;
        bool alphatested() const;
        bool decaled() const;
        void setkey();
        void setshaderparams(mesh &m, const animstate *as, bool skinned = true);
        Shader *loadshader();
        void cleanup();
        void preloadBIH();
        void preloadshader();
        void setshader(mesh &m, const animstate *as);
        void bind(mesh &b, const animstate *as);
    };

    struct meshgroup;

    struct mesh
    {
        meshgroup *group;
        char *name;
        bool cancollide, canrender, noclip;

        mesh() : group(NULL), name(NULL), cancollide(true), canrender(true), noclip(false)
        {
        }

        virtual ~mesh()
        {
            DELETEA(name);
        }

        virtual void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) {}

        virtual void genBIH(BIH::mesh &m) {}

        void genBIH(skin &s, vector<BIH::mesh> &bih, const matrix4x3 &t);

        virtual void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m)
        {
        }

        virtual void setshader(Shader *s, int row = 0)
        {
            if(row)
            {
                s->setvariant(0, row);
            }
            else
            {
                s->set();
            }
        }

        struct smoothdata
        {
            vec norm;
            int next;

            smoothdata() : norm(0, 0, 0), next(-1) {}
        };

        template<class V, class T>
        void smoothnorms(V *verts, int numverts, T *tris, int numtris, float limit, bool areaweight)
        {
            if(!numverts)
            {
                return;
            }
            smoothdata *smooth = new smoothdata[numverts];
            hashtable<vec, int> share;
            for(int i = 0; i < numverts; ++i)
            {
                V &v = verts[i];
                int &idx = share.access(v.pos, i);
                if(idx != i)
                {
                    smooth[i].next = idx;
                    idx = i;
                }
            }
            for(int i = 0; i < numtris; ++i)
            {
                T &t = tris[i];
                int v1 = t.vert[0],
                    v2 = t.vert[1],
                    v3 = t.vert[2];
                vec norm;
                norm.cross(verts[v1].pos, verts[v2].pos, verts[v3].pos);
                if(!areaweight)
                {
                    norm.normalize();
                }
                smooth[v1].norm.add(norm);
                smooth[v2].norm.add(norm);
                smooth[v3].norm.add(norm);
            }
            for(int i = 0; i < numverts; ++i)
            {
                verts[i].norm = vec(0, 0, 0);
            }
            for(int i = 0; i < numverts; ++i)
            {
                const smoothdata &n = smooth[i];
                verts[i].norm.add(n.norm);
                if(n.next >= 0)
                {
                    float vlimit = limit*n.norm.magnitude();
                    for(int j = n.next; j >= 0;)
                    {
                        const smoothdata &o = smooth[j];
                        if(n.norm.dot(o.norm) >= vlimit*o.norm.magnitude())
                        {
                            verts[i].norm.add(o.norm);
                            verts[j].norm.add(n.norm);
                        }
                        j = o.next;
                    }
                }
            }
            for(int i = 0; i < numverts; ++i)
            {
                verts[i].norm.normalize();
            }
            delete[] smooth;
        }

        template<class V, class T>
        void buildnorms(V *verts, int numverts, T *tris, int numtris, bool areaweight)
        {
            if(!numverts)
            {
                return;
            }
            for(int i = 0; i < numverts; ++i)
            {
                verts[i].norm = vec(0, 0, 0);
            }
            for(int i = 0; i < numtris; ++i)
            {
                T &t = tris[i];
                V &v1 = verts[t.vert[0]],
                  &v2 = verts[t.vert[1]],
                  &v3 = verts[t.vert[2]];
                vec norm;
                norm.cross(v1.pos, v2.pos, v3.pos);
                if(!areaweight)
                {
                    norm.normalize();
                }
                v1.norm.add(norm);
                v2.norm.add(norm);
                v3.norm.add(norm);
            }
            for(int i = 0; i < numverts; ++i)
            {
                verts[i].norm.normalize();
            }
        }

        template<class V, class T>
        void buildnorms(V *verts, int numverts, T *tris, int numtris, bool areaweight, int numframes)
        {
            if(!numverts)
            {
                return;
            }
            for(int i = 0; i < numframes; ++i)
            {
                buildnorms(&verts[i*numverts], numverts, tris, numtris, areaweight);
            }
        }

        static inline void fixqtangent(quat &q, float bt);

        template<class V>
        static inline void calctangent(V &v, const vec &n, const vec &t, float bt)
        {
            matrix3 m;
            m.c = n;
            m.a = t;
            m.b.cross(m.c, m.a);
            quat q(m);
            fixqtangent(q, bt);
            v.tangent = q;
        }

        template<class V, class TC, class T>
        void calctangents(V *verts, TC *tcverts, int numverts, T *tris, int numtris, bool areaweight)
        {
            vec *tangent = new vec[2*numverts],
                *bitangent = tangent+numverts;
            memset((void *)tangent, 0, 2*numverts*sizeof(vec));

            for(int i = 0; i < numtris; ++i)
            {
                const T &t = tris[i];
                const vec &e0 = verts[t.vert[0]].pos;
                vec e1 = vec(verts[t.vert[1]].pos).sub(e0),
                    e2 = vec(verts[t.vert[2]].pos).sub(e0);

                const vec2 &tc0 = tcverts[t.vert[0]].tc,
                           &tc1 = tcverts[t.vert[1]].tc,
                           &tc2 = tcverts[t.vert[2]].tc;
                float u1 = tc1.x - tc0.x,
                      v1 = tc1.y - tc0.y,
                      u2 = tc2.x - tc0.x,
                      v2 = tc2.y - tc0.y;
                vec u(e2), v(e2);
                u.mul(v1).sub(vec(e1).mul(v2));
                v.mul(u1).sub(vec(e1).mul(u2));

                if(vec().cross(e2, e1).dot(vec().cross(v, u)) >= 0)
                {
                    u.neg();
                    v.neg();
                }

                if(!areaweight)
                {
                    u.normalize();
                    v.normalize();
                }

                for(int j = 0; j < 3; ++j)
                {
                    tangent[t.vert[j]].sub(u);
                    bitangent[t.vert[j]].add(v);
                }
            }
            for(int i = 0; i < numverts; ++i)
            {
                V &v = verts[i];
                const vec &t = tangent[i],
                          &bt = bitangent[i];
                matrix3 m;
                m.c = v.norm;
                (m.a = t).project(m.c).normalize();
                m.b.cross(m.c, m.a);
                quat q(m);
                fixqtangent(q, m.b.dot(bt));
                v.tangent = q;
            }
            delete[] tangent;
        }

        template<class V, class TC, class T>
        void calctangents(V *verts, TC *tcverts, int numverts, T *tris, int numtris, bool areaweight, int numframes)
        {
            for(int i = 0; i < numframes; ++i)
            {
                calctangents(&verts[i*numverts], tcverts, numverts, tris, numtris, areaweight);
            }
        }
    };

    struct meshgroup
    {
        meshgroup *next;
        int shared;
        char *name;
        vector<mesh *> meshes;

        meshgroup() : next(NULL), shared(0), name(NULL)
        {
        }

        virtual ~meshgroup()
        {
            DELETEA(name);
            meshes.deletecontents();
            DELETEP(next);
        }

        virtual int findtag(const char *name)
        {
            return -1;
        }

        virtual void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n) {}

        #define LOOP_RENDER_MESHES(type, name, body) do { \
            for(int i = 0; i < meshes.length(); i++) \
            { \
                type &name = *(type *)meshes[i]; \
                if(name.canrender || dbgcolmesh) \
                { \
                    body; \
                } \
            } \
        } while(0)

        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &t)
        {
            LOOP_RENDER_MESHES(mesh, m, m.calcbb(bbmin, bbmax, t));
        }

        void genBIH(vector<skin> &skins, vector<BIH::mesh> &bih, const matrix4x3 &t)
        {
            for(int i = 0; i < meshes.length(); i++)
            {
                meshes[i]->genBIH(skins[i], bih, t);
            }
        }

        void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &t)
        {
            LOOP_RENDER_MESHES(mesh, m, m.genshadowmesh(tris, t));
        }

        virtual void *animkey()
        {
            return this;
        }

        virtual int totalframes() const
        {
            return 1;
        }

        bool hasframe(int i) const
        {
            return i>=0 && i<totalframes();
        }

        bool hasframes(int i, int n) const
        {
            return i>=0 && i+n<=totalframes();
        }

        int clipframes(int i, int n) const
        {
            return min(n, totalframes() - i);
        }

        virtual void cleanup() {}
        virtual void preload(part *p) {}
        virtual void render(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p) {}
        virtual void intersect(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p, const vec &o, const vec &ray) {}

        void bindpos(GLuint ebuf, GLuint vbuf, void *v, int stride, int type, int size)
        {
            if(lastebuf!=ebuf)
            {
                gle::bindebo(ebuf);
                lastebuf = ebuf;
            }
            if(lastvbuf!=vbuf)
            {
                gle::bindvbo(vbuf);
                if(!lastvbuf)
                {
                    gle::enablevertex();
                }
                gle::vertexpointer(stride, v, type, size);
                lastvbuf = vbuf;
            }
        }
        void bindpos(GLuint ebuf, GLuint vbuf, vec *v, int stride)
        {
            bindpos(ebuf, vbuf, v, stride, GL_FLOAT, 3);
        }

        void bindpos(GLuint ebuf, GLuint vbuf, GenericVec4<half> *v, int stride)
        {
            bindpos(ebuf, vbuf, v, stride, GL_HALF_FLOAT, 4);
        }

        void bindtc(void *v, int stride)
        {
            if(!enabletc)
            {
                gle::enabletexcoord0();
                enabletc = true;
            }
            if(lasttcbuf!=lastvbuf)
            {
                gle::texcoord0pointer(stride, v, GL_HALF_FLOAT);
                lasttcbuf = lastvbuf;
            }
        }

        void bindtangents(void *v, int stride)
        {
            if(!enabletangents)
            {
                gle::enabletangent();
                enabletangents = true;
            }
            if(lastxbuf!=lastvbuf)
            {
                gle::tangentpointer(stride, v, GL_SHORT);
                lastxbuf = lastvbuf;
            }
        }

        void bindbones(void *wv, void *bv, int stride)
        {
            if(!enablebones)
            {
                gle::enableboneweight();
                gle::enableboneindex();
                enablebones = true;
            }
            if(lastbbuf!=lastvbuf)
            {
                gle::boneweightpointer(stride, wv);
                gle::boneindexpointer(stride, bv);
                lastbbuf = lastvbuf;
            }
        }
    };

    static hashnameset<meshgroup *> meshgroups;

    struct linkedpart
    {
        part *p;
        int tag, anim, basetime;
        vec translate;
        vec *pos;
        matrix4 matrix;

        linkedpart() : p(NULL), tag(-1), anim(-1), basetime(0), translate(0, 0, 0), pos(NULL) {}
    };

    struct part
    {
        animmodel *model;
        int index;
        meshgroup *meshes;
        vector<linkedpart> links;
        vector<skin> skins;
        vector<animspec> *anims[maxanimparts];
        int numanimparts;
        float pitchscale, pitchoffset, pitchmin, pitchmax;

        part(animmodel *model, int index = 0) : model(model), index(index), meshes(NULL), numanimparts(1), pitchscale(1), pitchoffset(0), pitchmin(0), pitchmax(0)
        {
            for(int k = 0; k < maxanimparts; ++k)
            {
                anims[k] = NULL;
            }
        }
        virtual ~part()
        {
            for(int k = 0; k < maxanimparts; ++k)
            {
                DELETEA(anims[k]);
            }
        }

        virtual void cleanup()
        {
            if(meshes)
            {
                meshes->cleanup();
            }
            for(int i = 0; i < skins.length(); i++)
            {
                skins[i].cleanup();
            }
        }

        void disablepitch()
        {
            pitchscale = pitchoffset = pitchmin = pitchmax = 0;
        }

        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m)
        {
            matrix4x3 t = m;
            t.scale(model->scale);
            meshes->calcbb(bbmin, bbmax, t);
            for(int i = 0; i < links.length(); i++)
            {
                matrix4x3 n;
                meshes->concattagtransform(this, links[i].tag, m, n);
                n.translate(links[i].translate, model->scale);
                links[i].p->calcbb(bbmin, bbmax, n);
            }
        }

        void genBIH(vector<BIH::mesh> &bih, const matrix4x3 &m)
        {
            matrix4x3 t = m;
            t.scale(model->scale);
            meshes->genBIH(skins, bih, t);
            for(int i = 0; i < links.length(); i++)
            {
                matrix4x3 n;
                meshes->concattagtransform(this, links[i].tag, m, n);
                n.translate(links[i].translate, model->scale);
                links[i].p->genBIH(bih, n);
            }
        }

        void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &m)
        {
            matrix4x3 t = m;
            t.scale(model->scale);
            meshes->genshadowmesh(tris, t);
            for(int i = 0; i < links.length(); i++)
            {
                matrix4x3 n;
                meshes->concattagtransform(this, links[i].tag, m, n);
                n.translate(links[i].translate, model->scale);
                links[i].p->genshadowmesh(tris, n);
            }
        }

        bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), int anim = -1, int basetime = 0, vec *pos = NULL)
        {
            int i = meshes ? meshes->findtag(tag) : -1;
            if(i<0)
            {
                for(int i = 0; i < links.length(); i++)
                {
                    if(links[i].p && links[i].p->link(p, tag, translate, anim, basetime, pos))
                    {
                        return true;
                    }
                }
                return false;
            }
            linkedpart &l = links.add();
            l.p = p;
            l.tag = i;
            l.anim = anim;
            l.basetime = basetime;
            l.translate = translate;
            l.pos = pos;
            return true;
        }

        bool unlink(part *p)
        {
            for(int i = links.length(); --i >=0;) //note reverse iteration
            {
                if(links[i].p==p)
                {
                    links.remove(i, 1);
                    return true;
                }
            }
            for(int i = 0; i < links.length(); i++)
            {
                if(links[i].p && links[i].p->unlink(p))
                {
                    return true;
                }
            }
            return false;
        }

        void initskins(Texture *tex = notexture, Texture *masks = notexture, int limit = 0)
        {
            if(!limit)
            {
                if(!meshes)
                {
                    return;
                }
                limit = meshes->meshes.length();
            }
            while(skins.length() < limit)
            {
                skin &s = skins.add();
                s.owner = this;
                s.tex = tex;
                s.masks = masks;
            }
        }

        bool alphatested() const
        {
            for(int i = 0; i < skins.length(); i++)
            {
                if(skins[i].alphatested())
                {
                    return true;
                }
            }
            return false;
        }

        void preloadBIH()
        {
            for(int i = 0; i < skins.length(); i++)
            {
                skins[i].preloadBIH();
            }
        }

        void preloadshaders()
        {
            for(int i = 0; i < skins.length(); i++)
            {
                skins[i].preloadshader();
            }
        }

        void preloadmeshes()
        {
            if(meshes)
            {
                meshes->preload(this);
            }
        }

        virtual void getdefaultanim(animinfo &info, int anim, uint varseed, dynent *d)
        {
            info.frame = 0;
            info.range = 1;
        }

        bool calcanim(int animpart, int anim, int basetime, int basetime2, dynent *d, int interp, animinfo &info, int &animinterptime)
        {
            uint varseed = static_cast<uint>((size_t)d);
            info.anim = anim;
            info.basetime = basetime;
            info.varseed = varseed;
            info.speed = anim & Anim_SetSpeed ? basetime2 : 100.0f;
            if((anim & Anim_Index) == Anim_All)
            {
                info.frame = 0;
                info.range = meshes->totalframes();
            }
            else
            {
                animspec *spec = NULL;
                if(anims[animpart])
                {
                    vector<animspec> &primary = anims[animpart][anim & Anim_Index];
                    if(primary.length())
                    {
                        spec = &primary[static_cast<uint>(varseed + basetime)%primary.length()];
                    }
                    if((anim >> Anim_Secondary) & (Anim_Index | Anim_Dir))
                    {
                        vector<animspec> &secondary = anims[animpart][(anim >> Anim_Secondary) & Anim_Index];
                        if(secondary.length())
                        {
                            animspec &spec2 = secondary[static_cast<uint>(varseed + basetime2)%secondary.length()];
                            if(!spec || spec2.priority > spec->priority)
                            {
                                spec = &spec2;
                                info.anim >>= Anim_Secondary;
                                info.basetime = basetime2;
                            }
                        }
                    }
                }
                if(spec)
                {
                    info.frame = spec->frame;
                    info.range = spec->range;
                    if(spec->speed>0)
                    {
                        info.speed = 1000.0f/spec->speed;
                    }
                }
                else
                {
                    getdefaultanim(info, anim, static_cast<uint>(varseed + info.basetime), d);
                }
            }

            info.anim &= (1 << Anim_Secondary) - 1;
            info.anim |= anim & Anim_Flags;
            if(info.anim & Anim_Loop)
            {
                info.anim &= ~Anim_SetTime;
                if(!info.basetime)
                {
                    info.basetime = -(static_cast<int>(reinterpret_cast<size_t>(d)) & 0xFFF);
                }
                if(info.anim & Anim_Clamp)
                {
                    if(info.anim & Anim_Reverse)
                    {
                        info.frame += info.range-1;
                    }
                    info.range = 1;
                }
            }

            if(!meshes->hasframes(info.frame, info.range))
            {
                if(!meshes->hasframe(info.frame))
                {
                    return false;
                }
                info.range = meshes->clipframes(info.frame, info.range);
            }

            if(d && interp>=0)
            {
                animinterpinfo &animationinterpolation = d->animinterp[interp];
                if((info.anim&(Anim_Loop | Anim_Clamp)) == Anim_Clamp)
                {
                    animinterptime = min(animinterptime, static_cast<int>(info.range*info.speed*0.5e-3f));
                }
                void *ak = meshes->animkey();
                if(d->ragdoll && d->ragdoll->millis != lastmillis)
                {
                    animationinterpolation.prev.range = animationinterpolation.cur.range = 0;
                    animationinterpolation.lastswitch = -1;
                }
                else if(animationinterpolation.lastmodel!=ak || animationinterpolation.lastswitch<0 || lastmillis-d->lastrendered>animinterptime)
                {
                    animationinterpolation.prev = animationinterpolation.cur = info;
                    animationinterpolation.lastswitch = lastmillis-animinterptime*2;
                }
                else if(animationinterpolation.cur!=info)
                {
                    if(lastmillis-animationinterpolation.lastswitch>animinterptime/2)
                    {
                        animationinterpolation.prev = animationinterpolation.cur;
                    }
                    animationinterpolation.cur = info;
                    animationinterpolation.lastswitch = lastmillis;
                }
                else if(info.anim & Anim_SetTime)
                {
                    animationinterpolation.cur.basetime = info.basetime;
                }
                animationinterpolation.lastmodel = ak;
            }
            return true;
        }

        void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray)
        {
            animstate as[maxanimparts];
            intersect(anim, basetime, basetime2, pitch, axis, forward, d, o, ray, as);
        }

        void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, const vec &o, const vec &ray, animstate *as)
        {
            if((anim & Anim_Reuse) != Anim_Reuse)
            {
                for(int i = 0; i < numanimparts; ++i)
                {
                    animinfo info;
                    int interp = d && index+numanimparts<=maxanimparts ? index+i : -1,
                        animinterptime = animationinterpolationtime;
                    if(!calcanim(i, anim, basetime, basetime2, d, interp, info, animinterptime))
                    {
                        return;
                    }
                    animstate &p = as[i];
                    p.owner = this;
                    p.cur.setframes(info);
                    p.interp = 1;
                    if(interp>=0 && d->animinterp[interp].prev.range>0)
                    {
                        int diff = lastmillis-d->animinterp[interp].lastswitch;
                        if(diff<animinterptime)
                        {
                            p.prev.setframes(d->animinterp[interp].prev);
                            p.interp = diff/static_cast<float>(animinterptime);
                        }
                    }
                }
            }

            float resize = model->scale * sizescale;
            int oldpos = matrixpos;
            vec oaxis, oforward, oo, oray;
            matrixstack[matrixpos].transposedtransformnormal(axis, oaxis);
            float pitchamount = pitchscale*pitch + pitchoffset;
            if(pitchmin || pitchmax)
            {
                pitchamount = std::clamp(pitchamount, pitchmin, pitchmax);
            }
            if(as->cur.anim & Anim_NoPitch || (as->interp < 1 && as->prev.anim & Anim_NoPitch))
            {
                pitchamount *= (as->cur.anim & Anim_NoPitch ? 0 : as->interp) + (as->interp < 1 && as->prev.anim & Anim_NoPitch ? 0 : 1 - as->interp);
            }
            if(pitchamount)
            {
                ++matrixpos;
                matrixstack[matrixpos] = matrixstack[matrixpos-1];
                matrixstack[matrixpos].rotate(pitchamount*RAD, oaxis);
            }
            if(this == model->parts[0] && !model->translate.iszero())
            {
                if(oldpos == matrixpos)
                {
                    ++matrixpos;
                    matrixstack[matrixpos] = matrixstack[matrixpos-1];
                }
                matrixstack[matrixpos].translate(model->translate, resize);
            }
            matrixstack[matrixpos].transposedtransformnormal(forward, oforward);
            matrixstack[matrixpos].transposedtransform(o, oo);
            oo.div(resize);
            matrixstack[matrixpos].transposedtransformnormal(ray, oray);

            intersectscale = resize;
            meshes->intersect(as, pitch, oaxis, oforward, d, this, oo, oray);

            if((anim & Anim_Reuse) != Anim_Reuse)
            {
                for(int i = 0; i < links.length(); i++)
                {
                    linkedpart &link = links[i];
                    if(!link.p)
                    {
                        continue;
                    }
                    link.matrix.translate(link.translate, resize);

                    matrixpos++;
                    matrixstack[matrixpos].mul(matrixstack[matrixpos-1], link.matrix);

                    int nanim = anim,
                        nbasetime  = basetime,
                        nbasetime2 = basetime2;
                    if(link.anim>=0)
                    {
                        nanim = link.anim | (anim & Anim_Flags);
                        nbasetime = link.basetime;
                        nbasetime2 = 0;
                    }
                    link.p->intersect(nanim, nbasetime, nbasetime2, pitch, axis, forward, d, o, ray);

                    matrixpos--;
                }
            }

            matrixpos = oldpos;
        }

        void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d)
        {
            animstate as[maxanimparts];
            render(anim, basetime, basetime2, pitch, axis, forward, d, as);
        }

        void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, animstate *as)
        {
            if((anim & Anim_Reuse) != Anim_Reuse)
            {
                for(int i = 0; i < numanimparts; ++i)
                {
                    animinfo info;
                    int interp = d && index+numanimparts<=maxanimparts ? index+i : -1, animinterptime = animationinterpolationtime;
                    if(!calcanim(i, anim, basetime, basetime2, d, interp, info, animinterptime))
                    {
                        return;
                    }
                    animstate &p = as[i];
                    p.owner = this;
                    p.cur.setframes(info);
                    p.interp = 1;
                    if(interp>=0 && d->animinterp[interp].prev.range>0)
                    {
                        int diff = lastmillis-d->animinterp[interp].lastswitch;
                        if(diff<animinterptime)
                        {
                            p.prev.setframes(d->animinterp[interp].prev);
                            p.interp = diff/static_cast<float>(animinterptime);
                        }
                    }
                }
            }

            float resize = model->scale * sizescale;
            int oldpos = matrixpos;
            vec oaxis, oforward;
            matrixstack[matrixpos].transposedtransformnormal(axis, oaxis);
            float pitchamount = pitchscale*pitch + pitchoffset;
            if(pitchmin || pitchmax)
            {
                pitchamount = std::clamp(pitchamount, pitchmin, pitchmax);
            }
            if(as->cur.anim & Anim_NoPitch || (as->interp < 1 && as->prev.anim & Anim_NoPitch))
            {
                pitchamount *= (as->cur.anim & Anim_NoPitch ? 0 : as->interp) + (as->interp < 1 && as->prev.anim & Anim_NoPitch ? 0 : 1 - as->interp);
            }
            if(pitchamount)
            {
                ++matrixpos;
                matrixstack[matrixpos] = matrixstack[matrixpos-1];
                matrixstack[matrixpos].rotate(pitchamount*RAD, oaxis);
            }
            if(this == model->parts[0] && !model->translate.iszero())
            {
                if(oldpos == matrixpos)
                {
                    ++matrixpos;
                    matrixstack[matrixpos] = matrixstack[matrixpos-1];
                }
                matrixstack[matrixpos].translate(model->translate, resize);
            }
            matrixstack[matrixpos].transposedtransformnormal(forward, oforward);

            if(!(anim & Anim_NoRender))
            {
                matrix4 modelmatrix;
                modelmatrix.mul(shadowmapping ? shadowmatrix : camprojmatrix, matrixstack[matrixpos]);
                if(resize!=1)
                {
                    modelmatrix.scale(resize);
                }
                GLOBALPARAM(modelmatrix, modelmatrix);
                if(!(anim & Anim_NoSkin))
                {
                    GLOBALPARAM(modelworld, matrix3(matrixstack[matrixpos]));

                    vec modelcamera;
                    matrixstack[matrixpos].transposedtransform(camera1->o, modelcamera);
                    modelcamera.div(resize);
                    GLOBALPARAM(modelcamera, modelcamera);
                }
            }

            meshes->render(as, pitch, oaxis, oforward, d, this);

            if((anim & Anim_Reuse) != Anim_Reuse)
            {
                for(int i = 0; i < links.length(); i++)
                {
                    linkedpart &link = links[i];
                    link.matrix.translate(link.translate, resize);
                    matrixpos++;
                    matrixstack[matrixpos].mul(matrixstack[matrixpos-1], link.matrix);
                    if(link.pos)
                    {
                        *link.pos = matrixstack[matrixpos].gettranslation();
                    }
                    if(!link.p)
                    {
                        matrixpos--;
                        continue;
                    }
                    int nanim = anim, nbasetime = basetime, nbasetime2 = basetime2;
                    if(link.anim>=0)
                    {
                        nanim = link.anim | (anim & Anim_Flags);
                        nbasetime = link.basetime;
                        nbasetime2 = 0;
                    }
                    link.p->render(nanim, nbasetime, nbasetime2, pitch, axis, forward, d);
                    matrixpos--;
                }
            }

            matrixpos = oldpos;
        }

        void setanim(int animpart, int num, int frame, int range, float speed, int priority = 0)
        {
            if(animpart<0 || animpart>=maxanimparts || num<0 || num >= numanims)
            {
                return;
            }
            if(frame<0 || range<=0 || !meshes || !meshes->hasframes(frame, range))
            {
                conoutf("invalid frame %d, range %d in model %s", frame, range, model->name);
                return;
            }
            if(!anims[animpart])
            {
                anims[animpart] = new vector<animspec>[numanims];
            }
            animspec &spec = anims[animpart][num].add();
            spec.frame = frame;
            spec.range = range;
            spec.speed = speed;
            spec.priority = priority;
        }

        bool animated() const
        {
            for(int i = 0; i < maxanimparts; ++i)
            {
                if(anims[i])
                {
                    return true;
                }
            }
            return false;
        }

        virtual void loaded()
        {
            meshes->shared++;
            for(int i = 0; i < skins.length(); i++)
            {
                skins[i].setkey();
            }
        }
    };

    enum
    {
        Link_Tag = 0,
        Link_Coop,
        Link_Reuse
    };

    virtual int linktype(animmodel *m, part *p) const
    {
        return Link_Tag;
    }

    void intersect(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a, const vec &o, const vec &ray)
    {
        int numtags = 0;
        if(a)
        {
            int index = parts.last()->index + parts.last()->numanimparts;
            for(int i = 0; a[i].tag; i++)
            {
                numtags++;
                animmodel *m = (animmodel *)a[i].m;
                if(!m)
                {
                    continue;
                }
                part *p = m->parts[0];
                switch(linktype(m, p))
                {
                    case Link_Tag:
                    {
                        p->index = link(p, a[i].tag, vec(0, 0, 0), a[i].anim, a[i].basetime, a[i].pos) ? index : -1;
                        break;
                    }
                    case Link_Coop:
                    {
                        p->index = index;
                        break;
                    }
                    default:
                    {
                        continue;
                    }
                }
                index += p->numanimparts;
            }
        }

        animstate as[maxanimparts];
        parts[0]->intersect(anim, basetime, basetime2, pitch, axis, forward, d, o, ray, as);

        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case Link_Coop:
                    p->intersect(anim, basetime, basetime2, pitch, axis, forward, d, o, ray);
                    break;

                case Link_Reuse:
                    p->intersect(anim | Anim_Reuse, basetime, basetime2, pitch, axis, forward, d, o, ray, as);
                    break;
            }
        }

        if(a) for(int i = numtags-1; i >= 0; i--)
        {
            animmodel *m = (animmodel *)a[i].m;
            if(!m)
            {
                continue;
            }

            part *p = m->parts[0];
            switch(linktype(m, p))
            {
                case Link_Tag:
                {
                    if(p->index >= 0)
                    {
                        unlink(p);
                    }
                    p->index = 0;
                    break;
                }
                case Link_Coop:
                {
                    p->intersect(anim, basetime, basetime2, pitch, axis, forward, d, o, ray);
                    p->index = 0;
                    break;
                }
                case Link_Reuse:
                {
                    p->intersect(anim | Anim_Reuse, basetime, basetime2, pitch, axis, forward, d, o, ray, as);
                    break;
                }
            }
        }
    }

    static int intersectresult, intersectmode;
    static float intersectdist, intersectscale;

    int intersect(int anim, int basetime, int basetime2, const vec &pos, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec &o, const vec &ray, float &dist, int mode)
    {
        vec axis(1, 0, 0), forward(0, 1, 0);

        matrixpos = 0;
        matrixstack[0].identity();
        if(!d || !d->ragdoll || d->ragdoll->millis == lastmillis)
        {
            float secs = lastmillis/1000.0f;
            yaw += spinyaw*secs;
            pitch += spinpitch*secs;
            roll += spinroll*secs;

            matrixstack[0].settranslation(pos);
            matrixstack[0].rotate_around_z(yaw*RAD);
            bool usepitch = pitched();
            if(roll && !usepitch)
            {
                matrixstack[0].rotate_around_y(-roll*RAD);
            }
            matrixstack[0].transformnormal(vec(axis), axis);
            matrixstack[0].transformnormal(vec(forward), forward);
            if(roll && usepitch)
            {
                matrixstack[0].rotate_around_y(-roll*RAD);
            }
            if(offsetyaw)
            {
                matrixstack[0].rotate_around_z(offsetyaw*RAD);
            }
            if(offsetpitch)
            {
                matrixstack[0].rotate_around_x(offsetpitch*RAD);
            }
            if(offsetroll)
            {
                matrixstack[0].rotate_around_y(-offsetroll*RAD);
            }
        }
        else
        {
            matrixstack[0].settranslation(d->ragdoll->center);
            pitch = 0;
        }
        sizescale = size;
        intersectresult = -1;
        intersectmode = mode;
        intersectdist = dist;
        intersect(anim, basetime, basetime2, pitch, axis, forward, d, a, o, ray);
        if(intersectresult >= 0)
        {
            dist = intersectdist;
        }
        return intersectresult;
    }

    void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a)
    {
        int numtags = 0;
        if(a)
        {
            int index = parts.last()->index + parts.last()->numanimparts;
            for(int i = 0; a[i].tag; i++)
            {
                numtags++;

                animmodel *m = (animmodel *)a[i].m;
                if(!m)
                {
                    if(a[i].pos) link(NULL, a[i].tag, vec(0, 0, 0), 0, 0, a[i].pos);
                    continue;
                }
                part *p = m->parts[0];
                switch(linktype(m, p))
                {
                    case Link_Tag:
                    {
                        p->index = link(p, a[i].tag, vec(0, 0, 0), a[i].anim, a[i].basetime, a[i].pos) ? index : -1;
                        break;
                    }
                    case Link_Coop:
                    {
                        p->index = index;
                        break;
                    }
                    default:
                    {
                        continue;
                    }
                }
                index += p->numanimparts;
            }
        }

        animstate as[maxanimparts];
        parts[0]->render(anim, basetime, basetime2, pitch, axis, forward, d, as);

        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case Link_Coop:
                {
                    p->render(anim, basetime, basetime2, pitch, axis, forward, d);
                    break;
                }
                case Link_Reuse:
                {
                    p->render(anim | Anim_Reuse, basetime, basetime2, pitch, axis, forward, d, as);
                    break;
                }
            }
        }

        if(a)
        {
            for(int i = numtags-1; i >= 0; i--)
            {
                animmodel *m = (animmodel *)a[i].m;
                if(!m)
                {
                    if(a[i].pos)
                    {
                        unlink(NULL);
                    }
                    continue;
                }
                part *p = m->parts[0];
                switch(linktype(m, p))
                {
                    case Link_Tag:
                    {
                        if(p->index >= 0)
                        {
                            unlink(p);
                        }
                        p->index = 0;
                        break;
                    }
                    case Link_Coop:
                    {
                        p->render(anim, basetime, basetime2, pitch, axis, forward, d);
                        p->index = 0;
                        break;
                    }
                    case Link_Reuse:
                    {
                        p->render(anim | Anim_Reuse, basetime, basetime2, pitch, axis, forward, d, as);
                        break;
                    }
                }
            }
        }
    }

    void render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a, float size, const vec4 &color)
    {
        vec axis(1, 0, 0), forward(0, 1, 0);

        matrixpos = 0;
        matrixstack[0].identity();
        if(!d || !d->ragdoll || d->ragdoll->millis == lastmillis)
        {
            float secs = lastmillis/1000.0f;
            yaw += spinyaw*secs;
            pitch += spinpitch*secs;
            roll += spinroll*secs;

            matrixstack[0].settranslation(o);
            matrixstack[0].rotate_around_z(yaw*RAD);
            bool usepitch = pitched();
            if(roll && !usepitch)
            {
                matrixstack[0].rotate_around_y(-roll*RAD);
            }
            matrixstack[0].transformnormal(vec(axis), axis);
            matrixstack[0].transformnormal(vec(forward), forward);
            if(roll && usepitch)
            {
                matrixstack[0].rotate_around_y(-roll*RAD);
            }
            if(offsetyaw)
            {
                matrixstack[0].rotate_around_z(offsetyaw*RAD);
            }
            if(offsetpitch)
            {
                matrixstack[0].rotate_around_x(offsetpitch*RAD);
            }
            if(offsetroll)
            {
                matrixstack[0].rotate_around_y(-offsetroll*RAD);
            }
        }
        else
        {
            matrixstack[0].settranslation(d->ragdoll->center);
            pitch = 0;
        }

        sizescale = size;

        if(anim & Anim_NoRender)
        {
            render(anim, basetime, basetime2, pitch, axis, forward, d, a);
            if(d)
            {
                d->lastrendered = lastmillis;
            }
            return;
        }

        if(!(anim & Anim_NoSkin))
        {
            if(colorscale != color)
            {
                colorscale = color;
                shaderparamskey::invalidate();
            }
        }

        if(depthoffset && !enabledepthoffset)
        {
            enablepolygonoffset(GL_POLYGON_OFFSET_FILL);
            enabledepthoffset = true;
        }

        render(anim, basetime, basetime2, pitch, axis, forward, d, a);

        if(d)
        {
            d->lastrendered = lastmillis;
        }
    }

    vector<part *> parts;

    animmodel(const char *name) : model(name)
    {
    }

    ~animmodel()
    {
        parts.deletecontents();
    }

    void cleanup()
    {
        for(int i = 0; i < parts.length(); i++)
        {
            parts[i]->cleanup();
        }
    }

    virtual void flushpart() {}

    part &addpart()
    {
        flushpart();
        part *p = new part(this, parts.length());
        parts.add(p);
        return *p;
    }

    void initmatrix(matrix4x3 &m)
    {
        m.identity();
        if(offsetyaw)
        {
            m.rotate_around_z(offsetyaw*RAD);
        }
        if(offsetpitch)
        {
            m.rotate_around_x(offsetpitch*RAD);
        }
        if(offsetroll)
        {
            m.rotate_around_y(-offsetroll*RAD);
        }
        m.translate(translate, scale);
    }

    void genBIH(vector<BIH::mesh> &bih)
    {
        if(parts.empty())
        {
            return;
        }
        matrix4x3 m;
        initmatrix(m);
        parts[0]->genBIH(bih, m);
        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case Link_Coop:
                case Link_Reuse:
                {
                    p->genBIH(bih, m);
                    break;
                }
            }
        }
    }

    void genshadowmesh(std::vector<triangle> &tris, const matrix4x3 &orient)
    {
        if(parts.empty())
        {
            return;
        }
        matrix4x3 m;
        initmatrix(m);
        m.mul(orient, matrix4x3(m));
        parts[0]->genshadowmesh(tris, m);
        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case Link_Coop:
                case Link_Reuse:
                {
                    p->genshadowmesh(tris, m);
                    break;
                }
            }
        }
    }

    void preloadBIH()
    {
        model::preloadBIH();
        if(bih)
        {
            for(int i = 0; i < parts.length(); i++)
            {
                parts[i]->preloadBIH();
            }
        }
    }

    BIH *setBIH()
    {
        if(bih)
        {
            return bih;
        }
        vector<BIH::mesh> meshes;
        genBIH(meshes);
        bih = new BIH(meshes);
        return bih;
    }

    bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), int anim = -1, int basetime = 0, vec *pos = NULL)
    {
        if(parts.empty())
        {
            return false;
        }
        return parts[0]->link(p, tag, translate, anim, basetime, pos);
    }

    bool unlink(part *p)
    {
        if(parts.empty())
        {
            return false;
        }
        return parts[0]->unlink(p);
    }

    bool animated() const
    {
        if(spinyaw || spinpitch || spinroll)
        {
            return true;
        }
        for(int i = 0; i < parts.length(); i++)
        {
            if(parts[i]->animated())
            {
                return true;
            }
        }
        return false;
    }

    bool pitched() const
    {
        return parts[0]->pitchscale != 0;
    }

    bool alphatested() const
    {
        for(int i = 0; i < parts.length(); i++)
        {
            if(parts[i]->alphatested())
            {
                return true;
            }
        }
        return false;
    }

    virtual bool flipy() const
    {
        return false;
    }

    virtual bool loadconfig()
    {
        return false;
    }

    virtual bool loaddefaultparts()
    {
        return false;
    }

    virtual void startload()
    {
    }

    virtual void endload()
    {
    }

    bool load()
    {
        startload();
        bool success = loadconfig() && parts.length(); // configured model, will call the model commands below
        if(!success)
        {
            success = loaddefaultparts(); // model without configuration, try default tris and skin
        }
        flushpart();
        endload();
        if(flipy())
        {
            translate.y = -translate.y;
        }

        if(!success)
        {
            return false;
        }
        for(int i = 0; i < parts.length(); i++)
        {
            if(!parts[i]->meshes)
            {
                return false;
            }
        }
        loaded();
        return true;
    }

    void preloadshaders()
    {
        for(int i = 0; i < parts.length(); i++)
        {
            parts[i]->preloadshaders();
        }
    }

    void preloadmeshes()
    {
        for(int i = 0; i < parts.length(); i++)
        {
            parts[i]->preloadmeshes();
        }
    }

    void setshader(Shader *shader)
    {
        if(parts.empty())
        {
            loaddefaultparts();
        }
        for(int i = 0; i < parts.length(); i++)
        {
            for(int j = 0; j < parts[i]->skins.length(); j++)
            {
                parts[i]->skins[j].shader = shader;
            }
        }
    }

    void setspec(float spec)
    {
        if(parts.empty())
        {
            loaddefaultparts();
        }
        for(int i = 0; i < parts.length(); i++)
        {
            for(int j = 0; j < parts[i]->skins.length(); j++)
            {
                parts[i]->skins[j].spec = spec;
            }
        }
    }

    void setgloss(int gloss)
    {
        if(parts.empty())
        {
            loaddefaultparts();
        }
        for(int i = 0; i < parts.length(); i++)
        {
            for(int j = 0; j < parts[i]->skins.length(); j++)
            {
                parts[i]->skins[j].gloss = gloss;
            }
        }
    }

    void setglow(float glow, float delta, float pulse)
    {
        if(parts.empty())
        {
            loaddefaultparts();
        }
        for(int i = 0; i < parts.length(); i++)
        {
            for(int j = 0; j < parts[i]->skins.length(); j++)
            {
                skin &s = parts[i]->skins[j];
                s.glow = glow;
                s.glowdelta = delta;
                s.glowpulse = pulse;
            }
        }
    }

    void setalphatest(float alphatest)
    {
        if(parts.empty())
        {
            loaddefaultparts();
        }
        for(int i = 0; i < parts.length(); i++)
        {
            for(int j = 0; j < parts[i]->skins.length(); j++)
            {
                parts[i]->skins[j].alphatest = alphatest;
            }
        }
    }

    void setfullbright(float fullbright)
    {
        if(parts.empty())
        {
            loaddefaultparts();
        }
        for(int i = 0; i < parts.length(); i++)
        {
            for(int j = 0; j < parts[i]->skins.length(); j++)
            {
                parts[i]->skins[j].fullbright = fullbright;
            }
        }
    }

    void setcullface(int cullface)
    {
        if(parts.empty())
        {
            loaddefaultparts();
        }
        for(int i = 0; i < parts.length(); i++)
        {
            for(int j = 0; j < parts[i]->skins.length(); j++)
            {
                parts[i]->skins[j].cullface = cullface;
            }
        }
    }

    void setcolor(const vec &color)
    {
        if(parts.empty())
        {
            loaddefaultparts();
        }
        for(int i = 0; i < parts.length(); i++)
        {
            for(int j = 0; j < parts[i]->skins.length(); j++)
            {
                parts[i]->skins[j].color = color;
            }
        }
    }

    void calcbb(vec &center, vec &radius)
    {
        if(parts.empty())
        {
            return;
        }
        vec bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f);
        matrix4x3 m;
        initmatrix(m);
        parts[0]->calcbb(bbmin, bbmax, m);
        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case Link_Coop:
                case Link_Reuse:
                {
                    p->calcbb(bbmin, bbmax, m);
                    break;
                }
            }
        }
        radius = bbmax;
        radius.sub(bbmin);
        radius.mul(0.5f);
        center = bbmin;
        center.add(radius);
    }

    void calctransform(matrix4x3 &m)
    {
        initmatrix(m);
        m.scale(scale);
    }

    virtual void loaded()
    {
        for(int i = 0; i < parts.length(); i++)
        {
            parts[i]->loaded();
        }
    }

    static bool enabletc, enablecullface, enabletangents, enablebones, enabledepthoffset;
    static float sizescale;
    static vec4 colorscale;
    static GLuint lastvbuf, lasttcbuf, lastxbuf, lastbbuf, lastebuf;
    static Texture *lasttex, *lastdecal, *lastmasks, *lastnormalmap;
    static int matrixpos;
    static matrix4 matrixstack[64];

    void startrender()
    {
        enabletc = enabletangents = enablebones = enabledepthoffset = false;
        enablecullface = true;
        lastvbuf = lasttcbuf = lastxbuf = lastbbuf = lastebuf =0;
        lasttex = lastdecal = lastmasks = lastnormalmap = NULL;
        shaderparamskey::invalidate();
    }

    static void disablebones()
    {
        gle::disableboneweight();
        gle::disableboneindex();
        enablebones = false;
    }

    static void disabletangents()
    {
        gle::disabletangent();
        enabletangents = false;
    }

    static void disabletc()
    {
        gle::disabletexcoord0();
        enabletc = false;
    }

    static void disablevbo()
    {
        if(lastebuf)
        {
            gle::clearebo();
        }
        if(lastvbuf)
        {
            gle::clearvbo();
            gle::disablevertex();
        }
        if(enabletc)
        {
            disabletc();
        }
        if(enabletangents) disabletangents();
        if(enablebones) disablebones();
        lastvbuf = lasttcbuf = lastxbuf = lastbbuf = lastebuf = 0;
    }

    void endrender()
    {
        if(lastvbuf || lastebuf) disablevbo();
        if(!enablecullface) glEnable(GL_CULL_FACE);
        if(enabledepthoffset) disablepolygonoffset(GL_POLYGON_OFFSET_FILL);
    }
};

static inline uint hthash(const animmodel::shaderparams &k)
{
    return memhash(&k, sizeof(k));
}

static inline bool htcmp(const animmodel::shaderparams &x, const animmodel::shaderparams &y)
{
    return !memcmp(&x, &y, sizeof(animmodel::shaderparams));
}

template<class MDL, class BASE>
struct modelloader : BASE
{
    static MDL *loading;
    static string dir;

    modelloader(const char *name) : BASE(name) {}

    static bool cananimate()
    {
        return true;
    }
    static bool multiparted()
    {
        return true;
    }
    static bool multimeshed()
    {
        return true;
    }

    void startload()
    {
        loading = (MDL *)this;
    }

    void endload()
    {
        loading = NULL;
    }

    bool loadconfig()
    {
        formatstring(dir, "media/model/%s", BASE::name);
        DEF_FORMAT_STRING(cfgname, "media/model/%s/%s.cfg", BASE::name, MDL::formatname());

        identflags &= ~Idf_Persist;
        bool success = execfile(cfgname, false);
        identflags |= Idf_Persist;
        return success;
    }
};

template<class MDL, class BASE>
MDL *modelloader<MDL, BASE>::loading = NULL;

template<class MDL, class BASE>
string modelloader<MDL, BASE>::dir = {'\0'}; // crashes clang if "" is used here

template<class MDL, class MESH>
struct modelcommands
{
    typedef struct MDL::part part;
    typedef struct MDL::skin skin;

    static void setdir(char *name)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        formatstring(MDL::dir, "media/model/%s", name);
    }

    #define LOOP_MESHES(meshname, m, body) do { \
        if(!MDL::loading || MDL::loading->parts.empty()) \
        { \
            conoutf("not loading an %s", MDL::formatname()); \
            return; \
        } \
        part &mdl = *MDL::loading->parts.last(); \
        if(!mdl.meshes) \
        { \
            return; \
        } \
        for(int i = 0; i < mdl.meshes->meshes.length(); i++) \
        { \
            MESH &m = *(MESH *)mdl.meshes->meshes[i]; \
            if(!strcmp(meshname, "*") || (m.name && !strcmp(m.name, meshname))) \
            { \
                body; \
            } \
        } \
    } while(0)

    #define LOOP_SKINS(meshname, s, body) LOOP_MESHES(meshname, m, { skin &s = mdl.skins[i]; body; })

    static void setskin(char *meshname, char *tex, char *masks)
    {
        LOOP_SKINS(meshname, s,
            s.tex = textureload(makerelpath(MDL::dir, tex), 0, true, false);
            if(*masks)
            {
                s.masks = textureload(makerelpath(MDL::dir, masks), 0, true, false);
            }
        );
    }

    static void setspec(char *meshname, float *percent)
    {
        float spec = *percent > 0 ? *percent/100.0f : 0.0f;
        LOOP_SKINS(meshname, s, s.spec = spec);
    }

    static void setgloss(char *meshname, int *gloss)
    {
        LOOP_SKINS(meshname, s, s.gloss = std::clamp(*gloss, 0, 2));
    }

    static void setglow(char *meshname, float *percent, float *delta, float *pulse)
    {
        float glow = *percent > 0 ? *percent/100.0f : 0.0f,
              glowdelta = *delta/100.0f,
              glowpulse = *pulse > 0 ? *pulse/1000.0f : 0;
        glowdelta -= glow;
        LOOP_SKINS(meshname, s, { s.glow = glow; s.glowdelta = glowdelta; s.glowpulse = glowpulse; });
    }

    static void setalphatest(char *meshname, float *cutoff)
    {
        LOOP_SKINS(meshname, s, s.alphatest = max(0.0f, min(1.0f, *cutoff)));
    }

    static void setcullface(char *meshname, int *cullface)
    {
        LOOP_SKINS(meshname, s, s.cullface = *cullface);
    }

    static void setcolor(char *meshname, float *r, float *g, float *b)
    {
        LOOP_SKINS(meshname, s, s.color = vec(*r, *g, *b));
    }

    static void setbumpmap(char *meshname, char *normalmapfile)
    {
        Texture *normalmaptex = textureload(makerelpath(MDL::dir, normalmapfile), 0, true, false);
        LOOP_SKINS(meshname, s, s.normalmap = normalmaptex);
    }

    static void setdecal(char *meshname, char *decal)
    {
        LOOP_SKINS(meshname, s,
            s.decal = textureload(makerelpath(MDL::dir, decal), 0, true, false);
        );
    }

    static void setfullbright(char *meshname, float *fullbright)
    {
        LOOP_SKINS(meshname, s, s.fullbright = *fullbright);
    }

    static void setshader(char *meshname, char *shader)
    {
        LOOP_SKINS(meshname, s, s.shader = lookupshaderbyname(shader));
    }

    static void setscroll(char *meshname, float *scrollu, float *scrollv)
    {
        LOOP_SKINS(meshname, s, { s.scrollu = *scrollu; s.scrollv = *scrollv; });
    }

    static void setnoclip(char *meshname, int *noclip)
    {
        LOOP_MESHES(meshname, m, m.noclip = *noclip!=0);
    }

    static void settricollide(char *meshname)
    {
        bool init = true;
        LOOP_MESHES("*", m,
        {
            if(!m.cancollide)
            {
                init = false;
            }
        });
        if(init)
        {
            LOOP_MESHES("*", m, m.cancollide = false);
        }
        LOOP_MESHES(meshname, m,
        {
            m.cancollide = true;
            m.canrender = false;
        });
        MDL::loading->collide = Collide_TRI;
    }

#undef LOOP_MESHES
#undef LOOP_SKINS

    static void setlink(int *parent, int *child, char *tagname, float *x, float *y, float *z)
    {
        if(!MDL::loading)
        {
            conoutf("not loading an %s", MDL::formatname());
            return;
        }
        if(!MDL::loading->parts.inrange(*parent) || !MDL::loading->parts.inrange(*child))
        {
            conoutf("no models loaded to link");
            return;
        }
        if(!MDL::loading->parts[*parent]->link(MDL::loading->parts[*child], tagname, vec(*x, *y, *z)))
        {
            conoutf("could not link model %s", MDL::loading->name);
        }
    }

    template<class F>
    void modelcommand(F *fun, const char *suffix, const char *args)
    {
        DEF_FORMAT_STRING(name, "%s%s", MDL::formatname(), suffix);
        addcommand(newstring(name), (identfun)fun, args);
    }

    modelcommands()
    {
        modelcommand(setdir, "dir", "s");//<fmt>dir [name]
        if(MDL::multimeshed())
        {
            modelcommand(setskin, "skin", "sss"); //<fmt>skin [meshname] [tex] [masks]
            modelcommand(setspec, "spec", "sf"); //<fmt>spec [tex] [scale]
            modelcommand(setgloss, "gloss", "si");//<fmt>gloss [tex] [type] type ranges 0..2
            modelcommand(setglow, "glow", "sfff"); //<fmt>glow [tex] [pct] [del] [pulse]
            modelcommand(setalphatest, "alphatest", "sf");//<fmt>alphatest [mesh] [cutoff]
            modelcommand(setcullface, "cullface", "si");//<fmt>cullface [mesh] [cullface]
            modelcommand(setcolor, "color", "sfff");//<fmt>color [mesh] [r] [g] [b]
            modelcommand(setbumpmap, "bumpmap", "ss");//<fmt>bumpmap [mesh] [tex]
            modelcommand(setdecal, "decal", "ss");//<fmt>decal [mesh] [tex]
            modelcommand(setfullbright, "fullbright", "sf"); //<fmt>fullbright [mesh] [bright]
            modelcommand(setshader, "shader", "ss");//<fmt>shader [mesh] [shader]
            modelcommand(setscroll, "scroll", "sff");//<fmt>scroll [mesh] [x] [y]
            modelcommand(setnoclip, "noclip", "si");//<fmt>noclip [mesh] [bool]
            modelcommand(settricollide, "tricollide", "s"); //<fmt>settricollide [mesh]
        }
        if(MDL::multiparted())
        {
            modelcommand(setlink, "link", "iisfff");//<mdl>link [parent] [child] [tag] [x] [y] [z]
        }
    }
};

#endif

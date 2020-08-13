// worldio.cpp: loading & saving of maps and savegames

#include "engine.h"
#include "raycube.h"
#include "render/renderwindow.h"

void validmapname(char *dst, const char *src, const char *prefix = NULL, const char *alt = "untitled", size_t maxlen = 100)
{
    if(prefix)
    {
        while(*prefix)
        {
            *dst++ = *prefix++;
        }
    }
    const char *start = dst;
    if(src)
    {
        for(int i = 0; i < static_cast<int>(maxlen); ++i)
        {
            char c = *src++;
            if(iscubealnum(c) || c == '_' || c == '-' || c == '/' || c == '\\')
            {
                *dst++ = c;
            }
            else
            {
                break;
            }
        }
    }
    if(dst > start)
    {
        *dst = '\0';
    }
    else if(dst != alt)
    {
        copystring(dst, alt, maxlen);
    }
}

void fixmapname(char *name)
{
    validmapname(name, name, NULL, "");
}

static void fixent(entity &e, int version)
{
    if(version <= 0)
    {
        if(e.type >= EngineEnt_Decal)
        {
            e.type++;
        }
    }
}

static bool loadmapheader(stream *f, const char *ogzname, mapheader &hdr, octaheader &ohdr)
{
    if(f->read(&hdr, 3*sizeof(int)) != 3*sizeof(int))
    {
        conoutf(Console_Error, "map %s has malformatted header", ogzname);
        return false;
    }
    if(!memcmp(hdr.magic, "TMAP", 4))
    {
        if(hdr.version>MAPVERSION)
        {
            conoutf(Console_Error, "map %s requires a newer version of Tesseract", ogzname);
            return false;
        }
        if(f->read(&hdr.worldsize, 6*sizeof(int)) != 6*sizeof(int))
        {
            conoutf(Console_Error, "map %s has malformatted header", ogzname);
            return false;
        }
        if(hdr.worldsize <= 0|| hdr.numents < 0)
        {
            conoutf(Console_Error, "map %s has malformatted header", ogzname);
            return false;
        }
    }
    else if(!memcmp(hdr.magic, "OCTA", 4))
    {
        if(hdr.version!=OCTAVERSION)
        {
            conoutf(Console_Error, "map %s uses an unsupported map format version", ogzname);
            return false;
        }
        if(f->read(&ohdr.worldsize, 7*sizeof(int)) != 7*sizeof(int))
        {
            conoutf(Console_Error, "map %s has malformatted header", ogzname);
            return false;
        }
        if(ohdr.worldsize <= 0|| ohdr.numents < 0)
        {
            conoutf(Console_Error, "map %s has malformatted header", ogzname);
            return false;
        }
        memcpy(hdr.magic, "TMAP", 4);
        hdr.version = 0;
        hdr.headersize = sizeof(hdr);
        hdr.worldsize = ohdr.worldsize;
        hdr.numents = ohdr.numents;
        hdr.numvars = ohdr.numvars;
        hdr.numvslots = ohdr.numvslots;
    }
    else
    {
        conoutf(Console_Error, "map %s uses an unsupported map type", ogzname);
        return false;
    }

    return true;
}

bool loadents(const char *fname, const char *gameident, vector<entity> &ents, uint *crc)
{
    string name;
    validmapname(name, fname);
    DEF_FORMAT_STRING(ogzname, "media/map/%s.ogz", name);
    path(ogzname);
    stream *f = opengzfile(ogzname, "rb");
    if(!f)
    {
        return false;
    }
    mapheader hdr;
    octaheader ohdr;
    if(!loadmapheader(f, ogzname, hdr, ohdr))
    {
        delete f;
        return false;
    }
    for(int i = 0; i < hdr.numvars; ++i)
    {
        int type = f->getchar(),
            ilen = f->get<ushort>();
        f->seek(ilen, SEEK_CUR);
        switch(type)
        {
            case Id_Var:
            {
                f->get<int>();
                break;
            }
            case Id_FloatVar:
            {
                f->get<float>();
                break;
            }
            case Id_StringVar:
            {
                int slen = f->get<ushort>();
                f->seek(slen, SEEK_CUR);
                break;
            }
        }
    }
    string gametype;
    bool samegame = true;
    int len = f->getchar();
    if(len >= 0)
    {
        f->read(gametype, len+1);
    }
    gametype[max(len, 0)] = '\0';
    if(strcmp(gametype, gameident)) //compare game string from map with game
    {
        samegame = false;
        conoutf(Console_Warn, "WARNING: loading map from %s game, ignoring entities except for lights/mapmodels", gametype);
    }
    int eif = f->get<ushort>(),
        extrasize = f->get<ushort>();
    f->seek(extrasize, SEEK_CUR);

    ushort nummru = f->get<ushort>();
    f->seek(nummru*sizeof(ushort), SEEK_CUR);

    for(int i = 0; i < min(hdr.numents, maxents); ++i)
    {
        entity &e = ents.add();
        f->read(&e, sizeof(entity));
        fixent(e, hdr.version);
        if(eif > 0)
        {
            f->seek(eif, SEEK_CUR);
        }
        if(samegame)
        {
        }
        else if(e.type>=EngineEnt_GameSpecific)
        {
            ents.pop();
            continue;
        }
    }

    if(crc)
    {
        f->seek(0, SEEK_END);
        *crc = f->getcrc();
    }
    delete f;
    return true;
}

string ogzname, bakname, cfgname, picname;

VARP(savebak, 0, 2, 2);

void setmapfilenames(const char *fname, const char *cname = NULL)
{
    string name;
    validmapname(name, fname);
    formatstring(ogzname, "media/map/%s.ogz", name);
    formatstring(picname, "media/map/%s.png", name);
    if(savebak==1)
    {
        formatstring(bakname, "media/map/%s.BAK", name);
    }
    else
    {
        string baktime;
        time_t t = time(NULL);
        size_t len = strftime(baktime, sizeof(baktime), "%Y-%m-%d_%H.%M.%S", localtime(&t));
        baktime[min(len, sizeof(baktime)-1)] = '\0';
        formatstring(bakname, "media/map/%s_%s.BAK", name, baktime);
    }
    validmapname(name, cname ? cname : fname);
    formatstring(cfgname, "media/map/%s.cfg", name);
    path(ogzname);
    path(bakname);
    path(cfgname);
    path(picname);
}

void mapcfgname()
{
    const char *mname = game::getclientmap();
    string name;
    validmapname(name, mname);
    DEF_FORMAT_STRING(cfgname, "media/map/%s.cfg", name);
    path(cfgname);
    result(cfgname);
}

COMMAND(mapcfgname, "");

void backup(const char *name, const char *backupname)
{
    string backupfile;
    copystring(backupfile, findfile(backupname, "wb"));
    remove(backupfile);
    rename(findfile(name, "wb"), backupfile);
}

enum
{
    OctaSave_Children = 0,
    OctaSave_Empty,
    OctaSave_Solid,
    OctaSave_Normal
};

#define LAYER_DUP (1<<7)

struct polysurfacecompat
{
    uchar lmid[2];
    uchar verts, numverts;
};

static int savemapprogress = 0;

void savec(cube *c, const ivec &o, int size, stream *f)
{
    if((savemapprogress++&0xFFF)==0)
    {
        renderprogress(static_cast<float>(savemapprogress)/allocnodes, "saving octree...");
    }
    for(int i = 0; i < 8; ++i)
    {
        ivec co(i, o, size);
        if(c[i].children)
        {
            f->putchar(OctaSave_Children);
            savec(c[i].children, co, size>>1, f);
        }
        else
        {
            int oflags     = 0,
                surfmask   = 0,
                totalverts = 0;
            if(c[i].material!=Mat_Air)
            {
                oflags |= 0x40;
            }
            if(IS_EMPTY(c[i]))
            {
                f->putchar(oflags | OctaSave_Empty);
            }
            else
            {
                if(c[i].merged)
                {
                    oflags |= 0x80;
                }
                if(c[i].ext)
                {
                    for(int j = 0; j < 6; ++j)
                    {
                        {
                            const surfaceinfo &surf = c[i].ext->surfaces[j];
                            if(!surf.used())
                            {
                                continue;
                            }
                            oflags |= 0x20;
                            surfmask |= 1<<j;
                            totalverts += surf.totalverts();
                        }
                    }
                }
                if(IS_ENTIRELY_SOLID(c[i]))
                {
                    f->putchar(oflags | OctaSave_Solid);
                }
                else
                {
                    f->putchar(oflags | OctaSave_Normal);
                    f->write(c[i].edges, 12);
                }
            }

            for(int j = 0; j < 6; ++j)
            {
                f->put<ushort>(c[i].texture[j]);
            }

            if(oflags&0x40)
            {
                f->put<ushort>(c[i].material);
            }
            if(oflags&0x80)
            {
                f->putchar(c[i].merged);
            }
            if(oflags&0x20)
            {
                f->putchar(surfmask);
                f->putchar(totalverts);
                for(int j = 0; j < 6; ++j)
                {
                    if(surfmask&(1<<j))
                    {
                        surfaceinfo surf = c[i].ext->surfaces[j];
                        vertinfo *verts = c[i].ext->verts() + surf.verts;
                        int layerverts = surf.numverts&Face_MaxVerts, numverts = surf.totalverts(),
                            vertmask   = 0,
                            vertorder  = 0,
                            dim = DIMENSION(j),
                            vc  = C[dim],
                            vr  = R[dim];
                        if(numverts)
                        {
                            if(c[i].merged&(1<<j))
                            {
                                vertmask |= 0x04;
                                if(layerverts == 4)
                                {
                                    ivec v[4] = { verts[0].getxyz(), verts[1].getxyz(), verts[2].getxyz(), verts[3].getxyz() };
                                    for(int k = 0; k < 4; ++k)
                                    {
                                        const ivec &v0 = v[k], &v1 = v[(k+1)&3], &v2 = v[(k+2)&3], &v3 = v[(k+3)&3];
                                        if(v1[vc] == v0[vc] && v1[vr] == v2[vr] && v3[vc] == v2[vc] && v3[vr] == v0[vr])
                                        {
                                            vertmask |= 0x01;
                                            vertorder = k;
                                            break;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                int vis = visibletris(c[i], j, co, size);
                                if(vis&4 || faceconvexity(c[i], j) < 0)
                                {
                                    vertmask |= 0x01;
                                }
                                if(layerverts < 4 && vis&2)
                                {
                                    vertmask |= 0x02;
                                }
                            }
                            bool matchnorm = true;
                            for(int k = 0; k < numverts; ++k)
                            {
                                const vertinfo &v = verts[k];
                                if(v.norm)
                                {
                                    vertmask |= 0x80;
                                    if(v.norm != verts[0].norm)
                                    {
                                        matchnorm = false;
                                    }
                                }
                            }
                            if(matchnorm)
                            {
                                vertmask |= 0x08;
                            }
                        }
                        surf.verts = vertmask;
                        f->write(&surf, sizeof(surf));
                        bool hasxyz = (vertmask&0x04)!=0, hasnorm = (vertmask&0x80)!=0;
                        if(layerverts == 4)
                        {
                            if(hasxyz && vertmask&0x01)
                            {
                                ivec v0 = verts[vertorder].getxyz(), v2 = verts[(vertorder+2)&3].getxyz();
                                f->put<ushort>(v0[vc]); f->put<ushort>(v0[vr]);
                                f->put<ushort>(v2[vc]); f->put<ushort>(v2[vr]);
                                hasxyz = false;
                            }
                        }
                        if(hasnorm && vertmask&0x08)
                        {
                            f->put<ushort>(verts[0].norm);
                            hasnorm = false;
                        }
                        if(hasxyz || hasnorm)
                        {
                            for(int k = 0; k < layerverts; ++k)
                            {
                                const vertinfo &v = verts[(k+vertorder)%layerverts];
                                if(hasxyz)
                                {
                                    ivec xyz = v.getxyz();
                                    f->put<ushort>(xyz[vc]); f->put<ushort>(xyz[vr]);
                                }
                                if(hasnorm)
                                {
                                    f->put<ushort>(v.norm);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

cube *loadchildren(stream *f, const ivec &co, int size, bool &failed);

void loadc(stream *f, cube &c, const ivec &co, int size, bool &failed)
{
    int octsav = f->getchar();
    switch(octsav&0x7)
    {
        case OctaSave_Children:
            c.children = loadchildren(f, co, size>>1, failed);
            return;

        case OctaSave_Empty:  EMPTY_FACES(c);        break;
        case OctaSave_Solid:  SOLID_FACES(c);        break;
        case OctaSave_Normal: f->read(c.edges, 12);  break;
        default: failed = true; return;
    }
    for(int i = 0; i < 6; ++i)
    {
        c.texture[i] = f->get<ushort>();
    }
    if(octsav&0x40) c.material = f->get<ushort>();
    if(octsav&0x80) c.merged = f->getchar();
    if(octsav&0x20)
    {
        int surfmask, totalverts;
        surfmask = f->getchar();
        totalverts = max(f->getchar(), 0);
        newcubeext(c, totalverts, false);
        memset(c.ext->surfaces, 0, sizeof(c.ext->surfaces));
        memset(c.ext->verts(), 0, totalverts*sizeof(vertinfo));
        int offset = 0;
        for(int i = 0; i < 6; ++i)
        {
            if(surfmask&(1<<i))
            {
                surfaceinfo &surf = c.ext->surfaces[i];
                if(mapversion <= 0)
                {
                    polysurfacecompat psurf;
                    f->read(&psurf, sizeof(polysurfacecompat));
                    surf.verts = psurf.verts;
                    surf.numverts = psurf.numverts;
                }
                else f->read(&surf, sizeof(surf));
                int vertmask = surf.verts, numverts = surf.totalverts();
                if(!numverts) { surf.verts = 0; continue; }
                surf.verts = offset;
                vertinfo *verts = c.ext->verts() + offset;
                offset += numverts;
                ivec v[4], n, vo = ivec(co).mask(0xFFF).shl(3);
                int layerverts = surf.numverts&Face_MaxVerts, dim = DIMENSION(i), vc = C[dim], vr = R[dim], bias = 0;
                genfaceverts(c, i, v);
                bool hasxyz = (vertmask&0x04)!=0, hasuv = mapversion <= 0 && (vertmask&0x40)!=0, hasnorm = (vertmask&0x80)!=0;
                if(hasxyz)
                {
                    ivec e1, e2, e3;
                    n.cross((e1 = v[1]).sub(v[0]), (e2 = v[2]).sub(v[0]));
                    if(n.iszero()) n.cross(e2, (e3 = v[3]).sub(v[0]));
                    bias = -n.dot(ivec(v[0]).mul(size).add(vo));
                }
                else
                {
                    int vis = layerverts < 4 ? (vertmask&0x02 ? 2 : 1) : 3, order = vertmask&0x01 ? 1 : 0, k = 0;
                    verts[k++].setxyz(v[order].mul(size).add(vo));
                    if(vis&1)
                    {
                        verts[k++].setxyz(v[order+1].mul(size).add(vo));
                    }
                    verts[k++].setxyz(v[order+2].mul(size).add(vo));
                    if(vis&2)
                    {
                        verts[k++].setxyz(v[(order+3)&3].mul(size).add(vo));
                    }
                }
                if(layerverts == 4)
                {
                    if(hasxyz && vertmask&0x01)
                    {
                        ushort c1 = f->get<ushort>(),
                               r1 = f->get<ushort>(),
                               c2 = f->get<ushort>(),
                               r2 = f->get<ushort>();
                        ivec xyz;
                        xyz[vc] = c1;
                        xyz[vr] = r1;
                        xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        verts[0].setxyz(xyz);
                        xyz[vc] = c1;
                        xyz[vr] = r2;
                        xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        verts[1].setxyz(xyz);
                        xyz[vc] = c2;
                        xyz[vr] = r2;
                        xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        verts[2].setxyz(xyz);
                        xyz[vc] = c2;
                        xyz[vr] = r1;
                        xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        verts[3].setxyz(xyz);
                        hasxyz = false;
                    }
                    if(hasuv && vertmask&0x02)
                    {
                        for(int k = 0; k < 4; ++k)
                        {
                            f->get<ushort>();
                        }
                        if(surf.numverts&LAYER_DUP)
                        {
                            for(int k = 0; k < 4; ++k)
                            {
                                f->get<ushort>();
                            }
                        }
                        hasuv = false;
                    }
                }
                if(hasnorm && vertmask&0x08)
                {
                    ushort norm = f->get<ushort>();
                    for(int k = 0; k < layerverts; ++k)
                    {
                        verts[k].norm = norm;
                    }
                    hasnorm = false;
                }
                if(hasxyz || hasuv || hasnorm)
                {
                    for(int k = 0; k < layerverts; ++k)
                    {
                        vertinfo &v = verts[k];
                        if(hasxyz)
                        {
                            ivec xyz;
                            xyz[vc] = f->get<ushort>(); xyz[vr] = f->get<ushort>();
                            xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                            v.setxyz(xyz);
                        }
                        if(hasuv)
                        {
                            f->get<ushort>();
                            f->get<ushort>();
                        }
                        if(hasnorm)
                        {
                            v.norm = f->get<ushort>();
                        }
                    }
                }
                if(hasuv && surf.numverts&LAYER_DUP)
                {
                    for(int k = 0; k < layerverts; ++k)
                    {
                        f->get<ushort>();
                        f->get<ushort>();
                    }
                }
            }
        }
    }
}

cube *loadchildren(stream *f, const ivec &co, int size, bool &failed)
{
    cube *c = newcubes();
    for(int i = 0; i < 8; ++i)
    {
        loadc(f, c[i], ivec(i, co, size), size, failed);
        if(failed)
        {
            break;
        }
    }
    return c;
}

VAR(dbgvars, 0, 0, 1);

void savevslot(stream *f, VSlot &vs, int prev)
{
    f->put<int>(vs.changed);
    f->put<int>(prev);
    if(vs.changed & (1 << VSlot_ShParam))
    {
        f->put<ushort>(vs.params.length());
        for(int i = 0; i < vs.params.length(); i++)
        {
            SlotShaderParam &p = vs.params[i];
            f->put<ushort>(strlen(p.name));
            f->write(p.name, strlen(p.name));
            for(int k = 0; k < 4; ++k)
            {
                f->put<float>(p.val[k]);
            }
        }
    }
    if(vs.changed & (1 << VSlot_Scale))
    {
        f->put<float>(vs.scale);
    }
    if(vs.changed & (1 << VSlot_Rotation))
    {
        f->put<int>(vs.rotation);
    }
    if(vs.changed & (1 << VSlot_Angle))
    {
        f->put<float>(vs.angle.x);
        f->put<float>(vs.angle.y);
        f->put<float>(vs.angle.z);
    }
    if(vs.changed & (1 << VSlot_Offset))
    {
        for(int k = 0; k < 2; ++k)
        {
            f->put<int>(vs.offset[k]);
        }
    }
    if(vs.changed & (1 << VSlot_Scroll))
    {
        for(int k = 0; k < 2; ++k)
        {
            f->put<float>(vs.scroll[k]);
        }
    }
    if(vs.changed & (1 << VSlot_Alpha))
    {
        f->put<float>(vs.alphafront);
        f->put<float>(vs.alphaback);
    }
    if(vs.changed & (1 << VSlot_Color))
    {
        for(int k = 0; k < 3; ++k)
        {
            f->put<float>(vs.colorscale[k]);
        }
    }
    if(vs.changed & (1 << VSlot_Refract))
    {
        f->put<float>(vs.refractscale);
        for(int k = 0; k < 3; ++k)
        {
            f->put<float>(vs.refractcolor[k]);
        }
    }
}

void savevslots(stream *f, int numvslots)
{
    if(vslots.empty())
    {
        return;
    }
    int *prev = new int[numvslots];
    memset(prev, -1, numvslots*sizeof(int));
    for(int i = 0; i < numvslots; ++i)
    {
        VSlot *vs = vslots[i];
        if(vs->changed)
        {
            continue;
        }
        for(;;)
        {
            VSlot *cur = vs;
            do
            {
                vs = vs->next;
            } while(vs && vs->index >= numvslots);
            if(!vs)
            {
                break;
            }
            prev[vs->index] = cur->index;
        }
    }
    int lastroot = 0;
    for(int i = 0; i < numvslots; ++i)
    {
        VSlot &vs = *vslots[i];
        if(!vs.changed)
        {
            continue;
        }
        if(lastroot < i)
        {
            f->put<int>(-(i - lastroot));
        }
        savevslot(f, vs, prev[i]);
        lastroot = i+1;
    }
    if(lastroot < numvslots)
    {
        f->put<int>(-(numvslots - lastroot));
    }
    delete[] prev;
}

void loadvslot(stream *f, VSlot &vs, int changed)
{
    vs.changed = changed;
    if(vs.changed & (1 << VSlot_ShParam))
    {
        int numparams = f->get<ushort>();
        string name;
        for(int i = 0; i < numparams; ++i)
        {
            SlotShaderParam &p = vs.params.add();
            int nlen = f->get<ushort>();
            f->read(name, min(nlen, MAXSTRLEN-1));
            name[min(nlen, MAXSTRLEN-1)] = '\0';
            if(nlen >= MAXSTRLEN)
            {
                f->seek(nlen - (MAXSTRLEN-1), SEEK_CUR);
            }
            p.name = getshaderparamname(name);
            p.loc = -1;
            for(int k = 0; k < 4; ++k)
            {
                p.val[k] = f->get<float>();
            }
        }
    }
    //vslot properties (set by e.g. v-commands)
    if(vs.changed & (1 << VSlot_Scale)) //scale <factor>
    {
        vs.scale = f->get<float>();
    }
    if(vs.changed & (1 << VSlot_Rotation)) //rotate <index>
    {
        vs.rotation = std::clamp(f->get<int>(), 0, 7);
    }
    /*
     * angle uses three parameters to prebake sine/cos values for the angle it
     * stores despite there being only one parameter (angle) passed
     */
    if(vs.changed & (1 << VSlot_Angle)) //angle <angle>
    {
        for(int k = 0; k < 3; ++k)
        {
            vs.angle[k] = f->get<float>();
        }
    }
    if(vs.changed & (1 << VSlot_Offset)) //offset <x> <y>
    {
        for(int k = 0; k < 2; ++k)
        {
            vs.offset[k] = f->get<int>();
        }
    }
    if(vs.changed & (1 << VSlot_Scroll)) //scroll <x> <y>
    {
        for(int k = 0; k < 2; ++k)
        {
            vs.scroll[k] = f->get<float>();
        }
    }
    if(vs.changed & (1 << VSlot_Alpha)) //alpha <f> <b>
    {
        vs.alphafront = f->get<float>();
        vs.alphaback = f->get<float>();
    }
    if(vs.changed & (1 << VSlot_Color)) //color <r> <g> <b>
    {
        for(int k = 0; k < 3; ++k)
        {
            vs.colorscale[k] = f->get<float>();
        }
    }
    if(vs.changed & (1 << VSlot_Refract)) //refract <r> <g> <b>
    {
        vs.refractscale = f->get<float>();
        for(int k = 0; k < 3; ++k)
        {
            vs.refractcolor[k] = f->get<float>();
        }
    }
}

void loadvslots(stream *f, int numvslots)
{
    int *prev = new (false) int[numvslots];
    if(!prev)
    {
        return;
    }
    memset(prev, -1, numvslots*sizeof(int));
    while(numvslots > 0)
    {
        int changed = f->get<int>();
        if(changed < 0)
        {
            for(int i = 0; i < -changed; ++i)
            {
                vslots.add(new VSlot(NULL, vslots.length()));
            }
            numvslots += changed;
        }
        else
        {
            prev[vslots.length()] = f->get<int>();
            loadvslot(f, *vslots.add(new VSlot(NULL, vslots.length())), changed);
            numvslots--;
        }
    }
    for(int i = 0; i < vslots.length(); i++)
    {
        if(vslots.inrange(prev[i]))
        {
            vslots[prev[i]]->next = vslots[i];
        }
    }
    delete[] prev;
}

bool save_world(const char *mname, const char *gameident)
{
    if(!*mname)
    {
        mname = game::getclientmap();
    }
    setmapfilenames(*mname ? mname : "untitled");
    if(savebak)
    {
        backup(ogzname, bakname);
    }
    stream *f = opengzfile(ogzname, "wb");
    if(!f)
    {
        conoutf(Console_Warn, "could not write map to %s", ogzname);
        return false;
    }
    int numvslots = vslots.length();
    if(!multiplayer(false))
    {
        numvslots = compactvslots();
        allchanged();
    }

    savemapprogress = 0;
    renderprogress(0, "saving map...");

    mapheader hdr;
    memcpy(hdr.magic, "TMAP", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(hdr);
    hdr.worldsize = worldsize;
    hdr.numents = 0;
    const vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < ents.length(); i++)
    {
        if(ents[i]->type!=EngineEnt_Empty)
        {
            hdr.numents++;
        }
    }
    hdr.numvars = 0;
    hdr.numvslots = numvslots;
    ENUMERATE(idents, ident, id,
    {
        if((id.type == Id_Var || id.type == Id_FloatVar || id.type == Id_StringVar) &&
             id.flags&Idf_Override  &&
           !(id.flags&Idf_ReadOnly) &&
             id.flags&Idf_Overridden)
        {
            hdr.numvars++;
        }
    });
    f->write(&hdr, sizeof(hdr));

    ENUMERATE(idents, ident, id,
    {
        if((id.type!=Id_Var && id.type!=Id_FloatVar && id.type!=Id_StringVar) ||
          !(id.flags&Idf_Override)   ||
          id.flags&Idf_ReadOnly      ||
          !(id.flags&Idf_Overridden))
        {
            continue;
        }
        f->putchar(id.type);
        f->put<ushort>(strlen(id.name));
        f->write(id.name, strlen(id.name));
        switch(id.type)
        {
            case Id_Var:
                if(dbgvars)
                {
                    conoutf(Console_Debug, "wrote var %s: %d", id.name, *id.storage.i);
                }
                f->put<int>(*id.storage.i);
                break;

            case Id_FloatVar:
                if(dbgvars)
                {
                    conoutf(Console_Debug, "wrote fvar %s: %f", id.name, *id.storage.f);
                }
                f->put<float>(*id.storage.f);
                break;

            case Id_StringVar:
                if(dbgvars)
                {
                    conoutf(Console_Debug, "wrote svar %s: %s", id.name, *id.storage.s);
                }
                f->put<ushort>(strlen(*id.storage.s));
                f->write(*id.storage.s, strlen(*id.storage.s));
                break;
        }
    });
    if(dbgvars)
    {
        conoutf(Console_Debug, "wrote %d vars", hdr.numvars);
    }
    f->putchar(static_cast<int>(strlen(gameident)));
    f->write(gameident, static_cast<int>(strlen(gameident)+1));
    //=== padding for compatibility (extent properties no longer a feature)
    f->put<ushort>(0);
    f->put<ushort>(0);
    f->write(0, 0);
    //=== end of padding
    f->put<ushort>(texmru.length());
    for(int i = 0; i < texmru.length(); i++)
    {
        f->put<ushort>(texmru[i]);
    }
    for(int i = 0; i < ents.length(); i++)
    {
        if(ents[i]->type!=EngineEnt_Empty)
        {
            entity tmp = *ents[i];
            f->write(&tmp, sizeof(entity));
        }
    }
    savevslots(f, numvslots);
    renderprogress(0, "saving octree...");
    savec(worldroot, ivec(0, 0, 0), worldsize>>1, f);
    delete f;
    conoutf("wrote map file %s", ogzname);
    return true;
}

static uint mapcrc = 0;

uint getmapcrc() { return mapcrc; }
void clearmapcrc() { mapcrc = 0; }

bool load_world(const char *mname, const char *gameident, const char *gameinfo, const char *cname)
{
    int loadingstart = SDL_GetTicks();
    setmapfilenames(mname, cname);
    stream *f = opengzfile(ogzname, "rb");
    if(!f)
    {
        conoutf(Console_Error, "could not read map %s", ogzname);
        return false;
    }
    mapheader hdr;
    octaheader ohdr;
    memset(&ohdr, 0, sizeof(ohdr));
    if(!loadmapheader(f, ogzname, hdr, ohdr))
    {
        delete f;
        return false;
    }
    resetmap();
    Texture *mapshot = textureload(picname, 3, true, false);
    renderbackground("loading...", mapshot, mname, gameinfo);
    setvar("mapversion", hdr.version, true, false);
    renderprogress(0, "clearing world...");
    freeocta(worldroot);
    worldroot = NULL;
    int worldscale = 0;
    while(1<<worldscale < hdr.worldsize)
    {
        worldscale++;
    }
    setvar("mapsize", 1<<worldscale, true, false);
    setvar("mapscale", worldscale, true, false);
    renderprogress(0, "loading vars...");
    for(int i = 0; i < hdr.numvars; ++i)
    {
        int type = f->getchar(),
            ilen = f->get<ushort>();
        string name;
        f->read(name, min(ilen, MAXSTRLEN-1));
        name[min(ilen, MAXSTRLEN-1)] = '\0';
        if(ilen >= MAXSTRLEN)
        {
            f->seek(ilen - (MAXSTRLEN-1), SEEK_CUR);
        }
        ident *id = getident(name);
        tagval val;
        string str;
        switch(type)
        {
            case Id_Var:
            {
                val.setint(f->get<int>());
                break;
            }
            case Id_FloatVar:
            {
                val.setfloat(f->get<float>());
                break;
            }
            case Id_StringVar:
            {
                int slen = f->get<ushort>();
                f->read(str, min(slen, MAXSTRLEN-1));
                str[min(slen, MAXSTRLEN-1)] = '\0';
                if(slen >= MAXSTRLEN)
                {
                    f->seek(slen - (MAXSTRLEN-1), SEEK_CUR);
                }
                val.setstr(str);
                break;
            }
            default:
            {
                continue;
            }
        }
        if(id && id->flags&Idf_Override)
        {
            switch(id->type)
            {
                case Id_Var:
                {
                    int i = val.getint();
                    if(id->minval <= id->maxval && i >= id->minval && i <= id->maxval)
                    {
                        setvar(name, i);
                        if(dbgvars)
                        {
                            conoutf(Console_Debug, "read var %s: %d", name, i);
                        }
                    }
                    break;
                }
                case Id_FloatVar:
                {
                    float f = val.getfloat();
                    if(id->minvalf <= id->maxvalf && f >= id->minvalf && f <= id->maxvalf)
                    {
                        setfvar(name, f);
                        if(dbgvars)
                        {
                            conoutf(Console_Debug, "read fvar %s: %f", name, f);
                        }
                    }
                    break;
                }
                case Id_StringVar:
                {
                    setsvar(name, val.getstr());
                    if(dbgvars)
                    {
                        conoutf(Console_Debug, "read svar %s: %s", name, val.getstr());
                    }
                    break;
                }
            }
        }
    }
    if(dbgvars)
    {
        conoutf(Console_Debug, "read %d vars", hdr.numvars);
    }
    string gametype;
    bool samegame = true;
    int len = f->getchar();
    if(len >= 0)
    {
        f->read(gametype, len+1);
    }
    gametype[max(len, 0)] = '\0';
    if(strcmp(gametype, gameident)!=0)
    {
        samegame = false;
        conoutf(Console_Warn, "WARNING: loading map from %s game, ignoring entities except for lights/mapmodels", gametype);
    }
    int eif = f->get<ushort>(),
        extrasize = f->get<ushort>();
    vector<char> extras;
    f->read(extras.pad(extrasize), extrasize);
    texmru.shrink(0);
    ushort nummru = f->get<ushort>();
    for(int i = 0; i < nummru; ++i)
    {
        texmru.add(f->get<ushort>());
    }
    renderprogress(0, "loading entities...");
    vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < (min(hdr.numents, maxents)); ++i)
    {
        extentity &e = *entities::newentity();
        ents.add(&e);
        f->read(&e, sizeof(entity));
        fixent(e, hdr.version);
        //delete entities from other games
        if(!samegame)
        {
            if(eif > 0)
            {
                f->seek(eif, SEEK_CUR);
            }
            if(e.type>=EngineEnt_GameSpecific)
            {
                entities::deleteentity(ents.pop());
                continue;
            }
        }
        if(!insideworld(e.o))
        {
            if(e.type != EngineEnt_Light && e.type != EngineEnt_Spotlight)
            {
                conoutf(Console_Warn, "warning: ent outside of world: index %d (%f, %f, %f)", i, e.o.x, e.o.y, e.o.z);
            }
        }
    }
    if(hdr.numents > maxents)
    {
        conoutf(Console_Warn, "warning: map has %d entities", hdr.numents);
        f->seek((hdr.numents-maxents)*(samegame ? sizeof(entity) : eif), SEEK_CUR);
    }
    renderprogress(0, "loading slots...");
    loadvslots(f, hdr.numvslots);
    renderprogress(0, "loading octree...");
    bool failed = false;
    worldroot = loadchildren(f, ivec(0, 0, 0), hdr.worldsize>>1, failed);
    if(failed)
    {
        conoutf(Console_Error, "garbage in map");
    }
    renderprogress(0, "validating...");
    validatec(worldroot, hdr.worldsize>>1);

    mapcrc = f->getcrc();
    delete f;
    conoutf("read map %s (%.1f seconds)", ogzname, (SDL_GetTicks()-loadingstart)/1000.0f);
    clearmainmenu();

    identflags |= Idf_Overridden;
    execfile("config/default_map_settings.cfg", false);
    execfile(cfgname, false);
    identflags &= ~Idf_Overridden;

    preloadusedmapmodels(true);
    game::preload();
    flushpreloadedmodels();
    preloadmapsounds();
    entitiesinoctanodes();
    attachentities();
    allchanged(true);

    renderbackground("loading...", mapshot, mname, gameinfo);

    if(maptitle[0] && strcmp(maptitle, "Untitled Map by Unknown"))
    {
        conoutf(Console_Echo, "%s", maptitle);
    }
    startmap(cname ? cname : mname);
    return true;
}

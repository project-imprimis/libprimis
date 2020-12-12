
#include "engine.h"

#include "interface/console.h"
#include "interface/control.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"
#include "render/rendermodel.h"

#include "world/physics.h"

#include "ragdoll.h"
#include "animmodel.h"
#include "skelmodel.h"

VARP(gpuskel, 0, 1, 1);

VAR(maxskelanimdata, 1, 192, 0);

skelmodel::skelanimspec *skelmodel::skeleton::findskelanim(const char *name, char sep)
{
    int len = sep ? strlen(name) : 0;
    for(int i = 0; i < skelanims.length(); i++)
    {
        if(skelanims[i].name)
        {
            if(sep)
            {
                const char *end = strchr(skelanims[i].name, ':');
                if(end && end - skelanims[i].name == len && !memcmp(name, skelanims[i].name, len))
                {
                    return &skelanims[i];
                }
            }
            if(!strcmp(name, skelanims[i].name))
            {
                return &skelanims[i];
            }
        }
    }
    return NULL;
}

skelmodel::skelanimspec &skelmodel::skeleton::addskelanim(const char *name)
{
    skelanimspec &sa = skelanims.add();
    sa.name = name ? newstring(name) : NULL;
    return sa;
}

int skelmodel::skeleton::findbone(const char *name)
{
    for(int i = 0; i < numbones; ++i)
    {
        if(bones[i].name && !strcmp(bones[i].name, name))
        {
            return i;
        }
    }
    return -1;
}

int skelmodel::skeleton::findtag(const char *name)
{
    for(int i = 0; i < tags.length(); i++)
    {
        if(!strcmp(tags[i].name, name))
        {
            return i;
        }
    }
    return -1;
}

bool skelmodel::skeleton::addtag(const char *name, int bone, const matrix4x3 &matrix)
{
    int idx = findtag(name);
    if(idx >= 0)
    {
        if(!testtags)
        {
            return false;
        }
        tag &t = tags[idx];
        t.bone = bone;
        t.matrix = matrix;
    }
    else
    {
        tag &t = tags.add();
        t.name = newstring(name);
        t.bone = bone;
        t.matrix = matrix;
    }
    return true;
}

void skelmodel::skeleton::calcantipodes()
{
    antipodes.shrink(0);
    std::vector<int> schedule;
    for(int i = 0; i < numbones; ++i)
    {
        if(bones[i].group >= numbones)
        {
            bones[i].scheduled = schedule.size();
            schedule.push_back(i);
        }
        else
        {
            bones[i].scheduled = -1;
        }
    }
    for(uint i = 0; i < schedule.size(); i++)
    {
        int bone = schedule[i];
        const boneinfo &info = bones[bone];
        for(int j = 0; j < numbones; ++j)
        {
            if(abs(bones[j].group) == bone && bones[j].scheduled < 0)
            {
                antipodes.add(antipode(info.interpindex, bones[j].interpindex));
                bones[j].scheduled = schedule.size();
                schedule.push_back(j);
            }
        }
        if(i + 1 == schedule.size())
        {
            int conflict = INT_MAX;
            for(int j = 0; j < numbones; ++j)
            {
                if(bones[j].group < numbones && bones[j].scheduled < 0)
                {
                    conflict = min(conflict, abs(bones[j].group));
                }
            }
            if(conflict < numbones)
            {
                bones[conflict].scheduled = schedule.size();
                schedule.push_back(conflict);
            }
        }
    }
}

void skelmodel::skeleton::remapbones()
{
    for(int i = 0; i < numbones; ++i)//loop i
    {
        boneinfo &info = bones[i];
        info.interpindex = -1;
        info.ragdollindex = -1;
    }
    numgpubones = 0;
    for(int i = 0; i < users.length(); i++)
    {
        skelmeshgroup *group = users[i];
        for(int j = 0; j < group->blendcombos.length(); j++) //loop j
        {
            blendcombo &c = group->blendcombos[j];
            for(int k = 0; k < 4; ++k) //loop k
            {
                if(!c.weights[k])
                {
                    c.interpbones[k] = k > 0 ? c.interpbones[k-1] : 0;
                    continue;
                }
                boneinfo &info = bones[c.bones[k]];
                if(info.interpindex < 0)
                {
                    info.interpindex = numgpubones++;
                }
                c.interpbones[k] = info.interpindex;
                if(info.group < 0)
                {
                    continue;
                }
                for(int l = 0; l < 4; ++l) //note this is a loop l (level 4)
                {
                    if(!c.weights[l])
                    {
                        break;
                    }
                    if(l == k)
                    {
                        continue;
                    }
                    int parent = c.bones[l];
                    if(info.parent == parent || (info.parent >= 0 && info.parent == bones[parent].parent))
                    {
                        info.group = -info.parent;
                        break;
                    }
                    if(info.group <= parent)
                    {
                        continue;
                    }
                    int child = c.bones[k];
                    while(parent > child)
                    {
                        parent = bones[parent].parent;
                    }
                    if(parent != child)
                    {
                        info.group = c.bones[l];
                    }
                }
            }
        }
    }
    numinterpbones = numgpubones;
    for(int i = 0; i < tags.length(); i++)
    {
        boneinfo &info = bones[tags[i].bone];
        if(info.interpindex < 0)
        {
            info.interpindex = numinterpbones++;
        }
    }
    if(ragdoll)
    {
        for(int i = 0; i < ragdoll->joints.length(); i++)
        {
            boneinfo &info = bones[ragdoll->joints[i].bone];
            if(info.interpindex < 0)
            {
                info.interpindex = numinterpbones++;
            }
            info.ragdollindex = i;
        }
    }
    for(int i = 0; i < numbones; ++i)
    {
        boneinfo &info = bones[i];
        if(info.interpindex < 0)
        {
            continue;
        }
        for(int parent = info.parent; parent >= 0 && bones[parent].interpindex < 0; parent = bones[parent].parent)
            bones[parent].interpindex = numinterpbones++;
    }
    for(int i = 0; i < numbones; ++i)
    {
        boneinfo &info = bones[i];
        if(info.interpindex < 0) continue;
        info.interpparent = info.parent >= 0 ? bones[info.parent].interpindex : -1;
    }
    if(ragdoll)
    {
        for(int i = 0; i < numbones; ++i)
        {
            boneinfo &info = bones[i];
            if(info.interpindex < 0 || info.ragdollindex >= 0)
            {
                continue;
            }
            for(int parent = info.parent; parent >= 0; parent = bones[parent].parent)
            {
                if(bones[parent].ragdollindex >= 0)
                {
                    ragdoll->addreljoint(i, bones[parent].ragdollindex);
                    break;
                }
            }
        }
    }
    calcantipodes();
}

void skelmodel::skeleton::addpitchdep(int bone, int frame)
{
    for(; bone >= 0; bone = bones[bone].parent)
    {
        int pos = pitchdeps.length();
        for(int j = 0; j < pitchdeps.length(); j++)
        {
            if(bone <= pitchdeps[j].bone)
            {
                if(bone == pitchdeps[j].bone)
                {
                    goto nextbone;
                }
                pos = j;
                break;
            }
        }
        { //if goto nextbone not called (note: no control logic w/ braces)
            pitchdep d;
            d.bone = bone;
            d.parent = -1;
            d.pose = framebones[frame*numbones + bone];
            pitchdeps.insert(pos, d);
        }
    nextbone:;
    }
}

int skelmodel::skeleton::findpitchdep(int bone)
{
    for(int i = 0; i < pitchdeps.length(); i++)
    {
        if(bone <= pitchdeps[i].bone)
        {
            return bone == pitchdeps[i].bone ? i : -1;
        }
    }
    return -1;
}

int skelmodel::skeleton::findpitchcorrect(int bone)
{
    for(int i = 0; i < pitchcorrects.length(); i++)
    {
        if(bone <= pitchcorrects[i].bone)
        {
            return bone == pitchcorrects[i].bone ? i : -1;
        }
    }
    return -1;
}

void skelmodel::skeleton::initpitchdeps()
{
    pitchdeps.setsize(0);
    if(pitchtargets.empty())
    {
        return;
    }
    for(int i = 0; i < pitchtargets.length(); i++)
    {
        pitchtarget &t = pitchtargets[i];
        t.deps = -1;
        addpitchdep(t.bone, t.frame);
    }
    for(int i = 0; i < pitchdeps.length(); i++)
    {
        pitchdep &d = pitchdeps[i];
        int parent = bones[d.bone].parent;
        if(parent >= 0)
        {
            int j = findpitchdep(parent);
            if(j >= 0)
            {
                d.parent = j;
                d.pose.mul(pitchdeps[j].pose, dualquat(d.pose));
            }
        }
    }
    for(int i = 0; i < pitchtargets.length(); i++)
    {
        pitchtarget &t = pitchtargets[i];
        int j = findpitchdep(t.bone);
        if(j >= 0)
        {
            t.deps = j;
            t.pose = pitchdeps[j].pose;
        }
        t.corrects = -1;
        for(int parent = t.bone; parent >= 0; parent = bones[parent].parent)
        {
            t.corrects = findpitchcorrect(parent);
            if(t.corrects >= 0)
            {
                break;
            }
        }
    }
    for(int i = 0; i < pitchcorrects.length(); i++)
    {
        pitchcorrect &c = pitchcorrects[i];
        bones[c.bone].correctindex = i;
        c.parent = -1;
        for(int parent = c.bone;;)
        {
            parent = bones[parent].parent;
            if(parent < 0)
            {
                break;
            }
            c.parent = findpitchcorrect(parent);
            if(c.parent >= 0)
            {
                break;
            }
        }
    }
}

void skelmodel::skeleton::optimize()
{
    cleanup();
    if(ragdoll)
    {
        ragdoll->setup();
    }
    remapbones();
    initpitchdeps();
}

void skelmodel::skeleton::expandbonemask(uchar *expansion, int bone, int val)
{
    expansion[bone] = val;
    bone = bones[bone].children;
    while(bone>=0)
    {
        expandbonemask(expansion, bone, val);
        bone = bones[bone].next;
    }
}

void skelmodel::skeleton::applybonemask(ushort *mask, uchar *partmask, int partindex)
{
    if(!mask || *mask==BONEMASK_END)
    {
        return;
    }
    uchar *expansion = new uchar[numbones];
    memset(expansion, *mask&BONEMASK_NOT ? 1 : 0, numbones);
    while(*mask!=BONEMASK_END)
    {
        expandbonemask(expansion, *mask&BONEMASK_BONE, *mask&BONEMASK_NOT ? 0 : 1);
        mask++;
    }
    for(int i = 0; i < numbones; ++i)
    {
        if(expansion[i])
        {
            partmask[i] = partindex;
        }
    }
    delete[] expansion;
}

void skelmodel::skeleton::linkchildren()
{
    for(int i = 0; i < numbones; ++i)
    {
        boneinfo &b = bones[i];
        b.children = -1;
        if(b.parent<0)
        {
            b.next = -1;
        }
        else
        {
            b.next = bones[b.parent].children;
            bones[b.parent].children = i;
        }
    }
}

int skelmodel::skeleton::availgpubones() const
{
    return min(maxvsuniforms, maxskelanimdata) / 2;
}

bool skelmodel::skeleton::gpuaccelerate() const
{
    return numframes && gpuskel && numgpubones<=availgpubones();
}

float skelmodel::skeleton::calcdeviation(const vec &axis, const vec &forward, const dualquat &pose1, const dualquat &pose2)
{
    vec forward1 = pose1.transformnormal(forward).project(axis).normalize(),
        forward2 = pose2.transformnormal(forward).project(axis).normalize(),
        daxis = vec().cross(forward1, forward2);
    float dx = std::clamp(forward1.dot(forward2), -1.0f, 1.0f),
          dy = std::clamp(daxis.magnitude(), -1.0f, 1.0f);
    if(daxis.dot(axis) < 0)
    {
        dy = -dy;
    }
    return atan2f(dy, dx)/RAD;
}

void skelmodel::skeleton::calcpitchcorrects(float pitch, const vec &axis, const vec &forward)
{
    for(int i = 0; i < pitchtargets.length(); i++)
    {
        pitchtarget &t = pitchtargets[i];
        t.deviated = calcdeviation(axis, forward, t.pose, pitchdeps[t.deps].pose);
    }
    for(int i = 0; i < pitchcorrects.length(); i++)
    {
        pitchcorrect &c = pitchcorrects[i];
        c.pitchangle = c.pitchtotal = 0;
    }
    for(int j = 0; j < pitchtargets.length(); j++)
    {
        pitchtarget &t = pitchtargets[j];
        float tpitch = pitch - t.deviated;
        for(int parent = t.corrects; parent >= 0; parent = pitchcorrects[parent].parent)
            tpitch -= pitchcorrects[parent].pitchangle;
        if(t.pitchmin || t.pitchmax)
        {
            tpitch = std::clamp(tpitch, t.pitchmin, t.pitchmax);
        }
        for(int i = 0; i < pitchcorrects.length(); i++)
        {
            pitchcorrect &c = pitchcorrects[i];
            if(c.target != j)
            {
                continue;
            }
            float total = c.parent >= 0 ? pitchcorrects[c.parent].pitchtotal : 0,
                  avail = tpitch - total,
                  used = tpitch*c.pitchscale;
            if(c.pitchmin || c.pitchmax)
            {
                if(used < 0)
                {
                    used = std::clamp(c.pitchmin, used, 0.0f);
                }
                else
                {
                    used = std::clamp(c.pitchmax, 0.0f, used);
                }
            }
            if(used < 0)
            {
                used = std::clamp(avail, used, 0.0f);
            }
            else
            {
                used = std::clamp(avail, 0.0f, used);
            }
            c.pitchangle = used;
            c.pitchtotal = used + total;
        }
    }
}

#define INTERPBONE(bone) \
    const animstate &s = as[partmask[bone]]; \
    const framedata &f = partframes[partmask[bone]]; \
    dualquat d; \
    (d = f.fr1[bone]).mul((1-s.cur.t)*s.interp); \
    d.accumulate(f.fr2[bone], s.cur.t*s.interp); \
    if(s.interp<1) \
    { \
        d.accumulate(f.pfr1[bone], (1-s.prev.t)*(1-s.interp)); \
        d.accumulate(f.pfr2[bone], s.prev.t*(1-s.interp)); \
    }

void skelmodel::skeleton::interpbones(const animstate *as, float pitch, const vec &axis, const vec &forward, int numanimparts, const uchar *partmask, skelcacheentry &sc)
{
    if(!sc.bdata)
    {
        sc.bdata = new dualquat[numinterpbones];
    }
    sc.nextversion();
    struct framedata
    {
        const dualquat *fr1, *fr2, *pfr1, *pfr2;
    } partframes[maxanimparts];
    for(int i = 0; i < numanimparts; ++i)
    {
        partframes[i].fr1 = &framebones[as[i].cur.fr1*numbones];
        partframes[i].fr2 = &framebones[as[i].cur.fr2*numbones];
        if(as[i].interp<1)
        {
            partframes[i].pfr1 = &framebones[as[i].prev.fr1*numbones];
            partframes[i].pfr2 = &framebones[as[i].prev.fr2*numbones];
        }
    }
    for(int i = 0; i < pitchdeps.length(); i++)
    {
        pitchdep &p = pitchdeps[i];
        INTERPBONE(p.bone);
        d.normalize();
        if(p.parent >= 0)
        {
            p.pose.mul(pitchdeps[p.parent].pose, d);
        }
        else
        {
            p.pose = d;
        }
    }
    calcpitchcorrects(pitch, axis, forward);
    for(int i = 0; i < numbones; ++i)
    {
        if(bones[i].interpindex>=0)
        {
            INTERPBONE(i);
            d.normalize();
            const boneinfo &b = bones[i];
            if(b.interpparent<0)
            {
                sc.bdata[b.interpindex] = d;
            }
            else
            {
                sc.bdata[b.interpindex].mul(sc.bdata[b.interpparent], d);
            }
            float angle;
            if(b.pitchscale)
            {
                angle = b.pitchscale*pitch + b.pitchoffset;
                if(b.pitchmin || b.pitchmax)
                {
                    angle = std::clamp(angle, b.pitchmin, b.pitchmax);
                }
            }
            else if(b.correctindex >= 0)
            {
                angle = pitchcorrects[b.correctindex].pitchangle;
            }
            else
            {
                continue;
            }
            if(as->cur.anim & Anim_NoPitch || (as->interp < 1 && as->prev.anim & Anim_NoPitch))
            {
                angle *= (as->cur.anim & Anim_NoPitch ? 0 : as->interp) + (as->interp < 1 && as->prev.anim & Anim_NoPitch ? 0 : 1 - as->interp);
            }
            sc.bdata[b.interpindex].mulorient(quat(axis, angle*RAD), b.base);
        }
    }
    for(int i = 0; i < antipodes.length(); i++)
    {
        sc.bdata[antipodes[i].child].fixantipodal(sc.bdata[antipodes[i].parent]);
    }
}

void skelmodel::skeleton::initragdoll(ragdolldata &d, skelcacheentry &sc, part *p)
{
    const dualquat *bdata = sc.bdata;
    for(int i = 0; i < ragdoll->joints.length(); i++)
    {
        const ragdollskel::joint &j = ragdoll->joints[i];
        const boneinfo &b = bones[j.bone];
        const dualquat &q = bdata[b.interpindex];
        for(int k = 0; k < 3; ++k)
        {
            if(j.vert[k] >= 0)
            {
                ragdollskel::vert &v = ragdoll->verts[j.vert[k]];
                ragdolldata::vert &dv = d.verts[j.vert[k]];
                dv.pos.add(q.transform(v.pos).mul(v.weight));
            }
        }
    }
    if(ragdoll->animjoints)
    {
        for(int i = 0; i < ragdoll->joints.length(); i++)
        {
            const ragdollskel::joint &j = ragdoll->joints[i];
            const boneinfo &b = bones[j.bone];
            const dualquat &q = bdata[b.interpindex];
            d.calcanimjoint(i, matrix4x3(q));
        }
    }
    for(int i = 0; i < ragdoll->verts.length(); i++)
    {
        ragdolldata::vert &dv = d.verts[i];
        matrixstack[matrixpos].transform(vec(dv.pos).mul(p->model->scale), dv.pos);
    }
    for(int i = 0; i < ragdoll->reljoints.length(); i++)
    {
        const ragdollskel::reljoint &r = ragdoll->reljoints[i];
        const ragdollskel::joint &j = ragdoll->joints[r.parent];
        const boneinfo &br = bones[r.bone], &bj = bones[j.bone];
        d.reljoints[i].mul(dualquat(bdata[bj.interpindex]).invert(), bdata[br.interpindex]);
    }
}

void skelmodel::skeleton::genragdollbones(ragdolldata &d, skelcacheentry &sc, part *p)
{
    if(!sc.bdata)
    {
        sc.bdata = new dualquat[numinterpbones];
    }
    sc.nextversion();
    vec trans = vec(d.center).div(p->model->scale).add(p->model->translate);
    for(int i = 0; i < ragdoll->joints.length(); i++)
    {
        const ragdollskel::joint &j = ragdoll->joints[i];
        const boneinfo &b = bones[j.bone];
        vec pos(0, 0, 0);
        for(int k = 0; k < 3; ++k)
        {
            if(j.vert[k]>=0)
            {
                pos.add(d.verts[j.vert[k]].pos);
            }
        }
        pos.mul(j.weight/p->model->scale).sub(trans);
        matrix4x3 m;
        m.mul(d.tris[j.tri], pos, d.animjoints ? d.animjoints[i] : j.orient);
        sc.bdata[b.interpindex] = dualquat(m);
    }
    for(int i = 0; i < ragdoll->reljoints.length(); i++)
    {
        const ragdollskel::reljoint &r = ragdoll->reljoints[i];
        const ragdollskel::joint &j = ragdoll->joints[r.parent];
        const boneinfo &br = bones[r.bone], &bj = bones[j.bone];
        sc.bdata[br.interpindex].mul(sc.bdata[bj.interpindex], d.reljoints[i]);
    }
    for(int i = 0; i < antipodes.length(); i++)
    {
        sc.bdata[antipodes[i].child].fixantipodal(sc.bdata[antipodes[i].parent]);
    }
}

void skelmodel::skeleton::concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n)
{
    matrix4x3 t;
    t.mul(bones[tags[i].bone].base, tags[i].matrix);
    n.mul(m, t);
}

void skelmodel::skeleton::calctags(part *p, skelcacheentry *sc)
{
    for(int i = 0; i < p->links.length(); i++)
    {
        linkedpart &l = p->links[i];
        tag &t = tags[l.tag];
        dualquat q;
        if(sc)
        {
            q.mul(sc->bdata[bones[t.bone].interpindex], bones[t.bone].base);
        }
        else
        {
            q = bones[t.bone].base;
        }
        matrix4x3 m;
        m.mul(q, t.matrix);
        m.d.mul(p->model->scale * sizescale);
        l.matrix = m;
    }
}

void skelmodel::skeleton::cleanup(bool full)
{
    for(int i = 0; i < skelcache.length(); i++)
    {
        skelcacheentry &sc = skelcache[i];
        for(int j = 0; j < maxanimparts; ++j)
        {
            sc.as[j].cur.fr1 = -1;
        }
        DELETEA(sc.bdata);
    }
    skelcache.setsize(0);
    blendoffsets.clear();
    if(full)
    {
        for(int i = 0; i < users.length(); i++)
        {
            users[i]->cleanup();
        }
    }
}

bool skelmodel::skeleton::canpreload()
{
    return !numframes || gpuaccelerate();
}

void skelmodel::skeleton::preload()
{
    if(!numframes)
    {
        return;
    }
    if(skelcache.empty())
    {
        usegpuskel = gpuaccelerate();
    }
}

skelmodel::skelcacheentry &skelmodel::skeleton::checkskelcache(part *p, const animstate *as, float pitch, const vec &axis, const vec &forward, ragdolldata *rdata)
{
    if(skelcache.empty())
    {
        usegpuskel = gpuaccelerate();
    }
    int numanimparts = ((skelpart *)as->owner)->numanimparts;
    uchar *partmask = ((skelpart *)as->owner)->partmask;
    skelcacheentry *sc = NULL;
    bool match = false;
    for(int i = 0; i < skelcache.length(); i++)
    {
        skelcacheentry &c = skelcache[i];
        for(int j = 0; j < numanimparts; ++j)
        {
            if(c.as[j]!=as[j])
            {
                goto mismatch;
            }
        }
        if(c.pitch != pitch || c.partmask != partmask || c.ragdoll != rdata || (rdata && c.millis < rdata->lastmove))
        {
            goto mismatch;
        }
        match = true;
        sc = &c;
        break;
    mismatch:
        if(c.millis < lastmillis)
        {
            sc = &c;
            break;
        }
    }
    if(!sc)
    {
        sc = &skelcache.add();
    }
    if(!match)
    {
        for(int i = 0; i < numanimparts; ++i)
        {
            sc->as[i] = as[i];
        }
        sc->pitch = pitch;
        sc->partmask = partmask;
        sc->ragdoll = rdata;
        if(rdata)
        {
            genragdollbones(*rdata, *sc, p);
        }
        else
        {
            interpbones(as, pitch, axis, forward, numanimparts, partmask, *sc);
        }
    }
    sc->millis = lastmillis;
    return *sc;
}

int skelmodel::skeleton::getblendoffset(UniformLoc &u)
{
    int &offset = blendoffsets.access(Shader::lastshader->program, -1);
    if(offset < 0)
    {
        DEF_FORMAT_STRING(offsetname, "%s[%d]", u.name, 2*numgpubones);
        offset = glGetUniformLocation_(Shader::lastshader->program, offsetname);
    }
    return offset;
}

void skelmodel::skeleton::setglslbones(UniformLoc &u, skelcacheentry &sc, skelcacheentry &bc, int count)
{
    if(u.version == bc.version && u.data == bc.bdata)
    {
        return;
    }
    glUniform4fv_(u.loc, 2*numgpubones, sc.bdata[0].real.v);
    if(count > 0)
    {
        int offset = getblendoffset(u);
        if(offset >= 0)
        {
            glUniform4fv_(offset, 2*count, bc.bdata[0].real.v);
        }
    }
    u.version = bc.version;
    u.data = bc.bdata;
}

void skelmodel::skeleton::setgpubones(skelcacheentry &sc, blendcacheentry *bc, int count)
{
    if(!Shader::lastshader)
    {
        return;
    }
    if(Shader::lastshader->uniformlocs.length() < 1)
    {
        return;
    }
    UniformLoc &u = Shader::lastshader->uniformlocs[0];
    setglslbones(u, sc, bc ? *bc : sc, count);
}

bool skelmodel::skeleton::shouldcleanup() const
{
    return numframes && (skelcache.empty() || gpuaccelerate()!=usegpuskel);
}

void skelmodel::skelmeshgroup::genvbo(vbocacheentry &vc)
{
    if(!vc.vbuf)
    {
        glGenBuffers_(1, &vc.vbuf);
    }
    if(ebuf)
    {
        return;
    }

    vector<ushort> idxs;

    vlen = 0;
    vblends = 0;
    if(skel->numframes && !skel->usegpuskel)
    {
        vweights = 1;
        for(int i = 0; i < blendcombos.length(); i++)
        {
            blendcombo &c = blendcombos[i];
            c.interpindex = c.weights[1] ? skel->numgpubones + vblends++ : -1;
        }

        vertsize = sizeof(vvert);
        LOOP_RENDER_MESHES(skelmesh, m, vlen += m.genvbo(idxs, vlen));
        DELETEA(vdata);
        vdata = new uchar[vlen*vertsize];
        LOOP_RENDER_MESHES(skelmesh, m,
        {
            m.fillverts((vvert *)vdata);
        });
    }
    else
    {
        if(skel->numframes)
        {
            vweights = 4;
            int availbones = skel->availgpubones() - skel->numgpubones;
            while(vweights > 1 && availbones >= numblends[vweights-1])
            {
                availbones -= numblends[--vweights];
            }
            for(int i = 0; i < blendcombos.length(); i++)
            {
                blendcombo &c = blendcombos[i];
                c.interpindex = c.size() > vweights ? skel->numgpubones + vblends++ : -1;
            }
        }
        else
        {
            vweights = 0;
            for(int i = 0; i < blendcombos.length(); i++)
            {
                blendcombos[i].interpindex = -1;
            }
        }

        gle::bindvbo(vc.vbuf);
//====================================================================== GENVBO*
        #define GENVBO(type, args) \
            do \
            { \
                vertsize = sizeof(type); \
                vector<type> vverts; \
                LOOP_RENDER_MESHES(skelmesh, m, vlen += m.genvbo args); \
                glBufferData_(GL_ARRAY_BUFFER, vverts.length()*sizeof(type), vverts.getbuf(), GL_STATIC_DRAW); \
            } while(0)
        /* need these macros so it's possible to pass a variadic chain of
         * args in a single package
         *
         * (the set of values in the latter argument are all passed together
         * as "args" which can't be done by a single standard macro
         */
        #define GENVBOANIM(type) GENVBO(type, (idxs, vlen, vverts))
        #define GENVBOSTAT(type) GENVBO(type, (idxs, vlen, vverts, htdata, htlen))
        if(skel->numframes)
        {
            GENVBOANIM(vvertgw);
        }
        else
        {
            int numverts = 0,
                htlen = 128;
            LOOP_RENDER_MESHES(skelmesh, m, numverts += m.numverts);
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
            GENVBOSTAT(vvertg);
            delete[] htdata;
        }
        #undef GENVBO
        #undef GENVBOANIM
        #undef GENVBOSTAT
//==============================================================================
        gle::clearvbo();
    }

    glGenBuffers_(1, &ebuf);
    gle::bindebo(ebuf);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW);
    gle::clearebo();
}

void skelmodel::skelmeshgroup::render(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p)
{
    if(skel->shouldcleanup())
    {
        skel->cleanup();
        disablevbo();
    }

    if(!skel->numframes)
    {
        if(!(as->cur.anim & Anim_NoRender))
        {
            if(!vbocache->vbuf)
            {
                genvbo(*vbocache);
            }
            bindvbo(as, p, *vbocache);
            LOOP_RENDER_MESHES(skelmesh, m,
            {
                p->skins[i].bind(m, as);
                m.render(as, p->skins[i], *vbocache);
            });
        }
        skel->calctags(p);
        return;
    }

    skelcacheentry &sc = skel->checkskelcache(p, as, pitch, axis, forward, !d || !d->ragdoll || d->ragdoll->skel != skel->ragdoll || d->ragdoll->millis == lastmillis ? NULL : d->ragdoll);
    if(!(as->cur.anim & Anim_NoRender))
    {
        int owner = &sc-&skel->skelcache[0];
        vbocacheentry &vc = skel->usegpuskel ? *vbocache : checkvbocache(sc, owner);
        vc.millis = lastmillis;
        if(!vc.vbuf)
        {
            genvbo(vc);
        }
        blendcacheentry *bc = NULL;
        if(vblends)
        {
            bc = &checkblendcache(sc, owner);
            bc->millis = lastmillis;
            if(bc->owner!=owner)
            {
                bc->owner = owner;
                *(animcacheentry *)bc = sc;
                blendbones(sc, *bc);
            }
        }
        if(!skel->usegpuskel && vc.owner != owner)
        {
            vc.owner = owner;
            (animcacheentry &)vc = sc;
            LOOP_RENDER_MESHES(skelmesh, m,
            {
                m.interpverts(sc.bdata, bc ? bc->bdata : NULL, (vvert *)vdata, p->skins[i]);
            });
            gle::bindvbo(vc.vbuf);
            glBufferData_(GL_ARRAY_BUFFER, vlen*vertsize, vdata, GL_STREAM_DRAW);
        }

        bindvbo(as, p, vc, &sc, bc);

        LOOP_RENDER_MESHES(skelmesh, m,
        {
            p->skins[i].bind(m, as);
            if(skel->usegpuskel)
            {
                skel->setgpubones(sc, bc, vblends);
            }
            m.render(as, p->skins[i], vc);
        });
    }

    skel->calctags(p, &sc);

    if(as->cur.anim & Anim_Ragdoll && skel->ragdoll && !d->ragdoll)
    {
        d->ragdoll = new ragdolldata(skel->ragdoll, p->model->scale);
        skel->initragdoll(*d->ragdoll, sc, p);
        d->ragdoll->init(d);
    }
}

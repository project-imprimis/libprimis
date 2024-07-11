/* skelmodel.cpp: implementation for the skeletal animation object
 *
 * the skelmodel object handles the behavior of animated skeletal models -- such
 * as the ones used by the player and bots
 *
 * for the procedural modification of skeletal models using ragdoll physics, see
 * ragdoll.h
 *
 * this file contains the implementation for the skelmodel object, see skelmodel.h
 * for the class definition
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include <optional>
#include <memory>

#include "interface/console.h"
#include "interface/control.h"
#include "interface/cs.h"

#include "render/rendergl.h"
#include "render/renderlights.h"
#include "render/rendermodel.h"
#include "render/shader.h"
#include "render/shaderparam.h"
#include "render/texture.h"

#include "world/entities.h"
#include "world/octaworld.h"
#include "world/bih.h"

#include "model.h"
#include "ragdoll.h"
#include "animmodel.h"
#include "skelmodel.h"

VARP(gpuskel, 0, 1, 1); //toggles gpu acceleration of skeletal models

VAR(maxskelanimdata, 1, 192, 0); //sets maximum number of gpu bones

//animcacheentry child classes

bool skelmodel::vbocacheentry::check() const
{
    return !vbuf;
}

skelmodel::vbocacheentry::vbocacheentry() : vbuf(0), owner(-1)
{
}

void skelmodel::skelcacheentry::nextversion()
{
    version = Shader::uniformlocversion();
}

skelmodel::skelcacheentry::skelcacheentry() : bdata(nullptr), version(-1)
{
}

bool skelmodel::blendcacheentry::check() const
{
    return false;
}

skelmodel::blendcacheentry::blendcacheentry() : owner(-1)
{
}

//skelmodel::animcacheentry object
skelmodel::animcacheentry::animcacheentry() : ragdoll(nullptr)
{
    for(int k = 0; k < maxanimparts; ++k)
    {
        as[k].cur.fr1 = as[k].prev.fr1 = -1;
    }
}

bool skelmodel::animcacheentry::operator==(const animcacheentry &c) const
{
    for(int i = 0; i < maxanimparts; ++i)
    {
        if(as[i]!=c.as[i])
        {
            return false;
        }
    }
    return pitch==c.pitch && partmask==c.partmask && ragdoll==c.ragdoll && (!ragdoll || std::min(millis, c.millis) >= ragdoll->lastmove);
}

bool skelmodel::animcacheentry::operator!=(const animcacheentry &c) const
{
    return !(*this == c);
}

//pitchcorrect

skelmodel::pitchcorrect::pitchcorrect(int bone, int target, float pitchscale, float pitchmin, float pitchmax) :
    bone(bone), target(target), parent (-1), pitchmin(pitchmin), pitchmax(pitchmax),
    pitchscale(pitchscale), pitchangle(0), pitchtotal(0)
{
}

skelmodel::pitchcorrect::pitchcorrect() : parent(-1), pitchangle(0), pitchtotal(0)
{
}

//skeleton

const skelmodel::skelanimspec *skelmodel::skeleton::findskelanim(std::string_view name, char sep) const
{
    int len = sep ? std::strlen(name.data()) : 0;
    for(const skelanimspec &i : skelanims)
    {
        if(!i.name.empty())
        {
            if(sep)
            {
                const char *end = std::strchr(i.name.c_str(), ':');
                if(end && end - i.name.c_str() == len && !std::memcmp(name.data(), i.name.c_str(), len))
                {
                    return &i;
                }
            }
            if(!std::strcmp(name.data(), i.name.c_str()))
            {
                return &i;
            }
        }
    }
    return nullptr;
}

skelmodel::skelanimspec &skelmodel::skeleton::addskelanim(const std::string &name, int numframes, int animframes)
{
    skelanims.emplace_back();
    skelanimspec &sa = skelanims.back();
    sa.name = name;
    sa.frame = numframes;
    sa.range = animframes;
    return skelanims.back();
}

skelmodel::skeleton::skeleton(skelmeshgroup * const group) :
    numbones(0),
    numinterpbones(0),
    numgpubones(0),
    numframes(0),
    framebones(nullptr),
    ragdoll(nullptr),
    usegpuskel(false),
    owner(group),
    bones(nullptr)
{
}

skelmodel::skeleton::~skeleton()
{
    delete[] bones;
    delete[] framebones;
    if(ragdoll)
    {
        delete ragdoll;
        ragdoll = nullptr;
    }
    for(skelcacheentry &i : skelcache)
    {
        delete[] i.bdata;
    }
}

std::optional<int> skelmodel::skeleton::findbone(const std::string &name) const
{
    for(size_t i = 0; i < numbones; ++i)
    {
        if(bones[i].name && !std::strcmp(bones[i].name, name.c_str()))
        {
            return i;
        }
    }
    return std::nullopt;
}

int skelmodel::skeleton::findtag(std::string_view name) const
{
    for(uint i = 0; i < tags.size(); i++)
    {
        if(!std::strcmp(tags[i].name.c_str(), name.data()))
        {
            return i;
        }
    }
    return -1;
}

bool skelmodel::skeleton::addtag(std::string_view name, int bone, const matrix4x3 &matrix)
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
        tags.emplace_back();
        tag &t = tags.back();
        t.name.append(name);
        t.bone = bone;
        t.matrix = matrix;
    }
    return true;
}

void skelmodel::skeleton::calcantipodes()
{
    antipodes.clear();
    std::vector<uint> schedule;
    for(size_t i = 0; i < numbones; ++i)
    {
        if(bones[i].group >= static_cast<int>(numbones))
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
        uint bone = schedule[i];
        const boneinfo &info = bones[bone];
        for(size_t j = 0; j < numbones; ++j)
        {
            if(std::abs(bones[j].group) == bone && bones[j].scheduled < 0)
            {
                antipodes.emplace_back(antipode(info.interpindex, bones[j].interpindex));
                bones[j].scheduled = schedule.size();
                schedule.push_back(j);
            }
        }
        if(i + 1 == schedule.size())
        {
            std::optional<int> conflict = std::nullopt;
            for(size_t j = 0; j < numbones; ++j)
            {
                if(bones[j].group < static_cast<int>(numbones) && bones[j].scheduled < 0)
                {
                    conflict = std::min(conflict.value(), std::abs(bones[j].group));
                }
            }
            if(conflict)
            {
                bones[conflict.value()].scheduled = schedule.size();
                schedule.push_back(conflict.value());
            }
        }
    }
}

void skelmodel::skeleton::remapbones()
{
    for(size_t i = 0; i < numbones; ++i)//loop i
    {
        boneinfo &info = bones[i];
        info.interpindex = -1;
        info.ragdollindex = -1;
    }
    numgpubones = 0;
    for(blendcombo &c : owner->blendcombos)
    {
        for(size_t k = 0; k < c.size(); ++k) //loop k
        {
            if(!c.bonedata[k].weight)
            {
                c.setinterpbones(k > 0 ? c.bonedata[k-1].interpbone : 0, k);
                continue;
            }
            boneinfo &info = bones[c.getbone(k)];
            if(info.interpindex < 0)
            {
                info.interpindex = numgpubones++;
            }
            c.setinterpbones(info.interpindex, k);
            if(info.group < 0)
            {
                continue;
            }
            for(size_t l = 0; l < c.size(); ++l) //note this is a loop l (level 4)
            {
                if(l == k)
                {
                    continue;
                }
                int parent = c.getbone(l);
                if(info.parent == parent || (info.parent >= 0 && info.parent == bones[parent].parent))
                {
                    info.group = -info.parent;
                    break;
                }
                if(info.group <= parent)
                {
                    continue;
                }
                int child = c.getbone(k);
                while(parent > child)
                {
                    parent = bones[parent].parent;
                }
                if(parent != child)
                {
                    info.group = c.getbone(l);
                }
            }
        }
    }
    numinterpbones = numgpubones;
    for(const tag &i : tags)
    {
        boneinfo &info = bones[i.bone];
        if(info.interpindex < 0)
        {
            info.interpindex = numinterpbones++;
        }
    }
    if(ragdoll)
    {
        for(uint i = 0; i < ragdoll->joints.size(); i++)
        {
            boneinfo &info = bones[ragdoll->joints[i].bone];
            if(info.interpindex < 0)
            {
                info.interpindex = numinterpbones++;
            }
            info.ragdollindex = i;
        }
    }
    for(size_t i = 0; i < numbones; ++i)
    {
        boneinfo &info = bones[i];
        if(info.interpindex < 0)
        {
            continue;
        }
        for(int parent = info.parent; parent >= 0 && bones[parent].interpindex < 0; parent = bones[parent].parent)
        {
            bones[parent].interpindex = numinterpbones++;
        }
    }
    for(size_t i = 0; i < numbones; ++i)
    {
        boneinfo &info = bones[i];
        if(info.interpindex < 0)
        {
            continue;
        }
        info.interpparent = info.parent >= 0 ? bones[info.parent].interpindex : -1;
    }
    if(ragdoll)
    {
        for(size_t i = 0; i < numbones; ++i)
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
        uint pos = pitchdeps.size();
        for(uint j = 0; j < pitchdeps.size(); j++)
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
            pitchdeps.insert(pitchdeps.begin() + pos, d);
        }
    nextbone:;
    }
}

int skelmodel::skeleton::findpitchdep(int bone) const
{
    for(uint i = 0; i < pitchdeps.size(); i++)
    {
        if(bone <= pitchdeps[i].bone)
        {
            return bone == pitchdeps[i].bone ? i : -1;
        }
    }
    return -1;
}

int skelmodel::skeleton::findpitchcorrect(int bone) const
{
    for(uint i = 0; i < pitchcorrects.size(); i++)
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
    pitchdeps.clear();
    if(pitchtargets.empty())
    {
        return;
    }
    for(pitchtarget &t : pitchtargets)
    {
        t.deps = -1;
        addpitchdep(t.bone, t.frame);
    }
    for(pitchdep &d : pitchdeps)
    {
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
    for(pitchtarget &t : pitchtargets)
    {
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
    for(uint i = 0; i < pitchcorrects.size(); i++)
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

void skelmodel::skeleton::expandbonemask(uchar *expansion, int bone, int val) const
{
    expansion[bone] = val;
    bone = bones[bone].children;
    while(bone>=0)
    {
        expandbonemask(expansion, bone, val);
        bone = bones[bone].next;
    }
}

void skelmodel::skeleton::applybonemask(const std::vector<uint> &mask, std::vector<uchar> &partmask, int partindex) const
{
    if(mask.empty() || mask.front()==Bonemask_End)
    {
        return;
    }
    uchar *expansion = new uchar[numbones];
    std::memset(expansion, mask.front()&Bonemask_Not ? 1 : 0, numbones);
    for(const uint &i : mask)
    {
        if(i == Bonemask_End)
        {
            break;
        }
        expandbonemask(expansion, i&Bonemask_Bone, i&Bonemask_Not ? 0 : 1);
    }
    for(size_t i = 0; i < numbones; ++i)
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
    for(size_t i = 0; i < numbones; ++i)
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
    return std::min(maxvsuniforms, maxskelanimdata) / 2;
}

bool skelmodel::skeleton::gpuaccelerate() const
{
    return numframes && gpuskel && numgpubones<=availgpubones();
}

float skelmodel::skeleton::calcdeviation(const vec &axis, const vec &forward, const dualquat &pose1, const dualquat &pose2) const
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
    return atan2f(dy, dx)*RAD;
}

void skelmodel::skeleton::calcpitchcorrects(float pitch, const vec &axis, const vec &forward)
{
    for(pitchtarget& t : pitchtargets)
    {
        t.deviated = calcdeviation(axis, forward, t.pose, pitchdeps[t.deps].pose);
    }
    for(pitchcorrect& c : pitchcorrects)
    {
        c.pitchangle = c.pitchtotal = 0;
    }
    for(uint j = 0; j < pitchtargets.size(); j++)
    {
        pitchtarget &t = pitchtargets[j];
        float tpitch = pitch - t.deviated;
        for(int parent = t.corrects; parent >= 0; parent = pitchcorrects[parent].parent)
        {
            tpitch -= pitchcorrects[parent].pitchangle;
        }
        if(t.pitchmin || t.pitchmax)
        {
            tpitch = std::clamp(tpitch, t.pitchmin, t.pitchmax);
        }
        for(pitchcorrect& c : pitchcorrects)
        {
            if(c.target != static_cast<int>(j))
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

//private helper function for interpbones
dualquat skelmodel::skeleton::interpbone(int bone, const std::array<framedata, maxanimparts> &partframes, const AnimState *as, const uchar *partmask)
{
    const AnimState &s = as[partmask[bone]];
    const framedata &f = partframes[partmask[bone]];
    dualquat d;
    (d = f.fr1[bone]).mul((1-s.cur.t)*s.interp);
    d.accumulate(f.fr2[bone], s.cur.t*s.interp);
    if(s.interp<1)
    {
        d.accumulate(f.pfr1[bone], (1-s.prev.t)*(1-s.interp));
        d.accumulate(f.pfr2[bone], s.prev.t*(1-s.interp));
    }
    return d;
}

void skelmodel::skeleton::interpbones(const AnimState *as, float pitch, const vec &axis, const vec &forward, int numanimparts, const uchar *partmask, skelcacheentry &sc)
{
    if(!sc.bdata)
    {
        sc.bdata = new dualquat[numinterpbones];
    }
    sc.nextversion();
    std::array<framedata, maxanimparts> partframes;
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
    for(pitchdep &p : pitchdeps)
    {
        dualquat d = interpbone(p.bone, partframes, as, partmask);
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
    for(size_t i = 0; i < numbones; ++i)
    {
        if(bones[i].interpindex>=0)
        {
            dualquat d = interpbone(i, partframes, as, partmask);
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
            sc.bdata[b.interpindex].mulorient(quat(axis, angle/RAD), b.base);
        }
    }
    for(const antipode &i : antipodes)
    {
        sc.bdata[i.child].fixantipodal(sc.bdata[i.parent]);
    }
}

void skelmodel::skeleton::initragdoll(ragdolldata &d, const skelcacheentry &sc, const part * const p)
{
    const dualquat *bdata = sc.bdata;
    for(const ragdollskel::joint &j : ragdoll->joints)
    {
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
        for(uint i = 0; i < ragdoll->joints.size(); i++)
        {
            const ragdollskel::joint &j = ragdoll->joints[i];
            const boneinfo &b = bones[j.bone];
            const dualquat &q = bdata[b.interpindex];
            d.calcanimjoint(i, matrix4x3(q));
        }
    }
    for(uint i = 0; i < ragdoll->verts.size(); i++)
    {
        ragdolldata::vert &dv = d.verts[i];
        matrixstack.top().transform(vec(dv.pos).mul(p->model->locationsize().w), dv.pos);
    }
    for(uint i = 0; i < ragdoll->reljoints.size(); i++)
    {
        const ragdollskel::reljoint &r = ragdoll->reljoints[i];
        const ragdollskel::joint &j = ragdoll->joints[r.parent];
        const boneinfo &br = bones[r.bone], &bj = bones[j.bone];
        d.reljoints[i].mul(dualquat(bdata[bj.interpindex]).invert(), bdata[br.interpindex]);
    }
}

void skelmodel::skeleton::genragdollbones(const ragdolldata &d, skelcacheentry &sc, const part * const p)
{
    if(!sc.bdata)
    {
        sc.bdata = new dualquat[numinterpbones];
    }
    sc.nextversion();
    vec pmodeltranslate = vec(p->model->locationsize().x, p->model->locationsize().y, p->model->locationsize().z);
    vec trans = vec(d.center).div(p->model->locationsize().w).add(pmodeltranslate);
    for(uint i = 0; i < ragdoll->joints.size(); i++)
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
        pos.mul(j.weight/p->model->locationsize().w).sub(trans);
        matrix4x3 m;
        m.mul(d.tris[j.tri], pos, d.animjoints ? d.animjoints[i] : j.orient);
        sc.bdata[b.interpindex] = dualquat(m);
    }
    for(uint i = 0; i < ragdoll->reljoints.size(); i++)
    {
        const ragdollskel::reljoint &r = ragdoll->reljoints[i];
        const ragdollskel::joint &j = ragdoll->joints[r.parent];
        const boneinfo &br = bones[r.bone], &bj = bones[j.bone];
        sc.bdata[br.interpindex].mul(sc.bdata[bj.interpindex], d.reljoints[i]);
    }
    for(const antipode &i : antipodes)
    {
        sc.bdata[i.child].fixantipodal(sc.bdata[i.parent]);
    }
}

void skelmodel::skeleton::concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const
{
    matrix4x3 t;
    t.mul(bones[tags[i].bone].base, tags[i].matrix);
    n.mul(m, t);
}

void skelmodel::skeleton::calctags(part *p, const skelcacheentry *sc) const
{
    for(part::linkedpart &l : p->links)
    {
        const tag &t = tags[l.tag];
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
        m.d.mul(p->model->locationsize().w * sizescale);
        l.matrix = m;
    }
}

void skelmodel::skeleton::cleanup(bool full)
{
    for(skelcacheentry &sc : skelcache)
    {
        for(int j = 0; j < maxanimparts; ++j)
        {
            sc.as[j].cur.fr1 = -1;
        }
        delete[] sc.bdata;
        sc.bdata = nullptr;
    }
    skelcache.clear();
    blendoffsets.clear();
    if(full)
    {
        owner->cleanup();
    }
}

bool skelmodel::skeleton::canpreload() const
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

const skelmodel::skelcacheentry &skelmodel::skeleton::checkskelcache(const part * const p, const AnimState *as, float pitch, const vec &axis, const vec &forward, const ragdolldata * const rdata)
{
    if(skelcache.empty())
    {
        usegpuskel = gpuaccelerate();
    }
    const int numanimparts = (reinterpret_cast<const skelpart *>(as->owner))->numanimparts;
    const std::vector<uchar> &partmask = (reinterpret_cast<const skelpart *>(as->owner))->partmask;
    skelcacheentry *sc = nullptr;
    bool match = false;
    for(skelcacheentry &c : skelcache)
    {
        for(int j = 0; j < numanimparts; ++j)
        {
            if(c.as[j]!=as[j])
            {
                goto mismatch;
            }
        }
        if(c.pitch != pitch || *c.partmask != partmask || c.ragdoll != rdata || (rdata && c.millis < rdata->lastmove))
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
        skelcache.emplace_back(skelcacheentry());
        sc = &skelcache.back();
    }
    if(!match)
    {
        for(int i = 0; i < numanimparts; ++i)
        {
            sc->as[i] = as[i];
        }
        sc->pitch = pitch;
        sc->partmask = &partmask;
        sc->ragdoll = rdata;
        if(rdata)
        {
            genragdollbones(*rdata, *sc, p);
        }
        else
        {
            interpbones(as, pitch, axis, forward, numanimparts, partmask.data(), *sc);
        }
    }
    sc->millis = lastmillis;
    return *sc;
}

int skelmodel::skeleton::getblendoffset(const UniformLoc &u)
{
    auto itr = blendoffsets.find(Shader::lastshader->program);
    if(itr == blendoffsets.end())
    {
        itr = blendoffsets.insert( { Shader::lastshader->program, -1 } ).first;
        DEF_FORMAT_STRING(offsetname, "%s[%d]", u.name, 2*numgpubones);
        (*itr).second = glGetUniformLocation(Shader::lastshader->program, offsetname);
    }
    return (*itr).second;
}

void skelmodel::skeleton::setglslbones(UniformLoc &u, const skelcacheentry &sc, const skelcacheentry &bc, int count)
{
    if(u.version == bc.version && u.data == bc.bdata)
    {
        return;
    }
    glUniform4fv(u.loc, 2*numgpubones, sc.bdata[0].real.v);
    if(count > 0)
    {
        int offset = getblendoffset(u);
        if(offset >= 0)
        {
            glUniform4fv(offset, 2*count, bc.bdata[0].real.v);
        }
    }
    u.version = bc.version;
    u.data = bc.bdata;
}

void skelmodel::skeleton::setgpubones(const skelcacheentry &sc, blendcacheentry *bc, int count)
{
    if(!Shader::lastshader)
    {
        return;
    }
    if(Shader::lastshader->uniformlocs.size() < 1)
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

bool skelmodel::skeleton::setbonepitch(size_t index, float scale, float offset, float min, float max)
{
    if(index > numbones)
    {
        return false;
    }
    boneinfo &b = bones[index];
    b.pitchscale = scale;
    b.pitchoffset = offset;
    b.pitchmin = offset;
    b.pitchmax = offset;
    return true;
}

std::optional<dualquat> skelmodel::skeleton::getbonebase(size_t index) const
{
    if(index > numbones)
    {
        return std::nullopt;
    }
    return bones[index].base;
}

bool skelmodel::skeleton::setbonebases(const std::vector<dualquat> &bases)
{
    if(bases.size() != numbones)
    {
        return false;
    }
    for(size_t i = 0; i < numbones; ++i)
    {
        bones[i].base = bases[i];
    }
    return true;
}

bool skelmodel::skeleton::setbonename(size_t index, std::string_view name)
{
    if(index > numbones)
    {
        return false;
    }
    boneinfo &b = bones[index];
    if(!b.name)
    {
        b.name = newstring(name.data());
        return true;
    }
    return false;
}

bool skelmodel::skeleton::setboneparent(size_t index, size_t parent)
{
    if(index > numbones || parent > numbones)
    {
        return false;
    }
    boneinfo &b = bones[index];
    b.parent = parent;
    return true;
}

void skelmodel::skeleton::createbones(size_t num)
{
    numbones = num;
    bones = new boneinfo[numbones];
}

skelmodel::skelmeshgroup::~skelmeshgroup()
{
    if(skel)
    {
        delete skel;
        skel = nullptr;
    }
    if(ebuf)
    {
        glDeleteBuffers(1, &ebuf);
    }
    for(int i = 0; i < maxblendcache; ++i)
    {
        delete[] blendcache[i].bdata;
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

void skelmodel::skelmeshgroup::genvbo(vbocacheentry &vc)
{
    if(!vc.vbuf)
    {
        glGenBuffers(1, &vc.vbuf);
    }
    if(ebuf)
    {
        return;
    }

    std::vector<GLuint> idxs;

    vlen = 0;
    vblends = 0;
    if(skel->numframes && !skel->usegpuskel)
    {
        vweights = 1;
        for(blendcombo &c : blendcombos)
        {
            c.setinterpindex(skel->numgpubones + vblends);
            vblends++;
        }

        vertsize = sizeof(vvert);
        auto rendermeshes = getrendermeshes();
        for(auto i : rendermeshes)
        {
            vlen += static_cast<skelmesh *>(*i)->genvbo(blendcombos, idxs, vlen);
        }
        delete[] vdata;
        vdata = new uchar[vlen*vertsize];
        for(auto i : rendermeshes)
        {
             static_cast<skelmesh *>(*i)->fillverts(reinterpret_cast<vvert *>(vdata));
        }
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
            for(blendcombo &c : blendcombos)
            {
                c.interpindex = static_cast<int>(c.size()) > vweights ? skel->numgpubones + vblends++ : -1;
            }
        }
        else
        {
            vweights = 0;
            for(blendcombo &i : blendcombos)
            {
                i.interpindex = -1;
            }
        }

        gle::bindvbo(vc.vbuf);
        auto rendermeshes = getrendermeshes();
        if(skel->numframes)
        {
            vertsize = sizeof(vvertgw);//silent parameter to genvbo()
            std::vector<vvertgw> vvertgws;
            for(const auto &i : rendermeshes)
            {
                vlen += static_cast<skelmesh *>(*i)->genvbo(blendcombos, idxs, vlen, vvertgws);
            }
            glBufferData(GL_ARRAY_BUFFER, vvertgws.size()*sizeof(vvertgw), vvertgws.data(), GL_STATIC_DRAW);
        }
        else
        {
            int numverts = 0,
                htlen = 128;
            for(auto i : rendermeshes)
            {
                numverts += static_cast<skelmesh *>(*i)->numverts;
            }
            while(htlen < numverts)
            {
                htlen *= 2;
            }
            if(numverts*4 > htlen*3)
            {
                htlen *= 2;
            }
            int *htdata = new int[htlen];
            std::memset(htdata, -1, htlen*sizeof(int));
            vertsize = sizeof(vvertg); //silent parameter to genvbo()
            std::vector<vvertg> vvertgs;
            for(const auto &i : rendermeshes)
            {
                vlen += static_cast<skelmesh *>(*i)->genvbo(idxs, vlen, vvertgs, htdata, htlen);
            }
            glBufferData(GL_ARRAY_BUFFER, vvertgs.size()*sizeof(vvertg), vvertgs.data(), GL_STATIC_DRAW);
            delete[] htdata;
        }
        gle::clearvbo();
    }

    glGenBuffers(1, &ebuf);
    gle::bindebo(ebuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxs.size()*sizeof(GLuint), idxs.data(), GL_STATIC_DRAW);
    gle::clearebo();
}

void skelmodel::skelmeshgroup::render(const AnimState *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p)
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
                p->skins[i].bind(m, as, skel->usegpuskel, vweights);
                m.render();
            });
        }
        skel->calctags(p);
        return;
    }

    const skelcacheentry &sc = skel->checkskelcache(p, as, pitch, axis, forward, !d || !d->ragdoll || d->ragdoll->skel != skel->ragdoll || d->ragdoll->millis == lastmillis ? nullptr : d->ragdoll);
    if(!(as->cur.anim & Anim_NoRender))
    {
        int owner = &sc-&skel->skelcache[0];
        vbocacheentry &vc = skel->usegpuskel ? *vbocache : checkvbocache(sc, owner);
        vc.millis = lastmillis;
        if(!vc.vbuf)
        {
            genvbo(vc);
        }
        blendcacheentry *bc = nullptr;
        if(vblends)
        {
            bc = &checkblendcache(sc, owner);
            bc->millis = lastmillis;
            if(bc->owner!=owner)
            {
                bc->owner = owner;
                *reinterpret_cast<animcacheentry *>(bc) = sc;
                blendbones(sc, *bc);
            }
        }
        if(!skel->usegpuskel && vc.owner != owner)
        {
            vc.owner = owner;
            static_cast<animcacheentry &>(vc) = sc;
            LOOP_RENDER_MESHES(skelmesh, m,
            {
                m.interpverts(skel->numgpubones, sc.bdata, bc ? bc->bdata : nullptr, reinterpret_cast<vvert *>(vdata), p->skins[i]);
            });
            gle::bindvbo(vc.vbuf);
            glBufferData(GL_ARRAY_BUFFER, vlen*vertsize, vdata, GL_STREAM_DRAW);
        }

        bindvbo(as, p, vc, &sc);

        LOOP_RENDER_MESHES(skelmesh, m,
        {
            p->skins[i].bind(m, as, skel->usegpuskel, vweights);
            if(skel->usegpuskel)
            {
                skel->setgpubones(sc, bc, vblends);
            }
            m.render();
        });
    }

    skel->calctags(p, &sc);

    if(as->cur.anim & Anim_Ragdoll && skel->ragdoll && !d->ragdoll)
    {
        d->ragdoll = new ragdolldata(skel->ragdoll, p->model->locationsize().w);
        skel->initragdoll(*d->ragdoll, sc, p);
        d->ragdoll->init(d);
    }
}

void skelmodel::skelmeshgroup::bindbones(const vvertgw *vverts)
{
    meshgroup::bindbones(vverts->weights, vverts->bones, vertsize);
}

//blendcombo

skelmodel::blendcombo::blendcombo() : uses(1)
{
    bonedata.fill({0,0,0});
}

bool skelmodel::blendcombo::operator==(const blendcombo &c) const
{
    for(size_t i = 0; i < bonedata.size(); ++i)
    {
        if((bonedata[i].bone != c.bonedata[i].bone) ||
           (bonedata[i].weight != c.bonedata[i].weight))
        {
            return false;
        }
    }
    return true;
}

size_t skelmodel::blendcombo::size() const
{
    size_t i = 1;
    while(i < bonedata.size() && bonedata[i].weight)
    {
        i++;
    }
    return i;
}

bool skelmodel::blendcombo::sortcmp(const blendcombo &x, const blendcombo &y)
{
    for(size_t i = 0; i < x.bonedata.size(); ++i)
    {
        if(x.bonedata[i].weight)
        {
            if(!y.bonedata[i].weight)
            {
                return true;
            }
        }
        else if(y.bonedata[i].weight)
        {
            return false;
        }
        else
        {
            break;
        }
    }
    return false;
}

int skelmodel::blendcombo::addweight(int sorted, float weight, int bone)
{
    if(weight <= 1e-3f) //do not add trivially small weights
    {
        return sorted;
    }
    for(int k = 0; k < sorted; ++k)
    {
        if(weight > bonedata[k].weight)
        {
            //push weights in bonedata to make room for new larger weights
            for(int l = std::min(sorted-1, 2); l >= k; l--)
            {
                bonedata[l+1].weight = bonedata[l].weight;
                bonedata[l+1].bone = bonedata[l].bone;
            }
            bonedata[k].weight = weight;
            bonedata[k].bone = bone;
            return sorted < static_cast<int>(bonedata.size()) ? sorted+1 : sorted;
        }
    }
    if(sorted >= static_cast<int>(bonedata.size()))
    {
        return sorted;
    }
    bonedata[sorted].weight = weight;
    bonedata[sorted].bone = bone;
    return sorted+1;
}

void skelmodel::blendcombo::finalize(int sorted)
{
    if(sorted <= 0)
    {
        return;
    }
    float total = 0;
    for(int i = 0; i < sorted; ++i)
    {
        total += bonedata[i].weight;
    }
    total = 1.0f/total;
    for(int i = 0; i < sorted; ++i)
    {
        bonedata[i].weight *= total;
    }
}

void skelmodel::blendcombo::serialize(skelmodel::vvertgw &v) const
{
    if(interpindex >= 0)
    {
        v.weights[0] = 255;
        for(int k = 0; k < 3; ++k)
        {
            v.weights[k+1] = 0;
        }
        v.bones[0] = 2*interpindex;
        for(int k = 0; k < 3; ++k)
        {
            v.bones[k+1] = v.bones[0];
        }
    }
    else
    {
        int total = 0;
        for(size_t k = 0; k < bonedata.size(); ++k)
        {
            v.weights[k] = static_cast<uchar>(0.5f + bonedata[k].weight*255);
            total += (v.weights[k]);
        }
        while(total > 255)
        {
            for(int k = 0; k < 4; ++k)
            {
                if(v.weights[k] > 0 && total > 255)
                {
                    v.weights[k]--;
                    total--;
                }
            }
        }
        while(total < 255)
        {
            for(int k = 0; k < 4; ++k)
            {
                if(v.weights[k] < 255 && total < 255)
                {
                    v.weights[k]++;
                    total++;
                }
            }
        }
        for(size_t k = 0; k < bonedata.size(); ++k)
        {
            v.bones[k] = 2*bonedata[k].interpbone;
        }
    }
}

dualquat skelmodel::blendcombo::blendbones(const dualquat *bdata) const
{
    dualquat d = bdata[bonedata[0].interpbone];
    d.mul(bonedata[0].weight);
    d.accumulate(bdata[bonedata[1].interpbone], bonedata[1].weight);
    if(bonedata[2].weight)
    {
        d.accumulate(bdata[bonedata[2].interpbone], bonedata[2].weight);
        if(bonedata[3].weight)
        {
            d.accumulate(bdata[bonedata[3].interpbone], bonedata[3].weight);
        }
    }
    return d;
}

int skelmodel::blendcombo::remapblend() const
{
    return bonedata[1].weight ? interpindex : bonedata[0].interpbone;
}

void skelmodel::blendcombo::setinterpindex(int val)
{
    interpindex = bonedata[1].weight ? val : -1;
}

void skelmodel::blendcombo::setinterpbones(int val, size_t index)
{
    bonedata[index].interpbone = val;
}

int skelmodel::blendcombo::getbone(size_t index)
{
    return bonedata[index].bone;
}

template<class T>
T &searchcache(size_t cachesize, T *cache, const skelmodel::skelcacheentry &sc, int owner)
{
    for(size_t i = 0; i < cachesize; ++i)
    {
        T &c = cache[i];
        if(c.owner==owner)
        {
             if(c==sc)
             {
                 return c;
             }
             else
             {
                 c.owner = -1;
             }
             break;
        }
    }
    for(size_t i = 0; i < cachesize-1; ++i)
    {
        T &c = cache[i];
        if(c.check() || c.owner < 0 || c.millis < lastmillis)
        {
            return c;
        }
    }
    return cache[cachesize-1];
}

skelmodel::vbocacheentry &skelmodel::skelmeshgroup::checkvbocache(const skelcacheentry &sc, int owner)
{
    return searchcache<vbocacheentry>(maxvbocache, vbocache, sc, owner);
}

skelmodel::blendcacheentry &skelmodel::skelmeshgroup::checkblendcache(const skelcacheentry &sc, int owner)
{
    return searchcache<blendcacheentry>(maxblendcache, blendcache, sc, owner);
}

//skelmesh

skelmodel::skelmesh::skelmesh() : verts(nullptr), tris(nullptr), numverts(0), numtris(0), maxweights(0)
{
}

skelmodel::skelmesh::skelmesh(std::string_view name, vert *verts, uint numverts, tri *tris, uint numtris) : Mesh(name),
    verts(verts),
    tris(tris),
    numverts(numverts),
    numtris(numtris),
    maxweights(0)
{
}

skelmodel::skelmesh::~skelmesh()
{
    delete[] verts;
    delete[] tris;
}

int skelmodel::skelmesh::addblendcombo(const blendcombo &c)
{
    maxweights = std::max(maxweights, static_cast<int>(c.size()));
    return (reinterpret_cast<skelmeshgroup *>(group))->addblendcombo(c);
}

void skelmodel::skelmesh::smoothnorms(float limit, bool areaweight)
{
    Mesh::smoothnorms<skelmodel>(verts, numverts, tris, numtris, limit, areaweight);
}

void skelmodel::skelmesh::buildnorms(bool areaweight)
{
    Mesh::buildnorms<skelmodel>(verts, numverts, tris, numtris, areaweight);
}

void skelmodel::skelmesh::calctangents(bool areaweight)
{
    Mesh::calctangents<skelmodel, skelmodel::vert>(verts, verts, numverts, tris, numtris, areaweight);
}

void skelmodel::skelmesh::calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) const
{
    for(int j = 0; j < numverts; ++j)
    {
        vec v = m.transform(verts[j].pos);
        bbmin.min(v);
        bbmax.max(v);
    }
}

void skelmodel::skelmesh::genBIH(BIH::mesh &m) const
{
    m.setmesh(reinterpret_cast<const BIH::mesh::tri *>(tris), numtris,
              reinterpret_cast<const uchar *>(&verts->pos), sizeof(vert),
              reinterpret_cast<const uchar *>(&verts->tc), sizeof(vert));
}

void skelmodel::skelmesh::genshadowmesh(std::vector<triangle> &out, const matrix4x3 &m) const
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

void skelmodel::skelmesh::assignvert(vvertg &vv, const vert &v)
{
    vv.pos = vec4<half>(v.pos, 1);
    vv.tc = v.tc;
    vv.tangent = v.tangent;
}

void skelmodel::skelmesh::assignvert(vvertgw &vv, const vert &v, const blendcombo &c)
{
    vv.pos = vec4<half>(v.pos, 1);
    vv.tc = v.tc;
    vv.tangent = v.tangent;
    c.serialize(vv);
}

int skelmodel::skelmesh::genvbo(const std::vector<blendcombo> &bcs, std::vector<GLuint> &idxs, int offset, std::vector<vvertgw> &vverts)
{
    voffset = offset;
    eoffset = idxs.size();
    for(int i = 0; i < numverts; ++i)
    {
        const vert &v = verts[i];
        vverts.emplace_back(vvertgw());
        assignvert(vverts.back(), v, bcs[v.blend]);
    }
    for(int i = 0; i < numtris; ++i)
    {
        for(int j = 0; j < 3; ++j)
        {
            idxs.push_back(voffset + tris[i].vert[j]);
        }
    }
    elen = idxs.size()-eoffset;
    minvert = voffset;
    maxvert = voffset + numverts-1;
    return numverts;
}

int skelmodel::skelmesh::genvbo(std::vector<GLuint> &idxs, int offset, std::vector<vvertg> &vverts, int *htdata, int htlen)
{
    voffset = offset;
    eoffset = idxs.size();
    minvert = 0xFFFF;
    for(int i = 0; i < numtris; ++i)
    {
        const tri &t = tris[i];
        for(int j = 0; j < 3; ++j)
        {
            const uint index = t.vert[j];
            const vert &v = verts[index];
            vvertg vv;
            assignvert(vv, v);
            const auto hashfn = std::hash<vec>();
            int htidx = hashfn(v.pos)&(htlen-1);
            for(int k = 0; k < htlen; ++k)
            {
                int &vidx = htdata[(htidx+k)&(htlen-1)];
                if(vidx < 0)
                {
                    vidx = idxs.emplace_back(static_cast<GLuint>(vverts.size()));
                    vverts.push_back(vv);
                    break;
                }
                else if(!std::memcmp(&vverts[vidx], &vv, sizeof(vv)))
                {
                    minvert = std::min(minvert, idxs.emplace_back(static_cast<GLuint>(vidx)));
                    break;
                }
            }
        }
    }
    elen = idxs.size()-eoffset;
    minvert = std::min(minvert, static_cast<GLuint>(voffset));
    maxvert = std::max(minvert, static_cast<GLuint>(vverts.size()-1));
    return vverts.size()-voffset;
}

int skelmodel::skelmesh::genvbo(const std::vector<blendcombo> &bcs, std::vector<GLuint> &idxs, int offset)
{
    for(int i = 0; i < numverts; ++i)
    {
        verts[i].interpindex = bcs[verts[i].blend].remapblend();
    }

    voffset = offset;
    eoffset = idxs.size();
    for(int i = 0; i < numtris; ++i)
    {
        const tri &t = tris[i];
        for(int j = 0; j < 3; ++j)
        {
            idxs.emplace_back(voffset+t.vert[j]);
        }
    }
    minvert = voffset;
    maxvert = voffset + numverts-1;
    elen = idxs.size()-eoffset;
    return numverts;
}

void skelmodel::skelmesh::fillvert(vvert &vv, const vert &v)
{
    vv.tc = v.tc;
}

void skelmodel::skelmesh::fillverts(vvert *vdata)
{
    vdata += voffset;
    for(int i = 0; i < numverts; ++i)
    {
        fillvert(vdata[i], verts[i]);
    }
}

void skelmodel::skelmesh::interpverts(int numgpubones, const dualquat * RESTRICT bdata1, const dualquat * RESTRICT bdata2, vvert * RESTRICT vdata, skin &s)
{
    bdata2 -= numgpubones;
    vdata += voffset;
    for(int i = 0; i < numverts; ++i)
    {
        const vert &src = verts[i];
        vvert &dst = vdata[i];
        const dualquat &b = (src.interpindex < numgpubones ? bdata1 : bdata2)[src.interpindex];
        dst.pos = b.transform(src.pos);
        quat q = b.transform(src.tangent);
        fixqtangent(q, src.tangent.w);
        dst.tangent = q;
    }
}

void skelmodel::skelmesh::setshader(Shader *s, bool usegpuskel, int vweights, int row)
{
    if(row)
    {
        s->setvariant(usegpuskel ? std::min(maxweights, vweights) : 0, row);
    }
    else if(usegpuskel)
    {
        s->setvariant(std::min(maxweights, vweights)-1, 0);
    }
    else
    {
        s->set();
    }
}

void skelmodel::skelmesh::render()
{
    if(!Shader::lastshader)
    {
        return;
    }
    glDrawRangeElements(GL_TRIANGLES, minvert, maxvert, elen, GL_UNSIGNED_INT, &(static_cast<skelmeshgroup *>(group))->edata[eoffset]);
    glde++;
    xtravertsva += numverts;
}

// boneinfo

skelmodel::skeleton::boneinfo::boneinfo() :
    name(nullptr),
    parent(-1),
    children(-1),
    next(-1),
    group(INT_MAX),
    scheduled(-1),
    interpindex(-1),
    interpparent(-1),
    ragdollindex(-1),
    correctindex(-1),
    pitchscale(0),
    pitchoffset(0),
    pitchmin(0),
    pitchmax(0)
{
}

skelmodel::skeleton::boneinfo::~boneinfo()
{
    delete[] name;
}

// skelmeshgroup

int skelmodel::skelmeshgroup::findtag(std::string_view name)
{
    return skel->findtag(name);
}

void *skelmodel::skelmeshgroup::animkey()
{
    return skel;
}

int skelmodel::skelmeshgroup::totalframes() const
{
    return std::max(skel->numframes, 1);
}

void skelmodel::skelmeshgroup::bindvbo(const AnimState *as, const part *p, const vbocacheentry &vc, const skelcacheentry *sc)
{
    if(!skel->numframes)
    {
        bindvbo<vvertg>(as, p, vc);
    }
    else if(skel->usegpuskel)
    {
        bindvbo<vvertgw>(as, p, vc);
    }
    else
    {
        bindvbo<vvert>(as, p, vc);
    }
}

void skelmodel::skelmeshgroup::concattagtransform(int i, const matrix4x3 &m, matrix4x3 &n) const
{
    skel->concattagtransform(i, m, n);
}

int skelmodel::skelmeshgroup::addblendcombo(const blendcombo &c)
{
    for(uint i = 0; i < blendcombos.size(); i++)
    {
        if(blendcombos[i]==c)
        {
            blendcombos[i].uses += c.uses;
            return i;
        }
    }
    numblends[c.size()-1]++;
    blendcombos.push_back(c);
    blendcombo &a = blendcombos.back();
    a.interpindex = blendcombos.size()-1;
    return a.interpindex;
}

void skelmodel::skelmeshgroup::sortblendcombos()
{
    std::sort(blendcombos.begin(), blendcombos.end(), blendcombo::sortcmp);
    int *remap = new int[blendcombos.size()];
    for(uint i = 0; i < blendcombos.size(); i++)
    {
        remap[blendcombos[i].interpindex] = i;
    }
    auto rendermeshes = getrendermeshes();
    for(auto i : rendermeshes)
    {
        for(int j = 0; j < static_cast<skelmesh *>(*i)->numverts; ++j)
        {
            vert &v = static_cast<skelmesh *>(*i)->verts[j];
            v.blend = remap[v.blend];
        }
    }
    delete[] remap;
}

void skelmodel::skelmeshgroup::blendbones(const skelcacheentry &sc, blendcacheentry &bc)
{
    bc.nextversion();
    if(!bc.bdata)
    {
        bc.bdata = new dualquat[vblends];
    }
    dualquat *dst = bc.bdata - skel->numgpubones;
    bool normalize = !skel->usegpuskel || vweights<=1;
    for(const blendcombo &c : blendcombos)
    {
        if(c.interpindex<0)
        {
            break;
        }
        dualquat &d = dst[c.interpindex];
        d = c.blendbones(sc.bdata);
        if(normalize)
        {
            d.normalize();
        }
    }
}

void skelmodel::skelmeshgroup::cleanup()
{
    for(int i = 0; i < maxblendcache; ++i)
    {
        blendcacheentry &c = blendcache[i];
        delete[] c.bdata;
        c.bdata = nullptr;
        c.owner = -1;
    }
    for(int i = 0; i < maxvbocache; ++i)
    {
        vbocacheentry &c = vbocache[i];
        if(c.vbuf)
        {
            glDeleteBuffers(1, &c.vbuf);
            c.vbuf = 0;
        }
        c.owner = -1;
    }
    if(ebuf)
    {
        glDeleteBuffers(1, &ebuf);
        ebuf = 0;
    }
    if(skel)
    {
        skel->cleanup(false);
    }
}

void skelmodel::skelmeshgroup::preload()
{
    if(!skel->canpreload())
    {
        return;
    }
    if(skel->shouldcleanup())
    {
        skel->cleanup();
    }
    skel->preload();
    if(!vbocache->vbuf)
    {
        genvbo(*vbocache);
    }
}

// skelpart

skelmodel::skelpart::skelpart(animmodel *model, int index) : part(model, index)
{
}

skelmodel::skelpart::~skelpart()
{
}

/**
 * @brief Manages caching of part masking data.
 *
 * Attempts to match the passed vector of uchar with one in the internal static vector.
 * If a matching entry is found, empties the passed vector returns the entry in the cache.
 *
 * @param o a vector of uchar to delete or insert into the internal cache
 * @return the passed value o, or the equivalent entry in the internal cache
 */
std::vector<uchar> &skelmodel::skelpart::sharepartmask(std::vector<uchar> &o)
{
    static std::vector<std::vector<uchar>> partmasks;
    for(std::vector<uchar> &p : partmasks)
    {
        if(p == o)
        {
            o.clear();
            return p;
        }
    }
    partmasks.push_back(o);
    o.clear();
    return partmasks.back();
}

std::vector<uchar> skelmodel::skelpart::newpartmask()
{
    return std::vector<uchar>((static_cast<skelmeshgroup *>(meshes))->skel->numbones, 0);

}

void skelmodel::skelpart::initanimparts()
{
    buildingpartmask = newpartmask();
}

bool skelmodel::skelpart::addanimpart(const std::vector<uint> &bonemask)
{
    if(buildingpartmask.empty() || numanimparts>=maxanimparts)
    {
        return false;
    }
    (static_cast<skelmeshgroup *>(meshes))->skel->applybonemask(bonemask, buildingpartmask, numanimparts);
    numanimparts++;
    return true;
}

void skelmodel::skelpart::endanimparts()
{
    if(buildingpartmask.size())
    {
        partmask = sharepartmask(buildingpartmask);
        buildingpartmask.clear();
    }

    (static_cast<skelmeshgroup *>(meshes))->skel->optimize();
}

void skelmodel::skelpart::loaded()
{
    endanimparts();
    part::loaded();
}

//skelmodel

skelmodel::skelmodel(std::string name) : animmodel(name)
{
}

skelmodel::skelpart &skelmodel::addpart()
{
    skelpart *p = new skelpart(this, parts.size());
    parts.push_back(p);
    return *p;
}

animmodel::meshgroup * skelmodel::loadmeshes(const char *name, float smooth)
{
    skelmeshgroup *group = newmeshes();
    group->skel = new skeleton(group);
    part &p = *parts.back();
    if(!group->load(name, smooth, p))
    {
        delete group;
        return nullptr;
    }
    return group;
}

animmodel::meshgroup * skelmodel::sharemeshes(const char *name, float smooth)
{
    if(meshgroups.find(name) == meshgroups.end())
    {
        meshgroup *group = loadmeshes(name, smooth);
        if(!group)
        {
            return nullptr;
        }
        meshgroups[group->groupname()] = group;
    }
    return meshgroups[name];
}

//skelmodel overrides

int skelmodel::linktype(const animmodel *m, const part *p) const
{
    return type()==m->type() &&
        (static_cast<skelmeshgroup *>(parts[0]->meshes))->skel == (static_cast<skelmeshgroup *>(p->meshes))->skel ?
            Link_Reuse :
            Link_Tag;
}

bool skelmodel::skeletal() const
{
    return true;
}

/*    ====    skeladjustment    ====    */
/*======================================*/

void skeladjustment::adjust(dualquat &dq) const
{
    if(yaw)
    {
        dq.mulorient(quat(vec(0, 0, 1), yaw/RAD));
    }
    if(pitch)
    {
        dq.mulorient(quat(vec(0, -1, 0), pitch/RAD));
    }
    if(roll)
    {
        dq.mulorient(quat(vec(-1, 0, 0), roll/RAD));
    }
    if(!translate.iszero())
    {
        dq.translate(translate);
    }
}

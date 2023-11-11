// world.cpp: core map management stuff

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"

#include <memory>
#include <optional>

#include "bih.h"
#include "entities.h"
#include "light.h"
#include "octaedit.h"
#include "octaworld.h"
#include "raycube.h"
#include "world.h"

#include "interface/console.h"
#include "interface/cs.h"
#include "interface/menus.h"

#include "model/model.h"

#include "render/octarender.h"
#include "render/renderlights.h"
#include "render/rendermodel.h"
#include "render/renderparticles.h"
#include "render/shaderparam.h"
#include "render/stain.h"
#include "render/texture.h"

SVARR(maptitle, "Untitled Map by Unknown");

std::vector<int> outsideents;
std::vector<int> entgroup;

namespace entities
{
    std::vector<extentity *> ents;

    std::vector<extentity *> &getents()
    {
        return ents;
    }

    const char *entmodel(const entity &e)
    {
        return nullptr;
    }

    extentity *newentity()
    {
        return new extentity();
    }

    void deleteentity(extentity *e)
    {
        delete e;
    }

    void clearents()
    {
        while(ents.size())
        {
            deleteentity(ents.back());
            ents.pop_back();
        }
    }

}
//octaents are the ones that modify the level directly: other entities like
//sounds, lights, spawns etc. don't get directly rendered
VAR(entselradius, 0, 2, 10);
VAR(octaentsize, 0, 64, 1024);

void entcancel()
{
    entgroup.clear();
}

//need a getter fxn because decalslot obj not exposed to the game
float getdecalslotdepth(const DecalSlot &s)
{
    return s.depth;
}

void detachentity(extentity &e)
{
    if(!e.attached)
    {
        return;
    }
    e.attached->attached = nullptr;
    e.attached = nullptr;
}

VAR(attachradius, 1, 100, 1000);

void attachentity(extentity &e)
{
    //don't attempt to attach invalid to attach ents
    switch(e.type)
    {
        case EngineEnt_Spotlight:
        {
            break;
        }
        default:
        {
            return;
        }
    }
    detachentity(e);

    std::vector<extentity *> &ents = entities::getents();
    int closest = -1;
    float closedist = 1e10f; //some arbitrary high value
    for(uint i = 0; i < ents.size(); i++)
    {
        extentity *a = ents[i];
        if(a->attached)
        {
            continue;
        }
        switch(e.type)
        {
            case EngineEnt_Spotlight:
            {
                //only attempt to attach to lights
                if(a->type!=EngineEnt_Light)
                {
                    continue;
                }
                break;
            }
            default:
            {
                continue;
            }
        }
        float dist = e.o.dist(a->o);
        if(dist < closedist)
        {
            closest = i;
            closedist = dist;
        }
    }
    if(closedist>attachradius)
    {
        return;
    }
    e.attached = ents[closest];
    ents[closest]->attached = &e;
}

//used in iengine
void attachentities()
{
    for(extentity *& i : entities::getents())
    {
        attachentity(*i);
    }
}

enum ModOctaEnt
{
    ModOctaEnt_Add      = 1<<0,
    ModOctaEnt_UpdateBB = 1<<1,
    ModOctaEnt_Changed  = 1<<2
};

static void rotatebb(vec &center, vec &radius, int yaw, int pitch, int roll)
{
    matrix3 orient;
    orient.identity();
    if(yaw)
    {
        orient.rotate_around_z(sincosmod360(yaw));
    }
    if(pitch)
    {
        orient.rotate_around_x(sincosmod360(pitch));
    }
    if(roll)
    {
        orient.rotate_around_y(sincosmod360(-roll));
    }
    center = orient.transform(center);
    radius = orient.abstransform(radius);
}

static void transformbb(const entity &e, vec &center, vec &radius)
{
    if(e.attr5 > 0)
    {
        float scale = e.attr5/100.0f;
        center.mul(scale);
        radius.mul(scale);
    }
    rotatebb(center, radius, e.attr2, e.attr3, e.attr4);
}

//used in iengine
void mmboundbox(const entity &e, model *m, vec &center, vec &radius)
{
    m->boundbox(center, radius);
    transformbb(e, center, radius);
}

static void mmcollisionbox(const entity &e, model *m, vec &center, vec &radius)
{
    m->collisionbox(center, radius);
    transformbb(e, center, radius);
}

static void decalboundbox(const entity &e, const DecalSlot &s, vec &center, vec &radius)
{
    float size = std::max(static_cast<float>(e.attr5), 1.0f);
    center = vec(0, s.depth * size/2, 0);
    radius = vec(size/2, s.depth * size/2, size/2);
    rotatebb(center, radius, e.attr2, e.attr3, e.attr4);
}

static bool modifyoctaent(int flags, int id)
{
    std::vector<extentity *> &ents = entities::getents();
    return (static_cast<int>(ents.size()) > id) && ::rootworld.modifyoctaent(flags, id, *ents[id]);
}

static void removeentity(int id)
{
    modifyoctaent(ModOctaEnt_UpdateBB, id);
}

void freeoctaentities(cube &c)
{
    if(!c.ext)
    {
        return;
    }
    if(entities::getents().size())
    {
        while(c.ext->ents && !c.ext->ents->mapmodels.empty())
        {
            int id = c.ext->ents->mapmodels.back();
            c.ext->ents->mapmodels.pop_back();
            removeentity(id);
        }
        while(c.ext->ents && !c.ext->ents->decals.empty())
        {
            int id = c.ext->ents->decals.back();
            c.ext->ents->decals.pop_back();
            removeentity(id);
        }
        while(c.ext->ents && !c.ext->ents->other.empty())
        {
            int id = c.ext->ents->other.back();
            c.ext->ents->other.pop_back();
            removeentity(id);
        }
    }
    if(c.ext->ents)
    {
        delete c.ext->ents;
        c.ext->ents = nullptr;
    }
}

static bool getentboundingbox(const extentity &e, ivec &o, ivec &r)
{
    switch(e.type)
    {
        case EngineEnt_Empty:
        {
            return false;
        }
        case EngineEnt_Decal:
        {
            const DecalSlot &s = lookupdecalslot(e.attr1, false);
            vec center, radius;
            decalboundbox(e, s, center, radius);
            center.add(e.o);
            radius.max(entselradius);
            o = ivec(vec(center).sub(radius));
            r = ivec(vec(center).add(radius).add(1));
            break;
        }
        case EngineEnt_Mapmodel:
        {
            if(model *m = loadmapmodel(e.attr1))
            {
                vec center, radius;
                mmboundbox(e, m, center, radius);
                center.add(e.o);
                radius.max(entselradius);
                o = ivec(vec(center).sub(radius));
                r = ivec(vec(center).add(radius).add(1));
            }
            break;
        }
        // invisible mapmodels use entselradius: lights sounds spawns etc.
        default:
        {
            o = ivec(vec(e.o).sub(entselradius));
            r = ivec(vec(e.o).add(entselradius+1));
            break;
        }
    }
    return true;
}

static void modifyoctaentity(int flags, int id, const extentity &e, std::array<cube, 8> &c, const ivec &cor, int size, const ivec &bo, const ivec &br, int leafsize, vtxarray *lastva = nullptr)
{
    LOOP_OCTA_BOX(cor, size, bo, br)
    {
        ivec o(i, cor, size);
        vtxarray *va = c[i].ext && c[i].ext->va ? c[i].ext->va : lastva;
        if(c[i].children != nullptr && size > leafsize)
        {
            modifyoctaentity(flags, id, e, *(c[i].children), o, size>>1, bo, br, leafsize, va);
        }
        else if(flags&ModOctaEnt_Add)
        {
            if(!c[i].ext || !c[i].ext->ents)
            {
                ext(c[i]).ents = new octaentities(o, size);
            }
            octaentities &oe = *c[i].ext->ents;
            switch(e.type)
            {
                case EngineEnt_Decal:
                {
                    if(va)
                    {
                        va->bbmin.x = -1;
                        if(oe.decals.empty())
                        {
                            va->decals.push_back(&oe);
                        }
                    }
                    oe.decals.push_back(id);
                    oe.bbmin.min(bo).max(oe.o);
                    oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
                    break;
                }
                case EngineEnt_Mapmodel:
                {
                    if(loadmapmodel(e.attr1))
                    {
                        if(va)
                        {
                            va->bbmin.x = -1;
                            if(oe.mapmodels.empty())
                            {
                                va->mapmodels.push_back(&oe);
                            }
                        }
                        oe.mapmodels.push_back(id);
                        oe.bbmin.min(bo).max(oe.o);
                        oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
                    }
                    break;
                }
                // invisible mapmodels: lights sounds spawns etc.
                default:
                {
                    oe.other.push_back(id);
                    break;
                }
            }
        }
        else if(c[i].ext && c[i].ext->ents)
        {
            octaentities &oe = *c[i].ext->ents;
            switch(e.type)
            {
                case EngineEnt_Decal:
                {
                    if(std::find(oe.decals.begin(), oe.decals.end(), id) != oe.decals.end())
                    {
                        oe.decals.erase(std::find(oe.decals.begin(), oe.decals.end(), id));
                    }
                    if(va)
                    {
                        va->bbmin.x = -1;
                        if(oe.decals.empty())
                        {
                            auto itr = std::find(va->decals.begin(), va->decals.end(), &oe);
                            if(itr != va->decals.end())
                            {
                                va->decals.erase(itr);
                            }
                        }
                    }
                    oe.bbmin = oe.bbmax = oe.o;
                    oe.bbmin.add(oe.size);
                    for(uint j = 0; j < oe.decals.size(); j++)
                    {
                        extentity &e = *entities::getents()[oe.decals[j]];
                        ivec eo, er;
                        if(getentboundingbox(e, eo, er))
                        {
                            oe.bbmin.min(eo);
                            oe.bbmax.max(er);
                        }
                    }
                    oe.bbmin.max(oe.o);
                    oe.bbmax.min(ivec(oe.o).add(oe.size));
                    break;
                }
                case EngineEnt_Mapmodel:
                {
                    if(loadmapmodel(e.attr1))
                    {
                        auto itr = std::find(oe.mapmodels.begin(), oe.mapmodels.end(), id);
                        if(itr != oe.mapmodels.end())
                        {
                            oe.mapmodels.erase(itr);
                        }
                        if(va)
                        {
                            va->bbmin.x = -1;
                            if(oe.mapmodels.empty())
                            {
                                if(std::find(va->mapmodels.begin(), va->mapmodels.end(), &oe) != va->mapmodels.end())
                                {
                                    va->mapmodels.erase(std::find(va->mapmodels.begin(), va->mapmodels.end(), &oe));
                                }
                            }
                        }
                        oe.bbmin = oe.bbmax = oe.o;
                        oe.bbmin.add(oe.size);
                        for(const int &j : oe.mapmodels)
                        {
                            extentity &e = *entities::getents()[j];
                            ivec eo, er;
                            if(getentboundingbox(e, eo, er))
                            {
                                oe.bbmin.min(eo);
                                oe.bbmax.max(er);
                            }
                        }
                        oe.bbmin.max(oe.o);
                        oe.bbmax.min(ivec(oe.o).add(oe.size));
                    }
                    break;
                }
                // invisible mapmodels: light sounds spawns etc.
                default:
                {
                    if(std::find(oe.other.begin(), oe.other.end(), id) != oe.other.end())
                    {
                        oe.other.erase(std::find(oe.other.begin(), oe.other.end(), id));
                    }
                    break;
                }
            }
            if(oe.mapmodels.empty() && oe.decals.empty() && oe.other.empty())
            {
                freeoctaentities(c[i]);
            }
        }
        if(c[i].ext && c[i].ext->ents)
        {
            c[i].ext->ents->query = nullptr;
        }
        if(va && va!=lastva)
        {
            if(lastva)
            {
                if(va->bbmin.x < 0)
                {
                    lastva->bbmin.x = -1;
                }
            }
            else if(flags&ModOctaEnt_UpdateBB)
            {
                updatevabb(va);
            }
        }
    }
}

bool cubeworld::modifyoctaent(int flags, int id, extentity &e)
{
    if(flags&ModOctaEnt_Add ? e.flags&EntFlag_Octa : !(e.flags&EntFlag_Octa))
    {
        return false;
    }
    ivec o, r;
    if(!getentboundingbox(e, o, r))
    {
        return false;
    }
    if(!insideworld(e.o))
    {
        uint idx = std::distance(outsideents.begin(), std::find(outsideents.begin(), outsideents.end(), id));
        if(flags&ModOctaEnt_Add)
        {
            if(idx < outsideents.size())
            {
                outsideents.push_back(id);
            }
        }
        else if(idx >= 0)
        {
            outsideents.erase(outsideents.begin() + idx);
        }
    }
    else
    {
        int leafsize = octaentsize,
            limit    = std::max(r.x - o.x, std::max(r.y - o.y, r.z - o.z));
        while(leafsize < limit)
        {
            leafsize *= 2;
        }
        int diff = ~(leafsize-1) & ((o.x^r.x)|(o.y^r.y)|(o.z^r.z));
        if(diff && (limit > octaentsize/2 || diff < leafsize*2))
        {
            leafsize *= 2;
        }
        modifyoctaentity(flags, id, e, *worldroot, ivec(0, 0, 0), mapsize()>>1, o, r, leafsize);
    }
    e.flags ^= EntFlag_Octa;
    switch(e.type)
    {
        case EngineEnt_Light:
        {
            clearlightcache(id);
            if(e.attr5&LightEnt_Volumetric)
            {
                if(flags&ModOctaEnt_Add)
                {
                    volumetriclights++;
                }
                else
                {
                    --volumetriclights;
                }
            }
            if(e.attr5&LightEnt_NoSpecular)
            {
                if(!(flags&ModOctaEnt_Add ? nospeclights++ : --nospeclights))
                {
                    cleardeferredlightshaders();
                }
            }
            break;
        }
        case EngineEnt_Spotlight:
        {
            if(!(flags&ModOctaEnt_Add ? spotlights++ : --spotlights))
            {
                cleardeferredlightshaders();
                cleanupvolumetric();
            }
            break;
        }
        case EngineEnt_Particles:
        {
            clearparticleemitters();
            break;
        }
        case EngineEnt_Decal:
        {
            if(flags&ModOctaEnt_Changed)
            {
                changed(o, r, false);
            }
            break;
        }
    }
    return true;
}

void addentityedit(int id)
{
    modifyoctaent(ModOctaEnt_Add|ModOctaEnt_UpdateBB|ModOctaEnt_Changed, id);
}

void removeentityedit(int id)
{
    modifyoctaent(ModOctaEnt_UpdateBB|ModOctaEnt_Changed, id);
}

void cubeworld::entitiesinoctanodes()
{
    std::vector<extentity *> &ents = entities::getents();
    for(uint i = 0; i < ents.size(); i++)
    {
        modifyoctaent(ModOctaEnt_Add, i, *ents[i]);
    }
}

void entselectionbox(const entity &e, vec &eo, vec &es)
{
    model *m = nullptr;
    const char *mname = entities::entmodel(e);
    if(mname && (m = loadmodel(mname)))
    {
        m->collisionbox(eo, es);
        if(es.x > es.y)
        {
            es.y = es.x;
        }
        else
        {
            es.x = es.y; // square
        }
        es.z = (es.z + eo.z + 1 + entselradius)/2; // enclose ent radius box and model box
        eo.x += e.o.x;
        eo.y += e.o.y;
        eo.z = e.o.z - entselradius + es.z;
    }
    else if(e.type == EngineEnt_Mapmodel && (m = loadmapmodel(e.attr1)))
    {
        mmcollisionbox(e, m, eo, es);
        es.max(entselradius);
        eo.add(e.o);
    }
    else if(e.type == EngineEnt_Decal)
    {
        DecalSlot &s = lookupdecalslot(e.attr1, false);
        decalboundbox(e, s, eo, es);
        es.max(entselradius);
        eo.add(e.o);
    }
    else
    {
        es = vec(entselradius);
        eo = e.o;
    }
    eo.sub(es);
    es.mul(2);
}

////////////////////////////// world size/octa /////////////////////////////////

static void splitocta(std::array<cube, 8> &c, int size)
{
    if(size <= 0x1000)
    {
        return;
    }
    for(int i = 0; i < 8; ++i)
    {
        if(!c[i].children)
        {
            c[i].children = newcubes(c[i].isempty() ? faceempty : facesolid);
        }
        splitocta(*(c[i].children), size>>1);
    }
}

void cubeworld::resetmap()
{
    clearoverrides();
    clearlights();
    clearslots();
    clearparticles();
    clearstains();
    clearsleep();
    cancelsel();
    pruneundos();
    clearmapcrc();

    entities::clearents();
    outsideents.clear();
    spotlights = 0;
    volumetriclights = 0;
    nospeclights = 0;
}

bool cubeworld::emptymap(int scale, bool force, bool usecfg)    // main empty world creation routine
{
    if(!force && !editmode)
    {
        conoutf(Console_Error, "newmap only allowed in edit mode");
        return false;
    }
    resetmap();
    worldscale = std::clamp(scale, 10, 16);
    setvar("emptymap", 1, true, false);
    texmru.clear();
    freeocta(worldroot);
    worldroot = newcubes(faceempty);
    for(int i = 0; i < 4; ++i)
    {
        setcubefaces((*worldroot)[i], facesolid);
    }
    if(mapsize() > 0x1000)
    {
        splitocta(*worldroot, mapsize()>>1);
    }
    clearmainmenu();
    if(usecfg)
    {
        identflags |= Idf_Overridden;
        execfile("config/default_map_settings.cfg", false);
        identflags &= ~Idf_Overridden;
    }
    allchanged(true);
    return true;
}

/* enlargemap: grows the map to an additional gridsize
 * bool force forces the growth even if not in edit mode
 *
 * expands the map by placing the old map in the corner nearest the origin and
 * adding 7 old-map sized cubes to create a new largest cube
 *
 * this moves the worldroot cube to the new parent cube of the old map
 */
bool cubeworld::enlargemap(bool force)
{
    worldscale++;
    std::array<cube, 8> *c = newcubes(faceempty);
    (*c)[0].children = worldroot;
    for(int i = 0; i < 3; ++i)
    {
        setcubefaces((*c)[i+1], facesolid);
    }
    worldroot = c;

    if(mapsize() > 0x1000)
    {
        splitocta(*worldroot, mapsize()>>1);
    }
    allchanged();
    return true;
}

/* isallempty: checks whether the cube, and all of its children, are empty
 *
 * returns true if cube is empty and has no children, OR
 * returns true if cube has children cubes and none of their children (recursively)
 * have children that are not empty
 *
 * returns false if any cube or child cube is not empty (has geometry)
 *
 */
static bool isallempty(const cube &c)
{
    if(!c.children)
    {
        return c.isempty();
    }
    for(int i = 0; i < 8; ++i)
    {
        if(!isallempty((*c.children)[i]))
        {
            return false;
        }
    }
    return true;
}

/* shrinkmap: attempts to reduce the mapsize by 1 (halves all linear dimensions)
 *
 * fails if the 7 octants not at the origin are not empty
 * on success, the new map will have its maximum gridsize reduced by 1
 */
void cubeworld::shrinkmap()
{
    if(noedit(true) || (nompedit && multiplayer))
    {
        multiplayerwarn();
        return;
    }
    if(mapsize() <= 1<<10) //do not allow maps smaller than 2^10 cubits
    {
        return;
    }
    int octant = -1;
    for(int i = 0; i < 8; ++i)
    {
        if(!isallempty((*worldroot)[i]))
        {
            if(octant >= 0)
            {
                return;
            }
            octant = i;
        }
    }
    if(octant < 0)
    {
        return;
    }
    if(!(*worldroot)[octant].children)
    {
        subdividecube((*worldroot)[octant], false, false);
    }
    std::array<cube, 8> *&root = (*worldroot)[octant].children; //change worldroot to cube 0
    (*worldroot)[octant].children = nullptr; //free the old largest cube
    freeocta(worldroot);
    worldroot = root;
    worldscale--;
    ivec offset(octant, ivec(0, 0, 0), mapsize());
    std::vector<extentity *> &ents = entities::getents();
    for(extentity * const i : ents)
    {
        i->o.sub(vec(offset));
    }
    allchanged();
    conoutf("shrunk map to size %d", worldscale);
}

int cubeworld::mapsize() const
{
    return 1<<worldscale;
}

int cubeworld::mapscale() const
{
    return worldscale;
}

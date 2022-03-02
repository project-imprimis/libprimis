// world.cpp: core map management stuff

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"

#include "bih.h"
#include "entities.h"
#include "light.h"
#include "octaedit.h"
#include "octaworld.h"
#include "raycube.h"
#include "worldio.h"
#include "world.h"

#include "interface/console.h"
#include "interface/cs.h"
#include "interface/menus.h"

#include "model/model.h"

#include "render/octarender.h"
#include "render/renderlights.h"
#include "render/rendermodel.h"
#include "render/renderparticles.h"
#include "render/stain.h"
#include "render/texture.h"

VARR(mapversion, 1, currentmapversion, 0);
VARNR(mapscale, worldscale, 1, 0, 0);
VARNR(mapsize, worldsize, 1, 0, 0);
SVARR(maptitle, "Untitled Map by Unknown");
VARNR(emptymap, _emptymap, 1, 0, 0);

vector<int> outsideents;
vector<int> entgroup;

namespace entities
{
    vector<extentity *> ents;

    vector<extentity *> &getents()
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
        while(ents.length())
        {
            deleteentity(ents.pop());
        }
    }

}
//octaents are the ones that modify the level directly: other entities like
//sounds, lights, spawns etc. don't get directly rendered
VAR(entselradius, 0, 2, 10);
VAR(octaentsize, 0, 64, 1024);

void entcancel()
{
    entgroup.shrink(0);
}

//need a getter fxn because decalslot obj not exposed to the game
float getdecalslotdepth(DecalSlot &s)
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

    vector<extentity *> &ents = entities::getents();
    int closest = -1;
    float closedist = 1e10f; //some arbitrary high value
    for(int i = 0; i < ents.length(); i++)
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

void attachentities()
{
    vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < ents.length(); i++)
    {
        attachentity(*ents[i]);
    }
}

enum
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

static void decalboundbox(const entity &e, DecalSlot &s, vec &center, vec &radius)
{
    float size = std::max(static_cast<float>(e.attr5), 1.0f);
    center = vec(0, s.depth * size/2, 0);
    radius = vec(size/2, s.depth * size/2, size/2);
    rotatebb(center, radius, e.attr2, e.attr3, e.attr4);
}

void freeoctaentities(cube &c);

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
            DecalSlot &s = lookupdecalslot(e.attr1, false);
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

void modifyoctaentity(int flags, int id, extentity &e, cube *c, const ivec &cor, int size, const ivec &bo, const ivec &br, int leafsize, vtxarray *lastva = nullptr)
{
    LOOP_OCTA_BOX(cor, size, bo, br)
    {
        ivec o(i, cor, size);
        vtxarray *va = c[i].ext && c[i].ext->va ? c[i].ext->va : lastva;
        if(c[i].children != nullptr && size > leafsize)
        {
            modifyoctaentity(flags, id, e, c[i].children, o, size>>1, bo, br, leafsize, va);
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
                            va->decals.add(&oe);
                        }
                    }
                    oe.decals.add(id);
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
                                va->mapmodels.add(&oe);
                            }
                        }
                        oe.mapmodels.add(id);
                        oe.bbmin.min(bo).max(oe.o);
                        oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
                    }
                    break;
                }
                // invisible mapmodels: lights sounds spawns etc.
                default:
                {
                    oe.other.add(id);
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
                    oe.decals.removeobj(id);
                    if(va)
                    {
                        va->bbmin.x = -1;
                        if(oe.decals.empty())
                        {
                            va->decals.removeobj(&oe);
                        }
                    }
                    oe.bbmin = oe.bbmax = oe.o;
                    oe.bbmin.add(oe.size);
                    for(int j = 0; j < oe.decals.length(); j++)
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
                        oe.mapmodels.removeobj(id);
                        if(va)
                        {
                            va->bbmin.x = -1;
                            if(oe.mapmodels.empty())
                            {
                                va->mapmodels.removeobj(&oe);
                            }
                        }
                        oe.bbmin = oe.bbmax = oe.o;
                        oe.bbmin.add(oe.size);
                        for(int j = 0; j < oe.mapmodels.length(); j++)
                        {
                            extentity &e = *entities::getents()[oe.mapmodels[j]];
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
                    oe.other.removeobj(id);
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
        int idx = outsideents.find(id);
        if(flags&ModOctaEnt_Add)
        {
            if(idx < 0)
            {
                outsideents.add(id);
            }
        }
        else if(idx >= 0)
        {
            outsideents.removeunordered(idx);
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
        modifyoctaentity(flags, id, e, worldroot, ivec(0, 0, 0), worldsize>>1, o, r, leafsize);
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

static bool modifyoctaent(int flags, int id)
{
    vector<extentity *> &ents = entities::getents();
    return ents.inrange(id) && ::rootworld.modifyoctaent(flags, id, *ents[id]);
}

void addentityedit(int id)
{
    modifyoctaent(ModOctaEnt_Add|ModOctaEnt_UpdateBB|ModOctaEnt_Changed, id);
}

static void removeentity(int id)
{
    modifyoctaent(ModOctaEnt_UpdateBB, id);
}

void removeentityedit(int id)
{
    modifyoctaent(ModOctaEnt_UpdateBB|ModOctaEnt_Changed, id);
}

void freeoctaentities(cube &c)
{
    if(!c.ext)
    {
        return;
    }
    if(entities::getents().length())
    {
        while(c.ext->ents && !c.ext->ents->mapmodels.empty())
        {
            removeentity(c.ext->ents->mapmodels.pop());
        }
        while(c.ext->ents && !c.ext->ents->decals.empty())
        {
            removeentity(c.ext->ents->decals.pop());
        }
        while(c.ext->ents && !c.ext->ents->other.empty())
        {
            removeentity(c.ext->ents->other.pop());
        }
    }
    if(c.ext->ents)
    {
        delete c.ext->ents;
        c.ext->ents = nullptr;
    }
}

void entitiesinoctanodes()
{
    vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < ents.length(); i++)
    {
        ::rootworld.modifyoctaent(ModOctaEnt_Add, i, *ents[i]);
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

void splitocta(cube *c, int size)
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
        splitocta(c[i].children, size>>1);
    }
}

void resetmap()
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
    outsideents.setsize(0);
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
    setvar("mapscale", scale<10 ? 10 : (scale>16 ? 16 : scale), true, false);
    setvar("mapsize", 1<<worldscale, true, false);
    setvar("emptymap", 1, true, false);
    texmru.clear();
    freeocta(worldroot);
    worldroot = newcubes(faceempty);
    for(int i = 0; i < 4; ++i)
    {
        setcubefaces(worldroot[i], facesolid);
    }
    if(worldsize > 0x1000)
    {
        splitocta(worldroot, worldsize>>1);
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
    if(!force && !editmode)
    {
        conoutf(Console_Error, "mapenlarge only allowed in edit mode");
        return false;
    }
    if(worldsize >= 1<<16)
    {
        return false;
    }
    worldscale++;
    worldsize *= 2;
    cube *c = newcubes(faceempty);
    c[0].children = worldroot;
    for(int i = 0; i < 3; ++i)
    {
        setcubefaces(c[i+1], facesolid);
    }
    worldroot = c;

    if(worldsize > 0x1000)
    {
        splitocta(worldroot, worldsize>>1);
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
static bool isallempty(cube &c)
{
    if(!c.children)
    {
        return c.isempty();
    }
    for(int i = 0; i < 8; ++i)
    {
        if(!isallempty(c.children[i]))
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
    if(worldsize <= 1<<10) //do not allow maps smaller than 2^10 cubits
    {
        return;
    }
    int octant = -1;
    for(int i = 0; i < 8; ++i)
    {
        if(!isallempty(worldroot[i]))
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
    if(!worldroot[octant].children)
    {
        subdividecube(worldroot[octant], false, false);
    }
    cube *root = worldroot[octant].children; //change worldroot to cube 0
    worldroot[octant].children = nullptr; //free the old largest cube
    freeocta(worldroot);
    worldroot = root;
    worldscale--;
    worldsize /= 2;
    ivec offset(octant, ivec(0, 0, 0), worldsize);
    vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < ents.length(); i++)
    {
        ents[i]->o.sub(vec(offset));
    }
    allchanged();
    conoutf("shrunk map to size %d", worldscale);
}

int getworldsize()
{
    return worldsize;
}

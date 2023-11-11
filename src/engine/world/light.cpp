/* light.cpp: world light interaction functions
 *
 * while renderlights in /render handles the deferred rendering of point lights
 * on the world, light.cpp handles how lights behave in the world
 *
 * includes sunlight variables (direction/color of no-parallax sun lighting)
 * world light entity packing for the renderer to use
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"

#include "light.h"
#include "octaworld.h"
#include "raycube.h"
#include "world.h"

#include "interface/console.h"
#include "interface/input.h"

#include "render/radiancehints.h"
#include "render/renderlights.h"
#include "render/octarender.h"
#include "render/shaderparam.h"
#include "render/texture.h"

CVAR1R(ambient, 0x191919);
FVARR(ambientscale, 0, 1, 16);

CVAR1R(skylight, 0);
FVARR(skylightscale, 0, 1, 16);

CVAR1FR(sunlight, 0,
{
    clearradiancehintscache();
    cleardeferredlightshaders();
    clearshadowcache();
});
FVARFR(sunlightscale, 0, 1, 16, clearradiancehintscache(););

vec sunlightdir(0, 0, 1);
void setsunlightdir();
FVARFR(sunlightyaw, 0, 0, 360, setsunlightdir());
FVARFR(sunlightpitch, -90, 90, 90, setsunlightdir());

void setsunlightdir()
{
    sunlightdir = vec(sunlightyaw/RAD, sunlightpitch/RAD);
    for(int k = 0; k < 3; ++k)
    {
        if(std::fabs(sunlightdir[k]) < 1e-5f)
        {
            sunlightdir[k] = 0;
        }
    }
    sunlightdir.normalize();
    clearradiancehintscache();
}

void brightencube(cube &c)
{
    if(!c.ext)
    {
        newcubeext(c, 0, false);
    }
    c.ext->surfaces.fill(surfaceinfo());
}

void setsurfaces(cube &c, std::array<surfaceinfo, 6> surfs, const vertinfo *verts, int numverts)
{
    if(!c.ext || c.ext->maxverts < numverts)
    {
        newcubeext(c, numverts, false);
    }
    std::copy(c.ext->surfaces.begin(), c.ext->surfaces.end(), surfs.begin());
    std::memcpy(c.ext->verts(), verts, numverts*sizeof(vertinfo));
}

void setsurface(cube &c, int orient, const surfaceinfo &src, const vertinfo *srcverts, int numsrcverts)
{
    int dstoffset = 0;
    if(!c.ext)
    {
        newcubeext(c, numsrcverts, true);
    }
    else
    {
        int numbefore = 0,
            beforeoffset = 0;
        for(int i = 0; i < orient; ++i)
        {
            const surfaceinfo &surf = c.ext->surfaces[i];
            int numverts = surf.totalverts();
            if(!numverts)
            {
                continue;
            }
            numbefore += numverts;
            beforeoffset = surf.verts + numverts;
        }
        int numafter = 0,
            afteroffset = c.ext->maxverts;
        for(int i = 5; i > orient; i--) //note reverse iteration
        {
            const surfaceinfo &surf = c.ext->surfaces[i];
            int numverts = surf.totalverts();
            if(!numverts)
            {
                continue;
            }
            numafter += numverts;
            afteroffset = surf.verts;
        }
        if(afteroffset - beforeoffset >= numsrcverts)
        {
            dstoffset = beforeoffset;
        }
        else
        {
            cubeext *ext = c.ext;
            if(numbefore + numsrcverts + numafter > c.ext->maxverts)
            {
                ext = growcubeext(c.ext, numbefore + numsrcverts + numafter);
                std::copy(ext->surfaces.begin(), ext->surfaces.end(), c.ext->surfaces.begin());
            }
            int offset = 0;
            if(numbefore == beforeoffset)
            {
                if(numbefore && c.ext != ext)
                {
                    std::memcpy(ext->verts(), c.ext->verts(), numbefore*sizeof(vertinfo));
                }
                offset = numbefore;
            }
            else
            {
                for(int i = 0; i < orient; ++i)
                {
                    surfaceinfo &surf = ext->surfaces[i];
                    int numverts = surf.totalverts();
                    if(!numverts)
                    {
                        continue;
                    }
                    std::memmove(ext->verts() + offset, c.ext->verts() + surf.verts, numverts*sizeof(vertinfo));
                    surf.verts = offset;
                    offset += numverts;
                }
            }
            dstoffset = offset;
            offset += numsrcverts;
            if(numafter && offset > afteroffset)
            {
                offset += numafter;
                for(int i = 5; i > orient; i--) //note reverse iteration
                {
                    surfaceinfo &surf = ext->surfaces[i];
                    int numverts = surf.totalverts();
                    if(!numverts)
                    {
                        continue;
                    }
                    offset -= numverts;
                    std::memmove(ext->verts() + offset, c.ext->verts() + surf.verts, numverts*sizeof(vertinfo));
                    surf.verts = offset;
                }
            }
            if(c.ext != ext)
            {
                setcubeext(c, ext);
            }
        }
    }
    surfaceinfo &dst = c.ext->surfaces[orient];
    dst = src;
    dst.verts = dstoffset;
    if(srcverts)
    {
        std::memcpy(c.ext->verts() + dstoffset, srcverts, numsrcverts*sizeof(vertinfo));
    }
}

bool PackNode::insert(ushort &tx, ushort &ty, ushort tw, ushort th)
{
    if((available < tw && available < th) || w < tw || h < th)
    {
        return false;
    }
    if(child1)
    {
        bool inserted = child1->insert(tx, ty, tw, th) ||
                        child2->insert(tx, ty, tw, th);
        available = std::max(child1->available, child2->available);
        if(!available)
        {
            discardchildren();
        }
        return inserted;
    }
    if(w == tw && h == th)
    {
        available = 0;
        tx = x;
        ty = y;
        return true;
    }

    if(w - tw > h - th)
    {
        child1 = new PackNode(x, y, tw, h);
        child2 = new PackNode(x + tw, y, w - tw, h);
    }
    else
    {
        child1 = new PackNode(x, y, w, th);
        child2 = new PackNode(x, y + th, w, h - th);
    }

    bool inserted = child1->insert(tx, ty, tw, th);
    available = std::max(child1->available, child2->available);
    return inserted;
}

void PackNode::reserve(ushort tx, ushort ty, ushort tw, ushort th)
{
    if(tx + tw <= x || tx >= x + w || ty + th <= y || ty >= y + h)
    {
        return;
    }
    if(child1)
    {
        child1->reserve(tx, ty, tw, th);
        child2->reserve(tx, ty, tw, th);
        available = std::max(child1->available, child2->available);
        return;
    }
    int dx1 = tx - x,
        dx2 = x + w - tx - tw,
        dx = std::max(dx1, dx2),
        dy1 = ty - y,
        dy2 = y + h - ty - th,
        dy = std::max(dy1, dy2),
        split;
    if(dx > dy)
    {
        if(dx1 > dx2)
        {
            split = std::min(dx1, static_cast<int>(w));
        }
        else
        {
            split = w - std::max(dx2, 0);
        }
        if(w - split <= 0)
        {
            w = split;
            available = std::min(w, h);
            if(dy > 0)
            {
                reserve(tx, ty, tw, th);
            }
            else if(tx <= x && tx + tw >= x + w)
            {
                available = 0;
            }
            return;
        }
        if(split <= 0)
        {
            x += split;
            w -= split;
            available = std::min(w, h);
            if(dy > 0)
            {
                reserve(tx, ty, tw, th);
            }
            else if(tx <= x && tx + tw >= x + w)
            {
                available = 0;
            }
            return;
        }
        child1 = new PackNode(x, y, split, h);
        child2 = new PackNode(x + split, y, w - split, h);
    }
    else
    {
        if(dy1 > dy2)
        {
            split = std::min(dy1, static_cast<int>(h));
        }
        else
        {
            split = h - std::max(dy2, 0);
        }
        if(h - split <= 0)
        {
            h = split;
            available = std::min(w, h);
            if(dx > 0)
            {
                reserve(tx, ty, tw, th);
            }
            else if(ty <= y && ty + th >= y + h)
            {
                available = 0;
            }
            return;
        }
        if(split <= 0)
        {
            y += split;
            h -= split;
            available = std::min(w, h);
            if(dx > 0)
            {
                reserve(tx, ty, tw, th);
            }
            else if(ty <= y && ty + th >= y + h)
            {
                available = 0;
            }
            return;
        }
        child1 = new PackNode(x, y, w, split);
        child2 = new PackNode(x, y + split, w, h - split);
    }
    child1->reserve(tx, ty, tw, th);
    child2->reserve(tx, ty, tw, th);
    available = std::max(child1->available, child2->available);
}

static void clearsurfaces(std::array<cube, 8> &c)
{
    for(int i = 0; i < 8; ++i)
    {
        if(c[i].ext)
        {
            for(int j = 0; j < 6; ++j)
            {
                surfaceinfo &surf = c[i].ext->surfaces[j];
                if(!surf.used())
                {
                    continue;
                }
                surf.clear();
                int numverts = surf.numverts&Face_MaxVerts;
                if(numverts)
                {
                    if(!(c[i].merged&(1<<j)))
                    {
                        surf.numverts &= ~Face_MaxVerts;
                        continue;
                    }
                    vertinfo *verts = c[i].ext->verts() + surf.verts;
                    for(int k = 0; k < numverts; ++k)
                    {
                        vertinfo &v = verts[k];
                        v.norm = 0;
                    }
                }
            }
        }
        if(c[i].children)
        {
            clearsurfaces(*(c[i].children));
        }
    }
}


static constexpr int lightcacheentries = 1024;

static struct lightcacheentry
{
    int x, y;
} lightcache[lightcacheentries];

static int lightcachehash(int x, int y)
{
    return (((((x)^(y))<<5) + (((x)^(y))>>5)) & (lightcacheentries - 1));
}

VARF(lightcachesize, 4, 6, 12, clearlightcache());

void clearlightcache(int id)
{
    if(id >= 0)
    {
        const extentity &light = *entities::getents()[id];
        int radius = light.attr1;
        if(radius <= 0)
        {
            return;
        }
        for(int x = static_cast<int>(std::max(light.o.x-radius, 0.0f))>>lightcachesize, ex = static_cast<int>(std::min(light.o.x+radius, rootworld.mapsize()-1.0f))>>lightcachesize; x <= ex; x++)
        {
            for(int y = static_cast<int>(std::max(light.o.y-radius, 0.0f))>>lightcachesize, ey = static_cast<int>(std::min(light.o.y+radius, rootworld.mapsize()-1.0f))>>lightcachesize; y <= ey; y++)
            {
                lightcacheentry &lce = lightcache[lightcachehash(x, y)];
                if(lce.x != x || lce.y != y)
                {
                    continue;
                }
                lce.x = -1;
            }
        }
        return;
    }

    for(lightcacheentry *lce = lightcache; lce < &lightcache[lightcacheentries]; lce++)
    {
        lce->x = -1;
    }
}

static void calcsurfaces(cube &c, const ivec &co, int size, int usefacemask, int preview = 0)
{
    std::array<surfaceinfo, 6> surfaces;
    vertinfo litverts[6*2*Face_MaxVerts];
    int numlitverts = 0;
    surfaces.fill(surfaceinfo());
    for(int i = 0; i < 6; ++i) //for each face of the cube
    {
        int usefaces = usefacemask&0xF;
        usefacemask >>= 4;
        if(!usefaces)
        {
            if(!c.ext)
            {
                continue;
            }
            surfaceinfo &surf = c.ext->surfaces[i];
            int numverts = surf.totalverts();
            if(numverts)
            {
                std::memcpy(&litverts[numlitverts], c.ext->verts() + surf.verts, numverts*sizeof(vertinfo));
                surf.verts = numlitverts;
                numlitverts += numverts;
            }
            continue;
        }

        VSlot &vslot = lookupvslot(c.texture[i], false),
             *layer = vslot.layer && !(c.material&Mat_Alpha) ? &lookupvslot(vslot.layer, false) : nullptr;
        Shader *shader = vslot.slot->shader;
        int shadertype = shader->type;
        if(layer)
        {
            shadertype |= layer->slot->shader->type;
        }
        surfaceinfo &surf = surfaces[i];
        vertinfo *curlitverts = &litverts[numlitverts];
        int numverts = c.ext ? c.ext->surfaces[i].numverts&Face_MaxVerts : 0;
        ivec mo(co);
        int msz = size,
            convex = 0;
        if(numverts)
        {
            const vertinfo *verts = c.ext->verts() + c.ext->surfaces[i].verts;
            for(int j = 0; j < numverts; ++j)
            {
                curlitverts[j].set(verts[j].getxyz());
            }
            if(c.merged&(1<<i))
            {
                msz = 1<<calcmergedsize(i, mo, size, verts, numverts);
                mo.mask(~(msz-1));
                if(!(surf.numverts&Face_MaxVerts))
                {
                    surf.verts = numlitverts;
                    surf.numverts |= numverts;
                    numlitverts += numverts;
                }
            }
            else if(!flataxisface(c, i))
            {
                convex = faceconvexity(verts, numverts, size);
            }
        }
        else
        {
            std::array<ivec, 4> v;
            genfaceverts(c, i, v);
            if(!flataxisface(c, i))
            {
                convex = faceconvexity(v);
            }
            int order = usefaces&4 || convex < 0 ? 1 : 0;
            ivec vo = ivec(co).mask(0xFFF).shl(3);
            curlitverts[numverts++].set(v[order].mul(size).add(vo));
            if(usefaces&1)
            {
                curlitverts[numverts++].set(v[order+1].mul(size).add(vo));
            }
            curlitverts[numverts++].set(v[order+2].mul(size).add(vo));
            if(usefaces&2)
            {
                curlitverts[numverts++].set(v[(order+3)&3].mul(size).add(vo));
            }
        }

        vec pos[Face_MaxVerts],
            n[Face_MaxVerts],
            po(ivec(co).mask(~0xFFF));
        for(int j = 0; j < numverts; ++j)
        {
            pos[j] = vec(curlitverts[j].getxyz()).mul(1.0f/8).add(po);
        }

        int smooth = vslot.slot->smooth;
        plane planes[2];
        int numplanes = 0;
        planes[numplanes++].toplane(pos[0], pos[1], pos[2]);
        if(numverts < 4 || !convex)
        {
            for(int k = 0; k < numverts; ++k)
            {
                findnormal(pos[k], smooth, planes[0], n[k]);
            }
        }
        else
        {
            planes[numplanes++].toplane(pos[0], pos[2], pos[3]);
            vec avg = vec(planes[0]).add(planes[1]).normalize();
            findnormal(pos[0], smooth, avg, n[0]);
            findnormal(pos[1], smooth, planes[0], n[1]);
            findnormal(pos[2], smooth, avg, n[2]);
            for(int k = 3; k < numverts; k++)
            {
                findnormal(pos[k], smooth, planes[1], n[k]);
            }
        }
        for(int k = 0; k < numverts; ++k)
        {
            curlitverts[k].norm = encodenormal(n[k]);
        }
        if(!(surf.numverts&Face_MaxVerts))
        {
            surf.verts = numlitverts;
            surf.numverts |= numverts;
            numlitverts += numverts;
        }
        if(preview)
        {
            surf.numverts |= preview;
            continue;
        }
        int surflayer = BlendLayer_Top;
        if(vslot.layer)
        {
            int x1 = curlitverts[numverts-1].x,
                y1 = curlitverts[numverts-1].y,
                x2 = x1,
                y2 = y1;
            for(int j = 0; j < numverts-1; ++j)
            {
                const vertinfo &v = curlitverts[j];
                x1 = std::min(x1, static_cast<int>(v.x));
                y1 = std::min(y1, static_cast<int>(v.y));
                x2 = std::max(x2, static_cast<int>(v.x));
                y2 = std::max(y2, static_cast<int>(v.y));
            }
            x2 = std::max(x2, x1+1);
            y2 = std::max(y2, y1+1);
            x1 = (x1>>3) + (co.x&~0xFFF);
            y1 = (y1>>3) + (co.y&~0xFFF);
            x2 = ((x2+7)>>3) + (co.x&~0xFFF);
            y2 = ((y2+7)>>3) + (co.y&~0xFFF);
        }
        surf.numverts |= surflayer;
    }
    if(preview)
    {
        setsurfaces(c, surfaces, litverts, numlitverts);
    }
    else
    {
        for(const surfaceinfo &surf : surfaces)
        {
            if(surf.used())
            {
                cubeext *ext = c.ext && c.ext->maxverts >= numlitverts ? c.ext : growcubeext(c.ext, numlitverts);
                std::memcpy(ext->surfaces.data(), surfaces.data(), sizeof(ext->surfaces));
                std::memcpy(ext->verts(), litverts, numlitverts*sizeof(vertinfo));
                if(c.ext != ext)
                {
                    setcubeext(c, ext);
                }
                break;
            }
        }
    }
}

static void calcsurfaces(std::array<cube, 8> &c, const ivec &co, int size)
{
    for(int i = 0; i < 8; ++i)
    {
        ivec o(i, co, size);
        if(c[i].children)
        {
            calcsurfaces(*(c[i].children), o, size >> 1);
        }
        else if(!(c[i].isempty()))
        {
            if(c[i].ext)
            {
                for(surfaceinfo &s : c[i].ext->surfaces)
                {
                    s.clear();
                }
            }
            int usefacemask = 0;
            for(int j = 0; j < 6; ++j)
            {
                if(c[i].texture[j] != Default_Sky && (!(c[i].merged & (1 << j)) || (c[i].ext && c[i].ext->surfaces[j].numverts & Face_MaxVerts)))
                {
                    usefacemask |= visibletris(c[i], j, o, size)<<(4*j);
                }
            }
            if(usefacemask)
            {
                calcsurfaces(c[i], o, size, usefacemask);
            }
        }
    }
}

void cubeworld::calclight()
{
    remip();
    clearsurfaces(*worldroot);
    calcnormals(filltjoints > 0);
    calcsurfaces(*worldroot, ivec(0, 0, 0), rootworld.mapsize() >> 1);
    clearnormals();
    allchanged();
}

VAR(fullbright, 0, 0, 1);           //toggles rendering at fullbrightlevel light
VAR(fullbrightlevel, 0, 160, 255);  //grayscale shade for lighting when at fullbright

void clearlights()
{
    clearlightcache();
    clearshadowcache();
    cleardeferredlightshaders();
    resetsmoothgroups();
}

void initlights()
{
    clearlightcache();
    clearshadowcache();
    loaddeferredlightshaders();
}

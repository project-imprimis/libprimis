/* csm.cpp: cascaded shadow maps
 *
 * The cascaded shadow maps are used to provide levels of detail for sunlight
 * (nearer areas get higher resolution shadow maps, and farther areas are
 * covered by lower quality shadow maps). The cascading shadow maps go into the
 * shadow atlas and are treated the same as any other light (though their size
 * is typically a sizable portion of the atlas space)
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"

#include "csm.h"
#include "octarender.h"
#include "rendergl.h"
#include "renderlights.h"
#include "texture.h"

#include "world/light.h"

//`c`ascaded `s`hadow `m`ap vars

//vars & functions not used in other files
namespace
{
    VARF(csmmaxsize, 256, 768, 2048, clearshadowcache());
    FVAR(csmsplitweight, 0.20f, 0.75f, 0.95f);
    VAR(csmnearplane, 1, 1, 16);                        //short end cutoff of shadow rendering on view frustum
    VAR(csmfarplane, 64, 1024, 16384);                  //far end cutoff of shadow rendering on view frustum
    FVAR(csmpradiustweak, 1e-3f, 1, 1e3f);              //csm projection radius tweak factor to multiply calcfrustumboundsphere by
    FVAR(csmdepthrange, 0, 1024, 1e6f);
    FVAR(csmdepthmargin, 0, 0.1f, 1e3f);
    FVAR(csmbias, -1e6f, 1e-4f, 1e6f);                  //csm bias factor if smfilter <= 2
    FVAR(csmbias2, -1e16f, 2e-4f, 1e6f);                //csm bias factor if smfilter >  2
    VAR(csmcull, 0, 1, 1);
}

//vars used in other files
VARF(csmsplits, 1, 3, csmmaxsplits, { cleardeferredlightshaders(); clearshadowcache(); });
VARF(csmshadowmap, 0, 1, 1, { cleardeferredlightshaders(); clearshadowcache(); });
FVAR(csmpolyfactor, -1e3f, 2, 1e3f);
FVAR(csmpolyoffset, -1e4f, 0, 1e4f);

FVAR(csmpolyfactor2, -1e3f, 3, 1e3f);
FVAR(csmpolyoffset2, -1e4f, 0, 1e4f);
cascadedshadowmap csm;

//====================== cascaded shadow map object ============================//

int cascadedshadowmap::calcbbcsmsplits(const ivec &bbmin, const ivec &bbmax)
{
    int mask = (1<<csmsplits)-1;
    if(!csmcull)
    {
        return mask;
    }
    for(int i = 0; i < csmsplits; ++i)
    {
        const cascadedshadowmap::splitinfo &split = splits[i];
        int k;
        for(k = 0; k < 4; k++)
        {
            const plane &p = split.cull[k];
            ivec omin, omax;
            if(p.x > 0)
            {
                omin.x = bbmin.x;
                omax.x = bbmax.x;
            }
            else
            {
                omin.x = bbmax.x;
                omax.x = bbmin.x;
            }
            if(p.y > 0)
            {
                omin.y = bbmin.y;
                omax.y = bbmax.y;
            }
            else
            {
                omin.y = bbmax.y;
                omax.y = bbmin.y;
            }
            if(p.z > 0)
            {
                omin.z = bbmin.z;
                omax.z = bbmax.z;
            }
            else
            {
                omin.z = bbmax.z;
                omax.z = bbmin.z;
            }
            if(omax.dist(p) < 0)
            {
                mask &= ~(1<<i);
                goto nextsplit;//skip rest and restart loop
            }
            if(omin.dist(p) < 0)
            {
                goto notinside;
            }
        }
        mask &= (2<<i)-1;
        break;
    notinside:
        while(++k < 4)
        {
            const plane &p = split.cull[k];
            ivec omax(p.x > 0 ? bbmax.x : bbmin.x, p.y > 0 ? bbmax.y : bbmin.y, p.z > 0 ? bbmax.z : bbmin.z);
            if(omax.dist(p) < 0)
            {
                mask &= ~(1<<i);
                break;
            }
        }
    nextsplit:;
    }
    return mask;
}

int cascadedshadowmap::calcspherecsmsplits(const vec &center, float radius)
{
    int mask = (1<<csmsplits)-1;
    if(!csmcull)
    {
        return mask;
    }
    for(int i = 0; i < csmsplits; ++i)
    {
        const cascadedshadowmap::splitinfo &split = splits[i];
        int k;
        for(k = 0; k < 4; k++)
        {
            const plane &p = split.cull[k];
            float dist = p.dist(center);
            if(dist < -radius)
            {
                mask &= ~(1<<i);
                goto nextsplit; //skip rest and restart loop
            }
            if(dist < radius)
            {
                goto notinside;
            }
        }
        mask &= (2<<i)-1;
        break;
    notinside:
        while(++k < 4)
        {
            const plane &p = split.cull[k];
            if(p.dist(center) < -radius)
            {
                mask &= ~(1<<i);
                break;
            }
        }
    nextsplit:;
    }
    return mask;
}

void cascadedshadowmap::updatesplitdist()
{
    float lambda = csmsplitweight,
          nd     = csmnearplane,
          fd     = csmfarplane,
          ratio  = fd/nd;
    splits[0].nearplane = nd;
    for(int i = 1; i < csmsplits; ++i)
    {
        float si = i / static_cast<float>(csmsplits);
        splits[i].nearplane = lambda*(nd*std::pow(ratio, si)) + (1-lambda)*(nd + (fd - nd)*si);
        splits[i-1].farplane = splits[i].nearplane * 1.005f;
    }
    splits[csmsplits-1].farplane = fd;
}

void cascadedshadowmap::getmodelmatrix()
{
    model = viewmatrix;
    model.rotate_around_x(sunlightpitch*RAD);
    model.rotate_around_z((180-sunlightyaw)*RAD);
}

void cascadedshadowmap::getprojmatrix()
{
    lightview = vec(sunlightdir).neg();

    // compute the split frustums
    updatesplitdist();

    // find z extent
    float minz = lightview.project_bb(worldmin, worldmax),
          maxz = lightview.project_bb(worldmax, worldmin),
          zmargin = std::max((maxz - minz)*csmdepthmargin, 0.5f*(csmdepthrange - (maxz - minz)));
    minz -= zmargin;
    maxz += zmargin;

    // compute each split projection matrix
    for(int i = 0; i < csmsplits; ++i)
    {
        splitinfo &split = splits[i];
        if(split.idx < 0)
        {
            continue;
        }
        const shadowmapinfo &sm = shadowmaps[split.idx];

        vec c;
        float radius = calcfrustumboundsphere(split.nearplane, split.farplane, camera1->o, camdir, c);

        // compute the projected bounding box of the sphere
        vec tc;
        model.transform(c, tc);
        int border = smfilter > 2 ? smborder2 : smborder;
        const float pradius = std::ceil(radius * csmpradiustweak),
                    step    = (2*pradius) / (sm.size - 2*border);
        vec2 offset = vec2(tc).sub(pradius).div(step);
        offset.x = std::floor(offset.x);
        offset.y = std::floor(offset.y);
        split.center = vec(vec2(offset).mul(step).add(pradius), -0.5f*(minz + maxz));
        split.bounds = vec(pradius, pradius, 0.5f*(maxz - minz));

        // modify mvp with a scale and offset
        // now compute the update model view matrix for this split
        split.scale = vec(1/step, 1/step, -1/(maxz - minz));
        split.offset = vec(border - offset.x, border - offset.y, -minz/(maxz - minz));

        split.proj.identity();
        split.proj.settranslation(2*split.offset.x/sm.size - 1, 2*split.offset.y/sm.size - 1, 2*split.offset.z - 1);
        split.proj.setscale(2*split.scale.x/sm.size, 2*split.scale.y/sm.size, 2*split.scale.z);
    }
}

void cascadedshadowmap::gencullplanes()
{
    for(int i = 0; i < csmsplits; ++i)
    {
        splitinfo &split = splits[i];
        matrix4 mvp;
        mvp.mul(split.proj, model);
        vec4<float> px = mvp.rowx(),
             py = mvp.rowy(),
             pw = mvp.roww();
        split.cull[0] = plane(vec4<float>(pw).add(px)).normalize(); // left plane
        split.cull[1] = plane(vec4<float>(pw).sub(px)).normalize(); // right plane
        split.cull[2] = plane(vec4<float>(pw).add(py)).normalize(); // bottom plane
        split.cull[3] = plane(vec4<float>(pw).sub(py)).normalize(); // top plane
    }
}

void cascadedshadowmap::bindparams()
{
    GLOBALPARAM(csmmatrix, matrix3(model));

    static GlobalShaderParam csmtc("csmtc"),
                             csmoffset("csmoffset");
    vec4<float> *csmtcv = csmtc.reserve<vec4<float>>(csmsplits);
    vec  *csmoffsetv = csmoffset.reserve<vec>(csmsplits);
    for(int i = 0; i < csmsplits; ++i)
    {
        cascadedshadowmap::splitinfo &split = splits[i];
        if(split.idx < 0)
        {
            continue;
        }
        const shadowmapinfo &sm = shadowmaps[split.idx];

        csmtcv[i] = vec4<float>(vec2(split.center).mul(-split.scale.x), split.scale.x, split.bounds.x*split.scale.x);

        const float bias = (smfilter > 2 ? csmbias2 : csmbias) * (-512.0f / sm.size) * (split.farplane - split.nearplane) / (splits[0].farplane - splits[0].nearplane);
        csmoffsetv[i] = vec(sm.x, sm.y, 0.5f + bias).add2(0.5f*sm.size);
    }
    GLOBALPARAMF(csmz, splits[0].center.z*-splits[0].scale.z, splits[0].scale.z);
}

void cascadedshadowmap::setup()
{
    int size = (csmmaxsize * shadowatlaspacker.w) / shadowatlassize;
    for(int i = 0; i < csmsplits; i++)
    {
        ushort smx = USHRT_MAX,
               smy = USHRT_MAX;
        splits[i].idx = -1;
        if(shadowatlaspacker.insert(smx, smy, size, size))
        {
            addshadowmap(smx, smy, size, splits[i].idx);
        }
    }
    getmodelmatrix();
    getprojmatrix();
    gencullplanes();
}

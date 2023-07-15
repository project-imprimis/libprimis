/* grass.cpp: billboarded grass generation atop cube geometry
 *
 * grass can be rendered on the top side of geometry, in an "X" shape atop cubes
 * grass is billboarded, and faces the camera, and can be modified as to its draw
 * distance
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "grass.h"
#include "octarender.h"
#include "rendergl.h"
#include "renderva.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/control.h"

#include "world/octaworld.h"

namespace //internal functionality not seen by other files
{
    VARP(grass, 0, 1, 1);           //toggles rendering of grass
    VARP(grassdist, 0, 256, 10000); //maximum distance to render grass
    FVARP(grasstaper, 0, 0.2, 1);
    FVARP(grassstep, 0.5, 2, 8);
    VARP(grassheight, 1, 4, 64);    //height of grass in cube units

    constexpr int numgrasswedges = 8;

    struct grasswedge
    {
        vec dir, across, edge1, edge2;
        plane bound1, bound2;

        grasswedge(int i) :
          dir(2*M_PI*(i+0.5f)/static_cast<float>(numgrasswedges), 0),
          across(2*M_PI*((i+0.5f)/static_cast<float>(numgrasswedges) + 0.25f), 0),
          edge1(vec(2*M_PI*i/static_cast<float>(numgrasswedges), 0).div(std::cos(M_PI/numgrasswedges))),
          edge2(vec(2*M_PI*(i+1)/static_cast<float>(numgrasswedges), 0).div(std::cos(M_PI/numgrasswedges))),
          bound1(vec(2*M_PI*(i/static_cast<float>(numgrasswedges) - 0.25f), 0), 0),
          bound2(vec(2*M_PI*((i+1)/static_cast<float>(numgrasswedges) + 0.25f), 0), 0)
        {
            across.div(-across.dot(bound1));
        }
    } grasswedges[numgrasswedges] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    struct grassvert
    {
        vec pos;
        vec4<uchar> color;
        vec2 tc;
    };

    std::vector<grassvert> grassverts;
    GLuint grassvbo = 0;
    int grassvbosize = 0;

    VAR(maxgrass, 10, 10000, 10000);            //number of grass squares allowed to be rendered at a time

    struct grassgroup
    {
        const grasstri *tri;
        int tex, offset, numquads;
    };

    std::vector<grassgroup> grassgroups;

    constexpr int numgrassoffsets = 32;

    float grassoffsets[numgrassoffsets] = { -1 },
          grassanimoffsets[numgrassoffsets];
    int lastgrassanim = -1;

    VARR(grassanimmillis, 0, 3000, 60000);      //sets the characteristic rate of grass animation change
    FVARR(grassanimscale, 0, 0.03f, 1);         //sets the intensity of the animation (size of waviness)

    //updates the grass animation offset values based on the current time
    void animategrass()
    {
        for(int i = 0; i < numgrassoffsets; ++i)
        {
            grassanimoffsets[i] = grassanimscale*std::sin(2*M_PI*(grassoffsets[i] + lastmillis/static_cast<float>(grassanimmillis)));
        }
        lastgrassanim = lastmillis;
    }

    VARR(grassscale, 1, 2, 64);  //scale factor for grass texture
    CVAR0R(grasscolor, 0xFFFFFF);//tint color for grass
    FVARR(grasstest, 0, 0.6f, 1);

    //generate the grass geometry placed above cubes
    //grass always faces the camera (billboarded)
    //and therefore grass geom is calculated realtime to face the cam
    void gengrassquads(grassgroup *&group, const grasswedge &w, const grasstri &g, const Texture *tex)
    {
        float t = camera1->o.dot(w.dir);
        int tstep = static_cast<int>(std::ceil(t/grassstep));
        float tstart = tstep*grassstep,
              t0 = w.dir.dot(g.v[0]),
              t1 = w.dir.dot(g.v[1]),
              t2 = w.dir.dot(g.v[2]),
              t3 = w.dir.dot(g.v[3]),
              tmin = std::min(std::min(t0, t1), std::min(t2, t3)),
              tmax = std::max(std::max(t0, t1), std::max(t2, t3));
        if(tmax < tstart || tmin > t + grassdist)
        {
            return;
        }
        int minstep = std::max(static_cast<int>(std::ceil(tmin/grassstep)) - tstep, 1),
            maxstep = static_cast<int>(std::floor(std::min(tmax, t + grassdist)/grassstep)) - tstep,
            numsteps = maxstep - minstep + 1;

        float texscale = (grassscale*tex->ys)/static_cast<float>(grassheight*tex->xs),
              animscale = grassheight*texscale;
        vec tc;
        tc.cross(g.surface, w.dir).mul(texscale);

        int offset = tstep + maxstep;
        if(offset < 0)
        {
            offset = numgrassoffsets - (-offset)%numgrassoffsets;
        }
        offset += numsteps + numgrassoffsets - numsteps%numgrassoffsets;

        float leftdist = t0;
        const vec *leftv = &g.v[0];
        if(t1 > leftdist)
        {
            leftv = &g.v[1];
            leftdist = t1;
        }
        if(t2 > leftdist)
        {
            leftv = &g.v[2];
            leftdist = t2;
        }
        if(t3 > leftdist)
        {
            leftv = &g.v[3];
            leftdist = t3;
        }
        float rightdist = leftdist;
        const vec *rightv = leftv;

        vec across(w.across.x, w.across.y, g.surface.zdelta(w.across)),
            leftdir(0, 0, 0),
            rightdir(0, 0, 0),
            leftp = *leftv,
            rightp = *rightv;
        float taperdist = grassdist*grasstaper,
              taperscale = 1.0f / (grassdist - taperdist),
              dist = maxstep*grassstep + tstart,
              leftb = 0,
              rightb = 0,
              leftdb = 0,
              rightdb = 0;
        for(int i = maxstep; i >= minstep; i--, offset--, leftp.add(leftdir), rightp.add(rightdir), leftb += leftdb, rightb += rightdb, dist -= grassstep)
        {
            if(dist <= leftdist)
            {
                const vec *prev = leftv;
                float prevdist = leftdist;
                if(--leftv < &g.v[0])
                {
                    leftv += g.numv;
                }
                leftdist = leftv->dot(w.dir);
                if(dist <= leftdist)
                {
                    prev = leftv;
                    prevdist = leftdist;
                    if(--leftv < &g.v[0])
                    {
                        leftv += g.numv;
                    }
                    leftdist = leftv->dot(w.dir);
                }
                leftdir = vec(*leftv).sub(*prev);
                leftdir.mul(grassstep/-w.dir.dot(leftdir));
                leftp = vec(leftdir).mul((prevdist - dist)/grassstep).add(*prev);
                leftb = w.bound1.dist(leftp);
                leftdb = w.bound1.dot(leftdir);
            }
            if(dist <= rightdist)
            {
                const vec *prev = rightv;
                float prevdist = rightdist;
                if(++rightv >= &g.v[g.numv])
                {
                    rightv = &g.v[0];
                }
                rightdist = rightv->dot(w.dir);
                if(dist <= rightdist)
                {
                    prev = rightv;
                    prevdist = rightdist;
                    if(++rightv >= &g.v[g.numv])
                    {
                        rightv = &g.v[0];
                    }
                    rightdist = rightv->dot(w.dir);
                }
                rightdir = vec(*rightv).sub(*prev);
                rightdir.mul(grassstep/-w.dir.dot(rightdir));
                rightp = vec(rightdir).mul((prevdist - dist)/grassstep).add(*prev);
                rightb = w.bound2.dist(rightp);
                rightdb = w.bound2.dot(rightdir);
            }
            vec p1 = leftp,
                p2 = rightp;
            if(leftb > 0)
            {
                if(w.bound1.dist(p2) >= 0)
                {
                    continue;
                }
                p1.add(vec(across).mul(leftb));
            }
            if(rightb > 0)
            {
                if(w.bound2.dist(p1) >= 0)
                {
                    continue;
                }
                p2.sub(vec(across).mul(rightb));
            }

            if(static_cast<int>(grassverts.size()) >= 4*maxgrass)
            {
                break;
            }

            if(!group)
            {
                grassgroup group;
                group.tri = &g;
                group.tex = tex->id;
                group.offset = grassverts.size()/4;
                group.numquads = 0;
                grassgroups.push_back(group);
                if(lastgrassanim!=lastmillis)
                {
                    animategrass();
                }
            }

            group->numquads++;

            float tcoffset = grassoffsets[offset%numgrassoffsets],
                  animoffset = animscale*grassanimoffsets[offset%numgrassoffsets],
                  tc1 = tc.dot(p1) + tcoffset,
                  tc2 = tc.dot(p2) + tcoffset,
                  fade = dist - t > taperdist ? (grassdist - (dist - t))*taperscale : 1,
                  height = grassheight * fade;
            vec4<uchar> color(grasscolor, 255);
    //=====================================================================GRASSVERT
            #define GRASSVERT(n, tcv, modify) { \
                grassvert gv; \
                gv.pos = p##n; \
                gv.color = color; \
                gv.tc = vec2(tc##n, tcv); \
                grassverts.push_back(gv); \
                modify; \
            }

            GRASSVERT(2, 0, { gv.pos.z += height; gv.tc.x += animoffset; });
            GRASSVERT(1, 0, { gv.pos.z += height; gv.tc.x += animoffset; });
            GRASSVERT(1, 1, );
            GRASSVERT(2, 1, );

            #undef GRASSVERT
    //==============================================================================
        }
    }

    // generates grass geometry for a given vertex array
    void gengrassquads(const vtxarray &va)
    {
        for(const grasstri &g : va.grasstris)
        {
            if(view.isfoggedsphere(g.radius, g.center))
            {
                continue;
            }
            float dist = g.center.dist(camera1->o);
            if(dist - g.radius > grassdist)
            {
                continue;
            }
            Slot &s = *lookupvslot(g.texture, false).slot;
            if(!s.grasstex)
            {
                if(!s.grass)
                {
                    continue;
                }
                s.grasstex = textureload(s.grass, 2);
            }
            grassgroup *group = nullptr;
            for(const grasswedge &w : grasswedges)
            {
                if(w.bound1.dist(g.center) > g.radius || w.bound2.dist(g.center) > g.radius)
                {
                    continue;
                }
                gengrassquads(group, w, g, s.grasstex);
            }
        }
    }

    bool hasgrassshader = false;

    void cleargrassshaders()
    {
        hasgrassshader = false;
    }

    Shader *loadgrassshader()
    {
        std::string name = "grass";
        return generateshader(name.c_str(), "grassshader ");

    }
}

/* externally relevant functions */
///////////////////////////////////

void generategrass()
{
    if(!grass || !grassdist)
    {
        return;
    }
    grassgroups.clear();
    grassverts.clear();

    if(grassoffsets[0] < 0)
    {
        for(int i = 0; i < numgrassoffsets; ++i)
        {
            grassoffsets[i] = randomint(0x1000000)/static_cast<float>(0x1000000);
        }
    }

    for(grasswedge &w : grasswedges)
    {
        w.bound1.offset = -camera1->o.dot(w.bound1);
        w.bound2.offset = -camera1->o.dot(w.bound2);
    }

    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->grasstris.empty() || va->occluded >= Occlude_Geom)
        {
            continue;
        }
        if(va->distance > grassdist)
        {
            continue;
        }
        gengrassquads(*va);
    }

    if(grassgroups.empty())
    {
        return;
    }
    if(!grassvbo)
    {
        glGenBuffers(1, &grassvbo);
    }
    gle::bindvbo(grassvbo);
    int size = grassverts.size()*sizeof(grassvert);
    grassvbosize = std::max(grassvbosize, size);
    glBufferData(GL_ARRAY_BUFFER, grassvbosize, size == grassvbosize ? grassverts.data() : nullptr, GL_STREAM_DRAW);
    if(size != grassvbosize)
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, grassverts.data());
    }
    gle::clearvbo();
}

void loadgrassshaders()
{
    hasgrassshader = (loadgrassshader() != nullptr);
}

void rendergrass()
{
    if(!grass || !grassdist || grassgroups.empty() || !hasgrassshader)
    {
        return;
    }
    glDisable(GL_CULL_FACE);

    gle::bindvbo(grassvbo);

    const grassvert *ptr = nullptr;
    gle::vertexpointer(sizeof(grassvert), ptr->pos.v);
    gle::colorpointer(sizeof(grassvert), ptr->color.v);
    gle::texcoord0pointer(sizeof(grassvert), ptr->tc.v);
    gle::enablevertex();
    gle::enablecolor();
    gle::enabletexcoord0();
    gle::enablequads();

    GLOBALPARAMF(grasstest, grasstest); //toggles use of grass (depth) test shader

    int texid = -1;
    for(const grassgroup &g : grassgroups)
    {
        if(texid != g.tex)
        {
            glBindTexture(GL_TEXTURE_2D, g.tex);
            texid = g.tex;
        }

        gle::drawquads(g.offset, g.numquads);
        xtravertsva += 4*g.numquads;
    }

    gle::disablequads();
    gle::disablevertex();
    gle::disablecolor();
    gle::disabletexcoord0();

    gle::clearvbo();

    glEnable(GL_CULL_FACE);
}

void cleanupgrass()
{
    if(grassvbo)
    {
        glDeleteBuffers(1, &grassvbo);
        grassvbo = 0;
    }
    grassvbosize = 0;

    cleargrassshaders();
}

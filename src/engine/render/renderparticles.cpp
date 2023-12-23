/* renderparticles.cpp: billboard particle rendering
 *
 * renderparticles.cpp handles rendering of entity-and-weapon defined billboard particle
 * rendering. Particle entities spawn particles randomly (is not synced between different
 * clients in multiplayer), while weapon projectiles are placed at world-synced locations.
 *
 * Particles, being merely diffuse textures placed on the scene, do not have any special
 * effects (such as refraction or lights). Particles, being of the "billboard" type, always
 * face the camera, which works fairly well for small, simple effects. Particles are not
 * recommended for large, high detail special effects.
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "renderlights.h"
#include "rendergl.h"
#include "renderparticles.h"
#include "renderva.h"
#include "renderwindow.h"
#include "rendertext.h"
#include "renderttf.h"
#include "shader.h"
#include "shaderparam.h"
#include "stain.h"
#include "texture.h"
#include "water.h"

#include "interface/console.h"
#include "interface/control.h"
#include "interface/input.h"

#include "world/octaedit.h"
#include "world/octaworld.h"
#include "world/raycube.h"
#include "world/world.h"

static Shader *particleshader          = nullptr,
              *particlenotextureshader = nullptr,
              *particlesoftshader      = nullptr,
              *particletextshader      = nullptr;

VARP(particlelayers, 0, 1, 1);    //used in renderalpha
FVARP(particlebright, 0, 2, 100); //multiply particle colors by this factor in brightness
VARP(particlesize, 20, 100, 500); //particle size factor

VARP(softparticleblend, 1, 8, 64); //inverse of blend factor for soft particle blending

// Check canemitparticles() to limit the rate that paricles can be emitted for models/sparklies
// Automatically stops particles being emitted when paused or in reflective drawing
VARP(emitmillis, 1, 17, 1000); //note: 17 ms = ~60fps

static int emitoffset     = 0;
static bool canemit       = false,
            regenemitters = false,
            canstep       = false;

static bool canemitparticles()
{
    return canemit || emitoffset;
}
std::vector<std::string> entnames;

VARP(showparticles,  0, 1, 1);                  //toggles showing billboarded particles
VAR(cullparticles,   0, 1, 1);                  //toggles culling particles beyond fog distance
VAR(replayparticles, 0, 1, 1);                  //toggles re-rendering previously generated particles
static int seedmillis = variable("seedparticles", 0, 3000, 10000, &seedmillis, nullptr, 0);//sets the time between seeding particles
VAR(debugparticlecull, 0, 0, 1);                //print out console information about particles culled
VAR(debugparticleseed, 0, 0, 1);                //print out radius/maxfade info for particles upon spawn

class particleemitter
{
    public:
        const extentity *ent;
        vec center;
        float radius;
        int maxfade, lastemit, lastcull;

        particleemitter(const extentity *ent)
            : ent(ent), maxfade(-1), lastemit(0), lastcull(0), bbmin(ent->o), bbmax(ent->o)
        {}

        void finalize()
        {
            center = vec(bbmin).add(bbmax).mul(0.5f);
            radius = bbmin.dist(bbmax)/2;
            if(debugparticleseed)
            {
                conoutf(Console_Debug, "radius: %f, maxfade: %d", radius, maxfade);
            }
        }

        void extendbb(const vec &o, float size = 0)
        {
            bbmin.x = std::min(bbmin.x, o.x - size);
            bbmin.y = std::min(bbmin.y, o.y - size);
            bbmin.z = std::min(bbmin.z, o.z - size);
            bbmax.x = std::max(bbmax.x, o.x + size);
            bbmax.y = std::max(bbmax.y, o.y + size);
            bbmax.z = std::max(bbmax.z, o.z + size);
        }

        void extendbb(float z, float size = 0)
        {
            bbmin.z = std::min(bbmin.z, z - size);
            bbmax.z = std::max(bbmax.z, z + size);
        }
    private:
        vec bbmin, bbmax;
};

static std::vector<particleemitter> emitters;
static particleemitter *seedemitter = nullptr;

const char * getentname(int i)
{
    return i>=0 && static_cast<size_t>(i) < entnames.size() ? entnames[i].c_str() : "";
}

void clearparticleemitters()
{
    emitters.clear();
    regenemitters = true;
}

void addparticleemitters()
{
    emitters.clear();
    const std::vector<extentity *> &ents = entities::getents();
    for(const extentity *e : ents)
    {
        if(e->type != EngineEnt_Particles)
        {
            continue;
        }
        emitters.emplace_back(particleemitter(e));
    }
    regenemitters = false;
}
//particle types
enum ParticleTypes
{
    PT_PART = 0,
    PT_TAPE,
    PT_TRAIL,
    PT_TEXT,
    PT_TEXTUP,
    PT_METER,
    PT_METERVS,
    PT_FIREBALL,
};
//particle properties
enum ParticleProperties
{
    PT_MOD       = 1<<8,
    PT_RND4      = 1<<9,
    PT_LERP      = 1<<10, // use very sparingly - order of blending issues
    PT_TRACK     = 1<<11,
    PT_BRIGHT    = 1<<12,
    PT_SOFT      = 1<<13,
    PT_HFLIP     = 1<<14,
    PT_VFLIP     = 1<<15,
    PT_ROT       = 1<<16,
    PT_CULL      = 1<<17,
    PT_FEW       = 1<<18,
    PT_ICON      = 1<<19,
    PT_NOTEX     = 1<<20,
    PT_SHADER    = 1<<21,
    PT_NOLAYER   = 1<<22,
    PT_COLLIDE   = 1<<23,
    PT_FLIP      = PT_HFLIP | PT_VFLIP | PT_ROT
};

const std::string partnames[] = { "part", "tape", "trail", "text", "textup", "meter", "metervs", "fireball"};

struct particle
{
    vec o, d;                  //o: origin d: dir
    int gravity, fade, millis; //gravity intensity, fade rate, lifetime
    bvec color;                //color vector triple
    uchar flags;               //particle-specific flags
    float size;                //radius or scale factor
    union                      //for unique properties of particular entities
    {
        const char *text;      //text particle
        float val;             //fireball particle
        physent *owner;        //particle owner (a player/bot)
        struct                 //meter particle
        {
            uchar color2[3];   //color of bar
            uchar progress;    //bar fill %
        };
    };
};

struct partvert
{
    vec pos;     //x,y,z of particle
    vec4<uchar> color; //r,g,b,a color
    vec2 tc;     //texture coordinate
};

static constexpr float collideradius = 8.0f;
static constexpr float collideerror  = 1.0f;
class partrenderer
{
    public:
        partrenderer(const char *texname, int texclamp, int type, int stain = -1)
            : tex(nullptr), type(type), stain(stain), texname(texname), texclamp(texclamp)
        {
        }
        partrenderer(int type, int stain = -1)
            : tex(nullptr), type(type), stain(stain), texname(nullptr), texclamp(0)
        {
        }
        virtual ~partrenderer()
        {
        }

        virtual void init(int n) { }
        virtual void reset() = 0;
        virtual void resettracked(const physent *owner) { }
        virtual particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity = 0) = 0;
        virtual void update() { }
        virtual void render() = 0;
        virtual bool haswork() const = 0;
        virtual int count() const = 0; //for debug
        virtual void cleanup() {}

        virtual void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
        {
        }

        virtual void preload()
        {
            if(texname && !tex)
            {
                tex = textureload(texname, texclamp);
            }
        }

        //blend = 0 => remove it
        void calc(particle *p, int &blend, int &ts, vec &o, vec &d, bool step = true) const
        {
            o = p->o;
            d = p->d;
            if(p->fade <= 5)
            {
                ts = 1;
                blend = 255;
            }
            else
            {
                ts = lastmillis-p->millis;
                blend = std::max(255 - (ts<<8)/p->fade, 0);
                if(p->gravity)
                {
                    if(ts > p->fade)
                    {
                        ts = p->fade;
                    }
                    float t = ts;
                    constexpr float tfactor = 5000.f;
                    o.add(vec(d).mul(t/tfactor));
                    o.z -= t*t/(2.0f * tfactor * p->gravity);
                }
                if(type&PT_COLLIDE && o.z < p->val && step)
                {
                    if(stain >= 0)
                    {
                        vec surface;
                        float floorz = rayfloor(vec(o.x, o.y, p->val), surface, Ray_ClipMat, collideradius),
                              collidez = floorz<0 ? o.z-collideradius : p->val - floorz;
                        if(o.z >= collidez+collideerror)
                        {
                            p->val = collidez+collideerror;
                        }
                        else
                        {
                            int staintype = type&PT_RND4 ? (p->flags>>5)&3 : 0;
                            addstain(stain, vec(o.x, o.y, collidez), vec(p->o).sub(o).normalize(), 2*p->size, p->color, staintype);
                            blend = 0;
                        }
                    }
                    else
                    {
                        blend = 0;
                    }
                }
            }
        }

        //prints out info for a particle, with its letter denoting particle type
        void debuginfo() const
        {
            string info;
            formatstring(info, "%d\t%s(", count(), partnames[type&0xFF].c_str());
            if(type&PT_LERP)    concatstring(info, "l,");
            if(type&PT_MOD)     concatstring(info, "m,");
            if(type&PT_RND4)    concatstring(info, "r,");
            if(type&PT_TRACK)   concatstring(info, "t,");
            if(type&PT_FLIP)    concatstring(info, "f,");
            if(type&PT_COLLIDE) concatstring(info, "c,");
            int len = std::strlen(info);
            info[len-1] = info[len-1] == ',' ? ')' : '\0';
            if(texname)
            {
                const char *title = std::strrchr(texname, '/');
                if(title)
                {
                    concformatstring(info, ": %s", title+1);
                }
            }
        }

        bool hasstain() const
        {
            return stain >= 0;
        }

        uint parttype() const
        {
            return type;
        }
    protected:
        const Texture *tex;
    private:
        uint type;
        int stain;
        const char *texname;
        int texclamp;

};

struct listparticle : particle
{
    listparticle *next;
};

VARP(outlinemeters, 0, 0, 1);

class listrenderer : public partrenderer
{
    public:
        listrenderer(const char *texname, int texclamp, int type, int stain = -1)
            : partrenderer(texname, texclamp, type, stain), list(nullptr)
        {
        }
        listrenderer(int type, int stain = -1)
            : partrenderer(type, stain), list(nullptr)
        {
        }

        virtual ~listrenderer()
        {
        }

    private:
        virtual void killpart(listparticle *p)
        {
        }

        virtual void startrender() = 0;
        virtual void endrender() = 0;
        virtual void renderpart(const listparticle &p, const vec &o, const vec &d, int blend, int ts) = 0;

        bool haswork() const override final
        {
            return (list != nullptr);
        }

        bool renderpart(listparticle *p)
        {
            vec o, d;
            int blend, ts;
            calc(p, blend, ts, o, d, canstep);
            if(blend <= 0)
            {
                return false;
            }
            renderpart(*p, o, d, blend, ts);
            return p->fade > 5;
        }

        void render() override final
        {
            startrender();
            if(tex)
            {
                glBindTexture(GL_TEXTURE_2D, tex->id);
            }
            if(canstep)
            {
                for(listparticle **prev = &list, *p = list; p; p = *prev)
                {
                    if(renderpart(p))
                    {
                        prev = &p->next;
                    }
                    else
                    { // remove
                        *prev = p->next;
                        p->next = parempty;
                        killpart(p);
                        parempty = p;
                    }
                }
            }
            else
            {
                for(listparticle *p = list; p; p = p->next)
                {
                    renderpart(p);
                }
            }
            endrender();
        }
        void reset() override final
        {
            if(!list)
            {
                return;
            }
            listparticle *p = list;
            for(;;)
            {
                killpart(p);
                if(p->next)
                {
                    p = p->next;
                }
                else
                {
                    break;
                }
            }
            p->next = parempty;
            parempty = list;
            list = nullptr;
        }

        void resettracked(const physent *owner) override final
        {
            if(!(parttype()&PT_TRACK))
            {
                return;
            }
            for(listparticle **prev = &list, *cur = list; cur; cur = *prev)
            {
                if(!owner || cur->owner==owner)
                {
                    *prev = cur->next;
                    cur->next = parempty;
                    parempty = cur;
                }
                else
                {
                    prev = &cur->next;
                }
            }
        }

        particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity) override final
        {
            if(!parempty)
            {
                listparticle *ps = new listparticle[256];
                for(int i = 0; i < 255; ++i)
                {
                    ps[i].next = &ps[i+1];
                }
                ps[255].next = parempty;
                parempty = ps;
            }
            listparticle *p = parempty;
            parempty = p->next;
            p->next = list;
            list = p;
            p->o = o;
            p->d = d;
            p->gravity = gravity;
            p->fade = fade;
            p->millis = lastmillis + emitoffset;
            p->color = bvec::hexcolor(color);
            p->size = size;
            p->owner = nullptr;
            p->flags = 0;
            return p;
        }

        int count() const override final
        {
            int num = 0;
            const listparticle *lp;
            for(lp = list; lp; lp = lp->next)
            {
                num++;
            }
            return num;
        }
        static listparticle *parempty;
        listparticle *list;
};

listparticle *listrenderer::parempty = nullptr;

class meterrenderer final : public listrenderer
{
    public:
        meterrenderer(int type)
            : listrenderer(type|PT_NOTEX|PT_LERP|PT_NOLAYER)
        {
        }
    private:
        void startrender() override final
        {
            glDisable(GL_BLEND);
            gle::defvertex();
        }

        void endrender() override final
        {
            glEnable(GL_BLEND);
        }

        void renderpart(const listparticle &p, const vec &o, const vec &d, int blend, int ts) override final
        {
            int basetype = parttype()&0xFF;
            float scale  = FONTH*p.size/80.0f,
                  right  = 8,
                  left   = p.progress/100.0f*right;
            matrix4x3 m(camright(), camup().neg(), camdir().neg(), o);
            m.scale(scale);
            m.translate(-right/2.0f, 0, 0);

            if(outlinemeters)
            {
                gle::colorf(0, 0.8f, 0);
                gle::begin(GL_TRIANGLE_STRIP);
                for(int k = 0; k < 10; ++k)
                {
                    const vec2 &sc = sincos360[k*(180/(10-1))];
                    float c = (0.5f + 0.1f)*sc.y,
                          s = 0.5f - (0.5f + 0.1f)*sc.x;
                    gle::attrib(m.transform(vec2(-c, s)));
                    gle::attrib(m.transform(vec2(right + c, s)));
                }
                gle::end();
            }
            if(basetype==PT_METERVS)
            {
                gle::colorub(p.color2[0], p.color2[1], p.color2[2]);
            }
            else
            {
                gle::colorf(0, 0, 0);
            }
            gle::begin(GL_TRIANGLE_STRIP);
            for(int k = 0; k < 10; ++k)
            {
                const vec2 &sc = sincos360[k*(180/(10-1))];
                float c = 0.5f*sc.y,
                      s = 0.5f - 0.5f*sc.x;
                gle::attrib(m.transform(vec2(left + c, s)));
                gle::attrib(m.transform(vec2(right + c, s)));
            }
            gle::end();

            if(outlinemeters)
            {
                gle::colorf(0, 0.8f, 0);
                gle::begin(GL_TRIANGLE_FAN);
                for(int k = 0; k < 10; ++k)
                {
                    const vec2 &sc = sincos360[k*(180/(10-1))];
                    float c = (0.5f + 0.1f)*sc.y,
                          s = 0.5f - (0.5f + 0.1f)*sc.x;
                    gle::attrib(m.transform(vec2(left + c, s)));
                }
                gle::end();
            }

            gle::color(p.color);
            gle::begin(GL_TRIANGLE_STRIP);
            for(int k = 0; k < 10; ++k)
            {
                const vec2 &sc = sincos360[k*(180/(10-1))];
                float c = 0.5f*sc.y,
                      s = 0.5f - 0.5f*sc.x;
                gle::attrib(m.transform(vec2(-c, s)));
                gle::attrib(m.transform(vec2(left + c, s)));
            }
            gle::end();
        }
};
static meterrenderer meters(PT_METER), metervs(PT_METERVS);

template<int T>
static void modifyblend(const vec &o, int &blend)
{
    blend = std::min(blend<<2, 255);
}

template<int T>
static void genpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs)
{
    vec udir = camup().sub(camright()).mul(size),
        vdir = camup().add(camright()).mul(size);
    vs[0].pos = vec(o.x + udir.x, o.y + udir.y, o.z + udir.z);
    vs[1].pos = vec(o.x + vdir.x, o.y + vdir.y, o.z + vdir.z);
    vs[2].pos = vec(o.x - udir.x, o.y - udir.y, o.z - udir.z);
    vs[3].pos = vec(o.x - vdir.x, o.y - vdir.y, o.z - vdir.z);
}

template<>
void genpos<PT_TAPE>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec dir1 = vec(d).sub(o),
        dir2 = vec(d).sub(camera1->o), c;
    c.cross(dir2, dir1).normalize().mul(size);
    vs[0].pos = vec(d.x-c.x, d.y-c.y, d.z-c.z);
    vs[1].pos = vec(o.x-c.x, o.y-c.y, o.z-c.z);
    vs[2].pos = vec(o.x+c.x, o.y+c.y, o.z+c.z);
    vs[3].pos = vec(d.x+c.x, d.y+c.y, d.z+c.z);
}

template<>
void genpos<PT_TRAIL>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec e = d;
    if(grav)
    {
        e.z -= static_cast<float>(ts)/grav;
    }
    e.div(-75.0f).add(o);
    genpos<PT_TAPE>(o, e, size, ts, grav, vs);
}

template<int T>
void genrotpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs, int rot)
{
    genpos<T>(o, d, size, grav, ts, vs);
}

//==================================================================== ROTCOEFFS
#define ROTCOEFFS(n) { \
    vec2(-1,  1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2( 1,  1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2( 1, -1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2(-1, -1).rotate_around_z(n*2*M_PI/32.0f) \
}
static const vec2 rotcoeffs[32][4] =
{
    ROTCOEFFS(0),  ROTCOEFFS(1),  ROTCOEFFS(2),  ROTCOEFFS(3),  ROTCOEFFS(4),  ROTCOEFFS(5),  ROTCOEFFS(6),  ROTCOEFFS(7),
    ROTCOEFFS(8),  ROTCOEFFS(9),  ROTCOEFFS(10), ROTCOEFFS(11), ROTCOEFFS(12), ROTCOEFFS(13), ROTCOEFFS(14), ROTCOEFFS(15),
    ROTCOEFFS(16), ROTCOEFFS(17), ROTCOEFFS(18), ROTCOEFFS(19), ROTCOEFFS(20), ROTCOEFFS(21), ROTCOEFFS(22), ROTCOEFFS(7),
    ROTCOEFFS(24), ROTCOEFFS(25), ROTCOEFFS(26), ROTCOEFFS(27), ROTCOEFFS(28), ROTCOEFFS(29), ROTCOEFFS(30), ROTCOEFFS(31),
};
#undef ROTCOEFFS
//==============================================================================

template<>
void genrotpos<PT_PART>(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs, int rot)
{
    const vec2 *coeffs = rotcoeffs[rot];
    vs[0].pos = vec(o).madd(camright(), coeffs[0].x*size).madd(camup(), coeffs[0].y*size);
    vs[1].pos = vec(o).madd(camright(), coeffs[1].x*size).madd(camup(), coeffs[1].y*size);
    vs[2].pos = vec(o).madd(camright(), coeffs[2].x*size).madd(camup(), coeffs[2].y*size);
    vs[3].pos = vec(o).madd(camright(), coeffs[3].x*size).madd(camup(), coeffs[3].y*size);
}

template<int T>
void seedpos(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    constexpr float scale = 5000.f;
    if(grav)
    {
        float t = fade;
        vec end = vec(o).madd(d, t/scale);
        end.z -= t*t/(2.0f * scale * grav);
        pe.extendbb(end, size);
        float tpeak = d.z*grav;
        if(tpeak > 0 && tpeak < fade)
        {
            pe.extendbb(o.z + 1.5f*d.z*tpeak/scale, size);
        }
    }
}

template<>
void seedpos<PT_TAPE>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    pe.extendbb(d, size);
}

template<>
void seedpos<PT_TRAIL>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    vec e = d;
    if(grav)
    {
        e.z -= static_cast<float>(fade)/grav;
    }
    e.div(-75.0f).add(o);
    pe.extendbb(e, size);
}

//only used by template<> varenderer, but cannot be declared in a template in c++17
static void setcolor(float r, float g, float b, float a, partvert * vs)
{
    vec4<uchar> col(r, g, b, a);
    for(int i = 0; i < 4; ++i)
    {
        vs[i].color = col;
    }
};

template<int T>
struct varenderer final : partrenderer
{
    partvert *verts;
    particle *parts;
    int maxparts, numparts, lastupdate, rndmask;
    GLuint vbo;

    varenderer(const char *texname, int type, int stain = -1)
        : partrenderer(texname, 3, type, stain),
          verts(nullptr), parts(nullptr), maxparts(0), numparts(0), lastupdate(-1), rndmask(0), vbo(0)
    {
        if(type & PT_HFLIP)
        {
            rndmask |= 0x01;
        }
        if(type & PT_VFLIP)
        {
            rndmask |= 0x02;
        }
        if(type & PT_ROT)
        {
            rndmask |= 0x1F<<2;
        }
        if(type & PT_RND4)
        {
            rndmask |= 0x03<<5;
        }
    }

    void cleanup() override final
    {
        if(vbo)
        {
            glDeleteBuffers(1, &vbo);
            vbo = 0;
        }
    }

    void init(int n) override final
    {
        delete[] parts;
        delete[] verts;
        parts = new particle[n];
        verts = new partvert[n*4];
        maxparts = n;
        numparts = 0;
        lastupdate = -1;
    }

    void reset() override final
    {
        numparts = 0;
        lastupdate = -1;
    }

    void resettracked(const physent *owner) override final
    {
        if(!(parttype()&PT_TRACK))
        {
            return;
        }
        for(int i = 0; i < numparts; ++i)
        {
            particle *p = parts+i;
            if(!owner || (p->owner == owner))
            {
                p->fade = -1;
            }
        }
        lastupdate = -1;
    }

    int count() const override final
    {
        return numparts;
    }

    bool haswork() const override final
    {
        return (numparts > 0);
    }

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity) override final
    {
        particle *p = parts + (numparts < maxparts ? numparts++ : randomint(maxparts)); //next free slot, or kill a random kitten
        p->o = o;
        p->d = d;
        p->gravity = gravity;
        p->fade = fade;
        p->millis = lastmillis + emitoffset;
        p->color = bvec::hexcolor(color);
        p->size = size;
        p->owner = nullptr;
        p->flags = 0x80 | (rndmask ? randomint(0x80) & rndmask : 0);
        lastupdate = -1;
        return p;
    }

    void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity) override final
    {
        pe.maxfade = std::max(pe.maxfade, fade);
        size *= SQRT2;
        pe.extendbb(o, size);

        seedpos<T>(pe, o, d, fade, size, gravity);
        if(!gravity)
        {
            return;
        }
        vec end(o);
        float t = fade;
        end.add(vec(d).mul(t/5000.0f));
        end.z -= t*t/(2.0f * 5000.0f * gravity);
        pe.extendbb(end, size);
        float tpeak = d.z*gravity;
        if(tpeak > 0 && tpeak < fade)
        {
            pe.extendbb(o.z + 1.5f*d.z*tpeak/5000.0f, size);
        }
    }

    void genverts(particle *p, partvert *vs, bool regen)
    {
        vec o, d;
        int blend, ts;
        calc(p, blend, ts, o, d);
        if(blend <= 1 || p->fade <= 5)
        {
            p->fade = -1; //mark to remove on next pass (i.e. after render)
        }
        modifyblend<T>(o, blend);
        if(regen)
        {
            p->flags &= ~0x80;

            auto swaptexcoords = [&] (float u1, float u2, float v1, float v2)
            {
                if(p->flags&0x01)
                {
                    std::swap(u1, u2);
                }
                if(p->flags&0x02)
                {
                    std::swap(v1, v2);
                }
            };

            //sets the partvert vs array's tc fields to four permutations of input parameters
            auto settexcoords = [vs, swaptexcoords] (float u1c, float u2c, float v1c, float v2c, bool swap)
            {
                float u1 = u1c,
                      u2 = u2c,
                      v1 = v1c,
                      v2 = v2c;
                if(swap)
                {
                    swaptexcoords(u1, u2, v1, v2);
                }
                vs[0].tc = vec2(u1, v1);
                vs[1].tc = vec2(u2, v1);
                vs[2].tc = vec2(u2, v2);
                vs[3].tc = vec2(u1, v2);
            };

            if(parttype()&PT_RND4)
            {
                float tx = 0.5f*((p->flags>>5)&1),
                      ty = 0.5f*((p->flags>>6)&1);
                settexcoords(tx, tx + 0.5f, ty, ty + 0.5f, true);
            }
            else if(parttype()&PT_ICON)
            {
                float tx = 0.25f*(p->flags&3),
                      ty = 0.25f*((p->flags>>2)&3);
                settexcoords(tx, tx + 0.25f, ty, ty + 0.25f, false);
            }
            else
            {
                settexcoords(0, 1, 0, 1, false);
            }

            if(parttype()&PT_MOD)
            {
                setcolor((p->color.r*blend)>>8, (p->color.g*blend)>>8, (p->color.b*blend)>>8, 255, vs);
            }
            else
            {
                setcolor(p->color.r, p->color.g, p->color.b, blend, vs);
            }

        }
        else if(parttype()&PT_MOD)
        {
            //note: same call as `if(type&PT_MOD)` above
            setcolor((p->color.r*blend)>>8, (p->color.g*blend)>>8, (p->color.b*blend)>>8, 255, vs);
        }
        else
        {
            for(int i = 0; i < 4; ++i)
            {
                vs[i].color.a = blend;
            }
        }
        if(parttype()&PT_ROT)
        {
            genrotpos<T>(o, d, p->size, ts, p->gravity, vs, (p->flags>>2)&0x1F);
        }
        else
        {
            genpos<T>(o, d, p->size, ts, p->gravity, vs);
        }
    }

    void genverts()
    {
        for(int i = 0; i < numparts; ++i)
        {
            particle *p = &parts[i];
            partvert *vs = &verts[i*4];
            if(p->fade < 0)
            {
                do
                {
                    --numparts;
                    if(numparts <= i)
                    {
                        return;
                    }
                } while(parts[numparts].fade < 0);
                *p = parts[numparts];
                genverts(p, vs, true);
            }
            else
            {
                genverts(p, vs, (p->flags&0x80)!=0);
            }
        }
    }

    void genvbo()
    {
        if(lastmillis == lastupdate && vbo)
        {
            return;
        }
        lastupdate = lastmillis;
        genverts();
        if(!vbo)
        {
            glGenBuffers(1, &vbo);
        }
        gle::bindvbo(vbo);
        glBufferData(GL_ARRAY_BUFFER, maxparts*4*sizeof(partvert), nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, numparts*4*sizeof(partvert), verts);
        gle::clearvbo();
    }

    void render() override final
    {
        genvbo();

        glBindTexture(GL_TEXTURE_2D, tex->id);

        gle::bindvbo(vbo);
        const partvert *ptr = 0;
        gle::vertexpointer(sizeof(partvert), ptr->pos.v);
        gle::texcoord0pointer(sizeof(partvert), ptr->tc.v);
        gle::colorpointer(sizeof(partvert), ptr->color.v);
        gle::enablevertex();
        gle::enabletexcoord0();
        gle::enablecolor();
        gle::enablequads();
        gle::drawquads(0, numparts);
        gle::disablequads();
        gle::disablevertex();
        gle::disabletexcoord0();
        gle::disablecolor();
        gle::clearvbo();
    }
};

// explosions

VARP(softexplosion, 0, 1, 1); //toggles EXPLOSIONSOFT shader
VARP(softexplosionblend, 1, 16, 64);

class fireballrenderer final : public listrenderer
{
    public:
        fireballrenderer(const char *texname)
            : listrenderer(texname, 0, PT_FIREBALL|PT_SHADER)
        {}

        static constexpr float wobble = 1.25f; //factor to extend particle hitbox by due to placement movement

        void startrender() override final
        {
            if(softexplosion)
            {
                SETSHADER(explosionsoft);
            }
            else
            {
                SETSHADER(explosion);
            }
            sr.enable();
        }

        void endrender() override final
        {
            sr.disable();
        }

        void cleanup() override final
        {
            sr.cleanup();
        }

        void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity) override final
        {
            pe.maxfade = std::max(pe.maxfade, fade);
            pe.extendbb(o, (size+1+pe.ent->attr2)*wobble);
        }

        void renderpart(const listparticle &p, const vec &o, const vec &d, int blend, int ts) override final
        {
            float pmax = p.val,
                  size = p.fade ? static_cast<float>(ts)/p.fade : 1,
                  psize = p.size + pmax * size;

            if(view.isfoggedsphere(psize*wobble, p.o))
            {
                return;
            }
            vec dir = static_cast<vec>(o).sub(camera1->o), s, t;
            float dist = dir.magnitude();
            bool inside = dist <= psize*wobble;
            if(inside)
            {
                s = camright();
                t = camup();
            }
            else
            {
                float mag2 = dir.magnitude2();
                dir.x /= mag2;
                dir.y /= mag2;
                dir.z /= dist;
                s = vec(dir.y, -dir.x, 0);
                t = vec(dir.x*dir.z, dir.y*dir.z, -mag2/dist);
            }

            matrix3 rot(lastmillis/1000.0f*143/RAD, vec(1/SQRT3, 1/SQRT3, 1/SQRT3));
            LOCALPARAM(texgenS, rot.transposedtransform(s));
            LOCALPARAM(texgenT, rot.transposedtransform(t));

            matrix4 m(rot, o);
            m.scale(psize, psize, inside ? -psize : psize);
            m.mul(camprojmatrix, m);
            LOCALPARAM(explosionmatrix, m);

            LOCALPARAM(center, o);
            LOCALPARAMF(blendparams, inside ? 0.5f : 4, inside ? 0.25f : 0);
            if(2*(p.size + pmax)*wobble >= softexplosionblend)
            {
                LOCALPARAMF(softparams, -1.0f/softexplosionblend, 0, inside ? blend/(2*255.0f) : 0);
            }
            else
            {
                LOCALPARAMF(softparams, 0, -1, inside ? blend/(2*255.0f) : 0);
            }

            vec color = p.color.tocolor().mul(ldrscale);
            float alpha = blend/255.0f;

            for(int i = 0; i < (inside ? 2 : 1); ++i)
            {
                gle::color(color, i ? alpha/2 : alpha);
                if(i)
                {
                    glDepthFunc(GL_GEQUAL);
                }
                sr.draw();
                if(i)
                {
                    glDepthFunc(GL_LESS);
                }
            }
        }
    private:
        class sphererenderer
        {
            public:
                void cleanup()
                {
                    if(vbuf)
                    {
                        glDeleteBuffers(1, &vbuf);
                        vbuf = 0;
                    }
                    if(ebuf)
                    {
                        glDeleteBuffers(1, &ebuf);
                        ebuf = 0;
                    }
                }

                void enable()
                {
                    if(!vbuf)
                    {
                        init(12, 6); //12 slices, 6 stacks
                    }
                    gle::bindvbo(vbuf);
                    gle::bindebo(ebuf);

                    gle::vertexpointer(sizeof(vert), &verts->pos);
                    gle::texcoord0pointer(sizeof(vert), &verts->s, GL_UNSIGNED_SHORT, 2, GL_TRUE);
                    gle::enablevertex();
                    gle::enabletexcoord0();
                }

                void draw()
                {
                    glDrawRangeElements(GL_TRIANGLES, 0, numverts-1, numindices, GL_UNSIGNED_SHORT, indices);
                    xtraverts += numindices;
                    glde++;
                }

                void disable()
                {
                    gle::disablevertex();
                    gle::disabletexcoord0();

                    gle::clearvbo();
                    gle::clearebo();
                }
            private:
                struct vert
                {
                    vec pos;
                    ushort s, t;
                } *verts = nullptr;
                GLushort *indices = nullptr;
                int numverts   = 0,
                    numindices = 0;
                GLuint vbuf = 0,
                       ebuf = 0;

                void init(int slices, int stacks)
                {
                    numverts = (stacks+1)*(slices+1);
                    verts = new vert[numverts];
                    float ds = 1.0f/slices,
                          dt = 1.0f/stacks,
                          t  = 1.0f;
                    for(int i = 0; i < stacks+1; ++i)
                    {
                        float rho = M_PI*(1-t),
                              s = 0.0f,
                              sinrho = i && i < stacks ? std::sin(rho) : 0,
                              cosrho = !i ? 1 : (i < stacks ? std::cos(rho) : -1);
                        for(int j = 0; j < slices+1; ++j)
                        {
                            float theta = j==slices ? 0 : 2*M_PI*s;
                            vert &v = verts[i*(slices+1) + j];
                            v.pos = vec(std::sin(theta)*sinrho, std::cos(theta)*sinrho, -cosrho);
                            v.s = static_cast<ushort>(s*0xFFFF);
                            v.t = static_cast<ushort>(t*0xFFFF);
                            s += ds;
                        }
                        t -= dt;
                    }

                    numindices = (stacks-1)*slices*3*2;
                    indices = new ushort[numindices];
                    GLushort *curindex = indices;
                    for(int i = 0; i < stacks; ++i)
                    {
                        for(int k = 0; k < slices; ++k)
                        {
                            int j = i%2 ? slices-k-1 : k;
                            if(i)
                            {
                                *curindex++ = i*(slices+1)+j;
                                *curindex++ = (i+1)*(slices+1)+j;
                                *curindex++ = i*(slices+1)+j+1;
                            }
                            if(i+1 < stacks)
                            {
                                *curindex++ = i*(slices+1)+j+1;
                                *curindex++ = (i+1)*(slices+1)+j;
                                *curindex++ = (i+1)*(slices+1)+j+1;
                            }
                        }
                    }
                    if(!vbuf)
                    {
                        glGenBuffers(1, &vbuf);
                    }
                    gle::bindvbo(vbuf);
                    glBufferData(GL_ARRAY_BUFFER, numverts*sizeof(vert), verts, GL_STATIC_DRAW);
                    delete[] verts;
                    verts = nullptr;
                    if(!ebuf)
                    {
                        glGenBuffers(1, &ebuf);
                    }
                    gle::bindebo(ebuf);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numindices*sizeof(GLushort), indices, GL_STATIC_DRAW);
                    delete[] indices;
                    indices = nullptr;
                }
        };
        sphererenderer sr;

};
static fireballrenderer fireballs("media/particle/explosion.png"), pulsebursts("media/particle/pulse_burst.png");

//end explosion code

static partrenderer *parts[] =
{
    new varenderer<PT_PART>("<grey>media/particle/blood.png", PT_PART|PT_FLIP|PT_MOD|PT_RND4|PT_COLLIDE, Stain_Blood), // blood spats (note: rgb is inverted)
    new varenderer<PT_TRAIL>("media/particle/base.png", PT_TRAIL|PT_LERP),                                  // water, entity
    new varenderer<PT_PART>("<grey>media/particle/smoke.png", PT_PART|PT_FLIP|PT_LERP),                     // smoke
    new varenderer<PT_PART>("<grey>media/particle/steam.png", PT_PART|PT_FLIP),                             // steam
    new varenderer<PT_PART>("<grey>media/particle/flames.png", PT_PART|PT_HFLIP|PT_RND4|PT_BRIGHT),         // flame
    new varenderer<PT_TAPE>("media/particle/flare.png", PT_TAPE|PT_BRIGHT),                                 // streak
    new varenderer<PT_TAPE>("media/particle/rail_trail.png", PT_TAPE|PT_FEW|PT_BRIGHT),                     // rail trail
    new varenderer<PT_TAPE>("media/particle/pulse_side.png", PT_TAPE|PT_FEW|PT_BRIGHT),                     // pulse side
    new varenderer<PT_PART>("media/particle/pulse_front.png", PT_PART|PT_FLIP|PT_FEW|PT_BRIGHT),            // pulse front
    &fireballs,                                                                                             // explosion fireball
    &pulsebursts,                                                                                           // pulse burst
    new varenderer<PT_PART>("media/particle/spark.png", PT_PART|PT_FLIP|PT_BRIGHT),                         // sparks
    new varenderer<PT_PART>("media/particle/base.png",  PT_PART|PT_FLIP|PT_BRIGHT),                         // edit mode entities
    new varenderer<PT_PART>("media/particle/snow.png", PT_PART|PT_FLIP|PT_RND4|PT_COLLIDE),                 // colliding snow
    new varenderer<PT_PART>("media/particle/rail_muzzle.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),   // rail muzzle flash
    new varenderer<PT_PART>("media/particle/pulse_muzzle.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),  // pulse muzzle flash
    new varenderer<PT_PART>("media/interface/hud/items.png", PT_PART|PT_FEW|PT_ICON),                       // hud icon
    new varenderer<PT_PART>("<colorify:1/1/1>media/interface/hud/items.png", PT_PART|PT_FEW|PT_ICON),       // grey hud icon
    &meters,                                                                                                // meter
    &metervs,                                                                                               // meter vs.
};

//helper function to return int with # of entries in *parts[]
static constexpr size_t numparts()
{
    return sizeof(parts)/sizeof(parts[0]);
}

void initparticles(); //need to prototype either the vars or the the function

VARFP(maxparticles, 10, 4000, 10000, initparticles()); //maximum number of particle objects to create
VARFP(fewparticles, 10, 100, 10000, initparticles()); //if PT_FEW enabled, # of particles to create

void initparticles()
{
    if(initing)
    {
        return;
    }
    if(!particleshader)
    {
        particleshader = lookupshaderbyname("particle");
    }
    if(!particlenotextureshader)
    {
        particlenotextureshader = lookupshaderbyname("particlenotexture");
    }
    if(!particlesoftshader)
    {
        particlesoftshader = lookupshaderbyname("particlesoft");
    }
    if(!particletextshader)
    {
        particletextshader = lookupshaderbyname("particletext");
    }
    for(size_t i = 0; i < numparts(); ++i)
    {
        parts[i]->init(parts[i]->parttype()&PT_FEW ? std::min(fewparticles, maxparticles) : maxparticles);
    }
    for(size_t i = 0; i < numparts(); ++i)
    {
        loadprogress = static_cast<float>(i+1)/numparts();
        parts[i]->preload();
    }
    loadprogress = 0;
}

void clearparticles()
{
    for(size_t i = 0; i < numparts(); ++i)
    {
        parts[i]->reset();
    }
    clearparticleemitters();
}

void cleanupparticles()
{
    for(size_t i = 0; i < numparts(); ++i)
    {
        parts[i]->cleanup();
    }
}

void removetrackedparticles(physent *owner)
{
    for(size_t i = 0; i < numparts(); ++i)
    {
        parts[i]->resettracked(owner);
    }
}

static int debugparts = variable("debugparticles", 0, 0, 1, &debugparts, nullptr, 0);

void GBuffer::renderparticles(int layer) const
{
    canstep = layer != ParticleLayer_Under;

    //want to debug BEFORE the lastpass render (that would delete particles)
    if(debugparts && (layer == ParticleLayer_All || layer == ParticleLayer_Under))
    {
        for(size_t i = 0; i < numparts(); ++i)
        {
            parts[i]->debuginfo();
        }
    }

    bool rendered = false;
    uint lastflags = PT_LERP|PT_SHADER,
         flagmask = PT_LERP|PT_MOD|PT_BRIGHT|PT_NOTEX|PT_SOFT|PT_SHADER,
         excludemask = layer == ParticleLayer_All ? ~0 : (layer != ParticleLayer_NoLayer ? PT_NOLAYER : 0);

    for(size_t i = 0; i < numparts(); ++i)
    {
        partrenderer *p = parts[i];
        if((p->parttype()&PT_NOLAYER) == excludemask || !p->haswork())
        {
            continue;
        }
        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glActiveTexture(GL_TEXTURE2);
            if(msaalight)
            {
                glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
            }
            else
            {
                glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
            }
            glActiveTexture(GL_TEXTURE0);
        }

        uint flags = p->parttype() & flagmask,
             changedbits = flags ^ lastflags;
        if(changedbits)
        {
            if(changedbits&PT_LERP)
            {
                if(flags&PT_LERP)
                {
                    resetfogcolor();
                }
                else
                {
                    zerofogcolor();
                }
            }
            if(changedbits&(PT_LERP|PT_MOD))
            {
                if(flags&PT_LERP)
                {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
                else if(flags&PT_MOD)
                {
                    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                }
                else
                {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                }
            }
            if(!(flags&PT_SHADER))
            {
                if(changedbits&(PT_LERP|PT_SOFT|PT_NOTEX|PT_SHADER))
                {
                    if(flags&PT_SOFT)
                    {
                        particlesoftshader->set();
                        LOCALPARAMF(softparams, -1.0f/softparticleblend, 0, 0);
                    }
                    else if(flags&PT_NOTEX)
                    {
                        particlenotextureshader->set();
                    }
                    else
                    {
                        particleshader->set();
                    }
                }
                if(changedbits&(PT_MOD|PT_BRIGHT|PT_SOFT|PT_NOTEX|PT_SHADER))
                {
                    float colorscale = flags&PT_MOD ? 1 : ldrscale;
                    if(flags&PT_BRIGHT)
                    {
                        colorscale *= particlebright;
                    }
                    LOCALPARAMF(colorscale, colorscale, colorscale, colorscale, 1);
                }
            }
            lastflags = flags;
        }
        p->render();
    }
    if(rendered)
    {
        if(lastflags&(PT_LERP|PT_MOD))
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }
        if(!(lastflags&PT_LERP))
        {
            resetfogcolor();
        }
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

static int addedparticles = 0;

static particle *newparticle(const vec &o, const vec &d, int fade, int type, int color, float size, int gravity = 0)
{
    static particle dummy;
    if(seedemitter)
    {
        parts[type]->seedemitter(*seedemitter, o, d, fade, size, gravity);
        return &dummy;
    }
    if(fade + emitoffset < 0)
    {
        return &dummy;
    }
    addedparticles++;
    return parts[type]->addpart(o, d, fade, color, size, gravity);
}

VARP(maxparticledistance, 256, 1024, 4096); //cubits before particles stop rendering (1024 = 128m) (note that text particles have their own var)

static void splash(int type, int color, int radius, int num, int fade, const vec &p, float size, int gravity)
{
    if(camera1->o.dist(p) > maxparticledistance && !seedemitter)
    {
        return;
    }
    //ugly ternary assignment
    float collidez = parts[type]->parttype()&PT_COLLIDE ?
                     p.z - rootworld.raycube(p, vec(0, 0, -1), collideradius, Ray_ClipMat) + (parts[type]->hasstain() ? collideerror : 0) :
                     -1;
    int fmin = 1,
        fmax = fade*3;
    for(int i = 0; i < num; ++i)
    {
        int x, y, z;
        do
        {
            x = randomint(radius*2)-radius;
            y = randomint(radius*2)-radius;
            z = randomint(radius*2)-radius;
        } while(x*x + y*y + z*z > radius*radius);
        vec tmp = vec(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        int f = (num < 10) ? (fmin + randomint(fmax)) : (fmax - (i*(fmax-fmin))/(num-1)); //help deallocater by using fade distribution rather than random
        newparticle(p, tmp, f, type, color, size, gravity)->val = collidez;
    }
}

static void regularsplash(int type, int color, int radius, int num, int fade, const vec &p, float size, int gravity, int delay = 0)
{
    if(!canemitparticles() || (delay > 0 && randomint(delay) != 0))
    {
        return;
    }
    splash(type, color, radius, num, fade, p, size, gravity);
}

void regular_particle_splash(int type, int num, int fade, const vec &p, int color, float size, int radius, int gravity, int delay)
{
    if(minimized)
    {
        return;
    }
    regularsplash(type, color, radius, num, fade, p, size, gravity, delay);
}

void particle_splash(int type, int num, int fade, const vec &p, int color, float size, int radius, int gravity)
{
    if(minimized)
    {
        return;
    }
    splash(type, color, radius, num, fade, p, size, gravity);
}

VARP(maxtrail, 1, 500, 10000); //maximum number of steps allowed in a trail projectile

void particle_trail(int type, int fade, const vec &s, const vec &e, int color, float size, int gravity)
{
    if(minimized)
    {
        return;
    }
    vec v;
    float d = e.dist(s, v);
    int steps = std::clamp(static_cast<int>(d*2), 1, maxtrail);
    v.div(steps);
    vec p = s;
    for(int i = 0; i < steps; ++i)
    {
        p.add(v);
        //ugly long vec assignment
        vec tmp = vec(static_cast<float>(randomint(11)-5),
                      static_cast<float>(randomint(11)-5),
                      static_cast<float>(randomint(11)-5));
        newparticle(p, tmp, randomint(fade)+fade, type, color, size, gravity);
    }
}

VARP(particletext, 0, 1, 1);
VARP(maxparticletextdistance, 0, 128, 10000); //cubits at which text can be rendered (128 = 16m)

void particle_icon(const vec &s, int ix, int iy, int type, int fade, int color, float size, int gravity)
{
    if(minimized)
    {
        return;
    }
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, gravity);
    p->flags |= ix | (iy<<2);
}

void particle_meter(const vec &s, float val, int type, int fade, int color, int color2, float size)
{
    if(minimized)
    {
        return;
    }
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size);
    p->color2[0] = color2>>16;
    p->color2[1] = (color2>>8)&0xFF;
    p->color2[2] = color2&0xFF;
    p->progress = std::clamp(static_cast<int>(val*100), 0, 100);
}

void particle_flare(const vec &p, const vec &dest, int fade, int type, int color, float size, physent *owner)
{
    if(minimized)
    {
        return;
    }
    newparticle(p, dest, fade, type, color, size)->owner = owner;
}

void particle_fireball(const vec &dest, float maxsize, int type, int fade, int color, float size)
{
    if(minimized)
    {
        return;
    }
    float growth = maxsize - size;
    if(fade < 0)
    {
        fade = static_cast<int>(growth*20);
    }
    newparticle(dest, vec(0, 0, 1), fade, type, color, size)->val = growth;
}

//dir = 0..6 where 0=up
static vec offsetvec(vec o, int dir, int dist)
{
    vec v = vec(o);
    v[(2+dir)%3] += (dir>2)?(-dist):dist;
    return v;
}

//converts a 16bit color to 24bit
static int colorfromattr(int attr)
{
    return (((attr&0xF)<<4) | ((attr&0xF0)<<8) | ((attr&0xF00)<<12)) + 0x0F0F0F;
}

/* Experiments in shapes...
 * dir: (where dir%3 is similar to offsetvec with 0=up)
 * 0..2 circle
 * 3.. 5 cylinder shell
 * 6..11 cone shell
 * 12..14 plane volume
 * 15..20 line volume, i.e. wall
 * 21 sphere
 * 24..26 flat plane
 * +32 to inverse direction
 */
static void regularshape(int type, int radius, int color, int dir, int num, int fade, const vec &p, float size, int gravity, int vel = 200)
{
    if(!canemitparticles())
    {
        return;
    }
    int basetype = parts[type]->parttype()&0xFF;
    bool flare = (basetype == PT_TAPE),
         inv = (dir&0x20)!=0,
         taper = (dir&0x40)!=0 && !seedemitter;
    dir &= 0x1F;
    for(int i = 0; i < num; ++i)
    {
        vec to, from;
        if(dir < 12)
        {
            const vec2 &sc = sincos360[randomint(360)];
            to[dir%3] = sc.y*radius;
            to[(dir+1)%3] = sc.x*radius;
            to[(dir+2)%3] = 0.0;
            to.add(p);
            if(dir < 3) //circle
            {
                from = p;
            }
            else if(dir < 6) //cylinder
            {
                from = to;
                to[(dir+2)%3] += radius;
                from[(dir+2)%3] -= radius;
            }
            else //cone
            {
                from = p;
                to[(dir+2)%3] += (dir < 9)?radius:(-radius);
            }
        }
        else if(dir < 15) //plane
        {
            to[dir%3] = static_cast<float>(randomint(radius<<4)-(radius<<3))/8.0;
            to[(dir+1)%3] = static_cast<float>(randomint(radius<<4)-(radius<<3))/8.0;
            to[(dir+2)%3] = radius;
            to.add(p);
            from = to;
            from[(dir+2)%3] -= 2*radius;
        }
        else if(dir < 21) //line
        {
            if(dir < 18)
            {
                to[dir%3] = static_cast<float>(randomint(radius<<4)-(radius<<3))/8.0;
                to[(dir+1)%3] = 0.0;
            }
            else
            {
                to[dir%3] = 0.0;
                to[(dir+1)%3] = static_cast<float>(randomint(radius<<4)-(radius<<3))/8.0;
            }
            to[(dir+2)%3] = 0.0;
            to.add(p);
            from = to;
            to[(dir+2)%3] += radius;
        }
        else if(dir < 24) //sphere
        {
            to = vec(2*M_PI*static_cast<float>(randomint(1000))/1000.0, M_PI*static_cast<float>(randomint(1000)-500)/1000.0).mul(radius);
            to.add(p);
            from = p;
        }
        else if(dir < 27) // flat plane
        {
            to[dir%3] = static_cast<float>(randomfloat(2*radius)-radius);
            to[(dir+1)%3] = static_cast<float>(randomfloat(2*radius)-radius);
            to[(dir+2)%3] = 0.0;
            to.add(p);
            from = to;
        }
        else
        {
            from = to = p;
        }
        if(inv)
        {
            std::swap(from, to);
        }
        if(taper)
        {
            float dist = std::clamp(from.dist2(camera1->o)/maxparticledistance, 0.0f, 1.0f);
            if(dist > 0.2f)
            {
                dist = 1 - (dist - 0.2f)/0.8f;
                if(randomint(0x10000) > dist*dist*0xFFFF)
                {
                    continue;
                }
            }
        }
        if(flare)
        {
            newparticle(from, to, randomint(fade*3)+1, type, color, size, gravity);
        }
        else
        {
            vec d = vec(to).sub(from).rescale(vel); //velocity
            particle *n = newparticle(from, d, randomint(fade*3)+1, type, color, size, gravity);
            if(parts[type]->parttype()&PT_COLLIDE)
            {
                //long nasty ternary assignment
                n->val = from.z - rootworld.raycube(from, vec(0, 0, -1), parts[type]->hasstain() ?
                         collideradius :
                         std::max(from.z, 0.0f), Ray_ClipMat) + (parts[type]->hasstain() ? collideerror : 0);
            }
        }
    }
}

static void regularflame(int type, const vec &p, float radius, float height, int color, int density = 3, float scale = 2.0f, float speed = 200.0f, float fade = 600.0f, int gravity = -15)
{
    if(!canemitparticles())
    {
        return;
    }
    float size = scale * std::min(radius, height);
    vec v(0, 0, std::min(1.0f, height)*speed);
    for(int i = 0; i < density; ++i)
    {
        vec s = p;
        s.x += randomfloat(radius*2.0f)-radius;
        s.y += randomfloat(radius*2.0f)-radius;
        newparticle(s, v, randomint(std::max(static_cast<int>(fade*height), 1))+1, type, color, size, gravity);
    }
}

void regular_particle_flame(int type, const vec &p, float radius, float height, int color, int density, float scale, float speed, float fade, int gravity)
{
    if(minimized)
    {
        return;
    }
    regularflame(type, p, radius, height, color, density, scale, speed, fade, gravity);
}
void cubeworld::makeparticles(const entity &e)
{
    switch(e.attr1)
    {
        case 0: //fire and smoke -  <radius> <height> <rgb> - 0 values default to compat for old maps
        {
            float radius = e.attr2 ? static_cast<float>(e.attr2)/100.0f : 1.5f,
                  height = e.attr3 ? static_cast<float>(e.attr3)/100.0f : radius/3;
            regularflame(Part_Flame, e.o, radius, height, e.attr4 ? colorfromattr(e.attr4) : 0x903020, 3, 2.0f);
            regularflame(Part_Smoke, vec(e.o.x, e.o.y, e.o.z + 4.0f*std::min(radius, height)), radius, height, 0x303020, 1, 4.0f, 100.0f, 2000.0f, -20);
            break;
        }
        case 1: //steam vent - <dir>
        {
            regularsplash(Part_Steam, 0x897661, 50, 1, 200, offsetvec(e.o, e.attr2, randomint(10)), 2.4f, -20);
            break;
        }
        case 2: //water fountain - <dir>
        {
            int color;
            if(e.attr3 > 0)
            {
                color = colorfromattr(e.attr3);
            }
            else
            {
                int mat = Mat_Water + std::clamp(-e.attr3, 0, 3);
                color = getwaterfallcolor(mat).tohexcolor();
                if(!color)
                {
                    color = getwatercolor(mat).tohexcolor();
                }
            }
            regularsplash(Part_Water, color, 150, 4, 200, offsetvec(e.o, e.attr2, randomint(10)), 0.6f, 2);
            break;
        }
        case 3: //fire ball - <size> <rgb>
        {
            newparticle(e.o, vec(0, 0, 1), 1, Part_Explosion, colorfromattr(e.attr3), 4.0f)->val = 1+e.attr2;
            break;
        }
        case 4:  //tape - <dir> <length> <rgb>
        case 9:  //steam
        case 10: //water
        case 13: //snow
        {
            static constexpr int typemap[]   = { Part_Streak, -1, -1, -1, -1, Part_Steam, Part_Water, -1, -1, Part_Snow };
            static constexpr float sizemap[] = { 0.28f, 0.0f, 0.0f, 1.0f, 0.0f, 2.4f, 0.60f, 0.0f, 0.0f, 0.5f };
            static constexpr int gravmap[]   = { 0, 0, 0, 0, 0, -20, 2, 0, 0, 20 };
            int type = typemap[e.attr1-4];
            float size = sizemap[e.attr1-4];
            int gravity = gravmap[e.attr1-4];
            if(e.attr2 >= 256)
            {
                regularshape(type, std::max(1+e.attr3, 1), colorfromattr(e.attr4), e.attr2-256, 5, e.attr5 > 0 ? std::min(static_cast<int>(e.attr5), 10000) : 200, e.o, size, gravity);
            }
            else
            {
                newparticle(e.o, offsetvec(e.o, e.attr2, std::max(1+e.attr3, 0)), 1, type, colorfromattr(e.attr4), size, gravity);
            }
            break;
        }
        case 5: //meter, metervs - <percent> <rgb> <rgb2>
        case 6:
        {
            particle *p = newparticle(e.o, vec(0, 0, 1), 1, e.attr1==5 ? Part_Meter : Part_MeterVS, colorfromattr(e.attr3), 2.0f);
            int color2 = colorfromattr(e.attr4);
            p->color2[0] = color2>>16;
            p->color2[1] = (color2>>8)&0xFF;
            p->color2[2] = color2&0xFF;
            p->progress = std::clamp(static_cast<int>(e.attr2), 0, 100);
            break;
        }
        case 11: // flame <radius> <height> <rgb> - radius=100, height=100 is the classic size
        {
            regularflame(Part_Flame, e.o, static_cast<float>(e.attr2)/100.0f, static_cast<float>(e.attr3)/100.0f, colorfromattr(e.attr4), 3, 2.0f);
            break;
        }
        case 12: // smoke plume <radius> <height> <rgb>
        {
            regularflame(Part_Smoke, e.o, static_cast<float>(e.attr2)/100.0f, static_cast<float>(e.attr3)/100.0f, colorfromattr(e.attr4), 1, 4.0f, 100.0f, 2000.0f, -20);
            break;
        }
        default:
        {
            break;
        }
    }
}

void cubeworld::seedparticles()
{
    renderprogress(0, "seeding particles");
    addparticleemitters();
    canemit = true;
    for(particleemitter &pe : emitters)
    {
        const extentity &e = *pe.ent;
        seedemitter = &pe;
        for(int millis = 0; millis < seedmillis; millis += std::min(emitmillis, seedmillis/10))
        {
            makeparticles(e);
        }
        seedemitter = nullptr;
        pe.lastemit = -seedmillis;
        pe.finalize();
    }
}

void cubeworld::updateparticles()
{
    //note: static int carried across all calls of function
    static int lastemitframe = 0;

    if(regenemitters) //regenemitters called whenever a new particle generator is placed
    {
        addparticleemitters();
    }
    if(minimized) //don't emit particles unless window visible
    {
        canemit = false;
        return;
    }
    if(lastmillis - lastemitframe >= emitmillis) //don't update particles too often
    {
        canemit = true;
        lastemitframe = lastmillis - (lastmillis%emitmillis);
    }
    else
    {
        canemit = false;
    }
    for(size_t i = 0; i < numparts(); ++i)
    {
        parts[i]->update();
    }
    if(!editmode || showparticles)
    {
        int emitted = 0,
            replayed = 0;
        addedparticles = 0;
        for(particleemitter& pe : emitters) //foreach particle emitter
        {
            const extentity &e = *pe.ent; //get info for the entity associated w/ent
            if(e.o.dist(camera1->o) > maxparticledistance) //distance check (don't update faraway particle ents)
            {
                pe.lastemit = lastmillis;
                continue;
            }
            if(cullparticles && pe.maxfade >= 0)
            {
                if(view.isfoggedsphere(pe.radius, pe.center))
                {
                    pe.lastcull = lastmillis;
                    continue;
                }
            }
            makeparticles(e);
            emitted++;
            if(replayparticles && pe.maxfade > 5 && pe.lastcull > pe.lastemit) //recreate particles from previous ticks
            {
                for(emitoffset = std::max(pe.lastemit + emitmillis - lastmillis, -pe.maxfade); emitoffset < 0; emitoffset += emitmillis)
                {
                    makeparticles(e);
                    replayed++;
                }
                emitoffset = 0;
            }
            pe.lastemit = lastmillis;
        }
        if(debugparticlecull && (canemit || replayed) && addedparticles)
        {
            conoutf(Console_Debug, "%d emitters, %d particles", emitted, addedparticles);
        }
    }
    if(editmode) // show sparkly thingies for map entities in edit mode
    {
        const std::vector<extentity *> &ents = entities::getents();
        for(extentity * const &e : ents)
        {
            regular_particle_splash(Part_Edit, 2, 40, e->o, 0x3232FF, 0.32f*particlesize/100.0f);
        }
    }
}

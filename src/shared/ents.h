// this file defines static map entities ("entity") and dynamic entities (players/monsters, "dynent")
// the gamecode extends these types to add game specific functionality

// Ent_*: the only static entity types dictated by the engine... rest are gamecode dependent

enum
{
    EngineEnt_Empty=0,
    EngineEnt_Light,
    EngineEnt_Mapmodel,
    EngineEnt_Playerstart,
    EngineEnt_Particles,
    EngineEnt_Sound,
    EngineEnt_Spotlight,
    EngineEnt_Decal,
    EngineEnt_GameSpecific,
};

struct entity                                   // persistent map entity
{
    vec o;                                      // position
    short attr1, attr2, attr3, attr4, attr5;    // attributes
    uchar type;                                 // type is one of the above
    uchar reserved;
};

enum
{
    EntFlag_NoVis      = 1<<0,
    EntFlag_NoShadow   = 1<<1,
    EntFlag_NoCollide  = 1<<2,
    EntFlag_Anim       = 1<<3,
    EntFlag_ShadowMesh = 1<<4,
    EntFlag_Octa       = 1<<5,
    EntFlag_Render     = 1<<6,
    EntFlag_Sound      = 1<<7,
    EntFlag_Spawned    = 1<<8,

};

struct extentity : entity                       // part of the entity that doesn't get saved to disk
{
    int flags;
    extentity *attached;

    extentity() : flags(0), attached(NULL) {}

    bool spawned() const { return (flags&EntFlag_Spawned) != 0; }
    void setspawned(bool val) { if(val) flags |= EntFlag_Spawned; else flags &= ~EntFlag_Spawned; }
    void setspawned() { flags |= EntFlag_Spawned; }
    void clearspawned() { flags &= ~EntFlag_Spawned; }
};

#define MAXENTS 10000

//extern vector<extentity *> ents;                // map entities

enum
{
    ClientState_Alive = 0,
    ClientState_Dead,
    ClientState_Spawning,
    ClientState_Lagged,
    ClientState_Editing,
    ClientState_Spectator,
};

enum
{
    PhysEntState_Float = 0,
    PhysEntState_Fall,
    PhysEntState_Slide,
    PhysEntState_Slope,
    PhysEntState_Floor,
    PhysEntState_StepUp,
    PhysEntState_StepDown,
    PhysEntState_Bounce,
};

enum
{
    PhysEnt_Player = 0,
    PhysEnt_Camera,
    PhysEnt_Bounce,
};

enum
{
    Collide_None = 0,
    Collide_Ellipse,
    Collide_OrientedBoundingBox,
    Collide_TRI
};

#define CROUCHTIME 200
#define CROUCHHEIGHT 0.75f

struct physent                                  // base entity type, can be affected by physics
{
    vec o, vel, falling;                        // origin, velocity
    vec deltapos, newpos;                       // movement interpolation
    float yaw, pitch, roll;
    float maxspeed;                             // cubes per second, 100 for player
    int timeinair;
    float radius, eyeheight, maxheight, aboveeye; // bounding box size
    float xradius, yradius, zmargin;
    vec floor;                                  // the normal of floor the dynent is on

    int inwater;
    bool jumping;
    char move, strafe, crouching;

    uchar physstate;                            // one of PHYS_* above
    uchar state, editstate;                     // one of CS_* above
    uchar type;                                 // one of ENT_* above
    uchar collidetype;                          // one of COLLIDE_* above

    bool blocked;                               // used by physics to signal ai

    physent() : o(0, 0, 0), deltapos(0, 0, 0), newpos(0, 0, 0), yaw(0), pitch(0), roll(0), maxspeed(100),
               radius(4.1f), eyeheight(18), maxheight(18), aboveeye(2), xradius(4.1f), yradius(4.1f), zmargin(0),
               state(ClientState_Alive), editstate(ClientState_Alive), type(PhysEnt_Player),
               collidetype(Collide_Ellipse),
               blocked(false)
               { reset(); }

    void resetinterp()
    {
        newpos = o;
        deltapos = vec(0, 0, 0);
    }

    void reset()
    {
        inwater = 0;
        timeinair = 0;
        eyeheight = maxheight;
        jumping = false;
        strafe = move = crouching = 0;
        physstate = PhysEntState_Fall;
        vel = falling = vec(0, 0, 0);
        floor = vec(0, 0, 1);
    }

    vec feetpos(float offset = 0) const { return vec(o).addz(offset - eyeheight); }
    vec headpos(float offset = 0) const { return vec(o).addz(offset); }

    bool crouched() const { return fabs(eyeheight - maxheight*CROUCHHEIGHT) < 1e-4f; }
};

enum
{
    Anim_Mapmodel = 0,
    Anim_GameSpecific
};

#define ANIM_ALL         0x1FF
#define ANIM_INDEX       0x1FF
#define ANIM_LOOP        (1<<9)
#define ANIM_CLAMP       (1<<10)
#define ANIM_REVERSE     (1<<11)
#define ANIM_START       (ANIM_LOOP|ANIM_CLAMP)
#define ANIM_END         (ANIM_LOOP|ANIM_CLAMP|ANIM_REVERSE)
#define ANIM_DIR         0xE00
#define ANIM_SECONDARY   12
#define ANIM_REUSE       0xFFFFFF
#define ANIM_NOSKIN      (1<<24)
#define ANIM_SETTIME     (1<<25)
#define ANIM_FULLBRIGHT  (1<<26)
#define ANIM_NORENDER    (1<<27)
#define ANIM_RAGDOLL     (1<<28)
#define ANIM_SETSPEED    (1<<29)
#define ANIM_NOPITCH     (1<<30)
#define ANIM_FLAGS       0xFF000000

struct animinfo // description of a character's animation
{
    int anim, frame, range, basetime;
    float speed;
    uint varseed;

    animinfo() : anim(0), frame(0), range(0), basetime(0), speed(100.0f), varseed(0) { }

    bool operator==(const animinfo &o) const { return frame==o.frame && range==o.range && (anim&(ANIM_SETTIME|ANIM_DIR))==(o.anim&(ANIM_SETTIME|ANIM_DIR)) && (anim&ANIM_SETTIME || basetime==o.basetime) && speed==o.speed; }
    bool operator!=(const animinfo &o) const { return frame!=o.frame || range!=o.range || (anim&(ANIM_SETTIME|ANIM_DIR))!=(o.anim&(ANIM_SETTIME|ANIM_DIR)) || (!(anim&ANIM_SETTIME) && basetime!=o.basetime) || speed!=o.speed; }
};

struct animinterpinfo // used for animation blending of animated characters
{
    animinfo prev, cur;
    int lastswitch;
    void *lastmodel;

    animinterpinfo() : lastswitch(-1), lastmodel(NULL) {}

    void reset() { lastswitch = -1; }
};

#define MAXANIMPARTS 3

struct occludequery;
struct ragdolldata;

struct dynent : physent                         // animated characters, or characters that can receive input
{
    bool k_left, k_right, k_up, k_down;         // see input code

    animinterpinfo animinterp[MAXANIMPARTS];
    ragdolldata *ragdoll;
    occludequery *query;
    int lastrendered;

    dynent() : ragdoll(NULL), query(NULL), lastrendered(0)
    {
        reset();
    }

    ~dynent()
    {
#ifndef STANDALONE
        extern void cleanragdoll(dynent *d);
        if(ragdoll) cleanragdoll(this);
#endif
    }

    void stopmoving()
    {
        k_left = k_right = k_up = k_down = jumping = false;
        move = strafe = crouching = 0;
    }

    void reset()
    {
        physent::reset();
        stopmoving();
        for(int i = 0; i < MAXANIMPARTS; ++i)
        {
            animinterp[i].reset();
        }
    }

    vec abovehead() { return vec(o).addz(aboveeye+4); }
};

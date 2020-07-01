#ifndef CONSTS_H_
#define CONSTS_H_
/* consts.h 
 * 
 * This is a header filled with constants that are useful to pass appropriate information
 * from the game to the engine; it is only for those constants which are directly useful for
 * the engine-game interface and not for constants which solely lie in the engine or game
 */

#define MAXCLIENTS 128                 // DO NOT set this any higher
#define MAXTRANS 5000                  // max amount of data to swallow in 1 go

#define IS_LIQUID(mat) ((mat)==Mat_Water)
#define IS_CLIPPED(mat) ((mat)==Mat_Glass) //materials that are obligate clipping (always also get clipped)
#define LOOP_START(id, stack) if((id)->type != Id_Alias) return; identstack stack;

struct selinfo
{
    int corner;
    int cx, cxs, cy, cys;
    ivec o, s;
    int grid, orient;
    selinfo() : corner(0), cx(0), cxs(0), cy(0), cys(0), o(0, 0, 0), s(0, 0, 0), grid(8), orient(0) {}
    int size() const    { return s.x*s.y*s.z; }
    int us(int d) const { return s[d]*grid; }
    bool operator==(const selinfo &sel) const { return o==sel.o && s==sel.s && grid==sel.grid && orient==sel.orient; }
    bool validate()
    {
        extern int worldsize;
        if(grid <= 0 || grid >= worldsize)
        {
            return false;
        }
        if(o.x >= worldsize || o.y >= worldsize || o.z >= worldsize)
        {
            return false;
        }
        if(o.x < 0)
        {
            s.x -= (grid - 1 - o.x)/grid;
            o.x = 0;
        }
        if(o.y < 0)
        {
            s.y -= (grid - 1 - o.y)/grid;
            o.y = 0;
        }
        if(o.z < 0)
        {
            s.z -= (grid - 1 - o.z)/grid;
            o.z = 0;
        }
        s.x = clamp(s.x, 0, (worldsize - o.x)/grid);
        s.y = clamp(s.y, 0, (worldsize - o.y)/grid);
        s.z = clamp(s.z, 0, (worldsize - o.z)/grid);
        return s.x > 0 && s.y > 0 && s.z > 0;
    }
};

struct servinfo
{
    string name, map, desc;
    int protocol, numplayers, maxplayers, ping;
    vector<int> attr;

    servinfo() : protocol(INT_MIN), numplayers(0), maxplayers(0)
    {
        name[0] = map[0] = desc[0] = '\0';
    }
};

struct model;

struct modelattach
{
    const char *tag, *name;
    int anim, basetime;
    vec *pos;
    model *m;

    modelattach() : tag(NULL), name(NULL), anim(-1), basetime(0), pos(NULL), m(NULL) {}
    modelattach(const char *tag, const char *name, int anim = -1, int basetime = 0) : tag(tag), name(name), anim(anim), basetime(basetime), pos(NULL), m(NULL) {}
    modelattach(const char *tag, vec *pos) : tag(tag), name(NULL), anim(-1), basetime(0), pos(pos), m(NULL) {}
};

enum
{
    MatFlag_IndexShift  = 0,
    MatFlag_VolumeShift = 2,
    MatFlag_ClipShift   = 5,
    MatFlag_FlagShift   = 8,

    MatFlag_Index  = 3 << MatFlag_IndexShift,
    MatFlag_Volume = 7 << MatFlag_VolumeShift,
    MatFlag_Clip   = 7 << MatFlag_ClipShift,
    MatFlag_Flags  = 0xFF << MatFlag_FlagShift
};


enum
{
    Edit_Face = 0,
    Edit_Tex,
    Edit_Mat,
    Edit_Flip,
    Edit_Copy,
    Edit_Paste,
    Edit_Rotate,
    Edit_Replace,
    Edit_DelCube,
    Edit_CalcLight,
    Edit_Remip,
    Edit_VSlot,
    Edit_Undo,
    Edit_Redo
};

enum
{
    Ray_BB = 1,
    Ray_Poly = 3,
    Ray_AlphaPoly = 7,
    Ray_Ents = 9,
    Ray_ClipMat = 16,
    Ray_SkipFirst = 32,
    Ray_EditMat = 64,
    Ray_Shadow = 128,
    Ray_Pass = 256,
    Ray_SkipSky = 512
};

enum // cube empty-space materials
{
    Mat_Air      = 0,                      // the default, fill the empty space with air
    Mat_Water    = 1 << MatFlag_VolumeShift, // fill with water, showing waves at the surface
    Mat_Glass    = 3 << MatFlag_VolumeShift, // behaves like clip but is blended blueish

    Mat_NoClip   = 1 << MatFlag_ClipShift,  // collisions always treat cube as empty
    Mat_Clip     = 2 << MatFlag_ClipShift,  // collisions always treat cube as solid
    Mat_GameClip = 3 << MatFlag_ClipShift,  // game specific clip material

    Mat_Death    = 1 << MatFlag_FlagShift,  // force player suicide
    Mat_Alpha    = 4 << MatFlag_FlagShift   // alpha blended
};

enum
{
    Console_Info  = 1<<0,
    Console_Warn  = 1<<1,
    Console_Error = 1<<2,
    Console_Debug = 1<<3,
    Console_Init  = 1<<4,
    Console_Echo  = 1<<5
};

// renderlights

enum
{
    LightEnt_NoShadow   = 1<<0,
    LightEnt_Static     = 1<<1,
    LightEnt_Volumetric = 1<<2,
    LightEnt_NoSpecular = 1<<3
};

// dynlight
enum
{
    DynLight_Shrink = 1<<8,
    DynLight_Expand = 1<<9,
    DynLight_Flash  = 1<<10
};

//particles
enum
{
    Part_Blood = 0,
    Part_Water,
    Part_Smoke,
    Part_Steam,
    Part_Flame,
    Part_Streak,
    Part_RailTrail,
    Part_PulseSide,
    Part_PulseFront,
    Part_Explosion,
    Part_PulseBurst,
    Part_Spark,
    Part_Edit,
    Part_Snow,
    Part_RailMuzzleFlash,
    Part_PulseMuzzleFlash,
    Part_HUDIcon,
    Part_HUDIconGrey,
    Part_Text,
    Part_Meter,
    Part_MeterVS,
};

//stain
enum
{
    Stain_Blood = 0,
    Stain_PulseScorch,
    Stain_RailHole,
    Stain_PulseGlow,
    Stain_RailGlow
};

//models
enum
{
    Model_CullVFC          = 1<<0,
    Model_CullDist         = 1<<1,
    Model_CullOccluded     = 1<<2,
    Model_CullQuery        = 1<<3,
    Model_FullBright       = 1<<4,
    Model_NoRender         = 1<<5,
    Model_Mapmodel         = 1<<6,
    Model_NoBatch          = 1<<7,
    Model_OnlyShadow       = 1<<8,
    Model_NoShadow         = 1<<9,
    Model_ForceShadow      = 1<<10,
    Model_ForceTransparent = 1<<11
};

//sound
enum
{
    Music_Map     = 1<<0,
    Music_NoAlt  = 1<<1,
    Music_UseAlt = 1<<2
};

//server
enum
{
    Discon_None = 0,
    Discon_EndOfPacket,
    Discon_Local,
    Discon_Kick,
    Discon_MsgError,
    Discon_IPBan,
    Discon_Private,
    Discon_MaxClients,
    Discon_Timeout,
    Discon_Overflow,
    Discon_Password,
    Discon_NumDiscons
};

#endif

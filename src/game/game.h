#ifndef __GAME_H__
#define __GAME_H__

#include "cube.h"

//defines game statics, like animation names, weapon variables, entity properties
//includes:
//animation names
//console message types
//weapon vars
//game state information
//game entity definition

// animations
// used in render.cpp
enum
{
    Anim_Dead = Anim_GameSpecific, //1
    Anim_Dying,
    Anim_Idle,
    Anim_RunN,
    Anim_RunNE,
    Anim_RunE,
    Anim_RunSE,
    Anim_RunS,
    Anim_RunSW,
    Anim_RunW, //10
    Anim_RunNW,
    Anim_Jump,
    Anim_JumpN,
    Anim_JumpNE,
    Anim_JumpE,
    Anim_JumpSE,
    Anim_JumpS,
    Anim_JumpSW,
    Anim_JumpW,
    Anim_JumpNW, //20
    Anim_Sink,
    Anim_Swim,
    Anim_Crouch,
    Anim_CrouchN,
    Anim_CrouchNE,//unused
    Anim_CrouchE,//unused
    Anim_CrouchSE,//unused
    Anim_CrouchS,//unused
    Anim_CrouchSW,//unused
    Anim_CrouchW, //30 (unused)
    Anim_CrouchNW,//unused
    Anim_CrouchJump,
    Anim_CrouchJumpN,
    Anim_CrouchJumpNE,//unused
    Anim_CrouchJumpE,//unused
    Anim_CrouchJumpSE,//unused
    Anim_CrouchJumpS,//unused
    Anim_CrouchJumpSW,//unused
    Anim_CrouchJumpW,//unused
    Anim_CrouchJumpNW, //40 (unused)
    Anim_CrouchSink,
    Anim_CrouchSwim,
    Anim_Shoot,
    Anim_Melee,
    Anim_Pain,
    Anim_Edit,
    Anim_Lag,
    Anim_Taunt,
    Anim_Win,
    Anim_Lose, //50
    Anim_GunIdle,
    Anim_GunShoot,
    Anim_GunMelee,
    Anim_VWepIdle,
    Anim_VWepShoot,
    Anim_VWepMelee,
    Anim_NumAnims //57
};

static const char * const animnames[] =
{
    "mapmodel",
    "dead", "dying",
    "idle", "run N", "run NE", "run E", "run SE", "run S", "run SW", "run W", "run NW",
    "jump", "jump N", "jump NE", "jump E", "jump SE", "jump S", "jump SW", "jump W", "jump NW",
    "sink", "swim",
    "crouch", "crouch N", "crouch NE", "crouch E", "crouch SE", "crouch S", "crouch SW", "crouch W", "crouch NW",
    "crouch jump", "crouch jump N", "crouch jump NE", "crouch jump E", "crouch jump SE", "crouch jump S", "crouch jump SW", "crouch jump W", "crouch jump NW",
    "crouch sink", "crouch swim",
    "shoot", "melee",
    "pain",
    "edit", "lag", "taunt", "win", "lose",
    "gun idle", "gun shoot", "gun melee",
    "vwep idle", "vwep shoot", "vwep melee"
};

// console message types

enum
{
    ConsoleMsg_Chat       = 1<<8,
    ConsoleMsg_TeamChat   = 1<<9,
    ConsoleMsg_GameInfo   = 1<<10,
    ConsoleMsg_FragSelf   = 1<<11,
    ConsoleMsg_FragOther  = 1<<12,
    ConsoleMsg_TeamKill   = 1<<13
};

// network quantization scale
#define DMF 16.0f                // for world locations
#define DNF 100.0f              // for normalized vectors
#define DVELF 1.0f              // for playerspeed based velocity vectors

//these are called "GamecodeEnt" to avoid name collision (as opposed to gameents or engine static ents)
enum                            // static entity types
{
    GamecodeEnt_NotUsed              = EngineEnt_Empty,        // entity slot not in use in map
    GamecodeEnt_Light                = EngineEnt_Light,        // lightsource, attr1 = radius, attr2 = red, attr3 = green, attr4 = blue, attr5 = flags
    GamecodeEnt_Mapmodel             = EngineEnt_Mapmodel,     // attr1 = index, attr2 = yaw, attr3 = pitch, attr4 = roll, attr5 = scale
    GamecodeEnt_Playerstart,                                   // attr1 = angle, attr2 = team
    GamecodeEnt_Particles            = EngineEnt_Particles,    // attr1 = index, attrs2-5 vary on particle index
    GamecodeEnt_MapSound             = EngineEnt_Sound,        // attr1 = index, attr2 = sound
    GamecodeEnt_Spotlight            = EngineEnt_Spotlight,    // attr1 = angle
    GamecodeEnt_Decal                = EngineEnt_Decal,        // attr1 = index, attr2 = yaw, attr3 = pitch, attr4 = roll, attr5 = scale
    GamecodeEnt_Teleport,                                      // attr1 = channel, attr2 = model, attr3 = tag
    GamecodeEnt_Teledest,                                      // attr1 = yaw, attr2 = channel
    GamecodeEnt_Jumppad,                                       // attr1 = zpush, attr2 = ypush, attr3 = xpush
    GamecodeEnt_Flag,                                          // attr1 = yaw, attr2 = team
    GamecodeEnt_MaxEntTypes,                                   // used for looping through full enum
};

//the equivalent of the engine's extentity (and identical)
struct gameentity : extentity
{
};

enum
{
    Gun_Rail = 0,
    Gun_Pulse,
    Gun_NumGuns
};
enum
{
    Act_Idle = 0,
    Act_Shoot,
    Act_Melee,
    Act_NumActs
};

enum
{
    Attack_RailShot = 0,
    Attack_RailMelee,
    Attack_PulseShoot,
    Attack_PulseMelee,
    Attack_NumAttacks
};

#define VALID_GUN(n) ((n) >= 0 && (n) < Gun_NumGuns)
#define VALID_ATTACK(n) ((n) >= 0 && (n) < Attack_NumAttacks)

//enum of gameplay mechanic flags; bitwise sum determines what a mode's attributes are
enum
{
    Mode_Team           = 1<<0,
    Mode_CTF            = 1<<1,
    Mode_AllowOvertime  = 1<<2,
    Mode_Edit           = 1<<3,
    Mode_Demo           = 1<<4,
    Mode_LocalOnly      = 1<<5,
    Mode_Lobby          = 1<<6,
    Mode_Rail           = 1<<7,
    Mode_Pulse          = 1<<8,
    Mode_All            = 1<<9
};

enum
{
    Mode_Untimed         = Mode_Edit|Mode_LocalOnly|Mode_Demo,
    Mode_Bot             = Mode_LocalOnly|Mode_Demo
};

static struct gamemodeinfo
{
    const char *name, *prettyname;
    int flags;
    const char *info;
} gamemodes[] =
//list of valid game modes with their name/prettyname/game flags/desc
{
    { "demo", "Demo", Mode_Demo | Mode_LocalOnly, NULL},
    { "edit", "Edit", Mode_Edit | Mode_All, "Cooperative Editing:\nEdit maps with multiple players simultaneously." },
    { "ctf", "CTF", Mode_CTF | Mode_Team | Mode_All, "Capture The Flag:\nCapture \fs\f3the enemy flag\fr and bring it back to \fs\f1your flag\fr to score points for \fs\f1your team\fr." },
};

//these are the checks for particular mechanics in particular modes
//e.g. MODE_RAIL sees if the mode only have railguns
#define STARTGAMEMODE (-1)
#define NUMGAMEMODES ((int)(sizeof(gamemodes)/sizeof(gamemodes[0])))

//check fxn
inline bool modecheck(int mode, int flag)
{
    if((mode) >= STARTGAMEMODE && (mode) < STARTGAMEMODE + NUMGAMEMODES) //make sure input is within valid range
    {
        if(gamemodes[(mode) - STARTGAMEMODE].flags&(flag))
        {
            return true;
        }
        return false;
    }
    return false;
}

#define MODE_VALID(mode)          ((mode) >= STARTGAMEMODE && (mode) < STARTGAMEMODE + NUMGAMEMODES)

enum {
    MasterMode_Auth = -1,
    MasterMode_Open = 0,
    MasterMode_Veto,
    MasterMode_Locked,
    MasterMode_Private,
    MasterMode_Password,
    MasterMode_Start = MasterMode_Auth,
    MasterMode_Invalid = MasterMode_Start - 1
};

static const char * const mastermodenames[] =  { "auth",   "open",   "veto",       "locked",     "private",    "password" };
static const char * const mastermodecolors[] = { "",       "\f0",    "\f2",        "\f2",        "\f3",        "\f3" };
static const char * const mastermodeicons[] =  { "server", "server", "serverlock", "serverlock", "serverpriv", "serverpriv" };

// hardcoded sounds, defined in sounds.cfg
enum
{
    Sound_Jump = 0,
    Sound_Land,
    Sound_SplashIn,
    Sound_SplashOut,
    Sound_Burn,
    Sound_ItemSpawn,
    Sound_Teleport,
    Sound_JumpPad,
    Sound_Melee,
    Sound_Pulse1,
    Sound_Pulse2,
    Sound_PulseExplode,
    Sound_Rail1,
    Sound_Rail2,
    Sound_WeapLoad,
    Sound_NoAmmo,
    Sound_Hit,
    Sound_Pain1,
    Sound_Pain2,
    Sound_Die1,
    Sound_Die2,

    Sound_FlagPickup,
    Sound_FlagDrop,
    Sound_FlagReturn,
    Sound_FlagScore,
    Sound_FlagReset,
    Sound_FlagFail
};

// network messages codes, c2s, c2c, s2c

enum
{
    Priv_None = 0,
    Priv_Master,
    Priv_Auth,
    Priv_Admin
};

enum
{
    NetMsg_Connect = 0,
    NetMsg_ServerInfo,
    NetMsg_Welcome,
    NetMsg_InitClient,
    NetMsg_Pos,
    NetMsg_Text,
    NetMsg_Sound,
    NetMsg_ClientDiscon,
    NetMsg_Shoot,
    //game
    NetMsg_Explode,
    NetMsg_Suicide, //10
    NetMsg_Died,
    NetMsg_Damage,
    NetMsg_Hitpush,
    NetMsg_ShotFX,
    NetMsg_ExplodeFX,
    NetMsg_TrySpawn,
    NetMsg_SpawnState,
    NetMsg_Spawn,
    NetMsg_ForceDeath,
    NetMsg_GunSelect, //20
    NetMsg_Taunt,
    NetMsg_MapChange,
    NetMsg_MapVote,
    NetMsg_TeamInfo,
    NetMsg_ItemSpawn,
    NetMsg_ItemPickup,
    NetMsg_ItemAcceptance,
    NetMsg_Teleport,
    NetMsg_Jumppad,

    NetMsg_Ping, //30
    NetMsg_Pong,
    NetMsg_ClientPing,
    NetMsg_TimeUp,
    NetMsg_ForceIntermission,
    NetMsg_ServerMsg,
    NetMsg_ItemList,
    NetMsg_Resume,
    //edit
    NetMsg_EditMode,
    NetMsg_EditEnt,
    NetMsg_EditFace, //40
    NetMsg_EditTex,
    NetMsg_EditMat,
    NetMsg_EditFlip,
    NetMsg_Copy,
    NetMsg_Paste,
    NetMsg_Rotate,
    NetMsg_Replace,
    NetMsg_DelCube,
    NetMsg_CalcLight,
    NetMsg_Remip, //50
    NetMsg_EditVSlot,
    NetMsg_Undo,
    NetMsg_Redo,
    NetMsg_Newmap,
    NetMsg_GetMap,
    NetMsg_SendMap,
    NetMsg_Clipboard,
    NetMsg_EditVar,
    //master
    NetMsg_MasterMode,
    NetMsg_Kick, //60
    NetMsg_ClearBans,
    NetMsg_CurrentMaster,
    NetMsg_Spectator,
    NetMsg_SetMasterMaster,
    NetMsg_SetTeam,
    //demo
    NetMsg_ListDemos,
    NetMsg_SendDemoList,
    NetMsg_GetDemo,
    NetMsg_SendDemo,
    NetMsg_DemoPlayback, //70
    NetMsg_RecordDemo,
    NetMsg_StopDemo,
    NetMsg_ClearDemos,
    //flag
    NetMsg_TakeFlag,
    NetMsg_ReturnFlag,
    NetMsg_ResetFlag,
    NetMsg_TryDropFlag,
    NetMsg_DropFlag,
    NetMsg_ScoreFlag,
    NetMsg_InitFlags, //80
    //misc
    NetMsg_SayTeam,
    NetMsg_Client,
    NetMsg_AuthTry,
    NetMsg_AuthKick,
    NetMsg_AuthChallenge,
    NetMsg_AuthAnswer,
    NetMsg_ReqAuth,
    NetMsg_PauseGame,
    NetMsg_GameSpeed,
    NetMsg_AddBot, //90
    NetMsg_DelBot,
    NetMsg_InitAI,
    NetMsg_FromAI,
    NetMsg_BotLimit,
    NetMsg_BotBalance,
    NetMsg_MapCRC,
    NetMsg_CheckMaps,
    NetMsg_SwitchName,
    NetMsg_SwitchModel,
    NetMsg_SwitchColor, //100
    NetMsg_SwitchTeam,
    NetMsg_ServerCommand,
    NetMsg_DemoPacket,
    NetMsg_NumMsgs //104
};

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    NetMsg_Connect, 0,
    NetMsg_ServerInfo, 0,
    NetMsg_Welcome, 1,
    NetMsg_InitClient, 0,
    NetMsg_Pos, 0,
    NetMsg_Text, 0,
    NetMsg_Sound, 2,
    NetMsg_ClientDiscon, 2,

    NetMsg_Shoot, 0,
    NetMsg_Explode, 0,
    NetMsg_Suicide, 1,
    NetMsg_Died, 5,
    NetMsg_Damage, 5,
    NetMsg_Hitpush, 7,
    NetMsg_ShotFX, 10,
    NetMsg_ExplodeFX, 4,
    NetMsg_TrySpawn, 1,
    NetMsg_SpawnState, 8,
    NetMsg_Spawn, 3,
    NetMsg_ForceDeath, 2,
    NetMsg_GunSelect, 2,
    NetMsg_Taunt, 1,
    NetMsg_MapChange, 0,
    NetMsg_MapVote, 0,
    NetMsg_TeamInfo, 0,
    NetMsg_ItemSpawn, 2,
    NetMsg_ItemPickup, 2,
    NetMsg_ItemAcceptance, 3,

    NetMsg_Ping, 2,
    NetMsg_Pong, 2,
    NetMsg_ClientPing, 2,
    NetMsg_TimeUp, 2,
    NetMsg_ForceIntermission, 1,
    NetMsg_ServerMsg, 0,
    NetMsg_ItemList, 0,
    NetMsg_Resume, 0,

    NetMsg_EditMode, 2,
    NetMsg_EditEnt, 11,
    NetMsg_EditFace, 16,
    NetMsg_EditTex, 16,
    NetMsg_EditMat, 16,
    NetMsg_EditFlip, 14,
    NetMsg_Copy, 14,
    NetMsg_Paste, 14,
    NetMsg_Rotate, 15,
    NetMsg_Replace, 17,
    NetMsg_DelCube, 14,
    NetMsg_CalcLight, 1,
    NetMsg_Remip, 1,
    NetMsg_EditVSlot, 16,
    NetMsg_Undo, 0,
    NetMsg_Redo, 0,
    NetMsg_Newmap, 2,
    NetMsg_GetMap, 1,
    NetMsg_SendMap, 0,
    NetMsg_EditVar, 0,
    NetMsg_MasterMode, 2,
    NetMsg_Kick, 0,
    NetMsg_ClearBans, 1,
    NetMsg_CurrentMaster, 0,
    NetMsg_Spectator, 3,
    NetMsg_SetMasterMaster, 0,
    NetMsg_SetTeam, 0,

    NetMsg_ListDemos, 1,
    NetMsg_SendDemoList, 0,
    NetMsg_GetDemo, 2,
    NetMsg_SendDemo, 0,
    NetMsg_DemoPlayback, 3,
    NetMsg_RecordDemo, 2,
    NetMsg_StopDemo, 1,
    NetMsg_ClearDemos, 2,

    NetMsg_TakeFlag, 3,
    NetMsg_ReturnFlag, 4,
    NetMsg_ResetFlag, 3,
    NetMsg_TryDropFlag, 1,
    NetMsg_DropFlag, 7,
    NetMsg_ScoreFlag, 9,
    NetMsg_InitFlags, 0,

    NetMsg_SayTeam, 0,
    NetMsg_Client, 0,
    NetMsg_AuthTry, 0,
    NetMsg_AuthKick, 0,
    NetMsg_AuthChallenge, 0,
    NetMsg_AuthAnswer, 0,
    NetMsg_ReqAuth, 0,
    NetMsg_PauseGame, 0,
    NetMsg_GameSpeed, 0,
    NetMsg_AddBot, 2,
    NetMsg_DelBot, 1,
    NetMsg_InitAI, 0,
    NetMsg_FromAI, 2,
    NetMsg_BotLimit, 2,
    NetMsg_BotBalance, 2,
    NetMsg_MapCRC, 0,
    NetMsg_CheckMaps, 1,
    NetMsg_SwitchName, 0,
    NetMsg_SwitchModel, 2,
    NetMsg_SwitchColor, 2,
    NetMsg_SwitchTeam, 2,
    NetMsg_ServerCommand, 0,
    NetMsg_DemoPacket, 0,
    -1
};

#define TESSERACT_SERVER_PORT 42069
#define TESSERACT_LANINFO_PORT 42067
#define TESSERACT_MASTER_PORT 42068
#define PROTOCOL_VERSION 2              // bump when protocol changes
#define DEMO_VERSION 1                  // bump when demo format changes
#define DEMO_MAGIC "TESSERACT_DEMO\0\0"

struct demoheader
{
    char magic[16];
    int version, protocol;
};

#define MAXNAMELEN 15

enum
{
    HudIcon_RedFlag = 0,
    HudIcon_BlueFlag,

    HudIcon_Size    = 120,
};



#define VALID_ITEM(n) false //no items in this game thus far

#define MAXRAYS 1
#define EXP_SELFDAMDIV 2
#define EXP_SELFPUSH 2.5f
#define EXP_DISTSCALE 0.5f
// this defines weapon properties
//                                   1    2       3     4         5        6      7         8            9       10      11      12         13          14     15    16       17      18   19
static const struct attackinfo { int gun, action, anim, vwepanim, hudanim, sound, hudsound, attackdelay, damage, spread, margin, projspeed, kickamount, range, rays, hitpush, exprad, ttl, use; } attacks[Attack_NumAttacks] =

//    1          2          3           4                5               6         7        8     9  10 11    12  13    14 15    16  17 18 19
{
    { Gun_Rail,  Act_Shoot, Anim_Shoot, Anim_VWepShoot, Anim_GunShoot, Sound_Rail1,  Sound_Rail2, 1300, 10, 0, 0,    0, 30, 2048, 1, 1500,  0, 0, 0 },
    { Gun_Rail,  Act_Melee, Anim_Melee, Anim_VWepMelee, Anim_GunMelee, Sound_Melee,  Sound_Melee,  500, 10, 0, 2,    0,  0,   14, 1,    0,  0, 0, 0 },
    { Gun_Pulse, Act_Shoot, Anim_Shoot, Anim_VWepShoot, Anim_GunShoot, Sound_Pulse1, Sound_Pulse2, 130,  3, 0, 1, 3000, 10, 1024, 1, 2500,  3, 0, 0 },
    { Gun_Pulse, Act_Melee, Anim_Melee, Anim_VWepMelee, Anim_GunMelee, Sound_Melee,  Sound_Melee,  500, 10, 0, 2,    0,  0,   14, 1,    0,  0, 0, 0 }
};

static const struct guninfo { const char *name, *file, *vwep; int attacks[Act_NumActs]; } guns[Gun_NumGuns] =
{
    { "railgun", "railgun", "worldgun/railgun", { -1, Attack_RailShot, Attack_RailMelee }, },
    { "pulse rifle", "pulserifle", "worldgun/pulserifle", { -1, Attack_PulseShoot, Attack_PulseMelee } }
};

#include "ai.h"

// inherited by gameent and server clients
struct gamestate
{
    int health, maxhealth;
    int gunselect, gunwait;
    int ammo[Gun_NumGuns];
    int aitype, skill;

    gamestate() : maxhealth(1), aitype(AI_None), skill(0) {}

    bool canpickup(int type)
    {
        return VALID_ITEM(type);
    }

    void pickup(int type)
    {
    }

    void respawn()
    {
        health = maxhealth;
        gunselect = Gun_Rail;
        gunwait = 0;
        for(int i = 0; i < Gun_NumGuns; ++i)
        {
            ammo[i] = 0;
        }
    }

    void spawnstate(int gamemode)
    {
        if(modecheck(gamemode, Mode_All))
        {
            gunselect = Gun_Rail;
            for(int i = 0; i < Gun_NumGuns; ++i)
            {
                ammo[i] = 1;
            }
        }
        else if(modecheck(gamemode, Mode_Rail))
        {
            gunselect = Gun_Rail;
            ammo[Gun_Rail] = 1;
        }
        else if(modecheck(gamemode, Mode_Pulse))
        {
            gunselect = Gun_Pulse;
            ammo[Gun_Pulse] = 1;
        }
    }

    // just subtract damage here, can set death, etc. later in code calling this
    int dodamage(int damage)
    {
        health -= damage;
        return damage;
    }

    int hasammo(int gun, int exclude = -1)
    {
        return VALID_GUN(gun) && gun != exclude && ammo[gun] > 0;
    }
};

#define MAXTEAMS 2
static const char * const teamnames[1+MAXTEAMS] = { "", "azul", "rojo" };
static const char * const teamtextcode[1+MAXTEAMS] = { "\f0", "\f1", "\f3" };
static const int teamtextcolor[1+MAXTEAMS] = { 0x1EC850, 0x6496FF, 0xFF4B19 };
static const int teamscoreboardcolor[1+MAXTEAMS] = { 0, 0x3030C0, 0xC03030 };
static const char * const teamblipcolor[1+MAXTEAMS] = { "_neutral", "_blue", "_red" };
inline int teamnumber(const char *name)
{
    for(int i = 0; i < MAXTEAMS; ++i)
    {
        if(!strcmp(teamnames[1+i], name))
        {
            return 1+i;
        }
    }
    return 0;
}

#define VALID_TEAM(n) ((n) >= 1 && (n) <= MAXTEAMS)
#define TEAM_NAME(n) (teamnames[VALID_TEAM(n) ? (n) : 0])

struct gameent : dynent, gamestate
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int respawned, suicided;
    int lastpain;
    int lastaction, lastattack;
    int attacking;
    int lasttaunt;
    int lastpickup, lastpickupmillis, flagpickup;
    int frags, flags, deaths, totaldamage, totalshots;
    editinfo *edit;
    float deltayaw, deltapitch, deltaroll, newyaw, newpitch, newroll;
    int smoothmillis;

    string name, info;
    int team, playermodel, playercolor;
    ai::aiinfo *ai;
    int ownernum, lastnode;

    vec muzzle;

    gameent() : weight(100), clientnum(-1), privilege(Priv_None), lastupdate(0), plag(0), ping(0), lifesequence(0), respawned(-1), suicided(-1), lastpain(0), frags(0), flags(0), deaths(0), totaldamage(0), totalshots(0), edit(NULL), smoothmillis(-1), team(0), playermodel(-1), playercolor(0), ai(NULL), ownernum(-1), muzzle(-1, -1, -1)
    {
        name[0] = info[0] = 0;
        respawn();
    }
    ~gameent()
    {
        freeeditinfo(edit);
        if(ai) delete ai;
    }

    void hitpush(int damage, const vec &dir, gameent *actor, int atk)
    {
        vec push(dir);
        push.mul((actor==this && attacks[atk].exprad ? EXP_SELFPUSH : 1.0f)*attacks[atk].hitpush*damage/weight);
        vel.add(push);
    }

    void respawn()
    {
        dynent::reset();
        gamestate::respawn();
        respawned = suicided = -1;
        lastaction = 0;
        lastattack = -1;
        attacking = Act_Idle;
        lasttaunt = 0;
        lastpickup = -1;
        lastpickupmillis = 0;
        flagpickup = 0;
        lastnode = -1;
    }

    void startgame()
    {
        frags = flags = deaths = 0;
        totaldamage = totalshots = 0;
        maxhealth = 1;
        lifesequence = -1;
        respawned = suicided = -2;
    }
};

struct teamscore
{
    int team, score;
    teamscore() {}
    teamscore(int team, int n) : team(team), score(n) {}

    static bool compare(const teamscore &x, const teamscore &y)
    {
        if(x.score > y.score) return true;
        if(x.score < y.score) return false;
        return x.team < y.team;
    }
};

inline uint hthash(const teamscore &t) { return hthash(t.team); }
inline bool htcmp(int team, const teamscore &t) { return team == t.team; }

struct teaminfo
{
    int frags;

    teaminfo() { reset(); }

    void reset() { frags = 0; }
};

namespace entities
{
    extern vector<extentity *> ents;

    extern const char *entmdlname(int type);
    extern const char *itemname(int i);
    extern int itemicon(int i);

    extern void preloadentities();
    extern void renderentities();
    extern void checkitems(gameent *d);
    extern void resetspawns();
    extern void spawnitems(bool force = false);
    extern void putitems(packetbuf &p);
    extern void setspawn(int i, bool on);
    extern void teleport(int n, gameent *d);
    extern void pickupeffects(int n, gameent *d);
    extern void teleporteffects(gameent *d, int tp, int td, bool local = true);
    extern void jumppadeffects(gameent *d, int jp, bool local = true);
}

namespace game
{
    extern int gamemode;

    struct clientmode
    {
        virtual ~clientmode() {}

        virtual void preload() {}
        virtual float clipconsole(float w, float h) { return 0; }
        virtual void drawhud(gameent *d, int w, int h) {}
        virtual void rendergame() {}
        virtual void respawned(gameent *d) {}
        virtual void setup() {}
        virtual void checkitems(gameent *d) {}
        virtual int respawnwait(gameent *d) { return 0; }
        virtual void pickspawn(gameent *d) { findplayerspawn(d, -1, modecheck(gamemode, Mode_Team) ? d->team : 0); }
        virtual void senditems(packetbuf &p) {}
        virtual void removeplayer(gameent *d) {}
        virtual void gameover() {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(int team) { return 0; }
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests) {}
        virtual bool aicheck(gameent *d, ai::aistate &b) { return false; }
        virtual bool aidefend(gameent *d, ai::aistate &b) { return false; }
        virtual bool aipursue(gameent *d, ai::aistate &b) { return false; }
    };

    extern clientmode *cmode;
    extern void setclientmode();

    // game
    extern int nextmode;
    extern string clientmap;
    extern bool intermission;
    extern int maptime, maprealtime, maplimit;
    extern gameent *player1;
    extern vector<gameent *> players, clients;
    extern int lastspawnattempt;
    extern int lasthit;
    extern int following;
    extern int smoothmove, smoothdist;

    extern bool clientoption(const char *arg);
    extern gameent *getclient(int cn);
    extern gameent *newclient(int cn);
    extern const char *colorname(gameent *d, const char *name = NULL, const char *alt = NULL, const char *color = "");
    extern const char *teamcolorname(gameent *d, const char *alt = "you");
    extern const char *teamcolor(const char *prefix, const char *suffix, int team, const char *alt);
    extern void teamsound(bool sameteam, int n, const vec *loc = NULL);
    extern void teamsound(gameent *d, int n, const vec *loc = NULL);
    extern gameent *pointatplayer();
    extern gameent *hudplayer();
    extern gameent *followingplayer();
    extern void stopfollowing();
    extern void checkfollow();
    extern void nextfollow(int dir = 1);
    extern void clientdisconnected(int cn, bool notify = true);
    extern void clearclients(bool notify = true);
    extern void startgame();
    extern void spawnplayer(gameent *);
    extern void deathstate(gameent *d, bool restore = false);
    extern void damaged(int damage, gameent *d, gameent *actor, bool local = true);
    extern void killed(gameent *d, gameent *actor);
    extern void timeupdate(int timeremain);
    extern void msgsound(int n, physent *d = NULL);
    extern void drawicon(int icon, float x, float y, float sz = 120);
    const char *mastermodecolor(int n, const char *unknown);
    const char *mastermodeicon(int n, const char *unknown);

    // client
    extern bool connected, remote, demoplayback;
    extern string servdesc;
    extern vector<uchar> messages;

    extern int parseplayer(const char *arg);
    extern void ignore(int cn);
    extern void unignore(int cn);
    extern bool isignored(int cn);
    extern bool addmsg(int type, const char *fmt = NULL, ...);
    extern void switchname(const char *name);
    extern void switchteam(const char *name);
    extern void switchplayermodel(int playermodel);
    extern void switchplayercolor(int playercolor);
    extern void sendmapinfo();
    extern void stopdemo();
    extern void changemap(const char *name, int mode);
    extern void c2sinfo(bool force = false);
    extern void sendposition(gameent *d, bool reliable = false);

    // weapon
    extern int getweapon(const char *name);
    extern void shoot(gameent *d, const vec &targ);
    extern void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction);
    extern void explode(bool local, gameent *owner, const vec &v, const vec &vel, dynent *safe, int dam, int atk);
    extern void explodeeffects(int atk, gameent *d, bool local, int id = 0);
    extern void damageeffect(int damage, gameent *d, bool thirdperson = true);
    extern float intersectdist;
    extern bool intersect(dynent *d, const vec &from, const vec &to, float margin = 0, float &dist = intersectdist);
    extern dynent *intersectclosest(const vec &from, const vec &to, gameent *at, float margin = 0, float &dist = intersectdist);
    extern void clearbouncers();
    extern void updatebouncers(int curtime);
    extern void removebouncers(gameent *owner);
    extern void renderbouncers();
    extern void clearprojectiles();
    extern void updateprojectiles(int curtime);
    extern void removeprojectiles(gameent *owner);
    extern void renderprojectiles();
    extern void removeweapons(gameent *owner);
    extern void updateweapons(int curtime);
    extern void gunselect(int gun, gameent *d);
    extern void weaponswitch(gameent *d);
    extern void avoidweapons(ai::avoidset &obstacles, float radius);

    // scoreboard
    extern void showscores(bool on);
    extern void getbestplayers(vector<gameent *> &best);
    extern void getbestteams(vector<int> &best);
    extern void clearteaminfo();
    extern void setteaminfo(int team, int frags);
    extern void removegroupedplayer(gameent *d);

    // render
    struct playermodelinfo
    {
        const char *model[1+MAXTEAMS], *hudguns[1+MAXTEAMS],
                   *icon[1+MAXTEAMS];
        bool ragdoll;
    };

    extern void saveragdoll(gameent *d);
    extern void clearragdolls();
    extern void moveragdolls();
    extern const playermodelinfo &getplayermodelinfo(gameent *d);
    extern int getplayercolor(gameent *d, int team);
    extern int chooserandomplayermodel(int seed);
    extern void syncplayer();
    extern void swayhudgun(int curtime);
    extern vec hudgunorigin(int gun, const vec &from, const vec &to, gameent *d);
}

namespace server
{
    extern const char *modename(int n, const char *unknown = "unknown");
    extern const char *modeprettyname(int n, const char *unknown = "unknown");
    extern const char *mastermodename(int n, const char *unknown = "unknown");
    extern void startintermission();
    extern void stopdemo();
    extern void forcemap(const char *map, int mode);
    extern void forcepaused(bool paused);
    extern void forcegamespeed(int speed);
    extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = MAXSTRLEN);
    extern int msgsizelookup(int msg);
    extern bool serveroption(const char *arg);
    extern bool delayspawn(int type);
}

#endif


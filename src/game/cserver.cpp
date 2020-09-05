#include "game.h"

//server game handling
//includes:
//game moderation (e.g. bans)
//main serverupdate function (called from engine/server.cpp)
//voting
//parsing of packets for game events
//map crc checks
//core demo handling
//ai manager

namespace game
{
    void parseoptions(vector<const char *> &args)
    {
        for(int i = 0; i < args.length(); i++)
        {
            conoutf(Console_Error, "unknown command-line option: %s", args[i]);
        }
    }
    const char *gameident() { return "Imprimis"; }
}

extern ENetAddress masteraddress;

namespace server
{
    struct server_entity            // server side version of "entity" type
    {
        int type;
        int spawntime;
        bool spawned;
    };

    static const int DEATHMILLIS = 300;

    struct clientinfo;

    struct gameevent
    {
        virtual ~gameevent() {}

        virtual bool flush(clientinfo *ci, int fmillis);
        virtual void process(clientinfo *ci) {}

        virtual bool keepable() const { return false; }
    };

    struct timedevent : gameevent
    {
        int millis;

        bool flush(clientinfo *ci, int fmillis);
    };

    struct hitinfo
    {
        int target;
        int lifesequence;
        int rays;
        float dist;
        vec dir;
    };

    template <int N>
    struct projectilestate
    {
        int projs[N];
        int numprojs;

        projectilestate() : numprojs(0) {}

        void reset() { numprojs = 0; }

        void add(int val)
        {
            if(numprojs>=N) numprojs = 0;
            projs[numprojs++] = val;
        }

        bool remove(int val)
        {
            for(int i = 0; i < numprojs; ++i)
            {
                if(projs[i]==val)
                {
                    projs[i] = projs[--numprojs];
                    return true;
                }
            }
            return false;
        }
    };

    struct servstate : gamestate
    {
        vec o;
        int state, editstate;
        int lastdeath, deadflush, lastspawn, lifesequence;
        int lastshot;
        projectilestate<8> projs;
        int frags, flags, deaths, teamkills, shotdamage, damage;
        int lasttimeplayed, timeplayed;
        float effectiveness;

        servstate() : state(ClientState_Dead), editstate(ClientState_Dead), lifesequence(0) {}

        bool isalive(int gamemillis)
        {
            return state==ClientState_Alive || (state==ClientState_Dead && gamemillis - lastdeath <= DEATHMILLIS);
        }

        bool waitexpired(int gamemillis)
        {
            return gamemillis - lastshot >= gunwait;
        }

        void reset()
        {
            if(state!=ClientState_Spectator) state = editstate = ClientState_Dead;
            //sets client health
            maxhealth = 10;
            projs.reset();

            timeplayed = 0;
            effectiveness = 0;
            frags = flags = deaths = teamkills = shotdamage = damage = 0;

            lastdeath = 0;

            respawn();
        }

        void respawn()
        {
            gamestate::respawn();
            o = vec(-1e10f, -1e10f, -1e10f);
            deadflush = 0;
            lastspawn = -1;
            lastshot = 0;
        }

        void reassign()
        {
            respawn();
            projs.reset();
        }
    };

    struct savedscore
    {
        uint ip;
        string name;
        int frags, flags, deaths, teamkills, shotdamage, damage;
        int timeplayed;
        float effectiveness;

        void save(servstate &gs)
        {
            frags = gs.frags;
            flags = gs.flags;
            deaths = gs.deaths;
            teamkills = gs.teamkills;
            shotdamage = gs.shotdamage;
            damage = gs.damage;
            timeplayed = gs.timeplayed;
            effectiveness = gs.effectiveness;
        }

        void restore(servstate &gs)
        {
            gs.frags = frags;
            gs.flags = flags;
            gs.deaths = deaths;
            gs.teamkills = teamkills;
            gs.shotdamage = shotdamage;
            gs.damage = damage;
            gs.timeplayed = timeplayed;
            gs.effectiveness = effectiveness;
        }
    };

    extern int gamemillis, nextexceeded;

    struct clientinfo
    {
        int clientnum, ownernum, connectmillis, sessionid, overflow;
        string name, mapvote;
        int team, playermodel, playercolor;
        int modevote;
        int privilege;
        bool connected, local, timesync;
        int gameoffset, lastevent, pushed, exceeded;
        servstate state;
        vector<gameevent *> events;
        vector<uchar> position, messages;
        uchar *wsdata;
        int wslen;
        vector<clientinfo *> bots;
        int ping, aireinit;
        string clientmap;
        int mapcrc;
        bool warned, gameclip;
        ENetPacket *getdemo, *getmap, *clipboard;
        int lastclipboard, needclipboard;
        int connectauth;
        uint authreq;
        string authname, authdesc;
        void *authchallenge;
        int authkickvictim;
        char *authkickreason;

        clientinfo() : getdemo(NULL), getmap(NULL), clipboard(NULL), authchallenge(NULL), authkickreason(NULL) { reset(); }
        ~clientinfo() { events.deletecontents(); cleanclipboard(); cleanauth(); }

        void addevent(gameevent *e)
        {
            if(state.state==ClientState_Spectator || events.length()>100) delete e;
            else events.add(e);
        }

        enum
        {
            PUSHMILLIS = 3000
        };

        int calcpushrange()
        {
            ENetPeer *peer = getclientpeer(ownernum);
            return PUSHMILLIS + (peer ? peer->roundTripTime + peer->roundTripTimeVariance : ENET_PEER_DEFAULT_ROUND_TRIP_TIME);
        }

        bool checkpushed(int millis, int range)
        {
            return millis >= pushed - range && millis <= pushed + range;
        }

        void setpushed()
        {
            pushed = max(pushed, gamemillis);
            if(exceeded && checkpushed(exceeded, calcpushrange())) exceeded = 0;
        }

        bool checkexceeded()
        {
            return state.state==ClientState_Alive && exceeded && gamemillis > exceeded + calcpushrange();
        }

        void mapchange()
        {
            mapvote[0] = 0;
            modevote = INT_MAX;
            state.reset();
            events.deletecontents();
            overflow = 0;
            timesync = false;
            lastevent = 0;
            exceeded = 0;
            pushed = 0;
            clientmap[0] = '\0';
            mapcrc = 0;
            warned = false;
            gameclip = false;
        }

        void reassign()
        {
            state.reassign();
            events.deletecontents();
            timesync = false;
            lastevent = 0;
        }

        void cleanclipboard(bool fullclean = true)
        {
            if(clipboard) { if(--clipboard->referenceCount <= 0) enet_packet_destroy(clipboard); clipboard = NULL; }
            if(fullclean) lastclipboard = 0;
        }

        void cleanauthkick()
        {
            authkickvictim = -1;
            DELETEA(authkickreason);
        }

        void cleanauth(bool full = true)
        {
            authreq = 0;
            if(authchallenge) { freechallenge(authchallenge); authchallenge = NULL; }
            if(full) cleanauthkick();
        }

        void reset()
        {
            name[0] = 0;
            team = 0;
            playermodel = -1;
            playercolor = 0;
            privilege = Priv_None;
            connected = local = false;
            connectauth = 0;
            position.setsize(0);
            messages.setsize(0);
            ping = 0;
            aireinit = 0;
            needclipboard = 0;
            cleanclipboard();
            cleanauth();
            mapchange();
        }

        int geteventmillis(int servmillis, int clientmillis)
        {
            if(!timesync || (events.empty() && state.waitexpired(servmillis)))
            {
                timesync = true;
                gameoffset = servmillis - clientmillis;
                return servmillis;
            }
            else return gameoffset + clientmillis;
        }
    };

    struct ban
    {
        int time, expire;
        uint ip;
    };

    namespace aiman
    {
        extern void removeai(clientinfo *ci);
        extern void clearai();
        extern void checkai();
        extern void reqadd(clientinfo *ci, int skill);
        extern void reqdel(clientinfo *ci);
        extern void setbotlimit(clientinfo *ci, int limit);
        extern void addclient(clientinfo *ci);
        extern void changeteam(clientinfo *ci);
    }

    #define MM_MODE 0xF
    #define MM_AUTOAPPROVE 0x1000
    #define MM_PRIVSERV (MM_MODE | MM_AUTOAPPROVE)
    #define MM_PUBSERV ((1<<MasterMode_Open) | (1<<MasterMode_Veto))
    #define MM_COOPSERV (MM_AUTOAPPROVE | MM_PUBSERV | (1<<MasterMode_Locked))

    bool notgotitems = true;        // true when map has changed and waiting for clients to send item
    int gamemode = 0;
    int gamemillis = 0, gamelimit = 0, nextexceeded = 0, gamespeed = 100;
    bool gamepaused = false, shouldstep = true;

    string smapname = "";
    int interm = 0;
    enet_uint32 lastsend = 0;
    int mastermode = MasterMode_Open, mastermask = MM_PRIVSERV;
    stream *mapdata = NULL;

    vector<uint> allowedips;
    vector<ban> bannedips;

    void addban(uint ip, int expire)
    {
        allowedips.removeobj(ip);
        ban b;
        b.time = totalmillis;
        b.expire = totalmillis + expire;
        b.ip = ip;
        for(int i = 0; i < bannedips.length(); i++)
        {
            if(bannedips[i].expire - b.expire > 0)
            {
                bannedips.insert(i, b);
                return;
            }
        }
        bannedips.add(b);
    }

    vector<clientinfo *> connects, clients, bots;

    void kickclients(uint ip, clientinfo *actor = NULL, int priv = Priv_None)
    {
        for(int i = clients.length(); --i >=0;) //note reverse iteration
        {
            clientinfo &c = *clients[i];
            if(c.state.aitype != AI_None || c.privilege >= Priv_Admin || c.local)
            {
                continue;
            }
            if(actor && ((c.privilege > priv && !actor->local) || c.clientnum == actor->clientnum))
            {
                continue;
            }
            if(getclientip(c.clientnum) == ip)
            {
                disconnect_client(c.clientnum, Discon_Kick);
            }
        }
    }

    struct maprotation
    {
        static int exclude;
        int modes;
        string map;

        int calcmodemask() const { return modes&(1<<numgamemodes) ? modes & ~exclude : modes; }
        bool hasmode(int mode, int offset = STARTGAMEMODE) const { return (calcmodemask() & (1 << (mode-offset))) != 0; }

        int findmode(int mode) const
        {
            if(!hasmode(mode))
            {
                for(int i = 0; i < numgamemodes; ++i)
                {
                    if(hasmode(i, 0))
                    {
                        return i+STARTGAMEMODE;
                    }
                }
            }
            return mode;
        }

        bool match(int reqmode, const char *reqmap) const
        {
            return hasmode(reqmode) && (!map[0] || !reqmap[0] || !strcmp(map, reqmap));
        }

        bool includes(const maprotation &rot) const
        {
            return rot.modes == modes ? rot.map[0] && !map[0] : (rot.modes & modes) == rot.modes;
        }
    };
    int maprotation::exclude = 0;
    vector<maprotation> maprotations;
    int curmaprotation = 0;

    VAR(lockmaprotation, 0, 0, 2);

    void maprotationreset()
    {
        maprotations.setsize(0);
        curmaprotation = 0;
        maprotation::exclude = 0;
    }

    void nextmaprotation()
    {
        curmaprotation++;
        if(maprotations.inrange(curmaprotation) && maprotations[curmaprotation].modes) return;
        do curmaprotation--;
        while(maprotations.inrange(curmaprotation) && maprotations[curmaprotation].modes);
        curmaprotation++;
    }

    int findmaprotation(int mode, const char *map)
    {
        for(int i = max(curmaprotation, 0); i < maprotations.length(); i++)
        {
            maprotation &rot = maprotations[i];
            if(!rot.modes) break;
            if(rot.match(mode, map)) return i;
        }
        int start;
        for(start = max(curmaprotation, 0) - 1; start >= 0; start--) if(!maprotations[start].modes) break;
        start++;
        for(int i = start; i < curmaprotation; i++)
        {
            maprotation &rot = maprotations[i];
            if(!rot.modes) break;
            if(rot.match(mode, map)) return i;
        }
        int best = -1;
        for(int i = 0; i < maprotations.length(); i++)
        {
            maprotation &rot = maprotations[i];
            if(rot.match(mode, map) && (best < 0 || maprotations[best].includes(rot))) best = i;
        }
        return best;
    }

    bool searchmodename(const char *haystack, const char *needle)
    {
        if(!needle[0]) return true;
        do
        {
            if(needle[0] != '.')
            {
                haystack = strchr(haystack, needle[0]);
                if(!haystack) break;
                haystack++;
            }
            const char *h = haystack, *n = needle+1;
            for(; *h && *n; h++)
            {
                if(*h == *n)
                {
                    n++;
                }
                else if(*h != ' ')
                {
                    break;
                }
            }
            if(!*n) return true;
            if(*n == '.') return !*h;
        } while(needle[0] != '.');
        return false;
    }

    int genmodemask(vector<char *> &modes)
    {
        int modemask = 0;
        for(int i = 0; i < modes.length(); i++)
        {
            const char *mode = modes[i];
            int op = mode[0];
            switch(mode[0])
            {
                case '*':
                    modemask |= 1<<numgamemodes;
                    for(int k = 0; k < numgamemodes; ++k)
                    {
                        if(modecheck(k+STARTGAMEMODE, Mode_Untimed))
                        {
                            modemask |= 1<<k;
                        }
                    }
                    continue;
                case '!':
                    mode++;
                    if(mode[0] != '?') break;
                case '?':
                    mode++;
                    for(int k = 0; k < numgamemodes; ++k)
                    {
                        if(searchmodename(gamemodes[k].name, mode))
                        {
                            if(op == '!') modemask &= ~(1<<k);
                            else modemask |= 1<<k;
                        }
                    }
                    continue;
            }
            int modenum = INT_MAX;
            if(isdigit(mode[0]))
            {
                modenum = atoi(mode);
            }
            else
            {
                for(int k = 0; k < numgamemodes; ++k)
                {
                    if(searchmodename(gamemodes[k].name, mode))
                    {
                        modenum = k+STARTGAMEMODE;
                        break;
                    }
                }
            }
            if(!MODE_VALID(modenum)) continue;
            switch(op)
            {
                case '!': modemask &= ~(1 << (modenum - STARTGAMEMODE)); break;
                default: modemask |= 1 << (modenum - STARTGAMEMODE); break;
            }
        }
        return modemask;
    }

    bool addmaprotation(int modemask, const char *map)
    {
        if(!map[0])
        {
            for(int k = 0; k < numgamemodes; ++k)
            {
                if(modemask&(1<<k) && !modecheck(k+STARTGAMEMODE, Mode_Edit))
                {
                    modemask &= ~(1<<k);
                }
            }
        }
        if(!modemask)
        {
            return false;
        }
        if(!(modemask&(1<<numgamemodes)))
        {
            maprotation::exclude |= modemask;
        }
        maprotation &rot = maprotations.add();
        rot.modes = modemask;
        copystring(rot.map, map);
        return true;
    }

    void addmaprotations(tagval *args, int numargs)
    {
        vector<char *> modes, maps;
        for(int i = 0; i + 1 < numargs; i += 2)
        {
            explodelist(args[i].getstr(), modes);
            explodelist(args[i+1].getstr(), maps);
            int modemask = genmodemask(modes);
            if(maps.length())
            {
                for(int j = 0; j < maps.length(); j++)
                {
                    addmaprotation(modemask, maps[j]);
                }
            }
            else
            {
                addmaprotation(modemask, "");
            }
            modes.deletearrays();
            maps.deletearrays();
        }
        if(maprotations.length() && maprotations.last().modes)
        {
            maprotation &rot = maprotations.add();
            rot.modes = 0;
            rot.map[0] = '\0';
        }
    }

    COMMAND(maprotationreset, "");
    COMMANDN(maprotation, addmaprotations, "ss2V");

    struct demofile
    {
        string info;
        uchar *data;
        int len;
    };

    vector<demofile> demos;

    bool demonextmatch = false;
    stream *demotmp = NULL,
           *demorecord = NULL,
           *demoplayback = NULL;
    int nextplayback = 0,
        demomillis = 0;

    VAR(maxdemos, 0, 5, 25);
    VAR(maxdemosize, 0, 16, 31);
    VAR(restrictdemos, 0, 1, 1);

    VAR(restrictpausegame, 0, 1, 1);
    VAR(restrictgamespeed, 0, 1, 1);

    SVAR(serverdesc, "");
    SVAR(serverpass, "");
    SVAR(adminpass, "");
    VARF(publicserver, 0, 0, 2, {
        switch(publicserver)
        {
            case 0: default: mastermask = MM_PRIVSERV; break;
            case 1: mastermask = MM_PUBSERV; break;
            case 2: mastermask = MM_COOPSERV; break;
        }
    });
    SVAR(servermotd, "");

    struct teamkillkick
    {
        int modes, limit, ban;

        bool match(int mode) const
        {
            return (modes&(1<<(mode-STARTGAMEMODE)))!=0;
        }

        bool includes(const teamkillkick &tk) const
        {
            return tk.modes != modes && (tk.modes & modes) == tk.modes;
        }
    };
    vector<teamkillkick> teamkillkicks;

    void teamkillkickreset()
    {
        teamkillkicks.setsize(0);
    }

    void addteamkillkick(char *modestr, int *limit, int *ban)
    {
        vector<char *> modes;
        explodelist(modestr, modes);
        teamkillkick &kick = teamkillkicks.add();
        kick.modes = genmodemask(modes);
        kick.limit = *limit;
        kick.ban = *ban > 0 ? *ban*60000 : (*ban < 0 ? 0 : 30*60000);
        modes.deletearrays();
    }

    COMMAND(teamkillkickreset, "");
    COMMANDN(teamkillkick, addteamkillkick, "sii");

    struct teamkillinfo
    {
        uint ip;
        int teamkills;
    };
    vector<teamkillinfo> teamkills;
    bool shouldcheckteamkills = false;

    void addteamkill(clientinfo *actor, clientinfo *victim, int n)
    {
        if(modecheck(gamemode, Mode_Untimed) || actor->state.aitype != AI_None || actor->local || actor->privilege || (victim && victim->state.aitype != AI_None)) return;
        shouldcheckteamkills = true;
        uint ip = getclientip(actor->clientnum);
        for(int i = 0; i < teamkills.length(); i++)
        {
            if(teamkills[i].ip == ip)
            {
                teamkills[i].teamkills += n;
                return;
            }
        }
        teamkillinfo &tk = teamkills.add();
        tk.ip = ip;
        tk.teamkills = n;
    }

    void checkteamkills() //players who do too many teamkills may get kicked from the server
    {
        teamkillkick *kick = NULL;
        if(!modecheck(gamemode, Mode_Untimed))
        {
            for(int i = 0; i < teamkillkicks.length(); i++)
            {
                if(teamkillkicks[i].match(gamemode) && (!kick || kick->includes(teamkillkicks[i])))
                {
                    kick = &teamkillkicks[i];
                }
            }
        }
        if(kick)
        {
            for(int i = teamkills.length(); --i >=0;) //note reverse iteration
            {
                teamkillinfo &tk = teamkills[i];
                if(tk.teamkills >= kick->limit)
                {
                    if(kick->ban > 0)
                    {
                        addban(tk.ip, kick->ban);
                    }
                    kickclients(tk.ip);
                    teamkills.removeunordered(i);
                }
            }
        }
        shouldcheckteamkills = false;
    }

    void *newclientinfo() { return new clientinfo; }
    void deleteclientinfo(void *ci) { delete (clientinfo *)ci; }

    clientinfo *getinfo(int n)
    {
        if(n < MAXCLIENTS) return (clientinfo *)getclientinfo(n);
        n -= MAXCLIENTS;
        return bots.inrange(n) ? bots[n] : NULL;
    }

    uint mcrc = 0;
    vector<entity> ments;
    vector<server_entity> sents;
    vector<savedscore> scores;

    int msgsizelookup(int msg)
    {
        static int sizetable[NetMsg_NumMsgs] = { -1 };
        if(sizetable[0] < 0)
        {
            memset(sizetable, -1, sizeof(sizetable));
            for(const int *p = msgsizes; *p >= 0; p += 2) sizetable[p[0]] = p[1];
        }
        return msg >= 0 && msg < NetMsg_NumMsgs ? sizetable[msg] : -1;
    }

    const char *modename(int n, const char *unknown)
    {
        if(MODE_VALID(n)) return gamemodes[n - STARTGAMEMODE].name;
        return unknown;
    }

    const char *modeprettyname(int n, const char *unknown)
    {
        if(MODE_VALID(n)) return gamemodes[n - STARTGAMEMODE].prettyname;
        return unknown;
    }

    const char *mastermodename(int n, const char *unknown)
    {
        return (n>=MasterMode_Start && size_t(n-MasterMode_Start)<sizeof(mastermodenames)/sizeof(mastermodenames[0])) ? mastermodenames[n-MasterMode_Start] : unknown;
    }

    const char *privname(int type)
    {
        switch(type)
        {
            case Priv_Admin: return "admin";
            case Priv_Auth: return "auth";
            case Priv_Master: return "master";
            default: return "unknown";
        }
    }

    void sendservmsg(const char *s) { sendf(-1, 1, "ris", NetMsg_ServerMsg, s); }

    void sendservmsgf(const char *fmt, ...) PRINTFARGS(1, 2);
    void sendservmsgf(const char *fmt, ...)
    {
         DEFV_FORMAT_STRING(s, fmt, fmt);
         sendf(-1, 1, "ris", NetMsg_ServerMsg, s);
    }

    void resetitems()
    {
        mcrc = 0;
        ments.setsize(0);
        sents.setsize(0);
        //cps.reset();
    }

    void serverinit()
    {
        smapname[0] = '\0';
        resetitems();
    }

    int numclients(int exclude = -1, bool nospec = true, bool noai = true, bool priv = false)
    {
        int n = 0;
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(ci->clientnum!=exclude && (!nospec || ci->state.state!=ClientState_Spectator || (priv && (ci->privilege || ci->local))) && (!noai || ci->state.aitype == AI_None))
            {
                n++;
            }
        }
        return n;
    }

    bool duplicatename(clientinfo *ci, const char *name)
    {
        if(!name)
        {
            name = ci->name;
        }
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i]!=ci && !strcmp(name, clients[i]->name))
            {
                return true;
            }
        }
        return false;
    }

    const char *colorname(clientinfo *ci, const char *name = NULL)
    {
        if(!name)
        {
            name = ci->name;
        }
        if(name[0] && !duplicatename(ci, name) && ci->state.aitype == AI_None)
        {
            return name;
        }
        static string cname[3];
        static int cidx = 0;
        cidx = (cidx+1)%3;
        formatstring(cname[cidx], ci->state.aitype == AI_None ? "%s \fs\f5(%d)\fr" : "%s \fs\f5[%d]\fr", name, ci->clientnum);
        return cname[cidx];
    }

    struct servermode
    {
        virtual ~servermode() {}

        virtual void entergame(clientinfo *ci) {}
        virtual void leavegame(clientinfo *ci, bool disconnecting = false) {}

        virtual void moved(clientinfo *ci, const vec &oldpos, bool oldclip, const vec &newpos, bool newclip) {}
        virtual bool canspawn(clientinfo *ci, bool connecting = false) { return true; }
        virtual void spawned(clientinfo *ci) {}
        virtual int fragvalue(clientinfo *victim, clientinfo *actor)
        {
            if(victim==actor || (modecheck(gamemode, Mode_Team) && (victim->team == actor->team))) return -1;
            return 1;
        }
        virtual void died(clientinfo *victim, clientinfo *actor) {}
        virtual bool canchangeteam(clientinfo *ci, int oldteam, int newteam) { return true; }
        virtual void changeteam(clientinfo *ci, int oldteam, int newteam) {}
        virtual void initclient(clientinfo *ci, packetbuf &p, bool connecting) {}
        virtual void update() {}
        virtual void cleanup() {}
        virtual void setup() {}
        virtual void newmap() {}
        virtual void intermission() {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(int team) { return 0; }
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual bool extinfoteam(int team, ucharbuf &p) { return false; }
    };

    servermode *smode = NULL;

    bool canspawnitem(int type) { return 0; }

    int spawntime(int type)
    {
        int np = numclients(-1, true, false);
        np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
        int sec = 0;
        switch(type)
        {
        }
        return sec*1000;
    }

    static teaminfo teaminfos[MAXTEAMS];

    void clearteaminfo()
    {
        for(int i = 0; i < MAXTEAMS; ++i)
        {
            teaminfos[i].reset();
        }
    }

    clientinfo *choosebestclient(float &bestrank)
    {
        clientinfo *best = NULL;
        bestrank = -1;
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(ci->state.timeplayed<0) continue;
            float rank = ci->state.state!=ClientState_Spectator ? ci->state.effectiveness/max(ci->state.timeplayed, 1) : -1;
            if(!best || rank > bestrank) { best = ci; bestrank = rank; }
        }
        return best;
    }

    void prunedemos(int extra = 0)
    {
        int n = std::clamp(demos.length() + extra - maxdemos, 0, demos.length());
        if(n <= 0)
        {
            return;
        }
        for(int i = 0; i < n; ++i)
        {
            delete[] demos[i].data;
        }
        demos.remove(0, n);
    }

    void adddemo()
    {
        if(!demotmp) return;
        int len = static_cast<int>(min(demotmp->size(), stream::offset((maxdemosize<<20) + 0x10000)));
        demofile &d = demos.add();
        time_t t = time(NULL);
        char *timestr = ctime(&t),
             *trim = timestr + strlen(timestr);
        while(trim>timestr && iscubespace(*--trim))
        {
            *trim = '\0';
        }
        formatstring(d.info, "%s: %s, %s, %.2f%s", timestr, modeprettyname(gamemode), smapname, len > 1024*1024 ? len/(1024*1024.f) : len/1024.0f, len > 1024*1024 ? "MB" : "kB");
        sendservmsgf("demo \"%s\" recorded", d.info);
        d.data = new uchar[len];
        d.len = len;
        demotmp->seek(0, SEEK_SET);
        demotmp->read(d.data, len);
        DELETEP(demotmp);
    }

    void enddemorecord()
    {
        if(!demorecord) return;

        DELETEP(demorecord);

        if(!demotmp) return;
        if(!maxdemos || !maxdemosize) { DELETEP(demotmp); return; }

        prunedemos(1);
        adddemo();
    }

    void writedemo(int chan, void *data, int len)
    {
        if(!demorecord) return;
        int stamp[3] = { gamemillis, chan, len };
        demorecord->write(stamp, sizeof(stamp));
        demorecord->write(data, len);
        if(demorecord->rawtell() >= (maxdemosize<<20)) enddemorecord();
    }

    void recordpacket(int chan, void *data, int len)
    {
        writedemo(chan, data, len);
    }

    int welcomepacket(packetbuf &p, clientinfo *ci);
    void sendwelcome(clientinfo *ci);

    void listdemos(int cn)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, NetMsg_SendDemoList);
        putint(p, demos.length());
        for(int i = 0; i < demos.length(); i++)
        {
            sendstring(demos[i].info, p);
        }
        sendpacket(cn, 1, p.finalize());
    }

    void cleardemos(int n)
    {
        if(!n)
        {
            for(int i = 0; i < demos.length(); i++)
            {
                delete[] demos[i].data;
            }
            demos.shrink(0);
            sendservmsg("cleared all demos");
        }
        else if(demos.inrange(n-1))
        {
            delete[] demos[n-1].data;
            demos.remove(n-1);
            sendservmsgf("cleared demo %d", n);
        }
    }

    static void freegetdemo(ENetPacket *packet)
    {
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(ci->getdemo == packet) ci->getdemo = NULL;
        }
    }

    void senddemo(clientinfo *ci, int num)
    {
        if(ci->getdemo) return;
        if(!num) num = demos.length();
        if(!demos.inrange(num-1)) return;
        demofile &d = demos[num-1];
        if((ci->getdemo = sendf(ci->clientnum, 2, "rim", NetMsg_SendDemo, d.len, d.data)))
            ci->getdemo->freeCallback = freegetdemo;
    }

    void enddemoplayback()
    {
        if(!demoplayback) return;
        DELETEP(demoplayback);

        for(int i = 0; i < clients.length(); i++)
        {
            sendf(clients[i]->clientnum, 1, "ri3", NetMsg_DemoPlayback, 0, clients[i]->clientnum);
        }

        sendservmsg("demo playback finished");

        for(int i = 0; i < clients.length(); i++)
        {
            sendwelcome(clients[i]);
        }
    }

    void stopdemo()
    {
        if(modecheck(gamemode, Mode_Demo)) enddemoplayback();
        else enddemorecord();
    }

    void pausegame(bool val, clientinfo *ci = NULL)
    {
        if(gamepaused==val) return;
        gamepaused = val;
        sendf(-1, 1, "riii", NetMsg_PauseGame, gamepaused ? 1 : 0, ci ? ci->clientnum : -1);
    }

    void checkpausegame()
    {
        if(!gamepaused)
        {
            return;
        }
        int admins = 0;
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i]->privilege >= (restrictpausegame ? Priv_Admin : Priv_Master) || clients[i]->local)
            {
                admins++;
            }
        }
        if(!admins)
        {
            pausegame(false);
        }
    }

    void forcepaused(bool paused)
    {
        pausegame(paused);
    }

    bool ispaused() { return gamepaused; }

    void changegamespeed(int val, clientinfo *ci = NULL)
    {
        val = std::clamp(val, 10, 1000);
        if(gamespeed==val)
        {
            return;
        }
        gamespeed = val;
        sendf(-1, 1, "riii", NetMsg_GameSpeed, gamespeed, ci ? ci->clientnum : -1);
    }

    void forcegamespeed(int speed)
    {
        changegamespeed(speed);
    }

    int scaletime(int t) { return t*gamespeed; }

    SVAR(serverauth, "");

    struct userkey
    {
        char *name;
        char *desc;

        userkey() : name(NULL), desc(NULL) {}
        userkey(char *name, char *desc) : name(name), desc(desc) {}
    };

    static inline uint hthash(const userkey &k) { return ::hthash(k.name); }
    static inline bool htcmp(const userkey &x, const userkey &y) { return !strcmp(x.name, y.name) && !strcmp(x.desc, y.desc); }

    struct userinfo : userkey
    {
        void *pubkey;
        int privilege;

        userinfo() : pubkey(NULL), privilege(Priv_None) {}
        ~userinfo() { delete[] name; delete[] desc; if(pubkey) freepubkey(pubkey); }
    };
    hashset<userinfo> users;

    void adduser(char *name, char *desc, char *pubkey, char *priv)
    {
        userkey key(name, desc);
        userinfo &u = users[key];
        if(u.pubkey) { freepubkey(u.pubkey); u.pubkey = NULL; }
        if(!u.name) u.name = newstring(name);
        if(!u.desc) u.desc = newstring(desc);
        u.pubkey = parsepubkey(pubkey);
        switch(priv[0])
        {
            case 'a': case 'A': u.privilege = Priv_Admin; break;
            case 'm': case 'M': default: u.privilege = Priv_Auth; break;
            case 'n': case 'N': u.privilege = Priv_None; break;
        }
    }
    COMMAND(adduser, "ssss");

    void clearusers()
    {
        users.clear();
    }
    COMMAND(clearusers, "");

    void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen)
    {
        char buf[2*sizeof(string)];
        formatstring(buf, "%d %d %s", cn, sessionid, pwd);
        if(!hashstring(buf, result, maxlen)) *result = '\0';
    }

    bool checkpassword(clientinfo *ci, const char *wanted, const char *given)
    {
        string hash;
        hashpassword(ci->clientnum, ci->sessionid, wanted, hash, sizeof(hash));
        return !strcmp(hash, given);
    }

    void revokemaster(clientinfo *ci)
    {
        ci->privilege = Priv_None;
        if(ci->state.state==ClientState_Spectator && !ci->local)
        {
            aiman::removeai(ci);
        }
    }

    bool setmaster(clientinfo *ci, bool val, const char *pass = "", const char *authname = NULL, const char *authdesc = NULL, int authpriv = Priv_Master, bool force = false, bool trial = false)
    {
        if(authname && !val)
        {
            return false;
        }
        const char *name = "";
        if(val)
        {
            bool haspass = adminpass[0] && checkpassword(ci, adminpass, pass);
            int wantpriv = ci->local || haspass ? Priv_Admin : authpriv;
            if(wantpriv <= ci->privilege)
            {
                return true;
            }
            else if(wantpriv <= Priv_Master && !force)
            {
                if(ci->state.state==ClientState_Spectator)
                {
                    sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, "Spectators may not claim master.");
                    return false;
                }
                for(int i = 0; i < clients.length(); i++)
                {
                    if(ci!=clients[i] && clients[i]->privilege)
                    {
                        sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, "Master is already claimed.");
                        return false;
                    }
                }
                if(!authname && !(mastermask&MM_AUTOAPPROVE) && !ci->privilege && !ci->local)
                {
                    sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, "This server requires you to use the \"/auth\" command to claim master.");
                    return false;
                }
            }
            if(trial)
            {
                return true;
            }
            ci->privilege = wantpriv;
            name = privname(ci->privilege);
        }
        else
        {
            if(!ci->privilege) return false;
            if(trial) return true;
            name = privname(ci->privilege);
            revokemaster(ci);
        }
        bool hasmaster = false;
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i]->local || clients[i]->privilege >= Priv_Master)
            {
                hasmaster = true;
            }
        }
        if(!hasmaster)
        {
            mastermode = MasterMode_Open;
            allowedips.shrink(0);
        }
        string msg;
        if(val && authname)
        {
            if(authdesc && authdesc[0])
            {
                formatstring(msg, "%s claimed %s as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci), name, authname, authdesc);
            }
            else
            {
                formatstring(msg, "%s claimed %s as '\fs\f5%s\fr'", colorname(ci), name, authname);
            }
        }
        else
        {
            formatstring(msg, "%s %s %s", colorname(ci), val ? "claimed" : "relinquished", name);
        }
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, NetMsg_ServerMsg);
        sendstring(msg, p);
        putint(p, NetMsg_CurrentMaster);
        putint(p, mastermode);
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i]->privilege >= Priv_Master)
            {
                putint(p, clients[i]->clientnum);
                putint(p, clients[i]->privilege);
            }
        }
        putint(p, -1);
        sendpacket(-1, 1, p.finalize());
        checkpausegame();
        return true;
    }

    savedscore *findscore(clientinfo *ci, bool insert)
    {
        uint ip = getclientip(ci->clientnum);
        if(!ip && !ci->local)
        {
            return 0;
        }
        if(!insert)
        {
            for(int i = 0; i < clients.length(); i++)
            {
                clientinfo *oi = clients[i];
                if(oi->clientnum != ci->clientnum && getclientip(oi->clientnum) == ip && !strcmp(oi->name, ci->name))
                {
                    oi->state.timeplayed += lastmillis - oi->state.lasttimeplayed;
                    oi->state.lasttimeplayed = lastmillis;
                    static savedscore curscore;
                    curscore.save(oi->state);
                    return &curscore;
                }
            }
        }
        for(int i = 0; i < scores.length(); i++)
        {
            savedscore &sc = scores[i];
            if(sc.ip == ip && !strcmp(sc.name, ci->name))
            {
                return &sc;
            }
        }
        if(!insert)
        {
            return 0;
        }
        savedscore &sc = scores.add();
        sc.ip = ip;
        copystring(sc.name, ci->name);
        return &sc;
    }

    void savescore(clientinfo *ci)
    {
        savedscore *sc = findscore(ci, true);
        if(sc)
        {
            sc->save(ci->state);
        }
    }

    static struct msgfilter
    {
        uchar msgmask[NetMsg_NumMsgs];

        msgfilter(int msg, ...)
        {
            memset(msgmask, 0, sizeof(msgmask));
            va_list msgs;
            va_start(msgs, msg);
            for(uchar val = 1; msg < NetMsg_NumMsgs; msg = va_arg(msgs, int))
            {
                if(msg < 0)
                {
                    val = static_cast<uchar>(-msg);
                }
                else
                {
                    msgmask[msg] = val;
                }
            }
            va_end(msgs);
        }

        uchar operator[](int msg) const { return msg >= 0 && msg < NetMsg_NumMsgs ? msgmask[msg] : 0; }
    } msgfilter(-1, NetMsg_Connect, NetMsg_ServerInfo, NetMsg_InitClient, NetMsg_Welcome, NetMsg_MapChange, NetMsg_ServerMsg, NetMsg_Damage, NetMsg_Hitpush, NetMsg_ShotFX, NetMsg_ExplodeFX, NetMsg_Died, NetMsg_SpawnState, NetMsg_ForceDeath, NetMsg_TeamInfo, NetMsg_ItemAcceptance, NetMsg_ItemSpawn, NetMsg_TimeUp, NetMsg_ClientDiscon, NetMsg_CurrentMaster, NetMsg_Pong, NetMsg_Resume, NetMsg_SendDemoList, NetMsg_SendDemo, NetMsg_DemoPlayback, NetMsg_SendMap, NetMsg_DropFlag, NetMsg_ScoreFlag, NetMsg_ReturnFlag, NetMsg_ResetFlag, NetMsg_Client, NetMsg_AuthChallenge, NetMsg_InitAI, NetMsg_DemoPacket, -2, NetMsg_CalcLight, NetMsg_Remip, NetMsg_Newmap, NetMsg_GetMap, NetMsg_SendMap, NetMsg_Clipboard, -3, NetMsg_EditEnt, NetMsg_EditFace, NetMsg_EditTex, NetMsg_EditMat, NetMsg_EditFlip, NetMsg_Copy, NetMsg_Paste, NetMsg_Rotate, NetMsg_Replace, NetMsg_DelCube, NetMsg_EditVar, NetMsg_EditVSlot, NetMsg_Undo, NetMsg_Redo, -4, NetMsg_Pos, NetMsg_NumMsgs),
      connectfilter(-1, NetMsg_Connect, -2, NetMsg_AuthAnswer, -3, NetMsg_Ping, NetMsg_NumMsgs);

    struct worldstate
    {
        int uses, len;
        uchar *data;

        worldstate() : uses(0), len(0), data(NULL) {}

        void setup(int n) { len = n; data = new uchar[n]; }
        void cleanup() { DELETEA(data); len = 0; }
        bool contains(const uchar *p) const { return p >= data && p < &data[len]; }
    };
    vector<worldstate> worldstates;
    bool reliablemessages = false;

    void cleanworldstate(ENetPacket *packet)
    {
        for(int i = 0; i < worldstates.length(); i++)
        {
            worldstate &ws = worldstates[i];
            if(!ws.contains(packet->data))
            {
                continue;
            }
            ws.uses--;
            if(ws.uses <= 0)
            {
                ws.cleanup();
                worldstates.removeunordered(i);
            }
            break;
        }
    }

    template<class T>
    void sendstate(servstate &gs, T &p)
    {
        putint(p, gs.lifesequence);
        putint(p, gs.health);
        putint(p, gs.maxhealth);
        putint(p, gs.gunselect);
        for(int i = 0; i < Gun_NumGuns; ++i)
        {
            putint(p, gs.ammo[i]);
        }
    }

    void spawnstate(clientinfo *ci)
    {
        servstate &gs = ci->state;
        gs.spawnstate(gamemode);
        gs.lifesequence = (gs.lifesequence + 1)&0x7F;
    }

    void sendwelcome(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        int chan = welcomepacket(p, ci);
        sendpacket(ci->clientnum, chan, p.finalize());
    }

    void putinitclient(clientinfo *ci, packetbuf &p)
    {
        if(ci->state.aitype != AI_None)
        {
            putint(p, NetMsg_InitAI);
            putint(p, ci->clientnum);
            putint(p, ci->ownernum);
            putint(p, ci->state.aitype);
            putint(p, ci->state.skill);
            putint(p, ci->playermodel);
            putint(p, ci->playercolor);
            putint(p, ci->team);
            sendstring(ci->name, p);
        }
        else
        {
            putint(p, NetMsg_InitClient);
            putint(p, ci->clientnum);
            sendstring(ci->name, p);
            putint(p, ci->team);
            putint(p, ci->playermodel);
            putint(p, ci->playercolor);
        }
    }

    void welcomeinitclient(packetbuf &p, int exclude = -1)
    {
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(!ci->connected || ci->clientnum == exclude) continue;

            putinitclient(ci, p);
        }
    }

    bool hasmap(clientinfo *ci)
    {
        return (modecheck(gamemode, Mode_Edit) && (clients.length() > 0 || ci->local)) ||
               (smapname[0] && (modecheck(gamemode, Mode_Untimed) || gamemillis < gamelimit || (ci->state.state==ClientState_Spectator && !ci->privilege && !ci->local) || numclients(ci->clientnum, true, true, true)));
    }

    int welcomepacket(packetbuf &p, clientinfo *ci)
    {
        putint(p, NetMsg_Welcome);
        putint(p, NetMsg_MapChange);
        sendstring(smapname, p);
        putint(p, gamemode);
        putint(p, notgotitems ? 1 : 0);
        if(!ci || (!modecheck(gamemode, Mode_Untimed) && smapname[0]))
        {
            putint(p, NetMsg_TimeUp);
            putint(p, gamemillis < gamelimit && !interm ? max((gamelimit - gamemillis)/1000, 1) : 0);
        }
        if(!notgotitems)
        {
            putint(p, NetMsg_ItemList);
            for(int i = 0; i < sents.length(); i++)
            {
                if(sents[i].spawned)
                {
                    putint(p, i);
                    putint(p, sents[i].type);
                }
            }
            putint(p, -1);
        }
        bool hasmaster = false;
        if(mastermode != MasterMode_Open)
        {
            putint(p, NetMsg_CurrentMaster);
            putint(p, mastermode);
            hasmaster = true;
        }
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i]->privilege >= Priv_Master)
            {
                if(!hasmaster)
                {
                    putint(p, NetMsg_CurrentMaster);
                    putint(p, mastermode);
                    hasmaster = true;
                }
                putint(p, clients[i]->clientnum);
                putint(p, clients[i]->privilege);
            }
        }
        if(hasmaster)
        {
            putint(p, -1);
        }
        if(gamepaused)
        {
            putint(p, NetMsg_PauseGame);
            putint(p, 1);
            putint(p, -1);
        }
        if(gamespeed != 100)
        {
            putint(p, NetMsg_GameSpeed);
            putint(p, gamespeed);
            putint(p, -1);
        }
        if(modecheck(gamemode, Mode_Team))
        {
            putint(p, NetMsg_TeamInfo);
            for(int i = 0; i < MAXTEAMS; ++i)
            {
                teaminfo &t = teaminfos[i];
                putint(p, t.frags);
            }
        }
        if(ci)
        {
            putint(p, NetMsg_SetTeam);
            putint(p, ci->clientnum);
            putint(p, ci->team);
            putint(p, -1);
        }
        if(ci && (modecheck(gamemode, Mode_Demo) || !modecheck(gamemode, Mode_LocalOnly)) && ci->state.state!=ClientState_Spectator)
        {
            if(smode && !smode->canspawn(ci, true))
            {
                ci->state.state = ClientState_Dead;
                putint(p, NetMsg_ForceDeath);
                putint(p, ci->clientnum);
                sendf(-1, 1, "ri2x", NetMsg_ForceDeath, ci->clientnum, ci->clientnum);
            }
            else
            {
                servstate &gs = ci->state;
                spawnstate(ci);
                putint(p, NetMsg_SpawnState);
                putint(p, ci->clientnum);
                sendstate(gs, p);
                gs.lastspawn = gamemillis;
            }
        }
        if(ci && ci->state.state==ClientState_Spectator)
        {
            putint(p, NetMsg_Spectator);
            putint(p, ci->clientnum);
            putint(p, 1);
            sendf(-1, 1, "ri3x", NetMsg_Spectator, ci->clientnum, 1, ci->clientnum);
        }
        if(!ci || clients.length()>1)
        {
            putint(p, NetMsg_Resume);
            for(int i = 0; i < clients.length(); i++)
            {
                clientinfo *oi = clients[i];
                if(ci && oi->clientnum==ci->clientnum)
                {
                    continue;
                }
                putint(p, oi->clientnum);
                putint(p, oi->state.state);
                putint(p, oi->state.frags);
                putint(p, oi->state.flags);
                putint(p, oi->state.deaths);
                sendstate(oi->state, p);
            }
            putint(p, -1);
            welcomeinitclient(p, ci ? ci->clientnum : -1);
        }
        if(smode) smode->initclient(ci, p, true);
        return 1;
    }

    void sendresume(clientinfo *ci)
    {
        servstate &gs = ci->state;
        sendf(-1, 1, "ri3i7vi", NetMsg_Resume, ci->clientnum, gs.state,
            gs.frags, gs.flags, gs.deaths,
            gs.lifesequence,
            gs.health, gs.maxhealth,
            gs.gunselect, Gun_NumGuns, gs.ammo, -1);
    }

    void sendinitclient(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putinitclient(ci, p);
        sendpacket(-1, 1, p.finalize(), ci->clientnum);
    }

    struct votecount
    {
        char *map;
        int mode, count;
        votecount() {}
        votecount(char *s, int n) : map(s), mode(n), count(0) {}
    };

    void checkintermission()
    {
        if(gamemillis >= gamelimit && !interm)
        {
            sendf(-1, 1, "ri2", NetMsg_TimeUp, 0);
            if(smode)
            {
                smode->intermission();
            }
            changegamespeed(100);
            interm = gamemillis + 10000;
        }
    }

    void dodamage(clientinfo *target, clientinfo *actor, int damage, int atk, const vec &hitpush = vec(0, 0, 0))
    {
        servstate &ts = target->state;
        ts.dodamage(damage);
        if(target!=actor && !modecheck(gamemode, Mode_Team) && target->team != actor->team) actor->state.damage += damage;
        sendf(-1, 1, "ri5", NetMsg_Damage, target->clientnum, actor->clientnum, damage, ts.health);
        if(target==actor)
        {
            target->setpushed();
        }
        else if(!hitpush.iszero())
        {
            ivec v(vec(hitpush).rescale(DNF));
            sendf(ts.health<=0 ? -1 : target->ownernum, 1, "ri7", NetMsg_Hitpush, target->clientnum, atk, damage, v.x, v.y, v.z);
            target->setpushed();
        }
        if(ts.health<=0)
        {
            target->state.deaths++;
            int fragvalue = smode ? smode->fragvalue(target, actor) : (target==actor || (modecheck(gamemode, Mode_Team) && (target->team == actor->team)) ? -1 : 1);
            actor->state.frags += fragvalue;
            if(fragvalue>0)
            {
                int friends = 0, enemies = 0; // note: friends also includes the fragger
                if(modecheck(gamemode, Mode_Team))
                {
                    for(int i = 0; i < clients.length(); i++)
                    {
                        if(clients[i]->team != actor->team)
                        {
                            enemies++;
                        }
                        else
                        {
                            friends++;
                        }
                    }
                }
                else
                {
                    friends = 1;
                    enemies = clients.length()-1;
                }
                actor->state.effectiveness += fragvalue*friends/static_cast<float>(max(enemies, 1));
            }
            teaminfo *t = modecheck(gamemode, Mode_Team) && VALID_TEAM(actor->team) ? &teaminfos[actor->team-1] : NULL;
            if(t) t->frags += fragvalue;
            sendf(-1, 1, "ri5", NetMsg_Died, target->clientnum, actor->clientnum, actor->state.frags, t ? t->frags : 0);
            target->position.setsize(0);
            if(smode) smode->died(target, actor);
            ts.state = ClientState_Dead;
            ts.lastdeath = gamemillis;
            if(actor!=target && modecheck(gamemode, Mode_Team) && actor->team == target->team)
            {
                actor->state.teamkills++;
                addteamkill(actor, target, 1);
            }
            ts.deadflush = ts.lastdeath + DEATHMILLIS;
            // don't issue respawn yet until DEATHMILLIS has elapsed
            // ts.respawn();
        }
    }

    void suicide(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(gs.state!=ClientState_Alive) return;
        int fragvalue = smode ? smode->fragvalue(ci, ci) : -1;
        ci->state.frags += fragvalue;
        ci->state.deaths++;
        teaminfo *t = modecheck(gamemode, Mode_Team) && VALID_TEAM(ci->team) ? &teaminfos[ci->team-1] : NULL;
        if(t) t->frags += fragvalue;
        sendf(-1, 1, "ri5", NetMsg_Died, ci->clientnum, ci->clientnum, gs.frags, t ? t->frags : 0);
        ci->position.setsize(0);
        if(smode) smode->died(ci, NULL);
        gs.state = ClientState_Dead;
        gs.lastdeath = gamemillis;
        gs.respawn();
    }

    void clearevent(clientinfo *ci)
    {
        delete ci->events.remove(0);
    }

    void flushevents(clientinfo *ci, int millis)
    {
        while(ci->events.length())
        {
            gameevent *ev = ci->events[0];
            if(ev->flush(ci, millis)) clearevent(ci);
            else break;
        }
    }

    void processevents()
    {
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            flushevents(ci, gamemillis);
        }
    }

    void cleartimedevents(clientinfo *ci)
    {
        int keep = 0;
        for(int i = 0; i < ci->events.length(); i++)
        {
            if(ci->events[i]->keepable())
            {
                if(keep < i)
                {
                    for(int j = keep; j < i; j++) delete ci->events[j];
                    ci->events.remove(keep, i - keep);
                    i = keep;
                }
                keep = i+1;
                continue;
            }
        }
        while(ci->events.length() > keep) delete ci->events.pop();
        ci->timesync = false;
    }

    void forcespectator(clientinfo *ci)
    {
        if(ci->state.state==ClientState_Alive) suicide(ci);
        if(smode) smode->leavegame(ci);
        ci->state.state = ClientState_Spectator;
        ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
        if(!ci->local && (!ci->privilege || ci->warned)) aiman::removeai(ci);
        sendf(-1, 1, "ri3", NetMsg_Spectator, ci->clientnum, 1);
    }

    struct crcinfo
    {
        int crc, matches;

        crcinfo() {}
        crcinfo(int crc, int matches) : crc(crc), matches(matches) {}

        static bool compare(const crcinfo &x, const crcinfo &y) { return x.matches > y.matches; }
    };

    VAR(modifiedmapspectator, 0, 1, 2);

    void sendservinfo(clientinfo *ci)
    {
        sendf(ci->clientnum, 1, "ri5ss", NetMsg_ServerInfo, ci->clientnum, PROTOCOL_VERSION, ci->sessionid, serverpass[0] ? 1 : 0, serverdesc, serverauth);
    }

    void noclients()
    {
        bannedips.shrink(0);
        aiman::clearai();
    }

    void localconnect(int n)
    {
        clientinfo *ci = getinfo(n);
        ci->clientnum = ci->ownernum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (randomint(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;
        ci->local = true;

        connects.add(ci);
        sendservinfo(ci);
    }

    void localdisconnect(int n)
    {
        if(modecheck(gamemode, Mode_Demo)) enddemoplayback();
        clientdisconnect(n);
    }

    void clientdisconnect(int n)
    {
        clientinfo *ci = getinfo(n);
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i]->authkickvictim == ci->clientnum) clients[i]->cleanauth();
        }
        if(ci->connected)
        {
            if(ci->privilege)
            {
                setmaster(ci, false);
            }
            if(smode)
            {
                smode->leavegame(ci, true);
            }
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
            savescore(ci);
            sendf(-1, 1, "ri2", NetMsg_ClientDiscon, n);
            clients.removeobj(ci);
            aiman::removeai(ci);
            if(!numclients(-1, false, true))
            {
                noclients(); // bans clear when server empties
            }
            if(ci->local)
            {
                checkpausegame();
            }
        }
        else
        {
            connects.removeobj(ci);
        }
    }

    int reserveclients()
    {
        return 3;
    }

    extern void verifybans();

    struct banlist
    {
        vector<ipmask> bans;

        void clear() { bans.shrink(0); }

        bool check(uint ip)
        {
            for(int i = 0; i < bans.length(); i++)
            {
                if(bans[i].check(ip))
                {
                    return true;
                }
            }
            return false;
        }

        void add(const char *ipname)
        {
            ipmask ban;
            ban.parse(ipname);
            bans.add(ban);

            verifybans();
        }
    } ipbans, gbans;

    bool checkbans(uint ip)
    {
        for(int i = 0; i < bannedips.length(); i++)
        {
            if(bannedips[i].ip==ip)
            {
                return true;
            }
        }
        return ipbans.check(ip) || gbans.check(ip);
    }

    void verifybans()
    {
        for(int i = clients.length(); --i >=0;) //note reverse iteration
        {
            clientinfo *ci = clients[i];
            if(ci->state.aitype != AI_None || ci->local || ci->privilege >= Priv_Admin) continue;
            if(checkbans(getclientip(ci->clientnum))) disconnect_client(ci->clientnum, Discon_IPBan);
        }
    }

    ICOMMAND(clearipbans, "", (), ipbans.clear());
    ICOMMAND(ipban, "s", (const char *ipname), ipbans.add(ipname));

    bool allowbroadcast(int n)
    {
        clientinfo *ci = getinfo(n);
        return ci && ci->connected;
    }

    clientinfo *findauth(uint id)
    {
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i]->authreq == id)
            {
                return clients[i];
            }
        }
        return NULL;
    }

    void authfailed(clientinfo *ci)
    {
        if(!ci)
        {
            return;
        }
        ci->cleanauth();
        if(ci->connectauth)
        {
            disconnect_client(ci->clientnum, ci->connectauth);
        }
    }

    void authchallenged(uint id, const char *val, const char *desc = "")
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        sendf(ci->clientnum, 1, "risis", NetMsg_AuthChallenge, desc, id, val);
    }

    uint nextauthreq = 0;

    void masterconnected()
    {
    }

    void masterdisconnected()
    {
        for(int i = clients.length(); --i >=0;) //note reverse iteration
        {
            clientinfo *ci = clients[i];
            if(ci->authreq) authfailed(ci);
        }
    }

    int laninfoport() { return IMPRIMIS_LANINFO_PORT; }
    int serverport() { return IMPRIMIS_SERVER_PORT; }
    const char *defaultmaster() { return "project-imprimis.org"; }
    int masterport() { return IMPRIMIS_MASTER_PORT; }
    int numchannels() { return 3; }

    enum Ext
    {
        Ack                  = -1,
        Version              =  105,
        NoError              =  0,
        Error                =  1,
        PlayerStatsRespIds   = -10,
        PlayerStatsRespStats = -11,
        Uptime               =  0,
        PlayerStats          =  1,
        TeamScore            =  2
    };

    /*
        Client:
        -----
        A: 0 Ext::Uptime
        B: 0 Ext::PlayerStats cn #a client number or -1 for all players#
        C: 0 Ext::TeamScore

        Server:
        --------
        A: 0 Ext::Uptime Ext::Ack Ext::Version uptime #in seconds#
        B: 0 Ext::PlayerStats cn #send by client# Ext::Ack Ext::Version 0 or 1 #error, if cn was > -1 and client does not exist# ...
             Ext::PlayerStatsRespIds pid(s) #1 packet#
             Ext::PlayerStatsRespStats pid playerdata #1 packet for each player#
        C: 0 Ext::TeamScore Ext::Ack Ext::Version 0 or 1 #error, no teammode# remaining_time gamemode loop(teamdata [numbases bases] or -1)

        Errors:
        --------------
        B:C:default: 0 command Ext::Ack Ext::Version Ext::Error
    */

    VAR(extinfoip, 0, 0, 1);

    void extinfoplayer(ucharbuf &p, clientinfo *ci)
    {
        ucharbuf q = p;
        putint(q, Ext::PlayerStatsRespStats); // send player stats following
        putint(q, ci->clientnum); //add player id
        putint(q, ci->ping);
        sendstring(ci->name, q);
        sendstring(TEAM_NAME(modecheck(gamemode, Mode_Team) ? ci->team : 0), q);
        putint(q, ci->state.frags);
        putint(q, ci->state.flags);
        putint(q, ci->state.deaths);
        putint(q, ci->state.teamkills);
        putint(q, ci->state.damage*100/max(ci->state.shotdamage,1));
        putint(q, ci->state.health);
        putint(q, 0);
        putint(q, ci->state.gunselect);
        putint(q, ci->privilege);
        putint(q, ci->state.state);
        uint ip = extinfoip ? getclientip(ci->clientnum) : 0;
        q.put((uchar*)&ip, 3);
        sendserverinforeply(q);
    }

    static inline void extinfoteamscore(ucharbuf &p, int team, int score)
    {
        sendstring(TEAM_NAME(team), p);
        putint(p, score);
        if(!smode || !smode->extinfoteam(team, p))
        {
            putint(p,-1); //no bases follow
        }
    }

    void extinfoteams(ucharbuf &p)
    {
        putint(p, modecheck(gamemode, Mode_Team) ? 0 : 1);
        putint(p, gamemode);
        putint(p, max((gamelimit - gamemillis)/1000, 0));
        if(!modecheck(gamemode, Mode_Team))
        {
            return;
        }

        vector<teamscore> scores;
        if(smode && smode->hidefrags())
        {
            smode->getteamscores(scores);
        }
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state!=ClientState_Spectator && VALID_TEAM(ci->team) && scores.htfind(ci->team) < 0)
            {
                if(smode && smode->hidefrags())
                {
                    scores.add(teamscore(ci->team, 0));
                }
                else
                {
                    teaminfo &t = teaminfos[ci->team-1];
                    scores.add(teamscore(ci->team, t.frags));
                }
            }
        }
        for(int i = 0; i < scores.length(); i++)
        {
            extinfoteamscore(p, scores[i].team, scores[i].score);
        }
    }

    void extserverinforeply(ucharbuf &req, ucharbuf &p)
    {
        int extcmd = getint(req); // extended commands
        //Build a new packet
        putint(p, Ext::Ack); //send ack
        putint(p, Ext::Version); //send version of extended info
        switch(extcmd)
        {
            case Ext::Uptime:
            {
                putint(p, totalsecs); //in seconds
                break;
            }
            case Ext::PlayerStats:
            {
                int cn = getint(req); //a special player, -1 for all
                clientinfo *ci = NULL;
                if(cn >= 0)
                {
                    for(int i = 0; i < clients.length(); i++)
                    {
                        if(clients[i]->clientnum == cn)
                        {
                            ci = clients[i];
                            break;
                        }
                    }
                    if(!ci)
                    {
                        putint(p, Ext::Error); //client requested by id was not found
                        sendserverinforeply(p);
                        return;
                    }
                }
                putint(p, Ext::NoError); //so far no error can happen anymore
                ucharbuf q = p; //remember buffer position
                putint(q, Ext::PlayerStatsRespIds); //send player ids following
                if(ci)
                {
                    putint(q, ci->clientnum);
                }
                else
                {
                    for(int i = 0; i < clients.length(); i++)
                    {
                        putint(q, clients[i]->clientnum);
                    }
                }
                sendserverinforeply(q);
                if(ci)
                {
                    extinfoplayer(p, ci);
                }
                else
                {
                    for(int i = 0; i < clients.length(); i++)
                    {
                        extinfoplayer(p, clients[i]);
                    }
                }
                return;
            }
            case Ext::TeamScore:
            {
                extinfoteams(p);
                break;
            }
            default:
            {
                putint(p, Ext::Error);
                break;
            }
        }
        sendserverinforeply(p);
    }
//end of extinfo
    void serverinforeply(ucharbuf &req, ucharbuf &p)
    {
        if(req.remaining() && !getint(req))
        {
            extserverinforeply(req, p);
            return;
        }
        putint(p, PROTOCOL_VERSION);
        putint(p, numclients(-1, false, true));
        putint(p, maxclients);
        putint(p, gamepaused || gamespeed != 100 ? 5 : 3); // number of attrs following
        putint(p, gamemode);
        putint(p, !modecheck(gamemode, Mode_Untimed) ? max((gamelimit - gamemillis)/1000, 0) : 0);
        putint(p, serverpass[0] ? MasterMode_Password : (modecheck(gamemode, Mode_LocalOnly) ? MasterMode_Private : (mastermode || mastermask&MM_AUTOAPPROVE ? mastermode : MasterMode_Auth)));
        if(gamepaused || gamespeed != 100)
        {
            putint(p, gamepaused ? 1 : 0);
            putint(p, gamespeed);
        }
        sendstring(smapname, p);
        sendstring(serverdesc, p);
        sendserverinforeply(p);
    }
    int protocolversion()
    {
        return PROTOCOL_VERSION;
    }
    // server-side ai manager
    // note that server does not handle actual bot logic,
    // which is offloaded to the clients with the best connection
    namespace aiman
    {
        bool dorefresh = false, botbalance = true;
        VARN(serverbotlimit, botlimit, 0, 8, maxbots);
        VAR(serverbotbalance, 0, 1, 1);

        void calcteams(vector<teamscore> &teams)
        {
            for(int i = 0; i < clients.length(); i++)
            {
                clientinfo *ci = clients[i];
                if(ci->state.state==ClientState_Spectator || !VALID_TEAM(ci->team))
                {
                    continue;
                }
                teamscore *t = NULL;
                for(int j = 0; j < teams.length(); j++)
                {
                    if(teams[j].team == ci->team)
                    {
                        t = &teams[j];
                        break;
                    }
                }
                if(t)
                {
                    t->score++;
                }
                else
                {
                    teams.add(teamscore(ci->team, 1));
                }
            }
            teams.sort(teamscore::compare);
            if(teams.length() < MAXTEAMS)
            {
                for(int i = 0; i < MAXTEAMS; ++i)
                {
                    if(teams.htfind(1+i) < 0)
                    {
                        teams.add(teamscore(1+i, 0));
                    }
                }
            }
        }

        void balanceteams()
        {
            vector<teamscore> teams;
            calcteams(teams);
            vector<clientinfo *> reassign;
            for(int i = 0; i < bots.length(); i++)
            {
                if(bots[i])
                {
                    reassign.add(bots[i]);
                }
            }
            while(reassign.length() && teams.length() && teams[0].score > teams.last().score + 1)
            {
                teamscore &t = teams.last();
                clientinfo *bot = NULL;
                for(int i = 0; i < reassign.length(); i++)
                {
                    if(reassign[i] && reassign[i]->team != teams[0].team)
                    {
                        bot = reassign.removeunordered(i);
                        teams[0].score--;
                        t.score++;
                        for(int j = teams.length() - 2; j >= 0; j--) //note reverse iteration
                        {
                            if(teams[j].score >= teams[j+1].score)
                            {
                                break;
                            }
                            swap(teams[j], teams[j+1]);
                        }
                        break;
                    }
                }
                if(bot)
                {
                    if(smode && bot->state.state==ClientState_Alive)
                    {
                        smode->changeteam(bot, bot->team, t.team);
                    }
                    bot->team = t.team;
                    sendf(-1, 1, "riiii", NetMsg_SetTeam, bot->clientnum, bot->team, 0);
                }
                else
                {
                    teams.remove(0, 1);
                }
            }
        }

        int chooseteam()
        {
            vector<teamscore> teams;
            calcteams(teams);
            return teams.length() ? teams.last().team : 0;
        }

        //this fxn could be entirely in the return statement but is seperated for clarity
        static inline bool validaiclient(clientinfo *ci)
        {
            if(ci->clientnum >= 0 && ci->state.aitype == AI_None)
            {
                if(ci->state.state!=ClientState_Spectator || ci->local || (ci->privilege && !ci->warned))
                {
                    return true;
                }
            }
            return false;
        }

        clientinfo *findaiclient(clientinfo *exclude = NULL)
        {
            clientinfo *least = NULL;
            for(int i = 0; i < clients.length(); i++)
            {
                clientinfo *ci = clients[i];
                if(!validaiclient(ci) || ci==exclude)
                {
                    continue;
                }
                if(!least || ci->bots.length() < least->bots.length())
                {
                    least = ci;
                }
            }
            return least;
        }

        bool addai(int skill, int limit)
        {
            int numai = 0,
                cn = -1,
                maxai = limit >= 0 ? min(limit, maxbots) : maxbots;
            for(int i = 0; i < bots.length(); i++)
            {
                clientinfo *ci = bots[i];
                if(!ci || ci->ownernum < 0)
                {
                    if(cn < 0)
                    {
                        cn = i;
                        continue;
                    }
                }
                numai++;
            }
            if(numai >= maxai)
            {
                return false;
            }
            if(bots.inrange(cn))
            {
                clientinfo *ci = bots[cn];
                if(ci)
                { // reuse a slot that was going to removed

                    clientinfo *owner = findaiclient();
                    ci->ownernum = owner ? owner->clientnum : -1;
                    if(owner)
                    {
                        owner->bots.add(ci);
                    }
                    ci->aireinit = 2;
                    dorefresh = true;
                    return true;
                }
            }
            else
            {
                cn = bots.length();
                bots.add(NULL);
            }
            int team = modecheck(gamemode, Mode_Team) ? chooseteam() : 0;
            if(!bots[cn])
            {
                bots[cn] = new clientinfo;
            }
            clientinfo *ci = bots[cn];
            ci->clientnum = MAXCLIENTS + cn;
            ci->state.aitype = AI_Bot;
            clientinfo *owner = findaiclient();
            ci->ownernum = owner ? owner->clientnum : -1;
            if(owner)
            {
                owner->bots.add(ci);
            }
            ci->state.skill = skill <= 0 ? randomint(50) + 51 : std::clamp(skill, 1, 101);
            clients.add(ci);
            ci->state.lasttimeplayed = lastmillis;
            copystring(ci->name, "bot", MAXNAMELEN+1);
            ci->state.state = ClientState_Dead;
            ci->team = team;
            ci->playermodel = randomint(128);
            ci->playercolor = randomint(0x8000);
            ci->aireinit = 2;
            ci->connected = true;
            dorefresh = true;
            return true;
        }

        void deleteai(clientinfo *ci)
        {
            int cn = ci->clientnum - MAXCLIENTS;
            if(!bots.inrange(cn))
            {
                return;
            }
            if(ci->ownernum >= 0 && !ci->aireinit && smode)
            {
                smode->leavegame(ci, true);
            }
            sendf(-1, 1, "ri2", NetMsg_ClientDiscon, ci->clientnum);
            clientinfo *owner = (clientinfo *)getclientinfo(ci->ownernum);
            if(owner)
            {
                owner->bots.removeobj(ci);
            }
            clients.removeobj(ci);
            DELETEP(bots[cn]);
            dorefresh = true;
        }

        bool deleteai()
        {
            for(int i = bots.length(); --i >=0;) //note reverse iteration
            {
                if(bots[i] && bots[i]->ownernum >= 0)
                {
                    deleteai(bots[i]);
                    return true;
                }
            }
            return false;
        }

        void shiftai(clientinfo *ci, clientinfo *owner = NULL)
        {
            if(ci->ownernum >= 0 && !ci->aireinit && smode)
            {
                smode->leavegame(ci, true);
            }
            clientinfo *prevowner = (clientinfo *)getclientinfo(ci->ownernum);
            if(prevowner)
            {
                prevowner->bots.removeobj(ci);
            }
            if(!owner)
            {
                ci->aireinit = 0;
                ci->ownernum = -1;
            }
            else if(ci->ownernum != owner->clientnum)
            {
                ci->aireinit = 2;
                ci->ownernum = owner->clientnum;
                owner->bots.add(ci);
            }
            dorefresh = true;
        }

        void removeai(clientinfo *ci)
        { // either schedules a removal, or someone else to assign to

            for(int i = ci->bots.length(); --i >=0;) //note reverse iteration
            {
                shiftai(ci->bots[i], findaiclient(ci));
            }
        }

        bool reassignai()
        {
            clientinfo *hi = NULL, *lo = NULL;
            for(int i = 0; i < clients.length(); i++)
            {
                clientinfo *ci = clients[i];
                if(!validaiclient(ci))
                {
                    continue;
                }
                if(!lo || ci->bots.length() < lo->bots.length())
                {
                    lo = ci;
                }
                if(!hi || ci->bots.length() > hi->bots.length())
                {
                    hi = ci;
                }
            }
            if(hi && lo && hi->bots.length() - lo->bots.length() > 1)
            {
                for(int i = hi->bots.length(); --i >=0;) //note reverse iteration
                {
                    shiftai(hi->bots[i], lo);
                    return true;
                }
            }
            return false;
        }

        void clearai()
        { // clear and remove all ai immediately
            for(int i = bots.length(); --i >=0;) //note reverse iteration
            {
                if(bots[i])
                {
                    deleteai(bots[i]);
                }
            }
        }

        void reqadd(clientinfo *ci, int skill)
        {
            if(!ci->local && !ci->privilege)
            {
                return;
            }
            if(!addai(skill, !ci->local && ci->privilege < Priv_Admin ? botlimit : -1))
            {
                sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, "failed to create or assign bot");
            }
        }

        void reqdel(clientinfo *ci)
        {
            if(!ci->local && !ci->privilege)
            {
                return;
            }
            if(!deleteai())
            {
                sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, "failed to remove any bots");
            }
        }

        void setbotlimit(clientinfo *ci, int limit)
        {
            if(ci && !ci->local && ci->privilege < Priv_Admin)
            {
                return;
            }
            botlimit = std::clamp(limit, 0, maxbots);
            dorefresh = true;
            DEF_FORMAT_STRING(msg, "bot limit is now %d", botlimit);
            sendservmsg(msg);
        }

        void addclient(clientinfo *ci)
        {
            if(ci->state.aitype == AI_None)
            {
                dorefresh = true;
            }
        }

        void changeteam(clientinfo *ci)
        {
            if(ci->state.aitype == AI_None)
            {
                dorefresh = true;
            }
        }
    }
}


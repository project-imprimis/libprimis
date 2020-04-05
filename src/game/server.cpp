#include "game.h"

//server game handling
//includes:
//game moderation (e.g. bans)
//main serverupdate function (called from engine/server.cpp)
//voting
//parsing of packets for game events
//map crc checks
//core demo handling

namespace game
{
    void parseoptions(vector<const char *> &args)
    {
        for(int i = 0; i < args.length(); i++)
#ifndef STANDALONE
            if(!game::clientoption(args[i]))
#endif
            if(!server::serveroption(args[i]))
                conoutf(CON_ERROR, "unknown command-line option: %s", args[i]);
    }

    const char *gameident() { return "Tesseract"; }
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

    struct shotevent : timedevent
    {
        int id, atk;
        vec from, to;
        vector<hitinfo> hits;

        void process(clientinfo *ci);
    };

    struct explodeevent : timedevent
    {
        int id, atk;
        vector<hitinfo> hits;

        bool keepable() const { return true; }

        void process(clientinfo *ci);
    };

    struct suicideevent : gameevent
    {
        void process(clientinfo *ci);
    };

    struct pickupevent : gameevent
    {
        int ent;

        void process(clientinfo *ci);
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

        void scheduleexceeded()
        {
            if(state.state!=ClientState_Alive || !exceeded) return;
            int range = calcpushrange();
            if(!nextexceeded || exceeded + range < nextexceeded) nextexceeded = exceeded + range;
        }

        void setexceeded()
        {
            if(state.state==ClientState_Alive && !exceeded && !checkpushed(gamemillis, calcpushrange())) exceeded = gamemillis;
            scheduleexceeded();
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
        extern void setbotbalance(clientinfo *ci, bool balance);
        extern void changemap();
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
                disconnect_client(c.clientnum, DISC_KICK);
            }
        }
    }

    struct maprotation
    {
        static int exclude;
        int modes;
        string map;

        int calcmodemask() const { return modes&(1<<NUMGAMEMODES) ? modes & ~exclude : modes; }
        bool hasmode(int mode, int offset = STARTGAMEMODE) const { return (calcmodemask() & (1 << (mode-offset))) != 0; }

        int findmode(int mode) const
        {
            if(!hasmode(mode))
            {
                for(int i = 0; i < NUMGAMEMODES; ++i)
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
                if(*h == *n) n++;
                else if(*h != ' ') break;
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
                    modemask |= 1<<NUMGAMEMODES;
                    for(int k = 0; k < NUMGAMEMODES; ++k)
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
                    for(int k = 0; k < NUMGAMEMODES; ++k)
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
                for(int k = 0; k < NUMGAMEMODES; ++k)
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
            for(int k = 0; k < NUMGAMEMODES; ++k)
            {
                if(modemask&(1<<k) && !modecheck(k+STARTGAMEMODE, Mode_Edit))
                {
                    modemask &= ~(1<<k);
                }
            }
        }
        if(!modemask) return false;
        if(!(modemask&(1<<NUMGAMEMODES))) maprotation::exclude |= modemask;
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
    stream *demotmp = NULL, *demorecord = NULL, *demoplayback = NULL;
    int nextplayback = 0, demomillis = 0;

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

    bool serveroption(const char *arg)
    {
        return false;
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
        if(!name) name = ci->name;
        if(name[0] && !duplicatename(ci, name) && ci->state.aitype == AI_None) return name;
        static string cname[3];
        static int cidx = 0;
        cidx = (cidx+1)%3;
        formatstring(cname[cidx], ci->state.aitype == AI_None ? "%s \fs\f5(%d)\fr" : "%s \fs\f5[%d]\fr", name, ci->clientnum);
        return cname[cidx];
    }

    struct servmode
    {
        virtual ~servmode() {}

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

    #define SERVMODE 1
    #include "ctf.h"

    ctfservmode ctfmode;
    servmode *smode = NULL;

    bool canspawnitem(int type) { return VALID_ITEM(type); }

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

    bool delayspawn(int type)
    {
        switch(type)
        {
            default:
                return false;
        }
    }

    bool pickup(int i, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if((!modecheck(gamemode, Mode_Untimed) && gamemillis>=gamelimit) || !sents.inrange(i) || !sents[i].spawned) return false;
        clientinfo *ci = getinfo(sender);
        if(!ci || (!ci->local && !ci->state.canpickup(sents[i].type))) return false;
        sents[i].spawned = false;
        sents[i].spawntime = spawntime(sents[i].type);
        sendf(-1, 1, "ri3", NetMsg_ItemAcceptance, i, sender);
        ci->state.pickup(sents[i].type);
        return true;
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

    void autoteam()
    {
        vector<clientinfo *> team[MAXTEAMS];
        float teamrank[MAXTEAMS] = {0};
        for(int round = 0, remaining = clients.length(); remaining>=0; round++)
        {
            int first = round&1, second = (round+1)&1, selected = 0;
            while(teamrank[first] <= teamrank[second])
            {
                float rank;
                clientinfo *ci = choosebestclient(rank);
                if(!ci)
                {
                    break;
                }
                if(smode && smode->hidefrags())
                {
                    rank = 1;
                }
                else if(selected && rank<=0)
                {
                    break;
                }
                ci->state.timeplayed = -1;
                team[first].add(ci);
                if(rank>0)
                {
                    teamrank[first] += rank;
                }
                selected++;
                if(rank<=0)
                {
                    break;
                }
            }
            if(!selected)
            {
                break;
            }
            remaining -= selected;
        }
        for(int i = 0; i < MAXTEAMS; ++i)
        {
            for(int j = 0; j < team[i].length(); j++)
            {
                clientinfo *ci = team[i][j];
                if(ci->team == 1+i)
                {
                    continue;
                }
                ci->team = 1+i;
                sendf(-1, 1, "riiii", NetMsg_SetTeam, ci->clientnum, ci->team, -1);
            }
        }
    }

    struct teamrank
    {
        float rank;
        int clients;

        teamrank() : rank(0), clients(0) {}
    };

    int chooseworstteam(clientinfo *exclude = NULL)
    {
        teamrank teamranks[MAXTEAMS];
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(ci==exclude || ci->state.aitype!=AI_None || ci->state.state==ClientState_Spectator || !VALID_TEAM(ci->team)) continue;

            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
            ci->state.lasttimeplayed = lastmillis;

            teamrank &ts = teamranks[ci->team-1];
            ts.rank += ci->state.effectiveness/max(ci->state.timeplayed, 1);
            ts.clients++;
        }
        teamrank *worst = &teamranks[0];
        for(int i = 1; i < MAXTEAMS; i++)
        {
            teamrank &ts = teamranks[i];
            if(smode && smode->hidefrags())
            {
                if(ts.clients < worst->clients || (ts.clients == worst->clients && ts.rank < worst->rank)) worst = &ts;
            }
            else if(ts.rank < worst->rank || (ts.rank == worst->rank && ts.clients < worst->clients)) worst = &ts;
        }
        return 1+int(worst-teamranks);
    }

    void prunedemos(int extra = 0)
    {
        int n = clamp(demos.length() + extra - maxdemos, 0, demos.length());
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
        int len = (int)min(demotmp->size(), stream::offset((maxdemosize<<20) + 0x10000));
        demofile &d = demos.add();
        time_t t = time(NULL);
        char *timestr = ctime(&t), *trim = timestr + strlen(timestr);
        while(trim>timestr && iscubespace(*--trim)) *trim = '\0';
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
        LIL_ENDIAN_SWAP(stamp, 3);
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

    void setupdemorecord()
    {
        if(modecheck(gamemode, Mode_LocalOnly) || modecheck(gamemode, Mode_Edit)) return;

        demotmp = opentempfile("demorecord", "w+b");
        if(!demotmp) return;

        stream *f = opengzfile(NULL, "wb", demotmp);
        if(!f) { DELETEP(demotmp); return; }

        sendservmsg("recording demo");

        demorecord = f;

        demoheader hdr;
        memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
        hdr.version = DEMO_VERSION;
        hdr.protocol = PROTOCOL_VERSION;
        LIL_ENDIAN_SWAP(&hdr.version, 2);
        demorecord->write(&hdr, sizeof(demoheader));

        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        welcomepacket(p, NULL);
        writedemo(1, p.buf, p.len);
    }

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

    static void freegetmap(ENetPacket *packet)
    {
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(ci->getmap == packet) ci->getmap = NULL;
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

    void setupdemoplayback()
    {
        if(demoplayback) return;
        demoheader hdr;
        string msg;
        msg[0] = '\0';
        DEF_FORMAT_STRING(file, "%s.dmo", smapname);
        demoplayback = opengzfile(file, "rb");
        if(!demoplayback) formatstring(msg, "could not read demo \"%s\"", file);
        else if(demoplayback->read(&hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic)))
            formatstring(msg, "\"%s\" is not a demo file", file);
        else
        {
            LIL_ENDIAN_SWAP(&hdr.version, 2);
            if(hdr.version!=DEMO_VERSION) formatstring(msg, "demo \"%s\" requires an %s version of Tesseract", file, hdr.version<DEMO_VERSION ? "older" : "newer");
            else if(hdr.protocol!=PROTOCOL_VERSION) formatstring(msg, "demo \"%s\" requires an %s version of Tesseract", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
        }
        if(msg[0])
        {
            DELETEP(demoplayback);
            sendservmsg(msg);
            return;
        }

        sendservmsgf("playing demo \"%s\"", file);

        demomillis = 0;
        sendf(-1, 1, "ri3", NetMsg_DemoPlayback, 1, -1);

        if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
        {
            enddemoplayback();
            return;
        }
        LIL_ENDIAN_SWAP(&nextplayback, 1);
    }

    void readdemo()
    {
        if(!demoplayback) return;
        demomillis += curtime;
        while(demomillis>=nextplayback)
        {
            int chan, len;
            if(demoplayback->read(&chan, sizeof(chan))!=sizeof(chan) ||
               demoplayback->read(&len, sizeof(len))!=sizeof(len))
            {
                enddemoplayback();
                return;
            }
            LIL_ENDIAN_SWAP(&chan, 1);
            LIL_ENDIAN_SWAP(&len, 1);
            ENetPacket *packet = enet_packet_create(NULL, len+1, 0);
            if(!packet || demoplayback->read(packet->data+1, len)!=size_t(len))
            {
                if(packet) enet_packet_destroy(packet);
                enddemoplayback();
                return;
            }
            packet->data[0] = NetMsg_DemoPacket;
            sendpacket(-1, chan, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            if(!demoplayback) break;
            if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
            {
                enddemoplayback();
                return;
            }
            LIL_ENDIAN_SWAP(&nextplayback, 1);
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
        val = clamp(val, 10, 1000);
        if(gamespeed==val) return;
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
        if(ci->state.state==ClientState_Spectator && !ci->local) aiman::removeai(ci);
    }

    extern void connected(clientinfo *ci);

    bool setmaster(clientinfo *ci, bool val, const char *pass = "", const char *authname = NULL, const char *authdesc = NULL, int authpriv = Priv_Master, bool force = false, bool trial = false)
    {
        if(authname && !val) return false;
        const char *name = "";
        if(val)
        {
            bool haspass = adminpass[0] && checkpassword(ci, adminpass, pass);
            int wantpriv = ci->local || haspass ? Priv_Admin : authpriv;
            if(wantpriv <= ci->privilege) return true;
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
        else formatstring(msg, "%s %s %s", colorname(ci), val ? "claimed" : "relinquished", name);
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

    bool trykick(clientinfo *ci, int victim, const char *reason = NULL, const char *authname = NULL, const char *authdesc = NULL, int authpriv = Priv_None, bool trial = false)
    {
        int priv = ci->privilege;
        if(authname)
        {
            if(priv >= authpriv || ci->local) authname = authdesc = NULL;
            else priv = authpriv;
        }
        if((priv || ci->local) && ci->clientnum!=victim)
        {
            clientinfo *vinfo = (clientinfo *)getclientinfo(victim);
            if(vinfo && vinfo->connected && (priv >= vinfo->privilege || ci->local) && vinfo->privilege < Priv_Admin && !vinfo->local)
            {
                if(trial) return true;
                string kicker;
                if(authname)
                {
                    if(authdesc && authdesc[0]) formatstring(kicker, "%s as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci), authname, authdesc);
                    else formatstring(kicker, "%s as '\fs\f5%s\fr'", colorname(ci), authname);
                }
                else copystring(kicker, colorname(ci));
                if(reason && reason[0]) sendservmsgf("%s kicked %s because: %s", kicker, colorname(vinfo), reason);
                else sendservmsgf("%s kicked %s", kicker, colorname(vinfo));
                uint ip = getclientip(victim);
                addban(ip, 4*60*60000);
                kickclients(ip, ci, priv);
            }
        }
        return false;
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
            if(sc.ip == ip && !strcmp(sc.name, ci->name)) return &sc;
        }
        if(!insert) return 0;
        savedscore &sc = scores.add();
        sc.ip = ip;
        copystring(sc.name, ci->name);
        return &sc;
    }

    void savescore(clientinfo *ci)
    {
        savedscore *sc = findscore(ci, true);
        if(sc) sc->save(ci->state);
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
                if(msg < 0) val = uchar(-msg);
                else msgmask[msg] = val;
            }
            va_end(msgs);
        }

        uchar operator[](int msg) const { return msg >= 0 && msg < NetMsg_NumMsgs ? msgmask[msg] : 0; }
    } msgfilter(-1, NetMsg_Connect, NetMsg_ServerInfo, NetMsg_InitClient, NetMsg_Welcome, NetMsg_MapChange, NetMsg_ServerMsg, NetMsg_Damage, NetMsg_Hitpush, NetMsg_ShotFX, NetMsg_ExplodeFX, NetMsg_Died, NetMsg_SpawnState, NetMsg_ForceDeath, NetMsg_TeamInfo, NetMsg_ItemAcceptance, NetMsg_ItemSpawn, NetMsg_TimeUp, NetMsg_ClientDiscon, NetMsg_CurrentMaster, NetMsg_Pong, NetMsg_Resume, NetMsg_SendDemoList, NetMsg_SendDemo, NetMsg_DemoPlayback, NetMsg_SendMap, NetMsg_DropFlag, NetMsg_ScoreFlag, NetMsg_ReturnFlag, NetMsg_ResetFlag, NetMsg_Client, NetMsg_AuthChallenge, NetMsg_InitAI, NetMsg_DemoPacket, -2, NetMsg_CalcLight, NetMsg_Remip, NetMsg_Newmap, NetMsg_GetMap, NetMsg_SendMap, NetMsg_Clipboard, -3, NetMsg_EditEnt, NetMsg_EditFace, NetMsg_EditTex, NetMsg_EditMat, NetMsg_EditFlip, NetMsg_Copy, NetMsg_Paste, NetMsg_Rotate, NetMsg_Replace, NetMsg_DelCube, NetMsg_EditVar, NetMsg_EditVSlot, NetMsg_Undo, NetMsg_Redo, -4, NetMsg_Pos, NetMsg_NumMsgs),
      connectfilter(-1, NetMsg_Connect, -2, NetMsg_AuthAnswer, -3, NetMsg_Ping, NetMsg_NumMsgs);

    int checktype(int type, clientinfo *ci)
    {
        if(ci)
        {
            if(!ci->connected) switch(connectfilter[type])
            {
                // allow only before authconnect
                case 1: return !ci->connectauth ? type : -1;
                // allow only during authconnect
                case 2: return ci->connectauth ? type : -1;
                // always allow
                case 3: return type;
                // never allow
                default: return -1;
            }
            if(ci->local) return type;
        }
        switch(msgfilter[type])
        {
            // server-only messages
            case 1: return ci ? -1 : type;
            // only allowed in coop-edit
            case 2: if(modecheck(gamemode, Mode_Edit)) break; return -1;
            // only allowed in coop-edit, no overflow check
            case 3: return modecheck(gamemode, Mode_Edit) ? type : -1;
            // no overflow check
            case 4: return type;
        }
        if(ci && ++ci->overflow >= 200) return -2;
        return type;
    }

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
            if(!ws.contains(packet->data)) continue;
            ws.uses--;
            if(ws.uses <= 0)
            {
                ws.cleanup();
                worldstates.removeunordered(i);
            }
            break;
        }
    }

    void flushclientposition(clientinfo &ci)
    {
        if(ci.position.empty() || (!hasnonlocalclients() && !demorecord)) return;
        packetbuf p(ci.position.length(), 0);
        p.put(ci.position.getbuf(), ci.position.length());
        ci.position.setsize(0);
        sendpacket(-1, 0, p.finalize(), ci.ownernum);
    }

    static void sendpositions(worldstate &ws, ucharbuf &wsbuf)
    {
        if(wsbuf.empty()) return;
        int wslen = wsbuf.length();
        recordpacket(0, wsbuf.buf, wslen);
        wsbuf.put(wsbuf.buf, wslen);
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_None) continue;
            uchar *data = wsbuf.buf;
            int size = wslen;
            if(ci.wsdata >= wsbuf.buf) { data = ci.wsdata + ci.wslen; size -= ci.wslen; }
            if(size <= 0) continue;
            ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(ci.clientnum, 0, packet);
            if(packet->referenceCount) { ws.uses++; packet->freeCallback = cleanworldstate; }
            else enet_packet_destroy(packet);
        }
        wsbuf.offset(wsbuf.length());
    }

    static inline void addposition(worldstate &ws, ucharbuf &wsbuf, int mtu, clientinfo &bi, clientinfo &ci)
    {
        if(bi.position.empty()) return;
        if(wsbuf.length() + bi.position.length() > mtu) sendpositions(ws, wsbuf);
        int offset = wsbuf.length();
        wsbuf.put(bi.position.getbuf(), bi.position.length());
        bi.position.setsize(0);
        int len = wsbuf.length() - offset;
        if(ci.wsdata < wsbuf.buf) { ci.wsdata = &wsbuf.buf[offset]; ci.wslen = len; }
        else ci.wslen += len;
    }

    static void sendmessages(worldstate &ws, ucharbuf &wsbuf)
    {
        if(wsbuf.empty()) return;
        int wslen = wsbuf.length();
        recordpacket(1, wsbuf.buf, wslen);
        wsbuf.put(wsbuf.buf, wslen);
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_None) continue;
            uchar *data = wsbuf.buf;
            int size = wslen;
            if(ci.wsdata >= wsbuf.buf) { data = ci.wsdata + ci.wslen; size -= ci.wslen; }
            if(size <= 0) continue;
            ENetPacket *packet = enet_packet_create(data, size, (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(ci.clientnum, 1, packet);
            if(packet->referenceCount) { ws.uses++; packet->freeCallback = cleanworldstate; }
            else enet_packet_destroy(packet);
        }
        wsbuf.offset(wsbuf.length());
    }

    static inline void addmessages(worldstate &ws, ucharbuf &wsbuf, int mtu, clientinfo &bi, clientinfo &ci)
    {
        if(bi.messages.empty()) return;
        if(wsbuf.length() + 10 + bi.messages.length() > mtu) sendmessages(ws, wsbuf);
        int offset = wsbuf.length();
        putint(wsbuf, NetMsg_Client);
        putint(wsbuf, bi.clientnum);
        putuint(wsbuf, bi.messages.length());
        wsbuf.put(bi.messages.getbuf(), bi.messages.length());
        bi.messages.setsize(0);
        int len = wsbuf.length() - offset;
        if(ci.wsdata < wsbuf.buf) { ci.wsdata = &wsbuf.buf[offset]; ci.wslen = len; }
        else ci.wslen += len;
    }

    bool buildworldstate()
    {
        int wsmax = 0;
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo &ci = *clients[i];
            ci.overflow = 0;
            ci.wsdata = NULL;
            wsmax += ci.position.length();
            if(ci.messages.length()) wsmax += 10 + ci.messages.length();
        }
        if(wsmax <= 0)
        {
            reliablemessages = false;
            return false;
        }
        worldstate &ws = worldstates.add();
        ws.setup(2*wsmax);
        int mtu = getservermtu() - 100;
        if(mtu <= 0) mtu = ws.len;
        ucharbuf wsbuf(ws.data, ws.len);
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_None) continue;
            addposition(ws, wsbuf, mtu, ci, ci);
            for(int j = 0; j < ci.bots.length(); j++)
            {
                addposition(ws, wsbuf, mtu, *ci.bots[j], ci);
            }
        }
        sendpositions(ws, wsbuf);
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_None)
            {
                continue;
            }
            addmessages(ws, wsbuf, mtu, ci, ci);
            for(int j = 0; j < ci.bots.length(); j++)
            {
                addmessages(ws, wsbuf, mtu, *ci.bots[j], ci);
            }
        }
        sendmessages(ws, wsbuf);
        reliablemessages = false;
        if(ws.uses) return true;
        ws.cleanup();
        worldstates.drop();
        return false;
    }

    bool sendpackets(bool force)
    {
        if(clients.empty() || (!hasnonlocalclients() && !demorecord)) return false;
        enet_uint32 curtime = enet_time_get()-lastsend;
        if(curtime<7 && !force) return false;
        bool flush = buildworldstate();
        lastsend += curtime - (curtime%7); //delay of 7ms between packet reciepts (143fps)
        return flush;
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

    void sendspawn(clientinfo *ci)
    {
        servstate &gs = ci->state;
        spawnstate(ci);
        sendf(ci->ownernum, 1, "rii5v", NetMsg_SpawnState, ci->clientnum, gs.lifesequence,
            gs.health, gs.maxhealth,
            gs.gunselect, Gun_NumGuns, gs.ammo);
        gs.lastspawn = gamemillis;
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

    bool restorescore(clientinfo *ci)
    {
        //if(ci->local) return false;
        savedscore *sc = findscore(ci, false);
        if(sc)
        {
            sc->restore(ci->state);
            return true;
        }
        return false;
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

    void loaditems()
    {
        resetitems();
        notgotitems = true;
        if(modecheck(gamemode, Mode_Edit) || !loadents(smapname, ments, &mcrc))
            return;
        for(int i = 0; i < ments.length(); i++)
        {
            if(canspawnitem(ments[i].type))
            {
                server_entity se = { GamecodeEnt_NotUsed, 0, false };
                while(sents.length()<=i) sents.add(se);
                sents[i].type = ments[i].type;
                if(!modecheck(gamemode, Mode_LocalOnly) && delayspawn(sents[i].type)) sents[i].spawntime = spawntime(sents[i].type);
                else sents[i].spawned = true;
            }
        }
        notgotitems = false;
    }

    void changemap(const char *s, int mode)
    {
        stopdemo();
        pausegame(false);
        changegamespeed(100);
        if(smode) smode->cleanup();
        aiman::clearai();

        gamemode = mode;
        gamemillis = 0;
        gamelimit = (modecheck(gamemode, Mode_AllowOvertime) ? 15 : 10)*60000; //15 minute max in OT
        interm = 0;
        nextexceeded = 0;
        copystring(smapname, s);
        loaditems();
        scores.shrink(0);
        shouldcheckteamkills = false;
        teamkills.shrink(0);
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
        }

        if(modecheck(gamemode, Mode_LocalOnly)) kicknonlocalclients(DISC_LOCAL);

        sendf(-1, 1, "risii", NetMsg_MapChange, smapname, gamemode, 1);

        clearteaminfo();
        if(modecheck(gamemode, Mode_Team)) autoteam();

        if(modecheck(gamemode, Mode_CTF)) smode = &ctfmode;
        else smode = NULL;

        if(!modecheck(gamemode, Mode_Untimed) && smapname[0]) sendf(-1, 1, "ri2", NetMsg_TimeUp, gamemillis < gamelimit && !interm ? max((gamelimit - gamemillis)/1000, 1) : 0);
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            ci->mapchange();
            ci->state.lasttimeplayed = lastmillis;
            if(!modecheck(gamemode, Mode_LocalOnly) && ci->state.state!=ClientState_Spectator) sendspawn(ci);
        }

        aiman::changemap();

        if(modecheck(gamemode, Mode_Demo))
        {
            if(clients.length()) setupdemoplayback();
        }
        else if(demonextmatch)
        {
            demonextmatch = false;
            setupdemorecord();
        }

        if(smode) smode->setup();
    }

    void rotatemap(bool next)
    {
        if(!maprotations.inrange(curmaprotation))
        {
            changemap("", 0);
            return;
        }
        if(next)
        {
            curmaprotation = findmaprotation(gamemode, smapname);
            if(curmaprotation >= 0) nextmaprotation();
            else curmaprotation = smapname[0] ? max(findmaprotation(gamemode, ""), 0) : 0;
        }
        maprotation &rot = maprotations[curmaprotation];
        changemap(rot.map, rot.findmode(gamemode));
    }

    struct votecount
    {
        char *map;
        int mode, count;
        votecount() {}
        votecount(char *s, int n) : map(s), mode(n), count(0) {}
    };

    void checkvotes(bool force = false)
    {
        vector<votecount> votes;
        int maxvotes = 0;
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *oi = clients[i];
            if(oi->state.state==ClientState_Spectator && !oi->privilege && !oi->local) continue;
            if(oi->state.aitype!=AI_None) continue;
            maxvotes++;
            if(!MODE_VALID(oi->modevote)) continue;
            votecount *vc = NULL;
            for(int j = 0; j < votes.length(); j++)
            {
                if(!strcmp(oi->mapvote, votes[j].map) && oi->modevote==votes[j].mode)
                {
                    vc = &votes[j];
                    break;
                }
            }
            if(!vc)
            {
                vc = &votes.add(votecount(oi->mapvote, oi->modevote));
            }
            vc->count++;
        }
        votecount *best = NULL;
        for(int i = 0; i < votes.length(); i++)
        {
            if(!best || votes[i].count > best->count || (votes[i].count == best->count && RANDOM_INT(2)))
            {
                best = &votes[i];
            }
        }
        if(force || (best && best->count > maxvotes/2))
        {
            if(demorecord) enddemorecord();
            if(best && (best->count > (force ? 1 : maxvotes/2)))
            {
                sendservmsg(force ? "vote passed by default" : "vote passed by majority");
                changemap(best->map, best->mode);
            }
            else rotatemap(true);
        }
    }

    void forcemap(const char *map, int mode)
    {
        stopdemo();
        if(!map[0] && !modecheck(mode, Mode_Edit))
        {
            int idx = findmaprotation(mode, smapname);
            if(idx < 0 && smapname[0]) idx = findmaprotation(mode, "");
            if(idx < 0) return;
            map = maprotations[idx].map;
        }
        if(hasnonlocalclients()) sendservmsgf("local player forced %s on map %s", modeprettyname(mode), map[0] ? map : "[new map]");
        changemap(map, mode);
    }

    void vote(const char *map, int reqmode, int sender)
    {
        clientinfo *ci = getinfo(sender);
        if(!ci || (ci->state.state==ClientState_Spectator && !ci->privilege && !ci->local) || (!ci->local && modecheck(reqmode, Mode_LocalOnly))) return;
        if(!MODE_VALID(reqmode)) return;
        if(!map[0] && !modecheck(reqmode, Mode_Edit))
        {
            int idx = findmaprotation(reqmode, smapname);
            if(idx < 0 && smapname[0]) idx = findmaprotation(reqmode, "");
            if(idx < 0) return;
            map = maprotations[idx].map;
        }
        if(lockmaprotation && !ci->local && ci->privilege < (lockmaprotation > 1 ? Priv_Admin : Priv_Master) && findmaprotation(reqmode, map) < 0)
        {
            sendf(sender, 1, "ris", NetMsg_ServerMsg, "This server has locked the map rotation.");
            return;
        }
        copystring(ci->mapvote, map);
        ci->modevote = reqmode;
        if(ci->local || (ci->privilege && mastermode>=MasterMode_Veto))
        {
            if(demorecord) enddemorecord();
            if(!ci->local || hasnonlocalclients())
                sendservmsgf("%s forced %s on map %s", colorname(ci), modeprettyname(ci->modevote), ci->mapvote[0] ? ci->mapvote : "[new map]");
            changemap(ci->mapvote, ci->modevote);
        }
        else
        {
            sendservmsgf("%s suggests %s on map %s (select map to vote)", colorname(ci), modeprettyname(reqmode), map[0] ? map : "[new map]");
            checkvotes();
        }
    }

    void checkintermission()
    {
        if(gamemillis >= gamelimit && !interm)
        {
            sendf(-1, 1, "ri2", NetMsg_TimeUp, 0);
            if(smode) smode->intermission();
            changegamespeed(100);
            interm = gamemillis + 10000;
        }
    }

    void startintermission() { gamelimit = min(gamelimit, gamemillis); checkintermission(); }

    void dodamage(clientinfo *target, clientinfo *actor, int damage, int atk, const vec &hitpush = vec(0, 0, 0))
    {
        servstate &ts = target->state;
        ts.dodamage(damage);
        if(target!=actor && !modecheck(gamemode, Mode_Team) && target->team != actor->team) actor->state.damage += damage;
        sendf(-1, 1, "ri5", NetMsg_Damage, target->clientnum, actor->clientnum, damage, ts.health);
        if(target==actor) target->setpushed();
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
                actor->state.effectiveness += fragvalue*friends/float(max(enemies, 1));
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

    void suicideevent::process(clientinfo *ci)
    {
        suicide(ci);
    }

    void explodeevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        switch(atk)
        {
            case Attack_PulseShoot:
                if(!gs.projs.remove(id)) return;
                break;

            default:
                return;
        }
        sendf(-1, 1, "ri4x", NetMsg_ExplodeFX, ci->clientnum, atk, id, ci->ownernum);
        for(int i = 0; i < hits.length(); i++)
        {
            hitinfo &h = hits[i];
            clientinfo *target = getinfo(h.target);
            if(!target || target->state.state!=ClientState_Alive || h.lifesequence!=target->state.lifesequence || h.dist<0 || h.dist>attacks[atk].exprad)
            {
                continue;
            }

            bool dup = false;
            for(int j = 0; j < i; ++j)
            {
                if(hits[j].target==h.target)
                {
                    dup = true;
                    break;
                }
            }
            if(dup)
            {
                continue;
            }

            float damage = attacks[atk].damage*(1-h.dist/EXP_DISTSCALE/attacks[atk].exprad);
            if(target==ci) damage /= EXP_SELFDAMDIV;
            if(damage > 0) dodamage(target, ci, max(int(damage), 1), atk, h.dir);
        }
    }

    void shotevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        int wait = millis - gs.lastshot;
        if(!gs.isalive(gamemillis) ||
           wait<gs.gunwait ||
           !VALID_ATTACK(atk))
            return;
        int gun = attacks[atk].gun;
        if(gs.ammo[gun]<=0 || (attacks[atk].range && from.dist(to) > attacks[atk].range + 1))
            return;
        gs.ammo[gun] -= attacks[atk].use;
        gs.lastshot = millis;
        gs.gunwait = attacks[atk].attackdelay;
        sendf(-1, 1, "rii9x", NetMsg_ShotFX, ci->clientnum, atk, id,
                int(from.x*DMF), int(from.y*DMF), int(from.z*DMF),
                int(to.x*DMF), int(to.y*DMF), int(to.z*DMF),
                ci->ownernum);
        gs.shotdamage += attacks[atk].damage*attacks[atk].rays;
        switch(atk)
        {
            case Attack_PulseShoot: gs.projs.add(id); break;
            default:
            {
                int totalrays = 0, maxrays = attacks[atk].rays;
                for(int i = 0; i < hits.length(); i++)
                {
                    hitinfo &h = hits[i];
                    clientinfo *target = getinfo(h.target);
                    if(!target || target->state.state!=ClientState_Alive || h.lifesequence!=target->state.lifesequence || h.rays<1 || h.dist > attacks[atk].range + 1) continue;

                    totalrays += h.rays;
                    if(totalrays>maxrays) continue;
                    int damage = h.rays*attacks[atk].damage;
                    dodamage(target, ci, damage, atk, h.dir);
                }
                break;
            }
        }
    }

    void pickupevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(!modecheck(gamemode, Mode_LocalOnly) && !gs.isalive(gamemillis)) return;
        pickup(ent, ci->clientnum);
    }

    bool gameevent::flush(clientinfo *ci, int fmillis)
    {
        process(ci);
        return true;
    }

    bool timedevent::flush(clientinfo *ci, int fmillis)
    {
        if(millis > fmillis) return false;
        else if(millis >= ci->lastevent)
        {
            ci->lastevent = millis;
            process(ci);
        }
        return true;
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

    void serverupdate() //called from engine/server.src
    {

        ////////// This section only is run if people are online //////////

        if(shouldstep && !gamepaused) //if people are online and game is unpaused
        {
            gamemillis += curtime; //advance clock if applicable

            if(modecheck(gamemode, Mode_Demo)) readdemo();
            else if(!modecheck(gamemode, Mode_Untimed) || gamemillis < gamelimit)
            {
                processevents(); //foreach client flushevents (handle events & clear?)
                if(curtime)
                {
                    for(int i = 0; i < sents.length(); i++)
                    {
                        if(sents[i].spawntime) // spawn entities when timer reached
                        {
                            sents[i].spawntime -= curtime;
                            if(sents[i].spawntime<=0)
                            {
                                sents[i].spawntime = 0;
                                sents[i].spawned = true;
                                sendf(-1, 1, "ri2", NetMsg_ItemSpawn, i);
                            }
                        }
                    }
                }
                aiman::checkai();
                if(smode) smode->update();
            }
        }

        ////////// This section is run regardless of whether there are people are online //////////
        //         (though the loop over `connects` will always be empty with nobody on)

        while(bannedips.length() && bannedips[0].expire-totalmillis <= 0) bannedips.remove(0); //clear expired ip bans if there are any
        for(int i = 0; i < connects.length(); i++)
        {
            if(totalmillis-connects[i]->connectmillis>15000)
            {
                disconnect_client(connects[i]->clientnum, DISC_TIMEOUT); //remove clients who haven't responded in 15s
            }
        }

        if(nextexceeded && gamemillis > nextexceeded && (modecheck(gamemode, Mode_Untimed) || gamemillis < gamelimit))
        {
            nextexceeded = 0;
            for(int i = clients.length(); --i >=0;) //note reverse iteration
            {
                clientinfo &c = *clients[i];
                if(c.state.aitype != AI_None)
                {
                    continue;
                }
                if(c.checkexceeded())
                {
                    disconnect_client(c.clientnum, DISC_MSGERR);
                }
                else
                {
                    c.scheduleexceeded();
                }
            }
        }

        if(shouldcheckteamkills) checkteamkills(); //check team kills on matches that care

        ////////// This section is only run if there are people online //////////

        if(shouldstep && !gamepaused) //while unpaused & players ingame, check if match should be over
        {
            if(!modecheck(gamemode, Mode_Untimed) && smapname[0] && gamemillis-curtime>0) checkintermission();
            if(interm > 0 && gamemillis>interm)
            {
                if(demorecord) enddemorecord(); //close demo if one is being recorded
                interm = -1;
                checkvotes(true);
            }
        }

        //check if there are people online for next iteration of loop
        shouldstep = clients.length() > 0; //don't step if there's nobody online
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

    void checkmaps(int req = -1)
    {
        if(modecheck(gamemode, Mode_Edit) || !smapname[0])
        {
            return;
        }
        vector<crcinfo> crcs;
        int total = 0, unsent = 0, invalid = 0;
        if(mcrc)
        {
            crcs.add(crcinfo(mcrc, clients.length() + 1));
        }
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==ClientState_Spectator || ci->state.aitype != AI_None) continue;
            total++;
            if(!ci->clientmap[0])
            {
                if(ci->mapcrc < 0) invalid++;
                else if(!ci->mapcrc) unsent++;
            }
            else
            {
                crcinfo *match = NULL;
                for(int j = 0; j < crcs.length(); j++)
                {
                    if(crcs[j].crc == ci->mapcrc)
                    {
                        match = &crcs[j];
                        break;
                    }
                }
                if(!match)
                {
                    crcs.add(crcinfo(ci->mapcrc, 1));
                }
                else
                {
                    match->matches++;
                }
            }
        }
        if(!mcrc && total - unsent < min(total, 4))
        {
            return;
        }
        crcs.sort(crcinfo::compare);
        string msg;
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==ClientState_Spectator || ci->state.aitype != AI_None || ci->clientmap[0] || ci->mapcrc >= 0 || (req < 0 && ci->warned)) continue;
            formatstring(msg, "%s has modified map \"%s\"", colorname(ci), smapname);
            sendf(req, 1, "ris", NetMsg_ServerMsg, msg);
            if(req < 0) ci->warned = true;
        }
        if(crcs.length() >= 2)
        {
            for(int i = 0; i < crcs.length(); i++)
            {
                crcinfo &info = crcs[i];
                if(i || info.matches <= crcs[i+1].matches)
                {
                    for(int j = 0; j < clients.length(); j++)
                    {
                        clientinfo *ci = clients[j];
                        if(ci->state.state==ClientState_Spectator || ci->state.aitype != AI_None || !ci->clientmap[0] || ci->mapcrc != info.crc || (req < 0 && ci->warned))
                        {
                            continue;
                        }
                        formatstring(msg, "%s has modified map \"%s\"", colorname(ci), smapname);
                        sendf(req, 1, "ris", NetMsg_ServerMsg, msg);
                        if(req < 0)
                        {
                            ci->warned = true;
                        }
                    }
                }
            }
        }
        if(req < 0 && modifiedmapspectator && (mcrc || modifiedmapspectator > 1))
        {
            for(int i = 0; i < clients.length(); i++)
            {
                clientinfo *ci = clients[i];
                if(!ci->local && ci->warned && ci->state.state != ClientState_Spectator)
                {
                    forcespectator(ci);
                }
            }
        }
    }

    bool shouldspectate(clientinfo *ci)
    {
        return !ci->local && ci->warned && modifiedmapspectator && (mcrc || modifiedmapspectator > 1);
    }

    void unspectate(clientinfo *ci)
    {
        if(shouldspectate(ci)) return;
        ci->state.state = ClientState_Dead;
        ci->state.respawn();
        ci->state.lasttimeplayed = lastmillis;
        aiman::addclient(ci);
        sendf(-1, 1, "ri3", NetMsg_Spectator, ci->clientnum, 0);
        if(ci->clientmap[0] || ci->mapcrc) checkmaps();
        if(!hasmap(ci)) rotatemap(true);
    }

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
        ci->sessionid = (RANDOM_INT(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;
        ci->local = true;

        connects.add(ci);
        sendservinfo(ci);
    }

    void localdisconnect(int n)
    {
        if(modecheck(gamemode, Mode_Demo)) enddemoplayback();
        clientdisconnect(n);
    }

    int clientconnect(int n, uint ip)
    {
        clientinfo *ci = getinfo(n);
        ci->clientnum = ci->ownernum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (RANDOM_INT(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;

        connects.add(ci);
        if(modecheck(gamemode, Mode_LocalOnly)) return DISC_LOCAL;
        sendservinfo(ci);
        return DISC_NONE;
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
            if(checkbans(getclientip(ci->clientnum))) disconnect_client(ci->clientnum, DISC_IPBAN);
        }
    }

    ICOMMAND(clearipbans, "", (), ipbans.clear());
    ICOMMAND(ipban, "s", (const char *ipname), ipbans.add(ipname));

    int allowconnect(clientinfo *ci, const char *pwd = "")
    {
        if(ci->local) return DISC_NONE;
        if(modecheck(gamemode, Mode_LocalOnly)) return DISC_LOCAL;
        if(serverpass[0])
        {
            if(!checkpassword(ci, serverpass, pwd)) return DISC_PASSWORD;
            return DISC_NONE;
        }
        if(adminpass[0] && checkpassword(ci, adminpass, pwd)) return DISC_NONE;
        if(numclients(-1, false, true)>=maxclients) return DISC_MAXCLIENTS;
        uint ip = getclientip(ci->clientnum);
        if(checkbans(ip)) return DISC_IPBAN;
        if(mastermode>=MasterMode_Private && allowedips.find(ip)<0) return DISC_PRIVATE;
        return DISC_NONE;
    }

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

    void authfailed(uint id)
    {
        authfailed(findauth(id));
    }

    void authsucceeded(uint id)
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        ci->cleanauth(ci->connectauth!=0);
        if(ci->connectauth) connected(ci);
        if(ci->authkickvictim >= 0)
        {
            if(setmaster(ci, true, "", ci->authname, NULL, Priv_Auth, false, true))
                trykick(ci, ci->authkickvictim, ci->authkickreason, ci->authname, NULL, Priv_Auth);
            ci->cleanauthkick();
        }
        else setmaster(ci, true, "", ci->authname, NULL, Priv_Auth);
    }

    void authchallenged(uint id, const char *val, const char *desc = "")
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        sendf(ci->clientnum, 1, "risis", NetMsg_AuthChallenge, desc, id, val);
    }

    uint nextauthreq = 0;

    bool tryauth(clientinfo *ci, const char *user, const char *desc)
    {
        ci->cleanauth();
        if(!nextauthreq) nextauthreq = 1;
        ci->authreq = nextauthreq++;
        filtertext(ci->authname, user, false, false, 100);
        copystring(ci->authdesc, desc);
        if(ci->authdesc[0])
        {
            userinfo *u = users.access(userkey(ci->authname, ci->authdesc));
            if(u)
            {
                uint seed[3] = { ::hthash(serverauth) + DET_RND(size_t(ci) + size_t(user) + size_t(desc), 0x10000), uint(totalmillis), randomMT() };
                vector<char> buf;
                ci->authchallenge = genchallenge(u->pubkey, seed, sizeof(seed), buf);
                sendf(ci->clientnum, 1, "risis", NetMsg_AuthChallenge, desc, ci->authreq, buf.getbuf());
            }
            else ci->cleanauth();
        }
        else if(!requestmasterf("reqauth %u %s\n", ci->authreq, ci->authname))
        {
            ci->cleanauth();
            sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, "not connected to authentication server");
        }
        if(ci->authreq) return true;
        if(ci->connectauth) disconnect_client(ci->clientnum, ci->connectauth);
        return false;
    }

    bool answerchallenge(clientinfo *ci, uint id, char *val, const char *desc)
    {
        if(ci->authreq != id || strcmp(ci->authdesc, desc))
        {
            ci->cleanauth();
            return !ci->connectauth;
        }
        for(char *s = val; *s; s++)
        {
            if(!isxdigit(*s)) { *s = '\0'; break; }
        }
        if(desc[0])
        {
            if(ci->authchallenge && checkchallenge(val, ci->authchallenge))
            {
                userinfo *u = users.access(userkey(ci->authname, ci->authdesc));
                if(u)
                {
                    if(ci->connectauth) connected(ci);
                    if(ci->authkickvictim >= 0)
                    {
                        if(setmaster(ci, true, "", ci->authname, ci->authdesc, u->privilege, false, true))
                            trykick(ci, ci->authkickvictim, ci->authkickreason, ci->authname, ci->authdesc, u->privilege);
                    }
                    else setmaster(ci, true, "", ci->authname, ci->authdesc, u->privilege);
                }
            }
            ci->cleanauth();
        }
        else if(!requestmasterf("confauth %u %s\n", id, val))
        {
            ci->cleanauth();
            sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, "not connected to authentication server");
        }
        return ci->authreq || !ci->connectauth;
    }

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

    void processmasterinput(const char *cmd, int cmdlen, const char *args)
    {
        uint id;
        string val;
        if(sscanf(cmd, "failauth %u", &id) == 1)
            authfailed(id);
        else if(sscanf(cmd, "succauth %u", &id) == 1)
            authsucceeded(id);
        else if(sscanf(cmd, "chalauth %u %255s", &id, val) == 2)
            authchallenged(id, val);
        else if(matchstring(cmd, cmdlen, "cleargbans"))
            gbans.clear();
        else if(sscanf(cmd, "addgban %100s", val) == 1)
            gbans.add(val);
    }

    void receivefile(int sender, uchar *data, int len)
    {
        if(!modecheck(gamemode, Mode_Edit) || len <= 0 || len > 4*1024*1024) return;
        clientinfo *ci = getinfo(sender);
        if(ci->state.state==ClientState_Spectator && !ci->privilege && !ci->local) return;
        if(mapdata) DELETEP(mapdata);
        mapdata = opentempfile("mapdata", "w+b");
        if(!mapdata) { sendf(sender, 1, "ris", NetMsg_ServerMsg, "failed to open temporary file for map"); return; }
        mapdata->write(data, len);
        sendservmsgf("[%s sent a map to server, \"/getmap\" to receive it]", colorname(ci));
    }

    void sendclipboard(clientinfo *ci)
    {
        if(!ci->lastclipboard || !ci->clipboard) return;
        bool flushed = false;
        for(int i = 0; i < clients.length(); i++)
        {
            clientinfo &e = *clients[i];
            if(e.clientnum != ci->clientnum && e.needclipboard - ci->lastclipboard >= 0)
            {
                if(!flushed)
                {
                    flushserver(true);
                    flushed = true;
                }
                sendpacket(e.clientnum, 1, ci->clipboard);
            }
        }
    }

    void connected(clientinfo *ci)
    {
        if(modecheck(gamemode, Mode_Demo)) enddemoplayback();

        if(!hasmap(ci)) rotatemap(false);

        shouldstep = true;

        connects.removeobj(ci);
        clients.add(ci);

        ci->connectauth = 0;
        ci->connected = true;
        ci->needclipboard = totalmillis ? totalmillis : 1;
        if(mastermode>=MasterMode_Locked) ci->state.state = ClientState_Spectator;
        ci->state.lasttimeplayed = lastmillis;

        ci->team = modecheck(gamemode, Mode_Team) ? chooseworstteam(ci) : 0;

        sendwelcome(ci);
        if(restorescore(ci)) sendresume(ci);
        sendinitclient(ci);

        aiman::addclient(ci);

        if(modecheck(gamemode, Mode_Demo)) setupdemoplayback();

        if(servermotd[0]) sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, servermotd);
    }

    void parsepacket(int sender, int chan, packetbuf &p)     // has to parse exactly each byte of the packet
    {
        if(sender<0 || p.packet->flags&ENET_PACKET_FLAG_UNSEQUENCED || chan > 2) return;
        char text[MAXTRANS];
        int type;
        clientinfo *ci = sender>=0 ? getinfo(sender) : NULL, *cq = ci, *cm = ci;
        if(ci && !ci->connected)
        {
            if(chan==0) return;
            else if(chan!=1) { disconnect_client(sender, DISC_MSGERR); return; }
            else while(p.length() < p.maxlen) switch(checktype(getint(p), ci))
            {
                case NetMsg_Connect:
                {
                    getstring(text, p);
                    filtertext(text, text, false, false, MAXNAMELEN);
                    if(!text[0]) copystring(text, "unnamed");
                    copystring(ci->name, text, MAXNAMELEN+1);
                    ci->playermodel = getint(p);
                    ci->playercolor = getint(p);

                    string password, authdesc, authname;
                    getstring(password, p, sizeof(password));
                    getstring(authdesc, p, sizeof(authdesc));
                    getstring(authname, p, sizeof(authname));
                    int disc = allowconnect(ci, password);
                    if(disc)
                    {
                        if(disc == DISC_LOCAL || !serverauth[0] || strcmp(serverauth, authdesc) || !tryauth(ci, authname, authdesc))
                        {
                            disconnect_client(sender, disc);
                            return;
                        }
                        ci->connectauth = disc;
                    }
                    else connected(ci);
                    break;
                }

                case NetMsg_AuthAnswer:
                {
                    string desc, ans;
                    getstring(desc, p, sizeof(desc));
                    uint id = (uint)getint(p);
                    getstring(ans, p, sizeof(ans));
                    if(!answerchallenge(ci, id, ans, desc))
                    {
                        disconnect_client(sender, ci->connectauth);
                        return;
                    }
                    break;
                }

                case NetMsg_Ping:
                    getint(p);
                    break;

                default:
                    disconnect_client(sender, DISC_MSGERR);
                    return;
            }
            return;
        }
        else if(chan==2)
        {
            receivefile(sender, p.buf, p.maxlen);
            return;
        }

        if(p.packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;
        #define QUEUE_AI clientinfo *cm = cq;
        #define QUEUE_MSG { if(cm && (!cm->local || demorecord || hasnonlocalclients())) while(curmsg<p.length()) cm->messages.add(p.buf[curmsg++]); }
        #define QUEUE_BUF(body) { \
            if(cm && (!cm->local || demorecord || hasnonlocalclients())) \
            { \
                curmsg = p.length(); \
                { body; } \
            } \
        }
        #define QUEUE_INT(n) QUEUE_BUF(putint(cm->messages, n))
        #define QUEUE_UINT(n) QUEUE_BUF(putuint(cm->messages, n))
        #define QUEUE_STR(text) QUEUE_BUF(sendstring(text, cm->messages))
        int curmsg;
        while((curmsg = p.length()) < p.maxlen) switch(type = checktype(getint(p), ci))
        {
            case NetMsg_Pos:
            {
                int pcn = getuint(p);
                p.get();
                uint flags = getuint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                vec pos;
                for(int k = 0; k < 3; ++k)
                {
                    int n = p.get(); n |= p.get()<<8; if(flags&(1<<k)) { n |= p.get()<<16; if(n&0x800000) n |= ~0U<<24; }
                    pos[k] = n/DMF;
                }
                for(int k = 0; k < 3; ++k)
                {
                    p.get();
                }
                int mag = p.get();
                if(flags&(1<<3))
                {
                    mag |= p.get()<<8;
                }
                int dir = p.get(); dir |= p.get()<<8;
                vec vel = vec((dir%360)*RAD, (clamp(dir/360, 0, 180)-90)*RAD).mul(mag/DVELF);
                if(flags&(1<<4))
                {
                    p.get(); if(flags&(1<<5)) p.get();
                    if(flags&(1<<6))
                    {
                        for(int k = 0; k < 2; ++k)
                        {
                            p.get();
                        }
                    }
                }
                if(cp)
                {
                    if((!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==ClientState_Alive || cp->state.state==ClientState_Editing))
                    {
                        if(!ci->local && !modecheck(gamemode, Mode_Edit) && max(vel.magnitude2(), (float)fabs(vel.z)) >= 180)
                            cp->setexceeded();
                        cp->position.setsize(0);
                        while(curmsg<p.length()) cp->position.add(p.buf[curmsg++]);
                    }
                    if(smode && cp->state.state==ClientState_Alive)
                    {
                        smode->moved(cp, cp->state.o, cp->gameclip, pos, (flags&0x80)!=0);
                    }
                    cp->state.o = pos;
                    cp->gameclip = (flags&0x80)!=0;
                }
                break;
            }

            case NetMsg_Teleport:
            {
                int pcn = getint(p), teleport = getint(p), teledest = getint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender)
                {
                    cp = NULL;
                }
                if(cp && (!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==ClientState_Alive || cp->state.state==ClientState_Editing))
                {
                    flushclientposition(*cp);
                    sendf(-1, 0, "ri4x", NetMsg_Teleport, pcn, teleport, teledest, cp->ownernum);
                }
                break;
            }

            case NetMsg_Jumppad:
            {
                int pcn = getint(p), jumppad = getint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                if(cp && (!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==ClientState_Alive || cp->state.state==ClientState_Editing))
                {
                    cp->setpushed();
                    flushclientposition(*cp);
                    sendf(-1, 0, "ri3x", NetMsg_Jumppad, pcn, jumppad, cp->ownernum);
                }
                break;
            }

            case NetMsg_FromAI:
            {
                int qcn = getint(p);
                if(qcn < 0) cq = ci;
                else
                {
                    cq = getinfo(qcn);
                    if(cq && qcn != sender && cq->ownernum != sender) cq = NULL;
                }
                break;
            }

            case NetMsg_EditMode:
            {
                int val = getint(p);
                if(!ci->local && !modecheck(gamemode, Mode_Edit)) break;
                if(val ? ci->state.state!=ClientState_Alive && ci->state.state!=ClientState_Dead : ci->state.state!=ClientState_Editing) break;
                if(smode)
                {
                    if(val) smode->leavegame(ci);
                    else smode->entergame(ci);
                }
                if(val)
                {
                    ci->state.editstate = ci->state.state;
                    ci->state.state = ClientState_Editing;
                    ci->events.setsize(0);
                    ci->state.projs.reset();
                }
                else ci->state.state = ci->state.editstate;
                QUEUE_MSG;
                break;
            }

            case NetMsg_MapCRC:
            {
                getstring(text, p);
                int crc = getint(p);
                if(!ci) break;
                if(strcmp(text, smapname))
                {
                    if(ci->clientmap[0])
                    {
                        ci->clientmap[0] = '\0';
                        ci->mapcrc = 0;
                    }
                    else if(ci->mapcrc > 0) ci->mapcrc = 0;
                    break;
                }
                copystring(ci->clientmap, text);
                ci->mapcrc = text[0] ? crc : 1;
                checkmaps();
                if(cq && cq != ci && cq->ownernum != ci->clientnum) cq = NULL;
                break;
            }

            case NetMsg_CheckMaps:
                checkmaps(sender);
                break;

            case NetMsg_TrySpawn:
                if(!ci || !cq || cq->state.state!=ClientState_Dead || cq->state.lastspawn>=0 || (smode && !smode->canspawn(cq))) break;
                if(!ci->clientmap[0] && !ci->mapcrc)
                {
                    ci->mapcrc = -1;
                    checkmaps();
                    if(ci == cq) { if(ci->state.state != ClientState_Dead) break; }
                    else if(cq->ownernum != ci->clientnum) { cq = NULL; break; }
                }
                if(cq->state.deadflush)
                {
                    flushevents(cq, cq->state.deadflush);
                    cq->state.respawn();
                }
                cleartimedevents(cq);
                sendspawn(cq);
                break;

            case NetMsg_GunSelect:
            {
                int gunselect = getint(p);
                if(!cq || cq->state.state!=ClientState_Alive || !VALID_GUN(gunselect)) break;
                cq->state.gunselect = gunselect;
                QUEUE_AI;
                QUEUE_MSG;
                break;
            }

            case NetMsg_Spawn:
            {
                int ls = getint(p), gunselect = getint(p);
                if(!cq || (cq->state.state!=ClientState_Alive && cq->state.state!=ClientState_Dead && cq->state.state!=ClientState_Editing) || ls!=cq->state.lifesequence || cq->state.lastspawn<0 || !VALID_GUN(gunselect)) break;
                cq->state.lastspawn = -1;
                cq->state.state = ClientState_Alive;
                cq->state.gunselect = gunselect;
                cq->exceeded = 0;
                if(smode) smode->spawned(cq);
                QUEUE_AI;
                QUEUE_BUF({
                    putint(cm->messages, NetMsg_Spawn);
                    sendstate(cq->state, cm->messages);
                });
                break;
            }

            case NetMsg_Suicide:
            {
                if(cq) cq->addevent(new suicideevent);
                break;
            }

            case NetMsg_Shoot:
            {
                shotevent *shot = new shotevent;
                shot->id = getint(p);
                shot->millis = cq ? cq->geteventmillis(gamemillis, shot->id) : 0;
                shot->atk = getint(p);
                for(int k = 0; k < 3; ++k)
                {
                    shot->from[k] = getint(p)/DMF;
                }
                for(int k = 0; k < 3; ++k)
                {
                    shot->to[k] = getint(p)/DMF;
                }
                int hits = getint(p);
                for(int k = 0; k < hits; ++k)
                {
                    if(p.overread()) break;
                    hitinfo &hit = shot->hits.add();
                    hit.target = getint(p);
                    hit.lifesequence = getint(p);
                    hit.dist = getint(p)/DMF;
                    hit.rays = getint(p);
                    for(int k = 0; k < 3; ++k)
                    {
                        hit.dir[k] = getint(p)/DNF;
                    }
                }
                if(cq)
                {
                    cq->addevent(shot);
                    cq->setpushed();
                }
                else
                {
                    delete shot;
                }
                break;
            }

            case NetMsg_Explode:
            {
                explodeevent *exp = new explodeevent;
                int cmillis = getint(p);
                exp->millis = cq ? cq->geteventmillis(gamemillis, cmillis) : 0;
                exp->atk = getint(p);
                exp->id = getint(p);
                int hits = getint(p);
                for(int k = 0; k < hits; ++k)
                {
                    if(p.overread()) break;
                    hitinfo &hit = exp->hits.add();
                    hit.target = getint(p);
                    hit.lifesequence = getint(p);
                    hit.dist = getint(p)/DMF;
                    hit.rays = getint(p);
                    for(int k = 0; k < 3; ++k)
                    {
                        hit.dir[k] = getint(p)/DNF;
                    }
                }
                if(cq)
                {
                    cq->addevent(exp);
                }
                else
                {
                    delete exp;
                }
                break;
            }

            case NetMsg_ItemPickup:
            {
                int n = getint(p);
                if(!cq) break;
                pickupevent *pickup = new pickupevent;
                pickup->ent = n;
                cq->addevent(pickup);
                break;
            }

            case NetMsg_Text:
            {
                QUEUE_AI;
                QUEUE_MSG;
                getstring(text, p);
                filtertext(text, text, true, true);
                QUEUE_STR(text);
                if(isdedicatedserver() && cq) logoutf("%s: %s", colorname(cq), text);
                break;
            }

            case NetMsg_SayTeam:
            {
                getstring(text, p);
                if(!ci || !cq || (ci->state.state==ClientState_Spectator && !ci->local && !ci->privilege) || !modecheck(gamemode, Mode_Team) || !VALID_TEAM(cq->team)) break;
                filtertext(text, text, true, true);
                for(int i = 0; i < clients.length(); i++)
                {
                    clientinfo *t = clients[i];
                    if(t==cq || t->state.state==ClientState_Spectator || t->state.aitype != AI_None || cq->team != t->team) continue;
                    sendf(t->clientnum, 1, "riis", NetMsg_SayTeam, cq->clientnum, text);
                }
                if(isdedicatedserver() && cq)
                {
                    logoutf("%s <%s>: %s", colorname(cq), teamnames[cq->team], text);
                }
                break;
            }

            case NetMsg_SwitchName:
            {
                QUEUE_MSG;
                getstring(text, p);
                filtertext(ci->name, text, false, false, MAXNAMELEN);
                if(!ci->name[0]) copystring(ci->name, "unnamed");
                QUEUE_STR(ci->name);
                break;
            }

            case NetMsg_SwitchModel:
            {
                ci->playermodel = getint(p);
                QUEUE_MSG;
                break;
            }

            case NetMsg_SwitchColor:
            {
                ci->playercolor = getint(p);
                QUEUE_MSG;
                break;
            }

            case NetMsg_SwitchTeam:
            {
                int team = getint(p);
                if(modecheck(gamemode, Mode_Team) && VALID_TEAM(team) && ci->team != team && (!smode || smode->canchangeteam(ci, ci->team, team)))
                {
                    if(ci->state.state==ClientState_Alive) suicide(ci);
                    ci->team = team;
                    aiman::changeteam(ci);
                    sendf(-1, 1, "riiii", NetMsg_SetTeam, sender, ci->team, ci->state.state==ClientState_Spectator ? -1 : 0);
                }
                break;
            }

            case NetMsg_MapVote:
            {
                getstring(text, p);
                filtertext(text, text, false);
                fixmapname(text);
                int reqmode = getint(p);
                vote(text, reqmode, sender);
                break;
            }

            case NetMsg_ItemList:
            {
                if((ci->state.state==ClientState_Spectator && !ci->privilege && !ci->local) || !notgotitems || strcmp(ci->clientmap, smapname)) { while(getint(p)>=0 && !p.overread()) getint(p); break; }
                int n;
                while((n = getint(p))>=0 && n<MAXENTS && !p.overread())
                {
                    server_entity se = { GamecodeEnt_NotUsed, 0, false };
                    while(sents.length()<=n) sents.add(se);
                    sents[n].type = getint(p);
                    if(canspawnitem(sents[n].type))
                    {
                        if(!modecheck(gamemode, Mode_LocalOnly) && delayspawn(sents[n].type)) sents[n].spawntime = spawntime(sents[n].type);
                        else sents[n].spawned = true;
                    }
                }
                notgotitems = false;
                break;
            }

            case NetMsg_EditEnt:
            {
                int i = getint(p);
                for(int k = 0; k < 3; ++k)
                {
                    getint(p);
                }
                int type = getint(p);
                for(int k = 0; k < 5; ++k)
                {
                    getint(p);
                }
                if(!ci || ci->state.state==ClientState_Spectator) break;
                QUEUE_MSG;
                bool canspawn = canspawnitem(type);
                if(i<MAXENTS && (sents.inrange(i) || canspawnitem(type)))
                {
                    server_entity se = { GamecodeEnt_NotUsed, 0, false };
                    while(sents.length()<=i) sents.add(se);
                    sents[i].type = type;
                    if(canspawn ? !sents[i].spawned : (sents[i].spawned || sents[i].spawntime))
                    {
                        sents[i].spawntime = canspawn ? 1 : 0;
                        sents[i].spawned = false;
                    }
                }
                break;
            }

            case NetMsg_EditVar:
            {
                int type = getint(p);
                getstring(text, p);
                switch(type)
                {
                    case Id_Var: getint(p); break;
                    case Id_FloatVar: getfloat(p); break;
                    case Id_StringVar: getstring(text, p);
                }
                if(ci && ci->state.state!=ClientState_Spectator) QUEUE_MSG;
                break;
            }

            case NetMsg_Ping:
                sendf(sender, 1, "i2", NetMsg_Pong, getint(p));
                break;

            case NetMsg_ClientPing:
            {
                int ping = getint(p);
                if(ci)
                {
                    ci->ping = ping;
                    for(int i = 0; i < ci->bots.length(); i++)
                    {
                        ci->bots[i]->ping = ping;
                    }
                }
                QUEUE_MSG;
                break;
            }

            case NetMsg_MasterMode:
            {
                int mm = getint(p);
                if((ci->privilege || ci->local) && mm>=MasterMode_Open && mm<=MasterMode_Private)
                {
                    if((ci->privilege>=Priv_Admin || ci->local) || (mastermask&(1<<mm)))
                    {
                        mastermode = mm;
                        allowedips.shrink(0);
                        if(mm>=MasterMode_Private)
                        {
                            for(int i = 0; i < clients.length(); i++)
                            {
                                allowedips.add(getclientip(clients[i]->clientnum));
                            }
                        }
                        sendf(-1, 1, "rii", NetMsg_MasterMode, mastermode);
                        //sendservmsgf("mastermode is now %s (%d)", mastermodename(mastermode), mastermode);
                    }
                    else
                    {
                        sendf(sender, 1, "ris", NetMsg_ServerMsg, tempformatstring("mastermode %d is disabled on this server", mm));
                    }
                }
                break;
            }

            case NetMsg_ClearBans:
            {
                if(ci->privilege || ci->local)
                {
                    bannedips.shrink(0);
                    sendservmsg("cleared all bans");
                }
                break;
            }

            case NetMsg_Kick:
            {
                int victim = getint(p);
                getstring(text, p);
                filtertext(text, text);
                trykick(ci, victim, text);
                break;
            }

            case NetMsg_Spectator:
            {
                int spectator = getint(p), val = getint(p);
                if(!ci->privilege && !ci->local && (spectator!=sender || (ci->state.state==ClientState_Spectator && mastermode>=MasterMode_Locked))) break;
                clientinfo *spinfo = (clientinfo *)getclientinfo(spectator); // no bots
                if(!spinfo || !spinfo->connected || (spinfo->state.state==ClientState_Spectator ? val : !val)) break;

                if(spinfo->state.state!=ClientState_Spectator && val) forcespectator(spinfo);
                else if(spinfo->state.state==ClientState_Spectator && !val) unspectate(spinfo);

                if(cq && cq != ci && cq->ownernum != ci->clientnum) cq = NULL;
                break;
            }

            case NetMsg_SetTeam:
            {
                int who = getint(p), team = getint(p);
                if(!ci->privilege && !ci->local) break;
                clientinfo *wi = getinfo(who);
                if(!modecheck(gamemode, Mode_Team) || !VALID_TEAM(team) || !wi || !wi->connected || wi->team == team) break;
                if(!smode || smode->canchangeteam(wi, wi->team, team))
                {
                    if(wi->state.state==ClientState_Alive) suicide(wi);
                    wi->team = team;
                }
                aiman::changeteam(wi);
                sendf(-1, 1, "riiii", NetMsg_SetTeam, who, wi->team, 1);
                break;
            }

            case NetMsg_ForceIntermission:
                if(ci->local && !hasnonlocalclients()) startintermission();
                break;

            case NetMsg_RecordDemo:
            {
                int val = getint(p);
                if(ci->privilege < (restrictdemos ? Priv_Admin : Priv_Master) && !ci->local) break;
                if(!maxdemos || !maxdemosize)
                {
                    sendf(ci->clientnum, 1, "ris", NetMsg_ServerMsg, "the server has disabled demo recording");
                    break;
                }
                demonextmatch = val!=0;
                sendservmsgf("demo recording is %s for next match", demonextmatch ? "enabled" : "disabled");
                break;
            }

            case NetMsg_StopDemo:
            {
                if(ci->privilege < (restrictdemos ? Priv_Admin : Priv_Master) && !ci->local) break;
                stopdemo();
                break;
            }

            case NetMsg_ClearDemos:
            {
                int demo = getint(p);
                if(ci->privilege < (restrictdemos ? Priv_Admin : Priv_Master) && !ci->local) break;
                cleardemos(demo);
                break;
            }

            case NetMsg_ListDemos:
                if(!ci->privilege && !ci->local && ci->state.state==ClientState_Spectator) break;
                listdemos(sender);
                break;

            case NetMsg_GetDemo:
            {
                int n = getint(p);
                if(!ci->privilege && !ci->local && ci->state.state==ClientState_Spectator) break;
                senddemo(ci, n);
                break;
            }

            case NetMsg_GetMap:
                if(!mapdata) sendf(sender, 1, "ris", NetMsg_ServerMsg, "no map to send");
                else if(ci->getmap) sendf(sender, 1, "ris", NetMsg_ServerMsg, "already sending map");
                else
                {
                    sendservmsgf("[%s is getting the map]", colorname(ci));
                    if((ci->getmap = sendfile(sender, 2, mapdata, "ri", NetMsg_SendMap)))
                        ci->getmap->freeCallback = freegetmap;
                    ci->needclipboard = totalmillis ? totalmillis : 1;
                }
                break;

            case NetMsg_Newmap:
            {
                int size = getint(p);
                if(!ci->privilege && !ci->local && ci->state.state==ClientState_Spectator) break;
                if(size>=0)
                {
                    smapname[0] = '\0';
                    resetitems();
                    notgotitems = false;
                    if(smode) smode->newmap();
                }
                QUEUE_MSG;
                break;
            }

            case NetMsg_SetMasterMaster:
            {
                int mn = getint(p), val = getint(p);
                getstring(text, p);
                if(mn != ci->clientnum)
                {
                    if(!ci->privilege && !ci->local) break;
                    clientinfo *minfo = (clientinfo *)getclientinfo(mn);
                    if(!minfo || !minfo->connected || (!ci->local && minfo->privilege >= ci->privilege) || (val && minfo->privilege)) break;
                    setmaster(minfo, val!=0, "", NULL, NULL, Priv_Master, true);
                }
                else setmaster(ci, val!=0, text);
                // don't broadcast the master password
                break;
            }

            case NetMsg_AddBot:
            {
                aiman::reqadd(ci, getint(p));
                break;
            }

            case NetMsg_DelBot:
            {
                aiman::reqdel(ci);
                break;
            }

            case NetMsg_BotLimit:
            {
                int limit = getint(p);
                if(ci) aiman::setbotlimit(ci, limit);
                break;
            }

            case NetMsg_BotBalance:
            {
                int balance = getint(p);
                if(ci) aiman::setbotbalance(ci, balance!=0);
                break;
            }

            case NetMsg_AuthTry:
            {
                string desc, name;
                getstring(desc, p, sizeof(desc));
                getstring(name, p, sizeof(name));
                tryauth(ci, name, desc);
                break;
            }

            case NetMsg_AuthKick:
            {
                string desc, name;
                getstring(desc, p, sizeof(desc));
                getstring(name, p, sizeof(name));
                int victim = getint(p);
                getstring(text, p);
                filtertext(text, text);
                int authpriv = Priv_Auth;
                if(desc[0])
                {
                    userinfo *u = users.access(userkey(name, desc));
                    if(u) authpriv = u->privilege; else break;
                }
                if(ci->local || ci->privilege >= authpriv) trykick(ci, victim, text);
                else if(trykick(ci, victim, text, name, desc, authpriv, true) && tryauth(ci, name, desc))
                {
                    ci->authkickvictim = victim;
                    ci->authkickreason = newstring(text);
                }
                break;
            }

            case NetMsg_AuthAnswer:
            {
                string desc, ans;
                getstring(desc, p, sizeof(desc));
                uint id = (uint)getint(p);
                getstring(ans, p, sizeof(ans));
                answerchallenge(ci, id, ans, desc);
                break;
            }

            case NetMsg_PauseGame:
            {
                int val = getint(p);
                if(ci->privilege < (restrictpausegame ? Priv_Admin : Priv_Master) && !ci->local) break;
                pausegame(val > 0, ci);
                break;
            }

            case NetMsg_GameSpeed:
            {
                int val = getint(p);
                if(ci->privilege < (restrictgamespeed ? Priv_Admin : Priv_Master) && !ci->local) break;
                changegamespeed(val, ci);
                break;
            }

            case NetMsg_Copy:
                ci->cleanclipboard();
                ci->lastclipboard = totalmillis ? totalmillis : 1;
                goto genericmsg;

            case NetMsg_Paste:
                if(ci->state.state!=ClientState_Spectator) sendclipboard(ci);
                goto genericmsg;

            case NetMsg_Clipboard:
            {
                int unpacklen = getint(p), packlen = getint(p);
                ci->cleanclipboard(false);
                if(ci->state.state==ClientState_Spectator)
                {
                    if(packlen > 0) p.subbuf(packlen);
                    break;
                }
                if(packlen <= 0 || packlen > (1<<16) || unpacklen <= 0)
                {
                    if(packlen > 0) p.subbuf(packlen);
                    packlen = unpacklen = 0;
                }
                packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                putint(q, NetMsg_Clipboard);
                putint(q, ci->clientnum);
                putint(q, unpacklen);
                putint(q, packlen);
                if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                ci->clipboard = q.finalize();
                ci->clipboard->referenceCount++;
                break;
            }

            case NetMsg_EditTex:
            case NetMsg_Replace:
            case NetMsg_EditVSlot:
            {
                int size = server::msgsizelookup(type);
                if(size<=0)
                {
                    disconnect_client(sender, DISC_MSGERR);
                    return;
                }
                for(int i = 0; i < size-1; ++i)
                {
                    getint(p);
                }
                if(p.remaining() < 2)
                {
                    disconnect_client(sender, DISC_MSGERR);
                    return;
                }
                int extra = LIL_ENDIAN_SWAP(*(const ushort *)p.pad(2));
                if(p.remaining() < extra)
                {
                    disconnect_client(sender, DISC_MSGERR);
                    return;
                }
                p.pad(extra);
                if(ci && ci->state.state!=ClientState_Spectator)
                {
                    QUEUE_MSG;
                }
                break;
            }

            case NetMsg_Undo:
            case NetMsg_Redo:
            {
                int unpacklen = getint(p), packlen = getint(p);
                if(!ci || ci->state.state==ClientState_Spectator || packlen <= 0 || packlen > (1<<16) || unpacklen <= 0)
                {
                    if(packlen > 0) p.subbuf(packlen);
                    break;
                }
                if(p.remaining() < packlen) { disconnect_client(sender, DISC_MSGERR); return; }
                packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                putint(q, type);
                putint(q, ci->clientnum);
                putint(q, unpacklen);
                putint(q, packlen);
                if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                sendpacket(-1, 1, q.finalize(), ci->clientnum);
                break;
            }

            case NetMsg_ServerCommand:
                getstring(text, p);
                break;

            #define PARSEMESSAGES 1
            #include "ctf.h"
            #undef PARSEMESSAGES

            case -1:
                disconnect_client(sender, DISC_MSGERR);
                return;

            case -2:
                disconnect_client(sender, DISC_OVERFLOW);
                return;

            default: genericmsg:
            {
                int size = server::msgsizelookup(type);
                if(size<=0)
                {
                    disconnect_client(sender, DISC_MSGERR);
                    return;
                }
                for(int i = 0; i < size-1; ++i)
                {
                    getint(p);
                }
                if(ci) switch(msgfilter[type])
                {
                    case 2: case 3: if(ci->state.state != ClientState_Spectator) QUEUE_MSG; break;
                    default: if(cq && (ci != cq || ci->state.state!=ClientState_Spectator)) { QUEUE_AI; QUEUE_MSG; } break;
                }
                break;
            }
        }
    }

    int laninfoport() { return TESSERACT_LANINFO_PORT; }
    int serverport() { return TESSERACT_SERVER_PORT; }
    const char *defaultmaster() { return "master.tesseract.gg"; }
    int masterport() { return TESSERACT_MASTER_PORT; }
    int numchannels() { return 3; }

    #include "extinfo.h"

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

    #include "aiman.h"
}


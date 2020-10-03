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

extern ENetAddress masteraddress;

namespace game
{
    const char *gameident() { return "Imprimis"; }
}

namespace server
{
    struct server_entity            // server side version of "entity" type
    {
        int type;
        int spawntime;
        bool spawned;
    };

    static const int deathmillis = 300;

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

    template <int N>
    struct projectilestate
    {
        int projs[N];
        int numprojs;

        projectilestate() : numprojs(0) {}

        void reset() { numprojs = 0; }

        void add(int val)
        {
            if(numprojs>=N)
            {
                numprojs = 0;
            }
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
            return state==ClientState_Alive || (state==ClientState_Dead && gamemillis - lastdeath <= deathmillis);
        }

        bool waitexpired(int gamemillis)
        {
            return gamemillis - lastshot >= gunwait;
        }

        void reset()
        {
            if(state!=ClientState_Spectator)
            {
                state = editstate = ClientState_Dead;
            }
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

    extern int gamemillis;

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

        void addevent(gameevent *e)
        {
            if(state.state==ClientState_Spectator || events.length()>100)
            {
                delete e;
            }
            else
            {
                events.add(e);
            }
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

        void cleanclipboard(bool fullclean = true)
        {
            if(clipboard)
            {
                if(--clipboard->referenceCount <= 0)
                {
                    enet_packet_destroy(clipboard);
                }
                clipboard = NULL;
            }
            if(fullclean)
            {
                lastclipboard = 0;
            }
        }

        void cleanauthkick()
        {
            authkickvictim = -1;
            DELETEA(authkickreason);
        }

        void cleanauth(bool full = true)
        {
            authreq = 0;
            if(authchallenge)
            {
                freechallenge(authchallenge);
                authchallenge = NULL;
            }
            if(full)
            {
                cleanauthkick();
            }
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
            else
            {
                return gameoffset + clientmillis;
            }
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
    }

    enum
    {
        MasterMode_Mode = 0xF,
        MasterMode_AutoApprove = 0x1000,
        MasterMode_PrivateServer = (MasterMode_Mode | MasterMode_AutoApprove),
    };

    bool notgotitems = true;        // true when map has changed and waiting for clients to send item
    int gamemode = 0,
        gamemillis = 0,
        gamelimit = 0,
        gamespeed = 100;
    bool gamepaused = false;

    string smapname = "";
    int interm = 0;
    int mastermode = MasterMode_Open,
        mastermask = MasterMode_PrivateServer;

    vector<uint> allowedips;
    vector<ban> bannedips;

    vector<clientinfo *> connects, clients, bots;

    struct demofile
    {
        string info;
        uchar *data;
        int len;
    };

    vector<demofile> demos;

    stream *demotmp = NULL,
           *demorecord = NULL,
           *demoplayback = NULL;

    VAR(maxdemos, 0, 5, 25);
    VAR(maxdemosize, 0, 16, 31);

    VAR(restrictpausegame, 0, 1, 1);

    SVAR(serverdesc, "");
    SVAR(serverpass, "");
    SVAR(adminpass, "");

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

    bool shouldcheckteamkills = false;

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
        if(MODE_VALID(n))
        {
            return gamemodes[n - STARTGAMEMODE].name;
        }
        return unknown;
    }

    const char *modeprettyname(int n, const char *unknown)
    {
        if(MODE_VALID(n))
        {
            return gamemodes[n - STARTGAMEMODE].prettyname;
        }
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

    static teaminfo teaminfos[maxteams];

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
        if(!demotmp)
        {
            return;
        }
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
        if(!demorecord)
        {
            return;
        }
        DELETEP(demorecord);

        if(!demotmp)
        {
            return;
        }
        if(!maxdemos || !maxdemosize)
        {
            DELETEP(demotmp);
            return;
        }
        prunedemos(1);
        adddemo();
    }

    void writedemo(int chan, void *data, int len)
    {
        if(!demorecord)
        {
            return;
        }
        int stamp[3] = { gamemillis, chan, len };
        demorecord->write(stamp, sizeof(stamp));
        demorecord->write(data, len);
        if(demorecord->rawtell() >= (maxdemosize<<20))
        {
            enddemorecord();
        }
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

    void enddemoplayback()
    {
        if(!demoplayback)
        {
            return;
        }
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
        if(modecheck(gamemode, Mode_Demo))
        {
            enddemoplayback();
        }
        else
        {
            enddemorecord();
        }
    }

    void pausegame(bool val, clientinfo *ci = NULL)
    {
        if(gamepaused==val)
        {
            return;
        }
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

    SVAR(serverauth, "");

    void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen)
    {
        char buf[2*sizeof(string)];
        formatstring(buf, "%d %d %s", cn, sessionid, pwd);
        if(!hashstring(buf, result, maxlen))
        {
            *result = '\0';
        }
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
                if(!authname && !(mastermask&MasterMode_AutoApprove) && !ci->privilege && !ci->local)
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
            if(!ci->privilege)
            {
                return false;
            }
            if(trial)
            {
                return true;
            }
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

        uchar operator[](int msg) const
        {
            return msg >= 0 && msg < NetMsg_NumMsgs ? msgmask[msg] : 0;
        }
    } msgfilter(-1, NetMsg_Connect, NetMsg_ServerInfo, NetMsg_InitClient, NetMsg_Welcome, NetMsg_MapChange, NetMsg_ServerMsg, NetMsg_Damage, NetMsg_Hitpush, NetMsg_ShotFX, NetMsg_ExplodeFX, NetMsg_Died, NetMsg_SpawnState, NetMsg_ForceDeath, NetMsg_TeamInfo, NetMsg_ItemAcceptance, NetMsg_ItemSpawn, NetMsg_TimeUp, NetMsg_ClientDiscon, NetMsg_CurrentMaster, NetMsg_Pong, NetMsg_Resume, NetMsg_SendDemoList, NetMsg_SendDemo, NetMsg_DemoPlayback, NetMsg_SendMap, NetMsg_DropFlag, NetMsg_ScoreFlag, NetMsg_ReturnFlag, NetMsg_ResetFlag, NetMsg_Client, NetMsg_AuthChallenge, NetMsg_InitAI, NetMsg_DemoPacket, -2, NetMsg_CalcLight, NetMsg_Remip, NetMsg_Newmap, NetMsg_GetMap, NetMsg_SendMap, NetMsg_Clipboard, -3, NetMsg_EditEnt, NetMsg_EditFace, NetMsg_EditTex, NetMsg_EditMat, NetMsg_EditFlip, NetMsg_Copy, NetMsg_Paste, NetMsg_Rotate, NetMsg_Replace, NetMsg_DelCube, NetMsg_EditVar, NetMsg_EditVSlot, NetMsg_Undo, NetMsg_Redo, -4, NetMsg_Pos, NetMsg_NumMsgs),
      connectfilter(-1, NetMsg_Connect, -2, NetMsg_AuthAnswer, -3, NetMsg_Ping, NetMsg_NumMsgs);

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
            if(!ci->connected || ci->clientnum == exclude)
            {
                continue;
            }
            putinitclient(ci, p);
        }
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
            for(int i = 0; i < maxteams; ++i)
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
        if(smode)
        {
            smode->initclient(ci, p, true);
        }
        return 1;
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

    void clientdisconnect(int n)
    {
        clientinfo *ci = getinfo(n);
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i]->authkickvictim == ci->clientnum)
            {
                clients[i]->cleanauth();
            }
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

    bool allowbroadcast(int n)
    {
        clientinfo *ci = getinfo(n);
        return ci && ci->connected;
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

    void masterconnected()
    {
    }

    void masterdisconnected()
    {
        for(int i = clients.length(); --i >=0;) //note reverse iteration
        {
            clientinfo *ci = clients[i];
            if(ci->authreq)
            {
                authfailed(ci);
            }
        }
    }

    int laninfoport() { return IMPRIMIS_LANINFO_PORT; }
    int serverport() { return IMPRIMIS_SERVER_PORT; }
    const char *defaultmaster() { return "project-imprimis.org"; }
    int masterport() { return IMPRIMIS_MASTER_PORT; }
    int numchannels() { return 3; }

    int protocolversion()
    {
        return PROTOCOL_VERSION;
    }
    // server-side ai manager
    // note that server does not handle actual bot logic,
    // which is offloaded to the clients with the best connection
    namespace aiman
    {
        bool dorefresh = false;

        //this fxn could be entirely in the return statement but is separated for clarity
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
            clientinfo *owner = static_cast<clientinfo *>(getclientinfo(ci->ownernum));
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
            clientinfo *prevowner = static_cast<clientinfo *>(getclientinfo(ci->ownernum));
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
    }
}

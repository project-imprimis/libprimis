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
    extern int gamemillis;

    enum
    {
        MasterMode_Mode = 0xF,
        MasterMode_AutoApprove = 0x1000,
        MasterMode_PrivateServer = (MasterMode_Mode | MasterMode_AutoApprove),
    };

    bool notgotitems = true;        // true when map has changed and waiting for clients to send item
    int gamemode = 0,
        gamemillis = 0,
        gamespeed = 100;
    bool gamepaused = false;

    string smapname = "";
    int interm = 0;
    int mastermode = MasterMode_Open;

    vector<uint> allowedips;

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

    bool shouldcheckteamkills = false;

    vector<entity> ments;

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

    void sendservmsgf(const char *fmt, ...) PRINTFARGS(1, 2);
    void sendservmsgf(const char *fmt, ...)
    {
         DEFV_FORMAT_STRING(s, fmt, fmt);
         sendf(-1, 1, "ris", NetMsg_ServerMsg, s);
    }

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

    bool ispaused() { return gamepaused; }

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

    int laninfoport() { return IMPRIMIS_LANINFO_PORT; }
    int serverport() { return IMPRIMIS_SERVER_PORT; }
    const char *defaultmaster() { return "project-imprimis.org"; }
    int masterport() { return IMPRIMIS_MASTER_PORT; }
    int numchannels() { return 3; }

    int protocolversion()
    {
        return PROTOCOL_VERSION;
    }
}

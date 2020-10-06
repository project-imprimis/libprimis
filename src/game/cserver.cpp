#include "game.h"

//server game handling

extern ENetAddress masteraddress;

namespace game
{
    const char *gameident() { return "Imprimis"; }
}

namespace server
{
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

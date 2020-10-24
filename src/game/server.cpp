// server.cpp: little more than enhanced multicaster
// runs as client coroutine

#include "game.h"

static const int logstrlen = 512;

static FILE *logfile = NULL;

void closelogfile()
{
    if(logfile)
    {
        fclose(logfile);
        logfile = NULL;
    }
}

FILE *getlogfile()
{
#ifdef WIN32
    return logfile;
#else
    return logfile ? logfile : stdout;
#endif
}

void setlogfile(const char *fname)
{
    closelogfile();
    if(fname && fname[0])
    {
        fname = findfile(fname, "w");
        if(fname)
        {
            logfile = fopen(fname, "w");
        }
    }
    FILE *f = getlogfile();
    if(f)
    {
        setvbuf(f, NULL, _IOLBF, BUFSIZ);
    }
}

void logoutf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logoutfv(fmt, args);
    va_end(args);
}


static void writelog(FILE *file, const char *buf)
{
    static uchar ubuf[512];
    size_t len = strlen(buf),
           carry = 0;
    while(carry < len)
    {
        size_t numu = encodeutf8(ubuf, sizeof(ubuf)-1, &(reinterpret_cast<const uchar*>(buf))[carry], len - carry, &carry);
        if(carry >= len)
        {
            ubuf[numu++] = '\n';
        }
        fwrite(ubuf, 1, numu, file);
    }
}

static void writelogv(FILE *file, const char *fmt, va_list args)
{
    static char buf[logstrlen];
    vformatstring(buf, fmt, args, sizeof(buf));
    writelog(file, buf);
}

void logoutfv(const char *fmt, va_list args)
{
    FILE *f = getlogfile();
    if(f)
    {
        writelogv(f, fmt, args);
    }
}

enum
{
    ServerClient_Empty,
    ServerClient_Local,
    ServerClient_Remote
};

struct client                   // server side version of "dynent" type
{
    int type;
    int num;
    ENetPeer *peer;
    string hostname;
    void *info;
};

vector<client *> clients;

void sendpacket(int n, int chan, ENetPacket *packet, int exclude)
{
    switch(clients[n]->type)
    {
        case ServerClient_Remote:
        {
            enet_peer_send(clients[n]->peer, chan, packet);
            break;
        }
        case ServerClient_Local:
        {
            localservertoclient(chan, packet);
            break;
        }
    }
}

ENetPacket *sendfile(int cn, int chan, stream *file, const char *format, ...)
{
    if(cn < 0)
    {
    }
    else if(!clients.inrange(cn))
    {
        return NULL;
    }
    int len = static_cast<int>(min(file->size(), stream::offset(INT_MAX)));
    if(len <= 0 || len > 16<<20)
    {
        return NULL;
    }
    packetbuf p(maxtrans+len, ENET_PACKET_FLAG_RELIABLE);
    va_list args;
    va_start(args, format);
    while(*format)
    {
        switch(*format++)
        {
            case 'i':
            {
                int n = isdigit(*format) ? *format++-'0' : 1;
                for(int i = 0; i < n; ++i)
                {
                    putint(p, va_arg(args, int));
                }
                break;
            }
            case 's':
            {
                sendstring(va_arg(args, const char *), p);
                break;
            }
            case 'l':
            {
                putint(p, len); break;
            }
        }
    }
    va_end(args);

    file->seek(0, SEEK_SET);
    file->read(p.subbuf(len).buf, len);

    ENetPacket *packet = p.finalize();
    if(cn >= 0)
    {
        sendpacket(cn, chan, packet, -1);
    }
    else
    {
        sendclientpacket(packet, chan);
    }
    return packet->referenceCount > 0 ? packet : NULL;
}

//takes an int representing a value from the Discon enum and returns a drop message
const char *disconnectreason(int reason)
{
    switch(reason)
    {
        case Discon_EndOfPacket:
        {
            return "end of packet";
        }
        case Discon_Local:
        {
            return "server is in local mode";
        }
        case Discon_Kick:
        {
            return "kicked/banned";
        }
        case Discon_MsgError:
        {
            return "message error";
        }
        case Discon_IPBan:
        {
            return "ip is banned";
        }
        case Discon_Private:
        {
            return "server is in private mode";
        }
        case Discon_MaxClients:
        {
            return "server FULL";
        }
        case Discon_Timeout:
        {
            return "connection timed out";
        }
        case Discon_Overflow:
        {
            return "overflow";
        }
        case Discon_Password:
        {
            return "invalid password";
        }
        default:
        {
            return NULL;
        }
    }
}

ENetAddress masteraddress = { ENET_HOST_ANY, ENET_PORT_ANY },
            serveraddress = { ENET_HOST_ANY, ENET_PORT_ANY };
VARN(updatemaster, allowupdatemaster, 0, 1, 1);

SVAR(mastername, server::defaultmaster());
VAR(masterport, 1, Port_Master, 0xFFFF);

ENetSocket connectmaster(bool wait)
{
    if(!mastername[0])
    {
        return ENET_SOCKET_NULL;
    }
    if(masteraddress.host == ENET_HOST_ANY)
    {
        masteraddress.port = masterport;
        if(!resolverwait(mastername, &masteraddress))
        {
            return ENET_SOCKET_NULL;
        }
    }
    ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(sock == ENET_SOCKET_NULL)
    {
        return ENET_SOCKET_NULL;
    }
    if(wait || serveraddress.host == ENET_HOST_ANY || !enet_socket_bind(sock, &serveraddress))
    {
        enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
        if(wait)
        {
            if(!connectwithtimeout(sock, mastername, masteraddress))
            {
                return sock;
            }
        }
        else if(!enet_socket_connect(sock, &masteraddress))
        {
            return sock;
        }
    }
    enet_socket_destroy(sock);
    return ENET_SOCKET_NULL;
}

uint totalsecs = 0;

void updatetime()
{
    static int lastsec = 0;
    if(totalmillis - lastsec >= 1000)
    {
        int cursecs = (totalmillis - lastsec) / 1000;
        totalsecs += cursecs;
        lastsec += cursecs * 1000;
    }
}

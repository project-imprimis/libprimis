// client.cpp, mostly network related client game code

#include "game.h"

ENetHost *clienthost = NULL;
ENetPeer *curpeer = NULL,
         *connpeer = NULL;
int connmillis = 0,
    connattempts = 0,
    discmillis = 0;

void setrate(int rate)
{
   if(!curpeer)
   {
       return;
   }
   enet_host_bandwidth_limit(clienthost, rate*1024, rate*1024);
}

VARF(rate, 0, 0, 1024, setrate(rate));

void throttle();

VARF(throttleinterval, 0, 5, 30, throttle());
VARF(throttleaccel,    0, 2, 32, throttle());
VARF(throttledecel,    0, 2, 32, throttle());

void throttle()
{
    if(!curpeer)
    {
        return;
    }
    enet_peer_throttle_configure(curpeer, throttleinterval*1000, throttleaccel, throttledecel);
}

const ENetAddress *connectedpeer()
{
    return curpeer ? &curpeer->address : NULL;
}

ICOMMAND(connectedip, "", (),
{
    const ENetAddress *address = connectedpeer();
    string hostname;
    result(address && enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0 ? hostname : "");
});

ICOMMAND(connectedport, "", (),
{
    const ENetAddress *address = connectedpeer();
    intret(address ? address->port : -1);
});

void abortconnect()
{
    if(!connpeer)
    {
        return;
    }
    game::connectfail();
    if(connpeer->state!=ENET_PEER_STATE_DISCONNECTED)
    {
        enet_peer_reset(connpeer);
    }
    connpeer = NULL;
    if(curpeer)
    {
        return;
    }
    enet_host_destroy(clienthost);
    clienthost = NULL;
}

SVARP(connectname, "");
VARP(connectport, 0, 0, 0xFFFF);

void connectserv(const char *servername, int serverport, const char *serverpassword)
{
    if(connpeer)
    {
        conoutf("aborting connection attempt");
        abortconnect();
    }
    if(serverport <= 0)
    {
        serverport = serverport;
    }
    ENetAddress address;
    address.port = Port_Server;

    if(servername)
    {
        if(strcmp(servername, connectname))
        {
            setsvar("connectname", servername);
        }
        if(serverport != connectport)
        {
            setvar("connectport", serverport);
        }
        addserver(servername, serverport, serverpassword && serverpassword[0] ? serverpassword : NULL);
        conoutf("attempting to connect to %s:%d", servername, serverport);
        if(!resolverwait(servername, &address))
        {
            conoutf("\f3could not resolve server %s", servername);
            return;
        }
    }
    else
    {
        setsvar("connectname", "");
        setvar("connectport", 0);
        conoutf("attempting to connect over LAN");
        address.host = ENET_HOST_BROADCAST;
    }

    if(!clienthost)
    {
        clienthost = enet_host_create(NULL, 2, server::numchannels(), rate*1024, rate*1024);
        if(!clienthost)
        {
            conoutf("\f3could not connect to server");
            return;
        }
        clienthost->duplicatePeers = 0;
    }

    connpeer = enet_host_connect(clienthost, &address, server::numchannels(), 0);
    enet_host_flush(clienthost);
    connmillis = totalmillis;
    connattempts = 0;

    game::connectattempt(servername ? servername : "", serverpassword ? serverpassword : "", address);
}

void reconnect(const char *serverpassword)
{
    if(!connectname[0] || connectport <= 0)
    {
        conoutf(Console_Error, "no previous connection");
        return;
    }

    connectserv(connectname, connectport, serverpassword);
}

void disconnect(bool async, bool cleanup)
{
    if(curpeer)
    {
        if(!discmillis)
        {
            enet_peer_disconnect(curpeer, Discon_None);
            enet_host_flush(clienthost);
            discmillis = totalmillis;
        }
        if(curpeer->state!=ENET_PEER_STATE_DISCONNECTED)
        {
            if(async)
            {
                return;
            }
            enet_peer_reset(curpeer);
        }
        curpeer = NULL;
        discmillis = 0;
        conoutf("disconnected");
        game::gamedisconnect(cleanup);
        mainmenu = 1;
    }
    if(!connpeer && clienthost)
    {
        enet_host_destroy(clienthost);
        clienthost = NULL;
    }
}

void trydisconnect(bool local)
{
    if(connpeer)
    {
        conoutf("aborting connection attempt");
        abortconnect();
    }
    else if(curpeer)
    {
        conoutf("attempting to disconnect...");
        disconnect(!discmillis);
    }
    else conoutf("not connected");
    execident("resethud");
}

ICOMMAND(connect, "sis", (char *name, int *port, char *pw), connectserv(name, *port, pw));
ICOMMAND(lanconnect, "is", (int *port, char *pw), connectserv(NULL, *port, pw));
COMMAND(reconnect, "s");
ICOMMAND(disconnect, "b", (int *local), trydisconnect(*local != 0));

void sendclientpacket(ENetPacket *packet, int chan)
{
    if(curpeer)
    {
        enet_peer_send(curpeer, chan, packet);
    }
}

void flushclient()
{
    if(clienthost)
    {
        enet_host_flush(clienthost);
    }
}

void neterr(const char *s, bool disc)
{
    conoutf(Console_Error, "\f3illegal network message (%s)", s);
    if(disc)
    {
        disconnect();
    }
}

void localservertoclient(int chan, ENetPacket *packet)   // processes any updates from the server
{
    packetbuf p(packet);
    game::parsepacketclient(chan, p);
}

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost)
    {
        return;
    }
    if(connpeer && totalmillis/3000 > connmillis/3000)
    {
        conoutf("attempting to connect...");
        connmillis = totalmillis;
        ++connattempts;
        if(connattempts > 3)
        {
            conoutf("\f3could not connect to server");
            abortconnect();
            return;
        }
    }
    while(clienthost && enet_host_service(clienthost, &event, 0)>0)
    {
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                disconnect(false, false);
                curpeer = connpeer;
                connpeer = NULL;
                conoutf("connected to server");
                throttle();
                if(rate)
                {
                    setrate(rate);
                }
                game::gameconnect(true);
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE:
            {
                if(discmillis)
                {
                    conoutf("attempting to disconnect...");
                }
                else
                {
                    localservertoclient(event.channelID, event.packet);
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                if(event.data >= Discon_NumDiscons)
                {
                    event.data = Discon_None;
                }
                if(event.peer==connpeer)
                {
                    conoutf("\f3could not connect to server");
                    abortconnect();
                }
                else
                {
                    if(!discmillis || event.data)
                    {
                        const char *msg = disconnectreason(event.data);
                        if(msg)
                        {
                            conoutf("\f3server network error, disconnecting (%s) ...", msg);
                        }
                        else
                        {
                            conoutf("\f3server network error, disconnecting...");
                        }
                    }
                    disconnect();
                }
                return;
            }
            default:
            {
                break;
            }
        }
    }
}


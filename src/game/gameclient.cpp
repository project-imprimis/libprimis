#include "game.h"

//handles information taken from the server, minimap, and radar
//includes:
//getting current information on other players (health, team, kills, deaths, etc.)
//clientside authentication
//editing interactions
//parsing state changes from server
//demo commands (core logic in game/server.cpp)

VAR(editing, 1, 0, 0);
namespace game
{
    void edittoggled(bool);

    bool allowedittoggle()
    {
        if(editmode)
        {
            return true;
        }
        if(multiplayer && !modecheck(gamemode, Mode_Edit))
        {
            conoutf(Console_Error, "editing requires edit mode");
            return false;
        }
        return true;
    }
}

void toggleedit(bool force = true)
{
    if(!force)
    {
        if(!multiplayer)
        {
            return;
        }
        if(player->state!=ClientState_Alive && player->state!=ClientState_Dead && player->state!=ClientState_Editing)
        {
            return; // do not allow dead players to edit to avoid state confusion
        }
        if(!game::allowedittoggle())
        {
            return;         // not in most multiplayer modes
        }
    }
    if(!(editmode = !editmode))
    {
        player->state = player->editstate;
        player->o.z -= player->eyeheight;       // entinmap wants feet pos
        entinmap(player);                       // find spawn closest to current floating pos
    }
    else
    {
        player->editstate = player->state;
        player->state = ClientState_Editing;
    }
    cancelsel();
    keyrepeat(editmode, KeyRepeat_EditMode);
    editing = entediting = editmode;
    if(!force)
    {
        game::edittoggled(editmode);
    }
    execident("resethud");
}
ICOMMAND(edittoggle, "", (), toggleedit(false));

namespace game
{
    VARP(minimapminscale, 0, 384, 10000);
    VARP(minimapmaxscale, 1, 1024, 10000);
    VARP(minimapshowteammates, 0, 1, 1);
    FVARP(minimapalpha, 0, 1, 1);

    float calcradarscale()
    {
        //clamp minimapradius/3 to within min/max radar scale
        return std::clamp(max(minimapradius.x, minimapradius.y)/3, static_cast<float>(minimapminscale), static_cast<float>(minimapmaxscale));
    }

    void drawminimap(gameent *d, float x, float y, float s)
    {
        vec pos = vec(d->o).sub(minimapcenter).mul(minimapscale).add(0.5f),
            dir;
        vecfromyawpitch(camera1->yaw, 0, 1, 0, dir);
        float scale = calcradarscale();
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_FAN);
        for(int i = 0; i < 16; ++i)
        {
            vec v = vec(0, -1, 0).rotate_around_z(i/16.0f*2*M_PI);
            gle::attribf(x + 0.5f*s*(1.0f + v.x), y + 0.5f*s*(1.0f + v.y));
            vec tc = vec(dir).rotate_around_z(i/16.0f*2*M_PI);
            gle::attribf(1.0f - (pos.x + tc.x*scale*minimapscale.x), pos.y + tc.y*scale*minimapscale.y);
        }
        gle::end();
    }

    void setradartex()
    {
        settexture("media/interface/radar/radar.png", 3);
    }

    void drawradar(float x, float y, float s)
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,   y);   gle::attribf(0, 0);
        gle::attribf(x+s, y);   gle::attribf(1, 0);
        gle::attribf(x,   y+s); gle::attribf(0, 1);
        gle::attribf(x+s, y+s); gle::attribf(1, 1);
        gle::end();
    }

    void drawteammate(gameent *d, float x, float y, float s, gameent *o, float scale, float blipsize = 1)
    {
        vec dir = d->o;
        dir.sub(o->o).div(scale);
        float dist = dir.magnitude2(),
              maxdist = 1 - 0.05f - 0.05f;
        if(dist >= maxdist)
        {
            dir.mul(maxdist/dist);
        }
        dir.rotate_around_z(-camera1->yaw*RAD);
        float bs = 0.06f*blipsize*s,
              bx = x + s*0.5f*(1.0f + dir.x),
              by = y + s*0.5f*(1.0f + dir.y);
        vec v(-0.5f, -0.5f, 0);
        v.rotate_around_z((90+o->yaw-camera1->yaw)*RAD);
        gle::attribf(bx + bs*v.x, by + bs*v.y); gle::attribf(0, 0);
        gle::attribf(bx + bs*v.y, by - bs*v.x); gle::attribf(1, 0);
        gle::attribf(bx - bs*v.x, by - bs*v.y); gle::attribf(1, 1);
        gle::attribf(bx - bs*v.y, by + bs*v.x); gle::attribf(0, 1);
    }

    void setbliptex(int team, const char *type = "")
    {
        DEF_FORMAT_STRING(blipname, "media/interface/radar/blip%s%s.png", teamblipcolor[validteam(team) ? team : 0], type);
        settexture(blipname, 3);
    }

    void drawplayerblip(gameent *d, float x, float y, float s, float blipsize = 1)
    {
        if(d->state != ClientState_Alive && d->state != ClientState_Dead)
        {
            return;
        }
        float scale = calcradarscale();
        setbliptex(d->team, d->state == ClientState_Dead ? "_dead" : "_alive");
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_QUADS);
        drawteammate(d, x, y, s, d, scale, blipsize);
        gle::end();
    }

    void drawteammates(gameent *d, float x, float y, float s)
    {
        if(!minimapshowteammates)
        {
            return;
        }
        float scale = calcradarscale();
        int alive = 0, dead = 0;
        for(int i = 0; i < players.length(); i++)
        {
            gameent *o = players[i];
            if(o != d && o->state == ClientState_Alive && o->team == d->team)
            {
                if(!alive++)
                {
                    setbliptex(d->team, "_alive");
                    gle::defvertex(2);
                    gle::deftexcoord0();
                    gle::begin(GL_QUADS);
                }
                drawteammate(d, x, y, s, o, scale);
            }
        }
        if(alive)
        {
            gle::end();
        }
        for(int i = 0; i < players.length(); i++)
        {
            gameent *o = players[i];
            if(o != d && o->state == ClientState_Dead && o->team == d->team)
            {
                if(!dead++)
                {
                    setbliptex(d->team, "_dead");
                    gle::defvertex(2);
                    gle::deftexcoord0();
                    gle::begin(GL_QUADS);
                }
                drawteammate(d, x, y, s, o, scale);
            }
        }
        if(dead)
        {
            gle::end();
        }
    }

    #include "ctf.h"

    clientmode *cmode = NULL;
    ctfclientmode ctfmode;

    void setclientmode()
    {
        if(modecheck(gamemode, Mode_CTF))
        {
            cmode = &ctfmode;
        }
        else
        {
            cmode = NULL;
        }
    }

    bool senditemstoserver = false,
         sendcrc = false; // after a map change, since server doesn't have map data
    int lastping = 0;
    bool connected = false,
         remote = false,
         demoplayback = false,
         gamepaused = false;
    int sessionid = 0,
        mastermode = MasterMode_Open,
        gamespeed = 100;
    string servdesc = "",
           servauth = "",
           connectpass = "";

    VARP(deadpush, 1, 2, 20);

    void switchname(const char *name)
    {
        filtertext(player1->name, name, false, false, maxnamelength);
        if(!player1->name[0])
        {
            copystring(player1->name, "unnamed");
        }
        addmsg(NetMsg_SwitchName, "rs", player1->name);
    }
    void printname()
    {
        conoutf("your name is: %s", colorname(player1));
    }
    ICOMMAND(name, "sN", (char *s, int *numargs),
    {
        if(*numargs > 0)
        {
            switchname(s);
        }
        else if(!*numargs)
        {
            printname();
        }
        else
        {
            result(colorname(player1));
        }
    });
    ICOMMAND(getname, "", (), result(player1->name));

    void switchteam(const char *team)
    {
        int num = isdigit(team[0]) ? parseint(team) : teamnumber(team);
        if(!validteam(num))
        {
            return;
        }
        if(player1->clientnum < 0)
        {
            player1->team = num;
        }
        else
        {
            addmsg(NetMsg_SwitchTeam, "ri", num);
        }
    }
    void printteam()
    {
        if((player1->clientnum >= 0 && !modecheck(gamemode, Mode_Team)) || !validteam(player1->team))
        {
            conoutf("you are not in a team");
        }
        else
        {
            conoutf("your team is: \fs%s%s\fr", teamtextcode[player1->team], teamnames[player1->team]);
        }
    }
    ICOMMAND(team, "sN", (char *s, int *numargs),
    {
        if(*numargs > 0)
        {
            switchteam(s);
        }
        else if(!*numargs)
        {
            printteam();
        }
        else if((player1->clientnum < 0 || modecheck(gamemode, Mode_Team)) && validteam(player1->team))
        {
            result(tempformatstring("\fs%s%s\fr", teamtextcode[player1->team], teamnames[player1->team]));
        }
    });
    ICOMMAND(getteam, "", (), intret((player1->clientnum < 0 || modecheck(gamemode, Mode_Team)) && validteam(player1->team) ? player1->team : 0));
    ICOMMAND(getteamname, "i", (int *num), result(teamname(*num)));

    struct authkey
    {
        char *name, *key, *desc;
        int lastauth;

        authkey(const char *name, const char *key, const char *desc)
            : name(newstring(name)), key(newstring(key)), desc(newstring(desc)),
              lastauth(0)
        {
        }

        ~authkey()
        {
            DELETEA(name);
            DELETEA(key);
            DELETEA(desc);
        }
    };
    vector<authkey *> authkeys;

    authkey *findauthkey(const char *desc = "")
    {
        for(int i = 0; i < authkeys.length(); i++)
        {
            if(!strcmp(authkeys[i]->desc, desc) && !strcasecmp(authkeys[i]->name, player1->name))
            {
                return authkeys[i];
            }
        }
        for(int i = 0; i < authkeys.length(); i++)
        {
            if(!strcmp(authkeys[i]->desc, desc))
            {
                return authkeys[i];
            }
        }
        return NULL;
    }

    VARP(autoauth, 0, 1, 1);

    void addauthkey(const char *name, const char *key, const char *desc)
    {
        for(int i = authkeys.length(); --i >=0;) //note reverse iteration
        {
            if(!strcmp(authkeys[i]->desc, desc) && !strcmp(authkeys[i]->name, name))
            {
                delete authkeys.remove(i);
            }
        }
        if(name[0] && key[0])
        {
            authkeys.add(new authkey(name, key, desc));
        }
    }
    ICOMMAND(authkey, "sss", (char *name, char *key, char *desc), addauthkey(name, key, desc));

    bool hasauthkey(const char *name, const char *desc)
    {
        if(!name[0] && !desc[0])
        {
            return authkeys.length() > 0;
        }
        for(int i = authkeys.length(); --i >=0;) //note reverse iteration
        {
            if(!strcmp(authkeys[i]->desc, desc) && !strcmp(authkeys[i]->name, name))
            {
                return true;
            }
        }
        return false;
    }

    ICOMMAND(hasauthkey, "ss", (char *name, char *desc), intret(hasauthkey(name, desc) ? 1 : 0));

    void genauthkey(const char *secret)
    {
        if(!secret[0])
        {
            conoutf(Console_Error, "you must specify a secret password");
            return;
        }
        vector<char> privkey, pubkey;
        genprivkey(secret, privkey, pubkey);
        conoutf("private key: %s", privkey.getbuf());
        conoutf("public key: %s", pubkey.getbuf());
        result(privkey.getbuf());
    }
    COMMAND(genauthkey, "s");

    void getpubkey(const char *desc)
    {
        authkey *k = findauthkey(desc);
        if(!k)
        {
            if(desc[0])
            {
                conoutf("no authkey found: %s", desc);
            }
            else
            {
                conoutf("no global authkey found");
            }
            return;
        }
        vector<char> pubkey;
        if(!calcpubkey(k->key, pubkey))
        {
            conoutf("failed calculating pubkey");
            return;
        }
        result(pubkey.getbuf());
    }
    COMMAND(getpubkey, "s");

    void saveauthkeys()
    {
        string fname = "config/auth.cfg";
        stream *f = openfile(path(fname), "w");
        if(!f)
        {
            conoutf(Console_Error, "failed to open %s for writing", fname);
            return;
        }
        for(int i = 0; i < authkeys.length(); i++)
        {
            authkey *a = authkeys[i];
            f->printf("authkey %s %s %s\n", escapestring(a->name), escapestring(a->key), escapestring(a->desc));
        }
        conoutf("saved authkeys to %s", fname);
        delete f;
    }
    COMMAND(saveauthkeys, "");

    void sendmapinfo()
    {
        if(!connected)
        {
            return;
        }
        sendcrc = true;
        if(player1->state!=ClientState_Spectator || player1->privilege || !remote)
        {
            senditemstoserver = true;
        }
    }

    void edittoggled(bool on)
    {
        addmsg(NetMsg_EditMode, "ri", on ? 1 : 0);
        if(player1->state==ClientState_Dead)
        {
            deathstate(player1, true);
        }
        else if(player1->state==ClientState_Editing && player1->editstate==ClientState_Dead)
        {
            showscores(false);
        }
        disablezoom();
        player1->suicided = player1->respawned = -2;
        checkfollow();
    }

    const char *getclientname(int cn)
    {
        gameent *d = getclient(cn);
        return d ? d->name : "";
    }
    ICOMMAND(getclientname, "i", (int *cn), result(getclientname(*cn)));

    void getclientcolorname(int *cn)
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            result(colorname(d));
        }
    }
    COMMAND(getclientcolorname, "i");

    int getclientteam(int cn)
    {
        gameent *d = getclient(cn);
        return modecheck(gamemode, Mode_Team) && d && validteam(d->team) ? d->team : 0;
    }
    ICOMMAND(getclientteam, "i", (int *cn), intret(getclientteam(*cn)));

    int getclientmodel(int cn)
    {
        gameent *d = getclient(cn);
        return d ? d->playermodel : -1;
    }
    ICOMMAND(getclientmodel, "i", (int *cn), intret(getclientmodel(*cn)));

    const char *getclienticon(int cn)
    {
        gameent *d = getclient(cn);
        if(!d || d->state==ClientState_Spectator)
        {
            return "spectator";
        }
        const playermodelinfo &mdl = getplayermodelinfo(d);
        return modecheck(gamemode, Mode_Team) && validteam(d->team) ? mdl.icon[d->team] : mdl.icon[0];
    }
    ICOMMAND(getclienticon, "i", (int *cn), result(getclienticon(*cn)));

    int getclientcolor(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->state!=ClientState_Spectator ? getplayercolor(d, modecheck(gamemode, Mode_Team) && validteam(d->team) ? d->team : 0) : 0xFFFFFF;
    }
    ICOMMAND(getclientcolor, "i", (int *cn), intret(getclientcolor(*cn)));

    ICOMMAND(getclientfrags, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            intret(d->frags);
        }
    });

    ICOMMAND(getclientflags, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            intret(d->flags);
        }
    });

    ICOMMAND(getclientdeaths, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            intret(d->deaths);
        }
    });

    ICOMMAND(getclienthealth, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            intret(d->health);
        }
    });

    bool ismaster(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->privilege >= Priv_Master;
    }
    ICOMMAND(ismaster, "i", (int *cn), intret(ismaster(*cn) ? 1 : 0));

    bool isauth(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->privilege >= Priv_Auth;
    }
    ICOMMAND(isauth, "i", (int *cn), intret(isauth(*cn) ? 1 : 0));

    bool isadmin(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->privilege >= Priv_Admin;
    }
    ICOMMAND(isadmin, "i", (int *cn), intret(isadmin(*cn) ? 1 : 0));

    ICOMMAND(getmastermode, "", (), intret(mastermode));
    ICOMMAND(getmastermodename, "i", (int *mm), result(server::mastermodename(*mm, "")));

    bool isspectator(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->state==ClientState_Spectator;
    }
    ICOMMAND(isspectator, "i", (int *cn), intret(isspectator(*cn) ? 1 : 0));

    ICOMMAND(islagged, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            intret(d->state==ClientState_Lagged ? 1 : 0);
        }
    });

    ICOMMAND(isdead, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            intret(d->state==ClientState_Dead ? 1 : 0);
        }
    });

    bool isai(int cn, int type)
    {
        gameent *d = getclient(cn);
        int aitype = type > 0 && type < AI_Max ? type : AI_Bot;
        return d && d->aitype==aitype;
    }
    ICOMMAND(isai, "ii", (int *cn, int *type), intret(isai(*cn, *type) ? 1 : 0));

    int parseplayer(const char *arg)
    {
        char *end;
        int n = strtol(arg, &end, 10);
        if(*arg && !*end)
        {
            if(n!=player1->clientnum && !clients.inrange(n))
            {
                return -1;
            }
            return n;
        }
        // try case sensitive first
        for(int i = 0; i < players.length(); i++)
        {
            gameent *o = players[i];
            if(!strcmp(arg, o->name))
            {
                return o->clientnum;
            }
        }
        // nothing found, try case insensitive
        for(int i = 0; i < players.length(); i++)
        {
            gameent *o = players[i];
            if(!strcasecmp(arg, o->name))
            {
                return o->clientnum;
            }
        }
        return -1;
    }
    ICOMMAND(getclientnum, "s", (char *name), intret(name[0] ? parseplayer(name) : player1->clientnum));

    void listclients(bool local, bool bots)
    {
        vector<char> buf;
        string cn;
        int numclients = 0;
        if(local && connected)
        {
            formatstring(cn, "%d", player1->clientnum);
            buf.put(cn, strlen(cn));
            numclients++;
        }
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i] && (bots || clients[i]->aitype == AI_None))
            {
                formatstring(cn, "%d", clients[i]->clientnum);
                if(numclients++)
                {
                    buf.add(' ');
                }
                buf.put(cn, strlen(cn));
            }
        }
        buf.add('\0');
        result(buf.getbuf());
    }
    ICOMMAND(listclients, "bb", (int *local, int *bots), listclients(*local>0, *bots!=0));

    void clearbans()
    {
        addmsg(NetMsg_ClearBans, "r");
    }
    COMMAND(clearbans, "");

    void kick(const char *victim, const char *reason)
    {
        int vn = parseplayer(victim);
        if(vn>=0 && vn!=player1->clientnum)
        {
            addmsg(NetMsg_Kick, "ris", vn, reason);
        }
    }
    COMMAND(kick, "ss");

    void authkick(const char *desc, const char *victim, const char *reason)
    {
        authkey *a = findauthkey(desc);
        int vn = parseplayer(victim);
        if(a && vn>=0 && vn!=player1->clientnum)
        {
            a->lastauth = lastmillis;
            addmsg(NetMsg_AuthKick, "rssis", a->desc, a->name, vn, reason);
        }
    }
    ICOMMAND(authkick, "ss", (const char *victim, const char *reason), authkick("", victim, reason));
    ICOMMAND(sauthkick, "ss", (const char *victim, const char *reason), if(servauth[0]) authkick(servauth, victim, reason));
    ICOMMAND(dauthkick, "sss", (const char *desc, const char *victim, const char *reason), if(desc[0]) authkick(desc, victim, reason));

    vector<int> ignores;

    void ignore(int cn)
    {
        gameent *d = getclient(cn);
        if(!d || d == player1)
        {
            return;
        }
        conoutf("ignoring %s", d->name);
        if(ignores.find(cn) < 0)
        {
            ignores.add(cn);
        }
    }

    void unignore(int cn)
    {
        if(ignores.find(cn) < 0)
        {
            return;
        }
        gameent *d = getclient(cn);
        if(d)
        {
            conoutf("stopped ignoring %s", d->name);
        }
        ignores.removeobj(cn);
    }

    bool isignored(int cn) { return ignores.find(cn) >= 0; }

    ICOMMAND(ignore, "s", (char *arg), ignore(parseplayer(arg)));
    ICOMMAND(unignore, "s", (char *arg), unignore(parseplayer(arg)));
    ICOMMAND(isignored, "s", (char *arg), intret(isignored(parseplayer(arg)) ? 1 : 0));

    void setteam(const char *who, const char *team)
    {
        int i = parseplayer(who);
        if(i < 0)
        {
            return;
        }
        int num = isdigit(team[0]) ? parseint(team) : teamnumber(team);
        if(!validteam(num))
        {
            return;
        }
        addmsg(NetMsg_SetTeam, "rii", i, num);
    }
    COMMAND(setteam, "ss");

    void hashpwd(const char *pwd)
    {
        if(player1->clientnum<0)
        {
            return;
        }
        string hash;
        server::hashpassword(player1->clientnum, sessionid, pwd, hash);
        result(hash);
    }
    COMMAND(hashpwd, "s");

    void setmaster(const char *arg, const char *who)
    {
        if(!arg[0])
        {
            return;
        }
        int val = 1,
            cn = player1->clientnum;
        if(who[0])
        {
            cn = parseplayer(who);
            if(cn < 0)
            {
                return;
            }
        }
        string hash = "";
        if(!arg[1] && isdigit(arg[0]))
        {
            val = parseint(arg);
        }
        else
        {
            if(cn != player1->clientnum)
            {
                return;
            }
            server::hashpassword(player1->clientnum, sessionid, arg, hash);
        }
        addmsg(NetMsg_SetMaster, "riis", cn, val, hash);
    }
    COMMAND(setmaster, "ss");
    ICOMMAND(mastermode, "i", (int *val), addmsg(NetMsg_MasterMode, "ri", *val));

    bool tryauth(const char *desc)
    {
        authkey *a = findauthkey(desc);
        if(!a)
        {
            return false;
        }
        a->lastauth = lastmillis;
        addmsg(NetMsg_AuthTry, "rss", a->desc, a->name);
        return true;
    }
    ICOMMAND(auth, "s", (char *desc), tryauth(desc));
    ICOMMAND(sauth, "", (), if(servauth[0]) tryauth(servauth));
    ICOMMAND(dauth, "s", (char *desc), if(desc[0]) tryauth(desc));

    ICOMMAND(getservauth, "", (), result(servauth));

    void togglespectator(int val, const char *who)
    {
        int i = who[0] ? parseplayer(who) : player1->clientnum;
        if(i>=0)
        {
            addmsg(NetMsg_Spectator, "rii", i, val);
        }
    }
    ICOMMAND(spectator, "is", (int *val, char *who), togglespectator(*val, who));

    ICOMMAND(checkmaps, "", (), addmsg(NetMsg_CheckMaps, "r"));

    int gamemode = INT_MAX, nextmode = INT_MAX;

    void changemapserv(const char *name, int mode)        // forced map change from the server
    {
        if(multiplayer && modecheck(mode, Mode_LocalOnly))
        {
            conoutf(Console_Error, "mode %s (%d) not supported in multiplayer", server::modeprettyname(gamemode), gamemode);
            for(int i = 0; i < numgamemodes; ++i)
            {
                if(!modecheck(startgamemode + i, Mode_LocalOnly))
                {
                    mode = startgamemode + i;
                    break;
                }
            }
        }

        gamemode = mode;
        nextmode = mode;
        if(editmode)
        {
            toggleedit();
        }
        if(modecheck(gamemode, Mode_Demo))
        {
            entities::resetspawns();
            return;
        }
        if((modecheck(gamemode, Mode_Edit) && !name[0]) || !load_world(name, game::gameident(), game::getmapinfo()))
        {
            emptymap(0, true, name);
            startmap(name);
            senditemstoserver = false;
        }
        else //start the map loaded by load_world() in above if statement
        {
            preloadworld();
            startmap(name);
        }
        startgame();
    }

    void setmode(int mode)
    {
        if(multiplayer && modecheck(mode, Mode_LocalOnly))
        {
            conoutf(Console_Error, "mode %s (%d) not supported in multiplayer",  server::modeprettyname(mode), mode);
            intret(0);
            return;
        }
        nextmode = mode;
        intret(1);
    }
    ICOMMAND(mode, "i", (int *val), setmode(*val));
    ICOMMAND(getmode, "", (), intret(gamemode));
    ICOMMAND(getnextmode, "", (), intret(validmode(nextmode) ? nextmode : (remote ? 1 : 0)));
    ICOMMAND(getmodename, "i", (int *mode), result(server::modename(*mode, "")));
    ICOMMAND(getmodeprettyname, "i", (int *mode), result(server::modeprettyname(*mode, "")));
    ICOMMAND(timeremaining, "i", (int *formatted),
    {
        int val = max(maplimit - lastmillis, 0)/1000;
        if(*formatted)
        {
            result(tempformatstring("%d:%02d", val/60, val%60));
        }
        else
        {
            intret(val);
        }
    });
    ICOMMAND(intermission, "", (), intret(intermission ? 1 : 0));

    ICOMMANDS("MODE_CTF", "i", (int *mode), { int gamemode = *mode; modecheck(gamemode, Mode_CTF); });
    ICOMMANDS("MODE_TEAMMODE", "i", (int *mode), { int gamemode = *mode; modecheck(gamemode, Mode_Team); });
    ICOMMANDS("MODE_RAIL", "i", (int *mode), { int gamemode = *mode; modecheck(gamemode, Mode_Rail); });
    ICOMMANDS("MODE_PULSE", "i", (int *mode), { int gamemode = *mode; modecheck(gamemode, Mode_Pulse); });
    ICOMMANDS("MODE_DEMO", "i", (int *mode), { int gamemode = *mode; modecheck(gamemode, Mode_Demo); });
    ICOMMANDS("MODE_EDIT", "i", (int *mode), { int gamemode = *mode; modecheck(gamemode, Mode_Edit); });
    ICOMMANDS("MODE_LOBBY", "i", (int *mode), { int gamemode = *mode; modecheck(gamemode, Mode_Lobby); });
    ICOMMANDS("MODE_TIMED", "i", (int *mode), { int gamemode = *mode; !modecheck(gamemode, Mode_Demo|Mode_Edit|Mode_LocalOnly); });

    void changemap(const char *name, int mode) // request map change, server may ignore
    {
        if(player1->state!=ClientState_Spectator || player1->privilege)
        {
            addmsg(NetMsg_MapVote, "rsi", name, mode);
        }
    }
    void changemap(const char *name)
    {
        if(!connected) // need to be on a server to do anything
        {
            connectserv("localhost", -1, 0);
        }
        changemap(name, validmode(nextmode) ? nextmode : (remote ? 1 : 0));
    }
    ICOMMAND(map, "s", (char *name), changemap(name));

    void newmap(int size)
    {
        addmsg(NetMsg_Newmap, "ri", size);
    }

    void mapenlarge()
    {
        if(enlargemap(false))
        {
            newmap(-1);
        }
    }

    COMMAND(newmap, "i");
    COMMAND(mapenlarge, "");

    int needclipboard = -1;

    void sendclipboard()
    {
        uchar *outbuf = NULL;
        int inlen = 0,
            outlen = 0;
        if(!packeditinfo(localedit, inlen, outbuf, outlen))
        {
            outbuf = NULL;
            inlen = outlen = 0;
        }
        packetbuf p(16 + outlen, ENET_PACKET_FLAG_RELIABLE);
        putint(p, NetMsg_Clipboard);
        putint(p, inlen);
        putint(p, outlen);
        if(outlen > 0)
        {
            p.put(outbuf, outlen);
        }
        sendclientpacket(p.finalize(), 1);
        needclipboard = -1;
    }

    void edittrigger(const selinfo &sel, int op, int arg1, int arg2, int arg3, const VSlot *vs)
    {
        /* note below: argument # are noted for the sel.*, arg* parameters,
         * for the actual location in the addmsg() fxn add two for the NetMsg &
         * argument code (e.g. "ri9i4")
         */
        if(modecheck(gamemode, Mode_Edit)) switch(op)
        {
            case Edit_Flip:
            case Edit_Copy:
            case Edit_Paste:
            case Edit_DelCube:
            case Edit_AddCube:
            {
                switch(op)
                {
                    case Edit_Copy:
                    {
                        needclipboard = 0;
                        break;
                    }
                    case Edit_Paste:
                        if(needclipboard > 0)
                        {
                            c2sinfo(true);
                            sendclipboard();
                        }
                        break;
                }
                addmsg(NetMsg_EditFace + op, "ri9i4",
                   sel.o.x, sel.o.y, sel.o.z, //1-3
                   sel.s.x, sel.s.y, sel.s.z, //4-6
                   sel.grid, sel.orient,      //7,8
                   sel.cx, sel.cxs,           //9,10
                   sel.cy, sel.cys,           //11,12
                   sel.corner);               //13
                break;
            }
            case Edit_Rotate:
            {
                addmsg(NetMsg_EditFace + op, "ri9i5",
                   sel.o.x, sel.o.y, sel.o.z, //1-3
                   sel.s.x, sel.s.y, sel.s.z, //4-6
                   sel.grid, sel.orient,      //7,8
                   sel.cx, sel.cxs,           //9,10
                   sel.cy, sel.cys,           //11,12
                   sel.corner, arg1);         //13,14
                break;
            }
            case Edit_Mat:
            case Edit_Face:
            {
                addmsg(NetMsg_EditFace + op, "ri9i6",
                   sel.o.x, sel.o.y, sel.o.z, //1-3
                   sel.s.x, sel.s.y, sel.s.z, //4-6
                   sel.grid, sel.orient,      //7,8
                   sel.cx, sel.cxs,           //9,10
                   sel.cy, sel.cys,           //11,12
                   sel.corner,                //13
                   arg1, arg2);               //14,15
                break;
            }
            case Edit_Tex:
            {
                int tex1 = shouldpacktex(arg1);
                if(addmsg(NetMsg_EditFace + op, "ri9i6",
                    sel.o.x, sel.o.y, sel.o.z, //1-3
                    sel.s.x, sel.s.y, sel.s.z, //4-6
                    sel.grid, sel.orient,      //7,8
                    sel.cx, sel.cxs,           //9,10
                    sel.cy, sel.cys,           //11,12
                    sel.corner,                //13
                    tex1 ? tex1 : arg1,        //14
                    arg2))                     //15
                {
                    messages.pad(2);
                    int offset = messages.length();
                    if(tex1)
                    {
                        packvslot(messages, arg1);
                    }
                    *reinterpret_cast<ushort *>(&messages[offset-2]) = static_cast<ushort>(messages.length() - offset);
                }
                break;
            }
            case Edit_Replace:
            {
                int tex1 = shouldpacktex(arg1),
                    tex2 = shouldpacktex(arg2);
                if(addmsg(NetMsg_EditFace + op, "ri9i7",
                    sel.o.x, sel.o.y, sel.o.z, //args 1-3
                    sel.s.x, sel.s.y, sel.s.z, //4-6
                    sel.grid, sel.orient,      //7,8
                    sel.cx, sel.cxs,           //9,10
                    sel.cy, sel.cys,           //11,12
                    sel.corner,                //13
                    tex1 ? tex1 : arg1,        //14
                    tex2 ? tex2 : arg2,        //15
                    arg3))
                {
                    messages.pad(2);
                    int offset = messages.length();
                    if(tex1)
                    {
                        packvslot(messages, arg1);
                    }
                    if(tex2)
                    {
                        packvslot(messages, arg2);
                    }
                    *reinterpret_cast<ushort *>(&messages[offset-2]) = static_cast<ushort>(messages.length() - offset);
                }
                break;
            }
            case Edit_CalcLight:
            case Edit_Remip:
            {
                addmsg(NetMsg_EditFace + op, "r");
                break;
            }
            case Edit_VSlot:
            {
                if(addmsg(NetMsg_EditFace + op, "ri9i6",
                    sel.o.x, sel.o.y, sel.o.z, //1-3
                    sel.s.x, sel.s.y, sel.s.z, //4-6
                    sel.grid, sel.orient,      //7,8
                    sel.cx, sel.cxs,           //9,10
                    sel.cy, sel.cys,           //11,12
                    sel.corner,                //13
                    arg1, arg2))               //14,15
                {
                    messages.pad(2);
                    int offset = messages.length();
                    packvslot(messages, vs);
                    *reinterpret_cast<ushort *>(&messages[offset-2]) = static_cast<ushort>(messages.length() - offset);
                }
                break;
            }
            case Edit_Undo:
            case Edit_Redo:
            {
                uchar *outbuf = NULL;
                int inlen = 0, outlen = 0;
                if(packundo(op, inlen, outbuf, outlen))
                {
                    if(addmsg(NetMsg_EditFace + op, "ri2", inlen, outlen))
                    {
                        messages.put(outbuf, outlen);
                    }
                    delete[] outbuf;
                }
                break;
            }
        }
    }

    void printvar(gameent *d, ident *id)
    {
        if(id)
        {
            switch(id->type)
            {
                case Id_Var:
                {
                    int val = *id->storage.i;
                    string str;
                    if(val < 0)
                    {
                        formatstring(str, "%d", val);
                    }
                    else if(id->flags&Idf_Hex && id->maxval==0xFFFFFF)
                    {
                        formatstring(str, "0x%.6X (%d, %d, %d)", val, (val>>16)&0xFF, (val>>8)&0xFF, val&0xFF);
                    }
                    else
                    {
                        formatstring(str, id->flags&Idf_Hex ? "0x%X" : "%d", val);
                    }
                    conoutf("%s set map var \"%s\" to %s", colorname(d), id->name, str);
                    break;
                }
                case Id_FloatVar:
                {
                    conoutf("%s set map var \"%s\" to %s", colorname(d), id->name, floatstr(*id->storage.f));
                    break;
                }
                case Id_StringVar:
                {
                    conoutf("%s set map var \"%s\" to \"%s\"", colorname(d), id->name, *id->storage.s);
                    break;
                }
            }
        }
    }

    void vartrigger(ident *id)
    {
        if(!modecheck(gamemode, Mode_Edit))
        {
            return;
        }
        switch(id->type)
        {
            case Id_Var:
            {
                addmsg(NetMsg_EditVar, "risi", Id_Var, id->name, *id->storage.i);
                break;
            }
            case Id_FloatVar:
            {
                addmsg(NetMsg_EditVar, "risf", Id_FloatVar, id->name, *id->storage.f);
                break;
            }
            case Id_StringVar:
            {
                addmsg(NetMsg_EditVar, "riss", Id_StringVar, id->name, *id->storage.s);
                break;
            }
            default:
            {
                return;
            }
        }
        printvar(player1, id);
    }

    void pausegame(bool val)
    {
        if(!connected)
        {
            return;
        }
        else
        {
            addmsg(NetMsg_PauseGame, "ri", val ? 1 : 0);
        }
    }
    ICOMMAND(pausegame, "i", (int *val), pausegame(*val > 0));
    ICOMMAND(paused, "iN$", (int *val, int *numargs, ident *id),
    {
        if(*numargs > 0)
        {
            pausegame(clampvar(id, *val, 0, 1) > 0);
        }
        else if(*numargs < 0)
        {
            intret(gamepaused ? 1 : 0);
        }
        else
        {
            printvar(id, gamepaused ? 1 : 0);
        }
    });

    bool ispaused() { return gamepaused; }

    void changegamespeed(int val)
    {
        if(!connected)
        {
            return;
        }
        else
        {
            addmsg(NetMsg_GameSpeed, "ri", val);
        }
    }
    ICOMMAND(gamespeed, "iN$", (int *val, int *numargs, ident *id),
    {
        if(*numargs > 0)
        {
            changegamespeed(clampvar(id, *val, 10, 1000));
        }
        else if(*numargs < 0)
        {
            intret(gamespeed);
        }
        else
        {
            printvar(id, gamespeed);
        }
    });
    ICOMMAND(prettygamespeed, "i", (), result(tempformatstring("%d.%02dx", gamespeed/100, gamespeed%100)));

    int scaletime(int t)
    {
        return t*gamespeed;
    }

    // collect c2s messages conveniently
    vector<uchar> messages;
    int messagecn = -1,
        messagereliable = false;

    bool addmsg(int type, const char *fmt, ...)
    {
        if(!connected)
        {
            return false;
        }
        static uchar buf[maxtrans];
        ucharbuf p(buf, sizeof(buf));
        putint(p, type);
        int numi = 1,
            numf = 0,
            nums = 0,
            mcn = -1;
        bool reliable = false;
        if(fmt)
        {
            va_list args;
            va_start(args, fmt);
            while(*fmt)
            {
                switch(*fmt++)
                {
                    case 'r':
                    {
                        reliable = true;
                        break;
                    }
                    case 'c':
                    {
                        gameent *d = va_arg(args, gameent *);
                        mcn = !d || d == player1 ? -1 : d->clientnum;
                        break;
                    }
                    case 'v':
                    {
                        int  n = va_arg(args, int),
                            *v = va_arg(args, int *);
                        for(int i = 0; i < n; ++i)
                        {
                            putint(p, v[i]);
                        }
                        numi += n;
                        break;
                    }
                    case 'i':
                    {
                        int n = isdigit(*fmt) ? *fmt++-'0' : 1;
                        for(int i = 0; i < n; ++i)
                        {
                            putint(p, va_arg(args, int));
                        }
                        numi += n;
                        break;
                    }
                    case 'f':
                    {
                        int n = isdigit(*fmt) ? *fmt++-'0' : 1;
                        for(int i = 0; i < n; ++i)
                        {
                            putfloat(p, static_cast<float>(va_arg(args, double)));
                        }
                        numf += n;
                        break;
                    }
                    case 's': sendstring(va_arg(args, const char *), p); nums++; break;
                }
            }
            va_end(args);
        }
        int num = nums || numf ? 0 : numi, msgsize = server::msgsizelookup(type);
        if(msgsize && num!=msgsize)
        {
            fatal("inconsistent msg size for %d (%d != %d)", type, num, msgsize);
        }
        if(reliable)
        {
            messagereliable = true;
        }
        if(mcn != messagecn)
        {
            static uchar mbuf[16];
            ucharbuf m(mbuf, sizeof(mbuf));
            putint(m, NetMsg_FromAI);
            putint(m, mcn);
            messages.put(mbuf, m.length());
            messagecn = mcn;
        }
        messages.put(buf, p.length());
        return true;
    }

    void connectattempt(const char *name, const char *password, const ENetAddress &address)
    {
        copystring(connectpass, password);
    }

    void connectfail()
    {
        memset(connectpass, 0, sizeof(connectpass));
    }

    void gameconnect(bool _remote)
    {
        remote = _remote;
    }

    void gamedisconnect(bool cleanup)
    {
        if(remote)
        {
            stopfollowing();
        }
        ignores.setsize(0);
        connected = remote = false;
        player1->clientnum = -1;
        if(editmode)
        {
            toggleedit();
        }
        sessionid = 0;
        mastermode = MasterMode_Open;
        messages.setsize(0);
        messagereliable = false;
        messagecn = -1;
        player1->respawn();
        player1->lifesequence = 0;
        player1->state = ClientState_Alive;
        player1->privilege = Priv_None;
        sendcrc = senditemstoserver = false;
        demoplayback = false;
        gamepaused = false;
        gamespeed = 100;
        clearclients(false);
        if(cleanup)
        {
            nextmode = gamemode = INT_MAX;
            clientmap[0] = '\0';
        }
    }

    VARP(teamcolorchat, 0, 1, 1);
    const char *chatcolorname(gameent *d)
    {
        return teamcolorchat ? teamcolorname(d, NULL) : colorname(d);
    }
    void toserver(char *text)
    {
        conoutf(ConsoleMsg_Chat, "%s:%s %s", chatcolorname(player1), teamtextcode[0], text);
        addmsg(NetMsg_Text, "rcs", player1, text);
    }
    COMMANDN(say, toserver, "C");

    void sayteam(char *text)
    {
        if(!modecheck(gamemode, Mode_Team) || !validteam(player1->team))
        {
            return;
        }
        conoutf(ConsoleMsg_TeamChat, "%s:%s %s", chatcolorname(player1), teamtextcode[player1->team], text);
        addmsg(NetMsg_SayTeam, "rcs", player1, text);
    }
    COMMAND(sayteam, "C");

    ICOMMAND(servcmd, "C", (char *cmd), addmsg(NetMsg_ServerCommand, "rs", cmd));

    static void sendposition(gameent *d, packetbuf &q)
    {
        putint(q, NetMsg_Pos);
        putuint(q, d->clientnum);
        // 3 bits phys state, 1 bit life sequence, 2 bits move, 2 bits strafe
        uchar physstate = d->physstate | ((d->lifesequence&1)<<3) | ((d->move&3)<<4) | ((d->strafe&3)<<6);
        q.put(physstate);
        ivec o = ivec(vec(d->o.x, d->o.y, d->o.z-d->eyeheight).mul(DMF));
        uint vel  = min(static_cast<int>(d->vel.magnitude()*DVELF), 0xFFFF),
             fall = min(static_cast<int>(d->falling.magnitude()*DVELF), 0xFFFF);
        // 3 bits position, 1 bit velocity, 3 bits falling, 1 bit material, 1 bit crouching
        uint flags = 0;
        if(o.x < 0 || o.x > 0xFFFF)
        {
            flags |= 1<<0;
        }
        if(o.y < 0 || o.y > 0xFFFF)
        {
            flags |= 1<<1;
        }
        if(o.z < 0 || o.z > 0xFFFF)
        {
            flags |= 1<<2;
        }
        if(vel > 0xFF)
        {
            flags |= 1<<3;
        }
        if(fall > 0)
        {
            flags |= 1<<4;
            if(fall > 0xFF)
            {
                flags |= 1<<5;
            }
            if(d->falling.x || d->falling.y || d->falling.z > 0)
            {
                flags |= 1<<6;
            }
        }
        if((lookupmaterial(d->feetpos())&MatFlag_Clip) == Mat_GameClip)
        {
            flags |= 1<<7;
        }
        if(d->crouching < 0)
        {
            flags |= 1<<8;
        }
        putuint(q, flags);
        for(int k = 0; k < 3; ++k)
        {
            q.put(o[k]&0xFF);
            q.put((o[k]>>8)&0xFF);
            if(o[k] < 0 || o[k] > 0xFFFF)
            {
                q.put((o[k]>>16)&0xFF);
            }
        }
        uint dir = (d->yaw < 0 ? 360 + static_cast<int>(d->yaw)%360 : static_cast<int>(d->yaw)%360) + std::clamp(static_cast<int>(d->pitch+90), 0, 180)*360;
        q.put(dir&0xFF);
        q.put((dir>>8)&0xFF);
        q.put(std::clamp(static_cast<int>(d->roll+90), 0, 180));
        q.put(vel&0xFF);
        if(vel > 0xFF)
        {
            q.put((vel>>8)&0xFF);
        }
        float velyaw, velpitch;
        vectoyawpitch(d->vel, velyaw, velpitch);
        uint veldir = (velyaw < 0 ? 360 + static_cast<int>(velyaw)%360 : static_cast<int>(velyaw)%360) + std::clamp(static_cast<int>(velpitch+90), 0, 180)*360;
        q.put(veldir&0xFF);
        q.put((veldir>>8)&0xFF);
        if(fall > 0)
        {
            q.put(fall&0xFF);
            if(fall > 0xFF)
            {
                q.put((fall>>8)&0xFF);
            }
            if(d->falling.x || d->falling.y || d->falling.z > 0)
            {
                float fallyaw, fallpitch;
                vectoyawpitch(d->falling, fallyaw, fallpitch);
                uint falldir = (fallyaw < 0 ? 360 + static_cast<int>(fallyaw)%360 : static_cast<int>(fallyaw)%360) + std::clamp(static_cast<int>(fallpitch+90), 0, 180)*360;
                q.put(falldir&0xFF);
                q.put((falldir>>8)&0xFF);
            }
        }
    }

    void sendposition(gameent *d, bool reliable)
    {
        if(d->state != ClientState_Alive && d->state != ClientState_Editing)
        {
            return;
        }
        packetbuf q(100, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        sendposition(d, q);
        sendclientpacket(q.finalize(), 0);
    }

    void sendpositions()
    {
        for(int i = 0; i < players.length(); i++)
        {
            gameent *d = players[i];
            if((d == player1 || d->ai) && (d->state == ClientState_Alive || d->state == ClientState_Editing))
            {
                packetbuf q(100);
                sendposition(d, q);
                for(int j = i+1; j < players.length(); j++)
                {
                    gameent *d = players[j];
                    if((d == player1 || d->ai) && (d->state == ClientState_Alive || d->state == ClientState_Editing))
                    {
                        sendposition(d, q);
                    }
                }
                sendclientpacket(q.finalize(), 0);
                break;
            }
        }
    }

    void sendmessages()
    {
        packetbuf p(maxtrans);
        if(sendcrc)
        {
            p.reliable();
            sendcrc = false;
            const char *mname = getclientmap();
            putint(p, NetMsg_MapCRC);
            sendstring(mname, p);
            putint(p, mname[0] ? getmapcrc() : 0);
        }
        if(senditemstoserver)
        {
            p.reliable();
            entities::putitems(p);
            if(cmode)
            {
                cmode->senditems(p);
            }
            senditemstoserver = false;
        }
        if(messages.length())
        {
            p.put(messages.getbuf(), messages.length());
            messages.setsize(0);
            if(messagereliable)
            {
                p.reliable();
            }
            messagereliable = false;
            messagecn = -1;
        }
        if(totalmillis-lastping>250)
        {
            putint(p, NetMsg_Ping);
            putint(p, totalmillis);
            lastping = totalmillis;
        }
        sendclientpacket(p.finalize(), 1);
    }

    void c2sinfo(bool force) // send update to the server
    {
        static int lastupdate = -1000;
        if(totalmillis - lastupdate < 5 && !force)
        {
            return; // don't update faster than 1000/5 = 200fps
        }
        lastupdate = totalmillis;
        sendpositions();
        sendmessages();
        flushclient();
    }

    void sendintro()
    {
        packetbuf p(maxtrans, ENET_PACKET_FLAG_RELIABLE);
        putint(p, NetMsg_Connect);
        sendstring(player1->name, p);
        putint(p, player1->playermodel);
        putint(p, player1->playercolor);
        string hash = "";
        if(connectpass[0])
        {
            server::hashpassword(player1->clientnum, sessionid, connectpass, hash);
            memset(connectpass, 0, sizeof(connectpass));
        }
        sendstring(hash, p);
        authkey *a = servauth[0] && autoauth ? findauthkey(servauth) : NULL;
        if(a)
        {
            a->lastauth = lastmillis;
            sendstring(a->desc, p);
            sendstring(a->name, p);
        }
        else
        {
            sendstring("", p);
            sendstring("", p);
        }
        sendclientpacket(p.finalize(), 1);
    }

    void updatepos(gameent *d)
    {
        /* update the position of other clients in the game in our world
         * don't care if he's in the scenery or other players,
         * just don't overlap with our client
         */
        const float r = player1->radius+d->radius,
                    dx = player1->o.x-d->o.x,
                    dy = player1->o.y-d->o.y,
                    dz = player1->o.z-d->o.z,
                    rz = player1->aboveeye+d->eyeheight,
                    fx = static_cast<float>(fabs(dx)),
                    fy = static_cast<float>(fabs(dy)),
                    fz = static_cast<float>(fabs(dz));
        if(fx<r && fy<r && fz<rz && player1->state!=ClientState_Spectator && d->state!=ClientState_Dead)
        {
            if(fx<fy)
            {
                d->o.y += dy<0 ? r-fy : -(r-fy);  // push aside
            }
            else
            {
                d->o.x += dx<0 ? r-fx : -(r-fx);
            }
        }
        int lagtime = totalmillis-d->lastupdate;
        if(lagtime)
        {
            if(d->state!=ClientState_Spawning && d->lastupdate)
            {
                d->plag = (d->plag*5+lagtime)/6;
            }
            d->lastupdate = totalmillis;
        }
    }

    void parsepositions(ucharbuf &p)
    {
        int type;
        while(p.remaining())
        {
            switch(type = getint(p))
            {
                case NetMsg_DemoPacket:
                {
                    break;
                }
                case NetMsg_Pos:                        // position of another client
                {
                    int cn = getuint(p),
                        physstate = p.get(),
                        flags = getuint(p);
                    vec o, vel, falling;
                    float yaw, pitch, roll;
                    for(int k = 0; k < 3; ++k)
                    {
                        int n = p.get();
                        n |= p.get()<<8;
                        if(flags&(1<<k))
                        {
                            n |= p.get()<<16;
                            if(n&0x800000)
                            {
                                n |= ~0U<<24;
                            }
                        }
                        o[k] = n/DMF;
                    }
                    int dir = p.get();
                    dir |= p.get()<<8;
                    yaw = dir%360;
                    pitch = std::clamp(dir/360, 0, 180)-90;
                    roll = std::clamp(static_cast<int>(p.get()), 0, 180)-90;
                    int mag = p.get();
                    if(flags&(1<<3))
                    {
                        mag |= p.get()<<8;
                    }
                    dir = p.get(); dir |= p.get()<<8;
                    vecfromyawpitch(dir%360, std::clamp(dir/360, 0, 180)-90, 1, 0, vel);
                    vel.mul(mag/DVELF);
                    if(flags&(1<<4))
                    {
                        mag = p.get();
                        if(flags&(1<<5))
                        {
                            mag |= p.get()<<8;
                        }
                        if(flags&(1<<6))
                        {
                            dir = p.get(); dir |= p.get()<<8;
                            vecfromyawpitch(dir%360, std::clamp(dir/360, 0, 180)-90, 1, 0, falling);
                        }
                        else
                        {
                            falling = vec(0, 0, -1);
                        }
                        falling.mul(mag/DVELF);
                    }
                    else
                    {
                        falling = vec(0, 0, 0);
                    }
                    int seqcolor = (physstate>>3)&1;
                    gameent *d = getclient(cn);
                    if(!d || d->lifesequence < 0 || seqcolor!=(d->lifesequence&1) || d->state==ClientState_Dead)
                    {
                        continue;
                    }
                    float oldyaw = d->yaw,
                          oldpitch = d->pitch,
                          oldroll = d->roll;
                    d->yaw = yaw;
                    d->pitch = pitch;
                    d->roll = roll;
                    d->move = (physstate>>4)&2 ? -1 : (physstate>>4)&1;
                    d->strafe = (physstate>>6)&2 ? -1 : (physstate>>6)&1;
                    d->crouching = (flags&(1<<8))!=0 ? -1 : abs(d->crouching);
                    vec oldpos(d->o);
                    d->o = o;
                    d->o.z += d->eyeheight;
                    d->vel = vel;
                    d->falling = falling;
                    d->physstate = physstate&7;
                    updatephysstate(d);
                    updatepos(d);
                    if(smoothmove && d->smoothmillis>=0 && oldpos.dist(d->o) < smoothdist)
                    {
                        d->newpos = d->o;
                        d->newyaw = d->yaw;
                        d->newpitch = d->pitch;
                        d->newroll = d->roll;
                        d->o = oldpos;
                        d->yaw = oldyaw;
                        d->pitch = oldpitch;
                        d->roll = oldroll;
                        (d->deltapos = oldpos).sub(d->newpos);
                        d->deltayaw = oldyaw - d->newyaw;
                        if(d->deltayaw > 180)
                        {
                            d->deltayaw -= 360;
                        }
                        else if(d->deltayaw < -180)
                        {
                            d->deltayaw += 360;
                        }
                        d->deltapitch = oldpitch - d->newpitch;
                        d->deltaroll = oldroll - d->newroll;
                        d->smoothmillis = lastmillis;
                    }
                    else
                    {
                        d->smoothmillis = 0;
                    }
                    if(d->state==ClientState_Lagged || d->state==ClientState_Spawning)
                    {
                        d->state = ClientState_Alive;
                    }
                    break;
                }
                case NetMsg_Teleport:
                {
                    int cn = getint(p),
                        tp = getint(p),
                        td = getint(p);
                    gameent *d = getclient(cn);
                    if(!d || d->lifesequence < 0 || d->state==ClientState_Dead)
                    {
                        continue;
                    }
                    entities::teleporteffects(d, tp, td, false);
                    break;
                }
                case NetMsg_Jumppad:
                {
                    int cn = getint(p),
                        jp = getint(p);
                    gameent *d = getclient(cn);
                    if(!d || d->lifesequence < 0 || d->state==ClientState_Dead)
                    {
                        continue;
                    }
                    entities::jumppadeffects(d, jp, false);
                    break;
                }
                default:
                {
                    neterr("type");
                    return;
                }
            }
        }
    }

    void parsestate(gameent *d, ucharbuf &p, bool resume = false)
    {
        if(!d)
        {
            static gameent dummy;
            d = &dummy;
        }
        if(resume)
        {
            if(d==player1)
            {
                getint(p);
            }
            else
            {
                d->state = getint(p);
            }
            d->frags = getint(p);
            d->flags = getint(p);
            d->deaths = getint(p);
        }
        d->lifesequence = getint(p);
        d->health = getint(p);
        d->maxhealth = getint(p);
        if(resume && d==player1)
        {
            getint(p);
            for(int i = 0; i < Gun_NumGuns; ++i)
            {
                getint(p);
            }
        }
        else
        {
            int gun = getint(p);
            d->gunselect = std::clamp(gun, 0, Gun_NumGuns-1);
            for(int i = 0; i < Gun_NumGuns; ++i)
            {
                d->ammo[i] = getint(p);
            }
        }
    }

    extern int deathscore;

    void parsemessages(int cn, gameent *d, ucharbuf &p)
    {
        static char text[maxtrans];
        int type;
        bool mapchanged = false,
             demopacket = false;
        while(p.remaining())
        {
            switch(type = getint(p))
            {
                case NetMsg_DemoPacket:
                {
                    demopacket = true;
                    break;
                }
                case NetMsg_ServerInfo:                   // welcome messsage from the server
                {
                    int mycn = getint(p),
                        prot = getint(p);
                    if(prot!=ProtocolVersion)
                    {
                        conoutf(Console_Error, "you are using a different game protocol (you: %d, server: %d)", ProtocolVersion, prot);
                        disconnect();
                        return;
                    }
                    sessionid = getint(p);
                    player1->clientnum = mycn;      // we are now connected
                    if(getint(p) > 0)
                    {
                        conoutf("this server is password protected");
                    }
                    getstring(servdesc, p, sizeof(servdesc));
                    getstring(servauth, p, sizeof(servauth));
                    sendintro();
                    break;
                }
                case NetMsg_Welcome:
                {
                    connected = true;
                    notifywelcome();
                    break;
                }
                case NetMsg_PauseGame:
                {
                    bool val = getint(p) > 0;
                    int cn = getint(p);
                    gameent *a = cn >= 0 ? getclient(cn) : NULL;
                    if(!demopacket)
                    {
                        gamepaused = val;
                        player1->attacking = Act_Idle;
                    }
                    if(a)
                    {
                        conoutf("%s %s the game", colorname(a), val ? "paused" : "resumed");
                    }
                    else
                    {
                        conoutf("game is %s", val ? "paused" : "resumed");
                    }
                    break;
                }
                case NetMsg_GameSpeed:
                {
                    int val = std::clamp(getint(p), 10, 1000),
                        cn = getint(p);
                    gameent *a = cn >= 0 ? getclient(cn) : NULL;
                    if(!demopacket)
                    {
                        gamespeed = val;
                    }
                    if(a)
                    {
                        conoutf("%s set gamespeed to %d", colorname(a), val);
                    }
                    else
                    {
                        conoutf("gamespeed is %d", val);
                    }
                    break;
                }
                case NetMsg_Client:
                {
                    int cn = getint(p),
                        len = getuint(p);
                    ucharbuf q = p.subbuf(len);
                    parsemessages(cn, getclient(cn), q);
                    break;
                }
                case NetMsg_Sound:
                {
                    if(!d)
                    {
                        return;
                    }
                    playsound(getint(p), &d->o);
                    break;
                }
                case NetMsg_Text:
                {
                    if(!d)
                    {
                        return;
                    }
                    getstring(text, p);
                    filtertext(text, text, true, true);
                    if(isignored(d->clientnum))
                    {
                        break;
                    }
                    if(d->state!=ClientState_Dead && d->state!=ClientState_Spectator)
                    {
                        particle_textcopy(d->abovehead(), text, Part_Text, 2000, 0x32FF64, 4.0f, -8);
                    }
                    conoutf(ConsoleMsg_Chat, "%s:%s %s", chatcolorname(d), teamtextcode[0], text);
                    break;
                }
                case NetMsg_SayTeam:
                {
                    int tcn = getint(p);
                    gameent *t = getclient(tcn);
                    getstring(text, p);
                    filtertext(text, text, true, true);
                    if(!t || isignored(t->clientnum))
                    {
                        break;
                    }
                    int team = validteam(t->team) ? t->team : 0;
                    if(t->state!=ClientState_Dead && t->state!=ClientState_Spectator)
                    {
                        particle_textcopy(t->abovehead(), text, Part_Text, 2000, teamtextcolor[team], 4.0f, -8);
                    }
                    conoutf(ConsoleMsg_TeamChat, "%s:%s %s", chatcolorname(t), teamtextcode[team], text);
                    break;
                }
                case NetMsg_MapChange:
                {
                    getstring(text, p);
                    changemapserv(text, getint(p));
                    mapchanged = true;
                    if(!getint(p))
                    {
                        senditemstoserver = false;
                    }
                    break;
                }
                case NetMsg_ForceDeath:
                {
                    int cn = getint(p);
                    gameent *d = cn==player1->clientnum ? player1 : newclient(cn);
                    if(!d)
                    {
                        break;
                    }
                    if(d==player1)
                    {
                        if(editmode)
                        {
                            toggleedit();
                        }
                        if(deathscore)
                        {
                            showscores(true);
                        }
                    }
                    else
                    {
                        d->resetinterp();
                    }
                    d->state = ClientState_Dead;
                    checkfollow();
                    break;
                }
                case NetMsg_ItemList:
                {
                    int n;
                    while((n = getint(p))>=0 && !p.overread())
                    {
                        if(mapchanged)
                        {
                            entities::setspawn(n, true);
                        }
                        getint(p); // type
                    }
                    break;
                }
                case NetMsg_InitClient:            // another client either connected or changed name/team
                {
                    int cn = getint(p);
                    gameent *d = newclient(cn);
                    if(!d)
                    {
                        getstring(text, p);
                        getstring(text, p);
                        getint(p);
                        getint(p);
                        break;
                    }
                    getstring(text, p);
                    filtertext(text, text, false, false, maxnamelength);
                    if(!text[0])
                    {
                        copystring(text, "unnamed");
                    }
                    if(d->name[0])          // already connected
                    {
                        if(strcmp(d->name, text) && !isignored(d->clientnum))
                        {
                            conoutf("%s is now known as %s", colorname(d), colorname(d, text));
                        }
                    }
                    else                    // new client
                    {
                        conoutf("\f0join:\f7 %s", colorname(d, text));
                        if(needclipboard >= 0)
                        {
                            needclipboard++;
                        }
                    }
                    copystring(d->name, text, maxnamelength+1);
                    d->team = getint(p);
                    if(!validteam(d->team))
                    {
                        d->team = 0;
                    }
                    d->playermodel = getint(p);
                    d->playercolor = getint(p);
                    break;
                }
                case NetMsg_SwitchName:
                {
                    getstring(text, p);
                    if(d)
                    {
                        filtertext(text, text, false, false, maxnamelength);
                        if(!text[0])
                        {
                            copystring(text, "unnamed");
                        }
                        if(strcmp(text, d->name))
                        {
                            if(!isignored(d->clientnum))
                            {
                                conoutf("%s is now known as %s", colorname(d), colorname(d, text));
                            }
                            copystring(d->name, text, maxnamelength+1);
                        }
                    }
                    break;
                }
                case NetMsg_SwitchModel:
                {
                    int model = getint(p);
                    if(d)
                    {
                        d->playermodel = model;
                        if(d->ragdoll)
                        {
                            cleanragdoll(d);
                        }
                    }
                    break;
                }
                case NetMsg_SwitchColor:
                {
                    int color = getint(p);
                    if(d)
                    {
                        d->playercolor = color;
                    }
                    break;
                }
                case NetMsg_ClientDiscon:
                {
                    clientdisconnected(getint(p));
                    break;
                }
                case NetMsg_Spawn:
                {
                    if(d)
                    {
                        if(d->state==ClientState_Dead && d->lastpain)
                        {
                            saveragdoll(d);
                        }
                        d->respawn();
                    }
                    parsestate(d, p);
                    if(!d)
                    {
                        break;
                    }
                    d->state = ClientState_Spawning;
                    if(d == followingplayer())
                    {
                        lasthit = 0;
                    }
                    checkfollow();
                    break;
                }
                case NetMsg_SpawnState:
                {
                    int scn = getint(p);
                    gameent *s = getclient(scn);
                    if(!s)
                    {
                        parsestate(NULL, p);
                        break;
                    }
                    if(s->state==ClientState_Dead && s->lastpain)
                    {
                        saveragdoll(s);
                    }
                    if(s==player1)
                    {
                        if(editmode)
                        {
                            toggleedit();
                        }
                    }
                    s->respawn();
                    parsestate(s, p);
                    s->state = ClientState_Alive;
                    if(cmode)
                    {
                        cmode->pickspawn(s);
                    }
                    else
                    {
                        findplayerspawn(s, -1, modecheck(gamemode, Mode_Team) ? s->team : 0);
                    }
                    if(s == player1)
                    {
                        showscores(false);
                        lasthit = 0;
                    }
                    if(cmode)
                    {
                        cmode->respawned(s);
                    }
                    ai::spawned(s);
                    checkfollow();
                    addmsg(NetMsg_Spawn, "rcii", s, s->lifesequence, s->gunselect);
                    break;
                }
                case NetMsg_ShotFX:
                {
                    int scn = getint(p),
                        atk = getint(p),
                        id = getint(p);
                    vec from, to;
                    for(int k = 0; k < 3; ++k)
                    {
                        from[k] = getint(p)/DMF;
                    }
                    for(int k = 0; k < 3; ++k)
                    {
                        to[k] = getint(p)/DMF;
                    }
                    gameent *s = getclient(scn);
                    if(!s || !validattack(atk))
                    {
                        break;
                    }
                    int gun = attacks[atk].gun;
                    s->gunselect = gun;
                    s->ammo[gun] -= attacks[atk].use;
                    s->gunwait = attacks[atk].attackdelay;
                    int prevaction = s->lastaction;
                    s->lastaction = lastmillis;
                    s->lastattack = atk;
                    shoteffects(atk, from, to, s, false, id, prevaction);
                    break;
                }
                case NetMsg_ExplodeFX:
                {
                    int ecn = getint(p), atk = getint(p), id = getint(p);
                    gameent *e = getclient(ecn);
                    if(!e || !validattack(atk))
                    {
                        break;
                    }
                    explodeeffects(atk, e, false, id);
                    break;
                }
                case NetMsg_Damage:
                {
                    int tcn = getint(p),
                        acn = getint(p),
                        damage = getint(p),
                        health = getint(p);
                    gameent *target = getclient(tcn),
                            *actor  = getclient(acn);
                    if(!target || !actor)
                    {
                        break;
                    }
                    target->health = health;
                    if(target->state == ClientState_Alive && actor != player1)
                    {
                        target->lastpain = lastmillis;
                    }
                    damaged(damage, target, actor, false);
                    break;
                }
                case NetMsg_Hitpush:
                {
                    int tcn = getint(p),
                        atk = getint(p),
                        damage = getint(p);
                    gameent *target = getclient(tcn);
                    vec dir;
                    for(int k = 0; k < 3; ++k)
                    {
                        dir[k] = getint(p)/DNF;
                    }
                    if(!target || !validattack(atk))
                    {
                        break;
                    }
                    target->hitpush(damage * (target->health<=0 ? deadpush : 1), dir, NULL, atk);
                    break;
                }
                case NetMsg_Died:
                {
                    int vcn = getint(p),
                        acn = getint(p),
                        frags = getint(p),
                        tfrags = getint(p);
                    gameent *victim = getclient(vcn),
                            *actor  = getclient(acn);
                    if(!actor)
                    {
                        break;
                    }
                    actor->frags = frags;
                    if(modecheck(gamemode, Mode_Team))
                    {
                        setteaminfo(actor->team, tfrags);
                    }

                    if(!victim)
                    {
                        break;
                    }
                    killed(victim, actor);
                    break;
                }
                case NetMsg_TeamInfo:
                    for(int i = 0; i < maxteams; ++i)
                    {
                        int frags = getint(p);
                        if(modecheck(gamemode, Mode_Team))
                        {
                            setteaminfo(1+i, frags);
                        }
                    }
                    break;
                case NetMsg_GunSelect:
                {
                    if(!d)
                    {
                        return;
                    }
                    int gun = getint(p);
                    if(!validgun(gun))
                    {
                        return;
                    }
                    d->gunselect = gun;
                    playsound(Sound_WeapLoad, &d->o);
                    break;
                }
                case NetMsg_Taunt:
                {
                    if(!d)
                    {
                        return;
                    }
                    d->lasttaunt = lastmillis;
                    break;
                }
                case NetMsg_Resume:
                {
                    for(;;)
                    {
                        int cn = getint(p);
                        if(p.overread() || cn<0)
                        {
                            break;
                        }
                        gameent *d = (cn == player1->clientnum ? player1 : newclient(cn));
                        parsestate(d, p, true);
                    }
                    break;
                }
                case NetMsg_ItemSpawn:
                {
                    int i = getint(p);
                    if(!entities::ents.inrange(i))
                    {
                        break;
                    }
                    entities::setspawn(i, true);
                    playsound(Sound_ItemSpawn, &entities::ents[i]->o, NULL, 0, 0, 0, -1, 0, 1500);
                    break;
                }
                case NetMsg_ItemAcceptance:            // server acknowledges that I picked up this item
                {
                    break;
                }
                case NetMsg_Clipboard:
                {
                    int cn = getint(p),
                        unpacklen = getint(p),
                        packlen = getint(p);
                    gameent *d = getclient(cn);
                    ucharbuf q = p.subbuf(max(packlen, 0));
                    if(d)
                    {
                        unpackeditinfo(d->edit, q.buf, q.maxlen, unpacklen);
                    }
                    break;
                }
                case NetMsg_Undo:
                case NetMsg_Redo:
                {
                    int cn = getint(p),
                        unpacklen = getint(p),
                        packlen = getint(p);
                    gameent *d = getclient(cn);
                    ucharbuf q = p.subbuf(max(packlen, 0));
                    if(d)
                    {
                        unpackundo(q.buf, q.maxlen, unpacklen);
                    }
                    break;
                }
                case NetMsg_EditFace:              // coop editing messages
                case NetMsg_EditTex:
                case NetMsg_EditMat:
                case NetMsg_EditFlip:
                case NetMsg_Copy:
                case NetMsg_Paste:
                case NetMsg_Rotate:
                case NetMsg_Replace:
                case NetMsg_DelCube:
                case NetMsg_AddCube:
                case NetMsg_EditVSlot:
                {
                    if(!d)
                    {
                        return;
                    }
                    selinfo sel;
                    sel.o.x = getint(p);
                    sel.o.y = getint(p);
                    sel.o.z = getint(p);
                    sel.s.x = getint(p);
                    sel.s.y = getint(p);
                    sel.s.z = getint(p);
                    sel.grid = getint(p);
                    sel.orient = getint(p);
                    sel.cx = getint(p);
                    sel.cxs = getint(p);
                    sel.cy = getint(p),
                    sel.cys = getint(p);
                    sel.corner = getint(p);
                    switch(type)
                    {
                        case NetMsg_EditFace:
                        {
                            int dir = getint(p),
                                mode = getint(p);
                            if(sel.validate())
                            {
                                mpeditface(dir, mode, sel, false);
                            }
                            break;
                        }
                        case NetMsg_EditTex:
                        {
                            int tex = getint(p),
                                allfaces = getint(p);
                            if(p.remaining() < 2)
                            {
                                return;
                            }
                            int extra = *reinterpret_cast<const ushort *>(p.pad(2));
                            if(p.remaining() < extra)
                            {
                                return;
                            }
                            ucharbuf ebuf = p.subbuf(extra);
                            if(sel.validate())
                            {
                                mpedittex(tex, allfaces, sel, ebuf);
                            }
                            break;
                        }
                        case NetMsg_EditMat:
                        {
                            int mat = getint(p), filter = getint(p);
                            if(sel.validate())
                            {
                                mpeditmat(mat, filter, sel, false);
                            }
                            break;
                        }
                        case NetMsg_EditFlip:
                        {
                            if(sel.validate())
                            {
                                mpflip(sel, false);
                            }
                            break;
                        }
                        case NetMsg_Copy:
                        {
                            if(d && sel.validate())
                            {
                                mpcopy(d->edit, sel, false);
                            }
                            break;
                        }
                        case NetMsg_Paste:
                        {
                            if(d && sel.validate())
                            {
                                mppaste(d->edit, sel, false);
                            }
                            break;
                        }
                        case NetMsg_Rotate:
                        {
                            int dir = getint(p);
                            if(sel.validate())
                            {
                                mprotate(dir, sel, false);
                            }
                            break;
                        }
                        case NetMsg_Replace:
                        {
                            int oldtex = getint(p),
                                newtex = getint(p),
                                insel = getint(p);
                            if(p.remaining() < 2)
                            {
                                return;
                            }
                            int extra = *reinterpret_cast<const ushort *>(p.pad(2));
                            if(p.remaining() < extra)
                            {
                                return;
                            }
                            ucharbuf ebuf = p.subbuf(extra);
                            if(sel.validate())
                            {
                                mpreplacetex(oldtex, newtex, insel>0, sel, ebuf);
                            }
                            break;
                        }
                        case NetMsg_DelCube:
                        {
                            if(sel.validate())
                            {
                                mpdelcube(sel, false);
                            }
                            break;
                        }
                        case NetMsg_AddCube:
                        {
                            if(sel.validate())
                            {
                                mpplacecube(sel, 1, false);
                            }
                            break;
                        }
                        case NetMsg_EditVSlot:
                        {
                            int delta = getint(p),
                                allfaces = getint(p);
                            if(p.remaining() < 2)
                            {
                                return;
                            }
                            int extra = *reinterpret_cast<const ushort *>(p.pad(2));
                            if(p.remaining() < extra)
                            {
                                return;
                            }
                            ucharbuf ebuf = p.subbuf(extra);
                            if(sel.validate())
                            {
                                mpeditvslot(delta, allfaces, sel, ebuf);
                            }
                            break;
                        }
                    }
                    break;
                }
                case NetMsg_Remip:
                {
                    if(!d)
                    {
                        return;
                    }
                    conoutf("%s remipped", colorname(d));
                    mpremip(false);
                    break;
                }
                case NetMsg_CalcLight:
                {
                    if(!d)
                    {
                        return;
                    }
                    conoutf("%s calced lights", colorname(d));
                    mpcalclight(false);
                    break;
                }
                case NetMsg_EditEnt:            // coop edit of ent
                {
                    if(!d)
                    {
                        return;
                    }
                    int i = getint(p);
                    float x = getint(p)/DMF,
                          y = getint(p)/DMF,
                          z = getint(p)/DMF;
                    int type = getint(p);
                    int attr1 = getint(p),
                        attr2 = getint(p),
                        attr3 = getint(p),
                        attr4 = getint(p),
                        attr5 = getint(p);
                    mpeditent(i, vec(x, y, z), type, attr1, attr2, attr3, attr4, attr5, false);
                    break;
                }
                case NetMsg_EditVar:
                {
                    if(!d)
                    {
                        return;
                    }
                    int type = getint(p);
                    getstring(text, p);
                    string name;
                    filtertext(name, text, false);
                    ident *id = getident(name);
                    switch(type)
                    {
                        case Id_Var:
                        {
                            int val = getint(p);
                            if(id && id->flags&Idf_Override && !(id->flags&Idf_ReadOnly))
                            {
                                setvar(name, val);
                            }
                            break;
                        }
                        case Id_FloatVar:
                        {
                            float val = getfloat(p);
                            if(id && id->flags&Idf_Override && !(id->flags&Idf_ReadOnly))
                            {
                                setfvar(name, val);
                            }
                            break;
                        }
                        case Id_StringVar:
                        {
                            getstring(text, p);
                            if(id && id->flags&Idf_Override && !(id->flags&Idf_ReadOnly))
                            {
                                setsvar(name, text);
                            }
                            break;
                        }
                    }
                    printvar(d, id);
                    break;
                }
                case NetMsg_Pong:
                {
                    addmsg(NetMsg_ClientPing, "i", player1->ping = (player1->ping*5+totalmillis-getint(p))/6);
                    break;
                }
                case NetMsg_ClientPing:
                {
                    if(!d)
                    {
                        return;
                    }
                    d->ping = getint(p);
                    break;
                }
                case NetMsg_TimeUp:
                {
                    timeupdate(getint(p));
                    break;
                }
                case NetMsg_ServerMsg:
                {
                    getstring(text, p);
                    conoutf("%s", text);
                    break;
                }
                case NetMsg_SendDemoList:
                {
                    int demos = getint(p);
                    if(demos <= 0)
                    {
                        conoutf("no demos available");
                    }
                    else
                    {
                        for(int i = 0; i < demos; ++i)
                        {
                            getstring(text, p);
                            if(p.overread())
                            {
                                break;
                            }
                            conoutf("%d. %s", i+1, text);
                        }
                    }
                    break;
                }
                case NetMsg_DemoPlayback:
                {
                    int on = getint(p);
                    if(on)
                    {
                        player1->state = ClientState_Spectator;
                    }
                    else
                    {
                        clearclients();
                    }
                    demoplayback = on!=0;
                    player1->clientnum = getint(p);
                    gamepaused = false;
                    checkfollow();
                    execident(on ? "demostart" : "demoend");
                    break;
                }
                case NetMsg_CurrentMaster:
                {
                    int mm = getint(p), //mastermode
                        mn; //master[client]num
                    for(int i = 0; i < players.length(); i++)
                    {
                        players[i]->privilege = Priv_None;
                    }
                    while((mn = getint(p))>=0 && !p.overread())
                    {
                        gameent *m = mn==player1->clientnum ? player1 : newclient(mn);
                        int priv = getint(p);
                        if(m)
                        {
                            m->privilege = priv;
                        }
                    }
                    if(mm != mastermode)
                    {
                        mastermode = mm;
                        conoutf("mastermode is %s (%d)", server::mastermodename(mastermode), mastermode);
                    }
                    break;
                }
                case NetMsg_MasterMode:
                {
                    mastermode = getint(p);
                    conoutf("mastermode is %s (%d)", server::mastermodename(mastermode), mastermode);
                    break;
                }
                case NetMsg_EditMode:
                {
                    int val = getint(p);
                    if(!d)
                    {
                        break;
                    }
                    if(val)
                    {
                        d->editstate = d->state;
                        d->state = ClientState_Editing;
                    }
                    else
                    {
                        d->state = d->editstate;
                        if(d->state==ClientState_Dead)
                        {
                            deathstate(d, true);
                        }
                    }
                    checkfollow();
                    break;
                }
                case NetMsg_Spectator:
                {
                    int sn  = getint(p),
                        val = getint(p);
                    gameent *s;
                    if(sn==player1->clientnum)
                    {
                        s = player1;
                        if(val && remote && !player1->privilege)
                        {
                            senditemstoserver = false;
                        }
                    }
                    else
                    {
                        s = newclient(sn);
                    }
                    if(!s)
                    {
                        return;
                    }
                    if(val)
                    {
                        if(s==player1)
                        {
                            if(editmode)
                            {
                                toggleedit();
                            }
                            if(s->state==ClientState_Dead)
                            {
                                showscores(false);
                            }
                            disablezoom();
                        }
                        s->state = ClientState_Spectator;
                    }
                    else if(s->state==ClientState_Spectator)
                    {
                        deathstate(s, true);
                    }
                    checkfollow();
                    break;
                }
                case NetMsg_SetTeam:
                {
                    int wn = getint(p),
                        team = getint(p),
                        reason = getint(p);
                    gameent *w = getclient(wn);
                    if(!w)
                    {
                        return;
                    }
                    w->team = validteam(team) ? team : 0;
                    static const char * const fmt[2] =
                    {
                        "%s switched to team %s",
                        "%s forced to team %s"
                    };
                    if(reason >= 0 && size_t(reason) < sizeof(fmt)/sizeof(fmt[0]))
                    {
                        conoutf(fmt[reason], colorname(w), teamnames[w->team]);
                    }
                    break;
                }
                /* CTF network messages */
                case NetMsg_InitFlags:
                {
                    ctfmode.parseflags(p, modecheck(gamemode, Mode_CTF));
                    break;
                }

                case NetMsg_DropFlag:
                {
                    int ocn  = getint(p),
                        flag = getint(p),
                        version = getint(p);
                    vec droploc;
                    for(int k = 0; k < 3; ++k)
                    {
                        droploc[k] = getint(p)/DMF;
                    }
                    gameent *o = ocn==player1->clientnum ? player1 : newclient(ocn);
                    if(o && modecheck(gamemode, Mode_CTF))
                    {
                        ctfmode.dropflag(o, flag, version, droploc);
                    }
                    break;
                }

                case NetMsg_ScoreFlag:
                {
                    int ocn = getint(p),
                        relayflag    = getint(p),
                        relayversion = getint(p),
                        goalflag     = getint(p),
                        goalversion  = getint(p),
                        team   = getint(p),
                        score  = getint(p),
                        oflags = getint(p);
                    gameent *o = ocn==player1->clientnum ? player1 : newclient(ocn);
                    if(o && modecheck(gamemode, Mode_CTF))
                    {
                        ctfmode.scoreflag(o, relayflag, relayversion, goalflag, goalversion, team, score, oflags);
                    }
                    break;
                }

                case NetMsg_ReturnFlag:
                {
                    int ocn = getint(p),
                        flag = getint(p),
                        version = getint(p);
                    gameent *o = ocn==player1->clientnum ? player1 : newclient(ocn);
                    if(o && modecheck(gamemode, Mode_CTF))
                    {
                        ctfmode.returnflag(o, flag, version);
                    }
                    break;
                }

                case NetMsg_TakeFlag:
                {
                    int ocn = getint(p),
                        flag = getint(p),
                        version = getint(p);
                    gameent *o = ocn==player1->clientnum ? player1 : newclient(ocn);
                    if(o && modecheck(gamemode, Mode_CTF))
                    {
                        ctfmode.takeflag(o, flag, version);
                    }
                    break;
                }

                case NetMsg_ResetFlag:
                {
                    int flag = getint(p), version = getint(p);
                    if(modecheck(gamemode, Mode_CTF))
                    {
                        ctfmode.resetflag(flag, version);
                    }
                    break;
                }
                /* end of ctf messages */
                case NetMsg_Newmap:
                {
                    int size = getint(p);
                    if(size>=0)
                    {
                        emptymap(size, true, NULL);
                        startmap(NULL);
                    }
                    else
                    {
                        enlargemap(true);
                    }
                    if(d && d!=player1)
                    {
                        int newsize = 0;
                        while(1<<newsize < getworldsize())
                        {
                            newsize++;
                        }
                        conoutf(size>=0 ? "%s started a new map of size %d" : "%s enlarged the map to size %d", colorname(d), newsize);
                    }
                    break;
                }
                case NetMsg_ReqAuth:
                {
                    getstring(text, p);
                    if(autoauth && text[0] && tryauth(text))
                    {
                        conoutf("server requested authkey \"%s\"", text);
                    }
                    break;
                }
                case NetMsg_AuthChallenge:
                {
                    getstring(text, p);
                    authkey *a = findauthkey(text);
                    uint id = static_cast<uint>(getint(p));
                    getstring(text, p);
                    if(a && a->lastauth && lastmillis - a->lastauth < 60*1000)
                    {
                        vector<char> buf;
                        answerchallenge(a->key, text, buf);
                        //conoutf(CON_DEBUG, "answering %u, challenge %s with %s", id, text, buf.getbuf());
                        packetbuf p(maxtrans, ENET_PACKET_FLAG_RELIABLE);
                        putint(p, NetMsg_AuthAnswer);
                        sendstring(a->desc, p);
                        putint(p, id);
                        sendstring(buf.getbuf(), p);
                        sendclientpacket(p.finalize(), 1);
                    }
                    break;
                }
                case NetMsg_InitAI:
                {
                    int bn = getint(p),
                        on = getint(p),
                        at = getint(p),
                        sk = std::clamp(getint(p), 1, 101),
                        pm = getint(p),
                        col = getint(p),
                        team = getint(p);
                    string name;
                    getstring(text, p);
                    filtertext(name, text, false, false, maxnamelength);
                    gameent *b = newclient(bn);
                    if(!b)
                    {
                        break;
                    }
                    ai::init(b, at, on, sk, bn, pm, col, name, team);
                    break;
                }
                case NetMsg_ServerCommand:
                {
                    getstring(text, p);
                    break;
                }
                default:
                {
                    neterr("packet of unknown type", cn < 0);
                    return;
                }
            }
        }
    }

    void receivefile(packetbuf &p)
    {
        int type;
        while(p.remaining())
        {
            switch(type = getint(p))
            {
                case NetMsg_DemoPacket:
                {
                    return;
                }
                case NetMsg_SendDemo:
                {
                    DEF_FORMAT_STRING(fname, "%d.dmo", lastmillis);
                    stream *demo = openrawfile(fname, "wb");
                    if(!demo)
                    {
                        return;
                    }
                    conoutf("received demo \"%s\"", fname);
                    ucharbuf b = p.subbuf(p.remaining());
                    demo->write(b.buf, b.maxlen);
                    delete demo;
                    break;
                }
                case NetMsg_SendMap:
                {
                    if(!modecheck(gamemode, Mode_Edit))
                    {
                        return;
                    }
                    string oldname;
                    copystring(oldname, getclientmap());
                    DEF_FORMAT_STRING(mname, "getmap_%d", lastmillis);
                    DEF_FORMAT_STRING(fname, "media/map/%s.ogz", mname);
                    stream *map = openrawfile(path(fname), "wb");
                    if(!map)
                    {
                        return;
                    }
                    conoutf("received map");
                    ucharbuf b = p.subbuf(p.remaining());
                    map->write(b.buf, b.maxlen);
                    delete map;
                    remove(findfile(fname, "rb"));
                    break;
                }
            }
        }
    }

    void parsepacketclient(int chan, packetbuf &p)   // processes any updates from the server
    {
        if(p.packet->flags&ENET_PACKET_FLAG_UNSEQUENCED)
        {
            return;
        }
        switch(chan)
        {
            case 0:
            {
                parsepositions(p);
                break;
            }
            case 1:
            {
                parsemessages(-1, NULL, p);
                break;
            }
            case 2:
            {
                receivefile(p);
                break;
            }
        }
    }

    void getmap()
    {
        if(!modecheck(gamemode, Mode_Edit))
        {
            conoutf(Console_Error, "\"getmap\" only works in edit mode");
            return;
        }
        conoutf("getting map...");
        addmsg(NetMsg_GetMap, "r");
    }
    COMMAND(getmap, "");

    void stopdemo()
    {
        if(remote)
        {
            if(player1->privilege<Priv_Master)
            {
                return;
            }
            addmsg(NetMsg_StopDemo, "r");
        }
    }
    COMMAND(stopdemo, "");

    void recorddemo(int val)
    {
        if(remote && player1->privilege<Priv_Master)
        {
            return;
        }
        addmsg(NetMsg_RecordDemo, "ri", val);
    }
    ICOMMAND(recorddemo, "i", (int *val), recorddemo(*val));

    void cleardemos(int val)
    {
        if(remote && player1->privilege<Priv_Master)
        {
            return;
        }
        addmsg(NetMsg_ClearDemos, "ri", val);
    }
    ICOMMAND(cleardemos, "i", (int *val), cleardemos(*val));

    void getdemo(int i)
    {
        if(i<=0)
        {
            conoutf("getting demo...");
        }
        else
        {
            conoutf("getting demo %d...", i);
        }
        addmsg(NetMsg_GetDemo, "ri", i);
    }
    ICOMMAND(getdemo, "i", (int *val), getdemo(*val));

    void listdemos()
    {
        conoutf("listing demos...");
        addmsg(NetMsg_ListDemos, "r");
    }
    COMMAND(listdemos, "");

    void sendmap()
    {
        if(!modecheck(gamemode, Mode_Edit) || (player1->state==ClientState_Spectator && remote && !player1->privilege))
        {
            conoutf(Console_Error, "\"sendmap\" only works in coop edit mode");
            return;
        }
        conoutf("sending map...");
        DEF_FORMAT_STRING(mname, "sendmap_%d", lastmillis);
        save_world(mname, game::gameident());
        DEF_FORMAT_STRING(fname, "media/map/%s.ogz", mname);
        stream *map = openrawfile(path(fname), "rb");
        if(map)
        {
            stream::offset len = map->size();
            if(len > 4*1024*1024)
            {
                conoutf(Console_Error, "map is too large");
            }
            else if(len <= 0)
            {
                conoutf(Console_Error, "could not read map");
            }
            else
            {
                sendfile(-1, 2, map);
                if(needclipboard >= 0)
                {
                    needclipboard++;
                }
            }
            delete map;
        }
        else
        {
            conoutf(Console_Error, "could not read map");
        }
        remove(findfile(fname, "rb"));
    }
    COMMAND(sendmap, "");

    void gotoplayer(const char *arg)
    {
        if(player1->state!=ClientState_Spectator && player1->state!=ClientState_Editing)
        {
            return;
        }
        int i = parseplayer(arg);
        if(i>=0)
        {
            gameent *d = getclient(i);
            if(!d || d==player1)
            {
                return;
            }
            player1->o = d->o;
            vec dir;
            vecfromyawpitch(player1->yaw, player1->pitch, 1, 0, dir);
            player1->o.add(dir.mul(-32));
            player1->resetinterp();
        }
    }
    COMMANDN(goto, gotoplayer, "s");

    void gotosel()
    {
        if(player1->state!=ClientState_Editing)
        {
            return;
        }
        player1->o = getselpos();
        vec dir;
        vecfromyawpitch(player1->yaw, player1->pitch, 1, 0, dir);
        player1->o.add(dir.mul(-32));
        player1->resetinterp();
    }
    COMMAND(gotosel, "");
}

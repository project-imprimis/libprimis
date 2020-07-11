#include "game.h"

//for activites that can take place ingame, such as shooting, spectating, etc;
//also for:
//player position extrapolation
//updating the whole world
//player statistics (kills, deaths, accuracy)

namespace game
{
    bool intermission = false;
    int maptime = 0,
        maprealtime = 0,
        maplimit = -1;
    int lasthit = 0,
        lastspawnattempt = 0;

    gameent *player1 = NULL;         // our client
    vector<gameent *> players;       // other clients

    int following = -1;

    VARFP(specmode, 0, 0, 2,
    {
        if(!specmode)
        {
            stopfollowing();
        }
        else if(following < 0)
        {
            nextfollow();
        }
    });

    gameent *followingplayer()
    {
        if(player1->state!=ClientState_Spectator || following<0)
        {
            return NULL;
        }
        gameent *target = getclient(following);
        if(target && target->state!=ClientState_Spectator)
        {
            return target;
        }
        return NULL;
    }

    ICOMMAND(getfollow, "", (),
    {
        gameent *f = followingplayer();
        intret(f ? f->clientnum : -1);
    });

    void stopfollowing()
    {
        if(following<0)
        {
            return;
        }
        following = -1;
    }

    void follow(char *arg)
    {
        int cn = -1;
        if(arg[0])
        {
            if(player1->state != ClientState_Spectator)
            {
                return;
            }
            cn = parseplayer(arg);
            if(cn == player1->clientnum)
            {
                cn = -1;
            }
        }
        if(cn < 0 && (following < 0 || specmode))
        {
            return;
        }
        following = cn;
    }
    COMMAND(follow, "s");

    void nextfollow(int dir)
    {
        if(player1->state!=ClientState_Spectator)
        {
            return;
        }
        int cur = following >= 0 ? following : (dir < 0 ? clients.length() - 1 : 0);
        for(int i = 0; i < clients.length(); i++)
        {
            cur = (cur + dir + clients.length()) % clients.length();
            if(clients[cur] && clients[cur]->state!=ClientState_Spectator)
            {
                following = cur;
                return;
            }
        }
        stopfollowing();
    }
    ICOMMAND(nextfollow, "i", (int *dir), nextfollow(*dir < 0 ? -1 : 1));

    void checkfollow()
    {
        if(player1->state != ClientState_Spectator)
        {
            if(following >= 0)
            {
                stopfollowing();
            }
        }
        else
        {
            if(following >= 0)
            {
                gameent *d = clients.inrange(following) ? clients[following] : NULL;
                if(!d || d->state == ClientState_Spectator)
                {
                    stopfollowing();
                }
            }
            if(following < 0 && specmode)
            {
                nextfollow();
            }
        }
    }

    const char *getclientmap()
    {
        return clientmap;
    }

    gameent *spawnstate(gameent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        d->spawnstate(gamemode);
        return d;
    }

    void respawnself()
    {
        if(ispaused())
        {
            return;
        }
        if(!modecheck(gamemode, Mode_LocalOnly))
        {
            int seq = (player1->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
            if(player1->respawned!=seq)
            {
                addmsg(NetMsg_TrySpawn, "rc", player1);
                player1->respawned = seq;
            }
        }
        else
        {
            spawnplayer(player1);
            showscores(false);
            lasthit = 0;
            if(cmode)
            {
                cmode->respawned(player1);
            }
        }
    }

    gameent *pointatplayer()
    {
        for(int i = 0; i < players.length(); i++)
        {
            if(players[i] != player1 && intersect(players[i], player1->o, worldpos))
            {
                return players[i];
            }
        }
        return NULL;
    }

    gameent *hudplayer()
    {
        if((thirdperson && allowthirdperson()) || specmode > 1)
        {
            return player1;
        }
        gameent *target = followingplayer();
        return target ? target : player1;
    }

    void setupcamera()
    {
        gameent *target = followingplayer();
        if(target)
        {
            player1->yaw = target->yaw;
            player1->pitch = target->state==ClientState_Dead ? 0 : target->pitch;
            player1->o = target->o;
            player1->resetinterp();
        }
    }

    bool allowthirdperson()
    {
        return !multiplayer(false) || player1->state==ClientState_Spectator || player1->state==ClientState_Editing || modecheck(gamemode, Mode_Edit);
    }

    bool detachcamera()
    {
        gameent *d = followingplayer();
        if(d)
        {
            return specmode > 1 || d->state == ClientState_Dead;
        }
        return player1->state == ClientState_Dead;
    }

    bool collidecamera()
    {
        switch(player1->state)
        {
            case ClientState_Editing:
            {
                return false;
            }
            case ClientState_Spectator:
            {
                return followingplayer()!=NULL;
            }
        }
        return true;
    }

// player prediction variables

    VARP(smoothmove, 0, 75, 100); //divisor for smoothing scale
    VARP(smoothdist, 0, 32, 64); //used in game/client.cpp; distance threshold for player position extrapolation

    void predictplayer(gameent *d, bool move)
    {
        d->o = d->newpos;
        d->yaw = d->newyaw;
        d->pitch = d->newpitch;
        d->roll = d->newroll;
        if(move)
        {
            moveplayer(d, 1, false);
            d->newpos = d->o;
        }
        float k = 1.0f - static_cast<float>(lastmillis - d->smoothmillis)/smoothmove;
        if(k>0)
        {
            d->o.add(vec(d->deltapos).mul(k));
            d->yaw += d->deltayaw*k;
            if(d->yaw<0)
            {
                d->yaw += 360;
            }
            else if(d->yaw>=360)
            {
                d->yaw -= 360;
            }
            d->pitch += d->deltapitch*k;
            d->roll += d->deltaroll*k;
        }
    }

    void otherplayers(int curtime)
    {
        for(int i = 0; i < players.length(); i++)
        {
            gameent *d = players[i];
            if(d == player1 || d->ai)
            {
                continue;
            }
            if(d->state==ClientState_Dead && d->ragdoll)
            {
                moveragdoll(d);
            }
            else if(!intermission)
            {
                if(lastmillis - d->lastaction >= d->gunwait)
                {
                    d->gunwait = 0;
                }
            }
            const int lagtime = totalmillis-d->lastupdate;
            if(!lagtime || intermission)
            {
                continue;
            }
            else if(lagtime>1000 && d->state==ClientState_Alive)
            {
                d->state = ClientState_Lagged;
                continue;
            }
            if(d->state==ClientState_Alive || d->state==ClientState_Editing)
            {
                crouchplayer(d, 10, false);
                if(smoothmove && d->smoothmillis>0)
                {
                    predictplayer(d, true);
                }
                else
                {
                    moveplayer(d, 1, false);
                }
            }
            else if(d->state==ClientState_Dead && !d->ragdoll && lastmillis-d->lastpain<2000)
            {
                moveplayer(d, 1, true);
            }
        }
    }

    void updateworld()        // main game update loop
    {
        if(!maptime)
        {
            maptime = lastmillis;
            maprealtime = totalmillis;
            return;
        }
        if(!curtime)
        {
            gets2c();
            if(player1->clientnum>=0)
            {
                c2sinfo();
            }
            return;
        }
        physicsframe();
        ai::navigate();
        updateweapons(curtime); //updates projectiles & bouncers
        otherplayers(curtime);
        ai::update();
        moveragdolls();
        gets2c(); //get server to client info
        if(connected)
        {
            if(player1->state == ClientState_Dead) //ragdoll check
            {
                if(player1->ragdoll)
                {
                    moveragdoll(player1);
                }
                else if(lastmillis-player1->lastpain<2000)
                {
                    player1->move = player1->strafe = 0;
                    moveplayer(player1, 10, true);
                }
            }
            else if(!intermission) //nobody moves when it's intermission (between games)
            {
                if(player1->ragdoll)
                {
                    cleanragdoll(player1);
                }
                crouchplayer(player1, 10, true);
                moveplayer(player1, 10, true);
                swayhudgun(curtime);
                entities::checkitems(player1);
                if(cmode)
                {
                    cmode->checkitems(player1);
                }
            }
        }
        if(player1->clientnum>=0)
        {
            c2sinfo();   // do this last, to reduce the effective frame lag
        }
    }

    void spawnplayer(gameent *d)   // place at random spawn
    {
        if(cmode)
        {
            cmode->pickspawn(d);
        }
        else
        {
            findplayerspawn(d, -1, modecheck(gamemode, Mode_Team) ? d->team : 0);
        }
        spawnstate(d);
        if(d==player1)
        {
            if(editmode)
            {
                d->state = ClientState_Editing;
            }
            else if(d->state != ClientState_Spectator)
            {
                d->state = ClientState_Alive;
            }
        }
        else
        {
            d->state = ClientState_Alive;
        }
        checkfollow();
    }

    VARP(spawnwait, 0, 0, 1000);

    void respawn()
    {
        if(player1->state==ClientState_Dead)
        {
            player1->attacking = Act_Idle;
            int wait = cmode ? cmode->respawnwait(player1) : 0;
            if(wait>0)
            {
                lastspawnattempt = lastmillis;
                //conoutf(ConsoleMsg_GameInfo, "\f2you must wait %d second%s before respawn!", wait, wait!=1 ? "s" : "");
                return;
            }
            if(lastmillis < player1->lastpain + spawnwait)
            {
                return;
            }
            respawnself();
        }
    }
    COMMAND(respawn, "");

    // inputs
    VARP(attackspawn, 0, 1, 1);

    void doaction(int act)
    {
        if(!connected || intermission)
        {
            return;
        }
        if((player1->attacking = act) && attackspawn)
        {
            respawn();
        }
    }

    ICOMMAND(shoot, "D", (int *down), doaction(*down ? Act_Shoot : Act_Idle));
    ICOMMAND(melee, "D", (int *down), doaction(*down ? Act_Melee : Act_Idle));

    VARP(jumpspawn, 0, 1, 1);

    bool canjump()
    {
        if(!connected || intermission)
        {
            return false;
        }
        if(jumpspawn)
        {
            respawn();
        }
        return player1->state!=ClientState_Dead;
    }

    bool cancrouch()
    {
        if(!connected || intermission)
        {
            return false;
        }
        return player1->state!=ClientState_Dead;
    }

    bool allowmove(physent *d)
    {
        if(d->type!=PhysEnt_Player)
        {
            return true;
        }
        return !((gameent *)d)->lasttaunt || lastmillis-((gameent *)d)->lasttaunt>=1000;
    }

    void taunt()
    {
        if(player1->state!=ClientState_Alive || player1->physstate<PhysEntState_Slope)
        {
            return;
        }
        if(lastmillis-player1->lasttaunt<1000)
        {
            return;
        }
        player1->lasttaunt = lastmillis;
        addmsg(NetMsg_Taunt, "rc", player1);
    }
    COMMAND(taunt, "");

    VARP(hitsound, 0, 0, 1);

    void damaged(int damage, gameent *d, gameent *actor, bool local)
    {
        if((d->state!=ClientState_Alive && d->state != ClientState_Lagged && d->state != ClientState_Spawning) || intermission)
        {
            return;
        }
        if(local)
        {
            damage = d->dodamage(damage);
        }
        else if(actor==player1)
        {
            return;
        }
        gameent *h = hudplayer();
        if(h!=player1 && actor==h && d!=actor)
        {
            if(hitsound && lasthit != lastmillis)
            {
                playsound(Sound_Hit);
            }
            lasthit = lastmillis;
        }
        if(d==h)
        {
            damageblend(damage);
            damagecompass(damage, actor->o);
        }
        damageeffect(damage, d, d!=h);

        ai::damaged(d, actor);

        if(d->health<=0)
        {
            if(local)
            {
                killed(d, actor);
            }
        }
        else if(d==h)
        {
            playsound(Sound_Pain2);
        }
        else
        {
            playsound(Sound_Pain1, &d->o);
        }
    }

    VARP(deathscore, 0, 1, 1);

    void deathstate(gameent *d, bool restore)
    {
        d->state = ClientState_Dead;
        d->lastpain = lastmillis;
        if(!restore)
        {
            d->deaths++;
        }
        if(d==player1)
        {
            if(deathscore)
            {
                showscores(true);
            }
            disablezoom();
            d->attacking = Act_Idle;
            //d->pitch = 0;
            d->roll = 0;
            playsound(Sound_Die2);
        }
        else
        {
            d->move = d->strafe = 0;
            d->resetinterp();
            d->smoothmillis = 0;
            playsound(Sound_Die1, &d->o);
        }
    }

    VARP(teamcolorfrags, 0, 1, 1);

    void killed(gameent *d, gameent *actor)
    {
        if(d->state==ClientState_Editing)
        {
            d->editstate = ClientState_Dead;
            d->deaths++;
            if(d!=player1)
            {
                d->resetinterp();
            }
            return;
        }
        else if((d->state!=ClientState_Alive && d->state != ClientState_Lagged && d->state != ClientState_Spawning) || intermission)
        {
            return;
        }
        gameent *h = followingplayer();
        if(!h)
        {
            h = player1;
        }
        int contype = d==h || actor==h ? ConsoleMsg_FragSelf : ConsoleMsg_FragOther;
        const char *dname = "", *aname = "";
        if(modecheck(gamemode, Mode_Team) && teamcolorfrags)
        {
            dname = teamcolorname(d, "you");
            aname = teamcolorname(actor, "you");
        }
        else
        {
            dname = colorname(d, NULL, "you");
            aname = colorname(actor, NULL, "you");
        }
        if(d==actor)
        {
            conoutf(contype, "\f2%s suicided%s", dname, d==player1 ? "!" : "");
        }
        else if(modecheck(gamemode, Mode_Team) && (d->team == actor->team)) //if player is on the same team in a team mode
        {
            contype |= ConsoleMsg_TeamKill;
            if(actor==player1)
            {
                conoutf(contype, "\f6%s fragged a teammate (%s)", aname, dname);
            }
            else if(d==player1)
            {
                conoutf(contype, "\f6%s got fragged by a teammate (%s)", dname, aname);
            }
            else
            {
                conoutf(contype, "\f2%s fragged a teammate (%s)", aname, dname);
            }
        }
        else
        {
            if(d==player1)
            {
                conoutf(contype, "\f2%s got fragged by %s", dname, aname);
            }
            else
            {
                conoutf(contype, "\f2%s fragged %s", aname, dname);
            }
        }
        deathstate(d);
        ai::killed(d, actor);
    }

    void timeupdate(int secs)
    {
        if(secs > 0)
        {
            maplimit = lastmillis + secs*1000;
        }
        else
        {
            intermission = true;
            player1->attacking = Act_Idle;
            if(cmode)
            {
                cmode->gameover();
            }
            conoutf(ConsoleMsg_GameInfo, "\f2intermission:");
            conoutf(ConsoleMsg_GameInfo, "\f2game has ended!");
            if(modecheck(gamemode, Mode_CTF))
            {
                conoutf(ConsoleMsg_GameInfo, "\f2player frags: %d, flags: %d, deaths: %d", player1->frags, player1->flags, player1->deaths);
            }
            else
            {
                conoutf(ConsoleMsg_GameInfo, "\f2player frags: %d, deaths: %d", player1->frags, player1->deaths);
            }
            int accuracy = (player1->totaldamage*100)/max(player1->totalshots, 1);
            conoutf(ConsoleMsg_GameInfo, "\f2player total damage dealt: %d, damage wasted: %d, accuracy(%%): %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);
            showscores(true);
            disablezoom();
            execident("intermission");
        }
    }

    ICOMMAND(getfrags, "", (), intret(player1->frags));
    ICOMMAND(getflags, "", (), intret(player1->flags));
    ICOMMAND(getdeaths, "", (), intret(player1->deaths));
    ICOMMAND(getaccuracy, "", (), intret((player1->totaldamage*100)/max(player1->totalshots, 1)));
    ICOMMAND(gettotaldamage, "", (), intret(player1->totaldamage));
    ICOMMAND(gettotalshots, "", (), intret(player1->totalshots));

    vector<gameent *> clients;

    gameent *newclient(int cn)   // ensure valid entity
    {
        if(cn < 0 || cn > max(0xFF, MAXCLIENTS + maxbots))
        {
            neterr("clientnum", false);
            return NULL;
        }
        if(cn == player1->clientnum)
        {
            return player1;
        }
        while(cn >= clients.length())
        {
            clients.add(NULL);
        }
        if(!clients[cn])
        {
            gameent *d = new gameent;
            d->clientnum = cn;
            clients[cn] = d;
            players.add(d);
        }
        return clients[cn];
    }

    gameent *getclient(int cn)   // ensure valid entity
    {
        if(cn == player1->clientnum)
        {
            return player1;
        }
        return clients.inrange(cn) ? clients[cn] : NULL;
    }

    void clientdisconnected(int cn, bool notify)
    {
        if(!clients.inrange(cn))
        {
            return;
        }
        unignore(cn);
        gameent *d = clients[cn];
        if(d)
        {
            if(notify && d->name[0])
            {
                conoutf("\f4leave:\f7 %s", colorname(d));
            }
            removeweapons(d);
            removetrackedparticles(d);
            removetrackeddynlights(d);
            if(cmode)
            {
                cmode->removeplayer(d);
            }
            removegroupedplayer(d);
            players.removeobj(d);
            DELETEP(clients[cn]);
            cleardynentcache();
        }
        if(following == cn)
        {
            if(specmode)
            {
                nextfollow();
            }
            else
            {
                stopfollowing();
            }
        }
    }

    void clearclients(bool notify)
    {
        for(int i = 0; i < clients.length(); i++)
        {
            if(clients[i])
            {
                clientdisconnected(i, notify);
            }
        }
    }

    void initclient()
    {
        player1 = spawnstate(new gameent);
        filtertext(player1->name, "unnamed", false, false, MAXNAMELEN);
        players.add(player1);
    }

    VARP(showmodeinfo, 0, 1, 1);

    void startgame()
    {
        clearprojectiles();
        clearbouncers();
        clearragdolls();
        clearteaminfo();
        // reset perma-state
        for(int i = 0; i < players.length(); i++)
        {
            players[i]->startgame();
        }
        setclientmode();
        intermission = false;
        maptime = maprealtime = 0;
        maplimit = -1;
        if(cmode)
        {
            cmode->preload();
            cmode->setup();
        }
        conoutf(ConsoleMsg_GameInfo, "\f2game mode is %s", server::modeprettyname(gamemode));
        const char *info = MODE_VALID(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
        if(showmodeinfo && info)
        {
            conoutf(ConsoleMsg_GameInfo, "\f0%s", info);
        }
        syncplayer();
        showscores(false);
        disablezoom();
        lasthit = 0;
        execident("mapstart");
        execident("resethud"); //reconfigure hud
    }

    void startmap(const char *name)   // called just after a map load
    {
        ai::savewaypoints();
        ai::clearwaypoints(true);
        if(modecheck(gamemode, Mode_LocalOnly))
        {
            spawnplayer(player1);
        }
        else
        {
            findplayerspawn(player1, -1, modecheck(gamemode, Mode_Team) ? player1->team : 0);
        }
        entities::resetspawns();
        copystring(clientmap, name ? name : "");

        sendmapinfo();
    }

    const char *getmapinfo()
    {
        return showmodeinfo && MODE_VALID(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
    }

    const char *getscreenshotinfo()
    {
        return server::modename(gamemode, NULL);
    }

    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
    {
        if(waterlevel>0)
        {
            playsound(Sound_SplashOut, d==player1 ? NULL : &d->o);
        }
        else if(waterlevel<0)
        {
            playsound(Sound_SplashIn, d==player1 ? NULL : &d->o);
        }
        if(floorlevel>0)
        {
            if(d==player1 || d->type!=PhysEnt_Player || ((gameent *)d)->ai)
            {
                msgsound(Sound_Jump, d);
            }
        }
        else if(floorlevel<0)
        {
            if(d==player1 || d->type!=PhysEnt_Player || ((gameent *)d)->ai)
            {
                msgsound(Sound_Land, d);
            }
        }
    }

    void msgsound(int n, physent *d)
    {
        if(!d || d==player1)
        {
            addmsg(NetMsg_Sound, "ci", d, n);
            playsound(n);
        }
        else
        {
            if(d->type==PhysEnt_Player && ((gameent *)d)->ai)
            {
                addmsg(NetMsg_Sound, "ci", d, n);
            }
            playsound(n, &d->o);
        }
    }

    int numdynents() { return players.length(); }

    dynent *iterdynents(int i)
    {
        if(i<players.length())
        {
            return players[i];
        }
        return NULL;
    }

    bool duplicatename(gameent *d, const char *name = NULL, const char *alt = NULL)
    {
        if(!name)
        {
            name = d->name;
        }
        if(alt && d != player1 && !strcmp(name, alt))
        {
            return true;
        }
        for(int i = 0; i < players.length(); i++)
        {
            if(d!=players[i] && !strcmp(name, players[i]->name))
            {
                return true;
            }
        }
        return false;
    }

    const char *colorname(gameent *d, const char *name, const char * alt, const char *color)
    {
        if(!name)
        {
            name = alt && d == player1 ? alt : d->name;
        }
        bool dup = !name[0] || duplicatename(d, name, alt) || d->aitype != AI_None;
        if(dup || color[0])
        {
            if(dup)
            {
                return tempformatstring(d->aitype == AI_None ? "\fs%s%s \f5(%d)\fr" : "\fs%s%s \f5[%d]\fr", color, name, d->clientnum);
            }
            return tempformatstring("\fs%s%s\fr", color, name);
        }
        return name;
    }

    VARP(teamcolortext, 0, 1, 1);

    const char *teamcolorname(gameent *d, const char *alt)
    {
        if(!teamcolortext || modecheck(gamemode, Mode_Team) || !VALID_TEAM(d->team) || d->state == ClientState_Spectator)
        {
            return colorname(d, NULL, alt);
        }
        return colorname(d, NULL, alt, teamtextcode[d->team]);
    }

    const char *teamcolor(const char *prefix, const char *suffix, int team, const char *alt)
    {
        if(!teamcolortext || !Mode_Team || !VALID_TEAM(team))
        {
            return alt;
        }
        return tempformatstring("\fs%s%s%s%s\fr", teamtextcode[team], prefix, teamnames[team], suffix);
    }

    VARP(teamsounds, 0, 1, 1);

    void teamsound(bool sameteam, int n, const vec *loc)
    {
        playsound(n, loc, NULL, teamsounds ? (Mode_Team && sameteam ? Music_UseAlt : Music_NoAlt) : 0);
    }

    void teamsound(gameent *d, int n, const vec *loc)
    {
        teamsound((modecheck(gamemode, Mode_Team) && d->team == player1->team), n, loc);
    }

    void suicide(physent *d)
    {
        if(d==player1 || (d->type==PhysEnt_Player && ((gameent *)d)->ai))
        {
            if(d->state!=ClientState_Alive)
            {
                return;
            }
            gameent *pl = (gameent *)d;
            if(modecheck(gamemode, Mode_LocalOnly))
            {
                killed(pl, pl);
            }
            else
            {
                int seq = (pl->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
                if(pl->suicided!=seq)
                {
                    addmsg(NetMsg_Suicide, "rc", pl);
                    pl->suicided = seq;
                }
            }
        }
    }
    ICOMMAND(suicide, "", (), suicide(player1));

    bool needminimap()
    {
        return modecheck(gamemode, Mode_CTF);
    }

    void drawicon(int icon, float x, float y, float sz)
    {
        settexture("media/interface/hud/items.png");
        float tsz = 0.25f,
              tx = tsz*(icon%4),
              ty = tsz*(icon/4);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,    y);    gle::attribf(tx,     ty);
        gle::attribf(x+sz, y);    gle::attribf(tx+tsz, ty);
        gle::attribf(x,    y+sz); gle::attribf(tx,     ty+tsz);
        gle::attribf(x+sz, y+sz); gle::attribf(tx+tsz, ty+tsz);
        gle::end();
    }

    float abovegameplayhud(int w, int h)
    {
        switch(hudplayer()->state)
        {
            case ClientState_Editing:
            case ClientState_Spectator:
            {
                return 1;
            }
            default:
            {
                return 1650.0f/1800.0f;
            }
        }
    }

    void drawhudicons(gameent *d)
    {

    }

    void gameplayhud(int w, int h)
    {
        pushhudscale(h/1800.0f);

        if(player1->state==ClientState_Spectator)
        {
            float pw, ph, tw, th, fw, fh;
            text_boundsf("  ", pw, ph);
            text_boundsf("SPECTATOR", tw, th);
            th = max(th, ph);
            gameent *f = followingplayer();
            text_boundsf(f ? colorname(f) : " ", fw, fh);
            fh = max(fh, ph);
            draw_text("SPECTATOR", w*1800/h - tw - pw, 1650 - th - fh);
            if(f)
            {
                int color = f->state!=ClientState_Dead ? 0xFFFFFF : 0x606060;
                if(f->privilege)
                {
                    color = f->privilege>=Priv_Admin ? 0xFF8000 : 0x40FF80;
                    if(f->state==ClientState_Dead)
                    {
                        color = (color>>1)&0x7F7F7F;
                    }
                }
                draw_text(colorname(f), w*1800/h - fw - pw, 1650 - fh, (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF);
            }
            resethudshader();
        }

        gameent *d = hudplayer();
        if(d->state!=ClientState_Editing)
        {
            if(d->state!=ClientState_Spectator)
            {
                drawhudicons(d);
            }
            if(cmode)
            {
                cmode->drawhud(d, w, h);
            }
        }

        pophudmatrix();
    }

    float clipconsole(float w, float h)
    {
        if(cmode)
        {
            return cmode->clipconsole(w, h);
        }
        return 0;
    }

    VARP(teamcrosshair, 0, 1, 1);
    VARP(hitcrosshair, 0, 425, 1000);

    const char *defaultcrosshair(int index)
    {
        switch(index)
        {
            case 2:
            {
                return "media/interface/crosshair/default_hit.png";
            }
            case 1:
            {
                return "media/interface/crosshair/teammate.png";
            }
            default:
            {
                return "media/interface/crosshair/default.png";
            }
        }
    }

    int selectcrosshair(vec &col)
    {
        gameent *d = hudplayer();
        if(d->state==ClientState_Spectator || d->state==ClientState_Dead || UI::uivisible("scoreboard"))
        {
            return -1;
        }
        if(d->state!=ClientState_Alive)
        {
            return 0;
        }
        int crosshair = 0;
        if(lasthit && lastmillis - lasthit < hitcrosshair)
        {
            crosshair = 2;
        }
        else if(teamcrosshair && Mode_Team)
        {
            dynent *o = intersectclosest(d->o, worldpos, d);
            if(o && o->type==PhysEnt_Player && VALID_TEAM(d->team) && ((gameent *)o)->team == d->team)
            {
                crosshair = 1;
                col = vec::hexcolor(teamtextcolor[d->team]);
            }
        }


        if(d->gunwait)
        {
            col.mul(0.5f);
        }
        return crosshair;
    }

    const char *mastermodecolor(int n, const char *unknown)
    {
        return (n>=MasterMode_Start && size_t(n-MasterMode_Start)<sizeof(mastermodecolors)/sizeof(mastermodecolors[0])) ? mastermodecolors[n-MasterMode_Start] : unknown;
    }

    const char *mastermodeicon(int n, const char *unknown)
    {
        return (n>=MasterMode_Start && size_t(n-MasterMode_Start)<sizeof(mastermodeicons)/sizeof(mastermodeicons[0])) ? mastermodeicons[n-MasterMode_Start] : unknown;
    }

    ICOMMAND(servinfomode, "i", (int *i), GETSERVINFOATTR(*i, 0, mode, intret(mode)));
    ICOMMAND(servinfomodename, "i", (int *i),
        GETSERVINFOATTR(*i, 0, mode,
        {
            const char *name = server::modeprettyname(mode, NULL);
            if(name)
            {
                result(name);
            }
        }));
    ICOMMAND(servinfomastermode, "i", (int *i), GETSERVINFOATTR(*i, 2, mm, intret(mm)));
    ICOMMAND(servinfomastermodename, "i", (int *i),
        GETSERVINFOATTR(*i, 2, mm,
        {
            const char *name = server::mastermodename(mm, NULL);
            if(name)
            {
                stringret(newconcatstring(mastermodecolor(mm, ""), name));
            }
        }));
    ICOMMAND(servinfotime, "ii", (int *i, int *raw),
        GETSERVINFOATTR(*i, 1, secs,
        {
            secs = clamp(secs, 0, 59*60+59);
            if(*raw)
            {
                intret(secs);
            }
            else
            {
                int mins = secs/60;
                secs %= 60;
                result(tempformatstring("%d:%02d", mins, secs));
            }
        }));
    ICOMMAND(servinfoicon, "i", (int *i),
        GETSERVINFO(*i, si,
        {
            int mm = si->attr.inrange(2) ? si->attr[2] : MasterMode_Invalid;
            result(si->maxplayers > 0 && si->numplayers >= si->maxplayers ? "serverfull" : mastermodeicon(mm, "serverunk"));
        }));

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}

    const char *gameconfig()    { return "config/game.cfg"; }
    const char *savedconfig()   { return "config/saved.cfg"; }
    const char *restoreconfig() { return "config/restore.cfg"; }
    const char *defaultconfig() { return "config/default.cfg"; }
    const char *autoexec()      { return "config/autoexec.cfg"; }
    const char *savedservers()  { return "config/servers.cfg"; }

    void loadconfigs()
    {
        execfile("config/auth.cfg", false);
    }

    bool clientoption(const char *arg) { return false; }
}


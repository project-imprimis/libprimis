#ifndef PARSEMESSAGES

#ifdef SERVERMODE
VAR(ctftkpenalty, 0, 1, 1);

struct ctfservermode : servermode
#else
struct ctfclientmode : clientmode
#endif
{
    static const int MAXFLAGS = 20;
    static const int FLAGRADIUS = 16;
    static const int FLAGLIMIT = 10;
    static const int RESPAWNSECS = 5;

    struct flag
    {
        int id, version;
        vec droploc, spawnloc;
        int team, droptime, owntime;
#ifdef SERVERMODE
        int owner, dropcount, dropper;
#else
        gameent *owner;
        float dropangle, spawnangle;
        vec interploc;
        float interpangle;
        int interptime;
#endif

        flag() : id(-1) { reset(); }

        void reset()
        {
            version = 0;
            droploc = spawnloc = vec(0, 0, 0);
#ifdef SERVERMODE
            dropcount = 0;
            owner = dropper = -1;
            owntime = 0;
#else
            if(id >= 0)
            {
                for(int i = 0; i < players.length(); i++)
                {
                    players[i]->flagpickup &= ~(1<<id);
                }
            }
            owner = NULL;
            dropangle = spawnangle = 0;
            interploc = vec(0, 0, 0);
            interpangle = 0;
            interptime = 0;
#endif
            team = 0;
            droptime = owntime = 0;
        }

#ifndef SERVERMODE
        vec pos() const
        {
            if(owner)
            {
                return vec(owner->o).sub(owner->eyeheight);
            }
            if(droptime)
            {
                return droploc;
            }
            return spawnloc;
        }
#endif
    };

    vector<flag> flags;
    int scores[MAXTEAMS];

    void resetflags()
    {
        flags.shrink(0);
        for(int k = 0; k < MAXTEAMS; ++k)
        {
            scores[k] = 0;
        }
    }

#ifdef SERVERMODE
    bool addflag(int i, const vec &o, int team)
#else
    bool addflag(int i, const vec &o, int team)
#endif
    {
        if(i<0 || i>=MAXFLAGS)
        {
            return false;
        }
        while(flags.length()<=i)
        {
            flags.add();
        }
        flag &f = flags[i];
        f.id = i;
        f.reset();
        f.team = team;
        f.spawnloc = o;
        return true;
    }

#ifdef SERVERMODE
    void ownflag(int i, int owner, int owntime)
#else
    void ownflag(int i, gameent *owner, int owntime)
#endif
    {
        flag &f = flags[i];
        f.owner = owner;
        f.owntime = owntime;
#ifdef SERVERMODE
        if(owner == f.dropper)
        {
            if(f.dropcount < INT_MAX)
            {
                f.dropcount++;
            }
        }
        else
        {
            f.dropcount = 0;
        }
        f.dropper = -1;
#else
        for(int i = 0; i < players.length(); i++)
        {
            players[i]->flagpickup &= ~(1<<f.id);
        }
#endif
    }

#ifdef SERVERMODE
    void dropflag(int i, const vec &o, int droptime, int dropper = -1, bool penalty = false)
#else
    void dropflag(int i, const vec &o, float yaw, int droptime)
#endif
    {
        flag &f = flags[i];
        f.droploc = o;
        f.droptime = droptime;
#ifdef SERVERMODE
        if(dropper < 0)
        {
            f.dropcount = 0;
        }
        else if(penalty)
        {
            f.dropcount = INT_MAX;
        }
        f.dropper = dropper;
        f.owner = -1;
#else
        for(int i = 0; i < players.length(); i++)
        {
            players[i]->flagpickup &= ~(1<<f.id);
        }
        f.owner = NULL;
        f.dropangle = yaw;
#endif
    }

#ifdef SERVERMODE
    void returnflag(int i)
#else
    void returnflag(int i)
#endif
    {
        flag &f = flags[i];
        f.droptime = 0;
#ifdef SERVERMODE
        f.dropcount = 0;
        f.owner = f.dropper = -1;
#else
        for(int i = 0; i < players.length(); i++)
        {
            players[i]->flagpickup &= ~(1<<f.id);
        }
        f.owner = NULL;
#endif
    }

    int totalscore(int team)
    {
        return VALID_TEAM(team) ? scores[team-1] : 0;
    }

    int setscore(int team, int score)
    {
        if(VALID_TEAM(team)) return scores[team-1] = score;
        return 0;
    }

    int addscore(int team, int score)
    {
        if(VALID_TEAM(team)) return scores[team-1] += score;
        return 0;
    }

    bool hidefrags() { return true; }

    int getteamscore(int team)
    {
        return totalscore(team);
    }

    void getteamscores(vector<teamscore> &tscores)
    {
        for(int k = 0; k < MAXTEAMS; ++k)
        {
            if(scores[k])
            {
                tscores.add(teamscore(k+1, scores[k]));
            }
        }
    }

#ifdef SERVERMODE
    static const int RESETFLAGTIME = 10000;

    bool notgotflags;

    ctfservermode() : notgotflags(false) {}

    void reset(bool empty)
    {
        resetflags();
        notgotflags = !empty;
    }

    void cleanup()
    {
        reset(false);
    }

    void setup()
    {
        reset(false);
        if(notgotitems || ments.empty())
        {
            return;
        }
        for(int i = 0; i < ments.length(); i++)
        {
            entity &e = ments[i];
            if(e.type != GamecodeEnt_Flag || !VALID_TEAM(e.attr2))
            {
                continue;
            }
            if(!addflag(flags.length(), e.o, e.attr2))
            {
                break;
            }
        }
        notgotflags = false;
    }

    void newmap()
    {
        reset(true);
    }

    void dropflag(clientinfo *ci, clientinfo *dropper = NULL)
    {
        if(notgotflags)
        {
            return;
        }
        for(int i = 0; i < flags.length(); i++)
        {
            if(flags[i].owner==ci->clientnum)
            {
                flag &f = flags[i];
                ivec o(vec(ci->state.o).mul(DMF));
                sendf(-1, 1, "ri7", NetMsg_DropFlag, ci->clientnum, i, ++f.version, o.x, o.y, o.z);
                dropflag(i, vec(o).div(DMF), lastmillis, dropper ? dropper->clientnum : ci->clientnum, dropper && dropper!=ci);
            }
        }
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        dropflag(ci);
        for(int i = 0; i < flags.length(); i++)
        {
            if(flags[i].dropper == ci->clientnum)
            {
                flags[i].dropper = -1;
                flags[i].dropcount = 0;
            }
        }
    }

    void died(clientinfo *ci, clientinfo *actor)
    {
        dropflag(ci, ctftkpenalty && actor && actor != ci && modecheck(gamemode, Mode_Team) && actor->team == ci->team ? actor : NULL);
        for(int i = 0; i < flags.length(); i++)
        {
            if(flags[i].dropper == ci->clientnum)
            {
                flags[i].dropper = -1;
                flags[i].dropcount = 0;
            }
        }
    }

    bool canspawn(clientinfo *ci, bool connecting)
    {
        return connecting || !ci->state.lastdeath || gamemillis+curtime-ci->state.lastdeath >= RESPAWNSECS*1000;
    }

    bool canchangeteam(clientinfo *ci, int oldteam, int newteam)
    {
        return true;
    }

    void changeteam(clientinfo *ci, int oldteam, int newteam)
    {
        dropflag(ci);
    }

    void scoreflag(clientinfo *ci, int goal, int relay = -1)
    {
        returnflag(relay >= 0 ? relay : goal);
        ci->state.flags++;
        int team = ci->team, score = addscore(team, 1);
        sendf(-1, 1, "ri9", NetMsg_ScoreFlag, ci->clientnum, relay, relay >= 0 ? ++flags[relay].version : -1, goal, ++flags[goal].version, team, score, ci->state.flags);
        if(score >= FLAGLIMIT) startintermission();
    }

    void takeflag(clientinfo *ci, int i, int version)
    {
        if(notgotflags || !flags.inrange(i) || ci->state.state!=ClientState_Alive || !ci->team)
        {
            return;
        }
        flag &f = flags[i];
        if(!VALID_TEAM(f.team) || f.owner>=0 || f.version != version || (f.droptime && f.dropper == ci->clientnum && f.dropcount >= 3))
        {
            return;
        }
        if(f.team!=ci->team)
        {
            for(int j = 0; j < flags.length(); j++)
            {
                if(flags[j].owner==ci->clientnum)
                {
                    return;
                }
            }
            ownflag(i, ci->clientnum, lastmillis);
            sendf(-1, 1, "ri4", NetMsg_TakeFlag, ci->clientnum, i, ++f.version);
        }
        else if(f.droptime)
        {
            returnflag(i);
            sendf(-1, 1, "ri4", NetMsg_ReturnFlag, ci->clientnum, i, ++f.version);
        }
        else
        {
            for(int j = 0; j < flags.length(); j++)
            {
                if(flags[j].owner==ci->clientnum)
                {
                    scoreflag(ci, i, j);
                    break;
                }
            }
        }
    }

    void update()
    {
        if(gamemillis>=gamelimit || notgotflags)
        {
            return;
        }
        for(int i = 0; i < flags.length(); i++)
        {
            flag &f = flags[i];
            if(f.owner<0 && f.droptime && lastmillis - f.droptime >= RESETFLAGTIME)
            {
                returnflag(i);
                sendf(-1, 1, "ri3", NetMsg_ResetFlag, i, ++f.version);
            }
        }
    }

    void initclient(clientinfo *ci, packetbuf &p, bool connecting)
    {
        putint(p, NetMsg_InitFlags);
        for(int k = 0; k < 2; ++k)
        {
            putint(p, scores[k]);
        }
        putint(p, flags.length());
        for(int i = 0; i < flags.length(); i++)
        {
            flag &f = flags[i];
            putint(p, f.version);
            putint(p, f.owner);
            if(f.owner<0)
            {
                putint(p, f.droptime ? 1 : 0);
                if(f.droptime)
                {
                    putint(p, static_cast<int>(f.droploc.x*DMF));
                    putint(p, static_cast<int>(f.droploc.y*DMF));
                    putint(p, static_cast<int>(f.droploc.z*DMF));
                }
            }
        }
    }

    void parseflags(ucharbuf &p, bool commit)
    {
        int numflags = getint(p);
        for(int i = 0; i < numflags; ++i)
        {
            int team = getint(p);
            vec o;
            for(int k = 0; k < 3; ++k)
            {
                o[k] = max(getint(p)/DMF, 0.0f);
            }
            if(p.overread())
            {
                break;
            }
            if(commit && notgotflags)
            {
                addflag(i, o, team);
            }
        }
        if(commit && notgotflags)
        {
            notgotflags = false;
        }
    }
};
#else
    #define FLAGCENTER 3.5f
    #define FLAGFLOAT 7

    void preload()
    {
        preloadmodel("game/flag/rojo");
        preloadmodel("game/flag/azul");
        for(int i = Sound_FlagPickup; i <= Sound_FlagFail; i++) preloadsound(i);
    }

    void drawblip(gameent *d, float x, float y, float s, const vec &pos, bool flagblip)
    {
        float scale = calcradarscale();
        vec dir = d->o;
        dir.sub(pos).div(scale);
        float size = flagblip ? 0.1f : 0.05f,
              xoffset = flagblip ? -2*(3/32.0f)*size : -size,
              yoffset = flagblip ? -2*(1 - 3/32.0f)*size : -size,
              dist = dir.magnitude2(), maxdist = 1 - 0.05f - 0.05f;
        if(dist >= maxdist) dir.mul(maxdist/dist);
        dir.rotate_around_z(camera1->yaw*-RAD);
        drawradar(x + s*0.5f*(1.0f + dir.x + xoffset), y + s*0.5f*(1.0f + dir.y + yoffset), size*s);
    }

    void drawblip(gameent *d, float x, float y, float s, int i, bool flagblip)
    {
        flag &f = flags[i];
        setbliptex(f.team, flagblip ? "_flag" : "");
        drawblip(d, x, y, s, flagblip ? (f.owner ? f.owner->o : (f.droptime ? f.droploc : f.spawnloc)) : f.spawnloc, flagblip);
    }

    float clipconsole(float w, float h)
    {
        return (h*(1 + 1 + 10))/(4*10);
    }

    void drawhud(gameent *d, int w, int h)
    {
        if(d->state == ClientState_Alive)
        {
            for(int i = 0; i < flags.length(); i++)
            {
                if(flags[i].owner == d)
                {
                    float x = 1800*w/h*0.5f-HudIcon_Size/2,
                          y = 1800*0.95f-HudIcon_Size/2;
                    drawicon(flags[i].team==1 ? HudIcon_BlueFlag : HudIcon_RedFlag, x, y);
                    break;
                }
            }
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        int s = 1800/4,
            x = 1800*w/h - s - s/10,
            y = s/10;
        gle::colorf(1, 1, 1, minimapalpha);
        if(minimapalpha >= 1)
        {
            glDisable(GL_BLEND);
        }
        bindminimap();
        drawminimap(d, x, y, s);
        if(minimapalpha >= 1)
        {
            glEnable(GL_BLEND);
        }
        gle::colorf(1, 1, 1);
        float margin = 0.04f,
              roffset = s*margin,
              rsize = s + 2*roffset;
        setradartex();
        drawradar(x - roffset, y - roffset, rsize);
        settexture("media/interface/radar/compass.png", 3);
        pushhudmatrix();
        hudmatrix.translate(x - roffset + 0.5f*rsize, y - roffset + 0.5f*rsize, 0);
        hudmatrix.rotate_around_z((camera1->yaw + 180)*-RAD);
        flushhudmatrix();
        drawradar(-0.5f*rsize, -0.5f*rsize, rsize);
        pophudmatrix();
        drawplayerblip(d, x, y, s, 1.5f);
        for(int i = 0; i < flags.length(); i++)
        {
            flag &f = flags[i];
            if(!VALID_TEAM(f.team))
            {
                continue;
            }
            if(f.owner)
            {
                if(lastmillis%1000 >= 500) continue;
            }
            else if(f.droptime && (f.droploc.x < 0 || lastmillis%300 >= 150)) continue;
            drawblip(d, x, y, s, i, true);
        }
        drawteammates(d, x, y, s);
        if(d->state == ClientState_Dead)
        {
            int wait = respawnwait(d);
            if(wait>=0)
            {
                pushhudscale(2);
                bool flash = wait>0 && d==player1 && lastspawnattempt>=d->lastpain && lastmillis < lastspawnattempt+100;
                draw_textf("%s%d", (x+s/2)/2-(wait>=10 ? 28 : 16), (y+s/2)/2-32, flash ? "\f3" : "", wait);
                pophudmatrix();
                resethudshader();
            }
        }
    }

    void removeplayer(gameent *d)
    {
        for(int i = 0; i < flags.length(); i++)
        {
            if(flags[i].owner == d)
            {
                flag &f = flags[i];
                f.interploc.x = -1;
                f.interptime = 0;
                dropflag(i, f.owner->o, f.owner->yaw, 1);
            }
        }
    }

    vec interpflagpos(flag &f, float &angle)
    {
        vec pos = f.owner ? vec(f.owner->abovehead()).addz(1) : (f.droptime ? f.droploc : f.spawnloc);
        if(f.owner)
        {
            angle = f.owner->yaw;
        }
        else
        {
            angle = f.droptime ? f.dropangle : f.spawnangle;
            pos.addz(FLAGFLOAT);
        }
        if(pos.x < 0)
        {
            return pos;
        }
        pos.addz(FLAGCENTER);
        if(f.interptime && f.interploc.x >= 0)
        {
            float t = min((lastmillis - f.interptime)/500.0f, 1.0f);
            pos.lerp(f.interploc, pos, t);
            angle += (1-t)*(f.interpangle - angle);
        }
        return pos;
    }

    vec interpflagpos(flag &f) { float angle; return interpflagpos(f, angle); }

    void rendergame()
    {
        for(int i = 0; i < flags.length(); i++)
        {
            flag &f = flags[i];
            if(!f.owner && f.droptime && f.droploc.x < 0)
            {
                continue;
            }
            const char *flagname = f.team==1 ? "game/flag/azul" : "game/flag/rojo";
            float angle;
            vec pos = interpflagpos(f, angle);
            rendermodel(flagname, Anim_Mapmodel|Anim_Loop,
                        pos, angle, 0, 0,
                        Model_CullVFC | Model_CullOccluded);
        }
    }

    void setup()
    {
        resetflags();
        for(int i = 0; i < entities::ents.length(); i++)
        {
            extentity *e = entities::ents[i];
            if(e->type!=GamecodeEnt_Flag || !VALID_TEAM(e->attr2))
            {
                continue;
            }
            int index = flags.length();
            if(!addflag(index, e->o, e->attr2))
            {
                continue;
            }
            flags[index].spawnangle = e->attr1;
        }
    }

    void senditems(packetbuf &p)
    {
        putint(p, NetMsg_InitFlags);
        putint(p, flags.length());
        for(int i = 0; i < flags.length(); i++)
        {
            flag &f = flags[i];
            putint(p, f.team);
            for(int k = 0; k < 3; ++k)
            {
                putint(p, static_cast<int>(f.spawnloc[k]*DMF));
            }
        }
    }

    void parseflags(ucharbuf &p, bool commit)
    {
        for(int k = 0; k < 2; ++k)
        {
            int score = getint(p);
            if(commit)
            {
                scores[k] = score;
            }
        }
        int numflags = getint(p);
        for(int i = 0; i < numflags; ++i)
        {
            int version = getint(p), owner = getint(p), dropped = 0;
            vec droploc(0, 0, 0);
            if(owner<0)
            {
                dropped = getint(p);
                if(dropped)
                {
                    for(int k = 0; k < 3; ++k)
                    {
                        droploc[k] = getint(p)/DMF;
                    }
                }
            }
            if(p.overread())
            {
                break;
            }
            if(commit && flags.inrange(i))
            {
                flag &f = flags[i];
                f.version = version;
                f.owner = owner>=0 ? (owner==player1->clientnum ? player1 : newclient(owner)) : NULL;
                f.owntime = owner>=0 ? lastmillis : 0;
                f.droptime = dropped ? lastmillis : 0;
                f.droploc = dropped ? droploc : f.spawnloc;
                f.interptime = 0;

                if(dropped && !droptofloor(f.droploc.addz(4), 4, 0)) f.droploc = vec(-1, -1, -1);
            }
        }
    }

    void trydropflag()
    {
        if(!modecheck(gamemode, Mode_CTF))
        {
            return;
        }
        for(int i = 0; i < flags.length(); i++)
        {
            if(flags[i].owner == player1)
            {
                addmsg(NetMsg_TryDropFlag, "rc", player1);
                return;
            }
        }
    }

    const char *teamcolorflag(flag &f)
    {
        return teamcolor("", "'s flag", f.team, "a flag");
    }

    void dropflag(gameent *d, int i, int version, const vec &droploc)
    {
        if(!flags.inrange(i))
        {
            return;
        }
        flag &f = flags[i];
        f.version = version;
        f.interploc = interpflagpos(f, f.interpangle);
        f.interptime = lastmillis;
        dropflag(i, droploc, d->yaw, lastmillis);
        d->flagpickup |= 1<<f.id;
        if(!droptofloor(f.droploc.addz(4), 4, 0))
        {
            f.droploc = vec(-1, -1, -1);
            f.interptime = 0;
        }
        conoutf(ConsoleMsg_GameInfo, "%s dropped %s", teamcolorname(d), teamcolorflag(f));
        teamsound(d, Sound_FlagDrop);
    }

    void flagexplosion(int i, int team, const vec &loc)
    {
        int fcolor;
        vec color;
        if(team==1)
        {
            fcolor = 0x2020FF;
            color = vec(0.25f, 0.25f, 1);
        }
        else
        {
            fcolor = 0x802020;
            color = vec(1, 0.25f, 0.25f);
        }
        particle_fireball(loc, 30, Part_Explosion, -1, fcolor, 4.8f);
        adddynlight(loc, 35, color, 900, 100);
        particle_splash(Part_Spark, 150, 300, loc, fcolor, 0.24f);
    }

    void flageffect(int i, int team, const vec &from, const vec &to)
    {
        if(from.x >= 0)
        {
            flagexplosion(i, team, from);
        }
        if(from==to)
        {
            return;
        }
        if(to.x >= 0)
        {
            flagexplosion(i, team, to);
        }
        if(from.x >= 0 && to.x >= 0)
        {
            particle_flare(from, to, 600, Part_Streak, team==1 ? 0x2222FF : 0xFF2222, 1.0f);
        }
    }

    void returnflag(gameent *d, int i, int version)
    {
        if(!flags.inrange(i))
        {
            return;
        }
        flag &f = flags[i];
        f.version = version;
        flageffect(i, f.team, interpflagpos(f), vec(f.spawnloc).addz(FLAGFLOAT+FLAGCENTER));
        f.interptime = 0;
        returnflag(i);
        conoutf(ConsoleMsg_GameInfo, "%s returned %s", teamcolorname(d), teamcolorflag(f));
        teamsound(d, Sound_FlagReturn);
    }

    void resetflag(int i, int version)
    {
        if(!flags.inrange(i))
        {
            return;
        }
        flag &f = flags[i];
        f.version = version;
        flageffect(i, f.team, interpflagpos(f), vec(f.spawnloc).addz(FLAGFLOAT+FLAGCENTER));
        f.interptime = 0;
        returnflag(i);
        conoutf(ConsoleMsg_GameInfo, "%s reset", teamcolorflag(f));
        teamsound(f.team == player1->team, Sound_FlagReset);
    }

    void scoreflag(gameent *d, int relay, int relayversion, int goal, int goalversion, int team, int score, int dflags)
    {
        setscore(team, score);
        if(flags.inrange(goal))
        {
            flag &f = flags[goal];
            f.version = goalversion;
            if(relay >= 0)
            {
                flags[relay].version = relayversion;
                flageffect(goal, team, vec(f.spawnloc).addz(FLAGFLOAT+FLAGCENTER), vec(flags[relay].spawnloc).addz(FLAGFLOAT+FLAGCENTER));
            }
            else
            {
                flageffect(goal, team, interpflagpos(f), vec(f.spawnloc).addz(FLAGFLOAT+FLAGCENTER));
            }
            f.interptime = 0;
            returnflag(relay >= 0 ? relay : goal);
            d->flagpickup &= ~(1<<f.id);
            if(d->feetpos().dist(f.spawnloc) < FLAGRADIUS)
            {
                d->flagpickup |= 1<<f.id;
            }
        }
        if(d!=player1)
        {
            particle_textcopy(d->abovehead(), tempformatstring("%d", score), Part_Text, 2000, 0x32FF64, 4.0f, -8);
        }
        d->flags = dflags;
        conoutf(ConsoleMsg_GameInfo, "%s scored for %s", teamcolorname(d), teamcolor("team ", "", team, "a team"));
        playsound(team==player1->team ? Sound_FlagScore : Sound_FlagFail);

        if(score >= FLAGLIMIT)
        {
            conoutf(ConsoleMsg_GameInfo, "%s captured %d flags", teamcolor("team ", "", team, "a team"), score);
        }
    }

    void takeflag(gameent *d, int i, int version)
    {
        if(!flags.inrange(i))
        {
            return;
        }
        flag &f = flags[i];
        f.version = version;
        f.interploc = interpflagpos(f, f.interpangle);
        f.interptime = lastmillis;
        if(f.droptime)
        {
            conoutf(ConsoleMsg_GameInfo, "%s picked up %s", teamcolorname(d), teamcolorflag(f));
        }
        else
        {
            conoutf(ConsoleMsg_GameInfo, "%s stole %s", teamcolorname(d), teamcolorflag(f));
        }
        ownflag(i, d, lastmillis);
        teamsound(d, Sound_FlagPickup);
    }

    void checkitems(gameent *d)
    {
        if(d->state!=ClientState_Alive)
        {
            return;
        }
        vec o = d->feetpos();
        bool tookflag = false;
        for(int i = 0; i < flags.length(); i++)
        {
            flag &f = flags[i];
            if(!VALID_TEAM(f.team) || f.team==player1->team || f.owner || (f.droptime && f.droploc.x<0))
            {
                continue;
            }
            const vec &loc = f.droptime ? f.droploc : f.spawnloc;
            if(o.dist(loc) < FLAGRADIUS)
            {
                if(d->flagpickup&(1<<f.id))
                {
                    continue;
                }
                if((lookupmaterial(o)&MatFlag_Clip) != Mat_GameClip && (lookupmaterial(loc)&MatFlag_Clip) != Mat_GameClip)
                {
                    tookflag = true;
                    addmsg(NetMsg_TakeFlag, "rcii", d, i, f.version);
                }
                d->flagpickup |= 1<<f.id;
            }
            else
            {
                d->flagpickup &= ~(1<<f.id);
            }
        }
        for(int i = 0; i < flags.length(); i++)
        {
            flag &f = flags[i];
            if(!VALID_TEAM(f.team) || f.team!=player1->team || f.owner || (f.droptime && f.droploc.x<0))
            {
                continue;
            }
            const vec &loc = f.droptime ? f.droploc : f.spawnloc;
            if(o.dist(loc) < FLAGRADIUS)
            {
                if(!tookflag && d->flagpickup&(1<<f.id))
                {
                    continue;
                }
                if((lookupmaterial(o)&MatFlag_Clip) != Mat_GameClip && (lookupmaterial(loc)&MatFlag_Clip) != Mat_GameClip)
                {
                    addmsg(NetMsg_TakeFlag, "rcii", d, i, f.version);
                }
                d->flagpickup |= 1<<f.id;
            }
            else
            {
                d->flagpickup &= ~(1<<f.id);
            }
       }
    }

    void respawned(gameent *d)
    {
        vec o = d->feetpos();
        d->flagpickup = 0;
        for(int i = 0; i < flags.length(); i++)
        {
            flag &f = flags[i];
            if(!VALID_TEAM(f.team) || f.owner || (f.droptime && f.droploc.x<0))
            {
                continue;
            }
            if(o.dist(f.droptime ? f.droploc : f.spawnloc) < FLAGRADIUS)
            {
                d->flagpickup |= 1<<f.id;
            }
       }
    }

    int respawnwait(gameent *d)
    {
        return max(0, RESPAWNSECS-(lastmillis-d->lastpain)/1000);
    }

    bool aihomerun(gameent *d, ai::aistate &b)
    {
        vec pos = d->feetpos();
        for(int k = 0; k < 2; ++k)
        {
            int goal = -1;
            for(int i = 0; i < flags.length(); i++)
            {
                flag &g = flags[i];
                if(g.team == d->team && (k || (!g.owner && !g.droptime)) &&
                    (!flags.inrange(goal) || g.pos().squaredist(pos) < flags[goal].pos().squaredist(pos)))
                {
                    goal = i;
                }
            }
            if(flags.inrange(goal) && ai::makeroute(d, b, flags[goal].pos()))
            {
                d->ai->switchstate(b, ai::AIState_Pursue, ai::AITravel_Affinity, goal);
                return true;
            }
        }
        if(b.type == ai::AIState_Interest && b.targtype == ai::AITravel_Node)
        {
            return true; // we already did this..
        }
        if(randomnode(d, b, ai::sightmin, 1e16f))
        {
            d->ai->switchstate(b, ai::AIState_Interest, ai::AITravel_Node, d->ai->route[0]);
            return true;
        }
        return false;
    }

    bool aicheck(gameent *d, ai::aistate &b)
    {
        static vector<int> takenflags;
        takenflags.setsize(0);
        for(int i = 0; i < flags.length(); i++)
        {
            flag &g = flags[i];
            if(g.owner == d)
            {
                return aihomerun(d, b);
            }
            else if(g.team == d->team && ((g.owner && g.team != g.owner->team) || g.droptime))
            {
                takenflags.add(i);
            }
        }
        if(!ai::badhealth(d) && !takenflags.empty())
        {
            int flag = takenflags.length() > 2 ? randomint(takenflags.length()) : 0;
            d->ai->switchstate(b, ai::AIState_Pursue, ai::AITravel_Affinity, takenflags[flag]);
            return true;
        }
        return false;
    }

    void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests)
    {
        vec pos = d->feetpos();
        for(int j = 0; j < flags.length(); j++)
        {
            flag &f = flags[j];
            if(f.owner != d)
            {
                static vector<int> targets; // build a list of others who are interested in this
                targets.setsize(0);
                bool home = f.team == d->team;
                ai::checkothers(targets, d, home ? ai::AIState_Defend : ai::AIState_Pursue, ai::AITravel_Affinity, j, true);
                gameent *e = NULL;
                for(int i = 0; i < numdynents(); ++i)
                {
                    if((e = reinterpret_cast<gameent *>(iterdynents(i))) && !e->ai && e->state == ClientState_Alive && (modecheck(gamemode, Mode_Team) && d->team == e->team))
                    { // try to guess what non ai are doing
                        vec ep = e->feetpos();
                        if(targets.find(e->clientnum) < 0 && (ep.squaredist(f.pos()) <= (FLAGRADIUS*FLAGRADIUS*4) || f.owner == e))
                        {
                            targets.add(e->clientnum);
                        }
                    }
                }
                if(home)
                {
                    bool guard = false;
                    if((f.owner && f.team != f.owner->team) || f.droptime || targets.empty())
                    {
                        guard = true;
                    }
                    if(guard)
                    { // defend the flag
                        ai::interest &n = interests.add();
                        n.state = ai::AIState_Defend;
                        n.node = ai::closestwaypoint(f.pos(), ai::sightmin, true);
                        n.target = j;
                        n.targtype = ai::AITravel_Affinity;
                        n.score = pos.squaredist(f.pos())/100.f;
                    }
                }
                else
                {
                    if(targets.empty())
                    { // attack the flag
                        ai::interest &n = interests.add();
                        n.state = ai::AIState_Pursue;
                        n.node = ai::closestwaypoint(f.pos(), ai::sightmin, true);
                        n.target = j;
                        n.targtype = ai::AITravel_Affinity;
                        n.score = pos.squaredist(f.pos());
                    }
                    else
                    { // help by defending the attacker
                        gameent *t;
                        for(int k = 0; k < targets.length(); k++)
                        {
                            if((t = getclient(targets[k])))
                            {
                                ai::interest &n = interests.add();
                                n.state = ai::AIState_Defend;
                                n.node = t->lastnode;
                                n.target = t->clientnum;
                                n.targtype = ai::AITravel_Player;
                                n.score = d->o.squaredist(t->o);
                            }
                        }
                    }
                }
            }
        }
    }

    bool aidefend(gameent *d, ai::aistate &b)
    {
        for(int i = 0; i < flags.length(); i++)
        {
            flag &g = flags[i];
            if(g.owner == d)
            {
                return aihomerun(d, b);
            }
        }
        if(flags.inrange(b.target))
        {
            flag &f = flags[b.target];
            if(f.droptime)
            {
                return ai::makeroute(d, b, f.pos());
            }
            if(f.owner)
            {
                return ai::violence(d, b, f.owner, 4);
            }
            int walk = 0;
            if(lastmillis-b.millis >= (201-d->skill)*33)
            {
                static vector<int> targets; // build a list of others who are interested in this
                targets.setsize(0);
                ai::checkothers(targets, d, ai::AIState_Defend, ai::AITravel_Affinity, b.target, true);
                gameent *e = NULL;
                for(int i = 0; i < numdynents(); ++i)
                {
                    if((e = (gameent *)iterdynents(i)) && !e->ai && e->state == ClientState_Alive && (modecheck(gamemode, Mode_Team) && d->team == e->team))
                    { // try to guess what non ai are doing
                        vec ep = e->feetpos();
                        if(targets.find(e->clientnum) < 0 && (ep.squaredist(f.pos()) <= (FLAGRADIUS*FLAGRADIUS*4) || f.owner == e))
                        {
                            targets.add(e->clientnum);
                        }
                    }
                }
                if(!targets.empty())
                {
                    d->ai->trywipe = true; // re-evaluate so as not to herd
                    return true;
                }
                else
                {
                    walk = 2;
                    b.millis = lastmillis;
                }
            }
            vec pos = d->feetpos();
            float mindist = static_cast<float>(FLAGRADIUS*FLAGRADIUS*8);
            for(int i = 0; i < flags.length(); i++)
            { // get out of the way of the returnee!
                flag &g = flags[i];
                if(pos.squaredist(g.pos()) <= mindist)
                {
                    if(g.owner && g.owner->team == d->team)
                    {
                        walk = 1;
                    }
                    if(g.droptime && ai::makeroute(d, b, g.pos()))
                    {
                        return true;
                    }
                }
            }
            return ai::defend(d, b, f.pos(), static_cast<float>(FLAGRADIUS*2), static_cast<float>(FLAGRADIUS*(2+(walk*2))), walk);
        }
        return false;
    }

    bool aipursue(gameent *d, ai::aistate &b)
    {
        if(flags.inrange(b.target))
        {
            flag &f = flags[b.target];
            if(f.owner == d)
            {
                return aihomerun(d, b);
            }
            if(f.team == d->team)
            {
                if(f.droptime)
                {
                    return ai::makeroute(d, b, f.pos());
                }
                if(f.owner)
                {
                    return ai::violence(d, b, f.owner, 4);
                }
                for(int i = 0; i < flags.length(); i++)
                {
                    flag &g = flags[i];
                    if(g.owner == d)
                    {
                        return ai::makeroute(d, b, f.pos());
                    }
                }
            }
            else
            {
                if(f.owner)
                {
                    return ai::violence(d, b, f.owner, 4);
                }
                return ai::makeroute(d, b, f.pos());
            }
        }
        return false;
    }
};

extern ctfclientmode ctfmode;
ICOMMAND(dropflag, "", (), { ctfmode.trydropflag(); });

#endif

#elif SERVERMODE

case NetMsg_TryDropFlag:
{
    if((ci->state.state!=ClientState_Spectator || ci->local || ci->privilege) && cq && smode==&ctfmode)
    {
        ctfmode.dropflag(cq);
    }
    break;
}

case NetMsg_TakeFlag:
{
    int flag = getint(p), version = getint(p);
    if((ci->state.state!=ClientState_Spectator || ci->local || ci->privilege) && cq && smode==&ctfmode)
    {
        ctfmode.takeflag(cq, flag, version);
    }
    break;
}

case NetMsg_InitFlags:
    if(smode==&ctfmode)
    {
        ctfmode.parseflags(p, (ci->state.state!=ClientState_Spectator || ci->privilege || ci->local) && !strcmp(ci->clientmap, smapname));
    }
    break;

#else

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

#endif

#include "game.h"

//player model rendering (both hud player and 3p player rendering)
//for "real" rendering, see /src/engine/render*
//includes:
//hud player rendering
//3p player rendering
//ragdoll handling
//player colors


namespace game
{
    vector<gameent *> bestplayers;
    vector<int> bestteams;

    VARP(ragdoll, 0, 1, 1);
    VARP(ragdollmillis, 0, 10000, 300000);
    VARP(ragdollfade, 0, 100, 5000);
    VARP(forceplayermodels, 0, 0, 1);
    VARP(showdead, 0, 1, 1);

    extern int playermodel;

    vector<gameent *> ragdolls;

    void saveragdoll(gameent *d)
    {
        if(!d->ragdoll || !ragdollmillis || (!ragdollfade && lastmillis > d->lastpain + ragdollmillis))
        {
            return;
        }
        gameent *r = new gameent(*d);
        r->lastupdate = ragdollfade && lastmillis > d->lastpain + max(ragdollmillis - ragdollfade, 0) ? lastmillis - max(ragdollmillis - ragdollfade, 0) : d->lastpain;
        r->edit = NULL;
        r->ai = NULL;
        if(d==player1)
        {
            r->playermodel = playermodel;
        }
        ragdolls.add(r);
        d->ragdoll = NULL;
    }

    void clearragdolls()
    {
        ragdolls.deletecontents();
    }

    void moveragdolls()
    {
        for(int i = 0; i < ragdolls.length(); i++)
        {
            gameent *d = ragdolls[i];
            if(lastmillis > d->lastupdate + ragdollmillis)
            {
                delete ragdolls.remove(i--);
                continue;
            }
            moveragdoll(d);
        }
    }
    //ffa colors
    static const int playercolors[] =
    {
        0xA12020,
        0xA15B28,
        0xB39D52,
        0x3E752F,
        0x3F748C,
        0x214C85,
        0xB3668C,
        0x523678,
        0xB3ADA3
    };
    //azul (blue) team allowed colors
    static const int playercolorsazul[] =
    {
        0x27508A,
        0x3F748C,
        0x3B3B80,
        0x5364B5
    };
    //rojo (red) team allowed colors
    static const int playercolorsrojo[] =
    {
        0xAC2C2A,
        0x992417,
        0x802438,
        0xA3435B
    };

    extern void changedplayercolor();
    VARFP(playercolor, 0, 4, sizeof(playercolors)/sizeof(playercolors[0])-1, changedplayercolor());
    VARFP(playercolorazul, 0, 0, sizeof(playercolorsazul)/sizeof(playercolorsazul[0])-1, changedplayercolor());
    VARFP(playercolorrojo, 0, 0, sizeof(playercolorsrojo)/sizeof(playercolorsrojo[0])-1, changedplayercolor());

    static const playermodelinfo playermodels[] =
    {
        { { "player/bones", "player/bones", "player/bones" }, { "hudgun", "hudgun", "hudgun" }, { "player", "player_azul", "player_rojo" }, true }
    };

    extern void changedplayermodel();
    VARFP(playermodel, 0, 0, sizeof(playermodels)/sizeof(playermodels[0])-1, changedplayermodel());

    int chooserandomplayermodel(int seed)
    {
        return (seed&0xFFFF)%(sizeof(playermodels)/sizeof(playermodels[0]));
    }

    const playermodelinfo *getplayermodelinfo(int n)
    {
        if(size_t(n) >= sizeof(playermodels)/sizeof(playermodels[0]))
        {
            return NULL;
        }
        return &playermodels[n];
    }

    const playermodelinfo &getplayermodelinfo(gameent *d)
    {
        const playermodelinfo *mdl = getplayermodelinfo(d==player1 || forceplayermodels ? playermodel : d->playermodel);
        if(!mdl)
        {
            mdl = getplayermodelinfo(playermodel);
        }
        return *mdl;
    }

    int getplayercolor(int team, int color)
    {
        #define GETPLAYERCOLOR(playercolors) \
            return playercolors[color%(sizeof(playercolors)/sizeof(playercolors[0]))];
        switch(team)
        {
            case 1:
            {
                GETPLAYERCOLOR(playercolorsazul)
            }
            case 2:
            {
                GETPLAYERCOLOR(playercolorsrojo)
            }
            default:
            {
                GETPLAYERCOLOR(playercolors)
            }
        }
    }

    //returns color of a given player given a team (which they may or may not be on)
    ICOMMAND(getplayercolor, "ii", (int *color, int *team), intret(getplayercolor(*team, *color)));

    int getplayercolor(gameent *d, int team)
    {
        if(d==player1) switch(team)
        {
            case 1:
            {
                return getplayercolor(1, playercolorazul);
            }
            case 2:
            {
                return getplayercolor(2, playercolorrojo);
            }
            default:
            {
                return getplayercolor(0, playercolor);
            }
        }
        else
        {
            return getplayercolor(team, (d->playercolor>>(5*team))&0x1F);
        }
    }

    void changedplayermodel()
    {
        if(player1->clientnum < 0)
        {
            player1->playermodel = playermodel;
        }
        if(player1->ragdoll)
        {
            cleanragdoll(player1);
        }
        for(int i = 0; i < ragdolls.length(); i++)
        {
            gameent *d = ragdolls[i];
            if(!d->ragdoll)
            {
                continue;
            }
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl)
                {
                    continue;
                }
            }
            cleanragdoll(d);
        }
        for(int i = 0; i < players.length(); i++)
        {
            gameent *d = players[i];
            if(d == player1 || !d->ragdoll)
            {
                continue;
            }
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl)
                {
                    continue;
                }
            }
            cleanragdoll(d);
        }
    }

    void changedplayercolor()
    {
        if(player1->clientnum < 0)
        {
            player1->playercolor = playercolor | (playercolorazul<<5) | (playercolorrojo<<10);
        }
    }

    void syncplayer()
    {
        if(player1->playermodel != playermodel)
        {
            player1->playermodel = playermodel;
            addmsg(NetMsg_SwitchModel, "ri", player1->playermodel);
        }

        int col = playercolor | (playercolorazul<<5) | (playercolorrojo<<10);
        if(player1->playercolor != col)
        {
            player1->playercolor = col;
            addmsg(NetMsg_SwitchColor, "ri", player1->playercolor);
        }
    }

    void preloadplayermodel()
    {
        for(int i = 0; i < static_cast<int>(sizeof(playermodels)/sizeof(playermodels[0])); ++i)
        {
            const playermodelinfo *mdl = getplayermodelinfo(i);
            if(!mdl) //don't preload a model that isn't there
            {
                break;
            }
            if(i != playermodel && (!multiplayer(false) || forceplayermodels))
            {
                continue;
            }
            if(modecheck(gamemode, Mode_Team))
            {
                for(int j = 0; j < MAXTEAMS; ++j)
                {
                    preloadmodel(mdl->model[1+j]);
                }
            }
            else preloadmodel(mdl->model[0]);
        }
    }

    int numanims()
    {
        return Anim_NumAnims;
    }

    void findanims(const char *pattern, vector<int> &anims)
    {
        for(int i = 0; i < static_cast<int>(sizeof(animnames)/sizeof(animnames[0])); ++i)
        {
            if(matchanim(animnames[i], pattern))
            {
                anims.add(i);
            }
        }
    }

    //============================================ 3p/other player rendering =======================//

    VAR(animoverride, -1, 0, Anim_NumAnims-1);
    VAR(testanims, 0, 0, 1);
    VAR(testpitch, -90, 0, 90);

    void renderplayer(gameent *d, const playermodelinfo &mdl, int color, int team, float fade, int flags = 0, bool mainpass = true)
    {
        int lastaction = d->lastaction, anim = Anim_Idle|ANIM_LOOP, attack = 0, delay = 0;
        if(d->lastattack >= 0)
        {
            attack = attacks[d->lastattack].anim;
            delay = attacks[d->lastattack].attackdelay+50;
        }
        if(intermission && d->state!=ClientState_Dead)
        {
            anim = attack = Anim_Lose|ANIM_LOOP;
            if(VALID_TEAM(team) ? bestteams.htfind(team)>=0 : bestplayers.find(d)>=0)
            {
                anim = attack = Anim_Win|ANIM_LOOP;
            }
        }
        else if(d->state==ClientState_Alive && d->lasttaunt && lastmillis-d->lasttaunt<1000 && lastmillis-d->lastaction>delay)
        {
            lastaction = d->lasttaunt;
            anim = attack = Anim_Taunt;
            delay = 1000;
        }
        modelattach a[5];
        int ai = 0;
        if(guns[d->gunselect].vwep)
        {
            int vanim = Anim_VWepIdle|ANIM_LOOP, vtime = 0;
            if(lastaction && d->lastattack >= 0 && attacks[d->lastattack].gun==d->gunselect && lastmillis < lastaction + delay)
            {
                vanim = attacks[d->lastattack].vwepanim;
                vtime = lastaction;
            }
            a[ai++] = modelattach("tag_weapon", guns[d->gunselect].vwep, vanim, vtime);
        }
        if(mainpass && !(flags&Model_OnlyShadow))
        {
            d->muzzle = vec(-1, -1, -1);
            if(guns[d->gunselect].vwep)
            {
                a[ai++] = modelattach("tag_muzzle", &d->muzzle);
            }
        }
        const char *mdlname = mdl.model[VALID_TEAM(team) ? team : 0];
        float yaw = testanims && d==player1 ? 0 : d->yaw,
              pitch = testpitch && d==player1 ? testpitch : d->pitch;
        vec o = d->feetpos();
        int basetime = 0;
        if(animoverride)
        {
            anim = (animoverride<0 ? ANIM_ALL : animoverride)|ANIM_LOOP;
        }
        else if(d->state==ClientState_Dead)
        {
            anim = Anim_Dying|ANIM_NOPITCH;
            basetime = d->lastpain;
            if(ragdoll && mdl.ragdoll)
            {
                anim |= ANIM_RAGDOLL;
            }
            else if(lastmillis-basetime>1000)
            {
                anim = Anim_Dead|ANIM_LOOP|ANIM_NOPITCH;
            }
        }
        else if(d->state==ClientState_Editing || d->state==ClientState_Spectator)
        {
            anim = Anim_Edit|ANIM_LOOP;
        }
        else if(d->state==ClientState_Lagged)
        {
            anim = Anim_Lag|ANIM_LOOP;
        }
        else if(!intermission)
        {
            if(lastmillis-d->lastpain < 300)
            {
                anim = Anim_Pain;
                basetime = d->lastpain;
            }
            else if(d->lastpain < lastaction && lastmillis-lastaction < delay)
            {
                anim = attack;
                basetime = lastaction;
            }

            if(d->inwater && d->physstate<=PhysEntState_Fall)
            {
                anim |= (((game::allowmove(d) && (d->move || d->strafe)) || d->vel.z+d->falling.z>0 ? Anim_Swim : Anim_Sink)|ANIM_LOOP)<<ANIM_SECONDARY;
            }
            else
            {
                static const int dirs[9] =
                {
                    Anim_RunSE, Anim_RunS, Anim_RunSW,
                    Anim_RunE,  0,          Anim_RunSE,
                    Anim_RunNE, Anim_RunN, Anim_RunNW
                };
                int dir = dirs[(d->move+1)*3 + (d->strafe+1)];
                if(d->timeinair>100)
                {
                    anim |= ((dir ? dir+Anim_JumpN-Anim_RunN : Anim_Jump) | ANIM_END) << ANIM_SECONDARY;
                }
                else if(dir && game::allowmove(d))
                {
                    anim |= (dir | ANIM_LOOP) << ANIM_SECONDARY;
                }
            }
            if(d->crouching)
            {
                switch((anim>>ANIM_SECONDARY)&ANIM_INDEX)
                {
                    case Anim_Idle:
                    {
                        anim &= ~(ANIM_INDEX<<ANIM_SECONDARY);
                        anim |= Anim_Crouch<<ANIM_SECONDARY;
                        break;
                    }
                    case Anim_Jump:
                    {
                        anim &= ~(ANIM_INDEX<<ANIM_SECONDARY);
                        anim |= Anim_CrouchJump<<ANIM_SECONDARY;
                        break;
                    }
                    case Anim_Swim:
                    {
                        anim &= ~(ANIM_INDEX<<ANIM_SECONDARY);
                        anim |= Anim_CrouchSwim<<ANIM_SECONDARY;
                        break;
                    }
                    case Anim_Sink:
                    {
                        anim &= ~(ANIM_INDEX<<ANIM_SECONDARY);
                        anim |= Anim_CrouchSink<<ANIM_SECONDARY;
                        break;
                    }
                    case 0:
                    {
                        anim |= (Anim_Crouch|ANIM_LOOP)<<ANIM_SECONDARY;
                        break;
                    }
                    case Anim_RunN:
                    case Anim_RunNE:
                    case Anim_RunE:
                    case Anim_RunSE:
                    case Anim_RunS:
                    case Anim_RunSW:
                    case Anim_RunW:
                    case Anim_RunNW:
                    {
                        anim += (Anim_CrouchN - Anim_RunN) << ANIM_SECONDARY;
                        break;
                    }
                    case Anim_JumpN:
                    case Anim_JumpNE:
                    case Anim_JumpE:
                    case Anim_JumpSE:
                    case Anim_JumpS:
                    case Anim_JumpSW:
                    case Anim_JumpW:
                    case Anim_JumpNW:
                    {
                        anim += (Anim_CrouchJumpN - Anim_JumpN) << ANIM_SECONDARY;
                        break;
                    }
                }
            }
            if((anim&ANIM_INDEX)==Anim_Idle && (anim>>ANIM_SECONDARY)&ANIM_INDEX)
            {
                anim >>= ANIM_SECONDARY;
            }
        }
        if(!((anim>>ANIM_SECONDARY)&ANIM_INDEX))
        {
            anim |= (Anim_Idle|ANIM_LOOP)<<ANIM_SECONDARY;
        }
        if(d!=player1)
        {
            flags |= Model_CullVFC | Model_CullOccluded | Model_CullQuery;
        }
        if(d->type==PhysEnt_Player)
        {
            flags |= Model_FullBright;
        }
        else
        {
            flags |= Model_CullDist;
        }
        if(!mainpass)
        {
            flags &= ~(Model_FullBright | Model_CullVFC | Model_CullOccluded | Model_CullQuery | Model_CullDist);
        }
        float trans = d->state == ClientState_Lagged ? 0.5f : 1.0f;
        rendermodel(mdlname, anim, o, yaw, pitch, 0, flags, d, a[0].tag ? a : NULL, basetime, 0, fade, vec4(vec::hexcolor(color), trans));
    }

    static inline void renderplayer(gameent *d, float fade = 1, int flags = 0)
    {
        int team = modecheck(gamemode, Mode_Team) && VALID_TEAM(d->team) ? d->team : 0;
        renderplayer(d, getplayermodelinfo(d), getplayercolor(d, team), team, fade, flags);
    }

    void rendergame()
    {
        ai::render();
        if(intermission)
        {
            bestteams.shrink(0);
            bestplayers.shrink(0);
            if(modecheck(gamemode, Mode_Team))
            {
                getbestteams(bestteams);
            }
            else
            {
                getbestplayers(bestplayers);
            }
        }

        bool third = isthirdperson();
        gameent *f = followingplayer(),
                *exclude = third ? NULL : f;
        for(int i = 0; i < players.length(); i++)
        {
            gameent *d = players[i];
            if(   d == player1
               || d->state==ClientState_Spectator
               || d->state==ClientState_Spawning
               || d->lifesequence < 0
               || d == exclude
               || (d->state==ClientState_Dead && !showdead))
            {
                continue;
            }
            renderplayer(d);
            copystring(d->info, colorname(d));
            if(d->state!=ClientState_Dead)
            {
                int team = modecheck(gamemode, Mode_Team) && VALID_TEAM(d->team) ? d->team : 0;
                particle_text(d->abovehead(), d->info, Part_Text, 1, teamtextcolor[team], 2.0f);
            }
        }
        for(int i = 0; i < ragdolls.length(); i++)
        {
            gameent *d = ragdolls[i];
            float fade = 1.0f;
            if(ragdollmillis && ragdollfade)
                fade -= clamp(static_cast<float>(lastmillis - (d->lastupdate + max(ragdollmillis - ragdollfade, 0)))/min(ragdollmillis, ragdollfade), 0.0f, 1.0f);
            renderplayer(d, fade);
        }
        if(exclude)
        {
            renderplayer(exclude, 1, Model_OnlyShadow);
        }
        else if(!f && (player1->state==ClientState_Alive || (player1->state==ClientState_Editing && third) || (player1->state==ClientState_Dead && showdead)))
        {
            renderplayer(player1, 1, third ? 0 : Model_OnlyShadow);
        }
        renderbouncers();
        renderprojectiles();
        if(cmode)
        {
            cmode->rendergame();
        }
    }

    //============================================ hud player rendering ============================//

    VARP(hudgun, 0, 1, 1);
    VARP(hudgunsway, 0, 1, 1);

    FVAR(swaystep, 1, 35.0f, 100);
    FVAR(swayside, 0, 0.10f, 1);
    FVAR(swayup, -1, 0.15f, 1);

    float swayfade = 0,
          swayspeed = 0,
          swaydist = 0;
    vec swaydir(0, 0, 0);

    void swayhudgun(int curtime)
    {
        gameent *d = hudplayer();
        if(d->state != ClientState_Spectator)
        {
            if(d->physstate >= PhysEntState_Slope)
            {
                swayspeed = min(sqrtf(d->vel.x*d->vel.x + d->vel.y*d->vel.y), d->maxspeed);
                swaydist += swayspeed*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*swaystep);
                swayfade = 1;
            }
            else if(swayfade > 0)
            {
                swaydist += swayspeed*swayfade*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*swaystep);
                swayfade -= 0.5f*(curtime*d->maxspeed)/(swaystep*1000.0f);
            }

            float k = pow(0.7f, curtime/10.0f);
            swaydir.mul(k);
            vec vel(d->vel);
            vel.add(d->falling);
            swaydir.add(vec(vel).mul((1-k)/(15*max(vel.magnitude(), d->maxspeed))));
        }
    }

    struct hudent : dynent
    {
        hudent() { type = PhysEnt_Camera; }
    } guninterp;

    void drawhudmodel(gameent *d, int anim, int basetime)
    {
        const char *file = guns[d->gunselect].file;
        if(!file)
        {
            return;
        }
        vec sway;
        vecfromyawpitch(d->yaw, 0, 0, 1, sway);
        float steps = swaydist/swaystep*M_PI;
        sway.mul(swayside*cosf(steps));
        sway.z = swayup*(fabs(sinf(steps)) - 1);
        sway.add(swaydir).add(d->o);
        if(!hudgunsway)
        {
            sway = d->o;
        }
        const playermodelinfo &mdl = getplayermodelinfo(d);
        int team = modecheck(gamemode, Mode_Team) && VALID_TEAM(d->team) ? d->team : 0,
            color = getplayercolor(d, team);
        DEF_FORMAT_STRING(gunname, "%s/%s", mdl.hudguns[team], file);
        modelattach a[2];
        d->muzzle = vec(-1, -1, -1);
        a[0] = modelattach("tag_muzzle", &d->muzzle);
        rendermodel(gunname, anim, sway, d->yaw, d->pitch, 0, Model_NoBatch, NULL, a, basetime, 0, 1, vec4(vec::hexcolor(color), 1));
        if(d->muzzle.x >= 0)
        {
            d->muzzle = calcavatarpos(d->muzzle, 12);
        }
    }

    void drawhudgun()
    {
        gameent *d = hudplayer();
        if(d->state==ClientState_Spectator || d->state==ClientState_Editing || !hudgun || editmode)
        {
            d->muzzle = player1->muzzle = vec(-1, -1, -1);
            return;
        }

        int anim = Anim_GunIdle|ANIM_LOOP, basetime = 0;
        if(d->lastaction && d->lastattack >= 0 && attacks[d->lastattack].gun==d->gunselect && lastmillis-d->lastaction<attacks[d->lastattack].attackdelay)
        {
            anim = attacks[d->lastattack].hudanim;
            basetime = d->lastaction;
        }
        drawhudmodel(d, anim, basetime);
    }

    void renderavatar()
    {
        drawhudgun();
    }

    void renderplayerpreview(int model, int color, int team, int weap)
    {
        static gameent *previewent = NULL;
        if(!previewent)
        {
            previewent = new gameent;
            for(int i = 0; i < Gun_NumGuns; ++i)
            {
                previewent->ammo[i] = 1;
            }
        }
        float height = previewent->eyeheight + previewent->aboveeye,
              zrad = height/2;
        vec2 xyrad = vec2(previewent->xradius, previewent->yradius).max(height/4);
        previewent->o = calcmodelpreviewpos(vec(xyrad, zrad), previewent->yaw).addz(previewent->eyeheight - zrad);
        previewent->gunselect = VALID_GUN(weap) ? weap : Gun_Rail;
        const playermodelinfo *mdlinfo = getplayermodelinfo(model);
        if(!mdlinfo)
        {
            return;
        }
        renderplayer(previewent, *mdlinfo, getplayercolor(team, color), team, 1, 0, false);
    }

    vec hudgunorigin(int gun, const vec &from, const vec &to, gameent *d)
    {
        if(d->muzzle.x >= 0)
        {
            return d->muzzle;
        }
        vec offset(from);
        if(d!=hudplayer() || isthirdperson())
        {
            vec front, right;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, front);
            offset.add(front.mul(d->radius));
            offset.z += (d->aboveeye + d->eyeheight)*0.75f - d->eyeheight;
            vecfromyawpitch(d->yaw, 0, 0, -1, right);
            offset.add(right.mul(0.5f*d->radius));
            offset.add(front);
            return offset;
        }
        offset.add(vec(to).sub(from).normalize().mul(2));
        if(hudgun)
        {
            offset.sub(vec(camup).mul(1.0f));
            offset.add(vec(camright).mul(0.8f));
        }
        else
        {
            offset.sub(vec(camup).mul(0.8f));
        }
        return offset;
    }

    void preloadweapons()
    {
        const playermodelinfo &mdl = getplayermodelinfo(player1);
        for(int i = 0; i < Gun_NumGuns; ++i)
        {
            const char *file = guns[i].file;
            if(!file)
            {
                continue;
            }
            string fname;
            if(modecheck(gamemode, Mode_Team))
            {
                for(int j = 0; j < MAXTEAMS; ++j)
                {
                    formatstring(fname, "%s/%s", mdl.hudguns[1+j], file);
                    preloadmodel(fname);
                }
            }
            else
            {
                formatstring(fname, "%s/%s", mdl.hudguns[0], file);
                preloadmodel(fname);
            }
            formatstring(fname, "worldgun/%s", file);
            preloadmodel(fname);
        }
    }

    void preloadsounds()
    {
        for(int i = Sound_Jump; i <= Sound_Die2; i++)
        {
            preloadsound(i);
        }
    }

    void preload()
    {
        if(hudgun)
        {
            preloadweapons();
        }
        preloadplayermodel();
        preloadsounds();
        entities::preloadentities();
    }

}


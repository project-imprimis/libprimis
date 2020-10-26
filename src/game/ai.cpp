#include "game.h"

extern int fog;

namespace ai
{
    using namespace game;

    avoidset obstacles;
    int updatemillis = 0,
        iteration = 0,
        itermillis = 0,
        forcegun = -1;
    vec aitarget(0, 0, 0);

    VAR(aidebug, 0, 0, 6);
    VAR(aiforcegun, -1, -1, Gun_NumGuns-1);

    ICOMMAND(addbot, "s", (char *s), addmsg(NetMsg_AddBot, "ri", *s ? std::clamp(parseint(s), 1, 101) : -1));
    ICOMMAND(delbot, "", (), addmsg(NetMsg_DelBot, "r"));
    ICOMMAND(botlimit, "i", (int *n), addmsg(NetMsg_BotLimit, "ri", *n));
    ICOMMAND(botbalance, "i", (int *n), addmsg(NetMsg_BotBalance, "ri", *n));

    float viewdist(int x)
    {
        return x <= 100 ? std::clamp((sightmin+(sightmax-sightmin))/100.f*static_cast<float>(x), static_cast<float>(sightmin), static_cast<float>(fog)) : static_cast<float>(fog);
    }

    float viewfieldx(int x)
    {
        return x <= 100 ? std::clamp((viewmin+(viewmax-viewmin))/100.f*static_cast<float>(x), static_cast<float>(viewmin), static_cast<float>(viewmax)) : static_cast<float>(viewmax);
    }

    float viewfieldy(int x)
    {
        return viewfieldx(x)*3.f/4.f;
    }

    bool canmove(gameent *d)
    {
        return d->state != ClientState_Dead && !intermission;
    }

    float attackmindist(int atk)
    {
        return max(int(attacks[atk].exprad), 2);
    }

    float attackmaxdist(int atk)
    {
        return attacks[atk].range + 4;
    }

    bool attackrange(gameent *d, int atk, float dist)
    {
        float mindist = attackmindist(atk),
              maxdist = attackmaxdist(atk);
        return dist >= mindist*mindist && dist <= maxdist*maxdist;
    }

    //check if a player is alive and can be a valid target for another player (don't shoot up teammates)
    bool targetable(gameent *d, gameent *e)
    {
        if(d == e || !canmove(d))
        {
            return false;
        }
         //if player is alive and not on the same team
        return e->state == ClientState_Alive && !(modecheck(gamemode, Mode_Team) && d->team == e->team);
    }

    bool getsight(vec &o, float yaw, float pitch, vec &q, vec &v, float mdist, float fovx, float fovy)
    {
        float dist = o.dist(q);

        if(dist <= mdist)
        {
            float x = fmod(fabs(asin((q.z-o.z)/dist)/RAD-pitch), 360),
                  y = fmod(fabs(-atan2(q.x-o.x, q.y-o.y)/RAD-yaw), 360);
            if(min(x, 360-x) <= fovx && min(y, 360-y) <= fovy)
            {
                return raycubelos(o, q, v);
            }
        }
        return false;
    }

    bool cansee(gameent *d, vec &x, vec &y, vec &targ)
    {
        aistate &b = d->ai->getstate();
        if(canmove(d) && b.type != AIState_Wait)
        {
            return getsight(x, d->yaw, d->pitch, y, targ, d->ai->views[2], d->ai->views[0], d->ai->views[1]);
        }
        return false;
    }

    bool canshoot(gameent *d, int atk, gameent *e)
    {
        if(attackrange(d, atk, e->o.squaredist(d->o)) && targetable(d, e))
        {
            return d->ammo[attacks[atk].gun] > 0 && lastmillis - d->lastaction >= d->gunwait;
        }
        return false;
    }

    bool canshoot(gameent *d, int atk)
    {
        return !d->ai->becareful && d->ammo[attacks[atk].gun] > 0 && lastmillis - d->lastaction >= d->gunwait;
    }

    bool hastarget(gameent *d, int atk, aistate &b, gameent *e, float yaw, float pitch, float dist)
    { // add margins of error
        if(attackrange(d, atk, dist) || (d->skill <= 100 && !randomint(d->skill)))
        {
            float skew = std::clamp(static_cast<float>(lastmillis-d->ai->enemymillis)/static_cast<float>((d->skill*attacks[atk].attackdelay/200.f)), 0.f, attacks[atk].projspeed ? 0.25f : 1e16f),
                  offy = yaw-d->yaw,
                  offp = pitch-d->pitch;
            if(offy > 180)
            {
                offy -= 360;
            }
            else if(offy < -180)
            {
                offy += 360;
            }
            if(fabs(offy) <= d->ai->views[0]*skew && fabs(offp) <= d->ai->views[1]*skew)
            {
                return true;
            }
        }
        return false;
    }

    vec getaimpos(gameent *d, int atk, gameent *e)
    {
        vec o = e->o;
        if(atk == Attack_PulseShoot)
        {
            o.z += (e->aboveeye*0.2f)-(0.8f*d->eyeheight);
        }
        else
        {
            o.z += (e->aboveeye-e->eyeheight)*0.5f;
        }
        if(d->skill <= 100)
        {
            if(lastmillis >= d->ai->lastaimrnd)
            {
                int aiskew = 1;
                switch(atk)
                {
                    case Attack_RailShot:
                    {
                        aiskew = 5;
                        break;
                    }
                    case Attack_PulseShoot:
                    {
                        aiskew = 20;
                        break;
                    }
                    default: break;
                }
                for(int k = 0; k < 3; ++k)
                {//e->radius is what's being plugged in here
                    d->ai->aimrnd[k] = ((randomint(static_cast<int>((e->radius)*aiskew*2)+1)-((e->radius)*aiskew))*(1.f/static_cast<float>(max(d->skill, 1))));
                }
                int dur = (d->skill+10)*10;
                d->ai->lastaimrnd = lastmillis+dur+randomint(dur);
            }
            for(int k = 0; k < 3; ++k)
            {
                o[k] += d->ai->aimrnd[k];
            }
        }
        return o;
    }

    void create(gameent *d)
    {
        if(!d->ai)
        {
            d->ai = new aiinfo;
        }
    }

    void destroy(gameent *d)
    {
        if(d->ai)
        {
            DELETEP(d->ai);
        }
    }

    void init(gameent *d, int at, int ocn, int sk, int bn, int pm, int col, const char *name, int team)
    {
        loadwaypoints();

        gameent *o = newclient(ocn);

        d->aitype = at;

        bool resetthisguy = false;
        if(!d->name[0])
        {
            if(aidebug)
            {
                conoutf("%s assigned to %s at skill %d", colorname(d, name), o ? colorname(o) : "?", sk);
            }
            else
            {
                conoutf("\f0join:\f7 %s", colorname(d, name));
            }
            resetthisguy = true;
        }
        else
        {
            if(d->ownernum != ocn)
            {
                if(aidebug)
                {
                    conoutf("%s reassigned to %s", colorname(d, name), o ? colorname(o) : "?");
                }
                resetthisguy = true;
            }
            if(d->skill != sk && aidebug)
            {
                conoutf("%s changed skill to %d", colorname(d, name), sk);
            }
        }

        copystring(d->name, name, maxnamelength+1);
        d->team = validteam(team) ? team : 0;
        d->ownernum = ocn;
        d->plag = 0;
        d->skill = sk;
        d->playermodel = chooserandomplayermodel(pm);
        d->playercolor = col;

        if(resetthisguy)
        {
            removeweapons(d);
        }
        if(d->ownernum >= 0 && player1->clientnum == d->ownernum)
        {
            create(d);
            if(d->ai)
            {
                d->ai->views[0] = viewfieldx(d->skill);
                d->ai->views[1] = viewfieldy(d->skill);
                d->ai->views[2] = viewdist(d->skill);
            }
        }
        else if(d->ai)
        {
            destroy(d);
        }
    }

    void update()
    {
        if(intermission)
        {
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i]->ai)
                {
                    players[i]->stopmoving();
                }
            }
        }
        else // fixed rate logic done out-of-sequence at 1 frame per second for each ai
        {
            if(totalmillis-updatemillis > 1000)
            {
                avoid();
                forcegun = multiplayer ? -1 : aiforcegun;
                updatemillis = totalmillis;
            }
            if(!iteration && totalmillis-itermillis > 1000)
            {
                iteration = 1;
                itermillis = totalmillis;
            }
            int count = 0;
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i]->ai)
                {
                    think(players[i], ++count == iteration ? true : false);
                }
            }
            if(++iteration > count)
            {
                iteration = 0;
            }
        }
    }

    bool checkothers(vector<int> &targets, gameent *d, int state, int targtype, int target, bool teams, int *members)
    { // checks the states of other ai for a match
        targets.setsize(0);
        for(int i = 0; i < players.length(); i++)
        {
            gameent *e = players[i];
            if(targets.find(e->clientnum) >= 0)
            {
                continue;
            }
            if(teams && d && !(modecheck(gamemode, Mode_Team) && d->team == e->team))
            {
                continue;
            }
            if(members)
            {
                (*members)++;
            }
            if(e == d || !e->ai || e->state != ClientState_Alive)
            {
                continue;
            }
            aistate &b = e->ai->getstate();
            if(state >= 0 && b.type != state)
            {
                continue;
            }
            if(target >= 0 && b.target != target)
            {
                continue;
            }
            if(targtype >=0 && b.targtype != targtype)
            {
                continue;
            }
            targets.add(e->clientnum);
        }
        return !targets.empty();
    }

    bool makeroute(gameent *d, aistate &b, int node, bool changed, int retries)
    {
        if(!iswaypoint(d->lastnode))
        {
            return false;
        }
        if(changed && d->ai->route.length() > 1 && d->ai->route[0] == node)
        {
            return true;
        }
        if(route(d, d->lastnode, node, d->ai->route, obstacles, retries))
        {
            b.override = false;
            return true;
        }
        // retry fails: 0 = first attempt, 1 = try ignoring obstacles, 2 = try ignoring prevnodes too
        if(retries <= 1)
        {
            return makeroute(d, b, node, false, retries+1);
        }
        return false;
    }

    bool makeroute(gameent *d, aistate &b, const vec &pos, bool changed, int retries)
    {
        int node = closestwaypoint(pos, sightmin, true);
        return makeroute(d, b, node, changed, retries);
    }

    bool randomnode(gameent *d, aistate &b, const vec &pos, float guard, float wander)
    {
        static vector<int> candidates;
        candidates.setsize(0);
        findwaypointswithin(pos, guard, wander, candidates);
        while(!candidates.empty())
        {
            int w = randomint(candidates.length()),
                n = candidates.removeunordered(w);
            if(n != d->lastnode && !d->ai->hasprevnode(n) && !obstacles.find(n, d) && makeroute(d, b, n))
            {
                return true;
            }
        }
        return false;
    }

    bool randomnode(gameent *d, aistate &b, float guard, float wander)
    {
        return randomnode(d, b, d->feetpos(), guard, wander);
    }

    bool badhealth(gameent *d)
    {
        //if(d->skill <= 100) return d->health <= (111-d->skill)/4;
        return false;
    }

    bool enemy(gameent *d, aistate &b, const vec &pos, float guard = sightmin, int pursue = 0)
    {
        gameent *t = NULL;
        vec dp = d->headpos();
        float mindist = guard*guard,
              bestdist = 1e16f;
        int atk = guns[d->gunselect].attacks[Act_Shoot];
        for(int i = 0; i < players.length(); i++)
        {
            gameent *e = players[i];
            if(e == d || !targetable(d, e))
            {
                continue;
            }
            vec ep = getaimpos(d, atk, e);
            float dist = ep.squaredist(dp);
            if(dist < bestdist && (cansee(d, dp, ep) || dist <= mindist))
            {
                t = e;
                bestdist = dist;
            }
        }
        if(t && violence(d, b, t, pursue))
        {
            return true;
        }
        return false;
    }

    bool patrol(gameent *d, aistate &b, const vec &pos, float guard, float wander, int walk, bool retry)
    {
        vec feet = d->feetpos();
        if(walk == 2 || b.override || (walk && feet.squaredist(pos) <= guard*guard) || !makeroute(d, b, pos))
        { // run away and back to keep ourselves busy
            if(!b.override && randomnode(d, b, pos, guard, wander))
            {
                b.override = true;
                return true;
            }
            else if(d->ai->route.empty())
            {
                if(!retry)
                {
                    b.override = false;
                    return patrol(d, b, pos, guard, wander, walk, true);
                }
                b.override = false;
                return false;
            }
        }
        b.override = false;
        return true;
    }

    bool defend(gameent *d, aistate &b, const vec &pos, float guard, float wander, int walk)
    {
        bool hasenemy = enemy(d, b, pos, wander);
        if(!walk)
        {
            if(d->feetpos().squaredist(pos) <= guard*guard)
            {
                b.idle = hasenemy ? 2 : 1;
                return true;
            }
            walk++;
        }
        return patrol(d, b, pos, guard, wander, walk);
    }

    bool violence(gameent *d, aistate &b, gameent *e, int pursue)
    {
        if(e && targetable(d, e))
        {
            if(pursue)
            {
                if((b.targtype != AITravel_Affinity || !(pursue%2)) && makeroute(d, b, e->lastnode))
                {
                    d->ai->switchstate(b, AIState_Pursue, AITravel_Player, e->clientnum);
                }
                else if(pursue >= 3)
                {
                    return false; // can't pursue
                }
            }
            if(d->ai->enemy != e->clientnum)
            {
                d->ai->enemyseen = d->ai->enemymillis = lastmillis;
                d->ai->enemy = e->clientnum;
            }
            return true;
        }
        return false;
    }

    bool target(gameent *d, aistate &b, int pursue = 0, bool force = false, float mindist = 0.f)
    {
        static vector<gameent *> hastried; hastried.setsize(0);
        vec dp = d->headpos();
        while(true)
        {
            float dist = 1e16f;
            gameent *t = NULL;
            int atk = guns[d->gunselect].attacks[Act_Shoot];
            for(int i = 0; i < players.length(); i++)
            {
                gameent *e = players[i];
                if(e == d || hastried.find(e) >= 0 || !targetable(d, e))
                {
                    continue;
                }
                vec ep = getaimpos(d, atk, e);
                float v = ep.squaredist(dp);
                if((!t || v < dist) && (mindist <= 0 || v <= mindist) && (force || cansee(d, dp, ep)))
                {
                    t = e;
                    dist = v;
                }
            }
            if(t)
            {
                if(violence(d, b, t, pursue))
                {
                    return true;
                }
                hastried.add(t);
            }
            else
            {
                break;
            }
        }
        return false;
    }

    int isgoodammo(int gun)
    {
        return gun == Gun_Pulse || gun == Gun_Rail;
    }

    bool hasgoodammo(gameent *d)
    {
        static const int goodguns[] = { Gun_Pulse, Gun_Rail };
        for(int i = 0; i < static_cast<int>(sizeof(goodguns)/sizeof(goodguns[0])); ++i)
        {
            if(d->hasammo(goodguns[0]))
            {
                return true;
            }
        }
        return false;
    }

    void assist(gameent *d, aistate &b, vector<interest> &interests, bool all, bool force)
    {
        for(int i = 0; i < players.length(); i++) //loop through all players
        {
            gameent *e = players[i];
            //skip if player is a valid target (don't assist enemies)
            if(e == d || (!all && e->aitype != AI_None) || !(modecheck(gamemode, Mode_Team) && d->team == e->team))
            {
                continue;
            }
            interest &n = interests.add();
            n.state = AIState_Defend;
            n.node = e->lastnode;
            n.target = e->clientnum;
            n.targtype = AITravel_Player;
            n.score = e->o.squaredist(d->o)/(hasgoodammo(d) ? 1e8f : (force ? 1e4f : 1e2f));
        }
    }

    static vector<int> targets;

    bool parseinterests(gameent *d, aistate &b, vector<interest> &interests, bool override, bool ignore)
    {
        while(!interests.empty())
        {
            int q = interests.length()-1;
            for(int i = 0; i < interests.length()-1; ++i)
            {
                if(interests[i].score < interests[q].score)
                {
                    q = i;
                }
            }
            interest n = interests.removeunordered(q);
            bool proceed = true;
            if(!ignore)
            {
                switch(n.state)
                {
                    case AIState_Defend: // don't get into herds
                    {
                        int members = 0;
                        proceed = !checkothers(targets, d, n.state, n.targtype, n.target, true, &members) && members > 1;
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            if(proceed && makeroute(d, b, n.node))
            {
                d->ai->switchstate(b, n.state, n.targtype, n.target);
                return true;
            }
        }
        return false;
    }

    bool find(gameent *d, aistate &b, bool override = false)
    {
        static vector<interest> interests;
        interests.setsize(0);

        if(cmode)
        {
            cmode->aifind(d, b, interests);
        }
        if(modecheck(gamemode, Mode_Team))
        {
            assist(d, b, interests);
        }
        return parseinterests(d, b, interests, override);
    }

    bool findassist(gameent *d, aistate &b, bool override = false)
    {
        static vector<interest> interests;
        interests.setsize(0);
        assist(d, b, interests);
        while(!interests.empty())
        {
            int q = interests.length()-1;
            for(int i = 0; i < interests.length()-1; ++i)
            {
                if(interests[i].score < interests[q].score)
                {
                    q = i;
                }
            }
            interest n = interests.removeunordered(q);
            bool proceed = true;
            switch(n.state)
            {
                case AIState_Defend: // don't get into herds
                {
                    int members = 0;
                    proceed = !checkothers(targets, d, n.state, n.targtype, n.target, true, &members) && members > 1;
                    break;
                }
                default:
                {
                    break;
                }
            }
            if(proceed && makeroute(d, b, n.node))
            {
                d->ai->switchstate(b, n.state, n.targtype, n.target);
                return true;
            }
        }
        return false;
    }

    void damaged(gameent *d, gameent *e)
    {
        if(d->ai && canmove(d) && targetable(d, e)) // see if this ai is interested in a grudge
        {
            aistate &b = d->ai->getstate();
            if(violence(d, b, e))
            {
                return;
            }
        }
        if(checkothers(targets, d, AIState_Defend, AITravel_Player, d->clientnum, true))
        {
            for(int i = 0; i < targets.length(); i++)
            {
                gameent *t = getclient(targets[i]);
                if(!t->ai || !canmove(t) || !targetable(t, e))
                {
                    continue;
                }
                aistate &c = t->ai->getstate();
                if(violence(t, c, e))
                {
                    return;
                }
            }
        }
    }

    void findorientation(vec &o, float yaw, float pitch, vec &pos)
    {
        vec dir;
        vecfromyawpitch(yaw, pitch, 1, 0, dir);
        if(raycubepos(o, dir, pos, 0, Ray_ClipMat|Ray_SkipFirst) == -1)
        {
            pos = dir.mul(2*getworldsize()).add(o); //otherwise 3dgui won't work when outside of map
        }
    }

    void setup(gameent *d)
    {
        d->ai->clearsetup();
        d->ai->reset(true);
        d->ai->lastrun = lastmillis;
        if(forcegun >= 0 && forcegun < Gun_NumGuns)
        {
            d->ai->weappref = forcegun;
        }
        else
        {
            d->ai->weappref = randomint(Gun_NumGuns);
        }
        vec dp = d->headpos();
        findorientation(dp, d->yaw, d->pitch, d->ai->target);
    }

    void spawned(gameent *d)
    {
        if(d->ai)
        {
            setup(d);
        }
    }

    void killed(gameent *d, gameent *e)
    {
        if(d->ai)
        {
            d->ai->reset();
        }
    }

    bool check(gameent *d, aistate &b)
    {
        if(cmode && cmode->aicheck(d, b))
        {
            return true;
        }
        return false;
    }

    int dowait(gameent *d, aistate &b)
    {
        d->ai->clear(true); // ensure they're clean
        if(check(d, b) || find(d, b))
        {
            return 1;
        }
        if(target(d, b, 4, false))
        {
            return 1;
        }
        if(target(d, b, 4, true))
        {
            return 1;
        }
        if(randomnode(d, b, sightmin, 1e16f))
        {
            d->ai->switchstate(b, AIState_Interest, AITravel_Node, d->ai->route[0]);
            return 1;
        }
        return 0; // but don't pop the state
    }

    int dodefend(gameent *d, aistate &b)
    {
        if(d->state == ClientState_Alive)
        {
            switch(b.targtype)
            {
                case AITravel_Node:
                {
                    if(check(d, b))
                    {
                        return 1;
                    }
                    if(iswaypoint(b.target))
                    {
                        return defend(d, b, waypoints[b.target].o) ? 1 : 0;
                    }
                    break;
                }
                case AITravel_Entity:
                {
                    if(check(d, b))
                    {
                        return 1;
                    }
                    if(entities::ents.inrange(b.target))
                    {
                        return defend(d, b, entities::ents[b.target]->o) ? 1 : 0;
                    }
                    break;
                }
                case AITravel_Affinity:
                {
                    if(cmode)
                    {
                        return cmode->aidefend(d, b) ? 1 : 0;
                    }
                    break;
                }
                case AITravel_Player:
                {
                    if(check(d, b))
                    {
                        return 1;
                    }
                    gameent *e = getclient(b.target);
                    if(e && e->state == ClientState_Alive)
                    {
                        return defend(d, b, e->feetpos()) ? 1 : 0;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        return 0;
    }

    int dointerest(gameent *d, aistate &b)
    {
        if(d->state != ClientState_Alive)
        {
            return 0;
        }
        switch(b.targtype)
        {
            case AITravel_Node: // this is like a wait state without sitting still..
                if(check(d, b) || find(d, b))
                {
                    return 1;
                }
                if(target(d, b, 4, true))
                {
                    return 1;
                }
                if(iswaypoint(b.target) && vec(waypoints[b.target].o).sub(d->feetpos()).magnitude() > closedist)
                {
                    return makeroute(d, b, waypoints[b.target].o) ? 1 : 0;
                }
                break;
            case AITravel_Entity:
                if(entities::ents.inrange(b.target))
                {
                    extentity &e = *static_cast<extentity *>(entities::ents[b.target]);
                    return 0;
                    return makeroute(d, b, e.o) ? 1 : 0;
                }
                break;
        }
        return 0;
    }

    int dopursue(gameent *d, aistate &b)
    {
        if(d->state == ClientState_Alive)
        {
            switch(b.targtype)
            {
                case AITravel_Node:
                {
                    if(check(d, b))
                    {
                        return 1;
                    }
                    if(iswaypoint(b.target))
                    {
                        return defend(d, b, waypoints[b.target].o) ? 1 : 0;
                    }
                    break;
                }

                case AITravel_Affinity:
                {
                    if(cmode)
                    {
                        return cmode->aipursue(d, b) ? 1 : 0;
                    }
                    break;
                }

                case AITravel_Player:
                {
                    //if(check(d, b)) return 1;
                    gameent *e = getclient(b.target);
                    if(e && e->state == ClientState_Alive)
                    {
                        int atk = guns[d->gunselect].attacks[Act_Shoot];
                        float guard = sightmin,
                              wander = attacks[atk].range;
                        return patrol(d, b, e->feetpos(), guard, wander) ? 1 : 0;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        return 0;
    }

    int closenode(gameent *d)
    {
        vec pos = d->feetpos();
        int node1 = -1,
            node2 = -1;
        float mindist1 = closedist*closedist, //close_dist not closed_ist
              mindist2 = closedist*closedist;
        for(int i = 0; i < d->ai->route.length(); i++)
        {
            if(iswaypoint(d->ai->route[i]))
            {
                vec epos = waypoints[d->ai->route[i]].o;
                float dist = epos.squaredist(pos);
                if(dist > fardist*fardist)
                {
                    continue;
                }
                int entid = obstacles.remap(d, d->ai->route[i], epos);
                if(entid >= 0)
                {
                    if(entid != i)
                    {
                        dist = epos.squaredist(pos);
                    }
                    if(dist < mindist1)
                    {
                        node1 = i;
                        mindist1 = dist;
                    }
                }
                else if(dist < mindist2)
                {
                    node2 = i;
                    mindist2 = dist;
                }
            }
        }
        return node1 >= 0 ? node1 : node2;
    }

    int wpspot(gameent *d, int n, bool check = false)
    {
        if(iswaypoint(n))
        {
            for(int k = 0; k < 2; ++k)
            {
                vec epos = waypoints[n].o;
                int entid = obstacles.remap(d, n, epos, k!=0);
                if(iswaypoint(entid))
                {
                    d->ai->spot = epos;
                    d->ai->targnode = entid;
                    return !check || d->feetpos().squaredist(epos) > minwpdist*minwpdist ? 1 : 2;
                }
            }
        }
        return 0;
    }

    int randomlink(gameent *d, int n)
    {
        if(iswaypoint(n) && waypoints[n].haslinks())
        {
            waypoint &w = waypoints[n];
            static vector<int> linkmap; linkmap.setsize(0);
            for(int i = 0; i < maxwaypointlinks; ++i)
            {
                if(!w.links[i])
                {
                    break;
                }
                if(iswaypoint(w.links[i]) && !d->ai->hasprevnode(w.links[i]) && d->ai->route.find(w.links[i]) < 0)
                {
                    linkmap.add(w.links[i]);
                }
            }
            if(!linkmap.empty())
            {
                return linkmap[randomint(linkmap.length())];
            }
        }
        return -1;
    }

    bool anynode(gameent *d, aistate &b, int len = numprevnodes)
    {
        if(iswaypoint(d->lastnode))
        {
            for(int k = 0; k < 2; ++k)
            {
                d->ai->clear(k ? true : false);
                int n = randomlink(d, d->lastnode);
                if(wpspot(d, n))
                {
                    d->ai->route.add(n);
                    d->ai->route.add(d->lastnode);
                    for(int i = 0; i < len; ++i)
                    {
                        n = randomlink(d, n);
                        if(iswaypoint(n))
                        {
                            d->ai->route.insert(0, n);
                        }
                        else
                        {
                            break;
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }

    bool checkroute(gameent *d, int n)
    {
        if(d->ai->route.empty() || !d->ai->route.inrange(n))
        {
            return false;
        }
        int last = d->ai->lastcheck ? lastmillis-d->ai->lastcheck : 0;
        if(last < 500 || n < 3)
        {
            return false; // route length is too short
        }
        d->ai->lastcheck = lastmillis;
        int w = iswaypoint(d->lastnode) ? d->lastnode : d->ai->route[n], c = min(n-1, numprevnodes);
        // check ahead to see if we need to go around something
        for(int j = 0; j < c; ++j)
        {
            int p = n-j-1,
                v = d->ai->route[p];
            if(d->ai->hasprevnode(v) || obstacles.find(v, d)) // something is in the way, try to remap around it
            {
                int m = p-1;
                if(m < 3)
                {
                    return false; // route length is too short from this point
                }
                for(int i = m; --i >= 0;) //note reverse iteration
                {
                    int t = d->ai->route[i];
                    if(!d->ai->hasprevnode(t) && !obstacles.find(t, d))
                    {
                        static vector<int> remap; remap.setsize(0);
                        if(route(d, w, t, remap, obstacles))
                        { // kill what we don't want and put the remap in
                            while(d->ai->route.length() > i)
                            {
                                d->ai->route.pop();
                            }
                            for(int k = 0; k < remap.length(); k++)
                            {
                                d->ai->route.add(remap[k]);
                            }
                            return true;
                        }
                        return false; // we failed
                    }
                }
                return false;
            }
        }
        return false;
    }

    bool hunt(gameent *d, aistate &b)
    {
        if(!d->ai->route.empty())
        {
            int n = closenode(d);
            if(d->ai->route.inrange(n) && checkroute(d, n))
            {
                n = closenode(d);
            }
            if(d->ai->route.inrange(n))
            {
                if(!n)
                {
                    switch(wpspot(d, d->ai->route[n], true))
                    {
                        case 2:
                        {
                            d->ai->clear(false);
                        }
                        [[fallthrough]];
                        case 1:
                        {
                            return true; // not close enough to pop it yet
                        }
                        case 0:
                        default:
                        {
                            break;
                        }
                    }
                }
                else
                {
                    while(d->ai->route.length() > n+1)
                    {
                        d->ai->route.pop(); // waka-waka-waka-waka
                    }
                    int m = n-1; // next, please!
                    if(d->ai->route.inrange(m) && wpspot(d, d->ai->route[m]))
                    {
                        return true;
                    }
                }
            }
        }
        b.override = false;
        return anynode(d, b);
    }

    void jumpto(gameent *d, aistate &b, const vec &pos)
    {
        vec off = vec(pos).sub(d->feetpos()),
            dir(off.x, off.y, 0);
        bool sequenced = d->ai->blockseq || d->ai->targseq,
             offground = d->timeinair && !d->inwater,
             jump = !offground && lastmillis >= d->ai->jumpseed && (sequenced || off.z >= jumpmin || lastmillis >= d->ai->jumprand);
        if(jump)
        {
            vec old = d->o;
            d->o = vec(pos).addz(d->eyeheight);
            if(collide(d, vec(0, 0, 1)))
            {
                jump = false;
            }
            d->o = old;
            if(jump)
            {
                float squareradius = 324; //324 = 18^2; float because squaredist is also a float
                for(int i = 0; i < entities::ents.length(); i++)
                {
                    if(entities::ents[i]->type == GamecodeEnt_Jumppad)
                    {
                        extentity &e = *entities::ents[i];
                        if(e.o.squaredist(pos) <= squareradius)
                        {
                            jump = false;
                            break;
                        }
                    }
                }
            }
        }
        if(jump)
        {
            d->jumping = true;
            int seed = (111-d->skill)*(d->inwater ? 3 : 5);
            d->ai->jumpseed = lastmillis+seed+randomint(seed);
            seed *= b.idle ? 50 : 25;
            d->ai->jumprand = lastmillis+seed+randomint(seed);
        }
    }

    void fixfullrange(float &yaw, float &pitch, float &roll, bool full)
    {
        if(full) //modulus check if full range allowed
        {
            while(pitch < -180.0f) //if pitch is under -180, reset to 180
            {
                pitch += 360.0f;
            }
            while(pitch >= 180.0f) //if pitch is over 180, reset to -180
            {
                pitch -= 360.0f;
            }
            while(roll < -180.0f) //if roll is under -180, reset to 180
            {
                roll += 360.0f;
            }
            while(roll >= 180.0f) //if rill is over 180, reset to -180
            {
                roll -= 360.0f;
            }
        }
        else //if not, clamp pitch/roll to 180 degree max
        {
            if(pitch > 89.9f) //if pitch >=90 set to 90
            {
                pitch = 89.9f;
            }
            if(pitch < -89.9f)//if pitch <= -90 set to -90
            {
                pitch = -89.9f;
            }
            if(roll > 89.9f)//if roll >= 90 set to 90
            {
                roll = 89.9f;
            }
            if(roll < -89.9f)//if roll <=-90 set to -90
            {
                roll = -89.9f;
            }
        }
        while(yaw < 0.0f) //keep yaw within 0..360
        {
            yaw += 360.0f;
        }
        while(yaw >= 360.0f) //keep yaw within 0..360
        {
            yaw -= 360.0f;
        }
    }

    void fixrange(float &yaw, float &pitch)
    {
        float r = 0.f;
        fixfullrange(yaw, pitch, r, false);
    }

    void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch)
    {
        float dist = from.dist(pos);
        yaw = -atan2(pos.x-from.x, pos.y-from.y)/RAD;
        pitch = asin((pos.z-from.z)/dist)/RAD;
    }

    void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float frame, float scale)
    {
        if(yaw < targyaw-180.0f)
        {
            yaw += 360.0f;
        }
        if(yaw > targyaw+180.0f)
        {
            yaw -= 360.0f;
        }
        float offyaw = fabs(targyaw-yaw)*frame,
              offpitch = fabs(targpitch-pitch)*frame*scale;
        if(targyaw > yaw)
        {
            yaw += offyaw;
            if(targyaw < yaw)
            {
                yaw = targyaw;
            }
        }
        else if(targyaw < yaw)
        {
            yaw -= offyaw;
            if(targyaw > yaw)
            {
                yaw = targyaw;
            }
        }
        if(targpitch > pitch)
        {
            pitch += offpitch;
            if(targpitch < pitch)
            {
                pitch = targpitch;
            }
        }
        else if(targpitch < pitch)
        {
            pitch -= offpitch;
            if(targpitch > pitch)
            {
                pitch = targpitch;
            }
        }
        fixrange(yaw, pitch);
    }

    bool lockon(gameent *d, int atk, gameent *e, float maxdist)
    {
        if(attacks[atk].action == Act_Melee && !d->blocked && !d->timeinair)
        {
            vec dir = vec(e->o).sub(d->o);
            float xydist = dir.x*dir.x+dir.y*dir.y,
                  zdist = dir.z*dir.z,
                  mdist = maxdist*maxdist,
                  ddist = d->radius*d->radius+e->radius*e->radius;
            if(zdist <= ddist && xydist >= ddist+4 && xydist <= mdist+ddist)
            {
                return true;
            }
        }
        return false;
    }
    int process(gameent *d, aistate &b)
    {
        int result = 0,
            stupify = d->skill <= 10+randomint(15) ? randomint(d->skill*1000) : 0,
            skmod = 101-d->skill;
        float frame = d->skill <= 100 ? static_cast<float>(lastmillis-d->ai->lastrun)/static_cast<float>(max(skmod,1)*10) : 1;
        vec dp = d->headpos();
        bool idle = b.idle == 1 || (stupify && stupify <= skmod);
        d->ai->dontmove = false;
        if(idle)
        {
            d->ai->lastaction = d->ai->lasthunt = lastmillis;
            d->ai->dontmove = true;
            d->ai->spot = vec(0, 0, 0);
        }
        else if(hunt(d, b))
        {
            getyawpitch(dp, vec(d->ai->spot).addz(d->eyeheight), d->ai->targyaw, d->ai->targpitch);
            d->ai->lasthunt = lastmillis;
        }
        else
        {
            idle = d->ai->dontmove = true;
            d->ai->spot = vec(0, 0, 0);
        }
        if(!d->ai->dontmove)
        {
            jumpto(d, b, d->ai->spot);
        }
        gameent *e = getclient(d->ai->enemy);
        bool enemyok = e && targetable(d, e);
        if(!enemyok || d->skill >= 50)
        {
            gameent *f = static_cast<gameent *>(intersectclosest(dp, d->ai->target, d, 1));
            if(f)
            {
                if(targetable(d, f))
                {
                    if(!enemyok)
                    {
                        violence(d, b, f);
                    }
                    enemyok = true;
                    e = f;
                }
                else
                {
                    enemyok = false;
                }
            }
            else if(!enemyok && target(d, b, 0, false, sightmin))
            {
                enemyok = (e = getclient(d->ai->enemy)) != NULL;
            }
        }
        if(enemyok)
        {
            int atk = guns[d->gunselect].attacks[Act_Shoot];
            vec ep = getaimpos(d, atk, e);
            float yaw, pitch;
            getyawpitch(dp, ep, yaw, pitch);
            fixrange(yaw, pitch);
            bool insight = cansee(d, dp, ep),
                 hasseen = d->ai->enemyseen && lastmillis-d->ai->enemyseen <= (d->skill*10)+3000,
                 quick = d->ai->enemyseen && lastmillis-d->ai->enemyseen <= skmod+30;
            if(insight)
            {
                d->ai->enemyseen = lastmillis;
            }
            if(idle || insight || hasseen || quick)
            {
                float sskew = insight || d->skill > 100 ? 1.5f : (hasseen ? 1.f : 0.5f);
                if(insight && lockon(d, atk, e, 16))
                {
                    d->ai->targyaw = yaw;
                    d->ai->targpitch = pitch;
                    if(!idle)
                    {
                        frame *= 2;
                    }
                    d->ai->becareful = false;
                }
                scaleyawpitch(d->yaw, d->pitch, yaw, pitch, frame, sskew);
                if(insight || quick)
                {
                    if(canshoot(d, atk, e) && hastarget(d, atk, b, e, yaw, pitch, dp.squaredist(ep)))
                    {
                        d->attacking = attacks[atk].action;
                        d->ai->lastaction = lastmillis;
                        result = 3;
                    }
                    else
                    {
                        result = 2;
                    }
                }
                else
                {
                    result = 1;
                }
            }
            else
            {
                if(!d->ai->enemyseen || lastmillis-d->ai->enemyseen > (d->skill*50)+3000)
                {
                    d->ai->enemy = -1;
                    d->ai->enemyseen = d->ai->enemymillis = 0;
                }
                enemyok = false;
                result = 0;
            }
        }
        else
        {
            if(!enemyok)
            {
                d->ai->enemy = -1;
                d->ai->enemyseen = d->ai->enemymillis = 0;
            }
            enemyok = false;
            result = 0;
        }
        fixrange(d->ai->targyaw, d->ai->targpitch);
        if(!result)
        {
            scaleyawpitch(d->yaw, d->pitch, d->ai->targyaw, d->ai->targpitch, frame*0.25f, 1.f);
        }
        if(d->ai->becareful && d->physstate == PhysEntState_Fall)
        {
            float offyaw, offpitch;
            vectoyawpitch(d->vel, offyaw, offpitch);
            offyaw -= d->yaw;
            offpitch -= d->pitch;
            if(fabs(offyaw)+fabs(offpitch) >= 135)
            {
                d->ai->becareful = false;
            }
            else if(d->ai->becareful)
            {
                d->ai->dontmove = true;
            }
        }
        else
        {
            d->ai->becareful = false;
        }
        if(d->ai->dontmove)
        {
            d->move = d->strafe = 0;
        }
        else
        { // our guys move one way.. but turn another?! :)
            const struct aimdir
            {
                int move, strafe, offset;
            } aimdirs[8] =
            {
                {  1,   0,   0 },
                {  1,  -1,  45 },
                {  0,  -1,  90 },
                { -1,  -1, 135 },
                { -1,   0, 180 },
                { -1,   1, 225 },
                {  0,   1, 270 },
                {  1,   1, 315 }
            };
            float yaw = d->ai->targyaw-d->yaw;
            //reset yaws to within 0-360 bounds
            while(yaw < 0.0f)
            {
                yaw += 360.0f;
            }
            while(yaw >= 360.0f)
            {
                yaw -= 360.0f;
            }
            //set r to one of the 8 aim dirs depending on direction
            int r = std::clamp(static_cast<int>(floor((yaw+22.5f)/45.0f))&7, 0, 7);
            //get an aim dir from the assigned r above
            const aimdir &ad = aimdirs[r];
            //set move/strafe dirs to this aimdir
            d->move = ad.move;
            d->strafe = ad.strafe;
        }
        findorientation(dp, d->yaw, d->pitch, d->ai->target);
        return result;
    }

    bool hasrange(gameent *d, gameent *e, int weap)
    {
        if(!e)
        {
            return true;
        }
        if(targetable(d, e))
        {
            int atk = guns[weap].attacks[Act_Shoot];
            vec ep = getaimpos(d, atk, e);
            float dist = ep.squaredist(d->headpos());
            if(attackrange(d, atk, dist))
            {
                return true;
            }
        }
        return false;
    }

    bool request(gameent *d, aistate &b)
    {
        gameent *e = getclient(d->ai->enemy);
        if(!d->hasammo(d->gunselect) || !hasrange(d, e, d->gunselect) || (d->gunselect != d->ai->weappref && (!isgoodammo(d->gunselect) || d->hasammo(d->ai->weappref))))
        {
            static const int gunprefs[] =
            {
                Gun_Pulse,
                Gun_Rail
            };
            int gun = -1;
            if(d->hasammo(d->ai->weappref) && hasrange(d, e, d->ai->weappref))
            {
                gun = d->ai->weappref;
            }
            else
            {
                for(int i = 0; i < static_cast<int>(sizeof(gunprefs)/sizeof(gunprefs[0])); ++i)
                {
                    if(d->hasammo(gunprefs[i]) && hasrange(d, e, gunprefs[i]))
                    {
                        gun = gunprefs[i];
                        break;
                    }
                }
            }
            if(gun >= 0 && gun != d->gunselect)
            {
                gunselect(gun, d);
            }
        }
        return process(d, b) >= 2;
    }

    void timeouts(gameent *d, aistate &b)
    {
        if(d->blocked)
        {
            d->ai->blocktime += lastmillis-d->ai->lastrun;
            if(d->ai->blocktime > (d->ai->blockseq+1)*1000)
            {
                d->ai->blockseq++;
                switch(d->ai->blockseq)
                {
                    case 1:
                    case 2:
                    case 3:
                    {
                        if(entities::ents.inrange(d->ai->targnode))
                        {
                            d->ai->addprevnode(d->ai->targnode);
                        }
                        d->ai->clear(false);
                        break;
                    }
                    case 4:
                    {
                        d->ai->reset(true);
                        break;
                    }
                    case 5:
                    {
                        d->ai->reset(false);
                        break;
                    }
                    case 6:
                    default:
                    {
                        suicide(d);
                        return;
                    }// this is our last resort..
                }
            }
        }
        else
        {
            d->ai->blocktime = d->ai->blockseq = 0;
        }
        if(d->ai->targnode == d->ai->targlast)
        {
            d->ai->targtime += lastmillis-d->ai->lastrun;
            if(d->ai->targtime > (d->ai->targseq+1)*1000)
            {
                d->ai->targseq++;
                switch(d->ai->targseq)
                {
                    case 1:
                    case 2:
                    case 3:
                    {
                        if(entities::ents.inrange(d->ai->targnode)) d->ai->addprevnode(d->ai->targnode);
                        {
                            d->ai->clear(false);
                        }
                        break;
                    }
                    case 4:
                    {
                        d->ai->reset(true);
                        break;
                    }
                    case 5:
                    {
                        d->ai->reset(false);
                        break;
                    }
                    case 6:
                    default:
                    {
                        suicide(d);
                        return;
                    } // this is our last resort..
                }
            }
        }
        else
        {
            d->ai->targtime = d->ai->targseq = 0;
            d->ai->targlast = d->ai->targnode;
        }

        if(d->ai->lasthunt)
        {
            int millis = lastmillis-d->ai->lasthunt;
            if(millis <= 1000)
            {
                d->ai->tryreset = false;
                d->ai->huntseq = 0;
            }
            else if(millis > (d->ai->huntseq+1)*1000)
            {
                d->ai->huntseq++;
                switch(d->ai->huntseq)
                {
                    case 1:
                    {
                        d->ai->reset(true);
                        break;
                    }
                    case 2:
                    {
                        d->ai->reset(false);
                        break;
                    }
                    case 3:
                    default:
                    {
                        suicide(d);
                        return;
                    } // this is our last resort..
                }
            }
        }
    }

    void logic(gameent *d, aistate &b, bool run)
    {
        bool allowmove = canmove(d) && b.type != AIState_Wait;
        if(d->state != ClientState_Alive || !allowmove)
        {
            d->stopmoving();
        }
        if(d->state == ClientState_Alive)
        {
            if(allowmove)
            {
                if(!request(d, b)) target(d, b, 0, b.idle ? true : false);
                {
                    shoot(d, d->ai->target);
                }
            }
            if(!intermission)
            {
                if(d->ragdoll)
                {
                    cleanragdoll(d);
                }
                moveplayer(d, 10, true);
                if(allowmove && !b.idle)
                {
                    timeouts(d, b);
                }
                entities::checkitems(d);
                if(cmode)
                {
                    cmode->checkitems(d);
                }
            }
        }
        else if(d->state == ClientState_Dead)
        {
            if(d->ragdoll)
            {
                moveragdoll(d);
            }
            else if(lastmillis-d->lastpain<2000)
            {
                d->move = d->strafe = 0;
                moveplayer(d, 10, false);
            }
        }
        d->attacking = Act_Idle;
        d->jumping = false;
    }

    void avoid()
    {
        // guess as to the radius of ai and other critters relying on the avoid set for now
        float guessradius = player1->radius;
        obstacles.clear();
        for(int i = 0; i < players.length(); i++)
        {
            dynent *d = players[i];
            if(d->state != ClientState_Alive)
            {
                continue;
            }
            obstacles.avoidnear(d, d->o.z + d->aboveeye + 1, d->feetpos(), guessradius + d->radius);
        }
        extern avoidset wpavoid;
        obstacles.add(wpavoid);
        avoidweapons(obstacles, guessradius);
    }

    void think(gameent *d, bool run)
    {
        // the state stack works like a chain of commands, certain commands simply replace each other
        // others spawn new commands to the stack the ai reads the top command from the stack and executes
        // it or pops the stack and goes back along the history until it finds a suitable command to execute
        bool cleannext = false;
        if(d->ai->state.empty())
        {
            d->ai->addstate(AIState_Wait);
        }
        for(int i = d->ai->state.length(); --i >=0;) //note reverse iteration
        {
            aistate &c = d->ai->state[i];
            if(cleannext)
            {
                c.millis = lastmillis;
                c.override = false;
                cleannext = false;
            }
            if(d->state == ClientState_Dead && d->respawned!=d->lifesequence && (!cmode || cmode->respawnwait(d) <= 0) && lastmillis - d->lastpain >= 500)
            {
                addmsg(NetMsg_TrySpawn, "rc", d);
                d->respawned = d->lifesequence;
            }
            else if(d->state == ClientState_Alive && run)
            {
                int result = 0;
                c.idle = 0;
                switch(c.type)
                {
                    case AIState_Wait:
                    {
                        result = dowait(d, c);
                        break;
                    }
                    case AIState_Defend:
                    {
                        result = dodefend(d, c);
                        break;
                    }
                    case AIState_Pursue:
                    {
                        result = dopursue(d, c);
                        break;
                    }
                    case AIState_Interest:
                    {
                        result = dointerest(d, c);
                        break;
                    }
                    default:
                    {
                        result = 0;
                        break;
                    }
                }
                if(result <= 0)
                {
                    if(c.type != AIState_Wait)
                    {
                        switch(result)
                        {
                            case 0:
                            default:
                            {
                                d->ai->removestate(i);
                                cleannext = true;
                                break;
                            }
                            case -1:
                            {
                                i = d->ai->state.length()-1;
                                break;
                            }
                        }
                        continue; // shouldn't interfere
                    }
                }
            }
            logic(d, c, run);
            break;
        }
        if(d->ai->trywipe)
        {
            d->ai->wipe();
        }
        d->ai->lastrun = lastmillis;
    }

    void drawroute(gameent *d, float amt = 1.f)
    {
        int last = -1;
        for(int i = d->ai->route.length(); --i >=0;) //note reverse iteration
        {
            if(d->ai->route.inrange(last))
            {
                int index = d->ai->route[i],
                    prev = d->ai->route[last];
                if(iswaypoint(index) && iswaypoint(prev))
                {
                    waypoint &e = waypoints[index],
                             &f = waypoints[prev];
                    vec fr = f.o,
                        dr = e.o;
                    fr.z += amt;
                    dr.z += amt;
                    particle_flare(fr, dr, 1, Part_Streak, 0xFFFFFF);
                }
            }
            last = i;
        }
        if(aidebug >= 5)
        {
            vec pos = d->feetpos();
            if(d->ai->spot != vec(0, 0, 0))
            {
                particle_flare(pos, d->ai->spot, 1, Part_Streak, 0x00FFFF);
            }
            if(iswaypoint(d->ai->targnode))
            {
                particle_flare(pos, waypoints[d->ai->targnode].o, 1, Part_Streak, 0xFF00FF);
            }
            if(iswaypoint(d->lastnode))
            {
                particle_flare(pos, waypoints[d->lastnode].o, 1, Part_Streak, 0xFFFF00);
            }
            for(int i = 0; i < numprevnodes; ++i)
            {
                if(iswaypoint(d->ai->prevnodes[i]))
                {
                    particle_flare(pos, waypoints[d->ai->prevnodes[i]].o, 1, Part_Streak, 0x884400);
                    pos = waypoints[d->ai->prevnodes[i]].o;
                }
            }
        }
    }

    VAR(showwaypoints, 0, 0, 1); //display waypoint locations in edit mode
    VAR(showwaypointsradius, 0, 200, 10000); //maximum distance to display (200 = 25m)

    const char *stnames[AIState_Max] = {
        "wait", "defend", "pursue", "interest"
    }, *sttypes[AITravel_Max+1] = {
        "none", "node", "player", "affinity", "entity"
    };
    void render()
    {
        if(aidebug > 1)
        {
            int total = 0,
                alive = 0;
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i]->ai)
                {
                    total++;
                }
            }
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i]->state == ClientState_Alive && players[i]->ai)
                {
                    gameent *d = players[i];
                    vec pos = d->abovehead();
                    pos.z += 3;
                    alive++;
                    if(aidebug >= 4)
                    {
                        drawroute(d, 4.f*(static_cast<float>(alive)/static_cast<float>(total)));
                    }
                    if(aidebug >= 3)
                    {
                        DEF_FORMAT_STRING(q, "node: %d route: %d (%d)",
                            d->lastnode,
                            !d->ai->route.empty() ? d->ai->route[0] : -1,
                            d->ai->route.length()
                        );
                        particle_textcopy(pos, q, Part_Text, 1);
                        pos.z += 2;
                    }
                    bool top = true;
                    for(int i = d->ai->state.length(); --i >=0;) //note reverse iteration
                    {
                        aistate &b = d->ai->state[i];
                        DEF_FORMAT_STRING(s, "%s%s (%d ms) %s:%d",
                            top ? "\fg" : "\fy",
                            stnames[b.type],
                            lastmillis-b.millis,
                            sttypes[b.targtype+1], b.target
                        );
                        particle_textcopy(pos, s, Part_Text, 1);
                        pos.z += 2;
                        if(top)
                        {
                            if(aidebug >= 3)
                            {
                                 top = false;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    if(aidebug >= 3)
                    {
                        if(d->ai->weappref >= 0 && d->ai->weappref < Gun_NumGuns)
                        {
                            particle_textcopy(pos, guns[d->ai->weappref].name, Part_Text, 1);
                            pos.z += 2;
                        }
                        gameent *e = getclient(d->ai->enemy);
                        if(e)
                        {
                            particle_textcopy(pos, colorname(e), Part_Text, 1);
                            pos.z += 2;
                        }
                    }
                }
            }
            if(aidebug >= 4)
            {
                int cur = 0;
                for(int i = 0; i < obstacles.obstacles.length(); i++)
                {
                    const avoidset::obstacle &ob = obstacles.obstacles[i];
                    int next = cur + ob.numwaypoints;
                    for(; cur < next; cur++)
                    {
                        int ent = obstacles.waypoints[cur];
                        if(iswaypoint(ent))
                        {
                            regular_particle_splash(Part_Edit, 2, 40, waypoints[ent].o, 0xFF6600, 1.5f);
                        }
                    }
                    cur = next;
                }
            }
        }
        if(showwaypoints || aidebug >= 6)
        {
            vector<int> close;
            int len = waypoints.length();
            if(showwaypointsradius)
            {
                findwaypointswithin(camera1->o, 0, showwaypointsradius, close);
                len = close.length();
            }
            for(int i = 0; i < len; ++i)
            {
                waypoint &w = waypoints[showwaypointsradius ? close[i] : i];
                for(int j = 0; j < maxwaypointlinks; ++j)
                {
                    int link = w.links[j];
                    if(!link)
                    {
                        break;
                    }
                    particle_flare(w.o, waypoints[link].o, 1, Part_Streak, 0x0000FF);
                }
            }
        }
    }
}

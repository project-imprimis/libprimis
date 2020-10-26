// creation of scoreboard
#include "game.h"

namespace game
{
    VARP(showservinfo, 0, 1, 1);
    VARP(showclientnum, 0, 0, 1);
    VARP(showpj, 0, 0, 1);
    VARP(showping, 0, 1, 1);
    VARP(showspectators, 0, 1, 1);
    VARP(highlightscore, 0, 1, 1);
    VARP(showconnecting, 0, 0, 1);
    VARP(showfrags, 0, 1, 1);

    static teaminfo teaminfos[maxteams];

    void clearteaminfo()
    {
        for(int i = 0; i < maxteams; ++i)
        {
            teaminfos[i].reset();
        }
    }

    void setteaminfo(int team, int frags)
    {
        if(!validteam(team))
        {
            return;
        }
        teaminfo &t = teaminfos[team-1];
        t.frags = frags;
    }

    //orders a pair of players' rank according to different criteria depending on mode
    //spectators are ordered alphabetically
    //ctf players are ordered by captures
    //non-ctf is ordered by kills
    static inline bool playersort(const gameent *a, const gameent *b)
    {
        if(a->state==ClientState_Spectator)
        {
            if(b->state==ClientState_Spectator)
            {
                return strcmp(a->name, b->name) < 0;
            }
            else
            {
                return false;
            }
        }
        else if(b->state==ClientState_Spectator)
        {
            return true;
        }
        if(modecheck(gamemode, Mode_CTF))
        {
            if(a->flags > b->flags)
            {
                return true;
            }
            if(a->flags < b->flags)
            {
                return false;
            }
        }
        if(a->frags > b->frags)
        {
            return true;
        }
        if(a->frags < b->frags)
        {
            return false;
        }
        return strcmp(a->name, b->name) < 0;
    }

    //takes a vector of gameents and returns the one with the most kills
    void getbestplayers(vector<gameent *> &best)
    {
        for(int i = 0; i < players.length(); i++)
        {
            gameent *o = players[i];
            if(o->state!=ClientState_Spectator)
            {
                best.add(o);
            }
        }
        best.sort(playersort);
        while(best.length() > 1 && best.last()->frags < best[0]->frags)
        {
            best.drop();
        }
    }

    //taes a vector of team numbers and returns the one with the most frags
    void getbestteams(vector<int> &best)
    {
        if(cmode && cmode->hidefrags())
        {
            vector<teamscore> teamscores;
            cmode->getteamscores(teamscores);
            teamscores.sort(teamscore::compare);
            //loop through and drop teams
            while(teamscores.length() > 1 && teamscores.last().score < teamscores[0].score)
            {
                teamscores.drop();
            }
            for(int i = 0; i < teamscores.length(); i++)
            {
                best.add(teamscores[i].team);
            }
        }
        else
        {
            int bestfrags = INT_MIN;
            for(int i = 0; i < maxteams; ++i)
            {
                teaminfo &t = teaminfos[i];
                bestfrags = max(bestfrags, t.frags);
            }
            for(int i = 0; i < maxteams; ++i)
            {
                teaminfo &t = teaminfos[i];
                if(t.frags >= bestfrags)
                {
                    best.add(1+i);
                }
            }
        }
    }

    static vector<gameent *> teamplayers[1+maxteams], spectators;

    static void groupplayers()
    {
        for(int i = 0; i < 1+maxteams; ++i)
        {
            teamplayers[i].setsize(0);
        }
        spectators.setsize(0);
        for(int i = 0; i < players.length(); i++)
        {
            gameent *o = players[i];
            if(!showconnecting && !o->name[0])
            {
                continue;
            }
            if(o->state==ClientState_Spectator)
            {
                spectators.add(o);
                continue;
            }
            int team = modecheck(gamemode, Mode_Team) && validteam(o->team) ? o->team : 0;
            teamplayers[team].add(o);
        }
        for(int i = 0; i < 1+maxteams; ++i)
        {
            teamplayers[i].sort(playersort);
        }
        spectators.sort(playersort);
    }

    void removegroupedplayer(gameent *d)
    {
        for(int i = 0; i < 1+maxteams; ++i)
        {
            teamplayers[i].removeobj(d);
        }
        spectators.removeobj(d);
    }

    void refreshscoreboard()
    {
        groupplayers();
    }
//scoreboard commands
    COMMAND(refreshscoreboard, "");
    ICOMMAND(numscoreboard, "i", (int *team), intret(*team < 0 ? spectators.length() : (*team <= maxteams ? teamplayers[*team].length() : 0)));
    ICOMMAND(loopscoreboard, "rie", (ident *id, int *team, uint *body),
    {
        if(*team > maxteams)
        {
            return;
        }
        LOOP_START(id, stack);
        vector<gameent *> &p = *team < 0 ? spectators : teamplayers[*team];
        for(int i = 0; i < p.length(); i++)
        {
            loopiter(id, stack, p[i]->clientnum);
            execute(body);
        }
        loopend(id, stack);
    });

    ICOMMAND(scoreboardstatus, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            int status = d->state!=ClientState_Dead ? 0xFFFFFF : 0x606060;
            if(d->privilege)
            {
                status = d->privilege>=Priv_Admin ? 0xFF8000 : 0x40FF80;
                if(d->state==ClientState_Dead)
                {
                    status = (status>>1)&0x7F7F7F;
                }
            }
            intret(status);
        }
    });
    //scoreboard packet jump
    ICOMMAND(scoreboardpj, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d && d != player1)
        {
            if(d->state==ClientState_Lagged)
            {
                result("LAG");
            }
            else
            {
                intret(d->plag);
            }
        }
    });

    ICOMMAND(scoreboardping, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            if(!showpj && d->state==ClientState_Lagged)
            {
                result("LAG");
            }
            else
            {
                intret(d->ping);
            }
        }
    });
//scoreboard booleans
    ICOMMAND(scoreboardshowfrags, "", (), intret(cmode && cmode->hidefrags() && !showfrags ? 0 : 1));
    ICOMMAND(scoreboardshowclientnum, "", (), intret(showclientnum || player1->privilege>=Priv_Master ? 1 : 0));
    ICOMMAND(scoreboardmultiplayer, "", (), intret(multiplayer || demoplayback ? 1 : 0));

    ICOMMAND(scoreboardhighlight, "i", (int *cn),
        intret(*cn == player1->clientnum && highlightscore && (multiplayer || demoplayback || players.length() > 1) ? 0x808080 : 0));

    ICOMMAND(scoreboardservinfo, "", (),
    {
        if(!showservinfo)
        {
            return;
        }
        const ENetAddress *address = connectedpeer();
        if(address && player1->clientnum >= 0)
        {
            if(servdesc[0])
            {
                result(servdesc);
            }
            else
            {
                string hostname;
                if(enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0)
                {
                    result(tempformatstring("%s:%d", hostname, address->port));
                }
            }
        }
    });

    ICOMMAND(scoreboardmode, "", (),
    {
        result(server::modeprettyname(gamemode));
    });

    ICOMMAND(scoreboardmap, "", (),
    {
        const char *mname = getclientmap();
        result(mname[0] ? mname : "[new map]");
    });

    ICOMMAND(scoreboardtime, "", (),
    {
        if(!modecheck(gamemode, Mode_Untimed) && getclientmap() && (maplimit >= 0 || intermission))
        {
            if(intermission)
            {
                result("intermission");
            }
            else
            {
                int secs = max(maplimit-lastmillis, 0)/1000;
                result(tempformatstring("%d:%02d", secs/60, secs%60));
            }
        }
    });

    ICOMMAND(getteamscore, "i", (int *team),
    {
        if(modecheck(gamemode, Mode_Team) && validteam(*team))
        {
            if(cmode && cmode->hidefrags())
            {
                intret(cmode->getteamscore(*team));
            }
            else
            {
                intret(teaminfos[*team-1].frags);
            }
        }
    });

    void showscores(bool on)
    {
        UI::holdui("scoreboard", on);
    }
}


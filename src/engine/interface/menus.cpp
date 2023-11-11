/*
 * menus.cpp
 *
 * automatic triggers for certain UIs in game
 * does not implement menus, just some low level bindings for main menu functionality
 */

#include "../libprimis-headers/cube.h"

#include "control.h"
#include "menus.h"
#include "ui.h"

#include "world/octaedit.h"

//internally relevant functionality
namespace
{
    struct Change
    {
        int type;
        const char *desc;

        Change() {}
        Change(int type, const char *desc) : type(type), desc(desc) {}

        bool operator==(const Change & c) const
        {
            return type == c.type && std::strcmp(desc, c.desc) == 0;
        }
    };
    std::vector<Change> needsapply;

    VARP(applydialog, 0, 1, 1);

    //when 1: change UI shows up whenever a pending change is added
    //when 0: change UI does not appear and applychanges must be used manually
    VAR(showchanges, 0, 1, 1);

    //goes through and applies changes that are enqueued
    void applychanges()
    {
        int changetypes = 0;
        for(const Change &i : needsapply)
        {
            changetypes |= i.type;
        }
        if(changetypes&Change_Graphics)
        {
            execident("resetgl");
        }
        else if(changetypes&Change_Shaders)
        {
            execident("resetshaders");
        }
        if(changetypes&Change_Sound)
        {
            execident("resetsound");
        }
    }

    //returns if there are pending changes or not enqueued
    void pendingchanges(int *idx)
    {
        if(idx)
        {
            if((needsapply.size()) > static_cast<uint>(*idx))
            {
                result(needsapply.at(*idx).desc);
            }
            else if(*idx < 0)
            {
                intret(needsapply.size());
            }
        }
    }
}

//externally relevant functionality

//toggles if the main menu is shown
VAR(mainmenu, 1, 1, 0);

//adds a change to the queue of settings changes,
//if applydialog = 0 then this function does nothing
//if showchanges = 1 then this function does not display changes UI at the end
void addchange(const char *desc, int type)
{
    if(!applydialog)
    {
        return;
    }
    for(const Change &i : needsapply)
    {
        if(!std::strcmp(i.desc, desc))
        {
            return;
        }
    }
    needsapply.push_back(Change(type, desc));
    if(showchanges)
    {
        UI::showui("changes");
    }
}

void notifywelcome()
{
    UI::hideui("servers");
}

//clears out pending changes added by addchange()
void clearchanges(int type)
{
    for(int i = needsapply.size(); --i >=0;) //note reverse iteration
    {
        Change &c = needsapply[i];
        if(c.type&type)
        {
            c.type &= ~type;
            if(!c.type)
            {
                auto it = std::find(needsapply.begin(), needsapply.end(), needsapply[i]);
                needsapply.erase(it);
            }
        }
    }
    if(needsapply.empty())
    {
        UI::hideui("changes");
    }
}

void clearmainmenu()
{
    showchanges = 1;
    if(mainmenu && multiplayer)
    {
        mainmenu = 0;
        UI::hideui(nullptr);
    }
}

void initmenuscmds()
{
    addcommand("applychanges", reinterpret_cast<identfun>(applychanges), "", Id_Command);
    addcommand("pendingchanges", reinterpret_cast<identfun>(pendingchanges), "b", Id_Command);
}

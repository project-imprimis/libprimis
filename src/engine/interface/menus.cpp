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
    };
    vector<Change> needsapply;

    VARP(applydialog, 0, 1, 1);

    //when 1: change UI shows up whenever a pending change is added
    //when 0: change UI does not appear and applychanges must be used manually
    VAR(showchanges, 0, 1, 1);

    //goes through and applies changes that are enqueued
    void applychanges()
    {
        int changetypes = 0;
        for(int i = 0; i < needsapply.length(); i++)
        {
            changetypes |= needsapply[i].type;
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
    void pendingchanges (int *idx)
    {
        if(needsapply.inrange(*idx))
        {
            result(needsapply[*idx].desc);
        }
        else if(*idx < 0)
        {
            intret(needsapply.length());
        }
    }

    int lastmainmenu = -1;
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
    for(int i = 0; i < needsapply.length(); i++)
    {
        if(!std::strcmp(needsapply[i].desc, desc))
        {
            return;
        }
    }
    needsapply.add(Change(type, desc));
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
    for(int i = needsapply.length(); --i >=0;) //note reverse iteration
    {
        Change &c = needsapply[i];
        if(c.type&type)
        {
            c.type &= ~type;
            if(!c.type)
            {
                needsapply.remove(i);
            }
        }
    }
    if(needsapply.empty())
    {
        UI::hideui("changes");
    }
}

//used in main.cpp
void menuprocess()
{
    if(lastmainmenu != mainmenu)
    {
        lastmainmenu = mainmenu;
        execident("mainmenutoggled");
    }
    if(mainmenu && !multiplayer && !UI::hascursor())
    {
        UI::showui("main");
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
    addcommand("applychanges", reinterpret_cast<identfun>(applychanges));
    addcommand("pendingchanges", reinterpret_cast<identfun>(pendingchanges), "b", Id_Command); 
}

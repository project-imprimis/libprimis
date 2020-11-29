/*
 * menus.cpp
 *
 * automatic triggers for certain UIs in game
 * does not implement menus, just some low level bindings for main menu functionality
 */

#include "engine.h"

#include "ui.h"

void notifywelcome()
{
    UI::hideui("servers");
}

struct Change
{
    int type;
    const char *desc;

    Change() {}
    Change(int type, const char *desc) : type(type), desc(desc) {}
};
static std::vector<Change> needsapply;

VARP(applydialog, 0, 1, 1);

//when 1: change UI shows up whenever a pending change is added
//when 0: change UI does not appear and applychanges must be used manually
VAR(showchanges, 0, 1, 1);

//adds a change to the queue of settings changes,
//if applydialog = 0 then this function does nothing
//if showchanges = 1 then this function does not display changes UI at the end
void addchange(const char *desc, int type)
{
    if(!applydialog)
    {
        return;
    }
    for(uint i = 0; i < needsapply.size(); i++)
    {
        if(!strcmp(needsapply[i].desc, desc))
        {
            return;
        }
    }
    needsapply.emplace_back(Change(type, desc));
    if(showchanges)
    {
        UI::showui("changes");
    }
}

//clears out pending changes added by addchange()
void clearchanges(int type)
{
    for(uint i = needsapply.size(); --i >=0;) //note reverse iteration
    {
        Change &c = needsapply[i];
        if(c.type&type)
        {
            c.type &= ~type;
            if(!c.type)
            {
                needsapply.erase(needsapply.begin() + i);
            }
        }
    }
    if(needsapply.empty())
    {
        UI::hideui("changes");
    }
}

//goes through and applies changes that are enqueued
void applychanges()
{
    int changetypes = 0;
    for(uint i = 0; i < needsapply.size(); i++)
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

//executes applychanges()
COMMAND(applychanges, "");

//returns if there are pending changes or not enqueued
ICOMMAND(pendingchanges, "b", (int *idx),
{
    if(static_cast<int>(needsapply.size()) > *idx)
    {
        result(needsapply[*idx].desc);
    }
    else if(*idx < 0)
    {
        intret(needsapply.size());
    }
});

static int lastmainmenu = -1;

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

//toggles if the main menu is shown
VAR(mainmenu, 1, 1, 0);

void clearmainmenu()
{
    showchanges = 1;
    if(mainmenu && multiplayer)
    {
        mainmenu = 0;
        UI::hideui(NULL);
    }
}

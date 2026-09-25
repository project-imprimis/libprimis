/**
 * @file menus.cpp
 * @brief UI triggers for menu functionality
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
    /**
     * @brief Object containing information about an event that requires a change.
     *
     * This stores events which will require the engine to restart some components
     * in order to apply them to the currently running application instance.
     *
     * Settings that are changed that will require some kind of update will enqueue
     * changes such that all relevant changes will be applied after prompting upon
     * closing of the settings menu.
     */
    struct Change final
    {
        int type; /// one of the values in control.h Control enum
        std::string desc; /// text description of the thing that is requesting a change

        Change() {}
        Change(int inittype, const char *initdesc) : type(inittype), desc(initdesc) {}

        bool operator==(const Change & c) const
        {
            return type == c.type && (desc == c.desc) == 0;
        }
    };
    std::vector<Change> needsapply;

    VARP(applydialog, 0, 1, 1);

    //when 1: change UI shows up whenever a pending change is added
    //when 0: change UI does not appear and applychanges must be used manually
    VAR(showchanges, 0, 1, 1);

    /**
     * @brief Applies enqueued changes that require reloading parts of the engine
     *
     * These are settings that cannot be applied immediately and require reloading
     * parts of the engine (disrupting the currently rendered scene). Settings that
     * require resetting the GL configuration, the shader configuration or sound
     * configuration are added to the change buffer and are flushed when this function
     * is called.
     */
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

    /**
     * @brief returns if there are pending changes or not enqueued
     *
     * If idx is specified, and specifies a valid pending change index, prints out
     * the description of that pending change to CubeScript. If no idx is specified,
     * prints out how many changes there are enqueued to CubeScript.
     *
     * @param idx index to query, or nullptr if querying number of pending changes
     */
    void pendingchanges(const int *idx)
    {
        if(idx)
        {
            if((needsapply.size()) > static_cast<uint>(*idx))
            {
                result(needsapply.at(*idx).desc.c_str());
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

void addchange(const char *desc, int type)
{
    if(!applydialog)
    {
        return;
    }
    for(const Change &i : needsapply)
    {
        if(!std::strcmp(i.desc.c_str(), desc))
        {
            return;
        }
    }
    needsapply.emplace_back(type, desc);
    if(showchanges)
    {
        UI::showui("changes");
    }
}

void notifywelcome()
{
    UI::hideui("servers");
}

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

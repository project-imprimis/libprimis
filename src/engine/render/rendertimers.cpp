/* rendertimers.cpp: renderer functionality used for displaying rendering stats
 * while the program is running
 *
 * timers can be created with designated start/stop points in the code; sub-ms
 * times needed for accurate diagnosis possible (each frame is ~16.6ms @ 60Hz)
 *
 * used in rendergl.cpp
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/glexts.h"

#include "rendergl.h"
#include "rendertext.h"
#include "renderva.h"

#include "interface/control.h"

void cleanuptimers();                              //needed for timer script gvar
VARFN(timer, usetimers, 0, 0, 1, cleanuptimers()); //toggles logging timer information & rendering it
VAR(frametimer, 0, 0, 1);                          //toggles timing how long each frame takes (and rendering it to timer ui)

struct timer
{
    enum
    {
        Timer_MaxQuery = 4          //max number of gl queries
    };
    const char *name;               //name the timer reports as
    bool gpu;                       //whether the timer is for gpu time (true) or cpu time
    GLuint query[Timer_MaxQuery];   //gpu query information
    int waiting;                    //internal bitmask for queries
    uint starttime;                 //time the timer was started (in terms of ms since game started)
    float result,                   //raw value of the timer, -1 if no info available
          print;                    //the time the timer displays: ms per frame for whatever object
};

//locally relevant functionality
namespace
{
    static vector<timer> timers;
    static vector<int> timerorder;
    static int timercycle = 0;

    timer *findtimer(const char *name, bool gpu) //also creates a new timer if none found
    {
        for(int i = 0; i < timers.length(); i++)
        {
            if(!std::strcmp(timers[i].name, name) && timers[i].gpu == gpu)
            {
                timerorder.removeobj(i);
                timerorder.add(i);
                return &timers[i];
            }
        }
        timerorder.add(timers.length());
        timer &t = timers.add();
        t.name = name;
        t.gpu = gpu;
        memset(t.query, 0, sizeof(t.query));
        if(gpu)
        {
            glGenQueries_(timer::Timer_MaxQuery, t.query);
        }
        t.waiting = 0;
        t.starttime = 0;
        t.result = -1;
        t.print = -1;
        return &t;
    }
}

//externally relevant functionality

//used to start a timer in some part of the code, cannot be used outside of rendering part
/**
 * @brief activates a timer that starts its query from a given point in the code
 *
 * Creates a new timer if necessary.
 *
 * @param name The name of the timer to use
 * @param gpu Toggles timing GPU rendering time
 *
 * @return a pointer to the relevant timer
 */
timer *begintimer(const char *name, bool gpu)
{
    if(!usetimers || inbetweenframes || (gpu && (!hasTQ || deferquery)))
    {
        return nullptr;
    }
    timer *t = findtimer(name, gpu);
    if(t->gpu)
    {
        deferquery++;
        glBeginQuery_(GL_TIME_ELAPSED_EXT, t->query[timercycle]);
        t->waiting |= 1<<timercycle;
    }
    else
    {
        t->starttime = getclockmillis();
    }
    return t;
}

//used to end a timer started by begintimer(), needs to be included sometime after begintimer
//the part between begintimer() and endtimer() is what gets timed
void endtimer(timer *t)
{
    if(!t)
    {
        return;
    }
    if(t->gpu)
    {
        glEndQuery_(GL_TIME_ELAPSED_EXT);
        deferquery--;
    }
    else
    {
        t->result = std::max(static_cast<float>(getclockmillis() - t->starttime), 0.0f);
    }
}

//foreach timer, query what time has passed since last update
void synctimers()
{
    timercycle = (timercycle + 1) % timer::Timer_MaxQuery;

    for(int i = 0; i < timers.length(); i++)
    {
        timer &t = timers[i];
        if(t.waiting&(1<<timercycle))
        {
            GLint available = 0;
            while(!available)
            {
                glGetQueryObjectiv_(t.query[timercycle], GL_QUERY_RESULT_AVAILABLE, &available);
            }
            GLuint64EXT result = 0;
            glGetQueryObjectui64v_(t.query[timercycle], GL_QUERY_RESULT, &result);
            t.result = std::max(static_cast<float>(result) * 1e-6f, 0.0f);
            t.waiting &= ~(1<<timercycle);
        }
        else
        {
            t.result = -1;
        }
    }
}

/**
 * @brief deletes the elements in the timers global vector
 *
 * Deletes the elements in the `timer` global variable. If any GPU queries are active,
 * they are cancelled so as not to waste the GPU's time
 */
void cleanuptimers()
{
    for(int i = 0; i < timers.length(); i++)
    {
        timer &t = timers[i];
        if(t.gpu)
        {
            glDeleteQueries_(timer::Timer_MaxQuery, t.query);
        }
    }
    timers.shrink(0);
    timerorder.shrink(0);
}

/*
 * draws timers to the screen using hardcoded text
 *
 * if frametimer gvar is enabled, also shows the overall frame time
 * otherwise, prints out all timer information available
 */
void printtimers(int conw, int conh, int framemillis)
{
    if(!frametimer && !usetimers)
    {
        return;
    }
    static int lastprint = 0;
    int offset = 0;
    if(frametimer)
    {
        static int printmillis = 0;
        if(totalmillis - lastprint >= 200)
        {
            printmillis = framemillis;
        }
        draw_textf("frame time %i ms", conw-20*FONTH, conh-FONTH*3/2-offset*9*FONTH/8, printmillis);
        offset++;
    }
    if(usetimers)
    {
        for(int i = 0; i < timerorder.length(); i++)
        {
            timer &t = timers[timerorder[i]];
            if(t.print < 0 ? t.result >= 0 : totalmillis - lastprint >= 200)
            {
                t.print = t.result;
            }
            if(t.print < 0 || (t.gpu && !(t.waiting&(1<<timercycle))))
            {
                continue;
            }
            draw_textf("%s%s %5.2f ms", conw-20*FONTH, conh-FONTH*3/2-offset*9*FONTH/8, t.name, t.gpu ? "" : " (cpu)", t.print);
            offset++;
        }
    }
    if(totalmillis - lastprint >= 200)
    {
        lastprint = totalmillis;
    }
}

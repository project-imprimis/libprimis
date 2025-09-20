/**
 * @brief renderer functionality used for displaying rendering stats while the program is running
 *
 * timers can be created with designated start/stop points in the code; sub-ms
 * times needed for accurate diagnosis possible (each frame is ~16.6ms @ 60Hz)
 *
 * used in rendergl.cpp
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"

#include "rendergl.h"
#include "rendertext.h"
#include "renderttf.h"
#include "renderva.h"

#include "interface/control.h"

void cleanuptimers();                              //needed for timer script gvar
VAR(frametimer, 0, 0, 1);                          //toggles timing how long each frame takes (and rendering it to timer ui), used in hud

//declared and used as pointer in header
struct timer final
{
    enum
    {
        Timer_MaxQuery = 4          //max number of gl queries
    };
    const char *name;               //name the timer reports as
    bool gpu;                       //whether the timer is for gpu time (true) or cpu time
    std::array<GLuint, Timer_MaxQuery> query; //gpu query information
    int waiting;                    //internal bitmask for queries
    size_t starttime;               //time the timer was started (in terms of ms since game started)
    float result,                   //raw value of the timer, -1 if no info available
          print;                    //the time the timer displays: ms per frame for whatever object
};

//locally relevant functionality
namespace
{
    std::vector<timer> timers;
    std::vector<size_t> timerorder;
    size_t timercycle = 0;

    VARFN(timer, usetimers, 0, 0, 1, cleanuptimers()); //toggles logging timer information & rendering it

    timer *findtimer(const char *name, bool gpu) //also creates a new timer if none found
    {
        for(size_t i = 0; i < timers.size(); i++)
        {
            if(!std::strcmp(timers[i].name, name) && timers[i].gpu == gpu)
            {
                timerorder.erase(std::find(timerorder.begin(), timerorder.end(), i));
                timerorder.push_back(i);
                return &timers[i];
            }
        }
        timerorder.push_back(timers.size());
        timers.emplace_back();
        timer &t = timers.back();
        t.name = name;
        t.gpu = gpu;
        t.query.fill(0);
        if(gpu)
        {
            glGenQueries(timer::Timer_MaxQuery, t.query.data());
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
        glBeginQuery(GL_TIME_ELAPSED_EXT, t->query[timercycle]);
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
        glEndQuery(GL_TIME_ELAPSED_EXT);
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

    for(timer& t : timers)
    {
        if(t.waiting&(1<<timercycle))
        {
            GLint available = 0;
            while(!available)
            {
                glGetQueryObjectiv(t.query[timercycle], GL_QUERY_RESULT_AVAILABLE, &available);
            }
            GLuint64EXT result = 0;
            glGetQueryObjectui64v(t.query[timercycle], GL_QUERY_RESULT, &result);
            t.result = std::max(static_cast<float>(result) * 1e-6f, 0.0f);
            t.waiting &= ~(1<<timercycle);
        }
        else
        {
            t.result = -1;
        }
    }
}

void cleanuptimers()
{
    for(const timer& t : timers)
    {
        if(t.gpu)
        {
            glDeleteQueries(timer::Timer_MaxQuery, t.query.data());
        }
    }
    timers.clear();
    timerorder.clear();
}

void printtimers(int conw, int framemillis)
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
        std::array<char, 200> framestring;
        constexpr int size = 42;
        std::sprintf(framestring.data(), "frame time %i ms", printmillis);
        ttr.fontsize(size);
        ttr.renderttf(framestring.data(), {0xFF, 0xFF, 0xFF, 0}, conw-20*size, size*3/2+offset*9*size/8);
        //draw_textf("frame time %i ms", conw-20*FONTH, conh-FONTH*3/2-offset*9*FONTH/8, printmillis);
        offset++;
    }
    if(usetimers)
    {
        for(int i : timerorder)
        {
            timer &t = timers[i];
            if(t.print < 0 ? t.result >= 0 : totalmillis - lastprint >= 200)
            {
                t.print = t.result;
            }
            if(t.print < 0 || (t.gpu && !(t.waiting&(1<<timercycle))))
            {
                continue;
            }
            std::array<char, 200> framestring;
            constexpr int size = 42;
            std::sprintf(framestring.data(), "%s%s %5.2f ms", t.name, t.gpu ? "" : " (cpu)", t.print);
            ttr.fontsize(size);
            ttr.renderttf(framestring.data(), {0xFF, 0xFF, 0xFF, 0}, conw-20*size, size*3/2+offset*9*size/8);

            //draw_textf("%s%s %5.2f ms", conw-20*FONTH, conh-FONTH*3/2-offset*9*FONTH/8, t.name, t.gpu ? "" : " (cpu)", t.print);
            offset++;
        }
    }
    if(totalmillis - lastprint >= 200)
    {
        lastprint = totalmillis;
    }
}

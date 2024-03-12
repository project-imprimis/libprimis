// console.cpp: the console buffer, its display, and command line control

#include "../libprimis-headers/cube.h"
#include "../../shared/stream.h"

#include "console.h"
#include "control.h"
#include "cs.h"
#include "ui.h"
#include "menus.h"

//input.h needs rendertext's objects
#include "render/rendertext.h"
#include "render/renderttf.h"
#include "input.h"

#include "world/octaedit.h"

int commandmillis = -1;

struct FilesKey
{
    const int type;
    const std::string dir,
                      ext;

    FilesKey(int type, const std::string &dir, const std::string &ext) : type(type), dir(dir), ext(ext) {}

    bool operator==(const FilesKey &y) const
    {
        return type == y.type && dir == y.dir && ext == y.ext;
    }
};

template<>
struct std::hash<FilesKey>
{
    size_t operator()(const FilesKey &key) const
    {
        size_t h = 5381;
        for(int i = 0, k; (k = key.dir[i]); i++)
        {
            h = ((h<<5)+h)^k;    // bernstein k=33 xor
        }
        return h;
    }
};

class CompletionFinder
{
    public:
        enum
        {
            Files_Directory = 0,
            Files_List,
        };

        void resetcomplete();
        void addfilecomplete(char *command, char *dir, char *ext);
        void addlistcomplete(char *command, char *list);

        void complete(char *s, size_t maxlen, const char *cmdprefix);

        //print to a stream f the listcompletions in the completions filesval
        void writecompletions(std::fstream& f);

    private:

        struct FilesVal
        {
            public:
                int type;
                std::string dir,
                            ext;
                std::vector<char *> files;

                FilesVal(int type, std::string dir, std::string ext);
                ~FilesVal();

                void update();

            private:
                int millis;
        };

        friend std::hash<FilesKey>;

        std::unordered_map<FilesKey, FilesVal *> completefiles;
        std::unordered_map<const char *, FilesVal *> completions;

        int completesize = 0;
        char *lastcomplete = nullptr;

        void addcomplete(char *command, int type, char *dir, char *ext);

        char *prependstring(char *d, const char *s, size_t len) const;
};

CompletionFinder::FilesVal::FilesVal(int type, std::string dir, std::string ext) : type(type), dir(dir), ext(ext[0] ? std::string(ext) : ""), millis(-1)
{
}

CompletionFinder::FilesVal::~FilesVal()
{
    for(char* i : files)
    {
        delete[] i;
    }
}

void CompletionFinder::FilesVal::update()
{
    if(type!=Files_Directory || millis >= commandmillis)
    {
        return;
    }
    //first delete old cached file vector
    for(char* i : files)
    {
        delete[] i;
    }
    //generate new one
    listfiles(dir.c_str(), ext.c_str(), files);
    std::sort(files.begin(), files.end());
    for(uint i = 0; i < files.size(); i++)
    {
        if(i && !std::strcmp(files[i], files[i-1]))
        {
            delete[] files.at(i);
            files.erase(files.begin() + i);
            i--; //we need to make up for the element we destroyed
        }
    }
    millis = totalmillis;
}

void CompletionFinder::resetcomplete()
{
    completesize = 0;
}

void CompletionFinder::addfilecomplete(char *command, char *dir, char *ext)
{
    addcomplete(command, Files_Directory, dir, ext);
}

void CompletionFinder::addlistcomplete(char *command, char *list)
{
    addcomplete(command, Files_List, list, nullptr);
}

void CompletionFinder::complete(char *s, size_t maxlen, const char *cmdprefix)
{
    size_t cmdlen = 0;
    if(cmdprefix)
    {
        cmdlen = std::strlen(cmdprefix);
        if(std::strncmp(s, cmdprefix, cmdlen))
        {
            prependstring(s, cmdprefix, maxlen);
        }
    }
    if(!s[cmdlen])
    {
        return;
    }
    if(!completesize)
    {
        completesize = static_cast<int>(std::strlen(&s[cmdlen]));
        delete[] lastcomplete;
        lastcomplete = nullptr;
    }
    FilesVal *f = nullptr;
    if(completesize)
    {
        char *end = std::strchr(&s[cmdlen], ' ');
        if(end)
        {
            f = completions[stringslice(&s[cmdlen], end).str];
        }
    }
    const char *nextcomplete = nullptr;
    if(f) // complete using filenames
    {
        int commandsize = std::strchr(&s[cmdlen], ' ')+1-s;
        f->update();
        for(const char * i : f->files)
        {
            if(std::strncmp(i, &s[commandsize], completesize+cmdlen-commandsize)==0 &&
                      (!lastcomplete || std::strcmp(i, lastcomplete) > 0) &&
                      (!nextcomplete || std::strcmp(i, nextcomplete) < 0))
            {
                nextcomplete = i;
            }
        }
        cmdprefix = s;
        cmdlen = commandsize;
    }
    else // complete using command or var (ident) names
    {
        for(auto& [k, id] : idents)
        {
            if(std::strncmp(id.name, &s[cmdlen], completesize)==0 &&
                      (!lastcomplete || std::strcmp(id.name, lastcomplete) > 0) &&
                      (!nextcomplete || std::strcmp(id.name, nextcomplete) < 0))
            {
                nextcomplete = id.name;
            }
        }
    }

    delete[] lastcomplete;
    lastcomplete = nullptr;
    if(nextcomplete)
    {
        cmdlen = std::min(cmdlen, maxlen-1);
        if(cmdlen)
        {
            std::memmove(s, cmdprefix, cmdlen);
        }
        copystring(&s[cmdlen], nextcomplete, maxlen-cmdlen);
        lastcomplete = newstring(nextcomplete);
    }
}

//print to a stream f the listcompletions in the completions filesval
void CompletionFinder::writecompletions(std::fstream& f)
{
    std::vector<std::string> cmds;
    for(auto &[k, v] : completions)
    {
        if(v)
        {
            cmds.push_back(k);
        }
    }
    std::sort(cmds.begin(), cmds.end());
    for(std::string &k : cmds)
    {
        FilesVal *v = completions[k.c_str()];
        if(!v)
        {
            conoutf("could not write completion");
            return;
        }
        if(v->type==Files_List)
        {
            if(validateblock(v->dir.c_str()))
            {
                f << "listcomplete " << escapeid(k.c_str()) << " [" << v->dir << "]\n";
            }
            else
            {
                f << "listcomplete " << escapeid(k.c_str()) << " " << escapestring(v->dir.c_str()) << std::endl;
            }
        }
        else
        {
            f << "complete " << escapeid(k.c_str()) << " " << escapestring(v->dir.c_str()) << " " << escapestring(v->ext.size() ? v->ext.c_str() : "*") << std::endl;
        }
    }
}

void CompletionFinder::addcomplete(char *command, int type, char *dir, char *ext)
{
    if(identflags&Idf_Overridden)
    {
        conoutf(Console_Error, "cannot override complete %s", command);
        return;
    }
    if(!dir[0])
    {
        auto hasfilesitr = completions.find(command);
        if(hasfilesitr != completions.end())
        {
            (*hasfilesitr).second = nullptr;
        }
        return;
    }
    if(type==Files_Directory)
    {
        int dirlen = static_cast<int>(std::strlen(dir));
        while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
        {
            dir[--dirlen] = '\0';
        }
        if(ext)
        {
            if(std::strchr(ext, '*'))
            {
                ext[0] = '\0';
            }
            if(!ext[0])
            {
                ext = nullptr;
            }
        }
    }
    FilesKey key(type, dir ? dir : "", dir ? dir : "");
    auto itr = completefiles.find(key);
    if(itr == completefiles.end())
    {
        FilesVal *f = new FilesVal(type, dir ? dir : "", ext ? ext : "");
        if(type==Files_List)
        {
            explodelist(dir, f->files);
        }
        FilesKey newfile = FilesKey(type, f->dir, f->ext);
        itr = completefiles.insert(std::pair<FilesKey, FilesVal *>(newfile, f)).first;
    }
    auto hasfilesitr = completions.find(std::string(command).c_str());
    if(hasfilesitr != completions.end())
    {
        (*hasfilesitr).second = (*itr).second;
    }
    else
    {
        FilesVal *v = (*itr).second;
        completions[newstring(command)] = v;
    }
}

char *CompletionFinder::prependstring(char *d, const char *s, size_t len) const
{
    size_t slen = std::min(std::strlen(s), len);
    std::memmove(&d[slen], d, std::min(len - slen, std::strlen(d) + 1));
    std::memcpy(d, s, slen);
    d[len-1] = 0;
    return d;
}

//internally relevant functionality
namespace
{
    constexpr int maxconsolelines = 1000;  //maximum length of conlines reverse queue

    struct cline
    {
        char *line;   //text contents of the line
        int type,     //one of the enum values Console_* in headers/consts.h
            outtime;  //timestamp when the console line was created
    };
    std::deque<cline> conlines; //global storage of console lines

    string commandbuf;
    char *commandaction = nullptr,
         *commandprompt = nullptr;
    enum CommandFlags
    {
        CmdFlags_Complete = 1<<0,
        CmdFlags_Execute  = 1<<1,
    };

    int commandflags = 0,
        commandpos = -1;

    VARFP(maxcon, 10, 200, maxconsolelines,
    {
        while(static_cast<int>(conlines.size()) > maxcon)
        {
            delete[] conlines.front().line;
            conlines.pop_back();
        }
    });

    constexpr int constrlen = 512;

    // tab-completion of all idents and base maps

    CompletionFinder cfinder;

    void conline(int type, const char *sf)        // add a line to the console buffer
    {
        char *buf = static_cast<int>(conlines.size()) >= maxcon ? conlines.back().line : newstring("", constrlen-1);
        if(static_cast<int>(conlines.size()) >= maxcon)
        {
            conlines.pop_back();
        }
        cline cl;
        cl.line = buf;
        cl.type = type;
        cl.outtime = totalmillis;                // for how long to keep line on screen
        copystring(cl.line, sf, constrlen);
        conlines.push_front(cl);
    }

    void fullconsole(int *val, int *numargs, ident *id)
    {
        if(*numargs > 0)
        {
            UI::holdui("fullconsole", *val!=0);
        }
        else
        {
            int vis = UI::uivisible("fullconsole") ? 1 : 0;
            if(*numargs < 0)
            {
                intret(vis);
            }
            else
            {
                printvar(id, vis);
            }
        }
    }

    void toggleconsole()
    {
        UI::toggleui("fullconsole");
    }

    VARP(miniconsize, 0, 5, 100);               //miniature console font size
    VARP(miniconwidth, 0, 40, 100);             //miniature console width
    VARP(confade, 0, 30, 60);                   //seconds before fading console
    VARP(miniconfade, 0, 30, 60);
    HVARP(confilter, 0, 0xFFFFFF, 0xFFFFFF);
    HVARP(fullconfilter, 0, 0xFFFFFF, 0xFFFFFF);
    HVARP(miniconfilter, 0, 0, 0xFFFFFF);

    int conskip     = 0,
        miniconskip = 0;

    void setconskip(int &skip, int filter, int n)
    {
        int offsetnum = std::abs(n),
            dir = n < 0 ? -1 : 1;
        skip = std::clamp(skip, 0, static_cast<int>(conlines.size()-1));
        while(offsetnum)
        {
            skip += dir;
            if(!(static_cast<int>(conlines.size()) > skip))
            {
                skip = std::clamp(skip, 0, static_cast<int>(conlines.size()-1));
                return;
            }
            if(skip < 0)
            {
                skip = 0;
                break;
            }
            if(conlines[skip].type&filter)
            {
                --offsetnum;
            }
        }
    }

    void clearconsole()
    {
        while(conlines.size())
        {
            delete[] conlines.back().line;
            conlines.pop_back();
        }
    }

    float drawconlines(int conskip, int confade, float conwidth, float conheight, float conoff, int filter, float y = 0, int dir = 1)
    {
        int numl = conlines.size(),
            offsetlines = std::min(conskip, numl);
        if(confade)
        {
            if(!conskip)
            {
                numl = 0;
                for(int i = conlines.size(); --i >=0;) //note reverse iteration
                {
                    if(totalmillis-conlines[i].outtime < confade*1000)
                    {
                        numl = i+1;
                        break;
                    }
                }
            }
            else
            {
                offsetlines--;
            }
        }

        int totalheight = 0;
        for(int i = 0; i < numl; ++i) //determine visible height
        {
            // shuffle backwards to fill if necessary
            int idx = offsetlines+i < numl ? offsetlines+i : --offsetlines;
            if(!(conlines[idx].type&filter))
            {
                continue;
            }
            char *line = conlines[idx].line;
            float width, height;
            text_boundsf(line, width, height, conwidth);
            if(totalheight + height > conheight)
            {
                numl = i;
                if(offsetlines == idx)
                {
                    ++offsetlines;
                }
                break;
            }
            totalheight += height;
        }
        if(dir > 0)
        {
            y = conoff;
        }
        for(int i = 0; i < numl; ++i)
        {
            int idx = offsetlines + (dir > 0 ? numl-i-1 : i);
            if(!(conlines[idx].type&filter))
            {
                continue;
            }
            char *line = conlines[idx].line;
            float width, height;
            text_boundsf(line, width, height, conwidth);
            if(dir <= 0)
            {
                y -= height;
            }
            //draw_text(line, conoff, y, 0xFF, 0xFF, 0xFF, 0xFF, -1, conwidth);
            ttr.fontsize(50);
            ttr.renderttf(line, {0xFF, 0xFF, 0xFF, 0}, conoff, y);
            if(dir > 0)
            {
                y += height;
            }
        }
        return y+conoff;
    }

    // keymap is defined externally in keymap.cfg

    /*
     * defines a mapping for a single key
     * multiple keymap objects are aggregated in keyms to create the entire bindings list
     */
    struct KeyMap
    {
        enum
        {
            Action_Default = 0,
            Action_Spectator,
            Action_Editing,
            Action_NumActions
        };

        int code;                           //unique bind code assigned to the key
        char *name;                         //name to use to access this key
        char *actions[Action_NumActions];   //array of strings to execute depending on what mode is being used
        bool pressed;                       //whether this key is currently depressed

        KeyMap() : code(-1), name(nullptr), pressed(false)
        {
            for(int i = 0; i < Action_NumActions; ++i)
            {
                actions[i] = newstring("");
            }
        }
        ~KeyMap()
        {
            delete[] name;
            name = nullptr;
            for(int i = 0; i < Action_NumActions; ++i)
            {
                delete[] actions[i];
                actions[i] = nullptr;
            }
        }

        void clear(int type);
        void clear()
        {
            for(int i = 0; i < Action_NumActions; ++i)
            {
                clear(i);
            }
        }
    };


    KeyMap *keypressed = nullptr;
    char *keyaction = nullptr;

    void KeyMap::clear(int type)
    {
        char *&binding = actions[type];
        if(binding[0])
        {
            if(!keypressed || keyaction!=binding)
            {
                delete[] binding;
            }
            binding = newstring("");
        }
    }

    std::map<int, KeyMap> keyms;

    void keymap(int *code, char *key)
    {
        if(identflags&Idf_Overridden)
        {
            conoutf(Console_Error, "cannot override keymap %d", *code);
            return;
        }
        KeyMap &km = keyms[*code];
        km.code = *code;
        delete[] km.name;
        km.name = newstring(key);
    }

    void searchbinds(char *action, int type)
    {
        std::vector<char> names;
        for(auto &[k, km] : keyms)
        {
            if(!std::strcmp(km.actions[type], action))
            {
                if(names.size())
                {
                    names.push_back(' ');
                }
                for(uint i = 0; i < std::strlen(km.name); ++i)
                {
                    names.push_back(km.name[i]);
                }
            }
        }
        names.push_back('\0');
        result(names.data());
    }

    KeyMap *findbind(const char *key)
    {
        for(auto &[k, km] : keyms)
        {
            if(!strcasecmp(km.name, key))
            {
                return &km;
            }
        }
        return nullptr;
    }

    void getbind(const char *key, int type)
    {
        KeyMap *km = findbind(key);
        result(km ? km->actions[type] : "");
    }

    void bindkey(const char *key, const char *action, int state, const char *cmd)
    {
        if(identflags&Idf_Overridden)
        {
            conoutf(Console_Error, "cannot override %s \"%s\"", cmd, key);
            return;
        }
        KeyMap *km = findbind(key);
        if(!km)
        {
            conoutf(Console_Error, "unknown key \"%s\"", key);
            return;
        }
        char *&binding = km->actions[state];
        if(!keypressed || keyaction!=binding)
        {
            delete[] binding;
        }
        // trim white-space to make searchbinds more reliable
        while(iscubespace(*action))
        {
            action++;
        }
        int len = std::strlen(action);
        while(len>0 && iscubespace(action[len-1]))
        {
            len--;
        }
        binding = newstring(action, len);
    }

    void inputcommand(char *init, char *action = nullptr, char *prompt = nullptr, char *flags = nullptr) // turns input to the command line on or off
    {
        commandmillis = init ? totalmillis : -1;
        textinput(commandmillis >= 0, TextInput_Console);
        keyrepeat(commandmillis >= 0, KeyRepeat_Console);
        copystring(commandbuf, init ? init : "");

        delete[] commandaction;
        delete[] commandprompt;
        commandaction = nullptr;
        commandprompt = nullptr;

        commandpos = -1;
        if(action && action[0])
        {
            commandaction = newstring(action);
        }
        if(prompt && prompt[0])
        {
            commandprompt = newstring(prompt);
        }
        commandflags = 0;
        if(flags)
        {
            while(*flags)
            {
                switch(*flags++)
                {
                    case 'c':
                    {
                        commandflags |= CmdFlags_Complete;
                        break;
                    }
                    case 'x':
                    {
                        commandflags |= CmdFlags_Execute;
                        break;
                    }
                    case 's':
                    {
                        commandflags |= CmdFlags_Complete|CmdFlags_Execute;
                        break;
                    }
                }
            }
        }
        else if(init)
        {
            commandflags |= CmdFlags_Complete|CmdFlags_Execute;
        }
    }

    void saycommand(char *init)
    {
        inputcommand(init);
    }

    void pasteconsole()
    {
        if(!SDL_HasClipboardText())
        {
            return;
        }
        char *cb = SDL_GetClipboardText();
        if(!cb)
        {
            return;
        }
        size_t cblen = std::strlen(cb),
               commandlen = std::strlen(commandbuf);
        if(strlen(commandbuf) + cblen < 260)
        {
            std::memcpy(reinterpret_cast<uchar *>(&commandbuf[commandlen]), cb, cblen);
        }
        commandbuf[commandlen + cblen] = '\0';
        SDL_free(cb);
    }

    struct HLine
    {
        char *buf, *action, *prompt;
        int flags;

        HLine() : buf(nullptr), action(nullptr), prompt(nullptr), flags(0) {}
        ~HLine()
        {
            delete[] buf;
            delete[] action;
            delete[] prompt;

            buf = nullptr;
            action = nullptr;
            prompt = nullptr;
        }

        void restore() const
        {
            copystring(commandbuf, buf);
            if(commandpos >= static_cast<int>(std::strlen(commandbuf)))
            {
                commandpos = -1;
            }

            delete[] commandaction;
            delete[] commandprompt;

            commandaction = nullptr;
            commandprompt = nullptr;

            if(action)
            {
                commandaction = newstring(action);
            }
            if(prompt)
            {
                commandprompt = newstring(prompt);
            }
            commandflags = flags;
        }

        bool shouldsave() const
        {
            return std::strcmp(commandbuf, buf) ||
                   (commandaction ? !action || std::strcmp(commandaction, action) : action!=nullptr) ||
                   (commandprompt ? !prompt || std::strcmp(commandprompt, prompt) : prompt!=nullptr) ||
                   commandflags != flags;
        }

        void save()
        {
            buf = newstring(commandbuf);
            if(commandaction)
            {
                action = newstring(commandaction);
            }
            if(commandprompt)
            {
                prompt = newstring(commandprompt);
            }
            flags = commandflags;
        }

        void run() const
        {
            if(flags&CmdFlags_Execute && buf[0]=='/')
            {
                execute(buf+1);
            }
            else if(action)
            {
                alias("commandbuf", buf);
                execute(action);
            }
            else
            {
                conoutf(Console_Info, "%s", buf);
            }
        }
    };
    std::vector<HLine *> history;
    int histpos = 0;

    VARP(maxhistory, 0, 1000, 10000);

    void historycmd(int *n)
    {
        static bool inhistory = false;
        if(!inhistory && static_cast<int>(history.size()) > *n)
        {
            inhistory = true;
            history[history.size()-*n-1]->run();
            inhistory = false;
        }
    }

    struct releaseaction
    {
        KeyMap *key;
        union
        {
            char *action;
            ident *id;
        };
        int numargs;
        tagval args[3];
    };
    std::vector<releaseaction> releaseactions;

    const char *addreleaseaction(char *s)
    {
        if(!keypressed)
        {
            delete[] s;
            return nullptr;
        }
        releaseactions.emplace_back();
        releaseaction &ra = releaseactions.back();
        ra.key = keypressed;
        ra.action = s;
        ra.numargs = -1;
        return keypressed->name;
    }

    void onrelease(const char *s)
    {
        addreleaseaction(newstring(s));
    }

    static void execbind(KeyMap &k, bool isdown, int map)
    {
        for(uint i = 0; i < releaseactions.size(); i++)
        {
            releaseaction &ra = releaseactions[i];
            if(ra.key==&k)
            {
                if(ra.numargs < 0)
                {
                    if(!isdown)
                    {
                        execute(ra.action);
                    }
                    delete[] ra.action;
                }
                else
                {
                    execute(isdown ? nullptr : ra.id, ra.args, ra.numargs);
                }
                releaseactions.erase(releaseactions.begin() + i);
                i--;
            }
        }
        if(isdown)
        {
            int state = KeyMap::Action_Default;
            if(!mainmenu)
            {
                if(map == 1)
                {
                    state = KeyMap::Action_Editing;
                }
                else if(map == 2)
                {
                    state = KeyMap::Action_Spectator;
                }
            }
            char *&action = k.actions[state][0] ? k.actions[state] : k.actions[KeyMap::Action_Default];
            keyaction = action;
            keypressed = &k;
            execute(keyaction);
            keypressed = nullptr;
            if(keyaction!=action)
            {
                delete[] keyaction;
            }
        }
        k.pressed = isdown;
    }

    bool consoleinput(const char *str, int len)
    {
        if(commandmillis < 0)
        {
            return false;
        }
        ::cfinder.resetcomplete();
        int cmdlen = static_cast<int>(std::strlen(commandbuf)),
            cmdspace = static_cast<int>(sizeof(commandbuf)) - (cmdlen+1);
        len = std::min(len, cmdspace);
        if(commandpos<0)
        {
            std::memcpy(&commandbuf[cmdlen], str, len);
        }
        else
        {
            std::memmove(&commandbuf[commandpos+len], &commandbuf[commandpos], cmdlen - commandpos);
            std::memcpy(&commandbuf[commandpos], str, len);
            commandpos += len;
        }
        commandbuf[cmdlen + len] = '\0';

        return true;
    }

    bool consolekey(int code, bool isdown)
    {
        if(commandmillis < 0)
        {
            return false;
        }
        if(isdown)
        {
            switch(code)
            {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                {
                    break;
                }
                case SDLK_HOME:
                {
                    if(std::strlen(commandbuf))
                    {
                        commandpos = 0;
                    }
                    break;
                }
                case SDLK_END:
                {
                    commandpos = -1;
                    break;
                }
                case SDLK_DELETE:
                {
                    size_t len = std::strlen(commandbuf);
                    if(commandpos<0)
                    {
                        break;
                    }
                    std::memmove(&commandbuf[commandpos], &commandbuf[commandpos+1], len - commandpos);
                    ::cfinder.resetcomplete();
                    if(commandpos >= static_cast<int>(len-1))
                    {
                        commandpos = -1;
                    }
                    break;
                }
                case SDLK_BACKSPACE:
                {
                    size_t len = std::strlen(commandbuf);
                    int i = commandpos>=0 ? commandpos : len;
                    if(i<1)
                    {
                        break;
                    }
                    std::memmove(&commandbuf[i-1], &commandbuf[i], len - i + 1);
                    ::cfinder.resetcomplete();
                    if(commandpos>0)
                    {
                        commandpos--;
                    }
                    else if(!commandpos && len<=1)
                    {
                        commandpos = -1;
                    }
                    break;
                }
                case SDLK_LEFT:
                {
                    if(commandpos>0)
                    {
                        commandpos--;
                    }
                    else if(commandpos<0)
                    {
                        commandpos = static_cast<int>(std::strlen(commandbuf))-1;
                    }
                    break;
                }
                case SDLK_RIGHT:
                {
                    if(commandpos>=0 && ++commandpos >= static_cast<int>(std::strlen(commandbuf)))
                    {
                        commandpos = -1;
                    }
                    break;
                }
                case SDLK_UP:
                {
                    if(histpos > static_cast<int>(history.size()))
                    {
                        histpos = history.size();
                    }
                    if(histpos > 0)
                    {
                        history[--histpos]->restore();
                    }
                    break;
                }
                case SDLK_DOWN:
                {
                    if(histpos + 1 < static_cast<int>(history.size()))
                    {
                        history[++histpos]->restore();
                    }
                    break;
                }
                case SDLK_TAB:
                {
                    if(commandflags&CmdFlags_Complete)
                    {
                        ::cfinder.complete(commandbuf, sizeof(commandbuf), commandflags&CmdFlags_Execute ? "/" : nullptr);
                        if(commandpos>=0 && commandpos >= static_cast<int>(std::strlen(commandbuf)))
                        {
                            commandpos = -1;
                        }
                    }
                    break;
                }
                case SDLK_v:
                {
                    if(SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL))//mod keys
                    {
                        pasteconsole();
                    }
                    break;
                }
            }
        }
        else
        {
            if(code==SDLK_RETURN || code==SDLK_KP_ENTER)
            {
                HLine *h = nullptr;
                if(commandbuf[0])
                {
                    if(history.empty() || history.back()->shouldsave())
                    {
                        if(maxhistory && static_cast<int>(history.size()) >= maxhistory)
                        {
                            for(uint i = 0; i < (history.size()-maxhistory+1); ++i)
                            {
                                delete history[i];
                            }
                            history.erase(history.begin(), history.begin() + history.size()-maxhistory+1);
                        }
                        history.emplace_back(h = new HLine)->save();
                    }
                    else
                    {
                        h = history.back();
                    }
                }
                histpos = history.size();
                inputcommand(nullptr);
                if(h)
                {
                    h->run();
                }
            }
            else if(code==SDLK_ESCAPE)
            {
                histpos = history.size();
                inputcommand(nullptr);
            }
        }

        return true;
    }
}

//iengine.h
void clear_console()
{
    keyms.clear();
}

//console.h
void processtextinput(const char *str, int len)
{
    if(!UI::textinput(str, len))
    {
        consoleinput(str, len);
    }
}

void processkey(int code, bool isdown, int map)
{
    auto itr = keyms.find(code);
    if(itr != keyms.end() && (*itr).second.pressed)
    {
        execbind((*itr).second, isdown, map); // allow pressed keys to release
    }
    else if(!UI::keypress(code, isdown)) // UI key intercept
    {
        if(!consolekey(code, isdown))
        {
            if(itr != keyms.end())
            {
                execbind((*itr).second, isdown, map);
            }
        }
    }
}

float rendercommand(float x, float y, float w)
{
    if(commandmillis < 0)
    {
        return 0;
    }
    char buf[constrlen];
    const char *prompt = commandprompt ? commandprompt : ">";
    formatstring(buf, "%s %s", prompt, commandbuf);
    float width, height;
    text_boundsf(buf, width, height, w);
    y -= height;
    ttr.fontsize(50);
    ttr.renderttf(buf, {0xFF, 0xFF, 0xFF, 0}, x, y);
    //draw_text(buf, x, y, 0xFF, 0xFF, 0xFF, 0xFF, commandpos>=0 ? commandpos+1 + std::strlen(prompt) : std::strlen(buf), w);
    return height;
}

float renderfullconsole(float w, float h)
{
    float conpad = FONTH/2,
          conheight = h - 2*conpad,
          conwidth = w - 2*conpad;
    drawconlines(conskip, 0, conwidth, conheight, conpad, fullconfilter);
    return conheight + 2*conpad;
}

float renderconsole(float w, float h, float abovehud)
{
    static VARP(consize, 0, 5, 100);                   //font size of the console text
    float conpad = FONTH/2,
          conheight = std::min(static_cast<float>(FONTH*consize), h - 2*conpad),
          conwidth = w - 2*conpad,
          y = drawconlines(conskip, confade, conwidth, conheight, conpad, confilter);
    if(miniconsize && miniconwidth)
    {
        drawconlines(miniconskip, miniconfade, (miniconwidth*(w - 2*conpad))/100, std::min(static_cast<float>(FONTH*miniconsize), abovehud - y), conpad, miniconfilter, abovehud, -1);
    }
    return y;
}

void conoutfv(int type, const char *fmt, va_list args)
{
    static char buf[constrlen];
    vformatstring(buf, fmt, args, sizeof(buf));
    conline(type, buf);
    logoutf("%s", buf);
}

void conoutf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    conoutfv(Console_Info, fmt, args);
    va_end(args);
}

void conoutf(int type, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    conoutfv(type, fmt, args);
    va_end(args);
}

const char *getkeyname(int code)
{
    auto itr = keyms.find(code);
    return itr != keyms.end() ? (*itr).second.name : nullptr;
}

tagval *addreleaseaction(ident *id, int numargs)
{
    if(!keypressed || numargs > 3)
    {
        return nullptr;
    }
    releaseactions.emplace_back();
    releaseaction &ra = releaseactions.back();
    ra.key = keypressed;
    ra.id = id;
    ra.numargs = numargs;
    return ra.args;
}

//print to a stream f the binds in the binds vector
void writebinds(std::fstream& f)
{
    static const char * const cmds[3] = { "bind", "specbind", "editbind" };
    std::vector<KeyMap *> binds;
    for(auto &[k, km] : keyms)
    {
        binds.push_back(&km);
    }
    std::sort(binds.begin(), binds.end());
    for(int j = 0; j < 3; ++j)
    {
        for(KeyMap *&km : binds)
        {
            if(*(km->actions[j]))
            {
                if(validateblock(km->actions[j]))
                {
                    f << cmds[j] << " " << escapestring(km->name) << " [" << km->actions[j] << "]\n";
                }
                else
                {
                    f << cmds[j] << " " << escapestring(km->name) << " " << escapestring(km->actions[j]) << std::endl;
                }
            }
        }
    }
}

extern void writecompletions(std::fstream& f)
{
    ::cfinder.writecompletions(f);
}

void initconsolecmds()
{
    addcommand("fullconsole", reinterpret_cast<identfun>(fullconsole), "iN$", Id_Command);
    addcommand("toggleconsole", reinterpret_cast<identfun>(toggleconsole), "", Id_Command);

    static auto conskipcmd = [] (int *n)
    {
        setconskip(conskip, UI::uivisible("fullconsole") ? fullconfilter : confilter, *n);
    };
    addcommand("conskip", reinterpret_cast<identfun>(+conskipcmd), "i", Id_Command);


    static auto miniconskipcmd = [] (int *n)
    {
        setconskip(miniconskip, miniconfilter, *n);
    };
    addcommand("miniconskip", reinterpret_cast<identfun>(+miniconskipcmd), "i", Id_Command);

    addcommand("clearconsole", reinterpret_cast<identfun>(clearconsole), "", Id_Command);
    addcommand("keymap", reinterpret_cast<identfun>(keymap), "is", Id_Command);

    static auto bind = [] (char *key, char *action)
    {
        bindkey(key, action, KeyMap::Action_Default, "bind");
    };
    addcommand("bind", reinterpret_cast<identfun>(+bind), "ss", Id_Command);

    static auto specbind = [] (char *key, char *action)
    {
        bindkey(key, action, KeyMap::Action_Spectator, "specbind");
    };
    addcommand("specbind", reinterpret_cast<identfun>(+specbind), "ss", Id_Command);

    static auto editbind = [] (char *key, char *action)
    {
        bindkey(key, action, KeyMap::Action_Editing, "editbind");
    };
    addcommand("editbind", reinterpret_cast<identfun>(+editbind), "ss", Id_Command);

    static auto getbindcmd = [] (char *key)
    {
        getbind(key, KeyMap::Action_Default);
    };
    addcommand("getbind", reinterpret_cast<identfun>(+getbindcmd), "s", Id_Command);

    static auto getspecbind = [] (char *key)
    {
        getbind(key, KeyMap::Action_Spectator);
    };
    addcommand("getspecbind", reinterpret_cast<identfun>(+getspecbind), "s", Id_Command);

    static auto geteditbind = [] (char *key)
    {
        getbind(key, KeyMap::Action_Editing);
    };
    addcommand("geteditbind", reinterpret_cast<identfun>(+geteditbind), "s", Id_Command);

    static auto searchbindscmd = [] (char *action)
    {
        searchbinds(action, KeyMap::Action_Default);
    };
    addcommand("searchbinds", reinterpret_cast<identfun>(+searchbindscmd), "s", Id_Command);

    static auto searchspecbinds = [] (char *action)
    {
        searchbinds(action, KeyMap::Action_Spectator);
    };
    addcommand("searchspecbinds", reinterpret_cast<identfun>(+searchspecbinds), "s", Id_Command);

    static auto searcheditbinds = [] (char *action)
    {
        searchbinds(action, KeyMap::Action_Editing);
    };
    addcommand("searcheditbinds", reinterpret_cast<identfun>(+searcheditbinds), "s", Id_Command);

    static auto clearbinds = [] ()
    {
        for(auto &[k, km] : keyms)
        {
            km.clear(KeyMap::Action_Default);
        }
    };
    addcommand("clearbinds", reinterpret_cast<identfun>(+clearbinds), "", Id_Command);

    static auto clearspecbinds = [] ()
    {
        for(auto &[k, km] : keyms)
        {
            km.clear(KeyMap::Action_Spectator);
        }
    };
    addcommand("clearspecbinds", reinterpret_cast<identfun>(+clearspecbinds), "", Id_Command);

    static auto cleareditbinds = [] ()
    {
        for(auto &[k, km] : keyms)
        {
            km.clear(KeyMap::Action_Editing);
        }
    };
    addcommand("cleareditbinds", reinterpret_cast<identfun>(+cleareditbinds), "", Id_Command);

    static auto clearallbinds = [] ()
    {
        for(auto &[k, km] : keyms)
        {
            km.clear();
        }
    };
    addcommand("clearallbinds", reinterpret_cast<identfun>(+clearallbinds), "", Id_Command);
    addcommand("inputcommand", reinterpret_cast<identfun>(inputcommand), "ssss", Id_Command);
    addcommand("saycommand", reinterpret_cast<identfun>(saycommand), "C", Id_Command);
    addcommand("history", reinterpret_cast<identfun>(historycmd), "i", Id_Command);
    addcommand("onrelease", reinterpret_cast<identfun>(onrelease), "s", Id_Command);
    addcommand("complete", reinterpret_cast<identfun>(+[] (char *command, char *dir, char *ext) {::cfinder.addfilecomplete(command, dir, ext);}), "sss", Id_Command);
    addcommand("listcomplete", reinterpret_cast<identfun>(+[] (char *command, char *list) {::cfinder.addlistcomplete(command, list);}), "ss", Id_Command);
}

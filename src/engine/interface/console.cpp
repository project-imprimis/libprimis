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
#include "input.h"

#include "world/octaedit.h"

//internally relevant functionality
struct FilesKey
{
    int type;
    const char *dir, *ext;

    FilesKey() {}
    FilesKey(int type, const char *dir, const char *ext) : type(type), dir(dir), ext(ext) {}
};

static inline bool htcmp(const FilesKey &x, const FilesKey &y)
{
    return x.type == y.type && !std::strcmp(x.dir, y.dir) && (x.ext == y.ext || (x.ext && y.ext && !std::strcmp(x.ext, y.ext)));
}

static inline uint hthash(const FilesKey &k)
{
    return hthash(k.dir);
}

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

    int commandmillis = -1;
    string commandbuf;
    char *commandaction = nullptr,
         *commandprompt = nullptr;
    enum
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

    void resetcomplete();
    void complete(char *s, int maxlen, const char *cmdprefix);

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
    COMMAND(fullconsole, "iN$");

    void toggleconsole()
    {
        UI::toggleui("fullconsole");
    }
    COMMAND(toggleconsole, "");

    VARP(consize, 0, 5, 100);                   //font size of the console text
    VARP(miniconsize, 0, 5, 100);               //miniature console font size
    VARP(miniconwidth, 0, 40, 100);             //miniature console width
    VARP(confade, 0, 30, 60);                   //seconds before fading console
    VARP(miniconfade, 0, 30, 60);
    HVARP(confilter, 0, 0xFFFFFF, 0xFFFFFF);
    HVARP(fullconfilter, 0, 0xFFFFFF, 0xFFFFFF);
    HVARP(miniconfilter, 0, 0, 0xFFFFFF);

    int conskip = 0,
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
            if(conlines[skip].type&filter)
            {
                --offsetnum;
            }
        }
    }

    void conskipcmd(int *n)
    {
        setconskip(conskip, UI::uivisible("fullconsole") ? fullconfilter : confilter, *n);
    }
    COMMANDN(conskip, conskipcmd, "i");

    void miniconskipcmd(int *n)
    {
        setconskip(miniconskip, miniconfilter, *n);
    }
    COMMANDN(miniconskip, miniconskipcmd, "i");

    void clearconsole()
    {
        while(conlines.size())
        {
            delete[] conlines.back().line;
            conlines.pop_back();
        }
    }
    COMMAND(clearconsole, "");

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
            draw_text(line, conoff, y, 0xFF, 0xFF, 0xFF, 0xFF, -1, conwidth);
            if(dir > 0)
            {
                y += height;
            }
        }
        return y+conoff;
    }

    // keymap is defined externally in keymap.cfg

    struct KeyM
    {
        enum
        {
            Action_Default = 0,
            Action_Spectator,
            Action_Editing,
            Action_NumActions
        };

        int code;
        char *name;
        char *actions[Action_NumActions];
        bool pressed;

        KeyM() : code(-1), name(nullptr), pressed(false)
        {
            for(int i = 0; i < Action_NumActions; ++i)
            {
                actions[i] = newstring("");
            }
        }
        ~KeyM()
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

    hashtable<int, KeyM> keyms(128);

    void keymap(int *code, char *key)
    {
        if(identflags&Idf_Overridden)
        {
            conoutf(Console_Error, "cannot override keymap %d", *code);
            return;
        }
        KeyM &km = keyms[*code];
        km.code = *code;
        delete[] km.name;
        km.name = newstring(key);
    }
    COMMAND(keymap, "is");

    KeyM *keypressed = nullptr;
    char *keyaction = nullptr;

    void searchbinds(char *action, int type)
    {
        vector<char> names;
        ENUMERATE(keyms, KeyM, km,
        {
            if(!std::strcmp(km.actions[type], action))
            {
                if(names.length())
                {
                    names.add(' ');
                }
                names.put(km.name, std::strlen(km.name));
            }
        });
        names.add('\0');
        result(names.getbuf());
    }

    KeyM *findbind(char *key)
    {
        ENUMERATE(keyms, KeyM, km,
        {
            if(!strcasecmp(km.name, key))
            {
                return &km;
            }
        });
        return nullptr;
    }

    void getbind(char *key, int type)
    {
        KeyM *km = findbind(key);
        result(km ? km->actions[type] : "");
    }

    void bindkey(char *key, char *action, int state, const char *cmd)
    {
        if(identflags&Idf_Overridden)
        {
            conoutf(Console_Error, "cannot override %s \"%s\"", cmd, key);
            return;
        }
        KeyM *km = findbind(key);
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

    void bind(char *key, char *action)
    {
        bindkey(key, action, KeyM::Action_Default, "bind");
    }
    COMMAND(bind, "ss");

    void specbind(char *key, char *action)
    {
        bindkey(key, action, KeyM::Action_Spectator, "specbind");
    }
    COMMAND(specbind, "ss");

    void editbind(char *key, char *action)
    {
        bindkey(key, action, KeyM::Action_Editing, "editbind");
    }
    COMMAND(editbind, "ss");

    void getbindcmd(char *key)
    {
        getbind(key, KeyM::Action_Default);
    }
    COMMANDN(getbind, getbindcmd, "s");

    void getspecbind(char *key)
    {
        getbind(key, KeyM::Action_Spectator);
    }
    COMMAND(getspecbind, "s");

    void geteditbind(char *key)
    {
        getbind(key, KeyM::Action_Editing);
    }
    COMMAND(geteditbind, "s");

    void searchbindscmd(char *action)
    {
        searchbinds(action, KeyM::Action_Default);
    }
    COMMANDN(searchbinds, searchbindscmd, "s");

    void searchspecbinds(char *action)
    {
        searchbinds(action, KeyM::Action_Spectator);
    }
    COMMAND(searchspecbinds, "s");

    void searcheditbinds(char *action)
    {
        searchbinds(action, KeyM::Action_Editing);
    }
    COMMAND(searcheditbinds, "s");

    void KeyM::clear(int type)
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

    void clearbinds()
    {
        ENUMERATE(keyms, KeyM, km, km.clear(KeyM::Action_Default));
    }
    COMMAND(clearbinds, "");

    void clearspecbinds()
    {
        ENUMERATE(keyms, KeyM, km, km.clear(KeyM::Action_Spectator));
    }
    COMMAND(clearspecbinds, "");

    void cleareditbinds()
    {
        ENUMERATE(keyms, KeyM, km, km.clear(KeyM::Action_Editing));
    }
    COMMAND(cleareditbinds, "");

    void clearallbinds()
    {
        ENUMERATE(keyms, KeyM, km, km.clear());
    }
    COMMAND(clearallbinds, "");

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
    COMMAND(inputcommand, "ssss");

    void saycommand(char *init)
    {
        inputcommand(init);
    }
    COMMAND(saycommand, "C");

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
               commandlen = std::strlen(commandbuf),
               decoded = decodeutf8(reinterpret_cast<uchar *>(&commandbuf[commandlen]), sizeof(commandbuf)-1-commandlen, reinterpret_cast<const uchar *>(cb), cblen);
        commandbuf[commandlen + decoded] = '\0';
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

        void restore()
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

        bool shouldsave()
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

        void run()
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
    COMMANDN(history, historycmd, "i");

    struct releaseaction
    {
        KeyM *key;
        union
        {
            char *action;
            ident *id;
        };
        int numargs;
        tagval args[3];
    };
    vector<releaseaction> releaseactions;

    void onrelease(const char *s)
    {
        addreleaseaction(newstring(s));
    }
    COMMAND(onrelease, "s");

    void execbind(KeyM &k, bool isdown)
    {
        for(int i = 0; i < releaseactions.length(); i++)
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
                releaseactions.remove(i--);
            }
        }
        if(isdown)
        {
            int state = KeyM::Action_Default;
            if(!mainmenu)
            {
                if(editmode)
                {
                    state = KeyM::Action_Editing;
                }
                else if(player->state==ClientState_Spectator)
                {
                    state = KeyM::Action_Spectator;
                }
            }
            char *&action = k.actions[state][0] ? k.actions[state] : k.actions[KeyM::Action_Default];
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
        resetcomplete();
        int cmdlen = static_cast<int>(std::strlen(commandbuf)),
            cmdspace = static_cast<int>(sizeof(commandbuf)) - (cmdlen+1);
        len = std::min(len, cmdspace);
        if(commandpos<0)
        {
            memcpy(&commandbuf[cmdlen], str, len);
        }
        else
        {
            memmove(&commandbuf[commandpos+len], &commandbuf[commandpos], cmdlen - commandpos);
            memcpy(&commandbuf[commandpos], str, len);
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
                    int len = static_cast<int>(std::strlen(commandbuf));
                    if(commandpos<0)
                    {
                        break;
                    }
                    memmove(&commandbuf[commandpos], &commandbuf[commandpos+1], len - commandpos);
                    resetcomplete();
                    if(commandpos>=len-1)
                    {
                        commandpos = -1;
                    }
                    break;
                }
                case SDLK_BACKSPACE:
                {
                    int len = static_cast<int>(std::strlen(commandbuf)),
                        i = commandpos>=0 ? commandpos : len;
                    if(i<1)
                    {
                        break;
                    }
                    memmove(&commandbuf[i-1], &commandbuf[i], len - i + 1);
                    resetcomplete();
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
                        complete(commandbuf, sizeof(commandbuf), commandflags&CmdFlags_Execute ? "/" : nullptr);
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

    // tab-completion of all idents and base maps

    enum
    {
        Files_Directory = 0,
        Files_List,
    };

    struct FilesVal
    {
        public:
            int type;
            char *dir, *ext;
            vector<char *> files;

            FilesVal(int type, const char *dir, const char *ext) : type(type), dir(newstring(dir)), ext(ext && ext[0] ? newstring(ext) : nullptr), millis(-1) {}
            ~FilesVal()
            {
                delete[] dir;
                delete[] ext;

                dir = nullptr;
                ext = nullptr;
                files.deletearrays();
            }

            void update()
            {
                if(type!=Files_Directory || millis >= commandmillis)
                {
                    return;
                }
                files.deletearrays();
                listfiles(dir, ext, files);
                files.sort();
                for(int i = 0; i < files.length(); i++)
                {
                    if(i && !std::strcmp(files[i], files[i-1]))
                    {
                        delete[] files.remove(i--);
                    }
                }
                millis = totalmillis;
            }

        private:
            int millis;
    };

    char *prependstring(char *d, const char *s, size_t len)
    {
        size_t slen = std::min(std::strlen(s), len);
        memmove(&d[slen], d, std::min(len - slen, std::strlen(d) + 1));
        memcpy(d, s, slen);
        d[len-1] = 0;
        return d;
    }

    hashtable<FilesKey, FilesVal *> completefiles;
    hashtable<char *, FilesVal *> completions;

    int completesize = 0;
    char *lastcomplete = nullptr;

    void resetcomplete()
    {
        completesize = 0;
    }

    void addcomplete(char *command, int type, char *dir, char *ext)
    {
        if(identflags&Idf_Overridden)
        {
            conoutf(Console_Error, "cannot override complete %s", command);
            return;
        }
        if(!dir[0])
        {
            FilesVal **hasfiles = completions.access(command);
            if(hasfiles)
            {
                *hasfiles = nullptr;
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
        FilesKey key(type, dir, ext);
        FilesVal **val = completefiles.access(key);
        if(!val)
        {
            FilesVal *f = new FilesVal(type, dir, ext);
            if(type==Files_List)
            {
                explodelist(dir, f->files);
            }
            val = &completefiles[FilesKey(type, f->dir, f->ext)];
            *val = f;
        }
        FilesVal **hasfiles = completions.access(command);
        if(hasfiles)
        {
            *hasfiles = *val;
        }
        else
        {
            completions[newstring(command)] = *val;
        }
    }

    void addfilecomplete(char *command, char *dir, char *ext)
    {
        addcomplete(command, Files_Directory, dir, ext);
    }
    COMMANDN(complete, addfilecomplete, "sss");

    void addlistcomplete(char *command, char *list)
    {
        addcomplete(command, Files_List, list, nullptr);
    }
    COMMANDN(listcomplete, addlistcomplete, "ss");

    void complete(char *s, int maxlen, const char *cmdprefix)
    {
        int cmdlen = 0;
        if(cmdprefix)
        {
            cmdlen = std::strlen(cmdprefix);
            if(strncmp(s, cmdprefix, cmdlen))
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
                f = completions.find(stringslice(&s[cmdlen], end), nullptr);
            }
        }
        const char *nextcomplete = nullptr;
        if(f) // complete using filenames
        {
            int commandsize = std::strchr(&s[cmdlen], ' ')+1-s;
            f->update();
            for(int i = 0; i < f->files.length(); i++)
            {
                if(strncmp(f->files[i], &s[commandsize], completesize+cmdlen-commandsize)==0 &&
                          (!lastcomplete || std::strcmp(f->files[i], lastcomplete) > 0) &&
                          (!nextcomplete || std::strcmp(f->files[i], nextcomplete) < 0))
                {
                    nextcomplete = f->files[i];
                }
            }
            cmdprefix = s;
            cmdlen = commandsize;
        }
        else // complete using command or var (ident) names
        {
            ENUMERATE(idents, ident, id,
                if(strncmp(id.name, &s[cmdlen], completesize)==0 &&
                          (!lastcomplete || std::strcmp(id.name, lastcomplete) > 0) &&
                          (!nextcomplete || std::strcmp(id.name, nextcomplete) < 0))
                {
                    nextcomplete = id.name;
                }
            );
        }

        delete[] lastcomplete;
        lastcomplete = nullptr;
        if(nextcomplete)
        {
            cmdlen = std::min(cmdlen, maxlen-1);
            if(cmdlen)
            {
                memmove(s, cmdprefix, cmdlen);
            }
            copystring(&s[cmdlen], nextcomplete, maxlen-cmdlen);
            lastcomplete = newstring(nextcomplete);
        }
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

void processkey(int code, bool isdown)
{
    KeyM *haskey = keyms.access(code);
    if(haskey && haskey->pressed)
    {
        execbind(*haskey, isdown); // allow pressed keys to release
    }
    else if(!UI::keypress(code, isdown)) // UI key intercept
    {
        if(!consolekey(code, isdown))
        {
            if(haskey)
            {
                execbind(*haskey, isdown);
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
    draw_text(buf, x, y, 0xFF, 0xFF, 0xFF, 0xFF, commandpos>=0 ? commandpos+1 + std::strlen(prompt) : std::strlen(buf), w);
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
    KeyM *km = keyms.access(code);
    return km ? km->name : nullptr;
}

const char *addreleaseaction(char *s)
{
    if(!keypressed)
    {
        delete[] s;
        return nullptr;
    }
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.action = s;
    ra.numargs = -1;
    return keypressed->name;
}

tagval *addreleaseaction(ident *id, int numargs)
{
    if(!keypressed || numargs > 3)
    {
        return nullptr;
    }
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.id = id;
    ra.numargs = numargs;
    return ra.args;
}

//print to a stream f the binds in the binds vector
void writebinds(stream *f)
{
    static const char * const cmds[3] = { "bind", "specbind", "editbind" };
    vector<KeyM *> binds;
    ENUMERATE(keyms, KeyM, km, binds.add(&km));
    binds.sortname();
    for(int j = 0; j < 3; ++j)
    {
        for(int i = 0; i < binds.length(); i++)
        {
            KeyM &km = *binds[i];
            if(*km.actions[j])
            {
                if(validateblock(km.actions[j]))
                {
                    f->printf("%s %s [%s]\n", cmds[j], escapestring(km.name), km.actions[j]);
                }
                else
                {
                    f->printf("%s %s %s\n", cmds[j], escapestring(km.name), escapestring(km.actions[j]));
                }
            }
        }
    }
}

//print to a stream f the listcompletions in the completions filesval
void writecompletions(stream *f)
{
    vector<char *> cmds;
    ENUMERATE_KT(completions, char *, k, FilesVal *, v, { if(v) cmds.add(k); });
    cmds.sort();
    for(int i = 0; i < cmds.length(); i++)
    {
        char *k = cmds[i];
        FilesVal *v = completions[k];
        if(v->type==Files_List)
        {
            if(validateblock(v->dir))
            {
                f->printf("listcomplete %s [%s]\n", escapeid(k), v->dir);
            }
            else
            {
                f->printf("listcomplete %s %s\n", escapeid(k), escapestring(v->dir));
            }
        }
        else
        {
            f->printf("complete %s %s %s\n", escapeid(k), escapestring(v->dir), escapestring(v->ext ? v->ext : "*"));
        }
    }
}

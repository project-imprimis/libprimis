/* command.cpp: script binding and language interpretation functionality
 *
 * libprimis uses a bespoke scripting language called cubescript, which allows
 * for commands to be declared in the code which can be natively called upon in
 * games. cubescript "builtin" commands and variables are declared with macros
 * (see command.h) and further aliases can be defined in cubescript files.
 *
 * for the file containing the cubescript "standard library", see cubestd.cpp.
 * Other files contain their own relevant builtin declarations (e.g. sound vars
 * in sound.cpp)
 *
 * command.cpp largely handles cubescript language interpretation, through a
 * bytecode compiler which allows for greater speed than naive approaches; this
 * is mostly necessary to handle the UI system, which is built on cubescript
 */

#include "../libprimis-headers/cube.h"
#include "../../shared/stream.h"

#include "console.h"
#include "control.h"
#include "cs.h"

#include "world/octaedit.h"

hashnameset<ident> idents; // contains ALL vars/commands/aliases
vector<ident *> identmap;
ident *dummyident = nullptr;
std::queue<ident *> triggerqueue; //for the game to handle var change events
static constexpr uint cmdqueuedepth = 128; //how many elements before oldest queued data gets discarded
int identflags = 0;

const char *sourcefile = nullptr,
           *sourcestr  = nullptr;

NullVal nullval;

vector<char> strbuf[4];
int stridx = 0;

IdentLink noalias = { nullptr, nullptr, (1<<Max_Args)-1, nullptr },
          *aliasstack = &noalias;

VARN(numargs, _numargs, Max_Args, 0, 0);

void freearg(tagval &v)
{
    switch(v.type)
    {
        case Value_String:
        {
            delete[] v.s;
            break;
        }
        case Value_Code:
        {
            if(v.code[-1] == Code_Start)
            {
                //need to cast away constness then mangle type to uchar to delete w/ same type as created
                delete[] reinterpret_cast<uchar *>(const_cast<uint *>(&v.code[-1]));
            }
            break;
        }
    }
}

static void forcenull(tagval &v)
{
    switch(v.type)
    {
        case Value_Null:
        {
            return;
        }
    }
    freearg(v);
    v.setnull();
}

static float forcefloat(tagval &v)
{
    float f = 0.0f;
    switch(v.type)
    {
        case Value_Integer:
        {
            f = v.i;
            break;
        }
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            f = parsefloat(v.s);
            break;
        }
        case Value_Float:
        {
            return v.f;
        }
    }
    freearg(v);
    v.setfloat(f);
    return f;
}

static int forceint(tagval &v)
{
    int i = 0;
    switch(v.type)
    {
        case Value_Float:
        {
            i = v.f;
            break;
        }
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            i = parseint(v.s);
            break;
        }
        case Value_Integer:
        {
            return v.i;
        }
    }
    freearg(v);
    v.setint(i);
    return i;
}

static const char *forcestr(tagval &v)
{
    const char *s = "";
    switch(v.type)
    {
        case Value_Float:
        {
            s = floatstr(v.f);
            break;
        }
        case Value_Integer:
        {
            s = intstr(v.i);
            break;
        }
        case Value_Macro:
        case Value_CString:
        {
            s = v.s;
            break;
        }
        case Value_String:
        {
            return v.s;
        }
    }
    freearg(v);
    v.setstr(newstring(s));
    return s;
}

static void forcearg(tagval &v, int type)
{
    switch(type)
    {
        case Ret_String:
        {
            if(v.type != Value_String)
            {
                forcestr(v);
            }
            break;
        }
        case Ret_Integer:
        {
            if(v.type != Value_Integer)
            {
                forceint(v);
            }
            break;
        }
        case Ret_Float:
        {
            if(v.type != Value_Float)
            {
                forcefloat(v);
            }
            break;
        }
    }
}

void tagval::cleanup()
{
    freearg(*this);
}

static void freeargs(tagval *args, int &oldnum, int newnum)
{
    for(int i = newnum; i < oldnum; i++)
    {
        freearg(args[i]);
    }
    oldnum = newnum;
}

void cleancode(ident &id)
{
    if(id.code)
    {
        id.code[0] -= 0x100;
        if(static_cast<int>(id.code[0]) < 0x100)
        {
            delete[] id.code;
        }
        id.code = nullptr;
    }
}

tagval noret = nullval,
       *commandret = &noret;

void clear_command()
{
    ENUMERATE(idents, ident, i,
    {
        if(i.type==Id_Alias)
        {
            delete[] i.name;
            i.name = nullptr;

            i.forcenull();

            delete[] i.code;
            i.code = nullptr;
        }
    });
}

void clearoverride(ident &i)
{
    if(!(i.flags&Idf_Overridden))
    {
        return;
    }
    switch(i.type)
    {
        case Id_Alias:
        {
            if(i.valtype==Value_String)
            {
                if(!i.val.s[0])
                {
                    break;
                }
                delete[] i.val.s;
            }
            cleancode(i);
            i.valtype = Value_String;
            i.val.s = newstring("");
            break;
        }
        case Id_Var:
        {
            *i.storage.i = i.overrideval.i;
            i.changed();
            break;
        }
        case Id_FloatVar:
        {
            *i.storage.f = i.overrideval.f;
            i.changed();
            break;
        }
        case Id_StringVar:
        {
            delete[] *i.storage.s;
            *i.storage.s = i.overrideval.s;
            i.changed();
            break;
        }
    }
    i.flags &= ~Idf_Overridden;
}

void clearoverrides()
{
    ENUMERATE(idents, ident, i, clearoverride(i));
}

static bool initedidents = false;
static vector<ident> *identinits = nullptr;

static ident *addident(const ident &id)
{
    if(!initedidents)
    {
        if(!identinits)
        {
            identinits = new vector<ident>;
        }
        identinits->add(id);
        return nullptr;
    }
    ident &def = idents.access(id.name, id);
    def.index = identmap.length();
    return identmap.add(&def);
}

bool initidents()
{
    initedidents = true;
    for(int i = 0; i < Max_Args; i++)
    {
        DEF_FORMAT_STRING(argname, "arg%d", i+1);
        newident(argname, Idf_Arg);
    }
    dummyident = newident("//dummy", Idf_Unknown);
    if(identinits)
    {
        for(int i = 0; i < (*identinits).length(); i++)
        {
            addident((*identinits)[i]);
        }
        if(identinits)
        {
            delete identinits;
            identinits = nullptr;
        }
    }
    return true;
}

static const char *debugline(const char *p, const char *fmt)
{
    if(!sourcestr)
    {
        return fmt;
    }
    int num = 1;
    const char *line = sourcestr;
    for(;;)
    {
        const char *end = std::strchr(line, '\n'); //search for newline
        if(!end)
        {
            end = line + std::strlen(line);
        }
        if(p >= line && p <= end)
        {
            static string buf;
            if(sourcefile)
            {
                formatstring(buf, "%s:%d: %s", sourcefile, num, fmt);
            }
            else
            {
                formatstring(buf, "%d: %s", num, fmt);
            }
            return buf;
        }
        if(!*end)
        {
            break;
        }
        line = end + 1;
        num++;
    }
    return fmt;
}

VAR(debugalias, 0, 4, 1000); //depth to which alias aliasing should be debugged (disabled if 0)

static void dodebugalias()
{
    if(!debugalias)
    {
        return;
    }
    int total = 0,
        depth = 0;
    for(IdentLink *l = aliasstack; l != &noalias; l = l->next)
    {
        total++;
    }
    for(IdentLink *l = aliasstack; l != &noalias; l = l->next)
    {
        ident *id = l->id;
        ++depth;
        if(depth < debugalias)
        {
            conoutf(Console_Error, "  %d) %s", total-depth+1, id->name);
        }
        else if(l->next == &noalias)
        {
            conoutf(Console_Error, depth == debugalias ? "  %d) %s" : "  ..%d) %s", total-depth+1, id->name);
        }
    }
}

static int nodebug = 0;

static void debugcode(const char *fmt, ...) PRINTFARGS(1, 2);

static void debugcode(const char *fmt, ...)
{
    if(nodebug)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    conoutfv(Console_Error, fmt, args);
    va_end(args);

    dodebugalias();
}

static void debugcodeline(const char *p, const char *fmt, ...) PRINTFARGS(2, 3);

static void debugcodeline(const char *p, const char *fmt, ...)
{
    if(nodebug)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    conoutfv(Console_Error, debugline(p, fmt), args);
    va_end(args);

    dodebugalias();
}

static void nodebugcmd(uint *body)
{
    nodebug++;
    executeret(body, *commandret);
    nodebug--;
}
COMMANDN(nodebug, nodebugcmd, "e");

void addident(ident *id)
{
    addident(*id);
}

void pusharg(ident &id, const tagval &v, identstack &stack)
{
    stack.val = id.val;
    stack.valtype = id.valtype;
    stack.next = id.stack;
    id.stack = &stack;
    id.setval(v);
    cleancode(id);
}

void poparg(ident &id)
{
    if(!id.stack)
    {
        return;
    }
    identstack *stack = id.stack;
    if(id.valtype == Value_String)
    {
        delete[] id.val.s;
    }
    id.setval(*stack);
    cleancode(id);
    id.stack = stack->next;
}

void undoarg(ident &id, identstack &stack)
{
    identstack *prev = id.stack;
    stack.val = id.val;
    stack.valtype = id.valtype;
    stack.next = prev;
    id.stack = prev->next;
    id.setval(*prev);
    cleancode(id);
}

void redoarg(ident &id, const identstack &stack)
{
    identstack *prev = stack.next;
    prev->val = id.val;
    prev->valtype = id.valtype;
    id.stack = prev;
    id.setval(stack);
    cleancode(id);
}

void pushcmd(ident *id, tagval *v, uint *code)
{
    if(id->type != Id_Alias || id->index < Max_Args)
    {
        return;
    }
    identstack stack;
    pusharg(*id, *v, stack);
    v->type = Value_Null;
    id->flags &= ~Idf_Unknown;
    executeret(code, *commandret);
    poparg(*id);
}
COMMANDN(push, pushcmd, "rTe");

static void pushalias(ident &id, identstack &stack)
{
    if(id.type == Id_Alias && id.index >= Max_Args)
    {
        pusharg(id, nullval, stack);
        id.flags &= ~Idf_Unknown;
    }
}

static void popalias(ident &id)
{
    if(id.type == Id_Alias && id.index >= Max_Args)
    {
        poparg(id);
    }
}

KEYWORD(local, Id_Local);

static bool checknumber(const char *s)
{
    if(isdigit(s[0]))
    {
        return true;
    }
    else switch(s[0])
    {
        case '+':
        case '-':
        {
            return isdigit(s[1]) || (s[1] == '.' && isdigit(s[2]));
        }
        case '.':
        {
            return isdigit(s[1]) != 0;
        }
        default:
        {
            return false;
        }
    }
}
static bool checknumber(const stringslice &s)
{
    return checknumber(s.str);
}

template<class T>
static ident *newident(const T &name, int flags)
{
    ident *id = idents.access(name);
    if(!id)
    {
        if(checknumber(name))
        {
            debugcode("number %.*s is not a valid identifier name", stringlen(name), stringptr(name));
            return dummyident;
        }
        id = addident(ident(Id_Alias, newstring(name), flags));
    }
    return id;
}

static ident *forceident(tagval &v)
{
    switch(v.type)
    {
        case Value_Ident:
        {
            return v.id;
        }
        case Value_Macro:
        case Value_CString:
        {
            ident *id = newident(v.s, Idf_Unknown);
            v.setident(id);
            return id;
        }
        case Value_String:
        {
            ident *id = newident(v.s, Idf_Unknown);
            delete[] v.s;
            v.setident(id);
            return id;
        }
    }
    freearg(v);
    v.setident(dummyident);
    return dummyident;
}

ident *newident(const char *name, int flags)
{
    return newident<const char *>(name, flags);
}

ident *writeident(const char *name, int flags)
{
    ident *id = newident(name, flags);
    if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index)))
    {
        pusharg(*id, nullval, aliasstack->argstack[id->index]);
        aliasstack->usedargs |= 1<<id->index;
    }
    return id;
}

ident *readident(const char *name)
{
    ident *id = idents.access(name);
    if(id && id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index)))
    {
       return nullptr;
    }
    return id;
}

void resetvar(char *name)
{
    ident *id = idents.access(name);
    if(!id)
    {
        return;
    }
    if(id->flags&Idf_ReadOnly)
    {
        debugcode("variable %s is read-only", id->name);
    }
    else
    {
        clearoverride(*id);
    }
}

COMMAND(resetvar, "s");

void setarg(ident &id, tagval &v)
{
    if(aliasstack->usedargs&(1<<id.index))
    {
        if(id.valtype == Value_String)
        {
            delete[] id.val.s;
        }
        id.setval(v);
        cleancode(id);
    }
    else
    {
        pusharg(id, v, aliasstack->argstack[id.index]);
        aliasstack->usedargs |= 1<<id.index;
    }
}

void setalias(ident &id, tagval &v)
{
    if(id.valtype == Value_String)
    {
        delete[] id.val.s;
    }
    id.setval(v);
    cleancode(id);
    id.flags = (id.flags & identflags) | identflags;
}

static void setalias(const char *name, tagval &v)
{
    ident *id = idents.access(name);
    if(id)
    {
        switch(id->type)
        {
            case Id_Alias:
            {
                if(id->index < Max_Args)
                {
                    setarg(*id, v);
                }
                else
                {
                    setalias(*id, v);
                }
                return;
            }
            case Id_Var:
            {
                setvarchecked(id, v.getint());
                break;
            }
            case Id_FloatVar:
            {
                setfvarchecked(id, v.getfloat());
                break;
            }
            case Id_StringVar:
            {
                setsvarchecked(id, v.getstr());
                break;
            }
            default:
            {
                debugcode("cannot redefine builtin %s with an alias", id->name);
                break;
            }
        }
        freearg(v);
    }
    else if(checknumber(name))
    {
        debugcode("cannot alias number %s", name);
        freearg(v);
    }
    else
    {
        addident(ident(Id_Alias, newstring(name), v, identflags));
    }
}

void alias(const char *name, const char *str)
{
    tagval v;
    v.setstr(newstring(str));
    setalias(name, v);
}

void alias(const char *name, tagval &v)
{
    setalias(name, v);
}

void aliascmd(const char *name, tagval *v)
{
    setalias(name, *v);
    v->type = Value_Null;
}
COMMANDN(alias, aliascmd, "sT");

// variables and commands are registered through globals, see cube.h

int variable(const char *name, int min, int cur, int max, int *storage, identfun fun, int flags)
{
    addident(ident(Id_Var, name, min, max, storage, reinterpret_cast<void *>(fun), flags));
    return cur;
}

float fvariable(const char *name, float min, float cur, float max, float *storage, identfun fun, int flags)
{
    addident(ident(Id_FloatVar, name, min, max, storage, reinterpret_cast<void *>(fun), flags));
    return cur;
}

char *svariable(const char *name, const char *cur, char **storage, identfun fun, int flags)
{
    addident(ident(Id_StringVar, name, storage, reinterpret_cast<void *>(fun), flags));
    return newstring(cur);
}

struct DefVar : identval
{
    char *name;
    uint *onchange;

    DefVar() : name(nullptr), onchange(nullptr) {}

    ~DefVar()
    {
        delete[] name;
        name = nullptr;
        if(onchange)
        {
            freecode(onchange);
        }
    }

    static void changed(ident *id)
    {
        DefVar *v = static_cast<DefVar *>(id->storage.p);
        if(v->onchange)
        {
            execute(v->onchange);
        }
    }
};

hashnameset<DefVar> defvars;

#define DEFVAR(cmdname, fmt, args, body) \
    ICOMMAND(cmdname, fmt, args, \
    { \
        if(idents.access(name)) \
        { \
            debugcode("cannot redefine %s as a variable", name); \
            return; \
        } \
        name = newstring(name); \
        DefVar &def = defvars[name]; \
        def.name = name; \
        def.onchange = onchange[0] ? compilecode(onchange) : nullptr; \
        body; \
    });
#define DEFIVAR(cmdname, flags) \
    DEFVAR(cmdname, "siiis", (char *name, int *min, int *cur, int *max, char *onchange), \
        def.i = variable(name, *min, *cur, *max, &def.i, def.onchange ? DefVar::changed : nullptr, flags))
#define DEFFVAR(cmdname, flags) \
    DEFVAR(cmdname, "sfffs", (char *name, float *min, float *cur, float *max, char *onchange), \
        def.f = fvariable(name, *min, *cur, *max, &def.f, def.onchange ? DefVar::changed : nullptr, flags))
#define DEFSVAR(cmdname, flags) \
    DEFVAR(cmdname, "sss", (char *name, char *cur, char *onchange), \
        def.s = svariable(name, cur, &def.s, def.onchange ? DefVar::changed : nullptr, flags))

DEFIVAR(defvar, 0);
DEFIVAR(defvarp, Idf_Persist);
DEFFVAR(deffvar, 0);
DEFFVAR(deffvarp, Idf_Persist);
DEFSVAR(defsvar, 0);
DEFSVAR(defsvarp, Idf_Persist);

#define GETVAR_(id, vartype, name, retval) \
    ident *id = idents.access(name); \
    if(!id || id->type!=vartype) \
    { \
        return retval; \
    }

#define GETVAR(id, name, retval) GETVAR_(id, Id_Var, name, retval)

#define OVERRIDEVAR(errorval, saveval, resetval, clearval) \
    if(identflags&Idf_Overridden || id->flags&Idf_Override) \
    { \
        if(id->flags&Idf_Persist) \
        { \
            debugcode("cannot override persistent variable %s", id->name); \
            errorval; \
        } \
        if(!(id->flags&Idf_Overridden)) \
        { \
            saveval; \
            id->flags |= Idf_Overridden; \
        } \
        else \
        { \
            clearval; \
        } \
    } \
    else \
    { \
        if(id->flags&Idf_Overridden) \
        { \
            resetval; \
            id->flags &= ~Idf_Overridden; \
        } \
        clearval; \
    }

void setvar(const char *name, int i, bool dofunc, bool doclamp)
{
    GETVAR(id, name, );
    OVERRIDEVAR(return, id->overrideval.i = *id->storage.i, , )
    if(doclamp)
    {
        *id->storage.i = std::clamp(i, id->minval, id->maxval);
    }
    else
    {
        *id->storage.i = i;
    }
    if(dofunc)
    {
        id->changed();
    }
}
void setfvar(const char *name, float f, bool dofunc, bool doclamp)
{
    GETVAR_(id, Id_FloatVar, name, );
    OVERRIDEVAR(return, id->overrideval.f = *id->storage.f, , );
    if(doclamp)
    {
        *id->storage.f = std::clamp(f, id->minvalf, id->maxvalf);
    }
    else
    {
        *id->storage.f = f;
    }
    if(dofunc)
    {
        id->changed();
    }
}
void setsvar(const char *name, const char *str, bool dofunc)
{
    GETVAR_(id, Id_StringVar, name, );
    OVERRIDEVAR(return, id->overrideval.s = *id->storage.s, delete[] id->overrideval.s, delete[] *id->storage.s);
    *id->storage.s = newstring(str);
    if(dofunc)
    {
        id->changed();
    }
}
int getvar(const char *name)
{
    GETVAR(id, name, 0);
    return *id->storage.i;
}
int getvarmin(const char *name)
{
    GETVAR(id, name, 0);
    return id->minval;
}
int getvarmax(const char *name)
{
    GETVAR(id, name, 0);
    return id->maxval;
}
float getfvarmin(const char *name)
{
    GETVAR_(id, Id_FloatVar, name, 0);
    return id->minvalf;
}
float getfvarmax(const char *name)
{
    GETVAR_(id, Id_FloatVar, name, 0);
    return id->maxvalf;
}

ICOMMAND(getvarmin, "s", (char *s), intret(getvarmin(s)));
ICOMMAND(getvarmax, "s", (char *s), intret(getvarmax(s)));
ICOMMAND(getfvarmin, "s", (char *s), floatret(getfvarmin(s)));
ICOMMAND(getfvarmax, "s", (char *s), floatret(getfvarmax(s)));

bool identexists(const char *name)
{
    return idents.access(name) != nullptr;
}
ICOMMAND(identexists, "s", (char *s), intret(identexists(s) ? 1 : 0));

ident *getident(const char *name)
{
    return idents.access(name);
}

void touchvar(const char *name)
{
    ident *id = idents.access(name);
    if(id) switch(id->type)
    {
        case Id_Var:
        case Id_FloatVar:
        case Id_StringVar:
        {
            id->changed();
            break;
        }
    }
}

const char *getalias(const char *name)
{
    ident *i = idents.access(name);
    return i && i->type==Id_Alias && (i->index >= Max_Args || aliasstack->usedargs&(1<<i->index)) ? i->getstr() : "";
}

ICOMMAND(getalias, "s", (char *s), result(getalias(s)));

int clampvar(ident *id, int val, int minval, int maxval)
{
    if(val < minval)
    {
        val = minval;
    }
    else if(val > maxval)
    {
        val = maxval;
    }
    else
    {
        return val;
    }
    debugcode(id->flags&Idf_Hex ?
            (minval <= 255 ? "valid range for %s is %d..0x%X" : "valid range for %s is 0x%X..0x%X") :
            "valid range for %s is %d..%d",
        id->name, minval, maxval);
    return val;
}

void vartrigger(ident *id) //places an ident pointer into queue for the game to handle
{
    triggerqueue.push(id);
    if(triggerqueue.size() > cmdqueuedepth)
    {
        triggerqueue.pop();
    }
}

void setvarchecked(ident *id, int val)
{
    if(id->flags&Idf_ReadOnly)
    {
        debugcode("variable %s is read-only", id->name);
    }
    else if(!(id->flags&Idf_Override) || identflags&Idf_Overridden || allowediting)
    {
        OVERRIDEVAR(return, id->overrideval.i = *id->storage.i, , )
        if(val < id->minval || val > id->maxval)
        {
            val = clampvar(id, val, id->minval, id->maxval);
        }
        *id->storage.i = val;
        id->changed();                                             // call trigger function if available
        if(id->flags&Idf_Override && !(identflags&Idf_Overridden))
        {
            vartrigger(id);
        }
    }
}

static void setvarchecked(ident *id, tagval *args, int numargs)
{
    int val = forceint(args[0]);
    if(id->flags&Idf_Hex && numargs > 1)
    {
        val = (val << 16) | (forceint(args[1])<<8);
        if(numargs > 2)
        {
            val |= forceint(args[2]);
        }
    }
    setvarchecked(id, val);
}

float clampfvar(ident *id, float val, float minval, float maxval)
{
    if(val < minval)
    {
        val = minval;
    }
    else if(val > maxval)
    {
        val = maxval;
    }
    else
    {
        return val;
    }
    debugcode("valid range for %s is %s..%s", id->name, floatstr(minval), floatstr(maxval));
    return val;
}

void setfvarchecked(ident *id, float val)
{
    if(id->flags&Idf_ReadOnly)
    {
        debugcode("variable %s is read-only", id->name);
    }
    else if(!(id->flags&Idf_Override) || identflags&Idf_Overridden || allowediting)
    {
        OVERRIDEVAR(return, id->overrideval.f = *id->storage.f, , );
        if(val < id->minvalf || val > id->maxvalf)
        {
            val = clampfvar(id, val, id->minvalf, id->maxvalf);
        }
        *id->storage.f = val;
        id->changed();
        if(id->flags&Idf_Override && !(identflags&Idf_Overridden))
        {
            vartrigger(id);
        }
    }
}

void setsvarchecked(ident *id, const char *val)
{
    if(id->flags&Idf_ReadOnly)
    {
        debugcode("variable %s is read-only", id->name);
    }
    else if(!(id->flags&Idf_Override) || identflags&Idf_Overridden || allowediting)
    {
        OVERRIDEVAR(return, id->overrideval.s = *id->storage.s, delete[] id->overrideval.s, delete[] *id->storage.s);
        *id->storage.s = newstring(val);
        id->changed();
        if(id->flags&Idf_Override && !(identflags&Idf_Overridden))
        {
            vartrigger(id);
        }
    }
}

bool addcommand(const char *name, identfun fun, const char *args, int type)
{
    uint argmask = 0;
    int numargs = 0;
    bool limit = true;
    if(args)
    {
        for(const char *fmt = args; *fmt; fmt++)
        {
            switch(*fmt)
            {
                case 'i':
                case 'b':
                case 'f':
                case 'F':
                case 't':
                case 'T':
                case 'E':
                case 'N':
                case 'D':
                {
                    if(numargs < Max_Args)
                    {
                        numargs++;
                    }
                    break;
                }
                case 'S':
                case 's':
                case 'e':
                case 'r':
                case '$':
                {
                    if(numargs < Max_Args)
                    {
                        argmask |= 1<<numargs;
                        numargs++;
                    }
                    break;
                }
                case '1':
                case '2':
                case '3':
                case '4':
                {
                    if(numargs < Max_Args)
                    {
                        fmt -= *fmt-'0'+1;
                    }
                    break;
                }
                case 'C':
                case 'V':
                {
                    limit = false;
                    break;
                }
                default:
                {
                    fatal("builtin %s declared with illegal type: %s", name, args);
                    break;
                }
            }
        }
    }
    if(limit && numargs > Max_CommandArgs)
    {
        fatal("builtin %s declared with too many args: %d", name, numargs);
    }
    addident(ident(type, name, args, argmask, numargs, reinterpret_cast<void *>(fun)));
    return false;
}

const char *parsestring(const char *p)
{
    for(; *p; p++)
    {
        switch(*p)
        {
            case '\r':
            case '\n':
            case '\"':
            {
                return p;
            }
            case '^':
            {
                if(*++p)
                {
                    break;
                }
                return p;
            }
        }
    }
    return p;
}

int unescapestring(char *dst, const char *src, const char *end)
{
    char *start = dst;
    while(src < end)
    {
        int c = *src++;
        if(c == '^')
        {
            if(src >= end)
            {
                break;
            }
            int e = *src++;
            switch(e)
            {
                case 'n':
                {
                    *dst++ = '\n';
                    break;
                }
                case 't':
                {
                    *dst++ = '\t';
                    break;
                }
                case 'f':
                {
                    *dst++ = '\f';
                    break;
                }
                default:
                {
                    *dst++ = e;
                    break;
                }
            }
        }
        else
        {
            *dst++ = c;
        }
    }
    *dst = '\0';
    return dst - start;
}

static char *conc(vector<char> &buf, tagval *v, int n, bool space, const char *prefix = nullptr, int prefixlen = 0)
{
    if(prefix)
    {
        buf.put(prefix, prefixlen);
        if(space && n)
        {
            buf.add(' ');
        }
    }
    for(int i = 0; i < n; ++i)
    {
        const char *s = "";
        int len = 0;
        switch(v[i].type)
        {
            case Value_Integer:
            {
                s = intstr(v[i].i);
                break;
            }
            case Value_Float:
            {
                s = floatstr(v[i].f);
                break;
            }
            case Value_String:
            case Value_CString:
            {
                s = v[i].s;
                break;
            }
            case Value_Macro:
            {
                s = v[i].s;
                len = v[i].code[-1]>>8;
                goto haslen; //skip `len` assignment
            }
        }
        len = static_cast<int>(std::strlen(s));
    haslen:
        buf.put(s, len);
        if(i == n-1)
        {
            break;
        }
        if(space)
        {
            buf.add(' ');
        }
    }
    buf.add('\0');
    return buf.getbuf();
}

static char *conc(tagval *v, int n, bool space, const char *prefix, int prefixlen)
{
    static int vlen[Max_Args];
    static char numbuf[3*maxstrlen];
    int len    = prefixlen,
        numlen = 0,
        i      = 0;
    for(; i < n; i++)
    {
        switch(v[i].type)
        {
            case Value_Macro:
            {
                len += (vlen[i] = v[i].code[-1]>>8);
                break;
            }
            case Value_String:
            case Value_CString:
            {
                len += (vlen[i] = static_cast<int>(std::strlen(v[i].s)));
                break;
            }
            case Value_Integer:
            {
                if(numlen + maxstrlen > static_cast<int>(sizeof(numbuf)))
                {
                    goto overflow;
                }
                intformat(&numbuf[numlen], v[i].i);
                numlen += (vlen[i] = std::strlen(&numbuf[numlen]));
                break;
            }
            case Value_Float:
            {
                if(numlen + maxstrlen > static_cast<int>(sizeof(numbuf)))
                {
                    goto overflow;
                }
                floatformat(&numbuf[numlen], v[i].f);
                numlen += (vlen[i] = std::strlen(&numbuf[numlen]));
                break;
            }
            default:
            {
                vlen[i] = 0;
                break;
            }
        }
    }
overflow:
    if(space)
    {
        len += std::max(prefix ? i : i-1, 0);
    }
    char *buf = newstring(len + numlen);
    int offset = 0,
        numoffset = 0;
    if(prefix)
    {
        memcpy(buf, prefix, prefixlen);
        offset += prefixlen;
        if(space && i)
        {
            buf[offset++] = ' ';
        }
    }
    for(int j = 0; j < i; ++j)
    {
        if(v[j].type == Value_Integer || v[j].type == Value_Float)
        {
            memcpy(&buf[offset], &numbuf[numoffset], vlen[j]);
            numoffset += vlen[j];
        }
        else if(vlen[j])
        {
            memcpy(&buf[offset], v[j].s, vlen[j]);
        }
        offset += vlen[j];
        if(j==i-1)
        {
            break;
        }
        if(space)
        {
            buf[offset++] = ' ';
        }
    }
    buf[offset] = '\0';
    if(i < n)
    {
        char *morebuf = conc(&v[i], n-i, space, buf, offset);
        delete[] buf;
        return morebuf;
    }
    return buf;
}

char *conc(tagval *v, int n, bool space)
{
    return conc(v, n, space, nullptr, 0);
}

char *conc(tagval *v, int n, bool space, const char *prefix)
{
    return conc(v, n, space, prefix, std::strlen(prefix));
}

//ignore double slashes in cubescript lines
static void skipcomments(const char *&p)
{
    for(;;)
    {
        p += std::strspn(p, " \t\r");
        if(p[0]!='/' || p[1]!='/')
        {
            break;
        }
        p += std::strcspn(p, "\n\0");
    }
}

static void cutstring(const char *&p, stringslice &s)
{
    p++;
    const char *end = parsestring(p);
    int maxlen = static_cast<int>(end-p) + 1;

    stridx = (stridx + 1)%4;
    vector<char> &buf = strbuf[stridx];
    if(buf.capacity() < maxlen)
    {
        buf.growbuf(maxlen);
    }
    s.str = buf.getbuf();
    s.len = unescapestring(buf.getbuf(), p, end);
    p = end;
    if(*p=='\"')
    {
        p++;
    }
}

static char *cutstring(const char *&p)
{
    p++;
    const char *end = parsestring(p);
    char *buf = newstring(end-p);
    unescapestring(buf, p, end);
    p = end;
    if(*p=='\"')
    {
        p++;
    }
    return buf;
}

const char *parseword(const char *p)
{
    constexpr int maxbrak = 100;
    static char brakstack[maxbrak];
    int brakdepth = 0;
    for(;; p++)
    {
        p += std::strcspn(p, "\"/;()[] \t\r\n\0");
        switch(p[0])
        {
            case '"':
            case ';':
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '\0':
            {
                return p;
            }
            case '/':
            {
                if(p[1] == '/')
                {
                    return p;
                }
                break;
            }
            //change depth of bracket stack upon seeing a (, [ char
            case '[':
            case '(':
            {
                if(brakdepth >= maxbrak)
                {
                    return p;
                }
                brakstack[brakdepth++] = p[0];
                break;
            }
            case ']':
            {
                if(brakdepth <= 0 || brakstack[--brakdepth] != '[')
                {
                    return p;
                }
                break;
            }
                //parens get treated seperately
            case ')':
            {
                if(brakdepth <= 0 || brakstack[--brakdepth] != '(')
                {
                    return p;
                }
                break;
            }
        }
    }
    return p;
}

static void cutword(const char *&p, stringslice &s)
{
    s.str = p;
    p = parseword(p);
    s.len = static_cast<int>(p-s.str);
}

static char *cutword(const char *&p)
{
    const char *word = p;
    p = parseword(p);
    return p!=word ? newstring(word, p-word) : nullptr;
}

#define RET_CODE(type, defaultret) ((type) >= Value_Any ? ((type) == Value_CString ? Ret_String : (defaultret)) : (type) << Code_Ret)
#define RET_CODE_INT(type) RET_CODE(type, Ret_Integer)
#define RET_CODE_FLOAT(type) RET_CODE(type, Ret_Float)
#define RET_CODE_ANY(type) RET_CODE(type, 0)
#define RET_CODE_STRING(type) ((type) >= Value_Any ? Ret_String : (type) << Code_Ret)

static void compilestr(vector<uint> &code, const char *word, int len, bool macro = false)
{
    if(len <= 3 && !macro)
    {
        uint op = Code_ValI|Ret_String;
        for(int i = 0; i < len; ++i)
        {
            op |= static_cast<uint>(static_cast<uchar>(word[i]))<<((i+1)*8);
        }
        code.add(op);
        return;
    }
    code.add((macro ? Code_Macro : Code_Val|Ret_String)|(len<<8));
    code.put(reinterpret_cast<const uint *>(word), len/sizeof(uint));
    size_t endlen = len%sizeof(uint);
    union
    {
        char c[sizeof(uint)];
        uint u;
    } end;
    end.u = 0;
    memcpy(end.c, word + len - endlen, endlen);
    code.add(end.u);
}

static void compilestr(vector<uint> &code)
{
    code.add(Code_ValI|Ret_String);
}

static void compilestr(vector<uint> &code, const stringslice &word, bool macro = false)
{
    compilestr(code, word.str, word.len, macro);
}

//compile un-escape string
static void compileunescapestring(vector<uint> &code, const char *&p, bool macro = false)
{
    p++;
    const char *end = parsestring(p);
    code.add(macro ? Code_Macro : Code_Val|Ret_String);
    char *buf = reinterpret_cast<char *>(code.reserve(static_cast<int>(end-p)/sizeof(uint) + 1).buf);
    int len = unescapestring(buf, p, end);
    memset(&buf[len], 0, sizeof(uint) - len%sizeof(uint));
    code.last() |= len<<8;
    code.advance(len/sizeof(uint) + 1);
    p = end;
    if(*p == '\"')
    {
        p++;
    }
}

static void compileint(vector<uint> &code, int i = 0)
{
    if(i >= -0x800000 && i <= 0x7FFFFF)
    {
        code.add(Code_ValI|Ret_Integer|(i<<8));
    }
    else
    {
        code.add(Code_Val|Ret_Integer);
        code.add(i);
    }
}

static void compilenull(vector<uint> &code)
{
    code.add(Code_ValI|Ret_Null);
}

static uint emptyblock[Value_Any][2] =
{
    { Code_Start + 0x100, Code_Exit|Ret_Null },
    { Code_Start + 0x100, Code_Exit|Ret_Integer },
    { Code_Start + 0x100, Code_Exit|Ret_Float },
    { Code_Start + 0x100, Code_Exit|Ret_String }
};

static void compileblock(vector<uint> &code)
{
    code.add(Code_Empty);
}

static void compilestatements(vector<uint> &code, const char *&p, int rettype, int brak = '\0', int prevargs = 0);

static const char *compileblock(vector<uint> &code, const char *p, int rettype = Ret_Null, int brak = '\0')
{
    int start = code.length();
    code.add(Code_Block);
    code.add(Code_Offset|((start+2)<<8));
    if(p)
    {
        compilestatements(code, p, Value_Any, brak);
    }
    if(code.length() > start + 2)
    {
        code.add(Code_Exit|rettype);
        code[start] |= static_cast<uint>(code.length() - (start + 1))<<8;
    }
    else
    {
        code.setsize(start);
        code.add(Code_Empty|rettype);
    }
    return p;
}

static void compileident(vector<uint> &code, ident *id = dummyident)
{
    code.add((id->index < Max_Args ? Code_IdentArg : Code_Ident)|(id->index<<8));
}

static void compileident(vector<uint> &code, const stringslice &word)
{
    compileident(code, newident(word, Idf_Unknown));
}

static void compileint(vector<uint> &code, const stringslice &word)
{
    compileint(code, word.len ? parseint(word.str) : 0);
}

static void compilefloat(vector<uint> &code, float f = 0.0f)
{
    if(static_cast<int>(f) == f && f >= -0x800000 && f <= 0x7FFFFF)
    {
        code.add(Code_ValI|Ret_Float|(static_cast<int>(f)<<8));
    }
    else
    {
        union
        {
            float f;
            uint u;
        } conv;
        conv.f = f;
        code.add(Code_Val|Ret_Float);
        code.add(conv.u);
    }
}

static void compilefloat(vector<uint> &code, const stringslice &word)
{
    compilefloat(code, word.len ? parsefloat(word.str) : 0.0f);
}

static bool getbool(const char *s)
{
    switch(s[0])
    {
        case '+':
        case '-':
            switch(s[1])
            {
                case '0':
                {
                    break;
                }
                case '.':
                {
                    return !isdigit(s[2]) || parsefloat(s) != 0;
                }
                default:
                {
                    return true;
                }
            }
            [[fallthrough]];
        case '0':
        {
            char *end;
            int val = static_cast<int>(strtoul(const_cast<char *>(s), &end, 0));
            if(val)
            {
                return true;
            }
            switch(*end)
            {
                case 'e':
                case '.':
                {
                    return parsefloat(s) != 0;
                }
                default:
                {
                    return false;
                }
            }
        }
        case '.':
        {
            return !isdigit(s[1]) || parsefloat(s) != 0;
        }
        case '\0':
        {
            return false;
        }
        default:
        {
            return true;
        }
    }
}

bool getbool(const tagval &v)
{
    switch(v.type)
    {
        case Value_Float:
        {
            return v.f!=0;
        }
        case Value_Integer:
        {
            return v.i!=0;
        }
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            return getbool(v.s);
        }
        default:
        {
            return false;
        }
    }
}

static void compileval(vector<uint> &code, int wordtype, const stringslice &word = stringslice(nullptr, 0))
{
    switch(wordtype)
    {
        case Value_CAny:
        {
            if(word.len)
            {
                compilestr(code, word, true);
            }
            else
            {
                compilenull(code);
            }
            break;
        }
        case Value_CString:
        {
            compilestr(code, word, true);
            break;
        }
        case Value_Any:
        {
            if(word.len)
            {
                compilestr(code, word);
            }
            else
            {
                compilenull(code);
            }
            break;
        }
        case Value_String:
        {
            compilestr(code, word);
            break;
        }
        case Value_Float:
        {
            compilefloat(code, word);
            break;
        }
        case Value_Integer:
        {
            compileint(code, word);
            break;
        }
        case Value_Cond:
        {
            if(word.len)
            {
                compileblock(code, word.str);
            }
            else
            {
                compilenull(code);
            }
            break;
        }
        case Value_Code:
        {
            compileblock(code, word.str);
            break;
        }
        case Value_Ident:
        {
            compileident(code, word);
            break;
        }
        default:
        {
            break;
        }
    }
}

static stringslice unusedword(nullptr, 0);
static bool compilearg(vector<uint> &code, const char *&p, int wordtype, int prevargs = Max_Results, stringslice &word = unusedword);

static void compilelookup(vector<uint> &code, const char *&p, int ltype, int prevargs = Max_Results)
{
    stringslice lookup;
    switch(*++p)
    {
        case '(':
        case '[':
        {
            if(!compilearg(code, p, Value_CString, prevargs))
            {
                goto invalid;
            }
            break;
        }
        case '$':
        {
            compilelookup(code, p, Value_CString, prevargs);
            break;
        }
        case '\"':
        {
            cutstring(p, lookup);
            goto lookupid; //immediately below, part of default case
        }
        default:
        {
            cutword(p, lookup);
            if(!lookup.len)
            {
                goto invalid; //invalid is near bottom of fxn
            }
        lookupid:
            ident *id = newident(lookup, Idf_Unknown);
            if(id)
            {
                switch(id->type)
                {
                    case Id_Var:
                    {
                        code.add(Code_IntVar|RET_CODE_INT(ltype)|(id->index<<8));
                        switch(ltype)
                        {
                            case Value_Pop:
                            {
                                code.pop();
                                break;
                            }
                            case Value_Code:
                            {
                                code.add(Code_Compile);
                                break;
                            }
                            case Value_Ident:
                            {
                                code.add(Code_IdentU);
                                break;
                            }
                        }
                        return;
                    }
                    case Id_FloatVar:
                    {
                        code.add(Code_FloatVar|RET_CODE_FLOAT(ltype)|(id->index<<8));
                        switch(ltype)
                        {
                            case Value_Pop:
                            {
                                code.pop();
                                break;
                            }
                            case Value_Code:
                            {
                                code.add(Code_Compile);
                                break;
                            }
                            case Value_Ident:
                            {
                                code.add(Code_IdentU);
                                break;
                            }
                        }
                        return;
                    }
                    case Id_StringVar:
                    {
                        switch(ltype)
                        {
                            case Value_Pop:
                            {
                                return;
                            }
                            case Value_CAny:
                            case Value_CString:
                            case Value_Code:
                            case Value_Ident:
                            case Value_Cond:
                            {
                                code.add(Code_StrVarM|(id->index<<8));
                                break;
                            }
                            default:
                            {
                                code.add(Code_StrVar|RET_CODE_STRING(ltype)|(id->index<<8));
                                break;
                            }
                        }
                        goto done;
                    }
                    case Id_Alias:
                    {
                        switch(ltype)
                        {
                            case Value_Pop:
                            {
                                return;
                            }
                            case Value_CAny:
                            case Value_Cond:
                            {
                                code.add((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|(id->index<<8));
                                break;
                            }
                            case Value_CString:
                            case Value_Code:
                            case Value_Ident:
                            {
                                code.add((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|Ret_String|(id->index<<8));
                                break;
                            }
                            default:
                            {
                                code.add((id->index < Max_Args ? Code_LookupArg : Code_Lookup)|RET_CODE_STRING(ltype)|(id->index<<8));
                                break;
                            }
                        }
                        goto done;
                    }
                    case Id_Command:
                    {
                        int comtype = Code_Com,
                            numargs = 0;
                        if(prevargs >= Max_Results)
                        {
                            code.add(Code_Enter);
                        }
                        for(const char *fmt = id->args; *fmt; fmt++)
                        {
                            switch(*fmt)
                            {
                                case 'S':
                                {
                                    compilestr(code);
                                    numargs++;
                                    break;
                                }
                                case 's':
                                {
                                    compilestr(code, nullptr, 0, true);
                                    numargs++;
                                    break;
                                }
                                case 'i':
                                {
                                    compileint(code);
                                    numargs++;
                                    break;
                                }
                                case 'b':
                                {
                                    compileint(code, INT_MIN);
                                    numargs++;
                                    break;
                                }
                                case 'f':
                                {
                                    compilefloat(code);
                                    numargs++;
                                    break;
                                }
                                case 'F':
                                {
                                    code.add(Code_Dup|Ret_Float);
                                    numargs++;
                                    break;
                                }
                                case 'E':
                                case 'T':
                                case 't':
                                {
                                    compilenull(code);
                                    numargs++;
                                    break;
                                }
                                case 'e':
                                {
                                    compileblock(code);
                                    numargs++;
                                    break;
                                }
                                case 'r':
                                {
                                    compileident(code);
                                    numargs++;
                                    break;
                                }
                                case '$':
                                {
                                    compileident(code, id);
                                    numargs++;
                                    break;
                                }
                                case 'N':
                                {
                                    compileint(code, -1);
                                    numargs++;
                                    break;
                                }
                                case 'D':
                                {
                                    comtype = Code_ComD;
                                    numargs++;
                                    break;
                                }
                                case 'C':
                                {
                                    comtype = Code_ComC;
                                    goto compilecomv; //compilecomv beneath this switch statement
                                }
                                case 'V':
                                {
                                    comtype = Code_ComV;
                                    goto compilecomv;
                                }
                                case '1':
                                case '2':
                                case '3':
                                case '4':
                                {
                                    break;
                                }
                            }
                        }
                        code.add(comtype|RET_CODE_ANY(ltype)|(id->index<<8));
                        code.add((prevargs >= Max_Results ? Code_Exit : Code_ResultArg) | RET_CODE_ANY(ltype));
                        goto done;
                    compilecomv:
                        code.add(comtype|RET_CODE_ANY(ltype)|(numargs<<8)|(id->index<<13));
                        code.add((prevargs >= Max_Results ? Code_Exit : Code_ResultArg) | RET_CODE_ANY(ltype));
                        goto done;
                    }
                    default:
                    {
                        goto invalid;
                    }
                }
            compilestr(code, lookup, true);
            break;
            }
        }
    }
    switch(ltype)
    {
        case Value_CAny:
        case Value_Cond:
        {
            code.add(Code_LookupMU);
            break;
        }
        case Value_CString:
        case Value_Code:
        case Value_Ident:
        {
            code.add(Code_LookupMU|Ret_String);
            break;
        }
        default:
        {
            code.add(Code_LookupU|RET_CODE_ANY(ltype));
            break;
        }
    }
done:
    switch(ltype)
    {
        case Value_Pop:
        {
            code.add(Code_Pop);
            break;
        }
        case Value_Code:
        {
            code.add(Code_Compile);
            break;
        }
        case Value_Cond:
        {
            code.add(Code_Cond);
            break;
        }
        case Value_Ident:
        {
            code.add(Code_IdentU);
            break;
        }
    }
    return;
invalid:
    switch(ltype)
    {
        case Value_Pop:
        {
            break;
        }
        case Value_Null:
        case Value_Any:
        case Value_CAny:
        case Value_Word:
        case Value_Cond:
        {
            compilenull(code);
            break;
        }
        default:
        {
            compileval(code, ltype);
            break;
        }
    }
}

static bool compileblockstr(vector<uint> &code, const char *str, const char *end, bool macro)
{
    int start = code.length();
    code.add(macro ? Code_Macro : Code_Val|Ret_String);
    char *buf = reinterpret_cast<char *>(code.reserve((end-str)/sizeof(uint)+1).buf);
    int len = 0;
    while(str < end)
    {
        int n = std::strcspn(str, "\r/\"@]\0");
        memcpy(&buf[len], str, n);
        len += n;
        str += n;
        switch(*str)
        {
            case '\r':
            {
                str++;
                break;
            }
            case '\"':
            {
                const char *start = str;
                str = parsestring(str+1);
                if(*str=='\"')
                {
                    str++;
                }
                memcpy(&buf[len], start, str-start);
                len += str-start;
                break;
            }
            case '/':
                if(str[1] == '/')
                {
                    size_t comment = std::strcspn(str, "\n\0");
                    if (iscubepunct(str[2]))
                    {
                        memcpy(&buf[len], str, comment);
                        len += comment;
                    }
                    str += comment;
                }
                else
                {
                    buf[len++] = *str++;
                }
                break;
            case '@':
            case ']':
                if(str < end)
                {
                    buf[len++] = *str++;
                    break;
                }
            case '\0':
            {
                goto done;
            }
        }
    }
done:
    memset(&buf[len], '\0', sizeof(uint)-len%sizeof(uint));
    code.advance(len/sizeof(uint)+1);
    code[start] |= len<<8;
    return true;
}

static bool compileblocksub(vector<uint> &code, const char *&p, int prevargs)
{
    stringslice lookup;
    switch(*p)
    {
        case '(':
        {
            if(!compilearg(code, p, Value_CAny, prevargs))
            {
                return false;
            }
            break;
        }
        case '[':
        {
            if(!compilearg(code, p, Value_CString, prevargs))
            {
                return false;
            }
            code.add(Code_LookupMU);
            break;
        }
        case '\"':
        {
            cutstring(p, lookup);
            goto lookupid;
        }
        default:
        {

            lookup.str = p;
            while(iscubealnum(*p) || *p=='_')
            {
                p++;
            }
            lookup.len = static_cast<int>(p-lookup.str);
            if(!lookup.len)
            {
                return false;
            }
        lookupid:
            ident *id = newident(lookup, Idf_Unknown);
            if(id)
            {
                switch(id->type)
                {
                    case Id_Var:
                    {
                        code.add(Code_IntVar|(id->index<<8));
                        goto done;
                    }
                    case Id_FloatVar:
                    {
                        code.add(Code_FloatVar|(id->index<<8));
                        goto done;
                    }
                    case Id_StringVar:
                    {
                        code.add(Code_StrVarM|(id->index<<8));
                        goto done;
                    }
                    case Id_Alias:
                    {
                        code.add((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|(id->index<<8));
                        goto done;
                    }
                }
            }
            compilestr(code, lookup, true);
            code.add(Code_LookupMU);
        done:
            break;
        }
    }
    return true;
}

static void compileblockmain(vector<uint> &code, const char *&p, int wordtype, int prevargs)
{
    const char *line  = p,
               *start = p;
    int concs = 0;
    for(int brak = 1; brak;)
    {
        p += std::strcspn(p, "@\"/[]\0");
        int c = *p++;
        switch(c)
        {
            case '\0':
            {
                debugcodeline(line, "missing \"]\"");
                p--;
                goto done;
            }
            case '\"':
            {
                p = parsestring(p);
                if(*p=='\"')
                {
                    p++;
                }
                break;
            }
            case '/':
                if(*p=='/')
                {
                    p += std::strcspn(p, "\n\0");
                }
                break;
            case '[':
            {
                brak++;
                break;
            }
            case ']':
            {
                brak--;
                break;
            }
            case '@':
            {
                const char *esc = p;
                while(*p == '@')
                {
                    p++;
                }
                int level = p - (esc - 1);
                if(brak > level)
                {
                    continue;
                }
                else if(brak < level)
                {
                    debugcodeline(line, "too many @s");
                }
                if(!concs && prevargs >= Max_Results)
                {
                    code.add(Code_Enter);
                }
                if(concs + 2 > Max_Args)
                {
                    code.add(Code_ConCW|Ret_String|(concs<<8));
                    concs = 1;
                }
                if(compileblockstr(code, start, esc-1, true))
                {
                    concs++;
                }
                if(compileblocksub(code, p, prevargs + concs))
                {
                    concs++;
                }
                if(concs)
                {
                    start = p;
                }
                else if(prevargs >= Max_Results)
                {
                    code.pop();
                }
                break;
            }
        }
    }
done:
    if(p-1 > start)
    {
        if(!concs)
        {
            switch(wordtype)
            {
                case Value_Pop:
                {
                    return;
                }
                case Value_Code:
                case Value_Cond:
                {
                    p = compileblock(code, start, Ret_Null, ']');
                    return;
                }
                case Value_Ident:
                {
                    compileident(code, stringslice(start, p-1));
                    return;
                }
            }
        }
        switch(wordtype)
        {
            case Value_CString:
            case Value_Code:
            case Value_Ident:
            case Value_CAny:
            case Value_Cond:
            {
                compileblockstr(code, start, p-1, true);
                break;
            }
            default:
            {
                compileblockstr(code, start, p-1, concs > 0);
                break;
            }
        }
        if(concs > 1)
        {
            concs++;
        }
    }
    if(concs)
    {
        if(prevargs >= Max_Results)
        {
            code.add(Code_ConCM|RET_CODE_ANY(wordtype)|(concs<<8));
            code.add(Code_Exit|RET_CODE_ANY(wordtype));
        }
        else
        {
            code.add(Code_ConCW|RET_CODE_ANY(wordtype)|(concs<<8));
        }
    }
    switch(wordtype)
    {
        case Value_Pop:
        {
            if(concs || p-1 > start)
            {
                code.add(Code_Pop);
            }
            break;
        }
        case Value_Cond:
        {
            if(!concs && p-1 <= start)
            {
                compilenull(code);
            }
            else
            {
                code.add(Code_Cond);
            }
            break;
        }
        case Value_Code:
        {
            if(!concs && p-1 <= start)
            {
                compileblock(code);
            }
            else
            {
                code.add(Code_Compile);
            }
            break;
        }
        case Value_Ident:
        {
            if(!concs && p-1 <= start)
            {
                compileident(code);
            }
            else
            {
                code.add(Code_IdentU);
            }
            break;
        }
        case Value_CString:
        case Value_CAny:
        {
            if(!concs && p-1 <= start)
            {
                compilestr(code, nullptr, 0, true);
            }
            break;
        }
        case Value_String:
        case Value_Null:
        case Value_Any:
        case Value_Word:
        {
            if(!concs && p-1 <= start)
            {
                compilestr(code);
            }
            break;
        }
        default:
        {
            if(!concs)
            {
                if(p-1 <= start)
                {
                    compileval(code, wordtype);
                }
                else
                {
                    code.add(Code_Force|(wordtype<<Code_Ret));
                }
            }
            break;
        }
    }
}

static bool compilearg(vector<uint> &code, const char *&p, int wordtype, int prevargs, stringslice &word)
{
    skipcomments(p);
    switch(*p)
    {
        //cases for special chars: \[]()$
        case '\"':
        {
            switch(wordtype)
            {
                case Value_Pop:
                {
                    p = parsestring(p+1);
                    if(*p == '\"')
                    {
                        p++;
                    }
                    break;
                }
                case Value_Cond:
                {
                    char *s = cutstring(p);
                    if(s[0])
                    {
                        compileblock(code, s);
                    }
                    else
                    {
                        compilenull(code);
                    }
                    delete[] s;
                    break;
                }
                case Value_Code:
                {
                    char *s = cutstring(p);
                    compileblock(code, s);
                    delete[] s;
                    break;
                }
                case Value_Word:
                {
                    cutstring(p, word);
                    break;
                }
                case Value_Any:
                case Value_String:
                {
                    compileunescapestring(code, p);
                    break;
                }
                case Value_CAny:
                case Value_CString:
                {
                    compileunescapestring(code, p, true);
                    break;
                }
                default:
                {
                    stringslice s;
                    cutstring(p, s);
                    compileval(code, wordtype, s);
                    break;
                }
            }
            return true;
        }
        case '$':
        {
            compilelookup(code, p, wordtype, prevargs);
            return true;
        }
        case '(':
            p++;
            if(prevargs >= Max_Results)
            {
                code.add(Code_Enter);
                compilestatements(code, p, wordtype > Value_Any ? Value_CAny : Value_Any, ')');
                code.add(Code_Exit|RET_CODE_ANY(wordtype));
            }
            else
            {
                int start = code.length();
                compilestatements(code, p, wordtype > Value_Any ? Value_CAny : Value_Any, ')', prevargs);
                if(code.length() > start)
                {
                    code.add(Code_ResultArg|RET_CODE_ANY(wordtype));
                }
                else
                {
                    compileval(code, wordtype);
                    return true;
                }
            }
            switch(wordtype)
            {
                case Value_Pop:
                {
                    code.add(Code_Pop);
                    break;
                }
                case Value_Cond:
                {
                    code.add(Code_Cond);
                    break;
                }
                case Value_Code:
                {
                    code.add(Code_Compile);
                    break;
                }
                case Value_Ident:
                {
                    code.add(Code_IdentU);
                    break;
                }
            }
            return true;
        case '[':
        {
            p++;
            compileblockmain(code, p, wordtype, prevargs);
            return true;
        }
        //search for aliases
        default:
            switch(wordtype)
            {
                case Value_Pop:
                {
                    const char *s = p;
                    p = parseword(p);
                    return p != s;
                }
                case Value_Cond:
                {
                    char *s = cutword(p);
                    if(!s)
                    {
                        return false;
                    }
                    compileblock(code, s);
                    delete[] s;
                    return true;
                }
                case Value_Code:
                {
                    char *s = cutword(p);
                    if(!s)
                    {
                        return false;
                    }
                    compileblock(code, s);
                    delete[] s;
                    return true;
                }
                case Value_Word:
                    cutword(p, word);
                    return word.len!=0;
                default:
                {
                    stringslice s;
                    cutword(p, s);
                    if(!s.len)
                    {
                        return false;
                    }
                    compileval(code, wordtype, s);
                    return true;
                }
            }
    }
}

static void compilestatements(vector<uint> &code, const char *&p, int rettype, int brak, int prevargs)
{
    const char *line = p;
    stringslice idname;
    int numargs;
    for(;;)
    {
        skipcomments(p);
        idname.str = nullptr;
        bool more = compilearg(code, p, Value_Word, prevargs, idname);
        if(!more)
        {
            goto endstatement;
        }
        skipcomments(p);
        if(p[0] == '=')
        {
            switch(p[1])
            {
                case '/':
                {
                    if(p[2] != '/')
                    {
                        break;
                    }
                }
                [[fallthrough]];
                case ';':
                case ' ':
                case '\t':
                case '\r':
                case '\n':
                case '\0':
                    p++;
                    if(idname.str)
                    {
                        ident *id = newident(idname, Idf_Unknown);
                        if(id)
                        {
                            switch(id->type)
                            {
                                case Id_Alias:
                                {
                                    if(!(more = compilearg(code, p, Value_Any, prevargs)))
                                    {
                                        compilestr(code);
                                    }
                                    code.add((id->index < Max_Args ? Code_AliasArg : Code_Alias)|(id->index<<8));
                                    goto endstatement;
                                }
                                case Id_Var:
                                {
                                    if(!(more = compilearg(code, p, Value_Integer, prevargs)))
                                    {
                                        compileint(code);
                                    }
                                    code.add(Code_IntVar1|(id->index<<8));
                                    goto endstatement;
                                }
                                case Id_FloatVar:
                                {
                                    if(!(more = compilearg(code, p, Value_Float, prevargs)))
                                    {
                                        compilefloat(code);
                                    }
                                    code.add(Code_FloatVar1|(id->index<<8));
                                    goto endstatement;
                                }
                                case Id_StringVar:
                                {
                                    if(!(more = compilearg(code, p, Value_CString, prevargs)))
                                    {
                                        compilestr(code);
                                    }
                                    code.add(Code_StrVar1|(id->index<<8));
                                    goto endstatement;
                                }
                            }
                        }
                        compilestr(code, idname, true);
                    }
                    if(!(more = compilearg(code, p, Value_Any)))
                    {
                        compilestr(code);
                    }
                    code.add(Code_AliasU);
                    goto endstatement;
            }
        }
        numargs = 0;
        if(!idname.str)
        {
        noid:
            while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs)))
            {
                numargs++;
            }
            code.add(Code_CallU|(numargs<<8));
        }
        else
        {
            ident *id = idents.access(idname);
            if(!id)
            {
                if(!checknumber(idname))
                {
                    compilestr(code, idname, true);
                    goto noid;
                }
                switch(rettype)
                {
                case Value_Any:
                case Value_CAny:
                {
                    char *end = const_cast<char *>(idname.str);
                    int val = static_cast<int>(strtoul(idname.str, &end, 0));
                    if(end < idname.end())
                    {
                        compilestr(code, idname, rettype==Value_CAny);
                    }
                    else
                    {
                        compileint(code, val);
                    }
                    break;
                }
                default:
                    compileval(code, rettype, idname);
                    break;
                }
                code.add(Code_Result);
            }
            else
            {
                switch(id->type)
                {
                    case Id_Alias:
                    {
                        while(numargs < Max_Args && (more = compilearg(code, p, Value_Any, prevargs+numargs)))
                        {
                            numargs++;
                        }
                        code.add((id->index < Max_Args ? Code_CallArg : Code_Call)|(numargs<<8)|(id->index<<13));
                        break;
                    }
                    case Id_Command:
                    {
                        int comtype = Code_Com,
                            fakeargs = 0;
                        bool rep = false;
                        for(const char *fmt = id->args; *fmt; fmt++)
                        {
                            switch(*fmt)
                            {
                                case 'S':
                                case 's':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, *fmt == 's' ? Value_CString : Value_String, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        compilestr(code, nullptr, 0, *fmt=='s');
                                        fakeargs++;
                                    }
                                    else if(!fmt[1])
                                    {
                                        int numconc = 1;
                                        while(numargs + numconc < Max_Args && (more = compilearg(code, p, Value_CString, prevargs+numargs+numconc)))
                                        {
                                            numconc++;
                                        }
                                        if(numconc > 1)
                                        {
                                            code.add(Code_ConC|Ret_String|(numconc<<8));
                                        }
                                    }
                                    numargs++;
                                    break;
                                }
                                case 'i':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, Value_Integer, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        compileint(code);
                                        fakeargs++;
                                    }
                                    numargs++;
                                    break;
                                }
                                case 'b':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, Value_Integer, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        compileint(code, INT_MIN);
                                        fakeargs++;
                                    }
                                    numargs++;
                                    break;
                                }
                                case 'f':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, Value_Float, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        compilefloat(code);
                                        fakeargs++;
                                    }
                                    numargs++;
                                    break;
                                }
                                case 'F':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, Value_Float, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        code.add(Code_Dup|Ret_Float);
                                        fakeargs++;
                                    }
                                    numargs++;
                                    break;
                                }
                                case 'T':
                                case 't':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, *fmt == 't' ? Value_CAny : Value_Any, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        compilenull(code);
                                        fakeargs++;
                                    }
                                    numargs++;
                                    break;
                                }
                                case 'E':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, Value_Cond, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        compilenull(code);
                                        fakeargs++;
                                    }
                                    numargs++;
                                    break;
                                }
                                case 'e':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, Value_Code, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        compileblock(code);
                                        fakeargs++;
                                    }
                                    numargs++;
                                    break;
                                }
                                case 'r':
                                {
                                    if(more)
                                    {
                                        more = compilearg(code, p, Value_Ident, prevargs+numargs);
                                    }
                                    if(!more)
                                    {
                                        if(rep)
                                        {
                                            break;
                                        }
                                        compileident(code);
                                        fakeargs++;
                                    }
                                    numargs++;
                                    break;
                                }
                                case '$':
                                {
                                    compileident(code, id);
                                    numargs++;
                                    break;
                                }
                                case 'N':
                                {
                                    compileint(code, numargs-fakeargs);
                                    numargs++;
                                    break;
                                }
                                case 'D':
                                {
                                    comtype = Code_ComD;
                                    numargs++;
                                    break;
                                }
                                case 'C':
                                {
                                    comtype = Code_ComC;
                                    if(more)
                                    {
                                        while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs)))
                                        {
                                            numargs++;
                                        }
                                    }
                                    goto compilecomv;
                                }
                                case 'V':
                                {
                                    comtype = Code_ComV;
                                    if(more)
                                    {
                                        while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs)))
                                        {
                                            numargs++;
                                        }
                                    }
                                    goto compilecomv;
                                }
                                case '1':
                                case '2':
                                case '3':
                                case '4':
                                {
                                    if(more && numargs < Max_Args)
                                    {
                                        int numrep = *fmt-'0'+1;
                                        fmt -= numrep;
                                        rep = true;
                                    }
                                    else
                                    {
                                        for(; numargs > Max_Args; numargs--)
                                        {
                                            code.add(Code_Pop);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                        code.add(comtype|RET_CODE_ANY(rettype)|(id->index<<8));
                        break;
                    compilecomv:
                        code.add(comtype|RET_CODE_ANY(rettype)|(numargs<<8)|(id->index<<13));
                        break;
                    }
                    case Id_Local:
                    {
                        if(more)
                        {
                            while(numargs < Max_Args && (more = compilearg(code, p, Value_Ident, prevargs+numargs)))
                            {
                                numargs++;
                            }
                        }
                        if(more)
                        {
                            while((more = compilearg(code, p, Value_Pop)))
                            {
                                //(empty body)
                            }
                        }
                        code.add(Code_Local|(numargs<<8));
                        break;
                    }
                    case Id_Do:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_Code, prevargs);
                        }
                        code.add((more ? Code_Do : Code_Null) | RET_CODE_ANY(rettype));
                        break;
                    }
                    case Id_DoArgs:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_Code, prevargs);
                        }
                        code.add((more ? Code_DoArgs : Code_Null) | RET_CODE_ANY(rettype));
                        break;
                    }
                    case Id_If:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_CAny, prevargs);
                        }
                        if(!more) //more can be affected by above assignment
                        {
                            code.add(Code_Null | RET_CODE_ANY(rettype));
                        }
                        else
                        {
                            int start1 = code.length();
                            more = compilearg(code, p, Value_Code, prevargs+1);
                            if(!more)
                            {
                                code.add(Code_Pop);
                                code.add(Code_Null | RET_CODE_ANY(rettype));
                            }
                            else
                            {
                                int start2 = code.length();
                                more = compilearg(code, p, Value_Code, prevargs+2);
                                uint inst1 = code[start1], op1 = inst1&~Code_RetMask, len1 = start2 - (start1+1);
                                if(!more)
                                {
                                    if(op1 == (Code_Block|(len1<<8)))
                                    {
                                        code[start1] = (len1<<8) | Code_JumpFalse;
                                        code[start1+1] = Code_EnterResult;
                                        code[start1+len1] = (code[start1+len1]&~Code_RetMask) | RET_CODE_ANY(rettype);
                                        break;
                                    }
                                    compileblock(code);
                                }
                                else
                                {
                                    uint inst2 = code[start2], op2 = inst2&~Code_RetMask, len2 = code.length() - (start2+1);
                                    if(op2 == (Code_Block|(len2<<8)))
                                    {
                                        if(op1 == (Code_Block|(len1<<8)))
                                        {
                                            code[start1] = ((start2-start1)<<8) | Code_JumpFalse;
                                            code[start1+1] = Code_EnterResult;
                                            code[start1+len1] = (code[start1+len1]&~Code_RetMask) | RET_CODE_ANY(rettype);
                                            code[start2] = (len2<<8) | Code_Jump;
                                            code[start2+1] = Code_EnterResult;
                                            code[start2+len2] = (code[start2+len2]&~Code_RetMask) | RET_CODE_ANY(rettype);
                                            break;
                                        }
                                        else if(op1 == (Code_Empty|(len1<<8)))
                                        {
                                            code[start1] = Code_Null | (inst2&Code_RetMask);
                                            code[start2] = (len2<<8) | Code_JumpTrue;
                                            code[start2+1] = Code_EnterResult;
                                            code[start2+len2] = (code[start2+len2]&~Code_RetMask) | RET_CODE_ANY(rettype);
                                            break;
                                        }
                                    }
                                }
                                code.add(Code_Com|RET_CODE_ANY(rettype)|(id->index<<8));
                            }
                        }
                        break;
                    }
                    case Id_Result:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_Any, prevargs);
                        }
                        code.add((more ? Code_Result : Code_Null) | RET_CODE_ANY(rettype));
                        break;
                    }
                    case Id_Not:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_CAny, prevargs);
                        }
                        code.add((more ? Code_Not : Code_True) | RET_CODE_ANY(rettype));
                        break;
                    }
                    case Id_And:
                    case Id_Or:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_Cond, prevargs);
                        }
                        if(!more) //more can be affected by above assignment
                        {
                            code.add((id->type == Id_And ? Code_True : Code_False) | RET_CODE_ANY(rettype));
                        }
                        else
                        {
                            numargs++;
                            int start = code.length(),
                                end = start;
                            while(numargs < Max_Args)
                            {
                                more = compilearg(code, p, Value_Cond, prevargs+numargs);
                                if(!more)
                                {
                                    break;
                                }
                                numargs++;
                                if((code[end]&~Code_RetMask) != (Code_Block|(static_cast<uint>(code.length()-(end+1))<<8)))
                                {
                                    break;
                                }
                                end = code.length();
                            }
                            if(more)
                            {
                                while(numargs < Max_Args && (more = compilearg(code, p, Value_Cond, prevargs+numargs)))
                                {
                                    numargs++;
                                }
                                code.add(Code_ComV|RET_CODE_ANY(rettype)|(numargs<<8)|(id->index<<13));
                            }
                            else
                            {
                                uint op = id->type == Id_And ? Code_JumpResultFalse : Code_JumpResultTrue;
                                code.add(op);
                                end = code.length();
                                while(start+1 < end)
                                {
                                    uint len = code[start]>>8;
                                    code[start] = ((end-(start+1))<<8) | op;
                                    code[start+1] = Code_Enter;
                                    code[start+len] = (code[start+len]&~Code_RetMask) | RET_CODE_ANY(rettype);
                                    start += len+1;
                                }
                            }
                        }
                        break;
                    }
                    case Id_Var:
                    {
                        if(!(more = compilearg(code, p, Value_Integer, prevargs)))
                        {
                            code.add(Code_Print|(id->index<<8));
                        }
                        else if(!(id->flags&Idf_Hex) || !(more = compilearg(code, p, Value_Integer, prevargs+1)))
                        {
                            code.add(Code_IntVar1|(id->index<<8));
                        }
                        else if(!(more = compilearg(code, p, Value_Integer, prevargs+2)))
                        {
                            code.add(Code_IntVar2|(id->index<<8));
                        }
                        else
                        {
                            code.add(Code_IntVar3|(id->index<<8));
                        }
                        break;
                    }
                    case Id_FloatVar:
                    {
                        if(!(more = compilearg(code, p, Value_Float, prevargs)))
                        {
                            code.add(Code_Print|(id->index<<8));
                        }
                        else
                        {
                            code.add(Code_FloatVar1|(id->index<<8));
                        }
                        break;
                    }
                    case Id_StringVar:
                    {
                        if(!(more = compilearg(code, p, Value_CString, prevargs)))
                        {
                            code.add(Code_Print|(id->index<<8));
                        }
                        else
                        {
                            do
                            {
                                ++numargs;
                            } while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs)));
                            if(numargs > 1)
                            {
                                code.add(Code_ConC|Ret_String|(numargs<<8));
                            }
                            code.add(Code_StrVar1|(id->index<<8));
                        }
                        break;
                    }
                }
            }
        }
    endstatement:
        if(more)
        {
            while(compilearg(code, p, Value_Pop))
            {
                //(empty body)
            }
        }
        p += std::strcspn(p, ")];/\n\0");
        int c = *p++;
        switch(c)
        {
            case '\0':
            {
                if(c != brak)
                {
                    debugcodeline(line, "missing \"%c\"", brak);
                }
                p--;
                return;
            }
            case ')':
            case ']':
            {
                if(c == brak)
                {
                    return;
                }
                debugcodeline(line, "unexpected \"%c\"", c);
                break;
            }
            case '/':
            {
                if(*p == '/')
                {
                    p += std::strcspn(p, "\n\0");
                }
                goto endstatement;
            }
        }
    }
}

static void compilemain(vector<uint> &code, const char *p, int rettype = Value_Any)
{
    code.add(Code_Start);
    compilestatements(code, p, Value_Any);
    code.add(Code_Exit|(rettype < Value_Any ? rettype<<Code_Ret : 0));
}

uint *compilecode(const char *p)
{
    vector<uint> buf;
    buf.reserve(64);
    compilemain(buf, p);
    uint *code = new uint[buf.length()];
    memcpy(code, buf.getbuf(), buf.length()*sizeof(uint));
    code[0] += 0x100;
    return code;
}

static const uint *forcecode(tagval &v)
{
    if(v.type != Value_Code)
    {
        vector<uint> buf;
        buf.reserve(64);
        compilemain(buf, v.getstr());
        freearg(v);
        v.setcode(buf.disown()+1);
    }
    return v.code;
}

static void forcecond(tagval &v)
{
    switch(v.type)
    {
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            if(v.s[0])
            {
                forcecode(v);
            }
            else
            {
                v.setint(0);
            }
            break;
        }
    }
}

void freecode(uint *code)
{
    if(!code)
    {
        return;
    }
    switch(*code&Code_OpMask)
    {
        case Code_Start:
        {
            *code -= 0x100;
            if(static_cast<int>(*code) < 0x100)
            {
                delete[] code;
            }
            return;
        }
    }
    switch(code[-1]&Code_OpMask)
    {
        case Code_Start:
        {
            code[-1] -= 0x100;
            if(static_cast<int>(code[-1]) < 0x100)
            {
                delete[] &code[-1];
            }
            break;
        }
        case Code_Offset:
        {
            code -= static_cast<int>(code[-1]>>8);
            *code -= 0x100;
            if(static_cast<int>(*code) < 0x100)
            {
                delete[] code;
            }
            break;
        }
    }
}

void printvar(ident *id, int i)
{
    if(i < 0)
    {
        conoutf("%s = %d", id->name, i);
    }
    else if(id->flags&Idf_Hex && id->maxval==0xFFFFFF)
    {
        conoutf("%s = 0x%.6X (%d, %d, %d)", id->name, i, (i>>16)&0xFF, (i>>8)&0xFF, i&0xFF);
    }
    else
    {
        conoutf(id->flags&Idf_Hex ? "%s = 0x%X" : "%s = %d", id->name, i);
    }
}

void printfvar(ident *id, float f)
{
    conoutf("%s = %s", id->name, floatstr(f));
}

void printsvar(ident *id, const char *s)
{
    conoutf(std::strchr(s, '"') ? "%s = [%s]" : "%s = \"%s\"", id->name, s);
}

void printvar(ident *id)
{
    switch(id->type)
    {
        case Id_Var:
        {
            printvar(id, *id->storage.i);
            break;
        }
        case Id_FloatVar:
        {
            printfvar(id, *id->storage.f);
            break;
        }
        case Id_StringVar:
        {
            printsvar(id, *id->storage.s);
            break;
        }
    }
}
//You know what they awoke in the darkness of Khazad-dum... shadow and flame.
// these are typedefs for argument lists of variable sizes

// they will be used below to typecast various id->fun objects to different lengths
// which allows them to only accept certain lengths of arguments
// up to 12 args typedef'd here, could be extended with more typedefs (buy why?)

//comfun stands for COMmand FUNction
typedef void (__cdecl *comfun)();
typedef void (__cdecl *comfun1)(void *);
typedef void (__cdecl *comfun2)(void *, void *);
typedef void (__cdecl *comfun3)(void *, void *, void *);
typedef void (__cdecl *comfun4)(void *, void *, void *, void *);
typedef void (__cdecl *comfun5)(void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun6)(void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun7)(void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun8)(void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun9)(void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun10)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun11)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun12)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfunv)(tagval *, int);

static const uint *skipcode(const uint *code, tagval &result = noret)
{
    int depth = 0;
    for(;;)
    {
        uint op = *code++;
        switch(op&0xFF)
        {
            case Code_Macro:
            case Code_Val|Ret_String:
            {
                uint len = op>>8;
                code += len/sizeof(uint) + 1;
                continue;
            }
            case Code_Block:
            case Code_Jump:
            case Code_JumpTrue:
            case Code_JumpFalse:
            case Code_JumpResultTrue:
            case Code_JumpResultFalse:
            {
                uint len = op>>8;
                code += len;
                continue;
            }
            case Code_Enter:
            case Code_EnterResult:
            {
                ++depth;
                continue;
            }
            case Code_Exit|Ret_Null:
            case Code_Exit|Ret_String:
            case Code_Exit|Ret_Integer:
            case Code_Exit|Ret_Float:
            {
                if(depth <= 0)
                {
                    if(&result != &noret)
                    {
                        forcearg(result, op&Code_RetMask);
                    }
                    return code;
                }
                --depth;
                continue;
            }
        }
    }
}

static uint *copycode(const uint *src)
{
    const uint *end = skipcode(src);
    size_t len = end - src;
    uint *dst = new uint[len + 1];
    *dst++ = Code_Start;
    memcpy(dst, src, len*sizeof(uint));
    return dst;
}

static void copyarg(tagval &dst, const tagval &src)
{
    switch(src.type)
    {
        case Value_Integer:
        case Value_Float:
        case Value_Ident:
        {
            dst = src;
            break;
        }
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            dst.setstr(newstring(src.s));
            break;
        }
        case Value_Code:
        {
            dst.setcode(copycode(src.code));
            break;
        }
        default:
        {
            dst.setnull();
            break;
        }
    }
}

static void addreleaseaction(ident *id, tagval *args, int numargs)
{
    tagval *dst = addreleaseaction(id, numargs+1);
    if(dst)
    {
        args[numargs].setint(1);
        for(int i = 0; i < numargs+1; ++i)
        {
            copyarg(dst[i], args[i]);
        }
    }
    else
    {
        args[numargs].setint(0);
    }
}

static void callcommand(ident *id, tagval *args, int numargs, bool lookup = false)
{
    int i = -1,
        fakeargs = 0;
    bool rep = false;
    for(const char *fmt = id->args; *fmt; fmt++)
    {
        switch(*fmt)
        {
            case 'i':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setint(0);
                    fakeargs++;
                }
                else
                {
                    forceint(args[i]);
                }
                break;
            }
            case 'b':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setint(INT_MIN);
                    fakeargs++;
                }
                else
                {
                    forceint(args[i]);
                }
                break;
            }
            case 'f':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setfloat(0.0f);
                    fakeargs++;
                }
                else
                {
                    forcefloat(args[i]);
                }
                break;
            }
            [[fallthrough]];
            case 'F':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setfloat(args[i-1].getfloat());
                    fakeargs++;
                }
                else
                {
                    forcefloat(args[i]);
                }
                break;
            }
            case 'S':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setstr(newstring(""));
                    fakeargs++;
                }
                else
                {
                    forcestr(args[i]);
                }
                break;
            }
            case 's':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setcstr("");
                    fakeargs++;
                }
                else
                {
                    forcestr(args[i]);
                }
                break;
            }
            case 'T':
            case 't':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setnull();
                    fakeargs++;
                }
                break;
            }
            case 'E':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setnull();
                    fakeargs++;
                }
                else
                {
                    forcecond(args[i]);
                }
                break;
            }
            case 'e':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setcode(emptyblock[Value_Null]+1);
                    fakeargs++;
                }
                else
                {
                    forcecode(args[i]);
                }
                break;
            }
            case 'r':
            {
                if(++i >= numargs)
                {
                    if(rep)
                    {
                        break;
                    }
                    args[i].setident(dummyident);
                    fakeargs++;
                }
                else
                {
                    forceident(args[i]);
                    break;
                }
            }
            case '$':
            {
                if(++i < numargs)
                {
                    freearg(args[i]);
                }
                args[i].setident(id);
                break;
            }
            case 'N':
            {
                if(++i < numargs)
                {
                    freearg(args[i]);
                }
                args[i].setint(lookup ? -1 : i-fakeargs);
                break;
            }
            case 'D':
            {
                if(++i < numargs)
                {
                    freearg(args[i]);
                }
                addreleaseaction(id, args, i);
                fakeargs++;
                break;
            }
            case 'C':
            {
                i = std::max(i+1, numargs);
                vector<char> buf;
                reinterpret_cast<comfun1>(id->fun)(conc(buf, args, i, true));
                goto cleanup;
            }
            case 'V':
            {
                i = std::max(i+1, numargs);
                reinterpret_cast<comfunv>(id->fun)(args, i);
                goto cleanup;
            }
            case '1':
            case '2':
            case '3':
            case '4':
            {
                if(i+1 < numargs)
                {
                    fmt -= *fmt-'0'+1;
                    rep = true;
                }
                break;
            }
        }
    }
    ++i;
    //note about offsetarg: offsetarg is not defined the same way in different parts of command.cpp:
    //it is undefined as 'n' and redefined as something else
    #define OFFSETARG(n) n
    #define ARG(n) (id->argmask&(1<<(n)) ? reinterpret_cast<void *>(args[OFFSETARG(n)].s) : reinterpret_cast<void *>(&args[OFFSETARG(n)].i))


    //callcom macro: takes a number n and type mangles the id->fun field to whatever length function is desired
    // e.g. CALLCOM(6) takes the function pointer id->fun and changes its type to comfun6 (command function w/ 6 args)
    // each argument is then described by the above ARG macro for each argument slot
    #define CALLCOM(n) \
        switch(n) \
        { \
            case 0: reinterpret_cast<comfun>(id->fun)(); break; \
            case 1: reinterpret_cast<comfun1>(id->fun)(ARG(0)); break; \
            case 2: reinterpret_cast<comfun2>(id->fun)(ARG(0), ARG(1)); break; \
            case 3: reinterpret_cast<comfun3>(id->fun)(ARG(0), ARG(1), ARG(2)); break; \
            case 4: reinterpret_cast<comfun4>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3)); break; \
            case 5: reinterpret_cast<comfun5>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4)); break; \
            case 6: reinterpret_cast<comfun6>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); break; \
            case 7: reinterpret_cast<comfun7>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6)); break; \
            case 8: reinterpret_cast<comfun8>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7)); break; \
            case 9: reinterpret_cast<comfun9>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8)); break; \
            case 10: reinterpret_cast<comfun10>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9)); break; \
            case 11: reinterpret_cast<comfun11>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10)); break; \
            case 12: reinterpret_cast<comfun12>(id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10), ARG(11)); break; \
        }
    CALLCOM(i)
    #undef OFFSETARG
cleanup:
    for(int k = 0; k < i; ++k)
    {
        freearg(args[k]);
    }
    for(; i < numargs; i++)
    {
        freearg(args[i]);
    }
}

static constexpr int maxrundepth = 255; //limit for rundepth (nesting depth) var below
static int rundepth = 0; //current rundepth

static const uint *runcode(const uint *code, tagval &result)
{
    result.setnull();
    if(rundepth >= maxrundepth)
    {
        debugcode("exceeded recursion limit");
        return skipcode(code, result);
    }
    ++rundepth;
    int numargs = 0;
    tagval args[Max_Args+Max_Results],
          *prevret = commandret;
    commandret = &result;
    for(;;)
    {
        uint op = *code++;
        switch(op&0xFF)
        {
            case Code_Start:
            case Code_Offset:
            {
                continue;
            }
            #define RETOP(op, val) \
                case op: \
                    freearg(result); \
                    val; \
                    continue;
            RETOP(Code_Null|Ret_Null, result.setnull())
            RETOP(Code_Null|Ret_String, result.setstr(newstring("")))
            RETOP(Code_Null|Ret_Integer, result.setint(0))
            RETOP(Code_Null|Ret_Float, result.setfloat(0.0f))

            RETOP(Code_False|Ret_String, result.setstr(newstring("0")))
            case Code_False|Ret_Null:
            RETOP(Code_False|Ret_Integer, result.setint(0))
            RETOP(Code_False|Ret_Float, result.setfloat(0.0f))

            RETOP(Code_True|Ret_String, result.setstr(newstring("1")))
            case Code_True|Ret_Null:
            RETOP(Code_True|Ret_Integer, result.setint(1))
            RETOP(Code_True|Ret_Float, result.setfloat(1.0f))

            #define RETPOP(op, val) \
                RETOP(op, { --numargs; val; freearg(args[numargs]); })

            RETPOP(Code_Not|Ret_String, result.setstr(newstring(getbool(args[numargs]) ? "0" : "1")))
            case Code_Not|Ret_Null:
            RETPOP(Code_Not|Ret_Integer, result.setint(getbool(args[numargs]) ? 0 : 1))
            RETPOP(Code_Not|Ret_Float, result.setfloat(getbool(args[numargs]) ? 0.0f : 1.0f))

            case Code_Pop:
            {
                freearg(args[--numargs]);
                continue;
            }
            case Code_Enter:
            {
                code = runcode(code, args[numargs++]);
                continue;
            }
            case Code_EnterResult:
            {
                freearg(result);
                code = runcode(code, result);
                continue;
            }
            case Code_Exit|Ret_String:
            case Code_Exit|Ret_Integer:
            case Code_Exit|Ret_Float:
            {
                forcearg(result, op&Code_RetMask);
            }
            [[fallthrough]];
            case Code_Exit|Ret_Null:
            {
                goto exit;
            }
            case Code_ResultArg|Ret_String:
            case Code_ResultArg|Ret_Integer:
            case Code_ResultArg|Ret_Float:
            {
                forcearg(result, op&Code_RetMask);
            }
            [[fallthrough]];
            case Code_ResultArg|Ret_Null:
            {
                args[numargs++] = result;
                result.setnull();
                continue;
            }
            case Code_Print:
            {
                printvar(identmap[op>>8]);
                continue;
            }
            case Code_Local:
            {
                freearg(result);
                int numlocals = op>>8, offset = numargs-numlocals;
                identstack locals[Max_Args];
                for(int i = 0; i < numlocals; ++i)
                {
                    pushalias(*args[offset+i].id, locals[i]);
                }
                code = runcode(code, result);
                for(int i = offset; i < numargs; i++)
                {
                    popalias(*args[i].id);
                }
                goto exit;
            }
            case Code_DoArgs|Ret_Null:
            case Code_DoArgs|Ret_String:
            case Code_DoArgs|Ret_Integer:
            case Code_DoArgs|Ret_Float:
            {
                UNDOARGS
                freearg(result);
                runcode(args[--numargs].code, result);
                freearg(args[numargs]);
                forcearg(result, op&Code_RetMask);
                REDOARGS
                continue;
            }
            case Code_Do|Ret_Null:
            case Code_Do|Ret_String:
            case Code_Do|Ret_Integer:
            case Code_Do|Ret_Float:
                freearg(result);
                runcode(args[--numargs].code, result);
                freearg(args[numargs]);
                forcearg(result, op&Code_RetMask);
                continue;

            case Code_Jump:
            {
                uint len = op>>8;
                code += len;
                continue;
            }
            case Code_JumpTrue:
            {
                uint len = op>>8;
                if(getbool(args[--numargs]))
                {
                    code += len;
                }
                freearg(args[numargs]);
                continue;
            }
            case Code_JumpFalse:
            {
                uint len = op>>8;
                if(!getbool(args[--numargs]))
                {
                    code += len;
                }
                freearg(args[numargs]);
                continue;
            }
            case Code_JumpResultTrue:
            {
                uint len = op>>8;
                freearg(result);
                --numargs;
                if(args[numargs].type == Value_Code)
                {
                    runcode(args[numargs].code, result);
                    freearg(args[numargs]);
                }
                else
                {
                    result = args[numargs];
                }
                if(getbool(result))
                {
                    code += len;
                }
                continue;
            }
            case Code_JumpResultFalse:
            {
                uint len = op>>8;
                freearg(result);
                --numargs;
                if(args[numargs].type == Value_Code)
                {
                    runcode(args[numargs].code, result);
                    freearg(args[numargs]);
                }
                else
                {
                    result = args[numargs];
                }
                if(!getbool(result))
                {
                    code += len;
                }
                continue;
            }
            case Code_Macro:
            {
                uint len = op>>8;
                args[numargs++].setmacro(code);
                code += len/sizeof(uint) + 1;
                continue;
            }
            case Code_Val|Ret_String:
            {
                uint len = op>>8;
                args[numargs++].setstr(newstring(reinterpret_cast<const char *>(code), len));
                code += len/sizeof(uint) + 1;
                continue;
            }
            case Code_ValI|Ret_String:
            {
                char s[4] = { static_cast<char>((op>>8)&0xFF), static_cast<char>((op>>16)&0xFF), static_cast<char>((op>>24)&0xFF), '\0' };
                args[numargs++].setstr(newstring(s));
                continue;
            }
            case Code_Val|Ret_Null:
            case Code_ValI|Ret_Null:
            {
                args[numargs++].setnull();
                continue;
            }
            case Code_Val|Ret_Integer:
            {
                args[numargs++].setint(static_cast<int>(*code++));
                continue;
            }
            case Code_ValI|Ret_Integer:
            {
                args[numargs++].setint(static_cast<int>(op)>>8);
                continue;
            }
            case Code_Val|Ret_Float:
            {
                args[numargs++].setfloat(*reinterpret_cast<const float *>(code++));
                continue;
            }
            case Code_ValI|Ret_Float:
            {
                args[numargs++].setfloat(static_cast<float>(static_cast<int>(op)>>8));
                continue;
            }
            case Code_Dup|Ret_Null:
            {
                args[numargs-1].getval(args[numargs]);
                numargs++;
                continue;
            }
            case Code_Dup|Ret_Integer:
            {
                args[numargs].setint(args[numargs-1].getint());
                numargs++;
                continue;
            }
            case Code_Dup|Ret_Float:
            {
                args[numargs].setfloat(args[numargs-1].getfloat());
                numargs++;
                continue;
            }
            case Code_Dup|Ret_String:
            {
                args[numargs].setstr(newstring(args[numargs-1].getstr()));
                numargs++;
                continue;
            }
            case Code_Force|Ret_String:
            {
                forcestr(args[numargs-1]);
                continue;
            }
            case Code_Force|Ret_Integer:
            {
                forceint(args[numargs-1]);
                continue;
            }
            case Code_Force|Ret_Float:
            {
                forcefloat(args[numargs-1]);
                continue;
            }
            case Code_Result|Ret_Null:
            {
                freearg(result);
                result = args[--numargs];
                continue;
            }
            case Code_Result|Ret_String:
            case Code_Result|Ret_Integer:
            case Code_Result|Ret_Float:
            {
                freearg(result);
                result = args[--numargs];
                forcearg(result, op&Code_RetMask);
                continue;
            }
            case Code_Empty|Ret_Null:
            {
                args[numargs++].setcode(emptyblock[Value_Null]+1);
                break;
            }
            case Code_Empty|Ret_String:
            {
                args[numargs++].setcode(emptyblock[Value_String]+1);
                break;
            }
            case Code_Empty|Ret_Integer:
            {
                args[numargs++].setcode(emptyblock[Value_Integer]+1);
                break;
            }
            case Code_Empty|Ret_Float:
            {
                args[numargs++].setcode(emptyblock[Value_Float]+1);
                break;
            }
            case Code_Block:
            {
                uint len = op>>8;
                args[numargs++].setcode(code+1);
                code += len;
                continue;
            }
            case Code_Compile:
            {
                tagval &arg = args[numargs-1];
                vector<uint> buf;
                switch(arg.type)
                {
                    case Value_Integer:
                    {
                        buf.reserve(8);
                        buf.add(Code_Start);
                        compileint(buf, arg.i);
                        buf.add(Code_Result);
                        buf.add(Code_Exit);
                        break;
                    }
                    case Value_Float:
                    {
                        buf.reserve(8);
                        buf.add(Code_Start);
                        compilefloat(buf, arg.f);
                        buf.add(Code_Result);
                        buf.add(Code_Exit);
                        break;
                    }
                    case Value_String:
                    case Value_Macro:
                    case Value_CString:
                    {
                        buf.reserve(64);
                        compilemain(buf, arg.s);
                        freearg(arg);
                        break;
                    }
                    default:
                    {
                        buf.reserve(8);
                        buf.add(Code_Start);
                        compilenull(buf);
                        buf.add(Code_Result);
                        buf.add(Code_Exit);
                        break;
                    }
                }
                arg.setcode(buf.disown()+1);
                continue;
            }
            case Code_Cond:
            {
                tagval &arg = args[numargs-1];
                switch(arg.type)
                {
                    case Value_String:
                    case Value_Macro:
                    case Value_CString:
                        if(arg.s[0])
                        {
                            vector<uint> buf;
                            buf.reserve(64);
                            compilemain(buf, arg.s);
                            freearg(arg);
                            arg.setcode(buf.disown()+1);
                        }
                        else
                        {
                            forcenull(arg);
                        }
                        break;
                }
                continue;
            }
            case Code_Ident:
            {
                args[numargs++].setident(identmap[op>>8]);
                continue;
            }
            case Code_IdentArg:
            {
                ident *id = identmap[op>>8];
                if(!(aliasstack->usedargs&(1<<id->index)))
                {
                    pusharg(*id, nullval, aliasstack->argstack[id->index]);
                    aliasstack->usedargs |= 1<<id->index;
                }
                args[numargs++].setident(id);
                continue;
            }
            case Code_IdentU:
            {
                tagval &arg = args[numargs-1];
                ident *id = arg.type ==     Value_String
                                         || arg.type == Value_Macro
                                         || arg.type == Value_CString ? newident(arg.s, Idf_Unknown) : dummyident;
                if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index)))
                {
                    pusharg(*id, nullval, aliasstack->argstack[id->index]);
                    aliasstack->usedargs |= 1<<id->index;
                }
                freearg(arg);
                arg.setident(id);
                continue;
            }

            case Code_LookupU|Ret_String:
                #define LOOKUPU(aval, sval, ival, fval, nval) { \
                    tagval &arg = args[numargs-1]; \
                    if(arg.type != Value_String && arg.type != Value_Macro && arg.type != Value_CString) \
                    { \
                        continue; \
                    } \
                    ident *id = idents.access(arg.s); \
                    if(id) \
                    { \
                        switch(id->type) \
                        { \
                            case Id_Alias: \
                            { \
                                if(id->flags&Idf_Unknown) \
                                { \
                                    break; \
                                } \
                                freearg(arg); \
                                if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index))) \
                                { \
                                    nval; \
                                    continue; \
                                } \
                                aval; \
                                continue; \
                            } \
                            case Id_StringVar: \
                            { \
                                freearg(arg); \
                                sval; \
                                continue; \
                            } \
                            case Id_Var: \
                            { \
                                freearg(arg); \
                                ival; \
                                continue; \
                            } \
                            case Id_FloatVar: \
                            { \
                                freearg(arg); \
                                fval; \
                                continue; \
                            } \
                            case Id_Command: \
                            { \
                                freearg(arg); \
                                arg.setnull(); \
                                commandret = &arg; \
                                tagval buf[Max_Args]; \
                                callcommand(id, buf, 0, true); \
                                forcearg(arg, op&Code_RetMask); \
                                commandret = &result; \
                                continue; \
                            } \
                            default: \
                            { \
                                freearg(arg); \
                                nval; \
                                continue; \
                            } \
                        } \
                    } \
                    debugcode("unknown alias lookup: %s", arg.s); \
                    freearg(arg); \
                    nval; \
                    continue; \
                }
                LOOKUPU(arg.setstr(newstring(id->getstr())),
                        arg.setstr(newstring(*id->storage.s)),
                        arg.setstr(newstring(intstr(*id->storage.i))),
                        arg.setstr(newstring(floatstr(*id->storage.f))),
                        arg.setstr(newstring("")));
            case Code_Lookup|Ret_String:
                #define LOOKUP(aval) { \
                    ident *id = identmap[op>>8]; \
                    if(id->flags&Idf_Unknown) \
                    { \
                        debugcode("unknown alias lookup: %s", id->name); \
                    } \
                    aval; \
                    continue; \
                }
                LOOKUP(args[numargs++].setstr(newstring(id->getstr())));
            case Code_LookupArg|Ret_String:
                #define LOOKUPARG(aval, nval) { \
                    ident *id = identmap[op>>8]; \
                    if(!(aliasstack->usedargs&(1<<id->index))) \
                    { \
                        nval; \
                        continue; \
                    } \
                    aval; \
                    continue; \
                }
                LOOKUPARG(args[numargs++].setstr(newstring(id->getstr())), args[numargs++].setstr(newstring("")));
            case Code_LookupU|Ret_Integer:
                LOOKUPU(arg.setint(id->getint()),
                        arg.setint(parseint(*id->storage.s)),
                        arg.setint(*id->storage.i),
                        arg.setint(static_cast<int>(*id->storage.f)),
                        arg.setint(0));
            case Code_Lookup|Ret_Integer:
                LOOKUP(args[numargs++].setint(id->getint()));
            case Code_LookupArg|Ret_Integer:
                LOOKUPARG(args[numargs++].setint(id->getint()), args[numargs++].setint(0));
            case Code_LookupU|Ret_Float:
                LOOKUPU(arg.setfloat(id->getfloat()),
                        arg.setfloat(parsefloat(*id->storage.s)),
                        arg.setfloat(static_cast<float>(*id->storage.i)),
                        arg.setfloat(*id->storage.f),
                        arg.setfloat(0.0f));
            case Code_Lookup|Ret_Float:
                LOOKUP(args[numargs++].setfloat(id->getfloat()));
            case Code_LookupArg|Ret_Float:
                LOOKUPARG(args[numargs++].setfloat(id->getfloat()), args[numargs++].setfloat(0.0f));
            case Code_LookupU|Ret_Null:
                LOOKUPU(id->getval(arg),
                        arg.setstr(newstring(*id->storage.s)),
                        arg.setint(*id->storage.i),
                        arg.setfloat(*id->storage.f),
                        arg.setnull());
            case Code_Lookup|Ret_Null:
                LOOKUP(id->getval(args[numargs++]));
            case Code_LookupArg|Ret_Null:
                LOOKUPARG(id->getval(args[numargs++]), args[numargs++].setnull());
            case Code_LookupMU|Ret_String:
                LOOKUPU(id->getcstr(arg),
                        arg.setcstr(*id->storage.s),
                        arg.setstr(newstring(intstr(*id->storage.i))),
                        arg.setstr(newstring(floatstr(*id->storage.f))),
                        arg.setcstr(""));
            case Code_LookupM|Ret_String:
                LOOKUP(id->getcstr(args[numargs++]));
            case Code_LookupMArg|Ret_String:
                LOOKUPARG(id->getcstr(args[numargs++]), args[numargs++].setcstr(""));
            case Code_LookupMU|Ret_Null:
                LOOKUPU(id->getcval(arg),
                        arg.setcstr(*id->storage.s),
                        arg.setint(*id->storage.i),
                        arg.setfloat(*id->storage.f),
                        arg.setnull());
            case Code_LookupM|Ret_Null:
                LOOKUP(id->getcval(args[numargs++]));
            case Code_LookupMArg|Ret_Null:
                LOOKUPARG(id->getcval(args[numargs++]), args[numargs++].setnull());

            case Code_StrVar|Ret_String:
            case Code_StrVar|Ret_Null:
            {
                args[numargs++].setstr(newstring(*identmap[op>>8]->storage.s));
                continue;
            }
            case Code_StrVar|Ret_Integer:
            {
                args[numargs++].setint(parseint(*identmap[op>>8]->storage.s));
                continue;
            }
            case Code_StrVar|Ret_Float:
            {
                args[numargs++].setfloat(parsefloat(*identmap[op>>8]->storage.s));
                continue;
            }
            case Code_StrVarM:
            {
                args[numargs++].setcstr(*identmap[op>>8]->storage.s);
                continue;
            }
            case Code_StrVar1:
            {
                setsvarchecked(identmap[op>>8], args[--numargs].s); freearg(args[numargs]);
                continue;
            }
            case Code_IntVar|Ret_Integer:
            case Code_IntVar|Ret_Null:
            {
                args[numargs++].setint(*identmap[op>>8]->storage.i);
                continue;
            }
            case Code_IntVar|Ret_String:
            {
                args[numargs++].setstr(newstring(intstr(*identmap[op>>8]->storage.i)));
                continue;
            }
            case Code_IntVar|Ret_Float:
            {
                args[numargs++].setfloat(static_cast<float>(*identmap[op>>8]->storage.i));
                continue;
            }
            case Code_IntVar1:
            {
                setvarchecked(identmap[op>>8], args[--numargs].i);
                continue;
            }
            case Code_IntVar2:
            {
                numargs -= 2;
                setvarchecked(identmap[op>>8], (args[numargs].i<<16)|(args[numargs+1].i<<8));
                continue;
            }
            case Code_IntVar3:
            {
                numargs -= 3;
                setvarchecked(identmap[op>>8], (args[numargs].i<<16)|(args[numargs+1].i<<8)|args[numargs+2].i);
                continue;
            }
            case Code_FloatVar|Ret_Float:
            case Code_FloatVar|Ret_Null:
            {
                args[numargs++].setfloat(*identmap[op>>8]->storage.f);
                continue;
            }
            case Code_FloatVar|Ret_String:
            {
                args[numargs++].setstr(newstring(floatstr(*identmap[op>>8]->storage.f)));
                continue;
            }
            case Code_FloatVar|Ret_Integer:
            {
                args[numargs++].setint(static_cast<int>(*identmap[op>>8]->storage.f));
                continue;
            }
            case Code_FloatVar1:
            {
                setfvarchecked(identmap[op>>8], args[--numargs].f);
                continue;
            }
            #define OFFSETARG(n) offset+n
            case Code_Com|Ret_Null:
            case Code_Com|Ret_String:
            case Code_Com|Ret_Float:
            case Code_Com|Ret_Integer:
            {
                ident *id = identmap[op>>8];
                int offset = numargs-id->numargs;
                forcenull(result);
                CALLCOM(id->numargs)
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }
            case Code_ComD|Ret_Null:
            case Code_ComD|Ret_String:
            case Code_ComD|Ret_Float:
            case Code_ComD|Ret_Integer:
            {
                ident *id = identmap[op>>8];
                int offset = numargs-(id->numargs-1);
                addreleaseaction(id, &args[offset], id->numargs-1);
                CALLCOM(id->numargs)
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }
            #undef OFFSETARG

            case Code_ComV|Ret_Null:
            case Code_ComV|Ret_String:
            case Code_ComV|Ret_Float:
            case Code_ComV|Ret_Integer:
            {
                ident *id = identmap[op>>13];
                int callargs = (op>>8)&0x1F,
                    offset = numargs-callargs;
                forcenull(result);
                reinterpret_cast<comfunv>(id->fun)(&args[offset], callargs);
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }
            case Code_ComC|Ret_Null:
            case Code_ComC|Ret_String:
            case Code_ComC|Ret_Float:
            case Code_ComC|Ret_Integer:
            {
                ident *id = identmap[op>>13];
                int callargs = (op>>8)&0x1F,
                    offset = numargs-callargs;
                forcenull(result);
                {
                    vector<char> buf;
                    buf.reserve(maxstrlen);
                    reinterpret_cast<comfun1>(id->fun)(conc(buf, &args[offset], callargs, true));
                }
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }
            case Code_ConC|Ret_Null:
            case Code_ConC|Ret_String:
            case Code_ConC|Ret_Float:
            case Code_ConC|Ret_Integer:
            case Code_ConCW|Ret_Null:
            case Code_ConCW|Ret_String:
            case Code_ConCW|Ret_Float:
            case Code_ConCW|Ret_Integer:
            {
                int numconc = op>>8;
                char *s = conc(&args[numargs-numconc], numconc, (op&Code_OpMask)==Code_ConC);
                freeargs(args, numargs, numargs-numconc);
                args[numargs].setstr(s);
                forcearg(args[numargs], op&Code_RetMask);
                numargs++;
                continue;
            }

            case Code_ConCM|Ret_Null:
            case Code_ConCM|Ret_String:
            case Code_ConCM|Ret_Float:
            case Code_ConCM|Ret_Integer:
            {
                int numconc = op>>8;
                char *s = conc(&args[numargs-numconc], numconc, false);
                freeargs(args, numargs, numargs-numconc);
                result.setstr(s);
                forcearg(result, op&Code_RetMask);
                continue;
            }
            case Code_Alias:
            {
                setalias(*identmap[op>>8], args[--numargs]);
                continue;
            }
            case Code_AliasArg:
            {
                setarg(*identmap[op>>8], args[--numargs]);
                continue;
            }
            case Code_AliasU:
            {
                numargs -= 2;
                setalias(args[numargs].getstr(), args[numargs+1]);
                freearg(args[numargs]);
                continue;
            }
            #define SKIPARGS(offset) offset
            case Code_Call|Ret_Null:
            case Code_Call|Ret_String:
            case Code_Call|Ret_Float:
            case Code_Call|Ret_Integer:
            {
                #define FORCERESULT { \
                    freeargs(args, numargs, SKIPARGS(offset)); \
                    forcearg(result, op&Code_RetMask); \
                    continue; \
                }
                #define CALLALIAS { \
                    identstack argstack[Max_Args]; \
                    for(int i = 0; i < callargs; i++) \
                    { \
                        pusharg(*identmap[i], args[offset + i], argstack[i]); \
                    } \
                    int oldargs = _numargs; \
                    _numargs = callargs; \
                    int oldflags = identflags; \
                    identflags |= id->flags&Idf_Overridden; \
                    IdentLink aliaslink = { id, aliasstack, (1<<callargs)-1, argstack }; \
                    aliasstack = &aliaslink; \
                    if(!id->code) \
                    { \
                        id->code = compilecode(id->getstr()); \
                    } \
                    uint *code = id->code; \
                    code[0] += 0x100; \
                    runcode(code+1, result); \
                    code[0] -= 0x100; \
                    if(static_cast<int>(code[0]) < 0x100) \
                    { \
                        delete[] code; \
                    } \
                    aliasstack = aliaslink.next; \
                    identflags = oldflags; \
                    for(int i = 0; i < callargs; i++) \
                    { \
                        poparg(*identmap[i]); \
                    } \
                    for(int argmask = aliaslink.usedargs&(~0U<<callargs), i = callargs; argmask; i++) \
                    { \
                        if(argmask&(1<<i)) \
                        { \
                            poparg(*identmap[i]); \
                            argmask &= ~(1<<i); \
                        } \
                    } \
                    forcearg(result, op&Code_RetMask); \
                    _numargs = oldargs; \
                    numargs = SKIPARGS(offset); \
                }

                forcenull(result);
                ident *id = identmap[op>>13];
                int callargs = (op>>8)&0x1F,
                    offset = numargs-callargs;
                if(id->flags&Idf_Unknown)
                {
                    debugcode("unknown command: %s", id->name);
                    FORCERESULT;
                }
                CALLALIAS;
                continue;
            }
            case Code_CallArg|Ret_Null:
            case Code_CallArg|Ret_String:
            case Code_CallArg|Ret_Float:
            case Code_CallArg|Ret_Integer:
            {
                forcenull(result);
                ident *id = identmap[op>>13];
                int callargs = (op>>8)&0x1F,
                    offset = numargs-callargs;
                if(!(aliasstack->usedargs&(1<<id->index)))
                {
                    FORCERESULT;
                }
                CALLALIAS;
                continue;
            }
            #undef SKIPARGS
//==============================================================================
            #define SKIPARGS(offset) offset-1
            case Code_CallU|Ret_Null:
            case Code_CallU|Ret_String:
            case Code_CallU|Ret_Float:
            case Code_CallU|Ret_Integer:
            {
                int callargs = op>>8,
                    offset = numargs-callargs;
                tagval &idarg = args[offset-1];
                if(idarg.type != Value_String && idarg.type != Value_Macro && idarg.type != Value_CString)
                {
                litval:
                    freearg(result);
                    result = idarg;
                    forcearg(result, op&Code_RetMask);
                    while(--numargs >= offset)
                    {
                        freearg(args[numargs]);
                    }
                    continue;
                }
                ident *id = idents.access(idarg.s);
                if(!id)
                {
                noid:
                    if(checknumber(idarg.s))
                    {
                        goto litval;
                    }
                    debugcode("unknown command: %s", idarg.s);
                    forcenull(result);
                    FORCERESULT;
                }
                forcenull(result);
                switch(id->type)
                {
                    default:
                    {
                        if(!id->fun)
                        {
                            FORCERESULT;
                        }
                    }
                    [[fallthrough]];
                    case Id_Command:
                    {
                        freearg(idarg);
                        callcommand(id, &args[offset], callargs);
                        forcearg(result, op&Code_RetMask);
                        numargs = offset - 1;
                        continue;
                    }
                    case Id_Local:
                    {
                        identstack locals[Max_Args];
                        freearg(idarg);
                        for(int j = 0; j < callargs; ++j)
                        {
                            pushalias(*forceident(args[offset+j]), locals[j]);
                        }
                        code = runcode(code, result);
                        for(int j = 0; j < callargs; ++j)
                        {
                            popalias(*args[offset+j].id);
                        }
                        goto exit;
                    }
                    case Id_Var:
                    {
                        if(callargs <= 0)
                        {
                            printvar(id);
                        }
                        else
                        {
                            setvarchecked(id, &args[offset], callargs);
                        }
                        FORCERESULT;
                    }
                    case Id_FloatVar:
                        if(callargs <= 0)
                        {
                            printvar(id);
                        }
                        else
                        {
                            setfvarchecked(id, forcefloat(args[offset]));
                        }
                        FORCERESULT;
                    case Id_StringVar:
                        if(callargs <= 0)
                        {
                            printvar(id);
                        }
                        else
                        {
                            setsvarchecked(id, forcestr(args[offset]));
                        }
                        FORCERESULT;
                    case Id_Alias:
                        if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index)))
                        {
                            FORCERESULT;
                        }
                        if(id->valtype==Value_Null)
                        {
                            goto noid;
                        }
                        freearg(idarg);
                        CALLALIAS;
                        continue;
                }
            }
            #undef SKIPARGS
        }
    }
exit:
    commandret = prevret;
    --rundepth;
    return code;
}

void executeret(const uint *code, tagval &result)
{
    runcode(code, result);
}

void executeret(const char *p, tagval &result)
{
    vector<uint> code;
    code.reserve(64);
    compilemain(code, p, Value_Any);
    runcode(code.getbuf()+1, result);
    if(static_cast<int>(code[0]) >= 0x100) code.disown();
}

void executeret(ident *id, tagval *args, int numargs, bool lookup, tagval &result)
{
    result.setnull();
    ++rundepth;
    tagval *prevret = commandret;
    commandret = &result;
    if(rundepth > maxrundepth)
    {
        debugcode("exceeded recursion limit");
    }
    else if(id)
    {
        switch(id->type)
        {
            default:
                if(!id->fun)
                {
                    break;
                }
                [[fallthrough]];
            case Id_Command:
                if(numargs < id->numargs)
                {
                    tagval buf[Max_Args];
                    memcpy(buf, args, numargs*sizeof(tagval));
                    callcommand(id, buf, numargs, lookup);
                }
                else
                {
                    callcommand(id, args, numargs, lookup);
                }
                numargs = 0;
                break;
            case Id_Var:
                if(numargs <= 0)
                {
                    printvar(id);
                }
                else
                {
                    setvarchecked(id, args, numargs);
                }
                break;
            case Id_FloatVar:
                if(numargs <= 0)
                {
                    printvar(id);
                }
                else
                {
                    setfvarchecked(id, forcefloat(args[0]));
                }
                break;
            case Id_StringVar:
                if(numargs <= 0)
                {
                    printvar(id);
                }
                else
                {
                    setsvarchecked(id, forcestr(args[0]));
                }
                break;
            case Id_Alias:
            {
                if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index)))
                {
                    break;
                }
                if(id->valtype==Value_Null)
                {
                    break;
                }
                //C++ preprocessor abuse
                //uses CALLALIAS form but substitutes in a bunch of special values
                //and then undefines them immediately after
                #define callargs numargs
                #define offset 0
                #define op Ret_Null
                #define SKIPARGS(offset) offset
                CALLALIAS;
                #undef callargs
                #undef offset
                #undef op
                #undef SKIPARGS
                break;
            }
        }
    }
    freeargs(args, numargs, 0);
    commandret = prevret;
    --rundepth;
}

char *executestr(const uint *code)
{
    tagval result;
    runcode(code, result);
    if(result.type == Value_Null)
    {
        return nullptr;
    }
    forcestr(result);
    return result.s;
}

char *executestr(const char *p)
{
    tagval result;
    executeret(p, result);
    if(result.type == Value_Null)
    {
        return nullptr;
    }
    forcestr(result);
    return result.s;
}

char *executestr(ident *id, tagval *args, int numargs, bool lookup)
{
    tagval result;
    executeret(id, args, numargs, lookup, result);
    if(result.type == Value_Null)
    {
        return nullptr;
    }
    forcestr(result);
    return result.s;
}

char *execidentstr(const char *name, bool lookup)
{
    ident *id = idents.access(name);
    return id ? executestr(id, nullptr, 0, lookup) : nullptr;
}

int execute(const uint *code)
{
    tagval result;
    runcode(code, result);
    int i = result.getint();
    freearg(result);
    return i;
}

int execute(const char *p)
{
    vector<uint> code;
    code.reserve(64);
    compilemain(code, p, Value_Integer);
    tagval result;
    runcode(code.getbuf()+1, result);
    if(static_cast<int>(code[0]) >= 0x100)
    {
        code.disown();
    }
    int i = result.getint();
    freearg(result);
    return i;
}

int execute(ident *id, tagval *args, int numargs, bool lookup)
{
    tagval result;
    executeret(id, args, numargs, lookup, result);
    int i = result.getint();
    freearg(result);
    return i;
}

int execident(const char *name, int noid, bool lookup)
{
    ident *id = idents.access(name);
    return id ? execute(id, nullptr, 0, lookup) : noid;
}

float executefloat(const uint *code)
{
    tagval result;
    runcode(code, result);
    float f = result.getfloat();
    freearg(result);
    return f;
}

float executefloat(const char *p)
{
    tagval result;
    executeret(p, result);
    float f = result.getfloat();
    freearg(result);
    return f;
}

float executefloat(ident *id, tagval *args, int numargs, bool lookup)
{
    tagval result;
    executeret(id, args, numargs, lookup, result);
    float f = result.getfloat();
    freearg(result);
    return f;
}

float execidentfloat(const char *name, float noid, bool lookup)
{
    ident *id = idents.access(name);
    return id ? executefloat(id, nullptr, 0, lookup) : noid;
}

bool executebool(const uint *code)
{
    tagval result;
    runcode(code, result);
    bool b = getbool(result);
    freearg(result);
    return b;
}

bool executebool(const char *p)
{
    tagval result;
    executeret(p, result);
    bool b = getbool(result);
    freearg(result);
    return b;
}

bool executebool(ident *id, tagval *args, int numargs, bool lookup)
{
    tagval result;
    executeret(id, args, numargs, lookup, result);
    bool b = getbool(result);
    freearg(result);
    return b;
}

bool execidentbool(const char *name, bool noid, bool lookup)
{
    ident *id = idents.access(name);
    return id ? executebool(id, nullptr, 0, lookup) : noid;
}

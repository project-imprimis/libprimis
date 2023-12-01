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

std::unordered_map<std::string, ident> idents; // contains ALL vars/commands/aliases
static std::vector<ident *> identmap;
static ident *dummyident = nullptr;
std::queue<ident *> triggerqueue; //for the game to handle var change events
static constexpr uint cmdqueuedepth = 128; //how many elements before oldest queued data gets discarded
int identflags = 0;

const char *sourcefile = nullptr,
           *sourcestr  = nullptr;

std::vector<char> strbuf[4];
int stridx = 0;

static constexpr int undoflag = 1<<Max_Args;

static IdentLink noalias = { nullptr, nullptr, (1<<Max_Args)-1, nullptr },
             *aliasstack = &noalias;

static int _numargs = variable("numargs", Max_Args, 0, 0, &_numargs, nullptr, 0);

//ident object

void ident::getval(tagval &r) const
{
    ::getval(val, valtype, r);
}

void ident::getcstr(tagval &v) const
{
    switch(valtype)
    {
        case Value_Macro:
        {
            v.setmacro(val.code);
            break;
        }
        case Value_String:
        case Value_CString:
        {
            v.setcstr(val.s);
            break;
        }
        case Value_Integer:
        {
            v.setstr(newstring(intstr(val.i)));
            break;
        }
        case Value_Float:
        {
            v.setstr(newstring(floatstr(val.f)));
            break;
        }
        default:
        {
            v.setcstr("");
            break;
        }
    }
}

void ident::getcval(tagval &v) const
{
    switch(valtype)
    {
        case Value_Macro:
        {
            v.setmacro(val.code);
            break;
        }
        case Value_String:
        case Value_CString:
        {
            v.setcstr(val.s);
            break;
        }
        case Value_Integer:
        {
            v.setint(val.i);
            break;
        }
        case Value_Float:
        {
            v.setfloat(val.f);
            break;
        }
        default:
        {
            v.setnull();
            break;
        }
    }
}

//tagval object

void tagval::setint(int val)
{
    type = Value_Integer;
    i = val;
}

void tagval::setfloat(float val)
{
    type = Value_Float;
    f = val;
}

void tagval::setnumber(double val)
{
    i = static_cast<int>(val);
    if(val == i)
    {
        type = Value_Integer;
    }
    else
    {
        type = Value_Float;
        f = val;
    }
}

void tagval::setstr(char *val)
{
    type = Value_String;
    s = val;
}

void tagval::setnull()
{
    type = Value_Null;
    i = 0;
}

void tagval::setcode(const uint *val)
{
    type = Value_Code;
    code = val;
}

void tagval::setmacro(const uint *val)
{
    type = Value_Macro;
    code = val;
}

void tagval::setcstr(const char *val)
{
    type = Value_CString;
    cstr = val;
}

void tagval::setident(ident *val)
{
    type = Value_Ident;
    id = val;
}

//end tagval

static int getint(const identval &v, int type)
{
    switch(type)
    {
        case Value_Float:
        {
            return static_cast<int>(v.f);
        }
        case Value_Integer:
        {
            return static_cast<int>(v.i);
        }
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            return parseint(v.s);
        }
        default:
        {
            return 0;
        }
    }
}

int tagval::getint() const
{
    return ::getint(*this, type);
}

int ident::getint() const
{
    return ::getint(val, valtype);
}

float getfloat(const identval &v, int type)
{
    switch(type)
    {
        case Value_Float:
        {
            return static_cast<float>(v.f);
        }
        case Value_Integer:
        {
            return static_cast<float>(v.i);
        }
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            return parsefloat(v.s);
        }
        default:
        {
            return 0.f;
        }
    }
}

float tagval::getfloat() const
{
    return ::getfloat(*this, type);
}

float ident::getfloat() const
{
    return ::getfloat(val, valtype);
}

static double getnumber(const identval &v, int type)
{
    switch(type)
    {
        case Value_Float:
        {
            return static_cast<double>(v.f);
        }
        case Value_Integer:
        {
            return static_cast<double>(v.i);
        }
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            return parsenumber(v.s);
        }
        default:
        {
            return 0.0;
        }
    }
}

double tagval::getnumber() const
{
    return ::getnumber(*this, type);
}

double ident::getnumber() const
{
    return ::getnumber(val, valtype);
}

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
                //delete starting at index **[-1]**, since tagvals get passed arrays starting at index 1
                delete[] &v.code[-1];
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

static tagval noret = NullVal();

tagval * commandret = &noret;

void clear_command()
{
    for(auto& [k, i] : idents)
    {
        if(i.type==Id_Alias)
        {
            delete[] i.name;
            i.name = nullptr;

            i.forcenull();

            delete[] i.code;
            i.code = nullptr;
        }
    }
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
    for(auto& [k, id] : idents)
    {
        clearoverride(id);
    }
}

static bool initedidents = false;
static std::vector<ident> *identinits = nullptr;

static ident *addident(const ident &id)
{
    if(!initedidents)
    {
        if(!identinits)
        {
            identinits = new std::vector<ident>;
        }
        identinits->push_back(id);
        return nullptr;
    }
    const auto itr = idents.find(id.name);
    if(itr == idents.end())
    {
        //we need to make a new entry
        idents[id.name] = id;
    }
    ident &def = idents[id.name];
    def.index = identmap.size();
    identmap.push_back(&def);
    return identmap.back();
}

ident *newident(const char *name, int flags = 0);

bool initidents()
{
    initedidents = true;
    for(int i = 0; i < Max_Args; i++)
    {
        std::string argname = std::string("arg").append(std::to_string(i+1));
        newident(argname.c_str(), Idf_Arg);
    }
    dummyident = newident("//dummy", Idf_Unknown);
    if(identinits)
    {
        for(uint i = 0; i < (*identinits).size(); i++)
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

void pushcmd(ident *id, tagval *v, const uint *code)
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

static void pushalias(ident &id, identstack &stack)
{
    if(id.type == Id_Alias && id.index >= Max_Args)
    {
        pusharg(id, NullVal(), stack);
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

//returns whether the string passed starts with a valid number
//does not check that all characters are valid, only the first number possibly succeeding a +/-/.
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

ident *newident(const char *name, int flags)
{
    ident *id = nullptr;
    const auto itr = idents.find(name);
    if(itr == idents.end())
    {
        if(checknumber(name))
        {
            debugcode("number %s is not a valid identifier name", name);
            return dummyident;
        }
        id = addident(ident(Id_Alias, newstring(name), flags));
    }
    else
    {
        id = &(*(itr)).second;
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

ident *writeident(const char *name, int flags)
{
    ident *id = newident(name, flags);
    if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index)))
    {
        pusharg(*id, NullVal(), aliasstack->argstack[id->index]);
        aliasstack->usedargs |= 1<<id->index;
    }
    return id;
}

static void resetvar(char *name)
{
    const auto itr = idents.find(name);
    if(itr == idents.end())
    {
        return;
    }
    else
    {
        ident* id = &(*(itr)).second;
        if(id->flags&Idf_ReadOnly)
        {
            debugcode("variable %s is read-only", id->name);
        }
        else
        {
            clearoverride(*id);
        }
    }
}

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
    const auto itr = idents.find(name);
    if(itr != idents.end())
    {
        ident *id = &(*(itr)).second;
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

/**
 * @brief Gets the CubeScript variable.
 * @param vartype the identifier, such as float, integer, var, or command.
 * @param name the variable's name.
 * @return ident* the pointer to the variable.
 */
ident* getvar(int vartype, const char *name)
{
    const auto itr = idents.find(name);
    if(itr != idents.end())
    {
        ident *id = &(*(itr)).second;
        if(!id || id->type!=vartype)
        {
            return nullptr;
        }
        return id;
    }
    return nullptr;
}

/**
 * @brief Overwrite an ident's array with an array comming from storage.
 * @tparam T the array data type, typically StringVar.
 * @param id the StringVar identifier.
 * @param dst the string that will be overwritten by the `src`.
 * @param src the string that will be written to `dest`.
 */
template <class T>
void storevalarray(ident *id, T &dst, T *src) {

    if(identflags&Idf_Overridden || id->flags&Idf_Override)
    {
        if(id->flags&Idf_Persist)
        {
            // Return error.
            debugcode("Cannot override persistent variable %s", id->name);
            return;
        }
        if(!(id->flags&Idf_Overridden))
        {
            // Save source array.
            dst = *src;
            id->flags |= Idf_Overridden;
        }
        else
        {
            // Reset source array.
            delete[] *src;
        }
    }
    else
    {
        if(id->flags&Idf_Overridden)
        {
            // Reset saved array.
            delete[] dst;
            id->flags &= ~Idf_Overridden;
        }
        // Reset source array.
        delete[] *src;
    }
}

/**
 * @brief Overwrite an ident's value with a value comming from storage.
 * @tparam T the data type, whether integer or float.
 * @param id the identifier, whether integer.
 * @param dst the value that will be overwritten by the `src`.
 * @param src the value that will be written to `dest`.
 */
template <class T>
void storeval(ident *id, T &dst, T *src) {

    if(identflags&Idf_Overridden || id->flags&Idf_Override)
    {
        if(id->flags&Idf_Persist)
        {
            // Return error.
            debugcode("Cannot override persistent variable %s", id->name);
            return;
        }
        if(!(id->flags&Idf_Overridden))
        {
            // Save value.
            dst = *src;
            id->flags |= Idf_Overridden;
        }
    }
    if(id->flags&Idf_Overridden)
    {
        // Reset value.
        id->flags &= ~Idf_Overridden;
    }
}

void setvar(const char *name, int i, bool dofunc, bool doclamp)
{
    ident *id = getvar(Id_Var, name);
    if(!id)
    {
        return;
    }

    storeval(id, id->overrideval.i, id->storage.i);
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
    ident *id = getvar(Id_FloatVar, name);
    if(!id)
    {
        return;
    }

    storeval(id, id->overrideval.f, id->storage.f);
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
    ident *id = getvar(Id_StringVar, name);
    if(!id)
    {
        return;
    }

    storevalarray(id, id->overrideval.s, id->storage.s);
    *id->storage.s = newstring(str);
    if(dofunc)
    {
        id->changed();
    }
}
int getvar(const char *name)
{
    ident *id = getvar(Id_Var, name);
    if(!id)
    {
        return 0;
    }
    return *id->storage.i;
}
int getvarmin(const char *name)
{
    ident *id = getvar(Id_Var, name);
    if(!id)
    {
        return 0;
    }
    return id->minval;
}
int getvarmax(const char *name)
{
    ident *id = getvar(Id_Var, name);
    if(!id)
    {
        return 0;
    }
    return id->maxval;
}
float getfvarmin(const char *name)
{
    ident *id = getvar(Id_FloatVar, name);
    if(!id)
    {
        return 0;
    }
    return id->minvalf;
}
float getfvarmax(const char *name)
{
    ident *id = getvar(Id_FloatVar, name);
    if(!id)
    {
        return 0;
    }
    return id->maxvalf;
}

bool identexists(const char *name)
{
    return (idents.end() != idents.find(name));
}

ident *getident(const char *name)
{
    const auto itr = idents.find(name);
    if(itr != idents.end())
    {
        return &(*(itr)).second;
    }
    return nullptr;
}

void touchvar(const char *name)
{
    const auto itr = idents.find(name);
    if(itr != idents.end())
    {
        ident* id = &(*(itr)).second;
        switch(id->type)
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
}

const char *getalias(const char *name)
{
    ident *i = nullptr;
    const auto itr = idents.find(name);
    if(itr != idents.end())
    {
        i = &(*(itr)).second;
    }
    return i && i->type==Id_Alias && (i->index >= Max_Args || aliasstack->usedargs&(1<<i->index)) ? i->getstr() : "";
}

int clampvar(bool hex, std::string name, int val, int minval, int maxval)
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
    debugcode(hex ?
            (minval <= 255 ? "valid range for %s is %d..0x%X" : "valid range for %s is 0x%X..0x%X") :
            "valid range for %s is %d..%d",
        name.c_str(), minval, maxval);
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
        storeval(id, id->overrideval.i, id->storage.i);
        if(val < id->minval || val > id->maxval)
        {
            val = clampvar(id->flags&Idf_Hex, std::string(id->name), val, id->minval, id->maxval);
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

float clampfvar(std::string name, float val, float minval, float maxval)
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
    debugcode("valid range for %s is %s..%s", name.c_str(), floatstr(minval), floatstr(maxval));
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
        storeval(id, id->overrideval.f, id->storage.f);
        if(val < id->minvalf || val > id->maxvalf)
        {
            val = clampfvar(id->name, val, id->minvalf, id->maxvalf);
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
        storevalarray(id, id->overrideval.s, id->storage.s);
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
    /**
     * @brief The argmask is of type unsigned int, but it acts as a bitmap
     * corresponding to each argument passed.
     *
     * For parameters of type i, b, f, F, t, T, E, N, D the value is set to 0.
     * For parameters named S s e r $ (symbolic values) the value is set to 1.
     */
    uint argmask = 0;

    // The number of arguments in format string.
    int numargs = 0;
    bool limit = true;
    if(args)
    {
        /**
         * @brief Parse the format string *args, and set various
         * properties about the parameters of the indent. Usually up to
         * Max_CommandArgs are allowed in a single command. These values must
         * be set to pass to the ident::ident() constructor.
         *
         * Arguments are passed to the argmask bit by bit. A command that is
         * "iSsiiSSi" will get an argmask of 00000000000000000000000001100110.
         * Theoretically the argmask can accommodate up to a 32 parameter
         * command.
         *
         * For example, a command called "createBoxAtCoordinates x y" with two
         * parameters could be invoked by calling "createBoxAtCoordinates 2 5"
         * and the argstring would be "ii".
         *
         * Note that boolean is actually integral-typed in cubescript. Booleans
         * are an integer in functions because a function with a parameter
         * string "bb" still will look like foo(int *, int *).
         */

        for(const char *fmt = args; *fmt; fmt++)
        {
            switch(*fmt)
            {
                //normal arguments
                case 'i': // (int *)
                case 'b': // (int *) refers to boolean
                case 'f': // (float *)
                case 'F':
                case 't':
                case 'T':
                case 'E':
                case 'N':
                case 'D': // (int *)
                {
                    if(numargs < Max_Args)
                    {
                        numargs++;
                    }
                    break;
                }
                //special arguments: these will flip the corresponding bit in the argmask
                case 'S':
                case 's': // (char *) refers to string
                case 'e': // (uint *)
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
                //these are formatting flags, they do not add to numargs
                case '1':
                case '2':
                case '3':
                case '4':
                {
                    //shift the argstring down by the value 1,2,3,4 minus the char for 0 (so int 1 2 3 4) then down one extra element
                    if(numargs < Max_Args)
                    {
                        fmt -= *fmt-'0'+1;
                    }
                    break;
                }
                //these flags determine whether the limit flag is set, they do not add to numargs
                //the limit flag limits the number of parameters to Max_CommandArgs
                case 'C':
                case 'V': // (tagval *args, int numargs)
                {
                    limit = false;
                    break;
                }
                //kill the engine if one of the above parameter types above are not used
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
    //calls the ident() constructor to create a new ident object, then addident adds it to the
    //global hash table
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
                if(*(++p))
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
        int c = *(src++);
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
                    *dst++ = '^';
                    *dst++ = 'f';
                    break;
                }
                default:
                {
                    *(dst++) = e;
                    break;
                }
            }
        }
        else
        {
            *(dst++) = c;
        }
    }
    *dst = '\0';
    return dst - start;
}

static char *conc(std::vector<char> &buf, const tagval *v, int n, bool space, const char *prefix = nullptr, int prefixlen = 0)
{
    if(prefix)
    {
        for(int i = 0; i < prefixlen; ++i)
        {
            buf.push_back(prefix[i]);
        }
        if(space && n)
        {
            buf.push_back(' ');
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
        for(int i = 0; i < len; ++i)
        {
            buf.push_back(s[i]);
        }
        if(i == n-1)
        {
            break;
        }
        if(space)
        {
            buf.push_back(' ');
        }
    }
    buf.push_back('\0');
    return buf.data();
}

static char *conc(const tagval *v, int n, bool space, const char *prefix, int prefixlen)
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
        std::memcpy(buf, prefix, prefixlen);
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
            std::memcpy(&buf[offset], &numbuf[numoffset], vlen[j]);
            numoffset += vlen[j];
        }
        else if(vlen[j])
        {
            std::memcpy(&buf[offset], v[j].s, vlen[j]);
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

char *conc(const tagval *v, int n, bool space)
{
    return conc(v, n, space, nullptr, 0);
}

char *conc(const tagval *v, int n, bool space, const char *prefix)
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
    uint maxlen = (end-p) + 1;

    stridx = (stridx + 1)%4;
    std::vector<char> &buf = strbuf[stridx];
    if(buf.capacity() < maxlen)
    {
        buf.reserve(maxlen);
    }
    s.str = buf.data();
    s.len = unescapestring(buf.data(), p, end);
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

/**
 * @brief The functions compares the `type` with other types from an enum. Types
 * lower than Value_Any are primitives, so we are not interested in those.
 * @param type the kind of token, such as code, macro, indent, csrting, etc.
 * @param defaultret the default return type flag to return (either null, integer, or float).
 * @return int the type flag
 */
int ret_code(int type, int defaultret)
{
    // If value is bigger than a primitive:
    if(type >= Value_Any)
    {
        // If the value type is CString:
        if(type == Value_CString)
        {
            // Return string type flag.
            return Ret_String;
        }

        // Return the default type flag.
        return defaultret;
    }
    // Return type * 2 ^ Core_Ret.
    return type << Code_Ret;
}

int ret_code_int(int type)
{
    return ret_code(type, Ret_Integer);
}

int ret_code_float(int type)
{
    return ret_code(type, Ret_Float);
}

int ret_code_any(int type)
{
    return ret_code(type, 0);
}

/**
 * @brief Return a string type flag for any type that is not a primitive.
 * @param type the kind of token, such as code, macro, indent, csrting, etc.
 * @return int the type flag
 */
int ret_code_string(int type)
{
    // If value is bigger than a primitive:
    if(type >= Value_Any)
    {
        // Return string type flag.
        return Ret_String;
    }

    // Return type * 2 ^ Core_Ret.
    return type << Code_Ret;
}

static void compilestr(std::vector<uint> &code, const char *word, int len, bool macro = false)
{
    if(len <= 3 && !macro)
    {
        uint op = Code_ValI|Ret_String;
        for(int i = 0; i < len; ++i)
        {
            op |= static_cast<uint>(static_cast<uchar>(word[i]))<<((i+1)*8);
        }
        code.push_back(op);
        return;
    }
    code.push_back((macro ? Code_Macro : Code_Val|Ret_String));
    code.back() |= len << 8;
    for(uint i = 0; i < len/sizeof(uint); ++i)
    {
        code.push_back((reinterpret_cast<const uint *>(word))[i]);
    }
    size_t endlen = len%sizeof(uint);
    union
    {
        char c[sizeof(uint)];
        uint u;
    } end;
    end.u = 0;
    std::memcpy(end.c, word + len - endlen, endlen);
    code.push_back(end.u);
}

static void compilestr(std::vector<uint> &code)
{
    code.push_back(Code_ValI|Ret_String);
}

static void compilestr(std::vector<uint> &code, const stringslice &word, bool macro = false)
{
    compilestr(code, word.str, word.len, macro);
}

//compile un-escape string
// removes the escape characters from a c string reference `p` (such as "\")
// and appends it to the execution string referenced to by code. The len/(val/ret/macro) field
// of the code vector is created depending on the state of macro and the length of the string
// passed.
static void compileunescapestring(std::vector<uint> &code, const char *&p, bool macro = false)
{
    p++;
    const char *end = parsestring(p);
    code.emplace_back(macro ? Code_Macro : Code_Val|Ret_String);
    int size = static_cast<int>(end-p)/sizeof(uint) + 1;
    int oldvecsize = code.size();
    for(int i = 0; i < size; ++i)
    {
        code.emplace_back();
    }
    char *buf = reinterpret_cast<char *>(&(code[oldvecsize]));
    int len = unescapestring(buf, p, end);
    std::memset(&buf[len], 0, sizeof(uint) - len%sizeof(uint));
    code.at(oldvecsize-1) |= len<<8;
    p = end;
    if(*p == '\"')
    {
        p++;
    }
}
static void compileint(std::vector<uint> &code, int i = 0)
{
    if(i >= -0x800000 && i <= 0x7FFFFF)
    {
        code.push_back(Code_ValI|Ret_Integer|(i<<8));
    }
    else
    {
        code.push_back(Code_Val|Ret_Integer);
        code.push_back(i);
    }
}

static void compilenull(std::vector<uint> &code)
{
    code.push_back(Code_ValI|Ret_Null);
}

static uint emptyblock[Value_Any][2] =
{
    { Code_Start + 0x100, Code_Exit|Ret_Null },
    { Code_Start + 0x100, Code_Exit|Ret_Integer },
    { Code_Start + 0x100, Code_Exit|Ret_Float },
    { Code_Start + 0x100, Code_Exit|Ret_String }
};

static void compileblock(std::vector<uint> &code)
{
    code.push_back(Code_Empty);
}

static void compilestatements(std::vector<uint> &code, const char *&p, int rettype, int brak = '\0', int prevargs = 0);

static const char *compileblock(std::vector<uint> &code, const char *p, int rettype = Ret_Null, int brak = '\0')
{
    uint start = code.size();
    code.push_back(Code_Block);
    code.push_back(Code_Offset|((start+2)<<8));
    if(p)
    {
        compilestatements(code, p, Value_Any, brak);
    }
    if(code.size() > start + 2)
    {
        code.push_back(Code_Exit|rettype);
        code[start] |= static_cast<uint>(code.size() - (start + 1))<<8;
    }
    else
    {
        code.resize(start);
        code.push_back(Code_Empty|rettype);
    }
    return p;
}

static void compileident(std::vector<uint> &code, ident *id = dummyident)
{
    code.push_back((id->index < Max_Args ? Code_IdentArg : Code_Ident)|(id->index<<8));
}

static void compileident(std::vector<uint> &code, const stringslice &word)
{
    std::string lookupsubstr = std::string(word.str).substr(0, word.len);
    compileident(code, newident(lookupsubstr.c_str(), Idf_Unknown));
}

static void compileint(std::vector<uint> &code, const stringslice &word)
{
    std::string lookupsubstr = std::string(word.str).substr(0, word.len);
    compileint(code, word.len ? parseint(lookupsubstr.c_str()) : 0);
}

static void compilefloat(std::vector<uint> &code, float f = 0.0f)
{
    if(static_cast<int>(f) == f && f >= -0x800000 && f <= 0x7FFFFF)
    {
        code.push_back(Code_ValI|Ret_Float|(static_cast<int>(f)<<8));
    }
    else
    {
        union
        {
            float f;
            uint u;
        } conv;
        conv.f = f;
        code.push_back(Code_Val|Ret_Float);
        code.push_back(conv.u);
    }
}

static void compilefloat(std::vector<uint> &code, const stringslice &word)
{
    compilefloat(code, word.len ? parsefloat(word.str) : 0.0f);
}

bool getbool(const tagval &v)
{
    auto getbool = [] (const char *s)
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
                int val = static_cast<int>(std::strtoul(const_cast<char *>(s), &end, 0));
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
    };

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

static void compileval(std::vector<uint> &code, int wordtype, const stringslice &word = stringslice(nullptr, 0))
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
static bool compilearg(std::vector<uint> &code, const char *&p, int wordtype, int prevargs = Max_Results, stringslice &word = unusedword);

static void compilelookup(std::vector<uint> &code, const char *&p, int ltype, int prevargs = Max_Results)
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
            std::string lookupsubstr = std::string(lookup.str).substr(0, lookup.len);
            ident *id = newident(lookupsubstr.c_str(), Idf_Unknown);
            if(id)
            {
                switch(id->type)
                {
                    case Id_Var:
                    {
                        code.push_back(Code_IntVar|ret_code_int(ltype)|(id->index<<8));
                        switch(ltype)
                        {
                            case Value_Pop:
                            {
                                code.pop_back();
                                break;
                            }
                            case Value_Code:
                            {
                                code.push_back(Code_Compile);
                                break;
                            }
                            case Value_Ident:
                            {
                                code.push_back(Code_IdentU);
                                break;
                            }
                        }
                        return;
                    }
                    case Id_FloatVar:
                    {
                        code.push_back(Code_FloatVar|ret_code_float(ltype)|(id->index<<8));
                        switch(ltype)
                        {
                            case Value_Pop:
                            {
                                code.pop_back();
                                break;
                            }
                            case Value_Code:
                            {
                                code.push_back(Code_Compile);
                                break;
                            }
                            case Value_Ident:
                            {
                                code.push_back(Code_IdentU);
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
                                code.push_back(Code_StrVarM|(id->index<<8));
                                break;
                            }
                            default:
                            {
                                code.push_back(Code_StrVar|ret_code_string(ltype)|(id->index<<8));
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
                                code.push_back((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|(id->index<<8));
                                break;
                            }
                            case Value_CString:
                            case Value_Code:
                            case Value_Ident:
                            {
                                code.push_back((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|Ret_String|(id->index<<8));
                                break;
                            }
                            default:
                            {
                                code.push_back((id->index < Max_Args ? Code_LookupArg : Code_Lookup)|ret_code_string(ltype)|(id->index<<8));
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
                            code.push_back(Code_Enter);
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
                                    code.push_back(Code_Dup|Ret_Float);
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
                        code.push_back(comtype|ret_code_any(ltype)|(id->index<<8));
                        code.push_back((prevargs >= Max_Results ? Code_Exit : Code_ResultArg) | ret_code_any(ltype));
                        goto done;
                    compilecomv:
                        code.push_back(comtype|ret_code_any(ltype)|(numargs<<8)|(id->index<<13));
                        code.push_back((prevargs >= Max_Results ? Code_Exit : Code_ResultArg) | ret_code_any(ltype));
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
            code.push_back(Code_LookupMU);
            break;
        }
        case Value_CString:
        case Value_Code:
        case Value_Ident:
        {
            code.push_back(Code_LookupMU|Ret_String);
            break;
        }
        default:
        {
            code.push_back(Code_LookupU|ret_code_any(ltype));
            break;
        }
    }
done:
    switch(ltype)
    {
        case Value_Pop:
        {
            code.push_back(Code_Pop);
            break;
        }
        case Value_Code:
        {
            code.push_back(Code_Compile);
            break;
        }
        case Value_Cond:
        {
            code.push_back(Code_Cond);
            break;
        }
        case Value_Ident:
        {
            code.push_back(Code_IdentU);
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

static bool compileblockstr(std::vector<uint> &code, const char *str, const char *end, bool macro)
{
    int start = code.size();
    code.push_back(macro ? Code_Macro : Code_Val|Ret_String);
    int size = (end-str)/sizeof(uint)+1;
    int oldvecsize = code.size();
    for(int i = 0; i < size; ++i)
    {
        code.emplace_back();
    }
    char *buf = reinterpret_cast<char *>(&(code[oldvecsize]));
    int len = 0;
    while(str < end)
    {
        int n = std::strcspn(str, "\r/\"@]\0");
        std::memcpy(&buf[len], str, n);
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
                std::memcpy(&buf[len], start, str-start);
                len += str-start;
                break;
            }
            case '/':
                if(str[1] == '/')
                {
                    size_t comment = std::strcspn(str, "\n\0");
                    if (iscubepunct(str[2]))
                    {
                        std::memcpy(&buf[len], str, comment);
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
    std::memset(&buf[len], '\0', sizeof(uint)-len%sizeof(uint));
    code[start] |= len<<8;
    return true;
}

static bool compileblocksub(std::vector<uint> &code, const char *&p, int prevargs)
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
            code.push_back(Code_LookupMU);
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
            std::string lookupsubstr = std::string(lookup.str).substr(0, lookup.len);
            ident *id = newident(lookupsubstr.c_str(), Idf_Unknown);
            if(id)
            {
                switch(id->type)
                {
                    case Id_Var:
                    {
                        code.push_back(Code_IntVar|(id->index<<8));
                        goto done;
                    }
                    case Id_FloatVar:
                    {
                        code.push_back(Code_FloatVar|(id->index<<8));
                        goto done;
                    }
                    case Id_StringVar:
                    {
                        code.push_back(Code_StrVarM|(id->index<<8));
                        goto done;
                    }
                    case Id_Alias:
                    {
                        code.push_back((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|(id->index<<8));
                        goto done;
                    }
                }
            }
            compilestr(code, lookup, true);
            code.push_back(Code_LookupMU);
        done:
            break;
        }
    }
    return true;
}

static void compileblockmain(std::vector<uint> &code, const char *&p, int wordtype, int prevargs)
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
                    code.push_back(Code_Enter);
                }
                if(concs + 2 > Max_Args)
                {
                    code.push_back(Code_ConCW|Ret_String|(concs<<8));
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
                    code.pop_back();
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
            code.push_back(Code_ConCM|ret_code_any(wordtype)|(concs<<8));
            code.push_back(Code_Exit|ret_code_any(wordtype));
        }
        else
        {
            code.push_back(Code_ConCW|ret_code_any(wordtype)|(concs<<8));
        }
    }
    switch(wordtype)
    {
        case Value_Pop:
        {
            if(concs || p-1 > start)
            {
                code.push_back(Code_Pop);
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
                code.push_back(Code_Cond);
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
                code.push_back(Code_Compile);
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
                code.push_back(Code_IdentU);
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
                    code.push_back(Code_Force|(wordtype<<Code_Ret));
                }
            }
            break;
        }
    }
}

static bool compilearg(std::vector<uint> &code, const char *&p, int wordtype, int prevargs, stringslice &word)
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
                code.push_back(Code_Enter);
                compilestatements(code, p, wordtype > Value_Any ? Value_CAny : Value_Any, ')');
                code.push_back(Code_Exit|ret_code_any(wordtype));
            }
            else
            {
                uint start = code.size();
                compilestatements(code, p, wordtype > Value_Any ? Value_CAny : Value_Any, ')', prevargs);
                if(code.size() > start)
                {
                    code.push_back(Code_ResultArg|ret_code_any(wordtype));
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
                    code.push_back(Code_Pop);
                    break;
                }
                case Value_Cond:
                {
                    code.push_back(Code_Cond);
                    break;
                }
                case Value_Code:
                {
                    code.push_back(Code_Compile);
                    break;
                }
                case Value_Ident:
                {
                    code.push_back(Code_IdentU);
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

static void compilestatements(std::vector<uint> &code, const char *&p, int rettype, int brak, int prevargs)
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
                        std::string lookupsubstr = std::string(idname.str).substr(0, idname.len);
                        ident *id = newident(lookupsubstr.c_str(), Idf_Unknown);
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
                                    code.push_back((id->index < Max_Args ? Code_AliasArg : Code_Alias)|(id->index<<8));
                                    goto endstatement;
                                }
                                case Id_Var:
                                {
                                    if(!(more = compilearg(code, p, Value_Integer, prevargs)))
                                    {
                                        compileint(code);
                                    }
                                    code.push_back(Code_IntVar1|(id->index<<8));
                                    goto endstatement;
                                }
                                case Id_FloatVar:
                                {
                                    if(!(more = compilearg(code, p, Value_Float, prevargs)))
                                    {
                                        compilefloat(code);
                                    }
                                    code.push_back(Code_FloatVar1|(id->index<<8));
                                    goto endstatement;
                                }
                                case Id_StringVar:
                                {
                                    if(!(more = compilearg(code, p, Value_CString, prevargs)))
                                    {
                                        compilestr(code);
                                    }
                                    code.push_back(Code_StrVar1|(id->index<<8));
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
                    code.push_back(Code_AliasU);
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
            code.push_back(Code_CallU|(numargs<<8));
        }
        else
        {
            ident *id = nullptr;
            std::string lookupsubstr = std::string(idname.str).substr(0, idname.len);
            const auto itr = idents.find(lookupsubstr);
            if(itr != idents.end())
            {
                id = &(*(itr)).second;
            }
            if(!id)
            {
                if(!checknumber(idname.str))
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
                    int val = static_cast<int>(std::strtoul(idname.str, &end, 0));
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
                code.push_back(Code_Result);
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
                        code.push_back((id->index < Max_Args ? Code_CallArg : Code_Call)|(numargs<<8)|(id->index<<13));
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
                                            code.push_back(Code_ConC|Ret_String|(numconc<<8));
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
                                        code.push_back(Code_Dup|Ret_Float);
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
                                            code.push_back(Code_Pop);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                        code.push_back(comtype|ret_code_any(rettype)|(id->index<<8));
                        break;
                    compilecomv:
                        code.push_back(comtype|ret_code_any(rettype)|(numargs<<8)|(id->index<<13));
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
                        code.push_back(Code_Local|(numargs<<8));
                        break;
                    }
                    case Id_Do:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_Code, prevargs);
                        }
                        code.push_back((more ? Code_Do : Code_Null) | ret_code_any(rettype));
                        break;
                    }
                    case Id_DoArgs:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_Code, prevargs);
                        }
                        code.push_back((more ? Code_DoArgs : Code_Null) | ret_code_any(rettype));
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
                            code.push_back(Code_Null | ret_code_any(rettype));
                        }
                        else
                        {
                            int start1 = code.size();
                            more = compilearg(code, p, Value_Code, prevargs+1);
                            if(!more)
                            {
                                code.push_back(Code_Pop);
                                code.push_back(Code_Null | ret_code_any(rettype));
                            }
                            else
                            {
                                int start2 = code.size();
                                more = compilearg(code, p, Value_Code, prevargs+2);
                                uint inst1 = code[start1], op1 = inst1&~Code_RetMask, len1 = start2 - (start1+1);
                                if(!more)
                                {
                                    if(op1 == (Code_Block|(len1<<8)))
                                    {
                                        code[start1] = (len1<<8) | Code_JumpFalse;
                                        code[start1+1] = Code_EnterResult;
                                        code[start1+len1] = (code[start1+len1]&~Code_RetMask) | ret_code_any(rettype);
                                        break;
                                    }
                                    compileblock(code);
                                }
                                else
                                {
                                    uint inst2 = code[start2], op2 = inst2&~Code_RetMask, len2 = code.size() - (start2+1);
                                    if(op2 == (Code_Block|(len2<<8)))
                                    {
                                        if(op1 == (Code_Block|(len1<<8)))
                                        {
                                            code[start1] = ((start2-start1)<<8) | Code_JumpFalse;
                                            code[start1+1] = Code_EnterResult;
                                            code[start1+len1] = (code[start1+len1]&~Code_RetMask) | ret_code_any(rettype);
                                            code[start2] = (len2<<8) | Code_Jump;
                                            code[start2+1] = Code_EnterResult;
                                            code[start2+len2] = (code[start2+len2]&~Code_RetMask) | ret_code_any(rettype);
                                            break;
                                        }
                                        else if(op1 == (Code_Empty|(len1<<8)))
                                        {
                                            code[start1] = Code_Null | (inst2&Code_RetMask);
                                            code[start2] = (len2<<8) | Code_JumpTrue;
                                            code[start2+1] = Code_EnterResult;
                                            code[start2+len2] = (code[start2+len2]&~Code_RetMask) | ret_code_any(rettype);
                                            break;
                                        }
                                    }
                                }
                                code.push_back(Code_Com|ret_code_any(rettype)|(id->index<<8));
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
                        code.push_back((more ? Code_Result : Code_Null) | ret_code_any(rettype));
                        break;
                    }
                    case Id_Not:
                    {
                        if(more)
                        {
                            more = compilearg(code, p, Value_CAny, prevargs);
                        }
                        code.push_back((more ? Code_Not : Code_True) | ret_code_any(rettype));
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
                            code.push_back((id->type == Id_And ? Code_True : Code_False) | ret_code_any(rettype));
                        }
                        else
                        {
                            numargs++;
                            int start = code.size(),
                                end = start;
                            while(numargs < Max_Args)
                            {
                                more = compilearg(code, p, Value_Cond, prevargs+numargs);
                                if(!more)
                                {
                                    break;
                                }
                                numargs++;
                                if((code[end]&~Code_RetMask) != (Code_Block|(static_cast<uint>(code.size()-(end+1))<<8)))
                                {
                                    break;
                                }
                                end = code.size();
                            }
                            if(more)
                            {
                                while(numargs < Max_Args && (more = compilearg(code, p, Value_Cond, prevargs+numargs)))
                                {
                                    numargs++;
                                }
                                code.push_back(Code_ComV|ret_code_any(rettype)|(numargs<<8)|(id->index<<13));
                            }
                            else
                            {
                                uint op = id->type == Id_And ? Code_JumpResultFalse : Code_JumpResultTrue;
                                code.push_back(op);
                                end = code.size();
                                while(start+1 < end)
                                {
                                    uint len = code[start]>>8;
                                    code[start] = ((end-(start+1))<<8) | op;
                                    code[start+1] = Code_Enter;
                                    code[start+len] = (code[start+len]&~Code_RetMask) | ret_code_any(rettype);
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
                            code.push_back(Code_Print|(id->index<<8));
                        }
                        else if(!(id->flags&Idf_Hex) || !(more = compilearg(code, p, Value_Integer, prevargs+1)))
                        {
                            code.push_back(Code_IntVar1|(id->index<<8));
                        }
                        else if(!(more = compilearg(code, p, Value_Integer, prevargs+2)))
                        {
                            code.push_back(Code_IntVar2|(id->index<<8));
                        }
                        else
                        {
                            code.push_back(Code_IntVar3|(id->index<<8));
                        }
                        break;
                    }
                    case Id_FloatVar:
                    {
                        if(!(more = compilearg(code, p, Value_Float, prevargs)))
                        {
                            code.push_back(Code_Print|(id->index<<8));
                        }
                        else
                        {
                            code.push_back(Code_FloatVar1|(id->index<<8));
                        }
                        break;
                    }
                    case Id_StringVar:
                    {
                        if(!(more = compilearg(code, p, Value_CString, prevargs)))
                        {
                            code.push_back(Code_Print|(id->index<<8));
                        }
                        else
                        {
                            do
                            {
                                ++numargs;
                            } while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs)));
                            if(numargs > 1)
                            {
                                code.push_back(Code_ConC|Ret_String|(numargs<<8));
                            }
                            code.push_back(Code_StrVar1|(id->index<<8));
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

static void compilemain(std::vector<uint> &code, const char *p, int rettype = Value_Any)
{
    code.push_back(Code_Start);
    compilestatements(code, p, Value_Any);
    code.push_back(Code_Exit|(rettype < Value_Any ? rettype<<Code_Ret : 0));
}

uint *compilecode(const char *p)
{
    std::vector<uint> buf;
    buf.reserve(64);
    compilemain(buf, p);
    uint *code = new uint[buf.size()];
    std::memcpy(code, buf.data(), buf.size()*sizeof(uint));
    code[0] += 0x100;
    return code;
}

static const uint *forcecode(tagval &v)
{
    if(v.type != Value_Code)
    {
        std::vector<uint> buf;
        buf.reserve(64);
        compilemain(buf, v.getstr());
        freearg(v);
        uint * arr = new uint[buf.size()];
        std::memcpy(arr, buf.data(), buf.size()*sizeof(uint));
        v.setcode(arr+1);
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

void printvar(const ident *id, int i)
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

void printfvar(const ident *id, float f)
{
    conoutf("%s = %s", id->name, floatstr(f));
}

void printsvar(const ident *id, const char *s)
{
    conoutf(std::strchr(s, '"') ? "%s = [%s]" : "%s = \"%s\"", id->name, s);
}

void printvar(const ident *id)
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
    std::memcpy(dst, src, len*sizeof(uint));
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

/**
 * @brief Returns the pointer to a string or integer argument.
 * @param id the identifier, whether a string or integer.
 * @param args the array of arguments.
 * @param n the n-th argument to return.
 * @param offset the offset for accessing and returning the n-th argument.
 * @return void* the pointer to the string or integer.
 */
void* arg(const ident *id, tagval args[], int n, int offset = 0)
{
    if(id->argmask&(1<<n))
    {
        return reinterpret_cast<void *>(args[n + offset].s);
    }
    return  reinterpret_cast<void *>(&args[n + offset].i);
}

/**
 * @brief Takes a number `n` and type-mangles the id->fun field to
 * whatever length function is desired. E.g. callcom(id, args, 6) takes the function
 * pointer id->fun and changes its type to comfun6 (command function w/ 6 args).
 * Each argument is then described by the arg() function for each argument slot.
 *
 * the id->fun member is a pointer to a free function with the signature
 * void foo(ident *), which is then reinterpret_casted to a function with parameters
 * that it originally had before being stored as a generic function pointer.
 *
 * @param id the identifier for the type of command.
 * @param args the command arguments.
 * @param n the n-th argument to return.
 * @param offset the offset for accessing and returning the n-th argument.
 */
void callcom(const ident *id, tagval args[], int n, int offset=0)
{
    /**
     * @brief Return the n-th argument. The lambda expression captures the `id`
     * and `args` so only the parameter number `n` needs to be passed.
     */
    auto a = [id, args, offset](int n)
    {
        return arg(id, args, n, offset);
    };

    switch(n)
    {
        case 0: reinterpret_cast<comfun>(id->fun)(); break;
        case 1: reinterpret_cast<comfun1>(id->fun)(a(0)); break;
        case 2: reinterpret_cast<comfun2>(id->fun)(a(0), a(1)); break;
        case 3: reinterpret_cast<comfun3>(id->fun)(a(0), a(1), a(2)); break;
        case 4: reinterpret_cast<comfun4>(id->fun)(a(0), a(1), a(2), a(3)); break;
        case 5: reinterpret_cast<comfun5>(id->fun)(a(0), a(1), a(2), a(3), a(4)); break;
        case 6: reinterpret_cast<comfun6>(id->fun)(a(0), a(1), a(2), a(3), a(4), a(5)); break;
        case 7: reinterpret_cast<comfun7>(id->fun)(a(0), a(1), a(2), a(3), a(4), a(5), a(6)); break;
        case 8: reinterpret_cast<comfun8>(id->fun)(a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7)); break;
        case 9: reinterpret_cast<comfun9>(id->fun)(a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7), a(8)); break;
        case 10: reinterpret_cast<comfun10>(id->fun)(a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7), a(8), a(9)); break;
        case 11: reinterpret_cast<comfun11>(id->fun)(a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7), a(8), a(9), a(10)); break;
        case 12: reinterpret_cast<comfun12>(id->fun)(a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7), a(8), a(9), a(10), a(11)); break;
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
                std::vector<char> buf;
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
    callcom(id, args, i);

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

#define UNDOARGS \
    identstack argstack[Max_Args]; \
    IdentLink *prevstack = aliasstack; \
    IdentLink aliaslink; \
    for(int undos = 0; prevstack != &noalias; prevstack = prevstack->next) \
    { \
        if(prevstack->usedargs & undoflag) \
        { \
            ++undos; \
        } \
        else if(undos > 0) \
        { \
            --undos; \
        } \
        else \
        { \
            prevstack = prevstack->next; \
            for(int argmask = aliasstack->usedargs & ~undoflag, i = 0; argmask; argmask >>= 1, i++) \
            { \
                if(argmask&1) \
                { \
                    undoarg(*identmap[i], argstack[i]); \
                } \
            } \
            aliaslink.id = aliasstack->id; \
            aliaslink.next = aliasstack; \
            aliaslink.usedargs = undoflag | prevstack->usedargs; \
            aliaslink.argstack = prevstack->argstack; \
            aliasstack = &aliaslink; \
            break; \
        } \
    } \


#define REDOARGS \
    if(aliasstack == &aliaslink) \
    { \
        prevstack->usedargs |= aliaslink.usedargs & ~undoflag; \
        aliasstack = aliaslink.next; \
        for(int argmask = aliasstack->usedargs & ~undoflag, i = 0; argmask; argmask >>= 1, i++) \
        { \
            if(argmask&1) \
            { \
                redoarg(*identmap[i], argstack[i]); \
            } \
        } \
    }

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
            // For Code_Null cases, set results to null, empty, or 0 values.
            case Code_Null|Ret_Null:
            {
                freearg(result);
                result.setnull();
                continue;
            }
            case Code_Null|Ret_String:
            {
                freearg(result);
                result.setstr(newstring(""));
                continue;
            }
            case Code_Null|Ret_Integer:
            {
                freearg(result);
                result.setint(0);
                continue;
            }
            case Code_Null|Ret_Float:
            {
                freearg(result);
                result.setfloat(0.0f);
                continue;
            }
            // For Code_False cases, set results to 0 values.
            case Code_False|Ret_String:
            {
                freearg(result);
                result.setstr(newstring("0"));
                continue;
            }
            case Code_False|Ret_Null: // Null case left empty intentionally.
            case Code_False|Ret_Integer:
            {
                freearg(result);
                result.setint(0);
                continue;
            }
            case Code_False|Ret_Float:
            {
                freearg(result);
                result.setfloat(0.0f);
                continue;
            }
            // For Code_False cases, set results to 1 values.
            case Code_True|Ret_String:
            {
                freearg(result);
                result.setstr(newstring("1"));
                continue;
            }
            case Code_True|Ret_Null: // Null case left empty intentionally.
            case Code_True|Ret_Integer:
            {
                freearg(result);
                result.setint(1);
                continue;
            }
            case Code_True|Ret_Float:
            {
                freearg(result);
                result.setfloat(1.0f);
                continue;
            }
            // For Code_Not cases, negate values (flip 0's and 1's).
            case Code_Not|Ret_String:
            {
                freearg(result);
                --numargs;
                result.setstr(newstring(getbool(args[numargs]) ? "0" : "1"));
                freearg(args[numargs]);
                continue;
            }
            case Code_Not|Ret_Null: // Null case left empty intentionally.
            case Code_Not|Ret_Integer:
            {
                freearg(result);
                --numargs;
                result.setint(getbool(args[numargs]) ? 0 : 1);
                freearg(args[numargs]);
                continue;
            }
            case Code_Not|Ret_Float:
            {
                freearg(result);
                --numargs;
                result.setfloat(getbool(args[numargs]) ? 0.0f : 1.0f);
                freearg(args[numargs]);
                continue;
            }
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
                int numlocals = op>>8,
                                offset = numargs-numlocals;
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
            {
                freearg(result);
                runcode(args[--numargs].code, result);
                freearg(args[numargs]);
                forcearg(result, op&Code_RetMask);
                continue;
            }
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
                const char * codearr = reinterpret_cast<const char *>(code);
                //char * str = newstring(codearr, len);
                //copystring(new char[len+1], codearr, len+1);
                char * str = new char[len+1];
                std::memcpy(str, codearr, len*sizeof(uchar));
                str[len] = 0;

                args[numargs++].setstr(str);
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
                std::vector<uint> buf;
                switch(arg.type)
                {
                    case Value_Integer:
                    {
                        buf.reserve(8);
                        buf.push_back(Code_Start);
                        compileint(buf, arg.i);
                        buf.push_back(Code_Result);
                        buf.push_back(Code_Exit);
                        break;
                    }
                    case Value_Float:
                    {
                        buf.reserve(8);
                        buf.push_back(Code_Start);
                        compilefloat(buf, arg.f);
                        buf.push_back(Code_Result);
                        buf.push_back(Code_Exit);
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
                        buf.push_back(Code_Start);
                        compilenull(buf);
                        buf.push_back(Code_Result);
                        buf.push_back(Code_Exit);
                        break;
                    }
                }
                uint * arr = new uint[buf.size()];
                std::memcpy(arr, buf.data(), buf.size()*sizeof(uint));
                arg.setcode(arr+1);
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
                            std::vector<uint> buf;
                            buf.reserve(64);
                            compilemain(buf, arg.s);
                            freearg(arg);
                            uint * arr = new uint[buf.size()];
                            std::memcpy(arr, buf.data(), buf.size()*sizeof(uint));
                            arg.setcode(arr + 1);
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
                    pusharg(*id, NullVal(), aliasstack->argstack[id->index]);
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
                    pusharg(*id, NullVal(), aliasstack->argstack[id->index]);
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
                    const auto itr = idents.find(arg.s); \
                    if(itr != idents.end()) \
                    { \
                        ident* id = &(*(itr)).second; \
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
                    debugcode("unknown alias lookup(u): %s", arg.s); \
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
            {
                LOOKUPU(arg.setint(id->getint()),
                        arg.setint(static_cast<int>(strtoul(*id->storage.s, nullptr, 0))),
                        arg.setint(*id->storage.i),
                        arg.setint(static_cast<int>(*id->storage.f)),
                        arg.setint(0));
            }
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
                args[numargs++].setint(static_cast<int>(strtoul((*identmap[op>>8]->storage.s), nullptr, 0)));
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
            case Code_Com|Ret_Null:
            case Code_Com|Ret_String:
            case Code_Com|Ret_Float:
            case Code_Com|Ret_Integer:
            {
                ident *id = identmap[op>>8];
                int offset = numargs-id->numargs;
                forcenull(result);
                callcom(id, args, id->numargs, offset);
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
                callcom(id, args, id->numargs, offset);
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }

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
                    std::vector<char> buf;
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
                //==================================================== CALLALIAS
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
                ident *id = nullptr;
                const auto itr = idents.find(idarg.s);
                if(itr != idents.end())
                {
                    id = &(*(itr)).second;
                }
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
    std::vector<uint> code;
    code.reserve(64);
    compilemain(code, p, Value_Any);
    runcode(code.data()+1, result);
    if(static_cast<int>(code[0]) >= 0x100)
    {
        uint * arr = new uint[code.size()];
        std::memcpy(arr, code.data(), code.size()*sizeof(uint));
    }
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
                    std::memcpy(buf, args, numargs*sizeof(tagval)); //copy numargs number of args from passed args tagval
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
#undef CALLALIAS
//==============================================================================

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
    std::vector<uint> code;
    code.reserve(64);
    compilemain(code, p, Value_Integer);
    tagval result;
    runcode(code.data()+1, result);
    if(static_cast<int>(code[0]) >= 0x100)
    {
        uint * arr = new uint[code.size()];
        std::memcpy(arr, code.data(), code.size()*sizeof(uint));
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
    ident *id = nullptr;
    const auto itr = idents.find(name);
    if(itr != idents.end())
    {
        id = &(*(itr)).second;
    }
    return id ? execute(id, nullptr, 0, lookup) : noid;
}

bool executebool(const uint *code)
{
    tagval result;
    runcode(code, result);
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

static void doargs(uint *body)
{
    if(aliasstack != &noalias)
    {
        UNDOARGS
        executeret(body, *commandret);
        REDOARGS
    }
    else
    {
        executeret(body, *commandret);
    }
}

std::unordered_map<std::string, DefVar> defvars;

void initcscmds()
{
    addcommand("local", static_cast<identfun>(nullptr), nullptr, Id_Local);

    addcommand("defvar", reinterpret_cast<identfun>(+[] (char *name, int *min, int *cur, int *max, char *onchange)
    {
        {
            const auto itr = idents.find(name);
            if(itr != idents.end())
            {
                debugcode("cannot redefine %s as a variable", name);
                return;
            }
            name = newstring(name);
            auto insert = defvars.insert( { std::string(name), DefVar() } );
            DefVar &def = (*(insert.first)).second;
            def.name = name;
            def.onchange = onchange[0] ? compilecode(onchange) : nullptr;
            def.i = variable(name, *min, *cur, *max, &def.i, def.onchange ? DefVar::changed : nullptr, 0);
        };
    }), "siiis", Id_Command);
    addcommand("defvarp", reinterpret_cast<identfun>(+[] (char *name, int *min, int *cur, int *max, char *onchange)
    {
        {
            const auto itr = idents.find(name);
            if(itr != idents.end())
            {
                debugcode("cannot redefine %s as a variable", name);
                return;
            }
            name = newstring(name);
            auto insert = defvars.insert( { std::string(name), DefVar() } );
            DefVar &def = (*(insert.first)).second;
            def.name = name;
            def.onchange = onchange[0] ? compilecode(onchange) : nullptr;
            def.i = variable(name, *min, *cur, *max, &def.i, def.onchange ? DefVar::changed : nullptr, Idf_Persist);
        };
    }), "siiis", Id_Command);
    addcommand("deffvar", reinterpret_cast<identfun>(+[] (char *name, float *min, float *cur, float *max, char *onchange)
    {
        {
            const auto itr = idents.find(name);
            if(itr != idents.end())
            {
                debugcode("cannot redefine %s as a variable", name);
                return;
            }
            name = newstring(name);
            auto insert = defvars.insert( { std::string(name), DefVar() } );
            DefVar &def = (*(insert.first)).second;
            def.name = name;
            def.onchange = onchange[0] ? compilecode(onchange) : nullptr;
            def.f = fvariable(name, *min, *cur, *max, &def.f, def.onchange ? DefVar::changed : nullptr, 0);
        };
    }), "sfffs", Id_Command);
    addcommand("deffvarp", reinterpret_cast<identfun>(+[] (char *name, float *min, float *cur, float *max, char *onchange)
    {
        {
            const auto itr = idents.find(name);
            if(itr != idents.end())
            {
                debugcode("cannot redefine %s as a variable", name);
                return;
            }
            name = newstring(name);
            auto insert = defvars.insert( { std::string(name), DefVar() } );
            DefVar &def = (*(insert.first)).second;
            def.name = name;
            def.onchange = onchange[0] ? compilecode(onchange) : nullptr;
            def.f = fvariable(name, *min, *cur, *max, &def.f, def.onchange ? DefVar::changed : nullptr, Idf_Persist);
        };
    }), "sfffs", Id_Command);
    addcommand("defsvar", reinterpret_cast<identfun>(+[] (char *name, char *cur, char *onchange)
    {
        {
            const auto itr = idents.find(name);
            if(itr != idents.end())
            {
                debugcode("cannot redefine %s as a variable", name);
                return;
            }
            name = newstring(name);
            auto insert = defvars.insert( { std::string(name), DefVar() } );
            DefVar &def = (*(insert.first)).second;
            def.name = name; def.onchange = onchange[0] ? compilecode(onchange) : nullptr;
            def.s = svariable(name, cur, &def.s, def.onchange ? DefVar::changed : nullptr, 0);
        };
    }), "sss", Id_Command);
    addcommand("defsvarp", reinterpret_cast<identfun>(+[] (char *name, char *cur, char *onchange)
    {
        {
            const auto itr = idents.find(std::string(name));
            if(itr != idents.end())
            {
                debugcode("cannot redefine %s as a variable", name); return;
            }
            name = newstring(name);
            auto insert = defvars.insert( { std::string(name), DefVar() } );
            DefVar &def = (*(insert.first)).second;
            def.name = name;
            def.onchange = onchange[0] ? compilecode(onchange) : nullptr;
            def.s = svariable(name, cur, &def.s, def.onchange ? DefVar::changed : nullptr, Idf_Persist);
        };
    }), "sss", Id_Command);
    addcommand("getvarmin", reinterpret_cast<identfun>(+[] (char *s) { intret(getvarmin(s)); }), "s", Id_Command);
    addcommand("getfvarmin", reinterpret_cast<identfun>(+[] (char *s) { floatret(getfvarmin(s)); }), "s", Id_Command);
    addcommand("getfvarmax", reinterpret_cast<identfun>(+[] (char *s) { floatret(getfvarmax(s)); }), "s", Id_Command);
    addcommand("identexists", reinterpret_cast<identfun>(+[] (char *s) { intret(identexists(s) ? 1 : 0); }), "s", Id_Command);
    addcommand("getalias", reinterpret_cast<identfun>(+[] (char *s) { result(getalias(s)); }), "s", Id_Command);

    addcommand("nodebug", reinterpret_cast<identfun>(+[] (uint *body){nodebug++; executeret(body, *commandret); nodebug--;}), "e", Id_Command);
    addcommand("push", reinterpret_cast<identfun>(pushcmd), "rTe", Id_Command);
    addcommand("alias", reinterpret_cast<identfun>(+[] (const char *name, tagval *v){ setalias(name, *v); v->type = Value_Null;}), "sT", Id_Command);
    addcommand("resetvar", reinterpret_cast<identfun>(resetvar), "s", Id_Command);
    addcommand("doargs", reinterpret_cast<identfun>(doargs), "e", Id_DoArgs);
}

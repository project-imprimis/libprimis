// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include "engine.h"

hashnameset<ident> idents; // contains ALL vars/commands/aliases
vector<ident *> identmap;
ident *dummyident = NULL;

int identflags = 0;

enum
{
    Max_Args = 25,
    Max_Results = 7,
    Max_CommandArgs = 12
};

VARN(numargs, _numargs, Max_Args, 0, 0);

static inline void freearg(tagval &v)
{
    switch(v.type)
    {
        case Value_String: delete[] v.s; break;
        case Value_Code: if(v.code[-1] == Code_Start) delete[] (uchar *)&v.code[-1]; break;
    }
}

static inline void forcenull(tagval &v)
{
    switch(v.type)
    {
        case Value_Null: return;
    }
    freearg(v);
    v.setnull();
}

static inline float forcefloat(tagval &v)
{
    float f = 0.0f;
    switch(v.type)
    {
        case Value_Integer: f = v.i; break;
        case Value_String: case Value_Macro: case Value_CString: f = parsefloat(v.s); break;
        case Value_Float: return v.f;
    }
    freearg(v);
    v.setfloat(f);
    return f;
}

static inline int forceint(tagval &v)
{
    int i = 0;
    switch(v.type)
    {
        case Value_Float: i = v.f; break;
        case Value_String: case Value_Macro: case Value_CString: i = parseint(v.s); break;
        case Value_Integer: return v.i;
    }
    freearg(v);
    v.setint(i);
    return i;
}

static inline const char *forcestr(tagval &v)
{
    const char *s = "";
    switch(v.type)
    {
        case Value_Float: s = floatstr(v.f); break;
        case Value_Integer: s = intstr(v.i); break;
        case Value_Macro: case Value_CString: s = v.s; break;
        case Value_String: return v.s;
    }
    freearg(v);
    v.setstr(newstring(s));
    return s;
}

static inline void forcearg(tagval &v, int type)
{
    switch(type)
    {
        case Ret_String: if(v.type != Value_String) forcestr(v); break;
        case Ret_Integer: if(v.type != Value_Integer) forceint(v); break;
        case Ret_Float: if(v.type != Value_Float) forcefloat(v); break;
    }
}

void tagval::cleanup()
{
    freearg(*this);
}

static inline void freeargs(tagval *args, int &oldnum, int newnum)
{
    for(int i = newnum; i < oldnum; i++) freearg(args[i]);
    oldnum = newnum;
}

static inline void cleancode(ident &id)
{
    if(id.code)
    {
        id.code[0] -= 0x100;
        if(int(id.code[0]) < 0x100) delete[] id.code;
        id.code = NULL;
    }
}

struct nullval : tagval
{
    nullval() { setnull(); }
} nullval;
tagval noret = nullval, *commandret = &noret;

void clear_command()
{
    ENUMERATE(idents, ident, i,
    {
        if(i.type==Id_Alias)
        {
            DELETEA(i.name);
            i.forcenull();
            DELETEA(i.code);
        }
    });
}

void clearoverride(ident &i)
{
    if(!(i.flags&Idf_Overridden)) return;
    switch(i.type)
    {
        case Id_Alias:
            if(i.valtype==Value_String)
            {
                if(!i.val.s[0]) break;
                delete[] i.val.s;
            }
            cleancode(i);
            i.valtype = Value_String;
            i.val.s = newstring("");
            break;
        case Id_Var:
            *i.storage.i = i.overrideval.i;
            i.changed();
            break;
        case Id_FloatVar:
            *i.storage.f = i.overrideval.f;
            i.changed();
            break;
        case Id_StringVar:
            delete[] *i.storage.s;
            *i.storage.s = i.overrideval.s;
            i.changed();
            break;
    }
    i.flags &= ~Idf_Overridden;
}

void clearoverrides()
{
    ENUMERATE(idents, ident, i, clearoverride(i));
}

static bool initedidents = false;
static vector<ident> *identinits = NULL;

static inline ident *addident(const ident &id)
{
    if(!initedidents)
    {
        if(!identinits) identinits = new vector<ident>;
        identinits->add(id);
        return NULL;
    }
    ident &def = idents.access(id.name, id);
    def.index = identmap.length();
    return identmap.add(&def);
}

static bool initidents()
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
        DELETEP(identinits);
    }
    return true;
}
UNUSED static bool forceinitidents = initidents();

static const char *sourcefile = NULL, *sourcestr = NULL;

static const char *debugline(const char *p, const char *fmt)
{
    if(!sourcestr) return fmt;
    int num = 1;
    const char *line = sourcestr;
    for(;;)
    {
        const char *end = strchr(line, '\n');
        if(!end) end = line + strlen(line);
        if(p >= line && p <= end)
        {
            static string buf;
            if(sourcefile) formatstring(buf, "%s:%d: %s", sourcefile, num, fmt);
            else formatstring(buf, "%d: %s", num, fmt);
            return buf;
        }
        if(!*end) break;
        line = end + 1;
        num++;
    }
    return fmt;
}

static struct identlink
{
    ident *id;
    identlink *next;
    int usedargs;
    identstack *argstack;
} noalias = { NULL, NULL, (1<<Max_Args)-1, NULL }, *aliasstack = &noalias;

VAR(dbgalias, 0, 4, 1000);

static void debugalias()
{
    if(!dbgalias) return;
    int total = 0, depth = 0;
    for(identlink *l = aliasstack; l != &noalias; l = l->next) total++;
    for(identlink *l = aliasstack; l != &noalias; l = l->next)
    {
        ident *id = l->id;
        ++depth;
        if(depth < dbgalias) conoutf(Console_Error, "  %d) %s", total-depth+1, id->name);
        else if(l->next == &noalias) conoutf(Console_Error, depth == dbgalias ? "  %d) %s" : "  ..%d) %s", total-depth+1, id->name);
    }
}

static int nodebug = 0;

static void debugcode(const char *fmt, ...) PRINTFARGS(1, 2);

static void debugcode(const char *fmt, ...)
{
    if(nodebug) return;

    va_list args;
    va_start(args, fmt);
    conoutfv(Console_Error, fmt, args);
    va_end(args);

    debugalias();
}

static void debugcodeline(const char *p, const char *fmt, ...) PRINTFARGS(2, 3);

static void debugcodeline(const char *p, const char *fmt, ...)
{
    if(nodebug) return;

    va_list args;
    va_start(args, fmt);
    conoutfv(Console_Error, debugline(p, fmt), args);
    va_end(args);

    debugalias();
}

ICOMMAND(nodebug, "e", (uint *body), { nodebug++; executeret(body, *commandret); nodebug--; });

void addident(ident *id)
{
    addident(*id);
}

static inline void pusharg(ident &id, const tagval &v, identstack &stack)
{
    stack.val = id.val;
    stack.valtype = id.valtype;
    stack.next = id.stack;
    id.stack = &stack;
    id.setval(v);
    cleancode(id);
}

static inline void poparg(ident &id)
{
    if(!id.stack) return;
    identstack *stack = id.stack;
    if(id.valtype == Value_String) delete[] id.val.s;
    id.setval(*stack);
    cleancode(id);
    id.stack = stack->next;
}

static inline void undoarg(ident &id, identstack &stack)
{
    identstack *prev = id.stack;
    stack.val = id.val;
    stack.valtype = id.valtype;
    stack.next = prev;
    id.stack = prev->next;
    id.setval(*prev);
    cleancode(id);
}

#define UNDOFLAG (1<<Max_Args)
#define UNDOARGS \
    identstack argstack[Max_Args]; \
    identlink *prevstack = aliasstack; \
    identlink aliaslink; \
    for(int undos = 0; prevstack != &noalias; prevstack = prevstack->next) \
    { \
        if(prevstack->usedargs & UNDOFLAG) ++undos; \
        else if(undos > 0) --undos; \
        else \
        { \
            prevstack = prevstack->next; \
            for(int argmask = aliasstack->usedargs & ~UNDOFLAG, i = 0; argmask; argmask >>= 1, i++) if(argmask&1) \
                undoarg(*identmap[i], argstack[i]); \
            aliaslink.id = aliasstack->id; \
            aliaslink.next = aliasstack; \
            aliaslink.usedargs = UNDOFLAG | prevstack->usedargs; \
            aliaslink.argstack = prevstack->argstack; \
            aliasstack = &aliaslink; \
            break; \
        } \
    } \

static inline void redoarg(ident &id, const identstack &stack)
{
    identstack *prev = stack.next;
    prev->val = id.val;
    prev->valtype = id.valtype;
    id.stack = prev;
    id.setval(stack);
    cleancode(id);
}

#define REDOARGS \
    if(aliasstack == &aliaslink) \
    { \
        prevstack->usedargs |= aliaslink.usedargs & ~UNDOFLAG; \
        aliasstack = aliaslink.next; \
        for(int argmask = aliasstack->usedargs & ~UNDOFLAG, i = 0; argmask; argmask >>= 1, i++) if(argmask&1) \
            redoarg(*identmap[i], argstack[i]); \
    }

ICOMMAND(push, "rTe", (ident *id, tagval *v, uint *code),
{
    if(id->type != Id_Alias || id->index < Max_Args) return;
    identstack stack;
    pusharg(*id, *v, stack);
    v->type = Value_Null;
    id->flags &= ~Idf_Unknown;
    executeret(code, *commandret);
    poparg(*id);
});

static inline void pushalias(ident &id, identstack &stack)
{
    if(id.type == Id_Alias && id.index >= Max_Args)
    {
        pusharg(id, nullval, stack);
        id.flags &= ~Idf_Unknown;
    }
}

static inline void popalias(ident &id)
{
    if(id.type == Id_Alias && id.index >= Max_Args) poparg(id);
}

KEYWORD(local, Id_Local);

static inline bool checknumber(const char *s)
{
    if(isdigit(s[0])) return true;
    else switch(s[0])
    {
        case '+': case '-': return isdigit(s[1]) || (s[1] == '.' && isdigit(s[2]));
        case '.': return isdigit(s[1]) != 0;
        default: return false;
    }
}
static inline bool checknumber(const stringslice &s) { return checknumber(s.str); }

template<class T> static inline ident *newident(const T &name, int flags)
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

static inline ident *forceident(tagval &v)
{
    switch(v.type)
    {
        case Value_Ident: return v.id;
        case Value_Macro: case Value_CString:
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
       return NULL;
    return id;
}

void resetvar(char *name)
{
    ident *id = idents.access(name);
    if(!id) return;
    if(id->flags&Idf_ReadOnly) debugcode("variable %s is read-only", id->name);
    else clearoverride(*id);
}

COMMAND(resetvar, "s");

static inline void setarg(ident &id, tagval &v)
{
    if(aliasstack->usedargs&(1<<id.index))
    {
        if(id.valtype == Value_String) delete[] id.val.s;
        id.setval(v);
        cleancode(id);
    }
    else
    {
        pusharg(id, v, aliasstack->argstack[id.index]);
        aliasstack->usedargs |= 1<<id.index;
    }
}

static inline void setalias(ident &id, tagval &v)
{
    if(id.valtype == Value_String) delete[] id.val.s;
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
                if(id->index < Max_Args) setarg(*id, v); else setalias(*id, v);
                return;
            case Id_Var:
                setvarchecked(id, v.getint());
                break;
            case Id_FloatVar:
                setfvarchecked(id, v.getfloat());
                break;
            case Id_StringVar:
                setsvarchecked(id, v.getstr());
                break;
            default:
                debugcode("cannot redefine builtin %s with an alias", id->name);
                break;
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

ICOMMAND(alias, "sT", (const char *name, tagval *v),
{
    setalias(name, *v);
    v->type = Value_Null;
});

// variable's and commands are registered through globals, see cube.h

int variable(const char *name, int min, int cur, int max, int *storage, identfun fun, int flags)
{
    addident(ident(Id_Var, name, min, max, storage, (void *)fun, flags));
    return cur;
}

float fvariable(const char *name, float min, float cur, float max, float *storage, identfun fun, int flags)
{
    addident(ident(Id_FloatVar, name, min, max, storage, (void *)fun, flags));
    return cur;
}

char *svariable(const char *name, const char *cur, char **storage, identfun fun, int flags)
{
    addident(ident(Id_StringVar, name, storage, (void *)fun, flags));
    return newstring(cur);
}

struct defvar : identval
{
    char *name;
    uint *onchange;

    defvar() : name(NULL), onchange(NULL) {}

    ~defvar()
    {
        DELETEA(name);
        if(onchange) freecode(onchange);
    }

    static void changed(ident *id)
    {
        defvar *v = (defvar *)id->storage.p;
        if(v->onchange) execute(v->onchange);
    }
};

hashnameset<defvar> defvars;

#define DEFVAR(cmdname, fmt, args, body) \
    ICOMMAND(cmdname, fmt, args, \
    { \
        if(idents.access(name)) { debugcode("cannot redefine %s as a variable", name); return; } \
        name = newstring(name); \
        defvar &def = defvars[name]; \
        def.name = name; \
        def.onchange = onchange[0] ? compilecode(onchange) : NULL; \
        body; \
    });
#define DEFIVAR(cmdname, flags) \
    DEFVAR(cmdname, "siiis", (char *name, int *min, int *cur, int *max, char *onchange), \
        def.i = variable(name, *min, *cur, *max, &def.i, def.onchange ? defvar::changed : NULL, flags))
#define DEFFVAR(cmdname, flags) \
    DEFVAR(cmdname, "sfffs", (char *name, float *min, float *cur, float *max, char *onchange), \
        def.f = fvariable(name, *min, *cur, *max, &def.f, def.onchange ? defvar::changed : NULL, flags))
#define DEFSVAR(cmdname, flags) \
    DEFVAR(cmdname, "sss", (char *name, char *cur, char *onchange), \
        def.s = svariable(name, cur, &def.s, def.onchange ? defvar::changed : NULL, flags))

DEFIVAR(defvar, 0);
DEFIVAR(defvarp, Idf_Persist);
DEFFVAR(deffvar, 0);
DEFFVAR(deffvarp, Idf_Persist);
DEFSVAR(defsvar, 0);
DEFSVAR(defsvarp, Idf_Persist);

#define _GETVAR(id, vartype, name, retval) \
    ident *id = idents.access(name); \
    if(!id || id->type!=vartype) return retval;
#define GETVAR(id, name, retval) _GETVAR(id, Id_Var, name, retval)
#define OVERRIDEVAR(errorval, saveval, resetval, clearval) \
    if(identflags&Idf_Overridden || id->flags&Idf_Override) \
    { \
        if(id->flags&Idf_Persist) \
        { \
            debugcode("cannot override persistent variable %s", id->name); \
            errorval; \
        } \
        if(!(id->flags&Idf_Overridden)) { saveval; id->flags |= Idf_Overridden; } \
        else { clearval; } \
    } \
    else \
    { \
        if(id->flags&Idf_Overridden) { resetval; id->flags &= ~Idf_Overridden; } \
        clearval; \
    }

void setvar(const char *name, int i, bool dofunc, bool doclamp)
{
    GETVAR(id, name, );
    OVERRIDEVAR(return, id->overrideval.i = *id->storage.i, , )
    if(doclamp) *id->storage.i = clamp(i, id->minval, id->maxval);
    else *id->storage.i = i;
    if(dofunc) id->changed();
}
void setfvar(const char *name, float f, bool dofunc, bool doclamp)
{
    _GETVAR(id, Id_FloatVar, name, );
    OVERRIDEVAR(return, id->overrideval.f = *id->storage.f, , );
    if(doclamp) *id->storage.f = clamp(f, id->minvalf, id->maxvalf);
    else *id->storage.f = f;
    if(dofunc) id->changed();
}
void setsvar(const char *name, const char *str, bool dofunc)
{
    _GETVAR(id, Id_StringVar, name, );
    OVERRIDEVAR(return, id->overrideval.s = *id->storage.s, delete[] id->overrideval.s, delete[] *id->storage.s);
    *id->storage.s = newstring(str);
    if(dofunc) id->changed();
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
    _GETVAR(id, Id_FloatVar, name, 0);
    return id->minvalf;
}
float getfvarmax(const char *name)
{
    _GETVAR(id, Id_FloatVar, name, 0);
    return id->maxvalf;
}

ICOMMAND(getvarmin, "s", (char *s), intret(getvarmin(s)));
ICOMMAND(getvarmax, "s", (char *s), intret(getvarmax(s)));
ICOMMAND(getfvarmin, "s", (char *s), floatret(getfvarmin(s)));
ICOMMAND(getfvarmax, "s", (char *s), floatret(getfvarmax(s)));

bool identexists(const char *name) { return idents.access(name)!=NULL; }
ICOMMAND(identexists, "s", (char *s), intret(identexists(s) ? 1 : 0));

ident *getident(const char *name) { return idents.access(name); }

void touchvar(const char *name)
{
    ident *id = idents.access(name);
    if(id) switch(id->type)
    {
        case Id_Var:
        case Id_FloatVar:
        case Id_StringVar:
            id->changed();
            break;
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
    if(val < minval) val = minval;
    else if(val > maxval) val = maxval;
    else return val;
    debugcode(id->flags&Idf_Hex ?
            (minval <= 255 ? "valid range for %s is %d..0x%X" : "valid range for %s is 0x%X..0x%X") :
            "valid range for %s is %d..%d",
        id->name, minval, maxval);
    return val;
}

void setvarchecked(ident *id, int val)
{
    if(id->flags&Idf_ReadOnly) debugcode("variable %s is read-only", id->name);
#ifndef STANDALONE
    else if(!(id->flags&Idf_Override) || identflags&Idf_Overridden || game::allowedittoggle())
#else
    else
#endif
    {
        OVERRIDEVAR(return, id->overrideval.i = *id->storage.i, , )
        if(val < id->minval || val > id->maxval) val = clampvar(id, val, id->minval, id->maxval);
        *id->storage.i = val;
        id->changed();                                             // call trigger function if available
#ifndef STANDALONE
        if(id->flags&Idf_Override && !(identflags&Idf_Overridden)) game::vartrigger(id);
#endif
    }
}

static inline void setvarchecked(ident *id, tagval *args, int numargs)
{
    int val = forceint(args[0]);
    if(id->flags&Idf_Hex && numargs > 1)
    {
        val = (val << 16) | (forceint(args[1])<<8);
        if(numargs > 2) val |= forceint(args[2]);
    }
    setvarchecked(id, val);
}

float clampfvar(ident *id, float val, float minval, float maxval)
{
    if(val < minval) val = minval;
    else if(val > maxval) val = maxval;
    else return val;
    debugcode("valid range for %s is %s..%s", id->name, floatstr(minval), floatstr(maxval));
    return val;
}

void setfvarchecked(ident *id, float val)
{
    if(id->flags&Idf_ReadOnly) debugcode("variable %s is read-only", id->name);
#ifndef STANDALONE
    else if(!(id->flags&Idf_Override) || identflags&Idf_Overridden || game::allowedittoggle())
#else
    else
#endif
    {
        OVERRIDEVAR(return, id->overrideval.f = *id->storage.f, , );
        if(val < id->minvalf || val > id->maxvalf) val = clampfvar(id, val, id->minvalf, id->maxvalf);
        *id->storage.f = val;
        id->changed();
#ifndef STANDALONE
        if(id->flags&Idf_Override && !(identflags&Idf_Overridden)) game::vartrigger(id);
#endif
    }
}

void setsvarchecked(ident *id, const char *val)
{
    if(id->flags&Idf_ReadOnly) debugcode("variable %s is read-only", id->name);
#ifndef STANDALONE
    else if(!(id->flags&Idf_Override) || identflags&Idf_Overridden || game::allowedittoggle())
#else
    else
#endif
    {
        OVERRIDEVAR(return, id->overrideval.s = *id->storage.s, delete[] id->overrideval.s, delete[] *id->storage.s);
        *id->storage.s = newstring(val);
        id->changed();
#ifndef STANDALONE
        if(id->flags&Idf_Override && !(identflags&Idf_Overridden)) game::vartrigger(id);
#endif
    }
}

bool addcommand(const char *name, identfun fun, const char *args, int type)
{
    uint argmask = 0;
    int numargs = 0;
    bool limit = true;
    if(args) for(const char *fmt = args; *fmt; fmt++) switch(*fmt)
    {
        case 'i': case 'b': case 'f': case 'F': case 't': case 'T': case 'E': case 'N': case 'D': if(numargs < Max_Args) numargs++; break;
        case 'S': case 's': case 'e': case 'r': case '$': if(numargs < Max_Args) { argmask |= 1<<numargs; numargs++; } break;
        case '1': case '2': case '3': case '4': if(numargs < Max_Args) fmt -= *fmt-'0'+1; break;
        case 'C': case 'V': limit = false; break;
        default: fatal("builtin %s declared with illegal type: %s", name, args); break;
    }
    if(limit && numargs > Max_CommandArgs) fatal("builtin %s declared with too many args: %d", name, numargs);
    addident(ident(type, name, args, argmask, numargs, (void *)fun));
    return false;
}

const char *parsestring(const char *p)
{
    for(; *p; p++) switch(*p)
    {
        case '\r':
        case '\n':
        case '\"':
            return p;
        case '^':
            if(*++p) break;
            return p;
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
            if(src >= end) break;
            int e = *src++;
            switch(e)
            {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case 'f': *dst++ = '\f'; break;
                default: *dst++ = e; break;
            }
        }
        else *dst++ = c;
    }
    *dst = '\0';
    return dst - start;
}

static char *conc(vector<char> &buf, tagval *v, int n, bool space, const char *prefix = NULL, int prefixlen = 0)
{
    if(prefix)
    {
        buf.put(prefix, prefixlen);
        if(space && n) buf.add(' ');
    }
    for(int i = 0; i < n; ++i)
    {
        const char *s = "";
        int len = 0;
        switch(v[i].type)
        {
            case Value_Integer: s = intstr(v[i].i); break;
            case Value_Float: s = floatstr(v[i].f); break;
            case Value_String: case Value_CString: s = v[i].s; break;
            case Value_Macro: s = v[i].s; len = v[i].code[-1]>>8; goto haslen;
        }
        len = int(strlen(s));
    haslen:
        buf.put(s, len);
        if(i == n-1) break;
        if(space) buf.add(' ');
    }
    buf.add('\0');
    return buf.getbuf();
}

static char *conc(tagval *v, int n, bool space, const char *prefix, int prefixlen)
{
    static int vlen[Max_Args];
    static char numbuf[3*MAXSTRLEN];
    int len = prefixlen, numlen = 0, i = 0;
    for(; i < n; i++) switch(v[i].type)
    {
        case Value_Macro: len += (vlen[i] = v[i].code[-1]>>8); break;
        case Value_String: case Value_CString: len += (vlen[i] = int(strlen(v[i].s))); break;
        case Value_Integer:
            if(numlen + MAXSTRLEN > int(sizeof(numbuf))) goto overflow;
            intformat(&numbuf[numlen], v[i].i);
            numlen += (vlen[i] = strlen(&numbuf[numlen]));
            break;
        case Value_Float:
            if(numlen + MAXSTRLEN > int(sizeof(numbuf))) goto overflow;
            floatformat(&numbuf[numlen], v[i].f);
            numlen += (vlen[i] = strlen(&numbuf[numlen]));
            break;
        default: vlen[i] = 0; break;
    }
overflow:
    if(space) len += max(prefix ? i : i-1, 0);
    char *buf = newstring(len + numlen);
    int offset = 0, numoffset = 0;
    if(prefix)
    {
        memcpy(buf, prefix, prefixlen);
        offset += prefixlen;
        if(space && i) buf[offset++] = ' ';
    }
    for(int j = 0; j < i; ++j)
    {
        if(v[j].type == Value_Integer || v[j].type == Value_Float)
        {
            memcpy(&buf[offset], &numbuf[numoffset], vlen[j]);
            numoffset += vlen[j];
        }
        else if(vlen[j]) memcpy(&buf[offset], v[j].s, vlen[j]);
        offset += vlen[j];
        if(j==i-1) break;
        if(space) buf[offset++] = ' ';
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

static inline char *conc(tagval *v, int n, bool space)
{
    return conc(v, n, space, NULL, 0);
}

static inline char *conc(tagval *v, int n, bool space, const char *prefix)
{
    return conc(v, n, space, prefix, strlen(prefix));
}

static inline void skipcomments(const char *&p)
{
    for(;;)
    {
        p += strspn(p, " \t\r");
        if(p[0]!='/' || p[1]!='/') break;
        p += strcspn(p, "\n\0");
    }
}

static vector<char> strbuf[4];
static int stridx = 0;

static inline void cutstring(const char *&p, stringslice &s)
{
    p++;
    const char *end = parsestring(p);
    int maxlen = int(end-p) + 1;

    stridx = (stridx + 1)%4;
    vector<char> &buf = strbuf[stridx];
    if(buf.alen < maxlen) buf.growbuf(maxlen);

    s.str = buf.buf;
    s.len = unescapestring(buf.buf, p, end);
    p = end;
    if(*p=='\"') p++;
}

static inline char *cutstring(const char *&p)
{
    p++;
    const char *end = parsestring(p);
    char *buf = newstring(end-p);
    unescapestring(buf, p, end);
    p = end;
    if(*p=='\"') p++;
    return buf;
}

static inline const char *parseword(const char *p)
{
    const int maxbrak = 100;
    static char brakstack[maxbrak];
    int brakdepth = 0;
    for(;; p++)
    {
        p += strcspn(p, "\"/;()[] \t\r\n\0");
        switch(p[0])
        {
            case '"': case ';': case ' ': case '\t': case '\r': case '\n': case '\0': return p;
            case '/': if(p[1] == '/') return p; break;
            case '[': case '(': if(brakdepth >= maxbrak) return p; brakstack[brakdepth++] = p[0]; break;
            case ']': if(brakdepth <= 0 || brakstack[--brakdepth] != '[') return p; break;
            case ')': if(brakdepth <= 0 || brakstack[--brakdepth] != '(') return p; break;
        }
    }
    return p;
}

static inline void cutword(const char *&p, stringslice &s)
{
    s.str = p;
    p = parseword(p);
    s.len = int(p-s.str);
}

static inline char *cutword(const char *&p)
{
    const char *word = p;
    p = parseword(p);
    return p!=word ? newstring(word, p-word) : NULL;
}

#define RET_CODE(type, defaultret) ((type) >= Value_Any ? ((type) == Value_CString ? Ret_String : (defaultret)) : (type) << Code_Ret)
#define RET_CODE_INT(type) RET_CODE(type, Ret_Integer)
#define RET_CODE_FLOAT(type) RET_CODE(type, Ret_Float)
#define RET_CODE_ANY(type) RET_CODE(type, 0)
#define RET_CODE_STRING(type) ((type) >= Value_Any ? Ret_String : (type) << Code_Ret)

static inline void compilestr(vector<uint> &code, const char *word, int len, bool macro = false)
{
    if(len <= 3 && !macro)
    {
        uint op = Code_ValI|Ret_String;
        for(int i = 0; i < len; ++i)
        {
            op |= uint(uchar(word[i]))<<((i+1)*8);
        }
        code.add(op);
        return;
    }
    code.add((macro ? Code_Macro : Code_Val|Ret_String)|(len<<8));
    code.put((const uint *)word, len/sizeof(uint));
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

static inline void compilestr(vector<uint> &code) { code.add(Code_ValI|Ret_String); }
static inline void compilestr(vector<uint> &code, const stringslice &word, bool macro = false) { compilestr(code, word.str, word.len, macro); }
static inline void compilestr(vector<uint> &code, const char *word, bool macro = false) { compilestr(code, word, int(strlen(word)), macro); }

static inline void compileunescapestring(vector<uint> &code, const char *&p, bool macro = false)
{
    p++;
    const char *end = parsestring(p);
    code.add(macro ? Code_Macro : Code_Val|Ret_String);
    char *buf = (char *)code.reserve(int(end-p)/sizeof(uint) + 1).buf;
    int len = unescapestring(buf, p, end);
    memset(&buf[len], 0, sizeof(uint) - len%sizeof(uint));
    code.last() |= len<<8;
    code.advance(len/sizeof(uint) + 1);
    p = end;
    if(*p == '\"') p++;
}

static inline void compileint(vector<uint> &code, int i = 0)
{
    if(i >= -0x800000 && i <= 0x7FFFFF)
        code.add(Code_ValI|Ret_Integer|(i<<8));
    else
    {
        code.add(Code_Val|Ret_Integer);
        code.add(i);
    }
}

static inline void compilenull(vector<uint> &code)
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

static inline void compileblock(vector<uint> &code)
{
    code.add(Code_Empty);
}

static void compilestatements(vector<uint> &code, const char *&p, int rettype, int brak = '\0', int prevargs = 0);

static inline const char *compileblock(vector<uint> &code, const char *p, int rettype = Ret_Null, int brak = '\0')
{
    int start = code.length();
    code.add(Code_Block);
    code.add(Code_Offset|((start+2)<<8));
    if(p) compilestatements(code, p, Value_Any, brak);
    if(code.length() > start + 2)
    {
        code.add(Code_Exit|rettype);
        code[start] |= uint(code.length() - (start + 1))<<8;
    }
    else
    {
        code.setsize(start);
        code.add(Code_Empty|rettype);
    }
    return p;
}

static inline void compileident(vector<uint> &code, ident *id = dummyident)
{
    code.add((id->index < Max_Args ? Code_IdentArg : Code_Ident)|(id->index<<8));
}

static inline void compileident(vector<uint> &code, const stringslice &word)
{
    compileident(code, newident(word, Idf_Unknown));
}

static inline void compileint(vector<uint> &code, const stringslice &word)
{
    compileint(code, word.len ? parseint(word.str) : 0);
}

static inline void compilefloat(vector<uint> &code, float f = 0.0f)
{
    if(int(f) == f && f >= -0x800000 && f <= 0x7FFFFF)
        code.add(Code_ValI|Ret_Float|(int(f)<<8));
    else
    {
        union { float f; uint u; } conv;
        conv.f = f;
        code.add(Code_Val|Ret_Float);
        code.add(conv.u);
    }
}

static inline void compilefloat(vector<uint> &code, const stringslice &word)
{
    compilefloat(code, word.len ? parsefloat(word.str) : 0.0f);
}

static inline bool getbool(const char *s)
{
    switch(s[0])
    {
        case '+': case '-':
            switch(s[1])
            {
                case '0': break;
                case '.': return !isdigit(s[2]) || parsefloat(s) != 0;
                default: return true;
            }
            // fall-through
        case '0':
        {
            char *end;
            int val = int(strtoul((char *)s, &end, 0));
            if(val) return true;
            switch(*end)
            {
                case 'e': case '.': return parsefloat(s) != 0;
                default: return false;
            }
        }
        case '.': return !isdigit(s[1]) || parsefloat(s) != 0;
        case '\0': return false;
        default: return true;
    }
}

static inline bool getbool(const tagval &v)
{
    switch(v.type)
    {
        case Value_Float: return v.f!=0;
        case Value_Integer: return v.i!=0;
        case Value_String: case Value_Macro: case Value_CString: return getbool(v.s);
        default: return false;
    }
}

static inline void compileval(vector<uint> &code, int wordtype, const stringslice &word = stringslice(NULL, 0))
{
    switch(wordtype)
    {
        case Value_CAny: if(word.len) compilestr(code, word, true); else compilenull(code); break;
        case Value_CString: compilestr(code, word, true); break;
        case Value_Any: if(word.len) compilestr(code, word); else compilenull(code); break;
        case Value_String: compilestr(code, word); break;
        case Value_Float: compilefloat(code, word); break;
        case Value_Integer: compileint(code, word); break;
        case Value_Cond: if(word.len) compileblock(code, word.str); else compilenull(code); break;
        case Value_Code: compileblock(code, word.str); break;
        case Value_Ident: compileident(code, word); break;
        default: break;
    }
}

static stringslice unusedword(NULL, 0);
static bool compilearg(vector<uint> &code, const char *&p, int wordtype, int prevargs = Max_Results, stringslice &word = unusedword);

static void compilelookup(vector<uint> &code, const char *&p, int ltype, int prevargs = Max_Results)
{
    stringslice lookup;
    switch(*++p)
    {
        case '(':
        case '[':
            if(!compilearg(code, p, Value_CString, prevargs)) goto invalid;
            break;
        case '$':
            compilelookup(code, p, Value_CString, prevargs);
            break;
        case '\"':
            cutstring(p, lookup);
            goto lookupid;
        default:
        {
            cutword(p, lookup);
            if(!lookup.len) goto invalid;
        lookupid:
            ident *id = newident(lookup, Idf_Unknown);
            if(id) switch(id->type)
            {
                case Id_Var:
                    code.add(Code_IntVar|RET_CODE_INT(ltype)|(id->index<<8));
                    switch(ltype)
                    {
                        case Value_Pop: code.pop(); break;
                        case Value_Code: code.add(Code_Compile); break;
                        case Value_Ident: code.add(Code_IdentU); break;
                    }
                    return;
                case Id_FloatVar:
                    code.add(Code_FloatVar|RET_CODE_FLOAT(ltype)|(id->index<<8));
                    switch(ltype)
                    {
                        case Value_Pop: code.pop(); break;
                        case Value_Code: code.add(Code_Compile); break;
                        case Value_Ident: code.add(Code_IdentU); break;
                    }
                    return;
                case Id_StringVar:
                    switch(ltype)
                    {
                        case Value_Pop: return;
                        case Value_CAny: case Value_CString: case Value_Code: case Value_Ident: case Value_Cond:
                            code.add(Code_StrVarM|(id->index<<8));
                            break;
                        default:
                            code.add(Code_StrVar|RET_CODE_STRING(ltype)|(id->index<<8));
                            break;
                    }
                    goto done;
                case Id_Alias:
                    switch(ltype)
                    {
                        case Value_Pop: return;
                        case Value_CAny: case Value_Cond:
                            code.add((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|(id->index<<8));
                            break;
                        case Value_CString: case Value_Code: case Value_Ident:
                            code.add((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|Ret_String|(id->index<<8));
                            break;
                        default:
                            code.add((id->index < Max_Args ? Code_LookupArg : Code_Lookup)|RET_CODE_STRING(ltype)|(id->index<<8));
                            break;
                    }
                    goto done;
                case Id_Command:
                {
                    int comtype = Code_Com, numargs = 0;
                    if(prevargs >= Max_Results) code.add(Code_Enter);
                    for(const char *fmt = id->args; *fmt; fmt++) switch(*fmt)
                    {
                        case 'S': compilestr(code); numargs++; break;
                        case 's': compilestr(code, NULL, 0, true); numargs++; break;
                        case 'i': compileint(code); numargs++; break;
                        case 'b': compileint(code, INT_MIN); numargs++; break;
                        case 'f': compilefloat(code); numargs++; break;
                        case 'F': code.add(Code_Dup|Ret_Float); numargs++; break;
                        case 'E':
                        case 'T':
                        case 't': compilenull(code); numargs++; break;
                        case 'e': compileblock(code); numargs++; break;
                        case 'r': compileident(code); numargs++; break;
                        case '$': compileident(code, id); numargs++; break;
                        case 'N': compileint(code, -1); numargs++; break;
#ifndef STANDALONE
                        case 'D': comtype = Code_ComD; numargs++; break;
#endif
                        case 'C': comtype = Code_ComC; goto compilecomv;
                        case 'V': comtype = Code_ComV; goto compilecomv;
                        case '1': case '2': case '3': case '4': break;
                    }
                    code.add(comtype|RET_CODE_ANY(ltype)|(id->index<<8));
                    code.add((prevargs >= Max_Results ? Code_Exit : Code_ResultArg) | RET_CODE_ANY(ltype));
                    goto done;
                compilecomv:
                    code.add(comtype|RET_CODE_ANY(ltype)|(numargs<<8)|(id->index<<13));
                    code.add((prevargs >= Max_Results ? Code_Exit : Code_ResultArg) | RET_CODE_ANY(ltype));
                    goto done;
                }
                default: goto invalid;
            }
            compilestr(code, lookup, true);
            break;
        }
    }
    switch(ltype)
    {
    case Value_CAny: case Value_Cond:
        code.add(Code_LookupMU);
        break;
    case Value_CString: case Value_Code: case Value_Ident:
        code.add(Code_LookupMU|Ret_String);
        break;
    default:
        code.add(Code_LookupU|RET_CODE_ANY(ltype));
        break;
    }
done:
    switch(ltype)
    {
        case Value_Pop: code.add(Code_Pop); break;
        case Value_Code: code.add(Code_Compile); break;
        case Value_Cond: code.add(Code_Cond); break;
        case Value_Ident: code.add(Code_IdentU); break;
    }
    return;
invalid:
    switch(ltype)
    {
        case Value_Pop: break;
        case Value_Null: case Value_Any: case Value_CAny: case Value_Word: case Value_Cond: compilenull(code); break;
        default: compileval(code, ltype); break;
    }
}

static bool compileblockstr(vector<uint> &code, const char *str, const char *end, bool macro)
{
    int start = code.length();
    code.add(macro ? Code_Macro : Code_Val|Ret_String);
    char *buf = (char *)code.reserve((end-str)/sizeof(uint)+1).buf;
    int len = 0;
    while(str < end)
    {
        int n = strcspn(str, "\r/\"@]\0");
        memcpy(&buf[len], str, n);
        len += n;
        str += n;
        switch(*str)
        {
            case '\r': str++; break;
            case '\"':
            {
                const char *start = str;
                str = parsestring(str+1);
                if(*str=='\"') str++;
                memcpy(&buf[len], start, str-start);
                len += str-start;
                break;
            }
            case '/':
                if(str[1] == '/')
                {
                    size_t comment = strcspn(str, "\n\0");
                    if (iscubepunct(str[2]))
                    {
                        memcpy(&buf[len], str, comment);
                        len += comment;
                    }
                    str += comment;
                }
                else buf[len++] = *str++;
                break;
            case '@':
            case ']':
                if(str < end) { buf[len++] = *str++; break; }
            case '\0': goto done;
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
            if(!compilearg(code, p, Value_CAny, prevargs)) return false;
            break;
        case '[':
            if(!compilearg(code, p, Value_CString, prevargs)) return false;
            code.add(Code_LookupMU);
            break;
        case '\"':
            cutstring(p, lookup);
            goto lookupid;
        default:
        {

            lookup.str = p;
            while(iscubealnum(*p) || *p=='_') p++;
            lookup.len = int(p-lookup.str);
            if(!lookup.len) return false;
        lookupid:
            ident *id = newident(lookup, Idf_Unknown);
            if(id) switch(id->type)
            {
            case Id_Var: code.add(Code_IntVar|(id->index<<8)); goto done;
            case Id_FloatVar: code.add(Code_FloatVar|(id->index<<8)); goto done;
            case Id_StringVar: code.add(Code_StrVarM|(id->index<<8)); goto done;
            case Id_Alias: code.add((id->index < Max_Args ? Code_LookupMArg : Code_LookupM)|(id->index<<8)); goto done;
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
    const char *line = p, *start = p;
    int concs = 0;
    for(int brak = 1; brak;)
    {
        p += strcspn(p, "@\"/[]\0");
        int c = *p++;
        switch(c)
        {
            case '\0':
                debugcodeline(line, "missing \"]\"");
                p--;
                goto done;
            case '\"':
                p = parsestring(p);
                if(*p=='\"') p++;
                break;
            case '/':
                if(*p=='/') p += strcspn(p, "\n\0");
                break;
            case '[': brak++; break;
            case ']': brak--; break;
            case '@':
            {
                const char *esc = p;
                while(*p == '@') p++;
                int level = p - (esc - 1);
                if(brak > level) continue;
                else if(brak < level) debugcodeline(line, "too many @s");
                if(!concs && prevargs >= Max_Results) code.add(Code_Enter);
                if(concs + 2 > Max_Args)
                {
                    code.add(Code_ConCW|Ret_String|(concs<<8));
                    concs = 1;
                }
                if(compileblockstr(code, start, esc-1, true)) concs++;
                if(compileblocksub(code, p, prevargs + concs)) concs++;
                if(concs) start = p;
                else if(prevargs >= Max_Results) code.pop();
                break;
            }
        }
    }
done:
    if(p-1 > start)
    {
        if(!concs) switch(wordtype)
        {
            case Value_Pop:
                return;
            case Value_Code: case Value_Cond:
                p = compileblock(code, start, Ret_Null, ']');
                return;
            case Value_Ident:
                compileident(code, stringslice(start, p-1));
                return;
        }
        switch(wordtype)
        {
            case Value_CString: case Value_Code: case Value_Ident: case Value_CAny: case Value_Cond:
                compileblockstr(code, start, p-1, true);
                break;
            default:
                compileblockstr(code, start, p-1, concs > 0);
                break;
        }
        if(concs > 1) concs++;
    }
    if(concs)
    {
        if(prevargs >= Max_Results)
        {
            code.add(Code_ConCM|RET_CODE_ANY(wordtype)|(concs<<8));
            code.add(Code_Exit|RET_CODE_ANY(wordtype));
        }
        else code.add(Code_ConCW|RET_CODE_ANY(wordtype)|(concs<<8));
    }
    switch(wordtype)
    {
        case Value_Pop: if(concs || p-1 > start) code.add(Code_Pop); break;
        case Value_Cond: if(!concs && p-1 <= start) compilenull(code); else code.add(Code_Cond); break;
        case Value_Code: if(!concs && p-1 <= start) compileblock(code); else code.add(Code_Compile); break;
        case Value_Ident: if(!concs && p-1 <= start) compileident(code); else code.add(Code_IdentU); break;
        case Value_CString: case Value_CAny:
            if(!concs && p-1 <= start) compilestr(code, NULL, 0, true);
            break;
        case Value_String: case Value_Null: case Value_Any: case Value_Word:
            if(!concs && p-1 <= start) compilestr(code);
            break;
        default:
            if(!concs)
            {
                if(p-1 <= start) compileval(code, wordtype);
                else code.add(Code_Force|(wordtype<<Code_Ret));
            }
            break;
    }
}

static bool compilearg(vector<uint> &code, const char *&p, int wordtype, int prevargs, stringslice &word)
{
    skipcomments(p);
    switch(*p)
    {
        case '\"':
            switch(wordtype)
            {
                case Value_Pop:
                    p = parsestring(p+1);
                    if(*p == '\"') p++;
                    break;
                case Value_Cond:
                {
                    char *s = cutstring(p);
                    if(s[0]) compileblock(code, s);
                    else compilenull(code);
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
                    cutstring(p, word);
                    break;
                case Value_Any:
                case Value_String:
                    compileunescapestring(code, p);
                    break;
                case Value_CAny:
                case Value_CString:
                    compileunescapestring(code, p, true);
                    break;
                default:
                {
                    stringslice s;
                    cutstring(p, s);
                    compileval(code, wordtype, s);
                    break;
                }
            }
            return true;
        case '$': compilelookup(code, p, wordtype, prevargs); return true;
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
                if(code.length() > start) code.add(Code_ResultArg|RET_CODE_ANY(wordtype));
                else { compileval(code, wordtype); return true; }
            }
            switch(wordtype)
            {
                case Value_Pop: code.add(Code_Pop); break;
                case Value_Cond: code.add(Code_Cond); break;
                case Value_Code: code.add(Code_Compile); break;
                case Value_Ident: code.add(Code_IdentU); break;
            }
            return true;
        case '[':
            p++;
            compileblockmain(code, p, wordtype, prevargs);
            return true;
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
                    if(!s) return false;
                    compileblock(code, s);
                    delete[] s;
                    return true;
                }
                case Value_Code:
                {
                    char *s = cutword(p);
                    if(!s) return false;
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
                    if(!s.len) return false;
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
        idname.str = NULL;
        bool more = compilearg(code, p, Value_Word, prevargs, idname);
        if(!more) goto endstatement;
        skipcomments(p);
        if(p[0] == '=') switch(p[1])
        {
            case '/':
                if(p[2] != '/') break;
            case ';': case ' ': case '\t': case '\r': case '\n': case '\0':
                p++;
                if(idname.str)
                {
                    ident *id = newident(idname, Idf_Unknown);
                    if(id) switch(id->type)
                    {
                        case Id_Alias:
                            if(!(more = compilearg(code, p, Value_Any, prevargs))) compilestr(code);
                            code.add((id->index < Max_Args ? Code_AliasArg : Code_Alias)|(id->index<<8));
                            goto endstatement;
                        case Id_Var:
                            if(!(more = compilearg(code, p, Value_Integer, prevargs))) compileint(code);
                            code.add(Code_IntVar1|(id->index<<8));
                            goto endstatement;
                        case Id_FloatVar:
                            if(!(more = compilearg(code, p, Value_Float, prevargs))) compilefloat(code);
                            code.add(Code_FloatVar1|(id->index<<8));
                            goto endstatement;
                        case Id_StringVar:
                            if(!(more = compilearg(code, p, Value_CString, prevargs))) compilestr(code);
                            code.add(Code_StrVar1|(id->index<<8));
                            goto endstatement;
                    }
                    compilestr(code, idname, true);
                }
                if(!(more = compilearg(code, p, Value_Any))) compilestr(code);
                code.add(Code_AliasU);
                goto endstatement;
        }
        numargs = 0;
        if(!idname.str)
        {
        noid:
            while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs))) numargs++;
            code.add(Code_CallU|(numargs<<8));
        }
        else
        {
            ident *id = idents.access(idname);
            if(!id)
            {
                if(!checknumber(idname)) { compilestr(code, idname, true); goto noid; }
                switch(rettype)
                {
                case Value_Any:
                case Value_CAny:
                {
                    char *end = (char *)idname.str;
                    int val = int(strtoul(idname.str, &end, 0));
                    if(end < idname.end()) compilestr(code, idname, rettype==Value_CAny);
                    else compileint(code, val);
                    break;
                }
                default:
                    compileval(code, rettype, idname);
                    break;
                }
                code.add(Code_Result);
            }
            else switch(id->type)
            {
                case Id_Alias:
                    while(numargs < Max_Args && (more = compilearg(code, p, Value_Any, prevargs+numargs))) numargs++;
                    code.add((id->index < Max_Args ? Code_CallArg : Code_Call)|(numargs<<8)|(id->index<<13));
                    break;
                case Id_Command:
                {
                    int comtype = Code_Com, fakeargs = 0;
                    bool rep = false;
                    for(const char *fmt = id->args; *fmt; fmt++) switch(*fmt)
                    {
                    case 'S':
                    case 's':
                        if(more) more = compilearg(code, p, *fmt == 's' ? Value_CString : Value_String, prevargs+numargs);
                        if(!more)
                        {
                            if(rep) break;
                            compilestr(code, NULL, 0, *fmt=='s');
                            fakeargs++;
                        }
                        else if(!fmt[1])
                        {
                            int numconc = 1;
                            while(numargs + numconc < Max_Args && (more = compilearg(code, p, Value_CString, prevargs+numargs+numconc))) numconc++;
                            if(numconc > 1) code.add(Code_ConC|Ret_String|(numconc<<8));
                        }
                        numargs++;
                        break;
                    case 'i': if(more) more = compilearg(code, p, Value_Integer, prevargs+numargs); if(!more) { if(rep) break; compileint(code); fakeargs++; } numargs++; break;
                    case 'b': if(more) more = compilearg(code, p, Value_Integer, prevargs+numargs); if(!more) { if(rep) break; compileint(code, INT_MIN); fakeargs++; } numargs++; break;
                    case 'f': if(more) more = compilearg(code, p, Value_Float, prevargs+numargs); if(!more) { if(rep) break; compilefloat(code); fakeargs++; } numargs++; break;
                    case 'F': if(more) more = compilearg(code, p, Value_Float, prevargs+numargs); if(!more) { if(rep) break; code.add(Code_Dup|Ret_Float); fakeargs++; } numargs++; break;
                    case 'T':
                    case 't': if(more) more = compilearg(code, p, *fmt == 't' ? Value_CAny : Value_Any, prevargs+numargs); if(!more) { if(rep) break; compilenull(code); fakeargs++; } numargs++; break;
                    case 'E': if(more) more = compilearg(code, p, Value_Cond, prevargs+numargs); if(!more) { if(rep) break; compilenull(code); fakeargs++; } numargs++; break;
                    case 'e': if(more) more = compilearg(code, p, Value_Code, prevargs+numargs); if(!more) { if(rep) break; compileblock(code); fakeargs++; } numargs++; break;
                    case 'r': if(more) more = compilearg(code, p, Value_Ident, prevargs+numargs); if(!more) { if(rep) break; compileident(code); fakeargs++; } numargs++; break;
                    case '$': compileident(code, id); numargs++; break;
                    case 'N': compileint(code, numargs-fakeargs); numargs++; break;
#ifndef STANDALONE
                    case 'D': comtype = Code_ComD; numargs++; break;
#endif
                    case 'C': comtype = Code_ComC; if(more) while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs))) numargs++; goto compilecomv;
                    case 'V': comtype = Code_ComV; if(more) while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs))) numargs++; goto compilecomv;
                    case '1': case '2': case '3': case '4':
                        if(more && numargs < Max_Args)
                        { 
                            int numrep = *fmt-'0'+1;
                            fmt -= numrep;
                            rep = true;
                        }
                        else for(; numargs > Max_Args; numargs--) code.add(Code_Pop);
                        break;
                    }
                    code.add(comtype|RET_CODE_ANY(rettype)|(id->index<<8));
                    break;
                compilecomv:
                    code.add(comtype|RET_CODE_ANY(rettype)|(numargs<<8)|(id->index<<13));
                    break;
                }
                case Id_Local:
                    if(more) while(numargs < Max_Args && (more = compilearg(code, p, Value_Ident, prevargs+numargs))) numargs++;
                    if(more) while((more = compilearg(code, p, Value_Pop)));
                    code.add(Code_Local|(numargs<<8));
                    break;
                case Id_Do:
                    if(more) more = compilearg(code, p, Value_Code, prevargs);
                    code.add((more ? Code_Do : Code_Null) | RET_CODE_ANY(rettype));
                    break;
                case Id_DoArgs:
                    if(more) more = compilearg(code, p, Value_Code, prevargs);
                    code.add((more ? Code_DoArgs : Code_Null) | RET_CODE_ANY(rettype));
                    break;
                case Id_If:
                    if(more) more = compilearg(code, p, Value_CAny, prevargs);
                    if(!more) code.add(Code_Null | RET_CODE_ANY(rettype));
                    else
                    {
                        int start1 = code.length();
                        more = compilearg(code, p, Value_Code, prevargs+1);
                        if(!more) { code.add(Code_Pop); code.add(Code_Null | RET_CODE_ANY(rettype)); }
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
                case Id_Result:
                    if(more) more = compilearg(code, p, Value_Any, prevargs);
                    code.add((more ? Code_Result : Code_Null) | RET_CODE_ANY(rettype));
                    break;
                case Id_Not:
                    if(more) more = compilearg(code, p, Value_CAny, prevargs);
                    code.add((more ? Code_Not : Code_True) | RET_CODE_ANY(rettype));
                    break;
                case Id_And:
                case Id_Or:
                    if(more) more = compilearg(code, p, Value_Cond, prevargs);
                    if(!more) { code.add((id->type == Id_And ? Code_True : Code_False) | RET_CODE_ANY(rettype)); }
                    else
                    {
                        numargs++;
                        int start = code.length(), end = start;
                        while(numargs < Max_Args)
                        {
                            more = compilearg(code, p, Value_Cond, prevargs+numargs);
                            if(!more) break;
                            numargs++;
                            if((code[end]&~Code_RetMask) != (Code_Block|(uint(code.length()-(end+1))<<8))) break;
                            end = code.length();
                        }
                        if(more)
                        {
                            while(numargs < Max_Args && (more = compilearg(code, p, Value_Cond, prevargs+numargs))) numargs++;
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
                case Id_Var:
                    if(!(more = compilearg(code, p, Value_Integer, prevargs))) code.add(Code_Print|(id->index<<8));
                    else if(!(id->flags&Idf_Hex) || !(more = compilearg(code, p, Value_Integer, prevargs+1))) code.add(Code_IntVar1|(id->index<<8));
                    else if(!(more = compilearg(code, p, Value_Integer, prevargs+2))) code.add(Code_IntVar2|(id->index<<8));
                    else code.add(Code_IntVar3|(id->index<<8));
                    break;
                case Id_FloatVar:
                    if(!(more = compilearg(code, p, Value_Float, prevargs))) code.add(Code_Print|(id->index<<8));
                    else code.add(Code_FloatVar1|(id->index<<8));
                    break;
                case Id_StringVar:
                    if(!(more = compilearg(code, p, Value_CString, prevargs))) code.add(Code_Print|(id->index<<8));
                    else
                    {
                        do ++numargs;
                        while(numargs < Max_Args && (more = compilearg(code, p, Value_CAny, prevargs+numargs)));
                        if(numargs > 1) code.add(Code_ConC|Ret_String|(numargs<<8));
                        code.add(Code_StrVar1|(id->index<<8));
                    }
                    break;
            }
        }
    endstatement:
        if(more) while(compilearg(code, p, Value_Pop));
        p += strcspn(p, ")];/\n\0");
        int c = *p++;
        switch(c)
        {
            case '\0':
                if(c != brak) debugcodeline(line, "missing \"%c\"", brak);
                p--;
                return;

            case ')':
            case ']':
                if(c == brak) return;
                debugcodeline(line, "unexpected \"%c\"", c);
                break;

            case '/':
                if(*p == '/') p += strcspn(p, "\n\0");
                goto endstatement;
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

static inline const uint *forcecode(tagval &v)
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

static inline void forcecond(tagval &v)
{
    switch(v.type)
    {
        case Value_String: case Value_Macro: case Value_CString:
            if(v.s[0]) forcecode(v);
            else v.setint(0);
            break;
    }
}

void keepcode(uint *code)
{
    if(!code) return;
    switch(*code&Code_OpMask)
    {
        case Code_Start:
            *code += 0x100;
            return;
    }
    switch(code[-1]&Code_OpMask)
    {
        case Code_Start:
            code[-1] += 0x100;
            break;
        case Code_Offset:
            code -= int(code[-1]>>8);
            *code += 0x100;
            break;
    }
}

void freecode(uint *code)
{
    if(!code) return;
    switch(*code&Code_OpMask)
    {
        case Code_Start:
            *code -= 0x100;
            if(int(*code) < 0x100) delete[] code;
            return;
    }
    switch(code[-1]&Code_OpMask)
    {
        case Code_Start:
            code[-1] -= 0x100;
            if(int(code[-1]) < 0x100) delete[] &code[-1];
            break;
        case Code_Offset:
            code -= int(code[-1]>>8);
            *code -= 0x100;
            if(int(*code) < 0x100) delete[] code;
            break;
    }
}

void printvar(ident *id, int i)
{
    if(i < 0) conoutf("%s = %d", id->name, i);
    else if(id->flags&Idf_Hex && id->maxval==0xFFFFFF)
        conoutf("%s = 0x%.6X (%d, %d, %d)", id->name, i, (i>>16)&0xFF, (i>>8)&0xFF, i&0xFF);
    else
        conoutf(id->flags&Idf_Hex ? "%s = 0x%X" : "%s = %d", id->name, i);
}

void printfvar(ident *id, float f)
{
    conoutf("%s = %s", id->name, floatstr(f));
}

void printsvar(ident *id, const char *s)
{
    conoutf(strchr(s, '"') ? "%s = [%s]" : "%s = \"%s\"", id->name, s);
}

void printvar(ident *id)
{
    switch(id->type)
    {
        case Id_Var: printvar(id, *id->storage.i); break;
        case Id_FloatVar: printfvar(id, *id->storage.f); break;
        case Id_StringVar: printsvar(id, *id->storage.s); break;
    }
}

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
                ++depth;
                continue;
            case Code_Exit|Ret_Null: case Code_Exit|Ret_String: case Code_Exit|Ret_Integer: case Code_Exit|Ret_Float:
                if(depth <= 0)
                {
                    if(&result != &noret) forcearg(result, op&Code_RetMask);
                    return code;
                }
                --depth;
                continue;
        }
    }
}

#ifndef STANDALONE
static inline uint *copycode(const uint *src)
{
    const uint *end = skipcode(src);
    size_t len = end - src;
    uint *dst = new uint[len + 1];
    *dst++ = Code_Start;
    memcpy(dst, src, len*sizeof(uint));
    return dst;
}

static inline void copyarg(tagval &dst, const tagval &src)
{
    switch(src.type)
    {
        case Value_Integer:
        case Value_Float:
        case Value_Ident:
            dst = src;
            break;
        case Value_String:
        case Value_Macro:
        case Value_CString:
            dst.setstr(newstring(src.s));
            break;
        case Value_Code:
            dst.setcode(copycode(src.code));
            break;
        default:
            dst.setnull();
            break;
    }
}

static inline void addreleaseaction(ident *id, tagval *args, int numargs)
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
    else args[numargs].setint(0);
}
#endif

static inline void callcommand(ident *id, tagval *args, int numargs, bool lookup = false)
{
    int i = -1, fakeargs = 0;
    bool rep = false;
    for(const char *fmt = id->args; *fmt; fmt++) switch(*fmt)
    {
        case 'i': if(++i >= numargs) { if(rep) break; args[i].setint(0); fakeargs++; } else forceint(args[i]); break;
        case 'b': if(++i >= numargs) { if(rep) break; args[i].setint(INT_MIN); fakeargs++; } else forceint(args[i]); break;
        case 'f': if(++i >= numargs) { if(rep) break; args[i].setfloat(0.0f); fakeargs++; } else forcefloat(args[i]); break;
        case 'F': if(++i >= numargs) { if(rep) break; args[i].setfloat(args[i-1].getfloat()); fakeargs++; } else forcefloat(args[i]); break;
        case 'S': if(++i >= numargs) { if(rep) break; args[i].setstr(newstring("")); fakeargs++; } else forcestr(args[i]); break;
        case 's': if(++i >= numargs) { if(rep) break; args[i].setcstr(""); fakeargs++; } else forcestr(args[i]); break;
        case 'T':
        case 't': if(++i >= numargs) { if(rep) break; args[i].setnull(); fakeargs++; } break;
        case 'E': if(++i >= numargs) { if(rep) break; args[i].setnull(); fakeargs++; } else forcecond(args[i]); break;
        case 'e':
            if(++i >= numargs)
            {
                if(rep) break;
                args[i].setcode(emptyblock[Value_Null]+1);
                fakeargs++;
            }
            else forcecode(args[i]);
            break;
        case 'r': if(++i >= numargs) { if(rep) break; args[i].setident(dummyident); fakeargs++; } else forceident(args[i]); break;
        case '$': if(++i < numargs) freearg(args[i]); args[i].setident(id); break;
        case 'N': if(++i < numargs) freearg(args[i]); args[i].setint(lookup ? -1 : i-fakeargs); break;
#ifndef STANDALONE
        case 'D': if(++i < numargs) freearg(args[i]); addreleaseaction(id, args, i); fakeargs++; break;
#endif
        case 'C': { i = max(i+1, numargs); vector<char> buf; ((comfun1)id->fun)(conc(buf, args, i, true)); goto cleanup; }
        case 'V': i = max(i+1, numargs); ((comfunv)id->fun)(args, i); goto cleanup;
        case '1': case '2': case '3': case '4': if(i+1 < numargs) { fmt -= *fmt-'0'+1; rep = true; } break;
    }
    ++i;
    #define OFFSETARG(n) n
    #define ARG(n) (id->argmask&(1<<(n)) ? (void *)args[OFFSETARG(n)].s : (void *)&args[OFFSETARG(n)].i)
    #define CALLCOM(n) \
        switch(n) \
        { \
            case 0: ((comfun)id->fun)(); break; \
            case 1: ((comfun1)id->fun)(ARG(0)); break; \
            case 2: ((comfun2)id->fun)(ARG(0), ARG(1)); break; \
            case 3: ((comfun3)id->fun)(ARG(0), ARG(1), ARG(2)); break; \
            case 4: ((comfun4)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3)); break; \
            case 5: ((comfun5)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4)); break; \
            case 6: ((comfun6)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); break; \
            case 7: ((comfun7)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6)); break; \
            case 8: ((comfun8)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7)); break; \
            case 9: ((comfun9)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8)); break; \
            case 10: ((comfun10)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9)); break; \
            case 11: ((comfun11)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10)); break; \
            case 12: ((comfun12)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10), ARG(11)); break; \
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

#define MAXRUNDEPTH 255
static int rundepth = 0;

static const uint *runcode(const uint *code, tagval &result)
{
    result.setnull();
    if(rundepth >= MAXRUNDEPTH)
    {
        debugcode("exceeded recursion limit");
        return skipcode(code, result);
    }
    ++rundepth;
    int numargs = 0;
    tagval args[Max_Args+Max_Results], *prevret = commandret;
    commandret = &result;
    for(;;)
    {
        uint op = *code++;
        switch(op&0xFF)
        {
            case Code_Start: case Code_Offset: continue;

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
                freearg(args[--numargs]);
                continue;
            case Code_Enter:
                code = runcode(code, args[numargs++]);
                continue;
            case Code_EnterResult:
                freearg(result);
                code = runcode(code, result);
                continue;
            case Code_Exit|Ret_String: case Code_Exit|Ret_Integer: case Code_Exit|Ret_Float:
                forcearg(result, op&Code_RetMask);
                // fall-through
            case Code_Exit|Ret_Null:
                goto exit;
            case Code_ResultArg|Ret_String: case Code_ResultArg|Ret_Integer: case Code_ResultArg|Ret_Float:
                forcearg(result, op&Code_RetMask);
                // fall-through
            case Code_ResultArg|Ret_Null:
                args[numargs++] = result;
                result.setnull();
                continue;
            case Code_Print:
                printvar(identmap[op>>8]);
                continue;

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
                for(int i = offset; i < numargs; i++) popalias(*args[i].id);
                goto exit;
            }

            case Code_DoArgs|Ret_Null: case Code_DoArgs|Ret_String: case Code_DoArgs|Ret_Integer: case Code_DoArgs|Ret_Float:
            {
                UNDOARGS
                freearg(result);
                runcode(args[--numargs].code, result);
                freearg(args[numargs]);
                forcearg(result, op&Code_RetMask);
                REDOARGS
                continue;
            }

            case Code_Do|Ret_Null: case Code_Do|Ret_String: case Code_Do|Ret_Integer: case Code_Do|Ret_Float:
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
                if(getbool(args[--numargs])) code += len;
                freearg(args[numargs]);
                continue;
            }
            case Code_JumpFalse:
            {
                uint len = op>>8;
                if(!getbool(args[--numargs])) code += len;
                freearg(args[numargs]);
                continue;
            }
            case Code_JumpResultTrue:
            {
                uint len = op>>8;
                freearg(result);
                --numargs;
                if(args[numargs].type == Value_Code) { runcode(args[numargs].code, result); freearg(args[numargs]); }
                else result = args[numargs];
                if(getbool(result)) code += len;
                continue;
            }
            case Code_JumpResultFalse:
            {
                uint len = op>>8;
                freearg(result);
                --numargs;
                if(args[numargs].type == Value_Code) { runcode(args[numargs].code, result); freearg(args[numargs]); }
                else result = args[numargs];
                if(!getbool(result)) code += len;
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
                args[numargs++].setstr(newstring((const char *)code, len));
                code += len/sizeof(uint) + 1;
                continue;
            }
            case Code_ValI|Ret_String:
            {
                char s[4] = { char((op>>8)&0xFF), char((op>>16)&0xFF), char((op>>24)&0xFF), '\0' };
                args[numargs++].setstr(newstring(s));
                continue;
            }
            case Code_Val|Ret_Null:
            case Code_ValI|Ret_Null: args[numargs++].setnull(); continue;
            case Code_Val|Ret_Integer: args[numargs++].setint(int(*code++)); continue;
            case Code_ValI|Ret_Integer: args[numargs++].setint(int(op)>>8); continue;
            case Code_Val|Ret_Float: args[numargs++].setfloat(*(const float *)code++); continue;
            case Code_ValI|Ret_Float: args[numargs++].setfloat(float(int(op)>>8)); continue;

            case Code_Dup|Ret_Null: args[numargs-1].getval(args[numargs]); numargs++; continue;
            case Code_Dup|Ret_Integer: args[numargs].setint(args[numargs-1].getint()); numargs++; continue;
            case Code_Dup|Ret_Float: args[numargs].setfloat(args[numargs-1].getfloat()); numargs++; continue;
            case Code_Dup|Ret_String: args[numargs].setstr(newstring(args[numargs-1].getstr())); numargs++; continue;

            case Code_Force|Ret_String: forcestr(args[numargs-1]); continue;
            case Code_Force|Ret_Integer: forceint(args[numargs-1]); continue;
            case Code_Force|Ret_Float: forcefloat(args[numargs-1]); continue;

            case Code_Result|Ret_Null:
                freearg(result);
                result = args[--numargs];
                continue;
            case Code_Result|Ret_String: case Code_Result|Ret_Integer: case Code_Result|Ret_Float:
                freearg(result);
                result = args[--numargs];
                forcearg(result, op&Code_RetMask);
                continue;

            case Code_Empty|Ret_Null: args[numargs++].setcode(emptyblock[Value_Null]+1); break;
            case Code_Empty|Ret_String: args[numargs++].setcode(emptyblock[Value_String]+1); break;
            case Code_Empty|Ret_Integer: args[numargs++].setcode(emptyblock[Value_Integer]+1); break;
            case Code_Empty|Ret_Float: args[numargs++].setcode(emptyblock[Value_Float]+1); break;
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
                    case Value_Integer: buf.reserve(8); buf.add(Code_Start); compileint(buf, arg.i); buf.add(Code_Result); buf.add(Code_Exit); break;
                    case Value_Float: buf.reserve(8); buf.add(Code_Start); compilefloat(buf, arg.f); buf.add(Code_Result); buf.add(Code_Exit); break;
                    case Value_String: case Value_Macro: case Value_CString: buf.reserve(64); compilemain(buf, arg.s); freearg(arg); break;
                    default: buf.reserve(8); buf.add(Code_Start); compilenull(buf); buf.add(Code_Result); buf.add(Code_Exit); break;
                }
                arg.setcode(buf.disown()+1);
                continue;
            }
            case Code_Cond:
            {
                tagval &arg = args[numargs-1];
                switch(arg.type)
                {
                    case Value_String: case Value_Macro: case Value_CString:
                        if(arg.s[0])
                        {
                            vector<uint> buf;
                            buf.reserve(64);
                            compilemain(buf, arg.s);
                            freearg(arg);
                            arg.setcode(buf.disown()+1);
                        }
                        else forcenull(arg);
                        break;
                }
                continue;
            }

            case Code_Ident:
                args[numargs++].setident(identmap[op>>8]);
                continue;
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
                ident *id = arg.type == Value_String || arg.type == Value_Macro || arg.type == Value_CString ? newident(arg.s, Idf_Unknown) : dummyident;
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
                    if(arg.type != Value_String && arg.type != Value_Macro && arg.type != Value_CString) continue; \
                    ident *id = idents.access(arg.s); \
                    if(id) switch(id->type) \
                    { \
                        case Id_Alias: \
                            if(id->flags&Idf_Unknown) break; \
                            freearg(arg); \
                            if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index))) { nval; continue; } \
                            aval; \
                            continue; \
                        case Id_StringVar: freearg(arg); sval; continue; \
                        case Id_Var: freearg(arg); ival; continue; \
                        case Id_FloatVar: freearg(arg); fval; continue; \
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
                        default: freearg(arg); nval; continue; \
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
                    if(id->flags&Idf_Unknown) debugcode("unknown alias lookup: %s", id->name); \
                    aval; \
                    continue; \
                }
                LOOKUP(args[numargs++].setstr(newstring(id->getstr())));
            case Code_LookupArg|Ret_String:
                #define LOOKUPARG(aval, nval) { \
                    ident *id = identmap[op>>8]; \
                    if(!(aliasstack->usedargs&(1<<id->index))) { nval; continue; } \
                    aval; \
                    continue; \
                }
                LOOKUPARG(args[numargs++].setstr(newstring(id->getstr())), args[numargs++].setstr(newstring("")));
            case Code_LookupU|Ret_Integer:
                LOOKUPU(arg.setint(id->getint()),
                        arg.setint(parseint(*id->storage.s)),
                        arg.setint(*id->storage.i),
                        arg.setint(int(*id->storage.f)),
                        arg.setint(0));
            case Code_Lookup|Ret_Integer:
                LOOKUP(args[numargs++].setint(id->getint()));
            case Code_LookupArg|Ret_Integer:
                LOOKUPARG(args[numargs++].setint(id->getint()), args[numargs++].setint(0));
            case Code_LookupU|Ret_Float:
                LOOKUPU(arg.setfloat(id->getfloat()),
                        arg.setfloat(parsefloat(*id->storage.s)),
                        arg.setfloat(float(*id->storage.i)),
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

            case Code_StrVar|Ret_String: case Code_StrVar|Ret_Null: args[numargs++].setstr(newstring(*identmap[op>>8]->storage.s)); continue;
            case Code_StrVar|Ret_Integer: args[numargs++].setint(parseint(*identmap[op>>8]->storage.s)); continue;
            case Code_StrVar|Ret_Float: args[numargs++].setfloat(parsefloat(*identmap[op>>8]->storage.s)); continue;
            case Code_StrVarM: args[numargs++].setcstr(*identmap[op>>8]->storage.s); continue;
            case Code_StrVar1: setsvarchecked(identmap[op>>8], args[--numargs].s); freearg(args[numargs]); continue;

            case Code_IntVar|Ret_Integer: case Code_IntVar|Ret_Null: args[numargs++].setint(*identmap[op>>8]->storage.i); continue;
            case Code_IntVar|Ret_String: args[numargs++].setstr(newstring(intstr(*identmap[op>>8]->storage.i))); continue;
            case Code_IntVar|Ret_Float: args[numargs++].setfloat(float(*identmap[op>>8]->storage.i)); continue;
            case Code_IntVar1: setvarchecked(identmap[op>>8], args[--numargs].i); continue;
            case Code_IntVar2: numargs -= 2; setvarchecked(identmap[op>>8], (args[numargs].i<<16)|(args[numargs+1].i<<8)); continue;
            case Code_IntVar3: numargs -= 3; setvarchecked(identmap[op>>8], (args[numargs].i<<16)|(args[numargs+1].i<<8)|args[numargs+2].i); continue;

            case Code_FloatVar|Ret_Float: case Code_FloatVar|Ret_Null: args[numargs++].setfloat(*identmap[op>>8]->storage.f); continue;
            case Code_FloatVar|Ret_String: args[numargs++].setstr(newstring(floatstr(*identmap[op>>8]->storage.f))); continue;
            case Code_FloatVar|Ret_Integer: args[numargs++].setint(int(*identmap[op>>8]->storage.f)); continue;
            case Code_FloatVar1: setfvarchecked(identmap[op>>8], args[--numargs].f); continue;

            #define OFFSETARG(n) offset+n
            case Code_Com|Ret_Null: case Code_Com|Ret_String: case Code_Com|Ret_Float: case Code_Com|Ret_Integer:
            {
                ident *id = identmap[op>>8];
                int offset = numargs-id->numargs;
                forcenull(result);
                CALLCOM(id->numargs)
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }
#ifndef STANDALONE
            case Code_ComD|Ret_Null: case Code_ComD|Ret_String: case Code_ComD|Ret_Float: case Code_ComD|Ret_Integer:
            {
                ident *id = identmap[op>>8];
                int offset = numargs-(id->numargs-1);
                addreleaseaction(id, &args[offset], id->numargs-1);
                CALLCOM(id->numargs)
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }
#endif
            #undef OFFSETARG

            case Code_ComV|Ret_Null: case Code_ComV|Ret_String: case Code_ComV|Ret_Float: case Code_ComV|Ret_Integer:
            {
                ident *id = identmap[op>>13];
                int callargs = (op>>8)&0x1F, offset = numargs-callargs;
                forcenull(result);
                ((comfunv)id->fun)(&args[offset], callargs);
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }
            case Code_ComC|Ret_Null: case Code_ComC|Ret_String: case Code_ComC|Ret_Float: case Code_ComC|Ret_Integer:
            {
                ident *id = identmap[op>>13];
                int callargs = (op>>8)&0x1F, offset = numargs-callargs;
                forcenull(result);
                {
                    vector<char> buf;
                    buf.reserve(MAXSTRLEN);
                    ((comfun1)id->fun)(conc(buf, &args[offset], callargs, true));
                }
                forcearg(result, op&Code_RetMask);
                freeargs(args, numargs, offset);
                continue;
            }

            case Code_ConC|Ret_Null: case Code_ConC|Ret_String: case Code_ConC|Ret_Float: case Code_ConC|Ret_Integer:
            case Code_ConCW|Ret_Null: case Code_ConCW|Ret_String: case Code_ConCW|Ret_Float: case Code_ConCW|Ret_Integer:
            {
                int numconc = op>>8;
                char *s = conc(&args[numargs-numconc], numconc, (op&Code_OpMask)==Code_ConC);
                freeargs(args, numargs, numargs-numconc);
                args[numargs].setstr(s);
                forcearg(args[numargs], op&Code_RetMask);
                numargs++;
                continue;
            }

            case Code_ConCM|Ret_Null: case Code_ConCM|Ret_String: case Code_ConCM|Ret_Float: case Code_ConCM|Ret_Integer:
            {
                int numconc = op>>8;
                char *s = conc(&args[numargs-numconc], numconc, false);
                freeargs(args, numargs, numargs-numconc);
                result.setstr(s);
                forcearg(result, op&Code_RetMask);
                continue;
            }

            case Code_Alias:
                setalias(*identmap[op>>8], args[--numargs]);
                continue;
            case Code_AliasArg:
                setarg(*identmap[op>>8], args[--numargs]);
                continue;
            case Code_AliasU:
                numargs -= 2;
                setalias(args[numargs].getstr(), args[numargs+1]);
                freearg(args[numargs]);
                continue;

            #define SKIPARGS(offset) offset
            case Code_Call|Ret_Null: case Code_Call|Ret_String: case Code_Call|Ret_Float: case Code_Call|Ret_Integer:
            {
                #define FORCERESULT { \
                    freeargs(args, numargs, SKIPARGS(offset)); \
                    forcearg(result, op&Code_RetMask); \
                    continue; \
                }
                #define CALLALIAS { \
                    identstack argstack[Max_Args]; \
                    for(int i = 0; i < callargs; i++) \
                        pusharg(*identmap[i], args[offset + i], argstack[i]); \
                    int oldargs = _numargs; \
                    _numargs = callargs; \
                    int oldflags = identflags; \
                    identflags |= id->flags&Idf_Overridden; \
                    identlink aliaslink = { id, aliasstack, (1<<callargs)-1, argstack }; \
                    aliasstack = &aliaslink; \
                    if(!id->code) id->code = compilecode(id->getstr()); \
                    uint *code = id->code; \
                    code[0] += 0x100; \
                    runcode(code+1, result); \
                    code[0] -= 0x100; \
                    if(int(code[0]) < 0x100) delete[] code; \
                    aliasstack = aliaslink.next; \
                    identflags = oldflags; \
                    for(int i = 0; i < callargs; i++) \
                        poparg(*identmap[i]); \
                    for(int argmask = aliaslink.usedargs&(~0U<<callargs), i = callargs; argmask; i++) \
                        if(argmask&(1<<i)) { poparg(*identmap[i]); argmask &= ~(1<<i); } \
                    forcearg(result, op&Code_RetMask); \
                    _numargs = oldargs; \
                    numargs = SKIPARGS(offset); \
                }
                forcenull(result);
                ident *id = identmap[op>>13];
                int callargs = (op>>8)&0x1F, offset = numargs-callargs;
                if(id->flags&Idf_Unknown)
                {
                    debugcode("unknown command: %s", id->name);
                    FORCERESULT;
                }
                CALLALIAS;
                continue;
            }
            case Code_CallArg|Ret_Null: case Code_CallArg|Ret_String: case Code_CallArg|Ret_Float: case Code_CallArg|Ret_Integer:
            {
                forcenull(result);
                ident *id = identmap[op>>13];
                int callargs = (op>>8)&0x1F, offset = numargs-callargs;
                if(!(aliasstack->usedargs&(1<<id->index))) FORCERESULT;
                CALLALIAS;
                continue;
            }
            #undef SKIPARGS

            #define SKIPARGS(offset) offset-1
            case Code_CallU|Ret_Null: case Code_CallU|Ret_String: case Code_CallU|Ret_Float: case Code_CallU|Ret_Integer:
            {
                int callargs = op>>8, offset = numargs-callargs;
                tagval &idarg = args[offset-1];
                if(idarg.type != Value_String && idarg.type != Value_Macro && idarg.type != Value_CString)
                {
                litval:
                    freearg(result);
                    result = idarg;
                    forcearg(result, op&Code_RetMask);
                    while(--numargs >= offset) freearg(args[numargs]);
                    continue;
                }
                ident *id = idents.access(idarg.s);
                if(!id)
                {
                noid:
                    if(checknumber(idarg.s)) goto litval;
                    debugcode("unknown command: %s", idarg.s);
                    forcenull(result);
                    FORCERESULT;
                }
                forcenull(result);
                switch(id->type)
                {
                    default:
                        if(!id->fun) FORCERESULT;
                        // fall-through
                    case Id_Command:
                        freearg(idarg);
                        callcommand(id, &args[offset], callargs);
                        forcearg(result, op&Code_RetMask);
                        numargs = offset - 1;
                        continue;
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
                        if(callargs <= 0) printvar(id); else setvarchecked(id, &args[offset], callargs);
                        FORCERESULT;
                    case Id_FloatVar:
                        if(callargs <= 0) printvar(id); else setfvarchecked(id, forcefloat(args[offset]));
                        FORCERESULT;
                    case Id_StringVar:
                        if(callargs <= 0) printvar(id); else setsvarchecked(id, forcestr(args[offset]));
                        FORCERESULT;
                    case Id_Alias:
                        if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index))) FORCERESULT;
                        if(id->valtype==Value_Null) goto noid;
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
    if(int(code[0]) >= 0x100) code.disown();
}

void executeret(ident *id, tagval *args, int numargs, bool lookup, tagval &result)
{
    result.setnull();
    ++rundepth;
    tagval *prevret = commandret;
    commandret = &result;
    if(rundepth > MAXRUNDEPTH) debugcode("exceeded recursion limit");
    else if(id) switch(id->type)
    {
        default:
            if(!id->fun) break;
            // fall-through
        case Id_Command:
            if(numargs < id->numargs)
            {
                tagval buf[Max_Args];
                memcpy(buf, args, numargs*sizeof(tagval));
                callcommand(id, buf, numargs, lookup);
            }
            else callcommand(id, args, numargs, lookup);
            numargs = 0;
            break;
        case Id_Var:
            if(numargs <= 0) printvar(id); else setvarchecked(id, args, numargs);
            break;
        case Id_FloatVar:
            if(numargs <= 0) printvar(id); else setfvarchecked(id, forcefloat(args[0]));
            break;
        case Id_StringVar:
            if(numargs <= 0) printvar(id); else setsvarchecked(id, forcestr(args[0]));
            break;
        case Id_Alias:
            if(id->index < Max_Args && !(aliasstack->usedargs&(1<<id->index))) break;
            if(id->valtype==Value_Null) break;
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
    freeargs(args, numargs, 0);
    commandret = prevret;
    --rundepth;
}

char *executestr(const uint *code)
{
    tagval result;
    runcode(code, result);
    if(result.type == Value_Null) return NULL;
    forcestr(result);
    return result.s;
}

char *executestr(const char *p)
{
    tagval result;
    executeret(p, result);
    if(result.type == Value_Null) return NULL;
    forcestr(result);
    return result.s;
}

char *executestr(ident *id, tagval *args, int numargs, bool lookup)
{
    tagval result;
    executeret(id, args, numargs, lookup, result);
    if(result.type == Value_Null) return NULL;
    forcestr(result);
    return result.s;
}

char *execidentstr(const char *name, bool lookup)
{
    ident *id = idents.access(name);
    return id ? executestr(id, NULL, 0, lookup) : NULL;
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
    if(int(code[0]) >= 0x100) code.disown();
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
    return id ? execute(id, NULL, 0, lookup) : noid;
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
    return id ? executefloat(id, NULL, 0, lookup) : noid;
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
    return id ? executebool(id, NULL, 0, lookup) : noid;
}

bool execfile(const char *cfgfile, bool msg)
{
    string s;
    copystring(s, cfgfile);
    char *buf = loadfile(path(s), NULL);
    if(!buf)
    {
        if(msg) conoutf(Console_Error, "could not read \"%s\"", cfgfile);
        return false;
    }
    const char *oldsourcefile = sourcefile, *oldsourcestr = sourcestr;
    sourcefile = cfgfile;
    sourcestr = buf;
    execute(buf);
    sourcefile = oldsourcefile;
    sourcestr = oldsourcestr;
    delete[] buf;
    return true;
}
ICOMMAND(exec, "sb", (char *file, int *msg), intret(execfile(file, *msg != 0) ? 1 : 0));

const char *escapestring(const char *s)
{
    stridx = (stridx + 1)%4;
    vector<char> &buf = strbuf[stridx];
    buf.setsize(0);
    buf.add('"');
    for(; *s; s++) switch(*s)
    {
        case '\n': buf.put("^n", 2); break;
        case '\t': buf.put("^t", 2); break;
        case '\f': buf.put("^f", 2); break;
        case '"': buf.put("^\"", 2); break;
        case '^': buf.put("^^", 2); break;
        default: buf.add(*s); break;
    }
    buf.put("\"\0", 2);
    return buf.getbuf();
}

ICOMMAND(escape, "s", (char *s), result(escapestring(s)));
ICOMMAND(unescape, "s", (char *s),
{
    int len = strlen(s);
    char *d = newstring(len);
    unescapestring(d, s, &s[len]);
    stringret(d);
});

const char *escapeid(const char *s)
{
    const char *end = s + strcspn(s, "\"/;()[]@ \f\t\r\n\0");
    return *end ? escapestring(s) : s;
}

bool validateblock(const char *s)
{
    const int maxbrak = 100;
    static char brakstack[maxbrak];
    int brakdepth = 0;
    for(; *s; s++) switch(*s)
    {
        case '[': case '(': if(brakdepth >= maxbrak) return false; brakstack[brakdepth++] = *s; break;
        case ']': if(brakdepth <= 0 || brakstack[--brakdepth] != '[') return false; break;
        case ')': if(brakdepth <= 0 || brakstack[--brakdepth] != '(') return false; break;
        case '"': s = parsestring(s + 1); if(*s != '"') return false; break;
        case '/': if(s[1] == '/') return false; break;
        case '@': case '\f': return false;
    }
    return brakdepth == 0;
}

#ifndef STANDALONE
void writecfg(const char *name)
{
    stream *f = openutf8file(path(name && name[0] ? name : game::savedconfig(), true), "w");
    if(!f) return;
    f->printf("// automatically written on exit, DO NOT MODIFY\n// delete this file to have %s overwrite these settings\n// modify settings in game, or put settings in %s to override anything\n\n", game::defaultconfig(), game::autoexec());
    game::writeclientinfo(f);
    f->printf("\n");
    writecrosshairs(f);
    vector<ident *> ids;
    ENUMERATE(idents, ident, id, ids.add(&id));
    ids.sortname();
    for(int i = 0; i < ids.length(); i++)
    {
        ident &id = *ids[i];
        if(id.flags&Idf_Persist) 
        {
            switch(id.type)
            {
                case Id_Var:
                {
                    f->printf("%s %d\n", escapeid(id), *id.storage.i);
                    break;
                }
                case Id_FloatVar:
                {
                    f->printf("%s %s\n", escapeid(id), floatstr(*id.storage.f));
                    break;
                }
                case Id_StringVar:
                {
                    f->printf("%s %s\n", escapeid(id), escapestring(*id.storage.s));
                    break;
                }
            }
        }
    }
    f->printf("\n");
    writebinds(f);
    f->printf("\n");
    for(int i = 0; i < ids.length(); i++)
    {
        ident &id = *ids[i];
        if(id.type==Id_Alias && id.flags&Idf_Persist && !(id.flags&Idf_Overridden)) switch(id.valtype)
        {
        case Value_String:
            if(!id.val.s[0]) break;
            if(!validateblock(id.val.s)) { f->printf("%s = %s\n", escapeid(id), escapestring(id.val.s)); break; }
        case Value_Float:
        case Value_Integer:
            f->printf("%s = [%s]\n", escapeid(id), id.getstr()); break;
        }
    }
    f->printf("\n");
    writecompletions(f);
    delete f;
}

COMMAND(writecfg, "s");
#endif

void changedvars()
{
    vector<ident *> ids;
    ENUMERATE(idents, ident, id, if(id.flags&Idf_Overridden) ids.add(&id));
    ids.sortname();
    for(int i = 0; i < ids.length(); i++)
    {
        printvar(ids[i]);
    }
}
COMMAND(changedvars, "");

// below the commands that implement a small imperative language. thanks to the semantics of
// () and [] expressions, any control construct can be defined trivially.

static string retbuf[4];
static int retidx = 0;

const char *intstr(int v)
{
    retidx = (retidx + 1)%4;
    intformat(retbuf[retidx], v);
    return retbuf[retidx];
}

void intret(int v)
{
    commandret->setint(v);
}

const char *floatstr(float v)
{
    retidx = (retidx + 1)%4;
    floatformat(retbuf[retidx], v);
    return retbuf[retidx];
}

void floatret(float v)
{
    commandret->setfloat(v);
}

const char *numberstr(double v)
{
    retidx = (retidx + 1)%4;
    numberformat(retbuf[retidx], v);
    return retbuf[retidx];
}

void numberret(double v)
{
    int i = int(v);
    if(v == i) commandret->setint(i);
    else commandret->setfloat(v);
}

#undef ICOMMANDNAME
#define ICOMMANDNAME(name) _stdcmd
#undef ICOMMANDSNAME
#define ICOMMANDSNAME _stdcmd

ICOMMANDK(do, Id_Do, "e", (uint *body), executeret(body, *commandret));

static void doargs(uint *body)
{
    if(aliasstack != &noalias)
    {
        UNDOARGS
        executeret(body, *commandret);
        REDOARGS
    }
    else executeret(body, *commandret);
}
COMMANDK(doargs, Id_DoArgs, "e");

ICOMMANDK(if, Id_If, "tee", (tagval *cond, uint *t, uint *f), executeret(getbool(*cond) ? t : f, *commandret));
ICOMMAND(?, "tTT", (tagval *cond, tagval *t, tagval *f), result(*(getbool(*cond) ? t : f)));

ICOMMAND(pushif, "rTe", (ident *id, tagval *v, uint *code),
{
    if(id->type != Id_Alias || id->index < Max_Args) return;
    if(getbool(*v))
    {
        identstack stack;
        pusharg(*id, *v, stack);
        v->type = Value_Null;
        id->flags &= ~Idf_Unknown;
        executeret(code, *commandret);
        poparg(*id);
    }
});

void loopiter(ident *id, identstack &stack, const tagval &v)
{
    if(id->stack != &stack)
    {
        pusharg(*id, v, stack);
        id->flags &= ~Idf_Unknown;
    }
    else
    {
        if(id->valtype == Value_String) delete[] id->val.s;
        cleancode(*id);
        id->setval(v);
    }
}

void loopend(ident *id, identstack &stack)
{
    if(id->stack == &stack) poparg(*id);
}

static inline void setiter(ident &id, int i, identstack &stack)
{
    if(id.stack == &stack)
    {
        if(id.valtype != Value_Integer)
        {
            if(id.valtype == Value_String) delete[] id.val.s;
            cleancode(id);
            id.valtype = Value_Integer;
        }
        id.val.i = i;
    }
    else
    {
        tagval t;
        t.setint(i);
        pusharg(id, t, stack);
        id.flags &= ~Idf_Unknown;
    }
}

static inline void doloop(ident &id, int offset, int n, int step, uint *body)
{
    if(n <= 0 || id.type != Id_Alias) return;
    identstack stack;
    for(int i = 0; i < n; ++i)
    {
        setiter(id, offset + i*step, stack);
        execute(body);
    }
    poparg(id);
}
ICOMMAND(loop, "rie", (ident *id, int *n, uint *body), doloop(*id, 0, *n, 1, body));
ICOMMAND(loop+, "riie", (ident *id, int *offset, int *n, uint *body), doloop(*id, *offset, *n, 1, body));
ICOMMAND(loop*, "riie", (ident *id, int *step, int *n, uint *body), doloop(*id, 0, *n, *step, body));
ICOMMAND(loop+*, "riiie", (ident *id, int *offset, int *step, int *n, uint *body), doloop(*id, *offset, *n, *step, body));

static inline void loopwhile(ident &id, int offset, int n, int step, uint *cond, uint *body)
{
    if(n <= 0 || id.type!=Id_Alias) return;
    identstack stack;
    for(int i = 0; i < n; ++i)
    {
        setiter(id, offset + i*step, stack);
        if(!executebool(cond)) break;
        execute(body);
    }
    poparg(id);
}
ICOMMAND(loopwhile, "riee", (ident *id, int *n, uint *cond, uint *body), loopwhile(*id, 0, *n, 1, cond, body));
ICOMMAND(loopwhile+, "riiee", (ident *id, int *offset, int *n, uint *cond, uint *body), loopwhile(*id, *offset, *n, 1, cond, body));
ICOMMAND(loopwhile*, "riiee", (ident *id, int *step, int *n, uint *cond, uint *body), loopwhile(*id, 0, *n, *step, cond, body));
ICOMMAND(loopwhile+*, "riiiee", (ident *id, int *offset, int *step, int *n, uint *cond, uint *body), loopwhile(*id, *offset, *n, *step, cond, body));

ICOMMAND(while, "ee", (uint *cond, uint *body), while(executebool(cond)) execute(body));

static inline void loopconc(ident &id, int offset, int n, int step, uint *body, bool space)
{
    if(n <= 0 || id.type != Id_Alias) return;
    identstack stack;
    vector<char> s;
    for(int i = 0; i < n; ++i)
    {
        setiter(id, offset + i*step, stack);
        tagval v;
        executeret(body, v);
        const char *vstr = v.getstr();
        int len = strlen(vstr);
        if(space && i) s.add(' ');
        s.put(vstr, len);
        freearg(v);
    }
    if(n > 0) poparg(id);
    s.add('\0');
    commandret->setstr(s.disown());
}
ICOMMAND(loopconcat, "rie", (ident *id, int *n, uint *body), loopconc(*id, 0, *n, 1, body, true));
ICOMMAND(loopconcat+, "riie", (ident *id, int *offset, int *n, uint *body), loopconc(*id, *offset, *n, 1, body, true));
ICOMMAND(loopconcat*, "riie", (ident *id, int *step, int *n, uint *body), loopconc(*id, 0, *n, *step, body, true));
ICOMMAND(loopconcat+*, "riiie", (ident *id, int *offset, int *step, int *n, uint *body), loopconc(*id, *offset, *n, *step, body, true));
ICOMMAND(loopconcatword, "rie", (ident *id, int *n, uint *body), loopconc(*id, 0, *n, 1, body, false));
ICOMMAND(loopconcatword+, "riie", (ident *id, int *offset, int *n, uint *body), loopconc(*id, *offset, *n, 1, body, false));
ICOMMAND(loopconcatword*, "riie", (ident *id, int *step, int *n, uint *body), loopconc(*id, 0, *n, *step, body, false));
ICOMMAND(loopconcatword+*, "riiie", (ident *id, int *offset, int *step, int *n, uint *body), loopconc(*id, *offset, *n, *step, body, false));

void concat(tagval *v, int n)
{
    commandret->setstr(conc(v, n, true));
}
COMMAND(concat, "V");

void concatword(tagval *v, int n)
{
    commandret->setstr(conc(v, n, false));
}
COMMAND(concatword, "V");

void append(ident *id, tagval *v, bool space)
{
    if(id->type != Id_Alias || v->type == Value_Null) return;
    tagval r;
    const char *prefix = id->getstr();
    if(prefix[0]) r.setstr(conc(v, 1, space, prefix));
    else v->getval(r);
    if(id->index < Max_Args) setarg(*id, r); else setalias(*id, r);
}
ICOMMAND(append, "rt", (ident *id, tagval *v), append(id, v, true));
ICOMMAND(appendword, "rt", (ident *id, tagval *v), append(id, v, false));

void result(tagval &v)
{
    *commandret = v;
    v.type = Value_Null;
}

void stringret(char *s)
{
    commandret->setstr(s);
}

void result(const char *s)
{
    commandret->setstr(newstring(s));
}

ICOMMANDK(result, Id_Result, "T", (tagval *v),
{
    *commandret = *v;
    v->type = Value_Null;
});

void format(tagval *args, int numargs)
{
    vector<char> s;
    const char *f = args[0].getstr();
    while(*f)
    {
        int c = *f++;
        if(c == '%')
        {
            int i = *f++;
            if(i >= '1' && i <= '9')
            {
                i -= '0';
                const char *sub = i < numargs ? args[i].getstr() : "";
                while(*sub) s.add(*sub++);
            }
            else s.add(i);
        }
        else s.add(c);
    }
    s.add('\0');
    commandret->setstr(s.disown());
}
COMMAND(format, "V");

static const char *liststart = NULL, *listend = NULL, *listquotestart = NULL, *listquoteend = NULL;

static inline void skiplist(const char *&p)
{
    for(;;)
    {
        p += strspn(p, " \t\r\n");
        if(p[0]!='/' || p[1]!='/') break;
        p += strcspn(p, "\n\0");
    }
}

static bool parselist(const char *&s, const char *&start = liststart, const char *&end = listend, const char *&quotestart = listquotestart, const char *&quoteend = listquoteend)
{
    skiplist(s);
    switch(*s)
    {
        case '"': quotestart = s++; start = s; s = parsestring(s); end = s; if(*s == '"') s++; quoteend = s; break;
        case '(': case '[':
            quotestart = s;
            start = s+1;
            for(int braktype = *s++, brak = 1;;)
            {
                s += strcspn(s, "\"/;()[]\0");
                int c = *s++;
                switch(c)
                {
                    case '\0': s--; quoteend = end = s; return true;
                    case '"': s = parsestring(s); if(*s == '"') s++; break;
                    case '/': if(*s == '/') s += strcspn(s, "\n\0"); break;
                    case '(': case '[': if(c == braktype) brak++; break;
                    case ')': if(braktype == '(' && --brak <= 0) goto endblock; break;
                    case ']': if(braktype == '[' && --brak <= 0) goto endblock; break;
                }
            }
        endblock:
            end = s-1;
            quoteend = s;
            break;
        case '\0': case ')': case ']': return false;
        default: quotestart = start = s; s = parseword(s); quoteend = end = s; break;
    }
    skiplist(s);
    if(*s == ';') s++;
    return true;
}

static inline char *listelem(const char *start = liststart, const char *end = listend, const char *quotestart = listquotestart)
{
    size_t len = end-start;
    char *s = newstring(len);
    if(*quotestart == '"') unescapestring(s, start, end);
    else { memcpy(s, start, len); s[len] = '\0'; }
    return s;
}

void explodelist(const char *s, vector<char *> &elems, int limit)
{
    const char *start, *end, *qstart;
    while((limit < 0 || elems.length() < limit) && parselist(s, start, end, qstart))
        elems.add(listelem(start, end, qstart));
}

char *indexlist(const char *s, int pos)
{
    for(int i = 0; i < pos; ++i)
    {
        if(!parselist(s)) return newstring("");
    }
    const char *start, *end, *qstart;
    return parselist(s, start, end, qstart) ? listelem(start, end, qstart) : newstring("");
}

int listlen(const char *s)
{
    int n = 0;
    while(parselist(s)) n++;
    return n;
}
ICOMMAND(listlen, "s", (char *s), intret(listlen(s)));

void at(tagval *args, int numargs)
{
    if(!numargs) return;
    const char *start = args[0].getstr(), *end = start + strlen(start), *qstart = "";
    for(int i = 1; i < numargs; i++)
    {
        const char *list = start;
        int pos = args[i].getint();
        for(; pos > 0; pos--) if(!parselist(list)) break;
        if(pos > 0 || !parselist(list, start, end, qstart)) start = end = qstart = "";
    }
    commandret->setstr(listelem(start, end, qstart));
}
COMMAND(at, "si1V");

void substr(char *s, int *start, int *count, int *numargs)
{
    int len = strlen(s), offset = clamp(*start, 0, len);
    commandret->setstr(newstring(&s[offset], *numargs >= 3 ? clamp(*count, 0, len - offset) : len - offset));
}
COMMAND(substr, "siiN");

void sublist(const char *s, int *skip, int *count, int *numargs)
{
    int offset = max(*skip, 0), len = *numargs >= 3 ? max(*count, 0) : -1;
    for(int i = 0; i < offset; ++i)
    {
        if(!parselist(s))
        {
            break;
        }
    }
    if(len < 0) { if(offset > 0) skiplist(s); commandret->setstr(newstring(s)); return; }
    const char *list = s, *start, *end, *qstart, *qend = s;
    if(len > 0 && parselist(s, start, end, list, qend)) while(--len > 0 && parselist(s, start, end, qstart, qend));
    commandret->setstr(newstring(list, qend - list));
}
COMMAND(sublist, "siiN");

ICOMMAND(stripcolors, "s", (char *s),
{
    int len = strlen(s);
    char *d = newstring(len);
    filtertext(d, s, true, false, len);
    stringret(d);
});

static inline void setiter(ident &id, char *val, identstack &stack)
{
    if(id.stack == &stack)
    {
        if(id.valtype == Value_String) delete[] id.val.s;
        else id.valtype = Value_String;
        cleancode(id);
        id.val.s = val;
    }
    else
    {
        tagval t;
        t.setstr(val);
        pusharg(id, t, stack);
        id.flags &= ~Idf_Unknown;
    }
}

void listfind(ident *id, const char *list, const uint *body)
{
    if(id->type!=Id_Alias) { intret(-1); return; }
    identstack stack;
    int n = -1;
    for(const char *s = list, *start, *end; parselist(s, start, end);)
    {
        ++n;
        setiter(*id, newstring(start, end-start), stack);
        if(executebool(body)) { intret(n); goto found; }
    }
    intret(-1);
found:
    if(n >= 0) poparg(*id);
}
COMMAND(listfind, "rse");

void listassoc(ident *id, const char *list, const uint *body)
{
    if(id->type!=Id_Alias) return;
    identstack stack;
    int n = -1;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end);)
    {
        ++n;
        setiter(*id, newstring(start, end-start), stack);
        if(executebool(body)) { if(parselist(s, start, end, qstart)) stringret(listelem(start, end, qstart)); break; }
        if(!parselist(s)) break;
    }
    if(n >= 0) poparg(*id);
}
COMMAND(listassoc, "rse");

#define LISTFIND(name, fmt, type, init, cmp) \
    ICOMMAND(name, "s" fmt "i", (char *list, type *val, int *skip), \
    { \
        int n = 0; \
        init; \
        for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n++) \
        { \
            if(cmp) { intret(n); return; } \
            for(int i = 0; i < int(*skip); ++i) { if(!parselist(s)) goto notfound; n++; } \
        } \
    notfound: \
        intret(-1); \
    });
LISTFIND(listfind=, "i", int, , parseint(start) == *val);
LISTFIND(listfind=f, "f", float, , parsefloat(start) == *val);
LISTFIND(listfind=s, "s", char, int len = (int)strlen(val), int(end-start) == len && !memcmp(start, val, len));

#define LISTASSOC(name, fmt, type, init, cmp) \
    ICOMMAND(name, "s" fmt, (char *list, type *val), \
    { \
        init; \
        for(const char *s = list, *start, *end, *qstart; parselist(s, start, end);) \
        { \
            if(cmp) { if(parselist(s, start, end, qstart)) stringret(listelem(start, end, qstart)); return; } \
            if(!parselist(s)) break; \
        } \
    });
LISTASSOC(listassoc=, "i", int, , parseint(start) == *val);
LISTASSOC(listassoc=f, "f", float, , parsefloat(start) == *val);
LISTASSOC(listassoc=s, "s", char, int len = (int)strlen(val), int(end-start) == len && !memcmp(start, val, len));

void looplist(ident *id, const char *list, const uint *body)
{
    if(id->type!=Id_Alias) return;
    identstack stack;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n++)
    {
        setiter(*id, listelem(start, end, qstart), stack);
        execute(body);
    }
    if(n) poparg(*id);
}
COMMAND(looplist, "rse");

void looplist2(ident *id, ident *id2, const char *list, const uint *body)
{
    if(id->type!=Id_Alias || id2->type!=Id_Alias) return;
    identstack stack, stack2;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n += 2)
    {
        setiter(*id, listelem(start, end, qstart), stack);
        setiter(*id2, parselist(s, start, end, qstart) ? listelem(start, end, qstart) : newstring(""), stack2);
        execute(body);
    }
    if(n) { poparg(*id); poparg(*id2); }
}
COMMAND(looplist2, "rrse");

void looplist3(ident *id, ident *id2, ident *id3, const char *list, const uint *body)
{
    if(id->type!=Id_Alias || id2->type!=Id_Alias || id3->type!=Id_Alias) return;
    identstack stack, stack2, stack3;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n += 3)
    {
        setiter(*id, listelem(start, end, qstart), stack);
        setiter(*id2, parselist(s, start, end, qstart) ? listelem(start, end, qstart) : newstring(""), stack2);
        setiter(*id3, parselist(s, start, end, qstart) ? listelem(start, end, qstart) : newstring(""), stack3);
        execute(body);
    }
    if(n) { poparg(*id); poparg(*id2); poparg(*id3); }
}
COMMAND(looplist3, "rrrse");

void looplistconc(ident *id, const char *list, const uint *body, bool space)
{
    if(id->type!=Id_Alias) return;
    identstack stack;
    vector<char> r;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n++)
    {
        char *val = listelem(start, end, qstart);
        setiter(*id, val, stack);

        if(n && space) r.add(' ');

        tagval v;
        executeret(body, v);
        const char *vstr = v.getstr();
        int len = strlen(vstr);
        r.put(vstr, len);
        freearg(v);
    }
    if(n) poparg(*id);
    r.add('\0');
    commandret->setstr(r.disown());
}
ICOMMAND(looplistconcat, "rse", (ident *id, char *list, uint *body), looplistconc(id, list, body, true));
ICOMMAND(looplistconcatword, "rse", (ident *id, char *list, uint *body), looplistconc(id, list, body, false));

void listfilter(ident *id, const char *list, const uint *body)
{
    if(id->type!=Id_Alias) return;
    identstack stack;
    vector<char> r;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart, *qend; parselist(s, start, end, qstart, qend); n++)
    {
        char *val = newstring(start, end-start);
        setiter(*id, val, stack);

        if(executebool(body))
        {
            if(r.length()) r.add(' ');
            r.put(qstart, qend-qstart);
        }
    }
    if(n) poparg(*id);
    r.add('\0');
    commandret->setstr(r.disown());
}
COMMAND(listfilter, "rse");

void listcount(ident *id, const char *list, const uint *body)
{
    if(id->type!=Id_Alias) return;
    identstack stack;
    int n = 0, r = 0;
    for(const char *s = list, *start, *end; parselist(s, start, end); n++)
    {
        char *val = newstring(start, end-start);
        setiter(*id, val, stack);
        if(executebool(body)) r++;
    }
    if(n) poparg(*id);
    intret(r);
}
COMMAND(listcount, "rse");

void prettylist(const char *s, const char *conj)
{
    vector<char> p;
    const char *start, *end, *qstart;
    for(int len = listlen(s), n = 0; parselist(s, start, end, qstart); n++)
    {
        if(*qstart == '"') p.advance(unescapestring(p.reserve(end - start + 1).buf, start, end));
        else p.put(start, end - start);
        if(n+1 < len)
        {
            if(len > 2 || !conj[0]) p.add(',');
            if(n+2 == len && conj[0])
            {
                p.add(' ');
                p.put(conj, strlen(conj));
            }
            p.add(' ');
        }
    }
    p.add('\0');
    commandret->setstr(p.disown());
}
COMMAND(prettylist, "ss");

int listincludes(const char *list, const char *needle, int needlelen)
{
    int offset = 0;
    for(const char *s = list, *start, *end; parselist(s, start, end);)
    {
        int len = end - start;
        if(needlelen == len && !strncmp(needle, start, len)) return offset;
        offset++;
    }
    return -1;
}
ICOMMAND(indexof, "ss", (char *list, char *elem), intret(listincludes(list, elem, strlen(elem))));

#define LISTMERGECMD(name, init, iter, filter, dir) \
    ICOMMAND(name, "ss", (const char *list, const char *elems), \
    { \
        vector<char> p; \
        init; \
        for(const char *start, *end, *qstart, *qend; parselist(iter, start, end, qstart, qend);) \
        { \
            int len = end - start; \
            if(listincludes(filter, start, len) dir 0) \
            { \
                if(!p.empty()) p.add(' '); \
                p.put(qstart, qend-qstart); \
            } \
        } \
        p.add('\0'); \
        commandret->setstr(p.disown()); \
    })

LISTMERGECMD(listdel, , list, elems, <);
LISTMERGECMD(listintersect, , list, elems, >=);
LISTMERGECMD(listunion, p.put(list, strlen(list)), elems, list, <);

void listsplice(const char *s, const char *vals, int *skip, int *count)
{
    int offset = max(*skip, 0), len = max(*count, 0);
    const char *list = s, *start, *end, *qstart, *qend = s;
    for(int i = 0; i < offset; ++i)
    {
        if(!parselist(s, start, end, qstart, qend))
        {
            break;
        }
    }
    vector<char> p;
    if(qend > list) p.put(list, qend-list);
    if(*vals)
    {
        if(!p.empty()) p.add(' ');
        p.put(vals, strlen(vals));
    }
    for(int i = 0; i < len; ++i)
    {
        if(!parselist(s))
        {
            break;
        }
    }
    skiplist(s);
    switch(*s)
    {
        case '\0': case ')': case ']': break;
        default:
            if(!p.empty()) p.add(' ');
            p.put(s, strlen(s));
            break;
    }
    p.add('\0');
    commandret->setstr(p.disown());
}
COMMAND(listsplice, "ssii");

ICOMMAND(loopfiles, "rsse", (ident *id, char *dir, char *ext, uint *body),
{
    if(id->type!=Id_Alias) return;
    identstack stack;
    vector<char *> files;
    listfiles(dir, ext[0] ? ext : NULL, files);
    files.sort();
    files.uniquedeletearrays();
    for(int i = 0; i < files.length(); i++)
    {
        setiter(*id, files[i], stack);
        execute(body);
    }
    if(files.length()) poparg(*id);
});

void findfile_(char *name)
{
    string fname;
    copystring(fname, name);
    path(fname);
    intret(
#ifndef STANDALONE
        findzipfile(fname) || 
#endif
        fileexists(fname, "e") || findfile(fname, "e") ? 1 : 0
    );
}
COMMANDN(findfile, findfile_, "s");

struct sortitem
{
    const char *str, *quotestart, *quoteend;

    int quotelength() const { return int(quoteend-quotestart); }
};

struct sortfun
{
    ident *x, *y;
    uint *body;

    bool operator()(const sortitem &xval, const sortitem &yval)
    {
        if(x->valtype != Value_CString) x->valtype = Value_CString;
        cleancode(*x);
        x->val.code = (const uint *)xval.str;
        if(y->valtype != Value_CString) y->valtype = Value_CString;
        cleancode(*y);
        y->val.code = (const uint *)yval.str;
        return executebool(body);
    }
};

void sortlist(char *list, ident *x, ident *y, uint *body, uint *unique)
{
    if(x == y || x->type != Id_Alias || y->type != Id_Alias) return;

    vector<sortitem> items;
    int clen = strlen(list), total = 0;
    char *cstr = newstring(list, clen);
    const char *curlist = list, *start, *end, *quotestart, *quoteend;
    while(parselist(curlist, start, end, quotestart, quoteend))
    {
        cstr[end - list] = '\0';
        sortitem item = { &cstr[start - list], quotestart, quoteend };
        items.add(item);
        total += item.quotelength();
    }

    if(items.empty())
    {
        commandret->setstr(cstr);
        return;
    }

    identstack xstack, ystack;
    pusharg(*x, nullval, xstack); x->flags &= ~Idf_Unknown;
    pusharg(*y, nullval, ystack); y->flags &= ~Idf_Unknown;

    int totalunique = total, numunique = items.length();
    if(body)
    {
        sortfun f = { x, y, body };
        items.sort(f);
        if((*unique&Code_OpMask) != Code_Exit)
        {
            f.body = unique;
            totalunique = items[0].quotelength();
            numunique = 1;
            for(int i = 1; i < items.length(); i++)
            {
                sortitem &item = items[i];
                if(f(items[i-1], item)) item.quotestart = NULL;
                else { totalunique += item.quotelength(); numunique++; }
            }
        }
    }
    else
    {
        sortfun f = { x, y, unique };
        totalunique = items[0].quotelength();
        numunique = 1;
        for(int i = 1; i < items.length(); i++)
        {
            sortitem &item = items[i];
            for(int j = 0; j < i; ++j)
            {
                sortitem &prev = items[j];
                if(prev.quotestart && f(item, prev)) { item.quotestart = NULL; break; }
            }
            if(item.quotestart) { totalunique += item.quotelength(); numunique++; }
        }
    }

    poparg(*x);
    poparg(*y);

    char *sorted = cstr;
    int sortedlen = totalunique + max(numunique - 1, 0);
    if(clen < sortedlen)
    {
        delete[] cstr;
        sorted = newstring(sortedlen);
    }

    int offset = 0;
    for(int i = 0; i < items.length(); i++)
    {
        sortitem &item = items[i];
        if(!item.quotestart) continue;
        int len = item.quotelength();
        if(i) sorted[offset++] = ' ';
        memcpy(&sorted[offset], item.quotestart, len);
        offset += len;
    }
    sorted[offset] = '\0';

    commandret->setstr(sorted);
}
COMMAND(sortlist, "srree");
ICOMMAND(uniquelist, "srre", (char *list, ident *x, ident *y, uint *body), sortlist(list, x, y, NULL, body));

#define MATHCMD(name, fmt, type, op, initval, unaryop) \
    ICOMMANDS(name, #fmt "1V", (tagval *args, int numargs), \
    { \
        type val; \
        if(numargs >= 2) \
        { \
            val = args[0].fmt; \
            type val2 = args[1].fmt; \
            op; \
            for(int i = 2; i < numargs; i++) { val2 = args[i].fmt; op; } \
        } \
        else { val = numargs > 0 ? args[0].fmt : initval; unaryop; } \
        type##ret(val); \
    })
#define MATHICMDN(name, op, initval, unaryop) MATHCMD(#name, i, int, val = val op val2, initval, unaryop)
#define MATHICMD(name, initval, unaryop) MATHICMDN(name, name, initval, unaryop)
#define MATHFCMDN(name, op, initval, unaryop) MATHCMD(#name "f", f, float, val = val op val2, initval, unaryop)
#define MATHFCMD(name, initval, unaryop) MATHFCMDN(name, name, initval, unaryop)

#define CMPCMD(name, fmt, type, op) \
    ICOMMANDS(name, #fmt "1V", (tagval *args, int numargs), \
    { \
        bool val; \
        if(numargs >= 2) \
        { \
            val = args[0].fmt op args[1].fmt; \
            for(int i = 2; i < numargs && val; i++) val = args[i-1].fmt op args[i].fmt; \
        } \
        else val = (numargs > 0 ? args[0].fmt : 0) op 0; \
        intret(int(val)); \
    })
#define CMPICMDN(name, op) CMPCMD(#name, i, int, op)
#define CMPICMD(name) CMPICMDN(name, name)
#define CMPFCMDN(name, op) CMPCMD(#name "f", f, float, op)
#define CMPFCMD(name) CMPFCMDN(name, name)

MATHICMD(+, 0, );
MATHICMD(*, 1, );
MATHICMD(-, 0, val = -val);
CMPICMDN(=, ==);
CMPICMD(!=);
CMPICMD(<);
CMPICMD(>);
CMPICMD(<=);
CMPICMD(>=);
MATHICMD(^, 0, val = ~val);
MATHICMDN(~, ^, 0, val = ~val);
MATHICMD(&, 0, );
MATHICMD(|, 0, );
MATHICMD(^~, 0, );
MATHICMD(&~, 0, );
MATHICMD(|~, 0, );
MATHCMD("<<", i, int, val = val2 < 32 ? val << max(val2, 0) : 0, 0, );
MATHCMD(">>", i, int, val >>= clamp(val2, 0, 31), 0, );

MATHFCMD(+, 0, );
MATHFCMD(*, 1, );
MATHFCMD(-, 0, val = -val);
CMPFCMDN(=, ==);
CMPFCMD(!=);
CMPFCMD(<);
CMPFCMD(>);
CMPFCMD(<=);
CMPFCMD(>=);

ICOMMANDK(!, Id_Not, "t", (tagval *a), intret(getbool(*a) ? 0 : 1));
ICOMMANDK(&&, Id_And, "E1V", (tagval *args, int numargs),
{
    if(!numargs)
    {
        intret(1);
    }
    else
    {
        for(int i = 0; i < numargs; ++i)
        {
            if(i) freearg(*commandret);
            if(args[i].type == Value_Code) executeret(args[i].code, *commandret);
            else *commandret = args[i];
            if(!getbool(*commandret)) break;
        }
    }
});
ICOMMANDK(||, Id_Or, "E1V", (tagval *args, int numargs),
{
    if(!numargs)
    {
        intret(0);
    }
    else 
    {
        for(int i = 0; i < numargs; ++i)
        {
            if(i) freearg(*commandret);
            if(args[i].type == Value_Code) executeret(args[i].code, *commandret);
            else *commandret = args[i];
            if(getbool(*commandret)) break;
        }
    }
});


#define DIVCMD(name, fmt, type, op) MATHCMD(#name, fmt, type, { if(val2) op; else val = 0; }, 0, )

DIVCMD(div, i, int, val /= val2);
DIVCMD(mod, i, int, val %= val2);
DIVCMD(divf, f, float, val /= val2);
DIVCMD(modf, f, float, val = fmod(val, val2));
MATHCMD("pow", f, float, val = pow(val, val2), 0, );

ICOMMAND(sin, "f", (float *a), floatret(sin(*a*RAD)));
ICOMMAND(cos, "f", (float *a), floatret(cos(*a*RAD)));
ICOMMAND(tan, "f", (float *a), floatret(tan(*a*RAD)));
ICOMMAND(asin, "f", (float *a), floatret(asin(*a)/RAD));
ICOMMAND(acos, "f", (float *a), floatret(acos(*a)/RAD));
ICOMMAND(atan, "f", (float *a), floatret(atan(*a)/RAD));
ICOMMAND(atan2, "ff", (float *y, float *x), floatret(atan2(*y, *x)/RAD));
ICOMMAND(sqrt, "f", (float *a), floatret(sqrt(*a)));
ICOMMAND(loge, "f", (float *a), floatret(log(*a)));
ICOMMAND(log2, "f", (float *a), floatret(log(*a)/M_LN2));
ICOMMAND(log10, "f", (float *a), floatret(log10(*a)));
ICOMMAND(exp, "f", (float *a), floatret(exp(*a)));

#define MINMAXCMD(name, fmt, type, op) \
    ICOMMAND(name, #fmt "1V", (tagval *args, int numargs), \
    { \
        type val = numargs > 0 ? args[0].fmt : 0; \
        for(int i = 1; i < numargs; i++) val = op(val, args[i].fmt); \
        type##ret(val); \
    })

MINMAXCMD(min, i, int, min);
MINMAXCMD(max, i, int, max);
MINMAXCMD(minf, f, float, min);
MINMAXCMD(maxf, f, float, max);

ICOMMAND(bitscan, "i", (int *n), intret(BITSCAN(*n)));

ICOMMAND(abs, "i", (int *n), intret(abs(*n)));
ICOMMAND(absf, "f", (float *n), floatret(fabs(*n)));

ICOMMAND(floor, "f", (float *n), floatret(floor(*n)));
ICOMMAND(ceil, "f", (float *n), floatret(ceil(*n)));
ICOMMAND(round, "ff", (float *n, float *k),
{
    double step = *k;
    double r = *n;
    if(step > 0)
    {
        r += step * (r < 0 ? -0.5 : 0.5);
        r -= fmod(r, step);
    }
    else r = r < 0 ? ceil(r - 0.5) : floor(r + 0.5);
    floatret(float(r));
}); 

ICOMMAND(cond, "ee2V", (tagval *args, int numargs),
{
    for(int i = 0; i < numargs; i += 2)
    {
        if(i+1 < numargs)
        {
            if(executebool(args[i].code))
            {
                executeret(args[i+1].code, *commandret);
                break;
            }
        }
        else
        {
            executeret(args[i].code, *commandret);
            break;
        }
    }
});

#define CASECOMMAND(name, fmt, type, acc, compare) \
    ICOMMAND(name, fmt "te2V", (tagval *args, int numargs), \
    { \
        type val = acc; \
        int i; \
        for(i = 1; i+1 < numargs; i += 2) \
        { \
            if(compare) \
            { \
                executeret(args[i+1].code, *commandret); \
                return; \
            } \
        } \
    })

CASECOMMAND(case, "i", int, args[0].getint(), args[i].type == Value_Null || args[i].getint() == val);
CASECOMMAND(casef, "f", float, args[0].getfloat(), args[i].type == Value_Null || args[i].getfloat() == val);
CASECOMMAND(cases, "s", const char *, args[0].getstr(), args[i].type == Value_Null || !strcmp(args[i].getstr(), val));

ICOMMAND(rnd, "ii", (int *a, int *b), intret(*a - *b > 0 ? randomint(*a - *b) + *b : *b));
ICOMMAND(rndstr, "i", (int *len),
{
    int n = clamp(*len, 0, 10000);
    char *s = newstring(n);
    for(int i = 0; i < n;)
    {
        int r = rand();
        for(int j = min(i + 4, n); i < j; i++)
        {
            s[i] = (r%255) + 1;
            r /= 255;
        }
    }
    s[n] = '\0';
    stringret(s);
});

ICOMMAND(tohex, "ii", (int *n, int *p),
{
    const int len = 20;
    char *buf = newstring(len);
    nformatstring(buf, len, "0x%.*X", max(*p, 1), *n);
    stringret(buf);
});

#define CMPSCMD(name, op) \
    ICOMMAND(name, "s1V", (tagval *args, int numargs), \
    { \
        bool val; \
        if(numargs >= 2) \
        { \
            val = strcmp(args[0].s, args[1].s) op 0; \
            for(int i = 2; i < numargs && val; i++) val = strcmp(args[i-1].s, args[i].s) op 0; \
        } \
        else val = (numargs > 0 ? args[0].s[0] : 0) op 0; \
        intret(int(val)); \
    })

CMPSCMD(strcmp, ==);
CMPSCMD(=s, ==);
CMPSCMD(!=s, !=);
CMPSCMD(<s, <);
CMPSCMD(>s, >);
CMPSCMD(<=s, <=);
CMPSCMD(>=s, >=);

ICOMMAND(echo, "C", (char *s), conoutf("\f1%s", s));
ICOMMAND(error, "C", (char *s), conoutf(Console_Error, "%s", s));
ICOMMAND(strstr, "ss", (char *a, char *b), { char *s = strstr(a, b); intret(s ? s-a : -1); });
ICOMMAND(strlen, "s", (char *s), intret(strlen(s)));
ICOMMAND(strcode, "si", (char *s, int *i), intret(*i > 0 ? (memchr(s, 0, *i) ? 0 : uchar(s[*i])) : uchar(s[0])));
ICOMMAND(codestr, "i", (int *i), { char *s = newstring(1); s[0] = char(*i); s[1] = '\0'; stringret(s); });
ICOMMAND(struni, "si", (char *s, int *i), intret(*i > 0 ? (memchr(s, 0, *i) ? 0 : cube2uni(s[*i])) : cube2uni(s[0])));
ICOMMAND(unistr, "i", (int *i), { char *s = newstring(1); s[0] = uni2cube(*i); s[1] = '\0'; stringret(s); });

#define STRMAPCOMMAND(name, map) \
    ICOMMAND(name, "s", (char *s), \
    { \
        int len = strlen(s); \
        char *m = newstring(len); \
        for(int i = 0; i < int(len); ++i) m[i] = map(s[i]); \
        m[len] = '\0'; \
        stringret(m); \
    })

STRMAPCOMMAND(strlower, cubelower);
STRMAPCOMMAND(strupper, cubeupper);

char *strreplace(const char *s, const char *oldval, const char *newval, const char *newval2)
{
    vector<char> buf;

    int oldlen = strlen(oldval);
    if(!oldlen) return newstring(s);
    for(int i = 0;; i++)
    {
        const char *found = strstr(s, oldval);
        if(found)
        {
            while(s < found) buf.add(*s++);
            for(const char *n = i&1 ? newval2 : newval; *n; n++) buf.add(*n);
            s = found + oldlen;
        }
        else
        {
            while(*s) buf.add(*s++);
            buf.add('\0');
            return newstring(buf.getbuf(), buf.length());
        }
    }
}

ICOMMAND(strreplace, "ssss", (char *s, char *o, char *n, char *n2), commandret->setstr(strreplace(s, o, n, n2[0] ? n2 : n)));

void strsplice(const char *s, const char *vals, int *skip, int *count)
{
    int slen = strlen(s), vlen = strlen(vals),
        offset = clamp(*skip, 0, slen),
        len = clamp(*count, 0, slen - offset);
    char *p = newstring(slen - len + vlen);
    if(offset) memcpy(p, s, offset);
    if(vlen) memcpy(&p[offset], vals, vlen);
    if(offset + len < slen) memcpy(&p[offset + vlen], &s[offset + len], slen - (offset + len));
    p[slen - len + vlen] = '\0';
    commandret->setstr(p);
}
COMMAND(strsplice, "ssii");

#ifndef STANDALONE
ICOMMAND(getmillis, "i", (int *total), intret(*total ? totalmillis : lastmillis));

struct sleepcmd
{
    int delay, millis, flags;
    char *command;
};
vector<sleepcmd> sleepcmds;

void addsleep(int *msec, char *cmd)
{
    sleepcmd &s = sleepcmds.add();
    s.delay = max(*msec, 1);
    s.millis = lastmillis;
    s.command = newstring(cmd);
    s.flags = identflags;
}

COMMANDN(sleep, addsleep, "is");

void checksleep(int millis)
{
    for(int i = 0; i < sleepcmds.length(); i++)
    {
        sleepcmd &s = sleepcmds[i];
        if(millis - s.millis >= s.delay)
        {
            char *cmd = s.command; // execute might create more sleep commands
            s.command = NULL;
            int oldflags = identflags;
            identflags = s.flags;
            execute(cmd);
            identflags = oldflags;
            delete[] cmd;
            if(sleepcmds.inrange(i) && !sleepcmds[i].command) sleepcmds.remove(i--);
        }
    }
}

void clearsleep(bool clearoverrides)
{
    int len = 0;
    for(int i = 0; i < sleepcmds.length(); i++)
    {
        if(sleepcmds[i].command)
        {
            if(clearoverrides && !(sleepcmds[i].flags&Idf_Overridden))
            {
                sleepcmds[len++] = sleepcmds[i];
            }
            else
            {
                delete[] sleepcmds[i].command;
            }
        }
    }
    sleepcmds.shrink(len);
}

void clearsleep_(int *clearoverrides)
{
    clearsleep(*clearoverrides!=0 || identflags&Idf_Overridden);
}

COMMANDN(clearsleep, clearsleep_, "i");
#endif

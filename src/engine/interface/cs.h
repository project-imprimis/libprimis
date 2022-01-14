#ifndef CS_H_
#define CS_H_
// cs.h: low level cubscript functionality beyond script binding in command.h

enum
{
    Max_Args = 25,
    Max_Results = 7,
    Max_CommandArgs = 12
};
const int undoflag = 1<<Max_Args;

enum
{
    Code_Start = 0,          //0
    Code_Offset,
    Code_Null,
    Code_True,
    Code_False,
    Code_Not,               //5
    Code_Pop,
    Code_Enter,
    Code_EnterResult,
    Code_Exit,
    Code_ResultArg,       //10
    Code_Val,
    Code_ValI,
    Code_Dup,
    Code_Macro,
    Code_Bool,            //15 (unused)
    Code_Block,
    Code_Empty,
    Code_Compile,
    Code_Cond,
    Code_Force,           //20
    Code_Result,
    Code_Ident,
    Code_IdentU,
    Code_IdentArg,
    Code_Com,             //25
    Code_ComD,
    Code_ComC,
    Code_ComV,
    Code_ConC,
    Code_ConCW,           //30
    Code_ConCM,
    Code_Down, // (unused)
    Code_StrVar,
    Code_StrVarM,
    Code_StrVar1,         //35
    Code_IntVar,
    Code_IntVar1,
    Code_IntVar2,
    Code_IntVar3,
    Code_FloatVar,        //40
    Code_FloatVar1,
    Code_Lookup,
    Code_LookupU,
    Code_LookupArg,
    Code_LookupM,         //45
    Code_LookupMU,
    Code_LookupMArg,
    Code_Alias,
    Code_AliasU,
    Code_AliasArg,        //50
    Code_Call,
    Code_CallU,
    Code_CallArg,
    Code_Print,
    Code_Local,           //55
    Code_Do,
    Code_DoArgs,
    Code_Jump,
    Code_JumpTrue,
    Code_JumpFalse,
    Code_JumpResultTrue,  //60
    Code_JumpResultFalse,

    Code_OpMask = 0x3F,
    Code_Ret = 6,
    Code_RetMask = 0xC0,

    /* return type flags */
    Ret_Null    = Value_Null<<Code_Ret,
    Ret_String  = Value_String<<Code_Ret,
    Ret_Integer = Value_Integer<<Code_Ret,
    Ret_Float   = Value_Float<<Code_Ret,
};

#define PARSEFLOAT(name, type) \
    inline type parse##name(const char *s) \
    { \
        /* not all platforms (windows) can parse hexadecimal integers via strtod */ \
        char *end; \
        double val = strtod(s, &end); \
        return val || end==s || (*end!='x' && *end!='X') ? type(val) : type(parseint(s)); \
    }
PARSEFLOAT(float, float)
PARSEFLOAT(number, double)

//defines three getter functions, kind of like a bastard template (though we want the fxns to have different names)
// very fun!
#define GETNUMBER(name, ret) \
    inline ret get##name(const identval &v, int type) \
    { \
        switch(type) \
        { \
            case Value_Float: \
            { \
                return ret(v.f); \
            } \
            case Value_Integer: \
            { \
                return ret(v.i); \
            } \
            case Value_String: \
            case Value_Macro: \
            case Value_CString: \
            { \
                return parse##name(v.s); \
            } \
            default: \
            { \
                return ret(0); \
            } \
        } \
    } \
    inline ret tagval::get##name() const \
    { \
        return ::get##name(*this, type); \
    } \
    inline ret ident::get##name() const \
    { \
        return ::get##name(val, valtype); \
    }

GETNUMBER(int, int)
GETNUMBER(float, float)
GETNUMBER(number, double)

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

inline void intformat(char *buf, int v, int len = 20) { nformatstring(buf, len, "%d", v); }
inline void floatformat(char *buf, float v, int len = 20) { nformatstring(buf, len, v==static_cast<int>(v) ? "%.1f" : "%.7g", v); }
inline void numberformat(char *buf, double v, int len = 20)
{
    int i = static_cast<int>(v);
    if(v == i)
    {
        nformatstring(buf, len, "%d", i);
    }
    else
    {
        nformatstring(buf, len, "%.7g", v);
    }
}


inline const char *getstr(const identval &v, int type)
{
    switch(type)
    {
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            return v.s;
        }
        case Value_Integer:
        {
            return intstr(v.i);
        }
        case Value_Float:
        {
            return floatstr(v.f);
        }
        default:
        {
            return "";
        }
    }
}
inline const char *tagval::getstr() const
{
    return ::getstr(*this, type);
}

inline const char *ident::getstr() const
{
    return ::getstr(val, valtype);
}

inline void getval(const identval &v, int type, tagval &r)
{
    switch(type)
    {
        case Value_String:
        case Value_Macro:
        case Value_CString:
        {
            r.setstr(newstring(v.s));
            break;
        }
        case Value_Integer:
        {
            r.setint(v.i);
            break;
        }
        case Value_Float:
        {
            r.setfloat(v.f);
            break;
        }
        default:
        {
            r.setnull();
            break;
        }
    }
}

inline void tagval::getval(tagval &r) const
{
    ::getval(*this, type, r);
}

inline void ident::getval(tagval &r) const
{
    ::getval(val, valtype, r);
}

inline void ident::getcstr(tagval &v) const
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

inline void ident::getcval(tagval &v) const
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

struct NullVal : tagval
{
    NullVal() { setnull(); }
};

extern NullVal nullval;

struct IdentLink
{
    ident *id;
    IdentLink *next;
    int usedargs;
    identstack *argstack;
};

extern IdentLink noalias;
extern IdentLink *aliasstack;

extern const char *sourcefile,
                  *sourcestr;

extern vector<char> strbuf[4];
extern int stridx;

extern tagval *commandret;
extern void executeret(const uint *code, tagval &result = *commandret);
extern void executeret(const char *p, tagval &result = *commandret);
extern void executeret(ident *id, tagval *args, int numargs, bool lookup = false, tagval &result = *commandret);

extern void addident(ident *id);

extern void poparg(ident &id);
extern void pusharg(ident &id, const tagval &v, identstack &stack);
extern bool getbool(const tagval &v);
extern void cleancode(ident &id);
extern char *conc(tagval *v, int n, bool space);
extern char *conc(tagval *v, int n, bool space, const char *prefix);
extern void freearg(tagval &v);
extern int unescapestring(char *dst, const char *src, const char *end);
extern const char *parsestring(const char *p);
extern void setarg(ident &id, tagval &v);
extern void setalias(ident &id, tagval &v);
extern void undoarg(ident &id, identstack &stack);
extern void redoarg(ident &id, const identstack &stack);
extern const char *parseword(const char *p);

extern bool validateblock(const char *s);
extern hashnameset<ident> idents;
extern vector<ident *> identmap;

extern void setvarchecked(ident *id, int val);
extern void setfvarchecked(ident *id, float val);
extern void setsvarchecked(ident *id, const char *val);

extern const char *escapeid(const char *s);
inline const char *escapeid(ident &id) { return escapeid(id.name); }

extern void printvar(ident *id);
extern void printvar(ident *id, int i);

extern void clearoverrides();

extern void checksleep(int millis);
extern void clearsleep(bool clearoverrides = true);

extern char *executestr(const uint *code);
extern char *executestr(const char *p);
extern char *executestr(ident *id, tagval *args, int numargs, bool lookup = false);

#endif

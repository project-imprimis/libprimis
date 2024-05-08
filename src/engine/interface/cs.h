#ifndef CS_H_
#define CS_H_
// cs.h: low level cubscript functionality beyond script binding in command.h

enum
{
    Max_Args = 25,
    Max_Results = 7,
    Max_CommandArgs = 12
};

enum CubeScriptCodes
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


struct stringslice
{
    const char *str;
    int len;
    stringslice() {}
    stringslice(const char *str, int len) : str(str), len(len) {}
    stringslice(const char *str, const char *end) : str(str), len(static_cast<int>(end-str)) {}

    const char *end() const { return &str[len]; }
};


inline char *copystring(char *d, const stringslice &s, size_t len)
{
    size_t slen = min(size_t(s.len), len-1);
    std::memcpy(d, s.str, slen);
    d[slen] = 0;
    return d;
}

template<size_t N>
inline char *copystring(char (&d)[N], const stringslice &s) { return copystring(d, s, N); }

// not all platforms (windows) can parse hexadecimal integers via strtod
inline float parsefloat(const char *s)
{
    char *end;
    double val = std::strtod(s, &end);
    return val
        || end==s
        || (*end!='x' && *end!='X') ? static_cast<float>(val) : static_cast<float>(parseint(s));
}

inline double parsenumber(const char *s)
{
    char *end;
    double val = std::strtod(s, &end);
    return val
        || end==s
        || (*end!='x' && *end!='X') ? static_cast<double>(val) : static_cast<double>(parseint(s));
}


inline void intformat(char *buf, int v, int len = 20) { nformatstring(buf, len, "%d", v); }
inline void floatformat(char *buf, float v, int len = 20) { nformatstring(buf, len, v==static_cast<int>(v) ? "%.1f" : "%.7g", v); }


extern const char *intstr(int v);

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

struct NullVal : tagval
{
    NullVal() { setnull(); }
};

struct IdentLink
{
    ident *id;
    IdentLink *next;
    int usedargs;
    identstack *argstack;
};

extern const char *sourcefile,
                  *sourcestr;

extern std::vector<char> strbuf[4];
extern int stridx;

extern tagval *commandret;
extern void executeret(const uint *code, tagval &result = *commandret);

/**
 * @brief Executes a given string, returning a tagval by reference parameter
 *
 * @param p a string to execute
 * @param result tagval containing result metadata
 */
extern void executeret(const char *p, tagval &result = *commandret);

/**
 * @brief Executes a given ident, returning a tagval by reference parameter
 *
 * @param id the ident to execute
 * @param args an array of arguments
 * @param numargs size of args array
 * @param lookup whether to lookup (dereference) args (?)
 * @param result tagval containing result metadata
 */
extern void executeret(ident *id, tagval *args, int numargs, bool lookup = false, tagval &result = *commandret);

extern void poparg(ident &id);
extern void pusharg(ident &id, const tagval &v, identstack &stack);
extern bool getbool(const tagval &v);
extern void cleancode(ident &id);
extern char *conc(const tagval *v, int n, bool space);
extern char *conc(const tagval *v, int n, bool space, const char *prefix);
extern void freearg(tagval &v);
extern int unescapestring(char *dst, const char *src, const char *end);
extern const char *parsestring(const char *p);
extern void setarg(ident &id, tagval &v);
extern void setalias(ident &id, tagval &v);
extern void undoarg(ident &id, identstack &stack);
extern void redoarg(ident &id, const identstack &stack);
extern const char *parseword(const char *p);

extern bool validateblock(const char *s);
extern std::unordered_map<std::string, ident> idents;

extern void setvarchecked(ident *id, int val);
extern void setfvarchecked(ident *id, float val);
extern void setsvarchecked(ident *id, const char *val);

extern void printvar(const ident *id);
extern void printvar(const ident *id, int i);

extern void clearoverrides();

extern void clearsleep(bool clearoverrides = true);

extern char *executestr(ident *id, tagval *args, int numargs, bool lookup = false);
extern uint *compilecode(const char *p);
extern void freecode(uint *p);
extern int execute(ident *id, tagval *args, int numargs, bool lookup = false);
extern bool executebool(ident *id, tagval *args, int numargs, bool lookup = false);
extern void alias(const char *name, const char *action);
extern void explodelist(const char *s, std::vector<char *> &elems, int limit = -1);
extern void result(tagval &v);
extern const char *numberstr(double v);
extern float clampfvar(std::string name, float val, float minval, float maxval);

#endif

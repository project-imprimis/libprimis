/* cubescript commands
 *
 * these functions & assignment macros define standard functions used with the language
 * the language does not otherwise define special operators (besides bracket semantics)
 *
 * including, but not limited to:
 *    - file handling
 *    - arithmetic and boolean operators
 *    - control statements
 */

#include "../libprimis-headers/cube.h"
#include "../../shared/stream.h"

#include "console.h"
#include "control.h"
#include "cs.h"

#include "render/hud.h"

/* execfile
 *
 * executes the cubescript in a file located at a relative path to the game's home dir
 *
 * *cfgfile: a pointer to the name of the file
 * msg: if true, prints out a message if there is no file found
 */
bool execfile(const char *cfgfile, bool msg)
{
    string s;
    copystring(s, cfgfile);
    char *buf = loadfile(path(s), nullptr);
    if(!buf)
    {
        if(msg)
        {
            conoutf(Console_Error, "could not read \"%s\"", cfgfile);
        }
        return false;
    }
    const char *oldsourcefile = sourcefile,
               *oldsourcestr  = sourcestr;
    sourcefile = cfgfile;
    sourcestr = buf;
    execute(buf);
    sourcefile = oldsourcefile;
    sourcestr = oldsourcestr;
    delete[] buf;
    return true;
}

static void exec(char *file, int *msg)
{
    intret(execfile(file, *msg != 0) ? 1 : 0);
}
static bool dummy_exec = addcommand("exec", (identfun)exec, "sb", Id_Command);

const char *escapestring(const char *s)
{
    stridx = (stridx + 1)%4;
    vector<char> &buf = strbuf[stridx];
    buf.setsize(0);
    buf.add('"');
    for(; *s; s++)
    {
        switch(*s)
        {
            case '\n': buf.put("^n", 2);  break;
            case '\t': buf.put("^t", 2);  break;
            case '\f': buf.put("^f", 2);  break;
            case '"':  buf.put("^\"", 2); break;
            case '^':  buf.put("^^", 2);  break;
            default:   buf.add(*s);       break;
        }
    }
    buf.put("\"\0", 2);
    return buf.getbuf();
}

static void escapecmd(char *s)
{
    result(escapestring(s));
}
static bool dummy_escapecmd = addcommand("escape", (identfun)escapecmd, "s", Id_Command);

static void unescapecmd(char *s)
{
    int len = std::strlen(s);
    char *d = newstring(len);
    unescapestring(d, s, &s[len]);
    stringret(d);
}
static bool dummy_unescapecmd = addcommand("unescape", (identfun)unescapecmd, "s", Id_Command);

const char *escapeid(const char *s)
{
    const char *end = s + std::strcspn(s, "\"/;()[]@ \f\t\r\n\0");
    return *end ? escapestring(s) : s;
}

bool validateblock(const char *s)
{
    constexpr int maxbrak = 100;
    static char brakstack[maxbrak];
    int brakdepth = 0;
    for(; *s; s++) switch(*s)
    {
        case '[':
        case '(':
        {
            if(brakdepth >= maxbrak)
            {
                return false;
            }
            brakstack[brakdepth++] = *s;
            break;
        }
        case ']':
        {
            if(brakdepth <= 0 || brakstack[--brakdepth] != '[')
            {
                return false;
            }
            break;
        }
        case ')':
        {
            if(brakdepth <= 0 || brakstack[--brakdepth] != '(')
            {
                return false;
            }
            break;
        }
        case '"':
        {
            s = parsestring(s + 1);
            if(*s != '"')
            {
                return false;
            }
            break;
        }
        case '/':
        {
            if(s[1] == '/')
            {
                return false;
            }
            break;
        }
        case '@':
        case '\f':
        {
            return false;
        }
    }
    return brakdepth == 0;
}

void writecfg(const char *savedconfig, const char *autoexec, const char *defaultconfig, const char *name)
{
    stream *f = openutf8file(copypath(name && name[0] ? name : savedconfig), "w");
    if(!f)
    {
        return;
    }
    f->printf("// automatically written on exit, DO NOT MODIFY\n// delete this file to have %s overwrite these settings\n// modify settings in game, or put settings in %s to override anything\n\n", defaultconfig, autoexec);
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
        if(id.type==Id_Alias && id.flags&Idf_Persist && !(id.flags&Idf_Overridden))
        {
            switch(id.valtype)
            {
                case Value_String:
                {
                    if(!id.val.s[0])
                    {
                        break;
                    }
                    if(!validateblock(id.val.s))
                    {
                        f->printf("%s = %s\n", escapeid(id), escapestring(id.val.s));
                        break;
                    }
                }
                [[fallthrough]];
                case Value_Float:
                case Value_Integer:
                {
                    f->printf("%s = [%s]\n", escapeid(id), id.getstr());
                    break;
                }
            }
        }
    }
    f->printf("\n");
    writecompletions(f);
    delete f;
}
static bool dummy_writecfg = addcommand("writecfg", (identfun)writecfg, "s", Id_Command);

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
static bool dummy_changedvars = addcommand("changedvars", (identfun)changedvars, "", Id_Command);

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
    int i = static_cast<int>(v);
    if(v == i)
    {
        commandret->setint(i);
    }
    else
    {
        commandret->setfloat(v);
    }
}

bool docmd = addcommand("do", reinterpret_cast<identfun>(+[] (uint *body) { executeret(body, *commandret); }), "e", Id_Do);

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
static bool dummy_doargs = addcommand("doargs", (identfun)doargs, "e", Id_DoArgs);

bool ifcmd = addcommand("if", reinterpret_cast<identfun>(+[] (tagval *cond, uint *t, uint *f) { executeret(getbool(*cond) ? t : f, *commandret); }), "tee", Id_If);
bool boolcmd = addcommand("?", reinterpret_cast<identfun>(+[] (tagval *cond, tagval *t, tagval *f) { result(*(getbool(*cond) ? t : f)); }), "tTT", Id_Command);

bool _icmd_pushif = addcommand("pushif", reinterpret_cast<identfun>(+[] (ident *id, tagval *v, uint *code) { { if(id->type != Id_Alias || id->index < Max_Args) { return; } if(getbool(*v)) { identstack stack; pusharg(*id, *v, stack); v->type = Value_Null; id->flags &= ~Idf_Unknown; executeret(code, *commandret); poparg(*id); } }; }), "rTe", Id_Command);

void loopiter(ident *id, identstack &stack, const tagval &v)
{
    if(id->stack != &stack)
    {
        pusharg(*id, v, stack);
        id->flags &= ~Idf_Unknown;
    }
    else
    {
        if(id->valtype == Value_String)
        {
            delete[] id->val.s;
        }
        cleancode(*id);
        id->setval(v);
    }
}

void loopiter(ident *id, identstack &stack, int i)
{
    tagval v;
    v.setint(i);
    loopiter(id, stack, v);
}

void loopend(ident *id, identstack &stack)
{
    if(id->stack == &stack)
    {
        poparg(*id);
    }
}

static void setiter(ident &id, int i, identstack &stack)
{
    if(id.stack == &stack)
    {
        if(id.valtype != Value_Integer)
        {
            if(id.valtype == Value_String)
            {
                delete[] id.val.s;
            }
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

static void doloop(ident &id, int offset, int n, int step, uint *body)
{
    if(n <= 0 || id.type != Id_Alias)
    {
        return;
    }
    identstack stack;
    for(int i = 0; i < n; ++i)
    {
        setiter(id, offset + i*step, stack);
        execute(body);
    }
    poparg(id);
}
bool _icmd_loop = addcommand("loop", reinterpret_cast<identfun>(+[] (ident *id, int *n, uint *body) { doloop(*id, 0, *n, 1, body); }), "rie", Id_Command);
bool looppluscmd = addcommand("loop+", reinterpret_cast<identfun>(+[] (ident *id, int *offset, int *n, uint *body) { doloop(*id, *offset, *n, 1, body); }), "riie", Id_Command);
bool loopstarcmd = addcommand("loop*", reinterpret_cast<identfun>(+[] (ident *id, int *step, int *n, uint *body) { doloop(*id, 0, *n, *step, body); }), "riie", Id_Command);
bool loopstarplus = addcommand("loop+*", reinterpret_cast<identfun>(+[] (ident *id, int *offset, int *step, int *n, uint *body) { doloop(*id, *offset, *n, *step, body); }), "riiie", Id_Command);

bool whilecmd = addcommand("while", reinterpret_cast<identfun>(+[] (uint *cond, uint *body) { while(executebool(cond)) execute(body); }), "ee", Id_Command);

static void loopconc(ident &id, int offset, int n, uint *body, bool space)
{
    if(n <= 0 || id.type != Id_Alias)
    {
        return;
    }
    identstack stack;
    vector<char> s;
    for(int i = 0; i < n; ++i)
    {
        setiter(id, offset + i, stack);
        tagval v;
        executeret(body, v);
        const char *vstr = v.getstr();
        int len = std::strlen(vstr);
        if(space && i)
        {
            s.add(' ');
        }
        s.put(vstr, len);
        freearg(v);
    }
    if(n > 0)
    {
        poparg(id);
    }
    s.add('\0');
    commandret->setstr(s.disown());
}
bool _icmd_loopconcat = addcommand("loopconcat", reinterpret_cast<identfun>(+[] (ident *id, int *n, uint *body) { loopconc(*id, 0, *n, body, true); }), "rie", Id_Command);
bool loopconcatplus = addcommand("loopconcat+", reinterpret_cast<identfun>(+[] (ident *id, int *offset, int *n, uint *body) { loopconc(*id, *offset, *n, body, true); }), "riie", Id_Command);

void concat(tagval *v, int n)
{
    commandret->setstr(conc(v, n, true));
}
static bool dummy_concat = addcommand("concat", (identfun)concat, "V", Id_Command);

void concatword(tagval *v, int n)
{
    commandret->setstr(conc(v, n, false));
}
static bool dummy_concatword = addcommand("concatword", (identfun)concatword, "V", Id_Command);

void append(ident *id, tagval *v, bool space)
{
    if(id->type != Id_Alias || v->type == Value_Null)
    {
        return;
    }
    tagval r;
    const char *prefix = id->getstr();
    if(prefix[0])
    {
        r.setstr(conc(v, 1, space, prefix));
    }
    else
    {
        v->getval(r);
    }
    if(id->index < Max_Args)
    {
        setarg(*id, r);
    }
    else
    {
        setalias(*id, r);
    }

}
bool _icmd_append = addcommand("append", reinterpret_cast<identfun>(+[] (ident *id, tagval *v) { append(id, v, true); }), "rt", Id_Command);
bool _icmd_appendword = addcommand("appendword", reinterpret_cast<identfun>(+[] (ident *id, tagval *v) { append(id, v, false); }), "rt", Id_Command);

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

bool _icmd_result = addcommand("result", reinterpret_cast<identfun>(+[] (tagval *v) { { *commandret = *v; v->type = Value_Null; }; }), "T", Id_Result);



  ;

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
                while(*sub)
                {
                    s.add(*sub++);
                }
            }
            else
            {
                s.add(i);
            }
        }
        else
        {
            s.add(c);
        }
    }
    s.add('\0');
    commandret->setstr(s.disown());
}
static bool dummy_format = addcommand("format", (identfun)format, "V", Id_Command);

static const char *liststart      = nullptr,
                  *listend        = nullptr,
                  *listquotestart = nullptr,
                  *listquoteend   = nullptr;

static void skiplist(const char *&p)
{
    for(;;)
    {
        p += std::strspn(p, " \t\r\n");
        if(p[0]!='/' || p[1]!='/')
        {
            break;
        }
        p += std::strcspn(p, "\n\0");
    }
}

static bool parselist(const char *&s, const char *&start = liststart, const char *&end = listend, const char *&quotestart = listquotestart, const char *&quoteend = listquoteend)
{
    skiplist(s);
    switch(*s)
    {
        case '"':
        {
            quotestart = s++;
            start = s;
            s = parsestring(s);
            end = s;
            if(*s == '"')
            {
                s++;
            }
            quoteend = s;
            break;
        }
        case '(':
        case '[':
            quotestart = s;
            start = s+1;
            for(int braktype = *s++, brak = 1;;)
            {
                s += std::strcspn(s, "\"/;()[]\0");
                int c = *s++;
                switch(c)
                {
                    case '\0':
                    {
                        s--;
                        quoteend = end = s;
                        return true;
                    }
                    case '"':
                    {
                        s = parsestring(s);
                        if(*s == '"')
                        {
                            s++;
                        }
                        break;
                    }
                    case '/':
                    {
                        if(*s == '/')
                        {
                            s += std::strcspn(s, "\n\0");
                        }
                        break;
                    }
                    case '(':
                    case '[':
                    {
                        if(c == braktype)
                        {
                            brak++;
                        }
                        break;
                    }
                    case ')':
                    {
                        if(braktype == '(' && --brak <= 0)
                        {
                            goto endblock;
                        }
                        break;
                    }
                    case ']':
                    {
                        if(braktype == '[' && --brak <= 0)
                        {
                            goto endblock;
                        }
                        break;
                    }
                }
            }
        endblock:
            end = s-1;
            quoteend = s;
            break;
        case '\0':
        case ')':
        case ']':
        {
            return false;
        }
        default:
        {
            quotestart = start = s;
            s = parseword(s);
            quoteend = end = s;
            break;
        }
    }
    skiplist(s);
    if(*s == ';')
    {
        s++;
    }
    return true;
}

static char *listelem(const char *start = liststart, const char *end = listend, const char *quotestart = listquotestart)
{
    size_t len = end-start;
    char *s = newstring(len);
    if(*quotestart == '"')
    {
        unescapestring(s, start, end);
    }
    else
    {
        memcpy(s, start, len);
        s[len] = '\0';
    }
    return s;
}

void explodelist(const char *s, vector<char *> &elems, int limit)
{
    const char *start, *end, *qstart;
    while((limit < 0 || elems.length() < limit) && parselist(s, start, end, qstart))
    {
        elems.add(listelem(start, end, qstart));
    }
}

int listlen(const char *s)
{
    int n = 0;
    while(parselist(s))
    {
        n++;
    }
    return n;
}

void listlencmd(const char *s)
{
    intret(listlen(s));
}
static bool dummy_listlencmd = addcommand("listlen", (identfun)listlencmd, "s", Id_Command);

void at(tagval *args, int numargs)
{
    if(!numargs)
    {
        return;
    }
    const char *start  = args[0].getstr(),
               *end    = start + std::strlen(start),
               *qstart = "";
    for(int i = 1; i < numargs; i++)
    {
        const char *list = start;
        int pos = args[i].getint();
        for(; pos > 0; pos--)
        {
            if(!parselist(list))
            {
                break;
            }
        }
        if(pos > 0 || !parselist(list, start, end, qstart))
        {
            start = end = qstart = "";
        }
    }
    commandret->setstr(listelem(start, end, qstart));
}
static bool dummy_at = addcommand("at", (identfun)at, "si1V", Id_Command);

void substr(char *s, int *start, int *count, int *numargs)
{
    int len = std::strlen(s),
        offset = std::clamp(*start, 0, len);
    commandret->setstr(newstring(&s[offset], *numargs >= 3 ? std::clamp(*count, 0, len - offset) : len - offset));
}
static bool dummy_substr = addcommand("substr", (identfun)substr, "siiN", Id_Command);

void sublist(const char *s, int *skip, int *count, int *numargs)
{
    int offset = std::max(*skip, 0),
        len = *numargs >= 3 ? std::max(*count, 0) : -1;
    for(int i = 0; i < offset; ++i)
    {
        if(!parselist(s))
        {
            break;
        }
    }
    if(len < 0)
    {
        if(offset > 0)
        {
            skiplist(s);
        }
        commandret->setstr(newstring(s));
        return;
    }
    const char *list = s,
               *start, *end, *qstart,
               *qend = s;
    if(len > 0 && parselist(s, start, end, list, qend))
    {
        while(--len > 0 && parselist(s, start, end, qstart, qend))
        {
            //(empty body)
        }
    }
    commandret->setstr(newstring(list, qend - list));
}
static bool dummy_sublist = addcommand("sublist", (identfun)sublist, "siiN", Id_Command);

void stripcolors(char *s)
{
    int len = std::strlen(s);
    char *d = newstring(len);
    filtertext(d, s, true, false, len);
    stringret(d);
}
static bool dummy_stripcolors = addcommand("stripcolors", (identfun)stripcolors, "s", Id_Command);

static void setiter(ident &id, char *val, identstack &stack)
{
    if(id.stack == &stack)
    {
        if(id.valtype == Value_String)
        {
            delete[] id.val.s;
        }
        else
        {
            id.valtype = Value_String;
        }
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
    if(id->type!=Id_Alias)
    {
        intret(-1);
        return;
    }
    identstack stack;
    int n = -1;
    for(const char *s = list, *start, *end; parselist(s, start, end);)
    {
        ++n;
        setiter(*id, newstring(start, end-start), stack);
        if(executebool(body))
        {
            intret(n);
            goto found;
        }
    }
    intret(-1); //if element not found in list
found: //if element is found in list
    if(n >= 0)
    {
        poparg(*id);
    }
}
static bool dummy_listfind = addcommand("listfind", (identfun)listfind, "rse", Id_Command);

//note: the goto here is the opposite of listfind above: goto triggers when elem not found
void listfindeq(char *list, int *val, int *skip)
{
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n++)
    {
        if(parseint(start) == *val)
        {
            intret(n);
            return;
        }
        for(int i = 0; i < static_cast<int>(*skip); ++i)
        {
            if(!parselist(s))
            {
                goto notfound;
            }
            n++;
        }
    }
notfound:
    intret(-1);
}
static bool dummy_listfindeq = addcommand("listfind=", (identfun)listfindeq, "sii", Id_Command);

void listassoceq(char *list, int *val)
{
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end);)
    {
        if(parseint(start) == *val)
        {
            if(parselist(s, start, end, qstart))
            {
                stringret(listelem(start, end, qstart));
            }
            return;
        }
        if(!parselist(s))
        {
            break;
        }
    }
}
static bool dummy_listassoceq = addcommand("listassoc=", (identfun)listassoceq, "si", Id_Command);

void looplist(ident *id, const char *list, const uint *body)
{
    if(id->type!=Id_Alias)
    {
        return;
    }
    identstack stack;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n++)
    {
        setiter(*id, listelem(start, end, qstart), stack);
        execute(body);
    }
    if(n)
    {
        poparg(*id);
    }
}
static bool dummy_looplist = addcommand("looplist", (identfun)looplist, "rse", Id_Command);

void looplist2(ident *id, ident *id2, const char *list, const uint *body)
{
    if(id->type!=Id_Alias || id2->type!=Id_Alias)
    {
        return;
    }
    identstack stack, stack2;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n += 2)
    {
        setiter(*id, listelem(start, end, qstart), stack);
        setiter(*id2, parselist(s, start, end, qstart) ? listelem(start, end, qstart) : newstring(""), stack2);
        execute(body);
    }
    if(n)
    {
        poparg(*id);
        poparg(*id2);
    }
}
static bool dummy_looplist2 = addcommand("looplist2", (identfun)looplist2, "rrse", Id_Command);

void looplist3(ident *id, ident *id2, ident *id3, const char *list, const uint *body)
{
    if(id->type!=Id_Alias || id2->type!=Id_Alias || id3->type!=Id_Alias)
    {
        return;
    }
    identstack stack, stack2, stack3;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n += 3)
    {
        setiter(*id, listelem(start, end, qstart), stack);
        setiter(*id2, parselist(s, start, end, qstart) ? listelem(start, end, qstart) : newstring(""), stack2);
        setiter(*id3, parselist(s, start, end, qstart) ? listelem(start, end, qstart) : newstring(""), stack3);
        execute(body);
    }
    if(n)
    {
        poparg(*id);
        poparg(*id2);
        poparg(*id3);
    }
}
static bool dummy_looplist3 = addcommand("looplist3", (identfun)looplist3, "rrrse", Id_Command);

void looplistconc(ident *id, const char *list, const uint *body, bool space)
{
    if(id->type!=Id_Alias)
    {
        return;
    }
    identstack stack;
    vector<char> r;
    int n = 0;
    for(const char *s = list, *start, *end, *qstart; parselist(s, start, end, qstart); n++)
    {
        char *val = listelem(start, end, qstart);
        setiter(*id, val, stack);
        if(n && space)
        {
            r.add(' ');
        }
        tagval v;
        executeret(body, v);
        const char *vstr = v.getstr();
        int len = std::strlen(vstr);
        r.put(vstr, len);
        freearg(v);
    }
    if(n)
    {
        poparg(*id);
    }
    r.add('\0');
    commandret->setstr(r.disown());
}
bool _icmd_looplistconcat = addcommand("looplistconcat", reinterpret_cast<identfun>(+[] (ident *id, char *list, uint *body) { looplistconc(id, list, body, true); }), "rse", Id_Command);
bool _icmd_looplistconcatword = addcommand("looplistconcatword", reinterpret_cast<identfun>(+[] (ident *id, char *list, uint *body) { looplistconc(id, list, body, false); }), "rse", Id_Command);

void listcount(ident *id, const char *list, const uint *body)
{
    if(id->type!=Id_Alias)
    {
        return;
    }
    identstack stack;
    int n = 0,
        r = 0;
    for(const char *s = list, *start, *end; parselist(s, start, end); n++)
    {
        char *val = newstring(start, end-start);
        setiter(*id, val, stack);
        if(executebool(body))
        {
            r++;
        }
    }
    if(n)
    {
        poparg(*id);
    }
    intret(r);
}
static bool dummy_listcount = addcommand("listcount", (identfun)listcount, "rse", Id_Command);

void prettylist(const char *s, const char *conj)
{
    vector<char> p;
    const char *start, *end, *qstart;
    for(int len = listlen(s), n = 0; parselist(s, start, end, qstart); n++)
    {
        if(*qstart == '"')
        {
            p.advance(unescapestring(p.reserve(end - start + 1).buf, start, end));
        }
        else
        {
            p.put(start, end - start);
        }
        if(n+1 < len)
        {
            if(len > 2 || !conj[0])
            {
                p.add(',');
            }
            if(n+2 == len && conj[0])
            {
                p.add(' ');
                p.put(conj, std::strlen(conj));
            }
            p.add(' ');
        }
    }
    p.add('\0');
    commandret->setstr(p.disown());
}
static bool dummy_prettylist = addcommand("prettylist", (identfun)prettylist, "ss", Id_Command);

//returns the int position of the needle inside the passed list
int listincludes(const char *list, const char *needle, int needlelen)
{
    int offset = 0;
    for(const char *s = list, *start, *end; parselist(s, start, end);)
    {
        int len = end - start;
        if(needlelen == len && !strncmp(needle, start, len))
        {
            return offset;
        }
        offset++;
    }
    return -1;
}
bool _icmd_indexof = addcommand("indexof", reinterpret_cast<identfun>(+[] (char *list, char *elem) { intret(listincludes(list, elem, std::strlen(elem))); }), "ss", Id_Command);

bool _icmd_listdel = addcommand("listdel", reinterpret_cast<identfun>(+[] (const char *list, const char *elems) { { vector<char> p; ; for(const char *start, *end, *qstart, *qend; parselist(list, start, end, qstart, qend);) { int len = end - start; if(listincludes(elems, start, len) < 0) { if(!p.empty()) p.add(' '); p.put(qstart, qend-qstart); } } p.add('\0'); commandret->setstr(p.disown()); }; }), "ss", Id_Command);
bool _icmd_listintersect = addcommand("listintersect", reinterpret_cast<identfun>(+[] (const char *list, const char *elems) { { vector<char> p; ; for(const char *start, *end, *qstart, *qend; parselist(list, start, end, qstart, qend);) { int len = end - start; if(listincludes(elems, start, len) >= 0) { if(!p.empty()) p.add(' '); p.put(qstart, qend-qstart); } } p.add('\0'); commandret->setstr(p.disown()); }; }), "ss", Id_Command);
bool _icmd_listunion = addcommand("listunion", reinterpret_cast<identfun>(+[] (const char *list, const char *elems) { { vector<char> p; p.put(list, std::strlen(list)); for(const char *start, *end, *qstart, *qend; parselist(elems, start, end, qstart, qend);) { int len = end - start; if(listincludes(list, start, len) < 0) { if(!p.empty()) p.add(' '); p.put(qstart, qend-qstart); } } p.add('\0'); commandret->setstr(p.disown()); }; }), "ss", Id_Command);


void listsplice(const char *s, const char *vals, int *skip, int *count)
{
    int offset = std::max(*skip, 0),
        len = std::max(*count, 0);
    const char *list = s,
               *start, *end, *qstart,
               *qend = s;
    for(int i = 0; i < offset; ++i)
    {
        if(!parselist(s, start, end, qstart, qend))
        {
            break;
        }
    }
    vector<char> p;
    if(qend > list)
    {
        p.put(list, qend-list);
    }
    if(*vals)
    {
        if(!p.empty())
        {
            p.add(' ');
        }
        p.put(vals, std::strlen(vals));
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
        case '\0':
        case ')':
        case ']':
        {
            break;
        }
        default:
        {
            if(!p.empty())
            {
                p.add(' ');
            }
            p.put(s, std::strlen(s));
            break;
        }
    }
    p.add('\0');
    commandret->setstr(p.disown());
}
static bool dummy_listsplice = addcommand("listsplice", (identfun)listsplice, "ssii", Id_Command);

//executes the body for each file in the given path, using ident passed
void loopfiles(ident *id, char *dir, char *ext, uint *body)
{
    if(id->type!=Id_Alias)
    {
        return;
    }
    identstack stack;
    vector<char *> files;
    listfiles(dir, ext[0] ? ext : nullptr, files);
    files.sort();
    files.uniquedeletearrays();
    for(int i = 0; i < files.length(); i++)
    {
        setiter(*id, files[i], stack);
        execute(body);
    }
    if(files.length())
    {
        poparg(*id);
    }
}
static bool dummy_loopfiles = addcommand("loopfiles", (identfun)loopfiles, "rsse", Id_Command);

void findfile_(char *name)
{
    string fname;
    copystring(fname, name);
    path(fname);
    intret(
        findzipfile(fname) ||
        fileexists(fname, "e") || findfile(fname, "e") ? 1 : 0
    );
}
static bool dummy_findfile_ = addcommand("findfile", (identfun)findfile_, "s", Id_Command);

struct SortItem
{
    const char *str, *quotestart, *quoteend;

    int quotelength() const
    {
        return static_cast<int>(quoteend-quotestart);
    }
};

struct SortFunction
{
    ident *x, *y;
    uint *body;

    bool operator()(const SortItem &xval, const SortItem &yval)
    {
        if(x->valtype != Value_CString)
        {
            x->valtype = Value_CString;
        }
        cleancode(*x);
        x->val.code = reinterpret_cast<const uint *>(xval.str);
        if(y->valtype != Value_CString)
        {
            y->valtype = Value_CString;
        }
        cleancode(*y);
        y->val.code = reinterpret_cast<const uint *>(yval.str);
        return executebool(body);
    }
};

void sortlist(char *list, ident *x, ident *y, uint *body, uint *unique)
{
    if(x == y || x->type != Id_Alias || y->type != Id_Alias)
    {
        return;
    }
    vector<SortItem> items;
    int clen = std::strlen(list),
        total = 0;
    char *cstr = newstring(list, clen);
    const char *curlist = list,
               *start, *end, *quotestart, *quoteend;
    while(parselist(curlist, start, end, quotestart, quoteend))
    {
        cstr[end - list] = '\0';
        SortItem item = { &cstr[start - list], quotestart, quoteend };
        items.add(item);
        total += item.quotelength();
    }
    if(items.empty())
    {
        commandret->setstr(cstr);
        return;
    }
    identstack xstack, ystack;
    pusharg(*x, NullVal(), xstack);
    x->flags &= ~Idf_Unknown;
    pusharg(*y, NullVal(), ystack);
    y->flags &= ~Idf_Unknown;
    int totalunique = total,
        numunique = items.length();
    if(body)
    {
        SortFunction f = { x, y, body };
        items.sort(f);
        if((*unique&Code_OpMask) != Code_Exit)
        {
            f.body = unique;
            totalunique = items[0].quotelength();
            numunique = 1;
            for(int i = 1; i < items.length(); i++)
            {
                SortItem &item = items[i];
                if(f(items[i-1], item))
                {
                    item.quotestart = nullptr;
                }
                else
                {
                    totalunique += item.quotelength();
                    numunique++;
                }
            }
        }
    }
    else
    {
        SortFunction f = { x, y, unique };
        totalunique = items[0].quotelength();
        numunique = 1;
        for(int i = 1; i < items.length(); i++)
        {
            SortItem &item = items[i];
            for(int j = 0; j < i; ++j)
            {
                SortItem &prev = items[j];
                if(prev.quotestart && f(item, prev))
                {
                    item.quotestart = nullptr;
                    break;
                }
            }
            if(item.quotestart)
            {
                totalunique += item.quotelength();
                numunique++;
            }
        }
    }
    poparg(*x);
    poparg(*y);
    char *sorted = cstr;
    int sortedlen = totalunique + std::max(numunique - 1, 0);
    if(clen < sortedlen)
    {
        delete[] cstr;
        sorted = newstring(sortedlen);
    }
    int offset = 0;
    for(int i = 0; i < items.length(); i++)
    {
        SortItem &item = items[i];
        if(!item.quotestart)
        {
            continue;
        }
        int len = item.quotelength();
        if(i)
        {
            sorted[offset++] = ' ';
        }
        memcpy(&sorted[offset], item.quotestart, len);
        offset += len;
    }
    sorted[offset] = '\0';
    commandret->setstr(sorted);
}
static bool dummy_sortlist = addcommand("sortlist", (identfun)sortlist, "srree", Id_Command);
bool _icmd_uniquelist = addcommand("uniquelist", reinterpret_cast<identfun>(+[] (char *list, ident *x, ident *y, uint *body) { sortlist(list, x, y, nullptr, body); }), "srre", Id_Command);

void initmathcmds()
{
    //integer and boolean operators, used with named symbol, i.e. + or *
    //no native boolean type, they are treated like integers
    addcommand("+", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val + val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val + val2; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command); //0 substituted if nothing passed in arg2: n + 0 is still n
    addcommand("*", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val * val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val * val2; } } else { val = numargs > 0 ? args[0].i : 1; ; } intret(val); }; }), "i" "1V", Id_Command); //1 substituted if nothing passed in arg2: n * 1 is still n
    addcommand("-", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val - val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val - val2; } } else { val = numargs > 0 ? args[0].i : 0; val = -val; } intret(val); }; }), "i" "1V", Id_Command); //the minus operator inverts if used as unary
    addcommand("=", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].i == args[1].i; for(int i = 2; i < numargs && val; i++) { val = args[i-1].i == args[i].i; } } else { val = (numargs > 0 ? args[0].i : 0) == 0; } intret(static_cast<int>(val)); }; }), "i" "1V", Id_Command);
    addcommand("!=", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].i != args[1].i; for(int i = 2; i < numargs && val; i++) { val = args[i-1].i != args[i].i; } } else { val = (numargs > 0 ? args[0].i : 0) != 0; } intret(static_cast<int>(val)); }; }), "i" "1V", Id_Command);
    addcommand("<", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].i < args[1].i; for(int i = 2; i < numargs && val; i++) { val = args[i-1].i < args[i].i; } } else { val = (numargs > 0 ? args[0].i : 0) < 0; } intret(static_cast<int>(val)); }; }), "i" "1V", Id_Command);
    addcommand(">", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].i > args[1].i; for(int i = 2; i < numargs && val; i++) { val = args[i-1].i > args[i].i; } } else { val = (numargs > 0 ? args[0].i : 0) > 0; } intret(static_cast<int>(val)); }; }), "i" "1V", Id_Command);
    addcommand("<=", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].i <= args[1].i; for(int i = 2; i < numargs && val; i++) { val = args[i-1].i <= args[i].i; } } else { val = (numargs > 0 ? args[0].i : 0) <= 0; } intret(static_cast<int>(val)); }; }), "i" "1V", Id_Command);
    addcommand(">=", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].i >= args[1].i; for(int i = 2; i < numargs && val; i++) { val = args[i-1].i >= args[i].i; } } else { val = (numargs > 0 ? args[0].i : 0) >= 0; } intret(static_cast<int>(val)); }; }), "i" "1V", Id_Command);
    addcommand("^", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val ^ val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val ^ val2; } } else { val = numargs > 0 ? args[0].i : 0; val = ~val; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("~", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val ^ val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val ^ val2; } } else { val = numargs > 0 ? args[0].i : 0; val = ~val; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("&", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val & val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val & val2; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("|", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val | val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val | val2; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("^~", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val ^~ val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val ^~ val2; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("&~", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val &~ val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val &~ val2; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("|~", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val |~ val2; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val |~ val2; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("<<", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val = val2 < 32 ? val << std::max(val2, 0) : 0; for(int i = 2; i < numargs; i++) { val2 = args[i].i; val = val2 < 32 ? val << std::max(val2, 0) : 0; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand(">>", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; val >>= std::clamp(val2, 0, 31); for(int i = 2; i < numargs; i++) { val2 = args[i].i; val >>= std::clamp(val2, 0, 31); } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);

    //floating point operators, used with <operator>f, i.e. +f or *f
    addcommand("+" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val; if(numargs >= 2) { val = args[0].f; float val2 = args[1].f; val = val + val2; for(int i = 2; i < numargs; i++) { val2 = args[i].f; val = val + val2; } } else { val = numargs > 0 ? args[0].f : 0; ; } floatret(val); }; }), "f" "1V", Id_Command);
    addcommand("*" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val; if(numargs >= 2) { val = args[0].f; float val2 = args[1].f; val = val * val2; for(int i = 2; i < numargs; i++) { val2 = args[i].f; val = val * val2; } } else { val = numargs > 0 ? args[0].f : 1; ; } floatret(val); }; }), "f" "1V", Id_Command);
    addcommand("-" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val; if(numargs >= 2) { val = args[0].f; float val2 = args[1].f; val = val - val2; for(int i = 2; i < numargs; i++) { val2 = args[i].f; val = val - val2; } } else { val = numargs > 0 ? args[0].f : 0; val = -val; } floatret(val); }; }), "f" "1V", Id_Command);
    addcommand("=" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].f == args[1].f; for(int i = 2; i < numargs && val; i++) { val = args[i-1].f == args[i].f; } } else { val = (numargs > 0 ? args[0].f : 0) == 0; } intret(static_cast<int>(val)); }; }), "f" "1V", Id_Command);
    addcommand("!=" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].f != args[1].f; for(int i = 2; i < numargs && val; i++) { val = args[i-1].f != args[i].f; } } else { val = (numargs > 0 ? args[0].f : 0) != 0; } intret(static_cast<int>(val)); }; }), "f" "1V", Id_Command);
    addcommand("<" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].f < args[1].f; for(int i = 2; i < numargs && val; i++) { val = args[i-1].f < args[i].f; } } else { val = (numargs > 0 ? args[0].f : 0) < 0; } intret(static_cast<int>(val)); }; }), "f" "1V", Id_Command);
    addcommand(">" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].f > args[1].f; for(int i = 2; i < numargs && val; i++) { val = args[i-1].f > args[i].f; } } else { val = (numargs > 0 ? args[0].f : 0) > 0; } intret(static_cast<int>(val)); }; }), "f" "1V", Id_Command);
    addcommand("<=" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].f <= args[1].f; for(int i = 2; i < numargs && val; i++) { val = args[i-1].f <= args[i].f; } } else { val = (numargs > 0 ? args[0].f : 0) <= 0; } intret(static_cast<int>(val)); }; }), "f" "1V", Id_Command);
    addcommand(">=" "f", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = args[0].f >= args[1].f; for(int i = 2; i < numargs && val; i++) { val = args[i-1].f >= args[i].f; } } else { val = (numargs > 0 ? args[0].f : 0) >= 0; } intret(static_cast<int>(val)); }; }), "f" "1V", Id_Command);

    addcommand("!", reinterpret_cast<identfun>(+[] (tagval *a) { intret(getbool(*a) ? 0 : 1); }), "t", Id_Not);
    addcommand("&&", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { if(!numargs) { intret(1); } else { for(int i = 0; i < numargs; ++i) { if(i) { freearg(*commandret); } if(args[i].type == Value_Code) { executeret(args[i].code, *commandret); } else { *commandret = args[i]; } if(!getbool(*commandret)) { break; } } } }; }), "E1V", Id_And);

    addcommand("||", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { if(!numargs) { intret(0); } else { for(int i = 0; i < numargs; ++i) { if(i) { freearg(*commandret); } if(args[i].type == Value_Code) { executeret(args[i].code, *commandret); } else { *commandret = args[i]; } if(getbool(*commandret)) { break; } } } }; }), "E1V", Id_Or);

    //int division
    addcommand("div", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; { if(val2) val /= val2; else val = 0; }; for(int i = 2; i < numargs; i++) { val2 = args[i].i; { if(val2) val /= val2; else val = 0; }; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("mod", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val; if(numargs >= 2) { val = args[0].i; int val2 = args[1].i; { if(val2) val %= val2; else val = 0; }; for(int i = 2; i < numargs; i++) { val2 = args[i].i; { if(val2) val %= val2; else val = 0; }; } } else { val = numargs > 0 ? args[0].i : 0; ; } intret(val); }; }), "i" "1V", Id_Command);
    //float division
    addcommand("divf", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val; if(numargs >= 2) { val = args[0].f; float val2 = args[1].f; { if(val2) val /= val2; else val = 0; }; for(int i = 2; i < numargs; i++) { val2 = args[i].f; { if(val2) val /= val2; else val = 0; }; } } else { val = numargs > 0 ? args[0].f : 0; ; } floatret(val); }; }), "f" "1V", Id_Command);
    addcommand("modf", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val; if(numargs >= 2) { val = args[0].f; float val2 = args[1].f; { if(val2) val = std::fmod(val, val2); else val = 0; }; for(int i = 2; i < numargs; i++) { val2 = args[i].f; { if(val2) val = std::fmod(val, val2); else val = 0; }; } } else { val = numargs > 0 ? args[0].f : 0; ; } floatret(val); }; }), "f" "1V", Id_Command);
    addcommand("pow", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val; if(numargs >= 2) { val = args[0].f; float val2 = args[1].f; val = std::pow(val, val2); for(int i = 2; i < numargs; i++) { val2 = args[i].f; val = std::pow(val, val2); } } else { val = numargs > 0 ? args[0].f : 0; ; } floatret(val); }; }), "f" "1V", Id_Command);

    //float transcendentals
    addcommand("sin", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::sin(*a*RAD)); }), "f", Id_Command);
    addcommand("cos", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::cos(*a*RAD)); }), "f", Id_Command);
    addcommand("tan", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::tan(*a*RAD)); }), "f", Id_Command);
    addcommand("asin", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::asin(*a)/RAD); }), "f", Id_Command);
    addcommand("acos", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::acos(*a)/RAD); }), "f", Id_Command);
    addcommand("atan", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::atan(*a)/RAD); }), "f", Id_Command);
    addcommand("atan2", reinterpret_cast<identfun>(+[] (float *y, float *x) { floatret(std::atan2(*y, *x)/RAD); }), "ff", Id_Command);
    addcommand("sqrt", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::sqrt(*a)); }), "f", Id_Command);
    addcommand("loge", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::log(*a)); }), "f", Id_Command);
    addcommand("log2", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::log(*a)/M_LN2); }), "f", Id_Command);
    addcommand("log10", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::log10(*a)); }), "f", Id_Command);
    addcommand("exp", reinterpret_cast<identfun>(+[] (float *a) { floatret(std::exp(*a)); }), "f", Id_Command);

    addcommand("min", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val = numargs > 0 ? args[0].i : 0; for(int i = 1; i < numargs; i++) { val = std::min(val, args[i].i); } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("max", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val = numargs > 0 ? args[0].i : 0; for(int i = 1; i < numargs; i++) { val = std::max(val, args[i].i); } intret(val); }; }), "i" "1V", Id_Command);
    addcommand("minf", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val = numargs > 0 ? args[0].f : 0; for(int i = 1; i < numargs; i++) { val = std::min(val, args[i].f); } floatret(val); }; }), "f" "1V", Id_Command);
    addcommand("maxf", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val = numargs > 0 ? args[0].f : 0; for(int i = 1; i < numargs; i++) { val = std::max(val, args[i].f); } floatret(val); }; }), "f" "1V", Id_Command);

    addcommand("bitscan", reinterpret_cast<identfun>(+[] (int *n) { intret(BITSCAN(*n)); }), "i", Id_Command);

    addcommand("abs", reinterpret_cast<identfun>(+[] (int *n) { intret(std::abs(*n)); }), "i", Id_Command);
    addcommand("absf", reinterpret_cast<identfun>(+[] (float *n) { floatret(std::fabs(*n)); }), "f", Id_Command);

    addcommand("floor", reinterpret_cast<identfun>(+[] (float *n) { floatret(std::floor(*n)); }), "f", Id_Command);
    addcommand("ceil", reinterpret_cast<identfun>(+[] (float *n) { floatret(std::ceil(*n)); }), "f", Id_Command);
    addcommand("round", reinterpret_cast<identfun>(+[] (float *n, float *k) { { double step = *k; double r = *n; if(step > 0) { r += step * (r < 0 ? -0.5 : 0.5); r -= std::fmod(r, step); } else { r = r < 0 ? std::ceil(r - 0.5) : std::floor(r + 0.5); } floatret(static_cast<float>(r)); }; }), "ff", Id_Command);

    addcommand("cond", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { for(int i = 0; i < numargs; i += 2) { if(i+1 < numargs) { if(executebool(args[i].code)) { executeret(args[i+1].code, *commandret); break; } } else { executeret(args[i].code, *commandret); break; } } }; }), "ee2V", Id_Command);

    addcommand("case", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { int val = args[0].getint(); int i; for(i = 1; i+1 < numargs; i += 2) { if(args[i].type == Value_Null || args[i].getint() == val) { executeret(args[i+1].code, *commandret); return; } } }; }), "i" "te2V", Id_Command);
    addcommand("casef", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { float val = args[0].getfloat(); int i; for(i = 1; i+1 < numargs; i += 2) { if(args[i].type == Value_Null || args[i].getfloat() == val) { executeret(args[i+1].code, *commandret); return; } } }; }), "f" "te2V", Id_Command);
    addcommand("cases", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { const char * val = args[0].getstr(); int i; for(i = 1; i+1 < numargs; i += 2) { if(args[i].type == Value_Null || !std::strcmp(args[i].getstr(), val)) { executeret(args[i+1].code, *commandret); return; } } }; }), "s" "te2V", Id_Command);

    addcommand("rnd", reinterpret_cast<identfun>(+[] (int *a, int *b) { intret(*a - *b > 0 ? randomint(*a - *b) + *b : *b); }), "ii", Id_Command);
    addcommand("rndstr", reinterpret_cast<identfun>(+[] (int *len) { { int n = std::clamp(*len, 0, 10000); char *s = newstring(n); for(int i = 0; i < n;) { int r = rand(); for(int j = std::min(i + 4, n); i < j; i++) { s[i] = (r%255) + 1; r /= 255; } } s[n] = '\0'; stringret(s); }; }), "i", Id_Command);

    addcommand("tohex", reinterpret_cast<identfun>(+[] (int *n, int *p) { { constexpr int len = 20; char *buf = newstring(len); nformatstring(buf, len, "0x%.*X", std::max(*p, 1), *n); stringret(buf); }; }), "ii", Id_Command);

    addcommand("strcmp", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = std::strcmp(args[0].s, args[1].s) == 0; for(int i = 2; i < numargs && val; i++) { val = std::strcmp(args[i-1].s, args[i].s) == 0; } } else { val = (numargs > 0 ? args[0].s[0] : 0) == 0; } intret(static_cast<int>(val)); }; }), "s1V", Id_Command);
    addcommand("=s", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = std::strcmp(args[0].s, args[1].s) == 0; for(int i = 2; i < numargs && val; i++) { val = std::strcmp(args[i-1].s, args[i].s) == 0; } } else { val = (numargs > 0 ? args[0].s[0] : 0) == 0; } intret(static_cast<int>(val)); }; }), "s1V", Id_Command);
    addcommand("!=s", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = std::strcmp(args[0].s, args[1].s) != 0; for(int i = 2; i < numargs && val; i++) { val = std::strcmp(args[i-1].s, args[i].s) != 0; } } else { val = (numargs > 0 ? args[0].s[0] : 0) != 0; } intret(static_cast<int>(val)); }; }), "s1V", Id_Command);
    addcommand("<s", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = std::strcmp(args[0].s, args[1].s) < 0; for(int i = 2; i < numargs && val; i++) { val = std::strcmp(args[i-1].s, args[i].s) < 0; } } else { val = (numargs > 0 ? args[0].s[0] : 0) < 0; } intret(static_cast<int>(val)); }; }), "s1V", Id_Command);
    addcommand(">s", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = std::strcmp(args[0].s, args[1].s) > 0; for(int i = 2; i < numargs && val; i++) { val = std::strcmp(args[i-1].s, args[i].s) > 0; } } else { val = (numargs > 0 ? args[0].s[0] : 0) > 0; } intret(static_cast<int>(val)); }; }), "s1V", Id_Command);
    addcommand("<=s", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = std::strcmp(args[0].s, args[1].s) <= 0; for(int i = 2; i < numargs && val; i++) { val = std::strcmp(args[i-1].s, args[i].s) <= 0; } } else { val = (numargs > 0 ? args[0].s[0] : 0) <= 0; } intret(static_cast<int>(val)); }; }), "s1V", Id_Command);
    addcommand(">=s", reinterpret_cast<identfun>(+[] (tagval *args, int numargs) { { bool val; if(numargs >= 2) { val = std::strcmp(args[0].s, args[1].s) >= 0; for(int i = 2; i < numargs && val; i++) { val = std::strcmp(args[i-1].s, args[i].s) >= 0; } } else { val = (numargs > 0 ? args[0].s[0] : 0) >= 0; } intret(static_cast<int>(val)); }; }), "s1V", Id_Command);
}

char *strreplace(const char *s, const char *oldval, const char *newval, const char *newval2)
{
    vector<char> buf;

    int oldlen = std::strlen(oldval);
    if(!oldlen)
    {
        return newstring(s);
    }
    for(int i = 0;; i++)
    {
        const char *found = std::strstr(s, oldval);
        if(found)
        {
            while(s < found)
            {
                buf.add(*s++);
            }
            for(const char *n = i&1 ? newval2 : newval; *n; n++)
            {
                buf.add(*n);
            }
            s = found + oldlen;
        }
        else
        {
            while(*s)
            {
                buf.add(*s++);
            }
            buf.add('\0');
            return newstring(buf.getbuf(), buf.length());
        }
    }
}

void strsplice(const char *s, const char *vals, int *skip, int *count)
{
    int slen   = std::strlen(s),
        vlen   = std::strlen(vals),
        offset = std::clamp(*skip, 0, slen),
        len    = std::clamp(*count, 0, slen - offset);
    char *p = newstring(slen - len + vlen);
    if(offset)
    {
        memcpy(p, s, offset);
    }
    if(vlen)
    {
        memcpy(&p[offset], vals, vlen);
    }
    if(offset + len < slen)
    {
        memcpy(&p[offset + vlen], &s[offset + len], slen - (offset + len));
    }
    p[slen - len + vlen] = '\0';
    commandret->setstr(p);
}

//external api function, for loading the string manip functions into the global hashtable
void initstrcmds()
{
    addcommand("echo", reinterpret_cast<identfun>(+[] (char *s) { conoutf("\f1%s", s); }), "C", Id_Command);
    addcommand("error", reinterpret_cast<identfun>(+[] (char *s) { conoutf(Console_Error, "%s", s); }), "C", Id_Command);
    addcommand("strstr", reinterpret_cast<identfun>(+[] (char *a, char *b) { { char *s = std::strstr(a, b); intret(s ? s-a : -1); }; }), "ss", Id_Command);
    addcommand("strlen", reinterpret_cast<identfun>(+[] (char *s) { intret(std::strlen(s)); }), "s", Id_Command);
    addcommand("strcode", reinterpret_cast<identfun>(+[] (char *s, int *i) { intret(*i > 0 ? (memchr(s, 0, *i) ? 0 : static_cast<uchar>(s[*i])) : static_cast<uchar>(s[0])); }), "si", Id_Command);

    addcommand("codestr", reinterpret_cast<identfun>(+[] (int *i) { { char *s = newstring(1); s[0] = static_cast<char>(*i); s[1] = '\0'; stringret(s); }; }), "i", Id_Command);

    addcommand("struni", reinterpret_cast<identfun>(+[] (char *s, int *i) { intret(*i > 0 ? (memchr(s, 0, *i) ? 0 : cube2uni(s[*i])) : cube2uni(s[0])); }), "si", Id_Command);
    addcommand("unistr", reinterpret_cast<identfun>(+[] (int *i) { { char *s = newstring(1); s[0] = uni2cube(*i); s[1] = '\0'; stringret(s); }; }), "i", Id_Command);

    addcommand("strlower", reinterpret_cast<identfun>(+[] (char *s) { { int len = std::strlen(s); char *m = newstring(len); for(int i = 0; i < static_cast<int>(len); ++i) { m[i] = cubelower(s[i]); } m[len] = '\0'; stringret(m); }; }), "s", Id_Command);
    addcommand("strupper", reinterpret_cast<identfun>(+[] (char *s) { { int len = std::strlen(s); char *m = newstring(len); for(int i = 0; i < static_cast<int>(len); ++i) { m[i] = cubeupper(s[i]); } m[len] = '\0'; stringret(m); }; }), "s", Id_Command);

    addcommand("strsplice", (identfun)strsplice, "ssii", Id_Command);
    addcommand("strreplace", reinterpret_cast<identfun>(+[] (char *s, char *o, char *n, char *n2) { commandret->setstr(strreplace(s, o, n, n2[0] ? n2 : n)); }), "ssss", Id_Command);
}

bool _icmd_getmillis = addcommand("getmillis", reinterpret_cast<identfun>(+[] (int *total) { intret(*total ? totalmillis : lastmillis); }), "i", Id_Command);

struct sleepcmd
{
    int delay, millis, flags;
    char *command;
};
vector<sleepcmd> sleepcmds;

void addsleep(int *msec, char *cmd)
{
    sleepcmd &s = sleepcmds.add();
    s.delay = std::max(*msec, 1);
    s.millis = lastmillis;
    s.command = newstring(cmd);
    s.flags = identflags;
}

static bool dummy_addsleep = addcommand("sleep", (identfun)addsleep, "is", Id_Command);

void checksleep(int millis)
{
    for(int i = 0; i < sleepcmds.length(); i++)
    {
        sleepcmd &s = sleepcmds[i];
        if(millis - s.millis >= s.delay)
        {
            char *cmd = s.command; // execute might create more sleep commands
            s.command = nullptr;
            int oldflags = identflags;
            identflags = s.flags;
            execute(cmd);
            identflags = oldflags;
            delete[] cmd;
            if(sleepcmds.inrange(i) && !sleepcmds[i].command)
            {
                sleepcmds.remove(i--);
            }
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

static bool dummy_clearsleep_ = addcommand("clearsleep", (identfun)clearsleep_, "i", Id_Command);

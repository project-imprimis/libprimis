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
COMMAND(exec, "sb");

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
COMMANDN(escape, escapecmd, "s");

static void unescapecmd(char *s)
{
    int len = std::strlen(s);
    char *d = newstring(len);
    unescapestring(d, s, &s[len]);
    stringret(d);
}
COMMANDN(unescape, unescapecmd, "s");

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
COMMAND(writecfg, "s");

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

ICOMMANDKN(do, Id_Do, docmd, "e", (uint *body), executeret(body, *commandret));

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
COMMANDK(doargs, Id_DoArgs, "e");

ICOMMANDKN(if, Id_If, ifcmd, "tee", (tagval *cond, uint *t, uint *f), executeret(getbool(*cond) ? t : f, *commandret));
ICOMMANDN(?, boolcmd, "tTT", (tagval *cond, tagval *t, tagval *f), result(*(getbool(*cond) ? t : f)));

ICOMMAND(pushif, "rTe", (ident *id, tagval *v, uint *code),
{
    if(id->type != Id_Alias || id->index < Max_Args)
    {
        return;
    }
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
ICOMMAND(loop,                  "rie", (ident *id, int *n, uint *body), doloop(*id, 0, *n, 1, body));
ICOMMANDN(loop+, looppluscmd,  "riie", (ident *id, int *offset, int *n, uint *body), doloop(*id, *offset, *n, 1, body));
ICOMMANDN(loop*, loopstarcmd,  "riie", (ident *id, int *step, int *n, uint *body), doloop(*id, 0, *n, *step, body));
ICOMMANDN(loop+*,loopstarplus,"riiie", (ident *id, int *offset, int *step, int *n, uint *body), doloop(*id, *offset, *n, *step, body));

ICOMMANDN(while, whilecmd, "ee", (uint *cond, uint *body), while(executebool(cond)) execute(body));

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
ICOMMAND(loopconcat, "rie", (ident *id, int *n, uint *body), loopconc(*id, 0, *n, body, true));
ICOMMANDN(loopconcat+, loopconcatplus, "riie", (ident *id, int *offset, int *n, uint *body), loopconc(*id, *offset, *n, body, true));

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
COMMAND(format, "V");

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
COMMANDN(listlen, listlencmd, "s");

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
COMMAND(at, "si1V");

void substr(char *s, int *start, int *count, int *numargs)
{
    int len = std::strlen(s),
        offset = std::clamp(*start, 0, len);
    commandret->setstr(newstring(&s[offset], *numargs >= 3 ? std::clamp(*count, 0, len - offset) : len - offset));
}
COMMAND(substr, "siiN");

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
COMMAND(sublist, "siiN");

void stripcolors(char *s)
{
    int len = std::strlen(s);
    char *d = newstring(len);
    filtertext(d, s, true, false, len);
    stringret(d);
}
COMMAND(stripcolors, "s");

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
COMMAND(listfind, "rse");

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
COMMANDN(listfind=, listfindeq, "sii");

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
COMMANDN(listassoc=, listassoceq, "si");

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
COMMAND(looplist, "rse");

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
COMMAND(looplist2, "rrse");

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
COMMAND(looplist3, "rrrse");

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
ICOMMAND(looplistconcat, "rse", (ident *id, char *list, uint *body), looplistconc(id, list, body, true));
ICOMMAND(looplistconcatword, "rse", (ident *id, char *list, uint *body), looplistconc(id, list, body, false));

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
COMMAND(listcount, "rse");

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
COMMAND(prettylist, "ss");

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
ICOMMAND(indexof, "ss", (char *list, char *elem), intret(listincludes(list, elem, std::strlen(elem))));

//================================================================= LISTMERGECMD
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
LISTMERGECMD(listunion, p.put(list, std::strlen(list)), elems, list, <);
#undef LISTMERGECMD
//==============================================================================

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
COMMAND(listsplice, "ssii");

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
COMMAND(loopfiles, "rsse");

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
COMMANDN(findfile, findfile_, "s");

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
    pusharg(*x, nullval, xstack);
    x->flags &= ~Idf_Unknown;
    pusharg(*y, nullval, ystack);
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
COMMAND(sortlist, "srree");
ICOMMAND(uniquelist, "srre", (char *list, ident *x, ident *y, uint *body), sortlist(list, x, y, nullptr, body));

//===========MATHCMD MATHICMDN MATHICMD MATHFCMDN MATHFCMD CMPCMD CMPICMDN CMPICMD CMPFCMDN CMPFCMD DIVCMD MINMAXCMD CASECOMMAND CMPSCMD
#define MATHCMD(name, alias, fmt, type, op, initval, unaryop) \
    ICOMMANDNS(name, alias, #fmt "1V", (tagval *args, int numargs), \
    { \
        type val; \
        if(numargs >= 2) \
        { \
            val = args[0].fmt; \
            type val2 = args[1].fmt; \
            op; \
            for(int i = 2; i < numargs; i++) \
            { \
                val2 = args[i].fmt; \
                op; \
            } \
        } \
        else \
        { \
            val = numargs > 0 ? args[0].fmt : initval; \
            unaryop; \
        } \
        type##ret(val); \
    })
#define MATHICMDN(name, alias, op, initval, unaryop) MATHCMD(#name, alias, i, int, val = val op val2, initval, unaryop)
#define MATHICMD(name, alias, initval, unaryop) MATHICMDN(name, alias, name, initval, unaryop)
#define MATHFCMDN(name, alias, op, initval, unaryop) MATHCMD(#name "f", alias, f, float, val = val op val2, initval, unaryop)
#define MATHFCMD(name, alias, initval, unaryop) MATHFCMDN(name, alias, name, initval, unaryop)

#define CMPCMD(name, alias, fmt, type, op) \
    ICOMMANDNS(name, alias, #fmt "1V", (tagval *args, int numargs), \
    { \
        bool val; \
        if(numargs >= 2) \
        { \
            val = args[0].fmt op args[1].fmt; /* note here the bizzare syntax caused by macro substitution */ \
            for(int i = 2; i < numargs && val; i++) \
            { \
                val = args[i-1].fmt op args[i].fmt; /* note here the bizzare syntax caused by macro substitution */ \
            } \
        } \
        else \
        { \
            val = (numargs > 0 ? args[0].fmt : 0) op 0; \
        } \
        intret(static_cast<int>(val)); \
    })
#define CMPICMDN(name, alias, op) CMPCMD(#name, alias, i, int, op)
#define CMPICMD(name, alias) CMPICMDN(name, alias, name)
#define CMPFCMDN(name, alias, op) CMPCMD(#name "f", alias, f, float, op)
#define CMPFCMD(name, alias) CMPFCMDN(name, alias, name)

//integer and boolean operators, used with named symbol, i.e. + or *
//no native boolean type, they are treated like integers
MATHICMD(+, plus, 0, ); //0 substituted if nothing passed in arg2: n + 0 is still n
MATHICMD(*, star, 1, ); //1 substituted if nothing passed in arg2: n * 1 is still n
MATHICMD(-, minus, 0, val = -val); //the minus operator inverts if used as unary
CMPICMDN(=, equal, ==);
CMPICMD(!=, neq);
CMPICMD(<, lt);
CMPICMD(>, gt);
CMPICMD(<=, le);
CMPICMD(>=, ge);
MATHICMD(^, inv, 0, val = ~val);
MATHICMDN(~, notc, ^, 0, val = ~val);
MATHICMD(&, andc, 0, );
MATHICMD(|, orc, 0, );
MATHICMD(^~, invc, 0, );
MATHICMD(&~, nand, 0, );
MATHICMD(|~, nor, 0, );
MATHCMD("<<", lsft, i, int, val = val2 < 32 ? val << std::max(val2, 0) : 0, 0, );
MATHCMD(">>", rsft, i, int, val >>= std::clamp(val2, 0, 31), 0, );

//floating point operators, used with <operator>f, i.e. +f or *f
MATHFCMD(+, fplus, 0, );
MATHFCMD(*, fstar, 1, );
MATHFCMD(-, fminus, 0, val = -val);
CMPFCMDN(=, feq, ==);
CMPFCMD(!=, fneq);
CMPFCMD(<, flt);
CMPFCMD(>, fgt);
CMPFCMD(<=, fle);
CMPFCMD(>=, fge);

ICOMMANDKN(!, Id_Not, notcmd, "t", (tagval *a), intret(getbool(*a) ? 0 : 1));
ICOMMANDKN(&&, Id_And, andcmd, "E1V", (tagval *args, int numargs),
{
    if(!numargs)
    {
        intret(1);
    }
    else
    {
        for(int i = 0; i < numargs; ++i)
        {
            if(i)
            {
                freearg(*commandret);
            }
            if(args[i].type == Value_Code)
            {
                executeret(args[i].code, *commandret);
            }
            else
            {
                *commandret = args[i];
            }
            if(!getbool(*commandret))
            {
                break;
            }
        }
    }
});
ICOMMANDKN(||, Id_Or, orcmd, "E1V", (tagval *args, int numargs),
{
    if(!numargs)
    {
        intret(0);
    }
    else
    {
        for(int i = 0; i < numargs; ++i)
        {
            if(i)
            {
                freearg(*commandret);
            }
            if(args[i].type == Value_Code)
            {
                executeret(args[i].code, *commandret);
            }
            else
            {
                *commandret = args[i];
            }
            if(getbool(*commandret))
            {
                break;
            }
        }
    }
});


#define DIVCMD(name, alias, fmt, type, op) MATHCMD(#name, alias, fmt, type, { if(val2) op; else val = 0; }, 0, )

//int division
DIVCMD(div, divc, i, int, val /= val2);
DIVCMD(mod, modc, i, int, val %= val2);
//float division
DIVCMD(divf, divfc, f, float, val /= val2);
DIVCMD(modf, modfc, f, float, val = std::fmod(val, val2));
MATHCMD("pow", power, f, float, val = std::pow(val, val2), 0, );

//float transcendentals
ICOMMAND(sin, "f", (float *a), floatret(std::sin(*a*RAD)));
ICOMMAND(cos, "f", (float *a), floatret(std::cos(*a*RAD)));
ICOMMAND(tan, "f", (float *a), floatret(std::tan(*a*RAD)));
ICOMMAND(asin, "f", (float *a), floatret(std::asin(*a)/RAD));
ICOMMAND(acos, "f", (float *a), floatret(std::acos(*a)/RAD));
ICOMMAND(atan, "f", (float *a), floatret(std::atan(*a)/RAD));
ICOMMAND(atan2, "ff", (float *y, float *x), floatret(std::atan2(*y, *x)/RAD));
ICOMMAND(sqrt, "f", (float *a), floatret(std::sqrt(*a)));
ICOMMAND(loge, "f", (float *a), floatret(std::log(*a)));
ICOMMAND(log2, "f", (float *a), floatret(std::log(*a)/M_LN2));
ICOMMAND(log10, "f", (float *a), floatret(std::log10(*a)));
ICOMMAND(exp, "f", (float *a), floatret(std::exp(*a)));

#define MINMAXCMD(name, fmt, type, op) \
    ICOMMAND(name, #fmt "1V", (tagval *args, int numargs), \
    { \
        type val = numargs > 0 ? args[0].fmt : 0; \
        for(int i = 1; i < numargs; i++) \
        { \
            val = op(val, args[i].fmt); \
        } \
        type##ret(val); \
    })

MINMAXCMD(min, i, int, std::min);
MINMAXCMD(max, i, int, std::max);
MINMAXCMD(minf, f, float, std::min);
MINMAXCMD(maxf, f, float, std::max);

ICOMMAND(bitscan, "i", (int *n), intret(BITSCAN(*n)));

ICOMMAND(abs, "i", (int *n), intret(std::abs(*n)));
ICOMMAND(absf, "f", (float *n), floatret(std::fabs(*n)));

ICOMMAND(floor, "f", (float *n), floatret(std::floor(*n)));
ICOMMAND(ceil, "f", (float *n), floatret(std::ceil(*n)));
ICOMMAND(round, "ff", (float *n, float *k),
{
    double step = *k;
    double r    = *n;
    if(step > 0)
    {
        r += step * (r < 0 ? -0.5 : 0.5);
        r -= std::fmod(r, step);
    }
    else
    {
        r = r < 0 ? std::ceil(r - 0.5) : std::floor(r + 0.5);
    }
    floatret(static_cast<float>(r));
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
CASECOMMAND(cases, "s", const char *, args[0].getstr(), args[i].type == Value_Null || !std::strcmp(args[i].getstr(), val));

ICOMMAND(rnd, "ii", (int *a, int *b), intret(*a - *b > 0 ? randomint(*a - *b) + *b : *b));
ICOMMAND(rndstr, "i", (int *len),
{
    int n = std::clamp(*len, 0, 10000);
    char *s = newstring(n);
    for(int i = 0; i < n;)
    {
        int r = rand();
        for(int j = std::min(i + 4, n); i < j; i++)
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
    constexpr int len = 20;
    char *buf = newstring(len);
    nformatstring(buf, len, "0x%.*X", std::max(*p, 1), *n);
    stringret(buf);
});

//CoMPariSonCoMmanD (CMPSCMD)
#define CMPSCMD(name, alias, op) \
    ICOMMANDN(name, alias, "s1V", (tagval *args, int numargs), \
    { \
        bool val; \
        if(numargs >= 2) \
        { \
            val = std::strcmp(args[0].s, args[1].s) op 0; /* note here the bizzare syntax caused by macro substitution */ \
            for(int i = 2; i < numargs && val; i++) \
            { \
                val = std::strcmp(args[i-1].s, args[i].s) op 0;  /* note here the bizzare syntax caused by macro substitution */ \
            } \
        } \
        else \
        { \
            val = (numargs > 0 ? args[0].s[0] : 0) op 0; /* note here the bizzare syntax caused by macro substitution */ \
        } \
        intret(static_cast<int>(val)); \
    })

CMPSCMD(strcmp, strcomp, ==);
CMPSCMD(=s, eqstr, ==);
CMPSCMD(!=s, neqstr, !=);
CMPSCMD(<s, lts, <);
CMPSCMD(>s, gts, >);
CMPSCMD(<=s, les, <=);
CMPSCMD(>=s, ges, >=);

#undef MATHCMD
#undef MATHICMDN
#undef MATHICMD
#undef MATHFCMDN
#undef MATHFCMD
#undef CMPCMD
#undef CMPICMDN
#undef CMPICMD
#undef CMPFCMDN
#undef CMPFCMD
#undef DIVCMD
#undef MINMAXCMD
#undef CASECOMMAND
#undef CMPSCMD
//======================================================================================================================================

ICOMMAND(echo, "C", (char *s), conoutf("\f1%s", s));
ICOMMAND(error, "C", (char *s), conoutf(Console_Error, "%s", s));
ICOMMAND(strstr, "ss", (char *a, char *b), { char *s = std::strstr(a, b); intret(s ? s-a : -1); });
ICOMMAND(strlen, "s", (char *s), intret(std::strlen(s)));
ICOMMAND(strcode, "si", (char *s, int *i), intret(*i > 0 ? (memchr(s, 0, *i) ? 0 : static_cast<uchar>(s[*i])) : static_cast<uchar>(s[0])));

ICOMMAND(codestr, "i", (int *i),
{
    char *s = newstring(1);
    s[0] = static_cast<char>(*i);
    s[1] = '\0';
    stringret(s);
});

ICOMMAND(struni, "si", (char *s, int *i), intret(*i > 0 ? (memchr(s, 0, *i) ? 0 : cube2uni(s[*i])) : cube2uni(s[0])));
ICOMMAND(unistr, "i", (int *i),
{
    char *s = newstring(1);
    s[0] = uni2cube(*i);
    s[1] = '\0';
    stringret(s);
});

//================================================================ STRMAPCOMMAND
#define STRMAPCOMMAND(name, map) \
    ICOMMAND(name, "s", (char *s), \
    { \
        int len = std::strlen(s); \
        char *m = newstring(len); \
        for(int i = 0; i < static_cast<int>(len); ++i) \
        { \
            m[i] = map(s[i]); \
        } \
        m[len] = '\0'; \
        stringret(m); \
    })

STRMAPCOMMAND(strlower, cubelower);
STRMAPCOMMAND(strupper, cubeupper);
#undef STRMAPCOMMAND
//==============================================================================
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

ICOMMAND(strreplace, "ssss", (char *s, char *o, char *n, char *n2), commandret->setstr(strreplace(s, o, n, n2[0] ? n2 : n)));

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
COMMAND(strsplice, "ssii");

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
    s.delay = std::max(*msec, 1);
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

COMMANDN(clearsleep, clearsleep_, "i");

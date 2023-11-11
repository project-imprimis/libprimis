/* textedit.cpp: ui text editing functionality
 *
 * libprimis supports text entry and large text editor blocks, for creating user
 * interfaces that require input (files, string fields)
 *
 * For the objects which are actually called by ui.cpp, see textedit.h
 *
 */

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "textedit.h"
#include "render/rendertext.h"
#include "render/renderttf.h"
#include "render/shader.h"
#include "render/shaderparam.h"
#include "render/texture.h"

// editline

inline void text_bounds(const char *str, int &width, int &height, int maxwidth = -1)
{
    float widthf, heightf;
    text_boundsf(str, widthf, heightf, maxwidth);
    width = static_cast<int>(std::ceil(widthf));
    height = static_cast<int>(std::ceil(heightf));
}

inline void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth)
{
    float cxf, cyf;
    text_posf(str, cursor, cxf, cyf, maxwidth);
    cx = static_cast<int>(cxf);
    cy = static_cast<int>(cyf);
}

bool EditLine::empty()
{
    return len <= 0;
}

void EditLine::clear()
{
    delete[] text;
    text = nullptr;
    len = maxlen = 0;
}

bool EditLine::grow(int total, const char *fmt, ...)
{
    if(total + 1 <= maxlen)
    {
        return false;
    }
    maxlen = (total + Chunk_Size) - total%Chunk_Size;
    char *newtext = new char[maxlen];
    if(fmt)
    {
        va_list args;
        va_start(args, fmt);
        vformatstring(newtext, fmt, args, maxlen);
        va_end(args);
    }
    else
    {
        newtext[0] = '\0';
    }
    delete[] text;
    text = newtext;
    return true;
}

void EditLine::set(const char *str, int slen)
{
    if(slen < 0)
    {
        slen = std::strlen(str);
        if(!grow(slen, "%s", str))
        {
            std::memcpy(text, str, slen + 1);
        }
    }
    else
    {
        grow(slen);
        std::memcpy(text, str, slen);
        text[slen] = '\0';
    }
    len = slen;
}

void EditLine::prepend(const char *str)
{
    int slen = std::strlen(str);
    if(!grow(slen + len, "%s%s", str, text ? text : ""))
    {
        std::memmove(&text[slen], text, len + 1);
        std::memcpy(text, str, slen + 1);
    }
    len += slen;
}

void EditLine::append(const char *str)
{
    int slen = std::strlen(str);
    if(!grow(len + slen, "%s%s", text ? text : "", str))
    {
        std::memcpy(&text[len], str, slen + 1);
    }
    len += slen;
}

bool EditLine::read(std::fstream& f, int chop)
{
    if(chop < 0)
    {
        chop = INT_MAX;
    }
    else
    {
        chop++;
    }
    set("");
    while(len + 1 < chop && f.getline(&text[len], std::min(maxlen, chop) - len))
    {
        len += std::strlen(&text[len]);
        if(len > 0 && text[len-1] == '\n')
        {
            text[--len] = '\0';
            return true;
        }
        if(len + 1 >= maxlen && len + 1 < chop)
        {
            grow(len + Chunk_Size, "%s", text);
        }
    }
    if(len + 1 >= chop)
    {
        char buf[Chunk_Size];
        while(f.getline(buf, sizeof(buf)))
        {
            int blen = std::strlen(buf);
            if(blen > 0 && buf[blen-1] == '\n')
            {
                return true;
            }
        }
    }
    return len > 0;
}

void EditLine::del(int start, int count)
{
    if(!text)
    {
        return;
    }
    if(start < 0)
    {
        count += start;
        start = 0;
    }
    if(count <= 0 || start >= len)
    {
        return;
    }
    if(start + count > len)
    {
        count = len - start - 1;
    }
    std::memmove(&text[start], &text[start+count], len + 1 - (start + count));
    len -= count;
}

void EditLine::chop(int newlen)
{
    if(!text)
    {
        return;
    }
    len = std::clamp(newlen, 0, len);
    text[len] = '\0';
}

void EditLine::insert(char *str, int start, int count)
{
    if(count <= 0)
    {
        count = std::strlen(str);
    }
    start = std::clamp(start, 0, len);
    grow(len + count, "%s", text ? text : "");
    std::memmove(&text[start + count], &text[start], len - start + 1);
    std::memcpy(&text[start], str, count);
    len += count;
}

void EditLine::combinelines(std::vector<EditLine> &src)
{
    if(src.empty())
    {
        set("");
    }
    else
    {
        for(uint i = 0; i < src.size(); i++)
        {
            if(i)
            {
                append("\n");
            }
            if(!i)
            {
                set(src[i].text, src[i].len);
            }
            else
            {
                insert(src[i].text, len, src[i].len);
            }
        }
    }
}

// editor

bool Editor::empty()
{
    return lines.size() == 1 && lines[0].empty();
}

void Editor::clear(const char *init)
{
    cx = cy = 0;
    mark(false);
    for(EditLine &i : lines)
    {
        i.clear();
    }
    lines.clear();
    if(init)
    {
        lines.emplace_back();
        lines.back().set(init);
    }
}

void Editor::init(const char *inittext)
{
    if(std::strcmp(lines[0].text, inittext))
    {
        clear(inittext);
    }
}

void Editor::updateheight()
{
    int width;
    text_bounds(lines[0].text, width, pixelheight, pixelwidth);
}

void Editor::setfile(const char *fname)
{
    delete[] filename;
    if(fname)
    {
        filename = newstring(fname);
    }
    else
    {
        filename = nullptr;
    }
}

bool Editor::readback(std::fstream& file)
{
    lines.emplace_back();
    return lines.back().read(file, maxx) && (maxy < 0 || static_cast<int>(lines.size()) <= maxy);
}

void Editor::load()
{
    if(!filename)
    {
        return;
    }
    clear(nullptr);
    std::fstream file;
    file.open(filename);
    if(file)
    {
        while(readback(file))
        {
            lines.back().clear();
            lines.pop_back();
        }
    }
    if(lines.empty())
    {
        lines.emplace_back();
        lines.back().set("");
    }
    file.close();
}

void Editor::save()
{
    if(!filename)
    {
        return;
    }
    std::fstream file;
    file.open(filename);
    if(!file)
    {
        return;
    }
    for(uint i = 0; i < lines.size(); i++)
    {
        file << lines[i].text;
    }
    file.close();
}

void Editor::mark(bool enable)
{
    mx = (enable) ? cx : -1;
    my = cy;
}

void Editor::selectall()
{
    mx = my = INT_MAX;
    cx = cy = 0;
}

// constrain results to within buffer - s=start, e=end, return true if a selection range
// also ensures that cy is always within lines[] and cx is valid
bool Editor::region(int &sx, int &sy, int &ex, int &ey)
{
    uint n = lines.size();
    if(cy < 0)
    {
        cy = 0;
    }
    else if(cy >= static_cast<int>(n))
    {
        cy = n-1;
    }
    int len = lines[cy].len;
    if(cx < 0)
    {
        cx = 0;
    }
    else if(cx > len)
    {
        cx = len;
    }
    if(mx >= 0)
    {
        if(my < 0)
        {
            my = 0;
        }
        else if(my >= static_cast<int>(n))
        {
            my = n-1;
        }
        len = lines[my].len;
        if(mx > len)
        {
            mx = len;
        }
        sx = mx; sy = my;
    }
    else
    {
        sx = cx;
        sy = cy;
    }
    ex = cx;
    ey = cy;
    if(sy > ey)
    {
        std::swap(sy, ey);
        std::swap(sx, ex);
    }
    else if(sy==ey && sx > ex)
    {
        std::swap(sx, ex);
    }
    if(mx >= 0)
    {
        ex++;
    }
    return (sx != ex) || (sy != ey);
}

bool Editor::region()
{
    int sx, sy, ex, ey;
    return region(sx, sy, ex, ey);
}

// also ensures that cy is always within lines[] and cx is valid
EditLine &Editor::currentline()
{
    uint n = lines.size();
    if(cy < 0)
    {
        cy = 0;
    }
    else if(cy >= static_cast<int>(n))
    {
        cy = n-1;
    }
    if(cx < 0)
    {
        cx = 0;
    }
    else if(cx > lines[cy].len)
    {
        cx = lines[cy].len;
    }
    return lines[cy];
}

void Editor::copyselectionto(Editor *b)
{
    if(b==this)
    {
        return;
    }
    b->clear(nullptr);
    int sx, sy, ex, ey;
    region(sx, sy, ex, ey);
    for(int i = 0; i < 1+ey-sy; ++i)
    {
        if(b->maxy != -1 && static_cast<int>(b->lines.size()) >= b->maxy)
        {
            break;
        }
        int y = sy+i;
        char *line = lines[y].text;
        int len = lines[y].len;
        if(y == sy && y == ey)
        {
            line += sx;
            len = ex - sx;
        }
        else if(y == sy)
        {
            line += sx;
        }
        else if(y == ey)
        {
            len = ex;
        }
        b->lines.emplace_back();
        b->lines.back().set(line, len);
    }
    if(b->lines.empty())
    {
        b->lines.emplace_back();
        b->lines.back().set("");
    }
}

char *Editor::tostring()
{
    int len = 0;
    for(uint i = 0; i < lines.size(); i++)
    {
        len += lines[i].len + 1;
    }
    char *str = newstring(len);
    int offset = 0;
    for(uint i = 0; i < lines.size(); i++)
    {
        EditLine &l = lines[i];
        std::memcpy(&str[offset], l.text, l.len);
        offset += l.len;
        str[offset++] = '\n';
    }
    str[offset] = '\0';
    return str;
}

char *Editor::selectiontostring()
{
    std::vector<char> buf;
    int sx, sy, ex, ey;
    region(sx, sy, ex, ey);
    for(int i = 0; i < 1+ey-sy; ++i)
    {
        int y = sy+i;
        char *line = lines[y].text;
        int len = lines[y].len;
        if(y == sy && y == ey)
        {
            line += sx;
            len = ex - sx;
        }
        else if(y == sy)
        {
            line += sx;
        }
        else if(y == ey)
        {
            len = ex;
        }
        for(int i = 0; i < len; ++i)
        {
            buf.push_back(line[i]);
        }
        buf.push_back('\n');
    }
    buf.push_back('\0');
    return newstring(buf.data(), buf.size()-1);
}

void Editor::removelines(int start, int count)
{
    for(int i = 0; i < count; ++i)
    {
        lines[start+i].clear();
    }
    lines.erase(lines.begin() + start, lines.begin() + start + count);
}

bool Editor::del() // removes the current selection (if any)
{
    int sx, sy, ex, ey;
    if(!region(sx, sy, ex, ey))
    {
        mark(false);
        return false;
    }
    if(sy == ey)
    {
        if(sx == 0 && ex == lines[ey].len)
        {
            removelines(sy, 1);
        }
        else lines[sy].del(sx, ex - sx);
    }
    else
    {
        if(ey > sy+1)
        {
            removelines(sy+1, ey-(sy+1));
            ey = sy+1;
        }
        if(ex == lines[ey].len)
        {
            removelines(ey, 1);
        }
        else
        {
            lines[ey].del(0, ex);
        }
        if(sx == 0)
        {
            removelines(sy, 1);
        }
        else
        {
            lines[sy].del(sx, lines[sy].len - sx);
        }
    }
    if(lines.empty())
    {
        lines.emplace_back();
        lines.back().set("");
    }
    mark(false);
    cx = sx;
    cy = sy;
    EditLine &current = currentline();
    if(cx >= current.len && cy < static_cast<int>(lines.size()) - 1)
    {
        current.append(lines[cy+1].text);
        removelines(cy + 1, 1);
    }
    return true;
}

void Editor::insert(char ch)
{
    del();
    EditLine &current = currentline();
    if(ch == '\n')
    {
        if(maxy == -1 || cy < maxy-1)
        {
            EditLine newline(&current.text[cx]);
            current.chop(cx);
            cy = std::min(static_cast<int>(lines.size()), cy+1);
            lines.insert(lines.begin() + cy, newline);
        }
        else
        {
            current.chop(cx);
        }
        cx = 0;
    }
    else
    {
        int len = current.len;
        if(maxx >= 0 && len > maxx-1)
        {
            len = maxx-1;
        }
        if(cx <= len)
        {
            current.insert(&ch, cx++, 1);
        }
    }
}

void Editor::insert(const char *s)
{
    while(*s)
    {
        insert(*s++);
    }
}

void Editor::insertallfrom(const Editor * const b)
{
    if(b==this)
    {
        return;
    }

    del();

    if(b->lines.size() == 1 || maxy == 1)
    {
        EditLine &current = currentline();
        char *str = b->lines[0].text;
        int slen = b->lines[0].len;
        if(maxx >= 0 && b->lines[0].len + cx > maxx)
        {
            slen = maxx-cx;
        }
        if(slen > 0)
        {
            int len = current.len;
            if(maxx >= 0 && slen + cx + len > maxx)
            {
                len = std::max(0, maxx-(cx+slen));
            }
            current.insert(str, cx, slen);
            cx += slen;
        }
    }
    else
    {
        for(uint i = 0; i < b->lines.size(); i++)
        {
            if(!i)
            {
                lines[cy++].append(b->lines[i].text);
            }
            else if(i >= b->lines.size())
            {
                cx = b->lines[i].len;
                lines[cy].prepend(b->lines[i].text);
            }
            else if(maxy < 0 || static_cast<int>(lines.size()) < maxy)
            {
                lines.insert(lines.begin() + cy++, EditLine(b->lines[i].text));
            }
        }
    }
}

void Editor::scrollup()
{
    cy--;
}

void Editor::scrolldown()
{
    cy++;
}

void Editor::key(int code)
{
    switch(code)
    {
        case SDLK_UP:
        {
            if(linewrap)
            {
                int x, y;
                const char *str = currentline().text;
                text_pos(str, cx+1, x, y, pixelwidth);
                if(y > 0)
                {
                    cx = text_visible(str, x, y-FONTH, pixelwidth);
                    break;
                }
            }
            cy--;
            break;
        }
        case SDLK_DOWN:
        {
            if(linewrap)
            {
                int x, y, width, height;
                const char *str = currentline().text;
                text_pos(str, cx, x, y, pixelwidth);
                text_bounds(str, width, height, pixelwidth);
                y += FONTH;
                if(y < height)
                {
                    cx = text_visible(str, x, y, pixelwidth);
                    break;
                }
            }
            cy++;
            break;
        }
        case SDLK_PAGEUP:
        {
            cy-=pixelheight/FONTH;
            break;
        }
        case SDLK_PAGEDOWN:
        {
            cy+=pixelheight/FONTH;
            break;
        }
        case SDLK_HOME:
        {
            cx = cy = 0;
            break;
        }
        case SDLK_END:
        {
            cx = cy = INT_MAX;
            break;
        }
        case SDLK_LEFT:
        {
            cx--;
            break;
        }
        case SDLK_RIGHT:
        {
            cx++;
            break;
        }
        case SDLK_DELETE:
        {
            if(!del())
            {
                EditLine &current = currentline();
                if(cx < current.len)
                {
                    current.del(cx, 1);
                }
                else if(cy < static_cast<int>(lines.size())-1)
                {   //combine with next line
                    current.append(lines[cy+1].text);
                    removelines(cy+1, 1);
                }
            }
            break;
        }
        case SDLK_BACKSPACE:
        {
            if(!del())
            {
                EditLine &current = currentline();
                if(cx > 0)
                {
                    current.del(--cx, 1);
                }
                else if(cy > 0)
                {   //combine with previous line
                    cx = lines[cy-1].len;
                    lines[cy-1].append(current.text);
                    removelines(cy--, 1);
                }
            }
            break;
        }
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
        {
            break;
        }
        case SDLK_RETURN:
        {
            insert('\n');
            break;
        }
        case SDLK_TAB:
        {
            insert('\t');
            break;
        }
    }
}

void Editor::input(const char *str, int len)
{
    for(int i = 0; i < len; ++i)
    {
        insert(str[i]);
    }
}

void Editor::hit(int hitx, int hity, bool dragged)
{
    int maxwidth = linewrap?pixelwidth:-1,
        h = 0;
    for(uint i = scrolly; i < lines.size(); i++)
    {
        int width, height;
        text_bounds(lines[i].text, width, height, maxwidth);
        if(h + height > pixelheight)
        {
            break;
        }
        if(hity >= h && hity <= h+height)
        {
            int x = text_visible(lines[i].text, hitx, hity-h, maxwidth);
            if(dragged)
            {
                mx = x;
                my = i;
            }
            else
            {
                cx = x;
                cy = i;
            };
            break;
        }
       h+=height;
    }
}

void Editor::draw(int x, int y, int color, bool hit)
{
    int maxwidth = linewrap?pixelwidth:-1,
        sx, sy, ex, ey;
    bool selection = region(sx, sy, ex, ey);
    // fix scrolly so that <cx, cy> is always on screen
    if(cy < scrolly)
    {
        scrolly = cy;
    }
    else
    {
        if(scrolly < 0)
        {
            scrolly = 0;
        }
        int h = 0;
        for(int i = cy; i >= scrolly; i--)
        {
            int width, height;
            text_bounds(lines[i].text, width, height, maxwidth);
            if(h + height > pixelheight)
            {
                scrolly = i+1;
                break;
            }
            h += height;
        }
    }

    if(selection)
    {
        // convert from cursor coords into pixel coords
        int psx, psy, pex, pey;
        text_pos(lines[sy].text, sx, psx, psy, maxwidth);
        text_pos(lines[ey].text, ex, pex, pey, maxwidth);
        int maxy = static_cast<int>(lines.size()),
            h = 0;
        for(int i = scrolly; i < maxy; i++)
        {
            int width, height;
            text_bounds(lines[i].text, width, height, maxwidth);
            if(h + height > pixelheight)
            {
                maxy = i;
                break;
            }
            if(i == sy)
            {
                psy += h;
            }
            if(i == ey)
            {
                pey += h;
                break;
            }
            h += height;
        }
        maxy--;
        if(ey >= scrolly && sy <= maxy)
        {
            // crop top/bottom within window
            if(sy < scrolly)
            {
                sy = scrolly;
                psy = 0;
                psx = 0;
            }
            if(ey > maxy)
            {
                ey = maxy;
                pey = pixelheight - FONTH;
                pex = pixelwidth;
            }
            hudnotextureshader->set();
            gle::colorub(0xA0, 0x80, 0x80);
            gle::defvertex(2);
            gle::begin(GL_TRIANGLE_FAN);
            if(psy == pey)
            {
                gle::attribf(x+psx, y+psy);
                gle::attribf(x+pex, y+psy);
                gle::attribf(x+pex, y+pey+FONTH);
                gle::attribf(x+psx, y+pey+FONTH);
            }
            else
            {   gle::attribf(x+psx,        y+psy);
                gle::attribf(x+psx,        y+psy+FONTH);
                gle::attribf(x+pixelwidth, y+psy+FONTH);
                gle::attribf(x+pixelwidth, y+psy);
                if(pey-psy > FONTH)
                {
                    gle::attribf(x,            y+psy+FONTH);
                    gle::attribf(x+pixelwidth, y+psy+FONTH);
                    gle::attribf(x+pixelwidth, y+pey);
                    gle::attribf(x,            y+pey);
                }
                gle::attribf(x,     y+pey);
                gle::attribf(x,     y+pey+FONTH);
                gle::attribf(x+pex, y+pey+FONTH);
                gle::attribf(x+pex, y+pey);
            }
            gle::end();
        }
    }

    int h = 0;
    for(uint i = scrolly; i < lines.size(); i++)
    {
        int width, height;
        text_bounds(lines[i].text, width, height, maxwidth);
        if(h + height > pixelheight)
        {
            break;
        }
        //draw_text(lines[i].text, x, y+h, color>>16, (color>>8)&0xFF, color&0xFF, 0xFF, hit&&(static_cast<uint>(cy)==i)?cx:-1, maxwidth);
        ttr.renderttf(lines[i].text, {static_cast<uchar>(color>>16), static_cast<uchar>((color>>8)&0xFF), static_cast<uchar>(color&0xFF), 0}, x, y+h);
        if(linewrap && height > FONTH) // line wrap indicator
        {
            hudnotextureshader->set();
            gle::colorub(0x80, 0xA0, 0x80);
            gle::defvertex(2);
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(x,         y+h+FONTH);
            gle::attribf(x,         y+h+height);
            gle::attribf(x-fontwidth()/2, y+h+FONTH);
            gle::attribf(x-fontwidth()/2, y+h+height);
            gle::end();
        }
        h+=height;
    }
}

// global

std::vector<Editor *> editors;
Editor *textfocus = nullptr;

void readyeditors()
{
    for(Editor * i : editors)
    {
        i->active = (i->mode==Editor_Forever);
    }
}

void flusheditors()
{
    for(int i = editors.size(); --i >=0;) //note reverse iteration
    {
        if(!editors[i]->active)
        {
            Editor *e = editors.at(i);
            editors.erase(editors.begin() + i);
            if(e == textfocus)
            {
                textfocus = nullptr;
            }
            delete e;
        }
    }
}

Editor *useeditor(std::string name, int mode, bool focus, const char *initval)
{
    for(uint i = 0; i < editors.size(); i++)
    {
        if(editors[i]->name == name)
        {
            Editor *e = editors[i];
            if(focus)
            {
                textfocus = e;
            }
            e->active = true;
            return e;
        }
    }
    if(mode < 0)
    {
        return nullptr;
    }
    Editor *e = new Editor(name, mode, initval);
    editors.push_back(e);
    if(focus)
    {
        textfocus = e;
    }
    return e;
}

void textlist()
{
    if(!textfocus)
    {
        return;
    }
    std::string s;
    for(uint i = 0; i < editors.size(); i++)
    {
        if(i > 0)
        {
            s.push_back(',');
            s.push_back(' ');
        }
        s.append(editors[i]->name);
    }
    result(s.c_str());
}

void textfocuscmd(const char *name, const int *mode)
{
    if(identflags&Idf_Overridden)
    {
        return;
    }
    if(*name)
    {
        useeditor(name, *mode<=0 ? Editor_Forever : *mode, true);
    }
    else if(editors.size() > 0)
    {
        result(editors.back()->name.c_str());
    }
}

void textsave(const char *file)
{
    if(!textfocus)
    {
        return;
    }
    if(*file)
    {
        textfocus->setfile(copypath(file));
    }
    textfocus->save();
}


void textload(const char *file)
{
    if(!textfocus)
    {
        return;
    }
    if(*file)
    {
        textfocus->setfile(copypath(file));
        textfocus->load();
    }
    else if(textfocus->filename)
    {
        result(textfocus->filename);
    }
}


void textinit(std::string name, char *file, char *initval)
{
    if(identflags&Idf_Overridden)
    {
        return;
    }
    Editor *e = nullptr;
    for(Editor *i : editors)
    {
        if(i->name == name)
        {
            e = i;
            break;
        }
    }
    if(e && e->rendered && !e->filename && *file && (e->lines.empty() || (e->lines.size() == 1 && !std::strcmp(e->lines[0].text, initval))))
    {
        e->setfile(copypath(file));
        e->load();
    }
}

const std::string pastebuffer = "#pastebuffer";

void inittextcmds()
{
    addcommand("textinit", reinterpret_cast<identfun>(textinit), "sss", Id_Command); // loads into named editor if no file assigned and editor has been rendered
    addcommand("textlist", reinterpret_cast<identfun>(textlist), "", Id_Command);
    addcommand("textshow", reinterpret_cast<identfun>(+[] () { if(!textfocus || identflags&Idf_Overridden) return; /* @DEBUG return the start of the buffer*/ EditLine line; line.combinelines(textfocus->lines); result(line.text); line.clear();; }), "", Id_Command);
    addcommand("textfocus", reinterpret_cast<identfun>(textfocuscmd), "si", Id_Command);
    addcommand("textprev", reinterpret_cast<identfun>(+[] () { if(!textfocus || identflags&Idf_Overridden) return; editors.insert(editors.begin(), textfocus); editors.pop_back();; }), "", Id_Command);; // return to the previous editor
    addcommand("textmode", reinterpret_cast<identfun>(+[] (int *m) { if(!textfocus || identflags&Idf_Overridden) return; /* (1= keep while focused, 2= keep while used in gui, 3= keep forever (i.e. until mode changes)) topmost editor, return current setting if no args*/ if(*m) { textfocus->mode = *m; } else { intret(textfocus->mode); }; }), "i", Id_Command);
    addcommand("textsave", reinterpret_cast<identfun>(textsave), "s", Id_Command);
    addcommand("textload", reinterpret_cast<identfun>(textload), "s", Id_Command);
    addcommand("textcopy", reinterpret_cast<identfun>(+[] () { if(!textfocus || identflags&Idf_Overridden) return; Editor *b = useeditor(pastebuffer, Editor_Forever, false); textfocus->copyselectionto(b);; }), "", Id_Command);;
    addcommand("textpaste", reinterpret_cast<identfun>(+[] () { if(!textfocus || identflags&Idf_Overridden) return; Editor *b = useeditor(pastebuffer, Editor_Forever, false); textfocus->insertallfrom(b);; }), "", Id_Command);;
    addcommand("textmark", reinterpret_cast<identfun>(+[] (int *m) { if(!textfocus || identflags&Idf_Overridden) return; /* (1=mark, 2=unmark), return current mark setting if no args*/ if(*m) { textfocus->mark(*m==1); } else { intret(textfocus->region() ? 1 : 2); }; }), "i", Id_Command);;
    addcommand("textselectall", reinterpret_cast<identfun>(+[] () { if(!textfocus || identflags&Idf_Overridden) return; textfocus->selectall();; }), "", Id_Command);;
    addcommand("textclear", reinterpret_cast<identfun>(+[] () { if(!textfocus || identflags&Idf_Overridden) return; textfocus->clear();; }), "", Id_Command);;
    addcommand("textcurrentline", reinterpret_cast<identfun>(+[] () { if(!textfocus || identflags&Idf_Overridden) return; result(textfocus->currentline().text);; }), "", Id_Command);;
    addcommand("textexec", reinterpret_cast<identfun>(+[] (int *selected) { if(!textfocus || identflags&Idf_Overridden) return; /* execute script commands from the buffer (0=all, 1=selected region only)*/ char *script = *selected ? textfocus->selectiontostring() : textfocus->tostring(); execute(script); delete[] script;; }), "i", Id_Command);
}

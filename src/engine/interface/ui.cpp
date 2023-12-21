/* ui.cpp: cubescript user interfaces and menu functionality
 *
 * ui.cpp defines a series of convenient objects that can be created and destroyed
 * as groups to be used as interfaces by programs. They can be configured to grab
 * user input or not, (the former being useful for interactive menus, the latter
 * for passive interface material such as a HUD).
 *
 * ui.cpp uses font functionality in rendertext as well as textedit for interactive
 * text modification objects
 */

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include <memory>
#include <optional>

#include "console.h"
#include "control.h"
#include "input.h"
#include "ui.h"
#include "cs.h"

#include "world/entities.h"
#include "world/octaedit.h"
#include "world/bih.h"

#include "render/hud.h"

//model needs bih's objects
#include "model/model.h"
//textedit.h needs rendertext's objects
#include "render/rendergl.h"
#include "render/renderlights.h"
#include "render/rendermodel.h"
#include "render/rendertext.h"
#include "render/renderttf.h"
#include "render/shader.h"
#include "render/shaderparam.h"
#include "render/texture.h"

#include "textedit.h"

#include "render/renderwindow.h"

/* a quick note on unnamed function arguments, used here for many derived functions:
 *
 * c++ does legally allow functions to be defined with parameters with no name:
 * this is to allow the derived functions to match the same function "signature"
 * as the parent class without needlessly defining parameter names that don't
 * actually get used -- this can be confusing (one expects to see parameters in
 * a function actually get used)
 *
 * obviously, anything actually passed to them will be lost as there is no name
 * with which to access them inside the function body
 *
 * example:
 *
 * bool target(float, float) //note unnamed function parameters
 * {
 *      return true; //note that neither parameter was used in the body
 * }
 */

static ModelPreview modelpreview = ModelPreview();

namespace UI
{
    float cursorx = 0.499f,
          cursory = 0.499f;

    static void quads(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1)
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x+w, y);   gle::attribf(tx+tw, ty);
        gle::attribf(x,   y);   gle::attribf(tx,    ty);
        gle::attribf(x+w, y+h); gle::attribf(tx+tw, ty+th);
        gle::attribf(x,   y+h); gle::attribf(tx,    ty+th);
        gle::end();
    }

    static void quad(float x, float y, float w, float h, const vec2 tc[4])
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x+w, y);   gle::attrib(tc[1]);
        gle::attribf(x,   y);   gle::attrib(tc[0]);
        gle::attribf(x+w, y+h); gle::attrib(tc[2]);
        gle::attribf(x,   y+h); gle::attrib(tc[3]);
        gle::end();
    }

    class ClipArea
    {
        public:
            ClipArea(float x, float y, float w, float h) : x1(x), y1(y), x2(x+w), y2(y+h) {}

            void intersect(const ClipArea &c)
            {
                x1 = std::max(x1, c.x1);
                y1 = std::max(y1, c.y1);
                x2 = std::max(x1, std::min(x2, c.x2));
                y2 = std::max(y1, std::min(y2, c.y2));

            }

            bool isfullyclipped(float x, float y, float w, float h) const
            {
                return x1 == x2 || y1 == y2 || x >= x2 || y >= y2 || x+w <= x1 || y+h <= y1;
            }

            void scissor();
        private:
            float x1, y1, x2, y2;
    };

    namespace
    {
        std::vector<ClipArea> clipstack;

        void pushclip(float x, float y, float w, float h)
        {
            if(clipstack.empty())
            {
                glEnable(GL_SCISSOR_TEST);
            }
            ClipArea &c = clipstack.emplace_back(ClipArea(x, y, w, h));
            if(clipstack.size() >= 2)
            {
                c.intersect(clipstack[clipstack.size()-2]);
            }
            c.scissor();
        }

        void popclip()
        {
            clipstack.pop_back();
            if(clipstack.empty())
            {
                glDisable(GL_SCISSOR_TEST);
            }
            else
            {
                clipstack.back().scissor();
            }
        }

        bool isfullyclipped(float x, float y, float w, float h)
        {
            if(clipstack.empty())
            {
                return false;
            }
            return clipstack.back().isfullyclipped(x, y, w, h);
        }

        enum Alignment
        {
            Align_Mask = 0xF,

            Align_HMask   = 0x3,
            Align_HShift  = 0,
            Align_HNone   = 0,
            Align_Left    = 1,
            Align_HCenter = 2,
            Align_Right   = 3,

            Align_VMask   = 0xC,
            Align_VShift  = 2,
            Align_VNone   = 0 << 2,
            Align_Top     = 1 << 2,
            Align_VCenter = 2 << 2,
            Align_Bottom  = 3 << 2,
        };

        enum ClampDirection
        {
            Clamp_Mask    = 0xF0,
            Clamp_Left    = 0x10,
            Clamp_Right   = 0x20,
            Clamp_Top     = 0x40,
            Clamp_Bottom  = 0x80,

            NO_ADJUST     = Align_HNone | Align_VNone,
        };

        enum ElementState
        {
            State_Hover       = 1 << 0,
            State_Press       = 1 << 1,
            State_Hold        = 1 << 2,
            State_Release     = 1 << 3,
            State_AltPress    = 1 << 4,
            State_AltHold     = 1 << 5,
            State_AltRelease  = 1 << 6,
            State_EscPress    = 1 << 7,
            State_EscHold     = 1 << 8,
            State_EscRelease  = 1 << 9,
            State_ScrollUp    = 1 << 10,
            State_ScrollDown  = 1 << 11,
            State_Hidden      = 1 << 12,

            State_HoldMask = State_Hold | State_AltHold | State_EscHold
        };

        enum ElementBlend
        {
            Blend_Alpha,
            Blend_Mod
        };

        enum ChangeDrawFlags
        {
            Change_Shader = 1 << 0,
            Change_Color  = 1 << 1,
            Change_Blend  = 1 << 2
        };
    }
    class Object;

    static Object *buildparent = nullptr;
    static int buildchild = -1;

    //type: the type of object to build
    //o the name of the temp variable to use
    //setup: a snippet of c++ to run to set up the object
    //contents: the content passed to the buildchildren() call
    #define BUILD(type, o, setup, contents) do { \
        if(buildparent) \
        { \
            type *o = buildparent->buildtype<type>(); \
            setup; \
            o->buildchildren(contents); \
        } \
    } while(0)

    static int changed = 0;

    static Object *drawing = nullptr;

    static int blendtype = Blend_Alpha;

    static void changeblend(int type, GLenum src, GLenum dst)
    {
        if(blendtype != type)
        {
            blendtype = type;
            glBlendFunc(src, dst);
        }
    }

    void resetblend()
    {
        changeblend(Blend_Alpha, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void modblend()
    {
        changeblend(Blend_Mod, GL_ZERO, GL_SRC_COLOR);
    }

    class Object
    {
        public:
            float x, y, w, h;
            std::vector<Object *> children;
            uchar adjust;
            ushort state, childstate;
            Object *parent;

            //note reverse iteration
            #define LOOP_CHILDREN_REV(o, body) do { \
                for(int i = static_cast<int>(children.size()); --i >=0;) \
                { \
                    Object *o = children.at(i); \
                    body; \
                } \
            } while(0)

            #define LOOP_CHILD_RANGE(start, end, o, body) do { \
                for(int i = start; i < end; i++) \
                { \
                    Object *o = children.at(i); \
                    body; \
                } \
            } while(0)

            Object() :  x(), y(), w(), h(), adjust(0), state(0), childstate(0), parent() {}
            virtual ~Object()
            {
                clearchildren();
            }
            template<class T>
            T *buildtype()
            {
                T *t;
                if(static_cast<int>(children.size()) > buildchild )
                {
                    Object *o = children[buildchild];
                    if(o->istype<T>())
                    {
                        t = static_cast<T *>(o);
                    }
                    else
                    {
                        delete o;
                        t = new T;
                        children[buildchild] = t;
                    }
                }
                else
                {
                    t = new T;
                    children.push_back(t);
                }
                t->reset(this);
                buildchild++;
                return t;
            }

            virtual void layout()
            {
                w = h = 0;
                for(Object *o : children)
                {
                    o->x = o->y = 0;
                    o->layout();
                    w = std::max(w, o->x + o->w);
                    h = std::max(h, o->y + o->h);
                }
            }

            void buildchildren(uint *contents)
            {
                if((*contents&Code_OpMask) == Code_Exit)
                {
                    children.erase(children.begin(), children.end());
                }
                else
                {
                    Object *oldparent = buildparent;
                    int oldchild = buildchild;
                    buildparent = this;
                    buildchild = 0;
                    executeret(contents);
                    while(static_cast<int>(children.size()) > buildchild)
                    {
                        children.pop_back();
                    }
                    buildparent = oldparent;
                    buildchild = oldchild;
                }
                resetstate();
            }

            void clearstate(int flags)
            {
                state &= ~flags;
                if(childstate & flags)
                {
                    for(Object *o : children)
                    {
                        if((o->state | o->childstate) & flags) o->clearstate(flags);
                    }
                    childstate &= ~flags;
                }
            }

            void enddraw(int change)
            {
                enddraw();
                changed &= ~change;
                if(changed)
                {
                    if(changed & Change_Shader)
                    {
                        hudshader->set();
                    }
                    if(changed & Change_Color)
                    {
                        gle::colorf(1, 1, 1);
                    }
                    if(changed & Change_Blend)
                    {
                        resetblend();
                    }
                }
            }

            void resetchildstate()
            {
                resetstate();
                for(Object *o : children)
                {
                     o->resetchildstate();
                }
            }

            bool hasstate(int flags) const
            {
                return ((state & ~childstate) & flags) != 0;
            }

            bool haschildstate(int flags) const
            {
                return ((state | childstate) & flags) != 0;
            }

            virtual void draw(float sx, float sy)
            {
                for(Object *o : children)
                {
                    if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                    {
                        o->draw(sx + o->x, sy + o->y);
                    }
                }
            }

            /* DOSTATES: executes the DOSTATE macro for the applicable special keys
             *
             * ***NOTE***: DOSTATE is not defined by default, and is defined manually
             * in the code at every location the DOSTATES macro is called --
             * you cannot merely call DOSTATES whenever you wish;
             * you must also define DOSTATE (and then undefine it)
             */
            #define DOSTATES \
                DOSTATE(State_Hover, hover) \
                DOSTATE(State_Press, press) \
                DOSTATE(State_Hold, hold) \
                DOSTATE(State_Release, release) \
                DOSTATE(State_AltHold, althold) \
                DOSTATE(State_AltPress, altpress) \
                DOSTATE(State_AltRelease, altrelease) \
                DOSTATE(State_EscHold, eschold) \
                DOSTATE(State_EscPress, escpress) \
                DOSTATE(State_EscRelease, escrelease) \
                DOSTATE(State_ScrollUp, scrollup) \
                DOSTATE(State_ScrollDown, scrolldown)

            bool setstate(int state, float cx, float cy, int mask = 0, bool inside = true, int setflags = 0)
            {
                switch(state)
                {
                #define DOSTATE(flags, func) case flags: func##children(cx, cy, mask, inside, setflags | flags); return haschildstate(flags);
                DOSTATES
                #undef DOSTATE
                }
                return false;
            }

            void setup()
            {
            }

            template<class T>
            bool istype() const
            {
                return T::typestr() == gettype();
            }

            virtual bool rawkey(int code, bool isdown)
            {
                LOOP_CHILDREN_REV(o,
                {
                    if(o->rawkey(code, isdown))
                    {
                        return true;
                    }
                });
                return false;
            }

            virtual bool key(int code, bool isdown)
            {
                LOOP_CHILDREN_REV(o,
                {
                    if(o->key(code, isdown))
                    {
                        return true;
                    }
                });
                return false;
            }

            virtual bool textinput(const char *str, int len)
            {
                LOOP_CHILDREN_REV(o,
                {
                    if(o->textinput(str, len))
                    {
                        return true;
                    }
                });
                return false;
            }

            virtual int childcolumns() const
            {
                return static_cast<int>(children.size());
            }

            void adjustlayout(float px, float py, float pw, float ph)
            {
                switch(adjust & Align_HMask)
                {
                    case Align_Left:
                    {
                        x = px;
                        break;
                    }
                    case Align_HCenter:
                    {
                        x = px + (pw - w) / 2;
                        break;
                    }
                    case Align_Right:
                    {
                        x = px + pw - w;
                        break;
                    }
                }

                switch(adjust & Align_VMask)
                {
                    case Align_Top:
                    {
                        y = py;
                        break;
                    }
                    case Align_VCenter:
                    {
                        y = py + (ph - h) / 2;
                        break;
                    }
                    case Align_Bottom:
                    {
                        y = py + ph - h;
                        break;
                    }
                }

                if(adjust & Clamp_Mask)
                {
                    if(adjust & Clamp_Left)
                    {
                        w += x - px;
                        x = px;
                    }
                    if(adjust & Clamp_Right)
                    {
                        w = px + pw - x;
                    }
                    if(adjust & Clamp_Top)
                    {
                        h += y - py;
                        y = py;
                    }
                    if(adjust & Clamp_Bottom)
                    {
                        h = py + ph - y;
                    }
                }

                adjustchildren();
            }

            void setalign(int xalign, int yalign)
            {
                adjust &= ~Align_Mask;
                adjust |= (std::clamp(xalign, -2, 1)+2) << Align_HShift;
                adjust |= (std::clamp(yalign, -2, 1)+2) << Align_VShift;
            }

            void setclamp(int left, int right, int top, int bottom)
            {
                adjust &= ~Clamp_Mask;
                if(left)
                {
                    adjust |= Clamp_Left;
                }
                if(right)
                {
                    adjust |= Clamp_Right;
                }
                if(top)
                {
                    adjust |= Clamp_Top;
                }
                if(bottom)
                {
                    adjust |= Clamp_Bottom;
                }
            }
        protected:

            void reset()
            {
                resetlayout();
                parent = nullptr;
                adjust = Align_HCenter | Align_VCenter;
            }

            virtual uchar childalign() const
            {
                return Align_HCenter | Align_VCenter;
            }

            void reset(Object *parent_)
            {
                resetlayout();
                parent = parent_;
                adjust = parent->childalign();
            }

            void clearchildren()
            {
                children.erase(children.begin(), children.end());
            }

            void adjustchildrento(float px, float py, float pw, float ph)
            {
                for(Object *o : children)
                {
                    o->adjustlayout(px, py, pw, ph);
                }
            }

            virtual void adjustchildren()
            {
                adjustchildrento(0, 0, w, h);
            }

            virtual bool target(float, float) //note unnamed function parameters
            {
                return false;
            }

            virtual void startdraw() {}
            virtual void enddraw() {}

            void changedraw(int change = 0)
            {
                if(!drawing)
                {
                    startdraw();
                    changed = change;
                }
                else if(drawing->gettype() != gettype())
                {
                    drawing->enddraw(change);
                    startdraw();
                    changed = change;
                }
                drawing = this;
            }

            void resetstate()
            {
                state &= State_HoldMask;
                childstate &= State_HoldMask;
            }

            void changechildstate(Object * o, void (Object::*member)(float, float, int, bool, int), float ox, float oy, int mask, bool inside, int setflags)
            {
                (o->*member)(ox, oy, mask, inside, setflags); /*child's->func##children fxn called*/
                childstate |= (o->state | o->childstate) & (setflags); /*set childstate*/
            }

            void propagatestate(float cx, float cy, int mask, bool inside, int setflags, void (UI::Object::*method)(float, float, int, bool, int))
            {
                for(int i = static_cast<int>(children.size()); --i >= 0;)
                {
                    Object *o = children.at(i);
                    if(((o->state | o->childstate) & mask) != mask)
                    {
                        continue;
                    }
                    float ox = cx - o->x; /*offset x*/
                    float oy = cy - o->y; /*offset y*/
                    if(!inside)
                    {
                        ox = std::clamp(ox, 0.0f, o->w); /*clamp offsets to Object bounds in x*/
                        oy = std::clamp(oy, 0.0f, o->h); /*clamp offsets to Object bounds in y*/
                        changechildstate(o, method, ox, oy, mask, inside, setflags);
                    }
                    else if(ox >= 0 && ox < o->w && oy >= 0 && oy < o->h) /*if in bounds execute body*/
                    {
                        changechildstate(o, method, ox, oy, mask, inside, setflags);
                    }
                }
            }

            #define DOSTATE(flags, func) \
                virtual void func##children(float cx, float cy, int mask, bool inside, int setflags) \
                { \
                    propagatestate(cx, cy, mask, inside, setflags, &UI::Object::func##children); \
                    if(target(cx, cy)) \
                    { \
                        state |= (setflags); \
                    } \
                    func(cx, cy); \
                } \
                virtual void func(float, float) {} /*note unnamed function parameters*/
            DOSTATES
            #undef DOSTATE

            virtual const char *gettype() const
            {
                return typestr();
            }

            virtual const char *gettypename() const
            {
                return gettype();
            }

            Object *find(const char *name, bool recurse = true, const Object *exclude = nullptr) const
            {
                for(Object *o : children)
                {
                    if(o != exclude && o->isnamed(name))
                    {
                        return o;
                    }
                }
                if(recurse)
                {
                    for(Object *o : children)
                    {
                        if(o != exclude)
                        {
                            Object *found = o->find(name);
                            if(found)
                            {
                                return found;
                            }
                        }
                    }
                }
                return nullptr;
            }

            Object *findsibling(const char *name) const
            {
                for(const Object *prev = this, *cur = parent; cur; prev = cur, cur = cur->parent)
                {
                    Object *o = cur->find(name, true, prev);
                    if(o)
                    {
                        return o;
                    }
                }
                return nullptr;
            }

        private:

            virtual const char *getname() const
            {
                return gettype();
            }

            void resetlayout()
            {
                x = y = w = h = 0;
            }

            static const char *typestr()
            {
                return "#Object";
            }

            bool isnamed(const char *name) const
            {
                return name[0] == '#' ? name == gettypename() : !std::strcmp(name, getname());
            }

    };

    static void stopdrawing()
    {
        if(drawing)
        {
            drawing->enddraw(0);
            drawing = nullptr;
        }
    }

    struct Window;

    static Window *window = nullptr;

    struct Window final : Object
    {
        char *name;
        uint *contents, *onshow, *onhide;
        bool allowinput, eschide, abovehud;
        float px, py, pw, ph;
        vec2 sscale, soffset;

        Window(const char *name, const char *contents, const char *onshow, const char *onhide) :
            name(newstring(name)),
            contents(compilecode(contents)),
            onshow(onshow && onshow[0] ? compilecode(onshow) : nullptr),
            onhide(onhide && onhide[0] ? compilecode(onhide) : nullptr),
            allowinput(true), eschide(true), abovehud(false),
            px(0), py(0), pw(0), ph(0),
            sscale(1, 1), soffset(0, 0)
        {
        }
        ~Window()
        {
            delete[] name;
            freecode(contents);
            freecode(onshow);
            freecode(onhide);
        }

        static const char *typestr()
        {
            return "#Window";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        const char *getname() const override final
        {
            return name;
        }

        void build();

        void hide()
        {
            if(onhide)
            {
                execute(onhide);
            }
        }

        void show()
        {
            state |= State_Hidden;
            clearstate(State_HoldMask);
            if(onshow)
            {
                execute(onshow);
            }
        }

        void setup()
        {
            Object::setup();
            allowinput = eschide = true;
            abovehud = false;
            px = py = pw = ph = 0;
        }

        void layout() override final
        {
            if(state & State_Hidden)
            {
                w = h = 0;
                return;
            }
            window = this;
            Object::layout();
            window = nullptr;
        }

        void draw(float sx, float sy) override final
        {
            if(state & State_Hidden)
            {
                return;
            }
            window = this;

            projection();
            hudshader->set();

            glEnable(GL_BLEND);
            blendtype = Blend_Alpha;
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gle::colorf(1, 1, 1);

            changed = 0;
            drawing = nullptr;

            Object::draw(sx, sy);

            stopdrawing();

            glDisable(GL_BLEND);

            window = nullptr;
        }

        void draw()
        {
            draw(x, y);
        }

        void adjustchildren() override final
        {
            if(state & State_Hidden)
            {
                return;
            }
            window = this;
            Object::adjustchildren();
            window = nullptr;
        }

        void adjustlayout()
        {
            float aspect = static_cast<float>(hudw())/hudh();
            ph = std::max(std::max(h, w/aspect), 1.0f);
            pw = aspect*ph;
            Object::adjustlayout(0, 0, pw, ph);
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) override final \
            { \
                if(!allowinput || state&State_Hidden || pw <= 0 || ph <= 0) \
                { \
                    return; \
                } \
                cx = cx*pw + px-x; \
                cy = cy*ph + py-y; \
                if(!inside || (cx >= 0 && cy >= 0 && cx < w && cy < h)) \
                { \
                    Object::func##children(cx, cy, mask, inside, setflags); \
                } \
            }
        DOSTATES
        #undef DOSTATE

        void escrelease(float cx, float cy) override final;

        void projection()
        {
            hudmatrix.ortho(px, px + pw, py + ph, py, -1, 1);
            resethudmatrix();
            sscale = vec2(hudmatrix.a.x, hudmatrix.b.y).mul(0.5f);
            soffset = vec2(hudmatrix.d.x, hudmatrix.d.y).mul(0.5f).add(0.5f);
        }

        void calcscissor(float x1, float y1, float x2, float y2, int &sx1, int &sy1, int &sx2, int &sy2, bool clip = true) const
        {
            vec2 s1 = vec2(x1, y2).mul(sscale).add(soffset),
                 s2 = vec2(x2, y1).mul(sscale).add(soffset);
            sx1 = static_cast<int>(std::floor(s1.x*hudw() + 0.5f));
            sy1 = static_cast<int>(std::floor(s1.y*hudh() + 0.5f));
            sx2 = static_cast<int>(std::floor(s2.x*hudw() + 0.5f));
            sy2 = static_cast<int>(std::floor(s2.y*hudh() + 0.5f));
            if(clip)
            {
                sx1 = std::clamp(sx1, 0, hudw());
                sy1 = std::clamp(sy1, 0, hudh());
                sx2 = std::clamp(sx2, 0, hudw());
                sy2 = std::clamp(sy2, 0, hudh());
            }
        }

        float calcabovehud() const
        {
            return 1 - (y*sscale.y + soffset.y);
        }
    };

    static std::unordered_map<std::string, Window *> windows;

    void ClipArea::scissor()
    {
        int sx1, sy1, sx2, sy2;
        window->calcscissor(x1, y1, x2, y2, sx1, sy1, sx2, sy2);
        glScissor(sx1, sy1, sx2-sx1, sy2-sy1);
    }

    struct World final : Object
    {
        static const char *typestr() { return "#World"; }
        const char *gettype() const override final
        {
            return typestr();
        }

        #define LOOP_WINDOWS(o, body) do { \
            for(uint i = 0; i < children.size(); i++) \
            { \
                Window *o = static_cast<Window *>(children[i]); \
                body; \
            } \
        } while(0)
        //note reverse iteration
        #define LOOP_WINDOWS_REV(o, body) do { \
            for(int i = static_cast<int>(children.size()); --i >=0;) \
            { \
                Window *o = static_cast<Window *>(children[i]); \
                body; \
            } \
        } while(0)

        void adjustchildren() override final
        {
            LOOP_WINDOWS(w, w->adjustlayout());
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) override final \
            { \
                LOOP_WINDOWS_REV(w, \
                { \
                    if(((w->state | w->childstate) & mask) != mask) \
                    { \
                        continue; \
                    } \
                    w->func##children(cx, cy, mask, inside, setflags); \
                    int wflags = (w->state | w->childstate) & (setflags); \
                    if(wflags) \
                    { \
                        childstate |= wflags; \
                        break; \
                    } \
                }); \
            }
        DOSTATES
        #undef DOSTATE

        void build()
        {
            reset();
            setup();
            LOOP_WINDOWS(w,
            {
                w->build();
                if(children.size() <= i )
                {
                    break;
                }
                if(children.at(i) != w)
                {
                    i--;
                }
            });
            resetstate();
        }

        bool show(Window *w)
        {
            //if w is not found anywhere
            if(std::find(children.begin(), children.end(), w) != children.end())
            {
                return false;
            }
            w->resetchildstate();
            children.push_back(w);
            w->show();
            return true;
        }

        void hide(Window *w, int index)
        {
            children.erase(children.begin() + index);
            childstate = 0;
            for(Object *o : children)
            {
                childstate |= o->state | o->childstate;
            }
            w->hide();
        }

        bool hide(Window *w)
        {
            if(std::find(children.begin(), children.end(), w) != children.end())
            {
                hide(w, std::distance(children.begin(), std::find(children.begin(), children.end(), w)));
                return true;
            }
            else
            {
                return false;
            }
        }

        bool hidetop()
        {
            LOOP_WINDOWS_REV(w,
            {
                if(w->allowinput && !(w->state & State_Hidden))
                {
                    hide(w, i);
                    return true;
                }
            });
            return false;
        }

        int hideall()
        {
            int hidden = 0;
            LOOP_WINDOWS_REV(w,
            {
                hide(w, i);
                hidden++;
            });
            return hidden;
        }

        bool allowinput() const
        {
            LOOP_WINDOWS(w,
            {
                if(w->allowinput && !(w->state & State_Hidden))
                {
                    return true;
                }
            });
            return false;
        }

        void draw(float, float) override final //note unnamed function parameters
        {
        }

        void draw()
        {
            if(children.empty())
            {
                return;
            }
            LOOP_WINDOWS(w, w->draw());
        }

        float abovehud()
        {
            float y = 1;
            LOOP_WINDOWS(w,
            {
                if(w->abovehud && !(w->state & State_Hidden))
                {
                    y = std::min(y, w->calcabovehud());
                }
            });
            return y;
        }
    };

#undef LOOP_WINDOWS_REV
#undef LOOP_WINDOWS

    static World *world = nullptr;

    void Window::escrelease(float, float) //note unnamed function parameters
    {
        if(eschide)
        {
            world->hide(this);
        }
    }

    void Window::build()
    {
        reset(world);
        setup();
        window = this;
        buildchildren(contents);
        window = nullptr;
    }

    struct HorizontalList final : Object
    {
        float space, subw;

        HorizontalList () : space(), subw() {}

        static const char *typestr()
        {
            return "#HorizontalList";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void setup(float space_ = 0)
        {
            Object::setup();
            space = space_;
        }

        uchar childalign() const override final
        {
            return Align_VCenter;
        }

        void layout() override final
        {
            subw = h = 0;
            for(Object *o : children)
            {
                o->x = subw;
                o->y = 0;
                o->layout();
                subw += o->w;
                h = std::max(h, o->y + o->h);
            }
            w = subw + space*std::max(static_cast<int>(children.size()) - 1, 0);
        }

        void adjustchildren() override final
        {
            if(children.empty())
            {
                return;
            }
            float offset = 0,
                  sx = 0,
                  cspace = (w - subw) / std::max(static_cast<int>(children.size()) - 1, 1),
                  cstep = (w - subw) / children.size();
            for(int i = 0; i < static_cast<int>(children.size()); i++)
            {
                Object *o = children[i];
                o->x = offset;
                offset += o->w + cspace;
                float sw = o->w + cstep;
                o->adjustlayout(sx, 0, sw, h);
                sx += sw;
            }
        }
    };

    struct VerticalList final : Object
    {
        float space, subh;

        VerticalList() : space(), subh() {}

        static const char *typestr()
        {
            return "#VerticalList";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void setup(float space_ = 0)
        {
            Object::setup();
            space = space_;
        }

        uchar childalign() const override final
        {
            return Align_HCenter;
        }

        void layout() override final
        {
            w = subh = 0;
            for(Object *o : children)
            {
                o->x = 0;
                o->y = subh;
                o->layout();
                subh += o->h;
                w = std::max(w, o->x + o->w);
            }
            h = subh + space*std::max(static_cast<int>(children.size()) - 1, 0);
        }

        void adjustchildren() override final
        {
            if(children.empty())
            {
                return;
            }

            float offset = 0,
                  sy     = 0,
                  rspace = (h - subh) / std::max(static_cast<int>(children.size()) - 1, 1),
                  rstep = (h - subh) / children.size();
            for(Object *o : children)
            {
                o->y = offset;
                offset += o->h + rspace;
                float sh = o->h + rstep;
                o->adjustlayout(0, sy, w, sh);
                sy += sh;
            }
        }
    };

    struct Grid final : Object
    {
        int columns;
        float spacew, spaceh, subw, subh;
        std::vector<float> widths, heights;

        Grid() : columns(), spacew(), spaceh(), subw(), subh() {}

        static const char *typestr()
        {
            return "#Grid";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void setup(int columns_, float spacew_ = 0, float spaceh_ = 0)
        {
            Object::setup();
            columns = columns_;
            spacew = spacew_;
            spaceh = spaceh_;
        }

        uchar childalign() const override final
        {
            return 0;
        }

        void layout() override final
        {
            widths.clear();
            heights.clear();

            int column = 0,
                row = 0;
            for(Object *o : children)
            {
                o->layout();
                if(column >= static_cast<int>(widths.size()))
                {
                    widths.push_back(o->w);
                }
                else if(o->w > widths[column])
                {
                    widths[column] = o->w;
                }
                if(row >= static_cast<int>(heights.size()))
                {
                    heights.push_back(o->h);
                }
                else if(o->h > heights[row])
                {
                    heights[row] = o->h;
                }
                column = (column + 1) % columns;
                if(!column)
                {
                    row++;
                }
            }

            subw = subh = 0;
            for(const float &i : widths)
            {
                subw += i;
            }
            for(const float &i : heights)
            {
                subh += i;
            }
            w = subw + spacew*std::max(static_cast<int>(widths.size()) - 1, 0);
            h = subh + spaceh*std::max(static_cast<int>(heights.size()) - 1, 0);
        }

        void adjustchildren() override final
        {
            if(children.empty())
            {
                return;
            }
            int row = 0,
                column = 0;
            float offsety = 0,
                  sy = 0,
                  offsetx = 0,
                  sx = 0,
                  cspace = (w - subw) / std::max(static_cast<int>(widths.size()) - 1, 1),
                  cstep = (w - subw) / widths.size(),
                  rspace = (h - subh) / std::max(static_cast<int>(heights.size()) - 1, 1),
                  rstep = (h - subh) / heights.size();
            for(Object *o : children)
            {
                o->x = offsetx;
                o->y = offsety;
                o->adjustlayout(sx, sy, widths[column] + cstep, heights[row] + rstep);
                offsetx += widths[column] + cspace;
                sx += widths[column] + cstep;
                column = (column + 1) % columns;
                if(!column)
                {
                    offsetx = sx = 0;
                    offsety += heights[row] + rspace;
                    sy += heights[row] + rstep;
                    row++;
                }
            }
        }
    };

    struct TableHeader : Object
    {
        int columns;

        TableHeader() : columns(-1) {}

        static const char *typestr()
        {
            return "#TableHeader";
        }
        const char *gettype() const override
        {
            return typestr();
        }

        uchar childalign() const override final
        {
            return columns < 0 ? Align_VCenter : Align_HCenter | Align_VCenter;
        }

        int childcolumns() const override final
        {
            return columns;
        }

        void buildchildren(const uint *columndata, const uint *contents)
        {
            Object *oldparent = buildparent;
            int oldchild = buildchild;
            buildparent = this;
            buildchild = 0;
            executeret(columndata);
            if(columns != buildchild)
            {
                while(static_cast<int>(children.size()) > buildchild)
                {
                    children.pop_back();
                }
            }
            columns = buildchild;
            if((*contents&Code_OpMask) != Code_Exit)
            {
                executeret(contents);
            }
            while(static_cast<int>(children.size()) > buildchild)
            {
                children.pop_back();
            }
            buildparent = oldparent;
            buildchild = oldchild;
            resetstate();
        }

        void adjustchildren() override final
        {
            LOOP_CHILD_RANGE(columns, static_cast<int>(children.size()), o, o->adjustlayout(0, 0, w, h));
        }

        void draw(float sx, float sy) override final
        {
            LOOP_CHILD_RANGE(columns, static_cast<int>(children.size()), o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                {
                    o->draw(sx + o->x, sy + o->y);
                }
            });
            LOOP_CHILD_RANGE(0, columns, o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                {
                    o->draw(sx + o->x, sy + o->y);
                }
            });
        }
    };

    struct TableRow final: TableHeader
    {
        static const char *typestr() { return "#TableRow"; }
        const char *gettype() const override final
        {
            return typestr();
        }

        bool target(float, float) override final //note unnamed function parameters
        {
            return true;
        }
    };

    #define BUILDCOLUMNS(type, o, setup, columndata, contents) do { \
        if(buildparent) \
        { \
            type *o = buildparent->buildtype<type>(); \
            setup; \
            o->buildchildren(columndata, contents); \
        } \
    } while(0)

    struct Table final : Object
    {
        float spacew, spaceh, subw, subh;
        std::vector<float> widths;
        static const char *typestr()
        {
            return "#Table";
        }
        const char *gettype() const override final
        {
            return typestr();
        }

        void setup(float spacew_ = 0, float spaceh_ = 0)
        {
            Object::setup();
            spacew = spacew_;
            spaceh = spaceh_;
        }

        uchar childalign() const override final
        {
            return 0;
        }

        void layout() override final
        {
            widths.clear();

            w = subh = 0;
            for(Object *o : children)
            {
                o->layout();
                int cols = o->childcolumns();
                while(static_cast<int>(widths.size()) < cols)
                {
                    widths.push_back(0);
                }
                for(int j = 0; j < cols; ++j)
                {
                    Object *c = o->children[j];
                    if(c->w > widths[j])
                    {
                        widths[j] = c->w;
                    }
                }
                w = std::max(w, o->w);
                subh += o->h;
            }

            subw = 0;
            for(const float &i : widths)
            {
                subw += i;
            }
            w = std::max(w, subw + spacew*std::max(static_cast<int>(widths.size()) - 1, 0));
            h = subh + spaceh*std::max(static_cast<int>(children.size()) - 1, 0);
        }

        void adjustchildren() override final
        {
            if(children.empty())
            {
                return;
            }
            float offsety = 0,
                  sy = 0,
                  cspace = (w - subw) / std::max(static_cast<int>(widths.size()) - 1, 1),
                  cstep = (w - subw) / widths.size(),
                  rspace = (h - subh) / std::max(static_cast<int>(children.size()) - 1, 1),
                  rstep = (h - subh) / children.size();
            for(Object *o : children)
            {
                o->x = 0;
                o->y = offsety;
                o->w = w;
                offsety += o->h + rspace;
                float sh = o->h + rstep;
                o->adjustlayout(0, sy, w, sh);
                sy += sh;

                float offsetx = 0;
                float sx = 0;
                int cols = o->childcolumns();
                for(int j = 0; j < cols; ++j)
                {
                    Object *c = o->children[j];
                    c->x = offsetx;
                    offsetx += widths[j] + cspace;
                    float sw = widths[j] + cstep;
                    c->adjustlayout(sx, 0, sw, o->h);
                    sx += sw;
                }
            }
        }
    };

    struct Spacer final : Object
    {
        float spacew, spaceh;

        Spacer() : spacew(), spaceh() {}

        void setup(float spacew_, float spaceh_)
        {
            Object::setup();
            spacew = spacew_;
            spaceh = spaceh_;
        }

        static const char *typestr()
        {
            return "#Spacer";
        }
        const char *gettype() const override final
        {
            return typestr();
        }
        void layout() override final
        {
            w = spacew;
            h = spaceh;
            for(Object *o : children)
            {
                o->x = spacew;
                o->y = spaceh;
                o->layout();
                w = std::max(w, o->x + o->w);
                h = std::max(h, o->y + o->h);
            }
            w += spacew;
            h += spaceh;
        }

        void adjustchildren() override final
        {
            adjustchildrento(spacew, spaceh, w - 2*spacew, h - 2*spaceh);
        }
    };

    struct Offsetter final : Object
    {
        float offsetx, offsety;

        void setup(float offsetx_, float offsety_)
        {
            Object::setup();
            offsetx = offsetx_;
            offsety = offsety_;
        }

        static const char *typestr() { return "#Offsetter"; }
        const char *gettype() const override final
        {
            return typestr();
        }

        void layout() override final
        {
            Object::layout();

            for(Object *o : children)
            {
                o->x += offsetx;
                o->y += offsety;
            }

            w += offsetx;
            h += offsety;
        }

        void adjustchildren() override final
        {
            adjustchildrento(offsetx, offsety, w - offsetx, h - offsety);
        }
    };

    struct Filler : Object
    {
        float minw, minh;

        void setup(float minw_, float minh_)
        {
            Object::setup();
            minw = minw_;
            minh = minh_;
        }

        static const char *typestr()
        {
            return "#Filler";
        }

        const char *gettype() const override
        {
            return typestr();
        }

        void layout() override final
        {
            Object::layout();
            w = std::max(w, minw);
            h = std::max(h, minh);
        }
    };

    struct Target : Filler
    {
        static const char *typestr()
        {
            return "#Target";
        }
        const char *gettype() const override
        {
            return typestr();
        }
        bool target(float, float) override final //note unnamed function parameters
        {
            return true;
        }
    };

    struct Color
    {
        uchar r, g, b, a;

        Color() {}

        //converts an int color to components
        Color(uint c) : r((c>>16)&0xFF), g((c>>8)&0xFF), b(c&0xFF), a(c>>24 ? c>>24 : 0xFF) {}

        //converts an int color w/o alpha and alpha channel to components
        Color(uint c, uchar a) : r((c>>16)&0xFF), g((c>>8)&0xFF), b(c&0xFF), a(a) {}

        //assigns components normally
        Color(uchar r, uchar g, uchar b, uchar a = 255) : r(r), g(g), b(b), a(a) {}

        void init() { gle::colorub(r, g, b, a); }
        void attrib() { gle::attribub(r, g, b, a); }

        static void def() { gle::defcolor(4, GL_UNSIGNED_BYTE); }
    };

#undef LOOP_CHILDREN_REV
#undef LOOP_CHILD_RANGE

    struct FillColor : Target
    {
        enum
        {
            SOLID = 0,
            MODULATE
        };

        int type;
        Color color;

        void setup(int type_, const Color &color_, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_);
            type = type_;
            color = color_;
        }

        static const char *typestr() { return "#FillColor"; }
        const char *gettype() const override
        {
            return typestr();
        }

        void startdraw() override
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy) override
        {
            changedraw(Change_Shader | Change_Color | Change_Blend);
            if(type==MODULATE)
            {
                modblend();
            }
            else
            {
                resetblend();
            }
            color.init();
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(sx+w, sy);
            gle::attribf(sx,   sy);
            gle::attribf(sx+w, sy+h);
            gle::attribf(sx,   sy+h);
            gle::end();

            Object::draw(sx, sy);
        }
    };

    class Gradient final : public FillColor
    {
        public:
            enum { VERTICAL, HORIZONTAL };

            int dir;

            void setup(int type_, int dir_, const Color &color_, const Color &color2_, float minw_ = 0, float minh_ = 0)
            {
                FillColor::setup(type_, color_, minw_, minh_);
                dir = dir_;
                color2 = color2_;
            }

            static const char *typestr() { return "#Gradient"; }

        protected:
            const char *gettype() const override final
            {
                return typestr();
            }

            void startdraw() override final
            {
                hudnotextureshader->set();
                gle::defvertex(2);
                Color::def();
            }

        private:
            Color color2;

            void draw(float sx, float sy) override final
            {
                changedraw(Change_Shader | Change_Color | Change_Blend);
                if(type==MODULATE)
                {
                    modblend();
                }
                else
                {
                    resetblend();
                }
                gle::begin(GL_TRIANGLE_STRIP);
                gle::attribf(sx+w, sy);   (dir == HORIZONTAL ? color2 : color).attrib();
                gle::attribf(sx,   sy);   color.attrib();
                gle::attribf(sx+w, sy+h); color2.attrib();
                gle::attribf(sx,   sy+h); (dir == HORIZONTAL ? color : color2).attrib();
                gle::end();

                Object::draw(sx, sy);
            }
    };

    struct Line final : Filler
    {
        Color color;

        void setup(const Color &color_, float minw_ = 0, float minh_ = 0)
        {
            Filler::setup(minw_, minh_);
            color = color_;
        }

        static const char *typestr() { return "#Line"; }
        const char *gettype() const override final
        {
            return typestr();
        }

        void startdraw() override final
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy) override final
        {
            changedraw(Change_Shader | Change_Color);

            color.init();
            gle::begin(GL_LINES);
            gle::attribf(sx,   sy);
            gle::attribf(sx+w, sy+h);
            gle::end();

            Object::draw(sx, sy);
        }
    };

    class Outline final : public Filler
    {
        public:

            void setup(const Color &color_, float minw_ = 0, float minh_ = 0)
            {
                Filler::setup(minw_, minh_);
                color = color_;
            }

            const char *gettype() const override final
            {
                return typestr();
            }
            static const char *typestr() { return "#Outline"; }

            void draw(float sx, float sy) override final
            {
                changedraw(Change_Shader | Change_Color);

                color.init();
                gle::begin(GL_LINE_LOOP);
                gle::attribf(sx,   sy);
                gle::attribf(sx+w, sy);
                gle::attribf(sx+w, sy+h);
                gle::attribf(sx,   sy+h);
                gle::end();

                Object::draw(sx, sy);
            }
        protected:
            void startdraw() override final
            {
                hudnotextureshader->set();
                gle::defvertex(2);
            }
        private:
            Color color;
    };

    static bool checkalphamask(Texture *tex, float x, float y)
    {
        if(!tex->alphamask)
        {
            tex->loadalphamask();
            if(!tex->alphamask)
            {
                return true;
            }
        }
        int tx = std::clamp(static_cast<int>(x*tex->xs), 0, tex->xs-1),
            ty = std::clamp(static_cast<int>(y*tex->ys), 0, tex->ys-1);
        if(tex->alphamask[ty*((tex->xs+7)/8) + tx/8] & (1<<(tx%8)))
        {
            return true;
        }
        return false;
    }

    struct Image : Filler
    {
        static Texture *lasttex;

        Texture *tex;

        void setup(Texture *tex_, float minw_ = 0, float minh_ = 0)
        {
            Filler::setup(minw_, minh_);
            tex = tex_;
        }

        static const char *typestr() { return "#Image"; }
        const char *gettype() const override
        {
            return typestr();
        }

        bool target(float cx, float cy) override
        {
            return !(tex->type&Texture::ALPHA) || checkalphamask(tex, cx/w, cy/h);
        }

        void startdraw() override final
        {
            lasttex = nullptr;

            gle::defvertex(2);
            gle::deftexcoord0();
            gle::begin(GL_TRIANGLE_STRIP);
        }

        void enddraw() override final
        {
            gle::end();
        }

        void bindtex()
        {
            changedraw();
            if(lasttex != tex)
            {
                if(lasttex)
                {
                    gle::end();
                }
                lasttex = tex;
                glBindTexture(GL_TEXTURE_2D, tex->id);
            }
        }

        void draw(float sx, float sy) override
        {
            if(tex != notexture)
            {
                bindtex();
                quads(sx, sy, w, h);
            }

            Object::draw(sx, sy);
        }
    };

    Texture *Image::lasttex = nullptr;

    struct CroppedImage final : Image
    {
        public:
            void setup(Texture *tex_, float minw_ = 0, float minh_ = 0, float cropx_ = 0, float cropy_ = 0, float cropw_ = 1, float croph_ = 1)
            {
                Image::setup(tex_, minw_, minh_);
                cropx = cropx_;
                cropy = cropy_;
                cropw = cropw_;
                croph = croph_;
            }

            static const char *typestr() { return "#CroppedImage"; }

        private:
            bool target(float cx, float cy) override final
            {
                return !(tex->type&Texture::ALPHA) || checkalphamask(tex, cropx + cx/w*cropw, cropy + cy/h*croph);
            }

            void draw(float sx, float sy) override final
            {
                if(tex == notexture)
                {
                    Object::draw(sx, sy);
                    return;
                }

                bindtex();
                quads(sx, sy, w, h, cropx, cropy, cropw, croph);

                Object::draw(sx, sy);
            }

            const char *gettype() const override final
            {
                return typestr();
            }

            float cropx, cropy, cropw, croph;
    };

    struct StretchedImage final : Image
    {
        static const char *typestr() { return "#StretchedImage"; }
        const char *gettype() const override final
        {
            return typestr();
        }

        bool target(float cx, float cy) override final
        {
            if(!(tex->type&Texture::ALPHA))
            {
                return true;
            }
            float mx, my;
            if(w <= minw)
            {
                mx = cx/w;
            }
            else if(cx < minw/2)
            {
                mx = cx/minw;
            }
            else if(cx >= w - minw/2)
            {
                mx = 1 - (w - cx) / minw;
            }
            else
            {
                mx = 0.5f;
            }
            if(h <= minh)
            {
                my = cy/h;
            }
            else if(cy < minh/2)
            {
                my = cy/minh;
            }
            else if(cy >= h - minh/2)
            {
                my = 1 - (h - cy) / minh;
            }
            else
            {
                my = 0.5f;
            }

            return checkalphamask(tex, mx, my);
        }

        void draw(float sx, float sy) override final
        {
            if(tex == notexture)
            {
                Object::draw(sx, sy);
                return;
            }

            bindtex();

            float splitw = (minw ? std::min(minw, w) : w) / 2,
                  splith = (minh ? std::min(minh, h) : h) / 2,
                  vy = sy,
                  ty = 0;
            for(int i = 0; i < 3; ++i)
            {
                float vh = 0,
                      th = 0;
                switch(i)
                {
                    case 0:
                    {
                        if(splith < h - splith)
                        {
                            vh = splith;
                            th = 0.5f;
                        }
                        else
                        {
                            vh = h;
                            th = 1;
                        }
                        break;
                    }
                    case 1:
                    {
                        vh = h - 2*splith;
                        th = 0;
                        break;
                    }
                    case 2:
                    {
                        vh = splith;
                        th = 0.5f;
                        break;
                    }
                }
                float vx = sx,
                      tx = 0;
                for(int j = 0; j < 3; ++j)
                {
                    float vw = 0,
                          tw = 0;
                    switch(j)
                    {
                        case 0:
                        {
                            if(splitw < w - splitw)
                            {
                                vw = splitw;
                                tw = 0.5f;
                            }
                            else
                            {
                                vw = w;
                                tw = 1;
                            }
                            break;
                        }
                        case 1:
                        {
                            vw = w - 2*splitw;
                            tw = 0;
                            break;
                        }
                        case 2:
                        {
                            vw = splitw;
                            tw = 0.5f;
                            break;
                        }
                    }
                    quads(vx, vy, vw, vh, tx, ty, tw, th);
                    vx += vw;
                    tx += tw;
                    if(tx >= 1)
                    {
                        break;
                    }
                }
                vy += vh;
                ty += th;
                if(ty >= 1)
                {
                    break;
                }
            }

            Object::draw(sx, sy);
        }
    };

    struct BorderedImage final : Image
    {
        float texborder, screenborder;

        void setup(Texture *tex_, float texborder_, float screenborder_)
        {
            Image::setup(tex_);
            texborder = texborder_;
            screenborder = screenborder_;
        }

        static const char *typestr() { return "#BorderedImage"; }
        const char *gettype() const  override final
        {
            return typestr();
        }

        bool target(float cx, float cy) override final
        {
            if(!(tex->type&Texture::ALPHA))
            {
                return true;
            }
            float mx, my;
            if(cx < screenborder)
            {
                mx = cx/screenborder*texborder;
            }
            else if(cx >= w - screenborder)
            {
                mx = 1-texborder + (cx - (w - screenborder))/screenborder*texborder;
            }
            else
            {
                mx = texborder + (cx - screenborder)/(w - 2*screenborder)*(1 - 2*texborder);
            }
            if(cy < screenborder)
            {
                my = cy/screenborder*texborder;
            }
            else if(cy >= h - screenborder)
            {
                my = 1-texborder + (cy - (h - screenborder))/screenborder*texborder;
            }
            else
            {
                my = texborder + (cy - screenborder)/(h - 2*screenborder)*(1 - 2*texborder);
            }
            return checkalphamask(tex, mx, my);
        }

        void draw(float sx, float sy) override final
        {
            if(tex == notexture)
            {
                Object::draw(sx, sy);
                return;
            }

            bindtex();

            float vy = sy,
                  ty = 0;
            for(int i = 0; i < 3; ++i)
            {
                float vh = 0,
                      th = 0;
                switch(i)
                {
                    case 0:
                    {
                        vh = screenborder;
                        th = texborder;
                        break;
                    }
                    case 1:
                    {
                        vh = h - 2*screenborder;
                        th = 1 - 2*texborder;
                        break;
                    }
                    case 2:
                    {
                        vh = screenborder;
                        th = texborder;
                        break;
                    }
                }
                float vx = sx,
                      tx = 0;
                for(int j = 0; j < 3; ++j)
                {
                    float vw = 0,
                          tw = 0;
                    switch(j)
                    {
                        case 0:
                        {
                            vw = screenborder;
                            tw = texborder;
                            break;
                        }
                        case 1:
                        {
                            vw = w - 2*screenborder;
                            tw = 1 - 2*texborder;
                            break;
                        }
                        case 2:
                        {
                            vw = screenborder;
                            tw = texborder;
                            break;
                        }
                    }
                    quads(vx, vy, vw, vh, tx, ty, tw, th);
                    vx += vw;
                    tx += tw;
                }
                vy += vh;
                ty += th;
            }

            Object::draw(sx, sy);
        }
    };

    struct TiledImage final : Image
    {
        float tilew, tileh;

        void setup(Texture *tex_, float minw_ = 0, float minh_ = 0, float tilew_ = 0, float tileh_ = 0)
        {
            Image::setup(tex_, minw_, minh_);
            tilew = tilew_;
            tileh = tileh_;
        }

        static const char *typestr()
        {
            return "#TiledImage";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        bool target(float cx, float cy) override final
        {
            if(!(tex->type&Texture::ALPHA))
            {
                return true;
            }

            return checkalphamask(tex, std::fmod(cx/tilew, 1), std::fmod(cy/tileh, 1));
        }

        void draw(float sx, float sy) override final
        {
            if(tex == notexture)
            {
                Object::draw(sx, sy);
                return;
            }
            bindtex();
            if(tex->clamp)
            {
                for(float dy = 0; dy < h; dy += tileh)
                {
                    float dh = std::min(tileh, h - dy);
                    for(float dx = 0; dx < w; dx += tilew)
                    {
                        float dw = std::min(tilew, w - dx);
                        quads(sx + dx, sy + dy, dw, dh, 0, 0, dw / tilew, dh / tileh);
                    }
                }
            }
            else
            {
                quads(sx, sy, w, h, 0, 0, w/tilew, h/tileh);
            }
            Object::draw(sx, sy);
        }
    };

    struct Shape : Filler
    {
        enum
        {
            SOLID = 0,
            OUTLINE,
            MODULATE
        };

        int type;
        Color color;

        void setup(const Color &color_, int type_ = SOLID, float minw_ = 0, float minh_ = 0)
        {
            Filler::setup(minw_, minh_);

            color = color_;
            type = type_;
        }

        void startdraw() override final
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }
    };

    struct Triangle final : Shape
    {
        vec2 a, b, c;

        void setup(const Color &color_, float w = 0, float h = 0, int angle = 0, int type_ = SOLID)
        {
            a = vec2(0, -h*2.0f/3);
            b = vec2(-w/2, h/3);
            c = vec2(w/2, h/3);
            if(angle)
            {
                vec2 rot = sincosmod360(-angle);
                a.rotate_around_z(rot);
                b.rotate_around_z(rot);
                c.rotate_around_z(rot);
            }
            vec2 bbmin = vec2(a).min(b).min(c);
            a.sub(bbmin);
            b.sub(bbmin);
            c.sub(bbmin);
            vec2 bbmax = vec2(a).max(b).max(c);

            Shape::setup(color_, type_, bbmax.x, bbmax.y);
        }

        static const char *typestr()
        {
            return "#Triangle";
        }
        const char *gettype() const override final
        {
            return typestr();
        }

        bool target(float cx, float cy) override final
        {
            if(type == OUTLINE)
            {
                return false;
            }
            bool side = vec2(cx, cy).sub(b).cross(vec2(a).sub(b)) < 0;
            return (vec2(cx, cy).sub(c).cross(vec2(b).sub(c)) < 0) == side &&
                   (vec2(cx, cy).sub(a).cross(vec2(c).sub(a)) < 0) == side;
        }

        void draw(float sx, float sy) override final
        {
            Object::draw(sx, sy);

            changedraw(Change_Shader | Change_Color | Change_Blend);
            if(type==MODULATE)
            {
                modblend();
            }
            else
            {
                resetblend();
            }
            color.init();
            gle::begin(type == OUTLINE ? GL_LINE_LOOP : GL_TRIANGLES);
            gle::attrib(vec2(sx, sy).add(a));
            gle::attrib(vec2(sx, sy).add(b));
            gle::attrib(vec2(sx, sy).add(c));
            gle::end();
        }
    };

    struct Circle final : Shape
    {
        float radius;

        void setup(const Color &color_, float size, int type_ = SOLID)
        {
            Shape::setup(color_, type_, size, size);

            radius = size/2;
        }

        static const char *typestr() { return "#Circle"; }
        const char *gettype() const  override final
        {
            return typestr();
        }

        bool target(float cx, float cy) override final
        {
            if(type == OUTLINE)
            {
                return false;
            }
            float r = radius <= 0 ? std::min(w, h)/2 : radius;
            return vec2(cx, cy).sub(r).squaredlen() <= r*r;
        }

        void draw(float sx, float sy) override final
        {
            Object::draw(sx, sy);

            changedraw(Change_Shader | Change_Color | Change_Blend);
            if(type==MODULATE)
            {
                modblend();
            }
            else
            {
                resetblend();
            }

            float r = radius <= 0 ? std::min(w, h)/2 : radius;
            color.init();
            vec2 center(sx + r, sy + r);
            if(type == OUTLINE)
            {
                gle::begin(GL_LINE_LOOP);
                for(int angle = 0; angle < 360; angle += 360/15)
                    gle::attrib(vec2(sincos360[angle]).mul(r).add(center));
                gle::end();
            }
            else
            {
                gle::begin(GL_TRIANGLE_FAN);
                gle::attrib(center);
                gle::attribf(center.x + r, center.y);
                for(int angle = 360/15; angle < 360; angle += 360/15)
                {
                    vec2 p = vec2(sincos360[angle]).mul(r).add(center);
                    gle::attrib(p);
                    gle::attrib(p);
                }
                gle::attribf(center.x + r, center.y);
                gle::end();
            }
        }
    };

    // default size of text in terms of rows per screenful
    VARP(uitextrows, 1, 24, 200);
    FVAR(uitextscale, 1, 0, 0);

    static void setstring(char*& dst, const char* src)
    {
        if(dst)
        {
            if(dst != src && std::strcmp(dst, src))
            {
                delete[] dst;
                dst = newstring(src);
            }
        }
        else
        {
            dst = newstring(src);
        }
    }

    struct Text : Object
    {
        public:

            void setup(float scale_ = 1, const Color &color_ = Color(255, 255, 255), float wrap_ = -1)
            {
                Object::setup();

                scale = scale_;
                color = color_;
                wrap = wrap_;
            }

            virtual const char *getstr() const
            {
                return "";
            }

        protected:
            const char *gettype() const override
            {
                return typestr();
            }

            void draw(float sx, float sy) override final
            {
                Object::draw(sx, sy);

                changedraw(Change_Shader | Change_Color);

                float oldscale = textscale;
                textscale = drawscale();
                ttr.fontsize(36);
                const float conscalefactor = 0.000666;
                pushhudscale(conscalefactor);
                ttr.renderttf(getstr(), {color.r, color.g, color.b, color.a}, sx*1500, sy*1500, scale*33);
                pophudmatrix();
                //draw_text(getstr(), sx/textscale, sy/textscale, 0, color.g, color.b, color.a, -1, wrap >= 0 ? static_cast<int>(wrap/textscale) : -1);

                textscale = oldscale;
            }

            void layout() override final
            {
                Object::layout();

                float k = drawscale(), tw, th;
                ttr.ttfbounds(getstr(), tw, th, 42);
                w = std::max(w, tw*k);
                h = std::max(h, th*k);
            }

        private:
            float scale, wrap;
            Color color;

            static const char *typestr()
            {
                return "#Text";
            }

            float drawscale() const
            {
                return scale / FONTH;
            }

    };

    struct TextString final : Text
    {
        char *str;

        TextString() : str(nullptr)
        {
        }

        ~TextString()
        {
            delete[] str;
        }

        void setup(const char *str_, float scale_ = 1, const Color &color_ = Color(255, 255, 255), float wrap_ = -1)
        {
            Text::setup(scale_, color_, wrap_);

            setstring(str, str_);
        }

        static const char *typestr()
        {
            return "#TextString";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        const char *getstr() const override final
        {
            return str;
        }
    };

    struct TextInt final : Text
    {
        int val;
        char str[20];

        TextInt() : val(0) { str[0] = '0'; str[1] = '\0'; }

        void setup(int val_, float scale_ = 1, const Color &color_ = Color(255, 255, 255), float wrap_ = -1)
        {
            Text::setup(scale_, color_, wrap_);

            if(val != val_)
            {
                val = val_;
                intformat(str, val, sizeof(str));
            }
        }

        static const char *typestr()
        {
            return "#TextInt";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        const char *getstr() const override final
        {
            return str;
        }
    };

    struct TextFloat final : Text
    {
        float val;
        char str[20];

        TextFloat() : val(0) { std::memcpy(str, "0.0", 4); }

        void setup(float val_, float scale_ = 1, const Color &color_ = Color(255, 255, 255), float wrap_ = -1)
        {
            Text::setup(scale_, color_, wrap_);

            if(val != val_)
            {
                val = val_;
                floatformat(str, val, sizeof(str));
            }
        }

        static const char *typestr()
        {
            return "#TextFloat";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        const char *getstr() const override final
        {
            return str;
        }
    };

    struct Font final : Object
    {
        ::font *font;

        Font() : font(nullptr) {}

        void setup(const char *name)
        {
            Object::setup();
        }

        void layout() override final
        {
            pushfont();
            setfont(font);
            Object::layout();
            popfont();
        }

        void draw(float sx, float sy) override final
        {
            pushfont();
            setfont(font);
            Object::draw(sx, sy);
            popfont();
        }

        void buildchildren(uint *contents)
        {
            pushfont();
            setfont(font);
            Object::buildchildren(contents);
            popfont();
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) override final \
            { \
                pushfont(); \
                setfont(font); \
                Object::func##children(cx, cy, mask, inside, setflags); \
                popfont(); \
            }
        DOSTATES
        #undef DOSTATE

        bool rawkey(int code, bool isdown) override final
        {
            pushfont();
            setfont(font);
            bool result = Object::rawkey(code, isdown);
            popfont();
            return result;
        }

        bool key(int code, bool isdown) override final
        {
            pushfont();
            setfont(font);
            bool result = Object::key(code, isdown);
            popfont();
            return result;
        }

        bool textinput(const char *str, int len) override final
        {
            pushfont();
            setfont(font);
            bool result = Object::textinput(str, len);
            popfont();
            return result;
        }
    };

    float uicontextscale = 0;

    void uicontextscalecmd()
    {
        floatret(FONTH*uicontextscale);
    }

    struct Console final : Filler
    {
        void setup(float minw_ = 0, float minh_ = 0)
        {
            Filler::setup(minw_, minh_);
        }

        static const char *typestr()
        {
            return "#Console";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        float drawscale() const
        {
            return uicontextscale;
        }

        void draw(float sx, float sy) override final
        {
            Object::draw(sx, sy);

            changedraw(Change_Shader | Change_Color);

            float k = drawscale();
            pushhudtranslate(sx, sy, k);
            renderfullconsole(w/k, h/k);
            pophudmatrix();
        }
    };

    struct Clipper : Object
    {
        float clipw, cliph, virtw, virth;

        void setup(float clipw_ = 0, float cliph_ = 0)
        {
            Object::setup();
            clipw = clipw_;
            cliph = cliph_;
            virtw = virth = 0;
        }

        static const char *typestr()
        {
            return "#Clipper";
        }

        const char *gettype() const override
        {
            return typestr();
        }

        void layout() override
        {
            Object::layout();

            virtw = w;
            virth = h;
            if(clipw)
            {
                w = std::min(w, clipw);
            }
            if(cliph)
            {
                h = std::min(h, cliph);
            }
        }

        void adjustchildren() override final
        {
            adjustchildrento(0, 0, virtw, virth);
        }

        void draw(float sx, float sy) override
        {
            if((clipw && virtw > clipw) || (cliph && virth > cliph))
            {
                stopdrawing();
                pushclip(sx, sy, w, h);
                Object::draw(sx, sy);
                stopdrawing();
                popclip();
            }
            else
            {
                Object::draw(sx, sy);
            }
        }
    };

    struct Scroller final : Clipper
    {
        float offsetx, offsety;

        Scroller() : offsetx(0), offsety(0) {}

        void setup(float clipw_ = 0, float cliph_ = 0)
        {
            Clipper::setup(clipw_, cliph_);
        }

        static const char *typestr()
        {
            return "#Scroller";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void layout() override final
        {
            Clipper::layout();
            offsetx = std::min(offsetx, hlimit());
            offsety = std::min(offsety, vlimit());
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) override final \
            { \
                cx += offsetx; \
                cy += offsety; \
                if(cx < virtw && cy < virth) \
                { \
                    Clipper::func##children(cx, cy, mask, inside, setflags); \
                } \
            }
        DOSTATES
        #undef DOSTATE

        void draw(float sx, float sy) override final
        {
            if((clipw && virtw > clipw) || (cliph && virth > cliph))
            {
                stopdrawing();
                pushclip(sx, sy, w, h);
                Object::draw(sx - offsetx, sy - offsety);
                stopdrawing();
                popclip();
            }
            else
            {
                Object::draw(sx, sy);
            }
        }

        float hlimit() const
        {
            return std::max(virtw - w, 0.0f);
        }

        float vlimit() const
        {
            return std::max(virth - h, 0.0f);
        }

        float hoffset() const
        {
            return offsetx / std::max(virtw, w);
        }

        float voffset() const
        {
            return offsety / std::max(virth, h);
        }

        float hscale() const
        {
            return w / std::max(virtw, w);
        }

        float vscale() const
        {
            return h / std::max(virth, h);
        }

        void addhscroll(float hscroll)
        {
            sethscroll(offsetx + hscroll);
        }

        void addvscroll(float vscroll)
        {
            setvscroll(offsety + vscroll);
        }

        void sethscroll(float hscroll)
        {
            offsetx = std::clamp(hscroll, 0.0f, hlimit());
        }

        void setvscroll(float vscroll)
        {
            offsety = std::clamp(vscroll, 0.0f, vlimit());
        }

        void scrollup(float cx, float cy) override final;

        void scrolldown(float cx, float cy) override final;
    };

    struct ScrollButton final : Object
    {
        static const char *typestr()
        {
            return "#ScrollButton";
        }

        const char *gettype() const override final
        {
            return typestr();
        }
    };

    class ScrollBar : public Object
    {

        public:
            ScrollBar() : offsetx(0), offsety(0) {}

            static const char *typestr()
            {
                return "#ScrollBar";
            }

            void hold(float cx, float cy) override final
            {
                ScrollButton *button = static_cast<ScrollButton *>(find(ScrollButton::typestr(), false));
                if(button && button->haschildstate(State_Hold))
                {
                    movebutton(button, offsetx, offsety, cx - button->x, cy - button->y);
                }
            }

            void press(float cx, float cy) override final
            {
                ScrollButton *button = static_cast<ScrollButton *>(find(ScrollButton::typestr(), false));
                if(button && button->haschildstate(State_Press))
                {
                    offsetx = cx - button->x;
                    offsety = cy - button->y;
                }
                else
                {
                    scrollto(cx, cy, true);
                }
            }

            void arrowscroll(float dir)
            {
                addscroll(dir*curtime/1000.0f);
            }
            void wheelscroll(float step);
            virtual int wheelscrolldirection() const
            {
                return 1;
            }

        protected:
            const char *gettype() const override
            {
                return typestr();
            }

            const char *gettypename() const override final
            {
                return typestr();
            }

            bool target(float, float) override final //note unnamed function parameters
            {
                return true;
            }

            virtual void scrollto(float, float, bool) {} //note unnamed function parameters
            virtual void movebutton(Object *o, float fromx, float fromy, float tox, float toy) = 0;
            virtual void addscroll(Scroller *scroller, float dir) = 0;

        private:
            float offsetx, offsety;

            void addscroll(float dir)
            {
                Scroller *scroller = static_cast<Scroller *>(findsibling(Scroller::typestr()));
                if(scroller)
                {
                    addscroll(scroller, dir);
                }
            }
    };

    void Scroller::scrollup(float, float) //note unnamed function parameters
    {
        ScrollBar *scrollbar = static_cast<ScrollBar *>(findsibling(ScrollBar::typestr()));
        if(scrollbar)
        {
            scrollbar->wheelscroll(-scrollbar->wheelscrolldirection());
        }
    }

    void Scroller::scrolldown(float, float) //note unnamed function parameters
    {
        ScrollBar *scrollbar = static_cast<ScrollBar *>(findsibling(ScrollBar::typestr()));
        if(scrollbar)
        {
            scrollbar->wheelscroll(scrollbar->wheelscrolldirection());
        }
    }

    struct ScrollArrow : Object
    {
        float arrowspeed;

        void setup(float arrowspeed_ = 0)
        {
            Object::setup();
            arrowspeed = arrowspeed_;
        }

        static const char *typestr()
        {
            return "#ScrollArrow";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void hold(float, float) override final //note unnamed function parameters
        {
            ScrollBar *scrollbar = static_cast<ScrollBar *>(findsibling(ScrollBar::typestr()));
            if(scrollbar)
            {
                scrollbar->arrowscroll(arrowspeed);
            }
        }
    };

    VARP(uiscrollsteptime, 0, 50, 1000);

    void ScrollBar::wheelscroll(float step)
    {
        ScrollArrow *arrow = static_cast<ScrollArrow *>(findsibling(ScrollArrow::typestr()));
        if(arrow)
        {
            addscroll(arrow->arrowspeed*step*uiscrollsteptime/1000.0f);
        }
    }

    struct HorizontalScrollBar final : ScrollBar
    {
        static const char *typestr()
        {
            return "#HorizontalScrollBar";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void addscroll(Scroller *scroller, float dir) override final
        {
            scroller->addhscroll(dir);
        }

        void scrollto(float cx, float, bool closest = false) override final //note unnamed function parameter
        {
            Scroller *scroller = static_cast<Scroller *>(findsibling(Scroller::typestr()));
            if(!scroller)
            {
                return;
            }
            ScrollButton *button = static_cast<ScrollButton *>(find(ScrollButton::typestr(), false));
            if(!button)
            {
                return;
            }
            float bscale = (w - button->w) / (1 - scroller->hscale()),
                  offset = bscale > 1e-3f ? (closest && cx >= button->x + button->w ? cx - button->w : cx)/bscale : 0;
            scroller->sethscroll(offset*scroller->virtw);
        }

        void adjustchildren() override final
        {
            Scroller *scroller = static_cast<Scroller *>(findsibling(Scroller::typestr()));
            if(!scroller)
            {
                return;
            }
            ScrollButton *button = static_cast<ScrollButton *>(find(ScrollButton::typestr(), false));
            if(!button)
            {
                return;
            }
            float bw = w*scroller->hscale();
            button->w = std::max(button->w, bw);
            float bscale = scroller->hscale() < 1 ? (w - button->w) / (1 - scroller->hscale()) : 1;
            button->x = scroller->hoffset()*bscale;
            button->adjust &= ~Align_HMask;

            ScrollBar::adjustchildren();
        }

        void movebutton(Object *o, float fromx, float, float tox, float toy) override final //note unnamed function parameter
        {
            scrollto(o->x + tox - fromx, o->y + toy);
        }
    };

    struct VerticalScrollBar final : ScrollBar
    {
        static const char *typestr()
        {
            return "#VerticalScrollBar";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void addscroll(Scroller *scroller, float dir) override final
        {
            scroller->addvscroll(dir);
        }

        void scrollto(float, float cy, bool closest = false) override final //note unnamed function parameter
        {
            Scroller *scroller = static_cast<Scroller *>(findsibling(Scroller::typestr()));
            if(!scroller)
            {
                return;
            }
            ScrollButton *button = static_cast<ScrollButton *>(find(ScrollButton::typestr(), false));
            if(!button)
            {
                return;
            }
            float bscale = (h - button->h) / (1 - scroller->vscale()),
                  offset = bscale > 1e-3f ? (closest && cy >= button->y + button->h ? cy - button->h : cy)/bscale : 0;
            scroller->setvscroll(offset*scroller->virth);
        }

        void adjustchildren() override final
        {
            Scroller *scroller = static_cast<Scroller *>(findsibling(Scroller::typestr()));
            if(!scroller)
            {
                return;
            }
            ScrollButton *button = static_cast<ScrollButton *>(find(ScrollButton::typestr(), false));
            if(!button)
            {
                return;
            }
            float bh = h*scroller->vscale();
            button->h = std::max(button->h, bh);
            float bscale = scroller->vscale() < 1 ? (h - button->h) / (1 - scroller->vscale()) : 1;
            button->y = scroller->voffset()*bscale;
            button->adjust &= ~Align_VMask;

            ScrollBar::adjustchildren();
        }

        void movebutton(Object *o, float, float fromy, float tox, float toy) override final //note unnamed function parameter
        {
            scrollto(o->x + tox, o->y + toy - fromy);
        }

        int wheelscrolldirection() const override final
        {
            return -1;
        }
    };

    struct SliderButton final : Object
    {
        static const char *typestr()
        {
            return "#SliderButton";
        }
        const char *gettype() const override final
        {
            return typestr();
        }
    };

    static double getfval(ident *id, double val = 0)
    {
        switch(id->type)
        {
            case Id_Var:
            {
                val = *id->storage.i;
                break;
            }
            case Id_FloatVar:
            {
                val = *id->storage.f;
                break;
            }
            case Id_StringVar:
            {
                val = parsenumber(*id->storage.s);
                break;
            }
            case Id_Alias:
            {
                val = id->getnumber();
                break;
            }
            case Id_Command:
            {
                tagval t;
                executeret(id, nullptr, 0, true, t);
                val = t.getnumber();
                t.cleanup();
                break;
            }
        }
        return val;
    }

    static void setfval(ident *id, double val, uint *onchange = nullptr)
    {
        switch(id->type)
        {
            case Id_Var:
            {
                setvarchecked(id, static_cast<int>(std::clamp(val, double(INT_MIN), double(INT_MAX))));
                break;
            }
            case Id_FloatVar:
            {
                setfvarchecked(id, val);
                break;
            }
            case Id_StringVar:
            {
                setsvarchecked(id, numberstr(val));
                break;
            }
            case Id_Alias:
            {
                alias(id->name, numberstr(val));
                break;
            }
            case Id_Command:
            {
                tagval t;
                t.setnumber(val);
                execute(id, &t, 1);
                break;
            }
        }
        if(onchange && (*onchange&Code_OpMask) != Code_Exit)
        {
            execute(onchange);
        }
    }

    struct Slider : Object
    {
        ident *id;
        double val, vmin, vmax, vstep;
        bool changed;

        Slider() : id(nullptr), val(0), vmin(0), vmax(0), vstep(0), changed(false) {}

        void setup(ident *id_, double vmin_ = 0, double vmax_ = 0, double vstep_ = 1, uint *onchange = nullptr)
        {
            Object::setup();
            if(!vmin_ && !vmax_)
            {
                switch(id_->type)
                {
                    case Id_Var:
                    {
                        vmin_ = id_->minval;
                        vmax_ = id_->maxval;
                        break;
                    }
                    case Id_FloatVar:
                    {
                        vmin_ = id_->minvalf;
                        vmax_ = id_->maxvalf;
                        break;
                    }
                }
            }
            if(id != id_)
            {
                changed = false;
            }
            id = id_;
            vmin = vmin_;
            vmax = vmax_;
            vstep = vstep_ > 0 ? vstep_ : 1;
            if(changed)
            {
                setfval(id, val, onchange);
                changed = false;
            }
            else
            {
                val = getfval(id, vmin);
            }
        }

        static const char *typestr()
        {
            return "#Slider";
        }

        const char *gettype() const override
        {
            return typestr();
        }

        const char *gettypename() const override final
        {
            return typestr();
        }

        bool target(float, float) override final //note unnamed function parameters
        {
            return true;
        }

        void arrowscroll(double dir)
        {
            double newval = val + dir*vstep;
            newval += vstep * (newval < 0 ? -0.5 : 0.5);
            newval -= std::fmod(newval, vstep);
            newval = std::clamp(newval, std::min(vmin, vmax), std::max(vmin, vmax));
            if(val != newval)
            {
                changeval(newval);
            }
        }

        void wheelscroll(float step);
        virtual int wheelscrolldirection() const
        {
            return 1;
        }

        void scrollup(float, float) override final //note unnamed function parameters
        {
            wheelscroll(-wheelscrolldirection());
        }

        void scrolldown(float, float) override final //note unnamed function parameters
        {
            wheelscroll(wheelscrolldirection());
        }

        virtual void scrollto(float, float) {} //note unnamed function parameters

        void hold(float cx, float cy) override final
        {
            scrollto(cx, cy);
        }

        void changeval(double newval)
        {
            val = newval;
            changed = true;
        }
    };

    VARP(uislidersteptime, 0, 50, 1000);

    struct SliderArrow final : Object
    {
        double stepdir;
        int laststep;

        SliderArrow() : laststep(0) {}

        void setup(double dir_ = 0)
        {
            Object::setup();
            stepdir = dir_;
        }

        static const char *typestr()
        {
            return "#SliderArrow";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void press(float, float) override final //note unnamed function parameters
        {
            laststep = totalmillis + 2*uislidersteptime;

            Slider *slider = static_cast<Slider *>(findsibling(Slider::typestr()));
            if(slider)
            {
                slider->arrowscroll(stepdir);
            }
        }

        void hold(float, float) override final //note unnamed function parameters
        {
            if(totalmillis < laststep + uislidersteptime)
            {
                return;
            }
            laststep = totalmillis;

            Slider *slider = static_cast<Slider *>(findsibling(Slider::typestr()));
            if(slider)
            {
                slider->arrowscroll(stepdir);
            }
        }
    };

    void Slider::wheelscroll(float step)
    {
        SliderArrow *arrow = static_cast<SliderArrow *>(findsibling(SliderArrow::typestr()));
        if(arrow)
        {
            step *= arrow->stepdir;
        }
        arrowscroll(step);
    }

    struct HorizontalSlider final : Slider
    {
        static const char *typestr()
        {
            return "#HorizontalSlider";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void scrollto(float cx, float) override final //note unnamed function parameter
        {
            SliderButton *button = static_cast<SliderButton *>(find(SliderButton::typestr(), false));
            if(!button)
            {
                return;
            }
            float offset = w > button->w ? std::clamp((cx - button->w/2)/(w - button->w), 0.0f, 1.0f) : 0.0f;
            int step = static_cast<int>((val - vmin) / vstep),
                bstep = static_cast<int>(offset * (vmax - vmin) / vstep);
            if(step != bstep)
            {
                changeval(bstep * vstep + vmin);
            }
        }

        void adjustchildren() override final
        {
            SliderButton *button = static_cast<SliderButton *>(find(SliderButton::typestr(), false));
            if(!button)
            {
                return;
            }
            int step = static_cast<int>((val - vmin) / vstep),
                bstep = static_cast<int>(button->x / (w - button->w) * (vmax - vmin) / vstep);
            if(step != bstep)
            {
                button->x = (w - button->w) * step * vstep / (vmax - vmin);
            }
            button->adjust &= ~Align_HMask;

            Slider::adjustchildren();
        }
    };

    struct VerticalSlider final : Slider
    {
        static const char *typestr()
        {
            return "#VerticalSlider";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void scrollto(float, float cy) override final //note unnamed function parameter
        {
            SliderButton *button = static_cast<SliderButton *>(find(SliderButton::typestr(), false));
            if(!button)
            {
                return;
            }
            float offset = h > button->h ? std::clamp((cy - button->h/2)/(h - button->h), 0.0f, 1.0f) : 0.0f;
            int step = static_cast<int>((val - vmin) / vstep),
                bstep = static_cast<int>(offset * (vmax - vmin) / vstep);
            if(step != bstep)
            {
                changeval(bstep * vstep + vmin);
            }
        }

        void adjustchildren() override final
        {
            SliderButton *button = static_cast<SliderButton *>(find(SliderButton::typestr(), false));
            if(!button)
            {
                return;
            }
            int step = static_cast<int>((val - vmin) / vstep),
                bstep = static_cast<int>(button->y / (h - button->h) * (vmax - vmin) / vstep);
            if(step != bstep)
            {
                button->y = (h - button->h) * step * vstep / (vmax - vmin);
            }
            button->adjust &= ~Align_VMask;

            Slider::adjustchildren();
        }

        int wheelscrolldirection() const override final
        {
            return -1;
        }
    };

    struct TextEditor : Object
    {
        static TextEditor *focus;

        float scale, offsetx, offsety;
        Editor *edit;
        char *keyfilter;

        TextEditor() : edit(nullptr), keyfilter(nullptr) {}

        void setup(const char *name, int length, int height, float scale_ = 1, const char *initval = nullptr, int mode = Editor_Used, const char *keyfilter_ = nullptr)
        {
            Object::setup();
            Editor *edit_ = useeditor(name, mode, false, initval);
            if(edit_ != edit)
            {
                if(edit)
                {
                    clearfocus();
                }
                edit = edit_;
            }
            else if(isfocus() && !hasstate(State_Hover))
            {
                commit();
            }
            if(initval && edit->mode == Editor_Focused && !isfocus())
            {
                edit->init(initval);
            }
            edit->active = true;
            edit->linewrap = length < 0;
            edit->maxx = edit->linewrap ? -1 : length;
            edit->maxy = height <= 0 ? 1 : -1;
            edit->pixelwidth = std::abs(length)*fontwidth();
            if(edit->linewrap && edit->maxy == 1)
            {
                edit->updateheight();
            }
            else
            {
                edit->pixelheight = FONTH*std::max(height, 1);
            }
            scale = scale_;
            if(keyfilter_)
            {
                setstring(keyfilter, keyfilter_);
            }
            else
            {
                delete[] keyfilter;
                keyfilter = nullptr;
            }
        }
        ~TextEditor()
        {
            clearfocus();
            delete[] keyfilter;
            keyfilter = nullptr;
        }

        static void setfocus(TextEditor *e)
        {
            if(focus == e)
            {
                return;
            }
            focus = e;
            bool allowtextinput = focus!=nullptr && focus->allowtextinput();
            ::textinput(allowtextinput, TextInput_GUI);
            ::keyrepeat(allowtextinput, KeyRepeat_GUI);
        }
        void setfocus()
        {
            setfocus(this);
        }

        void clearfocus()
        {
            if(focus == this)
            {
                setfocus(nullptr);
            }
        }

        bool isfocus() const
        {
            return focus == this;
        }

        static const char *typestr()
        {
            return "#TextEditor";
        }

        const char *gettype() const override
        {
            return typestr();
        }

        bool target(float, float) override final//note unnamed function parameters
        {
            return true;
        }

        float drawscale() const
        {
            return scale / FONTH;
        }

        void draw(float sx, float sy) override final
        {
            changedraw(Change_Shader | Change_Color);

            edit->rendered = true;

            float k = drawscale();
            pushhudtranslate(sx, sy, k);

            edit->draw(fontwidth()/2, 0, 0xFFFFFF, isfocus());

            pophudmatrix();

            Object::draw(sx, sy);
        }

        void layout() override final
        {
            Object::layout();

            float k = drawscale();
            w = std::max(w, (edit->pixelwidth + fontwidth())*k);
            h = std::max(h, edit->pixelheight*k);
        }

        virtual void resetmark(float cx, float cy)
        {
            edit->mark(false);
            offsetx = cx;
            offsety = cy;
        }

        void press(float cx, float cy) override final
        {
            setfocus();
            resetmark(cx, cy);
        }

        void hold(float cx, float cy) override final
        {
            if(isfocus())
            {
                float k = drawscale();
                bool dragged = std::max(std::fabs(cx - offsetx), std::fabs(cy - offsety)) > (FONTH/8.0f)*k;
                edit->hit(static_cast<int>(std::floor(cx/k - fontwidth()/2)), static_cast<int>(std::floor(cy/k)), dragged);
            }
        }

        void scrollup(float, float) override final //note unnamed function parameters
        {
            edit->scrollup();
        }

        void scrolldown(float, float) override final //note unnamed function parameters
        {
            edit->scrolldown();
        }

        virtual void cancel()
        {
            clearfocus();
        }

        virtual void commit()
        {
            clearfocus();
        }

        bool key(int code, bool isdown) override final
        {
            if(Object::key(code, isdown))
            {
                return true;
            }
            if(!isfocus())
            {
                return false;
            }
            switch(code)
            {
                case SDLK_ESCAPE:
                {
                    if(isdown)
                    {
                        cancel();
                    }
                    return true;
                }
                case SDLK_RETURN:
                case SDLK_TAB:
                {
                    if(edit->maxy != 1)
                    {
                        break;
                    }
                }
                case SDLK_KP_ENTER:
                {
                    if(isdown)
                    {
                        commit();
                    }
                    return true;
                }
            }
            if(isdown)
            {
                edit->key(code);
            }
            return true;
        }

        virtual bool allowtextinput() const
        {
            return true;
        }

        bool textinput(const char *str, int len) override final
        {
            if(Object::textinput(str, len))
            {
                return true;
            }
            if(!isfocus() || !allowtextinput())
            {
                return false;
            }
            if(!keyfilter)
            {
                edit->input(str, len);
            }
            else while(len > 0)
            {
                int accept = std::min(len, static_cast<int>(std::strspn(str, keyfilter)));
                if(accept > 0)
                {
                    edit->input(str, accept);
                }
                str += accept + 1;
                len -= accept + 1;
                if(len <= 0)
                {
                    break;
                }
                int reject = static_cast<int>(std::strcspn(str, keyfilter));
                str += reject;
                str -= reject;
            }
            return true;
        }
    };

    TextEditor *TextEditor::focus = nullptr;

    static const char *getsval(ident *id, bool &shouldfree, const char *val = "")
    {
        switch(id->type)
        {
            case Id_Var:
            {
                val = intstr(*id->storage.i);
                break;
            }
            case Id_FloatVar:
            {
                val = floatstr(*id->storage.f);
                break;
            }
            case Id_StringVar:
            {
                val = *id->storage.s;
                break;
            }
            case Id_Alias:
            {
                val = id->getstr();
                break;
            }
            case Id_Command:
            {
                val = executestr(id, nullptr, 0, true);
                shouldfree = true;
                break;
            }
        }
        return val;
    }

    static void setsval(ident *id, const char *val, uint *onchange = nullptr)
    {
        switch(id->type)
        {
            case Id_Var:
            {
                setvarchecked(id, parseint(val));
                break;
            }
            case Id_FloatVar:
            {
                setfvarchecked(id, parsefloat(val));
                break;
            }
            case Id_StringVar:
            {
                setsvarchecked(id, val);
                break;
            }
            case Id_Alias:
            {
                alias(id->name, val);
                break;
            }
            case Id_Command:
            {
                tagval t;
                t.setstr(newstring(val));
                execute(id, &t, 1);
                break;
            }
        }
        if(onchange && (*onchange&Code_OpMask) != Code_Exit)
        {
            execute(onchange);
        }
    }

    struct Field : TextEditor
    {
        ident *id;
        bool changed;

        Field() : id(nullptr), changed(false) {}

        void setup(ident *id_, int length, uint *onchange, float scale = 1, const char *keyfilter_ = nullptr)
        {
            if(isfocus() && !hasstate(State_Hover))
            {
                commit();
            }
            if(changed)
            {
                if(id == id_)
                {
                    setsval(id, edit->lines[0].text, onchange);
                }
                changed = false;
            }
            bool shouldfree = false;
            const char *initval = id != id_ || !isfocus() ? getsval(id_, shouldfree) : nullptr;
            TextEditor::setup(id_->name, length, 0, scale, initval, Editor_Focused, keyfilter_);
            if(shouldfree)
            {
                delete[] initval;
            }
            id = id_;
        }

        static const char *typestr()
        {
            return "#Field";
        }
        const char *gettype() const override
        {
            return typestr();
        }

        void commit() override final
        {
            TextEditor::commit();
            changed = true;
        }

        void cancel() override final
        {
            TextEditor::cancel();
            changed = false;
        }
    };

    struct KeyField final : Field
    {
        static const char *typestr()
        {
            return "#KeyField";
        }
        const char *gettype() const override final
        {
            return typestr();
        }

        void resetmark(float cx, float cy) override final
        {
            edit->clear();
            Field::resetmark(cx, cy);
        }

        void insertkey(int code)
        {
            const char *keyname = getkeyname(code);
            if(keyname)
            {
                if(!edit->empty())
                {
                    edit->insert(" ");
                }
                edit->insert(keyname);
            }
        }

        bool rawkey(int code, bool isdown) override final
        {
            if(Object::rawkey(code, isdown))
            {
                return true;
            }
            if(!isfocus() || !isdown)
            {
                return false;
            }
            if(code == SDLK_ESCAPE)
            {
                commit();
            }
            else
            {
                insertkey(code);
            }
            return true;
        }

        bool allowtextinput() const override final
        {
            return false;
        }
    };

    struct Preview : Target
    {
        void startdraw() override final
        {
            glDisable(GL_BLEND);

            if(clipstack.size())
            {
                glDisable(GL_SCISSOR_TEST);
            }
        }

        void enddraw() override final
        {
            glEnable(GL_BLEND);

            if(clipstack.size())
            {
                glEnable(GL_SCISSOR_TEST);
            }
        }
    };

    struct ModelPreview final : Preview
    {
        char *name;
        int anim;

        ModelPreview() : name(nullptr) {}
        ~ModelPreview() { delete[] name; }

        void setup(const char *name_, const char *animspec, float minw_, float minh_)
        {
            Preview::setup(minw_, minh_);
            setstring(name, name_);

            anim = Anim_All;
            if(animspec[0])
            {
                if(isdigit(animspec[0]))
                {
                    anim = parseint(animspec);
                    if(anim >= 0)
                    {
                        anim %= Anim_Index;
                    }
                    else
                    {
                        anim = Anim_All;
                    }
                }
                else
                {
                    std::vector<size_t> anims = findanims(animspec);
                    if(anims.size())
                    {
                        anim = anims[0];
                    }
                }
            }
            anim |= Anim_Loop;
        }

        static const char *typestr()
        {
            return "#ModelPreview";
        }
        const char *gettype() const override final
        {
            return typestr();
        }

        void draw(float sx, float sy) override final
        {
            Object::draw(sx, sy);

            changedraw(Change_Shader);

            int sx1, sy1, sx2, sy2;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview.start(sx1, sy1, sx2-sx1, sy2-sy1, false, clipstack.size() > 0);
            model *m = loadmodel(name);
            if(m)
            {
                vec center, radius;
                m->boundbox(center, radius);
                float yaw;
                vec o = calcmodelpreviewpos(radius, yaw).sub(center);
                rendermodel(name, anim, o, yaw, 0, 0, 0, nullptr, nullptr, 0);
            }
            if(clipstack.size())
            {
                clipstack.back().scissor();
            }
            modelpreview.end();
        }
    };

    class PrefabPreview final : public Preview
    {
        public:
            PrefabPreview() : name(nullptr)
            {
            }

            ~PrefabPreview()
            {
                delete[] name;
            }

            void setup(const char *name_, int color_, float minw_, float minh_)
            {
                Preview::setup(minw_, minh_);
                setstring(name, name_);
                color = vec::hexcolor(color_);
            }

            static const char *typestr()
            {
                return "#PrefabPreview";
            }

            const char *gettype() const override final
            {
                return typestr();
            }

            void draw(float sx, float sy) override final
            {
                Object::draw(sx, sy);
                changedraw(Change_Shader);
                int sx1, sy1, sx2, sy2;
                window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
                modelpreview.start(sx1, sy1, sx2-sx1, sy2-sy1, false, clipstack.size() > 0);
                previewprefab(name, color);
                if(clipstack.size())
                {
                    clipstack.back().scissor();
                }
                modelpreview.end();
            }
        private:
            char *name;
            vec color;
    };

    VARP(uislotviewtime, 0, 25, 1000);
    static int lastthumbnail = 0;

    struct SlotViewer : Target
    {
        int index;

        void setup(int index_, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_);
            index = index_;
        }

        static const char *typestr() { return "#SlotViewer"; }
        const char *gettype() const override
        {
            return typestr();
        }

        void previewslot(Slot &slot, VSlot &vslot, float x, float y)
        {
            if(slot.sts.empty())
            {
                return;
            }
            Texture *t = nullptr,
                    *glowtex = nullptr;
            if(slot.loaded)
            {
                t = slot.sts[0].t;
                if(t == notexture)
                {
                    return;
                }
                Slot &slot = *vslot.slot;
                if(slot.texmask&(1 << Tex_Glow))
                {
                    for(uint j = 0; j < slot.sts.size(); j++)
                    {
                        if(slot.sts[j].type == Tex_Glow)
                        {
                            glowtex = slot.sts[j].t;
                            break;
                        }
                    }
                }
            }
            else
            {
                if(!slot.thumbnail)
                {
                    if(totalmillis - lastthumbnail < uislotviewtime)
                    {
                        return;
                    }
                    slot.loadthumbnail();
                    lastthumbnail = totalmillis;
                }
                if(slot.thumbnail != notexture)
                {
                    t = slot.thumbnail;
                }
                else
                {
                    return;
                }
            }

            changedraw(Change_Shader | Change_Color);

            SETSHADER(hudrgb);
            vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
            int xoff = vslot.offset.x,
                yoff = vslot.offset.y;
            if(vslot.rotation)
            {
                const texrotation &r = texrotations[vslot.rotation];
                if(r.swapxy)
                {
                    std::swap(xoff, yoff);
                    for(int k = 0; k < 4; ++k)
                    {
                        std::swap(tc[k].x, tc[k].y);
                    }
                }
                if(r.flipx)
                {
                    xoff *= -1;
                    for(int k = 0; k < 4; ++k)
                    {
                        tc[k].x *= -1;
                    }
                }
                if(r.flipy)
                {
                    yoff *= -1;
                    for(int k = 0; k < 4; ++k)
                    {
                        tc[k].y *= -1;
                    }
                }
            }
            float xt = std::min(1.0f, t->xs/static_cast<float>(t->ys)),
                  yt = std::min(1.0f, t->ys/static_cast<float>(t->xs));
            for(int k = 0; k < 4; ++k)
            {
                tc[k].x = tc[k].x/xt - static_cast<float>(xoff)/t->xs;
                tc[k].y = tc[k].y/yt - static_cast<float>(yoff)/t->ys;
            }
            glBindTexture(GL_TEXTURE_2D, t->id);
            if(slot.loaded)
            {
                gle::color(vslot.colorscale);
            }
            else gle::colorf(1, 1, 1);
            quad(x, y, w, h, tc);
            if(glowtex)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glBindTexture(GL_TEXTURE_2D, glowtex->id);
                gle::color(vslot.glowcolor);
                quad(x, y, w, h, tc);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
        }

        void draw(float sx, float sy) override
        {
            Slot &slot = lookupslot(index, false);
            previewslot(slot, *slot.variants, sx, sy);

            Object::draw(sx, sy);
        }
    };

    struct VSlotViewer final : SlotViewer
    {
        static const char *typestr()
        {
            return "#VSlotViewer";
        }

        const char *gettype() const override final
        {
            return typestr();
        }

        void draw(float sx, float sy) override final
        {
            VSlot &vslot = lookupvslot(index, false);
            previewslot(*vslot.slot, vslot, sx, sy);

            Object::draw(sx, sy);
        }
    };

    //new ui command
    void newui(char *name, char *contents, char *onshow, char *onhide)
    {
        auto itr = windows.find(name);
        if(itr != windows.end())
        {
            if((*itr).second == UI::window)
            {
                return;
            }
            world->hide((*itr).second);
            windows.erase(itr);
            delete window;
        }
        windows[name] = new Window(name, contents, onshow, onhide);
    }

    //command
    void uiallowinput(int *val)
    {
        if(window)
        {
            if(*val >= 0)
            {
                window->allowinput = *val!=0;
            }
            intret(window->allowinput ? 1 : 0);
        }
    }

    //command
    void uieschide (int *val)
    {
        if(window)
        {
            if(*val >= 0)
            {
                window->eschide = *val!=0;
                intret(window->eschide ? 1 : 0);
            }
        }
    }

    bool showui(const char *name)
    {
        auto itr = windows.find(name);
        return (itr != windows.end()) && world->show((*itr).second);
    }

    bool hideui(const char *name)
    {
        if(!name)
        {
            return world->hideall() > 0;
        }
        auto itr = windows.find(name);
        return (itr != windows.end()) && world->hide((*itr).second);
    }

    bool toggleui(const char *name)
    {
        if(showui(name))
        {
            return true;
        }
        hideui(name);
        return false;
    }

    void holdui(const char *name, bool on)
    {
        if(on)
        {
            showui(name);
        }
        else
        {
            hideui(name);
        }
    }

    bool uivisible(const char *name)
    {
        if(!name)
        {
            return world->children.size() > 0;
        }
        auto itr = windows.find(name);
        if(itr != windows.end() && (*itr).second)
        {
            return std::find(world->children.begin(), world->children.end(), (*itr).second) != world->children.end();
        }
        return false;
    }

    void ifstateval(bool state, tagval * t, tagval * f)
    {
        if(state)
        {
            if(t->type == Value_Null)
            {
                intret(1);
            }
            else
            {
                result(*t);
            }
        }
        else if(f->type == Value_Null)
        {
            intret(0);
        }
        else
        {
            result(*f);
        }
    }

    static float parsepixeloffset(const tagval *t, int size)
    {
        switch(t->type)
        {
            case Value_Integer:
            {
                return t->i;
            }
            case Value_Float:
            {
                return t->f;
            }
            case Value_Null:
            {
                return 0;
            }
            default:
            {
                const char *s = t->getstr();
                char *end;
                float val = std::strtod(s, &end);
                return *end == 'p' ? val/size : val;
            }
        }
    }

    static void buildtext(tagval &t, float scale, float scalemod, const Color &color, float wrap, uint *children)
    {
        if(scale <= 0)
        {
            scale = 1;
        }
        scale *= scalemod;
        switch(t.type)
        {
            case Value_Integer:
            {
                BUILD(TextInt, o, o->setup(t.i, scale, color, wrap), children);
                break;
            }
            case Value_Float:
            {
                BUILD(TextFloat, o, o->setup(t.f, scale, color, wrap), children);
                break;
            }
            case Value_CString:
            case Value_Macro:
            case Value_String:
            {
                if(t.s[0])
                {
                    BUILD(TextString, o, o->setup(t.s, scale, color, wrap), children);
                    break;
                }
            }
            default:
            {
                BUILD(Object, o, o->setup(), children);
                break;
            }
        }
    }

    void inituicmds()
    {

        static auto showuicmd = [] (char * name)
        {
            intret(showui(name) ? 1 : 0);
        };

        static auto hideuicmd = [] (char * name)
        {
            intret(hideui(name) ? 1 : 0);
        };

        static auto hidetopuicmd = [] ()
        {
            intret(world->hidetop() ? 1 : 0);
        };

        static auto hidealluicmd = [] ()
        {
            intret(world->hideall());
        };

        static auto toggleuicmd = [] (char * name)
        {
            intret(toggleui(name) ? 1 : 0);
        };

        static auto holduicmd = [] (char * name, int * down)
        {
            holdui(name, *down!=0);
        };

        static auto uivisiblecmd = [] (char * name)
        {
            intret(uivisible(name) ? 1 : 0);
        };

        addcommand("showui",        reinterpret_cast<identfun>(+showuicmd),   "s",    Id_Command);
        addcommand("hideui",        reinterpret_cast<identfun>(+hideuicmd),   "s",    Id_Command);
        addcommand("hidetopui",     reinterpret_cast<identfun>(+hidetopuicmd),"",     Id_Command);
        addcommand("hideallui",     reinterpret_cast<identfun>(+hidealluicmd),"",     Id_Command);
        addcommand("toggleui",      reinterpret_cast<identfun>(+toggleuicmd), "s",    Id_Command);
        addcommand("holdui",        reinterpret_cast<identfun>(+holduicmd),   "sD",   Id_Command);
        addcommand("uivisible",     reinterpret_cast<identfun>(+uivisiblecmd),"s",    Id_Command);

        static auto uinamecmd = [] ()
        {
            if(window)
            {
                result(window->name);
            }
        };
        addcommand("uiname",        reinterpret_cast<identfun>(+uinamecmd),   "",     Id_Command);

        #define DOSTATE(flags, func) \
            addcommand("ui" #func, reinterpret_cast<identfun>(+[] (uint *t, uint *f) { executeret(buildparent && buildparent->haschildstate(flags) ? t : f); }), "ee", Id_Command); \
            addcommand("ui" #func "?", reinterpret_cast<identfun>(+[] (tagval *t, tagval *f) { ifstateval(buildparent && buildparent->haschildstate(flags), t, f); }), "tt", Id_Command); \
            addcommand("ui" #func "+", reinterpret_cast<identfun>(+[] (uint *t, uint *f) { executeret(buildparent && static_cast<int>(buildparent->children.size()) > buildchild && buildparent->children[buildchild]->haschildstate(flags) ? t : f); }), "ee", Id_Command); \
            addcommand("ui" #func "+?", reinterpret_cast<identfun>(+[] (tagval *t, tagval *f) { ifstateval(buildparent && static_cast<int>(buildparent->children.size()) > buildchild && buildparent->children[buildchild]->haschildstate(flags), t, f); }), "tt", Id_Command);
        DOSTATES
        #undef DOSTATE

        addcommand("uifocus", reinterpret_cast<identfun>(+[] (uint *t, uint *f) { executeret(buildparent && TextEditor::focus == buildparent ? t : f); }), "ee", Id_Command);
        addcommand("uifocus?", reinterpret_cast<identfun>(+[] (tagval *t, tagval *f) { ifstateval(buildparent && TextEditor::focus == buildparent, t, f); }), "tt", Id_Command);
        addcommand("uifocus+", reinterpret_cast<identfun>(+[] (uint *t, uint *f) { executeret(buildparent && static_cast<int>(buildparent->children.size()) > buildchild && TextEditor::focus == buildparent->children[buildchild] ? t : f); }), "ee", Id_Command);
        addcommand("uifocus+?", reinterpret_cast<identfun>(+[] (tagval *t, tagval *f) { ifstateval(buildparent && static_cast<int>(buildparent->children.size()) > buildchild && TextEditor::focus == buildparent->children[buildchild], t, f); }), "tt", Id_Command);
        addcommand("uialign", reinterpret_cast<identfun>(+[] (int *xalign, int *yalign)
        {
            {
                if(buildparent)
                {
                    buildparent->setalign(*xalign, *yalign);
                }
            };
        }), "ii", Id_Command);
        addcommand("uialign-", reinterpret_cast<identfun>(+[] (int *xalign, int *yalign)
        {
            {
                if(buildparent && buildchild > 0)
                {
                    buildparent->children[buildchild-1]->setalign(*xalign, *yalign);
                }
            };
        }), "ii", Id_Command);
        addcommand("uialign*", reinterpret_cast<identfun>(+[] (int *xalign, int *yalign)
        {
            {
                if(buildparent)
                {
                    for(int i = 0; i < buildchild; ++i)
                    {
                        buildparent->children[i]->setalign(*xalign, *yalign);
                    }
                }
            };
        }), "ii", Id_Command);
        addcommand("uiclamp", reinterpret_cast<identfun>(+[] (int *left, int *right, int *top, int *bottom) { { if(buildparent) { buildparent->setclamp(*left, *right, *top, *bottom); } }; }), "iiii", Id_Command);
        addcommand("uiclamp-", reinterpret_cast<identfun>(+[] (int *left, int *right, int *top, int *bottom) { { if(buildparent && buildchild > 0) { buildparent->children[buildchild-1]->setclamp(*left, *right, *top, *bottom); } }; }), "iiii", Id_Command);
        addcommand("uiclamp*", reinterpret_cast<identfun>(+[] (int *left, int *right, int *top, int *bottom) { { if(buildparent) { for(int i = 0; i < buildchild; ++i) { buildparent->children[i]->setclamp(*left, *right, *top, *bottom); } } }; }), "iiii", Id_Command);
        addcommand("uigroup", reinterpret_cast<identfun>(+[] (uint *children) { BUILD(Object, o, o->setup(), children); }), "e", Id_Command);
        addcommand("uihlist", reinterpret_cast<identfun>(+[] (float *space, uint *children) { BUILD(HorizontalList, o, o->setup(*space), children); }), "fe", Id_Command);
        addcommand("uivlist", reinterpret_cast<identfun>(+[] (float *space, uint *children) { BUILD(VerticalList, o, o->setup(*space), children); }), "fe", Id_Command);
        addcommand("uilist", reinterpret_cast<identfun>(+[] (float *space, uint *children) { { for(Object *parent = buildparent; parent && !parent->istype<VerticalList>(); parent = parent->parent) { if(parent->istype<HorizontalList>()) { BUILD(VerticalList, o, o->setup(*space), children); return; } } BUILD(HorizontalList, o, o->setup(*space), children); }; }), "fe", Id_Command);
        addcommand("uigrid", reinterpret_cast<identfun>(+[] (int *columns, float *spacew, float *spaceh, uint *children) { BUILD(Grid, o, o->setup(*columns, *spacew, *spaceh), children); }), "iffe", Id_Command);
        addcommand("uitableheader", reinterpret_cast<identfun>(+[] (uint *columndata, uint *children) { BUILDCOLUMNS(TableHeader, o, o->setup(), columndata, children); }), "ee", Id_Command);
        addcommand("uitablerow", reinterpret_cast<identfun>(+[] (uint *columndata, uint *children) { BUILDCOLUMNS(TableRow, o, o->setup(), columndata, children); }), "ee", Id_Command);
        addcommand("uitable", reinterpret_cast<identfun>(+[] (float *spacew, float *spaceh, uint *children) { BUILD(Table, o, o->setup(*spacew, *spaceh), children); }), "ffe", Id_Command);
        addcommand("uispace", reinterpret_cast<identfun>(+[] (float *spacew, float *spaceh, uint *children) { BUILD(Spacer, o, o->setup(*spacew, *spaceh), children); }), "ffe", Id_Command);
        addcommand("uioffset", reinterpret_cast<identfun>(+[] (float *offsetx, float *offsety, uint *children) { BUILD(Offsetter, o, o->setup(*offsetx, *offsety), children); }), "ffe", Id_Command);
        addcommand("uifill", reinterpret_cast<identfun>(+[] (float *minw, float *minh, uint *children) { BUILD(Filler, o, o->setup(*minw, *minh), children); }), "ffe", Id_Command);
        addcommand("uitarget", reinterpret_cast<identfun>(+[] (float *minw, float *minh, uint *children) { BUILD(Target, o, o->setup(*minw, *minh), children); }), "ffe", Id_Command);
        addcommand("uiclip", reinterpret_cast<identfun>(+[] (float *clipw, float *cliph, uint *children) { BUILD(Clipper, o, o->setup(*clipw, *cliph), children); }), "ffe", Id_Command);
        addcommand("uiscroll", reinterpret_cast<identfun>(+[] (float *clipw, float *cliph, uint *children) { BUILD(Scroller, o, o->setup(*clipw, *cliph), children); }), "ffe", Id_Command);
        addcommand("uihscrolloffset", reinterpret_cast<identfun>(+[] () { { if(buildparent && buildparent->istype<Scroller>()) { Scroller *scroller = static_cast<Scroller *>(buildparent); floatret(scroller->offsetx); } }; }), "", Id_Command);
        addcommand("uivscrolloffset", reinterpret_cast<identfun>(+[] () { { if(buildparent && buildparent->istype<Scroller>()) { Scroller *scroller = static_cast<Scroller *>(buildparent); floatret(scroller->offsety); } }; }), "", Id_Command);
        addcommand("uihscrollbar", reinterpret_cast<identfun>(+[] (uint *children) { BUILD(HorizontalScrollBar, o, o->setup(), children); }), "e", Id_Command);
        addcommand("uivscrollbar", reinterpret_cast<identfun>(+[] (uint *children) { BUILD(VerticalScrollBar, o, o->setup(), children); }), "e", Id_Command);
        addcommand("uiscrollarrow", reinterpret_cast<identfun>(+[] (float *dir, uint *children) { BUILD(ScrollArrow, o, o->setup(*dir), children); }), "fe", Id_Command);
        addcommand("uiscrollbutton", reinterpret_cast<identfun>(+[] (uint *children) { BUILD(ScrollButton, o, o->setup(), children); }), "e", Id_Command);
        addcommand("uihslider", reinterpret_cast<identfun>(+[] (ident *var, float *vmin, float *vmax, float *vstep, uint *onchange, uint *children) { BUILD(HorizontalSlider, o, o->setup(var, *vmin, *vmax, *vstep, onchange), children); }), "rfffee", Id_Command);
        addcommand("uivslider", reinterpret_cast<identfun>(+[] (ident *var, float *vmin, float *vmax, float *vstep, uint *onchange, uint *children) { BUILD(VerticalSlider, o, o->setup(var, *vmin, *vmax, *vstep, onchange), children); }), "rfffee", Id_Command);
        addcommand("uisliderarrow", reinterpret_cast<identfun>(+[] (float *dir, uint *children) { BUILD(SliderArrow, o, o->setup(*dir), children); }), "fe", Id_Command);
        addcommand("uisliderbutton", reinterpret_cast<identfun>(+[] (uint *children) { BUILD(SliderButton, o, o->setup(), children); }), "e", Id_Command);
        addcommand("uicolor", reinterpret_cast<identfun>(+[] (int *c, float *minw, float *minh, uint *children) { BUILD(FillColor, o, o->setup(FillColor::SOLID, Color(*c), *minw, *minh), children); }), "iffe", Id_Command);
        addcommand("uimodcolor", reinterpret_cast<identfun>(+[] (int *c, float *minw, float *minh, uint *children) { BUILD(FillColor, o, o->setup(FillColor::MODULATE, Color(*c), *minw, *minh), children); }), "iffe", Id_Command);
        addcommand("uivgradient", reinterpret_cast<identfun>(+[] (int *c, int *c2, float *minw, float *minh, uint *children) { BUILD(Gradient, o, o->setup(Gradient::SOLID, Gradient::VERTICAL, Color(*c), Color(*c2), *minw, *minh), children); }), "iiffe", Id_Command);
        addcommand("uimodvgradient", reinterpret_cast<identfun>(+[] (int *c, int *c2, float *minw, float *minh, uint *children) { BUILD(Gradient, o, o->setup(Gradient::MODULATE, Gradient::VERTICAL, Color(*c), Color(*c2), *minw, *minh), children); }), "iiffe", Id_Command);
        addcommand("uihgradient", reinterpret_cast<identfun>(+[] (int *c, int *c2, float *minw, float *minh, uint *children) { BUILD(Gradient, o, o->setup(Gradient::SOLID, Gradient::HORIZONTAL, Color(*c), Color(*c2), *minw, *minh), children); }), "iiffe", Id_Command);
        addcommand("uimodhgradient", reinterpret_cast<identfun>(+[] (int *c, int *c2, float *minw, float *minh, uint *children) { BUILD(Gradient, o, o->setup(Gradient::MODULATE, Gradient::HORIZONTAL, Color(*c), Color(*c2), *minw, *minh), children); }), "iiffe", Id_Command);
        addcommand("uioutline", reinterpret_cast<identfun>(+[] (int *c, float *minw, float *minh, uint *children) { BUILD(Outline, o, o->setup(Color(*c), *minw, *minh), children); }), "iffe", Id_Command);
        addcommand("uiline", reinterpret_cast<identfun>(+[] (int *c, float *minw, float *minh, uint *children) { BUILD(Line, o, o->setup(Color(*c), *minw, *minh), children); }), "iffe", Id_Command);
        addcommand("uitriangle", reinterpret_cast<identfun>(+[] (int *c, float *minw, float *minh, int *angle, uint *children) { BUILD(Triangle, o, o->setup(Color(*c), *minw, *minh, *angle, Triangle::SOLID), children); }), "iffie", Id_Command);
        addcommand("uitriangleoutline", reinterpret_cast<identfun>(+[] (int *c, float *minw, float *minh, int *angle, uint *children) { BUILD(Triangle, o, o->setup(Color(*c), *minw, *minh, *angle, Triangle::OUTLINE), children); }), "iffie", Id_Command);
        addcommand("uimodtriangle", reinterpret_cast<identfun>(+[] (int *c, float *minw, float *minh, int *angle, uint *children) { BUILD(Triangle, o, o->setup(Color(*c), *minw, *minh, *angle, Triangle::MODULATE), children); }), "iffie", Id_Command);
        addcommand("uicircle", reinterpret_cast<identfun>(+[] (int *c, float *size, uint *children) { BUILD(Circle, o, o->setup(Color(*c), *size, Circle::SOLID), children); }), "ife", Id_Command);
        addcommand("uicircleoutline", reinterpret_cast<identfun>(+[] (int *c, float *size, uint *children) { BUILD(Circle, o, o->setup(Color(*c), *size, Circle::OUTLINE), children); }), "ife", Id_Command);
        addcommand("uimodcircle", reinterpret_cast<identfun>(+[] (int *c, float *size, uint *children) { BUILD(Circle, o, o->setup(Color(*c), *size, Circle::MODULATE), children); }), "ife", Id_Command);
        addcommand("uicolortext", reinterpret_cast<identfun>(+[] (tagval *text, int *c, float *scale, uint *children) { buildtext(*text, *scale, uitextscale, Color(*c), -1, children); }), "tife", Id_Command);
        addcommand("uitext", reinterpret_cast<identfun>(+[] (tagval *text, float *scale, uint *children)
        {
            buildtext(*text, *scale, uitextscale, Color(255, 255, 255), -1, children);
        }), "tfe", Id_Command);
        addcommand("uitextfill", reinterpret_cast<identfun>(+[] (float *minw, float *minh, uint *children) { BUILD(Filler, o, o->setup(*minw * uitextscale*0.5f, *minh * uitextscale), children); }), "ffe", Id_Command);
        addcommand("uiwrapcolortext", reinterpret_cast<identfun>(+[] (tagval *text, float *wrap, int *c, float *scale, uint *children) { buildtext(*text, *scale, uitextscale, Color(*c), *wrap, children); }), "tfife", Id_Command);
        addcommand("uiwraptext", reinterpret_cast<identfun>(+[] (tagval *text, float *wrap, float *scale, uint *children) { buildtext(*text, *scale, uitextscale, Color(255, 255, 255), *wrap, children); }), "tffe", Id_Command);
        addcommand("uicolorcontext", reinterpret_cast<identfun>(+[] (tagval *text, int *c, float *scale, uint *children) { buildtext(*text, *scale, FONTH*uicontextscale, Color(*c), -1, children); }), "tife", Id_Command);
        addcommand("uicontext", reinterpret_cast<identfun>(+[] (tagval *text, float *scale, uint *children) { buildtext(*text, *scale, FONTH*uicontextscale, Color(255, 255, 255), -1, children); }), "tfe", Id_Command);
        addcommand("uicontextfill", reinterpret_cast<identfun>(+[] (float *minw, float *minh, uint *children) { BUILD(Filler, o, o->setup(*minw * FONTH*uicontextscale*0.5f, *minh * FONTH*uicontextscale), children); }), "ffe", Id_Command);
        addcommand("uiwrapcolorcontext", reinterpret_cast<identfun>(+[] (tagval *text, float *wrap, int *c, float *scale, uint *children) { buildtext(*text, *scale, FONTH*uicontextscale, Color(*c), *wrap, children); }), "tfife", Id_Command);
        addcommand("uiwrapcontext", reinterpret_cast<identfun>(+[] (tagval *text, float *wrap, float *scale, uint *children) { buildtext(*text, *scale, FONTH*uicontextscale, Color(255, 255, 255), *wrap, children); }), "tffe", Id_Command);
        addcommand("uitexteditor", reinterpret_cast<identfun>(+[] (char *name, int *length, int *height, float *scale, char *initval, int *mode, uint *children) { BUILD(TextEditor, o, o->setup(name, *length, *height, (*scale <= 0 ? 1 : *scale) * uitextscale, initval, *mode <= 0 ? Editor_Forever : *mode), children); }), "siifsie", Id_Command);
        addcommand("uifont", reinterpret_cast<identfun>(+[] (char *name, uint *children) { BUILD(Font, o, o->setup(name), children); }), "se", Id_Command);
        addcommand("uiabovehud", reinterpret_cast<identfun>(+[] () { { if(window) window->abovehud = true; }; }), "", Id_Command);;
        addcommand("uiconsole", reinterpret_cast<identfun>(+[] (float *minw, float *minh, uint *children) { BUILD(Console, o, o->setup(*minw, *minh), children); }), "ffe", Id_Command);
        addcommand("uifield", reinterpret_cast<identfun>(+[] (ident *var, int *length, uint *onchange, float *scale, uint *children) { BUILD(Field, o, o->setup(var, *length, onchange, (*scale <= 0 ? 1 : *scale) * uitextscale), children); }), "riefe", Id_Command);
        addcommand("uikeyfield", reinterpret_cast<identfun>(+[] (ident *var, int *length, uint *onchange, float *scale, uint *children) { BUILD(KeyField, o, o->setup(var, *length, onchange, (*scale <= 0 ? 1 : *scale) * uitextscale), children); }), "riefe", Id_Command);
        addcommand("uiimage", reinterpret_cast<identfun>(+[] (char *texname, float *minw, float *minh, uint *children) { BUILD(Image, o, o->setup(textureload(texname, 3, true, false), *minw, *minh), children); }), "sffe", Id_Command);
        addcommand("uistretchedimage", reinterpret_cast<identfun>(+[] (char *texname, float *minw, float *minh, uint *children) { BUILD(StretchedImage, o, o->setup(textureload(texname, 3, true, false), *minw, *minh), children); }), "sffe", Id_Command);
        addcommand("uicroppedimage", reinterpret_cast<identfun>(+[] (char *texname, float *minw, float *minh, tagval *cropx, tagval *cropy, tagval *cropw, tagval *croph, uint *children) { BUILD(CroppedImage, o, { Texture *tex = textureload(texname, 3, true, false); o->setup(tex, *minw, *minh, parsepixeloffset(cropx, tex->xs), parsepixeloffset(cropy, tex->ys), parsepixeloffset(cropw, tex->xs), parsepixeloffset(croph, tex->ys)); }, children); }), "sfftttte", Id_Command);
        addcommand("uiborderedimage", reinterpret_cast<identfun>(+[] (char *texname, tagval *texborder, float *screenborder, uint *children) { BUILD(BorderedImage, o, { Texture *tex = textureload(texname, 3, true, false); o->setup(tex, parsepixeloffset(texborder, tex->xs), *screenborder); }, children); }), "stfe", Id_Command);
        addcommand("uitiledimage", reinterpret_cast<identfun>(+[] (char *texname, float *tilew, float *tileh, float *minw, float *minh, uint *children) { BUILD(TiledImage, o, { Texture *tex = textureload(texname, 0, true, false); o->setup(tex, *minw, *minh, *tilew <= 0 ? 1 : *tilew, *tileh <= 0 ? 1 : *tileh); }, children); }), "sffffe", Id_Command);
        addcommand("uimodelpreview", reinterpret_cast<identfun>(+[] (char *model, char *animspec, float *minw, float *minh, uint *children) { BUILD(ModelPreview, o, o->setup(model, animspec, *minw, *minh), children); }), "ssffe", Id_Command);
        addcommand("uiprefabpreview", reinterpret_cast<identfun>(+[] (char *prefab, int *color, float *minw, float *minh, uint *children) { BUILD(PrefabPreview, o, o->setup(prefab, *color, *minw, *minh), children); }), "siffe", Id_Command);
        addcommand("uislotview", reinterpret_cast<identfun>(+[] (int *index, float *minw, float *minh, uint *children) { BUILD(SlotViewer, o, o->setup(*index, *minw, *minh), children); }), "iffe", Id_Command);
        addcommand("uivslotview", reinterpret_cast<identfun>(+[] (int *index, float *minw, float *minh, uint *children) { BUILD(VSlotViewer, o, o->setup(*index, *minw, *minh), children); }), "iffe", Id_Command);

        addcommand("uicontextscale", reinterpret_cast<identfun>(uicontextscalecmd), "", Id_Command);
        addcommand("newui", reinterpret_cast<identfun>(newui), "ssss", Id_Command);
        addcommand("uiallowinput", reinterpret_cast<identfun>(uiallowinput), "b", Id_Command);
        addcommand("uieschide", reinterpret_cast<identfun>(uieschide), "b", Id_Command);
    }

    bool hascursor()
    {
        return world->allowinput();
    }

    void getcursorpos(float &x, float &y)
    {
        if(hascursor())
        {
            x = cursorx;
            y = cursory;
        }
        else
        {
            x = y = 0.5f;
        }
    }

    void resetcursor()
    {
        cursorx = cursory = 0.5f;
    }

    FVARP(uisensitivity, 1e-4f, 1, 1e4f);

    bool movecursor(int dx, int dy)
    {
        if(!hascursor())
        {
            return false;
        }
        cursorx = std::clamp(cursorx + dx*uisensitivity/hudw(), 0.0f, 1.0f);
        cursory = std::clamp(cursory + dy*uisensitivity/hudh(), 0.0f, 1.0f);
        return true;
    }

    bool keypress(int code, bool isdown)
    {
        if(world->rawkey(code, isdown))
        {
            return true;
        }
        int action = 0,
            hold = 0;
        switch(code)
        {
            case Key_Left:
            {
                action = isdown ? State_Press : State_Release; hold = State_Hold;
                break;
            }
            case Key_Middle:
            {
                action = isdown ? State_AltPress : State_AltRelease; hold = State_AltHold;
                break;
            }
            case Key_Right:
            {
                action = isdown ? State_EscPress : State_EscRelease; hold = State_EscHold;
                break;
            }
            case Key_ScrollUp:
            {
                action = State_ScrollUp;
                break;
            }
            case Key_ScrollDown:
            {
                action = State_ScrollDown;
                break;
            }
        }
        if(action)
        {
            if(isdown)
            {
                if(hold)
                {
                    world->clearstate(hold);
                }
                if(world->setstate(action, cursorx, cursory, 0, true, action|hold))
                {
                    return true;
                }
            }
            else if(hold)
            {
                if(world->setstate(action, cursorx, cursory, hold, true, action))
                {
                    world->clearstate(hold);
                    return true;
                }
                world->clearstate(hold);
            }
            return world->allowinput();
        }
        return world->key(code, isdown);
    }

    bool textinput(const char *str, int len)
    {
        return world->textinput(str, len);
    }

    void setup()
    {
        world = new World;
    }

    void cleanup()
    {
        world->children.clear();
        for(auto &[k, i] : windows)
        {
            delete i;
        }
        windows.clear();
        if(world)
        {
            delete world;
            world = nullptr;
        }
    }

    void calctextscale()
    {
        uitextscale = 1.0f/uitextrows;

        int tw = hudw(),
            th = hudh();
        if(forceaspect)
        {
            tw = static_cast<int>(std::ceil(th*forceaspect));
        }
        gettextres(tw, th);
        uicontextscale = conscale/th;
    }

    void update()
    {
        readyeditors();

        world->setstate(State_Hover, cursorx, cursory, world->childstate & State_HoldMask);
        if(world->childstate & State_Hold)
        {
            world->setstate(State_Hold, cursorx, cursory, State_Hold, false);
        }
        if(world->childstate & State_AltHold)
        {
            world->setstate(State_AltHold, cursorx, cursory, State_AltHold, false);
        }
        if(world->childstate & State_EscHold)
        {
            world->setstate(State_EscHold, cursorx, cursory, State_EscHold, false);
        }

        calctextscale();
        world->build();
        flusheditors();
    }

    void render()
    {
        world->layout();
        world->adjustchildren();
        world->draw();
    }

    float abovehud()
    {
        return world->abovehud();
    }
}

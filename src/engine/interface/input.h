
#ifndef INPUT_H_
#define INPUT_H_

extern void keyrepeat(bool on, int mask = ~0);

enum
{
    TextInput_Console = 1<<0,
    TextInput_GUI     = 1<<1,
};

enum
{
    Key_Left       = -1,
    Key_Middle     = -2,
    Key_Right      = -3,
    Key_ScrollUp   = -4,
    Key_ScrollDown = -5,
    Key_X1         = -6,
    Key_X2         = -7,
};

extern void textinput(bool on, int mask = ~0);

extern bool grabinput, minimized;

extern void inputgrab(bool on);
extern bool interceptkey(int sym);
extern void pushevent(const SDL_Event &e);
#endif

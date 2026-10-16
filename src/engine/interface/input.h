
#ifndef INPUT_H_
#define INPUT_H_

/**
 * @brief Manipulates the key repeat mask.
 *
 * If on is true, sets keyrepeat mask to the bitwise OR sum of the mask and the
 * keyrepeat mask. Otherwise sets the mask to the values that the keyrepeat mask
 * has that the mask value does not.
 *
 * @param on sets mask modification mode
 * @param mask the mask value to set
 */
extern void keyrepeat(bool on, int mask = ~0);

/**
 * @brief Enumerates location of text input.
 *
 * This will determine whether the input is directed to a ui window or a console window.
 */
enum
{
    TextInput_Console = 1<<0, //used in console.cpp
    TextInput_GUI     = 1<<1, //used in ui.cpp
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
#endif

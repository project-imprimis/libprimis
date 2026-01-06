#ifndef MENUS_H_
#define MENUS_H_

extern int mainmenu; /// boolean-like value whether the main menu UI is being shown

/**
 * @brief Adds a change to the vector queue of settings changes.
 *
 * if applydialog = 0 then this function does nothing
 * if showchanges = 1 then this function does not display changes UI at the end
 */
extern void addchange(const char *desc, int type);

/**
 * @brief Clears out pending changes added by addchange()
 *
 * @param type the mask of the changes to clear
 */
extern void clearchanges(int type);
extern void clearmainmenu();

#endif

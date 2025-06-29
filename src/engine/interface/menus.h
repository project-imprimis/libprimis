#ifndef MENUS_H_
#define MENUS_H_

extern int mainmenu;

/**
 * @brief Adds a change to the vector queue of settings changes.
 *
 * if applydialog = 0 then this function does nothing
 * if showchanges = 1 then this function does not display changes UI at the end
 */
extern void addchange(const char *desc, int type);

//clears out pending changes added by addchange()
extern void clearchanges(int type);
extern void clearmainmenu();

#endif

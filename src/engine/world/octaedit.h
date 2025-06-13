#ifndef OCTAEDIT_H_
#define OCTAEDIT_H_

extern std::vector<ushort> texmru;
extern bool allowediting;
extern bool multiplayer;
extern bool editmode;
extern selinfo sel;
extern int nompedit;

extern ivec cur;
extern bool havesel;
extern int gridpower, gridsize;
extern int orient;

extern void makeundo(selinfo &s);
extern void forcenextundo();
extern void multiplayerwarn();

/**
 * @brief Cancels cube and entity selections,
 *
 * Deselects entire selection context, including cube and ent selections.
 */
extern void cancelsel();
extern void rendertexturepanel(int w, int h);
extern bool noedit(bool view = false, bool msg = true);

/**
 * @brief Compacts most recently used vslots list.
 *
 * Deletes all elements in remappedvslots.
 * Goes through most recently used textures list, and removes trailing elements
 * unless the most recently used texture index is lower than the number of total
 * elements and the vslot at that index is nonzero. If this is the case, reassigns
 * the texmru to the slot pointed to by the object to be cleaned up.
 *
 * Also attempts to set lasttex to its pointed index in a similar manner.
 *
 * Attempts to set reptex to the index pointed to by its previous manner in a
 * similar manner.
 */
extern void compactmruvslots();

extern void previewprefab(const char *name, const vec &color);

/**
 * @brief Calls the cleanup function for all prefabs.
 *
 * Does not destroy elements in the prefabs container.
 */
extern void cleanupprefabs();

/**
 * @brief Cleans up undo blocks until less than `maxremain` memory is bound
 *
 * Cleans up undoblocks, front to back. Stops when the total number of undos is
 * less than maxremain. Then, cleans up all redos, such that if any redos exist
 * the end result will be less total undos than the passed maxremain.
 *
 * @param maxremain the max number of undos to retain
 */
extern void pruneundos(int maxremain = 0);

#endif

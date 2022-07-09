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
extern void cancelsel();
extern void rendertexturepanel(int w, int h);
extern bool noedit(bool view = false, bool msg = true);

extern void compactmruvslots();

extern void previewprefab(const char *name, const vec &color);
extern void cleanupprefabs();

struct editinfo;
extern void pruneundos(int maxremain = 0);
extern bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf);

#endif

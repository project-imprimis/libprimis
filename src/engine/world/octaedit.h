#ifndef OCTAEDIT_H_
#define OCTAEDIT_H_

extern vector<ushort> texmru;
extern bool allowediting;
extern bool multiplayer;
extern bool editmode;
extern selinfo sel;

extern void multiplayerwarn();
extern void cancelsel();
extern void rendertexturepanel(int w, int h);
extern void commitchanges(bool force = false);
extern void changed(const ivec &bbmin, const ivec &bbmax, bool commit = true);
extern void rendereditcursor();
extern bool noedit(bool view = false, bool msg = true);

extern void compactmruvslots();

extern void previewprefab(const char *name, const vec &color);
extern void cleanupprefabs();

struct editinfo;
extern editinfo *localedit;
extern void pruneundos(int maxremain = 0);
extern bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf);

#endif

#ifndef RENDERWINDOW_H_
#define RENDERWINDOW_H_

extern int fullscreen;
extern void setupscreen();
extern void restoregamma();
extern void restorevsync();
extern void resetfpshistory();
extern void limitfps(int &millis, int curmillis);
extern void updatefpshistory(int millis);

extern void renderbackground(const char *caption = NULL, Texture *mapshot = NULL, const char *mapname = NULL, const char *mapinfo = NULL, bool force = false);
extern void renderprogress(float bar, const char *text, bool background = false);

#endif

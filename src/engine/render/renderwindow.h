#ifndef RENDERWINDOW_H_
#define RENDERWINDOW_H_

extern int fullscreen;
extern int hudw, hudh;

extern SDL_Window *screen;
extern int scr_w, scr_h;
extern int screenw, screenh;

extern void setupscreen();
extern void restoregamma();
extern void restorevsync();
extern void resetfpshistory();
extern void limitfps(int &millis, int curmillis);
extern void updatefpshistory(int millis);

extern void renderbackground(const char *caption = nullptr, Texture *mapshot = nullptr, const char *mapname = nullptr, const char *mapinfo = nullptr, bool force = false);
extern void renderprogress(float bar, const char *text, bool background = false);

#endif

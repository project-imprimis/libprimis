#ifndef RENDERWINDOW_H_
#define RENDERWINDOW_H_

extern int fullscreen;

extern SDL_Window *screen;
extern int scr_w, scr_h;
extern int screenw, screenh;
extern float loadprogress;

/**
 * @brief Restores gamma to default level
 *
 * Uses SDL to set gamma level to 1.0f
 */
extern void cleargamma();

/**
 * @brief Retrieves fps info and diffs,
 *
 * Returns statistics about the frame rate to the passed parameters
 *
 * @param fps average fps
 * @param bestdiff difference between average and best fps
 * @param worstdiff difference between average and worst fps
 */
extern void getfps(int &fps, int &bestdiff, int &worstdiff);

extern void renderbackground(const char *caption = nullptr, const Texture *mapshot = nullptr, const char *mapname = nullptr, const char *mapinfo = nullptr, bool force = false);
extern void renderprogress(float bar, const char *text, bool background = false);

#endif

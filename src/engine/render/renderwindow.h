#ifndef RENDERWINDOW_H_
#define RENDERWINDOW_H_

extern SDL_Window *screen; /// handle to SDL object that manages the window
extern int scr_w, /// width of window in pixels, as requested by user via cubescript
           scr_h; /// height of window in pixels, as requested by user via cubescript
extern int screenw, /// width of window in pixels, queried from SDL
           screenh; /// height of window in pixels, queried from SDL
extern float loadprogress; /// value between 0 and 1, sets size of progress loading bar
extern bool inbetweenframes; /// returns true when renderer is not in the octarender() call, false if it is

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

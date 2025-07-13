#ifndef AO_H_
#define AO_H_

extern int aow, aoh;
extern std::array<GLuint, 4> aotex;
extern float aomin, aosunmin;
extern int ao, aosun, aopackdepth, aoreduce, aoreducedepth;
extern int aobilateral, aobilateralupscale;

/**
 * @brief sets the ao buffer depth format flag
 *
 * Sets the AO buffer depth format flag, depending on the aofloatdepth variable.
 * If aofloatdepth is enabled, use its value, otherwise use 0.
 * aofloatdepth represents a GL format.
 */
extern void initao();
extern void cleanupao();

/** @brief displays the raw output of the ao buffer, useful for debugging
 *
 * Displays the AO buffer either fullscreen (if debugfullscreen is 1) or corner
 * of screen. The frame drawn will be red, with darkening of the debug buffer
 * where the darkening of the scene is
 */
extern void viewao();
extern void setupao(int w, int h);

#endif

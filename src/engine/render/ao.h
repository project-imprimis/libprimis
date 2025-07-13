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
extern void viewao();
extern void setupao(int w, int h);

#endif

#ifndef RENDERTIMERS_H_
#define RENDERTIMERS_H_
struct timer;

/**
 * @brief activates a timer that starts its query from a given point in the code
 *
 * Creates a new timer if necessary.
 *
 * @param name The name of the timer to use
 * @param gpu Toggles timing GPU rendering time
 *
 * @return a pointer to the relevant timer
 */
extern timer *begintimer(const char *name, bool gpu = true);
extern void endtimer(timer *t);

/**
 * @brief draws timers to the screen using hardcoded text
 *
 * If frametimer gvar is enabled, also shows the overall frame time;
 * otherwise, prints out all timer information available
 *
 * @param conw console width to draw into
 * @param framemillis frame time inside the current frame
 */
extern void printtimers(int conw, int framemillis);
extern int frametimer;
extern void synctimers();

/**
 * @brief deletes the elements in the timers global vector
 *
 * Deletes the elements in the `timer` global variable. If any GPU queries are active,
 * they are cancelled so as not to waste the GPU's time
 */
extern void cleanuptimers();

#endif

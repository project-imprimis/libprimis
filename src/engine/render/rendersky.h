#ifndef RENDERSKY_H_
#define RENDERSKY_H_

extern int skyshadow;

extern void setexplicitsky(bool val);
extern void drawskybox(bool clear = false);

/**
 * @brief Returns status of sky rendering.
 *
 * Does not affect state of the program.
 *
 * @return true if explicit sky is enabled, and skytexture is being used or edit is enabled
 * @return false otherwise
 */
extern bool limitsky();

#endif

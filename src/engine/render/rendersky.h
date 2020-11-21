#ifndef RENDERSKY_H_
#define RENDERSKY_H_

extern int skytexture, skyshadow, explicitsky;

extern void drawskybox(bool clear = false);
extern bool limitsky();
extern bool renderexplicitsky(bool outline = false);

#endif

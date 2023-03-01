#ifndef RENDERSKY_H_
#define RENDERSKY_H_

extern int skyshadow;
extern bool explicitsky;

extern void drawskybox(bool clear = false);
extern bool limitsky();

#endif

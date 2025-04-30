#ifndef RENDERSKY_H_
#define RENDERSKY_H_

extern int skyshadow;

extern void setexplicitsky(bool val);
extern void drawskybox(bool clear = false);
extern bool limitsky();

#endif

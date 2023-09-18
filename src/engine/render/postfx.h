#ifndef POSTFX_H_
#define POSTFX_H_

class GBuffer;
extern void cleanuppostfx(bool fullclean = false);
extern GLuint setuppostfx(const GBuffer &buf, int w, int h, GLuint outfbo = 0);
extern void renderpostfx(GLuint outfbo = 0);

#endif

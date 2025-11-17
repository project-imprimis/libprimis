#ifndef HDR_H_
#define HDR_H_

extern float hdrgamma;
extern GLuint hdrfbo, hdrtex;
extern int bloomw, bloomh;
extern int hdrprec;
extern GLenum hdrformat;

/**
 * @brief Gets the HDR texture format associated with `prec`.
 *
 * If prec is below 1, returns `fallback`.
 * If prec is 1, 2, or 3+, returns GL_RGB10, GL_R11F_G11F_B10F, or GL_RGB16F respectively.
 *
 * @param prec the precision indicator for the HDR tex
 * @param fallback value to return if prec < 0
 *
 * @return a GL texture format number
 */
extern int  gethdrformat(int prec, int fallback = GL_RGB);
extern void cleanupbloom();

/**
 * @brief Sets up bloom with specified size.
 *
 * Will attempt to make a set of bloom buffers of size w pixels by h pixels. Uses texture
 * type set by gethdrformat() and hdrprec variable. There are six FBOs and textures
 * stored in bloomtex/bloomfbo which are modified by this setup operation.
 *
 * Size of the bloom buffers vary; the first two bloom buffers are at least half
 * and a quarter the size of g buffer size, respectively.
 *
 * The third and fourth bloom buffers ([2] [3]) are always the same size, and
 * are bounded by either w/h or the bloomsize variable. (The bloomsize variable
 * sets the maximum size of the bloom buffer as the exponent in a power of 2, e.g.
 * 10 = 2^10 = 1024)
 *
 * @param w width of bloom buffer
 * @param h height of bloom buffer
 */
extern void setupbloom(int w, int h);
extern void loadhdrshaders(int aa);
extern void copyhdr(int sw, int sh, GLuint fbo);

#endif

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
 * Size of the bl
 * @param w width of bloom buffer
 * @param h height of bloom buffer
 */
extern void setupbloom(int w, int h);
extern void loadhdrshaders(int aa);
extern void copyhdr(int sw, int sh, GLuint fbo);

#endif

#ifndef HDR_H_
#define HDR_H_

extern float hdrgamma;
extern GLuint hdrfbo, hdrtex;
extern int bloomw, bloomh;
extern int hdrprec;
extern GLenum hdrformat;

extern int  gethdrformat(int prec, int fallback = GL_RGB);
extern void cleanupbloom();
extern void setupbloom(int w, int h);
extern void loadhdrshaders(int aa);
extern void copyhdr(int sw, int sh, GLuint fbo);

#endif

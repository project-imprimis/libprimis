#ifndef AO_H_
#define AO_H_

extern int aow, aoh;
extern std::array<GLuint, 4> aotex;
extern float aomin, aosunmin;
extern int ao, aosun, aopackdepth, aoreduce, aoreducedepth;
extern int aobilateral, aobilateralupscale;

extern void initao();
extern void cleanupao();
extern void viewao();
extern void setupao(int w, int h);

#endif

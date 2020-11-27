#ifndef RENDERTIMERS_H_
#define RENDERTIMERS_H_
struct timer;
extern timer *begintimer(const char *name, bool gpu = true);
extern void endtimer(timer *t);

extern void printtimers(int conw, int conh);
extern int frametimer;
extern int framemillis;
extern void synctimers();
extern void cleanuptimers();

#endif

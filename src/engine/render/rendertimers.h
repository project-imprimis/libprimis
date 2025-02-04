#ifndef RENDERTIMERS_H_
#define RENDERTIMERS_H_
struct timer;
extern timer *begintimer(const char *name, bool gpu = true);
extern void endtimer(timer *t);

extern void printtimers(int conw, int framemillis);
extern int frametimer;
extern void synctimers();
extern void cleanuptimers();

#endif

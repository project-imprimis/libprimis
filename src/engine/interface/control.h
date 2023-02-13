#ifndef CONTROL_H_
#define CONTROL_H_

extern dynent *player;
extern int curtime;                     // current frame time
extern int lastmillis;                  // last time
extern int elapsedtime;                 // elapsed frame time
extern int totalmillis;                 // total elapsed time

extern FILE *getlogfile();
extern void logoutf(const char *fmt, ...) PRINTFARGS(1, 2);
extern void logoutfv(const char *fmt, va_list args, FILE *f = getlogfile());

extern bool inbetweenframes, renderedframe;

extern void fatal(const char *s, ...) PRINTFARGS(1, 2);

extern int initing;

enum
{
    Change_Graphics   = 1<<0,
    Change_Sound      = 1<<1,
    Change_Shaders    = 1<<2,
};
extern bool initwarning(const char *desc, int level = Init_Reset, int type = Change_Graphics);

extern int getclockmillis();

#endif

#ifndef CONSOLE_H_
#define CONSOLE_H_

extern void processkey(int code, bool isdown, int map);
extern void processtextinput(const char *str, int len);
extern float rendercommand(float x, float y, float w);
extern float renderfullconsole(float w, float h);
extern float renderconsole(float w, float h, float abovehud);
extern void conoutf(const char *s, ...) PRINTFARGS(1, 2);
extern void conoutf(int type, const char *s, ...) PRINTFARGS(2, 3);
extern void conoutfv(int type, const char *fmt, va_list args);
const char *getkeyname(int code);
extern tagval *addreleaseaction(ident *id, int numargs);
extern void writebinds(std::fstream& f);
extern void writecompletions(std::fstream& f);

#endif

#ifndef ENGINE_H_
#define ENGINE_H_

#include "../libprimis-headers/cube.h"

// command

extern void setvarchecked(ident *id, int val);
extern void setfvarchecked(ident *id, float val);
extern void setsvarchecked(ident *id, const char *val);

extern const char *escapeid(const char *s);
inline const char *escapeid(ident &id) { return escapeid(id.name); }

extern int variable(const char *name, int min, int cur, int max, int *storage, identfun fun, int flags);
extern float fvariable(const char *name, float min, float cur, float max, float *storage, identfun fun, int flags);
extern char *svariable(const char *name, const char *cur, char **storage, identfun fun, int flags);
extern void setvar(const char *name, int i, bool dofunc = true, bool doclamp = true);
extern void setfvar(const char *name, float f, bool dofunc = true, bool doclamp = true);
extern void setsvar(const char *name, const char *str, bool dofunc = true);
extern void touchvar(const char *name);
extern int getvar(const char *name);
extern int getvarmin(const char *name);
extern int getvarmax(const char *name);
extern bool identexists(const char *name);
extern ident *getident(const char *name);
extern ident *newident(const char *name, int flags = 0);
extern ident *readident(const char *name);
extern ident *writeident(const char *name, int flags = 0);
extern bool addcommand(const char *name, identfun fun, const char *narg, int type = Id_Command);
extern void keepcode(uint *p);
extern void freecode(uint *p);
extern void executeret(const uint *code, tagval &result = *commandret);
extern void executeret(const char *p, tagval &result = *commandret);
extern void executeret(ident *id, tagval *args, int numargs, bool lookup = false, tagval &result = *commandret);
extern char *executestr(const uint *code);
extern char *executestr(const char *p);
extern char *executestr(ident *id, tagval *args, int numargs, bool lookup = false);
extern char *execidentstr(const char *name, bool lookup = false);
extern int execute(const uint *code);
extern int execute(const char *p);
extern int execute(ident *id, tagval *args, int numargs, bool lookup = false);
extern int execident(const char *name, int noid = 0, bool lookup = false);
extern float executefloat(const uint *code);
extern float executefloat(const char *p);
extern float executefloat(ident *id, tagval *args, int numargs, bool lookup = false);
extern float execidentfloat(const char *name, float noid = 0, bool lookup = false);
extern bool executebool(const uint *code);
extern bool executebool(const char *p);
extern bool executebool(ident *id, tagval *args, int numargs, bool lookup = false);
extern bool execidentbool(const char *name, bool noid = false, bool lookup = false);
extern bool execfile(const char *cfgfile, bool msg = true);
extern void alias(const char *name, const char *action);
extern void alias(const char *name, tagval &v);
extern const char *getalias(const char *name);
extern const char *escapestring(const char *s);
extern bool validateblock(const char *s);
extern void explodelist(const char *s, vector<char *> &elems, int limit = -1);
extern int listlen(const char *s);
extern void printvar(ident *id);
extern void printvar(ident *id, int i);
extern void printfvar(ident *id, float f);
extern void printsvar(ident *id, const char *s);
extern int clampvar(ident *id, int i, int minval, int maxval);
extern float clampfvar(ident *id, float f, float minval, float maxval);
extern void loopiter(ident *id, identstack &stack, const tagval &v);
extern void loopiter(ident *id, identstack &stack, int i);
extern void loopend(ident *id, identstack &stack);
extern uint *compilecode(const char *p);

extern hashnameset<ident> idents;
extern int identflags;

extern void clearoverrides();

extern void checksleep(int millis);
extern void clearsleep(bool clearoverrides = true);
#endif

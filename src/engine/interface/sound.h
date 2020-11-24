// sound
extern void clearmapsounds();
extern void checkmapsounds();
extern void updatesounds();

extern int playsound(int n, const vec *loc = NULL, extentity *ent = NULL, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern int playsoundname(const char *s, const vec *loc = NULL, int vol = 0, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern void preloadsound(int n);
extern void preloadmapsound(int n);
extern bool stopsound(int n, int chanid, int fade = 0);
extern void stopsounds();
extern void initsound();

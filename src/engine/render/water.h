#ifndef WATER_H_
#define WATER_H_

extern int vertwater;

/* creates a set of functions by appending name & var to "get" and of the type "type"
 * e.g. GETMATIDXVAR(water, deep, int) creates `int getwaterdeep(int mat)`
 * where `mat` is the number of the material (for multiuplexed materials), e.g. water2, water3
 */
#define GETMATIDXVAR(name, var, type) \
    type get##name##var(int mat) \
    { \
        switch(mat&MatFlag_Index) \
        { \
            default: case 0: return name##var; \
            case 1: return name##2##var; \
            case 2: return name##3##var; \
            case 3: return name##4##var; \
        } \
    }

extern const bvec &getwatercolor(int mat);
extern const bvec &getwaterdeepcolor(int mat);
extern const bvec &getwaterfallcolor(int mat);
extern int getwaterfog(int mat);
extern int getwaterdeep(int mat);

extern void renderwater();
extern void renderwaterfalls();
extern void loadcaustics(bool force = false);
extern void preloadwatershaders(bool force = false);

#endif

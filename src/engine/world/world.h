#ifndef WORLD_H_
#define WORLD_H_

constexpr float wateramplitude = 0.4f; //max wave height
constexpr float wateroffset = 1.1f;    //wave offset from top of mat volume

enum MaterialSurfaces
{
    MatSurf_NotVisible = 0,
    MatSurf_Visible,
    MatSurf_EditOnly
};

constexpr float defaulttexscale = 16.0f;

extern char *maptitle;

extern std::vector<int> entgroup;
extern std::vector<int> outsideents;

/**
 * @brief Cleans up and then removes ent information from cubeext.
 *
 * Only affects the passed cube's cubeext's ents (an octaentities object). Cleans
 * up data associated with that octaentities object and then frees it from the
 * heap.
 *
 * @param c the cube to modify the cubeext's octaentities of
 */
extern void freeoctaentities(cube &c);
extern void entcancel();
extern void entselectionbox(const entity &e, vec &eo, vec &es);

namespace entities
{
    extern extentity *newentity();
    extern void deleteentity(extentity *e);
    extern std::vector<extentity *> &getents();
}

#endif

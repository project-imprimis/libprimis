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

extern std::vector<size_t> entgroup;
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

/**
 * @brief Clears the entgroup vector.
 *
 * This contains the current working set of entities.
 */
extern void entcancel();

/**
 * @brief Queries the selection box of the specified entity.
 *
 * `eo` and `es` are opposite corners of the bounding box, and are equally far from
 * the model center.
 *
 * @param e the entity to query
 * @param eo the minimum coordinates of the bounding box
 * @param es the maximum coordinates of the bounding box
 */
extern void entselectionbox(const entity &e, vec &eo, vec &es);

namespace entities
{
    /**
     * @brief Returns new heap-allocated extentity object.
     *
     * Must be freed with `delete` at the end of its lifespan. This can
     * be done with `deleteentity()`, for example.
     *
     * @return a new heap allocated entity
     */
    extern extentity *newentity();

    /**
     * @brief Frees heap allocated extentity object.
     *
     * Frees an entity created with `new` or via e.g. newentity().
     *
     * @param e the entity to delete
     */
    extern void deleteentity(extentity *e);
    extern std::vector<extentity *> &getents();
}

#endif

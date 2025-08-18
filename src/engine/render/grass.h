#ifndef GRASS_H_
#define GRASS_H_
extern void loadgrassshaders();
extern void generategrass();
extern void rendergrass();

/**
 * @brief Cleans up grass rendering resources.
 *
 * Cleans up grass rendering texture `grassvbo` and also cleans up grass shaders.
 */
extern void cleanupgrass();
#endif

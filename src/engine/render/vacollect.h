#ifndef VACOLLECT_H_
#define VACOLLECT_H_

/**
 * @brief Tries to destroy a VBO at the specified index
 *
 * Searches for the vbo with the specified name from the vbo map. If the vbo exists,
 * decrements the use counter for that vboinfo. If the use count hits zero,
 * destroys the VBO. and the vboinfo object related to it.
 *
 * Used extern in other files.
 *
 * @param vbo the vbo to decrement
 */
extern void destroyvbo(GLuint vbo);

#endif

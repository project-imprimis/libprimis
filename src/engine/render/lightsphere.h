#ifndef LIGHTSPHERE_H_
#define LIGHTSPHERE_H_

namespace lightsphere
{

    /**
     * @brief Frees the vbuf and ebuf objects used for lightsphere.
     *
     * The vbuf is the vertex buffer and the ebuf is the element buffer.
     */
    extern void cleanup();

    /**
     * @brief Enables the lightsphere drawing mode.
     *
     * Sets up the lightsphere GL configuration. This should be called before `draw()`
     * and should be cleaned up with `disable()`.
     */
    extern void enable();
    extern void draw();

    /**
     * @brief Disables the lightsphere drawing mode.
     *
     * Reverts the lightsphere GL setup done by enable(). This should be called
     * after lightsphere `draw()`.
     */
    extern void disable();
}

#endif

#ifndef LIGHTSPHERE_H_
#define LIGHTSPHERE_H_

namespace lightsphere
{
    extern void cleanup();
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

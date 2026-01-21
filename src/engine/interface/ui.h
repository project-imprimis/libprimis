#ifndef UI_H_
#define UI_H_

namespace UI
{
    /**
     * @brief Returns whether any UI is being grabbed by the cursor
     *
     * @return true if any window is accepting input, false otherwise
     */
    bool hascursor();

    /**
     * @brief Returns to reference parameters the position of the cursor
     *
     * Valid cursor ranges in x and y direction are 0...1 (regardless of aspect ratio)
     *
     * @brief x reference to return x position to
     * @param y reference to return y position to
     */
    void getcursorpos(float &x, float &y);

    /**
     * @brief Returns the cursor to the center of the screen.
     *
     * This is the coordinates (0.5, 0.5).
     */
    void resetcursor();

    /**
     * @brief Moves cursor relative to previous position
     *
     * No effect if there is no cursor. Clamps final position to 0..1 in each dimension.
     * Amount of total movement is scaled by `uisensitivity` divided by hudw() or hudh
     * for x,y dimensions specifically.
     *
     * @param dx amount to move in x direction
     * @param dx amount to move in y direction
     */
    bool movecursor(int dx, int dy);
    bool keypress(int code, bool isdown);
    bool textinput(const char *str, int len);
    float abovehud();

    /**
     * @brief Creates a new UI World object, used globally for UI functionality.
     *
     * Should only be called once. Does not clean up or de-allocate any existing UI world.
     * Call cleanup() before calling setup() if a world object already exists.
     */
    void setup();
    void update();
    void render();

    /**
     * @brief Cleans up and deletes the UI world object.
     *
     * Assumes a valid world object was created e.g. by setup(). Will cause a
     * dereferenced null pointer if no object exists.
     *
     * Deletes all of the world object's windows, its child objects, and then
     * destructs the heap-allocated world object.
     *
     * Sets the world global to nullptr afterwards.
     */
    void cleanup();

    bool showui(const char *name);
    bool hideui(const char *name);
    bool toggleui(const char *name);
    void holdui(const char *name, bool on);
    bool uivisible(const char *name);
}

#endif

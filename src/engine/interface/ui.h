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
    bool movecursor(int dx, int dy);
    bool keypress(int code, bool isdown);
    bool textinput(const char *str, int len);
    float abovehud();

    void setup();
    void update();
    void render();
    void cleanup();

    bool showui(const char *name);
    bool hideui(const char *name);
    bool toggleui(const char *name);
    void holdui(const char *name, bool on);
    bool uivisible(const char *name);
}

#endif

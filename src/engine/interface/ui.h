// ui

namespace UI
{
    bool hascursor();
    void getcursorpos(float &x, float &y);
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

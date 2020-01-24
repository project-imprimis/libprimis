#ifndef __PYTHON_H__
#define __PYTHON_H__

#include <python3.8/Python.h>

enum AccessLevel
{
    MAP         = 1<<0, // Access to the current map's elements         (server-side only)
    GAMEPLAY    = 1<<1, // Access to gameplay mechanic tweaks           (server-side only)
    CONFIG      = 1<<2, // Access to game configuration w/ file access  (client-side only)
    GUI         = 1<<3  // Access to UI elements                        (server-side and client-side)
};

void loadpymodule(const char* modulename);

class PyScript
{
private:
    AccessLevel level;

    const char* filename;

    PyObject
        *py_module,
        *py_func; // TESTING

    bool py_err_get(const char *&type, const char *&value, const char *&traceback);

public:

    PyScript(const char* _filename, AccessLevel _level);
    ~PyScript(void);

    bool load_script();
    bool run_script();
    bool stop_script();
};

#endif
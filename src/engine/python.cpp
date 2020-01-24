#include "engine.h"

/*  PYTHON.CPP

Implements Python as a faster, more powerful and cleaner replacement to CubeScript.
Runs immediately on class instantiation.

NOTE: These methods are currently undergoing implementation, here are some things to note:
    * Load a Python module: Type "/loadpymodule %s" in-game, %s being the module name relative to CWD, without the .py
    * The loaded Python module MUST have a method called "imprimis_test" as it is statically implemented for now
*/

// loadpyscript modulename
// Loads and execute a Python module
void loadpymodule(const char* modulename)
{
    conoutf(CON_INFO, "attempting to load python module %s from CWD", modulename);
    PyScript test_script(modulename, MAP); // Code something to pass output result, cnf or other
    return;
}

COMMAND(loadpymodule, "s");


PyScript::PyScript(const char* _filename, AccessLevel _level)
{
    filename    = _filename;
    level       = _level;

    if (load_script())
    {
        std::thread py_thread (run_script);
    }
    
    return; // Error handling here
}

PyScript::~PyScript(void)
{
    stop_script();

    return;
}

// Prints last Python error to the console, if available, then clears the error flag.
// Affected by execution state, such as whether the log is open and python verboseness
// Returns false if no error flag was raised
bool PyScript::py_err_get(const char *&type, const char *&value, const char *&traceback)
{
    if (PyErr_Occurred())
    {
        PyObject *e_type, *e_value, *e_traceback;
        PyErr_Fetch(&e_type, &e_value, &e_traceback);

        if (e_type != NULL)
        {
            PyObject *e_str_repr = PyObject_Repr(e_type);
            conoutf(CON_ERROR, "E-Type: %s", PyUnicode_AsUTF8(e_str_repr));
            Py_DECREF(e_str_repr);
            Py_DECREF(e_type);
        }
        if (e_value != NULL)
        {
            PyObject *e_str_repr = PyObject_Repr(e_value);
            conoutf(CON_ERROR, "E-Value: %s", PyUnicode_AsUTF8(e_str_repr));
            Py_DECREF(e_str_repr);
            Py_DECREF(e_value);
        }
        if (e_traceback != NULL)
        {
            PyObject *e_str_repr = PyObject_Repr(e_traceback);
            conoutf(CON_ERROR, "E-Traceback: %s", PyUnicode_AsUTF8(e_str_repr));
            Py_DECREF(e_str_repr);
            Py_DECREF(e_traceback);
        }

        return true;
    } else return false;
}

// Load Python script into memory
bool PyScript::load_script()
{
    Py_Initialize();

    // Import path module for proper file imports
    PyObject *sysmodule = PyImport_ImportModule("sys");
    PyObject *syspath   = PyObject_GetAttrString(sysmodule, "path");

    PyList_Append(syspath, PyUnicode_FromString("."));

    Py_DECREF(syspath);
    Py_DECREF(sysmodule);

    // Load specified module
    py_module = PyImport_ImportModule(filename);

    if (py_module == NULL) {
        const char *type, *value, *traceback;
        py_err_get(type, value, traceback);
    }

    return (py_module != NULL); // Check that module loaded properly
}

// Run Python script loaded using load_script()
bool PyScript::run_script()
{
    py_func = PyObject_GetAttrString(py_module, "imprimis_test"); // TESTING

    if (py_func && PyCallable_Check(py_func)) // py_func is valid AND callable
    {
        PyObject *py_value = PyObject_CallObject(py_func, NULL);
        printf("Result of call: %ld\n", PyLong_AsLong(py_value));
        Py_DECREF(py_value);
    }

    return true;
}

// Stop Python script that was launched using run_script()
bool PyScript::stop_script()
{
    // De-allocate memory used by the Python interpreter and kill any running scripts
    Py_DECREF(py_module);

    Py_FinalizeEx();

    return true; // Error code
}

/*
// e_ -> error
        // f_ -> function

        PyObject *e_type, *e_value, *e_traceback;
        static PyObject *f_traceback = NULL; // Stay, no reason to re-import traceback module.

        PyErr_Fetch(&e_type, &e_value, &e_traceback);
        PyErr_NormalizeException(&e_type, &e_value, &e_traceback); // Instantiate e_value if not instantiated

        if (e_traceback != NULL) // Python objects were not pre-normalized, clean up
        {
            PyException_SetTraceback(e_value, e_traceback);
            Py_DECREF(e_traceback);
        }

        if (f_traceback == NULL) // Traceback module hasn't been loaded
        {
            PyObject *py_module_traceback = PyImport_ImportModule("traceback");

            if (py_module_traceback) f_traceback = PyObject_GetAttrString(py_module_traceback, "format_exception");
        }

        if (f_traceback && PyCallable_Check(f_traceback))
        {
            PyObject *ret_val = PyObject_CallFunctionObjArgs(f_traceback, e_type, e_value, e_traceback, NULL);

            for (Py_ssize_t str_i = 0; str_i < PyList_Size(ret_val); str_i++) // ret_val is null - why
            {
                printf(PyUnicode_AsUTF8(PyList_GetItem(ret_val, str_i)));
            }
            Py_DECREF(ret_val);
        }

        assert(true);


        

        Py_DECREF(e_type);
        Py_DECREF(e_value);
        Py_DECREF(e_traceback);
*/
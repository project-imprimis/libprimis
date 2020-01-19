#include "engine.h"

/*  PYTHON.CPP

Implements Python as a faster, more powerful and cleaner replacement to CubeScript.
Runs immediately on class instantiation.
*/

// loadpyscript modulename
// Loads and execute a Python module
void loadpymodule(const char* modulename)
{
    conoutf(CON_INFO, "attempting to load python module %s from CWD", modulename);
    PyScript test_script(modulename, MAP); // Code something to pass output result, cnf or other
}

COMMAND(loadpymodule, "s");


PyScript::PyScript(const char* _filename, AccessLevel _level)
{
    filename    = _filename;
    level       = _level;

    if (load_script())
    {
        run_script();
    }
    
    return; // Error handling here
}

PyScript::~PyScript(void)
{
    stop_script();

    return;
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

    if (py_module == NULL) PyErr_Print();

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
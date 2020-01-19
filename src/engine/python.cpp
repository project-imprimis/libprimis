#include "python.h"
/*  PYTHON.CPP

Implements Python as a faster, more powerful and cleaner replacement to CubeScript.
Runs immediately on class instantiation.
*/


PyScript::PyScript(const char* _filename, AccessLevel _level)
{
    filename    = _filename;
    level       = _level;

    if (load_script())
    {
        run_script();
    }
    else return; // Error handling here
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

    PyErr_Print();

    return (py_module != NULL); // Check that module loaded properly
}

// Run Python script loaded using load_script()
bool PyScript::run_script()
{
    printf("catch me!\n");

    py_func = PyObject_GetAttrString(py_module, "imprimis_test"); // TESTING

    if (py_func && PyCallable_Check(py_func)) // py_func is valid AND callable
    {
        PyObject *py_value = PyObject_CallObject(py_func, NULL);
        //printf("Result of call: %ld\n", PyLong_AsLong(py_value));
        Py_DECREF(py_value);
    }
}

// Stop Python script that was launched using run_script()
bool PyScript::stop_script()
{
    // De-allocate memory used by the Python interpreter and kill any running scripts
    Py_DECREF(py_module);

    Py_FinalizeEx();

    return true; // Error code
}
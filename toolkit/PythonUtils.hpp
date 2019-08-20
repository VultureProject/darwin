/// \file     PythonUtils.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     30/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <Python.h>
#include <string>

/// \namespace darwin
namespace darwin {
    /// \namespace pythonutils
    namespace pythonutils {
        bool InitPythonProgram(const std::string &python_path_str, wchar_t **program_name,
                               const std::string *custom_python_path=nullptr);

        bool ImportPythonModule(const std::string &module_str, PyObject **py_module);

        bool GetPythonFunction(PyObject *py_module, const std::string &function_str, PyObject **py_function);

        bool CallPythonFunction(PyObject *py_function, PyObject **py_result);

        bool CallPythonFunction(PyObject *py_function, PyObject *py_args, PyObject **py_result);

        void ExitPythonProgram(wchar_t **program_name);
    }
}

// int main(int argc, char **argv) {
//     std::string python_path_str = "./testenv/bin/python";
//     std::string module_str = "request_test";
//     std::string function_str = "make_dummy_request";
//     wchar_t *program_name = nullptr;
//     PyObject *py_function = nullptr;
//     PyObject *py_module = nullptr;
//     PyObject *py_result = nullptr;

//     if (!darwin::pythonutils::InitPythonProgram(python_path_str, &program_name) ||
//         !darwin::pythonutils::ImportPythonModule(module_str, &py_module) ||
//         !darwin::pythonutils::GetPythonFunction(py_module, function_str, &py_function) ||
//         !darwin::pythonutils::CallPythonFunction(py_function, &py_result)) {
//         std::cout << "Exiting" << std::endl;
//         std::exit(EXIT_FAILURE);
//     }

//     PyObject *py_unicode_str = nullptr;

//     if ((py_unicode_str = PyUnicode_AsUTF8String(py_result)) == nullptr) {
//         std::cout << "An error occurred while getting the Unicode object from the function result" << std::endl;
//         PyErr_Print();
//         std::exit(EXIT_FAILURE);
//     }

//     char *result_char = nullptr;

//     if ((result_char = PyBytes_AsString(py_unicode_str)) == nullptr) {
//         std::cout << "An error occurred while getting the C string object from the Unicode object" << std::endl;
//         PyErr_Print();
//         std::exit(EXIT_FAILURE);
//     }

//     darwin::pythonutils::ExitPythonProgram(&program_name);

//     auto result_str = std::string(result_char);
//     std::cout << result_str << std::endl;

//     std::exit(EXIT_SUCCESS);
// }

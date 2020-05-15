/// \file     PythonUtils.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     30/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.


#include <boost/filesystem.hpp>
#include <string>

#include "Logger.hpp"
#include "PythonUtils.hpp"

/// \namespace darwin
namespace darwin {
    /// \namespace pythonutils
    namespace pythonutils {
        bool InitPythonProgram(const std::string &python_path_str, wchar_t **program_name,
                               const std::string *custom_python_path) {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("darwin:: pythonutils:: InitPythonProgram:: Initializing the Python program...");
            auto python_path = boost::filesystem::system_complete(boost::filesystem::path(python_path_str));
            auto complete_python_path_str = python_path.string();

            if (!boost::filesystem::exists(python_path)) {
                DARWIN_LOG_DEBUG("darwin:: pythonutils:: InitPythonProgram:: The path " + complete_python_path_str + " does not exist");
                return EXIT_FAILURE;
            }

            if ((*program_name = Py_DecodeLocale(complete_python_path_str.c_str(), nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("darwin:: pythonutils:: InitPythonProgram:: An error occurred while getting the Pythonic name of the program");
                PyErr_Print();
                return false;
            }

            Py_SetProgramName(*program_name);
            Py_Initialize();

            if (PyRun_SimpleString("import sys") != 0) {
                DARWIN_LOG_DEBUG("darwin:: pythonutils:: InitPythonProgram:: An error occurred while loading the 'sys' module");
                PyErr_Print();
                return false;
            }

            // we can add a custom Python path if we want
            if (custom_python_path != nullptr) {
                std::string command = "sys.path.append(\"" + *custom_python_path + "\")";
                if (PyRun_SimpleString(command.c_str()) != 0) {
                    DARWIN_LOG_DEBUG(
                            "darwin:: pythonutils:: InitPythonProgram:: An error occurred while appending the custom path '" +
                            *custom_python_path +
                            "' to the Python path"
                    );

                    PyErr_Print();
                    return false;
                }
            } else {
                if (PyRun_SimpleString("import os") != 0) {
                    DARWIN_LOG_DEBUG("darwin:: pythonutils:: InitPythonProgram:: An error occurred while loading the 'os' module");
                    PyErr_Print();
                    return false;
                }

                if (PyRun_SimpleString("sys.path.append(os.getcwd())") != 0) {
                    DARWIN_LOG_DEBUG("darwin:: pythonutils:: InitPythonProgram:: An error occurred while appending the current path to the Python path");
                    PyErr_Print();
                    return false;
                }
            }

            return true;
        }

        bool ImportPythonModule(const std::string &module_str, PyObject **py_module) {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("darwin:: pythonutils:: ImportPythonModule:: Importing the Python module '" + module_str + "'...");
            PyObject *py_module_str = nullptr;

            if ((py_module_str = PyUnicode_FromString(module_str.c_str())) == nullptr) {
                DARWIN_LOG_DEBUG("darwin:: pythonutils:: ImportPythonModule:: An error occurred while getting the Pythonic name of the module");
                PyErr_Print();
                return false;
            }

            if ((*py_module = PyImport_Import(py_module_str)) == nullptr) {
                DARWIN_LOG_DEBUG("darwin:: pythonutils:: ImportPythonModule:: An error occurred while loading the module");
                PyErr_Print();
                return false;
            }

            return true;
        }

        bool GetPythonFunction(PyObject *py_module, const std::string &function_str, PyObject **py_function) {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("darwin:: pythonutils:: GetPythonFunction:: Getting the Python function '" + function_str + "'...");

            if ((*py_function = PyObject_GetAttrString(py_module, function_str.c_str())) == nullptr) {
                DARWIN_LOG_DEBUG("darwin:: pythonutils:: GetPythonFunction:: An error occurred while getting the Python function '" + function_str + "'");
                PyErr_Print();
                return false;
            }

            return true;
        }

        bool CallPythonFunction(PyObject *py_function, PyObject **py_result) {
            return CallPythonFunction(py_function, nullptr, py_result);
        }

        bool CallPythonFunction(PyObject *py_function, PyObject *py_args, PyObject **py_result) {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("darwin:: pythonutils:: CallPythonFunction:: Calling the Python function...");

            if ((*py_result = PyObject_CallObject(py_function, py_args)) == nullptr) {
                DARWIN_LOG_DEBUG("darwin:: pythonutils:: CallPythonFunction:: An error occurred while calling the Python function");
                PyErr_Print();
                return false;
            }

            return true;
        }

        void ExitPythonProgram(wchar_t **program_name) {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("darwin:: pythonutils:: ExitPythonProgram:: Exiting the Python program...");

            Py_Finalize();
            PyMem_RawFree(*program_name);
        }
    }
}

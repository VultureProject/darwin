/// \file     Core.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     09/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <thread>
#include <atomic>
#include "Monitor.hpp"
#include "ThreadGroup.hpp"

#ifndef PID_PATH
# define PID_PATH "/var/run/darwin/"
#endif // !PID_PATH

/// \namespace darwin
namespace darwin {

    /// Singleton. The main class of the program.
    ///
    /// \class Core
    class Core {
    private:
        Core();

    public:
        ~Core() = default;

        /// Get the singleton instance. Create it of first call. Thread-safe.
        ///
        /// \return Instance of the singleton.
        static Core& instance() {
            static Core core;
            return core;
        }

        /// Run the program.
        ///
        /// \return 0 on success, an error code otherwise.
        int run();

        /// Parse command line parameters to configure the filter and the core.
        ///
        /// \param ac The number of argument passed to the program.
        /// \param av The string formatted argument of the program.
        /// \return true on success, false otherwise.
        bool Configure(int ac, char** av);

        /// Display usage on standard output.
        static void Usage();

        /// Write the .pid file based on the filter name passed in the program arguments.
        ///
        /// \return true on success, false otherwise.
        bool WritePID();

        /// Clear the previously wrote .pid file (cf. WritePID())
        void ClearPID();

        /// Get the argument value as an unsigned long long.
        ///
        /// \param res Used to store the converted value.
        /// \param arg The argument to convert.
        /// \return true on success, false otherwise.
        static bool GetULArg(unsigned long& res, const char* arg);

        /// Getter for the filter name.
        ///
        /// \return A const reference to the string containing the unique name given in the program arguments.
        const std::string& GetFilterName();

        /// Set the log level
        ///
        /// \param level The level to set.
        /// \return true on success, false otherwise.
        bool SetLogLevel(std::string level);

    private:
        std::string _name;
        std::string _socketPath;
        std::string _modConfigPath;
        std::string _monSocketPath;
        std::string _pidPath;
        std::string _nextFilterUnixSocketPath;
        std::string _output;
        std::size_t _nbThread;
        std::size_t _cacheSize;
        std::size_t _threshold;
        ThreadGroup _threadpool;


    public:
        // TODO Maybe a getter is a better idea...
        bool daemon;
    };
}

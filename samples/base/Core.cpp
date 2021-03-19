/// \file     Core.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     09/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <unistd.h>
#include <climits>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdio.h>
#include <ctype.h>

#include "Generator.hpp"
#include "Logger.hpp"
#include "Server.hpp"
#include "Core.hpp"
#include "Stats.hpp"

namespace darwin {

    Core::Core()
            : _name{}, _socketPath{}, _modConfigPath{}, _monSocketPath{},
              _pidPath{}, _nbThread{0},
              _threadpool{}, daemon{true} {}

    int Core::run() {
        DARWIN_LOGGER;
        int ret{0};

        SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::starting);

        try {
            Monitor monitor{_monSocketPath};
            std::thread t{std::bind(&Monitor::Run, std::ref(monitor))};

            SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::configuring);
            Generator gen{};
            if (not gen.Configure(_modConfigPath, _cacheSize)) {
                DARWIN_LOG_CRITICAL("Core:: Run:: Unable to configure the filter");
                raise(SIGTERM);
                t.join();
                return 1;
            }
            DARWIN_LOG_DEBUG("Core::run:: Configured generator");

            try {
                Server server{_socketPath, _output, _nextFilterUnixSocketPath, _threshold, gen};
                if (not gen.ConfigureNetworkObject(server.GetIOContext())) {
                    raise(SIGTERM);
                    t.join();
                    return 1;
                }
                SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::running);

                DARWIN_LOG_DEBUG("Core::run:: Creating threads...");
                // Starting from 1 because the current process will run
                // the io_context too.
                for (std::size_t i = 1; i < _nbThread; ++i) {
                    _threadpool.CreateThread(
                        std::bind(&Server::Run, std::ref(server))
                    );
                }
                server.Run();
                DARWIN_LOG_DEBUG("Core::run:: Joining threads...");
                _threadpool.JoinAll();
                server.Clean();
            } catch (const std::exception& e) {
                DARWIN_LOG_CRITICAL(std::string("Core::run:: Cannot open unix socket: ") + e.what());
                ret = 1;
                raise(SIGTERM);
            }
            DARWIN_LOG_DEBUG("Core::run:: Joining monitoring thread...");
            if (t.joinable())
                t.join();
        } catch (const std::exception& e) {
            DARWIN_LOG_CRITICAL(std::string("Core::run:: Cannot open monitoring socket: ") + e.what());
            return 1;
        }

        return ret;
    }

    bool Core::SetLogLevel(std::string level){
        DARWIN_LOGGER;
        if (level == "DEVELOPER") {
            log.setLevel(logger::Debug);
            daemon = false;
            return true;
        }

        return log.setLevel(level);
    }

    bool Core::Configure(int ac, char** av) {
        DARWIN_LOGGER;

        int opt;
        std::string log_level;

        // OPTIONS
        log.setLevel(logger::Warning); // Log level by default
        opt = -1;
        while((opt = getopt(ac, av, ":l:hno:")) != -1)
        {
            switch(opt)
            {
                case 'n':
                    daemon = false;
                    break;
                case 'o':
                    if (not log.setFilePath(optarg))
                        return false;
                    break;
                case 'l':
                    log_level.assign(optarg);
                    if(!Core::SetLogLevel(log_level)){
                        std::clog << "Unknown log-level " << optarg << std::endl << std::endl;
                        Core::Usage();
                        return false;
                    }
                    break;
                case 'h':
                    Core::Usage();
                    return false;
                case ':':
                    std::clog << "Missing an option argument for -" << (char)optopt << std::endl << std::endl;
                    Core::Usage();
                    return false;
                case '?':
                    std::clog << "Unknown option -" << (char)optopt << std::endl << std::endl;
                    Core::Usage();
                    return false;
            }
        }

        // After options we need 10 mandatory arguments
        if (ac-optind < 10) {
            std::clog << "Missing some parameters" << std::endl << std::endl;
            Core::Usage();
            return false;
        }

        // MANDATORY ARGUMENTS
        log.setName(av[optind]);
        _name = av[optind];
        _socketPath = av[optind + 1];
        _modConfigPath = av[optind + 2];
        _monSocketPath = av[optind + 3];
        _pidPath = av[optind + 4];
        _output = av[optind + 5];
        _nextFilterUnixSocketPath = av[optind + 6];
        if (!GetULArg(_nbThread, av[optind + 7]))
            return false;
        if (!GetULArg(_cacheSize, av[optind + 8]))
            return false;
        if (!GetULArg(_threshold, av[optind + 9]))
            return false;

        return true;
    }

    void Core::Usage() {
        std::cout << "Usage: ./filter <filter_name> <socket_path> <config_file> "
                  "<monitoring_socket_path> <pid_file> <output> <next_filter_socket_path> <nb_thread> "
                  "<cache_size> <threshold> [OPTIONS]\n\n";
        std::cout
                << "  filter_name\tSpecify the name of this filter in the logs\n";
        std::cout
                << "  socket_path\tSpecify the path to the unix socket for the main connection\n";
        std::cout
                << "  config_file\tSpecify the path to the configuration file\n";
        std::cout
                << "  monitoring_socket_path\tSpecify the path to the monitoring unix socket\n";
        std::cout
                << "  pid_file\tSpecify the path to the pid file\n";
        std::cout
                << "  output\tSpecify the filter's output\n";
        std::cout
                << "  next_filter_socket_path\tSpecify the path to the next filter unix socket\n";
        std::cout
                << "  nb_thread\tInteger specifying the number of thread used\n";
        std::cout
                << "  cache_size\tInteger specifying the cache's size\n";
        std::cout
                << "  threshold\tInteger specifying the minimal certitude at which the filter will output an alert. "
                   "If it's over 100, take the filter's default threshold (80)\n";
        std::cout << "\nOPTIONS\n";
        std::cout << "  -o <logfilepath> \tSet custom log file path.\n";
        std::cout << "  -n\t\t\tDon't daemonize, filter will stay attached to parent process.\n";
        std::cout << "  -l\t\t\tSet log level to [DEBUG|INFO|NOTICE|WARNING|ERROR|CRITICAL|DEVELOPER].\n"
                  << "    \t\t\tDefault is WARNING.\n"
                  << "    \t\t\tNOTE: DEVELOPER mode deactivates daemonization and sets log level to DEBUG.\n"
                  << std::endl;
        std::cout << "  -h\t\t\tPrint help" << std::endl;
    }

    bool Core::WritePID() {
        DARWIN_LOGGER;

        // TODO Use the system functions for PID files
        if (access(_pidPath.c_str(), F_OK) == 0) {
            DARWIN_LOG_CRITICAL("Another identical process already running. Exiting.");
            return false;
        }

        std::ofstream f{_pidPath, std::ios::out | std::ios::trunc};

        if (!f.is_open()) {
            DARWIN_LOG_CRITICAL("Core:: Write PID:: Unable to write to PID file '" +
                         _pidPath + "', exiting...");
            return false;
        }

        f << getpid();
        f.close();
        return true;
    }

    void Core::ClearPID() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Core::ClearPID:: Removing PID file");
        std::string name{_pidPath};
        unlink(name.c_str());
    }

    bool Core::GetULArg(unsigned long& res, const char* arg) {
        DARWIN_LOGGER;
        char* endptr{nullptr};
        errno = 0;

        res = strtoull(arg, &endptr, 10);
        if ((errno == ERANGE &&
             (res == ULLONG_MAX || res == 0))
            || (errno != 0 && res == 0)) {
            DARWIN_LOG_CRITICAL(std::string("Core:: Program Arguments:: ") +
                         strerror(errno));
            return false;
        }

        if (endptr == arg) {
            DARWIN_LOG_ERROR("Core:: Program Arguments:: could not parse number");
            return false;
        }
        return true;
    }

    const std::string& Core::GetFilterName() {
        return this->_name;
    }

}

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

#include "Generator.hpp"
#include "Logger.hpp"
#include "Server.hpp"
#include "Core.hpp"

namespace darwin {

    Core::Core()
            : _name{}, _socketPath{}, _modConfigPath{}, _monSocketPath{},
              _pidPath{}, _nbThread{0},
              _filter_status{FilterStatusEnum::starting},
              _threadpool{}, daemon{true} {}

    int Core::run() {
        DARWIN_LOGGER;
        int ret{0};

        try {
            Monitor monitor{_monSocketPath, _filter_status};
            std::thread t{std::bind(&Monitor::Run, std::ref(monitor))};

            _filter_status.store(FilterStatusEnum::configuring);
            Generator gen{};
            if (!gen.Configure(_modConfigPath, _cacheSize)) {
                DARWIN_LOG_CRITICAL("Core:: Run:: Unable to configure the filter");
                raise(SIGTERM);
                t.join();
                return 1;
            }
            DARWIN_LOG_DEBUG("Core::run:: Configured generator");

            try {
                Server server{_socketPath, _output, _nextFilterUnixSocketPath, _threshold, gen};
                _filter_status.store(FilterStatusEnum::running);

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

    bool Core::Configure(int ac, char** av) {
        DARWIN_LOGGER;
        if (ac < 11) {
            DARWIN_LOG_CRITICAL("Core:: Program Arguments:: Missing some parameters");
            Core::Usage();
            return false;
        }
        log.setName(av[1]);
        _name = av[1];
        _socketPath = av[2];
        _modConfigPath = av[3];
        _monSocketPath = av[4];
        _pidPath = av[5];
        _output = av[6];
        _nextFilterUnixSocketPath = av[7];
        if (!GetULArg(_nbThread, av[8]))
            return false;
        if (!GetULArg(_cacheSize, av[9]))
            return false;
        if (!GetULArg(_threshold, av[10]))
            return false;
        if (ac > 11) {
            if (!strncmp("-d", av[11], 2)) {
                log.setLevel(logger::Debug);
            } else if (!strncmp("-i", av[11], 2)) {
                log.setLevel(logger::Info);
            } else if (!strncmp("-n", av[11], 2)) {
                log.setLevel(logger::Notice);
            } else if (!strncmp("-w", av[11], 2)) {
                log.setLevel(logger::Warning);
            } else if (!strncmp("-e", av[11], 2)) {
                log.setLevel(logger::Error);
            } else if (!strncmp("-c", av[11], 2)) {
                log.setLevel(logger::Critical);
            } else if (!strncmp("-z", av[11], 2)) {
                log.setLevel(logger::Debug);
                DARWIN_LOG_DEBUG("Developer mode activated");
                daemon = false;
            }
        }

        return true;
    }

    void Core::Usage() {
        std::cout << "Usage: ./darwin filter_name socket_path config_file" <<
                  " monitoring_socket_path pid_file output next_filter_socket_path nb_thread "
                  <<
                  "cache_size threshold [OPTIONS]\n";
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
                << "  threshold\tInteger specifying the minimum certitude at which the filter will output a log."
                   "If it's over 100, take the filter's default threshold\n";
        std::cout << "\nOPTIONS\n";
        std::cout
                << "  -z\tDeveloper mode, does not create a daemon, set log level to debug"
                << std::endl;
        std::cout << "  -d\tSet log level to debug" << std::endl;
        std::cout << "  -i\tSet log level to info" << std::endl;
        std::cout << "  -n\tSet log level to notice" << std::endl;
        std::cout << "  -w\tSet log level to warning (DEFAULT)" << std::endl;
        std::cout << "  -e\tSet log level to error" << std::endl;
        std::cout << "  -c\tSet log level to critical" << std::endl;
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

        res = strtoull(arg, &endptr, 10);
        if ((errno == ERANGE &&
             (res == ULONG_MAX || res == 0))
            || (errno != 0 && res == 0)) {
            DARWIN_LOG_CRITICAL(std::string("Core:: Program Arguments:: ") +
                         strerror(errno));
            return false;
        }
        return true;
    }

}

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
#include "UnixServer.hpp"
#include "TcpServer.hpp"
#include "Core.hpp"
#include "Stats.hpp"
#include "StringUtils.hpp"

#include <boost/asio.hpp>

namespace darwin {

    Core::Core()
            : _name{}, _modConfigPath{}, _monSocketPath{},
              _pidPath{}, _nbThread{0}, _socketPath{}, daemon{true} {}

    int Core::run() {
        DARWIN_LOGGER;
        int ret{0};

        SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::starting);

        std::unique_ptr<AServer> server;

        try {
            Monitor monitor{_monSocketPath};
            std::thread t{std::bind(&Monitor::Run, std::ref(monitor))};

            SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::configuring);
            Generator gen{_nbThread};
            if (not gen.Configure(_modConfigPath, _cacheSize)) {
                DARWIN_LOG_CRITICAL("Core:: Run:: Unable to configure the filter");
                raise(SIGTERM);
                t.join();
                return 1;
            }
            DARWIN_LOG_DEBUG("Core::run:: Configured generator");

            switch(_net_type) {
                case NetworkSocketType::Unix:
                    DARWIN_LOG_DEBUG("Core::run:: Unix socket configured on path " + _socketPath);
                    server = std::make_unique<UnixServer>(_socketPath, _output, _nextFilterUnixSocketPath, _threshold, gen);
                    break;
                case NetworkSocketType::Tcp:
                    DARWIN_LOG_DEBUG("Core::run:: TCP configured on address " + _net_address.to_string() + ":" + std::to_string(_net_port));
                    server = std::make_unique<TcpServer>(_net_address, _net_port, 0 /* Next filter port */, _nextFilterUnixSocketPath, _threshold, gen);
                    break;
                case NetworkSocketType::Udp:
                    //not yet implemented
                    DARWIN_LOG_DEBUG("Core::run:: UDP configured on address " + _net_address.to_string() + ":" + std::to_string(_net_port));
                    DARWIN_LOG_CRITICAL("Core:: Run:: UDP Not implemented");
                    __attribute__((fallthrough));
                default:
                    DARWIN_LOG_CRITICAL("Core:: Run:: Network Configuration problem");
                    raise(SIGTERM);
                    t.join();
                    return 1;
            }

            try {
                if (not gen.ConfigureNetworkObject(server->GetIOContext())) {
                    raise(SIGTERM);
                    t.join();
                    return 1;
                }
                SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::running);

                DARWIN_LOG_DEBUG("Core::run:: Launching server...");
                server->Run();
                DARWIN_LOG_DEBUG("Core::run:: Joining threads...");

                server->Clean();
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
        if (level == "DEVELOPER"){
            log.setLevel(logger::Debug);
            DARWIN_LOG_DEBUG("Developer mode activated");
            daemon = false;
            return true;
        }

        return log.setLevel(level);
    }

    bool Core::Configure(int ac, char** av) {
        DARWIN_LOGGER;

        int opt;
        std::string log_level;

        bool is_udp = false;

        // OPTIONS
        log.setLevel(logger::Warning); // Log level by default
        opt = -1;
        while((opt = getopt(ac, av, ":l:hu")) != -1)
        {
            DARWIN_LOG_DEBUG("OPT : " + std::to_string(opt));
            DARWIN_LOG_DEBUG("OPTIND : " + std::to_string(optind));
            switch(opt)
            {
                case 'l':
                    log_level.assign(optarg);
                    if(!Core::SetLogLevel(log_level)){
                        DARWIN_LOG_ERROR("Core:: Program Arguments:: Unknown log-level given");
                        Core::Usage();
                        return false;
                    }
                    break;
                case 'h':
                    Core::Usage();
                    return false;
                    break;
                case ':':
                    DARWIN_LOG_ERROR("Core:: Program Arguments:: Missing option argument");
                    Core::Usage();
                    return false;
                case '?':
                    DARWIN_LOG_ERROR("Core:: Program Arguments:: Unknown option");
                    Core::Usage();
                    return false;
                case 'u':
                    is_udp = true;
                    break;
            }
        }

        // After options we need 10 mandatory arguments
        if(ac-optind < 10){
            DARWIN_LOG_ERROR("Core:: Program Arguments:: Missing some parameters");
            Core::Usage();
            return false;
        }

        // MANDATORY ARGUMENTS
        log.setName(av[optind]);
        _name = av[optind];
        if (! this->ParseSocketAddress(std::string(av[optind + 1]), is_udp))
            return false;
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
        std::cout << "  -l\tSet log level to [DEBUG|INFO|NOTICE|WARNING|ERROR|CRITICAL|DEVELOPER].\n"
                  << "    \tDefault is WARNING."
                  << "    \tNOTE: DEVELOPER mode does not create a daemon and sets log level to DEBUG."
                  << std::endl;
        std::cout << "  -h\tPrint help" << std::endl;
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

    bool Core::ParsePort(const char* pathOrAddress) {
        DARWIN_LOGGER;

        long port = 0;
        if(!darwin::strings::StrToLong(pathOrAddress, port)){
            DARWIN_LOG_ERROR("Core::ParseSocketAddress:: Error while parsing the port number, unrecognized number: '" + std::string(pathOrAddress) + "'");
            return false;
        }

        if (port < 0 || port > 65353) {
            DARWIN_LOG_ERROR("Core::ParseSocketAddress:: Error while parsing the port number : out of bounds [0; 65353]: '" + std::string(pathOrAddress) + "'");
            return false;
        }

        _net_port = static_cast<int>(port);

        return true;
    }

    bool Core::ParseSocketAddress(const std::string& pathOrAddress, bool isUdp) {
        DARWIN_LOGGER;
        size_t colon = pathOrAddress.rfind(':');
        if (colon != std::string::npos) {
            boost::system::error_code e;
            auto addr = pathOrAddress.substr(0, colon);
            if(addr.find('[') != std::string::npos && addr.rfind(']') != std::string::npos) {
                addr.pop_back();
                addr.erase(0, 1);
            }
            auto port = pathOrAddress.substr(colon+1, pathOrAddress.length()-1);
            bool portRes = ParsePort(port.c_str());

            _net_address = boost::asio::ip::make_address(addr, e); 
            if( ! portRes || e.failed()) {
                DARWIN_LOG_CRITICAL("Error while parsing the ip address: " + pathOrAddress);
                return false;
            }

            if(isUdp) {
                _net_type = NetworkSocketType::Udp;
            } else {
                _net_type = NetworkSocketType::Tcp;
            }
        } else {
            _net_type = NetworkSocketType::Unix;
            _socketPath = pathOrAddress;
        }
        return true;
    }

    bool Core::IsDaemon() const {
        return this->daemon;
    }

}

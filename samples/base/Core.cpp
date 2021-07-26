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
#include "UdpServer.hpp"
#include "Core.hpp"
#include "Stats.hpp"
#include "StringUtils.hpp"
#include "UnixNextFilterConnector.hpp"
#include "TcpNextFilterConnector.hpp"
#include "UdpNextFilterConnector.hpp"

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
            std::thread t_nextfilter;
            if(_next_filter_connector){
                t_nextfilter = std::thread{std::bind(&ANextFilterConnector::Run, std::ref(*_next_filter_connector))};
            }

            SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::configuring);
            Generator gen{_nbThread};
            if (not gen.Configure(_modConfigPath, _cacheSize)) {
                DARWIN_LOG_CRITICAL("Core:: Run:: Unable to configure the filter");
                raise(SIGTERM);
                if(t_nextfilter.joinable()) 
                    t_nextfilter.join();
                if(t.joinable()) 
                    t.join();
                return 1;
            }
            DARWIN_LOG_DEBUG("Core::run:: Configured generator");

            switch(_net_type) {
                case network::NetworkSocketType::Unix:
                    DARWIN_LOG_DEBUG("Core::run:: Unix socket configured on path " + _socketPath);
                    server = std::make_unique<UnixServer>(_socketPath, _output, _threshold, gen);
                    break;
                case network::NetworkSocketType::Tcp:
                    DARWIN_LOG_DEBUG("Core::run:: TCP configured on address " + _net_address.to_string() + ":" + std::to_string(_net_port));
                    server = std::make_unique<TcpServer>(_net_address, _net_port, _output, _threshold, gen);
                    break;
                case network::NetworkSocketType::Udp:
                    DARWIN_LOG_DEBUG("Core::run:: UDP configured on address " + _net_address.to_string() + ":" + std::to_string(_net_port));
                    server = std::make_unique<UdpServer>(_net_address, _net_port, _output, _threshold, gen);
                    break;
                default:
                    DARWIN_LOG_CRITICAL("Core:: Run:: Network Configuration problem");
                    raise(SIGTERM);
                    if(t_nextfilter.joinable()) 
                        t_nextfilter.join();
                    if (t.joinable())
                        t.join();
                    return 1;
            }

            try {
                if (not gen.ConfigureNetworkObject(server->GetIOContext())) {
                    raise(SIGTERM);
                    if(t_nextfilter.joinable())
                        t_nextfilter.join();
                    if (t.joinable())
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
            if(t_nextfilter.joinable())
                t_nextfilter.join();
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
        bool is_next_filter_udp = false;
        // OPTIONS
        log.setLevel(logger::Warning); // Log level by default
        opt = -1;
        while((opt = getopt(ac, av, ":l:huv")) != -1)
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
                case 'v':
                    is_next_filter_udp = true;
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
        if (! network::ParseSocketAddress(std::string(av[optind + 1]), is_udp, _net_type, _net_address, _net_port, _socketPath))
            return false;
        _modConfigPath = av[optind + 2];
        _monSocketPath = av[optind + 3];
        _pidPath = av[optind + 4];
        _output = av[optind + 5];

        if (!SetNextFilterConnector(std::string(av[optind + 6]), is_next_filter_udp))
            return false;
        if (!GetULArg(_nbThread, av[optind + 7]))
            return false;
        if (!GetULArg(_cacheSize, av[optind + 8]))
            return false;
        if (!GetULArg(_threshold, av[optind + 9]))
            return false;

        return true;
    }

    bool Core::SetNextFilterConnector(std::string const& path_address, bool is_udp) {
        DARWIN_LOGGER;
        if(path_address == "no"){
            DARWIN_LOG_DEBUG("Core::SetNextFilterConnector :: No Next Filter set");
            return true;
        }
        network::NetworkSocketType net_type;
        std::string path;
        int port;
        boost::asio::ip::address addr;
        if (! network::ParseSocketAddress(path_address, is_udp, net_type, addr, port, path))
            return false;

        switch(net_type) {
            case network::NetworkSocketType::Unix:
                DARWIN_LOG_DEBUG("Core::SetNextFilterConnector :: Next Filter UNIX set on " + path);
                _next_filter_connector = std::make_unique<UnixNextFilterConnector>(path);
                return true;
            case network::NetworkSocketType::Tcp:
                DARWIN_LOG_DEBUG("Core::SetNextFilterConnector :: Next Filter TCP set on " + addr.to_string() + ":" + std::to_string(port));
                _next_filter_connector = std::make_unique<TcpNextFilterConnector>(addr, port);
                return true;
            case network::NetworkSocketType::Udp:
                DARWIN_LOG_DEBUG("Core::SetNextFilterConnector :: Next Filter UDP set on " + addr.to_string() + ":" + std::to_string(port));
                _next_filter_connector = std::make_unique<UdpNextFilterConnector>(addr, port);
                return true;
            default:
                DARWIN_LOG_CRITICAL("Core:: SetNextFilterConnector:: Next Filter Configuration error : unrecognized type");
                return false;
        }
        return false;
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

    bool Core::IsDaemon() const {
        return this->daemon;
    }

    ANextFilterConnector* Core::GetNextFilterconnector() {
        return _next_filter_connector.get();
    }

    const std::string& Core::GetName() const {
        return this->_name;
    }

}

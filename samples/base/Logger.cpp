/// \file     Logger.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     09/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <chrono>
#include <sstream>
#include <regex>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <iomanip>
#include "Logger.hpp"
#include "Time.hpp"

namespace darwin {
    namespace logger {

        Logger::Logger() : _logLevel(Warning) {
            _file = std::ofstream(DARWIN_LOG_FILE, std::ios::out | std::ios::app);
            if (!_file.is_open())
                std::clog << "Can't open log file " << DARWIN_LOG_FILE << std::endl;
        }

        Logger::~Logger() {
            _file.close();
        }

        void Logger::log(log_type type, std::string const& logMsg) {
            if (type < this->_logLevel || !_file.is_open())
                return;

            std::string date = darwin::time_utils::GetTime();
            std::stringstream fmt{};

            fmt << '{';
            fmt << "\"date\":\"" << date << "\",";
            switch (type) {
                case Debug:
                    fmt << "\"level\":\"DEBUG\",";
                    break;
                case Info:
                    fmt << "\"level\":\"INFO\",";
                    break;
                case Notice:
                    fmt << "\"level\":\"NOTICE\",";
                    break;
                case Warning:
                    fmt << "\"level\":\"WARNING\",";
                    break;
                case Error:
                    fmt << "\"level\":\"ERROR\",";
                    break;
                case Critical:
                    fmt << "\"level\":\"CRITICAL\",";
                    break;
            }
            fmt << "\"filter\":\"" << _name << "\",";
            fmt << "\"message\":\"" << logMsg << "\"";
            fmt << '}';
            _fileMutex.lock();
            _file << fmt.str().c_str() << std::endl;
            _fileMutex.unlock();
        }

        void Logger::setLevel(darwin::logger::log_type type) {
            _logLevel = type;
        }

        bool Logger::setLevel(std::string level){
            std::unordered_map<std::string, logger::log_type>::iterator it;
            it = _map_log_type.find(level);

            if (it != _map_log_type.end()) {
                // FOUND
                setLevel(it->second);
                return true;
            } else {
                // NOT FOUND
                return false;
            }
            return false;
        }

        void Logger::setName(std::string const& name) {
            _name = name;
        }

        void Logger::RotateLogs() {
            if (access(DARWIN_LOG_FILE, F_OK) != 0) {
                _fileMutex.lock();
                _file.close();
                _file = std::ofstream(DARWIN_LOG_FILE, std::ios::out | std::ios::app);
                if (!_file.is_open())
                    std::clog << "Can't open log file " << DARWIN_LOG_FILE
                              << std::endl;
                _fileMutex.unlock();
            }
        }
    }
}
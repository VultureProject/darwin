/// \file     OutputConfig.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     09/07/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#pragma once

#include <map>
#include <vector>
#include <string>

class OutputConfig {
    /// This class is used to handle an output config for Filter Buffer.
    /// It holds everything needed to create an AConnector
    ///
    ///\class OutputConfig

    public:
    ///\brief Unique constructor
    ///
    ///\param filter_type To fill _filter_type
    ///\param filter_socket_path To fill _filter_socket_path
    ///\param interval To fill interval
    ///\param redis_lists To fill _redis_lists
    ///\param required_log_lines To fill _required_log_lines
    OutputConfig(std::string &filter_type,
                std::string &filter_socket_path,
                unsigned int interval,
                std::vector<std::pair<std::string, std::string>> &redis_lists,
                unsigned int required_log_lines);

    ///\brief unique default destructor
    ~OutputConfig() = default;

    /// All the following members are sent to the constructor of AConnector
    std::string _filter_type;
    std::string _filter_socket_path;
    unsigned int _interval;
    std::vector<std::pair<std::string, std::string>> _redis_lists;
    unsigned int _required_log_lines;
};
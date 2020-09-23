/// \file     Generator.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     09/07/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#include "OutputConfig.hpp"

OutputConfig::OutputConfig(std::string &filter_type, 
                            std::string &filter_socket_path,
                            unsigned int interval,
                            std::vector<std::pair<std::string, std::string>> &redis_lists,
                            unsigned int required_log_lines) :
                        _filter_type(filter_type),
                        _filter_socket_path(filter_socket_path),
                        _interval(interval),
                        _redis_lists(redis_lists),
                        _required_log_lines(required_log_lines) {}
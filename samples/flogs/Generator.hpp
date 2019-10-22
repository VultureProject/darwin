/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     07/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>

#include "Session.hpp"
#include "AGenerator.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/rapidjson/document.h"

class Generator: public AGenerator {
public:
    Generator() = default;
    ~Generator();

public:
    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

protected:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;

private:
    bool ConfigRedis(std::string redis_socket_path);

    bool _log; // If the filter will stock the data in a log file
    bool _redis; // If the filter will stock the data in a REDIS
    std::string _log_file_path = "";
    std::string _redis_list_name = "";
    std::ofstream _log_file;
};
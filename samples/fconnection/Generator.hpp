/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "../../toolkit/RedisManager.hpp"
#include "Session.hpp"
#include "AGenerator.hpp"

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
    bool ConfigRedis(const std::string &redis_socket_path,
                     const std::string &init_data_path);

    unsigned int _redis_expire = 0;
};
/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     30/08/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

extern "C" {
#include <hiredis/hiredis.h>
}

#include <mutex>
#include <thread>
#include <string>
// Used for unordered_map
#include <unordered_map>

#include "Session.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"
#include "AGenerator.hpp"



struct id_timeout {
    std::string app_id;
    uint64_t timeout;
};


class Generator: public AGenerator {
public:
    Generator() = default;
    ~Generator() = default;

public:
    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;
	std::unordered_map<std::string, std::unordered_map<std::string, id_timeout>> _applications;
};
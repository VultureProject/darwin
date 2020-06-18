/// \file     Generator.hpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "../toolkit/FileManager.hpp"
#include "Session.hpp"
#include "TAnomalyThreadManager.hpp"
#include "AGenerator.hpp"

#define REDIS_INTERNAL_LIST "_anomalyFilter_internal"

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

    std::string _redis_internal = REDIS_INTERNAL_LIST;
    std::shared_ptr<AnomalyThreadManager> _anomaly_thread_manager;
};
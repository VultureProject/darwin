/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     07/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "Session.hpp"
#include "data_pool.hpp"
#include "AGenerator.hpp"
#include "ContentInspectionTask.hpp"


class Generator: public AGenerator {
public:
    Generator();
    ~Generator();

public:
    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

protected:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlterting(const std::string& tags) override final;

private:
    Configurations _configurations;
    MemManagerParams *_memoryManager;
};
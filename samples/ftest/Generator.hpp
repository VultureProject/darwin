/// \file     Generator.hpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     03/12/2013
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "Session.hpp"
#include "AGenerator.hpp"
#include "../toolkit/Files.hpp"
#include "../toolkit/rapidjson/document.h"

class Generator: public AGenerator {
public:
    Generator() = default;
    ~Generator() = default;

public:
    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

protected:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
};
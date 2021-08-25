/// \file     Generator.hpp
/// \authors  Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \version  1.0
/// \date     18/08/2021
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <faup/faup.h>
#include <map>
#include <string>

#include "../toolkit/rapidjson/document.h"
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

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;

};

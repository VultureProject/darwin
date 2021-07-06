/// \file     Generator.hpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     11/12/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "ATask.hpp"
#include "AGenerator.hpp"
#include "../toolkit/Files.hpp"
#include "../toolkit/rapidjson/document.h"
#include "../toolkit/RedisManager.hpp"

class Generator: public AGenerator {
public:
    Generator(size_t nb_task_threads);
    ~Generator() = default;

public:
    virtual std::shared_ptr<darwin::ATask>
    CreateTask(darwin::session_ptr_t s) noexcept override final;

    virtual long GetFilterCode() const;

protected:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;

private:
    bool ConfigRedis(std::string redis_socket_path);

    std::string _redis_list_name;
    std::string _redis_channel_name;
};

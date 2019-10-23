/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     30/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <faup/faup.h>
#include <map>
#include <maxminddb.h>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "Session.hpp"
#include "AGenerator.hpp"
#include "tensorflow/core/public/session.h"

class Generator: public AGenerator {
public:
    Generator() = default;
    ~Generator();

public:
    static constexpr int DEFAULT_MAX_TOKENS = 75;

    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    bool LoadFaupOptions();
    bool LoadTokenMap(const std::string &token_map_path);
    bool LoadModel(const std::string &model_path);

    // The doc is quite hard to find so here is a link to the version currently used on BSD
    // (see vulture-libtensorflow)
    // https://github.com/tensorflow/tensorflow/blob/r1.13/tensorflow/core/public/session.h
    std::shared_ptr<tensorflow::Session> _session;
    std::map<std::string, unsigned int> _token_map;
    unsigned int _max_tokens = 75;
    faup_options_t* _faup_options = nullptr;
};

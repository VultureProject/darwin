/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     30/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <faup/faup.h>
#include <map>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "Session.hpp"
#include "AGenerator.hpp"
#include "TfLiteHelper.hpp"
#include "tensorflow/lite/model.h"

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
    virtual bool ConfigureAlerting(const std::string& tags) override final;
    bool LoadFaupOptions();
    bool LoadTokenMap(const std::string &token_map_path);
    bool LoadModel(const std::string &model_path);

    std::shared_ptr<tflite::FlatBufferModel> _model;
    std::map<std::string, unsigned int> _token_map;
    unsigned int _max_tokens = 75;
    faup_options_t* _faup_options = nullptr;

    // Object that distributes the thread_local interpreters
    DarwinTfLiteInterpreterFactory _interpreter_factory;
};

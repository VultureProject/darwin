/// \file     Generator.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     22/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <map>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "Session.hpp"
#include "AGenerator.hpp"
#include "BufferThreadManager.hpp"
#include "AConnector.hpp"
#include "enums.hpp"

class Generator: public AGenerator {
public:
    Generator() = default;
    ~Generator() = default;

public:
    static constexpr int DEFAULT_MAX_TOKENS = 75;

    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    bool LoadConnectorsConfig(const rapidjson::Document &configuration);
    bool LoadInputs(const rapidjson::Value &inputs_array);
    valueType _typeToEnum(std::string type);
    bool LoadOutputs(const rapidjson::Value &outputs_array);
    std::shared_ptr<AConnector> _createOutput(std::string filter_type, std::string filter_socket_path, int interval, std::string redis_list_name);
    

    std::shared_ptr<BufferThreadManager> _buffer_thread_manager;
    std::string _redis_list_name;

    std::vector<std::pair<std::string, valueType>> _inputs;
    std::vector<std::shared_ptr<AConnector>> _outputs;
};

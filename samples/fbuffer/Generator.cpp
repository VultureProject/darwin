/// \file     Generator.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     22/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <boost/tokenizer.hpp>
#include <fstream>
#include <memory>

#include "base/Core.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "BufferTask.hpp"
#include "Generator.hpp"
#include "Connectors.hpp"

bool Generator::ConfigureAlerting(const std::string& tags) {
    return true;
}

void Generator::ConfigureNetworkObject(boost::asio::io_context &context) {
    DARWIN_LOGGER;
    for (auto &output : this->_output_configs) {
        std::shared_ptr<AConnector> newOutput = _createOutput(context, output);
        if (newOutput == nullptr) {
            DARWIN_LOG_WARNING("Buffer::Generator::ConfigureNetworkObject Unable to create the Output. Output " + 
                                    output._filter_type + " ignored.");   
            continue;
        }
        _outputs.push_back(newOutput);
    }
    this->_output_configs.clear();

    if (this->_outputs.empty()) {
        DARWIN_LOG_ERROR("Buffer::Generator::ConfigureNetworkObject no outputs available for filter buffer.");
    }

    this->_buffer_thread_manager = std::make_shared<BufferThreadManager>(this->_outputs.size());
    DARWIN_LOG_DEBUG("Buffer::Generator bufferThreadManager created");
    for (auto &output : this->_outputs) {
        DARWIN_LOG_DEBUG("Buffer::Generator starting a thread");
        this->_buffer_thread_manager->setConnector(output);
        bool ret = this->_buffer_thread_manager->ThreadStart();
        if (not ret) {
            DARWIN_LOG_CRITICAL("Buffer:: Generator:: Error when starting polling thread");
            return;
        }
        DARWIN_LOG_DEBUG("Buffer::Generator:: Thread created successfully");
    }

}

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Buffer:: Generator:: Loading classifier...");

    if (not configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("Buffer::Generator:: 'redis_socket_path' parameter missing, mandatory");
        return false;
    }
    if (not configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("Buffer:: Generator:: 'redis_socket_path' needs to be a string");
        return false;
    }
    std::string redis_socket_path = configuration["redis_socket_path"].GetString();

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    redis.SetUnixConnection(redis_socket_path);
    // Done in AlertManager before arriving here, but will allow better transition from redis singleton
    if(not redis.FindAndConnect()) {
        DARWIN_LOG_CRITICAL("Buffer:: Generator:: Could not connect to a redis!");
        return false;
    }
    DARWIN_LOG_INFO("Buffer:: Generator:: Redis configured successfuly");

// TODO this section will desapear in the final version
    if (not configuration.HasMember("log_file_path")) {
        DARWIN_LOG_CRITICAL("Buffer::Generator:: 'log_file_path' parameter missing, mandatory");
        return false;
    }
    if (not configuration["log_file_path"].IsString()) {
        DARWIN_LOG_CRITICAL("Buffer:: Generator:: 'log_file_path' needs to be a string");
        return false;
    }
    std::string log_file_path = configuration["log_file_path"].GetString();

    if (not LoadConnectorsConfig(configuration)) {
        DARWIN_LOG_CRITICAL("Buffer::Generator:: Connectors config failed, filter will stop.");
        return false;
    }

    return true;
}

bool Generator::LoadConnectorsConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Buffer::Generator:: Loading connectors config...");

    if (not configuration.HasMember("input_format")) {
        DARWIN_LOG_CRITICAL("Buffer::Generator::LoadConnectorConfig 'input_format' parameter missing, mandatory (can be empty if needed)");
        return false;
    }
    if (not configuration["input_format"].IsArray()) {
        DARWIN_LOG_CRITICAL("Buffer::Generator::LoadConnectorConfig 'input_format' needs to be an array");
        return false;
    }
    const rapidjson::Value &inputs = configuration["input_format"];

    if(not configuration.HasMember("outputs")) {
        DARWIN_LOG_CRITICAL("Buffer::Generator::LoadConnectorConfig 'outputs' parameter missing, mandatory (can be empty if needed)");
        return false;
    }
    if (not configuration["outputs"].IsArray()) {
        DARWIN_LOG_CRITICAL("Buffer::Generator::LoadConnectorConfig 'outputs' needs to be an array");
        return false;
    }
    const rapidjson::Value &outputs = configuration["outputs"];

    if (not LoadInputs(inputs) or not LoadOutputs(outputs)) {
        DARWIN_LOG_CRITICAL("Buffer::Generator::LoadConnectorConfig 'input_format' or 'outputs' is not correctly configured.");
        return false;
    }

    DARWIN_LOG_DEBUG("Buffer::Generator:: connectors config load complete");

    return true;
}

bool Generator::LoadInputs(const rapidjson::Value &array) {
    DARWIN_LOGGER;
    for (rapidjson::SizeType i = 0; i < array.Size(); i++) {
        if (not array[i].HasMember("name") or not array[i]["name"].IsString()) {
            DARWIN_LOG_WARNING("Buffer::Generator::LoadInputs 'name' field missing or is not a string in input_format. Input ignored.");
            continue;
        } else if (not array[i].HasMember("type") or not array[i]["type"].IsString()) {
            DARWIN_LOG_WARNING("Buffer::Generator::LoadInputs 'type' field missing or is not a string in input_format. " + std::string(array[i]["name"].GetString()) + " ignored.");
            continue;
        } else {
            std::pair<std::string, valueType> input(array[i]["name"].GetString(), _typeToEnum(array[i]["type"].GetString()));
            if (input.second == UNKNOWN) {
                DARWIN_LOG_WARNING("Buffer::Generator::LoadInputs 'type' value is not recognized. " + std::string(array[i]["name"].GetString()) + " ignored.");
                continue;
            }
            _inputs.push_back(input);
        }
    }
    if (_inputs.empty()) {
        DARWIN_LOG_CRITICAL("Buffer::Generator::LoadInputs No inputs available. The filter is about to Stop");
        return false;
    }
    return true;
}

valueType Generator::_typeToEnum(std::string type) {
    if (type == "string")
        return STRING;
    if (type == "int")
        return INT;
    if (type == "double")
        return DOUBLE;
    if (type == "float")
        return FLOAT;
    return UNKNOWN;
}

std::shared_ptr<AConnector> Generator::_createOutput(boost::asio::io_context &context, OutputConfig &output_config) {
    DARWIN_LOGGER;
    std::string filter_type = output_config._filter_type;
    std::string filter_socket_path = output_config._filter_socket_path;
    unsigned int interval = output_config._interval;
    std::vector<std::pair<std::string, std::string>> redis_lists = output_config._redis_lists;
    unsigned int nb_log_lines = output_config._required_log_lines;

    if (filter_type == "fanomaly") {
        std::shared_ptr<fAnomalyConnector> output = std::make_shared<fAnomalyConnector>(context, filter_socket_path, interval, redis_lists, nb_log_lines);
        return std::static_pointer_cast<AConnector>(output);
    } else if (filter_type == "fsofa") {
        std::shared_ptr<fSofaConnector> output = std::make_shared<fSofaConnector>(context, filter_socket_path, interval, redis_lists, nb_log_lines);
        return std::static_pointer_cast<AConnector>(output);
    } else if (filter_type == "fbuffer") {
        std::shared_ptr<fBufferConnector> output = std::make_shared<fBufferConnector>(context, filter_socket_path, interval, redis_lists, nb_log_lines);
        return std::static_pointer_cast<AConnector>(output);
    }
    DARWIN_LOG_WARNING("Buffer::Generator::_createOutput " + filter_type + " is not recognized as a valid filter.");
    return nullptr;
}

bool Generator::LoadOutputs(const rapidjson::Value &array) {
    DARWIN_LOGGER;

    std::vector<std::pair<std::string, std::string>> redis_lists;
    for (rapidjson::SizeType i = 0; i < array.Size(); i++) {
        redis_lists.clear();
        if (not array[i].HasMember("filter_type") or not array[i]["filter_type"].IsString()) {
            DARWIN_LOG_WARNING("Buffer::Generator::LoadOutputs 'filter_type' field missing or is not a string in outputs. Output ignored.");
            continue;
        } 
        if (not array[i].HasMember("filter_socket_path") or not array[i]["filter_socket_path"].IsString()) {
            DARWIN_LOG_WARNING("Buffer::Generator::LoadOutputs 'filter_socket_path' field missing or is not a string in outputs. Output " + 
                                std::string(array[i]["filter_type"].GetString()) + " ignored.");
            continue;
        } 
        if (not array[i].HasMember("interval") or not array[i]["interval"].IsInt64()) {
            DARWIN_LOG_WARNING("Buffer::Generator::LoadOutputs 'interval' field missing or is not an integer in outputs. Output " + 
                                std::string(array[i]["filter_type"].GetString()) + " ignored.");
            continue;
        }
        if (not array[i].HasMember("redis_lists") or not array[i]["redis_lists"].IsArray()) {
            DARWIN_LOG_WARNING("Buffer::Generator::LoadOuptuts 'redis_lists' field missing or is not an array. Output " +
                                std::string(array[i]["filter_type"].GetString()) + " ignored");
            continue;
        }
        redis_lists = this->FormatRedisListVector(array[i]["redis_lists"]);
        if (redis_lists.empty()) {
            DARWIN_LOG_WARNING("Buffer::Generator::LoadOuptuts no well formatted redis_list . Output " +
                                std::string(array[i]["filter_type"].GetString()) + " ignored");
            continue;
        }
        if (not array[i].HasMember("required_log_lines") or not array[i]["required_log_lines"].IsInt64()) {
            DARWIN_LOG_WARNING("Buffer::Generator::LoadOuptuts 'required_log_lines' field missing or is not an integer. Output " +
                                std::string(array[i]["filter_type"].GetString()) + " ignored");
            continue;
        }
        std::string filter_type = array[i]["filter_type"].GetString();
        std::string filter_socket_path = array[i]["filter_socket_path"].GetString();
        unsigned int interval = array[i]["interval"].GetUint64();
        unsigned int required_log_lines = array[i]["required_log_lines"].GetUint64();
        OutputConfig tmp(filter_type, filter_socket_path, interval, redis_lists, required_log_lines);
        this->_output_configs.emplace_back(tmp);
        redis_lists.clear();
    }
    if (_output_configs.empty()) {
        DARWIN_LOG_CRITICAL("Buffer::Generator::LoadOutputs No outputs available. The filter is about to Stop");
        return false;
    }
    DARWIN_LOG_DEBUG("Buffer::Generator::LoadOutputs there are " + std::to_string(_outputs.size()) + " output(s) available.");
    return true;
}

std::vector<std::pair<std::string, std::string>> Generator::FormatRedisListVector(const rapidjson::Value &array) const {
    DARWIN_LOGGER;
    std::vector<std::pair<std::string, std::string>> vector;

    for (unsigned int i = 0; i < array.Size(); i++) {
        if (not array[i].HasMember("source") or not array[i]["source"].IsString()) {
            DARWIN_LOG_WARNING("Buffer::Generator::FormatRedisListVector no source specified or not a string, source ignored.");
            continue;
        }
        if (not array[i].HasMember("name") or not array[i]["name"].IsString()) {
            DARWIN_LOG_WARNING("Buffer::Generator::FormatRedisListVector no name specified or not a string, source ignored.");
            continue;
        }
        vector.emplace_back(array[i]["source"].GetString(), array[i]["name"].GetString());
    }
    return vector;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Buffer::CreateTask Starting creating a task");
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<BufferTask>(socket, manager, _cache, _cache_mutex, this->_inputs, this->_outputs));
}
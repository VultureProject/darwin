/// \file     Generator.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     30/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <boost/tokenizer.hpp>
#include <faup/faup.h>
#include <faup/options.h>
#include <fstream>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "DGATask.hpp"
#include "Generator.hpp"
#include "tensorflow/core/framework/graph.pb.h"

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("DGA:: Generator:: Loading classifier...");

    std::string token_map_path;
    std::string model_path;

    if (!configuration.HasMember("token_map_path")) {
        DARWIN_LOG_CRITICAL("DGA:: Generator:: Missing parameter: 'token_map_path'");
        return false;
    }

    if (!configuration["token_map_path"].IsString()) {
        DARWIN_LOG_CRITICAL("DGA:: Generator:: 'token_map_path' needs to be a string");
        return false;
    }

    token_map_path = configuration["token_map_path"].GetString();

    if (!configuration.HasMember("model_path")) {
        DARWIN_LOG_CRITICAL("DGA:: Generator:: Missing parameter: 'model_path'");
        return false;
    }

    if (!configuration["model_path"].IsString()) {
        DARWIN_LOG_CRITICAL("DGA:: Generator:: 'model_path' needs to be a string");
        return false;
    }

    model_path = configuration["model_path"].GetString();

    if (!configuration.HasMember("max_tokens")) {
        DARWIN_LOG_CRITICAL("DGA:: Generator:: 'max_tokens' not provided. Setting default value: " +
                            std::to_string(DEFAULT_MAX_TOKENS));

        _max_tokens = DEFAULT_MAX_TOKENS;
    } else {
        if (!configuration["max_tokens"].IsUint()) {
            DARWIN_LOG_CRITICAL("DGA:: Generator:: 'max_tokens' needs to be an unsigned integer");
            return false;
        }

        _max_tokens = configuration["max_tokens"].GetUint();
    }

    return LoadFaupOptions() && LoadTokenMap(token_map_path) && LoadModel(model_path);
}

bool Generator::LoadTokenMap(const std::string &token_map_path) {
    DARWIN_LOGGER;
    std::string current_line;
    boost::char_separator<char> separator{","};

    DARWIN_LOG_DEBUG("DGA:: LoadTokenMap:: Loading token file '" + token_map_path + "'...");

    std::ifstream token_map_stream;
    token_map_stream.open(token_map_path, std::ifstream::in);

    if (!token_map_stream.is_open()) {
        DARWIN_LOG_ERROR("DGA:: LoadTokenMap:: Could not open the token map file");

        return false;
    }

    while (!token_map_stream.eof() && std::getline(token_map_stream, current_line)) {
        boost::tokenizer<boost::char_separator<char>> tokens(current_line, separator);

        boost::tokenizer<boost::char_separator<char>>::iterator key(tokens.begin());
        if (key==tokens.end()){
            DARWIN_LOG_CRITICAL("DGA:: LoadTokenMap:: Error when load token map : Blank line");
            token_map_stream.close();
            return false;
        }

        boost::tokenizer<boost::char_separator<char>>::iterator value(++tokens.begin());
        if (value==tokens.end()){
            DARWIN_LOG_CRITICAL("DGA:: LoadTokenMap:: Error when load token map on this line : "
                                + current_line);
            token_map_stream.close();
            return false;
        }

        _token_map[*key] = (unsigned int)std::stoi(*value);

        try{
            _token_map[*key] = (unsigned int)std::stoi(*value);
        }catch(const std::invalid_argument& ia){
            std::string exc(ia.what());
            DARWIN_LOG_CRITICAL("DGA:: LoadTokenMap:: Value " + *value + " is invalid : " + exc);
        }catch(const std::out_of_range& oor){
            std::string exc(oor.what());
            DARWIN_LOG_CRITICAL("DGA:: LoadTokenMap:: Value is out of range : " + exc);
        }
    }

    token_map_stream.close();

    DARWIN_LOG_DEBUG("DGA:: LoadTokenMap:: Token map loaded");

    return true;
}

bool Generator::LoadModel(const std::string &model_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Generator:: LoadModel:: Loading model...");
    tensorflow::GraphDef graph_def;
    tensorflow::Status load_graph_status = ReadBinaryProto(tensorflow::Env::Default(), model_path, &graph_def);

    if (!load_graph_status.ok()) {
        DARWIN_LOG_ERROR("Generator:: LoadModel:: Failed to load graph at " + model_path + ". Status: " + load_graph_status.ToString());
        return false;
    }

    _session.reset(tensorflow::NewSession(tensorflow::SessionOptions()));
    tensorflow::Status session_create_status = _session->Create(graph_def);

    if (!session_create_status.ok()) {
        DARWIN_LOG_ERROR("Generator:: LoadModel:: Failed to create a new session. Status: " + session_create_status.ToString());
        return false;
    }

    return true;
}

bool Generator::LoadFaupOptions() {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("DGA:: Generator:: Initializing faup options...");
    _faup_options = faup_options_new();

    if (!_faup_options) {
        DARWIN_LOG_CRITICAL("DGA:: Generator:: Cannot allocate faup options");
        return false;
    }
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<DGATask>(socket, manager, _cache, _cache_mutex, _session, _faup_options, _token_map, _max_tokens));
}

Generator::~Generator() {
    if (_session) {
        tensorflow::Status s = _session->Close();
        if (not s.ok()) {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("DGA:: Generator:: Unable to close the Tensorflow Session");
        }
    }
}

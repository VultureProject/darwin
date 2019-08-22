/// \file     Generator.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     10/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "ReputationTask.hpp"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Reputation:: Generator:: Configuring...");

    if (!SetUpClassifier(configFile)) return false;

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("Reputation:: Generator:: Configured");
    return true;
}

bool Generator::SetUpClassifier(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Reputation:: Generator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("Reputation:: Generator:: Parsing configuration from \"" + configuration_file_path + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("Reputation:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("Reputation:: Generator:: Reading configuration...");

    if (!LoadClassifier(configuration)) {
        return false;
    }

    conf_file_stream.close();

    return true;
}

bool Generator::LoadClassifier(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Reputation:: Generator:: Loading classifier...");
    std::string db;

    if (!configuration.HasMember("mmdb_database")) {
        DARWIN_LOG_CRITICAL("Reputation:: Generator:: Missing parameter: \"mmdb_database\"");
        return false;
    }

    if (!configuration["mmdb_database"].IsString()) {
        DARWIN_LOG_CRITICAL("Reputation:: Generator:: \"mmdb_database\" needs to be a string");
        return false;
    }
   
    db = configuration["mmdb_database"].GetString();
    
    auto status = MMDB_open(db.c_str(), MMDB_MODE_MMAP, &_database);
    if (status != MMDB_SUCCESS) {
        DARWIN_LOG_CRITICAL("Reputation:: Generator:: Configure:: Cannot open database");
        return false;
    }

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<ReputationTask>(socket, manager, _cache, &_database));
}

Generator::~Generator() {
    MMDB_close(&_database);
}
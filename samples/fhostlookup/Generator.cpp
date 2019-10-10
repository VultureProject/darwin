/// \file     Generator.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     07/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "HostLookupTask.hpp"
#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Configuring...");

    if (!SetUpClassifier(configFile)) return false;

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Configured");
    return true;
}

bool Generator::SetUpClassifier(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Parsing configuration from \"" + configuration_file_path + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("HostLookup:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Reading configuration...");

    if (!LoadClassifier(configuration)) {
        return false;
    }

    conf_file_stream.close();

    return true;
}

bool Generator::LoadClassifier(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Loading classifier...");
    std::string line, db;

    if (!configuration.HasMember("database")) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Missing parameter: \"database\"");
        return false;
    }

    if (!configuration["database"].IsString()) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: \"database\" needs to be a string");
        return false;
    }

    db = configuration["database"].GetString();
    std::ifstream file(db.c_str());
    std::string host;

    if (!file) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Cannot open host database");
        return false;
    }

    while (!darwin::files_utils::GetLineSafe(file, host).eof()) {
        if(file.fail() or file.bad()){
            DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Error when reading host database");
            return false;
        }
        if (!host.empty()){
            _database.insert({host,0});
        }
    }

    file.close();
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<HostLookupTask>(socket, manager, _cache, _database));
}

/// \file     Generator.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     30/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <fstream>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "NSigmaTask.hpp"
#include "Generator.hpp"
#include "AlertManager.hpp"

bool Generator::ConfigureAlerting(const std::string& tags) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("NSigma:: ConfigureAlerting:: Configuring Alerting");
    DARWIN_ALERT_MANAGER_SET_FILTER_NAME(DARWIN_FILTER_NAME);
    DARWIN_ALERT_MANAGER_SET_RULE_NAME(DARWIN_ALERT_RULE_NAME);
    if (tags.empty()) {
        DARWIN_LOG_DEBUG("NSigma:: ConfigureAlerting:: No alert tags provided in the configuration. Using default.");
        DARWIN_ALERT_MANAGER_SET_TAGS(DARWIN_ALERT_TAGS);
    } else {
        DARWIN_ALERT_MANAGER_SET_TAGS(tags);
    }
    return true;
}

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("NSigma:: Generator:: Loading Configuration...");

    std::string token_map_path;
    std::string model_path;

    if (!configuration.HasMember("n_sigma")) {
        DARWIN_LOG_DEBUG("NSigma:: Generator:: Missing parameter: 'n_sigma', using default ...");
    } else {
        n_sigma = configuration["n_sigma"].GetInt();
    }

    return true;
}


darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<NSigmaTask>(socket, manager, _cache, _cache_mutex, n_sigma));
}

Generator::~Generator() = default;

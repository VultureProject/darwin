/// \file     Generator.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "AnomalyTask.hpp"
#include "ASession.hpp"
#include "AlertManager.hpp"

Generator::Generator(size_t nb_task_threads) 
    : AGenerator(nb_task_threads) 
{

}

bool Generator::ConfigureAlerting(const std::string& tags) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Anomaly:: ConfigureAlerting:: Configuring Alerting");
    DARWIN_ALERT_MANAGER_SET_FILTER_NAME(DARWIN_FILTER_NAME);
    DARWIN_ALERT_MANAGER_SET_RULE_NAME(DARWIN_ALERT_RULE_NAME);
    if (tags.empty()) {
        DARWIN_LOG_DEBUG("Anomaly:: ConfigureAlerting:: No alert tags provided in the configuration. Using default.");
        DARWIN_ALERT_MANAGER_SET_TAGS(DARWIN_ALERT_TAGS);
    } else {
        DARWIN_ALERT_MANAGER_SET_TAGS(tags);
    }
    return true;
}

bool Generator::LoadConfig(const rapidjson::Document &configuration __attribute__((unused))) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Anomaly:: Generator:: Configured");
    return true;
}

std::shared_ptr<darwin::ATask>
Generator::CreateTask(darwin::session_ptr_t s) noexcept {
    return std::static_pointer_cast<darwin::ATask>(
            std::make_shared<AnomalyTask>(_cache, _cache_mutex, s, s->_packet)
        );
}

long Generator::GetFilterCode() const {
    return DARWIN_FILTER_ANOMALY;
}
/// \file     AnomalyTask.hpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>
#include "ATask.hpp"
#include "DarwinPacket.hpp"
#include "ASession.fwd.hpp"
#include "TAnomalyThreadManager.hpp"

#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/lru_cache.hpp"

#define DARWIN_FILTER_TANOMALY 0x544D4C59
#define DARWIN_FILTER_NAME "anomaly"
#define DARWIN_ALERT_RULE_NAME "Abnormal Number of Unique Port Connexion"
#define DARWIN_ALERT_TAGS "[\"attack.discovery\", \"attack.t1046\", \"attack.command_and_control\", \"attack.defense_evasion\", \"attack.t1205\"]"


// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class AnomalyTask: public darwin::ATask {
public:
    explicit AnomalyTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            std::mutex& cache_mutex,
                            darwin::session_ptr_t s,
                            darwin::DarwinPacket& packet,
                            std::shared_ptr<AnomalyThreadManager> vat,
                            std::string redis_list_name);
    ~AnomalyTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Parse a line from the body.
    /// \return true on parsing success, false otherwise
    /// \warning modifies _entry class attribute
    bool ParseLine(rapidjson::Value& line) final;

    /// Add the _entry parsed to REDIS
    /// \return true on success, false otherwise.
    bool REDISAddEntry() noexcept;

private:
    std::string _redis_list_name;
    std::string _entry;
    std::shared_ptr<AnomalyThreadManager> _anomaly_thread_manager = nullptr;
};

/// \file     EndTask.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <config.hpp>
#include <string>
#include <string.h>
#include <thread>

#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "EndTask.hpp"
#include "Logger.hpp"
#include "protocol.h"

EndTask::EndTask(boost::asio::local::stream_protocol::socket& socket,
                                                     darwin::Manager& manager,
                                                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache)
        : Session{"end", socket, manager, cache}{
    _is_cache = _cache != nullptr;
}

void EndTask::operator()() {
    std::string evt_id, nb_result;

    evt_id = Evt_idToString();
    nb_result = std::to_string(_header.certitude_size);
    REDISAdd(evt_id, nb_result);
    Workflow();
}

long EndTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_END;
}

void EndTask::Workflow() {
    switch (_header.response) {
        case DARWIN_RESPONSE_SEND_BOTH:
            SendToDarwin();
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_BACK:
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_DARWIN:
            SendToDarwin();
            break;
        case DARWIN_RESPONSE_SEND_NO:
        default:
            break;
    }
}

bool EndTask::REDISAdd(const std::string& evt_id, const std::string& nb_result) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("EndTask::REDISAdd:: Add to key 'darwin_<" +  evt_id + ">' the number :" + nb_result);

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    if(redis.Query(std::vector<std::string>{"SET", "darwin_" + evt_id, nb_result}) != REDIS_REPLY_STATUS) {
        DARWIN_LOG_ERROR("EndTask::REDISAdd:: Not the expected Redis response ");
        return false;
    }

    return true;
}

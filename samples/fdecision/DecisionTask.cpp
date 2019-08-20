/// \file     DecisionTask.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     17/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <thread>
#include "Logger.hpp"
#include "DecisionTask.hpp"
#include "protocol.h"

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"

DecisionTask::DecisionTask(boost::asio::local::stream_protocol::socket& socket,
                           darwin::Manager& manager,
                           std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                           request_data_map_t* data,
                           std::mutex* mut)
        : Session{socket, manager, cache}, _data{data}, _data_mutex{mut} {
    _is_cache = _cache != nullptr;
}

void DecisionTask::operator()() {
    for (const std::string &data : _data_list) {
        _current_data = data;

        if (header.type == DARWIN_PACKET_FILTER) {
            SaveInfo(data);
        }

        if (header.type == DARWIN_PACKET_OTHER) {
            Decide(data);
        }
    }
}

void DecisionTask::Decide(const std::string &data) {
//    DARWIN_LOGGER;
//    darwin_filter_packet_t response = {
//            .type = DARWIN_PACKET_FILTER,
//            .response = DARWIN_RESPONSE_SEND_NO,
//            .filter_code = DARWIN_FILTER_DECISION,
//            .body_size = 0,
//    };
//
//    DARWIN_LOG_INFO("DecisionTask:: Received instruction to send decision");
//    {
//        _data_mutex->lock();
//        auto i = _data->find(data);
//
//        if (i == _data->end()) {
//            DARWIN_LOG_NOTICE(std::string("DecisionTask:: Unable to find results "
//                              "associated to request: ") + data);
//            _data_mutex->unlock();
//            this->Send(response, nullptr);
//            return;
//        } else {
//            DARWIN_LOG_DEBUG("DecisionTask:: Found: " + i->second);
//        }
//        if (i->second == "FR")
//            response.certitude = 1;
//        _data_mutex->unlock();
//    }
//    DARWIN_LOG_DEBUG("DecisionTask:: Sending result " +
//              std::to_string(response.certitude));
//    this->Send(response, nullptr);
}

void DecisionTask::SaveInfo(const std::string &data) {
//    DARWIN_LOGGER;
//
//    DARWIN_LOG_INFO("DecisionTask:: Received info from a filter");
//    _data_mutex->lock();
//    (*_data)[_ip] = _data_string;
//    _data_mutex->unlock();
}

bool DecisionTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("DecisionTask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("DecisionTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("DecisionTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("DecisionTask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            auto items = request.GetArray();

            if (items.Size() != 1) {
                DARWIN_LOG_ERROR(
                        "DecisionTask:: ParseBody: You must provide exactly one argument per request: the domain"
                );

                return false;
            }

            if (!items[0].IsString()) {
                DARWIN_LOG_ERROR("DecisionTask:: ParseBody: The domain sent must be a string");
                return false;
            }

            std::string data = items[0].GetString();
            _data_list.push_back(data);
            DARWIN_LOG_DEBUG("DecisionTask:: ParseBody: Parsed element: " + data);
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("DecisionTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}

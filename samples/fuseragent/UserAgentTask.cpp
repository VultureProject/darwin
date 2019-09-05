/// \file     UserAgentTask.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     16/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <boost/tokenizer.hpp>
#include <cmath>
#include <protocol.h>
#include <string.h>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "Logger.hpp"
#include "protocol.h"
#include "tensorflow/core/framework/tensor.h"
#include "UserAgentTask.hpp"

const std::vector<std::string> UserAgentTask::USER_AGENT_CLASSES({"Desktop", "Tool", "Libraries", "Good bot", "Bad bot", "Mail", "IOT", "Mobile"});

UserAgentTask::UserAgentTask(boost::asio::local::stream_protocol::socket& socket,
                             darwin::Manager& manager,
                             std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                             std::shared_ptr<tensorflow::Session> &session,
                             std::map<std::string, unsigned int> &token_map,
                             const unsigned int max_tokens)
        : Session{socket, manager, cache}, _session{session}, _max_tokens{max_tokens}, _token_map{token_map} {
    _is_cache = _cache != nullptr;
}


xxh::hash64_t UserAgentTask::GenerateHash() {
    return xxh::xxhash<64>(_current_user_agent);
}

long UserAgentTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_USER_AGENT;
}

void UserAgentTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    for (const std::string &user_agent : _user_agents) {
        SetStartingTime();
        // We have a generic hash function, which takes no arguments as these can be of very different types depending
        // on the nature of the filter
        // So instead, we set an attribute corresponding to the current user agent being processed, to compute the hash
        // accordingly
        _current_user_agent = user_agent;
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                if (is_log && (certitude>=_threshold)){
                    _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() + "\", \"user_agent\": \"" + user_agent + "\", \"ua_classification\": " + std::to_string(certitude) + "}\n";
                }
                _certitudes.push_back(certitude);
                DARWIN_LOG_DEBUG("UserAgentTask:: processed entry in "
                                 + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                continue;
            }
        }

        certitude = Predict(user_agent);
        if (is_log && (certitude>=_threshold)){
            _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() + "\", \"user_agent\": \"" + user_agent + "\", \"ua_classification\": " + std::to_string(certitude) + "}\n";
        }
        _certitudes.push_back(certitude);

        if (_is_cache) {
            SaveToCache(hash, certitude);
        }
        DARWIN_LOG_DEBUG("UserAgentTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }

    Workflow();
    _user_agents = std::vector<std::string>();
}

void UserAgentTask::Workflow() {
    switch (header.response) {
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

UserAgentTask::~UserAgentTask() = default;

void UserAgentTask::UserAgentTokenizer(const std::string &user_agent, std::vector<std::size_t> &ua_tokens) {
    DARWIN_LOGGER;
    boost::tokenizer<boost::char_separator<char>> tokens(user_agent, _separator);
    std::map<unsigned int, unsigned int> tmp_ua_tokens;
    std::size_t size = 0;

    for (const auto& token : tokens) {
        auto it = _token_map.find(token);

        // token is not in our dictionary
        if (it == _token_map.end()) {
            it = _token_map.find("UNK");

            // "UNK" token is not in the dictionary: this should NEVER happen!
            if (it == _token_map.end()) {
                DARWIN_LOG_ERROR("UserAgentTokenizer:: Error: Invalid dictionary file: \"UNK\" token not found!");
                continue;
            }
        }

        tmp_ua_tokens[size++] = it->second;
    }

    std::size_t index = _max_tokens - size;

    for (size = index; size < _max_tokens; ++size) {
        ua_tokens[size] = tmp_ua_tokens[size - index];
    }
}

unsigned int UserAgentTask::Predict(const std::string &user_agent) {
    DARWIN_LOGGER;

    std::vector<std::size_t> ua_tokens(_max_tokens, 0);
    UserAgentTokenizer(user_agent, ua_tokens);

    tensorflow::Tensor input_tensor(tensorflow::DT_FLOAT, tensorflow::TensorShape({1, _max_tokens}));
    auto input_tensor_mapped = input_tensor.tensor<float, 2>();
    std::size_t index = 0;

    for (const auto &ua_token : ua_tokens) {
        input_tensor_mapped(index++) = ua_token;
    }

    std::vector<tensorflow::Tensor> output_tensors;

    tensorflow::Status run_status = _session->Run({{"embedding_4_input", input_tensor}},
                                                  {"output_node0"},
                                                  {},
                                                  &output_tensors);

    if (!run_status.ok()) {
        DARWIN_LOG_ERROR("Predict:: Error: Running model failed: " + run_status.ToString());
        return 101;
    }

    std::map<std::string, float> result;
    index = 0;

    for (const auto &ua_type : USER_AGENT_CLASSES) {
        result[ua_type] = output_tensors[0].tensor<float, 2>()(index++);
    }

    DARWIN_LOG_DEBUG("Predict:: Results:");

    unsigned int certitude = 0;

    for (auto it = result.begin(); it != result.end(); ++it) {
        DARWIN_LOG_DEBUG("Predict:: \"" + it->first + "\": " + std::to_string(it->second));

        if (it->first == "Bad bot") {
            certitude = (unsigned int)round((double)it->second * 100);
            break;
        }
    }

    return certitude;
}

bool UserAgentTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("UserAgentTask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("UserAgentTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("UserAgentTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("UserAgentTask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            auto items = request.GetArray();

            if (items.Size() != 1) {
                DARWIN_LOG_ERROR(
                        "UserAgentTask:: ParseBody: You must provide exactly one argument per request: the user agent"
                );

                return false;
            }

            if (!items[0].IsString()) {
                DARWIN_LOG_ERROR("UserAgentTask:: ParseBody: The user agent sent must be a string");
                return false;
            }

            std::string user_agent = items[0].GetString();
            _user_agents.push_back(user_agent);
            DARWIN_LOG_DEBUG("UserAgentTask:: ParseBody: Parsed element: " + user_agent);
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("UserAgentTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}

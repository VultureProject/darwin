/// \file     DGATask.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     30/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/tokenizer.hpp>
#include <faup/decode.h>
#include <faup/output.h>
#include <string.h>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/Validators.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "DGATask.hpp"
#include "Logger.hpp"
#include "protocol.h"
#include "tensorflow/core/framework/tensor.h"

DGATask::DGATask(boost::asio::local::stream_protocol::socket& socket,
                 darwin::Manager& manager,
                 std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                 std::shared_ptr<tensorflow::Session> &session,
                 faup_handler_t *faup_handler,
                 std::map<std::string, unsigned int> &token_map,
                 const unsigned int max_tokens)
        : Session{socket, manager, cache}, _session{session}, _max_tokens{max_tokens}, _token_map{token_map},
          _faup_handler(faup_handler) {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t DGATask::GenerateHash() {
    return xxh::xxhash<64>(_current_domain);
}

long DGATask::GetFilterCode() noexcept {
    return DARWIN_FILTER_DGA;
}

void DGATask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    for (const std::string &domain : _domains) {
        SetStartingTime();
        // We have a generic hash function, which takes no arguments as these can be of very different types depending
        // on the nature of the filter
        // So instead, we set an attribute corresponding to the current domain being processed, to compute the hash
        // accordingly
        _current_domain = domain;
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                if (is_log && (certitude>=_threshold)){
                    _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + GetTime() + "\", \"domain\": \""+ domain + "\", \"dga_prob\": " + std::to_string(certitude) + "}\n";
                }
                _certitudes.push_back(certitude);
                DARWIN_LOG_DEBUG("DGATask:: processed entry in "
                                 + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                continue;
            }
        }

        certitude = Predict();
        if (is_log && (certitude>=_threshold)){
            _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + GetTime() + "\", \"domain\": \""+ domain + "\", \"dga_prob\": "+std::to_string(certitude) + "}\n";
        }
        _certitudes.push_back(certitude);
        if (_is_cache) {
            SaveToCache(hash, certitude);
        }
        DARWIN_LOG_DEBUG("DGATask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }

    Workflow();
    _domains = std::vector<std::string>();
}

std::string DGATask::GetTime(){
    char str_time[256];
    time_t rawtime;
    struct tm * timeinfo;
    std::string res;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(str_time, sizeof(str_time), "%F%Z%T%z", timeinfo);
    res = str_time;

    return res;
}

void DGATask::Workflow(){
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

DGATask::~DGATask() = default;

bool DGATask::ExtractRegisteredDomain(std::string &to_predict) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ExtractRegisteredDomain:: Analyzing domain \"" + _current_domain + "\"");

    bool is_domain_valid = darwin::validator::IsDomainValid(_current_domain);

    if (!is_domain_valid) return false;

    try {
        faup_decode(_faup_handler, _current_domain.c_str(), _current_domain.size());

        std::string tld = _current_domain.substr(
                faup_get_tld_pos(_faup_handler),
                faup_get_tld_size(_faup_handler)
        );

        if (tld == "yu" || tld == "za") {
            DARWIN_LOG_DEBUG("ExtractRegisteredDomain::TLD found is \"" + tld + "\", returning false");
            return false;
        }

        DARWIN_LOG_DEBUG("ExtractRegisteredDomain:: TLD found is \"" + tld + "\"");

        std::string registered_domain = _current_domain.substr(
                faup_get_domain_without_tld_pos(_faup_handler),
                faup_get_domain_without_tld_size(_faup_handler)
        );

        DARWIN_LOG_DEBUG("ExtractRegisteredDomain:: Registered domain found is \"" + registered_domain + "\"");

        to_predict = registered_domain + "." + tld;

    } catch (const std::out_of_range& exception) {
        DARWIN_LOG_INFO("ExtractRegisteredDomain:: domain appears to be invalid: \"" + _current_domain + "\"");
        return false;

    } catch (const std::exception& exception) {
        DARWIN_LOG_ERROR("ExtractRegisteredDomain:: Unexpected error with domain \"" + _current_domain + "\": \"" +
                         exception.what() + "\"");

        return false;
    } catch (...) {
        DARWIN_LOG_ERROR("ExtractRegisteredDomain:: Unknown error with domain \"" + _current_domain + "\"");
        return false;
    }

    return true;
}

void DGATask::DomainTokenizer(std::vector<std::size_t> &domain_tokens, const std::string &to_predict) {
    DARWIN_LOGGER;
    boost::tokenizer<boost::char_separator<char>> tokens(to_predict, _separator);
    std::map<unsigned int, unsigned int> tmp_domain_tokens;
    std::size_t size = 0;

    for (const auto &c_character : to_predict) {
        auto character = std::string(1, c_character);
        auto it = _token_map.find(character);

        // token is not in our dictionary
        if (it == _token_map.end()) {
            DARWIN_LOG_DEBUG("DomainTokenizer:: No token has been found for the character \"" + character + "\"");
            continue;
        }

        tmp_domain_tokens[size++] = it->second;
    }

    std::size_t index = _max_tokens - size;

    for (size = index; size < _max_tokens; ++size) {
        domain_tokens[size] = tmp_domain_tokens[size - index];
    }
}

unsigned int DGATask::Predict() {
    std::string to_predict;

    if (!ExtractRegisteredDomain(to_predict)) {
        return 101;
    }

    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Predict:: Classifying \"" + to_predict + "\"...");

    std::vector<std::size_t> domain_tokens(_max_tokens, 0);
    DomainTokenizer(domain_tokens, to_predict);

    tensorflow::Tensor input_tensor(tensorflow::DT_FLOAT, tensorflow::TensorShape({1, _max_tokens}));
    auto input_tensor_mapped = input_tensor.tensor<float, 2>();
    std::size_t index = 0;

    for (const auto &domain_token : domain_tokens) {
        input_tensor_mapped(index++) = domain_token;
    }

    std::vector<tensorflow::Tensor> output_tensors;

    tensorflow::Status run_status = _session->Run({{"embedding_1_input", input_tensor}},
                                                  {"activation_1/Sigmoid"},
                                                  {},
                                                  &output_tensors);

    if (!run_status.ok()) {
        DARWIN_LOG_ERROR("Predict:: Error: Running model failed: " + run_status.ToString());
        return 101;
    }

    std::map<std::string, float> result;

    unsigned int certitude = round(output_tensors[0].tensor<float, 2>()(0) * 100);
    DARWIN_LOG_DEBUG("Predict:: DGA score obtained: " + std::to_string(certitude));

    return certitude;
}

bool DGATask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("DGATask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("DGATask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("DGATask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("DGATask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            auto items = request.GetArray();

            if (items.Size() != 1) {
                DARWIN_LOG_ERROR(
                        "DGATask:: ParseBody: You must provide exactly one argument per request: the domain"
                );

                return false;
            }

            if (!items[0].IsString()) {
                DARWIN_LOG_ERROR("DGATask:: ParseBody: The domain sent must be a string");
                return false;
            }

            std::string domain = items[0].GetString();
            _domains.push_back(domain);
            DARWIN_LOG_DEBUG("DGATask:: ParseBody: Parsed element: " + domain);
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("DGATask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}

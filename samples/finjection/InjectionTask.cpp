/// \file     InjectionTask.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     06/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <cmath>
#include <string.h>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "Logger.hpp"
#include "InjectionTask.hpp"
#include "protocol.h"

InjectionTask::InjectionTask(boost::asio::local::stream_protocol::socket& socket,
                             darwin::Manager& manager,
                             std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                             BoosterHandle booster, std::set<std::string> *keywords,
                             std::size_t additional_features_length, std::size_t features_length)
        : Session{socket, manager, cache}, _additional_features_length{additional_features_length},
          _features_length{features_length}, _booster{booster}, _keywords{keywords} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t InjectionTask::GenerateHash() {
    return xxh::xxhash<64>(_current_request);
}

void InjectionTask::operator()() {
    DARWIN_ACCESS_LOGGER;

    // We have a generic hash function, which takes no arguments as these can be of very different types depending
    // on the nature of the filter
    // So instead, we set an attribute corresponding to the current request being processed, to compute the hash
    // accordingly
    for (const std::string &request : _requests) {
        SetStartingTime();
        _current_request = request;
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                _certitudes.push_back(certitude);
                DARWIN_LOG_ACCESS(_current_request.size(), certitude, GetDuration());
                continue;
            }
        }

        certitude = Predict(request);
        _certitudes.push_back(certitude);

        if (_is_cache) {
            SaveToCache(hash, certitude);
        }

        DARWIN_LOG_ACCESS(_current_request.size(), certitude, GetDuration());
    }

    Workflow();
    _requests = std::vector<std::string>();
}

void InjectionTask::Workflow(){
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

InjectionTask::~InjectionTask() {
    DARWIN_LOGGER;

    if (!_booster) {
        if (XGBoosterFree(_booster) == -1) {
            DARWIN_LOG_CRITICAL("InjectionTask:: ~InjectionTask:: XGBoosterFree:: '" +
                                std::string(XGBGetLastError()) + "'");
        }
    }
}

std::size_t InjectionTask::CountOccurrences(const std::string& keyword, const std::string& target) {
    std::size_t occurrences = 0;
    std::size_t keyword_length = keyword.length();
    std::string::size_type current_position = 0;

    while ((current_position = target.find(keyword, current_position)) != std::string::npos) {
        ++occurrences;
        current_position += keyword_length;
    }

    return occurrences;
}

DMatrixHandle InjectionTask::ExtractFeatures(const std::string &request) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("InjectionTask:: Loading features...");
    DMatrixHandle features = nullptr;
    auto *features_tmp = new float[_features_length];

    // We store the occurrence number of the current keyword (keywords[feature_index])
    for (std::size_t feature_index = 0; feature_index < _features_length - _additional_features_length;
         ++feature_index) {
        std::size_t occurrences = CountOccurrences(*std::next(_keywords->begin(), feature_index), request);
        features_tmp[feature_index] = occurrences;
    }

    // We store the length of the request here
    features_tmp[_features_length - 1] = request.size();

    if (XGDMatrixCreateFromMat((float *)features_tmp, 1, _features_length, -1, &features) == -1) {
        DARWIN_LOG_ERROR("InjectionTask:: ExtractFeatures:: XGDMatrixCreateFromMat:: '" +
                         std::string(XGBGetLastError()) + "'");

        delete[] features_tmp;
        return nullptr;
    }

    delete[] features_tmp;

    return features;
}

unsigned int InjectionTask::Predict(const std::string &request) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("InjectionTask:: Predict:: Classifying request: '" + request + "'");
    DMatrixHandle features = ExtractFeatures(request);

    if (!features) return 101;

    unsigned int certitude = 0;
    bst_ulong predictions_length;
    const float *predictions = nullptr;

    if (XGBoosterPredict(_booster, features, 0, 0, &predictions_length, &predictions) == -1) {
        DARWIN_LOG_ERROR("InjectionTask:: Predict:: XGBoosterPredict:: '" + std::string(XGBGetLastError()) + "'");
        return 101;
    }

    if (predictions_length <= 0 || !predictions) {
        DARWIN_LOG_ERROR("InjectionTask:: Predict:: Results are empty. Something wrong happened");
        return 101;
    }

    certitude = static_cast<unsigned int>(std::round(predictions[0] * 100));
    DARWIN_LOG_DEBUG("InjectionTask:: Predict:: Prediction found is " + std::to_string(certitude));

    if (XGDMatrixFree(features) == -1) {
        DARWIN_LOG_CRITICAL("InjectionTask:: ~InjectionTask:: XGDMatrixFree:: '" +
                            std::string(XGBGetLastError()) + "'");
    }

    return certitude;
}

bool InjectionTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("InjectionTask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("InjectionTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("InjectionTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("InjectionTask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            auto items = request.GetArray();

            if (items.Size() != 1) {
                DARWIN_LOG_ERROR(
                    "InjectionTask:: ParseBody: You must provide exactly one argument per request: the HTTP request"
                );

                return false;
            }

            if (!items[0].IsString()) {
                DARWIN_LOG_ERROR("InjectionTask:: ParseBody: The request sent must be a string");
                return false;
            }

            std::string to_classify = items[0].GetString();
            _requests.push_back(to_classify);
            DARWIN_LOG_DEBUG("InjectionTask:: ParseBody: Parsed element: " + to_classify);
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("InjectionTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}

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
#include <string>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/Validators.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "DGATask.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "protocol.h"
#include "AlertManager.hpp"

DGATask::DGATask(boost::asio::local::stream_protocol::socket& socket,
                 darwin::Manager& manager,
                 std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                 std::mutex& cache_mutex,
                 DarwinTfLiteInterpreterFactory& interpreter_factory,
                 faup_options_t *faup_options,
                 std::map<std::string, unsigned int> &token_map,
                 const unsigned int max_tokens)
        : Session{"dga", socket, manager, cache, cache_mutex}, _interpreter_factory{interpreter_factory}, _faup_options{faup_options}, _token_map{token_map}, _max_tokens{max_tokens} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t DGATask::GenerateHash() {
    return xxh::xxhash<64>(_domain);
}

long DGATask::GetFilterCode() noexcept {
    return DARWIN_FILTER_DGA;
}

void DGATask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    // Should not fail, as the Session body parser MUST check for validity !
    rapidjson::GenericArray<false, rapidjson::Value> array = _body.GetArray();
    std::shared_ptr<tflite::Interpreter> interpreter = _interpreter_factory.GetInterpreter();
    if(! interpreter) {
        // Error in the configuration stage, cannot happen in the actual workflow, the program will kill itself, 
        // see DarwinTfLiteInterpreterFactory::GetInterpreter for more information
        DARWIN_LOG_ERROR("DGATask:: TFLite Interpreter is null, the filter cannot process data");
        STAT_PARSE_ERROR_INC;
        _certitudes.push_back(DARWIN_ERROR_RETURN);
        return;
    }
    for (rapidjson::Value &value : array) {
        STAT_INPUT_INC;
        SetStartingTime();
        // We have a generic hash function, which takes no arguments as these can be of very different types depending
        // on the nature of the filter
        // So instead, we set an attribute corresponding to the current domain being processed, to compute the hash
        // accordingly

        if(ParseLine(value)) {
            unsigned int certitude;
            xxh::hash64_t hash;

            if (_is_cache) {
                hash = GenerateHash();

                if (GetCacheResult(hash, certitude)) {
                    if (certitude >= _threshold and certitude < DARWIN_ERROR_RETURN){
                        STAT_MATCH_INC;
                        DARWIN_ALERT_MANAGER.Alert(_domain, certitude, Evt_idToString());
                        if (is_log) {
                            std::string alert_log = R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                                    R"(", "filter": ")" + GetFilterName() + "\", \"domain\": \""+ _domain + "\", \"dga_prob\": " + std::to_string(certitude) + "}";
                            _logs += alert_log + '\n';
                        }
                    }
                    _certitudes.push_back(certitude);
                    DARWIN_LOG_DEBUG("DGATask:: processed entry in "
                                    + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                    continue;
                }
            }

            certitude = Predict(interpreter);
            if (certitude >= _threshold and certitude < DARWIN_ERROR_RETURN){
                STAT_MATCH_INC;
                DARWIN_ALERT_MANAGER.Alert(_domain, certitude, Evt_idToString());
                if (is_log) {
                    std::string alert_log = R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                                    R"(", "filter": ")" + GetFilterName() + "\", \"domain\": \""+ _domain + "\", \"dga_prob\": " + std::to_string(certitude) + "}";
                    _logs += alert_log + '\n';
                }
            }
            _certitudes.push_back(certitude);
            if (_is_cache) {
                SaveToCache(hash, certitude);
            }
            DARWIN_LOG_DEBUG("DGATask:: processed entry in "
                            + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
        }
        else {
            STAT_PARSE_ERROR_INC;
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }

    }
}

DGATask::~DGATask() = default;

bool DGATask::ExtractRegisteredDomain(std::string &to_predict) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ExtractRegisteredDomain:: Analyzing domain '" + _domain + "'");

    bool is_domain_valid = darwin::validator::IsDomainValid(_domain);

    if (!is_domain_valid) return false;

    faup_handler_t* fh = faup_init(_faup_options);

    try {
        faup_decode(fh, _domain.c_str(), _domain.size());

        std::string tld = _domain.substr(
                faup_get_tld_pos(fh),
                faup_get_tld_size(fh)
        );

        if (tld == "yu" || tld == "za") {
            DARWIN_LOG_DEBUG("ExtractRegisteredDomain::TLD found is '" + tld + "', returning false");
            faup_terminate(fh);
            return false;
        }

        DARWIN_LOG_DEBUG("ExtractRegisteredDomain:: TLD found is '" + tld + "'");

        std::string registered_domain = _domain.substr(
                faup_get_domain_without_tld_pos(fh),
                faup_get_domain_without_tld_size(fh)
        );

        DARWIN_LOG_DEBUG("ExtractRegisteredDomain:: Registered domain found is '" + registered_domain + "'");

        to_predict = registered_domain + "." + tld;

    } catch (const std::out_of_range& exception) {
        DARWIN_LOG_INFO("ExtractRegisteredDomain:: domain appears to be invalid: '" + _domain + "'");
        faup_terminate(fh);
        return false;

    } catch (const std::exception& exception) {
        DARWIN_LOG_ERROR("ExtractRegisteredDomain:: Unexpected error with domain '" + _domain + "': '" +
                         exception.what() + "'");
        faup_terminate(fh);
        return false;
    } catch (...) {
        DARWIN_LOG_ERROR("ExtractRegisteredDomain:: Unknown error with domain '" + _domain + "'");
        faup_terminate(fh);
        return false;
    }
    faup_terminate(fh);
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
            DARWIN_LOG_DEBUG("DomainTokenizer:: No token has been found for the character '" + character + "'");
            continue;
        }

        tmp_domain_tokens[size++] = it->second;
    }

    std::size_t index = _max_tokens - size;

    for (size = index; size < _max_tokens; ++size) {
        domain_tokens[size] = tmp_domain_tokens[size - index];
    }
}

unsigned int DGATask::Predict(std::shared_ptr<tflite::Interpreter> interpreter) {
    std::string to_predict;

    if (!ExtractRegisteredDomain(to_predict)) {
        return DARWIN_ERROR_RETURN;
    }

    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Predict:: Classifying '" + to_predict + "'...");

    std::vector<std::size_t> domain_tokens(_max_tokens, 0);
    DomainTokenizer(domain_tokens, to_predict);

    TfLiteStatus status = interpreter->AllocateTensors();

    if(status != TfLiteStatus::kTfLiteOk) {
        DARWIN_LOG_ERROR("DGATask::Predict:: Tflite Error while allocating tensors : " + std::to_string(static_cast<int>(status)));
        return DARWIN_ERROR_RETURN;
    }

    float* input_tensor_mapped = interpreter->typed_input_tensor<float>(0);
    size_t index = 0;
    for (const auto &domain_token : domain_tokens) {
        input_tensor_mapped[index++] = domain_token;
    }

    status = interpreter->Invoke();
    if(status != TfLiteStatus::kTfLiteOk) {
        DARWIN_LOG_ERROR("DGATask::Predict:: Tflite Error while predicting : " + std::to_string(static_cast<int>(status)));
        return DARWIN_ERROR_RETURN;
    }
    
    float* output_tensor = interpreter->typed_output_tensor<float>(0);

    unsigned int certitude = round(output_tensor[0] * 100);
    DARWIN_LOG_DEBUG("Predict:: DGA score obtained: " + std::to_string(certitude));

    return certitude;
}

bool DGATask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("DGATask:: ParseBody: The input line is not an array");
        return false;
    }

    _domain.clear();
    auto values = line.GetArray();

    if (values.Size() != 1) {
        DARWIN_LOG_ERROR("DGATask:: ParseLine: You must provide only the domain name in the list");
        return false;
    }

    if (!values[0].IsString()) {
        DARWIN_LOG_ERROR("DGATask:: ParseLine: The domain sent must be a string");
        return false;
    }

    _domain = values[0].GetString();
    DARWIN_LOG_DEBUG("DGATask:: ParseLine: Parsed element: " + _domain);

    return true;
}

/// \file     InjectionTask.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     06/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <set>
#include <xgboost/c_api.h>

#include "../../toolkit/lru_cache.hpp"
#include "protocol.h"
#include "Session.hpp"

#define DARWIN_FILTER_INJECTION 0x696E6A65

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class InjectionTask : public darwin::Session {
public:
    explicit InjectionTask(boost::asio::local::stream_protocol::socket& socket,
                           darwin::Manager& manager,
                           std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                           BoosterHandle booster, std::set<std::string> *keywords,
                           std::size_t _additional_features_length, std::size_t features_length);
    ~InjectionTask() override;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    xxh::hash64_t GenerateHash() override;

private:
    /// According to the header response,
    /// init the following Darwin workflow
    void Workflow();

    /// Classify the parsed request.
    ///
    /// \return true on success, false otherwise.
    unsigned int Predict(const std::string &request);

    /// Extract the features of the request
    ///
    /// \return true on success, false otherwise.
    DMatrixHandle ExtractFeatures(const std::string &request);

    /// Send result into the session.
    void SendResToSession() noexcept;

    /// Send result into DARWIN.
    void SendToDarwin() noexcept;

    /// Parse the body received.
    ///
    /// \return true on success, false otherwise.
    bool ParseBody() override;

    /// Count occurrences of a keyword in a given request
    std::size_t CountOccurrences(const std::string& keyword, const std::string& target);

private:
    std::vector<std::string> _requests; // The requests to check
    std::string _current_request; // The current request to check
    // A feature is the number of occurrences of a keyword in a given request
    // An additional feature is... something else. For now (1), only 1 additional feature exists
    // Which is the length of the request itself
    std::size_t _additional_features_length = 0; // The number of additional features to be extracted
    std::size_t _features_length = 0; // The total number of features to be extracted
    BoosterHandle _booster = nullptr; // The XGBoost classifier
    std::set<std::string> *_keywords = nullptr; // The keywords used to classify the requests
};

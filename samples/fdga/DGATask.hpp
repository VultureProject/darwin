/// \file     DGATask.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     30/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <boost/token_functions.hpp>
#include <faup/faup.h>
#include <map>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "protocol.h"
#include "Session.hpp"
#include "tensorflow/core/public/session.h"

#define DARWIN_FILTER_DGA 0x64676164

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class DGATask : public darwin::Session {
public:
    explicit DGATask(boost::asio::local::stream_protocol::socket& socket,
                     darwin::Manager& manager,
                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                     std::mutex& cache_mutex,
                     std::shared_ptr<tensorflow::Session> &session,
                     faup_options_t *faup_options,
                     std::map<std::string, unsigned int> &token_map, const unsigned int max_tokens = 50);
    ~DGATask() override;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Extract the registered domain with the TLD
    ///
    /// \param to_predict Will contain the string extracted.
    /// \return true on success, false otherwise.
    bool ExtractRegisteredDomain(std::string &to_predict);

    /// Classify the parsed request.
    ///
    /// \return true on success, false otherwise.
    unsigned int Predict();

    /// Parse the body received.
    ///
    /// \return true on success, false otherwise.
    bool ParseBody() override;

    /// Tokenize the DGA to be classified.
    ///
    /// \param domain_tokens The DGA tokens to be used.
    /// \param to_predict The DGA to be predicted.
    void DomainTokenizer(std::vector<std::size_t> &domain_tokens, const std::string &to_predict);


private:
    unsigned int _max_tokens = 75;
    boost::char_separator<char> _separator {" ());,:-~?!{}/[]"};
    std::shared_ptr<tensorflow::Session> _session = nullptr; // The tensorflow session to use
    std::map<std::string, unsigned int> _token_map; // The token map to help classifying domains
    std::vector<std::string> _domains; // The domains to check
    std::string _current_domain; // The current domain being checked
    faup_options_t *_faup_options = nullptr;
};

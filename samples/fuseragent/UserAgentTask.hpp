/// \file     UserAgentTask.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     16/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <boost/token_functions.hpp>
#include <map>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "ATask.hpp"
#include "DarwinPacket.hpp"
#include "ASession.fwd.hpp"
#include "tensorflow/core/public/session.h"

#define DARWIN_FILTER_USER_AGENT 0x75736572

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class UserAgentTask : public darwin::ATask {
public:
    explicit UserAgentTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                           std::mutex& cache_mutex,
                           darwin::session_ptr_t s,
                           darwin::DarwinPacket& packet,
                           std::shared_ptr<tensorflow::Session> &session,
                           std::map<std::string, unsigned int> &token_map, const unsigned int max_tokens = 50);
    ~UserAgentTask() override;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;
    static const std::vector<std::string> USER_AGENT_CLASSES;

protected:
    /// Get the result from the cache
    xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Classify the parsed request.
    ///
    /// \return true on success, false otherwise.
    unsigned int Predict(const std::string &user_agent);

    /// Parse the body received.
    ///
    /// \return true on success, false otherwise.
    bool ParseBody() override;

    /// Tokenize the user agent to be classified.
    ///
    /// \param ua_tokens The user agent tokens to be used.
    void UserAgentTokenizer(const std::string &user_agent, std::vector<std::size_t> &ua_tokens);

private:
    unsigned int _max_tokens = 50;
    boost::char_separator<char> _separator {" ());,:-~?!{}/[]"};
    std::shared_ptr<tensorflow::Session> _session = nullptr; // The tensorflow session to use
    std::map<std::string, unsigned int> _token_map; // The token map to help classifying user agents
    std::string _current_user_agent; // The user agent to check
    std::vector<std::string> _user_agents;
};

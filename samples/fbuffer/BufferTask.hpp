/// \file     BufferTask.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     22/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <boost/token_functions.hpp>
#include <map>
#include <string>

#include "../../toolkit/RedisManager.hpp"
#include "BufferThreadManager.hpp"
#include "protocol.h"
#include "Session.hpp"

#define DARWIN_FILTER_BUFFER 0x62756672 // Made from: bufr

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class BufferTask : public darwin::Session {
public:
    BufferTask(boost::asio::local::stream_protocol::socket& socket,
                     darwin::Manager& manager,
                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                     std::mutex& cache_mutex,
                     std::string redis_list_name,
                     std::shared_ptr<BufferThreadManager> vat,
                     std::vector<std::pair<std::string, valueType>> inputs,
                     std::vector<std::shared_ptr<AConnector>> connectors);
    ~BufferTask() override;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Parse a line in the body.
    bool ParseLine(rapidjson::Value &line) final;
    bool ParseLine(rapidjson::Value &line, std::pair<std::string, valueType> &input);

    bool AddEntries();


private:
    std::string _redis_list_name;
    std::string _entry;
    boost::char_separator<char> _separator {" ();,:-~?!{}/[]"};
    std::string _string;
    std::shared_ptr<BufferThreadManager> _buffer_thread_manager = nullptr;
    std::vector<std::pair<std::string, valueType>> _inputs;
    std::vector<std::shared_ptr<AConnector>> _connectors;
    std::map<std::string, std::string> _input_line;
};

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

class BufferTask final : public darwin::Session {
    /// This class inherits from Session (see Session.hpp)
    /// This class handles Tasks for Buffer Filter.
    /// It parses the body of incomming messages and splits data into several REDIS lists depending on
    /// the output Filters described in the config file.
    ///
    ///\class BufferTask

    public:
    ///\brief Unique constructor
    ///
    ///\param socket Transfered to Session constructor
    ///\param manager Transfered to Session constructor
    ///\param cache Transfered to Session constructor
    ///\param cache_mutex Transfered to Session constructor
    ///\param inputs This vector holds the name and types of datas in input (in the correct order)
    ///\param connectors This vector holds all the Connectors needed depending on the output Filters in config file.
    BufferTask(boost::asio::local::stream_protocol::socket& socket,
                     darwin::Manager& manager,
                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                     std::mutex& cache_mutex,
                     std::vector<std::pair<std::string, valueType>> &inputs,
                     std::vector<std::shared_ptr<AConnector>> &connectors);

    ///\brief Virtual final default constructor    
    virtual ~BufferTask() override final = default;

    public:
    ///\brief This operator is the entry point of BufferTask.
    /// You must overload this operator in order to compile and be executed.
    /// Calls ParseLine on every line received in the body.
    void operator()() override;

    protected:
    ///\brief Returns the Filter code
    long GetFilterCode() noexcept override;

    private:
    ///\brief Parses a line of the body.
    /// Calls ParseData for every input in the config file and put the result (in _data) in _input_line
    /// Then it call AddEntries() to add the complete _input_line into REDIS.
    ///
    ///\param line the line to parse
    ///
    ///\return true on success, false otherwise.
    bool ParseLine(rapidjson::Value &line) final;

    ///\brief Parses a data if it fits the valueType and store it in _data
    ///
    ///\param data The data to parse
    ///\param input_type The expected type of the data.
    ///
    ///\return true on success, false otherwise.
    bool ParseData(rapidjson::Value &data, valueType input_type);

    ///\brief this function add the Parsed line (_input_line) into the correct REDIS storages by callind sendToRedis method for each Connector in _connectors.
    ///
    ///\return true on success, false otherwise.
    bool AddEntries();


    private:
    /// Temporarily used during the lifetime of ParseLine, used to store a data just parsed by ParseData before adding it inti _input_line in ParseLine.
    std::string _data;

    /// It needs to be a vector of pair and not a map to keep the order correct. It holds all the names and types of the inputs.
    std::vector<std::pair<std::string, valueType>> _inputs_format;

    /// This vector holds all the Connectors set in the config file. 
    std::vector<std::shared_ptr<AConnector>> _connectors;

    /// Temporarily used during the lifetime of ParseLine.
    /// It is send to Connector when adding a line into Redis (in AddEntries)
    std::map<std::string, std::string> _input_line;
};

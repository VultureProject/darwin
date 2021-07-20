/// \file     BufferTask.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     22/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <map>
#include <string>

#include "../../toolkit/RedisManager.hpp"
#include "BufferThreadManager.hpp"
#include "ATask.hpp"
#include "DarwinPacket.hpp"
#include "ASession.fwd.hpp"

#define DARWIN_FILTER_BUFFER 0x62756672 // Made from: bufr
#define DARWIN_FILTER_NAME "buffer"

class BufferTask final : public darwin::ATask {
    /// This class inherits from Session (see Session.hpp)
    /// This class handles Tasks for Buffer Filter.
    /// It parses the body of incoming messages and splits data into several REDIS lists depending on
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
    ///\param inputs This vector holds the name and types of data in input (in the correct order)
    ///\param connectors This vector holds all the Connectors needed depending on the output Filters in config file.
    BufferTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                    std::mutex& cache_mutex,
                    darwin::session_ptr_t s,
                    darwin::DarwinPacket& packet,
                     std::vector<std::pair<std::string, darwin::valueType>> &inputs,
                     std::vector<std::shared_ptr<AConnector>> &connectors);

    ///\brief Virtual final default constructor    
    virtual ~BufferTask() override final = default;

    ///\brief This operator is the entry point of BufferTask.
    /// Calls ParseLine on every line received in the body.
    void operator()() override;

    protected:
    ///\brief Returns the Filter code
    long GetFilterCode() noexcept override;

    private:
    ///\brief Parses a line of the body.
    /// Calls ParseData for every input in the config file and puts the result (in _data) in _input_line
    /// Then it calls AddEntries() to add the complete _input_line into REDIS.
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
    bool ParseData(rapidjson::Value &data, darwin::valueType input_type);

    ///\brief this function add the Parsed line (_input_line) into the correct REDIS storages by calling ParseInputForRedis method for each Connector in _connectors.
    ///
    ///\return true on success, false otherwise.
    bool AddEntries();


    private:
    /// Temporarily used during the lifetime of ParseLine, used to store a data just parsed by ParseData before adding it into _input_line in ParseLine.
    std::string _data;

    /// It needs to be a vector of pair and not a map to keep the order correct. It holds all the names and types of the inputs.
    std::vector<std::pair<std::string, darwin::valueType>> _inputs_format;

    /// This vector holds all the Connectors set in the config file. 
    std::vector<std::shared_ptr<AConnector>> _connectors;

    /// Temporarily used during the lifetime of ParseLine.
    /// It is sent to Connector when adding a line into Redis (in AddEntries)
    std::map<std::string, std::string> _input_line;
};

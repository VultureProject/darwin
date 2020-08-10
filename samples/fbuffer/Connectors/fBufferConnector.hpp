/// \file     fBufferConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     02/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include "AConnector.hpp"

class fBufferConnector final : public AConnector {
    /// This class inherits from AConnector (see AConnector.hpp)
    /// It provides correct info picking to send to Buffer type filter.
    /// 
    ///\class fBufferConnector

    public:
    ///\brief Unique constructor. It contains all stuff needed to ensure REDIS and output Buffer Filter communication
    ///
    ///\param io_context The boost::asio::io_context used by the Server. Needed for communication with output Filter.
    ///\param filter_socket_path The socket path used to connect with output filter.
    ///\param interval Interval between two data sendings to output filter if there are enough logs in redis_list REDIS storage.
    ///\param redis_lists The names of the Redis List on which the connector will store and retrieve data depending to source, before sending to output Filter
    ///\param required_log_lines The number of logs required before sending data to output Filter
    fBufferConnector(boost::asio::io_context &context,
                        std::string &filter_socket_path,
                        unsigned int interval,
                        std::vector<std::pair<std::string, std::string>> &redis_lists,
                        unsigned int required_log_lines);

    ///\brief Virtual final default constructor
    virtual ~fBufferConnector() override final = default;

    public:
    ///\brief This functions sends datas to the REDIS storage. It override default pure virtual one as each filter doesn't need the same datas.
    ///
    /// It should fill _entry with the datas to send as REDISAddEntry is picking in it.
    ///
    ///\param input_line is a map representing all the entry received by the BufferTask.
    ///
    ///\return true on success, false otherwise.
    virtual bool sendToRedis(std::map<std::string, std::string> &input_line) override final;
};
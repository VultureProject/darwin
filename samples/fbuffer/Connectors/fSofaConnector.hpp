/// \file     fSofaConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     03/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include "AConnector.hpp"

class fSofaConnector final : public AConnector {
    /// This class inherits from AConnector (see AConnector.hpp)
    /// It provides correct info picking to send to Sofa type filter.
    /// 
    ///\class fSofaConnector

    public:
    ///\brief Unique constructor. It contains all stuff needed to ensure REDIS and output Sofa Filter communication
    ///
    ///\param io_context The boost::asio::io_context used by the Server. Needed for communication with output Filter.
    ///\param filter_socket_path The socket path used to connect to the output filter.
    ///\param interval Interval between two data sendings to output filter if there are enough logs in redis_list REDIS storage.
    ///\param redis_lists The names of the Redis List on which the connector will store and retrieve data depending on source, before sending to output Filter
    ///\param required_log_lines The number of logs required before sending data to output Filter
    fSofaConnector(boost::asio::io_context &context,
                    std::string &filter_socket_path,
                    unsigned int interval,
                    std::vector<std::pair<std::string, std::string>> &redis_lists,
                    unsigned int required_log_lines);

    ///\brief Virtual final default constructor
    virtual ~fSofaConnector() override final = default;

    public:
    ///\brief This function sends data to the REDIS storage. It overrides default pure virtual one as each filter doesn't need the same data.
    ///
    /// It should fill _entry with the data to send as REDISAddEntry is picking from it.
    ///
    ///\param input_line is a map representing all the entries received by the BufferTask.
    ///
    ///\return true on success, false otherwise.
    virtual bool ParseInputForRedis(std::map<std::string, std::string> &input_line) override final;
};
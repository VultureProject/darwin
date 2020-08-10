/// \file     AConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     29/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#pragma once

#include <string>
#include <map>
#include <vector>
#include <boost/asio.hpp>

#include "Logger.hpp"
#include "enums.hpp"

class AConnector {
    /// This abstract class is made to be inheritated by subclasses named f<FilterName>Connector
    /// Those classes are instanciated in Generator.cpp, function ConfigureNetworkObject.
    /// They are meant to be used in threads.
    /// The subclasses' goal is to serve as an interface between the Generator, the BufferThread
    /// and the filter connected as output.
    /// The subclasses are in charge to implement sendToRedis and can override REDISAddEntry to use
    /// another REDIS storage type (default uses a Redis set and adds with SADD).
    ///
    ///\class AConnector

    public:
    ///\brief Unique constructor. It contains all stuff needed to ensure REDIS and output Filter communication
    ///
    ///\param io_context The boost::asio::io_context used by the Server. Needed for communication with output Filter.
    ///\param filter_type enum outputType is defined in enums.hpp. The filter type is here to recognize the connector type when stored as AConnector ptr.
    ///\param filter_socket_path The socket path used to connect with output filter.
    ///\param interval Interval between two data sendings to output filter if there are enough logs in redis_list REDIS storage.
    ///\param redis_lists The names of the Redis List on which the connector will store and retrieve data depending to source, before sending to output Filter
    ///\param required_log_lines The number of logs required before sending data to output Filter
    AConnector(boost::asio::io_context &io_context,
                outputType filter_type,
                std::string &filter_socket_path,
                unsigned int interval,
                std::vector<std::pair<std::string, std::string>> &redis_lists,
                unsigned int required_log_lines);

    ///\brief Virtual default constructor
    virtual ~AConnector() = default;

    public:
    ///\brief Get the interval set in the connector.
    ///
    ///\return this->_interval
    unsigned int getInterval() const;

    ///\brief Get the required number of logs lines for sending to output Filter.
    ///
    ///\return this->_required_log_lines
    unsigned int getRequiredLogLength() const;

    ///\brief Get the REDIS storage name on which to store or retrieve datas.
    ///
    ///\return this->_redis_lists
    std::vector<std::pair<std::string, std::string>> getRedisList() const;

    ///\brief Virtual function that can be override if needed. Used to add entry in the set list_name REDIS storage.
    ///
    /// It can be override by children if they need another type of REDIS storage. 
    /// (Default is using sets, SADD to add data, each data must be unique).
    ///
    ///\param source The source of the input, used to know on which redis list to store the input.
    ///\param list_name The Redis list on which to add data
    ///
    ///\return true on success, false otherwise.
    virtual bool REDISAddEntry(const std::string &entry, const std::string &list_name);

    ///\brief Get the logs from the Redis List
    ///
    /// \param len THe number of elements to pick up in the list
    /// \param logs the vector where we want to stock our logs
    ///
    /// \return true on success, false otherwise.
    virtual bool REDISPopLogs(long long int len, std::vector<std::string> &logs, const std::string &list_name) noexcept;

    ///\brief Query Redis to get number of elements in _redis_list.
    ///
    /// \return The number of elements in _redis_list.
    virtual long long int REDISListLen(const std::string &list_name) noexcept;

    ///\brief Reinserts logs into the _redis_list
    ///
    ///\param logs The logs to be reinserted
    ///
    ///\return true on success, false otherwise.
    virtual bool REDISReinsertLogs(std::vector<std::string> &logs, const std::string &list_name);

    ///\brief This function extracts from _input_line some datas, format it and add it to _entry.
    ///
    ///\param fieldname The name of the data to retrieve
    ///\param type valueType enum is defined in enums.hpp. The type of the value to retrieve.
    ///
    ///\return true on success, false otherwise.
    bool parseData(std::string fieldname, valueType type);

    ///\brief Set the connexion with the output Filter and sends logs to it.
    ///
    ///\param logs The logs lines to be sent.
    ///
    ///\return true on success, false otherwise.
    bool sendToFilter(std::vector<std::string> &logs); // To send logs to the filter

    public: // Functions that needs to be implemented by children
    ///\brief This functions sends datas to the REDIS storage. It must be override as each filter doesn't need the same datas.
    ///
    /// It should fill _entry with the datas to send as REDISAddEntry is picking in it.
    ///
    ///\param input_line is a map representing all the entry received by the BufferTask.
    ///
    ///\return true on success, false otherwise.
    virtual bool sendToRedis(std::map<std::string, std::string> &input_line) = 0;

    private:
    ///\brief Get the Buffer filter code
    ///
    ///\return the Buffer filter code
    long GetFilterCode() noexcept;

    protected:
    ///\brief this virtual function "jsonifies" the vector of strings into a single string.
    /// By default it performs no other actions on the data but CAN be override if needed. (e.g fAnomalyConnector)
    ///
    ///\param logs The logs to jsonify
    ///\param format The string to fill with the result
    ///
    ///\return True on success (formatting successfull), False otherwise.
    virtual bool FormatDataToSendToFilter(std::vector<std::string> &logs, std::string &formatted);

    ///\brief Extracts the source of the current input line
    ///
    ///\return The source of the current input line
    std::string getSource(std::map<std::string, std::string> &input_line);

    // Used to link with the correct task
    outputType _filter_type;

    // The boost::asio::io_service needed to ensure communication with output Filter
    boost::asio::io_service _io_service;

    // Needed to know if the socket is connected or not
    bool _connected;

    // Socket filepath on which to write data every _interval seconds if there is more than _required_log_lines of data in Redis
    std::string _filter_socket_path;

    // The output Filter's socket
    boost::asio::local::stream_protocol::socket _filter_socket; //!< Filter's socket.

    // The interval between two checks on the number of elements in REDIS. (default 300s = 5min)
    unsigned int _interval = 300;

    // The different REDIS lists used to store data depending to source before sending to filter
    std::vector<std::pair<std::string, std::string>> _redis_lists;

    // Temporarily used to between formating and adding data to REDIS
    std::string _entry;

    // Temporarily used to store an input in a form that the connectors can pick what they need
    std::map<std::string, std::string> _input_line;

    // The number of log lines in REDIS needed to send to the output Filter
    unsigned int _required_log_lines;
};
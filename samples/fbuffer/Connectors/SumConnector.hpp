/// \file     SumConnector.cpp
/// \authors  Theo Bertin
/// \version  1.0
/// \date     15/12/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include "AConnector.hpp"

class SumConnector final : public AConnector {
    /// This class inherits from AConnector (see AConnector.hpp)
    /// It provides cumulative incrementation of a value
    ///
    ///\class SumConnector

public:
    ///\brief Unique constructor.
    ///
    ///\param io_context The boost::asio::io_context used by the Server. Needed for communication with output Filter.
    ///\param filter_socket_path The socket path used to connect to the output filter.
    ///\param interval Interval between two data sendings to output filter.
    ///\param redis_lists The names of the Redis List on which the connector will store and retrieve data depending on source, before sending to output Filter
    ///\param required_log_lines The number of logs required before sending data to output Filter
    SumConnector(boost::asio::io_context &context,
                    std::string &filter_socket_path,
                    unsigned int interval,
                    std::vector<std::pair<std::string, std::string>> &redis_lists,
                    unsigned int required_log_lines);

    ///\brief Virtual final default constructor
    virtual ~SumConnector() override final = default;

    ///\brief test every Redis Key in _redis_lists to ensure keys are of the correct type if they exist in Redis
    ///
    ///\return true if the keys are not in redis, or are of the good type, false otherwise.
    virtual bool PrepareKeysInRedis() override;

    ///\brief This function sends data to the REDIS storage. It overrides default pure virtual one as each filter doesn't need the same data.
    ///
    /// It should fill _entry with the data to send as REDISAddEntry is picking from it.
    ///
    ///\param input_line is a map representing all the entries received by the BufferTask.
    ///
    ///\return true on success, false otherwise.
    virtual bool ParseInputForRedis(std::map<std::string, std::string> &input_line) override final;

    ///\brief Increment sum by the amount given
    ///
    ///\param entry The amount with which to increment the sum.
    ///\param sum_name The name of the sum in Redis.
    ///
    ///\return true on success, false otherwise.
    virtual bool REDISAddEntry(const std::string &entry, const std::string &sum_name) override final;

    ///\brief Overrided function. This function does nothing, as reinsertion is detrimentakl to normal detection
    ///
    ///\param logs unused parameter
    ///\param sum_name unused parameter
    ///
    ///\return always true
    virtual bool REDISReinsertLogs(std::vector<std::string> &logs __attribute__((unused)), const std::string &sum_name __attribute__((unused))) override;

    ///\brief Get the sum from Redis
    ///
    /// \param len The number of elements to pick up in the list (unused, for compatibility with format handled by BufferThread)
    /// \param logs The vector used to store our log
    /// \param sum_name The name of the sum in Redis
    ///
    /// \return true on success, false otherwise.
    virtual bool REDISPopLogs(long long int len __attribute__((unused)), std::vector<std::string> &logs, const std::string &sum_name) noexcept override final;

    ///\brief Override default REDISListLen to check if sum was incremented at least once.
    ///
    /// \param sum_name The name of the sum to check
    ///
    /// \return 0 if the sum is null, 1 if the sum is not null, -1 in case of error
    virtual long long int REDISListLen(const std::string &sum_name) noexcept override final;

protected:
    ///\brief Overrode to generate a simple list (instead of double list, useless here)
    ///
    ///\param count The sum to jsonify
    ///\param format The string to fill with the result
    ///
    ///\return True on success (formatting successful), False otherwise.
    virtual bool FormatDataToSendToFilter(std::vector<std::string> &logs, std::string &formatted) override;
};
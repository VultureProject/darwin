/// \file     AConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     29/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#include <boost/bind.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/asio.hpp>

#include "../../../toolkit/Uuid.hpp"
#include "../../../toolkit/RedisManager.hpp"
#include "AConnector.hpp"
#include "protocol.h"
#include "BufferTask.hpp"

AConnector::AConnector(boost::asio::io_context &context, darwin::outputType filter_type, std::string &filter_socket_path, unsigned int interval, std::vector<std::pair<std::string, std::string>> &redis_lists, unsigned int required_log_lines) :
                        _filter_type(std::move(filter_type)),
                        _filter_socket_path(std::move(filter_socket_path)),
                        _filter_socket(context),
                        _interval(interval),
                        _redis_lists(redis_lists),
                        _required_log_lines(required_log_lines) {}

unsigned int AConnector::GetInterval() const {
    return this->_interval;
}

unsigned int AConnector::GetRequiredLogLength() const {
    return this->_required_log_lines;
}

std::vector<std::pair<std::string, std::string>> AConnector::GetRedisLists() const {
    return this->_redis_lists;
}

bool AConnector::ParseData(std::map<std::string, std::string> &input_line, std::string fieldname, std::string &entry) {
    DARWIN_LOGGER;
    if (input_line.find(fieldname) == input_line.end()) {
        DARWIN_LOG_ERROR("AConnector::ParseData '" + fieldname + "' is missing in the input line. Output ignored.");
        return false;
    }
    if (not entry.empty()) {
        entry += ";";
    }
    entry += input_line[fieldname];
    return true;
}

bool AConnector::PrepareKeysInRedis(){
    DARWIN_LOGGER;
    std::string redis_list, reply;
    bool ret = true;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    for(auto key : this->_redis_lists) {
        redis_list = key.second;
        DARWIN_LOG_DEBUG("AConnector::PrepareKeysInRedis:: testing key " + redis_list);
        if(redis.Query(std::vector<std::string>{"TYPE", redis_list}, reply, true) != REDIS_REPLY_STATUS) {
            DARWIN_LOG_ERROR("AConnector::PrepareKeysInRedis:: Could not get current type of key");
            ret = false;
            continue;
        }

        DARWIN_LOG_DEBUG("AConnector::PrepareKeysInRedis:: key '"+ redis_list + "' is '" + reply + "'");

        if(reply != "none") {
            if(reply != "list") {
                DARWIN_LOG_ERROR("AConnector::PrepareKeysInRedis:: key '" + redis_list + "' is already present "
                                    "but seems to be used for something else, "
                                    "cannot start the filter and risk overriding data in Redis");
                ret = false;
            }
            else {
                DARWIN_LOG_WARNING("AConnector::PrepareKeysInRedis:: The list '" + redis_list + "' "
                                    "was already set in Redis, the key will be overrode!");
                if(redis.Query(std::vector<std::string>{"DEL", redis_list}) != REDIS_REPLY_INTEGER) {
                    DARWIN_LOG_WARNING("SumConnector::PrepareKeysInRedis:: could not reset the key");
                }
            }
        }
    }

    return ret;
}

bool AConnector::REDISAddEntry(const std::string &entry, const std::string &list_name) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AConnector::REDISAddEntry:: Add data in Redis...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    std::vector<std::string> arguments;
    arguments.clear();
    arguments.emplace_back("SADD");
    arguments.emplace_back(list_name);
    arguments.emplace_back(entry);

    if(redis.Query(arguments, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AConnector::REDISAddEntry:: Not the expected Redis response, impossible to add to " + list_name + " redis list.");
        return false;
    }
    return true;
}

bool AConnector::REDISReinsertLogs(std::vector<std::string> &logs, const std::string &list_name) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AConnector::REDISReinsertLogs:: About to reinsert logs in redis list");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    std::vector<std::string> arguments;
    arguments.clear();
    arguments.emplace_back("SADD");
    arguments.emplace_back(list_name);
    for (const auto &log : logs)
        arguments.emplace_back(log);

    if(redis.Query(arguments, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AConnector::REDISReinsertLogs:: Not the expected Redis response, impossible to add to " + list_name + " redis list.");
        return false;
    }

    return true;
}

long long int AConnector::REDISListLen(const std::string &list_name) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AConnector::REDISListLen:: Querying Redis for list size...");

    long long int result;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"SCARD", list_name}, result, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AConnector::REDISListLen:: Not the expected Redis response");
        return -1;
    }
    return result;
}

bool AConnector::REDISPopLogs(long long int len, std::vector<std::string> &logs, const std::string &list_name) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AConnector::REDISPopLogs:: Querying Redis for logs...");

    std::any result;
    std::vector<std::any> result_vector;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"SPOP", list_name, std::to_string(len)}, result, true) != REDIS_REPLY_ARRAY) {
        DARWIN_LOG_ERROR("AConnector::REDISPopLogs:: Not the expected Redis response");
        return false;
    }

    try {
        result_vector = std::any_cast<std::vector<std::any>>(result);
    }
    catch (const std::bad_any_cast&) {
        DARWIN_LOG_ERROR("AConnector:REDISPopLogs:: Impossible to cast redis response into a vector.");
        return false;
    }

    DARWIN_LOG_DEBUG("AConnector::REDISPopLogs:: Got " + std::to_string(result_vector.size()) + " entries from Redis");

    for(auto& object : result_vector) {
        try {
            logs.emplace_back(std::any_cast<std::string>(object));
        }
        catch(const std::bad_any_cast&) {
            DARWIN_LOG_WARNING("AConnector::REDISPopLogs:: Impossible to cast one element of the redis response into a string, log ignored.");
        }
    }

    return true;
}

bool AConnector::FormatDataToSendToFilter(std::vector<std::string> &logs, std::string &res) {
    res = "[[";

    for (size_t i = 0; i < logs.size(); i++) {
        if (i != 0)
            res += "],[";
        res += logs[i];
    }
    res += "]]";
    return true;
}

bool AConnector::SendToFilter(std::vector<std::string> &logs) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AConnector::SendToFilter:: Sending logs to connected filter");

    // Format data
    std::string data;
    if (not FormatDataToSendToFilter(logs, data))
        return false;

    // Connect to socket
    DARWIN_LOG_DEBUG("AConnector::SendToFilter:: Trying to connect to: " +
                     this->_filter_socket_path);
    try {
        this->_filter_socket.connect(
            boost::asio::local::stream_protocol::endpoint(
                        this->_filter_socket_path.c_str()));
    } catch (std::exception const& e) {
        DARWIN_LOG_ERROR(std::string("AConnector::SendToFilter:: "
                                     "Unable to connect to output filter: ") +
                         e.what());
        return false;
    }

    // Prepare packet
    DARWIN_LOG_DEBUG("AConnector::SendToFilter:: data to send: " + data + ", data size: " + std::to_string(data.size()));
    /*
     * Allocate the header +
     * the size of the certitude -
     * DEFAULT_CERTITUDE_LIST_SIZE certitude already in header size
     */
    std::size_t certitude_size = 1;
    std::size_t packet_size = sizeof(darwin_filter_packet_t) + data.size();

    DARWIN_LOG_DEBUG("AConnector::SendToFilter:: Computed packet size: " + std::to_string(packet_size));
    darwin_filter_packet_t* packet;
    packet = (darwin_filter_packet_t *)malloc(packet_size);
    if (!packet) {
        DARWIN_LOG_CRITICAL("AConnector:: SendToFilter:: Could not create a Darwin packet");
        _filter_socket.close();
        return false;
    }
    /*
     * Initialisation of the structure for the padding bytes because of
     * missing __attribute__((packed)) in the protocol structure.
     */
    memset(packet, 0, packet_size);
    if(data.size() != 0) {
        // TODO: set a proper pointer in protocol.h for the body
        // Yes We Hack...
        memcpy(&packet->certitude_list[certitude_size + 1], data.c_str(), data.size());
    }
    packet->type = DARWIN_PACKET_FILTER;
    packet->response = DARWIN_RESPONSE_SEND_DARWIN;
    packet->certitude_size = certitude_size;
    packet->filter_code = GetFilterCode();
    packet->body_size = data.size();

    std::vector<char> uuid = darwin::uuid::GenUuid();
    memcpy(packet->evt_id, uuid.data(), 16);
    DARWIN_LOG_DEBUG("AConnector::SendToFilter:: Sending header + data");
    boost::system::error_code ec;
    boost::asio::write(this->_filter_socket,
                        boost::asio::buffer(packet, packet_size), ec);
    free(packet);
    _filter_socket.close();
    if (ec) {
        DARWIN_LOG_ERROR("AConnector::SendToFilter:: 'Unable to write to output filter: " + ec.message());
        return false;
    }
    return true;
}

long AConnector::GetFilterCode() noexcept {
    return DARWIN_FILTER_BUFFER;
}

std::string AConnector::GetSource(std::map<std::string, std::string> &input_line) {
    DARWIN_LOGGER;
    if (input_line.find("source") == input_line.end()) {
        DARWIN_LOG_ERROR("AConnector::GetSource:: 'source' is missing in the input line. Output ignored.");
        return std::string();
    }
    return input_line["source"];
}

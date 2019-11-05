/// \file     Session.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     05/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <boost/bind.hpp>
#include <chrono>
#include <vector>
#include <ctime>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "Logger.hpp"
#include "Manager.hpp"
#include "Session.hpp"

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"

namespace darwin {
    Session::Session(std::string name, boost::asio::local::stream_protocol::socket& socket,
                     darwin::Manager& manager,
                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                     std::mutex& cache_mutex)
            : _filter_name(name), _socket{std::move(socket)}, _manager{manager},
              _filter_socket{socket.get_executor()}, _connected{false},
              _cache{cache}, _cache_mutex{cache_mutex} {}

    void Session::SetStartingTime() {
        starting_time = std::chrono::high_resolution_clock::now();
    }

    double Session::GetDurationMs() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - starting_time
        ).count() / ((double)1000);
    }

    void Session::Start() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Session::Start::");
        ReadHeader();
    }

    void Session::Stop() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Session::Stop::");
        _socket.close();
        if(_connected) _filter_socket.close();
    }

    void Session::SetThreshold(std::size_t const& threshold) {
        DARWIN_LOGGER;
        //If the threshold is negative, we keep the actual default threshold
        if(threshold>100){
            DARWIN_LOG_DEBUG("Session::SetThreshold:: Default threshold " + std::to_string(_threshold) + "applied");
            return;
        }
        _threshold = threshold;
    }

    void Session::SetNextFilterSocketPath(std::string const& path){
        _next_filter_path = path;
    }

    void Session::SetOutputType(std::string const& output) {
        _output = config::convert_output_string(output);
    }

    config::output_type Session::GetOutputType() {
        return _output;
    }

    /*std::string Session::GetLogs(){
        char str_time[256];
        time_t rawtime;
        struct tm * timeinfo;

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(str_time, sizeof(str_time), "%F%Z%T%z", timeinfo);

        return R"({"evt_id":")" + Evt_idToString() + R"(", "time": ")" +  str_time  + "\"" + _logs + "}";
    }*/

    std::string Session::GetLogs(){
        return _logs;
    }

    std::string Session::GetDataToSendToFilter(){
        switch (GetOutputType()){
            case config::output_type::RAW:
                return raw_body;
            case config::output_type::PARSED:
                return body;
            case config::output_type::NONE:
                return "";
            case config::output_type::LOG:
                return GetLogs();
        }
        return "";
    }

    void Session::ReadHeader() {
        DARWIN_LOGGER;

        DARWIN_LOG_DEBUG("Session::ReadHeader:: Starting to read incoming header...");
        boost::asio::async_read(_socket,
                                boost::asio::buffer(&header, sizeof(header)),
                                boost::bind(&Session::ReadHeaderCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void Session::ReadHeaderCallback(const boost::system::error_code& e,
                                     std::size_t size) {
        DARWIN_LOGGER;

        body = std::string();
        _certitudes = std::vector<unsigned int>();
        _logs = std::string();

        DARWIN_LOG_DEBUG("Session::ReadHeaderCallback:: Reading header");
        if (!e) {
            if (size != sizeof(header)) {
                DARWIN_LOG_ERROR("Session::ReadHeaderCallback:: Mismatching header size");
                goto header_callback_stop_session;
            }
            if (header.body_size == 0) {
                ExecuteFilter();
                return;
            } // Else the ReadBodyCallback will call ExecuteFilter
            ReadBody(header.body_size);
            return;
        }

        if (boost::asio::error::eof == e) {
            DARWIN_LOG_INFO("Session::ReadHeaderCallback:: " + e.message());
        } else {
            DARWIN_LOG_WARNING("Session::ReadHeaderCallback:: " + e.message());
        }

        header_callback_stop_session:
        _manager.Stop(shared_from_this());
    }

    void Session::ReadBody(std::size_t size) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Session::ReadBody:: Starting to read incoming body...");

        _socket.async_read_some(boost::asio::buffer(_buffer,
                                                    size),
                                boost::bind(&Session::ReadBodyCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void Session::ReadBodyCallback(const boost::system::error_code& e,
                                   std::size_t size) {
        DARWIN_LOGGER;

        if (!e) {
            body.append(_buffer.data(), size);
            DARWIN_LOG_DEBUG("Session::ReadBodyCallback:: Body len (" +
                             std::to_string(body.length()) +
                             ") - Header body size (" +
                             std::to_string(header.body_size) +
                             ")");
            unsigned int bodyLength = body.length();
            unsigned int totalBodyLength = header.body_size;
            if (bodyLength < totalBodyLength) {
                ReadBody(totalBodyLength - bodyLength);
            } else {
                if (body.empty()) {
                    DARWIN_LOG_WARNING("Session::ReadBodyCallback Empty body retrieved");

                    return;
                }
                if (header.body_size <= 0) {
                    DARWIN_LOG_ERROR(
                            "Session::ReadBodyCallback Body is not empty, but the header appears to be invalid"
                    );

                    return;
                }

                raw_body = body;
                if (!ParseBody()) {
                    DARWIN_LOG_DEBUG("Session::ReadBodyCallback Something went wrong while parsing the body");

                    return;
                }

                ExecuteFilter();
            }
            return;
        }
        DARWIN_LOG_ERROR("Session::ReadBodyCallback:: " + e.message());
        _manager.Stop(shared_from_this());
    }

    void Session::ExecuteFilter() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Session::ExecuteFilter::");

        (*this)();
        this->SendNext();
    }

    void Session::SendNext() {
        switch(header.response) {
            case DARWIN_RESPONSE_SEND_BOTH:
                if(this->SendToFilter()) break;
            case DARWIN_RESPONSE_SEND_BACK:
                if(not this->SendToClient()) Start();
                break;
            case DARWIN_RESPONSE_SEND_DARWIN:
                if(not this->SendToFilter()) Start();
                break;
            default:
                Start();
        }
    }

    bool Session::SendToClient() noexcept {
        DARWIN_LOGGER;
        const std::size_t certitude_size = _certitudes.size();

        /*
         * Allocate the header +
         * the size of the certitude -
         * DEFAULT_CERTITUDE_LIST_SIZE certitude already in header size
         */
        std::size_t packet_size = 0;
        if (certitude_size > DEFAULT_CERTITUDE_LIST_SIZE) {
            packet_size = sizeof(darwin_filter_packet_t) +
                (certitude_size - DEFAULT_CERTITUDE_LIST_SIZE) * sizeof(unsigned int);
        } else {
            packet_size = sizeof(darwin_filter_packet_t);
        }

        DARWIN_LOG_DEBUG("Session::SendResToSession: Computed packet size: " + std::to_string(packet_size));

        darwin_filter_packet_t* packet;
        packet = (darwin_filter_packet_t *) malloc(packet_size);

        if (!packet) {
            DARWIN_LOG_CRITICAL("Session::SendResToSession: Could not create a Darwin packet");
            return false;
        }

        /*
         * Initialisation of the structure for the padding bytes because of
         * missing __attribute__((packed)) in the protocol structure.
         */
        memset(packet, 0, packet_size);

        for (std::size_t index = 0; index < certitude_size; ++index) {
            packet->certitude_list[index] = _certitudes[index];
        }

        packet->type = DARWIN_PACKET_FILTER;
        packet->response = header.response;
        packet->certitude_size = certitude_size;
        packet->filter_code = GetFilterCode();
        packet->body_size = 0;
        memcpy(packet->evt_id, header.evt_id, 16);

        boost::asio::async_write(_socket,
                                boost::asio::buffer(packet, packet_size),
                                boost::bind(&Session::SendToClientCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));

        free(packet);
        return true;
    }

    bool Session::SendToFilter() noexcept {
        DARWIN_LOGGER;

        if (!_next_filter_path.compare("no")) {
            DARWIN_LOG_NOTICE("Session::SendToFilter:: No next filter provided. Ignoring...");
            return false;
        }

        if (!_connected) {
            DARWIN_LOG_DEBUG("Session::SendToFilter:: Trying to connect to: " +
                             _next_filter_path);
            try {
                _filter_socket.connect(
                        boost::asio::local::stream_protocol::endpoint(
                                _next_filter_path.c_str()));
                _connected = true;
            } catch (std::exception const& e) {
                DARWIN_LOG_ERROR(std::string("Session::SendToFilter:: "
                                             "Unable to connect to next filter: ") +
                                 e.what());
                return false;
            }
        }

        std::string data = GetDataToSendToFilter();
        DARWIN_LOG_DEBUG("Session::SendToFilter:: data to send: " + data);
        DARWIN_LOG_DEBUG("Session::SendToFilter:: data size: " + std::to_string(data.size()));

        const std::size_t certitude_size = _certitudes.size();

        /*
         * Allocate the header +
         * the size of the certitude -
         * DEFAULT_CERTITUDE_LIST_SIZE certitude already in header size
         */
        std::size_t packet_size = 0;
        if (certitude_size > DEFAULT_CERTITUDE_LIST_SIZE) {
            packet_size = sizeof(darwin_filter_packet_t) +
                (certitude_size - DEFAULT_CERTITUDE_LIST_SIZE) * sizeof(unsigned int);
        } else {
            packet_size = sizeof(darwin_filter_packet_t);
        }

        packet_size += data.size();

        DARWIN_LOG_DEBUG("Session::SendToFilter:: Computed packet size: " + std::to_string(packet_size));

        darwin_filter_packet_t* packet;
        packet = (darwin_filter_packet_t *) malloc(packet_size);

        if (!packet) {
            DARWIN_LOG_CRITICAL("Session:: SendToFilter:: Could not create a Darwin packet");
            return false;
        }

        /*
         * Initialisation of the structure for the padding bytes because of
         * missing __attribute__((packed)) in the protocol structure.
         */
        memset(packet, 0, packet_size);

        for (std::size_t index = 0; index < certitude_size; ++index) {
            packet->certitude_list[index] = _certitudes[index];
        }

        if(data.size() != 0) {
            // TODO: set a proper pointer in protocol.h for the body
            // Yes We Hack...
            memcpy(&packet->certitude_list[certitude_size+1], data.c_str(), data.size());
        }

        packet->type = DARWIN_PACKET_FILTER;
        packet->response = header.response == DARWIN_RESPONSE_SEND_BOTH ? DARWIN_RESPONSE_SEND_DARWIN : header.response;
        packet->certitude_size = certitude_size;
        packet->filter_code = GetFilterCode();
        packet->body_size = data.size();
        memcpy(packet->evt_id, header.evt_id, 16);

        DARWIN_LOG_DEBUG("Session:: SendToFilter:: Sending header + data");
        boost::asio::async_write(_filter_socket,
                            boost::asio::buffer(packet, packet_size),
                            boost::bind(&Session::SendToFilterCallback, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));

        free(packet);
        return true;
    }

    void Session::SendToClientCallback(const boost::system::error_code& e,
                               std::size_t size __attribute__((unused))) {
        DARWIN_LOGGER;

        if (e) {
            DARWIN_LOG_ERROR("Session::SendCallback:: " + e.message());
            _manager.Stop(shared_from_this());
            DARWIN_LOG_DEBUG("Session::SendCallback:: Stopped session in manager");
            return;
        }

        Start();
    }

    void Session::SendToFilterCallback(const boost::system::error_code& e,
                                       std::size_t size __attribute__((unused))) {
        DARWIN_LOGGER;

        if (e) {
            DARWIN_LOG_ERROR("Session::SendToFilterCallback:: " + e.message());
            _filter_socket.close();
            _connected = false;
        }

        if(header.response == DARWIN_RESPONSE_SEND_BOTH) {
            this->SendToClient();
        }
        else {
            Start();
        }
    }

    xxh::hash64_t Session::GenerateHash() {
        // could be easily overridden in the children
        return xxh::xxhash<64>(body);
    }

    void Session::SaveToCache(const xxh::hash64_t &hash, const unsigned int certitude) const {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("SaveToCache:: Saving certitude " + std::to_string(certitude) + " to cache");
        std::unique_lock<std::mutex> lck{_cache_mutex};
        _cache->insert(hash, certitude);
    }

    bool Session::GetCacheResult(const xxh::hash64_t &hash, unsigned int& certitude) {
        DARWIN_LOGGER;
        boost::optional<unsigned int> cached_certitude;

        {
            std::unique_lock<std::mutex> lck{_cache_mutex};
            cached_certitude = _cache->get(hash);
        }

        if (cached_certitude != boost::none) {
            certitude = cached_certitude.get();
            DARWIN_LOG_DEBUG("GetCacheResult:: Already processed request. Cached certitude is " +
                             std::to_string(certitude));

            return true;
        }

        return false;
    }

    std::string Session::Evt_idToString() {
        char str[37] = {};
        snprintf(str,
                37,
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                header.evt_id[0], header.evt_id[1], header.evt_id[2], header.evt_id[3],
                header.evt_id[4], header.evt_id[5], header.evt_id[6], header.evt_id[7],
                header.evt_id[8], header.evt_id[9], header.evt_id[10], header.evt_id[11],
                header.evt_id[12], header.evt_id[13], header.evt_id[14], header.evt_id[15]
        );
        std::string res(str);
        return res;
    }

    std::string Session::GetFilterName() {
        return _filter_name;
    }
}
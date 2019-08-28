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
    Session::Session(boost::asio::local::stream_protocol::socket& socket,
                     darwin::Manager& manager,
                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache)
            : _socket{std::move(socket)}, _manager{manager},
              _filter_socket{socket.get_executor()}, _connected{false}, _cache{cache} {}

    void Session::SendResToSession() noexcept {
        DARWIN_LOGGER;
        const std::size_t certitude_size = _certitudes.size();

        darwin_filter_packet_t* packet;
        packet = (darwin_filter_packet_t *) malloc(offsetof(darwin_filter_packet_t, certitude_list[certitude_size]));

        if (!packet) {
            DARWIN_LOG_CRITICAL("Session::SendResToSession: Could not create a Darwin packet");
            return;
        }

        for (std::size_t index = 0; index < certitude_size; ++index) {
            packet->certitude_list[index] = _certitudes[index];
        }

        packet->type = DARWIN_PACKET_FILTER;
        packet->response = header.response;
        packet->certitude_size = certitude_size;
        packet->filter_code = GetFilterCode();
        packet->body_size = 0;
        memcpy(packet->evt_id, header.evt_id, 16);

        Send(*packet, nullptr);

        free(packet);
    }

    void Session::SendToDarwin() noexcept {
        DARWIN_LOGGER;
        std::string data = GetDataToSendToFilter();

        const std::size_t certitude_size = _certitudes.size();

        darwin_filter_packet_t* packet;
        packet = (darwin_filter_packet_t *) malloc(offsetof(darwin_filter_packet_t, certitude_list[certitude_size]));

        if (!packet) {
            DARWIN_LOG_CRITICAL("Session:: SendResToSession: Could not create a Darwin packet");
            return;
        }

        for (std::size_t index = 0; index < certitude_size; ++index) {
            packet->certitude_list[index] = _certitudes[index];
        }

        packet->type = DARWIN_PACKET_FILTER;
        packet->response = header.response;
        packet->certitude_size = certitude_size;
        packet->filter_code = GetFilterCode();
        packet->body_size = data.size();
        memcpy(packet->evt_id, header.evt_id, 16);

        if(packet->body_size <= 0) {
            DARWIN_LOG_DEBUG("Session:: SendToDarwin: no data");
            SendToFilter(*packet, nullptr);
        } else {
            DARWIN_LOG_DEBUG("Session:: SendToDarwin: Send data : " + data);
            SendToFilter(*packet, data.c_str());
        }

        free(packet);
    }

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
        DARWIN_LOG_WARNING("Session::ReadHeaderCallback:: " + e.message());

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
        Start();
    }

    void Session::SendCallback(const boost::system::error_code& e,
                               std::size_t size __attribute__((unused))) {
        DARWIN_LOGGER;

        if (e) {
            DARWIN_LOG_ERROR("Session::SendCallback:: " + e.message());
            _manager.Stop(shared_from_this());
        }
    }

    void Session::Send(darwin_filter_packet_t const& hdr,
                       void const* data) {
        DARWIN_LOGGER;

        // Here is the synchronous way.
        // I let it because it may be useful if you want to create
        // synchronous methods or for debug purposes.
//        DARWIN_LOG_DEBUG("Session::Send:: Sending header to session...");
//        try {
//            boost::asio::write(_socket, boost::asio::buffer(&hdr, sizeof(hdr)));
//        } catch (boost::system::system_error const& e) {
//            DARWIN_LOG_ERROR(std::string(
//                    "Session::Send:: Error sending header to session: ") +
//                      e.what());
//            _manager.Stop(shared_from_this());
//            return;
//        }
        DARWIN_LOG_DEBUG("Session::Send:: Async sending header to session...");

        std::size_t packet_size;

        if (hdr.certitude_size > 1) {
            packet_size = sizeof(hdr) + sizeof(unsigned int) * (hdr.certitude_size - 1);
        } else {
            packet_size = sizeof(hdr);
        }

        DARWIN_LOG_DEBUG("Session::Send:: Computed packet size: " + std::to_string(packet_size));

        boost::asio::async_write(_socket,
                                 boost::asio::buffer(&hdr, packet_size),
                                 boost::bind(&Session::SendCallback, this,
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
        if (data != nullptr && hdr.body_size > 0) {
//            DARWIN_LOG_DEBUG("Session::Send:: Sending body to session...");
//            try {
//                boost::asio::write(_socket,
//                                   boost::asio::buffer(data, hdr.body_size));
//            } catch (boost::system::system_error const& e) {
//                DARWIN_LOG_ERROR(std::string(
//                        "Session::Send:: Error sending header to session: ") +
//                          e.what());
//                _manager.Stop(shared_from_this());
//                return;
//            }
            DARWIN_LOG_DEBUG("Session::Send:: Async sending body to session...");
            boost::asio::async_write(_socket,
                                     boost::asio::buffer(data, hdr.body_size),
                                     boost::bind(&Session::SendCallback, this,
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred));
        }
    }

    void Session::SendToFilter(darwin_filter_packet_t const& hdr,
                               void const* data) {
        DARWIN_LOGGER;

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
                return;
            }
        }

        // Here is the synchronous way.
        // I let it because it may be useful if you want to create
        // synchronous methods or for debug purposes.
        DARWIN_LOG_DEBUG("Session::SendToFilter:: Sending header to filter...");
        try {
            boost::asio::write(_filter_socket,
                               boost::asio::buffer(&hdr, sizeof(hdr)));
        } catch (boost::system::system_error const& e) {
            DARWIN_LOG_ERROR(std::string(
                                     "Session::SendToFilter:: Error sending header to filter: ") +
                                     e.what());
            _filter_socket.close();
            _connected = false;
            return;
        }
//        DARWIN_LOG_DEBUG("Session::SendToFilter:: Async sending header to filter...");
//        boost::asio::async_write(_filter_socket,
//                                 boost::asio::buffer(&hdr, sizeof(hdr)),
//                                 boost::bind(
//                                         &Session::SendToFilterCallback,
//                                         this,
//                                         boost::asio::placeholders::error,
//                                         boost::asio::placeholders::bytes_transferred));
        if (data != nullptr && hdr.body_size > 0) {
            DARWIN_LOG_DEBUG("Session::SendToFilter:: Sending body to filter...");
            try {
                boost::asio::write(_filter_socket,
                                   boost::asio::buffer(data, hdr.body_size));
            } catch (boost::system::system_error const& e) {
                DARWIN_LOG_ERROR(std::string(
                                         "Session::SendToFilter:: Error sending body to filter: ") +
                                         e.what());
                _filter_socket.close();
                _connected = false;
                return;
            }
//            DARWIN_LOG_DEBUG("Session::SendToFilter:: Async sending body to filter...");
//            boost::asio::async_write(_filter_socket,
//                                     boost::asio::buffer(data, hdr.body_size),
//                                     boost::bind(&Session::SendToFilterCallback,
//                                                 this,
//                                                 boost::asio::placeholders::error,
//                                                 boost::asio::placeholders::bytes_transferred));
        }
    }

    void Session::SendToFilterCallback(const boost::system::error_code& e,
                                       std::size_t size __attribute__((unused))) {
        DARWIN_LOGGER;

        if (e) {
            DARWIN_LOG_ERROR("Session::SendToFilterCallback:: " + e.message());
            _filter_socket.close();
            _connected = false;
        }
    }

    xxh::hash64_t Session::GenerateHash() {
        // could be easily overridden in the children
        return xxh::xxhash<64>(body);
    }

    void Session::SaveToCache(const xxh::hash64_t &hash, const unsigned int certitude) const {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("SaveToCache:: Saving certitude " + std::to_string(certitude) + " to cache");
        _cache->insert(hash, certitude);
    }

    bool Session::GetCacheResult(const xxh::hash64_t &hash, unsigned int& certitude) {
        DARWIN_LOGGER;
        boost::optional<unsigned int> cached_certitude;
        cached_certitude = _cache->get(hash);

        if (cached_certitude != boost::none) {
            certitude = cached_certitude.get();
            DARWIN_LOG_DEBUG("GetCacheResult:: Already processed request. Cached certitude is " +
                             std::to_string(certitude));

            return true;
        }

        return false;
    }

    std::string Session::Evt_idToString(){
        char str[37] = {};
        sprintf(str,
                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                header.evt_id[0], header.evt_id[1], header.evt_id[2], header.evt_id[3],
                header.evt_id[4], header.evt_id[5], header.evt_id[6], header.evt_id[7],
                header.evt_id[8], header.evt_id[9], header.evt_id[10], header.evt_id[11],
                header.evt_id[12], header.evt_id[13], header.evt_id[14], header.evt_id[15]
        );
        std::string res(str);
        return res;
    }


}
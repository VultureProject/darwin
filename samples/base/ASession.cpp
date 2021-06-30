/// \file     ASession.cpp
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
#include "Generator.hpp"
#include "ASession.hpp"
#include "errors.hpp"
#include "Core.hpp"

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"

namespace darwin {
    ASession::ASession(darwin::Manager& manager,
                     Generator& generator)
            : _manager{manager}, _generator{generator}, _has_next_filter{false} {}

    

    void ASession::Start() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("ASession::Start::");
        ReadHeader();
    }

    void ASession::SetOutputType(std::string const& output) {
        _output = config::convert_output_string(output);
    }

    config::output_type ASession::GetOutputType() {
        return _output;
    }

    std::string ASession::GetLogs(){
        return _logs;
    }

    std::string ASession::GetDataToSendToFilter(){
        switch (GetOutputType()){
            case config::output_type::RAW:
                return _packet.GetBody();
            case config::output_type::PARSED:
                return JsonStringify(_packet.JsonBody());
            case config::output_type::NONE:
                return "";
            case config::output_type::LOG:
                return _packet.GetLogs();
        }
        return "";
    }

    void ASession::ReadHeaderCallback(const boost::system::error_code& e,
                                     std::size_t size) {
        DARWIN_LOGGER;

        _packet.clear();
        _logs.clear();

        DARWIN_LOG_DEBUG("ASession::ReadHeaderCallback:: Reading header");
        if (!e) {
            if (size != sizeof(darwin_filter_packet_t)) {
                DARWIN_LOG_ERROR("ASession::ReadHeaderCallback:: Mismatching header size");
                goto header_callback_stop_session;
            }
            _packet = DarwinPacket(_header);
            if (_packet.GetParsedBodySize() == 0) {
                ExecuteFilter();
                return;
            } // Else the ReadBodyCallback will call ExecuteFilter
            ReadBody(_packet.GetParsedBodySize());
            return;
        }

        if (boost::asio::error::eof == e) {
            DARWIN_LOG_DEBUG("ASession::ReadHeaderCallback:: " + e.message());
        } else {
            DARWIN_LOG_WARNING("ASession::ReadHeaderCallback:: " + e.message());
        }

        header_callback_stop_session:
        _manager.Stop(shared_from_this());
    }

    void ASession::ReadBodyCallback(const boost::system::error_code& e,
                                   std::size_t size) {
        DARWIN_LOGGER;

        if (!e) {
            _packet.GetMutableBody().append(_body_buffer.data(), size);
            DARWIN_LOG_DEBUG("ASession::ReadBodyCallback:: Body len (" +
                             std::to_string(_packet.GetBody().length()) +
                             ") - Header body size (" +
                             std::to_string(_packet.GetParsedBodySize()) +
                             ")");
            size_t bodyLength = _packet.GetBody().length();
            size_t totalBodyLength = _packet.GetParsedBodySize();
            if (bodyLength < totalBodyLength) {
                ReadBody(totalBodyLength - bodyLength);
            } else {
                if (_packet.GetBody().empty()) {
                    DARWIN_LOG_WARNING("ASession::ReadBodyCallback Empty body retrieved");
                    this->SendErrorResponse("Error receiving body: Empty body retrieved", DARWIN_RESPONSE_CODE_REQUEST_ERROR);
                    return;
                }
                if (_packet.GetParsedBodySize() <= 0) { //TODO useless branch
                    DARWIN_LOG_ERROR(
                            "ASession::ReadBodyCallback Body is not empty, but the header appears to be invalid"
                    );
                    this->SendErrorResponse("Error receiving body: Body is not empty, but the header appears to be invalid", DARWIN_RESPONSE_CODE_REQUEST_ERROR);
                    return;
                }

                ExecuteFilter();
            }
            return;
        }
        DARWIN_LOG_ERROR("ASession::ReadBodyCallback:: " + e.message());
        _manager.Stop(shared_from_this());
    }

    void ASession::ExecuteFilter() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("ASession::ExecuteFilter::");
        auto t = _generator.CreateTask(shared_from_this()); //shared pt Task

        _generator.GetTaskThreadPool().post(std::bind(&ATask::run, t));
    }

    void ASession::SendNext(DarwinPacket& packet) {
        // TODO : Unsure of the logic behind the if's and and the Start calls
        switch(packet.GetResponse()) {
            case DARWIN_RESPONSE_SEND_BOTH:
                this->SendToFilter(packet);
            case DARWIN_RESPONSE_SEND_BACK:
                if(not this->SendToClient(packet)) Start();
                break;
            case DARWIN_RESPONSE_SEND_DARWIN:
                if(not this->SendToFilter(packet)) Start();
                break;
            default:
                Start();
        }
    }


    std::string ASession::JsonStringify(rapidjson::Document &json) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        json.Accept(writer);

        return std::string(buffer.GetString());
    }

    bool ASession::SendToClient(DarwinPacket& packet) noexcept {
        DARWIN_LOGGER;
        auto vec = packet.Serialize();
        DARWIN_LOG_DEBUG("ASession::SendToClient: Computed packet size: " + std::to_string(vec.size()));
        this->WriteToClient(vec);
        return true;
    }

    bool ASession::SendToFilter(DarwinPacket& packet) noexcept {
        DARWIN_LOGGER;

        if (!_has_next_filter) {
            DARWIN_LOG_NOTICE("ASession::SendToFilter:: No next filter provided. Ignoring...");
            return false;
        }

        ANextFilterConnector& c = Core::instance().GetNextFilterconnector();

        c.Send(packet);
        return true;
    }

    void ASession::SendToClientCallback(const boost::system::error_code& e,
                               std::size_t size __attribute__((unused))) {
        DARWIN_LOGGER;

        if (e) {
            DARWIN_LOG_ERROR("ASession::SendToClientCallback:: " + e.message());
            _manager.Stop(shared_from_this());
            DARWIN_LOG_DEBUG("ASession::SendToClientCallback:: Stopped session in manager");
            return;
        }

        Start();
    }

    std::string ASession::Evt_idToString() {
        DARWIN_LOGGER;

        const unsigned char * evt_id = _packet.GetEventId();
        char str[37] = {};
        snprintf(str,
                37,
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                evt_id[0], evt_id[1], evt_id[2], evt_id[3],
                evt_id[4], evt_id[5], evt_id[6], evt_id[7],
                evt_id[8], evt_id[9], evt_id[10], evt_id[11],
                evt_id[12], evt_id[13], evt_id[14], evt_id[15]
        );
        std::string res(str);
        DARWIN_LOG_DEBUG(std::string("ASession::Evt_idToString:: UUID - ") + res);
        return res;
    }

    

    void ASession::SendErrorResponse(const std::string& message, const unsigned int code) {
        if (this->_packet.GetResponse() != DARWIN_RESPONSE_SEND_BACK && this->_packet.GetResponse() != DARWIN_RESPONSE_SEND_BOTH)
            return;
        _packet.GetMutableBody().clear();
        _packet.GetMutableBody() += "{\"error\":\"" + message + "\", \"error_code\":" + std::to_string(code) + "}";
        this->SendToClient(_packet);
    }

    void ASession::SetThreshold(std::size_t const& threshold) {
        DARWIN_LOGGER;
        //If the threshold is negative, we keep the actual default threshold
        if(threshold>100){
            DARWIN_LOG_DEBUG("Session::SetThreshold:: Default threshold " + std::to_string(_threshold) + "applied");
            return;
        }
        _threshold = threshold;
    }

    size_t ASession::GetThreshold() {
        return _threshold;
    }
}

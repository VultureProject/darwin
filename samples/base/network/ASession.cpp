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
#include <iomanip>
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
            : _manager{manager}, _generator{generator} {}

    

    void ASession::Start() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("ASession::Start::");
        ReadHeader();
    }

    void ASession::SetOutputType(std::string const& output) {
        _output = config::convert_output_string(output);
    }

    config::output_type ASession::GetOutputType() const {
        return _output;
    }

    std::string ASession::GetLogs(){
        return _logs;
    }

    void ASession::ModifyDataToSendToFilter(DarwinPacket &packet) const {
        std::string& body = packet.GetMutableBody();
        switch (this->_output){
            case config::output_type::RAW:
                return;
            case config::output_type::PARSED:
                body = std::move(JsonStringify(packet.JsonBody()));
                return;
            case config::output_type::NONE:
                body.clear();
                return;
            case config::output_type::LOG:
                body = packet.GetLogs();
                return;
        }
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
            _packet = std::move(DarwinPacket(_header));
            if (_packet.GetParsedBodySize() == 0 && _packet.GetParsedCertitudeSize() <= 1) {
                ExecuteFilter();
                return;
            } // Else the ReadBodyCallback willcall ExecuteFilter
            size_t sizeToRead = _packet.GetParsedBodySize();
            if(_packet.GetParsedCertitudeSize() > 1){
                sizeToRead += _packet.GetParsedCertitudeSize() * sizeof(unsigned int);
            }

            ReadBody(sizeToRead);
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
            size_t already_parsed_size = 0;
            if(_packet.GetParsedCertitudeSize() > 1){
                while(_packet.GetParsedCertitudeSize() != _packet.GetCertitudeList().size()
                    && already_parsed_size + sizeof(unsigned int) <= size) {
                    unsigned int cert = 0;
                    std::memcpy(&cert, _body_buffer.data() + already_parsed_size, sizeof(cert));
                    already_parsed_size += sizeof(cert);
                    _packet.AddCertitude(cert);
                }
            }

            _packet.GetMutableBody().append(_body_buffer.data() + already_parsed_size, size - already_parsed_size);
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
                if (_packet.GetParsedBodySize() == 0) { 
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
        auto t = _generator.CreateTask(shared_from_this());

        _generator.GetTaskThreadPool().post(std::bind(&ATask::run, t));
    }

    void ASession::SendNext(DarwinPacket& packet) {
        switch(packet.GetResponseType()) {
            case DARWIN_RESPONSE_SEND_BOTH:
                // We try to send to filter, then send back to client anyway
                this->SendToFilter(packet);
                __attribute((fallthrough));
            case DARWIN_RESPONSE_SEND_BACK:
                this->SendToClient(packet);
                // We don't resume the session by calling Start() because the SendToClientCallback already does
                break;
            case DARWIN_RESPONSE_SEND_DARWIN:
                this->SendToFilter(packet);
                Start();
                break;
            default:
                Start();
        }
    }


    std::string ASession::JsonStringify(rapidjson::Document &json) const {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        json.Accept(writer);

        return std::string(buffer.GetString());
    }

    void ASession::SendToClient(DarwinPacket& packet) noexcept {
        DARWIN_LOGGER;
        std::vector<unsigned char> serialized_packet = std::move(packet.Serialize());
        DARWIN_LOG_DEBUG("ASession::SendToClient: Computed packet size: " + std::to_string(serialized_packet.size()));
        this->WriteToClient(serialized_packet);
    }

    void ASession::SendToFilter(DarwinPacket& packet) noexcept {
        DARWIN_LOGGER;

        ANextFilterConnector* connector_ptr = Core::instance().GetNextFilterconnector();

        if (!connector_ptr) {
            DARWIN_LOG_NOTICE("ASession::SendToFilter:: No next filter provided. Ignoring...");
            return;
        }
        this->ModifyDataToSendToFilter(packet);
        connector_ptr->Send(packet);
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

    void ASession::SendErrorResponse(const std::string& message, const unsigned int code) {
        if (this->_packet.GetResponseType() != DARWIN_RESPONSE_SEND_BACK && this->_packet.GetResponseType() != DARWIN_RESPONSE_SEND_BOTH)
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

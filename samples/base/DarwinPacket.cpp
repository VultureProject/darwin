#include "DarwinPacket.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "../../toolkit/rapidjson/writer.h"
#include "../../toolkit/rapidjson/stringbuffer.h"
#include "Logger.hpp"

namespace darwin {

    DarwinPacket::DarwinPacket(
        darwin_packet_type type, 
        darwin_filter_response_type response, 
        long filter_code,
        unsigned char event_id[16],
        size_t certitude_size,
        size_t body_size)
        :_type{type}, _response{response}, _filter_code{filter_code},
        _parsed_certitude_size{certitude_size}, _parsed_body_size{body_size} 
    {
        std::memcpy(this->_evt_id, event_id, 16);

    }

    DarwinPacket::DarwinPacket(DarwinPacket&& other) noexcept
     :  _type{std::move(other._type)}, _response{std::move(other._response)}, _filter_code{std::move(other._filter_code)},
        _parsed_certitude_size{std::move(other._parsed_certitude_size)}, _parsed_body_size{std::move(other._parsed_body_size)},
        _certitude_list{std::move(other._certitude_list)}, _body{std::move(other._body)}, 
        _parsed_body{other._parsed_body.release()}, _logs{std::move(other._logs)} 
    {
        std::memcpy(this->_evt_id, other._evt_id, sizeof(this->_evt_id));
        std::memset(other._evt_id, 0, sizeof(other._evt_id));
    }

    DarwinPacket& DarwinPacket::operator=(DarwinPacket&& other) noexcept {
        this->clear();
        _type = std::move(other._type);
        _response = std::move(other._response);
        _filter_code = std::move(other._filter_code);
        std::memcpy(this->_evt_id, other._evt_id, sizeof(this->_evt_id));
        std::memset(other._evt_id, 0, sizeof(other._evt_id));
        _parsed_certitude_size = std::move(other._parsed_certitude_size);
        _parsed_body_size = std::move(other._parsed_body_size);
        _certitude_list = std::move(other._certitude_list);
        _body = std::move(other._body);
        _parsed_body.reset(other._parsed_body.release());
        _logs = std::move(other._logs);
        return *this;
    }

    DarwinPacket::DarwinPacket(darwin_filter_packet_t& input) 
        : _type{input.type}, _response{input.response}, _filter_code{input.filter_code},
        _parsed_certitude_size{input.certitude_size}, _parsed_body_size{input.body_size} 
    {
        std::memcpy(this->_evt_id, input.evt_id, 16);
    }

    std::vector<unsigned char> DarwinPacket::Serialize() const {
        darwin_filter_packet_t header {
            this->_type,
            this->_response,
            this->_filter_code,
            this->_body.size(),
            {0},
            this->_certitude_list.size(),
            {}
        };

        std::memcpy(header.evt_id, this->_evt_id, 16);

        size_t size = sizeof(header) + _body.size() + _certitude_list.size() * sizeof(unsigned int);

        std::vector<unsigned char> ret(size, 0);
        
        unsigned char * pt = ret.data();

        std::memcpy(pt, &header, sizeof(header));
        pt += sizeof(header);
        //certitudes
        for(size_t i=0; i < _certitude_list.size(); i++) {
            unsigned int certitude = _certitude_list[i];
            std::memcpy(pt,(void*)(&certitude), sizeof(certitude));
            pt += sizeof(certitude);
        }

        //body
        std::memcpy(pt, _body.data(), _body.length());
        return ret;
    }
    
    void DarwinPacket::clear() {
        this->_type = DARWIN_PACKET_OTHER;
        this->_response = DARWIN_RESPONSE_SEND_NO;
        this->_filter_code = 0;
        this->_parsed_body_size = 0;
        std::memset(this->_evt_id, 0, sizeof(_evt_id));
        this->_parsed_certitude_size = 0;
        _certitude_list.clear();
        _body.clear();
        this->_parsed_body.reset(nullptr);
    }

    rapidjson::Document& DarwinPacket::JsonBody() {
        if(! this->_parsed_body) {
            this->_parsed_body = std::make_unique<rapidjson::Document>();
        }
        this->_parsed_body->Parse(this->_body.c_str());
        return *(this->_parsed_body);
    }

    std::string DarwinPacket::Evt_idToString() const {
        DARWIN_LOGGER;
        std::ostringstream oss;
        auto default_flags = oss.flags();

        for(size_t i=0; i<sizeof(_evt_id);i++){
            if(i==4 || i==6 || i==8 || i==10){
                oss.flags(default_flags);
                oss << '-';
            }
            oss << std::hex << std::setw(2) << std::setfill('0') << (static_cast<int>(_evt_id[i]) & 0xFF);
        }
        DARWIN_LOG_DEBUG("DarwinPacket::Evt_idToString:: UUID - " + oss.str());
        return oss.str();
    }

    enum darwin_packet_type DarwinPacket::GetType() const {
        return this->_type;
    }

    enum darwin_filter_response_type DarwinPacket::GetResponseType() const {
        return this->_response;
    }

    void DarwinPacket::SetResponseType(enum darwin_filter_response_type response) {
        this->_response = response; 
    }

    long DarwinPacket::GetFilterCode() const {
        return this->_filter_code;
    }

    void DarwinPacket::SetFilterCode(long filter_code) {
        this->_filter_code = filter_code;
    }

    const unsigned char * DarwinPacket::GetEventId() const {
        return this->_evt_id;
    }

    size_t DarwinPacket::GetEventIdSize() const {
        return sizeof(this->_evt_id);
    }

    size_t DarwinPacket::GetParsedCertitudeSize() const {
        return this->_parsed_certitude_size;
    }

    size_t DarwinPacket::GetParsedBodySize() const {
        return this->_parsed_body_size;
    }

    const std::string& DarwinPacket::GetBody() const {
        return this->_body;
    }

    std::string& DarwinPacket::GetMutableBody() {
        return this->_body;
    }

    const std::vector<unsigned int>& DarwinPacket::GetCertitudeList() const {
        return this->_certitude_list;
    }

    std::vector<unsigned int>& DarwinPacket::GetMutableCertitudeList() {
        return this->_certitude_list;
    }

    const std::string& DarwinPacket::GetLogs() const {
        return this->_logs;
    }

    std::string& DarwinPacket::GetMutableLogs() {
        return this->_logs;
    }
    
    void DarwinPacket::SetLogs(std::string& logs) {
        this->_logs = logs;
    }
}
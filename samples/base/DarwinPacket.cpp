#include "DarwinPacket.hpp"
#include <cstring>
#include <iostream>
#include <iomanip>
#include "../../toolkit/rapidjson/writer.h"
#include "../../toolkit/rapidjson/stringbuffer.h"

namespace darwin {

    DarwinPacket::DarwinPacket(
        darwin_packet_type type, 
        darwin_filter_response_type response, 
        long filter_code,
        unsigned char event_id[16],
        size_t certitude_size,
        size_t body_size)
        : _type{type}, _response{response}, _filter_code{filter_code},
        _parsed_certitude_size{certitude_size}, _parsed_body_size{body_size} 
    {
        std::memcpy(this->_evt_id, event_id, 16);

    }

    DarwinPacket::DarwinPacket(darwin_filter_packet_t& input) 
        : _type{input.type}, _response{input.response}, _filter_code{input.filter_code},
        _parsed_certitude_size{input.certitude_size}, _parsed_body_size{input.body_size} 
    
    {
        std::memcpy(this->_evt_id, input.evt_id, 16);
        if(input.certitude_size > 0)
            AddCertitude(input.certitude_list[0]);
    }

    std::vector<unsigned char> DarwinPacket::Serialize() const {
        darwin_filter_packet_t header {
            this->_type,
            this->_response,
            this->_filter_code,
            this->_body.size(),
            {0},
            this->_certitude_list.size(),
            {0}
        };

        std::memcpy(header.evt_id, this->_evt_id, 16);

        size_t size = sizeof(header) + _body.size();
        
        if( ! _certitude_list.empty()) {
            // From the packet format, at least one certitude is always allocated
            size += (_certitude_list.size() - 1) * sizeof(unsigned int);
            header.certitude_list[0] = _certitude_list[0];
        }
            
        
        std::vector<unsigned char> ret(size, 0);
        
        unsigned char * pt = ret.data();

        std::memcpy(pt, &header, sizeof(header));
        pt += sizeof(header) - sizeof(int);

        //certitudes
        for(size_t i=1; i < _certitude_list.size(); i++) {
            auto certitude = _certitude_list[i];
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
    }

    rapidjson::Document& DarwinPacket::JsonBody() {
        if(this->_parsed_body == nullptr) {
            this->_parsed_body = new rapidjson::Document();
        }
        this->_parsed_body->Parse(this->_body.c_str());
        return *(this->_parsed_body);
    }

    DarwinPacket::~DarwinPacket(){
        delete this->_parsed_body;
    }

    enum darwin_packet_type DarwinPacket::GetType() const {
        return this->_type;
    }

    enum darwin_filter_response_type DarwinPacket::GetResponse() const {
        return this->_response;
    }

    void DarwinPacket::SetResponse(enum darwin_filter_response_type response) {
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
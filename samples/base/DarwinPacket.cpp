#include "DarwinPacket.hpp"
#include <cstring>
#include "../../toolkit/rapidjson/writer.h"
#include "../../toolkit/rapidjson/stringbuffer.h"

namespace darwin {

    std::vector<unsigned char> DarwinPacket::Serialize() const {
        darwin_filter_packet_t header {
            this->type,
            this->response,
            this->filter_code,
            this->body.size(),
            {0},
            this->certitude_list.size(),
            0
        };
        
        std::copy(&this->evt_id[0], &this->evt_id[15], &header.evt_id[0]);

        size_t body_size = body.size(), certitude_size = certitude_list.size();

        size_t size = sizeof(type) + sizeof(response) + sizeof(filter_code)
            + sizeof(body_size) + sizeof(evt_id) + sizeof(certitude_size) + certitude_list.size()
            + body.size();
        
        std::vector<unsigned char> ret(size, 0);
        
        auto pt = ret.data();

        std::memcpy(pt, &header, sizeof(header) - sizeof(unsigned int));
        pt += sizeof(header) - sizeof(unsigned int);

        //certitudes
        for(auto certitude: certitude_list) {
            std::memcpy(pt,(void*)(&certitude), sizeof(certitude));
            pt += sizeof(certitude);
        }

        //body
        std::memcpy(pt, body.data(), body.length());
        return ret;
    }

    DarwinPacket DarwinPacket::ParseHeader(darwin_filter_packet_t& input) {
        
        DarwinPacket packet;
        
        packet.type = input.type;
        packet.response = input.response;
        packet.filter_code = input.filter_code;
        packet.parsed_body_size = input.body_size;
        std::memcpy(packet.evt_id, input.evt_id, sizeof(packet.evt_id));
        packet.parsed_certitude_size = input.certitude_size;

        if(packet.parsed_certitude_size > 0)
            packet.AddCertitude(input.certitude_list[0]);
        
        return packet;
    }

    void DarwinPacket::clear() {
        this->type = DARWIN_PACKET_OTHER;
        this->response = DARWIN_RESPONSE_SEND_NO;
        this->filter_code = 0;
        this->parsed_body_size = 0;
        std::memset(this->evt_id, 0, sizeof(evt_id));
        this->parsed_certitude_size = 0;
        certitude_list.clear();
        body.clear();
    }

    rapidjson::Document& DarwinPacket::JsonBody() {
        if(this->_parsed_body == nullptr) {
            this->_parsed_body = new rapidjson::Document();
        }
        this->_parsed_body->Parse(this->body.c_str());
        return *(this->_parsed_body);
    }

    DarwinPacket::~DarwinPacket(){
        delete this->_parsed_body;
    }

    enum darwin_packet_type DarwinPacket::GetType() const {
        return this->type;
    }

    enum darwin_filter_response_type DarwinPacket::GetResponse() const {
        return this->response;
    }

    long DarwinPacket::GetFilterCode() const {
        return this->filter_code;
    }

    const unsigned char * DarwinPacket::GetEventId() const {
        return this->evt_id;
    }

    size_t DarwinPacket::GetEventIdSize() const {
        return sizeof(this->evt_id);
    }

    size_t DarwinPacket::GetParsedCertitudeSize() const {
        return this->parsed_certitude_size;
    }

    size_t DarwinPacket::GetParsedBodySize() const {
        return this->parsed_body_size;
    }

    const std::string& DarwinPacket::GetBody() const {
        return this->body;
    }

    std::string& DarwinPacket::GetMutableBody() {
        return this->body;
    }

    const std::vector<unsigned int>& DarwinPacket::GetCertitudeList() const {
        return this->certitude_list;
    }

    void DarwinPacket::AddCertitude(unsigned int certitude) {
        this->certitude_list.push_back(certitude);
    }
}
#include "DarwinPacket.hpp"
#include <cstring>
#include "../../toolkit/rapidjson/writer.h"
#include "../../toolkit/rapidjson/stringbuffer.h"

namespace darwin {

    std::vector<unsigned char> DarwinPacket::Serialize() const {
        size_t body_size = body.size(), certitude_size = certitude_list.size();

        size_t size = sizeof(type) + sizeof(response) + sizeof(filter_code)
            + sizeof(body_size) + sizeof(evt_id) + sizeof(certitude_size) + certitude_list.size()
            + body.size();
        
        std::vector<unsigned char> ret(size, 0);
        
        auto pt = ret.data();

        std::memcpy(pt,(void*)(&type), sizeof(type));
        pt += sizeof(type);

        std::memcpy(pt,(void*)(&response), sizeof(response));
        pt += sizeof(response);

        std::memcpy(pt,(void*)(&filter_code), sizeof(filter_code));
        pt += sizeof(filter_code);

        std::memcpy(pt,(void*)(&body_size), sizeof(body_size));
        pt += sizeof(body_size);

        std::memcpy(pt,(void*)(&evt_id), sizeof(evt_id));
        pt += sizeof(evt_id);

        std::memcpy(pt,(void*)(&certitude_size), sizeof(certitude_size));
        pt += sizeof(certitude_size);

        //certitudes
        for(auto certitude: certitude_list) {
            std::memcpy(pt,(void*)(&certitude), sizeof(certitude));
            pt += sizeof(certitude);
        }

        //body
        std::memcpy(pt, body.data(), body.length());
        return ret;
    }

    DarwinPacket DarwinPacket::ParseHeader(std::vector<char>& input) {
        size_t minimum_size = getMinimalSize();
        if(input.size() <= minimum_size){
            // error
        }

        DarwinPacket packet;

        auto pt = input.data();

        std::memcpy((void*)(&packet.type), pt, sizeof(type));
        pt += sizeof(type);

        std::memcpy((void*)(&packet.response), pt, sizeof(response));
        pt += sizeof(response);

        std::memcpy((void*)(&packet.filter_code), pt, sizeof(filter_code));
        pt += sizeof(filter_code);

        std::memcpy((void*)(&packet.parsed_body_size), pt, sizeof(parsed_body_size));
        pt += sizeof(parsed_body_size);

        std::memcpy((void*)(&packet.evt_id), pt, sizeof(evt_id));
        pt += sizeof(evt_id);

        std::memcpy((void*)(&packet.parsed_certitude_size), pt, sizeof(parsed_certitude_size));
        pt += sizeof(parsed_certitude_size);

        packet.certitude_list.reserve(packet.parsed_certitude_size);

        packet.body.reserve(packet.parsed_body_size);
        
        return packet;
    }


    DarwinPacket DarwinPacket::Parse(std::vector<char>& input) {
        DarwinPacket packet = ParseHeader(input);

        if (input.size() != getMinimalSize() + packet.parsed_body_size + packet.parsed_certitude_size) {
            // error
        }

        auto pt = input.data();
        pt += getMinimalSize();

        for(size_t i=0; i < packet.parsed_certitude_size; i++) {
            unsigned int cert = 0;
            std::memcpy((void*)(&cert), pt, sizeof(cert));
            pt += sizeof(cert);

            packet.certitude_list.push_back(cert);
        }

        packet.body = std::string((char*)pt, packet.parsed_body_size);

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
}
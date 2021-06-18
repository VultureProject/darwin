#include "DarwinPacket.hpp"
#include <cstring>


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

    DarwinPacket DarwinPacket::Parse(std::vector<unsigned char>& input) {
        
        size_t minimum_size = sizeof(type) + sizeof(response) + sizeof(filter_code)
            + sizeof(size_t /* body size */) + sizeof(evt_id) + sizeof(size_t /* certitude list size */);
        
        if(input.size() <= minimum_size){
            // error
        }

        DarwinPacket packet;

        size_t body_size = 0, certitude_size = 0;

        auto pt = input.data();

        std::memcpy((void*)(&packet.type), pt, sizeof(type));
        pt += sizeof(type);

        std::memcpy((void*)(&packet.response), pt, sizeof(response));
        pt += sizeof(response);

        std::memcpy((void*)(&packet.filter_code), pt, sizeof(filter_code));
        pt += sizeof(filter_code);

        std::memcpy((void*)(&body_size), pt, sizeof(body_size));
        pt += sizeof(body_size);

        std::memcpy((void*)(&packet.evt_id), pt, sizeof(evt_id));
        pt += sizeof(evt_id);

        std::memcpy((void*)(&certitude_size), pt, sizeof(certitude_size));
        pt += sizeof(certitude_size);

        if (input.size() != minimum_size + body_size + certitude_size) {
            // error
        }

        packet.certitude_list.reserve(certitude_size);

        for(size_t i=0; i < certitude_size; i++) {
            unsigned int cert = 0;
            std::memcpy((void*)(&cert), pt, sizeof(cert));
            pt += sizeof(cert);

            packet.certitude_list.push_back(cert);
        }

        packet.body = std::string((char*)pt, body_size);

        return packet;
    }
}
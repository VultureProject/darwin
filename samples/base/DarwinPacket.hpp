#pragma once

#include <vector>
#include <string>
#include <array>
#include <memory>
#include "../protocol.h"
#include "../../toolkit/rapidjson/document.h"

namespace darwin {
    struct DarwinPacket {
    public:
        DarwinPacket() = default; // todo oé oé
        ~DarwinPacket();

        enum darwin_packet_type type; //!< The type of information sent.
        enum darwin_filter_response_type response; //!< Whom the response will be sent to.
        long filter_code; //!< The unique identifier code of a filter.
        size_t parsed_body_size;
        unsigned char evt_id[16]; //!< An array containing the event ID
        size_t parsed_certitude_size;
        std::vector<unsigned int> certitude_list; //!< The scores or the certitudes of the module. May be used to pass other info in specific cases.
        std::string body;


        static DarwinPacket Parse(std::vector<char>& input); // Unsure

        std::vector<unsigned char> Serialize() const;
        
        constexpr static size_t getMinimalSize() {
            return sizeof(type) + sizeof(response) + sizeof(filter_code)
                + sizeof(size_t /* body size */) + sizeof(evt_id) + sizeof(size_t /* certitude list size */);
        };

        static DarwinPacket ParseHeader(std::vector<char>& input);

        void clear();

        rapidjson::Document& JsonBody();

    protected:


    private:

        // TODO replace with a better solution 
        // It is added as a ptr because Document, and smart pointers can't be moved but we use move semantics
        rapidjson::Document* _parsed_body = nullptr;

    };
}
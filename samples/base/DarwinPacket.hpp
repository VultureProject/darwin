#pragma once

#include <vector>
#include <string>
#include "../protocol.h"

namespace darwin {
    struct DarwinPacket {
    public:
        DarwinPacket() = default; // todo oé oé
        ~DarwinPacket() = default;

        enum darwin_packet_type type; //!< The type of information sent.
        enum darwin_filter_response_type response; //!< Whom the response will be sent to.
        long filter_code; //!< The unique identifier code of a filter.
        unsigned char evt_id[16]; //!< An array containing the event ID
        std::vector<unsigned int> certitude_list; //!< The scores or the certitudes of the module. May be used to pass other info in specific cases.
        std::string body;

        static DarwinPacket Parse(std::vector<unsigned char>& input); // Unsure

        std::vector<unsigned char> Serialize() const;


    protected:


    private:


    };
}
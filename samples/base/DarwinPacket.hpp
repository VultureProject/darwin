#pragma once

#include <vector>
#include <string>
#include <array>
#include <memory>
#include "protocol.h"
#include "../../toolkit/rapidjson/document.h"

#define DEFAULT_CERTITUDE_LIST_SIZE 1

namespace darwin {

    class DarwinPacket {
    public:
        DarwinPacket(
            enum darwin_packet_type type, 
            enum darwin_filter_response_type response, 
            long filter_code,
            unsigned char event_id[16],
            size_t certitude_size,
            size_t body_size);

        DarwinPacket(darwin_filter_packet_t& input);
        
        DarwinPacket() = default;


        virtual ~DarwinPacket();

        std::vector<unsigned char> Serialize() const;
        
        constexpr static size_t getMinimalSize() {
            return sizeof(_type) + sizeof(_response) + sizeof(_filter_code)
                + sizeof(_parsed_body_size) + sizeof(_evt_id) + sizeof(_parsed_certitude_size) 
                + DEFAULT_CERTITUDE_LIST_SIZE * sizeof(unsigned int); // Protocol has at least DEFAULT (one) certitude
        };

        static DarwinPacket ParseHeader(darwin_filter_packet_t& input);

        void clear();

        rapidjson::Document& JsonBody();

        enum darwin_packet_type GetType() const;

        enum darwin_filter_response_type GetResponse() const;

        long GetFilterCode() const;

        const unsigned char * GetEventId() const;
        
        size_t GetEventIdSize() const;

        size_t GetParsedCertitudeSize() const;

        size_t GetParsedBodySize() const;

        const std::string& GetBody() const;

        std::string& GetMutableBody();

        const std::vector<unsigned int>& GetCertitudeList() const;

        inline void AddCertitude(unsigned int certitude) {
            this->_certitude_list.push_back(certitude);
        };

        const std::string& GetLogs() const;

        std::string& GetMutableLogs();

        void SetLogs(std::string& logs);

    protected:

    private:

        // TODO replace with a better solution 
        // It is added as a ptr because Document, and smart pointers can't be moved but we use move semantics
        rapidjson::Document* _parsed_body = nullptr;

        darwin_packet_type _type; //!< The type of information sent.
        darwin_filter_response_type _response; //!< Whom the response will be sent to.
        long _filter_code; //!< The unique identifier code of a filter.
        unsigned char _evt_id[16]; //!< An array containing the event ID
        size_t _parsed_certitude_size;
        size_t _parsed_body_size;
        std::vector<unsigned int> _certitude_list; //!< The scores or the certitudes of the module. May be used to pass other info in specific cases.
        std::string _body;

        std::string _logs;

    };

    
}
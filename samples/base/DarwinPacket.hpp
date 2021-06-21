#pragma once

#include <vector>
#include <string>
#include <array>
#include <memory>
#include "../../toolkit/rapidjson/document.h"

#define DEFAULT_CERTITUDE_LIST_SIZE 1

/// Represent the receiver of the results.
///
/// \enum darwin_response_type
enum darwin_filter_response_type {
    DARWIN_RESPONSE_SEND_NO = 0,//!< Don't send results to anybody.
    DARWIN_RESPONSE_SEND_BACK, //!< Send results back to caller.
    DARWIN_RESPONSE_SEND_DARWIN, //!< Send results to the next filter.
    DARWIN_RESPONSE_SEND_BOTH, //!< Send results to both caller and the next filter.
};

/// Represent the type of information sent.
///
/// \enum darwin_packet_type
enum darwin_packet_type {
    DARWIN_PACKET_OTHER = 0, //!< Information sent by something else.
    DARWIN_PACKET_FILTER, //!< Information sent by another filter.
};

/// First packet to be sent to a filter.
///
/// \struct darwin_filter_packet_t
typedef struct {
    enum darwin_packet_type             type; //!< The type of information sent.
    enum darwin_filter_response_type    response; //!< Whom the response will be sent to.
    long                                filter_code; //!< The unique identifier code of a filter.
    size_t                              body_size; //!< The complete size of the the parameters to be sent (if needed).
    unsigned char			            evt_id[16]; //!< An array containing the event ID
    size_t                              certitude_size; //!< The size of the list containing the certitudes.
    unsigned int                        certitude_list[DEFAULT_CERTITUDE_LIST_SIZE]; //!< The scores or the certitudes of the module. May be used to pass other info in specific cases.
} darwin_filter_packet_t;

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
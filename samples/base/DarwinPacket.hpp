///
/// \file DarwinPacket.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief A Description of a darwin packet with serializing and deserializing capacity
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///
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
        ///
        /// \brief Construct a new Darwin Packet object
        /// 
        /// \param type type of the packet
        /// \param response response type of the packet
        /// \param filter_code filter code
        /// \param event_id event id
        /// \param certitude_size size of the certitude
        /// \param body_size size of the body
        ///
        DarwinPacket(
            enum darwin_packet_type type, 
            enum darwin_filter_response_type response, 
            long filter_code,
            unsigned char event_id[16],
            size_t certitude_size,
            size_t body_size);

        ///
        /// \brief Construct a new Darwin Packet object from a darwin_filter_packet_t
        /// 
        /// \param input the native struct to be parsed
        ///
        DarwinPacket(darwin_filter_packet_t& input);
        
        DarwinPacket() = default;

        DarwinPacket(DarwinPacket&& other) noexcept;

        DarwinPacket& operator=(DarwinPacket&& other) noexcept;

        virtual ~DarwinPacket() = default;

        // No copy allowed of a DarwinPacket, only moves
        // It may be implemented in the future as a deep copy if this is needed
        DarwinPacket(const DarwinPacket&) = delete;
        DarwinPacket& operator=(const DarwinPacket&) = delete;

        ///
        /// \brief Serialize the packet
        /// 
        /// \return std::vector<unsigned char> a vector holding the serialized packet
        ///
        std::vector<unsigned char> Serialize() const;
        
        ///
        /// \brief Get the Minimal Size of a packet
        /// 
        /// \return constexpr size_t size of the packet
        ///
        constexpr static size_t getMinimalSize() {
            return sizeof(_type) + sizeof(_response) + sizeof(_filter_code)
                + sizeof(_parsed_body_size) + sizeof(_evt_id) + sizeof(_parsed_certitude_size) 
                + DEFAULT_CERTITUDE_LIST_SIZE * sizeof(unsigned int); // Protocol has at least DEFAULT (one) certitude
        };

        ///
        /// \brief Clears the packet
        /// 
        ///
        void clear();

        ///
        /// \brief Parse the body as a json document and returns a reference to it
        ///        While the parsed Body is kept inside the DarwinPacket, subsequent calls to JsonBody()
        ///        will re-parse the body as the body may have been modified
        ///
        /// \return rapidjson::Document& the reference to the parsed body
        ///
        rapidjson::Document& JsonBody();

        ///
        /// \brief Get the Type
        /// 
        /// \return enum darwin_packet_type 
        ///
        enum darwin_packet_type GetType() const;

        ///
        /// \brief Get the Response Type
        /// 
        /// \return enum darwin_filter_response_type 
        ///
        enum darwin_filter_response_type GetResponseType() const;

        ///
        /// \brief Set the Response Type
        /// 
        ///
        void SetResponseType(enum darwin_filter_response_type);

        ///
        /// \brief Get the Filter Code
        /// 
        /// \return long filter code
        ///
        long GetFilterCode() const;

        ///
        /// \brief Set the Filter Code
        /// 
        /// \param filter_code 
        ///
        void SetFilterCode(long filter_code);

        ///
        /// \brief Get the Event Id pointer
        /// 
        /// \return const unsigned char* pointer of a 16 bytes buffer
        ///
        const unsigned char * GetEventId() const;
        ///
        /// \brief Get the Event Id Size 
        /// 
        /// \return size_t 16
        ///
        size_t GetEventIdSize() const;

        ///
        /// \brief Serialize the event id as a UUID String
        /// 
        /// \return std::string UUID encoded event id
        ///
        std::string Evt_idToString() const;

        ///
        /// \brief Get the Parsed Certitude Size, it may differs from GetCertitudeList().size()
        /// 
        /// \return size_t Parsed Certitude Size
        ///
        size_t GetParsedCertitudeSize() const;

        ///
        /// \brief Get the Parsed Body Size, it may differ from GetBody().size()
        /// 
        /// \return size_t Parsed Body size
        ///
        size_t GetParsedBodySize() const;

        ///
        /// \brief Get a const ref to the Body
        /// 
        /// \return const std::string& body
        ///
        const std::string& GetBody() const;

        ///
        /// \brief Get a mutable ref to the Body
        /// 
        /// \return std::string& body
        ///
        std::string& GetMutableBody();

        ///
        /// \brief Get a const ref to the Certitude List
        /// 
        /// \return const std::vector<unsigned int>& certitude list
        ///
        const std::vector<unsigned int>& GetCertitudeList() const;

        ///
        /// \brief Get a mutable ref to the Certitude List
        /// 
        /// \return std::vector<unsigned int>& certitude list
        ///
        std::vector<unsigned int>& GetMutableCertitudeList();

        ///
        /// \brief Add a certitude to the list
        /// 
        /// \param certitude to add
        ///
        inline void AddCertitude(unsigned int certitude) {
            this->_certitude_list.push_back(certitude);
        };

        ///
        /// \brief Get a const ref to the Logs
        /// 
        /// \return const std::string& logs
        ///
        const std::string& GetLogs() const;

        ///
        /// \brief Get a mutable ref to the Logs
        /// 
        /// \return std::string& logs
        ///
        std::string& GetMutableLogs();

        ///
        /// \brief Set the Logs 
        /// 
        /// \param logs 
        ///
        void SetLogs(std::string& logs);

    protected:

    private:

        darwin_packet_type _type; //!< The type of information sent.
        darwin_filter_response_type _response; //!< Whom the response will be sent to.
        long _filter_code; //!< The unique identifier code of a filter.
        unsigned char _evt_id[16]; //!< An array containing the event ID
        size_t _parsed_certitude_size;
        size_t _parsed_body_size;
        std::vector<unsigned int> _certitude_list; //!< The scores or the certitudes of the module. May be used to pass other info in specific cases.
        std::string _body;

        // Parsed Body is stored as late-initiliaized pointer because is cannot be moved, nor assigned
        // For move semantics, we want to be able to move this, hence the std::unique_ptr
        std::unique_ptr<rapidjson::Document> _parsed_body;

        std::string _logs;

    };

    
}
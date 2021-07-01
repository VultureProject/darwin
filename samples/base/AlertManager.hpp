/// \file     AlerManager.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     12/11/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>
#include "../../toolkit/FileManager.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/rapidjson/document.h"

#define DARWIN_ALERT_MANAGER darwin::AlertManager::instance()
#define DARWIN_ALERT_MANAGER_SET_RULE_NAME(str) darwin::AlertManager::instance().SetRuleName(str)
#define DARWIN_ALERT_MANAGER_SET_FILTER_NAME(str) darwin::AlertManager::instance().SetFilterName(str)
#define DARWIN_ALERT_MANAGER_SET_TAGS(str) darwin::AlertManager::instance().SetTags(str)

/// \namespace darwin
namespace darwin {

/// \class AlertManager
    class AlertManager {
    public:
        /// \brief Function to get the AlertManager or create it if he hasn't be created yet.
        /// \return Return the AlertManager object.
        static AlertManager& instance() {
            static AlertManager manager;
            return manager;
        }

        /// Send an alert message to the configured canals.
        ///
        /// \param message The alert message.
        void Alert(const std::string& message);

        /// Raise an alert with the given informations.
        ///
        /// \param entry The entry that trigered the alert
        /// \param certitude The score associated to the entry
        /// \param evt_id The unique id of the event associated to the alert
        /// \param details A 1 level JSON containing details about the alert
        /// \param tags A list containing the tags of the alert
        void Alert(const std::string& entry,
                   const unsigned int certitude,
                   const std::string& evt_id,
                   const std::string& details = "{}",
                   const std::string& tags = "");

        /// Configures the AlertManager alerting canals.
        ///
        /// \param configuration The json object containig the whole filter configuration.
        /// \return True on success, false if no alerting canal was configured.
        bool Configure(const rapidjson::Document &configuration);

        inline void SetRuleName(std::string const& rule_name) {
            this->_rule_name = rule_name;
        }

        inline void SetFilterName(std::string const& filter_name) {
            this->_filter_name = filter_name;
        }

        inline void SetTags(std::string const& tags) {
            this->_tags = tags;
        }

        /// Close and reopen the alert file to handle log rotate
        void Rotate();

    protected:
        /// \brief Create the alert JSON
        /// \param detail A string containing the details json. Defaul is an empty JSON ("{}").
        /// \return A string containing the alert JSON to log
        virtual std::string FormatLog(const std::string& entry,
                                      const unsigned int certitude,
                                      const std::string& evt_id,
                                      const std::string& details = "{}",
                                      const std::string& tags = "") const;

    private:
        /// \brief This is the constructor of AlertManager object.
        /// This constructor is private because only getAlertManager method can call it.
        AlertManager();

        /// \brief This is the destructor of AlertManager object.
        ~AlertManager() = default;

        /// \brief Copy operator.
        /// \param other, The AlertManager to copy
        /// \return *this
        AlertManager &operator=(AlertManager const &other) {
            static_cast<void>(other);
            return *this;
        };

        /// \brief Do nothing just for good working of singleton
        /// \param other useless
        AlertManager(AlertManager const &other) { static_cast<void>(other); };

        /// \brief Load the redis configuration from json object.
        /// \param configuration The configurations a json object.
        /// \return False on error, true no success.
        bool LoadRedisConfig(const rapidjson::Document& configuration);

        /// \brief Load the logs configuration from json object. Configure the FileManager.
        /// \param configuration The configurations a json object.
        /// \return False on error, true no success.
        bool LoadLogsConfig(const rapidjson::Document& configuration);

        /// \brief Configure the RedisManager.
        /// \param redis_socket_path The path to the redis socket used for alerting
        /// \return True on success, false on error
        bool ConfigRedis(const std::string redis_socket_path);

        /// \brief Configure the RedisManager.
        /// \param redis_ip The ip to the redis node used for alerting
        /// \param redis_port The port to the redis node used for alerting
        /// \return True on success, false on error
        bool ConfigRedis(const std::string redis_ip, const int redis_port);

        /// \brief Check and extract `field_name' of `configuration'. `field_name' *MUST* be a string.
        /// \param configuration The configurations a json object.
        /// \param field_name The field name. Creating a string here would be pointless.
        /// \param var The string object to fill with the extracted value. Initial value is not modified on error.
        /// \return False on error, true on success. Logs appropriately.
        static bool GetStringField(const rapidjson::Document& configuration,
                                   const char* const field_name,
                                   std::string& var);

        /// \brief Check and extract `field_name' of `configuration'. `field_name' *MUST* be an unsigned integer.
        /// \param configuration The configurations a json object.
        /// \param field_name The field name. Creating a string here would be pointless.
        /// \param var The unsigned integer object to fill with the extracted value. Initial value is not modified on error.
        /// \return False on error, true on success. Logs appropriately.
        static bool GetUIntField(const rapidjson::Document& configuration,
                                   const char* const field_name,
                                   unsigned int& var);

        /// \brief Write the logs in file
        /// \return True on success, false on error
        bool WriteLogs(const std::string& logs);

        /// \brief Write the logs in REDIS
        /// \return True on success, false on error
        bool REDISAddLogs(const std::string& logs);

    protected:
        static constexpr unsigned int RETRY = 1;
        bool _log; // If the filter will stock the data in a log file
        bool _redis; // If the filter will stock the data in a REDIS
        std::string _filter_name;
        std::string _rule_name;
        std::string _tags;
        std::string _log_file_path;
        std::string _redis_list_name;
        std::string _redis_channel_name;
        std::shared_ptr<darwin::toolkit::FileManager> _log_file = nullptr;
    };

}
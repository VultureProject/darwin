/// \file     Generator.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     22/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>

#include "../toolkit/rapidjson/document.h"
#include "Session.hpp"
#include "AGenerator.hpp"
#include "BufferThreadManager.hpp"
#include "AConnector.hpp"
#include "enums.hpp"
#include "OutputConfig.hpp"

class Generator: public AGenerator {
    /// This class his inheriting from AGenerator
    /// It reads and loads config for Filter Buffer
    /// It creates a BufferTask when input is received.
    ///
    ///\class Generator

    public:
    ///\brief Unique default constructor
    /// DOES NOT fill the attributes members.
    /// They are filled by ConfigureNetworkObject LoadConfig, LoadInputs and LoadOutputs
    Generator() = default;

    ///\brief default destructor
    virtual ~Generator() = default;

    public:
    ///\brief Creates a BufferTask when input is received
    ///
    ///\param socket Transfered to BufferTask constructor
    ///\param manager Transfered to BufferTask constructor
    ///
    ///\return A shared ptr on the newly created BufferTask (in the form of a Session)
    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

    ///\brief Creates the Connectors depending on the _output_configs vector.
    ///
    ///\param context The IO context transfered to AConnector constructor
    virtual bool ConfigureNetworkObject(boost::asio::io_context &context) override final;

    ///\brief This function takes a json object containing all the different pairs
    /// source name + redis list name and makes a vector of pairs from that
    /// This function will ignore the non well formatted sources
    ///
    ///\param array The json array to parse in a vector
    ///
    ///\return The vector containing the pairs representating the sources
    std::vector<std::pair<std::string, std::string>> FormatRedisListVector(const rapidjson::Value &array) const;

    private:
    virtual bool ConfigureAlerting(const std::string& tags) override final;

    ///\brief LoadConfig loads the config of Filter Buffer and calls LoadConnectorsConfig
    ///
    ///\param configuration The rapidjson object containing the config
    ///
    ///\return true on success, false otherwise.
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;

    ///\brief This function calls LoadInputs and LoadOutputs
    ///
    ///\param configuration The rapidjson object containing the entire config
    /// from which will be extracted input and output configs.
    ///
    ///\return true on success, false otherwise.
    bool LoadConnectorsConfig(const rapidjson::Document &configuration);

    ///\brief This function loads inputs config. Wrongly formatted input will be ignored.
    ///
    ///\param inputs_array The rapidjson array containing all inputs info
    ///
    ///\return true on success, false otherwise.
    bool LoadInputs(const rapidjson::Value &inputs_array);

    ///\brief Switches from value type in a string format to enum format
    ///
    ///\param type The string type
    ///
    ///\return The enum type
    darwin::valueType _TypeToEnum(std::string type);


    ///\brief This function loads outputs config. Wrongly formatted outputs will be ignored.
    ///
    ///\param outputs_array The rapidjson array containing all outputs infos
    ///
    ///\return true on success, false otherwise.
    bool LoadOutputs(const rapidjson::Value &outputs_array);

    ///\brief This function creates an AConnector from an OutputConfig and returns a pointer to it
    ///
    ///\param context The io_context for buffer communication
    ///\param output_config The OutputConfig to create the AConnector from.
    ///
    ///\return A newly created shared ptr on the newly created AConnector
    std::shared_ptr<AConnector> _CreateOutput(boost::asio::io_context &context, OutputConfig &output_config);

    /// The bufferTreadManager created in ConfigureNetworkObject. It handles all the threads needed.
    std::shared_ptr<BufferThreadManager> _buffer_thread_manager;

    /// The vector of inputs names and types
    std::vector<std::pair<std::string, darwin::valueType>> _inputs;

    /// The vector of OutputConfigs
    std::vector<OutputConfig> _output_configs;

    /// The vector of AConnectors
    std::vector<std::shared_ptr<AConnector>> _outputs;
};

#ifndef DARWIN_YARA_HPP
#define DARWIN_YARA_HPP

extern "C" {
#include <yara.h>
}

#include <memory>
#include <vector>
#include <set>
#include "rapidjson/document.h"

namespace darwin {

    namespace toolkit {

        class YaraEngine {
        public:
            /// The YaraEngine constructor
            /// \param fastmode if set to on, won't search several times for the same rule. Default is true
            /// \param timeout delay (in seconds) before returing from scan. Default is zero (0), meaning no delay
            YaraEngine(bool fastmode = true, int timeout = 0);

            /// The YaraEngine destructor
            ~YaraEngine();

            /// Initializes the engine with compiled rules
            /// \warning this is a necessary step before scaning
            /// \param rules a pointer on the compiled rules
            /// \return whether initialization was successful
            bool Init(YR_RULES *rules);

            /// Scans an array of bytes, and give back a score
            /// \param data the vector of bytes to scan
            /// \param certitude a reference on the certitude field to update, will get the score from the scan if it was successful
            /// \return 0 if no rules were found, 1 if at least a rule was found, -1 if an error occured
            /// \warning the engine should be initialised with Init() first
            int ScanData(std::vector<unsigned char> &data, unsigned int &certitude);

            /// Gets a rapidjson document with the results from the last scan
            /// the results are formated directly as an array
            /// \return the rapidjson document in the form [{"rule": "_rule_name_", "tags": ["_tag1_", "_tag2_", ...]}]
            rapidjson::Document GetResults();

        private:
            /// A callback to get information from the yara library during the scan
            /// \param message the type of message received
            /// \param messageData a pointer on the data returned, depending on _message_
            /// \param userData a pointer on previously configured data to return with the message
            static int ScanOrImportCallback(int message, void *messageData, void *userData);

            /// A simple function to add a rule to the list of matching rules
            /// \param rule a pointer on the rule to add
            void AddRuleToMatch(YR_RULE *rule);

        private:
            YR_SCANNER *_scanner = nullptr; //the scanner
            bool _fastmode; //fastmode = stop scanning after first found occurence
            std::set<YR_RULE*> _rule_match_list; //list of matching rules
            int _timeout; //number of seconds before stopping scanning
        };

        class YaraCompiler {
        public:
            enum Status {
                NO_RULES,
                NEW_RULES,
                RULES_COMPILED,
                ERROR}; //status of compiler

            /// The YaraCompiler constructor
            YaraCompiler() = default;

            /// The YaraCompiler destructor
            ~YaraCompiler();

            /// Initializes the compiler with yara libraries
            /// \return true if the initialization was successful, false otherwise
            bool Init();

            /// Adds the yar rules contained in the file to the list of rules to compile
            /// \warning the compiler whould be initialised with Init() first
            /// \param file the FILE handle on the file containing the rules
            /// \param namespace the (potential) namespace to set the rule(s) to (see [libyara documentation](https://yara.readthedocs.io/en/latest/capi.html) for more details)
            /// \param filename the name of the file (used for error/callback message generation)
            /// \return true if the rules were added successfuly, false otherwise
            bool AddRuleFile(FILE *file, std::string nameSpace, std::string filename);

            /// Compiles all the added rules
            /// \warning rules should be added with AddRuleFile() first
            /// \return true if the rules were compiled successfuly, false otherwise
            bool CompileRules();

            /// Returns the current status of the compiler
            /// \return one of YaraCompiler::Status
            Status GetStatus();

            /// Helper function to get an YaraEngine directly initialised with the compiled rules
            /// \warning rules should be added with AddRuleFile() and compiled with CompileRules() before trying to get an Engine
            /// \param fastmode if set to on, won't search several times for the same rule. Default is false
            /// \param timeout delay (in seconds) before returing from scan. Default is zero (0), meaning no delay
            /// \return a shared pointer on an initialised YaraEngine, ready to scan data with the rules previously added and compiled on this YaraCompiler, or a nullptr if a YaraEngine could not be created
            std::shared_ptr<YaraEngine> GetEngine(bool fastmode = false, int timeout = 0);

        private:
            static void YaraErrorCallback(int errorLevel, const char *filename, int lineNumber, const char *message, void *userData);

        private:
            YR_COMPILER *_compiler = nullptr; //Yara compiler
            YR_RULES *_rules = nullptr; //Yara compiled rules
            Status _status = Status::NO_RULES;
        };
    }
}

#endif //DARWIN_YARA_HPP
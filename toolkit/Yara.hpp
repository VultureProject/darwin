#ifndef DARWIN_YARA_HPP
#define DARWIN_YARA_HPP

extern "C" {
#include <yara.h>
}

#include <vector>
#include <set>
#include "rapidjson/document.h"

namespace darwin {

    namespace toolkit {

        class YaraEngine {

        public:
            YaraEngine(YR_RULES *rules, bool fastmode = false, int timeout = 0);

            ~YaraEngine();

            unsigned int ScanData(std::vector<unsigned char> &data);

            rapidjson::Document GetResults(); 

        private:
            static int ScanOrImportCallback(int message, void *messageData, void *userData);
            void AddRuleToMatch(YR_RULE *rule);

        public:
            enum YaraEngineStatus {
                READY,
                SCANNING,
                FINISHED,
                RULE_MATCH,
                RULE_NOMATCH,
                ERROR};

        private:
            YR_RULES *_rules; //the compiled rules
            bool _fastmode; //fastmode = stop scanning after first found occurence
            std::set<YR_RULE*> _rule_match_list; //list of matching rules
            YaraEngineStatus _status; //status of the engine 
            int _timeout; //number of seconds before stopping scanning
        };

        class YaraCompiler {
        public:
            YaraCompiler();

            ~YaraCompiler();

            bool AddRuleFile(FILE *file, std::string nameSpace, std::string filename);

            YR_RULES *GetCompiledRules();

            std::shared_ptr<YaraEngine> GetEngine(bool fastode = false, int timeout = 0);

        private:
            static void YaraErrorCallback(int errorLevel, const char *filename, int lineNumber, const char *message, void *userData);

        public:
            enum YaraCompilerStatus {
                NO_RULES,
                NEW_RULES,
                ALL_RULES_COMPILED,
                ERROR}; //status of compiler

        private:
            YR_COMPILER *_compiler; //Yara compiler object
            YaraCompilerStatus _status;
        };
    }
}

#endif //DARWIN_YARA_HPP
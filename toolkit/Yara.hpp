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
            YaraEngine(bool fastmode = true, int timeout = 0);
            ~YaraEngine();

            bool Init(YR_RULES *rules);
            int ScanData(std::vector<unsigned char> &data, unsigned int &certitude);
            rapidjson::Document GetResults();

        private:
            static int ScanOrImportCallback(int message, void *messageData, void *userData);
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

            YaraCompiler() = default;
            ~YaraCompiler();

            bool Init();
            bool AddRuleFile(FILE *file, std::string nameSpace, std::string filename);
            bool CompileRules();
            Status GetStatus();
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
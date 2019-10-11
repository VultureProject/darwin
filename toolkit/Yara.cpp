#include "Yara.hpp"
#include "base/Logger.hpp"


namespace darwin {
    
    namespace toolkit {
        
        YaraEngine::YaraEngine(YR_RULES *rules, bool fastmode, int timeout): _fastmode {fastmode}, _timeout {timeout} {
            DARWIN_LOGGER;
            if(rules) {
                _rules = rules;
                _status = YaraEngineStatus::READY;
            }
            else {
                DARWIN_LOG_ERROR("Toolkit::YaraEngine::YaraEngine:: tried to create engine without valid rules");
                _status = YaraEngineStatus::ERROR;
            }
        }

        YaraEngine::~YaraEngine() {
            DARWIN_LOGGER;
            DARWIN_LOG_INFO("Toolkit::YaraEngine::~YaraEngine:: destroying set of compiled rules");
            if(_rules) yr_rules_destroy(_rules);
            DARWIN_LOG_INFO("Toolkit::YaraEngine::~YaraEngine:: finished destroying");
        }

        unsigned int YaraEngine::ScanData(std::vector<unsigned char> &data) {
            DARWIN_LOGGER;
            _rule_match_list = std::set<YR_RULE*>();
            _status = YaraEngineStatus::READY;
            int errNum;

            if(_status == YaraEngineStatus::ERROR) {
                DARWIN_LOG_WARNING("Toolkit::YaraEngine::ScanData:: cannot start scanning, engine is not ready");
                return -1;
            }

            DARWIN_LOG_DEBUG("Toolkit::YaraEngine::ScanData:: starting scan of data (" + std::to_string(data.size()) + " bytes to scan)");
            errNum = yr_rules_scan_mem(
                _rules,
                data.data(),
                data.size(),
                _fastmode ? SCAN_FLAGS_FAST_MODE : 0,
                YaraEngine::ScanOrImportCallback,
                (void *)this,
                _timeout
            );

            DARWIN_LOG_DEBUG("Toolkit::YaraEngine::ScanData:: scan finished");

            if(errNum) {
                switch(errNum) {
                    case ERROR_INSUFFICIENT_MEMORY:
                        DARWIN_LOG_ERROR("Toolkit::YaraEngine::ScanData:: could not scan memory -> insufficient memory");
                    return -1;
                    case ERROR_TOO_MANY_SCAN_THREADS:
                        DARWIN_LOG_ERROR("Toolkit::YaraEngine::ScanData:: could not scan memory -> too many scan threads");
                        return -1;
                    case ERROR_SCAN_TIMEOUT:
                        DARWIN_LOG_ERROR("Toolkit::YaraEngine::ScanData:: could not scan memory -> timeout reached");
                        return -1;
                    case ERROR_CALLBACK_ERROR:
                        DARWIN_LOG_ERROR("Toolkit::YaraEngine::ScanData:: could not scan memory -> callback error");
                        return -1;
                    case ERROR_TOO_MANY_MATCHES:
                        DARWIN_LOG_ERROR("Toolkit::YaraEngine::ScanData:: could not scan memory -> too many matches");
                        return -1;
                    default:
                        DARWIN_LOG_ERROR("Toolkit::YaraEngine::ScanData:: could not scan memory -> unknown error");
                        return -1;
                }
            }

            if(_status == YaraEngineStatus::RULE_MATCH) {
                return 100;
            }

            return 0;
        }

        rapidjson::Document YaraEngine::GetResults() {
            const char *yaraRuleTag;
            rapidjson::Document rules;
            rapidjson::Document::AllocatorType& alloc = rules.GetAllocator();

            if(_rule_match_list.size() > 0) {
                rules.SetArray();
                
                for(auto rule : _rule_match_list) {
                    rapidjson::Value ruleJson(rapidjson::kObjectType);
                    rapidjson::Value tags(rapidjson::kArrayType);
                    ruleJson.AddMember("rule", rapidjson::StringRef(rule->identifier), alloc);

                    yr_rule_tags_foreach(rule, yaraRuleTag) {
                        tags.PushBack(rapidjson::StringRef(yaraRuleTag), alloc);
                    }

                    ruleJson.AddMember("tags", tags, alloc);
                    rules.PushBack(ruleJson, alloc);
                }
            }
            else {
                rules.SetNull();
            }

            return rules;
        }

        int YaraEngine::ScanOrImportCallback(int message, void *messageData, void *userData) {
            DARWIN_LOGGER;
            YR_RULE *rule;

            YaraEngine *engine = (YaraEngine *)userData;

            switch(message) {
                case CALLBACK_MSG_RULE_MATCHING:
                    rule = (YR_RULE *)messageData;
                    engine->AddRuleToMatch(rule);
                    DARWIN_LOG_INFO("Toolkit::YaraEngine::ScanOrImportCallback:: rule match");
                    break;
                case CALLBACK_MSG_RULE_NOT_MATCHING:
                    break;
                case CALLBACK_MSG_SCAN_FINISHED:
                    break;
                case CALLBACK_MSG_IMPORT_MODULE:
                    break;
                case CALLBACK_MSG_MODULE_IMPORTED:
                    break;
            }

            return CALLBACK_CONTINUE;
        }

        void YaraEngine::AddRuleToMatch(YR_RULE *rule) {
            _rule_match_list.insert(rule);
            _status = YaraEngineStatus::RULE_MATCH;
        }
    } // namespace toolkit  
} // namespace darwin


namespace darwin {

    namespace toolkit {

        YaraCompiler::YaraCompiler() {
            DARWIN_LOGGER;
            if(yr_initialize() != 0) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::YaraCompiler:: could not initialize yara module");
            }

            if(yr_compiler_create(&_compiler) != 0) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::YaraCompiler:: could not create the compiler, do we have enough memory ?");
            }

            yr_compiler_set_callback(_compiler, YaraCompiler::YaraErrorCallback, this);
        }

        YaraCompiler::~YaraCompiler() {
            DARWIN_LOGGER;
            yr_compiler_destroy(_compiler);

            if(yr_finalize() != 0) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::~YaraCompiler:: could not finalize yara module");
            }
        }

        bool YaraCompiler::AddRuleFile(FILE *file, std::string nameSpace, std::string filename) {
            DARWIN_LOGGER;
            int errNum;

            if(!file) {
                DARWIN_LOG_WARNING("Toolkit::YaraCompiler::AddRuleFie:: file provided is not valid");
                return false;
            }

            if(_status == YaraCompilerStatus::ALL_RULES_COMPILED) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::AddRuleFie:: cannot add new rules to compiler after compilation");
                return false;
            }

            errNum = yr_compiler_add_file(_compiler, file, nameSpace.empty() ? nullptr : nameSpace.c_str(), filename.c_str());
            if(errNum){
                DARWIN_LOG_WARNING("Toolkit::YaraCompiler::AddRuleFile:: found " + std::to_string(errNum) + " error/warning while compiling file " + filename);
                return false;
            }

            DARWIN_LOG_INFO("Toolkit::YaraCompiler::AddRuleFile:: added file '" + filename + "' to compiler");
            _status = YaraCompilerStatus::NEW_RULES;

            return true;
        }

        YR_RULES *YaraCompiler::GetCompiledRules() {
            DARWIN_LOGGER;
            int errNum;
            YR_RULES *compiled_rules;

            if(_status == YaraCompilerStatus::NO_RULES) {
                DARWIN_LOG_WARNING("Toolkit::YaraCompiler::GetCompiledRules:: trying to get compiled rules without adding any (please call AddRuleXXX first)");
                return nullptr;
            }

            errNum = yr_compiler_get_rules(_compiler, &compiled_rules);
            if(errNum) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::GetCompiledRules:: could not compile rules, do we have enough memory ?");
                return nullptr;
            }

            _status = YaraCompilerStatus::ALL_RULES_COMPILED;

            return compiled_rules;
        }

        std::shared_ptr<YaraEngine> YaraCompiler::GetEngine(bool fastmode, int timeout) {
            DARWIN_LOGGER;
            YR_RULES *rules = this->GetCompiledRules();
            if(!rules) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::GetEngine:: Could not get compiled rules for engine");
                return nullptr;
            }

            return std::make_shared<YaraEngine>(rules, fastmode, timeout);
        }

        void YaraCompiler::YaraErrorCallback(int errorLevel, const char *filename, int lineNumber, const char *message, void *userData) {
            DARWIN_LOGGER;
            char errStr[2048];
            YaraCompiler *compiler = (YaraCompiler *)userData;
            if(filename) {
                std::snprintf(errStr, 2048, "Toolkit::YaraCompiler::YaraErrorCallback: on file '%s' (line %d) -> %s", filename, lineNumber, message);
                if(errorLevel == YARA_ERROR_LEVEL_WARNING) {
                    DARWIN_LOG_WARNING(errStr);
                }
                else {
                    DARWIN_LOG_ERROR(errStr);
                    compiler->_status = YaraCompilerStatus::ERROR;
                }
            }
            else {
                std::snprintf(errStr, 2048, "Toolkit::YaraCompiler::YaraErrorCallback: %s", message);
                if(errorLevel == YARA_ERROR_LEVEL_WARNING) {
                    DARWIN_LOG_WARNING(errStr);
                }
                else {
                    DARWIN_LOG_ERROR(errStr);
                    compiler->_status = YaraCompilerStatus::ERROR;
                }
            }

            return;
        }
    }
}

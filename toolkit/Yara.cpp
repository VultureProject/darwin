#include "Yara.hpp"
#include "base/Logger.hpp"


namespace darwin {

    namespace toolkit {

        YaraEngine::YaraEngine(bool fastmode, int timeout): _fastmode {fastmode}, _timeout {timeout} {}

        YaraEngine::~YaraEngine() {
            DARWIN_LOGGER;
            DARWIN_LOG_INFO("Toolkit::YaraEngine::~YaraEngine:: destroying set of compiled rules");
            if(_scanner) yr_scanner_destroy(_scanner);
            DARWIN_LOG_INFO("Toolkit::YaraEngine::~YaraEngine:: finished destroying");
        }

        bool YaraEngine::Init(YR_RULES *rules) {
            DARWIN_LOGGER;
            int errCode = yr_scanner_create(rules, &_scanner);

            if(errCode == ERROR_INSUFFICIENT_MEMORY) {
                DARWIN_LOG_ERROR("Toolkit::YaraEngine::Init:: Could not initialize scanner, do we have enough memory ?");
                return false;
            }

            yr_scanner_set_callback(_scanner, YaraEngine::ScanOrImportCallback, this);
            yr_scanner_set_timeout(_scanner, _timeout);
            yr_scanner_set_flags(_scanner, _fastmode ? SCAN_FLAGS_FAST_MODE : 0);
            return true;
        }

        int YaraEngine::ScanData(std::vector<unsigned char> &data, unsigned int& certitude) {
            DARWIN_LOGGER;
            _rule_match_list = std::set<YR_RULE*>();
            int errNum;

            if(not _scanner) {
                DARWIN_LOG_ERROR("Toolkit::YaraEngine::ScanData:: trying to start a scan without a valid scanner");
                return -1;
            }

            DARWIN_LOG_DEBUG("Toolkit::YaraEngine::ScanData:: starting scan of data (" + std::to_string(data.size()) + " bytes to scan)");
            errNum = yr_scanner_scan_mem(
                _scanner,
                data.data(),
                data.size()
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

            if(_rule_match_list.size() > 0) {
                certitude = 100;
                return 1;
            }

            certitude = 0;
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
        }
    } // namespace toolkit
} // namespace darwin


namespace darwin {

    namespace toolkit {

        bool YaraCompiler::Init() {
            DARWIN_LOGGER;
            if(yr_initialize() != 0) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::YaraCompiler:: could not initialize yara module");
                _status = Status::ERROR;
                return false;
            }

            if(yr_compiler_create(&_compiler) != 0) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::YaraCompiler:: could not create the compiler, do we have enough memory ?");
                _status = Status::ERROR;
                return false;
            }

            yr_compiler_set_callback(_compiler, YaraCompiler::YaraErrorCallback, this);
            return true;
        }

        YaraCompiler::~YaraCompiler() {
            DARWIN_LOGGER;
            yr_compiler_destroy(_compiler);
            if(_rules) yr_rules_destroy(_rules);

            if(yr_finalize() != 0) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::~YaraCompiler:: could not finalize yara module");
            }
        }

        YaraCompiler::Status YaraCompiler::GetStatus() {
            return _status;
        }

        bool YaraCompiler::AddRuleFile(FILE *file, std::string nameSpace, std::string filename) {
            DARWIN_LOGGER;
            int errNum;

            if(!file) {
                DARWIN_LOG_WARNING("Toolkit::YaraCompiler::AddRuleFile:: file provided is not valid");
                return false;
            }

            if(_status == Status::ERROR) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::AddRuleFile:: cannot add new rules, compiler is unclean");
                return false;
            }

            if(_status == Status::RULES_COMPILED) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::AddRuleFile:: cannot add new rules to compiler after compilation");
                return false;
            }

            errNum = yr_compiler_add_file(_compiler, file, nameSpace.empty() ? nullptr : nameSpace.c_str(), filename.c_str());
            if(errNum){
                DARWIN_LOG_WARNING("Toolkit::YaraCompiler::AddRuleFile:: found " + std::to_string(errNum) + " error/warning while compiling file " + filename);
                _status = Status::ERROR;
                return false;
            }

            DARWIN_LOG_INFO("Toolkit::YaraCompiler::AddRuleFile:: added file '" + filename + "' to compiler");
            _status = Status::NEW_RULES;

            return true;
        }

        bool YaraCompiler::CompileRules() {
            DARWIN_LOGGER;
            int errNum = 0;

            switch(_status) {
                case Status::ERROR:
                    DARWIN_LOG_ERROR("Toolkit::YaraCompiler::CompileRules:: trying to compile rules when compiler is unclean");
                    return false;
                case Status::NO_RULES:
                    DARWIN_LOG_WARNING("Toolkit::YaraCompiler::CompileRules:: trying to compile rules without adding any (please call AddRuleXXX first)");
                    return false;
                case Status::RULES_COMPILED:
                    DARWIN_LOG_INFO("Toolkit::YaraCompiler::CompileRules:: Rules are already compiled, skipping");
                    return true;
                case Status::NEW_RULES:
                    errNum = yr_compiler_get_rules(_compiler, &_rules);
                    break;
                default:
                    DARWIN_LOG_ERROR("Toolkit::YaraCompiler::CompileRules:: unhandled status '" + std::to_string(_status) + "', aborting!");
                    abort();
            }

            if(errNum) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::CompileRules:: could not compile rules, do we have enough memory ?");
                _status = Status::ERROR;
                return false;
            }

            _status = Status::RULES_COMPILED;
            return true;
        }

        std::shared_ptr<YaraEngine> YaraCompiler::GetEngine(bool fastmode, int timeout) {
            DARWIN_LOGGER;
            std::shared_ptr<YaraEngine> engine = nullptr;

            if(_status != Status::RULES_COMPILED) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::GetEngine:: Could not get engine, no rules compiled");
                return nullptr;
            }

            engine = std::make_shared<YaraEngine>(fastmode, timeout);
            if(engine->Init(_rules) != true) {
                DARWIN_LOG_ERROR("Toolkit::YaraCompiler::GetEngine:: Could not initialize engine");
                return nullptr;
            }

            return engine;
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
                    compiler->_status = Status::ERROR;
                }
            }
            else {
                std::snprintf(errStr, 2048, "Toolkit::YaraCompiler::YaraErrorCallback: %s", message);
                if(errorLevel == YARA_ERROR_LEVEL_WARNING) {
                    DARWIN_LOG_WARNING(errStr);
                }
                else {
                    DARWIN_LOG_ERROR(errStr);
                    compiler->_status = Status::ERROR;
                }
            }

            return;
        }
    }
}

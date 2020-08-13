/* yara_utils.c
 *
 *  This file contains functions related to yara API
 *
 * File begun on 2018-27-5
 *
 * Created by:
 *  - Théo Bertin (theo.bertin@advens.fr)
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "yara_utils.hpp"
#include "Logger.hpp"

YaraCnf *globalYaraCnf;

YaraRuleList *yaraCreateRuleList() {
    YaraRuleList *ruleList = (YaraRuleList *)calloc(1, sizeof(YaraRuleList));

    ruleList->list = (YR_RULE **)calloc(RULELIST_DEFAULT_INIT_SIZE, sizeof(YR_RULE *));
    ruleList->size = RULELIST_DEFAULT_INIT_SIZE;

    return ruleList;
}

void yaraDeleteRuleList(YaraRuleList *ruleList) {
    if(ruleList) {
        if(ruleList->list) free(ruleList->list);
        free(ruleList);
    }
    return;
}

void yaraAddRuleToList(YaraRuleList *list, YR_RULE *rule) {
    DARWIN_LOGGER;
    if(list && rule) {
        if(list->size == list->fill) {
            list->list = (YR_RULE **)realloc((YR_RULE **)list->list, sizeof(YR_RULE *)*(list->size + RULELIST_DEFAULT_INIT_SIZE));
            list->size += RULELIST_DEFAULT_INIT_SIZE;
        }
        list->list[list->fill++] = rule;
    }
    else {
        DARWIN_LOG_WARNING("could not add rule to list, list or rule is NULL");
    }

    return;
}

int yaraIsRuleInList(YaraRuleList *list, YR_RULE *rule) {
    DARWIN_LOGGER;
    if(list && rule) {
        uint32_t i;
        for(i = 0; i < list->fill; i++) {
            if(strcmp(list->list[i]->identifier, rule->identifier) == 0) {
                return 1;
            }
        }
        return 0;
    }
    else {
        DARWIN_LOG_WARNING("could not search rule in list, list or rule is NULL");
    }
    return 0;
}

int yaraInitConfig(YaraCnf *conf) {
    DARWIN_LOGGER;
    if(!conf) {
        DARWIN_LOG_ERROR("invalid yara conf passed");
        return -1;
    }

    if(yr_initialize()) {
        DARWIN_LOG_ERROR("could not initialize yara module");
        return -1;
    }

    if(yr_compiler_create(&conf->compiler)) {
        DARWIN_LOG_ERROR("could not create compiler, insufficient memory");
        return -1;
    }
    yr_compiler_set_callback(conf->compiler, yaraErrorCallback, NULL);

    conf->scanMaxSize = SCAN_SIZE_DEFAULT;
    conf->scanType = SCAN_TYPE_DEFAULT;

    conf->status |= YARA_CNF_INIT;
    conf->ruleFilename = NULL;
    globalYaraCnf = conf;

    DARWIN_LOG_INFO("successfully initialized yara configuration");
    return 0;
}

int yaraDeleteConfig(YaraCnf *conf) {
    DARWIN_LOGGER;
    if(yr_finalize()) {
        DARWIN_LOG_ERROR("could not finalize yara module");
        return -1;
    }

    if(conf->compiler) {
        yr_compiler_destroy(conf->compiler);
    }
    if(conf->rules) {
        yr_rules_destroy(conf->rules);
    }

    DARWIN_LOG_INFO("successfully cleared yara configuration");
    free(conf);
    return 0;
}

int yaraAddRuleFile(FILE *file, const char *nspace, const char *fileName) {
    DARWIN_LOGGER;
    int errNum;
    if(globalYaraCnf->status & YARA_CNF_INIT) {
        errNum = yr_compiler_add_file(globalYaraCnf->compiler, file, nspace, fileName);
        if(errNum) {
            DARWIN_LOG_WARNING("found " + std::to_string(errNum) + " errors while compiling a file");
            return -1;
        }

        DARWIN_LOG_INFO("added file to compiler");
        globalYaraCnf->status |= YARA_CNF_RULES_ADDED;
        return 0;
    }
    else {
        DARWIN_LOG_ERROR("trying to add ruleFile when yara context is not initialised");
        return -1;
    }
}

int yaraCompileRules() {
    DARWIN_LOGGER;
    int errNum;
    if(globalYaraCnf->status & YARA_CNF_RULES_ADDED) {
        errNum = yr_compiler_get_rules(globalYaraCnf->compiler, &globalYaraCnf->rules);
        if(errNum) {
            DARWIN_LOG_ERROR("could not compile rules->insufficient memory");
            return -1;
        }
        globalYaraCnf->status |= YARA_CNF_RULES_COMPILED;
        return 0;
    }
    else {
        DARWIN_LOG_ERROR("trying to compile rules when no rule was added to compiler");
        return -1;
    }
}

static inline int yaraScanStreamElem(YaraStreamElem *elem, int fastMode, int timeout) {
    DARWIN_LOGGER;
    int errNum;
    struct timespec start, stop;

    if(globalYaraCnf->status & YARA_CNF_RULES_COMPILED && elem->status & YSE_READY) {

        clock_gettime(CLOCK_MONOTONIC, &start);
        errNum = yr_rules_scan_mem(
                globalYaraCnf->rules,
                elem->buffer,
                elem->length,
                fastMode ? SCAN_FLAGS_FAST_MODE : 0,
                yaraScanOrImportCallback,
                (void *)elem,
                timeout);
        clock_gettime(CLOCK_MONOTONIC, &stop);

        DARWIN_LOG_DEBUG("scanning time: " + std::to_string((stop.tv_nsec - start.tv_nsec)/1000));

        if(errNum) {
            switch(errNum) {
                case ERROR_INSUFFICIENT_MEMORY:
                    DARWIN_LOG_ERROR("could not scan memory -> insufficient memory");
                    return -1;
                case ERROR_TOO_MANY_SCAN_THREADS:
                    DARWIN_LOG_ERROR("could not scan memory -> too many scan threads");
                    return -1;
                case ERROR_SCAN_TIMEOUT:
                    DARWIN_LOG_ERROR("could not scan memory -> timeout reached");
                    return -1;
                case ERROR_CALLBACK_ERROR:
                    DARWIN_LOG_ERROR("could not scan memory -> callback error");
                    return -1;
                case ERROR_TOO_MANY_MATCHES:
                    DARWIN_LOG_ERROR("could not scan memory -> too many matches");
                    return -1;
                default:
                    DARWIN_LOG_ERROR("could not scan memory -> unknown error");
                    return -1;
            }
        }

        return 0;
    }
    else {
        DARWIN_LOG_ERROR("not scanning memory -> global conf not ready or stream element not ready");
        return -1;
    }
}

YaraResults yaraScan(uint8_t *buffer, uint32_t buffLen, StreamBuffer *sb) {
    DARWIN_LOGGER;
    YaraStreamElem *elem = (YaraStreamElem *)calloc(1, sizeof(YaraStreamElem));
    elem->ruleList = yaraCreateRuleList();

    YaraResults results;

    if(!sb) {
        DARWIN_LOG_DEBUG("initializing packet scan");
        if(buffer && buffLen) {
            elem->buffer = buffer;
            elem->length = (buffLen > globalYaraCnf->scanMaxSize) ? globalYaraCnf->scanMaxSize : buffLen;
            elem->status |= YSE_READY;
        }
        else {
            DARWIN_LOG_ERROR("trying to launch packet scan without providing buffer and length");
        }
    }
    else {
        DARWIN_LOG_DEBUG("initializing stream scan");
        pthread_mutex_lock(&(sb->mutex));
        if(globalYaraCnf->scanMaxSize >= sb->bufferFill) {
            elem->buffer = sb->buffer;
            elem->length = sb->bufferFill;
        }
        else {
            elem->buffer = sb->buffer + sb->bufferFill - globalYaraCnf->scanMaxSize - 1;
            elem->length = globalYaraCnf->scanMaxSize;
        }
        elem->status |= YSE_READY;
    }

    if(elem->length) {
        if(yaraScanStreamElem(elem, 0, 1)) {
            DARWIN_LOG_ERROR("error while trying to launch scan");
        }
        else if(elem->ruleList->fill) {
            const char *yaraRuleTag;
            uint32_t i;
            DARWIN_LOG_DEBUG("found elems in scan");
            DARWIN_LOG_INFO("number of elements found in scan: " + std::to_string(elem->ruleList->fill));

            if(sb && !sb->ruleList) sb->ruleList = yaraCreateRuleList();

            for(i = 0; i < elem->ruleList->fill; i++) {
                YR_RULE *rule = elem->ruleList->list[i];

                if(sb && yaraIsRuleInList(sb->ruleList, rule)) {
                    continue;
                }
                else {

                    if(sb)  yaraAddRuleToList(sb->ruleList, rule);
                    results.rules.insert(rule->identifier);

                    yr_rule_tags_foreach(rule, yaraRuleTag)
                    {
                        results.tags.insert(yaraRuleTag);
                    }
                }
            }
        }
    }
    if(sb) pthread_mutex_unlock(&(sb->mutex));

    yaraDeleteRuleList(elem->ruleList);
    free(elem);
    return results;
}

#if YR_MAJOR_VERSION == 3
void yaraErrorCallback(int errorLevel, const char *fileName, int lineNumber, const char *message, void *userData) {
    DARWIN_LOGGER;
    char errStr[2048];
    if(fileName) {
        std::snprintf(errStr, 2048, "YARA ERROR[%d]: on file '%s' (line %d) -> %s", errorLevel, fileName, lineNumber, message);
        DARWIN_LOG_ERROR(errStr);
    }
    else {
        std::snprintf(errStr, 2048, "YARA ERROR[%d]: %s", errorLevel, message);
        DARWIN_LOG_ERROR(errStr);
    }

    return;
}

int yaraScanOrImportCallback(int message, void *messageData, void *userData) {
    DARWIN_LOGGER;
    YR_RULE *rule;
    YR_MODULE_IMPORT *import;
    YaraStreamElem *elem = (YaraStreamElem *)userData;

    if(elem) elem->status |= YSE_FINISHED;

    switch(message) {
        case CALLBACK_MSG_RULE_MATCHING:
            rule = (YR_RULE *)messageData;
            if(elem) {
                elem->status |= YSE_RULE_MATCHED;
                if(!yaraIsRuleInList(elem->ruleList, rule))  yaraAddRuleToList(elem->ruleList, rule);
            }
            DARWIN_LOG_INFO("yara rule match");
            break;
        case CALLBACK_MSG_RULE_NOT_MATCHING:
            if(elem)    elem->status |= YSE_RULE_NOMATCH;
            break;
        case CALLBACK_MSG_SCAN_FINISHED:
            break;
        case CALLBACK_MSG_IMPORT_MODULE:
            import = (YR_MODULE_IMPORT *)messageData;
            DARWIN_LOG_DEBUG("YARA IMPORT: importing module");
            break;
        case CALLBACK_MSG_MODULE_IMPORTED:
            DARWIN_LOG_DEBUG("YARA IMPORT: module imported");
    }

    return CALLBACK_CONTINUE;
}
#elif YR_MAJOR_VERSION == 4
void yaraErrorCallback(
        int errorLevel,
        const char *fileName,
        int lineNumber,
        __attribute__((unused)) const YR_RULE *rule,
        const char *message,
        void *userData) {
    DARWIN_LOGGER;
    char errStr[2048];
    if(fileName) {
        std::snprintf(errStr, 2048, "YARA ERROR[%d]: on file '%s' (line %d) -> %s", errorLevel, fileName, lineNumber, message);
        DARWIN_LOG_ERROR(errStr);
    }
    else {
        std::snprintf(errStr, 2048, "YARA ERROR[%d]: %s", errorLevel, message);
        DARWIN_LOG_ERROR(errStr);
    }

    return;
}

int yaraScanOrImportCallback(
        __attribute__((unused)) YR_SCAN_CONTEXT *context,
        int message,
        void *messageData,
        void *userData) {
    DARWIN_LOGGER;
    YR_RULE *rule;
    YR_MODULE_IMPORT *import;
    YaraStreamElem *elem = (YaraStreamElem *)userData;

    if(elem) elem->status |= YSE_FINISHED;

    switch(message) {
        case CALLBACK_MSG_RULE_MATCHING:
            rule = (YR_RULE *)messageData;
            if(elem) {
                elem->status |= YSE_RULE_MATCHED;
                if(!yaraIsRuleInList(elem->ruleList, rule))  yaraAddRuleToList(elem->ruleList, rule);
            }
            DARWIN_LOG_INFO("yara rule match");
            break;
        case CALLBACK_MSG_RULE_NOT_MATCHING:
            if(elem)    elem->status |= YSE_RULE_NOMATCH;
            break;
        case CALLBACK_MSG_SCAN_FINISHED:
            break;
        case CALLBACK_MSG_IMPORT_MODULE:
            import = (YR_MODULE_IMPORT *)messageData;
            DARWIN_LOG_DEBUG("YARA IMPORT: importing module");
            break;
        case CALLBACK_MSG_MODULE_IMPORTED:
            DARWIN_LOG_DEBUG("YARA IMPORT: module imported");
    }

    return CALLBACK_CONTINUE;
}
#endif
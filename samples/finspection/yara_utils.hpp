/* yara_utils.h
 *
 * This header contains prototypes for yara_utils.c,
 * being functions to manage yara API
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
#ifndef YARA_UTILS_H
#define YARA_UTILS_H

#include "stream_buffer.hpp"
#include "../../toolkit/rapidjson/document.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <yara.h>
#include <pthread.h>
#include <time.h>

typedef struct YaraCnf_ {
    uint8_t status;
#define YARA_CNF_NONE 0x00
#define YARA_CNF_INIT 0x01
#define YARA_CNF_RULES_ADDED 0x02
#define YARA_CNF_RULES_COMPILED 0x04

    uint8_t scanType;
#define SCAN_TYPE_DEFAULT   0
#define SCAN_NONE           0
#define SCAN_PACKET_ONLY    1
#define SCAN_STREAM         2

    uint32_t scanMaxSize;
#define SCAN_SIZE_DEFAULT   4096
    YR_COMPILER *compiler;
    YR_RULES *rules;

    char *ruleFilename;
} YaraCnf;

extern YaraCnf *globalYaraCnf;

typedef struct YaraRuleList_ {
#define RULELIST_DEFAULT_INIT_SIZE   10
    uint32_t size;
    uint32_t fill;
    YR_RULE **list;
} YaraRuleList;

typedef struct YaraStreamElem_ {
    uint8_t *buffer;
    uint32_t length;

    uint8_t status;
#define YSE_INIT 0
#define YSE_READY 1
#define YSE_PROCESS 2
#define YSE_FINISHED 4
#define YSE_RULE_MATCHED 8
#define YSE_RULE_NOMATCH 16

    YaraRuleList *ruleList;

    struct YaraStreamElem_ *next;
    struct YaraStreamElem_ *prev;
} YaraStreamElem;

YaraRuleList *yaraCreateRuleList();
void yaraDeleteRuleList(YaraRuleList *);
void yaraAddRuleToList(YaraRuleList *, YR_RULE *);
int yaraIsRuleInList(YaraRuleList *, YR_RULE *);
int yaraInitConfig(YaraCnf *);
int yaraDeleteConfig(YaraCnf *);
int yaraAddRuleFile(FILE *, const char *, const char *);
int yaraCompileRules();
rapidjson::Document yaraScan(uint8_t *, uint32_t, StreamBuffer *);
void yaraErrorCallback(int, const char *, int, const char *, void *);
int yaraScanOrImportCallback(int, void *, void *);

#ifdef __cplusplus
};
#endif

#endif /* YARA_UTILS_H */

/* stream_buffer.h
 *
 * This header contains the definition of stream buffers
 *
 * File begun on 2019-20-05
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

#ifndef STREAM_BUFFER_H
#define STREAM_BUFFER_H

#include "file_utils.hpp"
#include "data_pool.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#define DEFAULT_BUFF_START_SIZE     4096
#define BUFF_ADD_BLOCK_SIZE         4096

typedef struct StreamsCnf_ {
    char *streamStoreFolder;
    uint32_t streamMaxBufferSize;

    DataPool *sbPool;
} StreamsCnf;

extern StreamsCnf *streamsCnf;

typedef struct StreamBuffer_ {
    uint8_t *buffer;
    uint32_t bufferSize;
    uint32_t bufferFill;
    uint32_t streamOffset;

    struct YaraRuleList_ *ruleList;

    FileStruct *bufferDump;

    pthread_mutex_t mutex;

    DataObject *object;
} StreamBuffer;

void yaraDeleteRuleList(struct YaraRuleList_ *);
void streamBufferReset(void *);
void streamInitConfig(StreamsCnf *);
void streamDeleteConfig(StreamsCnf *);
int linkStreamBufferToDumpFile(StreamBuffer *, char *);
uint32_t streamBufferDumpToFile(StreamBuffer *);
int streamBufferExtend(StreamBuffer *, uint32_t);
int streamBufferAddDataSegment(StreamBuffer *, uint32_t, uint8_t *);

#ifdef __cplusplus
};
#endif

#endif /* STREAM_BUFFER_H */

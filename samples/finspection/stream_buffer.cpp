/* stream_buffer.c
 *
 * This file contains functions to bufferize streams
 *
 * File begun on 2019-20-5
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

#include "stream_buffer.hpp"
#include "Logger.hpp"

StreamsCnf *streamsCnf;

static inline int initBuffer(StreamBuffer *sb) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("initBuffer");

    sb->buffer = (uint8_t *)calloc(1, DEFAULT_BUFF_START_SIZE);

    if(sb->buffer) {
        sb->bufferSize = DEFAULT_BUFF_START_SIZE;
        return 0;
    }

    return -1;
}

static inline void *streamBufferCreate(void *object) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamBufferCreate");
    DataObject *dObject = (DataObject *)object;

    StreamBuffer *sb = (StreamBuffer *)calloc(1, sizeof(StreamBuffer));
    if(sb) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

        sb->object = dObject;
        dObject->pObject = sb;
        if(initBuffer(sb) == 0) {
            pthread_mutex_init(&(sb->mutex), &attr);
            pthread_mutexattr_destroy(&attr);
            return (void *) (sizeof(StreamBuffer) + DEFAULT_BUFF_START_SIZE);
        }

        free(sb);
    }

    return (void *)0;
}

static inline void streamBufferDelete(void *sbObject) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamBufferDelete");

    if(sbObject) {
        StreamBuffer *sb = (StreamBuffer *)sbObject;

        pthread_mutex_destroy(&(sb->mutex));

        if(sb->bufferDump) {
            streamBufferDumpToFile(sb);
            deleteFileStruct(sb->bufferDump);
        }

        if(sb->buffer) free(sb->buffer);
        if(sb->ruleList) yaraDeleteRuleList(sb->ruleList);

        free(sb);
    }
}

void streamBufferReset(void *sbObject) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamBufferReset");

    if(sbObject) {
        StreamBuffer *sb = (StreamBuffer *)sbObject;
        pthread_mutex_lock(&(sb->mutex));

        sb->bufferFill = 0;
        sb->streamOffset = 0;
        if(sb->ruleList) {
            yaraDeleteRuleList(sb->ruleList);
            sb->ruleList = NULL;
        }
        if(sb->bufferDump) {
            deleteFileStruct(sb->bufferDump);
            sb->bufferDump = NULL;
        }

        pthread_mutex_unlock(&(sb->mutex));
    }
    return;
}

void streamInitConfig(StreamsCnf *conf) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamInitConfig");
    memset(conf, 0, sizeof(StreamsCnf));

    conf->sbPool = createPool("streamBufferPool", streamBufferCreate, streamBufferDelete, streamBufferReset, 20);

    streamsCnf = conf;
    return;
}

void streamDeleteConfig(StreamsCnf *conf) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamDeleteConfig");

    if(conf->streamStoreFolder) free(conf->streamStoreFolder);
    destroyPool(conf->sbPool);
    free(conf);
}

int linkStreamBufferToDumpFile(StreamBuffer *sb, char *filename) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("linkStreamBufferToDumpFile");

    if(streamsCnf->streamStoreFolder) {
        FILE *opened = openFile(streamsCnf->streamStoreFolder, filename);
        if(!opened) return -1;

        sb->bufferDump = createFileStruct();

        strncpy(sb->bufferDump->filename, filename, 256);
        strncpy(sb->bufferDump->directory, streamsCnf->streamStoreFolder, 2048);
        sb->bufferDump->pFile = opened;
    }
    return 0;
}

uint32_t streamBufferDumpToFile(StreamBuffer *sb) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamBufferDumpToFile");
    uint32_t writeAmount = 0;

    pthread_mutex_lock(&(sb->mutex));

    if(sb->bufferDump->pFile) {
        addDataToFile((char *)(sb->buffer), sb->bufferFill, sb->streamOffset, sb->bufferDump);
        writeAmount += sb->bufferFill;
    }

    pthread_mutex_unlock(&(sb->mutex));

    return writeAmount;
}

int streamBufferExtend(StreamBuffer *sb, uint32_t extLength) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamBufferExtend: extLength=" + std::to_string(extLength));

    if(sb) {
        pthread_mutex_lock(&(sb->mutex));

        sb->buffer = (uint8_t *)realloc(sb->buffer, sb->bufferSize + extLength);
        if(sb->buffer) {
            sb->bufferSize += extLength;
            updateDataObjectSize(sb->object, (int)extLength);
            pthread_mutex_unlock(&(sb->mutex));
            return 0;
        }
        else {
            pthread_mutex_unlock(&(sb->mutex));

            updateDataObjectSize(sb->object, (int)-sb->bufferSize);
            sb->bufferSize = 0;

            DARWIN_LOG_DEBUG("error while extending stream buffer");
        }
    }
    else {
        return 0;
    }

    return -1;
}

static inline void streamBufferShift(StreamBuffer *sb, uint32_t amount) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamBufferShift, amount=" + std::to_string(amount));

    if(sb) {
        pthread_mutex_lock(&(sb->mutex));

        if(amount > sb->bufferSize) amount = sb->bufferSize;
        if(sb->bufferDump) addDataToFile((char *)sb->buffer, amount, sb->streamOffset, sb->bufferDump);
        memmove(sb->buffer, sb->buffer + amount, sb->bufferFill - amount);
        sb->bufferFill -= amount;
        sb->streamOffset += amount;

        pthread_mutex_unlock(&(sb->mutex));
    }
    else {
        DARWIN_LOG_DEBUG("streamBufferShift: ERROR trying to shift StreamBuffer, but object is NULL");
    }
    return;
}

/**
 * The data given at this point SHOULD BE the next immediate data for the stream
 * @param sb
 * @param dataLength
 * @param data
 * @return
 */
int streamBufferAddDataSegment(StreamBuffer *sb, uint32_t dataLength, uint8_t *data) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("streamBufferAddDataSegment, dataLength: " + std::to_string(dataLength));

    if(sb) {
        pthread_mutex_lock(&(sb->mutex));

        if(dataLength > streamsCnf->streamMaxBufferSize) {
            dataLength = streamsCnf->streamMaxBufferSize;
        }
        // extend buffer if not big enough
        if(sb->bufferFill + dataLength > sb->bufferSize) {
            uint32_t addition = sb->bufferFill + dataLength - sb->bufferSize;
            if(sb->bufferSize + addition <= streamsCnf->streamMaxBufferSize) {
                streamBufferExtend(sb, addition);
            }
            else if(sb->bufferSize < streamsCnf->streamMaxBufferSize) {
                uint32_t extension = streamsCnf->streamMaxBufferSize - sb->bufferSize;
                streamBufferExtend(sb, extension);
                streamBufferShift(sb, addition - extension);
            }
            else {
                streamBufferShift(sb, addition);
            }
        }
        memcpy(sb->buffer + sb->bufferFill, data, dataLength);
        sb->bufferFill += dataLength;

        pthread_mutex_unlock(&(sb->mutex));

        return 0;
    }

    return 1;
}

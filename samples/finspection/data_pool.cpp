/* data_pool.c
 *
 * This file contains functions used for memory management.
 *
 * File begun on 2019-21-06
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
#include "data_pool.hpp"
#include "Logger.hpp"
#include "tcp_sessions.hpp"
#include "flow.hpp"

PoolStorage *poolStorage;

static inline DataObject *createDataObject(DataPool *pool) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("createDataObject");

    DataObject *newDataObject = (DataObject *)malloc(sizeof(DataObject));
    if (newDataObject) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

        newDataObject->pObject = NULL;
        newDataObject->state = DataObject::INIT;
        newDataObject->stale = 1;
        pthread_mutex_init(&(newDataObject->mutex), &attr);
        newDataObject->pool = pool;
        newDataObject->size = sizeof(DataObject);

        pthread_mutexattr_destroy(&attr);
        return newDataObject;
    }

    DARWIN_LOG_ERROR("ERROR: could not create new Data Object");
    return NULL;
}

static inline void addObjectToPool(DataPool *pool, DataObject *object) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("addObjectToPool");

    pthread_mutex_lock(&(pool->mutex));

    if (pool->head) {
        object->prev = pool->head;
        pool->head->next = object;
    }
    pool->head = object;
    object->next = NULL;
    pool->listSize++;
    if (!pool->tail) pool->tail = object;

    pthread_mutex_unlock(&(pool->mutex));

    return;
}

static inline void addPoolToStorage(DataPool *pool) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("addPoolToStorage");

    if (poolStorage->head) {
        pool->prev = poolStorage->head;
        poolStorage->head->next = pool;
    }
    poolStorage->head = pool;
    pool->next = NULL;
    poolStorage->listSize++;
    if (!poolStorage->tail) poolStorage->tail = pool;

    pool->poolStorage = poolStorage;

    return;
}

uint32_t deleteDataObjectFromPool(DataObject *object, DataPool *pool) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("deleteDataObjectFromPool");
    uint32_t dataFreed = 0;

    if (pool && object && object->state != DataObject::USED) {

        pthread_mutex_lock(&(pool->mutex));

        if (object->next) object->next->prev = object->prev;
        if (object->prev) object->prev->next = object->next;
        if (pool->head == object) pool->head = object->prev;
        if (pool->tail == object) pool->tail = object->next;
        pool->listSize--;
        pool->availableElems--;
        pool->totalAllocSize -= object->size;
        dataFreed = object->size;

        pthread_mutex_lock(&(pool->poolStorage->mutex));
        pool->poolStorage->totalDataSize -= object->size;
        pthread_mutex_unlock(&(pool->poolStorage->mutex));

        pthread_mutex_unlock(&(pool->mutex));


        pool->objectDestructor(object->pObject);
        pthread_mutex_destroy(&(object->mutex));
        free(object);
    }
    return dataFreed;
}

void setObjectAvailable(DataObject *object) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("setObjectAvailable");
    pthread_mutex_lock(&(object->mutex));
    object->state = DataObject::AVAILABLE;
    object->pool->objectResetor(object->pObject);
    object->pool->availableElems++;
    pthread_mutex_unlock(&(object->mutex));
}

void updateDataObjectSize(DataObject *object, int diffSize) {
    if (object) {
        pthread_mutex_lock(&(object->mutex));
        object->size += diffSize;
        pthread_mutex_unlock(&(object->mutex));

        pthread_mutex_lock(&(object->pool->mutex));
        object->pool->totalAllocSize += diffSize;
        pthread_mutex_unlock(&(object->pool->mutex));

        pthread_mutex_lock(&(object->pool->poolStorage->mutex));
        object->pool->poolStorage->totalDataSize += diffSize;
        pthread_mutex_unlock(&(object->pool->poolStorage->mutex));
    }
    return;
}

DataObject *getOrCreateAvailableObject(DataPool *pool) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("getOrCreateAvailableObject");

    pthread_mutex_lock(&(pool->mutex));

    DataObject *object = pool->tail;
    while (object) {
        pthread_mutex_lock(&(object->mutex));
        if (object->state == DataObject::AVAILABLE) {
            pthread_mutex_unlock(&(object->mutex));
            break;
        }
        pthread_mutex_unlock(&(object->mutex));
        object = object->next;
    }

    if (!object) {
        if (poolStorage->totalDataSize >= poolStorage->maxDataSize) {
            DARWIN_LOG_WARNING("WARNING: max memory usage reached, cannot create new objects");
            pthread_mutex_unlock(&(pool->mutex));
            return NULL;
        }

        object = createDataObject(pool);
        uint64_t sizeAlloc = (uint64_t)pool->objectConstructor((void *) object);

        if (sizeAlloc > 0) {
            object->size += sizeAlloc;
            addObjectToPool(pool, object);
            object->pool->totalAllocSize += object->size;
            object->pool->poolStorage->totalDataSize += object->size;
            pool->availableElems++;
        } else {
            pthread_mutex_destroy(&(object->mutex));
            free(object);
            pthread_mutex_unlock(&(pool->mutex));
            return NULL;
        }
    }

    object->state = DataObject::USED;
    object->stale = 0;
    pool->availableElems--;

    pthread_mutex_unlock(&(pool->mutex));

    return object;
}

DataPool *createPool(const char *poolName, constructor_t objectConstructor,
                     destructor_t objectDestructor, resetor_t objectResetor,
                     uint32_t minAvailableElems) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("createPool");

    DataPool *newPool = (DataPool *)malloc(sizeof(DataPool));
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (newPool) {
        strncpy(newPool->poolName, poolName, 50);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if (pthread_mutex_init(&(newPool->mutex), &attr) != 0) {
            DARWIN_LOG_ERROR("ERROR: could not initialize mutex while creating new pool");
            return NULL;
        }
        newPool->head = NULL;
        newPool->tail = NULL;
        newPool->listSize = 0;
        newPool->objectConstructor = objectConstructor;
        newPool->objectDestructor = objectDestructor;
        newPool->objectResetor = objectResetor;
        newPool->totalAllocSize = sizeof(DataPool);
        newPool->minAvailableElems = minAvailableElems;
        newPool->availableElems = 0;
    } else {
        DARWIN_LOG_ERROR("ERROR: could not create new pool");
    }
    pthread_mutexattr_destroy(&attr);

    addPoolToStorage(newPool);
    return newPool;
}

void destroyPool(DataPool *pool) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("destroyPool");

    pthread_mutex_lock(&(pool->mutex));

    DataObject *object = pool->tail, *destroy;
    while (object != NULL) {
        destroy = object;
        object = object->next;
        pthread_mutex_destroy(&(destroy->mutex));
        if (destroy->pObject) pool->objectDestructor(destroy->pObject);
        free(destroy);
    }

    pthread_mutex_unlock(&(pool->mutex));
    pthread_mutex_destroy(&(pool->mutex));
    free(pool);

    return;
}

PoolStorage *initPoolStorage() {
    DARWIN_LOGGER;
    PoolStorage *poolStorage = (PoolStorage *)malloc(sizeof(PoolStorage));

    if (poolStorage) {
        poolStorage->head = NULL;
        poolStorage->tail = NULL;
        poolStorage->listSize = 0;
        poolStorage->totalDataSize = 0;
        poolStorage->maxDataSize = DEFAULT_MAX_POOL_STORAGE_SIZE;
        pthread_mutex_init(&(poolStorage->mutex), NULL);
        return poolStorage;

    }

    DARWIN_LOG_ERROR("ERROR: could not initialize pool storage");
    return NULL;
}

void deletePoolStorage(PoolStorage *poolStorage) {
    if (poolStorage) {
        pthread_mutex_destroy(&(poolStorage->mutex));
        free(poolStorage);
    }
    return;
}


/* ###################### */
/* --- memory manager --- */
/* ###################### */

void *memoryManagerDoWork(void *pData) {
    DARWIN_LOGGER;
    MemManagerParams *params = (MemManagerParams *)pData;
    DARWIN_LOG_INFO("memory manager: started");

    struct timespec waitTime;

    while(1) {
        clock_gettime(CLOCK_REALTIME, &waitTime);
        waitTime.tv_sec += 10;

        pthread_mutex_lock(&(params->mSignal));
        pthread_cond_timedwait(&(params->cSignal), &(params->mSignal), &waitTime);

        if(params->sigStop) {
            DARWIN_LOG_INFO("memory manager: closing");
            pthread_mutex_unlock(&(params->mSignal));
            pthread_exit(0);
        }

        DARWIN_LOG_DEBUG("memory manager: launching cleanup");
        DataPool *pool;
        uint32_t totalMemFreed = 0;
        for(pool = poolStorage->tail; pool != NULL; pool = pool->next) {
            pthread_mutex_lock(&(pool->mutex));
            uint32_t freeAmount = 0, usedAmount = 0;
            DataObject *object = pool->tail, *deleteElem;
            while(object) {
                deleteElem = object;
                object = object->next;
                pthread_mutex_lock(&(deleteElem->mutex));

                if(deleteElem->state != DataObject::USED) freeAmount++;
                else usedAmount++;

                if(deleteElem->state != DataObject::USED && deleteElem->stale &&
                pool->availableElems > pool->minAvailableElems) {
                    totalMemFreed += deleteDataObjectFromPool(deleteElem, pool);
                    DARWIN_LOG_DEBUG("memory manager: deleting object in '" + std::string(pool->poolName) +
                                     "', new listSize: " + std::to_string(pool->listSize));
                    continue;
                }
                else if(!deleteElem->stale){
                    deleteElem->stale = 1;
                }
                pthread_mutex_unlock(&(deleteElem->mutex));
            }
            pthread_mutex_unlock(&(pool->mutex));
            DARWIN_LOG_DEBUG("memory manager: " + std::to_string(pool->availableElems) + " free, "
                             + std::to_string(pool->listSize) + " total in '" + std::string(pool->poolName) +
                            ", for " + std::to_string(pool->totalAllocSize) + " total memory");
        }

        DARWIN_LOG_DEBUG("memory manager: cleanup finished, memory freed: " + std::to_string(totalMemFreed) + ","
                  " total memory used: " + std::to_string(poolStorage->totalDataSize));

        DARWIN_LOG_DEBUG("memory manager: starting TCP session cleanup");
        DataObject *sessObject;
        TcpSession *session;
        Flow *flow;
        time_t currentTime = time(NULL);

        pthread_mutex_lock(&(sessPool->mutex));
        for(sessObject = sessPool->tail; sessObject != NULL; sessObject = sessObject->next) {
            if(sessObject->state == DataObject::AVAILABLE) continue;
            session = (TcpSession*)sessObject->pObject;
            if(session) {
                flow = session->flow;
                if(flow) {
                    if(session->cCon->state > TCP_SESS_ESTABLISHED &&
                       session->sCon->state > TCP_SESS_ESTABLISHED &&
                       (currentTime - flow->lastPacketTime) > 10) {
                        DARWIN_LOG_DEBUG("memory manager: found closed session, freeing");
                        pthread_mutex_lock(&(flow->mFlow));
                        setObjectAvailable(sessObject);
                        pthread_mutex_unlock(&(flow->mFlow));
                    }
                    else if((currentTime - flow->lastPacketTime) > 300) {
                        DARWIN_LOG_DEBUG("memory manager: found idle session, freeing");
                        pthread_mutex_lock(&(flow->mFlow));
                        setObjectAvailable(sessObject);
                        pthread_mutex_unlock(&(flow->mFlow));
                    }
                }
            }
        }
        pthread_mutex_unlock(&(sessPool->mutex));
        DARWIN_LOG_DEBUG("memory manager: finished TCP session cleanup");

        pthread_mutex_unlock(&(params->mSignal));
    }
}

MemManagerParams *initMemoryManager() {
    MemManagerParams *memManagerParams = (MemManagerParams *)malloc(sizeof(MemManagerParams));

    pthread_mutex_init(&(memManagerParams->mSignal), NULL);
    pthread_cond_init(&(memManagerParams->cSignal), NULL);
    memManagerParams->sigStop = 0;

    return memManagerParams;
}

void startMemoryManager(MemManagerParams *memManagerParams) {
    DARWIN_LOGGER;
    if(memManagerParams) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        if(pthread_create(&(memManagerParams->thread), &attr, memoryManagerDoWork, (void *)memManagerParams) != 0) {
            memManagerParams->thread = 0;
            pthread_attr_destroy(&attr);
            return;
        }

        pthread_attr_destroy(&attr);
        return;
    }
    else {
        DARWIN_LOG_DEBUG("cannot start memory manager: object given is not initialised");
    }
    return;
}

void stopMemoryManager(MemManagerParams *memManagerParams) {
    if(memManagerParams) {
        pthread_mutex_lock(&(memManagerParams->mSignal));
        memManagerParams->sigStop = 1;
        pthread_cond_signal(&(memManagerParams->cSignal));
        pthread_mutex_unlock(&(memManagerParams->mSignal));

        pthread_join(memManagerParams->thread, NULL);

        free(memManagerParams);
        return;
    }
}
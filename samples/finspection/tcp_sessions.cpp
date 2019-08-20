/* tcp_sessions.c
 *
 * This file contains functions to handle TCP sessions
 *
 * File begun on 2018-12-5
 *
 * Created by:
 *  - François Bernard (francois.bernard@isen.yncrea.fr)
 *  - Théo Bertin (theo.bertin@isen.yncrea.fr)
 *  - Tianyu Geng (tianyu.geng@isen.yncrea.fr)
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

#include "tcp_sessions.hpp"
#include "Logger.hpp"

DataPool *queuePool;
DataPool *connPool;
DataPool *sessPool;

static inline void *tcpQueueCreate(void *object) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpQueueCreate");
    DataObject *dObject = (DataObject *)object;

    TcpQueue *queue = (TcpQueue *)calloc(1, sizeof(TcpQueue));
    if(queue) {
        queue->object = dObject;
        dObject->pObject = (void *)queue;
        queue->data = (uint8_t *)calloc(1, DEFAULT_QUEUE_BUFFER_SIZE);
        if(queue->data) queue->dataBufferSize = DEFAULT_QUEUE_BUFFER_SIZE;
        else queue->dataBufferSize = 0;
        return (void *)(sizeof(TcpQueue) + queue->dataBufferSize);
    }
    DARWIN_LOG_ERROR("tcp_sessions:: could not create new TcpQueue");
    return (void *)0;
}

static inline void tcpQueueDelete(void *queueObject) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpQueueDelete");

    if(queueObject) {
        TcpQueue *queue = (TcpQueue *)queueObject;
        if(queue->data) free(queue->data);
    }
    return;
}

static inline void tcpQueueReset(void *queueObject) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpQueueReset");

    if(queueObject) {
        TcpQueue *queue = (TcpQueue *)queueObject;
        queue->tcp_flags[0] = '\0';
        queue->seq = 0;
        queue->ack = 0;
        queue->dataLength = 0;
        queue->used = 0;
        queue->prev = NULL;
        queue->next = NULL;
    }
    return;
}

static inline void *tcpConnectionCreate(void *object) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpConnectionCreate");
    DataObject *dObject = (DataObject *)object;

    TcpConnection *conn = (TcpConnection *)calloc(1, sizeof(TcpConnection));
    if(conn) {
        conn->object = dObject;
        dObject->pObject = (void *)conn;
        DataObject *sbObject = getOrCreateAvailableObject(streamsCnf->sbPool);
        if(sbObject) {
            conn->streamBuffer = (StreamBuffer *)sbObject->pObject;
            return (void *)sizeof(TcpConnection);
        }
        else {
            free(conn);
            dObject->pObject = NULL;
            DARWIN_LOG_WARNING("tcp_sessions::could not get new StreamBuffer from pool, aborting TcpConnection object creation");
            return NULL;
        }
    }

    DARWIN_LOG_ERROR("tcp_sessions::could not create new TcpConnection");
    return (void *)0;
}

static inline void tcpConnectionDelete(void *connObject) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpConnectionDelete");

    if(connObject) {
        TcpConnection *conn = (TcpConnection *)connObject;
        setObjectAvailable(conn->streamBuffer->object);
        TcpQueue *current, *queue = conn->queueTail;
        while(queue) {
            current = queue;
            queue = queue->next;
            setObjectAvailable(current->object);
        }

        free(conn);
    }

    return;
}

static inline void tcpConnectionReset(void *connObject) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpConnectionReset");

    if(connObject) {
        TcpConnection *conn = (TcpConnection *)connObject;
        conn->sPort = 0;
        conn->state = TCP_NONE;
        conn->initSeq = 0;
        conn->nextSeq = 0;
        conn->lastAck = 0;

        TcpQueue *queue = conn->queueTail, *current;

        while(queue) {
            current = queue;
            queue = queue->next;
            setObjectAvailable(current->object);
        }
        conn->queueSize = 0;

        conn->queueHead = NULL;
        conn->queueTail = NULL;
    }
    return;
}

static inline void *tcpSessionCreate(void *object) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpSessionCreate");
    DataObject *dObject = (DataObject *)object;

    TcpSession *tcpSession = (TcpSession *)calloc(1, sizeof(TcpSession));

    if(tcpSession) {
        tcpSession->object = dObject;
        dObject->pObject = (void *)tcpSession;
        DataObject *srcConnObject = getOrCreateAvailableObject(connPool);
        DataObject *dstConnObject = getOrCreateAvailableObject(connPool);

        if(!srcConnObject || !dstConnObject) {
            DARWIN_LOG_WARNING("could not get new TcpConnection objects, aborting");
            free(tcpSession);
            if(srcConnObject) {
                pthread_mutex_lock(&(srcConnObject->mutex));
                srcConnObject->state = DataObject::AVAILABLE;
                pthread_mutex_unlock(&(srcConnObject->mutex));
            }
            if(dstConnObject) {
                pthread_mutex_lock(&(dstConnObject->mutex));
                dstConnObject->state = DataObject::AVAILABLE;
                pthread_mutex_unlock(&(dstConnObject->mutex));
            }
            return (void *)0;
        }

        tcpSession->cCon = (TcpConnection *)srcConnObject->pObject;
        tcpSession->sCon = (TcpConnection *)dstConnObject->pObject;
        return (void *)sizeof(TcpSession);
    }

    DARWIN_LOG_ERROR("tcp_sessions::could not create new TcpSession");
    return (void *)0;
}

static inline void tcpSessionDelete(void *sessionObject) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpSessionDelete");

    if(sessionObject) {
        TcpSession *session = (TcpSession *)sessionObject;
        setObjectAvailable(session->cCon->object);
        setObjectAvailable(session->sCon->object);
        free(session);
    }

    return;
}

static inline void tcpSessionReset(void *sessionObject) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpSessionReset");

    if(sessionObject) {
        TcpSession *session = (TcpSession *)sessionObject;
        tcpConnectionReset(session->sCon);
        tcpConnectionReset(session->cCon);

        session->flow->protoCtx = NULL;
        session->flow = NULL;
    }
    return;
}

int initTCPPools() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::initTCPPools");

    queuePool = createPool("tcpQueuePool", tcpQueueCreate, tcpQueueDelete, tcpQueueReset, 40);
    connPool = createPool("tcpConnectionPool", tcpConnectionCreate, tcpConnectionDelete, tcpConnectionReset, 40);
    sessPool = createPool("tcpSessionPool", tcpSessionCreate, tcpSessionDelete, tcpSessionReset, 20);
    return !queuePool || !connPool || !sessPool;
}

void destroyTCPPools() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::destroyTCPPools");

    destroyPool(queuePool);
    destroyPool(connPool);
    destroyPool(sessPool);
    return;
}

static inline void swapTcpConnections(TcpSession *tcpSession) {
    TcpConnection *tmp = tcpSession->cCon;
    tcpSession->cCon = tcpSession->sCon;
    tcpSession->sCon = tmp;

    return;
}

static inline TcpQueue *packetEnqueue(Packet *pkt) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::packetEnqueue");

    DataObject *queueObject = getOrCreateAvailableObject(queuePool);
    TcpQueue *queue;
    if(queueObject) queue = (TcpQueue *)queueObject->pObject;
    else {
        DARWIN_LOG_WARNING("tcp_sessions::could not get new TcpQueue object, aborting");
        return NULL;
    }

    DARWIN_LOG_DEBUG(std::string(pkt->tcph->flags));
    strncpy(queue->tcp_flags, pkt->tcph->flags, 10);
    queue->seq = pkt->tcph->seq;
    queue->ack = pkt->tcph->ack;
    queue->dataLength = pkt->tcph->TCPDataLength;
    if(queue->dataLength) {
        if(queue->dataLength > queue->dataBufferSize) {
            DARWIN_LOG_DEBUG("tcp_sessions:: packetEnqueue:: resizing buffer");
            uint8_t *newBuffer = (uint8_t *)realloc(queue->data, queue->dataLength);
            if(newBuffer) {
                queue->data = newBuffer;
                updateDataObjectSize(queue->object, (int)(queue->dataLength - queue->dataBufferSize));
                queue->dataBufferSize = queue->dataLength;
            }
            else {
                DARWIN_LOG_ERROR("tcp_sessions::could not realloc buffer of TcpQueue, using old buffer");
                queue->dataLength = queue->dataBufferSize;
            }
        }
        DARWIN_LOG_DEBUG("tcp_sessions:: packetEnqueue:: copying data to buffer, number of bytes: " + std::to_string(queue->dataLength));
        memcpy(queue->data, pkt->payload, queue->dataLength);
    }

    return queue;
}

int tcpSessionInitFromPacket(TcpSession *tcpSession, Packet *pkt) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpSessionInitFromPacket");

    if(pkt && tcpSession) {
        if(pkt->proto == IPPROTO_TCP && pkt->tcph) {
            char flags[10];
            uint32_t tcpDataLength = pkt->tcph->TCPDataLength;
            TcpConnection *srcCon = tcpSession->cCon;
            TcpConnection *dstCon = tcpSession->sCon;
            TCPHdr *header = pkt->tcph;

            srcCon->initSeq = header->seq;
            srcCon->lastAck = header->ack;
            srcCon->sPort = pkt->sp;
            dstCon->sPort = pkt->dp;
            tcpSession->flow = pkt->flow;

            strncpy(flags, header->flags, 10);

            if(HAS_TCP_FLAG(flags, 'S') && HAS_TCP_FLAG(flags, 'A')) {
                // connection was initiated by the destination, we need to swap connections
                swapTcpConnections(tcpSession);
                swapFlowDirection(tcpSession->flow);
                srcCon->state = TCP_SYN_SENT;
                dstCon->state = TCP_SYN_RECV;
                srcCon->initSeq = header->ack-1;
                srcCon->nextSeq = header->ack;
                dstCon->nextSeq = header->seq + 1;
            }
            else if(HAS_TCP_FLAG(flags, 'S')) {
                // connection is beginning
                srcCon->state = TCP_SYN_SENT;
                dstCon->state = TCP_LISTEN;
                srcCon->nextSeq = srcCon->initSeq + 1;
            }
            else if(HAS_TCP_FLAG(flags, 'F')) {
                /* connection is closing, but specific state is unknown
                 * it's not a problem as there is still at least one packet to receive */
                dstCon->state = TCP_FIN_WAIT1;
                srcCon->state = TCP_CLOSE_WAIT;

                srcCon->nextSeq = header->seq + 1;
            }
            else if(HAS_TCP_FLAG(flags, 'A')) {
                // connection is established or closing
                srcCon->state = TCP_ESTABLISHED;
            }
            else {
                // probably RST or illegal state, dropping
                return 1;
            }


            if(tcpDataLength) {
                uint32_t dataLength = tcpDataLength;
                streamBufferAddDataSegment(srcCon->streamBuffer, dataLength, pkt->payload);
                srcCon->nextSeq = srcCon->initSeq + dataLength;
            }

            return 0;
        }
    }

    return -1;
}

int tcpConnectionsUpdateFromQueueElem(TcpConnection *srcCon, TcpConnection *dstCon, TcpQueue *queue) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpConnectionsUpdateFromQueueElem");

    if(queue && srcCon && dstCon) {
        queue->used = 1;

        char flags[10];
        uint32_t tcpDataLength = queue->dataLength;

        strncpy(flags, queue->tcp_flags, 10);

        // if connection was init while active
        if(!srcCon->initSeq) {
            srcCon->initSeq = queue->seq - 1; /* to "simulate" influence of SYN packet for dataLength calculations */
            srcCon->nextSeq = srcCon->initSeq + 1;
        }

        if(tcpDataLength) {
            uint32_t dataLength = tcpDataLength;
            uint32_t offset = queue->seq - srcCon->initSeq - 1 /* SYN packet = seq+1 but no data */;
            if(srcCon->state > TCP_ESTABLISHED && offset > 0) {
                offset--; /* FIN packet = seq+1 but no data */
            }

            streamBufferAddDataSegment(srcCon->streamBuffer, dataLength, queue->data);
            srcCon->nextSeq += tcpDataLength;
        }

        if(HAS_TCP_FLAG(flags, 'R')){
            srcCon->state = TCP_CLOSED;
            dstCon->state = TCP_CLOSED;
            DARWIN_LOG_INFO("tcp_sessions::tcp session RESET");
        }
        else if(HAS_TCP_FLAG(flags, 'S') && HAS_TCP_FLAG(flags, 'A')) {
            srcCon->state = TCP_SYN_RECV;
            srcCon->initSeq = queue->seq;
            srcCon->nextSeq = srcCon->initSeq + 1;
        }
        else if(HAS_TCP_FLAG(flags, 'F')) {
            srcCon->nextSeq += 1;

            if(srcCon->state == TCP_CLOSE_WAIT) {
                srcCon->state = TCP_LAST_ACK;
            }
            else if(srcCon->state <= TCP_ESTABLISHED) {
                srcCon->state = TCP_FIN_WAIT1;
                /* to ease computation, we assume destination
                 * received this packet */
                dstCon->state = TCP_CLOSE_WAIT;
            }
        }
        else if(HAS_TCP_FLAG(flags, 'A')) {
            if(srcCon->state == TCP_FIN_WAIT1) {
                srcCon->state = TCP_CLOSING;
            }
            else if(srcCon->state == TCP_FIN_WAIT2) {
                srcCon->state = TCP_TIME_WAIT;
            }
            else if(srcCon->state <= TCP_SYN_SENT) {
                srcCon->state = TCP_ESTABLISHED;
            }

            if(dstCon->state == TCP_TIME_WAIT) {
                dstCon->state = TCP_CLOSED;
            }
            else if(dstCon->state == TCP_LAST_ACK) {
                dstCon->state = TCP_CLOSED;
            }
            else if(dstCon->state == TCP_SYN_RECV) {
                dstCon->state = TCP_ESTABLISHED;
            }
        }
        else {
            DARWIN_LOG_INFO("tcp_sessions::tcp session flags unhandled");
        }

        srcCon->lastAck = queue->ack;

        if(srcCon->state >= TCP_CLOSING && dstCon->state >= TCP_CLOSING) {
            return 1;
        }

        return 0;
    }
    return -1;
}

static inline void tcpConnectionInsertToQueue(TcpConnection *connection, TcpQueue *newElem) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpConnectionInsertToQueue");

    if(connection && newElem) {
        TcpQueue *scan;
        for(scan = connection->queueHead; scan != NULL && newElem->seq < scan->seq; scan = scan->prev) {}
        if(scan) {
            newElem->next = scan->next;
            newElem->prev = scan;
            if(scan->next) scan->next->prev = newElem;
            else connection->queueHead = newElem;
            scan->next = newElem;
        }
        else {
            if(connection->queueTail) {
                connection->queueTail->prev = newElem;
                newElem->next = connection->queueTail;
            }
            connection->queueTail = newElem;
        }
        if(!connection->queueHead) connection->queueHead = newElem;
        connection->queueSize++;
        DARWIN_LOG_DEBUG("tcp_sessions::tcpConnectionInsertToQueue:: new queue size is " + std::to_string(connection->queueSize));

    }
    else {
        DARWIN_LOG_ERROR("tcp_sessions::connection or newElem are NULL");
    }

    return;
}

static inline TcpQueue *tcpConnectionGetNextInQueue(TcpConnection *connection) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpConnectionGetNextInQueue");

    if(connection->state <= TCP_LISTEN) return connection->queueTail;

    TcpQueue *scan = connection->queueHead;
    while(scan != NULL) {
        if(connection->nextSeq == scan->seq && !scan->used) return scan;
        if(scan->seq < connection->nextSeq) return NULL;
        scan = scan->prev;
    }

    return NULL;
}

static inline int tcpQueueRemoveFromConnection(TcpConnection *connection, TcpQueue *queue) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::tcpQueueRemoveFromConnection");

    if(queue) {
        if(queue->prev) queue->prev->next = queue->next;
        if(queue->next) queue->next->prev = queue->prev;

        if(connection->queueHead == queue) connection->queueHead = queue->prev;
        if(connection->queueTail == queue) connection->queueTail = queue->next;
        connection->queueSize--;
        return 0;
    }
    return 1;
}

TcpConnection *getTcpSrcConnectionFromPacket(TcpSession *session, Packet *pkt) {
    if(session && pkt) {
        if(session->cCon->sPort == pkt->sp) return session->cCon;
        else if(session->sCon->sPort == pkt->sp) return session->sCon;
    }
    return NULL;
}

TcpConnection *getTcpDstConnectionFromPacket(TcpSession *session, Packet *pkt) {
    if(session && pkt) {
        if(session->cCon->sPort == pkt->dp) return session->cCon;
        else if(session->sCon->sPort == pkt->dp) return session->sCon;
    }
    return NULL;
}

int handleTcpFromPacket(Packet *pkt) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("tcp_sessions::handleTcpFromPacket");

    int ret = 0;

    if(pkt) {
        if(pkt->proto == IPPROTO_TCP) {
            TcpSession *session = (TcpSession *)pkt->flow->protoCtx;

            if(!session) {
                DataObject *sessionObject = getOrCreateAvailableObject(sessPool);
                if(!sessionObject) {
                    DARWIN_LOG_WARNING("tcp_sessions::could not get new TcpSession, dropping session handling");
                    return -1;
                }
                session = (TcpSession *)sessionObject->pObject;

                tcpSessionInitFromPacket(session, pkt);
                TcpQueue *tcpQueue = packetEnqueue(pkt);
                if(!tcpQueue) {
                    DARWIN_LOG_WARNING("tcp_sessions::couldn't enqueue packet, dropping session handling");
                    return -1;
                }
                tcpConnectionInsertToQueue(session->cCon, tcpQueue);
                tcpQueue->used = 1;
                pkt->flow->protoCtx = (void *)session;
            }
            else
            {
                TcpConnection *srcCon, *dstCon;
                TcpQueue *tcpQueue = packetEnqueue(pkt);
                if(!tcpQueue) {
                    DARWIN_LOG_WARNING("tcp_sessions::couldn't enqueue packet, dropping session handling");
                    return -1;
                }
                srcCon = getTcpSrcConnectionFromPacket(session, pkt);
                dstCon = getTcpDstConnectionFromPacket(session, pkt);

                tcpConnectionInsertToQueue(srcCon, tcpQueue);
                tcpQueue = tcpConnectionGetNextInQueue(srcCon);
                if(tcpQueue) {
                    do {
                        ret = tcpConnectionsUpdateFromQueueElem(srcCon, dstCon, tcpQueue);
                        tcpQueueRemoveFromConnection(srcCon, tcpQueue);

                        setObjectAvailable(tcpQueue->object);

                        if(ret) return 1;

                        tcpQueue = tcpConnectionGetNextInQueue(srcCon);
                    }while(tcpQueue);
                }

                if(streamsCnf->streamStoreFolder &&
                   srcCon->state >= TCP_ESTABLISHED &&
                   dstCon->state >= TCP_ESTABLISHED &&
                   (!srcCon->streamBuffer->bufferDump || !dstCon->streamBuffer->bufferDump)) {
                    char fileNameClient[100], fileNameServer[100];

                    snprintf(fileNameClient,
                             100, "tcp[%s->%s](%d->%d).dmp",
                             getAddrString(session->flow->src),
                             getAddrString(session->flow->dst),
                             session->flow->sp, session->flow->dp);
                    snprintf(fileNameServer,
                             100, "tcp[%s->%s](%d->%d).dmp",
                             getAddrString(session->flow->dst),
                             getAddrString(session->flow->src),
                             session->flow->dp, session->flow->sp);

                    if(linkStreamBufferToDumpFile(session->cCon->streamBuffer, fileNameClient) != 0) {
                        DARWIN_LOG_WARNING("tcp_sessions::could not link file to stream");
                    }
                    if(linkStreamBufferToDumpFile(session->sCon->streamBuffer, fileNameServer) != 0) {
                        DARWIN_LOG_WARNING("tcp_sessions::could not link file to stream");
                    }
                }
            }

            return 0;
        }
    }
    return -1;
}

/* tcp_sessions.h
 *
 * This header contains the definition of TCP sessions structures
 * as well as prototypes for tcp_sessions.c
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

#ifndef TCP_SESSIONS_H
#define TCP_SESSIONS_H

#include "packets.hpp"
#include "stream_buffer.hpp"
#include "flow.hpp"
#include "file_utils.hpp"
#include "data_pool.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define MAX_TCP_SESSIONS 512
#define TCP_PROTO 6

#define HAS_TCP_FLAG(flags, flag) ((strchr(flags, flag) == NULL) ? 0 : 1)

extern DataPool *queuePool;
extern DataPool *connPool;
extern DataPool *sessPool;

typedef struct TcpQueue_ {
    char tcp_flags[10];
    uint32_t seq;
    uint32_t ack;
    uint32_t dataLength;
#define DEFAULT_QUEUE_BUFFER_SIZE   1500
    uint8_t *data;
    uint32_t dataBufferSize;
    uint8_t used;
    struct TcpQueue_ *prev;
    struct TcpQueue_ *next;

    DataObject *object;
} TcpQueue;

enum tcpState {
    TCP_NONE,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_LAST_ACK,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_CLOSED,
};

typedef struct TcpConnection_ {
    Port sPort;
    enum tcpState state;
    uint32_t initSeq;
    uint32_t nextSeq;
    uint32_t lastAck;
    StreamBuffer *streamBuffer;
    TcpQueue *queueHead;
    TcpQueue *queueTail;
    uint32_t queueSize;

    DataObject *object;
} TcpConnection;

typedef struct TcpSession_ {
    TcpConnection *cCon;
    TcpConnection *sCon;
    Flow *flow;

    DataObject *object;
} TcpSession;

int initTCPPools();
void destroyTCPPools();
int tcpSessionInitFromPacket(TcpSession *, Packet *);
int tcpConnectionsUpdateFromQueueElem(TcpConnection *, TcpConnection *, TcpQueue *);
int handleTcpFromPacket(Packet *);

#ifdef __cplusplus
};
#endif

#endif /* TCP_SESSIONS_H */

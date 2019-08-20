/* flow.h
 *
 * This file contains structures and prototypes of functions used
 * for flow handling.
 *
 * File begun on 2019-05-15
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

#ifndef FLOW_H
#define FLOW_H

#include "packet-utils.hpp"
#include "packets.hpp"
#include "rand_utils.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <time.h>

#define TO_SERVER 0
#define TO_CLIENT 1

typedef struct FlowList_ {
    uint32_t listSize;
    struct Flow_ *head;
    struct Flow_ *tail;
    pthread_mutex_t mFlowList;
} FlowList;

typedef struct FlowCnf_ {
    uint32_t hash_rand;
    uint32_t hash_size;
#define FLOW_DEFAULT_HASHSIZE   65536

    uint32_t maxFlow;
#define FLOW_DEFAULT_MAXCONN    8192

    FlowList **flowHashLists;
    FlowList *flowList;

    pthread_mutex_t mConf;
} FlowCnf;

extern FlowCnf *globalFlowCnf;

/* Hash key for the flow hash */
typedef struct FlowHashKey4_
{
    union {
        struct {
            uint32_t addrs[2];
            uint16_t ports[2];
            uint32_t proto;
        };
        const uint32_t u32[4];
    };
} FlowHashKey4;

typedef struct FlowHashKey6_
{
    union {
        struct {
            uint32_t addrs[8];
            uint16_t ports[2];
            uint32_t proto;
        };
        const uint32_t u32[4];
    };
} FlowHashKey6;

typedef struct Flow_ {
    Address src, dst;
    uint16_t sp, dp;

    uint8_t proto;

    uint32_t flowHash;

    void *protoCtx;

    uint32_t toDstPktCnt;
    uint32_t toSrcPktCnt;
    uint64_t toDstByteCnt;
    uint64_t toSrcByteCnt;

    struct Flow_ *prevFlow;
    struct Flow_ *nextFlow;

    time_t initPacketTime;
    time_t lastPacketTime;

    pthread_mutex_t mFlow;
} Flow;

#define CMP_FLOW(f1,f2) \
    (((CMP_ADDR(&(f1)->src, &(f2)->src) && \
       CMP_ADDR(&(f1)->dst, &(f2)->dst) && \
       CMP_PORT((f1)->sp, (f2)->sp) && CMP_PORT((f1)->dp, (f2)->dp)) || \
      (CMP_ADDR(&(f1)->src, &(f2)->dst) && \
       CMP_ADDR(&(f1)->dst, &(f2)->src) && \
       CMP_PORT((f1)->sp, (f2)->dp) && CMP_PORT((f1)->dp, (f2)->sp))) && \
     (f1)->proto == (f2)->proto)

void deleteFlow(Flow *);
void flowInitConfig(FlowCnf *);
void flowDeleteConfig(FlowCnf *);
Flow *createNewFlowFromPacket(struct Packet_ *);
Flow *getOrCreateFlowFromHash(struct Packet_ *);
void swapFlowDirection(Flow *);
int getFlowDirectionFromAddrs(Flow *, Address *, Address *);
int getFlowDirectionFromPorts(Flow *, const Port, const Port);
int getPacketFlowDirection(Flow *, struct Packet_ *);

#ifdef __cplusplus
};
#endif

#endif /* FLOW_H */

/* flow.c
 *
 * This file contains functions used for flow handling.
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

#include "flow.hpp"
#include "Logger.hpp"

FlowCnf *globalFlowCnf;

static inline Flow *createNewFlow() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("createNewFlow");

    Flow *flow = (Flow *)calloc(1, sizeof(Flow));

    if(flow) {
        CLEAR_ADDR(&flow->src);
        CLEAR_ADDR(&flow->dst);

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if(pthread_mutex_init(&flow->mFlow, &attr) != 0) {
            DARWIN_LOG_ERROR("could not init flow mutex");
        }
        pthread_mutexattr_destroy(&attr);
    }
    else {
        DARWIN_LOG_ERROR("could not claim memory for new Flow object");
    }

    return flow;
}

void deleteFlow(Flow *flow) {
    if(flow) {
        pthread_mutex_destroy(&(flow->mFlow));
        free(flow);
    }

    return;
}

static inline FlowList *initNewFlowList() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("initNewFlowList");

    FlowList *flowList = NULL;

    flowList = (FlowList *)calloc(1, sizeof(FlowList));
    if(flowList) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if(pthread_mutex_init(&(flowList->mFlowList), &attr) == 0) {
            pthread_mutexattr_destroy(&attr);
            return flowList;
        }
        pthread_mutexattr_destroy(&attr);
    }

    return NULL;
}

static inline void deleteFlowListElems(FlowList *flowList) {
    if(flowList) {
        pthread_mutex_lock(&(flowList->mFlowList));
        Flow *flowDelete, *flow = flowList->head;
        while(flow) {
            flowDelete = flow;
            flow = flow->nextFlow;
            deleteFlow(flowDelete);
        }

        flowList->listSize = 0;
        pthread_mutex_unlock(&(flowList->mFlowList));
    }

    return;
}

static inline void deleteFlowList(FlowList *flowList) {
    if(flowList) {
        pthread_mutex_destroy(&(flowList->mFlowList));
        free(flowList);
    }
    return ;
}

static inline int addFlowToList(Flow *flow, FlowList *flowList) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("addFlowToList");

    if(flow && flowList) {
        pthread_mutex_lock(&flowList->mFlowList);
        if(flowList->tail) {
            flowList->tail->nextFlow = flow;
            flowList->tail = flow;

            flow->prevFlow = flowList->tail;
            flow->nextFlow = NULL;
        }
        else {
            flowList->tail = flow;
            flowList->head = flow;
        }
        flowList->listSize++;
        pthread_mutex_unlock(&flowList->mFlowList);
        return 1;
    }
    else {
        return 0;
    }
}

static inline int removeFlowFromList(Flow *flow, FlowList *flowList) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("removeFlowFromList");

    if(flow && flowList) {
        pthread_mutex_lock(&flowList->mFlowList);

        if(flowList->head) {
            Flow *flowSearch = flowList->head;
            uint8_t found = 0;

            do {
                found = (flow == flowSearch) ? 1 : 0;
            }while(flowSearch->nextFlow && !found);

            if(found) {
                if (flowSearch == flowList->head) {
                    flowList->head = flowList->head->nextFlow;
                }
                if (flowSearch == flowList->tail) {
                    flowList->tail = flowList->tail->prevFlow;
                }
                flowList->listSize--;

                if (flowSearch->nextFlow) {
                    flowSearch->nextFlow = flowSearch->prevFlow;
                }

                if (flowSearch->prevFlow) {
                    flowSearch->prevFlow = flowSearch->nextFlow;
                }

                flowSearch->nextFlow = NULL;
                flowSearch->prevFlow = NULL;

                pthread_mutex_unlock(&flowList->mFlowList);
                return 1;
            }
        }
        pthread_mutex_unlock(&flowList->mFlowList);
    }

    return 0;
}

void flowInitConfig(FlowCnf *conf) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("init flow config");

    conf->hash_rand = (uint32_t) getRandom();
    conf->hash_size = FLOW_DEFAULT_HASHSIZE;
    conf->maxFlow = FLOW_DEFAULT_MAXCONN;

    pthread_mutex_init(&(conf->mConf), NULL);

    DARWIN_LOG_DEBUG("global flow conf hash_rand: " + std::to_string(conf->hash_rand));
    DARWIN_LOG_DEBUG("global flow conf hash_size: " + std::to_string(conf->hash_size));
    DARWIN_LOG_DEBUG("global flow conf maxFlow: " + std::to_string(conf->maxFlow));

    conf->flowHashLists = (FlowList **)calloc(conf->hash_size, sizeof(FlowList *));
    conf->flowList = initNewFlowList();
    if(!conf->flowList) {
        DARWIN_LOG_ERROR("could not create new flowList for global flow configuration");
        return;
    }

    globalFlowCnf = conf;
    return;
}

void flowDeleteConfig(FlowCnf *conf) {
    if(conf) {
        if(conf->flowList)  {
            deleteFlowListElems(conf->flowList);
            deleteFlowList(conf->flowList);
        }
        uint32_t i;
        for(i = 0; i < conf->hash_size; i++) {
            deleteFlowList(conf->flowHashLists[i]);
        }
        free(conf->flowHashLists);

        pthread_mutex_destroy(&(conf->mConf));

        free(conf);
    }
    return;
}

Flow *createNewFlowFromPacket(struct Packet_ *packet) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("createNewFlowFromPacket");

    Flow *flow = createNewFlow();

    if(flow && packet) {
        COPY_ADDR(&(packet->src), &(flow->src));
        COPY_ADDR(&(packet->dst), &(flow->dst));

        flow->sp = packet->sp;
        flow->dp = packet->dp;
        flow->proto = packet->proto;
        if(!packet->hash) {
            packet->hash = calculatePacketFlowHash(packet);
        }
        flow->flowHash = packet->hash;
        flow->initPacketTime = packet->enterTime;
        flow->lastPacketTime = packet->enterTime;
        flow->toDstPktCnt = 1;
        flow->toDstByteCnt = packet->payloadLen;
        flow->toSrcPktCnt = 0;
        flow->toDstByteCnt = 0;
        packet->flow = flow;
    }

    return flow;
}

Flow *getOrCreateFlowFromHash(struct Packet_ *packet) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("getOrCreateFlowFromHash");
    FlowList *flowList = NULL;
    FlowHash hash = packet->hash;
    Flow *flow;

    pthread_mutex_lock(&(globalFlowCnf->mConf));
    flowList = globalFlowCnf->flowHashLists[hash % globalFlowCnf->hash_size];

    if(flowList == NULL) {
        flowList = initNewFlowList();
        globalFlowCnf->flowHashLists[hash % globalFlowCnf->hash_size] = flowList;
    }
    pthread_mutex_unlock(&(globalFlowCnf->mConf));

    pthread_mutex_lock(&(flowList->mFlowList));
    flow = flowList->head;
    uint8_t found = 0;
    while(flow && !found) {
        if(CMP_FLOW(flow, packet))  found = 1;
        else flow = flow->nextFlow;
    }


    if(!flow) {
        pthread_mutex_lock(&(globalFlowCnf->flowList->mFlowList));
        if(globalFlowCnf->flowList->listSize < globalFlowCnf->maxFlow) {
            DARWIN_LOG_DEBUG("creating new flow and adding it to lists");
            flow = createNewFlowFromPacket(packet);
            addFlowToList(flow, flowList);
            addFlowToList(flow, globalFlowCnf->flowList);
        }
        else {
            DARWIN_LOG_DEBUG("max number of flows reached, cannot open new Flow");
        }
        pthread_mutex_unlock(&(globalFlowCnf->flowList->mFlowList));
        pthread_mutex_unlock(&(flowList->mFlowList));
    }
    else {
        pthread_mutex_unlock(&(flowList->mFlowList));
        DARWIN_LOG_DEBUG("found existing flow");
        if(getPacketFlowDirection(flow, packet) == TO_SERVER) {
            flow->toDstByteCnt++;
            flow->toDstByteCnt += packet->payloadLen;
        }
        else {
            flow->toSrcPktCnt++;
            flow->toSrcByteCnt += packet->payloadLen;
        }
        flow->lastPacketTime = packet->enterTime;
    }

    return flow;
}

void swapFlowDirection(Flow *flow) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("swapFlowDirection");

    pthread_mutex_lock(&(flow->mFlow));
    uint16_t portTemp = flow->sp;
    flow->sp = flow->dp;
    flow->dp = portTemp;

    Address addrTemp;
    COPY_ADDR(&flow->src, &addrTemp);
    COPY_ADDR(&flow->dst, &flow->src);
    COPY_ADDR(&addrTemp, &flow->dst);
    pthread_mutex_unlock(&(flow->mFlow));

    return;
}

int getFlowDirectionFromAddrs(Flow *flow, Address *src, Address *dst) {
    if(!CMP_ADDR(src, dst)) {
        if(CMP_ADDR(&flow->src, src)) {
            return TO_SERVER;
        }
        else {
            return TO_CLIENT;
        }
    }
    else {
        return -1;
    }
}

int getFlowDirectionFromPorts(Flow *flow, const Port sp, const Port dp) {
    if(!CMP_PORT(sp, dp)) {
        if(CMP_PORT(sp, flow->sp)) {
            return TO_SERVER;
        }
        else {
            return TO_CLIENT;
        }
    }
    else {
        return -1;
    }
}

/**
 * WARNING: will return default TO_SERVER when protocol is not handled
 * @param flow
 * @param pkt
 * @return
 */
int getPacketFlowDirection(Flow *flow, struct Packet_ *pkt) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("getPacketFlowDirection");
    int ret = 0;

    pthread_mutex_lock(&(flow->mFlow));
    if(pkt->proto == IPPROTO_TCP || pkt->proto == IPPROTO_UDP) {
        ret = getFlowDirectionFromPorts(flow, pkt->sp, pkt->dp);
        if(ret == -1) {
            ret = getFlowDirectionFromAddrs(flow, &pkt->src, &pkt->dst);
        }
    }
    else if(pkt->proto == IPPROTO_ICMP || pkt->proto == IPPROTO_ICMPV6) {
        ret = getFlowDirectionFromAddrs(flow, &pkt->src, &pkt->dst);
    }
    pthread_mutex_unlock(&(flow->mFlow));

    return ret;
}

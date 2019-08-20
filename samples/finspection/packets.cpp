/* packets.c
 *
 * This file contains functions to parse metadata from Rsyslog packets
 * to internal 'packet' structures, defined in packets.h
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

#include "packets.hpp"
#include "Logger.hpp"

Packet *createPacket() {
    Packet *pkt = (Packet *)calloc(1, sizeof(Packet));

    return pkt;
}

void freePacket(Packet *pkt) {
    if(pkt) {
        if(pkt->ipv4h)      free(pkt->ipv4h);
        if(pkt->ipv6h)      free(pkt->ipv6h);
        if(pkt->tcph)       free(pkt->tcph);
        if(pkt->payload)    free(pkt->payload);
        free(pkt);
    }
}

void updatePacketFromHeaders(Packet *pkt) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("updatePacketFromHeaders");
    if(pkt) {
        if(pkt->ipv4h) {
            if(inet_pton(AF_INET, pkt->ipv4h->src, &(pkt->src.address)) != 1) {
                DARWIN_LOG_WARNING("copy of src ipv4 address failed");
            }
            if(inet_pton(AF_INET, pkt->ipv4h->dst, &(pkt->dst.address)) != 1) {
                DARWIN_LOG_WARNING("copy of dst ipv4 address failed");
            }
            pkt->src.family = AF_INET;
            pkt->dst.family = AF_INET;
            pkt->proto = pkt->ipv4h->proto;
            pkt->flags |= PKT_ADDRS_KNOWN;
            pkt->flags |= PKT_PROTO_KNOWN;
            pkt->flags |= PKT_IPV4_ADDR;
        }
        else if(pkt->ipv6h) {
            if(inet_pton(AF_INET6, pkt->ipv6h->src, &(pkt->src.addr_in6addr)) != 1) {
                DARWIN_LOG_WARNING("copy of src ipv6 address failed");
            }
            if(inet_pton(AF_INET6, pkt->ipv6h->dst, &(pkt->dst.addr_in6addr)) != 1) {
                DARWIN_LOG_WARNING("copy of dst ipv6 address failed");
            }
            pkt->src.family = AF_INET6;
            pkt->dst.family = AF_INET6;
            pkt->proto = pkt->ipv6h->proto;
            pkt->flags |= PKT_ADDRS_KNOWN;
            if(pkt->proto) {
                /* proto '0' is a valid Hop-By-Hop IPv6 header,
                 * but should never be the value contained in impcap metadata
                 * as it should be further handled by IPv6 parser */
                pkt->flags |= PKT_PROTO_KNOWN;
            }
            pkt->flags |= PKT_IPV6_ADDR;
        }

        if(pkt->tcph) {
            pkt->sp = (Port) pkt->tcph->sport;
            pkt->dp = (Port) pkt->tcph->dport;
            pkt->flags |= PKT_PORTS_KNOWN;
        }

        if(pkt->flags & PKT_PROTO_KNOWN) {
            switch(pkt->proto) {
                case IPPROTO_TCP:
                case IPPROTO_UDP:
                    if(pkt->flags & PKT_ADDRS_KNOWN && pkt->flags & PKT_PORTS_KNOWN) pkt->flags |= PKT_HASH_READY;
                    break;
                default:
                    if(pkt->flags & PKT_ADDRS_KNOWN) pkt->flags |= PKT_HASH_READY;
            }
        }
    }
}

/**
 * simple inline function to compare 2 IPV6 addresses, used to calculate hashes
 * (shouldn't be used elsewhere)
 * @param a
 * @param b
 * @return
 */
static inline int ip6AddressCompare(const uint32_t *a, const uint32_t *b)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (a[i] > b[i])
            return 1;
        if (a[i] < b[i])
            break;
    }

    return 0;
}

FlowHash calculatePacketFlowHash(Packet *pkt) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("calculatePacketFlowHash");
    uint32_t hash = 0;

    if(pkt) {
        if(pkt->flags & PKT_HASH_READY) {
            pthread_mutex_lock(&(globalFlowCnf->mConf));
            uint32_t hash_rand = (uint32_t)globalFlowCnf->hash_rand;
            pthread_mutex_unlock(&(globalFlowCnf->mConf));

            if(pkt->flags & PKT_IPV4_ADDR) {
                FlowHashKey4 *fk = (FlowHashKey4 *)malloc(sizeof(FlowHashKey4));

                int ai = (pkt->src.addr_data32[0] > pkt->dst.addr_data32[0]);
                fk->addrs[1-ai] = pkt->src.addr_data32[0];
                fk->addrs[ai] = pkt->dst.addr_data32[0];

                const int pi = (pkt->sp > pkt->dp);
                fk->ports[1-pi] = pkt->sp;
                fk->ports[pi] = pkt->dp;

                fk->proto = (uint32_t) pkt->proto;

                hash = hashword(fk->u32, 4, hash_rand);

                free(fk);
            }
            else if(pkt->flags & PKT_IPV6_ADDR) {
                FlowHashKey6 *fk = (FlowHashKey6 *)malloc(sizeof(FlowHashKey6));

                if(ip6AddressCompare(pkt->src.addr_data32, pkt->dst.addr_data32)) {
                    fk->addrs[0] = pkt->src.addr_data32[0];
                    fk->addrs[1] = pkt->src.addr_data32[1];
                    fk->addrs[2] = pkt->src.addr_data32[2];
                    fk->addrs[3] = pkt->src.addr_data32[3];
                    fk->addrs[4] = pkt->dst.addr_data32[0];
                    fk->addrs[5] = pkt->dst.addr_data32[1];
                    fk->addrs[6] = pkt->dst.addr_data32[2];
                    fk->addrs[7] = pkt->dst.addr_data32[3];
                }
                else
                {
                    fk->addrs[0] = pkt->dst.addr_data32[0];
                    fk->addrs[1] = pkt->dst.addr_data32[1];
                    fk->addrs[2] = pkt->dst.addr_data32[2];
                    fk->addrs[3] = pkt->dst.addr_data32[3];
                    fk->addrs[4] = pkt->src.addr_data32[0];
                    fk->addrs[5] = pkt->src.addr_data32[1];
                    fk->addrs[6] = pkt->src.addr_data32[2];
                    fk->addrs[7] = pkt->src.addr_data32[3];
                }

                const int pi = (pkt->sp > pkt->dp);
                fk->ports[1-pi] = pkt->sp;
                fk->ports[pi] = pkt->dp;

                fk->proto = (uint32_t) pkt->proto;

                hash = hashword(fk->u32, 10, hash_rand);

                free(fk);
            }
        }
        else {
            DARWIN_LOG_WARNING("packet is not complete, cannot compute hash");
        }
    }

    return hash;
}
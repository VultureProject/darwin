/* packets.h
 *
 * This header contains the definition of internal structures
 * representing packets metadata and payload, as well as prototypes
 * for packets.c
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

#ifndef PACKETS_H
#define PACKETS_H

#include "extract_impcap.hpp"
#include "flow.hpp"
#include "hash_utils.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

typedef struct Packet_ {
    Address src, dst;
    Port sp, dp;
    uint8_t proto;

    struct Flow_ *flow;
    FlowHash hash;

    uint8_t flags;
#define PKT_ADDRS_KNOWN 0x01
#define PKT_PORTS_KNOWN 0x02
#define PKT_PROTO_KNOWN 0x04
#define PKT_HASH_READY  0x08
#define PKT_IPV4_ADDR   0x10
#define PKT_IPV6_ADDR   0x20

    struct IPV6Hdr_ *ipv6h;
    struct IPV4Hdr_ *ipv4h;
    struct TCPHdr_ *tcph;

    uint8_t *payload;
    uint16_t payloadLen;

    uint32_t pktNumber;

    time_t enterTime;
} Packet;

Packet *createPacket();
void freePacket(Packet *);
void updatePacketFromHeaders(Packet *);
FlowHash calculatePacketFlowHash(Packet *);

#ifdef __cplusplus
};
#endif

#endif /* PACKETS_H */

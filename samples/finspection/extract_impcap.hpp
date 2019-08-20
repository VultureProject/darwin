/* extract_impcap.h
 *
 * This header contains the definition of structures and functions
 * to get Impcap data
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
#ifndef EXTRACT_IMPCAP_H
#define EXTRACT_IMPCAP_H

#define IMPCAP_METADATA "!impcap"
#define IMPCAP_DATA     "!data"

#include <stdint.h>
#include <arpa/inet.h>
#include "packets.hpp"

#include "../toolkit/rapidjson/document.h"

#define HTTP_PORT 80
#define FTP_PORT 21
#define FTP_PORT_DATA 20

typedef struct TCPHdr_ {
    uint16_t sport;
    uint16_t dport;
    uint32_t seq;
    uint32_t ack;
    uint32_t TCPDataLength;
    const char *flags;
} TCPHdr;

#define ETHERTYPE_IPV4  0x0800
#define ETHERTYPE_IPV6  0X86DD

typedef struct IPV4Hdr_ {
    const char *src;
    const char *dst;
    uint8_t hLen;
    uint8_t ttl;
    uint8_t proto;
} IPV4Hdr;

typedef struct IPV6Hdr_ {
    const char *src;
    const char *dst;
    uint8_t ttl;
    uint8_t proto;
} IPV6Hdr;

struct Packet_ *getImpcapData(std::string, std::string);
uint8_t *ImpcapDataDecode(const char *, uint32_t);
TCPHdr *getTcpHeader(rapidjson::Document&);
IPV4Hdr *getIpv4Header(rapidjson::Document&);
IPV6Hdr *getIpv6Header(rapidjson::Document&);

#endif /* EXTRACT_IMPCAP_H */

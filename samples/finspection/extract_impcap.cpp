/* extract_impcap.c
 *
 * This file contains functions to get fields given by Impcap
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

#include "extract_impcap.hpp"
#include "Logger.hpp"
#include "../toolkit/rapidjson/document.h"

Packet *getImpcapData(std::string impcapMeta, std::string impcapData) {
    DARWIN_LOGGER;
    int localret;
    uint32_t contentLength;
    const char *content;
    uint16_t ethType;
    Packet *pkt = NULL;
    rapidjson::Document docMeta, docData;

    if(!impcapMeta.empty()) {
        pkt = createPacket();
        docMeta.Parse(impcapMeta.c_str());

        if(docMeta.HasMember("ID") && docMeta["ID"].IsUint()) {
            pkt->pktNumber = docMeta["ID"].GetUint();
        }

        if(docMeta.HasMember("ETH_type") && docMeta["ETH_type"].IsUint()) {
            ethType = docMeta["ETH_type"].GetUint();
            if(ethType == ETHERTYPE_IPV4) {
                pkt->ipv4h = getIpv4Header(docMeta);
                pkt->proto = pkt->ipv4h->proto;
            }
            else if(ethType == ETHERTYPE_IPV6) {
                pkt->ipv6h = getIpv6Header(docMeta);
                pkt->proto = pkt->ipv6h->proto;
            }

            if(pkt->proto == IPPROTO_TCP) {
                pkt->tcph = getTcpHeader(docMeta);
            }
        }
        updatePacketFromHeaders(pkt);
    }

    if(!impcapData.empty() && pkt) {
        docData.Parse(impcapData.c_str());

        if(docData.HasMember("length") && docData["length"].IsUint()) {
            contentLength = docData["length"].GetUint();

            if(docData.HasMember("content") && docData["content"].IsString()) {
                content = docData["content"].GetString();
                pkt->payload = ImpcapDataDecode(content, contentLength);
                pkt->payloadLen = contentLength/2;
            }
        }
    }


    return pkt;
}

uint8_t *ImpcapDataDecode(const char *hex, uint32_t length) {
    uint8_t *retBuf = (uint8_t *)malloc(length/2*sizeof(uint8_t));
    int i;

    for(i = 0; i < length; ++i) {
        if(i%2) {
            retBuf[i/2] <<= 4;
            if(hex[i] >= '0' && hex[i] <= '9') {
                retBuf[i/2] += hex[i] - '0';
            }
            else if(hex[i] >= 'A' && hex[i] <= 'F') {
                retBuf[i/2] += hex[i] - 'A' + 10;
            }
        }
        else {
            if(hex[i] >= '0' && hex[i] <= '9') {
                retBuf[i/2] = hex[i] - '0';
            }
            else if(hex[i] >= 'A' && hex[i] <= 'F') {
                retBuf[i/2] = hex[i] - 'A' + 10;
            }
        }
    }

    return retBuf;
}

TCPHdr *getTcpHeader(rapidjson::Document& doc) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("getting tcp header");
    TCPHdr *tcph = (TCPHdr *)calloc(1, sizeof(TCPHdr));

    if (doc.HasMember("net_src_port") && doc["net_src_port"].IsUint()) {
        tcph->sport = doc["net_src_port"].GetUint();

    }

    if (doc.HasMember("net_dst_port") && doc["net_dst_port"].IsUint()) {
        tcph->dport = doc["net_dst_port"].GetUint();
    }

    if (doc.HasMember("TCP_seq_number") && doc["TCP_seq_number"].IsUint64()) {
        tcph->seq = doc["TCP_seq_number"].GetUint64();
    }

    if (doc.HasMember("TCP_ack_number") && doc["TCP_ack_number"].GetUint64()) {
        tcph->ack = doc["TCP_ack_number"].GetUint64();
    }

    if (doc.HasMember("net_flags") && doc["net_flags"].IsString()) {
        tcph->flags = strndup(doc["net_flags"].GetString(), 10);
    }

    if (doc.HasMember("net_bytes_data") && doc["net_bytes_data"].IsUint()) {
        tcph->TCPDataLength = doc["net_bytes_data"].GetUint();
    }

    DARWIN_LOG_DEBUG("finished getting tcp header");
    return tcph;
}

IPV6Hdr *getIpv6Header(rapidjson::Document& doc) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("getting IPV6 header");
    IPV6Hdr *ipv6h = (IPV6Hdr *)malloc(sizeof(IPV6Hdr));
    memset(ipv6h, 0, sizeof(IPV6Hdr));

    if (doc.HasMember("net_dst_ip") && doc["net_dst_ip"].IsString()) {
        ipv6h->dst = strndup(doc["net_dst_ip"].GetString(), 39);
    }

    if (doc.HasMember("net_src_ip") && doc["net_src_ip"].IsString()) {
        ipv6h->src = strndup(doc["net_src_ip"].GetString(), 39);

    }

    if (doc.HasMember("net_ttl") && doc["net_ttl"].IsUint()) {
        ipv6h->ttl = doc["net_ttl"].GetUint();
    }

    if (doc.HasMember("IP_proto") && doc["IP_proto"].IsUint()) {
        ipv6h->proto = doc["IP_proto"].GetUint();
    }

    DARWIN_LOG_DEBUG("finished getting IPV6 header");
    return ipv6h;
}

IPV4Hdr *getIpv4Header(rapidjson::Document& doc) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("getting IPV4 header");
    IPV4Hdr *ipv4h = (IPV4Hdr *)malloc(sizeof(IPV4Hdr));
    memset(ipv4h, 0, sizeof(IPV4Hdr));

    if (doc.HasMember("net_dst_ip") && doc["net_dst_ip"].IsString()) {
        ipv4h->dst = strndup(doc["net_dst_ip"].GetString(), 15);
    }

    if (doc.HasMember("net_src_ip") && doc["net_src_ip"].IsString()) {
        ipv4h->src = strndup(doc["net_src_ip"].GetString(), 15);
    }

    if (doc.HasMember("IP_ihl") && doc["IP_ihl"].IsUint()) {
        ipv4h->hLen = doc["IP_ihl"].GetUint();
    }

    if (doc.HasMember("net_ttl") && doc["net_ttl"].IsUint()) {
        ipv4h->ttl = doc["net_ttl"].GetUint();
    }

    if (doc.HasMember("IP_proto") && doc["IP_proto"].IsUint()) {
        ipv4h->proto = doc["IP_proto"].GetUint();
    }

    DARWIN_LOG_DEBUG("finished getting IPV4 header");
    return ipv4h;
}

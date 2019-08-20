/// \file     ContentInspection.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     10/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <thread>
#include <unistd.h>

#include "../../toolkit/lru_cache.hpp"
#include "../toolkit/rapidjson/document.h"
#include "Logger.hpp"
#include "ContentInspectionTask.hpp"
#include "protocol.h"

ContentInspectionTask::ContentInspectionTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               Configurations configurations)
        : Session{socket, manager, cache} {
    _is_cache = _cache != nullptr;
    _configurations = configurations;
}

long ContentInspectionTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_CONTENT_INSPECTION;
}

void ContentInspectionTask::operator()() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ContentInspectionTask:: started task");
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    int tcpStatus;

    for(Packet *pkt : _packetList) {
        unsigned int certitude = 0;

        pkt->enterTime = std::time(NULL);
        pkt->hash = calculatePacketFlowHash(pkt);

        if(pkt->flags & PKT_HASH_READY && _configurations.yaraCnf->scanType == SCAN_STREAM) {
            pkt->flow = getOrCreateFlowFromHash(pkt);

            if(pkt->flow && pkt->proto == IPPROTO_TCP) {
                pthread_mutex_lock(&(pkt->flow->mFlow));
                tcpStatus = handleTcpFromPacket(pkt);
                pthread_mutex_unlock(&(pkt->flow->mFlow));

            }
        }

        if(pkt->payloadLen) {
            rapidjson::Document yaraMeta;

            if(_configurations.yaraCnf->scanType == SCAN_STREAM &&
               pkt->flow && pkt->proto == IPPROTO_TCP && tcpStatus != -1) {
                StreamBuffer *sb;
                pthread_mutex_lock(&(pkt->flow->mFlow));
                TcpSession *session = (TcpSession *)pkt->flow->protoCtx;
                if(getPacketFlowDirection(pkt->flow, pkt) == TO_SERVER) {
                    sb = session->cCon->streamBuffer;
                }
                else {
                    sb = session->sCon->streamBuffer;
                }
                pthread_mutex_lock(&(sb->mutex));
                pthread_mutex_unlock(&(pkt->flow->mFlow));

                yaraMeta = yaraScan(pkt->payload, pkt->payloadLen, sb);
                pthread_mutex_unlock(&(sb->mutex));
            }
            else if(_configurations.yaraCnf->scanType == SCAN_PACKET_ONLY ||
                    tcpStatus == -1){
                yaraMeta = yaraScan(pkt->payload, pkt->payloadLen, NULL);
            }

            if(yaraMeta.IsArray()) {
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                yaraMeta.Accept(writer);

                certitude = 100;
                if (is_log && (certitude>=_threshold)){
                    _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + GetTime() +
                             R"(", "certitude": )" + std::to_string(certitude) + R"(, "yara_match": )" +
                             std::string(buffer.GetString()) +
                             "}\n";
                }
            }
        }

        _certitudes.push_back(certitude);
        freePacket(pkt);
    }

    DARWIN_LOG_DEBUG("ContentInspectionTask:: task finished");
    Workflow();

    _packetList = std::vector<Packet *>();
}

void ContentInspectionTask::Workflow() {
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    switch (header.response) {
        case DARWIN_RESPONSE_SEND_BOTH:
            SendToDarwin();
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_BACK:
            SendResToSession();
            if(is_log) SendToDarwin();
            break;
        case DARWIN_RESPONSE_SEND_DARWIN:
            SendToDarwin();
            break;
        case DARWIN_RESPONSE_SEND_NO:
            if(is_log) SendToDarwin();
        default:
            break;
    }
}

bool ContentInspectionTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ContentInspectionTask:: ParseBody: '" + body + "'");

    try {
        std::size_t packetMeta = 0, packetMetaEnd;
        std::size_t packetData, packetDataEnd;
        std::size_t openingBracket;

        do {
            packetMeta = body.find("\"{", packetMeta + 1);
            if(packetMeta == std::string::npos) break;

            packetMetaEnd = body.find("}\",", packetMeta);
            if(packetMetaEnd == std::string::npos) break;

            packetData = body.find("\"{", packetMetaEnd);
            if(packetData == std::string::npos) break;

            packetDataEnd = body.find("}\"", packetData);
            if(packetDataEnd == std::string::npos) break;

            _packetList.push_back(getImpcapData(
                    body.substr(packetMeta + 1, packetMetaEnd - packetMeta),
                    body.substr(packetData + 1, packetDataEnd - packetData)
            ));
        } while(true);

    } catch (...) {
        DARWIN_LOG_CRITICAL("ContentInspectionTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}

std::string ContentInspectionTask::GetTime(){
    char str_time[256];
    time_t rawtime;
    struct tm * timeinfo;
    std::string res;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(str_time, sizeof(str_time), "%F%Z%T%z", timeinfo);
    res = str_time;

    return res;
}
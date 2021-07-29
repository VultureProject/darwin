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
#include "ContentInspectionTask.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "ASession.hpp"
#include "AlertManager.hpp"

ContentInspectionTask::ContentInspectionTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               darwin::session_ptr_t s,
                               darwin::DarwinPacket& packet,
                               Configurations& configurations)
        : ATask(DARWIN_FILTER_NAME, cache, cache_mutex, s, packet) {
    _is_cache = _cache != nullptr;
    _configurations = configurations;
}

long ContentInspectionTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_CONTENT_INSPECTION;
}

void ContentInspectionTask::operator()() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ContentInspectionTask:: started task");
    bool is_log = _s->GetOutputType() == darwin::config::output_type::LOG;
    auto logs = _packet.GetMutableLogs();

    for(Packet *pkt : _packetList) {
        unsigned int certitude = 0;
        int tcpStatus = 0;
        SetStartingTime();

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
            YaraResults results;

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

                results = yaraScan(pkt->payload, pkt->payloadLen, sb);
                pthread_mutex_unlock(&(sb->mutex));
            }
            else if(_configurations.yaraCnf->scanType == SCAN_PACKET_ONLY ||
                    tcpStatus == -1){
                results = yaraScan(pkt->payload, pkt->payloadLen, NULL);
            }

            if(not results.rules.empty()) {

                std::string ruleListJson = ContentInspectionTask::GetJsonListFromSet(results.rules);
                std::string tagListJson = ContentInspectionTask::GetJsonListFromSet(results.tags);
                std::string details = "{\"rules\": " + ruleListJson + "}";

                certitude = 100;
                if (certitude >= _threshold and certitude < DARWIN_ERROR_RETURN){
                    STAT_MATCH_INC;
                    DARWIN_ALERT_MANAGER.SetTags(tagListJson);
                    DARWIN_ALERT_MANAGER.Alert("raw_data", certitude, _packet.Evt_idToString(), details);
                    if (is_log) {
                        std::string alert_log = R"({"evt_id": ")" + _packet.Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                                R"(", "filter": ")" + GetFilterName() + R"(", "certitude": )" + std::to_string(certitude) +
                                R"(, "rules": )" + ruleListJson + R"(, "tags": )" + tagListJson +
                                "}";
                        logs += alert_log + "\n";
                    }
                }
            }
        }

        _packet.AddCertitude(certitude);
        DARWIN_LOG_INFO("ContentInspectionTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
        freePacket(pkt);
    }

    DARWIN_LOG_DEBUG("ContentInspectionTask:: task finished");
}

bool ContentInspectionTask::ParseBody() {
    DARWIN_LOGGER;
    _packetList.clear();
    auto raw_body = _packet.GetBody();
    DARWIN_LOG_DEBUG("ContentInspectionTask:: ParseBody: _raw_body: " + raw_body);

    try {
        auto logs = _packet.GetMutableLogs();
        logs.clear();
        std::size_t packetMeta = 0, packetMetaEnd;
        std::size_t packetData, packetDataEnd = 0;

        do {
            packetMeta = raw_body.find("\"{", packetDataEnd + 1);
            if(packetMeta == std::string::npos) {
                break;
            }
            STAT_INPUT_INC;

            packetMetaEnd = raw_body.find("}\",", packetMeta);
            if(packetMetaEnd == std::string::npos) {
                DARWIN_LOG_WARNING("ContentInspectionTask:: parse fail 1");
                STAT_PARSE_ERROR_INC;
                break;
            }

            packetData = raw_body.find("\"{", packetMetaEnd);
            if(packetData == std::string::npos) {
                DARWIN_LOG_WARNING("ContentInspectionTask:: parse fail 2");
                STAT_PARSE_ERROR_INC;
                break;
            }

            packetDataEnd = raw_body.find("}\"", packetData);
            if(packetDataEnd == std::string::npos) {
                DARWIN_LOG_WARNING("ContentInspectionTask:: parse fail 3");
                STAT_PARSE_ERROR_INC;
                break;
            }

            _packetList.push_back(getImpcapData(
                    raw_body.substr(packetMeta + 1, packetMetaEnd - packetMeta),
                    raw_body.substr(packetData + 1, packetDataEnd - packetData)
            ));
        } while(true);

    } catch (...) {
        DARWIN_LOG_CRITICAL("ContentInspectionTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}


std::string ContentInspectionTask::GetJsonListFromSet(std::set<std::string> &input) {
    std::string result;
    // Reserve some space for each entry (10 chars) + 2 for beginning and end of list
    // This will be too much almost every time, but this doesn't matter as the string will be short-lived
    // and the input won't contain more than a dozen fields
    result.reserve(input.size() * 32 + 2);

    result = "[";
    bool first = true;
    for(auto rule : input) {
        if(not first)
            result += ",";
        result += "\"" + rule + "\"";
        first = false;
    }
    result += "]";

    return result;
}
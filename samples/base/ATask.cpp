#include <chrono>
#include <vector>
#include <ctime>

#include "Logger.hpp"
#include "Manager.hpp"
#include "ASession.hpp"
#include "ATask.hpp"
#include "errors.hpp"
#include "DarwinPacket.hpp"

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"

namespace darwin {

    ATask::ATask(std::string name, 
                std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                std::mutex& cache_mutex, session_ptr_t s,
                DarwinPacket& packet) 
            : _filter_name(name), _cache{cache}, _cache_mutex{cache_mutex}, _s{s},
            _packet{std::move(packet)}, _threshold{_s->GetThreshold()}
    {
        ;
    }

    void ATask::SetStartingTime() {
        _starting_time = std::chrono::high_resolution_clock::now();
    }

    double ATask::GetDurationMs() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - _starting_time
        ).count() / ((double)1000);
    }

    xxh::hash64_t ATask::GenerateHash() {
        // could be easily overridden in the children
        return xxh::xxhash<64>(_packet.GetBody());
    }

    void ATask::SaveToCache(const xxh::hash64_t &hash, const unsigned int certitude) const {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("SaveToCache:: Saving certitude " + std::to_string(certitude) + " to cache");
        std::unique_lock<std::mutex> lck{_cache_mutex};
        _cache->insert(hash, certitude);
    }

    bool ATask::GetCacheResult(const xxh::hash64_t &hash, unsigned int& certitude) {
        DARWIN_LOGGER;
        boost::optional<unsigned int> cached_certitude;

        {
            std::unique_lock<std::mutex> lck{_cache_mutex};
            cached_certitude = _cache->get(hash);
        }

        if (cached_certitude != boost::none) {
            certitude = cached_certitude.get();
            DARWIN_LOG_DEBUG("GetCacheResult:: Already processed request. Cached certitude is " +
                             std::to_string(certitude));

            return true;
        }

        return false;
    }

    std::string ATask::GetFilterName() {
        return _filter_name;
    }

    bool ATask::ParseBody() {
        DARWIN_LOGGER;
        try {
            _body.Parse(_packet.GetBody().c_str());

            if (!_body.IsArray()) {
                DARWIN_LOG_ERROR("ATask:: ParseBody: You must provide a list");
                return false;
            }
        } catch (...) {
            DARWIN_LOG_CRITICAL("ATask:: ParseBody: Could not parse raw body");
            return false;
        }
        // DARWIN_LOG_DEBUG("ATask:: ParseBody:: parsed body : " + _raw_body);
        return true;
    }

    void ATask::run() {
        (*this)();
        auto body = _packet.GetMutableBody();
        body.clear();
        body.append(_response_body);
        _s->SendNext(_packet);
    }

}
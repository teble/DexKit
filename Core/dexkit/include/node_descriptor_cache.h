#pragma once

#include "parallel_hashmap/phmap.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>

namespace dexkit {

// Nodes own immutable strings, including their SSO bodies. Rehash moves only
// node pointers. All table access is locked; returned character views may be
// read after unlocking and remain valid until quiescent DexItem destruction.
class NodeDescriptorCache {
    using Map = phmap::node_hash_map<uint32_t, std::string>;
    static constexpr size_t kShards = 32;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    struct Counters {
        uint64_t calls = 0, hits = 0, growths = 0;
        size_t largest_bucket_overlap = 0;
    };
    static size_t BucketBytes(const Map &map) {
        using Access = phmap::priv::hashtable_debug_internal::HashtableDebugAccess<Map>;
        // The bundled NodeHashMapPolicy reports sizeof(value_type) per node.
        return Access::AllocatedByteSize(map) - map.size() * sizeof(Map::value_type);
    }
#endif
    struct Shard {
        mutable std::mutex mutex;
        std::array<Map, 2> maps;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        std::array<Counters, 2> counters;
#endif
    };
    std::array<Shard, kShards> shards_;
    std::array<size_t, 2> limits_{};
    bool initialized_ = false;

public:
    NodeDescriptorCache() = default;
    NodeDescriptorCache(const NodeDescriptorCache &) = delete;
    NodeDescriptorCache &operator=(const NodeDescriptorCache &) = delete;

    void Initialize(size_t methods, size_t fields) {
        if (initialized_) std::abort();
        limits_ = {methods, fields};
        initialized_ = true;
        // Deliberately do not reserve from the number of possible member IDs.
    }

    // The factory reads only raw DEX metadata. It must not reenter this cache.
    template<bool Method, class Build>
    std::string_view GetOrCreate(uint32_t index, Build &&build) {
        constexpr size_t kind = Method ? 0 : 1;
        if (index >= limits_[kind]) std::abort();
        auto &shard = shards_[index % kShards];
        std::lock_guard lock(shard.mutex);
        auto &map = shard.maps[kind];
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        auto &counters = shard.counters[kind];
        ++counters.calls;
#endif
        if (auto found = map.find(index); found != map.end()) {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            ++counters.hits;
#endif
            return found->second;
        }
        auto descriptor = build();
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        const auto old_capacity = map.capacity(), old_buckets = BucketBytes(map);
#endif
        // Copy the lvalue, matching the dense optional cache's capacity policy.
        const auto [entry, inserted] = map.try_emplace(index, descriptor);
        if (!inserted) std::abort();
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        if (map.capacity() != old_capacity) {
            ++counters.growths;
            counters.largest_bucket_overlap = std::max(counters.largest_bucket_overlap,
                                                       old_buckets + BucketBytes(map));
        }
#endif
        return entry->second;
    }

#if DEXKIT_BENCHMARK_DIAGNOSTICS
    struct TableStatistics {
        size_t records = 0, capacity = 0, bucket_bytes = 0, node_bytes = 0;
        size_t char_bytes = 0, char_buffers = 0, sso_records = 0;
        uint64_t calls = 0, hits = 0, growths = 0;
        // Largest old+new bucket allocation for this table, not a process peak.
        size_t largest_bucket_overlap = 0;
    };
    struct Statistics {
        std::array<std::array<TableStatistics, 2>, kShards> tables;
        std::array<size_t, 2> limits;
        size_t fixed_bytes = 0, instrumentation_bytes = 0;
    };
    // Diagnostics run after queries drain; locks also make each table snapshot
    // safe on its own. The aggregate is not a concurrent point-in-time snapshot.
    Statistics GetStatistics() const {
        Statistics stats{};
        stats.limits = limits_;
        stats.instrumentation_bytes = kShards * sizeof(Shard::counters);
        stats.fixed_bytes = sizeof(*this) - stats.instrumentation_bytes;
        for (size_t i = 0; i < kShards; ++i) {
            const auto &shard = shards_[i];
            std::lock_guard lock(shard.mutex);
            for (size_t kind = 0; kind < 2; ++kind) {
                const auto &map = shard.maps[kind];
                const auto &counters = shard.counters[kind];
                auto &table = stats.tables[i][kind];
                table.records = map.size(); table.capacity = map.capacity();
                table.bucket_bytes = BucketBytes(map);
                table.node_bytes = map.size() * sizeof(Map::value_type);
                table.calls = counters.calls; table.hits = counters.hits;
                table.growths = counters.growths;
                table.largest_bucket_overlap = counters.largest_bucket_overlap;
                for (const auto &[index, value] : map) {
                    const auto object = reinterpret_cast<uintptr_t>(&value);
                    const auto data = reinterpret_cast<uintptr_t>(value.data());
                    if (data >= object && data < object + sizeof(value)) {
                        ++table.sso_records;
                    } else {
                        table.char_bytes += value.capacity() + 1;
                        ++table.char_buffers;
                    }
                }
            }
        }
        return stats;
    }
#endif
};

} // namespace dexkit

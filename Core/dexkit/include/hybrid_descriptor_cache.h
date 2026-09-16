#pragma once

#include "parallel_hashmap/phmap.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include <chrono>
#endif

namespace dexkit {

#if DEXKIT_BENCHMARK_DIAGNOSTICS
namespace descriptor_cache_detail {
struct Allocations {
    size_t bytes = 0, live = 0, allocations = 0, peak_bytes = 0;
};
struct PayloadAllocations {
    Allocations blocks, directory;
};

// Only diagnostic builds replace std::allocator. Rebinding lets the standard
// deque report its real string blocks and pointer directory, including slack.
template<class T>
struct CountingAllocator {
    using value_type = T;
    PayloadAllocations *counts = nullptr;
    CountingAllocator() = default;
    explicit CountingAllocator(PayloadAllocations *value) : counts(value) {}
    template<class U>
    CountingAllocator(const CountingAllocator<U> &other) : counts(other.counts) {}
    auto &Category() const {
        if (!counts) std::abort();
        if constexpr (std::is_same_v<T, std::string>) return counts->blocks;
        else return counts->directory;
    }
    T *allocate(size_t n) {
        auto *result = std::allocator<T>{}.allocate(n);
        auto &count = Category();
        count.bytes += n * sizeof(T); ++count.live; ++count.allocations;
        count.peak_bytes = std::max(count.peak_bytes, count.bytes);
        return result;
    }
    void deallocate(T *p, size_t n) {
        auto &count = Category();
        count.bytes -= n * sizeof(T); --count.live;
        std::allocator<T>{}.deallocate(p, n);
    }
    template<class U>
    bool operator==(const CountingAllocator<U> &other) const { return counts == other.counts; }
};
} // namespace descriptor_cache_detail
#endif

// The deque owns immutable string objects from their first publication. End
// insertion preserves references, including SSO character bodies. Only indexes
// change form: sparse flat buckets -> one fixed atomic pointer array per domain
// and shard. Destruction still requires all users/borrowed views to have drained.
template<bool Promote>
class HybridDescriptorCache {
    using Map = phmap::flat_hash_map<uint32_t, const std::string *>;
    using Pointer = std::atomic<const std::string *>;
    using NormalPayload = std::deque<std::string>;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    using Payload = std::deque<std::string, descriptor_cache_detail::CountingAllocator<std::string>>;
    struct Counters {
        std::atomic<uint64_t> calls{0}, hits{0}, dense_hits{0};
        descriptor_cache_detail::PayloadAllocations allocations;
        size_t char_bytes = 0, char_buffers = 0, sso_records = 0;
        size_t growths = 0, largest_bucket_overlap = 0, promotions = 0;
        size_t promotion_records = 0, promotion_capacity = 0, promotion_hash_bytes = 0;
        size_t promotion_payload_bytes = 0, promotion_overlap_bytes = 0;
        uint64_t promotion_ns = 0;
    };
#else
    using Payload = NormalPayload;
#endif
    static constexpr size_t kShards = 32;
    struct DenseIndex { std::unique_ptr<Pointer[]> slots; };
    struct Domain {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        // Outlives payload destruction and its allocator callbacks.
        Counters counters;
#endif
        std::unique_ptr<Payload> payload;
        Map sparse;
        std::unique_ptr<DenseIndex> dense_owner;
        std::atomic<DenseIndex *> published_dense{nullptr};
    };
    struct Shard {
        mutable std::mutex mutex;
        std::array<Domain, 2> domains;
    };
    std::array<Shard, kShards> shards_;
    std::array<size_t, 2> limits_{};
    bool initialized_ = false;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    void (*before_slow_lock_)(void *, bool, uint32_t) = nullptr;
    void *hook_context_ = nullptr;
#endif

    static size_t BucketBytes(const Map &map) {
        // Bundled flat policy has zero per-element external bytes, so this
        // existing layout helper is O(1), including control/alignment bytes.
        using Access = phmap::priv::hashtable_debug_internal::HashtableDebugAccess<Map>;
        return Access::AllocatedByteSize(map);
    }
    static size_t SlotCount(size_t limit, size_t shard) {
        return limit <= shard ? 0 : 1 + (limit - 1 - shard) / kShards;
    }
    static size_t DenseBytes(size_t slots) {
        if (slots > (std::numeric_limits<size_t>::max() - sizeof(DenseIndex)) / sizeof(Pointer))
            std::abort();
        return sizeof(DenseIndex) + slots * sizeof(Pointer);
    }

    void MaybePromoteLocked(Domain &domain, size_t slots) {
        const auto dense_bytes = DenseBytes(slots), hash_bytes = BucketBytes(domain.sparse);
        if (dense_bytes > hash_bytes) return;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        const auto started = std::chrono::steady_clock::now();
        auto &counters = domain.counters;
        counters.promotion_records = domain.sparse.size();
        counters.promotion_capacity = domain.sparse.capacity();
        counters.promotion_hash_bytes = hash_bytes;
        counters.promotion_payload_bytes = sizeof(NormalPayload) + counters.allocations.blocks.bytes
                + counters.allocations.directory.bytes + counters.char_bytes;
        counters.promotion_overlap_bytes = counters.promotion_payload_bytes + hash_bytes + dense_bytes;
#endif
        auto dense = std::make_unique<DenseIndex>();
        dense->slots = std::make_unique<Pointer[]>(slots);
        for (size_t i = 0; i < slots; ++i) dense->slots[i].store(nullptr, std::memory_order_relaxed);
        for (const auto &[id, value] : domain.sparse)
            dense->slots[id / kShards].store(value, std::memory_order_relaxed);
        auto *published = dense.get();
        domain.dense_owner = std::move(dense);
        domain.sparse.clear();
        domain.sparse.rehash(0); // clear alone retains buckets in bundled phmap.
        domain.published_dense.store(published, std::memory_order_release);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        ++counters.promotions;
        counters.promotion_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started).count();
#endif
    }

    template<bool Method, class Build>
    std::string_view GetOrCreateSlow(uint32_t index, Build &&build) PHMAP_ATTRIBUTE_NOINLINE {
        constexpr size_t kind = Method ? 0 : 1;
        const auto shard_id = index % kShards;
        auto &shard = shards_[shard_id];
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        if (before_slow_lock_) before_slow_lock_(hook_context_, Method, index);
#endif
        std::lock_guard lock(shard.mutex);
        auto &domain = shard.domains[kind];
        DenseIndex *dense = nullptr;
        // A caller can observe sparse, wait for a converter, then acquire this
        // lock after sparse buckets have been freed. Always recheck here.
        if constexpr (Promote) dense = domain.published_dense.load(std::memory_order_acquire);
        const std::string *found = nullptr;
        if (dense) found = dense->slots[index / kShards].load(std::memory_order_acquire);
        else if (auto it = domain.sparse.find(index); it != domain.sparse.end()) found = it->second;
        if (found) {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            domain.counters.hits.fetch_add(1, std::memory_order_relaxed);
#endif
            return *found;
        }
        auto descriptor = build(); // Raw DEX metadata only; no cache reentry.
        if (!domain.payload) {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            domain.payload = std::make_unique<Payload>(
                    descriptor_cache_detail::CountingAllocator<std::string>(&domain.counters.allocations));
#else
            domain.payload = std::make_unique<Payload>();
#endif
        }
        // Copy the lvalue, preserving the existing dense optional cache policy.
        const auto *value = &domain.payload->emplace_back(descriptor);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        const auto object = reinterpret_cast<uintptr_t>(value);
        const auto data = reinterpret_cast<uintptr_t>(value->data());
        if (data >= object && data < object + sizeof(*value)) ++domain.counters.sso_records;
        else { domain.counters.char_bytes += value->capacity() + 1; ++domain.counters.char_buffers; }
#endif
        if (dense) {
            dense->slots[index / kShards].store(value, std::memory_order_release);
        } else {
            const auto old_capacity = domain.sparse.capacity();
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            const auto old_bytes = BucketBytes(domain.sparse);
#endif
            if (!domain.sparse.emplace(index, value).second) std::abort();
            if (old_capacity != domain.sparse.capacity()) {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
                ++domain.counters.growths;
                domain.counters.largest_bucket_overlap = std::max(domain.counters.largest_bucket_overlap,
                        old_bytes + BucketBytes(domain.sparse));
#endif
                if constexpr (Promote) MaybePromoteLocked(domain, SlotCount(limits_[kind], shard_id));
            }
        }
        return *value;
    }

public:
    HybridDescriptorCache() = default;
    HybridDescriptorCache(const HybridDescriptorCache &) = delete;
    HybridDescriptorCache &operator=(const HybridDescriptorCache &) = delete;
    void Initialize(size_t methods, size_t fields) {
        if (initialized_) std::abort();
        limits_ = {methods, fields};
        initialized_ = true;
    }
    // Probe before materializing a Cold factory. A miss is not a completed
    // cache access; GetOrCreate accounts for it when entering the slow path.
    template<bool Method>
    const std::string *TryGet(uint32_t index) {
        constexpr size_t kind = Method ? 0 : 1;
        if (index >= limits_[kind]) std::abort();
        if constexpr (Promote) {
            auto &domain = shards_[index % kShards].domains[kind];
            if (auto *dense = domain.published_dense.load(std::memory_order_acquire)) {
                if (auto *value = dense->slots[index / kShards].load(std::memory_order_acquire)) {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
                    domain.counters.calls.fetch_add(1, std::memory_order_relaxed);
                    domain.counters.hits.fetch_add(1, std::memory_order_relaxed);
                    domain.counters.dense_hits.fetch_add(1, std::memory_order_relaxed);
#endif
                    return value;
                }
            }
        }
        return nullptr;
    }
    template<bool Method, class Build>
    std::string_view GetOrCreate(uint32_t index, Build &&build) {
        if (auto *value = TryGet<Method>(index)) return *value;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        constexpr size_t kind = Method ? 0 : 1;
        shards_[index % kShards].domains[kind].counters.calls.fetch_add(1, std::memory_order_relaxed);
#endif
        return GetOrCreateSlow<Method>(index, std::forward<Build>(build));
    }

#if DEXKIT_BENCHMARK_DIAGNOSTICS
    struct TableStatistics {
        size_t slots = 0, records = 0, capacity = 0, bucket_bytes = 0, dense_bytes = 0;
        size_t payload_owner_bytes = 0, payload_instrumentation_bytes = 0;
        descriptor_cache_detail::PayloadAllocations allocations;
        size_t char_bytes = 0, char_buffers = 0, sso_records = 0;
        uint64_t calls = 0, hits = 0, dense_hits = 0;
        size_t growths = 0, largest_bucket_overlap = 0, promotions = 0;
        size_t promotion_records = 0, promotion_capacity = 0, promotion_hash_bytes = 0;
        size_t promotion_payload_bytes = 0, promotion_overlap_bytes = 0;
        uint64_t promotion_ns = 0;
    };
    struct Statistics {
        std::array<std::array<TableStatistics, 2>, kShards> tables;
        std::array<size_t, 2> limits;
        size_t fixed_bytes = 0, instrumentation_bytes = 0;
    };
    static constexpr size_t FixedObjectBytes() {
        struct NormalDomain {
            std::unique_ptr<NormalPayload> payload;
            Map sparse;
            std::unique_ptr<DenseIndex> dense_owner;
            std::atomic<DenseIndex *> published_dense;
        };
        struct NormalShard { std::mutex mutex; std::array<NormalDomain, 2> domains; };
        struct NormalCache {
            std::array<NormalShard, kShards> shards;
            std::array<size_t, 2> limits;
            bool initialized;
        };
        return sizeof(NormalCache);
    }
    // Set before starting test threads; change only after joining all of them.
    void SetBeforeSlowLockHook(void (*hook)(void *, bool, uint32_t), void *context) {
        before_slow_lock_ = hook; hook_context_ = context;
    }
    // Each domain snapshot is locked. Aggregate diagnostics require drained
    // queries; sums of conversion overlaps are not physical process peaks.
    Statistics GetStatistics() const {
        Statistics stats{};
        stats.limits = limits_;
        stats.fixed_bytes = FixedObjectBytes();
        stats.instrumentation_bytes = sizeof(*this) - stats.fixed_bytes;
        for (size_t i = 0; i < kShards; ++i) {
            const auto &shard = shards_[i];
            std::lock_guard lock(shard.mutex);
            for (size_t kind = 0; kind < 2; ++kind) {
                const auto &domain = shard.domains[kind];
                const auto &counters = domain.counters;
                auto &table = stats.tables[i][kind];
                table.slots = SlotCount(limits_[kind], i);
                table.records = domain.payload ? domain.payload->size() : 0;
                table.capacity = domain.sparse.capacity(); table.bucket_bytes = BucketBytes(domain.sparse);
                table.dense_bytes = domain.dense_owner ? DenseBytes(table.slots) : 0;
                table.payload_owner_bytes = domain.payload ? sizeof(NormalPayload) : 0;
                table.payload_instrumentation_bytes = domain.payload ? sizeof(Payload) - sizeof(NormalPayload) : 0;
                stats.instrumentation_bytes += table.payload_instrumentation_bytes;
                table.allocations = counters.allocations;
                table.char_bytes = counters.char_bytes; table.char_buffers = counters.char_buffers;
                table.sso_records = counters.sso_records;
                table.calls = counters.calls.load(std::memory_order_relaxed);
                table.hits = counters.hits.load(std::memory_order_relaxed);
                table.dense_hits = counters.dense_hits.load(std::memory_order_relaxed);
                table.growths = counters.growths; table.largest_bucket_overlap = counters.largest_bucket_overlap;
                table.promotions = counters.promotions; table.promotion_records = counters.promotion_records;
                table.promotion_capacity = counters.promotion_capacity; table.promotion_hash_bytes = counters.promotion_hash_bytes;
                table.promotion_payload_bytes = counters.promotion_payload_bytes;
                table.promotion_overlap_bytes = counters.promotion_overlap_bytes; table.promotion_ns = counters.promotion_ns;
            }
        }
        return stats;
    }
#endif
};

} // namespace dexkit

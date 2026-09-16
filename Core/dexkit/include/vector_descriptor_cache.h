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
#include <vector>
#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include <chrono>
#endif

namespace dexkit {
namespace vector_descriptor_detail {
struct SparseAllocations {
    size_t block_bytes = 0, directory_bytes = 0;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    size_t block_live = 0, directory_live = 0;
    size_t block_allocations = 0, directory_allocations = 0;
#endif
};

// Production accounting is required for the conversion rule. It counts only
// deque allocation/deallocation, under the existing shard mutex, never hits.
template<class T>
struct SparseAllocator {
    using value_type = T;
    SparseAllocations *counts = nullptr;
    SparseAllocator() = default;
    explicit SparseAllocator(SparseAllocations *value) : counts(value) {}
    template<class U>
    SparseAllocator(const SparseAllocator<U> &other) : counts(other.counts) {}
    T *allocate(size_t n) {
        if (!counts || n > std::numeric_limits<size_t>::max() / sizeof(T)) std::abort();
        auto *result = std::allocator<T>{}.allocate(n);
        if constexpr (std::is_same_v<T, std::string>) {
            counts->block_bytes += n * sizeof(T);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            ++counts->block_live; ++counts->block_allocations;
#endif
        } else {
            counts->directory_bytes += n * sizeof(T);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            ++counts->directory_live; ++counts->directory_allocations;
#endif
        }
        return result;
    }
    void deallocate(T *p, size_t n) {
        if constexpr (std::is_same_v<T, std::string>) {
            counts->block_bytes -= n * sizeof(T);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            --counts->block_live;
#endif
        } else {
            counts->directory_bytes -= n * sizeof(T);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            --counts->directory_live;
#endif
        }
        std::allocator<T>{}.deallocate(p, n);
    }
    template<class U>
    bool operator==(const SparseAllocator<U> &other) const { return counts == other.counts; }
};
} // namespace vector_descriptor_detail

// Form and owning storage change only while all query/native borrows and
// warmup workers are drained. Dense vectors never resize after that boundary.
// A sparse miss can request maintenance but never frees a published string.
template<bool Promote>
class VectorDescriptorCache {
    using Map = phmap::flat_hash_map<uint32_t, const std::string *>;
    using Ready = std::atomic<uint8_t>;
    using Allocations = vector_descriptor_detail::SparseAllocations;
    using Payload = std::deque<std::string, vector_descriptor_detail::SparseAllocator<std::string>>;
    using Notify = void (*)(void *);
    static constexpr size_t kShards = 32;
    struct SparsePart {
        Allocations allocations; // Outlives the allocator callbacks.
        std::unique_ptr<Payload> payload;
        Map index;
        size_t structural_bytes = 0;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        size_t sparse_chars = 0, sparse_char_buffers = 0, sparse_sso = 0;
        size_t dense_chars = 0, dense_char_buffers = 0, dense_sso = 0;
#endif
    };
    struct Shard {
        mutable std::mutex mutex;
        std::array<SparsePart, 2> parts;
    };
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    struct Counters {
        std::atomic<uint64_t> calls{0}, hits{0}, dense_hits{0};
        std::atomic<uint64_t> sparse_builds{0}, dense_builds{0};
        size_t conversions = 0, discarded_records = 0, discarded_chars = 0;
        size_t structural_at_conversion = 0;
        uint64_t conversion_ns = 0;
    };
#endif
    struct Domain {
        size_t limit = 0;
        std::vector<std::string> values;
        std::unique_ptr<Ready[]> ready;
        std::atomic<size_t> sparse_structural_bytes{0};
        std::atomic<bool> pending{false};
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        Counters counters;
#endif
    };
    std::array<Shard, kShards> shards_;
    std::array<Domain, 2> domains_;
    Notify notify_ = nullptr;
    void *notify_context_ = nullptr;
    bool initialized_ = false;

    static size_t BucketBytes(const Map &map) {
        using Access = phmap::priv::hashtable_debug_internal::HashtableDebugAccess<Map>;
        return Access::AllocatedByteSize(map);
    }
    static size_t DenseBaseBytes(size_t limit) {
        constexpr auto stride = sizeof(std::string) + sizeof(Ready);
        if (limit > std::numeric_limits<size_t>::max() / stride) std::abort();
        // Both owners already exist inline in the fixed cache object.
        return limit * stride;
    }
    void AccountSparseGrowth(Domain &domain, SparsePart &part) {
        const auto bytes = BucketBytes(part.index) + (part.payload ? sizeof(Payload) : 0)
                + part.allocations.block_bytes + part.allocations.directory_bytes;
        if (bytes == part.structural_bytes) return;
        if (bytes < part.structural_bytes) std::abort();
        const auto growth = bytes - part.structural_bytes;
        part.structural_bytes = bytes;
        const auto total = domain.sparse_structural_bytes.fetch_add(growth, std::memory_order_relaxed) + growth;
        if constexpr (Promote) {
            if (total >= DenseBaseBytes(domain.limit)
                    && !domain.pending.exchange(true, std::memory_order_release)) {
                if (notify_) notify_(notify_context_);
            }
        }
    }
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    static bool IsSso(const std::string &value) {
        const auto object = reinterpret_cast<uintptr_t>(&value);
        const auto data = reinterpret_cast<uintptr_t>(value.data());
        return data >= object && data < object + sizeof(value);
    }
#endif
    template<bool Method, class Build>
    const std::string *GetOrCreateSlow(uint32_t index, Build &&build) PHMAP_ATTRIBUTE_NOINLINE {
        constexpr size_t kind = Method ? 0 : 1;
        auto &domain = domains_[kind];
        auto &shard = shards_[index % kShards];
        std::lock_guard lock(shard.mutex);
        auto &part = shard.parts[kind];
        const std::string *found = nullptr;
        if (domain.ready) {
            if (domain.ready[index].load(std::memory_order_acquire)) found = &domain.values[index];
        } else if (auto it = part.index.find(index); it != part.index.end()) found = it->second;
        if (found) {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            domain.counters.hits.fetch_add(1, std::memory_order_relaxed);
#endif
            return found;
        }
        auto descriptor = build();
        if (domain.ready) {
            auto &value = domain.values[index];
            value = descriptor;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            if (IsSso(value)) ++part.dense_sso;
            else { part.dense_chars += value.capacity() + 1; ++part.dense_char_buffers; }
            domain.counters.dense_builds.fetch_add(1, std::memory_order_relaxed);
#endif
            domain.ready[index].store(1, std::memory_order_release);
            return &value;
        }
        if (!part.payload) part.payload = std::make_unique<Payload>(
                vector_descriptor_detail::SparseAllocator<std::string>(&part.allocations));
        const auto *value = &part.payload->emplace_back(descriptor);
        if (!part.index.emplace(index, value).second) std::abort();
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        if (IsSso(*value)) ++part.sparse_sso;
        else { part.sparse_chars += value->capacity() + 1; ++part.sparse_char_buffers; }
        domain.counters.sparse_builds.fetch_add(1, std::memory_order_relaxed);
#endif
        AccountSparseGrowth(domain, part);
        return value;
    }

public:
    VectorDescriptorCache() = default;
    VectorDescriptorCache(const VectorDescriptorCache &) = delete;
    VectorDescriptorCache &operator=(const VectorDescriptorCache &) = delete;
    void Initialize(size_t methods, size_t fields, Notify notify = nullptr, void *context = nullptr) {
        if (initialized_) std::abort();
        domains_[0].limit = methods; domains_[1].limit = fields;
        DenseBaseBytes(methods); DenseBaseBytes(fields);
        notify_ = notify; notify_context_ = context; initialized_ = true;
    }
    template<bool Method>
    const std::string *TryGet(uint32_t index) {
        auto &domain = domains_[Method ? 0 : 1];
        if (index >= domain.limit) std::abort();
        // The query admission barrier protects these ordinary owner reads.
        // Only per-slot contents require a ready publication within a query.
        if (domain.ready && domain.ready[index].load(std::memory_order_acquire)) {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            domain.counters.calls.fetch_add(1, std::memory_order_relaxed);
            domain.counters.hits.fetch_add(1, std::memory_order_relaxed);
            domain.counters.dense_hits.fetch_add(1, std::memory_order_relaxed);
#endif
            return &domain.values[index];
        }
        return nullptr;
    }
    template<bool Method, class Build>
    std::string_view GetOrCreate(uint32_t index, Build &&build) {
        if (const auto *value = TryGet<Method>(index)) return *value;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        domains_[Method ? 0 : 1].counters.calls.fetch_add(1, std::memory_order_relaxed);
#endif
        return *GetOrCreateSlow<Method>(index, std::forward<Build>(build));
    }
    bool HasPending() const {
        return domains_[0].pending.load(std::memory_order_acquire)
                || domains_[1].pending.load(std::memory_order_acquire);
    }
    template<bool Method>
    bool IsDense() const { return domains_[Method ? 0 : 1].ready != nullptr; }

    // The caller must exclusively own maintenance: no admitted operations,
    // native borrow sessions, worker tasks or warmup may still use this cache.
    void ConvertPending() {
        if constexpr (!Promote) return;
        for (size_t kind = 0; kind < 2; ++kind) {
            auto &domain = domains_[kind];
            if (!domain.pending.load(std::memory_order_acquire)) continue;
            if (domain.ready || domain.limit == 0) std::abort();
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            const auto started = std::chrono::steady_clock::now();
            domain.counters.structural_at_conversion = domain.sparse_structural_bytes.load();
#endif
            for (auto &shard : shards_) {
                auto &part = shard.parts[kind];
#if DEXKIT_BENCHMARK_DIAGNOSTICS
                domain.counters.discarded_records += part.index.size();
                domain.counters.discarded_chars += part.sparse_chars;
                part.sparse_chars = part.sparse_char_buffers = part.sparse_sso = 0;
#endif
                part.index.clear(); part.index.rehash(0);
                part.payload.reset();
                if (part.allocations.block_bytes || part.allocations.directory_bytes) std::abort();
                part.structural_bytes = 0;
            }
            domain.sparse_structural_bytes.store(0, std::memory_order_relaxed);
            // Old values are deliberately discarded before allocating the new
            // storage. No recovery/fallback to the old generation is promised.
            domain.values = std::vector<std::string>(domain.limit);
            domain.ready = std::make_unique<Ready[]>(domain.limit);
            for (size_t index = 0; index < domain.limit; ++index)
                domain.ready[index].store(0, std::memory_order_relaxed);
            domain.pending.store(false, std::memory_order_relaxed);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            ++domain.counters.conversions;
            domain.counters.conversion_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started).count();
#endif
        }
    }

#if DEXKIT_BENCHMARK_DIAGNOSTICS
    struct DomainStatistics {
        size_t limit = 0, records = 0, sparse_records = 0, bucket_bytes = 0;
        size_t payload_owner_bytes = 0, block_bytes = 0, directory_bytes = 0;
        size_t sparse_char_bytes = 0, dense_char_bytes = 0, char_buffers = 0, sso_records = 0;
        size_t payload_owners = 0, block_live = 0, directory_live = 0, bucket_buffers = 0;
        size_t dense_object_bytes = 0, ready_bytes = 0, conversion_base_bytes = 0;
        size_t sparse_structural_bytes = 0, conversions = 0, discarded_records = 0, discarded_chars = 0;
        size_t structural_at_conversion = 0;
        uint64_t calls = 0, hits = 0, dense_hits = 0, sparse_builds = 0, dense_builds = 0, conversion_ns = 0;
        bool dense = false, pending = false;
    };
    struct Statistics {
        std::array<DomainStatistics, 2> domains;
        size_t fixed_bytes = 0, instrumentation_bytes = 0;
    };
    static constexpr size_t FixedObjectBytes() {
        struct NormalAllocations { size_t block_bytes, directory_bytes; };
        struct NormalPart {
            NormalAllocations allocations;
            std::unique_ptr<Payload> payload;
            Map index;
            size_t structural_bytes;
        };
        struct NormalShard { std::mutex mutex; std::array<NormalPart, 2> parts; };
        struct NormalDomain {
            size_t limit;
            std::vector<std::string> values;
            std::unique_ptr<Ready[]> ready;
            std::atomic<size_t> sparse_structural_bytes;
            std::atomic<bool> pending;
        };
        struct NormalCache {
            std::array<NormalShard, kShards> shards;
            std::array<NormalDomain, 2> domains;
            Notify notify; void *context; bool initialized;
        };
        return sizeof(NormalCache);
    }
    // Query/native borrows must be drained for an aggregate snapshot. This
    // locks sparse bookkeeping but is not a reclamation/maintenance barrier.
    Statistics GetStatistics() const {
        Statistics result;
        result.fixed_bytes = FixedObjectBytes();
        result.instrumentation_bytes = sizeof(*this) - result.fixed_bytes;
        for (size_t kind = 0; kind < 2; ++kind) {
            auto &out = result.domains[kind];
            const auto &domain = domains_[kind];
            const auto &counts = domain.counters;
            out.limit = domain.limit; out.dense = domain.ready != nullptr;
            out.pending = domain.pending.load(std::memory_order_relaxed);
            out.conversion_base_bytes = DenseBaseBytes(domain.limit);
            out.dense_object_bytes = domain.values.capacity() * sizeof(std::string);
            out.ready_bytes = out.dense ? domain.limit * sizeof(Ready) : 0;
            out.sparse_structural_bytes = domain.sparse_structural_bytes.load();
            out.calls = counts.calls.load(); out.hits = counts.hits.load(); out.dense_hits = counts.dense_hits.load();
            out.sparse_builds = counts.sparse_builds.load(); out.dense_builds = counts.dense_builds.load();
            out.conversions = counts.conversions; out.discarded_records = counts.discarded_records;
            out.discarded_chars = counts.discarded_chars; out.structural_at_conversion = counts.structural_at_conversion;
            out.conversion_ns = counts.conversion_ns;
            for (const auto &shard : shards_) {
                std::lock_guard lock(shard.mutex);
                const auto &part = shard.parts[kind];
                out.sparse_records += part.index.size(); out.bucket_bytes += BucketBytes(part.index);
                out.bucket_buffers += part.index.capacity() != 0;
                out.payload_owner_bytes += part.payload ? sizeof(Payload) : 0;
                out.payload_owners += part.payload != nullptr;
                out.block_bytes += part.allocations.block_bytes; out.directory_bytes += part.allocations.directory_bytes;
                out.block_live += part.allocations.block_live; out.directory_live += part.allocations.directory_live;
                out.sparse_char_bytes += part.sparse_chars; out.dense_char_bytes += part.dense_chars;
                out.char_buffers += part.sparse_char_buffers + part.dense_char_buffers;
                out.sso_records += part.sparse_sso + part.dense_sso;
            }
            out.records = out.sparse_records + out.dense_builds;
        }
        return result;
    }
#endif
};
} // namespace dexkit

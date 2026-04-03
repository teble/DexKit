// DexKit - An high-performance runtime parsing library for dex
// implemented in C++.
// Copyright (C) 2022-2023 LuckyPray
// https://github.com/LuckyPray/DexKit
//
// This program is free software: you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see
// <https://www.gnu.org/licenses/>.
// <https://github.com/LuckyPray/DexKit/blob/master/LICENSE>.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include "parallel_hashmap/phmap.h"

namespace dexkit {

enum class QueryKind : uint8_t {
    FindClass,
    FindMethod,
    FindField,
    BatchFindClassUsingStrings,
    BatchFindMethodUsingStrings,
};

enum class QueryPriority : uint8_t {
    Normal = 0,
    LatencySensitive = 1,
};

struct QueryMetrics {
    std::chrono::steady_clock::time_point created_at = std::chrono::steady_clock::now();
    std::atomic<uint32_t> submitted_tasks = 0;
    std::atomic<uint32_t> dispatched_tasks = 0;
    std::atomic<uint32_t> base_dispatched_tasks = 0;
    std::atomic<uint32_t> bonus_dispatched_tasks = 0;
    std::atomic<uint32_t> completed_tasks = 0;
    std::atomic<uint32_t> max_in_flight = 0;
    std::atomic<uint32_t> max_query_share_count = 0;
    std::atomic<int64_t> first_dispatch_delay_ns = -1;
};

struct QueryMetricsSnapshot {
    uint32_t submitted_tasks = 0;
    uint32_t dispatched_tasks = 0;
    uint32_t base_dispatched_tasks = 0;
    uint32_t bonus_dispatched_tasks = 0;
    uint32_t completed_tasks = 0;
    uint32_t max_in_flight = 0;
    uint32_t max_query_share_count = 0;
    int64_t first_dispatch_delay_ns = -1;
};

struct QueryCacheKey {
    uint8_t scope = 0;
    std::uintptr_t key = 0;

    [[nodiscard]] bool operator==(const QueryCacheKey &other) const {
        return scope == other.scope && key == other.key;
    }
};

struct QueryCacheKeyHash {
    [[nodiscard]] size_t operator()(const QueryCacheKey &value) const {
        auto h1 = std::hash<uint8_t>{}(value.scope);
        auto h2 = std::hash<std::uintptr_t>{}(value.key);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6U) + (h1 >> 2U));
    }
};

class QueryContext {
public:
    class ScopedBinding {
    public:
        explicit ScopedBinding(QueryContext &query_context)
                : previous_(current_), current_query_(&query_context) {
            current_ = &query_context;
        }

        ScopedBinding(const ScopedBinding &) = delete;
        ScopedBinding &operator=(const ScopedBinding &) = delete;

        ScopedBinding(ScopedBinding &&other) noexcept
                : previous_(other.previous_), current_query_(other.current_query_) {
            other.current_query_ = nullptr;
        }

        ScopedBinding &operator=(ScopedBinding &&other) = delete;

        ~ScopedBinding() {
            if (current_query_ != nullptr) {
                current_ = previous_;
            }
        }

    private:
        QueryContext *previous_ = nullptr;
        QueryContext *current_query_ = nullptr;
    };

    explicit QueryContext(QueryKind kind) : kind_(kind), query_id_(NextQueryId()) {}

    [[nodiscard]] uint64_t GetQueryId() const {
        return query_id_;
    }

    [[nodiscard]] QueryKind GetKind() const {
        return kind_;
    }

    void SetQueryPriority(QueryPriority priority) {
        priority_ = priority;
    }

    [[nodiscard]] QueryPriority GetQueryPriority() const {
        return priority_;
    }

    void Cancel() {
        cancelled_.store(true, std::memory_order_release);
    }

    [[nodiscard]] bool IsCancelled() const {
        return cancelled_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool RequestEarlyExit() {
        return !early_exit_.exchange(true, std::memory_order_acq_rel);
    }

    [[nodiscard]] bool ShouldEarlyExit() const {
        return early_exit_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool ShouldStop() const {
        return IsCancelled() || ShouldEarlyExit();
    }

    void MarkTaskSubmitted() {
        metrics_.submitted_tasks.fetch_add(1, std::memory_order_relaxed);
    }

    void MarkTaskCompleted() {
        metrics_.completed_tasks.fetch_add(1, std::memory_order_relaxed);
    }

    void MarkTaskDispatched(bool used_bonus_dispatch, uint32_t query_in_flight, uint32_t query_share_count) {
        metrics_.dispatched_tasks.fetch_add(1, std::memory_order_relaxed);
        if (used_bonus_dispatch) {
            metrics_.bonus_dispatched_tasks.fetch_add(1, std::memory_order_relaxed);
        } else {
            metrics_.base_dispatched_tasks.fetch_add(1, std::memory_order_relaxed);
        }
        UpdateMax(metrics_.max_in_flight, query_in_flight);
        UpdateMax(metrics_.max_query_share_count, query_share_count);

        int64_t expected = -1;
        auto first_dispatch_delay_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - metrics_.created_at
        ).count();
        (void) metrics_.first_dispatch_delay_ns.compare_exchange_strong(
                expected,
                first_dispatch_delay_ns,
                std::memory_order_acq_rel,
                std::memory_order_relaxed
        );
    }

    [[nodiscard]] const QueryMetrics &GetMetrics() const {
        return metrics_;
    }

    [[nodiscard]] QueryMetricsSnapshot SnapshotMetrics() const {
        QueryMetricsSnapshot snapshot;
        snapshot.submitted_tasks = metrics_.submitted_tasks.load(std::memory_order_relaxed);
        snapshot.dispatched_tasks = metrics_.dispatched_tasks.load(std::memory_order_relaxed);
        snapshot.base_dispatched_tasks = metrics_.base_dispatched_tasks.load(std::memory_order_relaxed);
        snapshot.bonus_dispatched_tasks = metrics_.bonus_dispatched_tasks.load(std::memory_order_relaxed);
        snapshot.completed_tasks = metrics_.completed_tasks.load(std::memory_order_relaxed);
        snapshot.max_in_flight = metrics_.max_in_flight.load(std::memory_order_relaxed);
        snapshot.max_query_share_count = metrics_.max_query_share_count.load(std::memory_order_relaxed);
        snapshot.first_dispatch_delay_ns = metrics_.first_dispatch_delay_ns.load(std::memory_order_relaxed);
        return snapshot;
    }

    void PublishSnapshotToCurrentThread() const {
        last_query_metrics_snapshot_ = SnapshotMetrics();
    }

    [[nodiscard]] static QueryMetricsSnapshot LastQueryMetricsSnapshot() {
        return last_query_metrics_snapshot_;
    }

    [[nodiscard]] ScopedBinding BindToCurrentThread() {
        return ScopedBinding(*this);
    }

    [[nodiscard]] static QueryContext *Current() {
        return current_;
    }

    template<typename T, typename Factory>
    std::shared_ptr<T> GetOrCreateCache(uint8_t scope, std::uintptr_t key, Factory &&factory) {
        auto cache_key = QueryCacheKey{scope, key};
        std::lock_guard lock(cache_mutex_);
        if (query_cache_.contains(cache_key)) {
            auto &cached = query_cache_[cache_key];
            return std::shared_ptr<T>(cached, reinterpret_cast<T *>(cached.get()));
        }
        auto value = std::make_shared<T>(std::forward<Factory>(factory)());
        query_cache_[cache_key] = value;
        return value;
    }

private:
    static void UpdateMax(std::atomic<uint32_t> &target, uint32_t value) {
        auto current = target.load(std::memory_order_relaxed);
        while (current < value &&
               !target.compare_exchange_weak(current, value, std::memory_order_relaxed, std::memory_order_relaxed)) {}
    }

    static uint64_t NextQueryId() {
        return next_query_id_.fetch_add(1, std::memory_order_relaxed);
    }

    QueryKind kind_;
    uint64_t query_id_;
    QueryPriority priority_ = QueryPriority::Normal;
    std::atomic<bool> cancelled_ = false;
    std::atomic<bool> early_exit_ = false;
    QueryMetrics metrics_{};
    std::mutex cache_mutex_;
    phmap::flat_hash_map<QueryCacheKey, std::shared_ptr<void>, QueryCacheKeyHash> query_cache_;

    inline static std::atomic<uint64_t> next_query_id_ = 1;
    inline static thread_local QueryMetricsSnapshot last_query_metrics_snapshot_{};
    inline static thread_local QueryContext *current_ = nullptr;
};

} // namespace dexkit

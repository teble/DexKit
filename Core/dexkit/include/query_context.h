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
    std::atomic<uint32_t> completed_tasks = 0;
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

    [[nodiscard]] const QueryMetrics &GetMetrics() const {
        return metrics_;
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
    inline static thread_local QueryContext *current_ = nullptr;
};

} // namespace dexkit

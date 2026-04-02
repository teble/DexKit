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

namespace dexkit {

enum class QueryKind : uint8_t {
    FindClass,
    FindMethod,
    FindField,
    BatchFindClassUsingStrings,
    BatchFindMethodUsingStrings,
};

struct QueryMetrics {
    std::chrono::steady_clock::time_point created_at = std::chrono::steady_clock::now();
    std::atomic<uint32_t> submitted_tasks = 0;
    std::atomic<uint32_t> completed_tasks = 0;
};

class QueryContext {
public:
    explicit QueryContext(QueryKind kind) : kind_(kind), query_id_(NextQueryId()) {}

    [[nodiscard]] uint64_t GetQueryId() const {
        return query_id_;
    }

    [[nodiscard]] QueryKind GetKind() const {
        return kind_;
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

private:
    static uint64_t NextQueryId() {
        return next_query_id_.fetch_add(1, std::memory_order_relaxed);
    }

    QueryKind kind_;
    uint64_t query_id_;
    std::atomic<bool> cancelled_ = false;
    std::atomic<bool> early_exit_ = false;
    QueryMetrics metrics_{};

    inline static std::atomic<uint64_t> next_query_id_ = 1;
};

} // namespace dexkit

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

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ThreadPool.h"

namespace dexkit {

class QueryScheduler final : public std::enable_shared_from_this<QueryScheduler> {
public:
    QueryScheduler(std::shared_ptr<ThreadPool> pool, size_t worker_count)
            : pool_(std::move(pool)), worker_count_(std::max<size_t>(1, worker_count)) {}

    void AttachQuery(uint64_t query_id) {
        std::lock_guard lock(mutex_);
        auto &slot = query_slots_[query_id];
        slot.query_id = query_id;
        slot.attached = true;
    }

    void DetachQuery(uint64_t query_id) {
        std::vector<DispatchTask> dispatch_tasks;
        {
            std::lock_guard lock(mutex_);
            auto it = query_slots_.find(query_id);
            if (it == query_slots_.end()) {
                return;
            }
            it->second.attached = false;
            TryEraseSlotLocked(it);
            DispatchReadyTasksLocked(dispatch_tasks);
        }
        EnqueueDispatchTasks(std::move(dispatch_tasks));
    }

    void Submit(uint64_t query_id, std::function<void()> task) {
        std::vector<DispatchTask> dispatch_tasks;
        {
            std::lock_guard lock(mutex_);
            auto &slot = query_slots_[query_id];
            slot.query_id = query_id;
            slot.attached = true;
            slot.pending_tasks.emplace_back(std::move(task));
            TryEnqueueRunnableLocked(slot);
            DispatchReadyTasksLocked(dispatch_tasks);
        }
        EnqueueDispatchTasks(std::move(dispatch_tasks));
    }

private:
    struct QuerySlot {
        uint64_t query_id = 0;
        bool attached = true;
        bool queued = false;
        size_t in_flight = 0;
        std::deque<std::function<void()>> pending_tasks;
    };

    struct DispatchTask {
        uint64_t query_id = 0;
        std::function<void()> task;
    };

    using QuerySlotMap = std::unordered_map<uint64_t, QuerySlot>;

    class TaskCompletionGuard {
    public:
        TaskCompletionGuard(std::shared_ptr<QueryScheduler> scheduler, uint64_t query_id)
                : scheduler_(std::move(scheduler)), query_id_(query_id) {}

        TaskCompletionGuard(const TaskCompletionGuard &) = delete;
        TaskCompletionGuard &operator=(const TaskCompletionGuard &) = delete;

        ~TaskCompletionGuard() {
            if (scheduler_ != nullptr) {
                scheduler_->OnTaskFinished(query_id_);
            }
        }

    private:
        std::shared_ptr<QueryScheduler> scheduler_;
        uint64_t query_id_ = 0;
    };

    [[nodiscard]] size_t ActiveRunnableQueryCountLocked() const {
        size_t active_query_count = 0;
        for (const auto &[query_id, slot]: query_slots_) {
            (void) query_id;
            if (!slot.pending_tasks.empty() || slot.in_flight != 0) {
                ++active_query_count;
            }
        }
        return active_query_count;
    }

    [[nodiscard]] size_t QueryInFlightLimitLocked() const {
        auto active_query_count = std::max<size_t>(1, ActiveRunnableQueryCountLocked());
        return std::max<size_t>(1, (worker_count_ + active_query_count - 1) / active_query_count);
    }

    void TryEnqueueRunnableLocked(QuerySlot &slot) {
        if (slot.queued || slot.pending_tasks.empty()) {
            return;
        }
        if (slot.in_flight >= QueryInFlightLimitLocked()) {
            return;
        }
        slot.queued = true;
        runnable_queries_.push_back(slot.query_id);
    }

    void TryEraseSlotLocked(QuerySlotMap::iterator it) {
        if (it->second.attached) {
            return;
        }
        if (!it->second.pending_tasks.empty()) {
            return;
        }
        if (it->second.in_flight != 0) {
            return;
        }
        query_slots_.erase(it);
    }

    void DispatchReadyTasksLocked(std::vector<DispatchTask> &dispatch_tasks) {
        while (total_in_flight_ < worker_count_ && !runnable_queries_.empty()) {
            auto query_id = runnable_queries_.front();
            runnable_queries_.pop_front();

            auto it = query_slots_.find(query_id);
            if (it == query_slots_.end()) {
                continue;
            }

            auto &slot = it->second;
            slot.queued = false;

            auto query_in_flight_limit = QueryInFlightLimitLocked();
            if (slot.pending_tasks.empty() || slot.in_flight >= query_in_flight_limit) {
                TryEnqueueRunnableLocked(slot);
                TryEraseSlotLocked(it);
                continue;
            }

            auto task = std::move(slot.pending_tasks.front());
            slot.pending_tasks.pop_front();
            ++slot.in_flight;
            ++total_in_flight_;
            if (!slot.pending_tasks.empty() && slot.in_flight < query_in_flight_limit) {
                slot.queued = true;
                runnable_queries_.push_back(query_id);
            }

            dispatch_tasks.push_back(DispatchTask{query_id, std::move(task)});
        }
    }

    void EnqueueDispatchTasks(std::vector<DispatchTask> dispatch_tasks) {
        if (dispatch_tasks.empty()) {
            return;
        }

        auto self = shared_from_this();
        for (auto &dispatch_task: dispatch_tasks) {
            pool_->enqueue([self, dispatch_task = std::move(dispatch_task)]() mutable {
                TaskCompletionGuard completion_guard(self, dispatch_task.query_id);
                dispatch_task.task();
            });
        }
    }

    void OnTaskFinished(uint64_t query_id) {
        std::vector<DispatchTask> dispatch_tasks;
        {
            std::lock_guard lock(mutex_);
            if (total_in_flight_ > 0) {
                --total_in_flight_;
            }

            auto it = query_slots_.find(query_id);
            if (it != query_slots_.end()) {
                auto &slot = it->second;
                if (slot.in_flight > 0) {
                    --slot.in_flight;
                }
                TryEnqueueRunnableLocked(slot);
                TryEraseSlotLocked(it);
            }

            DispatchReadyTasksLocked(dispatch_tasks);
        }
        EnqueueDispatchTasks(std::move(dispatch_tasks));
    }

    std::shared_ptr<ThreadPool> pool_;
    size_t worker_count_;
    std::mutex mutex_;
    QuerySlotMap query_slots_;
    std::deque<uint64_t> runnable_queries_;
    size_t total_in_flight_ = 0;
};

} // namespace dexkit

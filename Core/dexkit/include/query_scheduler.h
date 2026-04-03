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
#include "query_context.h"

namespace dexkit {

class QueryScheduler final : public std::enable_shared_from_this<QueryScheduler> {
public:
    QueryScheduler(std::shared_ptr<ThreadPool> pool, size_t worker_count)
            : pool_(std::move(pool)), worker_count_(std::max<size_t>(1, worker_count)) {}

    void AttachQuery(uint64_t query_id, QueryPriority priority) {
        std::lock_guard lock(mutex_);
        auto &slot = query_slots_[query_id];
        slot.query_id = query_id;
        slot.priority = priority;
        slot.attached = true;
    }

    void ActivateQuery(uint64_t query_id) {
        std::vector<DispatchTask> dispatch_tasks;
        {
            std::lock_guard lock(mutex_);
            auto &slot = query_slots_[query_id];
            slot.query_id = query_id;
            slot.attached = true;
            if (!slot.activated) {
                slot.activated = true;
            }
            if (!slot.pending_tasks.empty() && slot.dispatch_budget == 0) {
                slot.dispatch_budget = ComputeDispatchBudgetLocked(slot, QueryShareCountLocked());
            }
            TryEnqueueRunnableLocked(slot);
            DispatchReadyTasksLocked(dispatch_tasks);
        }
        EnqueueDispatchTasks(std::move(dispatch_tasks));
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
            slot.submission_started = true;
            auto was_idle = slot.pending_tasks.empty() && slot.in_flight == 0;
            slot.pending_tasks.emplace_back(std::move(task));
            if (slot.activated && was_idle && slot.dispatch_budget == 0) {
                slot.dispatch_budget = ComputeDispatchBudgetLocked(slot, QueryShareCountLocked());
            }
            TryEnqueueRunnableLocked(slot);
            DispatchReadyTasksLocked(dispatch_tasks);
        }
        EnqueueDispatchTasks(std::move(dispatch_tasks));
    }

private:
    struct QuerySlot {
        uint64_t query_id = 0;
        QueryPriority priority = QueryPriority::Normal;
        bool attached = true;
        bool activated = false;
        bool submission_started = false;
        bool queued = false;
        size_t in_flight = 0;
        size_t dispatch_budget = 0;
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

    [[nodiscard]] size_t QueryInFlightLimitForActiveQueryCount(size_t active_query_count) const {
        return std::max<size_t>(1, (worker_count_ + active_query_count - 1) / active_query_count);
    }

    [[nodiscard]] size_t VisibleQueryShareCountLocked() const {
        size_t visible_query_count = 0;
        for (const auto &[query_id, slot]: query_slots_) {
            (void) query_id;
            if (!slot.attached) {
                continue;
            }
            if (slot.activated) {
                if (!slot.pending_tasks.empty() || slot.in_flight != 0) {
                    ++visible_query_count;
                }
                continue;
            }
            if (slot.submission_started) {
                ++visible_query_count;
            }
        }
        return visible_query_count;
    }

    [[nodiscard]] size_t QueryShareCountLocked() const {
        return std::max<size_t>(1, VisibleQueryShareCountLocked());
    }

    [[nodiscard]] size_t QueryInFlightLimitLocked() const {
        return QueryInFlightLimitForActiveQueryCount(QueryShareCountLocked());
    }

    [[nodiscard]] size_t ComputeDispatchBudgetLocked(const QuerySlot &slot, size_t active_query_count) const {
        if (active_query_count <= 1) {
            return worker_count_;
        }

        auto base_budget = QueryInFlightLimitForActiveQueryCount(active_query_count);
        if (slot.priority == QueryPriority::LatencySensitive) {
            return std::min(worker_count_, base_budget + 1);
        }
        return base_budget;
    }

    [[nodiscard]] bool HasRunnableQueriesLocked() const {
        return !latency_sensitive_runnable_queries_.empty() || !normal_runnable_queries_.empty();
    }

    [[nodiscard]] uint64_t PopRunnableQueryLocked() {
        if (!latency_sensitive_runnable_queries_.empty()) {
            auto query_id = latency_sensitive_runnable_queries_.front();
            latency_sensitive_runnable_queries_.pop_front();
            return query_id;
        }
        auto query_id = normal_runnable_queries_.front();
        normal_runnable_queries_.pop_front();
        return query_id;
    }

    void PushRunnableQueryLocked(const QuerySlot &slot) {
        if (slot.priority == QueryPriority::LatencySensitive) {
            latency_sensitive_runnable_queries_.push_back(slot.query_id);
            return;
        }
        normal_runnable_queries_.push_back(slot.query_id);
    }

    bool RefillDispatchBudgetsLocked() {
        auto query_share_count = QueryShareCountLocked();
        auto query_in_flight_limit = QueryInFlightLimitForActiveQueryCount(query_share_count);
        bool refilled = false;

        for (auto &[query_id, slot]: query_slots_) {
            (void) query_id;
            if (!slot.activated) {
                continue;
            }
            if (slot.pending_tasks.empty()) {
                continue;
            }
            if (slot.in_flight >= query_in_flight_limit) {
                continue;
            }
            if (slot.dispatch_budget == 0) {
                slot.dispatch_budget = ComputeDispatchBudgetLocked(slot, query_share_count);
            }
            if (slot.dispatch_budget == 0 || slot.queued) {
                continue;
            }
            slot.queued = true;
            PushRunnableQueryLocked(slot);
            refilled = true;
        }

        return refilled;
    }

    void TryEnqueueRunnableLocked(QuerySlot &slot) {
        if (!slot.activated) {
            return;
        }
        if (slot.queued || slot.pending_tasks.empty()) {
            return;
        }
        if (slot.in_flight >= QueryInFlightLimitLocked()) {
            return;
        }
        if (slot.dispatch_budget == 0) {
            return;
        }
        slot.queued = true;
        PushRunnableQueryLocked(slot);
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
        while (total_in_flight_ < worker_count_) {
            if (!HasRunnableQueriesLocked() && !RefillDispatchBudgetsLocked()) {
                break;
            }
            if (!HasRunnableQueriesLocked()) {
                break;
            }

            auto query_id = PopRunnableQueryLocked();

            auto it = query_slots_.find(query_id);
            if (it == query_slots_.end()) {
                continue;
            }

            auto &slot = it->second;
            slot.queued = false;

            auto query_in_flight_limit = QueryInFlightLimitLocked();
            if (slot.pending_tasks.empty() || slot.in_flight >= query_in_flight_limit || slot.dispatch_budget == 0) {
                TryEnqueueRunnableLocked(slot);
                TryEraseSlotLocked(it);
                continue;
            }

            auto task = std::move(slot.pending_tasks.front());
            slot.pending_tasks.pop_front();
            --slot.dispatch_budget;
            ++slot.in_flight;
            ++total_in_flight_;
            TryEnqueueRunnableLocked(slot);

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
    std::deque<uint64_t> latency_sensitive_runnable_queries_;
    std::deque<uint64_t> normal_runnable_queries_;
    size_t total_in_flight_ = 0;
};

} // namespace dexkit

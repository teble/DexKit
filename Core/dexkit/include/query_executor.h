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

#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>

#include "ThreadPool.h"
#include "query_scheduler.h"

namespace dexkit {

class ThreadPoolQueryExecutor;

enum class QueryExecutorMode : uint8_t {
    LegacyPerQuery = 0,
    SharedPool = 1,
};

class IQueryExecutor {
public:
    virtual ~IQueryExecutor() = default;
    virtual void Submit(std::function<void()> task) = 0;
    virtual void OnSubmissionComplete() = 0;
    [[nodiscard]] virtual bool ShouldSkipTask() const = 0;
    [[nodiscard]] virtual std::function<bool()> GetShouldSkipTaskFn() const = 0;
    [[nodiscard]] virtual ThreadPoolQueryExecutor *GetThreadPoolQueryExecutor() { return nullptr; }
};

class ThreadPoolQueryExecutor final : public IQueryExecutor {
public:
    explicit ThreadPoolQueryExecutor(size_t thread_num, std::function<bool()> should_skip_task = {})
            : should_skip_task_(should_skip_task), pool_(thread_num, should_skip_task_) {}

    void Submit(std::function<void()> task) override {
        pool_.enqueue([task = std::move(task)]() mutable {
            task();
        });
    }

    void OnSubmissionComplete() override {}

    [[nodiscard]] bool ShouldSkipTask() const override {
        return should_skip_task_ && should_skip_task_();
    }

    [[nodiscard]] std::function<bool()> GetShouldSkipTaskFn() const override {
        return should_skip_task_;
    }

    [[nodiscard]] ThreadPoolQueryExecutor *GetThreadPoolQueryExecutor() override {
        return this;
    }

    template<typename F>
    auto Enqueue(F &&task) -> std::future<std::invoke_result_t<std::decay_t<F>>> {
        return pool_.enqueue(std::forward<F>(task));
    }

private:
    std::function<bool()> should_skip_task_;
    ThreadPool pool_;
};

class SharedThreadPoolQueryExecutor final : public IQueryExecutor {
public:
    explicit SharedThreadPoolQueryExecutor(
            std::shared_ptr<QueryScheduler> scheduler,
            uint64_t query_id,
            QueryContext &query_context,
            QueryPriority query_priority,
            std::function<bool()> should_skip_task = {}
    )
            : should_skip_task_(std::move(should_skip_task)),
              scheduler_(std::move(scheduler)),
              query_id_(query_id) {
        scheduler_->AttachQuery(query_id_, query_priority, &query_context);
    }

    ~SharedThreadPoolQueryExecutor() override {
        OnSubmissionComplete();
        scheduler_->DetachQuery(query_id_);
    }

    void Submit(std::function<void()> task) override {
        scheduler_->Submit(query_id_, std::move(task));
    }

    void OnSubmissionComplete() override {
        if (submission_completed_) {
            return;
        }
        submission_completed_ = true;
        scheduler_->ActivateQuery(query_id_);
    }

    [[nodiscard]] bool ShouldSkipTask() const override {
        return should_skip_task_ && should_skip_task_();
    }

    [[nodiscard]] std::function<bool()> GetShouldSkipTaskFn() const override {
        return should_skip_task_;
    }

private:
    std::function<bool()> should_skip_task_;
    std::shared_ptr<QueryScheduler> scheduler_;
    uint64_t query_id_ = 0;
    bool submission_completed_ = false;
};

template<typename ReturnType, typename F>
static auto BuildPackagedQueryTask(F &&task, std::function<bool()> should_skip_task) {
    return [task = std::decay_t<F>(std::forward<F>(task)), should_skip_task = std::move(should_skip_task)]() mutable -> ReturnType {
        if (should_skip_task && should_skip_task()) {
            if constexpr (!std::is_void_v<ReturnType>) {
                return ReturnType();
            } else {
                return;
            }
        }
        if constexpr (std::is_void_v<ReturnType>) {
            task();
            return;
        } else {
            return task();
        }
    };
}

template<typename F>
auto SubmitQueryTask(IQueryExecutor &executor, F &&task)
-> std::future<std::invoke_result_t<std::decay_t<F>>> {
    using ReturnType = std::invoke_result_t<std::decay_t<F>>;
    if (auto *thread_pool_executor = executor.GetThreadPoolQueryExecutor(); thread_pool_executor != nullptr) {
        return thread_pool_executor->Enqueue(std::forward<F>(task));
    }

    auto task_ptr = std::make_shared<std::packaged_task<ReturnType()>>(
            BuildPackagedQueryTask<ReturnType>(std::forward<F>(task), executor.GetShouldSkipTaskFn())
    );
    auto future = task_ptr->get_future();
    executor.Submit([task_ptr]() mutable {
        (*task_ptr)();
    });
    return future;
}

} // namespace dexkit

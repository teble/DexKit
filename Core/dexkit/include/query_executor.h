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

namespace dexkit {

enum class QueryExecutorMode : uint8_t {
    LegacyPerQuery = 0,
    SharedPool = 1,
};

class IQueryExecutor {
public:
    virtual ~IQueryExecutor() = default;
    virtual void Submit(std::function<void()> task) = 0;
    [[nodiscard]] virtual bool ShouldSkipTask() const = 0;
};

class ThreadPoolQueryExecutor final : public IQueryExecutor {
public:
    explicit ThreadPoolQueryExecutor(size_t thread_num, std::function<bool()> should_skip_task = {})
            : should_skip_task_(std::move(should_skip_task)), pool_(thread_num) {}

    void Submit(std::function<void()> task) override {
        pool_.enqueue([task = std::move(task)]() mutable {
            task();
        });
    }

    [[nodiscard]] bool ShouldSkipTask() const override {
        return should_skip_task_ && should_skip_task_();
    }

private:
    std::function<bool()> should_skip_task_;
    ThreadPool pool_;
};

class SharedThreadPoolQueryExecutor final : public IQueryExecutor {
public:
    explicit SharedThreadPoolQueryExecutor(std::shared_ptr<ThreadPool> pool, std::function<bool()> should_skip_task = {})
            : should_skip_task_(std::move(should_skip_task)), pool_(std::move(pool)) {}

    void Submit(std::function<void()> task) override {
        pool_->enqueue([task = std::move(task)]() mutable {
            task();
        });
    }

    [[nodiscard]] bool ShouldSkipTask() const override {
        return should_skip_task_ && should_skip_task_();
    }

private:
    std::function<bool()> should_skip_task_;
    std::shared_ptr<ThreadPool> pool_;
};

template<typename F>
auto SubmitQueryTask(IQueryExecutor &executor, F &&task)
-> std::future<std::invoke_result_t<std::decay_t<F>>> {
    using ReturnType = std::invoke_result_t<std::decay_t<F>>;
    auto promise = std::make_shared<std::promise<ReturnType>>();
    auto future = promise->get_future();
    executor.Submit([task = std::decay_t<F>(std::forward<F>(task)), promise, &executor]() mutable {
        if (executor.ShouldSkipTask()) {
            if constexpr (std::is_void_v<ReturnType>) {
                promise->set_value();
            } else {
                promise->set_value(ReturnType());
            }
            return;
        }

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try {
#endif
            if constexpr (std::is_void_v<ReturnType>) {
                task();
                promise->set_value();
            } else {
                promise->set_value(task());
            }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
#endif
    });
    return future;
}

} // namespace dexkit

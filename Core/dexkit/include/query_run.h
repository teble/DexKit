#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "query_executor.h"

namespace dexkit::internal {

// Owns one query's executor until all accepted callable captures have been
// released. The shared state deliberately contains no QueryContext pointer.
// Only the coordinating caller submits, activates and seals this run.
class QueryRun final : public IQueryExecutor {
    struct State {
        std::atomic<size_t> captures = 0;
        std::mutex mutex;
        std::condition_variable changed;
        std::exception_ptr error;

        void RecordError(std::exception_ptr value) noexcept {
            std::lock_guard lock(mutex);
            if (!error) error = std::move(value);
        }

        void Release() noexcept {
            if (captures.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard lock(mutex);
                changed.notify_all();
            }
        }

        void Drain() noexcept {
            std::unique_lock lock(mutex);
            changed.wait(lock, [&] { return captures.load(std::memory_order_acquire) == 0; });
        }
    };

    struct Task {
        std::shared_ptr<State> state;
        std::function<void()> body;
        bool finished = false;

        Task(std::shared_ptr<State> owner, std::function<void()> function)
                : state(std::move(owner)), body(std::move(function)) {
            state->captures.fetch_add(1, std::memory_order_relaxed);
        }
        ~Task() { Finish(); }

        void Finish() noexcept {
            if (finished) return;
            // Executors may keep copies of their wrapper after invocation.
            // Clear the common body before releasing the query's inputs.
            body = {};
            finished = true;
            state->Release();
        }

        void operator()() noexcept {
            try {
                body();
            } catch (...) {
                state->RecordError(std::current_exception());
            }
            Finish();
        }
    };

public:
    explicit QueryRun(std::unique_ptr<IQueryExecutor> executor)
            : state_(std::make_shared<State>()), executor_(std::move(executor)) {}
    QueryRun(const QueryRun &) = delete;
    QueryRun &operator=(const QueryRun &) = delete;

    ~QueryRun() override {
        Seal();
        state_->Drain();
        // Detach while the caller's context and query inputs are still alive.
        // Scheduler bookkeeping can finish later without borrowed captures.
        executor_.reset();
    }

    void Activate() {
        if (active_) return;
        // Activate before accepting any tasks, so cleanup never has to start
        // a partially submitted query during stack unwinding.
        executor_->OnSubmissionComplete();
        active_ = true;
    }

    void Submit(std::function<void()> task) override {
        if (!active_ || sealed_) throw std::logic_error("QueryRun is not accepting tasks");
        try {
            auto tracked = std::make_shared<Task>(state_, std::move(task));
            executor_->Submit([tracked = std::move(tracked)] { (*tracked)(); });
        } catch (...) {
            state_->RecordError(std::current_exception());
            throw;
        }
    }

    // A promise's shared state stores the result, not the callable. Unlike a
    // packaged_task, retaining a completed future cannot retain its inputs.
    template<class F>
    auto SubmitFuture(F &&function) -> std::future<std::invoke_result_t<std::decay_t<F>>> {
        using Function = std::decay_t<F>;
        using Result = std::invoke_result_t<Function>;
        auto promise = std::make_shared<std::promise<Result>>();
        auto future = promise->get_future();
        auto callable = std::make_shared<std::optional<Function>>(std::forward<F>(function));
        Submit([promise = std::move(promise), callable = std::move(callable)]() mutable {
            try {
                auto invoke = [&]() -> Result {
                    Function local(std::move(**callable));
                    callable->reset();
                    if constexpr (std::is_void_v<Result>) local();
                    else return local();
                };
                if constexpr (std::is_void_v<Result>) {
                    invoke();
                    promise->set_value();
                } else {
                    promise->set_value(invoke());
                }
            } catch (...) {
                callable->reset();
                promise->set_exception(std::current_exception());
            }
        });
        return future;
    }

    void OnSubmissionComplete() override { Activate(); }
    void Seal() noexcept { sealed_ = true; }
    void Drain() {
        if (!sealed_) throw std::logic_error("Seal QueryRun before draining it");
        state_->Drain();
        if (state_->error) std::rethrow_exception(state_->error);
    }

    [[nodiscard]] size_t OutstandingCaptures() const {
        return state_->captures.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool ShouldSkipTask() const override { return executor_->ShouldSkipTask(); }
    [[nodiscard]] std::function<bool()> GetShouldSkipTaskFn() const override {
        return executor_->GetShouldSkipTaskFn();
    }

private:
    std::shared_ptr<State> state_;
    std::unique_ptr<IQueryExecutor> executor_;
    bool active_ = false;
    bool sealed_ = false;
};

} // namespace dexkit::internal

#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "query_candidates.h"
#include "query_executor.h"

namespace dexkit::internal {

// Ordinary candidate tasks have no early-exit callback. Move their owning
// captures onto the worker stack so cleanup precedes future readiness, even
// when the executor or a shared future retains the completed packaged_task.
template<class F>
auto SubmitCandidateTask(IQueryExecutor &executor, F &&function) noexcept
        -> std::future<std::invoke_result_t<std::decay_t<F>>> {
    using Function = std::decay_t<F>;
    using Result = std::invoke_result_t<Function>;
    auto task = std::make_shared<std::packaged_task<Result()>>(
            [function = std::optional<Function>(std::forward<F>(function))]() mutable noexcept -> Result {
                Function local(std::move(*function));
                function.reset();
                if constexpr (std::is_void_v<Result>) local();
                else return local();
            });
    auto future = task->get_future();
    executor.Submit([task = std::move(task)] { (*task)(); });
    return future;
}

struct CandidateExecutionOptions {
    size_t working_bytes = 64 * 1024 * 1024;
    size_t preparation_window = 1;
};

class CandidateTaskScope {
public:
    explicit CandidateTaskScope(QueryContext &context)
            : context_(context), binding_(context.BindToCurrentThread()), execution_(context.TrackTaskExecution()) {}
    ~CandidateTaskScope() { context_.MarkTaskCompleted(); }
private:
    QueryContext &context_;
    QueryContext::ScopedBinding binding_;
    QueryContext::TaskExecutionScope execution_;
};

template<class Bean> struct CandidateOutcome {
    enum class Kind { Completed, Prepared };
    Kind kind;
    std::vector<Bean> result;
    std::shared_ptr<const PreparedCandidates> candidates;
    explicit CandidateOutcome(std::vector<Bean> value) : kind(Kind::Completed), result(std::move(value)) {}
    explicit CandidateOutcome(std::shared_ptr<const PreparedCandidates> value)
            : kind(Kind::Prepared), candidates(std::move(value)) {
        if (!candidates) std::abort();
    }
};

// The producer and matcher callbacks share no mutable candidate data. The
// legacy callback must submit original ranges using the supplied frozen plan;
// force_range requests a producer rejection fallback, with no string proof.
template<class Bean, class Legacy, class Prepare, class Match>
std::vector<Bean> RunCandidatePipeline(const std::vector<CandidateSource> &sources,
        std::unique_ptr<IQueryExecutor> executor, QueryContext &context,
        Legacy &&legacy, Prepare &&prepare, Match &&match, CandidateExecutionOptions options = {}) noexcept {
    using Outcome = CandidateOutcome<Bean>;
    struct Slot {
        std::future<Outcome> preparation;
        std::vector<std::future<std::vector<Bean>>> validation;
        std::vector<Bean> result;
    };
    CandidateBudget budget(options.working_bytes);
    std::vector<Slot> slots(sources.size());
    std::vector<size_t> producers;
    std::deque<size_t> pending;
    size_t next = 0, unresolved = 0;
    size_t prepare_tasks = 0, validate_tasks = 0, inline_dexes = 0, fallback_dexes = 0;

    auto append = [](std::vector<Bean> &to, std::vector<Bean> from) {
        to.insert(to.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end()));
    };
    auto consume = [&](Slot &slot) {
        for (auto &future : slot.validation) append(slot.result, future.get());
        slot.validation.clear();
    };

    // Dynamic preparation needs an active executor before the first window.
    executor->OnSubmissionComplete();
    for (size_t i = 0; i < sources.size(); ++i) {
        const auto &source = sources[i];
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        std::fprintf(stderr, "BENCH_CANDIDATE_SOURCE query=%llu dex=%u classes=%d route=%d reason=%d ready=%d postings=%zu width=%u split=%d request=%zu\n",
                static_cast<unsigned long long>(source.domain.query_id), source.domain.dex_id,
                source.domain.entity == CandidateEntity::Class, int(source.strings.route), int(source.strings.reason),
                source.strings.index_ready, source.strings.postings, source.slice_width, source.split, source.work_bytes);
#endif
        if (!source.strings.Admitted()) {
            slots[i].validation = legacy(source, *executor, false);
            validate_tasks += slots[i].validation.size();
        } else if (source.strings.route != inverted_string::QueryPlan::Route::Empty) {
            producers.push_back(i);
        }
    }
    unresolved = producers.size();
    auto mark_submission_if_done = [&] {
        if (!unresolved) {
            context.MarkSubmissionCompleted();
        }
    };
    auto submit_window = [&] {
        const auto window = std::max<size_t>(1, options.preparation_window);
        while (next < producers.size() && pending.size() < window) {
            const auto i = producers[next];
            const auto &source = sources[i];
            if (source.work_bytes > budget.Limit()) {
                slots[i].validation = legacy(source, *executor, true);
                validate_tasks += slots[i].validation.size();
                ++fallback_dexes;
                ++next;
                --unresolved;
                continue;
            }
            auto reservation = budget.TryReserve(source.work_bytes);
            if (!reservation) break; // Consume earlier work; never block a worker on credit.
            context.MarkTaskSubmitted();
            slots[i].preparation = SubmitCandidateTask(*executor, [&, source, reservation = std::move(*reservation)]() mutable {
                CandidateTaskScope task_scope(context);
                auto candidates = prepare(source, std::move(reservation));
                if (!candidates) std::abort();
                if (candidates->kind == CandidateView::Kind::FullRange) return Outcome(std::move(candidates));
                if (!source.split || candidates->slices.size() <= 1) {
                    const auto slice = candidates->slices.empty() ? CandidateSlice{0, source.domain.count}
                            : candidates->slices.front();
                    return Outcome(match(source, *candidates, slice));
                }
                return Outcome(std::move(candidates));
            });
            pending.push_back(i);
            ++prepare_tasks;
            ++next;
        }
        mark_submission_if_done();
    };

    submit_window();
    while (unresolved) {
        if (pending.empty()) std::abort(); // All reservation owners must belong to this window.
        const auto i = pending.front();
        pending.pop_front();
        auto outcome = slots[i].preparation.get();
        const auto &source = sources[i];
        if (outcome.kind == Outcome::Kind::Completed) {
            slots[i].result = std::move(outcome.result);
            ++inline_dexes;
        } else if (outcome.candidates->kind == CandidateView::Kind::FullRange) {
            slots[i].validation = legacy(source, *executor, true);
            validate_tasks += slots[i].validation.size();
            ++fallback_dexes;
        } else {
            slots[i].validation.reserve(outcome.candidates->slices.size());
            for (auto slice : outcome.candidates->slices) {
                context.MarkTaskSubmitted();
                slots[i].validation.push_back(SubmitCandidateTask(*executor, [&, source, slice, candidates = outcome.candidates] {
                    CandidateTaskScope task_scope(context);
                    return match(source, *candidates, slice);
                }));
                ++validate_tasks;
            }
        }
        --unresolved;
        mark_submission_if_done();
        consume(slots[i]);
        outcome.candidates.reset();
        submit_window();
    }
    for (auto &slot : slots) consume(slot);
    // All task bodies and owning candidate captures have completed. Detach
    // scheduler bookkeeping while the caller's query context is still alive.
    executor.reset();
    context.MarkWorkersCompleted();
    std::vector<Bean> result;
    for (auto &slot : slots) append(result, std::move(slot.result));
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    std::fprintf(stderr, "BENCH_CANDIDATE_PIPELINE query=%llu prepare=%zu validate=%zu inline=%zu fallback=%zu peak_reserved=%zu remaining_reserved=%zu\n",
            static_cast<unsigned long long>(context.GetQueryId()), prepare_tasks, validate_tasks, inline_dexes,
            fallback_dexes, budget.Peak(), budget.Used());
#endif
    return result;
}

} // namespace dexkit::internal

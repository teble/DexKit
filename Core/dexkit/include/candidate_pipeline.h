#pragma once

#include <algorithm>
#include <cstdio>
#include <deque>
#include <future>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "query_candidates.h"
#include "query_run.h"

namespace dexkit::internal {

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
        if (!candidates) throw std::logic_error("Missing prepared candidates");
    }
};

// The producer and matcher callbacks share no mutable candidate data. The
// legacy callback must submit original ranges using the supplied frozen plan;
// force_range requests a producer rejection fallback, with no string proof.
template<class Bean, class Legacy, class Prepare, class Match>
std::vector<Bean> RunCandidatePipeline(const std::vector<CandidateSource> &sources,
        std::unique_ptr<IQueryExecutor> executor, QueryContext &context,
        Legacy &&legacy, Prepare &&prepare, Match &&match, CandidateExecutionOptions options = {}) {
    using Outcome = CandidateOutcome<Bean>;
    struct Slot {
        std::future<Outcome> preparation;
        std::vector<std::future<std::vector<Bean>>> validation;
        std::vector<Bean> result;
    };
    // Declare the run before every future and result: their owners disappear
    // before its exceptional cleanup waits for borrowed task inputs.
    QueryRun run(std::move(executor));
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
        // On failure, QueryRun still drains work whose future was not reached.
        for (auto &future : slot.validation) append(slot.result, future.get());
        slot.validation.clear();
    };

    // Selection was frozen before activation. No work is accepted until the
    // backend has started successfully, including on the exception path.
    run.Activate();
    for (size_t i = 0; i < sources.size(); ++i) {
        const auto &source = sources[i];
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        std::fprintf(stderr, "BENCH_CANDIDATE_SOURCE query=%llu dex=%u classes=%d route=%d reason=%d ready=%d postings=%zu width=%u split=%d request=%zu\n",
                static_cast<unsigned long long>(source.domain.query_id), source.domain.dex_id,
                source.domain.entity == CandidateEntity::Class, int(source.strings.route), int(source.strings.reason),
                source.strings.index_ready, source.strings.postings, source.slice_width, source.split, source.work_bytes);
#endif
        if (!source.strings.Admitted()) {
            slots[i].validation = legacy(source, run, false);
            validate_tasks += slots[i].validation.size();
        } else if (source.strings.route != inverted_string::QueryPlan::Route::Empty) {
            producers.push_back(i);
        }
    }
    unresolved = producers.size();
    auto seal_if_done = [&] {
        if (!unresolved) {
            run.Seal();
            context.MarkSubmissionCompleted();
        }
    };
    auto submit_window = [&] {
        const auto window = std::max<size_t>(1, options.preparation_window);
        while (next < producers.size() && pending.size() < window) {
            const auto i = producers[next];
            const auto &source = sources[i];
            if (source.work_bytes > budget.Limit()) {
                slots[i].validation = legacy(source, run, true);
                validate_tasks += slots[i].validation.size();
                ++fallback_dexes;
                ++next;
                --unresolved;
                continue;
            }
            auto reservation = budget.TryReserve(source.work_bytes);
            if (!reservation) break; // Consume earlier work; never block a worker on credit.
            context.MarkTaskSubmitted();
            slots[i].preparation = run.SubmitFuture([&, source, reservation = std::move(*reservation)]() mutable {
                CandidateTaskScope task_scope(context);
                auto candidates = prepare(source, std::move(reservation));
                if (!candidates) throw std::logic_error("Missing prepared candidates");
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
        seal_if_done();
    };

    submit_window();
    while (unresolved) {
        if (pending.empty()) throw std::logic_error("Candidate reservation outlived its preparation window");
        const auto i = pending.front();
        pending.pop_front();
        auto outcome = slots[i].preparation.get();
        const auto &source = sources[i];
        if (outcome.kind == Outcome::Kind::Completed) {
            slots[i].result = std::move(outcome.result);
            ++inline_dexes;
        } else if (outcome.candidates->kind == CandidateView::Kind::FullRange) {
            slots[i].validation = legacy(source, run, true);
            validate_tasks += slots[i].validation.size();
            ++fallback_dexes;
        } else {
            slots[i].validation.reserve(outcome.candidates->slices.size());
            for (auto slice : outcome.candidates->slices) {
                context.MarkTaskSubmitted();
                slots[i].validation.push_back(run.SubmitFuture([&, source, slice, candidates = outcome.candidates] {
                    CandidateTaskScope task_scope(context);
                    return match(source, *candidates, slice);
                }));
                ++validate_tasks;
            }
        }
        --unresolved;
        seal_if_done();
        consume(slots[i]);
        outcome.candidates.reset();
        submit_window();
    }
    for (auto &slot : slots) consume(slot);
    run.Seal();
    run.Drain();
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

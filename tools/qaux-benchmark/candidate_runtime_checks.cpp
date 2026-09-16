#include "query_candidates.h"
#include "query_run.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace dexkit;
using namespace dexkit::internal;

static void Check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

// Keep the invoked wrapper alive after its result is ready. This models a
// worker's closure tail and catches cleanup which relies only on future.get().
class RetainingExecutor final : public IQueryExecutor {
public:
    std::vector<std::thread> workers;
    std::vector<std::shared_ptr<std::function<void()>>> retained;
    bool activated = false;
    size_t reject_at = SIZE_MAX;
    bool reject_after_accept = false;

    ~RetainingExecutor() override {
        for (auto &worker : workers) worker.join();
    }
    void Submit(std::function<void()> task) override {
        Check(activated, "submit before activation");
        const bool reject = retained.size() == reject_at;
        if (reject && !reject_after_accept) throw std::runtime_error("rejected before acceptance");
        auto holder = std::make_shared<std::function<void()>>(std::move(task));
        retained.push_back(holder);
        workers.emplace_back([holder] { (*holder)(); });
        if (reject) throw std::runtime_error("rejected after acceptance");
    }
    void OnSubmissionComplete() override { activated = true; }
    bool ShouldSkipTask() const override { return false; }
    std::function<bool()> GetShouldSkipTaskFn() const override { return {}; }
};

struct CaptureProbe {
    std::atomic<unsigned> &destroyed;
    ~CaptureProbe() { destroyed.fetch_add(1); }
};

static void CheckCaptureCleanup() {
    std::atomic<unsigned> destroyed = 0;
    auto executor = std::make_unique<RetainingExecutor>();
    auto *retained = executor.get();
    QueryRun run(std::move(executor));
    run.Activate();
    auto probe = std::make_shared<CaptureProbe>(destroyed);
    auto future = run.SubmitFuture([probe] { return 17; }).share();
    probe.reset();
    Check(future.get() == 17, "result changed");
    run.Seal();
    run.Drain();
    Check(destroyed.load() == 1, "completed shared future retained callable captures");
    Check(retained->retained.size() == 1, "test did not retain worker closure");
    Check(run.OutstandingCaptures() == 0, "completed work still registered");
    bool rejected = false;
    try { run.Submit([] {}); } catch (const std::logic_error &) { rejected = true; }
    Check(rejected, "submission after seal accepted");
}

static void CheckEarlyFuture() {
    QueryRun run(std::make_unique<RetainingExecutor>());
    run.Activate();
    std::promise<void> ready, release;
    auto ready_future = ready.get_future();
    auto gate = release.get_future().share();
    std::atomic<unsigned> destroyed = 0;
    auto probe = std::make_shared<CaptureProbe>(destroyed);
    run.Submit([probe, &ready, gate] { ready.set_value(); gate.wait(); });
    probe.reset();
    ready_future.get();
    Check(run.OutstandingCaptures() != 0 && destroyed.load() == 0, "future readiness ended task lifetime");
    release.set_value();
    run.Seal();
    run.Drain();
    Check(destroyed.load() == 1, "capture cleanup not drained");
}

static void CheckFailures() {
    for (bool after : {false, true}) {
        std::atomic<unsigned> finished = 0;
        auto executor = std::make_unique<RetainingExecutor>();
        executor->reject_at = 1;
        executor->reject_after_accept = after;
        {
            QueryRun run(std::move(executor));
            run.Activate();
            auto first = run.SubmitFuture([&] { ++finished; throw std::runtime_error("prepare failed"); });
            bool rejected = false;
            try { run.SubmitFuture([&] { ++finished; }); } catch (const std::runtime_error &) { rejected = true; }
            Check(rejected, "submission failure not reported");
            bool threw = false;
            try { first.get(); } catch (const std::runtime_error &) { threw = true; }
            Check(threw, "prepare exception converted to success");
            run.Seal();
            try { run.Drain(); } catch (const std::runtime_error &) {}
        }
        Check(finished.load() == (after ? 2u : 1u), "accepted task escaped exception cleanup");
    }
}

static void CheckBudget() {
    CandidateBudget budget(100);
    auto first = budget.TryReserve(70);
    Check(first.has_value() && budget.Used() == 70, "reservation missing");
    Check(!budget.TryReserve(31), "budget exceeded");
    auto retained = std::make_shared<PreparedCandidates>();
    retained->reservation = std::move(*first);
    auto copy = retained;
    retained.reset();
    Check(budget.Used() == 70, "shared candidate released reservation early");
    copy->reservation.Shrink(20);
    Check(budget.Used() == 20 && budget.Peak() == 70, "reservation shrink incorrect");
    auto second = budget.TryReserve(80);
    Check(second.has_value() && !budget.TryReserve(1), "budget reuse incorrect");
    copy.reset();
    second.reset();
    Check(budget.Used() == 0, "reservation leaked");
    CandidateBudget::Lease surviving;
    {
        CandidateBudget temporary(1);
        surviving = std::move(*temporary.TryReserve(1));
    }
    surviving.Reset();
}

static void CheckSlices() {
    for (size_t count : {size_t(0), size_t(1), size_t(64), size_t(65), size_t(4505)}) {
        PreparedCandidates prepared;
        prepared.domain.count = static_cast<uint32_t>(count);
        prepared.kind = CandidateView::Kind::Bitmap;
        prepared.bits = inverted_string::Bits(count);
        std::vector<uint32_t> expected;
        for (uint32_t id = 0; id < count; ++id) if (id % 31 == 0 || id % 1000 == 999) {
            prepared.bits.Set(id);
            expected.push_back(id);
        }
        prepared.MakeSlices(1000);
        std::vector<uint32_t> actual;
        for (auto slice : prepared.slices) prepared.View().Each(slice, [&](uint32_t id) { actual.push_back(id); });
        Check(actual == expected, "slices duplicated, omitted or reordered IDs");
        for (size_t lo = 0; lo <= count; lo += 13) {
            const auto hi = std::min(count, lo + 137);
            actual.clear();
            prepared.bits.EachRange(lo, hi, [&](uint32_t id) { actual.push_back(id); });
            const std::vector<uint32_t> subset(std::lower_bound(expected.begin(), expected.end(), lo),
                    std::lower_bound(expected.begin(), expected.end(), hi));
            Check(actual == subset, "word-boundary enumeration changed");
        }
    }
    PreparedCandidates classes;
    classes.domain = {nullptr, 0, CandidateEntity::Class, 2200};
    classes.kind = CandidateView::Kind::Ids;
    classes.ids = {7, 900, 2100};
    classes.MakeSlices(500);
    Check(classes.slices.size() == 3 && classes.slices[1].begin == 500
            && classes.slices[2].begin == 2000, "ClassDef ranges collapsed");
    PreparedCandidates fallback;
    fallback.domain.count = 4505;
    fallback.MakeSlices(1000);
    Check(fallback.slices.size() == 5 && fallback.slices.back().end == 4505, "fallback did not restore original ranges");
}

int main() {
    CheckCaptureCleanup();
    CheckEarlyFuture();
    CheckFailures();
    CheckBudget();
    CheckSlices();
    std::cout << "CANDIDATE_RUNTIME_OK cleanup failures budget ranges\n";
}

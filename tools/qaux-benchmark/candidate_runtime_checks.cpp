#include "candidate_pipeline.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

using namespace dexkit;
using namespace dexkit::internal;

static void Check(bool value, const char *message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}

// Keep the invoked wrapper alive after its result is ready. This models a
// worker's closure tail and catches cleanup which relies only on future.get().
class RetainingExecutor final : public IQueryExecutor {
public:
    std::vector<std::thread> workers;
    std::vector<std::shared_ptr<std::function<void()>>> retained;
    bool activated = false;

    ~RetainingExecutor() override {
        for (auto &worker : workers) worker.join();
    }
    void Submit(std::function<void()> task) override {
        Check(activated, "submit before activation");
        auto holder = std::make_shared<std::function<void()>>(std::move(task));
        retained.push_back(holder);
        workers.emplace_back([holder] { (*holder)(); });
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
    RetainingExecutor executor;
    executor.OnSubmissionComplete();
    auto probe = std::make_shared<CaptureProbe>(destroyed);
    auto future = SubmitCandidateTask(executor, [probe] { return 17; }).share();
    probe.reset();
    Check(future.get() == 17, "result changed");
    Check(destroyed.load() == 1, "completed shared future retained callable captures");
    Check(executor.retained.size() == 1, "test did not retain worker closure");

    CandidateBudget budget(1);
    auto complete = SubmitCandidateTask(executor, [lease = std::move(*budget.TryReserve(1))] {}).share();
    complete.get();
    Check(budget.Used() == 0, "completed void future retained move-only reservation");
    Check(executor.retained.size() == 2, "test did not retain void worker closure");
}

static void CheckEarlyFuture() {
    RetainingExecutor executor;
    executor.OnSubmissionComplete();
    std::promise<void> ready, release;
    auto ready_future = ready.get_future();
    auto gate = release.get_future().share();
    std::atomic<unsigned> destroyed = 0;
    auto probe = std::make_shared<CaptureProbe>(destroyed);
    auto future = SubmitCandidateTask(executor, [probe, &ready, gate] { ready.set_value(); gate.wait(); });
    probe.reset();
    ready_future.get();
    Check(future.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout
            && destroyed.load() == 0, "task result published before completion");
    release.set_value();
    future.get();
    Check(destroyed.load() == 1, "capture cleanup outlived task result");
}

static void CheckBudget() {
    // Two million keywords over one entity pass the older bitmap-only cap.
    // Their plane headers and last_string array must exceed the query cap.
    constexpr size_t keywords = 2'000'000;
    const auto bitmaps = inverted_string::BitmapPlanBytes(1, 1, keywords, 1);
    Check(bitmaps.has_value(), "counterexample did not pass bitmap admission");
    const auto arrays = CandidateArrayBytes(*bitmaps, keywords, 0, 0, 0);
    CandidateBudget query_budget(64 * 1024 * 1024);
    Check(arrays && !query_budget.TryReserve(*arrays), "keyword metadata escaped array budget");
    Check(!CandidateArrayBytes(0, SIZE_MAX, 0, 0, 0), "keyword accounting overflowed");
    Check(!CandidateArrayBytes(SIZE_MAX, 0, 0, 0, 0), "bitmap accounting overflowed");
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
    CheckBudget();
    CheckSlices();
    std::cout << "CANDIDATE_RUNTIME_OK cleanup completion budget ranges\n";
}

#include "candidate_pipeline.h"

#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>

using namespace dexkit;
using namespace dexkit::internal;

static void Check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

// A bounded test executor. Production scheduler integration is covered by the
// public-query checks; here gates force failure/cleanup interleavings exactly.
class TestExecutor final : public IQueryExecutor {
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> pending_;
    std::vector<std::thread> workers_;
    bool stop_ = false, active_ = false;
public:
    explicit TestExecutor(unsigned count) {
        for (unsigned i = 0; i < count; ++i) workers_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock lock(mutex_);
                    ready_.wait(lock, [&] { return stop_ || !pending_.empty(); });
                    if (pending_.empty()) return;
                    task = std::move(pending_.front());
                    pending_.pop_front();
                }
                task();
            }
        });
    }
    ~TestExecutor() override {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        ready_.notify_all();
        for (auto &worker : workers_) worker.join();
    }
    void Submit(std::function<void()> task) override {
        Check(active_, "inactive test executor");
        {
            std::lock_guard lock(mutex_);
            pending_.push_back(std::move(task));
        }
        ready_.notify_one();
    }
    void OnSubmissionComplete() override { active_ = true; }
    bool ShouldSkipTask() const override { return false; }
    std::function<bool()> GetShouldSkipTaskFn() const override { return {}; }
};

static const std::vector<uint32_t> wide{1, 999, 1001, 2001, 3001, 4001, 4100};
static const std::vector<uint32_t> narrow{1, 999};
static const std::vector<uint32_t> empty;
static int matcher_identity;

static const std::vector<uint32_t> &References(uint32_t dex) {
    if (dex == 2) return narrow;
    if (dex == 4) return empty;
    return wide;
}

static bool Expected(uint32_t dex, uint32_t id) {
    const auto &references = References(dex);
    return id % 1000 == 1 && std::binary_search(references.begin(), references.end(), id);
}

struct Fixture {
    QueryContext context{QueryKind::FindMethod};
    std::vector<CandidateSource> sources;
    std::atomic<size_t> fallback_tasks = 0, validated = 0;
    std::function<void(uint32_t)> before_prepare;
    std::function<void(uint32_t, CandidateSlice)> before_match;

    Fixture(bool split, size_t count = 6) {
        for (uint32_t dex = 0; dex < count; ++dex) {
            CandidateSource source;
            source.domain = {reinterpret_cast<const void *>(uintptr_t(dex) + 1), context.GetQueryId(),
                    CandidateEntity::Method, 4505, dex};
            source.strings.route = dex == 3 ? inverted_string::QueryPlan::Route::Legacy
                    : dex == 4 ? inverted_string::QueryPlan::Route::Empty
                    : inverted_string::QueryPlan::Route::Keywords;
            source.root_matchers = &matcher_identity;
            source.work_bytes = 4096;
            source.slice_width = 1000;
            source.split = split;
            sources.push_back(source);
        }
    }

    std::vector<uint64_t> Run(unsigned workers, size_t budget = 8192) {
        auto prepare = [&](const CandidateSource &source, CandidateBudget::Lease lease) {
            Check(QueryContext::Current() == &context, "producer lost query binding");
            if (before_prepare) before_prepare(source.domain.dex_id);
            auto value = std::make_shared<PreparedCandidates>();
            value->domain = source.domain;
            value->reservation = std::move(lease);
            if (source.domain.dex_id == 1) {
                value->reservation.Reset();
                return std::shared_ptr<const PreparedCandidates>(value);
            }
            value->kind = CandidateView::Kind::Bitmap;
            // DEX 5 is a necessary seed only, without a complete root proof.
            if (source.domain.dex_id != 5) value->root_matchers = source.root_matchers;
            value->bits = inverted_string::Bits(source.domain.count);
            for (auto id : References(source.domain.dex_id)) value->bits.Set(id);
            if (source.split) value->MakeSlices(source.slice_width);
            Check(value->RetainedBytes() <= value->reservation.Bytes(), "test exceeded reservation");
            value->reservation.Shrink(value->RetainedBytes());
            return std::shared_ptr<const PreparedCandidates>(value);
        };
        auto match = [&](const CandidateSource &source, const PreparedCandidates &prepared, CandidateSlice slice) {
            Check(QueryContext::Current() == &context, "consumer lost query binding");
            Check(prepared.domain.query_id == context.GetQueryId() && prepared.domain.dex == source.domain.dex,
                    "wrong candidate identity");
            if (before_match) before_match(source.domain.dex_id, slice);
            ++validated;
            if (const auto *truth = prepared.RootTruth()) {
                Check(truth->Has(4001) == (source.domain.dex_id != 2), "slice truncated root truth");
                Check(truth->Has(4100) == (source.domain.dex_id != 2), "residual filter truncated root truth");
            }
            std::vector<uint64_t> out;
            prepared.View().Each(slice, [&](uint32_t id) {
                if (Expected(source.domain.dex_id, id)) out.push_back((uint64_t(source.domain.dex_id) << 32) | id);
            });
            return out;
        };
        auto legacy = [&](const CandidateSource &source, IQueryExecutor &executor, bool fallback) {
            std::vector<std::future<std::vector<uint64_t>>> out;
            for (uint32_t begin = 0; begin < source.domain.count; begin += source.slice_width) {
                CandidateSlice slice{begin, std::min(source.domain.count, begin + source.slice_width)};
                if (fallback) ++fallback_tasks;
                context.MarkTaskSubmitted();
                out.push_back(SubmitQueryTask(executor, [&, source, slice] {
                    CandidateTaskScope task_scope(context);
                    PreparedCandidates all;
                    all.domain = source.domain;
                    return match(source, all, slice);
                }));
            }
            return out;
        };
        return RunCandidatePipeline<uint64_t>(sources, std::make_unique<TestExecutor>(workers), context,
                legacy, prepare, match, CandidateExecutionOptions{budget, 2});
    }

    void CheckResult(const std::vector<uint64_t> &actual) const {
        std::vector<uint64_t> expected;
        for (const auto &source : sources) for (uint32_t id = 0; id < source.domain.count; ++id) {
            if (Expected(source.domain.dex_id, id)) expected.push_back((uint64_t(source.domain.dex_id) << 32) | id);
        }
        Check(actual == expected, "candidate execution changed ordered results");
    }
};

static void CheckResults() {
    for (unsigned workers : {1u, 4u}) for (bool split : {false, true}) {
        Fixture fixture(split);
        fixture.CheckResult(fixture.Run(workers));
        Check(fixture.fallback_tasks == 5, "producer rejection did not restore five original slices");
    }
    Fixture small_budget(true);
    small_budget.CheckResult(small_budget.Run(1, 1));
    Check(small_budget.fallback_tasks == 20, "individual budget rejection was not a range fallback");
    Fixture a(true), b(true);
    auto first = std::async(std::launch::async, [&] { return a.Run(1, 4096); });
    auto second = std::async(std::launch::async, [&] { return b.Run(4, 4096); });
    a.CheckResult(first.get());
    b.CheckResult(second.get());
}

static void CheckFailure(bool during_validation) {
    Fixture fixture(true, 2);
    std::promise<void> entered, release;
    auto entered_future = entered.get_future();
    auto gate = release.get_future().share();
    if (during_validation) {
        // One DEX needs several consumer tasks. One fails while another still
        // borrows query data; failure must wait for the latter to finish.
        fixture.sources.resize(1);
        fixture.before_match = [&](uint32_t, CandidateSlice slice) {
            if (slice.begin == 0) throw std::runtime_error("validation failure");
            if (slice.begin == 1000) { entered.set_value(); gate.wait(); }
        };
    } else {
        fixture.before_prepare = [&](uint32_t dex) {
            if (dex == 0) throw std::runtime_error("preparation failure");
            entered.set_value();
            gate.wait();
        };
    }
    auto query = std::async(std::launch::async, [&] {
        try { fixture.Run(4); } catch (const std::runtime_error &) { return true; }
        return false;
    });
    entered_future.get();
    Check(query.wait_for(std::chrono::seconds(0)) == std::future_status::timeout, "error returned before live work drained");
    release.set_value();
    Check(query.get(), "pipeline exception disappeared");
}

int main() {
    CheckResults();
    CheckFailure(false);
    CheckFailure(true);
    std::cout << "CANDIDATE_PIPELINE_OK ordered split fallback proof budget failures workers=1,4\n";
}

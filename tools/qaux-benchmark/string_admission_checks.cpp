#include "string_admission_queries.h"
#include "benchmark_diagnostics.h"
#include "dex_item.h"
#include <latch>
#include <thread>

namespace {
using namespace string_admission_fixture;
void Require(bool value) { if (!value) std::abort(); }

class DeferredExecutor final : public IQueryExecutor {
public:
    void Submit(std::function<void()> task) override { tasks.push_back(std::move(task)); }
    void OnSubmissionComplete() override { for (auto &task : tasks) task(); }
    bool ShouldSkipTask() const override { return false; }
    std::function<bool()> GetShouldSkipTaskFn() const override { return {}; }
    std::vector<std::function<void()>> tasks;
};
}

void dexkit::BenchmarkDiagnostics::CheckStringAdmission(std::string_view apk) {
#if DEXKIT_EXPERIMENT_INVERTED_STRINGS
    using Plan = inverted_string::QueryPlan;
    using Route = Plan::Route;
    constexpr bool small = DEXKIT_EXPERIMENT_INVERTED_STRING_RANGES;
    size_t decisions = 0, scoped = 0, frozen = 0;
    for (bool warm : {false, true}) {
        DexKit bridge(apk, 1); bridge.SetThreadNum(4);
        auto guard = bridge.EnterQueryExecution(kUsingString);
        for (const auto &item : bridge.dex_items) {
            auto &dex = *item;
            Require(!dex.inverted_strings_ready.load(std::memory_order_acquire));
            if (warm) Require(dex.EnsureInvertedStrings());
            for (int variant = 0; variant < string_admission_fixture::QueryCount; ++variant) {
                for (bool absent : {false, true}) {
                    auto data = string_admission_fixture::Query(variant, absent);
                    const auto *query = flatbuffers::GetRoot<schema::FindMethod>(data->GetBufferPointer());
                    const auto plan = dex.PlanRootStringCandidates(query->matcher());
                    Route expected = Route::Legacy;
                    if (variant == 0) expected = Route::Keywords;
                    if (variant == 18) expected = absent ? Route::Empty : Route::Range;
                    if (small && warm && ((variant >= 7 && variant <= 13)
                            || (variant >= 15 && variant <= 17) || variant == 19 || variant == 22)) {
                        expected = absent || variant == 12 || variant == 13 ? Route::Empty
                            : variant >= 9 && variant <= 11 ? Route::Legacy : Route::Range;
                    }
                    Require(plan.route == expected);
                    if (small && warm && !absent && variant >= 9 && variant <= 11)
                        Require(plan.postings == 2 && plan.reason == Plan::Reason::PostingBound);
                    if (small && warm && !absent && variant == 7) Require(plan.postings == 1);
                    ++decisions;
                }
            }
            for (int variant = 0; variant < ClassQueryCount; ++variant) {
                for (bool absent : {false, true}) {
                    auto data = ClassQuery(variant, absent);
                    const auto *query = flatbuffers::GetRoot<schema::FindClass>(data->GetBufferPointer());
                    const auto plan = dex.PlanRootStringCandidates(query->matcher());
                    const auto expected = variant == 5 ? Route::Keywords
                        : small && warm ? absent ? Route::Empty : variant == 1 ? Route::Legacy : Route::Range
                        : Route::Legacy;
                    Require(plan.route == expected);
                    ++decisions;
                }
            }
            Require(dex.inverted_strings_ready.load(std::memory_order_acquire) == warm);
            for (int restriction = 1; restriction <= 5; ++restriction) {
                auto data = string_admission_fixture::Query(7, false, restriction);
                const auto *query = flatbuffers::GetRoot<schema::FindMethod>(data->GetBufferPointer());
                QueryContext context(QueryKind::FindMethod);
                if (restriction == 5) context.EnableEarlyExit();
                std::set<uint32_t> classes, methods;
                trie::PackageTrie packages;
                DeferredExecutor executor;
                auto futures = dex.FindMethod(query, classes, methods, packages, executor, 1000, context);
                Require(futures.size() == (dex.reader.MethodIds().size() + 999) / 1000);
                executor.OnSubmissionComplete();
                size_t found = 0;
                for (auto &future : futures) found += future.get().size();
                Require(found == (restriction >= 4 ? 1 : 0));
                ++scoped;
            }
            if (!warm && dex.reader.MethodIds().size() > 1000) {
                Builder data;
                const auto value = data.CreateString("Needle");
                const auto atom = schema::CreateStringMatcher(data, value, schema::StringMatchType::Contains);
                const auto words = data.CreateVector(std::vector(100000, atom));
                schema::MethodMatcherBuilder matcher(data); matcher.add_using_strings(words);
                data.Finish(schema::CreateFindMethod(data, 0, 0, false, 0, 0, false, matcher.Finish()));
                const auto plan = dex.PlanRootStringCandidates(
                        flatbuffers::GetRoot<schema::FindMethod>(data.GetBufferPointer())->matcher());
                Require(plan.route == Route::Legacy && plan.reason == Plan::Reason::BitmapBudget);
                Require(!dex.inverted_strings_ready.load(std::memory_order_acquire));
            }
        }
    }
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        DexKit bridge(apk, 1); bridge.SetThreadNum(4);
        auto guard = bridge.EnterQueryExecution(kUsingString);
        for (const auto &item : bridge.dex_items) {
            auto &dex = *item;
            auto data = string_admission_fixture::Query(7, false);
            const auto *query = flatbuffers::GetRoot<schema::FindMethod>(data->GetBufferPointer());
            const auto plan = dex.PlanRootStringCandidates(query->matcher());
            Require(plan.route == Route::Legacy);
            QueryContext context(QueryKind::FindMethod);
            std::set<uint32_t> classes, methods;
            trie::PackageTrie packages;
            DeferredExecutor executor;
            auto futures = dex.FindMethod(query, classes, methods, packages, executor, 1000, context);
            Require(futures.size() == (dex.reader.MethodIds().size() + 999) / 1000);
            std::latch start(1);
            std::thread publisher([&] { start.wait(); Require(dex.EnsureInvertedStrings()); });
            start.count_down();
            for (unsigned i = 0; i < 1000; ++i) {
                const auto observed = dex.PlanRootStringCandidates(query->matcher());
                Require(observed.route == Route::Legacy || (small && observed.route == Route::Range));
                if (observed.route == Route::Range) Require(observed.index_ready && observed.postings == 1);
            }
            publisher.join();
            Require(dex.inverted_strings_ready.load(std::memory_order_acquire));
            inverted_string::Bits candidates;
            Require(!dex.BuildRootStringCandidates(query->matcher()->using_strings(), false, plan, candidates));
            Require(candidates.words.empty());
            executor.OnSubmissionComplete();
            size_t found = 0;
            for (auto &future : futures) found += future.get().size();
            Require(found == 1);
            ++frozen;
        }
    }
    std::printf("CHECK_STRING_ADMISSION {\"decisions\":%zu,\"restricted\":%zu,\"frozen_publications\":%zu,\"small_ranges\":%s,\"passed\":true}\n",
            decisions, scoped, frozen, small ? "true" : "false");
#else
    std::printf("CHECK_STRING_ADMISSION {\"inverse_enabled\":false,\"passed\":true}\n");
#endif
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    dexkit::BenchmarkDiagnostics::CheckStringAdmission(argv[1]);
}

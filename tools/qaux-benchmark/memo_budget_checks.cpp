#include "negative_string_memo.h"
#include <atomic>
#include <cstdlib>
#include <latch>
#include <thread>
#include <vector>

// Only this correctness executable replaces nothrow array allocation. Ordinary
// allocations use the real throwing operator and its matching ordinary delete.
static std::atomic<bool> fail_next_array{false};
void *operator new[](std::size_t bytes, const std::nothrow_t &) noexcept {
    if (fail_next_array.exchange(false)) return nullptr;
    try { return ::operator new[](bytes); } catch (...) { return nullptr; }
}

using namespace dexkit;
void Require(bool value) {
    if (!value) { std::fprintf(stderr, "Memo budget check failed\n"); std::abort(); }
}

int main() {
    constexpr size_t budget = DEXKIT_EXPERIMENT_STRING_MEMO_BYTES;
    if (budget < 16) {
        QueryContext query(QueryKind::BatchFindMethodUsingStrings);
        Require(!query.TryReserveStringMemo(budget + 1));
        std::puts("CHECK_MEMO_BUDGET zero_or_tiny_budget_passed");
        return 0;
    }
    const size_t requested = (budget / 16 + 1) * 8;
    uint64_t rejected = 0;
    for (int round = 0; round < 16; ++round) {
        QueryContext query(QueryKind::BatchFindMethodUsingStrings);
        std::latch start(1), ready(8), release(1);
        std::atomic<size_t> live_requested{0};
        std::atomic<uint32_t> denied{0};
        std::vector<std::thread> workers;
        for (uint32_t job = 0; job < 8; ++job) workers.emplace_back([&, job] {
            start.wait();
            NegativeStringMemo memo(query, requested * 8, job);
            memo.RecordEmpty(0);
            if (memo.Contains(0)) live_requested.fetch_add(requested);
            else { ++denied; Require(!memo.Contains(0)); }
            ready.count_down();
            release.wait();
        });
        start.count_down();
        ready.wait();
        Require(live_requested == requested && live_requested <= budget);
        Require(denied == 7);
        Require(!query.TryReserveStringMemo(budget));
        rejected += denied;
        release.count_down();
        for (auto &worker : workers) worker.join();
        Require(query.TryReserveStringMemo(budget));
        query.ReleaseStringMemo(budget);
    }
    QueryContext query(QueryKind::BatchFindMethodUsingStrings);
    {
        fail_next_array = true;
        NegativeStringMemo failed(query, 64, 0);
        Require(!fail_next_array);
        failed.RecordEmpty(0);
        Require(!failed.Contains(0));
        // The failed object is still alive: reservation must be returned now.
        Require(query.TryReserveStringMemo(budget));
        query.ReleaseStringMemo(budget);
    }
    NegativeStringMemo recovered(query, 64, 0);
    recovered.RecordEmpty(0);
    Require(recovered.Contains(0));
    std::printf("CHECK_MEMO_BUDGET {\"rounds\":16,\"jobs_per_round\":8,\"live_requested_bytes\":%zu,\"budget_bytes\":%zu,\"rejections\":%llu,\"allocation_failure_recovered\":true,\"passed\":true}\n",
                requested, budget, (unsigned long long) rejected);
}

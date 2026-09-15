#pragma once

#if DEXKIT_BENCHMARK_BATCH_TRACE
#include <cstdint>
#include <cstdio>

namespace dexkit {
// One stack-owned record per batch DEX task; no per-candidate clocks or atomics.
struct BatchGroupTrace {
    uint64_t query;
    uint32_t dex;
    bool classes;
    uint64_t candidates = 0, fallback_candidates = 0, active_candidates = 0;
    uint64_t group_checks = 0, matched_groups = 0;
    uint64_t nonempty_intersections = 0, intersection_items = 0, capacity_bytes = 0;

    ~BatchGroupTrace() {
        std::fprintf(stderr,
            "BENCH_BATCH_GROUP {\"query\":%llu,\"dex\":%u,\"classes\":%s,"
            "\"candidates\":%llu,\"fallback_candidates\":%llu,\"active_candidates\":%llu,"
            "\"group_checks\":%llu,\"matched_groups\":%llu,"
            "\"nonempty_intersections\":%llu,\"intersection_items\":%llu,\"capacity_bytes\":%llu}\n",
            (unsigned long long)query, dex, classes ? "true" : "false",
            (unsigned long long)candidates, (unsigned long long)fallback_candidates,
            (unsigned long long)active_candidates, (unsigned long long)group_checks,
            (unsigned long long)matched_groups, (unsigned long long)nonempty_intersections,
            (unsigned long long)intersection_items, (unsigned long long)capacity_bytes);
    }
};
}
#define DEXKIT_BATCH_COUNT(field, value) (group_trace.field += (value))
#else
#define DEXKIT_BATCH_COUNT(field, value) ((void)0)
#endif

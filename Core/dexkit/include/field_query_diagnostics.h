#pragma once

#if DEXKIT_BENCHMARK_FIELD_TRACE
#include <cstdint>
#include <cstdio>

namespace dexkit {
struct FieldQueryCounts {
    uint64_t calls = 0, absent = 0, empty = 0, single = 0, multiple = 0;
    uint64_t row_items = 0, single_row_items = 0, direct = 0;
    uint64_t cache_builds = 0, solver_calls = 0, prepared_items = 0, judges = 0;
    inline static thread_local FieldQueryCounts *current = nullptr;
};

// Task-local counters restore any outer binding. They include nested matcher
// work under the task, so dex identifies the task rather than every judged row.
class FieldQueryTraceScope {
public:
    FieldQueryTraceScope(uint64_t query, uint32_t dex, uint8_t kind)
            : query_(query), dex_(dex), kind_(kind), previous_(FieldQueryCounts::current) {
        FieldQueryCounts::current = &counts_;
    }
    ~FieldQueryTraceScope() {
        FieldQueryCounts::current = previous_;
        if (!counts_.calls) return;
        std::fprintf(stderr,
            "BENCH_FIELD {\"query\":%llu,\"dex\":%u,\"kind\":%u,"
            "\"calls\":%llu,\"absent\":%llu,\"empty\":%llu,\"single\":%llu,\"multiple\":%llu,"
            "\"row_items\":%llu,\"single_row_items\":%llu,\"direct\":%llu,"
            "\"cache_builds\":%llu,\"solver_calls\":%llu,\"prepared_items\":%llu,\"judges\":%llu}\n",
            (unsigned long long)query_, dex_, unsigned(kind_),
            (unsigned long long)counts_.calls, (unsigned long long)counts_.absent,
            (unsigned long long)counts_.empty, (unsigned long long)counts_.single,
            (unsigned long long)counts_.multiple, (unsigned long long)counts_.row_items,
            (unsigned long long)counts_.single_row_items, (unsigned long long)counts_.direct,
            (unsigned long long)counts_.cache_builds, (unsigned long long)counts_.solver_calls,
            (unsigned long long)counts_.prepared_items, (unsigned long long)counts_.judges);
    }
    FieldQueryTraceScope(const FieldQueryTraceScope &) = delete;
    FieldQueryTraceScope &operator=(const FieldQueryTraceScope &) = delete;
private:
    uint64_t query_;
    uint32_t dex_;
    uint8_t kind_;
    FieldQueryCounts counts_;
    FieldQueryCounts *previous_;
};
}
#define DEXKIT_FIELD_COUNT(field, value) do { \
    if (auto *counts = ::dexkit::FieldQueryCounts::current) counts->field += (value); \
} while (false)
#else
#define DEXKIT_FIELD_COUNT(field, value) ((void)0)
#endif

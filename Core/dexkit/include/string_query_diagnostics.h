#pragma once

#if DEXKIT_BENCHMARK_STRING_TRACE
#include <cstdint>
#include <cstdio>

namespace dexkit {
struct StringQueryCounts {
    uint64_t method_calls = 0, class_calls = 0, keyword_calls = 0;
    uint64_t ac_refs = 0, ac_bytes = 0, hits = 0, intersections = 0;
    uint64_t direct_calls = 0, direct_refs = 0, index_calls = 0, index_refs = 0;
    uint64_t plans = 0, ranges = 0, comparisons = 0, decoded_units = 0, beans = 0;
    inline static thread_local StringQueryCounts *current = nullptr;
};

// Stack-owned per-task counters. The TLS pointer is trivially destructible,
// and nested scopes restore their previous binding. No hot-path atomics/clocks.
class StringQueryTraceScope {
public:
    StringQueryTraceScope(uint64_t query, uint32_t dex, uint8_t kind)
            : query_(query), dex_(dex), kind_(kind), previous_(StringQueryCounts::current) {
        StringQueryCounts::current = &counts_;
    }
    ~StringQueryTraceScope() {
        StringQueryCounts::current = previous_;
        if (!(counts_.method_calls || counts_.class_calls || counts_.plans || counts_.ranges)) return;
        std::fprintf(stderr,
            "BENCH_STRING {\"query\":%llu,\"dex\":%u,\"kind\":%u,"
            "\"method_calls\":%llu,\"class_calls\":%llu,\"keyword_calls\":%llu,"
            "\"ac_refs\":%llu,\"ac_bytes\":%llu,\"hits\":%llu,\"intersections\":%llu,"
            "\"direct_calls\":%llu,\"direct_refs\":%llu,\"index_calls\":%llu,\"index_refs\":%llu,"
            "\"plans\":%llu,\"ranges\":%llu,\"comparisons\":%llu,\"decoded_units\":%llu,\"beans\":%llu}\n",
            (unsigned long long)query_, dex_, unsigned(kind_),
            (unsigned long long)counts_.method_calls, (unsigned long long)counts_.class_calls,
            (unsigned long long)counts_.keyword_calls, (unsigned long long)counts_.ac_refs,
            (unsigned long long)counts_.ac_bytes, (unsigned long long)counts_.hits,
            (unsigned long long)counts_.intersections, (unsigned long long)counts_.direct_calls,
            (unsigned long long)counts_.direct_refs, (unsigned long long)counts_.index_calls,
            (unsigned long long)counts_.index_refs, (unsigned long long)counts_.plans,
            (unsigned long long)counts_.ranges, (unsigned long long)counts_.comparisons,
            (unsigned long long)counts_.decoded_units, (unsigned long long)counts_.beans);
    }
    StringQueryTraceScope(const StringQueryTraceScope &) = delete;
    StringQueryTraceScope &operator=(const StringQueryTraceScope &) = delete;
private:
    uint64_t query_;
    uint32_t dex_;
    uint8_t kind_;
    StringQueryCounts counts_;
    StringQueryCounts *previous_;
};
}
#define DEXKIT_STRING_COUNT(field, value) do { \
    if (auto *counts = ::dexkit::StringQueryCounts::current) counts->field += (value); \
} while (false)
#else
#define DEXKIT_STRING_COUNT(field, value) ((void)0)
#endif

#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include "query_context.h"

namespace dexkit {

// Owned by one batch-query / DEX job. A bit means ParseText completed with no
// hits for this exact trie. Positive and unvisited values both take the normal
// path; no incomplete result, query identity or cross-DEX ID is shared.
class NegativeStringMemo {
    struct Lease {
        QueryContext &query;
        size_t bytes = 0;
        Lease(QueryContext &query, size_t requested) : query(query) {
            if (query.TryReserveStringMemo(requested)) bytes = requested;
        }
        void Reset() { query.ReleaseStringMemo(bytes); bytes = 0; }
        ~Lease() { Reset(); }
    };
public:
    NegativeStringMemo(QueryContext &query, size_t strings, uint32_t dex)
        : lease_(query, (strings / 64 + (strings % 64 != 0)) * sizeof(uint64_t)), dex_(dex) {
        // This optimization is optional: allocation failure falls back to parsing.
        if (lease_.bytes) {
            bits_.reset(new (std::nothrow) uint64_t[lease_.bytes / sizeof(uint64_t)]());
            if (!bits_) lease_.Reset();
        }
    }
    NegativeStringMemo(const NegativeStringMemo &) = delete;
    NegativeStringMemo &operator=(const NegativeStringMemo &) = delete;

    bool Contains(uint32_t string) {
        const bool hit = bits_ && (bits_[string / 64] & (uint64_t{1} << (string % 64)));
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        ++probes_;
        hits_ += hit;
#endif
        return hit;
    }

    void RecordEmpty(uint32_t string) {
        if (bits_) bits_[string / 64] |= uint64_t{1} << (string % 64);
    }

    ~NegativeStringMemo() {
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        std::fprintf(stderr, "BENCH_MEMO {\"dex\":%u,\"bytes\":%zu,\"probes\":%llu,\"hits\":%llu}\n",
                dex_, lease_.bytes, (unsigned long long) probes_, (unsigned long long) hits_);
#endif
    }

private:
    Lease lease_;
    std::unique_ptr<uint64_t[]> bits_;
    uint32_t dex_;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    uint64_t probes_ = 0, hits_ = 0;
#endif
};

} // namespace dexkit

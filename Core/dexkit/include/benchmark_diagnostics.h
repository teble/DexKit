#pragma once

#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace dexkit {
class DexKit;

struct BenchmarkDiagnostics {
    static void Dump(const DexKit &bridge, const char *phase);
};

// Diagnostic builds only: no counters or clocks enter a measurement binary.
struct BatchScanDiagnostics {
    using Clock = std::chrono::steady_clock;
    uint32_t dex_id;
    size_t groups;
    std::vector<uint8_t> seen;
    uint64_t visits = 0, unique = 0, bytes = 0, unique_bytes = 0;
    uint64_t samples = 0, sample_ns = 0, duplicate_samples = 0, duplicate_sample_ns = 0;
    uint32_t random = 0x9e3779b9U;

    BatchScanDiagnostics(uint32_t dex_id, size_t string_count, size_t groups)
        : dex_id(dex_id), groups(groups), seen(string_count) {}

    bool Observe(uint32_t index, size_t length, bool &duplicate) {
        ++visits;
        bytes += length;
        duplicate = seen[index] != 0;
        if (!duplicate) { ++unique; unique_bytes += length; seen[index] = 1; }
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        return (random & 1023U) == 0;
    }
    void Record(uint64_t ns, bool duplicate) {
        ++samples; sample_ns += ns;
        if (duplicate) { ++duplicate_samples; duplicate_sample_ns += ns; }
    }
    ~BatchScanDiagnostics() {
        std::fprintf(stderr,
            "BENCH_SCAN {\"dex\":%u,\"groups\":%zu,\"visits\":%llu,\"unique\":%llu,"
            "\"bytes\":%llu,\"unique_bytes\":%llu,\"samples\":%llu,\"sample_wall_ns\":%llu,"
            "\"duplicate_samples\":%llu,\"duplicate_sample_wall_ns\":%llu}\n",
            dex_id, groups, (unsigned long long) visits, (unsigned long long) unique,
            (unsigned long long) bytes, (unsigned long long) unique_bytes,
            (unsigned long long) samples, (unsigned long long) sample_ns,
            (unsigned long long) duplicate_samples, (unsigned long long) duplicate_sample_ns);
    }
};
}
#endif

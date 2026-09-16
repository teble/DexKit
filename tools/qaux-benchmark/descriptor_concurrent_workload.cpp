#include "dexkit.h"
#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include "benchmark_diagnostics.h"
#endif
#include <barrier>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
using namespace dexkit;
void Require(bool condition) { if (!condition) std::abort(); }
int64_t Ns(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}
uint64_t Query(DexKit &bridge) {
    uint64_t checksum = 0;
    {
        const auto result = bridge.GetMethodData("Lfixture/Api;->a()I");
        Require(result != nullptr);
        const auto *method = flatbuffers::GetRoot<schema::MethodMeta>(result->GetBufferPointer());
        Require(method->dex_descriptor()->string_view() == "Lfixture/Api;->a()I");
        checksum += method->id() + method->dex_descriptor()->size();
    }
    Require(bridge.GetMethodData("Lfixture/Api;->a()F") == nullptr);
    {
        const auto result = bridge.GetFieldData("Lfixture/Api;->a:I");
        Require(result != nullptr);
        const auto *field = flatbuffers::GetRoot<schema::FieldMeta>(result->GetBufferPointer());
        Require(field->dex_descriptor()->string_view() == "Lfixture/Api;->a:I");
        checksum += field->id() + field->dex_descriptor()->size();
    }
    Require(bridge.GetFieldData("Lfixture/Api;->a:F") == nullptr);
    return checksum;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        std::fprintf(stderr, "Usage: dexkit_descriptor_concurrent_workload symbols.apk warm_repeats calling_threads\n");
        return 2;
    }
    char *end = nullptr;
    const auto repeats = std::strtoull(argv[2], &end, 10);
    Require(end && *end == '\0' && repeats > 0 && repeats <= 1000000);
    const auto workers = std::strtoull(argv[3], &end, 10);
    Require(end && *end == '\0' && (workers == 1 || workers == 4));
    const auto lifecycle_start = Clock::now();
    auto begin = Clock::now();
    auto bridge = std::make_unique<DexKit>(argv[1], 1);
    const auto create = Ns(begin);
    bridge->SetThreadNum(workers);
    // Stage barriers measure wall time. Four calling threads really issue
    // public APIs together; SetThreadNum alone would not exercise cache locks.
    std::barrier stages(workers + 1);
    std::vector<uint64_t> checksums(workers);
    std::vector<std::thread> threads;
    for (size_t worker = 0; worker < workers; ++worker) threads.emplace_back([&, worker] {
        stages.arrive_and_wait();
        checksums[worker] += Query(*bridge);
        stages.arrive_and_wait();
        stages.arrive_and_wait();
        for (size_t i = 0; i < repeats; ++i) checksums[worker] += Query(*bridge);
        stages.arrive_and_wait();
    });
    begin = Clock::now();
    stages.arrive_and_wait(); stages.arrive_and_wait();
    const auto first = Ns(begin);
    begin = Clock::now();
    stages.arrive_and_wait(); stages.arrive_and_wait();
    const auto warm = Ns(begin);
    for (auto &thread : threads) thread.join();
    uint64_t checksum = 0;
    for (const auto value : checksums) { Require(value == checksums[0]); checksum += value; }
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    BenchmarkDiagnostics::Dump(*bridge, "concurrent-workload-complete");
#endif
    begin = Clock::now();
    bridge.reset();
    const auto close = Ns(begin), lifecycle = Ns(lifecycle_start);
    std::printf("WORKLOAD {\"mode\":\"lookup-concurrent\",\"repeats\":%llu,\"calling_threads\":%llu,"
        "\"create_ns\":%lld,\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"checksum\":%llu,\"returned\":%llu}\n", repeats, workers, (long long)create, (long long)first,
        (long long)warm, (long long)close, (long long)lifecycle, (unsigned long long)checksum,
        (repeats + 1) * workers * 2);
}

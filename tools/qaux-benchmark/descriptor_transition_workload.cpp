#include "dexkit.h"
#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include "benchmark_diagnostics.h"
#endif
#include <barrier>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <thread>

namespace {
using namespace dexkit;
using Clock = std::chrono::steady_clock;
void Require(bool ok) { if (!ok) std::abort(); }
int64_t Ns(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}
uint64_t Query(DexKit &bridge, bool method, const std::vector<int64_t> &ids) {
    const auto output = method ? bridge.GetMethodByIds(ids) : bridge.GetFieldByIds(ids);
    uint64_t checksum = 0;
    auto check = [&](const auto *values, size_t length) {
        Require(values->size() == ids.size());
        for (size_t i = 0; i < ids.size(); ++i) {
            const auto *value = values->Get(i);
            Require(value->dex_id() == 0 && value->id() == ids[i]
                    && value->dex_descriptor()->size() == length);
            checksum += value->id() + length;
        }
    };
    if (method) check(flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(output->GetBufferPointer())->methods(), 14);
    else check(flatbuffers::GetRoot<schema::FieldMetaArrayHolder>(output->GetBufferPointer())->fields(), 13);
    return checksum;
}
struct Stats {
    int64_t create = 0, setup = 0, first = 0, repeated = 0, close = 0;
    uint64_t checksum = 0, returned = 0;
};
Stats Run(const char *apk, bool method, std::string_view pattern, size_t repeats, size_t workers) {
    Stats result;
    auto started = Clock::now();
    auto bridge = std::make_unique<DexKit>(apk, 1);
    result.create = Ns(started);
    started = Clock::now();
    bridge->SetThreadNum(workers);
    std::vector<int64_t> first_ids(pattern == "full" ? 60000 : pattern == "changed" ? 30000 : 64);
    std::iota(first_ids.begin(), first_ids.end(), int64_t(0));
    if (pattern == "low") for (auto &id : first_ids) id = id * 911 % 60000;
    auto repeated_ids = first_ids;
    if (pattern == "changed") for (auto &id : repeated_ids) id += 30000;
    std::barrier phases(workers + 1);
    std::vector<uint64_t> checksums(workers);
    std::vector<std::thread> threads;
    for (size_t worker = 0; worker < workers; ++worker) threads.emplace_back([&, worker] {
        phases.arrive_and_wait();
        checksums[worker] = Query(*bridge, method, first_ids);
        phases.arrive_and_wait();
        phases.arrive_and_wait();
        for (size_t i = 1; i < repeats; ++i)
            checksums[worker] += Query(*bridge, method, repeated_ids);
        phases.arrive_and_wait();
    });
    result.setup = Ns(started);
    started = Clock::now();
    phases.arrive_and_wait(); phases.arrive_and_wait();
    result.first = Ns(started);
    started = Clock::now();
    phases.arrive_and_wait(); phases.arrive_and_wait();
    result.repeated = repeats == 1 ? 0 : Ns(started);
    for (auto &thread : threads) thread.join();
    for (auto checksum : checksums) { Require(checksum == checksums[0]); result.checksum += checksum; }
    result.returned = workers * (first_ids.size() + (repeats - 1) * repeated_ids.size());
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    BenchmarkDiagnostics::Dump(*bridge, "transition-workload-complete");
#endif
    started = Clock::now();
    bridge.reset();
    result.close = Ns(started);
    return result;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 6) {
        std::fprintf(stderr, "Usage: descriptor_transition_workload dense.apk method|field full|changed|low total_passes calling_threads\n");
        return 2;
    }
    const std::string_view kind(argv[2]), pattern(argv[3]);
    Require((kind == "method" || kind == "field") && (pattern == "full" || pattern == "changed" || pattern == "low"));
    char *end = nullptr;
    const auto repeats = std::strtoull(argv[4], &end, 10);
    Require(end && *end == '\0' && repeats >= 1 && repeats <= 100000);
    const auto workers = std::strtoull(argv[5], &end, 10);
    Require(end && *end == '\0' && (workers == 1 || workers == 4));
    const auto started = Clock::now();
    const auto stats = Run(argv[1], kind == "method", pattern, repeats, workers);
    const auto lifecycle = Ns(started);
    std::printf("WORKLOAD {\"mode\":\"transition\",\"kind\":\"%s\",\"pattern\":\"%s\",\"repeats\":%llu,\"calling_threads\":%llu,"
        "\"create_ns\":%lld,\"setup_ns\":%lld,\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"checksum\":%llu,\"returned\":%llu}\n", argv[2], argv[3], repeats, workers,
        (long long)stats.create, (long long)stats.setup, (long long)stats.first, (long long)stats.repeated,
        (long long)stats.close, (long long)lifecycle, (unsigned long long)stats.checksum, (unsigned long long)stats.returned);
}

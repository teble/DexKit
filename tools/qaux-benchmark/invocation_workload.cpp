#include "invocation_queries.h"
#include "benchmark_memory_snapshot.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace dexkit {
// Benchmark access to the existing barrier, so the late-caller case can warm
// only forward invocations. Public invocation queries request both domains.
struct CallerBenchmark {
    static void Warm(DexKit &bridge, uint32_t flags) {
        auto guard = bridge.EnterQueryExecution(flags);
    }
};
}

namespace {
using namespace invocation_fixture;
using Clock = std::chrono::steady_clock;
void Require(bool condition) { if (!condition) std::abort(); }
int64_t Ns(Clock::time_point begin) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count();
}
struct Statistics {
    int64_t create_ns = 0, setup_ns = 0, first_ns = 0, repeated_ns = 0, close_ns = 0;
    int64_t positive_ns = 0, negative_ns = 0;
    int64_t forward_build_ns = 0, caller_build_ns = 0;
    BenchmarkMemorySnapshot warm_memory, closed_memory;
    uint64_t checksum = 0, returned = 0;
};
void Consume(Statistics &s, std::unique_ptr<Builder> data) {
    Require(data != nullptr);
    auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods();
    s.returned += methods->size();
    for (auto method : *methods)
        s.checksum += uint64_t(method->dex_id()) * 0x100000001ULL + method->id() + method->dex_descriptor()->size();
}
Statistics Run(const char *apk, std::string_view mode, size_t repeats) {
    Statistics s;
    int workers = 4;
    bool staged = false;
    if (mode.ends_with("-w1") || mode.ends_with("-w4")) {
        workers = mode.back() - '0';
        mode.remove_suffix(3);
        staged = true;
    }
    const bool late = staged && mode.ends_with("-late"), full = staged && mode.ends_with("-full");
    if (staged) {
        Require(late || full || mode.ends_with("-cold"));
        mode.remove_suffix(5);
        Require(mode.starts_with("caller-"));
    }
    Require(mode == "invoke-output" || mode == "caller-output" || mode == "invoke-match" || mode == "caller-match"
        || mode == "invoke-early" || mode == "caller-early" || mode == "invoke-multiple" || mode == "caller-multiple");
    auto begin = Clock::now();
    auto bridge = std::make_unique<DexKit>(apk, 1);
    s.create_ns = Ns(begin);
    begin = Clock::now();
    bridge->SetThreadNum(workers);
    const bool callers = mode.starts_with("caller-");
    const bool early = mode.ends_with("early"), multiple = mode.ends_with("multiple");
    auto positive = Query(callers, early ? 9 : multiple ? 3 : 0), negative = Query(callers, multiple ? 10 : 1);
    const auto descriptor = callers ? "Lrelations/Target;->aEarly()V" : "Lrelations/Source1;->run00000()V";
    auto data = bridge->GetMethodData(descriptor);
    Require(data != nullptr);
    auto meta = flatbuffers::GetRoot<schema::MethodMeta>(data->GetBufferPointer());
    const int64_t id = (int64_t(meta->dex_id()) << 32) | uint32_t(meta->id());
    if (late) {
        const auto forward = Clock::now();
        CallerBenchmark::Warm(*bridge, kMethodInvoking);
        s.forward_build_ns = Ns(forward);
    }
    if (staged) {
        const auto caller = Clock::now();
        if (full) bridge->InitFullCache();
        else CallerBenchmark::Warm(*bridge, kMethodInvoking | kCallerMethod);
        s.caller_build_ns = Ns(caller);
        s.warm_memory = ReadBenchmarkMemory();
    }
    s.setup_ns = Ns(begin);
    for (size_t i = 0; i < repeats; ++i) {
        begin = Clock::now();
        if (mode.ends_with("output")) {
            Consume(s, callers ? bridge->GetCallMethods(id) : bridge->GetInvokeMethods(id));
        } else {
            for (auto *query : {positive.get(), negative.get()}) {
                if (early && query == negative.get()) continue;
                auto leg = Clock::now();
                Consume(s, bridge->FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer())));
                (query == positive.get() ? s.positive_ns : s.negative_ns) += Ns(leg);
            }
        }
        (i ? s.repeated_ns : s.first_ns) += Ns(begin);
    }
    begin = Clock::now(); bridge.reset(); s.close_ns = Ns(begin);
    if (staged) s.closed_memory = ReadBenchmarkMemory();
    return s;
}
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    const std::string_view mode(argv[2]);
    char *end = nullptr;
    auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && !*end && repeats > 0 && repeats <= 100000);
    auto begin = Clock::now();
    auto s = Run(argv[1], mode, repeats);
    auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"positive_ns\":%lld,\"negative_ns\":%lld,\"checksum\":%llu,\"returned\":%llu,"
        "\"forward_build_ns\":%lld,\"caller_build_ns\":%lld,\"warm_footprint_bytes\":%lld,\"warm_malloc_in_use_bytes\":%lld,"
        "\"closed_footprint_bytes\":%lld,\"closed_malloc_in_use_bytes\":%lld}\n", argv[2], repeats,
        (long long)s.create_ns, (long long)s.setup_ns, (long long)s.first_ns, (long long)s.repeated_ns,
        (long long)s.close_ns, (long long)lifecycle, (long long)s.positive_ns, (long long)s.negative_ns,
        (unsigned long long)s.checksum, (unsigned long long)s.returned,
        (long long)s.forward_build_ns, (long long)s.caller_build_ns, (long long)s.warm_memory.footprint,
        (long long)s.warm_memory.malloc_in_use, (long long)s.closed_memory.footprint, (long long)s.closed_memory.malloc_in_use);
}

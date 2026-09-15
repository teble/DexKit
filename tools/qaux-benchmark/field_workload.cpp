#include "field_queries.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {
using namespace field_fixture;
using Clock = std::chrono::steady_clock;
void Require(bool value) { if (!value) std::abort(); }
int64_t Ns(Clock::time_point begin) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count();
}
struct Statistics {
    int64_t create_ns = 0, setup_ns = 0, first_ns = 0, repeated_ns = 0, close_ns = 0;
    uint64_t checksum = 0, returned = 0;
};
template<typename Values>
void Consume(Statistics &s, const Values *values) {
    s.returned += values->size();
    for (auto value : *values)
        s.checksum += uint64_t(value->dex_id()) * 0x100000001ULL + uint32_t(value->id())
                      + value->dex_descriptor()->size();
}
Statistics Run(const char *apk, std::string_view mode, size_t repeats) {
    Statistics s;
    auto begin = Clock::now();
    auto bridge = std::make_unique<DexKit>(apk, 1); s.create_ns = Ns(begin);
    begin = Clock::now();
    bridge->SetThreadNum(4);
    const bool classes = mode == "using-class";
    auto query = Query(classes, mode == "using-multiple" ? 10 : mode == "using-sparse" ? 23 : 2,
                       mode == "using-late" ? "late" : mode == "using-miss" ? "miss"
                       : mode == "using-sparse" ? "early00000" : "early", mode == "using-sparse");
    s.setup_ns = Ns(begin);
    for (size_t i = 0; i < repeats; ++i) {
        begin = Clock::now();
        {
            auto data = classes ? bridge->FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()))
                                : bridge->FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
            Require(data != nullptr);
            if (classes) Consume(s, flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(data->GetBufferPointer())->classes());
            else Consume(s, flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods());
        }
        (i ? s.repeated_ns : s.first_ns) += Ns(begin);
    }
    Require((s.returned == 0) == (mode == "using-miss"));
    begin = Clock::now(); bridge.reset(); s.close_ns = Ns(begin);
    return s;
}
}
int main(int argc, char **argv) {
    if (argc != 4) return 2;
    std::string_view mode(argv[2]);
    Require(mode == "using-early" || mode == "using-late" || mode == "using-miss"
            || mode == "using-sparse" || mode == "using-multiple" || mode == "using-class");
    char *end = nullptr;
    auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && !*end && repeats > 0 && repeats <= 100000);
    auto begin = Clock::now(); auto s = Run(argv[1], mode, repeats); auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"positive_ns\":%lld,\"negative_ns\":0,\"checksum\":%llu,\"returned\":%llu}\n",
        argv[2], repeats, (long long)s.create_ns, (long long)s.setup_ns, (long long)s.first_ns,
        (long long)s.repeated_ns, (long long)s.close_ns, (long long)lifecycle,
        (long long)(s.first_ns + s.repeated_ns), (unsigned long long)s.checksum,
        (unsigned long long)s.returned);
}

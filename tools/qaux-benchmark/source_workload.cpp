#include "source_queries.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace {
using namespace source_fixture;
using Clock = std::chrono::steady_clock;
void Require(bool condition) { if (!condition) std::abort(); }
int64_t Ns(Clock::time_point begin) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count();
}
struct Statistics {
    int64_t create_ns = 0, setup_ns = 0, first_ns = 0, repeated_ns = 0, close_ns = 0;
    int64_t positive_ns = 0, negative_ns = 0;
    uint64_t checksum = 0, returned = 0;
};
void Consume(Statistics &s, std::unique_ptr<Builder> data, bool single = false) {
    Require(data != nullptr);
    const auto add = [&](const schema::ClassMeta *value) {
        ++s.returned;
        s.checksum += uint64_t(value->dex_id()) * 0x100000001ULL + value->id()
            + value->dex_descriptor()->size() + value->source_file()->size();
    };
    if (single) add(flatbuffers::GetRoot<schema::ClassMeta>(data->GetBufferPointer()));
    else for (auto value : *flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(data->GetBufferPointer())->classes()) add(value);
}
Statistics Run(const char *apk, std::string_view mode, size_t repeats) {
    Statistics s;
    auto begin = Clock::now();
    auto bridge = std::make_unique<DexKit>(apk, 1);
    s.create_ns = Ns(begin);
    begin = Clock::now();
    bridge->SetThreadNum(4);
    auto positive = Query(mode == "source-output" ? 0 : 1), negative = Query(2);
    s.setup_ns = Ns(begin);
    for (size_t i = 0; i < repeats; ++i) {
        begin = Clock::now();
        if (mode == "source-hot") Consume(s, bridge->GetClassData("Lsources/Case;"), true);
        else {
            for (auto *query : {positive.get(), negative.get()}) {
                if (query == negative.get() && mode == "source-output") continue;
                auto leg = Clock::now();
                Consume(s, bridge->FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer())));
                (query == positive.get() ? s.positive_ns : s.negative_ns) += Ns(leg);
            }
        }
        (i ? s.repeated_ns : s.first_ns) += Ns(begin);
    }
    begin = Clock::now(); bridge.reset(); s.close_ns = Ns(begin);
    return s;
}
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    const std::string_view mode(argv[2]);
    Require(mode == "source-output" || mode == "source-match" || mode == "source-hot");
    char *end = nullptr;
    auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && !*end && repeats > 0 && repeats <= 100000);
    auto begin = Clock::now(); auto s = Run(argv[1], mode, repeats); auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"positive_ns\":%lld,\"negative_ns\":%lld,\"checksum\":%llu,\"returned\":%llu}\n", argv[2], repeats,
        (long long)s.create_ns, (long long)s.setup_ns, (long long)s.first_ns, (long long)s.repeated_ns,
        (long long)s.close_ns, (long long)lifecycle, (long long)s.positive_ns, (long long)s.negative_ns,
        (unsigned long long)s.checksum, (unsigned long long)s.returned);
}

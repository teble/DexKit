#define main OriginalStringWorkloadMain
#include "string_workload.cpp"
#undef main
#include "string_admission_queries.h"
#include <map>

namespace {
void Warm(DexKit &bridge) {
    auto query = string_admission_fixture::WarmupQuery();
    auto result = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
    Require(flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods()->size() == 0);
}

void Dump(const char *apk) {
    std::map<std::tuple<bool, int, bool>, std::string> expected;
    for (int state = 0; state < 3; ++state) {
        for (bool classes : {false, true}) {
            const auto count = classes ? string_admission_fixture::ClassQueryCount : string_admission_fixture::QueryCount;
            for (int variant = 0; variant < count; ++variant) {
                for (bool absent : {false, true}) {
                    DexKit bridge(apk, 1); bridge.SetThreadNum(4);
                    if (state == 1) bridge.InitFullCache();
                    if (state == 2) Warm(bridge);
                    auto query = classes ? string_admission_fixture::ClassQuery(variant, absent)
                                         : string_admission_fixture::Query(variant, absent);
                    auto result = classes ? bridge.FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()))
                                          : bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
                    const std::string bytes(reinterpret_cast<const char *>(result->GetBufferPointer()), result->GetSize());
                    const auto key = std::make_tuple(classes, variant, absent);
                    if (state) { Require(expected.at(key) == bytes); continue; }
                    expected.emplace(key, bytes);
                    std::fprintf(stderr, "ADMISSION_RESULTS {\"classes\":%s,\"variant\":%d,\"absent\":%s,\"values\":[",
                            classes ? "true" : "false", variant, absent ? "true" : "false");
                    bool first = true;
                    const auto output = [&](const auto *items) {
                        for (const auto *item : *items) {
                            const auto descriptor = item->dex_descriptor()->string_view();
                            std::fprintf(stderr, "%s[%u,%u,\"%.*s\"]", first ? "" : ",", item->dex_id(),
                                    uint32_t(item->id()), int(descriptor.size()), descriptor.data());
                            first = false;
                        }
                    };
                    if (classes) output(flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(result->GetBufferPointer())->classes());
                    else output(flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods());
                    std::fprintf(stderr, "]}\n");
                    const uint32_t size = result->GetSize();
                    Require(std::fwrite(&size, sizeof(size), 1, stdout) == 1);
                    Require(std::fwrite(bytes.data(), 1, bytes.size(), stdout) == bytes.size());
                }
            }
        }
    }
}

Statistics Measure(const char *apk, int variant, bool warm, size_t repeats) {
    Statistics s;
    auto begin = Clock::now(); auto bridge = std::make_unique<DexKit>(apk, 1); s.create_ns = Ns(begin);
    begin = Clock::now(); bridge->SetThreadNum(4);
    auto positive = string_admission_fixture::Query(variant, false);
    auto negative = string_admission_fixture::Query(variant, true);
    if (warm) Warm(*bridge);
    s.setup_ns = Ns(begin);
    for (size_t repeat = 0; repeat < repeats; ++repeat) {
        begin = Clock::now();
        for (auto query : {positive.get(), negative.get()}) {
            auto leg = Clock::now();
            Consume(s, bridge->FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer())), false);
            (query == positive.get() ? s.positive_ns : s.negative_ns) += Ns(leg);
        }
        (repeat ? s.repeated_ns : s.first_ns) += Ns(begin);
    }
    begin = Clock::now(); bridge.reset(); s.close_ns = Ns(begin);
    return s;
}
}

int main(int argc, char **argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--dump") { Dump(argv[2]); return 0; }
    if (argc != 4) return 2;
    const std::map<std::string_view, int> variants{
        {"string-admission-rare", 1}, {"string-admission-wide", 2},
        {"string-admission-nested-rare", 3}, {"string-admission-nested-wide", 4},
        {"string-admission-flags", 5}, {"string-admission-return", 6},
        {"string-admission-equal-one", 7}, {"string-admission-prefix-one", 8},
        {"string-admission-equal-two", 9}, {"string-admission-prefix-two", 10},
    };
    std::string_view mode(argv[2]);
    const bool warm = mode.ends_with("-warm");
    if (warm) mode.remove_suffix(5);
    const auto found = variants.find(mode); Require(found != variants.end());
    char *end = nullptr; const auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && !*end && repeats > 0 && repeats <= 100000);
    auto begin = Clock::now(); auto s = Measure(argv[1], found->second, warm, repeats); const auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"positive_ns\":%lld,\"negative_ns\":%lld,\"checksum\":%llu,\"returned\":%llu}\n", argv[2], repeats,
        (long long)s.create_ns, (long long)s.setup_ns, (long long)s.first_ns, (long long)s.repeated_ns,
        (long long)s.close_ns, (long long)lifecycle, (long long)s.positive_ns, (long long)s.negative_ns,
        (unsigned long long)s.checksum, (unsigned long long)s.returned);
}

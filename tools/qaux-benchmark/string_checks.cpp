#include "string_queries.h"
#include "single_string_index.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>

namespace {
using namespace string_fixture;
void Require(bool condition) { if (!condition) std::abort(); }

std::string EncodeUnit(uint16_t unit) {
    if (unit > 0 && unit < 0x80) return std::string(1, char(unit));
    if (unit < 0x800) return {char(0xc0 | (unit >> 6)), char(0x80 | (unit & 63))};
    return {char(0xe0 | (unit >> 12)), char(0x80 | ((unit >> 6) & 63)), char(0x80 | (unit & 63))};
}

void CheckOrdering() {
    std::vector<std::pair<std::u16string, std::string>> source;
    source.emplace_back(u"", "");
    for (uint32_t unit = 0; unit <= 0xffff; ++unit) {
        source.emplace_back(std::u16string(1, char16_t(unit)), EncodeUnit(unit));
    }
    for (uint32_t unit = 0; unit <= 0xffff; unit += 73) {
        source.emplace_back(std::u16string(u"A") + char16_t(unit), "A" + EncodeUnit(unit));
    }
    source.emplace_back(u"AB", "AB"); source.emplace_back(u"ABC", "ABC");
    std::sort(source.begin(), source.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    source.erase(std::unique(source.begin(), source.end(), [](const auto &a, const auto &b) { return a.first == b.first; }), source.end());
    std::vector<std::string_view> pool;
    for (auto &item : source) pool.push_back(item.second);
    std::vector<std::string> patterns{"A", "AB", "ABC", "ABCD", "A\177", "Absent"};
    for (int i = 1; i <= 127; ++i) patterns.emplace_back(1, char(i));
    for (const auto &pattern : patterns) {
        for (bool prefix : {false, true}) {
            auto range = single_string::FindIds(pool, pattern, prefix);
            Require(range.valid && range.begin <= range.end && range.end <= pool.size());
            for (size_t i = 0; i < pool.size(); ++i) {
                const bool expected = prefix ? pool[i].starts_with(pattern) : pool[i] == pattern;
                Require(expected == (range.begin <= i && i < range.end));
            }
        }
    }
    const std::vector<std::string_view> empty;
    auto empty_range = single_string::FindIds(empty, "A", true);
    Require(empty_range.valid && empty_range.begin == empty_range.end);
    for (auto pattern : {std::string(), std::string("A\0B", 3), std::string("\316\273")})
        Require(!single_string::FindIds(pool, pattern, true).valid);
    for (auto bad : {std::string("\300"), std::string("\300A"), std::string("\301\201"),
                     std::string("\340\240"), std::string("\340AA"), std::string("\360\237\230\200")}) {
        std::vector<std::string_view> malformed{bad};
        Require(!single_string::FindIds(malformed, "A", true).valid);
    }
}

void CheckDexIsolation() {
#if DEXKIT_EXPERIMENT_SINGLE_STRING_ID
    single_string::DexRanges ranges(65537);
    const std::vector<std::string_view> low{"A", "Needle", "Z"}, high{"Needle", "Z"};
    std::array<const single_string::IdRange *, 8> observed{};
    std::vector<std::thread> threads;
    std::latch start(1);
    for (size_t i = 0; i < observed.size(); ++i) {
        threads.emplace_back([&, i] {
            start.wait();
            for (int repeat = 0; repeat < 16; ++repeat) {
                auto *range = ranges.Get(i % 2 ? uint32_t(65536) : 0, i % 2 ? high : low, "Needle", false);
                Require(range && range->begin == (i % 2 ? 0 : 1) && range->end == range->begin + 1);
                if (observed[i]) Require(observed[i] == range);
                observed[i] = range;
            }
        });
    }
    start.count_down();
    for (auto &thread : threads) thread.join();
    for (size_t i = 2; i < observed.size(); ++i) Require(observed[i] == observed[i % 2]);
    Require(observed[0] != observed[1]);
    Require(!ranges.Get(65537, low, "Needle", false));
    Require(!ranges.Get(UINT32_MAX, low, "Needle", false));
    single_string::DexRanges empty(0);
    Require(!empty.Get(0, low, "Needle", false));
#endif
}

void Append(std::string &out, const Builder &data) {
    Require(data.GetSize() <= UINT32_MAX);
    uint32_t size = data.GetSize();
    for (int i = 0; i < 4; ++i) out.push_back(char(size >> (i * 8)));
    out.append(reinterpret_cast<const char *>(data.GetBufferPointer()), size);
}

template<typename Values>
void Describe(bool classes, int variant, const Values *values) {
    std::fprintf(stderr, "STRING_RESULTS {\"classes\":%s,\"variant\":%d,\"results\":[", classes ? "true" : "false", variant);
    bool first = true;
    for (auto value : *values) {
        const auto descriptor = value->dex_descriptor()->string_view();
        std::fprintf(stderr, "%s[%u,%u,\"%.*s\"]", first ? "" : ",", value->dex_id(),
                     uint32_t(value->id()), int(descriptor.size()), descriptor.data());
        first = false;
    }
    std::fprintf(stderr, "]}\n");
}

std::string Collect(DexKit &bridge, bool describe = false) {
    std::string out;
    for (bool classes : {false, true}) {
        for (int variant = 0; variant < QueryCount; ++variant) {
            auto query = Query(classes, variant);
            auto data = classes ? bridge.FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()))
                                : bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
            Require(data != nullptr);
            if (describe) {
                if (classes) Describe(true, variant, flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(data->GetBufferPointer())->classes());
                else Describe(false, variant, flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods());
            }
            Append(out, *data);
        }
    }
    return out;
}
}

int main(int argc, char **argv) {
    const bool dump = argc == 3 && std::string_view(argv[1]) == "--dump";
    if (argc != 2 && !dump) return 2;
    CheckOrdering();
    CheckDexIsolation();
    DexKit reference(argv[argc - 1], 1);
    reference.SetThreadNum(4);
    reference.InitFullCache();
    const auto expected = Collect(reference, true);
    for (bool warm : {false, true}) {
        DexKit bridge(argv[argc - 1], 1);
        bridge.SetThreadNum(4);
        if (warm) bridge.InitFullCache();
        Require(Collect(bridge) == expected);
        Require(Collect(bridge) == expected);
        std::latch start(1);
        std::thread a([&] { start.wait(); Require(Collect(bridge) == expected); });
        std::thread b([&] { start.wait(); Require(Collect(bridge) == expected); });
        start.count_down(); a.join(); b.join();
        bridge.InitFullCache();
        Require(Collect(bridge) == expected);
    }
    if (dump) Require(std::fwrite(expected.data(), 1, expected.size(), stdout) == expected.size());
    else std::printf("CHECK_STRINGS {\"queries\":%d,\"ordering_units\":65536,\"passed\":true}\n", QueryCount * 2);
}

#include "inverted_string_index.h"
#include <cstdio>
#include <cstdlib>
#include <set>

namespace {
using namespace dexkit::inverted_string;
void Require(bool condition) { if (!condition) std::abort(); }

void Check(size_t strings, size_t methods, uint32_t seed) {
    std::vector<std::vector<uint32_t>> forward(methods);
    std::vector<std::set<uint32_t>> reverse(strings);
    auto random = [&] { seed = seed * 1664525U + 1013904223U; return seed; };
    for (size_t method = 0; method < methods && strings; ++method) {
        const auto count = random() % 7;
        for (unsigned i = 0; i < count; ++i) {
            const auto id = random() % strings;
            forward[method].push_back(id);
            forward[method].push_back(id);
            reverse[id].insert(method);
        }
    }
    // Explicitly exercise 65535/65536, regardless of the random generator.
    if (methods && strings) {
        forward.back().push_back(strings - 1);
        reverse.back().insert(methods - 1);
    }
    Index index;
    Require(index.Build(strings, methods, forward));
    Require(index.Ready() && index.Narrow() == (methods <= 65536));
    size_t used = 0, edges = 0, singles = 0;
    for (size_t id = 0; id < strings; ++id) {
        used += !reverse[id].empty(); edges += reverse[id].size(); singles += reverse[id].size() == 1;
        std::vector<uint32_t> actual;
        index.VisitRange(id, id + 1, [&](uint32_t method) { actual.push_back(method); });
        Require(std::equal(actual.begin(), actual.end(), reverse[id].begin(), reverse[id].end()));
    }
    Require(index.UsedStrings() == used && index.Singletons() == singles && index.Edges() == edges);
    std::vector<uint32_t> actual_strings, expected_strings;
    index.EachString([&](uint32_t id) { actual_strings.push_back(id); });
    for (uint32_t id = 0; id < strings; ++id) if (!reverse[id].empty()) expected_strings.push_back(id);
    Require(actual_strings == expected_strings);
    for (unsigned i = 0; i < 100; ++i) {
        size_t begin = i == 0 ? 0 : random() % (strings + 1);
        size_t end = i == 0 ? strings : random() % (strings + 1);
        if (begin > end) std::swap(begin, end);
        Bits expected(methods), actual(methods);
        for (size_t id = begin; id < end; ++id) for (auto method : reverse[id]) expected.Set(method);
        index.VisitRange(begin, end, [&](uint32_t method) { actual.Set(method); });
        Require(actual.words == expected.words);
    }
}
}

int main() {
    Require(!BitmapPlanBytes(4096, 60000, 1, 32765));
    Require(BitmapPlanBytes(4096, 60000, 1, 32752) == 16777040);
    Require(!BitmapPlanBytes(4096, 60000, 1, 32753));
    Require(!BitmapPlanBytes(4096, 60000, SIZE_MAX, 1));
    Require(!BitmapPlanBytes(4096, 60000, 1, SIZE_MAX));
    Require(!BitmapPlanBytes(SIZE_MAX, 60000, 1, 1));
    Require(!BitmapPlanBytes(4096, SIZE_MAX, 1, 1));
    Require(WordCount(SIZE_MAX) == SIZE_MAX / 64 + 1);
    for (size_t strings : {size_t{0}, size_t{1}, size_t{63}, size_t{64}, size_t{65}, size_t{129}, size_t{1024}})
        for (size_t methods : {size_t{0}, size_t{1}, size_t{67}, size_t{65536}, size_t{65537}})
            Check(strings, methods, 20260916);
    for (uint32_t seed = 0; seed < 16; ++seed) Check(131, 277, seed);
    if constexpr (sizeof(size_t) > sizeof(uint32_t)) {
        const std::vector<std::vector<uint32_t>> empty;
        Index too_large;
        Require(!too_large.Build(size_t{UINT32_MAX} + 1, 0, empty));
        Require(!too_large.Build(0, size_t{UINT32_MAX} + 1, empty));
    }
    std::printf("CHECK_INVERTED_STRINGS {\"layouts\":51,\"intervals\":5100,\"budget_boundaries\":8,\"width_boundary\":65536,\"passed\":true}\n");
}

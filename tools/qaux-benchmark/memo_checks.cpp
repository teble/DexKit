#include "negative_string_memo.h"
#include "acdat/Builder.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>

using namespace dexkit;

void Require(bool value) {
    if (!value) { std::fprintf(stderr, "Memo counterexample failed\n"); std::abort(); }
}

struct Result { uint64_t ns, parses, hits; };

template<bool UseMemo>
Result Run(acdat::AhoCorasickDoubleArrayTrie<std::string_view> &trie,
           const std::vector<std::string> &strings, size_t directory_size, size_t visits) {
    const auto begin = std::chrono::steady_clock::now();
    uint64_t parses = 0, hits = 0;
    {
        QueryContext query(QueryKind::BatchFindMethodUsingStrings);
        std::optional<NegativeStringMemo> memo;
        if constexpr (UseMemo) memo.emplace(query, directory_size, 0);
        for (size_t i = 0; i < visits; ++i) {
            const auto id = static_cast<uint32_t>(i % strings.size());
            if constexpr (UseMemo) { if (memo->Contains(id)) continue; }
            auto found = trie.ParseText(strings[id]);
            ++parses;
            hits += found.size();
            if constexpr (UseMemo) { if (found.empty()) memo->RecordEmpty(id); }
        }
    }
    return {static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - begin).count()), parses, hits};
}

int main() {
    constexpr size_t budget = DEXKIT_EXPERIMENT_STRING_MEMO_BYTES;
    QueryContext query(QueryKind::BatchFindMethodUsingStrings);
    Require(query.TryReserveStringMemo(budget));
    {
        NegativeStringMemo denied(query, 64, 0);
        denied.RecordEmpty(0);
        Require(!denied.Contains(0));
    }
    query.ReleaseStringMemo(budget);
    {
        NegativeStringMemo first(query, 64, 0), other_dex(query, 64, 1);
        first.RecordEmpty(0);
        Require(first.Contains(0) == (budget >= 8));
        Require(!other_dex.Contains(0));
    }
    {
        QueryContext next(QueryKind::BatchFindMethodUsingStrings);
        NegativeStringMemo fresh(next, 64, 0);
        Require(!fresh.Contains(0));
    }

    std::vector<std::string_view> keywords{"needle"};
    acdat::AhoCorasickDoubleArrayTrie<std::string_view> trie;
    acdat::Builder<std::string_view>().Build(keywords, &trie);
    for (const std::string name : {"unique_short_negative", "repeated_negative", "repeated_positive", "over_budget", "no_visits", "one_visit_large_directory"}) {
        const size_t count = name == "unique_short_negative" ? 32768 : 64;
        std::vector<std::string> values;
        for (size_t i = 0; i < count; ++i)
            values.push_back((name == "repeated_positive" ? "needle" : "x") + std::to_string(i));
        const bool small_scope = name == "no_visits" || name == "one_visit_large_directory";
        const size_t directory = name == "over_budget" ? (budget / 8 + 1) * 64
                                : small_scope ? std::max(size_t{64}, budget / 8 * 64) : count;
        const size_t visits = name == "unique_short_negative" ? count
                             : name == "no_visits" ? 0 : small_scope ? 1 : 500000;
        for (int pair = 0; pair < 6; ++pair) {
            Result base{}, candidate{};
            if (pair % 2) {
                candidate = Run<true>(trie, values, directory, visits);
                base = Run<false>(trie, values, directory, visits);
            } else {
                base = Run<false>(trie, values, directory, visits);
                candidate = Run<true>(trie, values, directory, visits);
            }
            Require(base.hits == candidate.hits);
            const bool fits = (directory / 64 + (directory % 64 != 0)) * 8 <= budget;
            const auto expected_parses = name == "repeated_negative" && fits ? count : visits;
            Require(candidate.parses == expected_parses);
            std::printf("CHECK_MEMO {\"case\":\"%s\",\"pair\":%d,\"base_ns\":%llu,\"memo_ns\":%llu,\"base_parses\":%llu,\"memo_parses\":%llu,\"returned_hits\":%llu,\"passed\":true}\n",
                    name.c_str(), pair, (unsigned long long) base.ns, (unsigned long long) candidate.ns,
                    (unsigned long long) base.parses, (unsigned long long) candidate.parses, (unsigned long long) candidate.hits);
        }
    }
}

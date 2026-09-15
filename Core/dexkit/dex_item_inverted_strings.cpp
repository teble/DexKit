#include "dex_item.h"

#if DEXKIT_EXPERIMENT_INVERTED_STRINGS
#include "single_string_index.h"
#include <cstdio>

namespace dexkit {

bool DexItem::EnsureInvertedStrings() {
    // EnterQueryExecution(kUsingString) finishes forward-cache publication
    // before any find task can access this immutable reverse index.
    DEXKIT_CHECK((dex_flag.load(std::memory_order_acquire) & kUsingString) != 0);
    std::call_once(inverted_strings_once, [this] {
        inverted_strings.Build(strings.size(), reader.MethodIds().size(), method_using_string_ids);
        if (inverted_strings.Ready()) inverted_strings_ready.store(true, std::memory_order_release);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        std::fprintf(stderr, "BENCH_STRING_INVERSE_INDEX dex=%u ready=%d narrow=%d bytes=%zu scratch_bytes=%zu used=%u singletons=%zu edges=%zu\n",
                dex_id, inverted_strings.Ready(), inverted_strings.Narrow(), inverted_strings.Bytes(),
                strings.size() * sizeof(uint32_t) * 2, inverted_strings.UsedStrings(),
                inverted_strings.Singletons(), inverted_strings.Edges());
#endif
    });
    return inverted_strings.Ready();
}

bool DexItem::BuildStringCandidateGroups(
        acdat::AhoCorasickDoubleArrayTrie<std::string_view> &trie,
        const std::map<std::string_view, std::set<std::string_view>> &groups,
        const phmap::flat_hash_map<std::string_view, schema::StringMatchType> &types,
        bool classes, StringCandidateGroups &result) {
    const size_t entities = classes ? type_names.size() : reader.MethodIds().size();
    // Includes keyword planes, output groups, and two working bitmaps. Fall
    // back before building the index when either phase exceeds the budget.
    const auto bitmap_bytes = inverted_string::BitmapPlanBytes(entities, type_names.size(), types.size(), groups.size());
    if (!bitmap_bytes) return false;
    if (!EnsureInvertedStrings()) return false;
    phmap::flat_hash_map<std::string_view, size_t> ids;
    std::vector<inverted_string::Bits> planes;
    planes.reserve(types.size());
    for (const auto &[word, type] : types) {
        ids.emplace(word, planes.size());
        planes.emplace_back(entities);
    }
    std::vector<uint32_t> last_string(planes.size(), UINT32_MAX);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    uint64_t scans = 0, bytes = 0, accepted = 0, postings = 0;
#endif
    auto accept = [&](uint32_t string_id, std::string_view word) {
        const auto plane = ids.find(word)->second;
        if (last_string[plane] == string_id) return;
        last_string[plane] = string_id;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        ++accepted;
#endif
        inverted_strings.VisitRange(string_id, static_cast<size_t>(string_id) + 1, [&](uint32_t method) {
            const auto entity = classes ? reader.MethodIds()[method].class_idx : method;
            planes[plane].Set(entity);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            ++postings;
#endif
        });
    };
    inverted_strings.EachString([&](uint32_t string_id) {
        const auto value = strings[string_id];
        const auto hits = trie.ParseText(value);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        ++scans;
        bytes += value.size();
#endif
        for (const auto &hit : hits) {
            const auto type = types.find(hit.value)->second;
            bool match = false;
            switch (type) {
                case schema::StringMatchType::Contains: match = true; break;
                case schema::StringMatchType::StartWith: match = hit.begin == 0; break;
                case schema::StringMatchType::EndWith: match = hit.end == value.size(); break;
                case schema::StringMatchType::Equal: match = hit.begin == 0 && hit.end == value.size(); break;
                case schema::StringMatchType::SimilarRegex: abort();
            }
            if (match) accept(string_id, hit.value);
        }
        if (string_id == empty_string_id && types.contains("")) {
            DEXKIT_CHECK(types.find("")->second == schema::StringMatchType::Equal);
            accept(string_id, "");
        }
    });
    inverted_string::Bits any(entities);
    for (const auto &plane : planes) any.Or(plane);
    result.reserve(groups.size());
    for (const auto &[key, required] : groups) {
        // The original Batch matcher skips empty search_set even when this
        // group has no required keywords. Keep that observable condition.
        auto matched = any;
        for (auto word : required) {
            const auto found = ids.find(word);
            DEXKIT_CHECK(found != ids.end());
            matched.And(planes[found->second]);
        }
        result.emplace_back(key, std::move(matched));
    }
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    std::fprintf(stderr, "BENCH_STRING_INVERSE_QUERY dex=%u classes=%d strings=%llu bytes=%llu accepted=%llu postings=%llu bitmap_budget_bytes=%zu\n",
            dex_id, classes, static_cast<unsigned long long>(scans), static_cast<unsigned long long>(bytes),
            static_cast<unsigned long long>(accepted), static_cast<unsigned long long>(postings),
            *bitmap_bytes);
#endif
    return true;
}

} // namespace dexkit
#endif

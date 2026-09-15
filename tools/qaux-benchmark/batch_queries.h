#pragma once

#include "string_queries.h"

namespace batch_fixture {
using namespace dexkit;
using string_fixture::Atom;
using string_fixture::Builder;
using T = schema::StringMatchType;
inline constexpr int QueryCount = 16;

struct Group {
    std::string key;
    std::vector<Atom> atoms;
    bool null_list = false;
    bool composite = false;
};

inline std::vector<Group> Groups(int variant) {
    switch (variant) {
        case 0: return {};
        case 1: return {{"empty", {}}};
        case 2: return {{"empty", {}}, {"needle", {{"Needle", T::Contains}}},
                        {"absent", {{"AbsentEveryPool", T::Contains}}}};
        case 3: return {{"z-two", {{"Needle", T::Contains}, {"Second", T::Contains}}},
                        {"a-need", {{"Need", T::Contains}}},
                        {"duplicate", {{"Needle", T::Contains}, {"Needle", T::Contains}}},
                        {"overlap", {{"Need", T::Contains}, {"Needle", T::Contains}}},
                        {"miss", {{"Needle", T::Contains}, {"AbsentEveryPool", T::Contains}}}};
        case 4: return {{"empty-value", {{""}}}, {"equal", {{"Needle"}}},
                        {"both", {{"Needle"}, {"Second"}}}};
        case 5: return {{"prefix", {{"Needle", T::StartWith}}},
                        {"long", {{string_fixture::LongNeedle(), T::StartWith}}},
                        {"overlap", {{"Needle", T::StartWith}, {"Need", T::StartWith}}}};
        case 6: return {{"suffix", {{"Needle", T::EndWith}}},
                        {"two", {{"Needle", T::EndWith}, {"Second", T::EndWith}}}};
        case 7: return {{"exact", {{"^Needle$", T::SimilarRegex}}},
                        {"prefix", {{"^Second", T::SimilarRegex}}},
                        {"suffix", {{"tail$", T::SimilarRegex}}}};
        case 8: return {{"fold", {{"needle", T::Contains, true}}},
                        {"both", {{"needle", T::Contains, true}, {"second", T::Contains, true}}}};
        case 9: return {{"nul", {{"Needle\300\200tail"}}}, {"unicode", {{"\316\273", T::Contains}}},
                        {"surrogate", {{"\355\240\200"}}}, {"supplementary", {{"\355\240\275\355\270\200"}}},
                        {"del", {{"\177", T::Contains}}}};
        case 10: return {{"same", {{"Needle"}}}, {"other", {{"Needle"}}}, {"same", {{"Second"}}}};
        case 11: return {{"or", {}, false, true}, {"empty", {}}, {"null-list", {}, true},
                         {"plain", {{"Second"}}}};
        case 12: return {{"or", {}, false, true}, {"any-value", {{"", T::Equal, false, true}}}};
        case 13: case 14: return {{"equal", {{"Needle"}}}, {"empty", {}}};
        case 15: return {{"", {{"Needle"}}}, {"z", {{"Needle"}}}, {"", {{"Needle"}}}};
        default: std::abort();
    }
}

inline std::unique_ptr<Builder> Build(bool classes, const std::vector<Group> &groups,
                                      bool empty_classes = false, bool empty_methods = false) {
    auto b = std::make_unique<Builder>();
    std::vector<flatbuffers::Offset<schema::BatchUsingStringsMatcher>> all;
    for (const auto &group : groups) {
        auto strings = string_fixture::Strings(*b, group.atoms);
        if (group.null_list) strings = {};
        if (group.composite) {
            auto children = string_fixture::Strings(*b, {{"Needle"}, {"OnlySecond"}});
            schema::StringMatcherBuilder logical(*b);
            logical.add_any_of(children);
            auto node = logical.Finish();
            strings = b->CreateVector(std::vector{node});
        }
        all.push_back(schema::CreateBatchUsingStringsMatcher(*b, b->CreateString(group.key), strings));
    }
    auto matchers = b->CreateVector(all);
    auto no_classes = empty_classes ? b->CreateVector(std::vector<int64_t>{})
                                    : flatbuffers::Offset<flatbuffers::Vector<int64_t>>{};
    auto no_methods = empty_methods ? b->CreateVector(std::vector<int64_t>{})
                                    : flatbuffers::Offset<flatbuffers::Vector<int64_t>>{};
    if (classes) b->Finish(schema::CreateBatchFindClassUsingStrings(*b, 0, 0, false, no_classes, matchers));
    else b->Finish(schema::CreateBatchFindMethodUsingStrings(*b, 0, 0, false, no_classes, no_methods, matchers));
    return b;
}

inline auto Query(bool classes, int variant) {
    return Build(classes, Groups(variant), variant == 13 || (classes && variant == 14), variant == 14);
}

inline auto WorkQuery(bool classes, bool absent) {
    std::vector<Group> groups;
    for (int i = 0; i < 64; ++i) {
        Group group{"group-" + std::to_string(i), {}};
        if (absent) group.atoms.push_back({"AbsentEveryPool" + std::to_string(i), T::Contains});
        else {
            for (char c = 'a'; c <= 'g'; ++c) group.atoms.push_back({std::string(1, c), T::Contains});
            if (i % 16) group.atoms.push_back({"zz-missing-" + std::to_string(i), T::Contains});
        }
        groups.push_back(std::move(group));
    }
    return Build(classes, groups);
}

inline auto Run(DexKit &bridge, const Builder &query, bool classes) {
    return classes ? bridge.BatchFindClassUsingStrings(flatbuffers::GetRoot<schema::BatchFindClassUsingStrings>(query.GetBufferPointer()))
                   : bridge.BatchFindMethodUsingStrings(flatbuffers::GetRoot<schema::BatchFindMethodUsingStrings>(query.GetBufferPointer()));
}
}

#pragma once

#include "dexkit.h"
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace string_fixture {
using namespace dexkit;
using Builder = flatbuffers::FlatBufferBuilder;
inline std::string LongNeedle() { return "LongPrefix/" + std::string(192, 'x') + "/Needle"; }
inline constexpr int QueryCount = 34;

struct Atom {
    std::string value;
    schema::StringMatchType type = schema::StringMatchType::Equal;
    bool ignore_case = false;
    bool null_value = false;
};

inline std::vector<Atom> Atoms(int variant) {
    using T = schema::StringMatchType;
    switch (variant) {
        case 0: case 9: return {};
        case 1: return {{"Needle"}};
        case 2: return {{"Needle", T::StartWith}};
        case 3: return {{"Needle", T::Contains}};
        case 4: return {{"Needle", T::EndWith}};
        case 5: return {{"needle", T::Equal, true}};
        case 6: return {{"needle"}};
        case 7: return {{""}};
        case 8: return {{"", T::Equal, false, true}};
        case 10: return {{"\316\273Needle"}};
        case 11: return {{"\357\277\277"}};
        case 12: return {{"\355\240\200"}};
        case 13: return {{"\360\237\230\200"}};
        case 14: return {{"\355\240\275\355\270\200"}};
        case 15: return {{"prefix\177", T::StartWith}};
        case 16: return {{LongNeedle()}};
        case 17: case 27: return {{"OnlySecond"}};
        case 18: return {{"UnusedNeedle"}};
        case 19: return {{"AbsentEveryPool"}};
        case 20: return {{"Needle"}, {"Second"}};
        case 21: return {{"Needle", T::Contains}, {"Second"}};
        case 22: case 23: case 24: case 25: return {};
        case 26: return {{"Needle"}};
        case 28: return {{"^Needle$", T::SimilarRegex}};
        case 29: return {{"\177Needle"}};
        case 30: return {{std::string("Needle\0tail", 11)}};
        case 31: return {{"Needle\300\200tail"}};
        case 32: return {{"Needle", T::StartWith, true}};
        case 33: return {{"Needle"}, {"Needle"}};
        default: std::abort();
    }
}

inline auto Strings(Builder &b, const std::vector<Atom> &atoms) {
    std::vector<flatbuffers::Offset<schema::StringMatcher>> offsets;
    for (const auto &atom : atoms) {
        auto text = atom.null_value ? flatbuffers::Offset<flatbuffers::String>{} : b.CreateString(atom.value);
        offsets.push_back(schema::CreateStringMatcher(b, text, atom.type, atom.ignore_case));
    }
    return b.CreateVector(offsets);
}

inline flatbuffers::Offset<schema::ClassMatcher> NamedClass(Builder &b, const char *name) {
    const auto value = schema::CreateStringMatcher(b, b.CreateString(name), schema::StringMatchType::Equal);
    schema::ClassMatcherBuilder cls(b); cls.add_class_name(value); return cls.Finish();
}

inline flatbuffers::Offset<schema::ClassMatcher> ClassNode(Builder &b, int variant) {
    auto atoms = Atoms(variant);
    auto strings = Strings(b, atoms);
    std::vector<flatbuffers::Offset<schema::ClassMatcher>> all, any, none;
    if (variant == 22) { any = {ClassNode(b, 19), ClassNode(b, 17)}; }
    if (variant == 23) { none = {ClassNode(b, 1)}; all = {ClassNode(b, 3)}; }
    if (variant == 24) { all = {ClassNode(b, 1)}; any = {ClassNode(b, 19), ClassNode(b, 20)}; }
    if (variant == 25) all = {ClassNode(b, 1)};
    auto all_vec = b.CreateVector(all), any_vec = b.CreateVector(any), none_vec = b.CreateVector(none);
    auto name = variant == 26 ? schema::CreateStringMatcher(b, b.CreateString("strings.Sparse"), schema::StringMatchType::Equal)
                             : flatbuffers::Offset<schema::StringMatcher>{};
    schema::ClassMatcherBuilder cls(b);
    if (!atoms.empty() || variant == 9) cls.add_using_strings(strings);
    if (!all.empty()) cls.add_all_of(all_vec);
    if (!any.empty()) cls.add_any_of(any_vec);
    if (!none.empty()) cls.add_none_of(none_vec);
    cls.add_class_name(name); return cls.Finish();
}

inline flatbuffers::Offset<schema::MethodMatcher> MethodNode(Builder &b, int variant) {
    auto atoms = Atoms(variant);
    auto strings = Strings(b, atoms);
    std::vector<flatbuffers::Offset<schema::MethodMatcher>> all, any, none;
    if (variant == 22) { any = {MethodNode(b, 19), MethodNode(b, 17)}; }
    if (variant == 23) { none = {MethodNode(b, 1)}; all = {MethodNode(b, 3)}; }
    if (variant == 24) { all = {MethodNode(b, 1)}; any = {MethodNode(b, 19), MethodNode(b, 20)}; }
    auto owner = variant == 25 ? ClassNode(b, 1) : variant == 26 ? NamedClass(b, "strings.Sparse")
                            : flatbuffers::Offset<schema::ClassMatcher>{};
    auto all_vec = b.CreateVector(all), any_vec = b.CreateVector(any), none_vec = b.CreateVector(none);
    schema::MethodMatcherBuilder method(b);
    if (!atoms.empty() || variant == 9) method.add_using_strings(strings);
    if (!all.empty()) method.add_all_of(all_vec);
    if (!any.empty()) method.add_any_of(any_vec);
    if (!none.empty()) method.add_none_of(none_vec);
    method.add_declaring_class(owner); return method.Finish();
}

inline std::unique_ptr<Builder> Query(bool classes, int variant) {
    auto b = std::make_unique<Builder>();
    if (classes) {
        auto matcher = ClassNode(*b, variant);
        schema::FindClassBuilder query(*b); query.add_matcher(matcher); query.add_find_first(variant == 27);
        b->Finish(query.Finish());
    } else {
        auto matcher = MethodNode(*b, variant);
        schema::FindMethodBuilder query(*b); query.add_matcher(matcher); query.add_find_first(variant == 27);
        b->Finish(query.Finish());
    }
    return b;
}

inline std::unique_ptr<Builder> WorkQuery(bool classes, const std::string &needle,
        schema::StringMatchType type, bool sparse = false, bool multiple = false) {
    auto b = std::make_unique<Builder>();
    std::vector<Atom> atoms{{needle, type}};
    if (multiple) atoms.push_back({"Needle", schema::StringMatchType::Contains});
    auto strings = Strings(*b, atoms);
    if (classes) {
        auto name = sparse ? schema::CreateStringMatcher(*b, b->CreateString("strings.Sparse"), schema::StringMatchType::Equal)
                           : flatbuffers::Offset<schema::StringMatcher>{};
        schema::ClassMatcherBuilder cls(*b); cls.add_using_strings(strings); cls.add_class_name(name);
        auto matcher = cls.Finish();
        schema::FindClassBuilder query(*b); query.add_matcher(matcher); b->Finish(query.Finish());
    } else {
        auto owner = sparse ? NamedClass(*b, "strings.Sparse") : flatbuffers::Offset<schema::ClassMatcher>{};
        schema::MethodMatcherBuilder method(*b); method.add_using_strings(strings); method.add_declaring_class(owner);
        auto matcher = method.Finish();
        schema::FindMethodBuilder query(*b); query.add_matcher(matcher); b->Finish(query.Finish());
    }
    return b;
}
}

#pragma once

#include "dexkit.h"
#include <memory>
#include <string>
#include <vector>

namespace source_fixture {
using namespace dexkit;
using Builder = flatbuffers::FlatBufferBuilder;
inline std::string LongSource() { std::string s = "prefix-"; for (int i = 0; i < 128; ++i) s += "Long"; return s + ".java"; }
inline constexpr auto UtfSource = "\346\272\220\346\226\207\344\273\266.java";
inline constexpr auto MutfSource = "A\300\200B.java";

inline flatbuffers::Offset<schema::ClassMatcher> Source(Builder &b, const std::string &value,
        schema::StringMatchType type = schema::StringMatchType::Equal, bool ignore_case = false) {
    auto source = schema::CreateStringMatcher(b, b.CreateString(value), type, ignore_case);
    schema::ClassMatcherBuilder matcher(b);
    matcher.add_smali_source(source);
    return matcher.Finish();
}

inline std::unique_ptr<Builder> Query(int variant) {
    auto b = std::make_unique<Builder>();
    flatbuffers::Offset<schema::ClassMatcher> matcher;
    if (variant == 0) matcher = schema::CreateClassMatcher(*b);
    if (variant == 1) matcher = Source(*b, "Rare.java");
    if (variant == 2) matcher = Source(*b, "Never.java");
    if (variant == 3) matcher = Source(*b, "");
    if (variant == 4) matcher = Source(*b, "MiXeD.java");
    if (variant == 5) matcher = Source(*b, "mixed.JAVA", schema::StringMatchType::Equal, true);
    if (variant == 6) matcher = Source(*b, "mixed.JAVA");
    if (variant == 7) matcher = Source(*b, "MiX", schema::StringMatchType::StartWith);
    if (variant == 8) matcher = Source(*b, ".java", schema::StringMatchType::EndWith);
    if (variant == 9) matcher = Source(*b, "xEd", schema::StringMatchType::Contains, true);
    if (variant == 10) matcher = Source(*b, UtfSource);
    if (variant == 11) matcher = Source(*b, MutfSource);
    if (variant == 12) matcher = Source(*b, LongSource());
    if (variant == 13) matcher = Source(*b, "First.java");
    if (variant == 14) matcher = Source(*b, "Last.java");
    if (variant == 15) {
        auto any = b->CreateVector(std::vector{Source(*b, "Rare.java"), Source(*b, "Never.java")});
        auto none = b->CreateVector(std::vector{Source(*b, "MiXeD.java")});
        auto all = b->CreateVector(std::vector{Source(*b, ".java", schema::StringMatchType::EndWith)});
        schema::ClassMatcherBuilder composite(*b);
        composite.add_any_of(any); composite.add_none_of(none); composite.add_all_of(all);
        matcher = composite.Finish();
    }
    schema::FindClassBuilder query(*b);
    query.add_matcher(matcher);
    b->Finish(query.Finish());
    return b;
}
} // namespace source_fixture

#pragma once

#include "dexkit.h"
#include <memory>
#include <string>
#include <vector>

namespace invocation_fixture {
using namespace dexkit;
using Builder = flatbuffers::FlatBufferBuilder;

inline flatbuffers::Offset<schema::MethodMatcher> Named(Builder &b, const char *value,
        schema::StringMatchType type = schema::StringMatchType::Equal) {
    auto name = schema::CreateStringMatcher(b, b.CreateString(value), type);
    schema::MethodMatcherBuilder m(b);
    m.add_method_name(name);
    return m.Finish();
}

// One requirement with real AND/OR/NOT children, reused by both directions.
inline flatbuffers::Offset<schema::MethodMatcher> Nested(Builder &b, const char *hit, bool miss) {
    auto disjunction = b.CreateVector(std::vector{Named(b, "never-present"), Named(b, miss ? "also-absent" : hit)});
    schema::MethodMatcherBuilder any(b);
    any.add_any_of(disjunction);
    auto child = any.Finish();
    auto excluded = b.CreateVector(std::vector{Named(b, "excluded")});
    auto conjunction = b.CreateVector(std::vector{child});
    auto params = schema::CreateParametersMatcher(b, 0, schema::CreateIntRange(b, 0, 0));
    schema::MethodMatcherBuilder result(b);
    result.add_all_of(conjunction);
    result.add_none_of(excluded);
    result.add_parameters(params);
    return result.Finish();
}

// Cases retain general-solver semantics: absent/empty requirements, counts,
// Equal, duplicate positional witnesses, conflicts, late positive and miss.
inline std::unique_ptr<Builder> Query(bool callers, int variant) {
    auto b = std::make_unique<Builder>();
    std::vector<flatbuffers::Offset<schema::MethodMatcher>> requirements;
    if (variant == 0 || variant == 1 || variant == 2)
        requirements.push_back(Nested(*b, callers ? "zRun" : "zLate", variant == 1));
    if (variant == 3 || variant == 4) {
        requirements.push_back(Named(*b, callers ? "run" : "aEarly", schema::StringMatchType::StartWith));
        requirements.push_back(Named(*b, variant == 4 ? "impossible" : (callers ? "run" : "aEarly"),
                                     schema::StringMatchType::StartWith));
    }
    if (variant == 9) requirements.push_back(Named(*b, callers ? "run" : "aEarly", schema::StringMatchType::StartWith));
    if (variant == 10 || variant == 11) {
        auto name = schema::CreateStringMatcher(*b, b->CreateString(callers ? "zRun" : "zLate"), schema::StringMatchType::Equal);
        auto owner_name = schema::CreateStringMatcher(*b, b->CreateString("relations.Source1"), schema::StringMatchType::Equal);
        schema::ClassMatcherBuilder owner(*b);
        owner.add_class_name(owner_name);
        auto declaring = owner.Finish();
        schema::MethodMatcherBuilder unique(*b);
        unique.add_method_name(name);
        if (callers) unique.add_declaring_class(declaring);
        auto witness = unique.Finish();
        requirements.push_back(witness);
        // Both requirements have a witness, but only one target position.
        if (variant == 10) requirements.push_back(witness);
    }
    auto matchers = variant == 8 ? flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<schema::MethodMatcher>>>{}
                                : b->CreateVector(requirements);
    auto range = variant == 7 ? schema::CreateIntRange(*b, 0, 0)
               : variant == 6 ? schema::CreateIntRange(*b, 1, 1) : flatbuffers::Offset<schema::IntRange>{};
    auto relation = schema::CreateMethodsMatcher(*b, matchers,
        variant == 2 || variant == 5 ? schema::MatchType::Equal : schema::MatchType::Contains, range);
    schema::MethodMatcherBuilder method(*b);
    if (callers) method.add_method_callers(relation);
    else method.add_invoking_methods(relation);
    auto matcher = method.Finish();
    schema::FindMethodBuilder query(*b);
    query.add_matcher(matcher);
    b->Finish(query.Finish());
    return b;
}
} // namespace invocation_fixture

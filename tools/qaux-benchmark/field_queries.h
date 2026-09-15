#pragma once
#include "dexkit.h"
#include <memory>
#include <string>
#include <vector>

namespace field_fixture {
using namespace dexkit;
using Builder = flatbuffers::FlatBufferBuilder;
inline constexpr int QueryCount = 24;

inline flatbuffers::Offset<schema::FieldMatcher> Field(Builder &b, const char *name,
        const char *owner = nullptr, int reverse = 0) {
    auto named = schema::CreateStringMatcher(b, b.CreateString(name), schema::StringMatchType::Equal);
    flatbuffers::Offset<schema::ClassMatcher> declaring;
    if (owner) {
        auto class_name = schema::CreateStringMatcher(b, b.CreateString(owner), schema::StringMatchType::Equal);
        schema::ClassMatcherBuilder c(b); c.add_class_name(class_name); declaring = c.Finish();
    }
    auto methods = reverse ? schema::CreateMethodsMatcher(b, 0, schema::MatchType::Contains,
                                   schema::CreateIntRange(b, 1, INT32_MAX))
                           : flatbuffers::Offset<schema::MethodsMatcher>{};
    schema::FieldMatcherBuilder f(b);
    f.add_field_name(named);
    if (owner) f.add_declaring_class(declaring);
    if (reverse == 1) f.add_get_methods(methods);
    if (reverse == 2) f.add_put_methods(methods);
    return f.Finish();
}

inline flatbuffers::Offset<schema::MethodMatcher> Using(Builder &b,
        const std::vector<flatbuffers::Offset<schema::UsingFieldMatcher>> &requirements, bool absent = false) {
    auto uses = absent ? flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<schema::UsingFieldMatcher>>>{}
                       : b.CreateVector(requirements);
    schema::MethodMatcherBuilder m(b);
    if (!absent) m.add_using_fields(uses);
    return m.Finish();
}

inline std::unique_ptr<Builder> Query(bool classes, int variant, const char *select = nullptr, bool exact = false) {
    auto b = std::make_unique<Builder>();
    std::vector<flatbuffers::Offset<schema::UsingFieldMatcher>> requirements;
    const auto alpha = Field(*b, "alpha"), beta = Field(*b, "beta");
    const auto a = schema::CreateUsingFieldMatcher(*b, alpha);
    const auto g = schema::CreateUsingFieldMatcher(*b, alpha, schema::UsingType::Get);
    const auto p = schema::CreateUsingFieldMatcher(*b, alpha, schema::UsingType::Put);
    const auto bb = schema::CreateUsingFieldMatcher(*b, beta);
    if (variant == 2 || variant == 20 || variant == 23) requirements = {a};
    if (variant == 3) requirements = {g};
    if (variant == 4) requirements = {p};
    // A missing inner field intentionally preserves existing type-agnostic behavior.
    if (variant >= 5 && variant <= 7) requirements = {schema::CreateUsingFieldMatcher(*b, 0,
        variant == 5 ? schema::UsingType::Any : variant == 6 ? schema::UsingType::Get : schema::UsingType::Put)};
    if (variant == 8) requirements = {a, a};
    if (variant == 9) requirements = {g, p};
    if (variant == 10) requirements = {a, bb};
    if (variant == 11) requirements = {a, schema::CreateUsingFieldMatcher(*b, Field(*b, "never"))};
    if (variant == 12) requirements = {bb};
    if (variant == 13) requirements = {schema::CreateUsingFieldMatcher(*b, Field(*b, "alpha", "ufields.Absent"))};
    if (variant == 14) {
        auto choices = b->CreateVector(std::vector{alpha, beta});
        schema::FieldMatcherBuilder f(*b); f.add_any_of(choices); auto any = f.Finish();
        auto included = b->CreateVector(std::vector{any});
        auto excluded = b->CreateVector(std::vector{beta});
        schema::FieldMatcherBuilder result(*b); result.add_all_of(included); result.add_none_of(excluded);
        requirements = {schema::CreateUsingFieldMatcher(*b, result.Finish())};
    }
    if (variant == 15 || variant == 16)
        requirements = {schema::CreateUsingFieldMatcher(*b, Field(*b, "alpha", nullptr, variant - 14))};
    if (variant == 21) requirements = {schema::CreateUsingFieldMatcher(*b, Field(*b, "never"))};
    if (variant == 22) {
        auto wildcard = schema::CreateUsingFieldMatcher(*b, 0, schema::UsingType::Put);
        requirements = {wildcard, wildcard};
    }
    auto matcher = Using(*b, requirements, variant == 0);
    if (variant >= 17 && variant <= 19) {
        auto left = Using(*b, {a});
        auto right = Using(*b, {variant == 18 ? bb : g});
        auto children = b->CreateVector(variant == 17 ? std::vector{left} : std::vector{left, right});
        schema::MethodMatcherBuilder m(*b);
        if (variant == 17) m.add_none_of(children);
        if (variant == 18) m.add_any_of(children);
        if (variant == 19) m.add_all_of(children);
        matcher = m.Finish();
    }
    if (select || variant == 20 || variant == 23) {
        auto name = schema::CreateStringMatcher(*b, b->CreateString(select ? select : variant == 20 ? "unique" : "early00000"),
                        exact || variant == 20 || variant == 23 ? schema::StringMatchType::Equal : schema::StringMatchType::StartWith);
        auto children = b->CreateVector(std::vector{matcher});
        schema::MethodMatcherBuilder m(*b); m.add_method_name(name); m.add_all_of(children); matcher = m.Finish();
    }
    if (classes) {
        auto methods = schema::CreateMethodsMatcher(*b, b->CreateVector(std::vector{matcher}));
        schema::ClassMatcherBuilder c(*b); c.add_methods(methods); auto cls = c.Finish();
        schema::FindClassBuilder q(*b); q.add_matcher(cls); q.add_find_first(variant == 20); b->Finish(q.Finish());
    } else {
        schema::FindMethodBuilder q(*b); q.add_matcher(matcher); q.add_find_first(variant == 20); b->Finish(q.Finish());
    }
    return b;
}
}

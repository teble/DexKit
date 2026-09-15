#pragma once

#include "string_queries.h"

namespace string_admission_fixture {
using namespace string_fixture;
inline constexpr int QueryCount = 23;
inline constexpr int ClassQueryCount = 6;

inline std::unique_ptr<Builder> Query(int variant, bool absent, int restriction = 0) {
    using T = schema::StringMatchType;
    auto b = std::make_unique<Builder>();
    std::vector<Atom> atoms{{"Needle", T::Contains}, {"LongPrefix/", T::Contains}};
    std::string name;
    bool nested_name = false;
    switch (variant) {
        case 0: break;
        case 1: name = "m00000"; break;
        case 2: name = "sharedName"; break;
        case 3: name = "m00000"; nested_name = true; break;
        case 4: name = "sharedName"; nested_name = true; break;
        case 5: case 6: break;
        case 7: atoms = {{"OnlyOne"}}; name = "single"; break;
        case 8: atoms = {{"SinglePrefix/", T::StartWith}}; name = "single"; break;
        case 9: atoms = {{"ExactlyTwo"}}; name = "pairA"; break;
        case 10: atoms = {{"PairPrefix/", T::StartWith}}; name = "pairA"; break;
        case 11: atoms = {{"SameMethodPrefix/", T::StartWith}}; name = "single"; break;
        case 12: atoms = {{"PresentButUnreferenced"}}; name = "single"; break;
        case 13: atoms = {{"AbsentEveryPool"}}; name = "single"; break;
        case 14: break;
        case 15: case 16: atoms = {{"OnlyOne"}}; name = "single"; nested_name = true; break;
        case 17: atoms = {{"OnlyOne"}}; name = "wrongName"; break;
        case 18: atoms = {{"OnlyOne"}}; break;
        case 19: atoms = {{"OnlyOne"}}; break;
        case 20: break;
        case 21: break;
        case 22: atoms = {{"OnlyOne"}}; break;
        default: std::abort();
    }
    if (absent) atoms.front().value = "AbsentEveryPool";
    const auto strings = Strings(*b, atoms);
    const auto method_name = name.empty() ? flatbuffers::Offset<schema::StringMatcher>{}
        : schema::CreateStringMatcher(*b, b->CreateString(name), T::Equal);
    const auto flags = variant == 5 || variant == 21 || variant == 22
        ? schema::CreateAccessFlagsMatcher(*b, variant == 21 ? 0x100 : variant == 22 ? 1 : 8, schema::MatchType::Contains)
                                  : flatbuffers::Offset<schema::AccessFlagsMatcher>{};
    const auto returns = variant == 6 || variant == 14 || variant == 19 ? NamedClass(*b, "void")
                                                     : flatbuffers::Offset<schema::ClassMatcher>{};
    std::vector<flatbuffers::Offset<schema::MethodMatcher>> children;
    if (nested_name) {
        schema::MethodMatcherBuilder child(*b); child.add_method_name(method_name);
        children.push_back(child.Finish());
    }
    if (variant == 14 || variant == 19) {
        const auto never = schema::CreateStringMatcher(*b, b->CreateString("wrongName"), T::Equal);
        schema::MethodMatcherBuilder a(*b); a.add_method_name(never); children.push_back(a.Finish());
        schema::MethodMatcherBuilder c(*b); c.add_return_type(returns); children.push_back(c.Finish());
    }
    if (variant == 20) {
        const auto never = Strings(*b, {{"AbsentEveryPool"}});
        const auto single = schema::CreateStringMatcher(*b, b->CreateString("single"), T::Equal);
        schema::MethodMatcherBuilder a(*b); a.add_using_strings(never); children.push_back(a.Finish());
        schema::MethodMatcherBuilder c(*b); c.add_method_name(single); children.push_back(c.Finish());
    }
    const auto group = b->CreateVector(children);
    const auto empty_ids = b->CreateVector(std::vector<int64_t>{});
    const auto empty_packages = b->CreateVector(std::vector<flatbuffers::Offset<flatbuffers::String>>{});
    schema::MethodMatcherBuilder root(*b);
    if (variant != 20) root.add_using_strings(strings);
    if (!nested_name) root.add_method_name(method_name);
    root.add_access_flags(flags);
    if (variant == 6) root.add_return_type(returns);
    if (nested_name && variant != 15) root.add_all_of(group);
    if (variant == 14 || variant == 18 || variant == 19 || variant == 20) root.add_any_of(group);
    if (variant == 15) root.add_none_of(group);
    b->Finish(schema::CreateFindMethod(*b, restriction == 3 ? empty_packages : 0,
            restriction == 4 ? empty_packages : 0, false, restriction == 2 ? empty_ids : 0,
            restriction == 1 ? empty_ids : 0, restriction == 5, root.Finish()));
    return b;
}

inline std::unique_ptr<Builder> ClassQuery(int variant, bool absent) {
    using T = schema::StringMatchType;
    auto b = std::make_unique<Builder>();
    std::vector<Atom> atoms{{"OnlyOne"}};
    if (variant == 1) atoms = {{"SameMethodPrefix/", T::StartWith}};
    if (variant == 2 || variant == 4) atoms = {{"OneLargeClass"}};
    if (variant == 5) atoms = {{"ExactlyTwo"}, {"OnlyOne"}};
    if (absent) atoms.front().value = "AbsentEveryPool";
    const auto strings = Strings(*b, atoms);
    const auto flags = variant < 3 ? schema::CreateAccessFlagsMatcher(*b, 1, schema::MatchType::Contains)
                                 : flatbuffers::Offset<schema::AccessFlagsMatcher>{};
    flatbuffers::Offset<schema::MethodsMatcher> methods;
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<schema::ClassMatcher>>> none;
    if (variant == 3) none = b->CreateVector(std::vector{NamedClass(*b, "admission.Special")});
    if (variant == 4) {
        // The string witness is m00000, while this member witness is m00001.
        const auto name = schema::CreateStringMatcher(*b, b->CreateString("m00001"), T::Equal);
        schema::MethodMatcherBuilder child(*b); child.add_method_name(name);
        const auto members = b->CreateVector(std::vector{child.Finish()});
        methods = schema::CreateMethodsMatcher(*b, members);
    }
    schema::ClassMatcherBuilder root(*b);
    root.add_using_strings(strings); root.add_access_flags(flags); root.add_methods(methods); root.add_none_of(none);
    b->Finish(schema::CreateFindClass(*b, 0, 0, false, 0, false, root.Finish()));
    return b;
}

inline std::unique_ptr<Builder> WarmupQuery() {
    auto b = std::make_unique<Builder>();
    const auto strings = Strings(*b, {{"WarmupAbsent", schema::StringMatchType::Contains}});
    schema::MethodMatcherBuilder matcher(*b); matcher.add_using_strings(strings);
    b->Finish(schema::CreateFindMethod(*b, 0, 0, false, 0, 0, false, matcher.Finish()));
    return b;
}
}

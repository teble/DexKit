#include "string_queries.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace {
using namespace string_fixture;
using Clock = std::chrono::steady_clock;
void Require(bool condition) { if (!condition) std::abort(); }
int64_t Ns(Clock::time_point begin) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count();
}
struct Statistics {
    int64_t create_ns = 0, setup_ns = 0, first_ns = 0, repeated_ns = 0, close_ns = 0;
    int64_t positive_ns = 0, negative_ns = 0;
    uint64_t checksum = 0, returned = 0;
};
std::unique_ptr<Builder> NestedBroadQuery(bool absent) {
    auto b = std::make_unique<Builder>();
    auto strings = Strings(*b, {{absent ? "AbsentEveryPool" : "Needle", schema::StringMatchType::Contains}});
    std::vector<flatbuffers::Offset<schema::MethodMatcher>> children;
    for (auto value : {"LongPrefix/", "Needle"}) {
        auto nested = Strings(*b, {{value, schema::StringMatchType::Contains}});
        schema::MethodMatcherBuilder child(*b); child.add_using_strings(nested);
        children.push_back(child.Finish());
    }
    auto all = b->CreateVector(children);
    schema::MethodMatcherBuilder root(*b); root.add_using_strings(strings); root.add_all_of(all);
    b->Finish(schema::CreateFindMethod(*b, 0, 0, false, 0, 0, false, root.Finish()));
    return b;
}
void Consume(Statistics &s, std::unique_ptr<Builder> data, bool classes) {
    Require(data != nullptr);
    const auto visit = [&](const auto *values) {
        s.returned += values->size();
        for (auto value : *values)
            s.checksum += uint64_t(value->dex_id()) * 0x100000001ULL + value->id() + value->dex_descriptor()->size();
    };
    if (classes) visit(flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(data->GetBufferPointer())->classes());
    else visit(flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods());
}
Statistics Run(const char *apk, std::string_view mode, size_t repeats) {
    Statistics s;
    auto begin = Clock::now();
    auto bridge = std::make_unique<DexKit>(apk, 1);
    s.create_ns = Ns(begin);
    begin = Clock::now();
    bridge->SetThreadNum(4);
    const bool classes = mode == "string-class" || mode == "string-prefix-class";
    const bool sparse = mode == "string-sparse" || mode == "string-prefix-sparse";
    auto type = mode.starts_with("string-prefix") ? schema::StringMatchType::StartWith
              : mode == "string-contains" ? schema::StringMatchType::Contains : schema::StringMatchType::Equal;
    const auto needle = mode == "string-eq-long" || mode == "string-multiple"
                      || mode == "string-prefix-tail" || mode == "string-prefix-multiple" ? LongNeedle()
                      : mode == "string-prefix-long" ? std::string("LongPrefix/") : std::string("Needle");
    auto positive = mode == "string-nested-broad" ? NestedBroadQuery(false)
            : WorkQuery(classes, needle, type, sparse, mode == "string-multiple", mode == "string-prefix-multiple");
    auto negative = mode == "string-nested-broad" ? NestedBroadQuery(true)
            : WorkQuery(classes, "AbsentEveryPool", type, sparse, mode == "string-multiple", mode == "string-prefix-multiple");
    s.setup_ns = Ns(begin);
    for (size_t i = 0; i < repeats; ++i) {
        begin = Clock::now();
        for (auto *query : {positive.get(), negative.get()}) {
            auto leg = Clock::now();
            Consume(s, classes ? bridge->FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()))
                               : bridge->FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer())), classes);
            (query == positive.get() ? s.positive_ns : s.negative_ns) += Ns(leg);
        }
        (i ? s.repeated_ns : s.first_ns) += Ns(begin);
    }
    begin = Clock::now(); bridge.reset(); s.close_ns = Ns(begin);
    return s;
}
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    const std::string_view mode(argv[2]);
    Require(mode == "string-eq" || mode == "string-eq-long" || mode == "string-prefix"
        || mode == "string-prefix-long" || mode == "string-class" || mode == "string-sparse"
        || mode == "string-contains" || mode == "string-multiple" || mode == "string-prefix-tail"
        || mode == "string-prefix-multiple" || mode == "string-prefix-class" || mode == "string-prefix-sparse"
        || mode == "string-nested-broad");
    char *end = nullptr;
    auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && !*end && repeats > 0 && repeats <= 100000);
    auto begin = Clock::now(); auto s = Run(argv[1], mode, repeats); auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"positive_ns\":%lld,\"negative_ns\":%lld,\"checksum\":%llu,\"returned\":%llu}\n", argv[2], repeats,
        (long long)s.create_ns, (long long)s.setup_ns, (long long)s.first_ns, (long long)s.repeated_ns,
        (long long)s.close_ns, (long long)lifecycle, (long long)s.positive_ns, (long long)s.negative_ns,
        (unsigned long long)s.checksum, (unsigned long long)s.returned);
    return 0;
}

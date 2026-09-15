#include "dexkit.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>

namespace {
using namespace dexkit;
using Clock = std::chrono::steady_clock;
void Require(bool value, const char *message) {
    if (!value) { std::fprintf(stderr, "Relation workload failed: %s\n", message); std::abort(); }
}
int64_t Ns(Clock::time_point begin) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count();
}
struct Statistics {
    int64_t create_ns = 0, setup_ns = 0, first_ns = 0, repeated_ns = 0, close_ns = 0;
    int64_t positive_ns = 0, negative_ns = 0;
    int64_t forward_first_ns = 0, forward_repeated_ns = 0;
    int64_t reverse_first_ns = 0, reverse_repeated_ns = 0;
    uint64_t checksum = 0, returned = 0;
};
void Consume(Statistics &s, const std::unique_ptr<flatbuffers::FlatBufferBuilder> &data) {
    Require(data != nullptr, "result buffer");
    auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods();
    s.returned += methods->size();
    for (auto method : *methods)
        s.checksum += uint64_t(method->dex_id()) * 0x100000001ULL + method->id()
                      + method->dex_descriptor()->size();
}
Statistics Run(std::string_view apk, std::string_view mode, size_t repeats) {
    Statistics s;
    auto begin = Clock::now();
    auto bridge = std::make_unique<DexKit>(apk, 1);
    s.create_ns = Ns(begin);
    begin = Clock::now();
    bridge->SetThreadNum(4);
    flatbuffers::FlatBufferBuilder query;
    auto name = schema::CreateStringMatcher(query, query.CreateString("value"), schema::StringMatchType::Equal);
    schema::FieldMatcherBuilder field(query);
    field.add_field_name(name);
    auto field_matcher = field.Finish();
    auto fields = query.CreateVector(std::vector{schema::CreateUsingFieldMatcher(query, field_matcher)});
    schema::MethodMatcherBuilder method(query);
    method.add_using_fields(fields);
    auto matcher = method.Finish();
    schema::FindMethodBuilder find(query);
    find.add_matcher(matcher);
    query.Finish(find.Finish());
    auto data = bridge->GetFieldData("Lrelations/Target;->value:I");
    Require(data != nullptr, "target field");
    auto meta = flatbuffers::GetRoot<schema::FieldMeta>(data->GetBufferPointer());
    const int64_t field_id = (int64_t(meta->dex_id()) << 32) | uint32_t(meta->id());
    if (mode == "field-full-first") bridge->InitFullCache();
    s.setup_ns = Ns(begin);
    for (size_t iteration = 0; iteration < repeats; ++iteration) {
        begin = Clock::now();
        {
            auto positive = Clock::now();
            { auto result = bridge->FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query.GetBufferPointer())); Consume(s, result); }
            auto forward_ns = Ns(positive);
            s.positive_ns += forward_ns;
            (iteration ? s.forward_repeated_ns : s.forward_first_ns) += forward_ns;
            if (mode != "field-forward") {
                auto reverse = Clock::now();
                { auto result = bridge->FieldGetMethods(field_id); Consume(s, result); }
                { auto result = bridge->FieldPutMethods(field_id); Consume(s, result); }
                auto reverse_ns = Ns(reverse);
                s.negative_ns += reverse_ns;  // Printed separately as the second API leg.
                (iteration ? s.reverse_repeated_ns : s.reverse_first_ns) += reverse_ns;
            }
        }
        if (iteration == 0) s.first_ns = Ns(begin);
        else s.repeated_ns += Ns(begin);
    }
    begin = Clock::now();
    bridge.reset();
    s.close_ns = Ns(begin);
    return s;
}
}

int main(int argc, char **argv) {
    if (argc != 4) { std::fprintf(stderr, "Usage: dexkit_relation_workload fixture.apk field-forward|field-late|field-full-first repeats\n"); return 2; }
    std::string_view mode(argv[2]);
    Require(mode == "field-forward" || mode == "field-late" || mode == "field-full-first", "mode");
    char *end = nullptr;
    auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && !*end && repeats > 0 && repeats <= 100000, "repeats");
    auto begin = Clock::now();
    auto s = Run(argv[1], mode, repeats);
    auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"positive_ns\":%lld,\"negative_ns\":%lld,\"checksum\":%llu,\"returned\":%llu,"
        "\"forward_first_ns\":%lld,\"forward_repeated_ns\":%lld,\"reverse_first_ns\":%lld,\"reverse_repeated_ns\":%lld}\n", argv[2], repeats,
        (long long)s.create_ns, (long long)s.setup_ns, (long long)s.first_ns, (long long)s.repeated_ns,
        (long long)s.close_ns, (long long)lifecycle, (long long)s.positive_ns, (long long)s.negative_ns,
        (unsigned long long)s.checksum, (unsigned long long)s.returned,
        (long long)s.forward_first_ns, (long long)s.forward_repeated_ns,
        (long long)s.reverse_first_ns, (long long)s.reverse_repeated_ns);
}

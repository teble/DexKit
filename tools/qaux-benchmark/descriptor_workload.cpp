#include "dexkit.h"
#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include "benchmark_diagnostics.h"
#endif
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace dexkit;

void Require(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "Descriptor workload failed: %s\n", message); std::abort(); }
}
int64_t Ns(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}
struct Statistics {
    int64_t create_ns = 0, setup_ns = 0, first_ns = 0, repeated_ns = 0, close_ns = 0;
    int64_t positive_ns = 0, negative_ns = 0;
    uint64_t checksum = 0, returned = 0;
};

std::unique_ptr<flatbuffers::FlatBufferBuilder> InterfaceQuery(bool conflict) {
    auto builder = std::make_unique<flatbuffers::FlatBufferBuilder>();
    std::vector<flatbuffers::Offset<schema::ClassMatcher>> targets;
    for (int i = 0; i < 8; ++i) {
        const bool exact = conflict && i >= 6;
        auto name = builder->CreateString(exact ? "fixture.I0" : "fixture.I");
        auto string_matcher = schema::CreateStringMatcher(*builder, name,
                exact ? schema::StringMatchType::Equal : schema::StringMatchType::StartWith);
        schema::ClassMatcherBuilder matcher(*builder);
        matcher.add_class_name(string_matcher);
        targets.push_back(matcher.Finish());
    }
    auto interfaces = schema::CreateInterfacesMatcher(*builder, builder->CreateVector(targets));
    auto name = builder->CreateString("fixture.Implementor");
    auto string_matcher = schema::CreateStringMatcher(*builder, name, schema::StringMatchType::Equal);
    schema::ClassMatcherBuilder matcher(*builder);
    matcher.add_class_name(string_matcher);
    matcher.add_interfaces(interfaces);
    auto class_matcher = matcher.Finish();
    schema::FindClassBuilder query(*builder);
    query.add_matcher(class_matcher);
    builder->Finish(query.Finish());
    return builder;
}

Statistics Run(std::string_view apk, std::string_view mode, size_t repeats) {
    Statistics stats;
    auto begin = Clock::now();
    auto bridge = std::make_unique<DexKit>(apk, 1);
    stats.create_ns = Ns(begin);
    begin = Clock::now();
    bridge->SetThreadNum(4);
    std::vector<int64_t> ids, field_ids;
    std::string hit, miss;
    std::unique_ptr<flatbuffers::FlatBufferBuilder> positive, negative;
    if (mode == "interfaces") {
        positive = InterfaceQuery(false);
        negative = InterfaceQuery(true);
    } else if (mode.starts_with("output-sso")) {
        auto data = bridge->GetClassData("LA;");
        Require(data != nullptr, "dense short descriptor fixture");
        auto meta = flatbuffers::GetRoot<schema::ClassMeta>(data->GetBufferPointer());
        for (auto id : *meta->methods()) ids.push_back((int64_t(meta->dex_id()) << 32) | uint32_t(id));
        for (auto id : *meta->fields()) field_ids.push_back((int64_t(meta->dex_id()) << 32) | uint32_t(id));
        Require(ids.size() == 60000 && field_ids.size() == 60000, "dense fixture size");
        if (mode != "output-sso") {
            std::vector<int64_t> selected_methods, selected_fields;
            for (size_t i = 0; i < 1024; ++i) {
                const auto index = mode == "output-sso-prefix" ? i
                        : mode == "output-sso-shard" ? i * 32 : i * 53 % 60000;
                selected_methods.push_back(ids[index]);
                selected_fields.push_back(field_ids[index]);
            }
            ids = std::move(selected_methods); field_ids = std::move(selected_fields);
        }
    } else {
        auto data = bridge->GetClassData("Lfixture/Wide;");
        Require(data != nullptr, "wide fixture class");
        auto meta = flatbuffers::GetRoot<schema::ClassMeta>(data->GetBufferPointer());
        for (auto id : *meta->methods()) ids.push_back((int64_t(meta->dex_id()) << 32) | uint32_t(id));
        Require(ids.size() == 4096, "fixed wide class size");
        if (mode.starts_with("lookup")) {
            auto last = bridge->GetMethodByIds({ids.back()});
            auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(last->GetBufferPointer())->methods();
            hit = methods->Get(0)->dex_descriptor()->str();
            miss = "Lfixture/Wide;->__missing__" + hit.substr(hit.find('('));
            if (mode == "lookup-prefix") {
                const auto tail = hit.rfind("Lfixture/Tail");
                Require(tail != std::string::npos, "same-name overload fixture required");
                miss = hit.substr(0, tail) + "Lfixture/Tail_missing;)V";
            } else if (mode == "lookup-hot") {
                hit = "Lfixture/Api;->a()I";
                miss = "Lfixture/Api;->a()F";
                auto warm = bridge->GetMethodData(hit);
                Require(warm != nullptr, "narrow lookup fixture");
            }
        }
    }
    stats.setup_ns = Ns(begin);
    for (size_t iteration = 0; iteration < repeats; ++iteration) {
        begin = Clock::now();
        if (mode == "output" || mode.starts_with("output-sso")) {
            auto result = bridge->GetMethodByIds(ids);
            auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods();
            Require(methods->size() == ids.size(), "output count");
            for (auto method : *methods) {
                Require(mode == "output" ? method->dex_descriptor()->size() > 1024
                                         : method->dex_descriptor()->size() == 14, "descriptor fixture length");
                stats.checksum += method->id() + method->dex_descriptor()->size();
            }
            stats.returned += methods->size();
            if (mode.starts_with("output-sso")) {
                auto fields_result = bridge->GetFieldByIds(field_ids);
                auto fields = flatbuffers::GetRoot<schema::FieldMetaArrayHolder>(fields_result->GetBufferPointer())->fields();
                Require(fields->size() == field_ids.size(), "field output count");
                for (auto field : *fields) {
                    Require(field->dex_descriptor()->size() == 13, "short field descriptor");
                    stats.checksum += field->id() + field->dex_descriptor()->size();
                }
                stats.returned += fields->size();
            }
        } else if (mode.starts_with("lookup")) {
            const auto hit_begin = Clock::now();
            {
                auto result = bridge->GetMethodData(hit);
                Require(result != nullptr, "lookup hit");
                auto method = flatbuffers::GetRoot<schema::MethodMeta>(result->GetBufferPointer());
                Require(method->dex_descriptor()->string_view() == hit, "lookup descriptor identity");
                if (mode != "lookup-hot") Require(method->id() == uint32_t(ids.back()), "wide lookup identity");
                stats.checksum += method->id() + method->dex_descriptor()->size();
                ++stats.returned;
            }
            stats.positive_ns += Ns(hit_begin);
            const auto miss_begin = Clock::now();
            { Require(bridge->GetMethodData(miss) == nullptr, "lookup miss"); }
            stats.negative_ns += Ns(miss_begin);
        } else {
            for (auto *query : {positive.get(), negative.get()}) {
                const auto query_begin = Clock::now();
                const bool should_match = query == positive.get();
                {
                    auto result = bridge->FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()));
                    auto classes = flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(result->GetBufferPointer())->classes();
                    Require(classes->size() == size_t(should_match), "one-to-one interface matching");
                    if (should_match) {
                        Require(classes->Get(0)->interfaces()->size() == 8, "interface output list");
                        stats.checksum += classes->Get(0)->id() + classes->Get(0)->interfaces()->size();
                    }
                    stats.returned += classes->size();
                }
                (should_match ? stats.positive_ns : stats.negative_ns) += Ns(query_begin);
            }
        }
        // Result buffer destruction occurs before taking this sample.
        const auto elapsed = Ns(begin);
        if (iteration == 0) stats.first_ns = elapsed;
        else stats.repeated_ns += elapsed;
    }
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    BenchmarkDiagnostics::Dump(*bridge, "workload-complete");
#endif
    begin = Clock::now();
    bridge.reset();
    stats.close_ns = Ns(begin);
    // Remaining setup buffers are destroyed on return and charged by main.
    return stats;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        std::fprintf(stderr, "Usage: dexkit_descriptor_workload symbols.apk output|output-sso[-prefix|-scattered|-shard]|lookup|lookup-prefix|lookup-hot|interfaces repeats\n");
        return 2;
    }
    const std::string_view mode(argv[2]);
    Require(mode == "output" || mode == "output-sso" || mode == "output-sso-prefix"
            || mode == "output-sso-scattered" || mode == "output-sso-shard"
            || mode == "lookup" || mode == "lookup-prefix" || mode == "lookup-hot" || mode == "interfaces", "mode");
    char *end = nullptr;
    const auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && *end == '\0' && repeats > 0 && repeats <= 100000, "repeat count");
    const auto begin = Clock::now();
    const auto stats = Run(argv[1], mode, repeats);
    const auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"positive_ns\":%lld,\"negative_ns\":%lld,\"checksum\":%llu,\"returned\":%llu}\n", argv[2], (unsigned long long) repeats,
        (long long) stats.create_ns, (long long) stats.setup_ns, (long long) stats.first_ns,
        (long long) stats.repeated_ns, (long long) stats.close_ns, (long long) lifecycle,
        (long long) stats.positive_ns, (long long) stats.negative_ns,
        (unsigned long long) stats.checksum, (unsigned long long) stats.returned);
}

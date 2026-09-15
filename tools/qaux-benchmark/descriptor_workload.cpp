#include "dexkit.h"
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
    std::vector<int64_t> ids;
    std::string hit, miss;
    std::unique_ptr<flatbuffers::FlatBufferBuilder> positive, negative;
    if (mode == "interfaces") {
        positive = InterfaceQuery(false);
        negative = InterfaceQuery(true);
    } else {
        auto data = bridge->GetClassData("Lfixture/Wide;");
        Require(data != nullptr, "wide fixture class");
        auto meta = flatbuffers::GetRoot<schema::ClassMeta>(data->GetBufferPointer());
        for (auto id : *meta->methods()) ids.push_back((int64_t(meta->dex_id()) << 32) | uint32_t(id));
        Require(ids.size() == 4096, "fixed wide class size");
        if (mode == "lookup") {
            auto last = bridge->GetMethodByIds({ids.back()});
            auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(last->GetBufferPointer())->methods();
            hit = methods->Get(0)->dex_descriptor()->str();
            miss = "Lfixture/Wide;->__missing__" + hit.substr(hit.find('('));
        }
    }
    stats.setup_ns = Ns(begin);
    for (size_t iteration = 0; iteration < repeats; ++iteration) {
        begin = Clock::now();
        if (mode == "output") {
            auto result = bridge->GetMethodByIds(ids);
            auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods();
            Require(methods->size() == ids.size(), "output count");
            for (auto method : *methods) {
                Require(method->dex_descriptor()->size() > 1024, "long descriptor fixture");
                stats.checksum += method->id() + method->dex_descriptor()->size();
            }
            stats.returned += methods->size();
        } else if (mode == "lookup") {
            auto result = bridge->GetMethodData(hit);
            Require(result != nullptr, "wide lookup hit");
            auto method = flatbuffers::GetRoot<schema::MethodMeta>(result->GetBufferPointer());
            Require(method->id() == uint32_t(ids.back()) && method->dex_descriptor()->size() == hit.size(), "wide lookup identity");
            Require(bridge->GetMethodData(miss) == nullptr, "wide lookup miss");
            stats.checksum += method->id() + method->dex_descriptor()->size();
            ++stats.returned;
        } else {
            for (auto *query : {positive.get(), negative.get()}) {
                auto result = bridge->FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()));
                auto classes = flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(result->GetBufferPointer())->classes();
                const bool should_match = query == positive.get();
                Require(classes->size() == size_t(should_match), "one-to-one interface matching");
                if (should_match) {
                    Require(classes->Get(0)->interfaces()->size() == 8, "interface output list");
                    stats.checksum += classes->Get(0)->id() + classes->Get(0)->interfaces()->size();
                }
                stats.returned += classes->size();
            }
        }
        // Result buffer destruction occurs before taking this sample.
        const auto elapsed = Ns(begin);
        if (iteration == 0) stats.first_ns = elapsed;
        else stats.repeated_ns += elapsed;
    }
    begin = Clock::now();
    bridge.reset();
    stats.close_ns = Ns(begin);
    // Remaining setup buffers are destroyed on return and charged by main.
    return stats;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        std::fprintf(stderr, "Usage: dexkit_descriptor_workload symbols.apk output|lookup|interfaces repeats\n");
        return 2;
    }
    const std::string_view mode(argv[2]);
    Require(mode == "output" || mode == "lookup" || mode == "interfaces", "mode");
    char *end = nullptr;
    const auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && *end == '\0' && repeats > 0 && repeats <= 100000, "repeat count");
    const auto begin = Clock::now();
    const auto stats = Run(argv[1], mode, repeats);
    const auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"checksum\":%llu,\"returned\":%llu}\n", argv[2], (unsigned long long) repeats,
        (long long) stats.create_ns, (long long) stats.setup_ns, (long long) stats.first_ns,
        (long long) stats.repeated_ns, (long long) stats.close_ns, (long long) lifecycle,
        (unsigned long long) stats.checksum, (unsigned long long) stats.returned);
}

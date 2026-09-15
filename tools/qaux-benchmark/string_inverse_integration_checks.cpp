#include "batch_queries.h"
#include "benchmark_diagnostics.h"
#include "dex_item.h"
#include <array>
#include <latch>
#include <thread>

namespace {
using namespace string_fixture;
void Require(bool value) { if (!value) std::abort(); }
std::string Bytes(const Builder &data) {
    return {reinterpret_cast<const char *>(data.GetBufferPointer()), data.GetSize()};
}

std::unique_ptr<Builder> Nested(bool classes, int variant) {
    auto b = std::make_unique<Builder>();
    const auto root_strings = Strings(*b, {{"Needle", schema::StringMatchType::Contains}});
    const auto second_strings = Strings(*b, {{variant == 1 && classes ? "Absent" : "Second"}});
    if (classes) {
        schema::ClassMatcherBuilder child(*b); child.add_using_strings(second_strings);
        const auto none = b->CreateVector(std::vector{child.Finish()});
        schema::ClassMatcherBuilder root(*b); root.add_using_strings(root_strings); root.add_none_of(none);
        b->Finish(schema::CreateFindClass(*b, 0, 0, false, 0, false, root.Finish()));
    } else {
        schema::MethodMatcherBuilder child(*b);
        child.add_using_strings(variant == 3 || variant == 4 ? root_strings : second_strings);
        const auto children = b->CreateVector(std::vector{child.Finish()});
        const auto invoking = variant >= 4 ? schema::CreateMethodsMatcher(*b, children)
                                          : flatbuffers::Offset<schema::MethodsMatcher>{};
        schema::MethodMatcherBuilder root(*b); root.add_using_strings(root_strings);
        if (variant == 1 || variant == 3) root.add_none_of(children);
        if (variant == 2) root.add_all_of(children);
        if (variant >= 4) root.add_invoking_methods(invoking);
        b->Finish(schema::CreateFindMethod(*b, 0, 0, false, 0, 0, false, root.Finish()));
    }
    return b;
}

std::string Collect(DexKit &bridge) {
    std::string output;
    for (bool classes : {false, true}) {
        for (int variant = 0; variant < (classes ? 2 : 6); ++variant) {
            auto query = Nested(classes, variant);
            auto result = classes ? bridge.FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()))
                                  : bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
            const auto count = classes ? flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(result->GetBufferPointer())->classes()->size()
                                       : flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods()->size();
            const std::array<uint32_t, 6> method_counts{4, 3, 1, 0, 0, 1};
            Require(count == (classes ? (variant == 0 ? 1U : 2U) : method_counts[variant]));
            const uint32_t size = result->GetSize();
            for (unsigned i = 0; i < 4; ++i) output.push_back(char(size >> (i * 8)));
            output += Bytes(*result);
        }
    }
    return output;
}
}

void dexkit::BenchmarkDiagnostics::CheckInvertedStrings(std::string_view apk) {
    DexKit reference(apk, 1); reference.SetThreadNum(4); reference.InitFullCache();
    const auto expected = Collect(reference);
    auto ordinary = Query(false, 3);
    auto batch = batch_fixture::Query(false, 3);
    auto run_ordinary = [&](DexKit &bridge) {
        return Bytes(*bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(ordinary->GetBufferPointer())));
    };
    auto run_batch = [&](DexKit &bridge) { return Bytes(*batch_fixture::Run(bridge, *batch, false)); };
    const auto expected_ordinary = run_ordinary(reference), expected_batch = run_batch(reference);
    for (bool warm : {false, true}) {
        DexKit bridge(apk, 1); bridge.SetThreadNum(4);
        if (warm) bridge.InitFullCache();
        Require(Collect(bridge) == expected);
        Require(Collect(bridge) == expected);
        // The reference binding must resolve into DEX 1 at the same numeric
        // ID as the matching root caller in DEX 0 (a TLS identity trap).
        bool witnessed = false;
        for (const auto &binding : bridge.dex_items[0]->method_cross_info)
            witnessed |= binding && binding->first == 1 && binding->second == 1;
        Require(witnessed);
    }
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        DexKit bridge(apk, 1); bridge.SetThreadNum(4);
        {
            auto forward = bridge.EnterQueryExecution(kUsingString);
            for (const auto &dex : bridge.dex_items) {
                Require((dex->dex_flag.load(std::memory_order_acquire) & kUsingString) != 0);
#if DEXKIT_EXPERIMENT_INVERTED_STRINGS
                Require(!dex->inverted_strings.Ready());
#endif
            }
        }
        std::fprintf(stderr, "INVERSE_FIRST_CONCURRENT_BEGIN attempt=%u\n", attempt);
        std::latch start(1);
        std::thread a([&] { start.wait(); Require(run_ordinary(bridge) == expected_ordinary); });
        std::thread b([&] { start.wait(); Require(run_batch(bridge) == expected_batch); });
        start.count_down(); a.join(); b.join();
#if DEXKIT_EXPERIMENT_INVERTED_STRINGS
        for (const auto &dex : bridge.dex_items) Require(dex->inverted_strings.Ready());
#endif
        std::fprintf(stderr, "INVERSE_FIRST_CONCURRENT_END attempt=%u\n", attempt);
        Require(Collect(bridge) == expected);
    }
    Require(std::fwrite(expected.data(), 1, expected.size(), stdout) == expected.size());
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    dexkit::BenchmarkDiagnostics::CheckInvertedStrings(argv[1]);
}

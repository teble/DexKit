#include "invocation_queries.h"
#include "benchmark_diagnostics.h"
#include "dex_item.h"
#include "analyze.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>

namespace {
using namespace invocation_fixture;
void Require(bool condition) { if (!condition) std::abort(); }
std::string Collect(DexKit &bridge, bool callers, bool log_counts = false) {
    std::string out;
    for (int variant = 0; variant < 14; ++variant) {
        auto query = Query(callers, variant);
        auto data = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
        Require(data != nullptr);
        auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods();
        if (variant == 0 || variant == 11 || variant == 12) Require(methods->size() != 0);
        if (variant == 1 || variant == 4 || variant == 10 || variant == 13) Require(methods->size() == 0);
        if (log_counts) std::fprintf(stderr, "CHECK_INVOCATION_CASE {\"callers\":%s,\"variant\":%d,\"returned\":%u}\n",
                                     callers ? "true" : "false", variant, methods->size());
        uint32_t length = data->GetSize();
        for (size_t i = 0; i < 4; ++i) out.push_back(char(length >> (8 * i)));
        out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), length);
    }
    return out;
}
std::string CollectNestedFieldCaller(DexKit &bridge) {
    auto query = NestedFieldCallerQuery();
    auto data = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
    Require(data != nullptr);
    Require(flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods()->size() != 0);
    uint32_t length = data->GetSize();
    std::string out;
    for (size_t i = 0; i < 4; ++i) out.push_back(char(length >> (8 * i)));
    out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), length);
    return out;
}
}

void dexkit::BenchmarkDiagnostics::CheckInvocations(std::string_view apk, bool dump) {
    DexKit reference(apk, 1);
    reference.SetThreadNum(4);
    reference.InitFullCache();
    const auto forward = Collect(reference, false, true), reverse = Collect(reference, true, true);
    const auto nested_field = CollectNestedFieldCaller(reference);
    // Compare the exact serial judge sequence against fixture names and rows.
    // A rejected count must not even enter the judge, in either implementation.
    for (bool callers : {false, true}) {
        auto method = reference.GetMethodData(callers ? "Lrelations/Target;->zLate()V" : "Lrelations/Source1;->run00000()V");
        Require(method != nullptr);
        const auto *meta = flatbuffers::GetRoot<schema::MethodMeta>(method->GetBufferPointer());
        auto *item = reference.dex_items[meta->dex_id()].get();
        std::vector<int64_t> targets;
        if (callers) for (auto [dex, id] : item->method_caller_ids[meta->id()]) targets.push_back((int64_t(dex) << 32) | id);
        else for (auto id : item->method_invoking_ids[meta->id()]) targets.push_back((int64_t(item->dex_id) << 32) | id);
        Require(!targets.empty());
        for (int variant : {0, 1, 12, 13}) {
            auto query = Query(callers, variant);
            const auto *matcher = flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer())->matcher();
            std::vector<int64_t> expected, actual;
            if (variant != 13) for (auto encoded : targets) {
                expected.push_back(encoded);
                const auto &target = reference.dex_items[encoded >> 32];
                const auto name = target->strings[target->reader.MethodIds()[uint32_t(encoded)].name_idx];
                if (variant == 12 && name.starts_with(callers ? "run" : "aEarly")) break;
                if (variant == 0 && name == (callers ? "zRun" : "zLate")) break;
            }
            QueryContext context(QueryKind::FindMethod);
            auto binding = context.BindToCurrentThread();
            RelationJudgeTrace::current = &actual;
            const bool matched = callers ? item->IsCallMethodsMatched(meta->id(), matcher->method_callers())
                                         : item->IsInvokingMethodsMatched(meta->id(), matcher->invoking_methods());
            RelationJudgeTrace::current = nullptr;
            Require(matched == (variant == 0 || variant == 12));
            Require(actual == expected);
            std::fprintf(stderr, "CHECK_RELATION_JUDGES {\"callers\":%s,\"variant\":%d,\"calls\":%zu,\"ordered\":true}\n",
                         callers ? "true" : "false", variant, actual.size());
        }
    }
    {
        DexKit bridge(apk, 1);
        bridge.SetThreadNum(4);
        {
            auto guard = bridge.EnterQueryExecution(kFieldIdentity | kMethodUsingField);
#if DEXKIT_EXPERIMENT_FIELD_IDENTITY_SPLIT
            for (const auto &item : bridge.dex_items) {
                Require(item->NeedInitCache(kRwFieldMethod));
                Require(item->field_get_method_ids.empty() && item->field_put_method_ids.empty());
            }
#endif
        }
        Require(CollectNestedFieldCaller(bridge) == nested_field);
        for (const auto &item : bridge.dex_items) Require(!item->NeedInitCache(kRwFieldMethod));
        Require(CollectNestedFieldCaller(bridge) == nested_field);
        bridge.InitFullCache();
        Require(CollectNestedFieldCaller(bridge) == nested_field);
    }
    // Public invocation queries request both domains. Exercise the separate
    // internal admission as well, including a caller queued behind a live span.
    for (bool queued : {false, true}) {
        DexKit bridge(apk, 1);
        bridge.SetThreadNum(4);
        std::vector<std::vector<std::span<const InvokeOperandId>>> held(bridge.dex_items.size());
        std::thread caller;
        {
            auto guard = bridge.EnterQueryExecution(kMethodInvoking);
            for (size_t dex = 0; dex < bridge.dex_items.size(); ++dex) {
                const auto &item = bridge.dex_items[dex];
                Require(!item->NeedInitCache(kMethodInvoking));
                Require(item->NeedInitCache(kCallerMethod) && item->method_caller_ids.empty());
                for (size_t method = 0; method < item->reader.MethodIds().size(); ++method) {
                    const auto &row = item->method_invoking_ids[method];
                    const auto &expected = reference.dex_items[dex]->method_invoking_ids[method];
                    Require(std::equal(row.begin(), row.end(), expected.begin(), expected.end()));
                    held[dex].emplace_back(row.data(), row.size());
                }
            }
            if (queued) {
                caller = std::thread([&] { Require(Collect(bridge, true) == reverse); });
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (true) {
                    bool pending;
                    {
                        std::lock_guard lock(bridge.query_execution_mutex);
                        pending = (bridge.pending_warmup_flags & kCallerMethod) != 0;
                    }
                    if (pending) break;
                    Require(std::chrono::steady_clock::now() < deadline);
                    std::this_thread::yield();
                }
            }
        }
        if (queued) caller.join();
        else Require(Collect(bridge, true) == reverse);
        bridge.InitFullCache();
        for (size_t dex = 0; dex < bridge.dex_items.size(); ++dex) {
            for (size_t method = 0; method < held[dex].size(); ++method) {
                const auto &row = bridge.dex_items[dex]->method_invoking_ids[method];
                const auto &expected = reference.dex_items[dex]->method_invoking_ids[method];
                Require(row.data() == held[dex][method].data() && row.size() == held[dex][method].size());
                Require(std::equal(held[dex][method].begin(), held[dex][method].end(), expected.begin(), expected.end()));
            }
        }
        Require(Collect(bridge, false) == forward);
        Require(Collect(bridge, true) == reverse);
    }
    for (int sequence = 0; sequence < 3; ++sequence) {
        DexKit bridge(apk, 1);
        bridge.SetThreadNum(4);
        if (sequence == 1) Require(Collect(bridge, true) == reverse);
        if (sequence == 2) {
            std::latch start(1);
            std::thread a([&] { start.wait(); Require(Collect(bridge, false) == forward); });
            std::thread b([&] { start.wait(); Require(Collect(bridge, true) == reverse); });
            start.count_down(); a.join(); b.join();
        }
        Require(Collect(bridge, false) == forward);
        Require(Collect(bridge, true) == reverse);
        bridge.InitFullCache();
        Require(Collect(bridge, false) == forward);
        Require(Collect(bridge, true) == reverse);
    }
    if (dump) {
        for (auto *bytes : {&forward, &reverse, &nested_field})
            Require(std::fwrite(bytes->data(), 1, bytes->size(), stdout) == bytes->size());
    } else std::puts("CHECK_INVOCATIONS {\"cases\":29,\"sequences\":6,\"passed\":true}");
}

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 3 && std::string_view(argv[1]) == "--dump")) return 2;
    dexkit::BenchmarkDiagnostics::CheckInvocations(argv[argc - 1], argc == 3);
}

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
std::string Collect(DexKit &bridge, bool callers) {
    std::string out;
    for (int variant = 0; variant < 12; ++variant) {
        auto query = Query(callers, variant);
        auto data = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
        Require(data != nullptr);
        auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods();
        if (variant == 0 || variant == 11) Require(methods->size() != 0);
        if (variant == 1 || variant == 4 || variant == 10) Require(methods->size() == 0);
        uint32_t length = data->GetSize();
        for (size_t i = 0; i < 4; ++i) out.push_back(char(length >> (8 * i)));
        out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), length);
    }
    return out;
}
}

void dexkit::BenchmarkDiagnostics::CheckInvocations(std::string_view apk, bool dump) {
    DexKit reference(apk, 1);
    reference.SetThreadNum(4);
    reference.InitFullCache();
    const auto forward = Collect(reference, false), reverse = Collect(reference, true);
    // Public invocation queries request both domains. Exercise the separate
    // internal admission as well, including a caller queued behind a live span.
    for (bool queued : {false, true}) {
        DexKit bridge(apk, 1);
        bridge.SetThreadNum(4);
        std::vector<std::vector<std::span<const uint32_t>>> held(bridge.dex_items.size());
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
        for (auto *bytes : {&forward, &reverse})
            Require(std::fwrite(bytes->data(), 1, bytes->size(), stdout) == bytes->size());
    } else std::puts("CHECK_INVOCATIONS {\"cases\":24,\"sequences\":5,\"passed\":true}");
}

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 3 && std::string_view(argv[1]) == "--dump")) return 2;
    dexkit::BenchmarkDiagnostics::CheckInvocations(argv[argc - 1], argc == 3);
}

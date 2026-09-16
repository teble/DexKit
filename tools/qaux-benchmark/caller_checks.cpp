#include "benchmark_diagnostics.h"
#include "dexkit.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>

namespace {
using namespace dexkit;
using Pair = std::pair<uint16_t, uint32_t>;
using Rows = std::vector<std::vector<std::vector<Pair>>>;
void Require(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "Caller check failed: %s\n", message); std::abort(); }
}
void Append(std::string &out, uint64_t value, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) { out.push_back(char(value & 255)); value >>= 8; }
}
std::string Collect(DexKit &bridge, const std::vector<int64_t> &ids) {
    std::string result;
    for (auto id : ids) {
        auto data = bridge.GetCallMethods(id);
        Require(data && data->GetSize() <= UINT32_MAX, "public caller buffer");
        Append(result, data->GetSize(), 4);
        result.append(reinterpret_cast<const char *>(data->GetBufferPointer()), data->GetSize());
    }
    return result;
}
}

void dexkit::BenchmarkDiagnostics::CheckCallers(std::string_view apk, bool dump) {
    DexKit reference(apk, 1);
    reference.SetThreadNum(1);
    reference.InitFullCache();
    Rows expected(reference.dex_items.size());
    std::vector<int64_t> ids;
    for (size_t dex = 0; dex < reference.dex_items.size(); ++dex) {
        const auto &item = *reference.dex_items[dex];
        expected[dex].resize(item.reader.MethodIds().size());
        for (uint32_t method = 0; method < expected[dex].size(); ++method) {
            ids.push_back((int64_t(dex) << 32) | method);
            for (auto edge : item.method_caller_ids[method]) expected[dex][method].emplace_back(edge.first, edge.second);
            if (item.method_cross_info[method]) Require(expected[dex][method].empty(), "transferred raw source row stays empty");
            const auto &raw = item.reader.MethodIds()[method];
            if (!item.type_def_flag[raw.class_idx] && item.type_names[raw.class_idx] == "Lcaller/Target;"
                    && item.strings[raw.name_idx] == "bUnused") {
                const auto &binding = item.method_cross_info[method];
                Require(binding.has_value() && expected[dex][method].empty(), "zero-count reference still resolves");
                std::fprintf(stderr, "CHECK_CALLER_ZERO_BINDING [%u,%u,%u,%u]\n", item.dex_id, method, binding->first, binding->second);
            }
        }
    }
    const auto expected_bytes = Collect(reference, ids);
    const auto check_rows = [&](const DexKit &bridge) {
        Require(bridge.dex_items.size() == expected.size(), "DEX count");
        for (size_t dex = 0; dex < expected.size(); ++dex) {
            const auto &item = *bridge.dex_items[dex];
#if DEXKIT_EXPERIMENT_COMPACT_CALLERS
            Require(item.method_caller_ids.BuildCapacityBytes() == 0, "count/cursor allocation released");
            Require(item.pending_aggregate_method_work_items.capacity() == 0, "import allocation released");
#endif
            for (size_t method = 0; method < expected[dex].size(); ++method) {
                const auto &row = item.method_caller_ids[method];
                Require(row.size() == expected[dex][method].size(), "raw caller count");
                for (size_t i = 0; i < row.size(); ++i)
                    Require(row[i].first == expected[dex][method][i].first && row[i].second == expected[dex][method][i].second,
                            "raw caller order and duplicates");
                if (item.method_cross_info[method]) Require(row.empty(), "source reference does not alias final target");
                const auto &raw = item.reader.MethodIds()[method];
                if (!item.type_def_flag[raw.class_idx] && item.type_names[raw.class_idx] == "Lcaller/Target;"
                        && item.strings[raw.name_idx] == "bUnused") {
                    Require(item.method_cross_info[method] == reference.dex_items[dex]->method_cross_info[method], "zero-count binding survives every initialization order");
                }
            }
        }
    };
    check_rows(reference);
    for (int workers : {1, 4}) {
        for (int sequence = 0; sequence < 5; ++sequence) {
            DexKit bridge(apk, 1);
            bridge.SetThreadNum(workers);
            if (sequence == 1) {
                auto guard = bridge.EnterQueryExecution(kMethodInvoking);
                for (const auto &item : bridge.dex_items)
                    Require(item->method_caller_ids.empty() && item->NeedInitCache(kCallerMethod), "forward-only leaves callers absent");
            } else if (sequence == 2) {
                bridge.InitFullCache();
            } else if (sequence == 3) {
                std::latch start(1);
                std::thread a([&] { start.wait(); Require(Collect(bridge, ids) == expected_bytes, "concurrent callers"); });
                std::thread b([&] { start.wait(); bridge.InitFullCache(); });
                start.count_down(); a.join(); b.join();
            } else if (sequence == 4) {
                std::thread caller;
                {
                    auto guard = bridge.EnterQueryExecution(kMethodInvoking);
                    caller = std::thread([&] { Require(Collect(bridge, ids) == expected_bytes, "queued caller bytes"); });
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    while (true) {
                        bool queued;
                        {
                            std::lock_guard lock(bridge.query_execution_mutex);
                            queued = (bridge.pending_warmup_flags & kCallerMethod) != 0;
                        }
                        if (queued) break;
                        Require(std::chrono::steady_clock::now() < deadline, "caller queued behind live query");
                        std::this_thread::yield();
                    }
                    for (const auto &item : bridge.dex_items) Require(item->method_caller_ids.empty(), "no partially published caller rows");
                }
                caller.join();
            }
            Require(Collect(bridge, ids) == expected_bytes, "cold/late/full public bytes");
            check_rows(bridge);
            std::vector<std::vector<const void *>> addresses(bridge.dex_items.size());
            for (size_t dex = 0; dex < bridge.dex_items.size(); ++dex)
                for (size_t method = 0; method < expected[dex].size(); ++method)
                    addresses[dex].push_back(bridge.dex_items[dex]->method_caller_ids[method].data());
            {
                auto guard = bridge.EnterQueryExecution(kRwFieldMethod | kMethodUsingField);
            }
            bridge.InitFullCache();
            Require(Collect(bridge, ids) == expected_bytes, "no duplicate aggregation after RW/full");
            check_rows(bridge);
            for (size_t dex = 0; dex < bridge.dex_items.size(); ++dex)
                for (size_t method = 0; method < expected[dex].size(); ++method)
                    Require(addresses[dex][method] == bridge.dex_items[dex]->method_caller_ids[method].data(), "published caller address stable");
        }
    }
    if (dump) {
        std::string raw("CALLERS1", 8);
        Append(raw, expected.size(), 4);
        for (const auto &dex : expected) {
            Append(raw, dex.size(), 4);
            for (const auto &row : dex) {
                Append(raw, row.size(), 8);
                for (auto [dex_id, method] : row) { Append(raw, dex_id, 2); Append(raw, method, 4); }
            }
        }
        Append(raw, expected_bytes.size(), 8);
        raw += expected_bytes;
        Require(std::fwrite(raw.data(), 1, raw.size(), stdout) == raw.size(), "write caller oracle");
    } else {
        std::printf("CHECK_CALLERS {\"methods\":%zu,\"workers\":[1,4],\"sequences\":5,\"passed\":true}\n", ids.size());
    }
}

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 3 && std::string_view(argv[1]) == "--dump")) return 2;
    dexkit::BenchmarkDiagnostics::CheckCallers(argv[argc - 1], argc == 3);
}

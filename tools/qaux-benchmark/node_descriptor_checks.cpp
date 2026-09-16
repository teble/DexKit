#include "node_descriptor_cache.h"
#include <atomic>
#include <cstdio>
#include <latch>
#include <thread>
#include <vector>

namespace {
void Require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "Node descriptor check failed: %s\n", message);
        std::abort();
    }
}

void CheckGrowth(bool one_shard) {
    dexkit::NodeDescriptorCache cache;
    cache.Initialize(65536, 65536);
    const auto sso = cache.GetOrCreate<true>(0, [] { return std::string("short"); });
    const auto field = cache.GetOrCreate<false>(0, [] { return std::string("field"); });
    const std::string long_value(4096, 'x');
    const auto large = cache.GetOrCreate<true>(65535, [&] { return long_value; });
    const auto empty = cache.GetOrCreate<false>(65535, [] { return std::string(); });
    const auto *sso_data = sso.data(), *field_data = field.data();
    const auto *large_data = large.data(), *empty_data = empty.data();
    std::atomic<uint32_t> builds{0}, readers_ready{0};
    std::atomic<bool> finished{false};
    std::latch start(1);
    std::array<std::string_view, 8> same;
    std::vector<std::thread> threads;
    threads.emplace_back([&] {
        readers_ready.store(1, std::memory_order_release);
        start.wait();
        do {
            // Deliberately read only borrowed characters while writers rehash.
            Require(sso == "short" && field == "field" && large == long_value && empty.empty(),
                    "retained characters during growth");
        } while (!finished.load(std::memory_order_acquire));
    });
    for (size_t worker = 0; worker < same.size(); ++worker) threads.emplace_back([&, worker] {
        start.wait();
        same[worker] = cache.GetOrCreate<true>(1, [&] { ++builds; return std::string("same"); });
        for (uint32_t step = 1; step < 2048; ++step) {
            const uint32_t i = (step + uint32_t(worker) * 97 - 1) % 2047 + 1;
            const uint32_t id = one_shard ? i * 32 : i * 17 + 2;
            const auto expected = std::to_string(id);
            Require(cache.GetOrCreate<true>(id, [&] { ++builds; return "m" + expected; }) == "m" + expected,
                    "concurrent method content");
            Require(cache.GetOrCreate<false>(id, [&] { ++builds; return "f" + expected; }) == "f" + expected,
                    "concurrent field content");
        }
    });
    while (!readers_ready.load(std::memory_order_acquire)) std::this_thread::yield();
    start.count_down();
    for (size_t i = 1; i < threads.size(); ++i) threads[i].join();
    finished.store(true, std::memory_order_release);
    threads[0].join();
    Require(builds == 4095, "one build per newly requested method or field");
    for (const auto value : same) Require(value == "same" && value.data() == same[0].data(), "same ID publication");
    Require(cache.GetOrCreate<true>(0, [] { std::abort(); return std::string(); }).data() == sso_data,
            "SSO object address");
    Require(cache.GetOrCreate<false>(0, [] { std::abort(); return std::string(); }).data() == field_data,
            "method and field maps are independent");
    Require(cache.GetOrCreate<true>(65535, [] { std::abort(); return std::string(); }).data() == large_data,
            "long string address");
    Require(cache.GetOrCreate<false>(65535, [] { std::abort(); return std::string(); }).data() == empty_data,
            "empty string address");
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    const auto stats = cache.GetStatistics();
    uint64_t records = 0, calls = 0, hits = 0, growths = 0;
    for (const auto &shard : stats.tables) for (const auto &table : shard) {
        records += table.records; calls += table.calls; hits += table.hits; growths += table.growths;
    }
    Require(records == 4099 && calls - hits == records, "diagnostic accounting");
    Require(growths > 10 && stats.tables[0][0].capacity > 1, "real bucket growth exercised");
    if (one_shard) Require(stats.tables[0][0].growths > 8, "repeated same-table rehash");
#endif
}
} // namespace

int main() {
    { dexkit::NodeDescriptorCache unused; }
    { dexkit::NodeDescriptorCache empty; empty.Initialize(0, 0); }
    CheckGrowth(true);
    CheckGrowth(false);
    std::puts("CHECK_NODE_DESCRIPTOR_COMPONENT {\"passed\":true,\"writers\":8,\"borrowed_view_readers\":1}");
}

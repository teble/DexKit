#include "hybrid_descriptor_cache.h"
#include <atomic>
#include <cstdio>
#include <latch>
#include <thread>
#include <vector>

namespace {
void Require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "Hybrid descriptor check failed: %s\n", message);
        std::abort();
    }
}

template<bool Method, bool Promote, class Build>
std::string_view GetWithProbe(dexkit::HybridDescriptorCache<Promote> &cache, uint32_t id, Build &&build) {
    if (auto *value = cache.template TryGet<Method>(id)) return *value;
    return cache.template GetOrCreate<Method>(id, std::forward<Build>(build));
}

template<bool Promote>
void CheckTryGet() {
    dexkit::HybridDescriptorCache<Promote> cache;
    cache.Initialize(65, 33);
    for (uint32_t id : {0, 32, 64})
        Require(cache.template TryGet<true>(id) == nullptr, "unused method probe misses");
    for (uint32_t id : {0, 32})
        Require(cache.template TryGet<false>(id) == nullptr, "unused field probe misses");
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    for (const auto &shard : cache.GetStatistics().tables) for (const auto &table : shard)
        Require(table.calls == 0 && table.payload_owner_bytes == 0 && table.bucket_bytes == 0
                && table.dense_bytes == 0, "failed probe neither counts nor allocates");
#endif
    const auto short_value = GetWithProbe<true>(cache, 0, [] { return std::string("short"); });
    const auto long_value = GetWithProbe<true>(cache, 32, [] { return std::string(4096, 'x'); });
    Require(cache.template TryGet<true>(64) == nullptr, "unfilled dense slot misses");
    const auto empty_value = GetWithProbe<true>(cache, 64, [] { return std::string(); });
    const auto field = GetWithProbe<false>(cache, 0, [] { return std::string("field"); });
    const auto empty_field = GetWithProbe<false>(cache, 32, [] { return std::string(); });
    auto check = [](const std::string *value, std::string_view retained) {
        if constexpr (Promote)
            Require(value && *value == retained && value->data() == retained.data(), "probe returns stable SSO/long/empty body");
        else Require(value == nullptr, "sparse-only probe never reads the hash without its lock");
    };
    check(cache.template TryGet<true>(0), short_value);
    check(cache.template TryGet<true>(32), long_value);
    check(cache.template TryGet<true>(64), empty_value);
    check(cache.template TryGet<false>(0), field);
    check(cache.template TryGet<false>(32), empty_field);
    Require(cache.template GetOrCreate<true>(0, [] { std::abort(); return std::string(); }) == short_value,
            "generic hit still probes once");
    // Another call can fill the slot between the outer probe and Cold entry.
    Require(cache.template TryGet<true>(1) == nullptr, "outer probe misses before another call fills");
    cache.template GetOrCreate<true>(1, [] { return std::string("late"); });
    Require(cache.template GetOrCreate<true>(1, [] { std::abort(); return std::string(); }) == "late",
            "Cold recheck sees a fill after the outer probe");
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    uint64_t calls = 0, hits = 0, records = 0;
    for (const auto &shard : cache.GetStatistics().tables) for (const auto &table : shard) {
        calls += table.calls; hits += table.hits; records += table.records;
    }
    Require(records == 6 && calls == (Promote ? 13 : 8) && hits == (Promote ? 7 : 2),
            "outer misses, direct hits and Cold rechecks count each completed access once");
#endif
}

template<bool Promote>
void CheckBounds() {
    for (uint32_t methods : {0, 1, 31, 32, 33, 1057}) {
        const uint32_t fields = methods / 2;
        dexkit::HybridDescriptorCache<Promote> cache;
        cache.Initialize(methods, fields);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        for (const auto &shard : cache.GetStatistics().tables) for (const auto &table : shard)
            Require(table.payload_owner_bytes == 0 && table.allocations.blocks.allocations == 0
                    && table.allocations.directory.allocations == 0 && table.bucket_bytes == 0
                    && table.dense_bytes == 0, "unused domain allocates nothing");
#endif
        for (uint32_t id = 0; id < methods; ++id) {
            const auto expected = "m" + std::to_string(id);
            Require(cache.template GetOrCreate<true>(id, [&] { return expected; }) == expected, "method bounds");
        }
        for (uint32_t id = 0; id < fields; ++id) {
            const auto expected = "f" + std::to_string(id);
            Require(cache.template GetOrCreate<false>(id, [&] { return expected; }) == expected, "field bounds");
        }
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        const auto stats = cache.GetStatistics();
        size_t method_slots = 0, field_slots = 0;
        for (const auto &shard : stats.tables) {
            method_slots += shard[0].slots; field_slots += shard[1].slots;
            for (const auto &table : shard) {
                Require(table.records == table.slots, "all and only valid slots used");
                if constexpr (Promote) {
                    Require(table.promotions == (table.slots != 0), "small full shards convert exactly once");
                    Require(table.capacity == 0 && table.bucket_bytes == 0, "converted hash allocation released");
                }
            }
        }
        Require(method_slots == methods && field_slots == fields, "unequal nonmultiple domains");
#endif
    }
}

template<bool Promote>
void CheckGrowth(bool one_shard) {
    const uint32_t limit = one_shard ? 65536 : 8192;
    dexkit::HybridDescriptorCache<Promote> cache;
    cache.Initialize(limit, limit);
    const auto sso = cache.template GetOrCreate<true>(0, [] { return std::string("short"); });
    const auto field = cache.template GetOrCreate<false>(0, [] { return std::string("field"); });
    const std::string long_value(4096, 'x');
    const auto large = cache.template GetOrCreate<true>(limit - 1, [&] { return long_value; });
    const auto empty = cache.template GetOrCreate<false>(limit - 1, [] { return std::string(); });
    const auto *sso_data = sso.data(), *field_data = field.data();
    const auto *large_data = large.data(), *empty_data = empty.data();
    std::atomic<uint32_t> builds{0};
    std::atomic<bool> finished{false};
    std::latch start(1), reader_ready(1);
    std::array<std::string_view, 8> same;
    std::vector<std::thread> threads;
    threads.emplace_back([&] {
        reader_ready.count_down(); start.wait();
        do {
            // Reads borrowed characters while other threads append/convert.
            Require(sso == "short" && field == "field" && large == long_value && empty.empty(),
                    "retained characters during deque growth and index conversion");
        } while (!finished.load(std::memory_order_acquire));
    });
    for (size_t worker = 0; worker < same.size(); ++worker) threads.emplace_back([&, worker] {
        start.wait();
        same[worker] = GetWithProbe<true>(cache, 1, [&] { ++builds; return std::string("same"); });
        for (uint32_t step = 1; step < 2048; ++step) {
            const uint32_t i = (step + uint32_t(worker) * 97 - 1) % 2047 + 1;
            const uint32_t id = one_shard ? i * 32 : i * 3 + 2;
            const auto expected = std::to_string(id);
            Require(GetWithProbe<true>(cache, id, [&] { ++builds; return "m" + expected; }) == "m" + expected,
                    "concurrent method content");
            Require(GetWithProbe<false>(cache, id, [&] { ++builds; return "f" + expected; }) == "f" + expected,
                    "concurrent field content");
        }
    });
    reader_ready.wait(); start.count_down();
    for (size_t i = 1; i < threads.size(); ++i) threads[i].join();
    finished.store(true, std::memory_order_release); threads[0].join();
    Require(builds == 4095, "one construction per newly requested member ID");
    for (const auto value : same) Require(value == "same" && value.data() == same[0].data(), "same ID publication");
    Require(cache.template GetOrCreate<true>(0, [] { std::abort(); return std::string(); }).data() == sso_data,
            "SSO address after conversion");
    Require(cache.template GetOrCreate<false>(0, [] { std::abort(); return std::string(); }).data() == field_data,
            "separate field payload address");
    Require(cache.template GetOrCreate<true>(limit - 1, [] { std::abort(); return std::string(); }).data() == large_data,
            "long string address");
    Require(cache.template GetOrCreate<false>(limit - 1, [] { std::abort(); return std::string(); }).data() == empty_data,
            "empty string is a cached value");
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    uint64_t records = 0, calls = 0, hits = 0, promotions = 0, dense_hits = 0;
    const auto stats = cache.GetStatistics();
    for (const auto &shard : stats.tables) for (const auto &table : shard) {
        records += table.records; calls += table.calls; hits += table.hits;
        promotions += table.promotions; dense_hits += table.dense_hits;
        Require(table.allocations.blocks.bytes >= table.records * sizeof(std::string), "real block capacity includes slack");
        if (table.promotions) Require(table.promotions == 1 && table.capacity == 0 && table.bucket_bytes == 0,
                                     "one-way conversion frees sparse buckets");
    }
    Require(records == 4099 && calls - hits == records, "construction accounting");
    if constexpr (Promote) Require(promotions >= 2 && dense_hits > 100, "real concurrent conversions and dense hits");
    else Require(promotions == 0 && dense_hits == 0, "sparse-only control never converts");
    if (one_shard) Require(stats.tables[0][0].allocations.blocks.allocations > 1
                           && stats.tables[0][0].allocations.directory.allocations > 1, "real deque block/directory growth");
#endif
}

#if DEXKIT_BENCHMARK_DIAGNOSTICS
void CheckThresholdAndStaleReader() {
    using Cache = dexkit::HybridDescriptorCache<true>;
    Cache probe;
    probe.Initialize(60000, 0);
    size_t threshold = 0;
    Cache::TableStatistics before{};
    for (uint32_t id = 0; id < 60000; id += 32) {
        probe.GetOrCreate<true>(id, [=] { return std::to_string(id); });
        auto state = probe.GetStatistics().tables[0][0];
        if (state.promotions) {
            threshold = state.records;
            Require(before.promotions == 0 && before.records + 1 == threshold, "threshold adjacent states");
            Require(before.bucket_bytes < state.dense_bytes && state.promotion_hash_bytes >= state.dense_bytes,
                    "actual ABI byte costs straddle the threshold");
            Require(state.capacity == 0 && state.bucket_bytes == 0, "threshold frees hash storage");
            std::printf("CHECK_HYBRID_THRESHOLD {\"slots\":%zu,\"before_records\":%zu,\"at_records\":%zu,"
                "\"before_hash_bytes\":%zu,\"at_hash_bytes\":%zu,\"dense_bytes\":%zu,\"fixed_bytes\":%zu}\n",
                state.slots, before.records, threshold, before.bucket_bytes, state.promotion_hash_bytes,
                state.dense_bytes, Cache::FixedObjectBytes());
            break;
        }
        before = state;
    }
    Require(threshold > 1, "conversion reached after a sparse interval");
    Cache cache;
    cache.Initialize(60000, 0);
    const auto retained = cache.GetOrCreate<true>(0, [] { return std::string("retained"); });
    for (uint32_t i = 1; i + 1 < threshold; ++i)
        cache.GetOrCreate<true>(i * 32, [=] { return std::to_string(i); });
    Require(cache.GetStatistics().tables[0][0].promotions == 0, "fresh cache stops immediately before conversion");
    struct Gate { std::latch paused{1}, resume{1}; std::atomic<bool> armed{true}; } gate;
    cache.SetBeforeSlowLockHook([](void *context, bool method, uint32_t id) {
        auto &gate = *static_cast<Gate *>(context);
        if (method && id == 0 && gate.armed.exchange(false)) {
            gate.paused.count_down(); gate.resume.wait();
        }
    }, &gate);
    std::thread stale_reader([&] {
        const auto value = GetWithProbe<true>(cache, 0, [] { std::abort(); return std::string(); });
        Require(value == "retained" && value.data() == retained.data(), "stale sparse reader rechecks dense under lock");
    });
    gate.paused.wait();
    cache.GetOrCreate<true>(uint32_t(threshold - 1) * 32, [] { return std::string("trigger"); });
    const auto at = cache.GetStatistics().tables[0][0];
    Require(at.promotions == 1 && at.records == threshold && at.capacity == 0, "writer converts while reader paused");
    gate.resume.count_down(); stale_reader.join();
    cache.SetBeforeSlowLockHook(nullptr, nullptr);
    const auto growths = at.growths;
    std::atomic<size_t> builds{0};
    std::latch start(1);
    std::vector<std::thread> threads;
    std::array<std::string_view, 8> views;
    for (size_t i = 0; i < views.size(); ++i) threads.emplace_back([&, i] {
        start.wait();
        views[i] = GetWithProbe<true>(cache, uint32_t(threshold) * 32, [&] { ++builds; return std::string(); });
    });
    start.count_down(); for (auto &thread : threads) thread.join();
    Require(builds == 1, "concurrent first fill of the same dense null slot");
    for (auto value : views) Require(value.empty() && value.data() == views[0].data(), "dense empty value publication");
    const auto after = cache.GetStatistics();
    Require(after.tables[0][0].records == threshold + 1 && after.tables[0][0].promotions == 1
            && after.tables[0][0].growths == growths && after.tables[0][0].capacity == 0, "after threshold never regrows sparse");
    for (const auto &shard : after.tables) Require(shard[1].payload_owner_bytes == 0
            && shard[1].allocations.blocks.allocations == 0 && shard[1].allocations.directory.allocations == 0,
            "empty field domain stays unallocated");
}
#endif
} // namespace

int main(int argc, char **argv) {
    if (argc == 4) {
        dexkit::HybridDescriptorCache<true> cache;
        const auto limit = size_t(std::strtoull(argv[2], nullptr, 10));
        const auto id = uint32_t(std::strtoull(argv[3], nullptr, 10));
        cache.Initialize(limit, limit);
        if (std::string_view(argv[1]) == "try-method") cache.TryGet<true>(id);
        else if (std::string_view(argv[1]) == "try-field") cache.TryGet<false>(id);
        else if (std::string_view(argv[1]) == "method") cache.GetOrCreate<true>(id, [] { return std::string("m"); });
        else cache.GetOrCreate<false>(id, [] { return std::string("f"); });
        return 0;
    }
    CheckBounds<false>(); CheckBounds<true>();
    CheckTryGet<false>(); CheckTryGet<true>();
    CheckGrowth<false>(true); CheckGrowth<true>(true);
    CheckGrowth<false>(false); CheckGrowth<true>(false);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    CheckThresholdAndStaleReader();
#endif
    std::puts("CHECK_HYBRID_DESCRIPTOR_COMPONENT {\"passed\":true,\"writers\":8,\"borrowed_view_readers\":1}");
}

#include "dexkit.h"
#include <atomic>
#include <cstdio>
#include <latch>
#include <numeric>
#include <thread>

namespace dexkit {
struct VectorDescriptorBenchmark {
    static constexpr uint32_t count = 60000;
    static constexpr bool promote = !DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS_NO_PROMOTION;
    static void Require(bool ok, const char *message) {
        if (!ok) { std::fprintf(stderr, "Vector descriptor check failed: %s\n", message); std::abort(); }
    }
    static std::vector<int64_t> Ids() {
        std::vector<int64_t> ids(count);
        std::iota(ids.begin(), ids.end(), int64_t(0));
        return ids;
    }
    static std::string Descriptor(uint32_t id, bool method) {
        char text[40];
        std::snprintf(text, sizeof(text), method ? "LA;->m%05u()V" : "LA;->f%05u:I", id);
        return text;
    }
    static void CheckArray(const flatbuffers::FlatBufferBuilder &buffer, bool method, size_t expected = count) {
        if (method) {
            auto rows = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(buffer.GetBufferPointer())->methods();
            Require(rows->size() == expected, "method result count");
            for (uint32_t i = 0; i < rows->size(); ++i)
                Require(rows->Get(i)->id() == i && rows->Get(i)->dex_id() == 0
                        && rows->Get(i)->dex_descriptor()->string_view() == Descriptor(i, true), "ordered method bytes");
        } else {
            auto rows = flatbuffers::GetRoot<schema::FieldMetaArrayHolder>(buffer.GetBufferPointer())->fields();
            Require(rows->size() == expected, "field result count");
            for (uint32_t i = 0; i < rows->size(); ++i)
                Require(rows->Get(i)->id() == i && rows->Get(i)->dex_id() == 0
                        && rows->Get(i)->dex_descriptor()->string_view() == Descriptor(i, false), "ordered field bytes");
        }
    }
    static void CheckBasic(std::string_view apk) {
        DexKit bridge(apk, 1);
        auto &item = *bridge.dex_items[0];
        Require(bridge.dex_items.size() == 1 && item.reader.MethodIds().size() == count
                && item.reader.FieldIds().size() == count, "dense fixture shape");
        const auto ids = Ids();
        auto first = bridge.GetMethodByIds(ids);
        CheckArray(*first, true);
        Require(!item.vector_descriptors.IsDense<true>() && !item.vector_descriptors.IsDense<false>(), "triggering API remains sparse");
        Require(item.vector_descriptors.HasPending() == promote, "pending conversion follows the selected control");
        auto second = bridge.GetMethodByIds(ids);
        CheckArray(*second, true);
        CheckArray(*first, true); // The earlier serialized result owns its bytes.
        Require(item.vector_descriptors.IsDense<true>() == promote
                && !item.vector_descriptors.IsDense<false>(), "only the whole method domain changes form");
        auto fields = bridge.GetFieldByIds(ids);
        CheckArray(*fields, false);
        Require(!item.vector_descriptors.IsDense<false>(), "field API retains its own sparse generation");
        auto repeated_fields = bridge.GetFieldByIds({0, 1, 2});
        CheckArray(*repeated_fields, false, 3);
        CheckArray(*fields, false);
        Require(item.vector_descriptors.IsDense<false>() == promote, "independent field transition");
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        const auto stats = item.vector_descriptors.GetStatistics();
        for (const auto &d : stats.domains) {
            Require(d.calls - d.hits == d.sparse_builds + d.dense_builds, "generation counts reconcile");
            Require(d.sparse_builds == count, "initial generation builds each descriptor once");
            if (promote) Require(d.conversions == 1 && d.discarded_records == count && d.sparse_records == 0
                    && d.block_bytes == 0 && d.directory_bytes == 0 && d.bucket_bytes == 0
                    && d.sparse_char_bytes == 0 && d.payload_owner_bytes == 0, "old storage reclaimed");
        }
        Require(stats.domains[0].dense_builds == (promote ? count : 0), "second full method generation rebuilds");
        Require(stats.domains[1].dense_builds == (promote ? 3 : 0), "only requested new fields regenerate");
#endif
    }

    static void CheckWorkerBorrow(std::string_view apk) {
        for (uint32_t workers : {1u, 4u}) {
            DexKit bridge(apk, 1);
            bridge.SetThreadNum(workers);
            Require(!DescriptorBorrowScope::Contains(&bridge), "no session before admission");
            {
                auto borrow = bridge.BorrowDescriptors();
                QueryContext context(QueryKind::FindMethod);
                auto executor = bridge.CreateQueryExecutor(context);
                std::vector<std::future<std::pair<std::string, std::string>>> results;
                for (uint32_t id = 0; id < 8; ++id) {
                    results.push_back(SubmitQueryTask(*executor, [&, id] {
                        Require(DescriptorBorrowScope::Contains(&bridge), "query worker inherits borrowing context");
                        auto *item = bridge.GetDexItem(0);
                        return std::make_pair(std::string(item->GetMethodBean(id).dex_descriptor),
                                std::string(item->GetFieldBean(id).dex_descriptor));
                    }));
                }
                executor->OnSubmissionComplete();
                for (uint32_t id = 0; id < results.size(); ++id) {
                    const auto value = results[id].get();
                    Require(value.first == Descriptor(id, true) && value.second == Descriptor(id, false),
                            "worker copies descriptor bytes before finishing");
                }
            }
            Require(!DescriptorBorrowScope::Contains(&bridge), "session restored after admission");
        }
    }

#if DEXKIT_BENCHMARK_DIAGNOSTICS
    struct Pause {
        std::atomic<bool> used{false};
        std::latch reached{1}, release{1};
        static void Hook(void *context) {
            auto &self = *static_cast<Pause *>(context);
            if (!self.used.exchange(true)) { self.reached.count_down(); self.release.wait(); }
        }
    };
    struct Waiting {
        int wanted;
        std::atomic<bool> used{false};
        std::latch reached{1};
        explicit Waiting(int reason) : wanted(reason) {}
        static void Hook(void *context, int reason) {
            auto &self = *static_cast<Waiting *>(context);
            if (reason == self.wanted && !self.used.exchange(true)) self.reached.count_down();
        }
    };
    static void ObserveWaiting(DexKit &bridge, Waiting &waiting) {
        bridge.descriptor_waiting_hook_ = Waiting::Hook;
        bridge.descriptor_waiting_context_ = &waiting;
    }
    static void CheckUnserializedResults(std::string_view apk, bool method) {
        if constexpr (!promote) return;
        DexKit bridge(apk, 1);
        const auto ids = Ids();
        Pause serialization;
        Waiting waiting(1);
        bridge.descriptor_before_serialize_hook_ = Pause::Hook;
        bridge.descriptor_before_serialize_context_ = &serialization;
        ObserveWaiting(bridge, waiting);
        std::unique_ptr<flatbuffers::FlatBufferBuilder> first, second;
        std::thread producer([&] {
            first = method ? bridge.GetMethodByIds(ids) : bridge.GetFieldByIds(ids);
        });
        serialization.reached.wait();
        std::thread entrant([&] {
            second = method ? bridge.GetMethodByIds({0, 1, 2}) : bridge.GetFieldByIds({0, 1, 2});
        });
        waiting.reached.wait();
        {
            std::lock_guard lock(bridge.query_execution_mutex);
            Require(bridge.active_query_count == 1 && bridge.descriptor_maintenance_runs_ == 0
                    && !bridge.descriptor_maintenance_inflight_, "entrant waits for actual unserialized Beans");
            Require(!bridge.dex_items[0]->vector_descriptors.IsDense<true>()
                    && !bridge.dex_items[0]->vector_descriptors.IsDense<false>(), "no premature reclamation");
        }
        serialization.release.count_down();
        producer.join(); entrant.join();
        CheckArray(*first, method); CheckArray(*second, method, 3);
        Require(bridge.descriptor_maintenance_runs_ == 1 && bridge.descriptor_maintenance_wait_ns_ > 0,
                "one conversion after borrowed result serialization");
    }
    static void CheckNativeBorrow(std::string_view apk) {
        if constexpr (!promote) return;
        DexKit bridge(apk, 1);
        Waiting waiting(1);
        ObserveWaiting(bridge, waiting);
        std::latch filled(1), release(1);
        std::thread native([&] {
            auto borrow = bridge.BorrowDescriptors();
            auto *item = bridge.GetDexItem(0);
            auto retained = item->GetMethodBean(0);
            const auto *address = retained.dex_descriptor.data();
            for (uint32_t id = 1; id < count; ++id) item->GetMethodBean(id);
            filled.count_down(); release.wait();
            Require(retained.dex_descriptor == Descriptor(0, true)
                    && retained.dex_descriptor.data() == address, "native Bean valid for the full borrowing session");
        });
        filled.wait();
        std::unique_ptr<flatbuffers::FlatBufferBuilder> next;
        std::thread entrant([&] { next = bridge.GetMethodByIds({0, 1, 2}); });
        waiting.reached.wait();
        Require(!bridge.dex_items[0]->vector_descriptors.IsDense<true>(), "live native session delays maintenance");
        release.count_down(); native.join(); entrant.join();
        CheckArray(*next, true, 3);
        Require(bridge.dex_items[0]->vector_descriptors.IsDense<true>(), "native release permits full reclamation");
    }
    static void CheckWarmupExclusion(std::string_view apk, bool warmup_first) {
        if constexpr (!promote) return;
        DexKit bridge(apk, 1);
        auto populated = bridge.GetMethodByIds(Ids());
        Require(bridge.NeedWarmUp(UINT32_MAX), "fixture still needs an independent warmup");
        Pause paused;
        Waiting waiting(warmup_first ? 2 : 3);
        ObserveWaiting(bridge, waiting);
        if (warmup_first) {
            bridge.descriptor_before_warmup_hook_ = Pause::Hook;
            bridge.descriptor_before_warmup_context_ = &paused;
        } else {
            bridge.descriptor_before_maintenance_hook_ = Pause::Hook;
            bridge.descriptor_before_maintenance_context_ = &paused;
        }
        std::thread first([&] {
            if (warmup_first) bridge.InitFullCache();
            else Require(bridge.GetClassData("LA;") != nullptr, "maintenance-triggering query");
        });
        paused.reached.wait();
        std::thread second([&] {
            if (warmup_first) Require(bridge.GetClassData("LA;") != nullptr, "waiting query");
            else bridge.InitFullCache();
        });
        waiting.reached.wait();
        {
            std::lock_guard lock(bridge.query_execution_mutex);
            Require(bridge.active_query_count == 0 && bridge.warmup_inflight == warmup_first
                    && bridge.descriptor_maintenance_inflight_ != warmup_first, "warmup and reclamation are mutually exclusive");
        }
        paused.release.count_down(); first.join(); second.join();
        CheckArray(*populated, true);
        Require(bridge.dex_items[0]->vector_descriptors.IsDense<true>() && !bridge.NeedWarmUp(UINT32_MAX),
                "both exclusive maintenance phases completed");
    }
#endif
    static void Run(std::string_view apk) {
        CheckBasic(apk);
        CheckWorkerBorrow(apk);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        CheckUnserializedResults(apk, true); CheckUnserializedResults(apk, false);
        CheckNativeBorrow(apk); CheckWarmupExclusion(apk, true); CheckWarmupExclusion(apk, false);
#endif
        std::printf("CHECK_VECTOR_DESCRIPTORS {\"promotion_enabled\":%s,\"passed\":true}\n", promote ? "true" : "false");
    }
};
} // namespace dexkit

int main(int argc, char **argv) {
    if (argc == 3) {
        dexkit::DexKit bridge(argv[2], 1);
        if (std::string_view(argv[1]) == "--unscoped") bridge.GetDexItem(0)->GetMethodBean(0);
        else if (std::string_view(argv[1]) == "--reenter") {
            auto borrow = bridge.BorrowDescriptors();
            bridge.GetClassData("LA;");
        } else return 2;
        return 3;
    }
    if (argc != 2) return 2;
    dexkit::VectorDescriptorBenchmark::Run(argv[1]);
}

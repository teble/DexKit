#include "benchmark_diagnostics.h"
#include "dexkit.h"
#include "dex_item.h"
#include "analyze.h"
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <chrono>
#include <string>
#include <thread>
#include <algorithm>
#include <span>
#include <vector>

namespace {
using namespace dexkit;
using Builder = flatbuffers::FlatBufferBuilder;

void Require(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "Relation check failed: %s\n", message); std::abort(); }
}

std::unique_ptr<Builder> Query(bool reverse) {
    auto b = std::make_unique<Builder>();
    auto name = schema::CreateStringMatcher(*b, b->CreateString("value"), schema::StringMatchType::Equal);
    auto no_methods = schema::CreateMethodsMatcher(*b, 0, schema::MatchType::Contains,
                                                   schema::CreateIntRange(*b, 0, 0));
    schema::FieldMatcherBuilder leaf(*b);
    if (reverse) leaf.add_get_methods(no_methods);
    else leaf.add_field_name(name);
    auto field = leaf.Finish();
    if (reverse) {
        auto children = b->CreateVector(std::vector{field});
        schema::FieldMatcherBuilder negation(*b);
        negation.add_none_of(children);
        field = negation.Finish();
        children = b->CreateVector(std::vector{field});
        schema::FieldMatcherBuilder conjunction(*b);
        conjunction.add_all_of(children);
        field = conjunction.Finish();
        children = b->CreateVector(std::vector{field});
        schema::FieldMatcherBuilder disjunction(*b);
        disjunction.add_any_of(children);
        field = disjunction.Finish();
    }
    auto uses = b->CreateVector(std::vector{schema::CreateUsingFieldMatcher(*b, field)});
    schema::MethodMatcherBuilder method(*b);
    method.add_using_fields(uses);
    auto matcher = method.Finish();
    schema::FindMethodBuilder query(*b);
    query.add_matcher(matcher);
    b->Finish(query.Finish());
    return b;
}

void Append(std::string &out, std::unique_ptr<Builder> data) {
    Require(data != nullptr && data->GetSize() <= UINT32_MAX, "valid result buffer");
    uint32_t length = data->GetSize();
    for (size_t i = 0; i < 4; ++i) out.push_back(static_cast<char>(length >> (i * 8)));
    out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), length);
}

std::string Forward(DexKit &bridge, const std::vector<int64_t> &methods) {
    std::string out;
    auto query = Query(false);
    Append(out, bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer())));
    for (auto id : methods) Append(out, bridge.GetUsingFields(id));
    return out;
}

std::string Reverse(DexKit &bridge, const std::vector<int64_t> &fields) {
    std::string out;
    auto query = Query(true);
    Append(out, bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer())));
    for (auto id : fields) {
        Append(out, bridge.FieldGetMethods(id));
        Append(out, bridge.FieldPutMethods(id));
    }
    return out;
}

std::string Calls(DexKit &bridge, const std::vector<int64_t> &methods) {
    std::string out;
    for (auto id : methods) {
        Append(out, bridge.GetInvokeMethods(id));
        Append(out, bridge.GetCallMethods(id));
    }
    return out;
}
} // namespace

void dexkit::BenchmarkDiagnostics::CheckRelations(std::string_view apk, bool dump) {
    DexKit reference(apk, 1);
    reference.SetThreadNum(4);
    reference.InitFullCache();
    std::vector<int64_t> methods, fields;
    for (const auto &item : reference.dex_items) {
        for (uint32_t i = 0; i < item->reader.MethodIds().size(); ++i)
            methods.push_back((int64_t(item->dex_id) << 32) | i);
        for (uint32_t i = 0; i < item->reader.FieldIds().size(); ++i)
            fields.push_back((int64_t(item->dex_id) << 32) | i);
    }
    Require(!methods.empty() && !fields.empty(), "nonempty fixture");
    const auto expected_forward = Forward(reference, methods);
    const auto expected_reverse = Reverse(reference, fields);
    const auto expected_calls = Calls(reference, methods);
#if DEXKIT_EXPERIMENT_COMPACT_FIELDS
    struct HeldFieldRow {
        const DexItem *item;
        uint32_t method;
        std::span<const CompactFieldIndex::Use> view;
        std::vector<CompactFieldIndex::Use> values;
    };
    const auto hold_fields = [&](const DexKit &bridge) {
        std::vector<HeldFieldRow> held;
        for (size_t dex = 0; dex < bridge.dex_items.size(); ++dex) {
            const auto *item = bridge.dex_items[dex].get();
            for (uint32_t method = 0; method < item->reader.MethodIds().size(); ++method) {
                auto row = item->method_using_field_ids[method];
                auto expected = reference.dex_items[dex]->method_using_field_ids[method];
                Require(std::equal(row.begin(), row.end(), expected.begin(), expected.end()), "raw field row equals full initialization");
                held.push_back({item, method, row, {row.begin(), row.end()}});
            }
        }
        return held;
    };
    const auto check_fields = [](const std::vector<HeldFieldRow> &held) {
        for (const auto &saved : held) {
            auto current = saved.item->method_using_field_ids[saved.method];
            Require(current.data() == saved.view.data() && current.size() == saved.view.size(), "published field view address/size stable");
            Require(std::equal(saved.view.begin(), saved.view.end(), saved.values.begin(), saved.values.end()), "held field view order/multiplicity stable");
        }
    };
#endif
    constexpr int sequence_count = 7;
    for (int sequence = 0; sequence < sequence_count; ++sequence) {
        DexKit bridge(apk, 1);
        bridge.SetThreadNum(4);
        if (sequence == 1) {
            Require(Reverse(bridge, fields) == expected_reverse, "reverse-first");
        } else if (sequence == 3) {
            std::latch start(1);
            std::thread a([&] { start.wait(); Require(Forward(bridge, methods) == expected_forward, "concurrent forward"); });
            std::thread b([&] { start.wait(); Require(Reverse(bridge, fields) == expected_reverse, "concurrent reverse"); });
            std::thread c([&] { start.wait(); bridge.InitFullCache(); });
            start.count_down();
            a.join(); b.join(); c.join();
        } else if (sequence == 4) {
            std::thread reverse;
#if DEXKIT_EXPERIMENT_COMPACT_FIELDS
            std::vector<HeldFieldRow> pending_views;
#endif
            {
                // Hold a real forward admission while reverse warm-up queues.
                auto guard = bridge.EnterQueryExecution(kFieldIdentity | kMethodUsingField);
#if DEXKIT_EXPERIMENT_COMPACT_FIELDS
                pending_views = hold_fields(bridge);
#endif
                reverse = std::thread([&] { Require(Reverse(bridge, fields) == expected_reverse, "queued reverse"); });
#if DEXKIT_EXPERIMENT_FIELD_IDENTITY_SPLIT
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (true) {
                    bool queued;
                    {
                        std::lock_guard lock(bridge.query_execution_mutex);
                        queued = (bridge.pending_warmup_flags & kRwFieldMethod) != 0;
                    }
                    if (queued) break;
                    Require(std::chrono::steady_clock::now() < deadline, "reverse warm-up queued behind active forward");
                    std::this_thread::yield();
                }
#endif
#if DEXKIT_EXPERIMENT_COMPACT_FIELDS
                check_fields(pending_views);
#endif
            }
            reverse.join();
#if DEXKIT_EXPERIMENT_COMPACT_FIELDS
            check_fields(pending_views);
#endif
        } else if (sequence == 5) {
            // Reverse state is definitely ready before another thread enters.
            auto guard = bridge.EnterQueryExecution(kRwFieldMethod | kMethodUsingField);
            std::thread forward([&] { Require(Forward(bridge, methods) == expected_forward, "forward after reverse admission"); });
            forward.join();
        } else if (sequence == 6) {
            {
                auto guard = bridge.EnterQueryExecution(kFieldIdentity | kMethodUsingField | kMethodInvoking);
            }
#if DEXKIT_EXPERIMENT_FIELD_IDENTITY_SPLIT
            for (const auto &item : bridge.dex_items) {
                Require(!item->NeedInitCache(kMethodUsingField | kMethodInvoking), "joint reverse inputs already ready");
                Require(item->NeedInitCache(kRwFieldMethod) && item->NeedInitCache(kCallerMethod), "both local reverse tables still pending");
            }
            Require(!(bridge.cross_ref_aggregate_flag.load(std::memory_order_acquire) & (kRwFieldMethod | kCallerMethod)), "both reverse aggregates still pending");
#endif
            // Both reverse relations can be requested together after their
            // instruction-derived inputs are ready; neither needs extraction.
            {
                auto guard = bridge.EnterQueryExecution(kRwFieldMethod | kCallerMethod);
            }
#if DEXKIT_EXPERIMENT_FIELD_IDENTITY_SPLIT
            for (const auto &item : bridge.dex_items)
                Require(!item->NeedInitCache(kRwFieldMethod | kCallerMethod), "both local reverse tables ready before getters");
            Require((bridge.cross_ref_aggregate_flag.load(std::memory_order_acquire) & (kRwFieldMethod | kCallerMethod)) == (kRwFieldMethod | kCallerMethod), "both reverse aggregates ready before getters");
#endif
        }
        Require(Forward(bridge, methods) == expected_forward, "forward results/order");
#if DEXKIT_EXPERIMENT_COMPACT_FIELDS
        const auto held_fields = hold_fields(bridge);
#endif
#if DEXKIT_EXPERIMENT_FIELD_IDENTITY_SPLIT
        if (sequence == 0 || sequence == 2) {
            for (const auto &item : bridge.dex_items) {
                Require(item->field_get_method_ids.empty() && item->field_put_method_ids.empty(), "forward does not allocate reverse rows");
                Require(!item->NeedPutCrossRef(kFieldIdentity), "forward identity published");
                Require(item->NeedInitCache(kRwFieldMethod), "reverse remains uninitialized");
            }
            Require(!(bridge.cross_ref_aggregate_flag.load() & kRwFieldMethod), "reverse aggregate remains pending");
        }
#endif
        if (sequence == 2) bridge.InitFullCache();
        Require(Reverse(bridge, fields) == expected_reverse, "late reverse results/order/multiplicity");
        const auto warmup_counts = [&] {
            std::array<uint64_t, 3> counts{0, 0, bridge.benchmark_aggregate_calls.load()};
            for (const auto &item : bridge.dex_items)
                for (size_t i = 0; i < 2; ++i) counts[i] += item->benchmark_warmup_calls[i].load();
            return counts;
        };
        const auto before_repeat = warmup_counts();
        Require(Forward(bridge, methods) == expected_forward, "repeated forward bytes");
        Require(Reverse(bridge, fields) == expected_reverse, "repeated reverse bytes");
        Require(warmup_counts() == before_repeat, "warm forward/reverse performs no init, resolve or aggregate jobs");
        Require(Calls(bridge, methods) == expected_calls, "forward and reverse calls");
        bridge.InitFullCache();
        Require(Forward(bridge, methods) == expected_forward, "full warmup preserves forward");
        Require(Reverse(bridge, fields) == expected_reverse, "no duplicate aggregation");
        Require(Calls(bridge, methods) == expected_calls, "full warmup preserves calls");
#if DEXKIT_EXPERIMENT_COMPACT_FIELDS
        check_fields(held_fields);
#endif
    }
    if (dump) {
        for (const auto *data : {&expected_forward, &expected_reverse, &expected_calls})
            Require(std::fwrite(data->data(), 1, data->size(), stdout) == data->size(), "write oracle");
    } else {
        std::printf("CHECK_RELATIONS {\"methods\":%zu,\"fields\":%zu,\"sequences\":%d,\"passed\":true}\n",
                    methods.size(), fields.size(), sequence_count);
    }
}

int main(int argc, char **argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--dump")
        dexkit::BenchmarkDiagnostics::CheckRelations(argv[2], true);
    else if (argc == 2) dexkit::BenchmarkDiagnostics::CheckRelations(argv[1], false);
    else { std::fprintf(stderr, "Usage: dexkit_relation_checks [--dump] relations.apk\n"); return 2; }
}

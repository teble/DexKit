#include "dexkit.h"

#if DEXKIT_EXPERIMENT_COMPACT_CALLERS
#include "benchmark_diagnostics.h"
#include "ThreadPool.h"

namespace dexkit {

// Internal segment consistency is checked in debug and diagnostic builds.
// Normal release keeps the checked size arithmetic and array write bounds,
// but does not pay for the final validation-only replay.
#if !defined(NDEBUG) || DEXKIT_BENCHMARK_DIAGNOSTICS
#define DEXKIT_CALLER_CHECK(expr) do { if (!(expr)) _checkFailed(#expr, __LINE__, __FILE__); } while (false)
#else
#define DEXKIT_CALLER_CHECK(expr)
#endif

void DexKit::BuildCompactCallers(uint32_t thread_num) {
    // Local counts and the original identity bindings are ready; no caller
    // payload has been allocated. Keep the original source/work-list order.
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    BenchmarkDiagnostics::DumpCallerBuild(*this, "counted");
#endif
    for (auto &owner : dex_items) owner->method_caller_ids.BeginLayout();
    for (auto &owner : dex_items) {
        auto &source = owner->method_caller_ids;
        for (const auto &work : owner->pending_aggregate_method_work_items) {
            DEXKIT_CALLER_CHECK(work.source_count != 0 && work.source_count == source.LocalCount(work.source_method_idx));
            source.EmptyRow(work.source_method_idx);
            dex_items[work.target_dex_id]->method_caller_ids.AddRowCount(work.target_method_idx, work.source_count);
        }
    }
    for (auto &owner : dex_items) owner->method_caller_ids.Allocate();
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    BenchmarkDiagnostics::DumpCallerBuild(*this, "allocated");
#endif

    // An unbound row first reserves its local prefix. It then serves as the
    // next-import cursor. A bound source row will instead hold its assigned
    // target segment's start. Count and cursor arrays are the same allocation.
    for (auto &owner : dex_items) {
        auto &index = owner->method_caller_ids;
        for (uint32_t method = 0; method < index.size(); ++method) {
            if (!owner->method_cross_info[method])
                index.Cursor(method) = CompactCallerIndex::CheckedAdd(index.RowBegin(method), index.LocalCount(method));
            else
                DEXKIT_CALLER_CHECK(index.RowBegin(method) == index.RowEnd(method));
        }
    }
    for (auto &owner : dex_items) {
        for (const auto &work : owner->pending_aggregate_method_work_items) {
            auto &target = dex_items[work.target_dex_id]->method_caller_ids;
            auto &next = target.Cursor(work.target_method_idx);
            owner->method_caller_ids.Cursor(work.source_method_idx) = next;
            next = CompactCallerIndex::CheckedAdd(next, work.source_count);
            DEXKIT_CALLER_CHECK(next <= target.RowEnd(work.target_method_idx));
        }
    }
    for (auto &owner : dex_items) {
        auto &index = owner->method_caller_ids;
        for (uint32_t method = 0; method < index.size(); ++method) {
            if (!owner->method_cross_info[method]) {
                DEXKIT_CALLER_CHECK(index.Cursor(method) == index.RowEnd(method));
                index.Cursor(method) = index.RowBegin(method);
            }
        }
    }

    const auto fill_source = [this](DexItem *source) {
        auto &index = source->method_caller_ids;
        for (const auto &definition : source->reader.ClassDefs()) {
            for (auto caller : source->class_method_ids[definition.class_idx]) {
                for (auto invoked : source->method_invoking_ids[caller]) {
                    auto &cursor = index.Cursor(invoked);
                    const auto &binding = source->method_cross_info[invoked];
                    auto &target = binding ? dex_items[binding.value().first]->method_caller_ids : index;
                    target.Write(cursor++, static_cast<uint16_t>(source->dex_id), caller);
                }
            }
        }
    };
    if (thread_num > 1 && dex_items.size() > 1) {
        ThreadPool pool(std::min(static_cast<size_t>(thread_num), dex_items.size()));
        for (auto &owner : dex_items) pool.enqueue(fill_source, owner.get());
    } else {
        for (auto &owner : dex_items) fill_source(owner.get());
    }

#if !defined(NDEBUG) || DEXKIT_BENCHMARK_DIAGNOSTICS
    // Local cursors now point just past their local prefixes. Replaying the
    // same imports checks each source's final cursor against its expected end,
    // then checks every final row end before any consumer can see the payload.
    for (auto &owner : dex_items) {
        for (const auto &work : owner->pending_aggregate_method_work_items) {
            auto &target = dex_items[work.target_dex_id]->method_caller_ids;
            auto &end = target.Cursor(work.target_method_idx);
            end = CompactCallerIndex::CheckedAdd(end, work.source_count);
            DEXKIT_CALLER_CHECK(owner->method_caller_ids.Cursor(work.source_method_idx) == end);
        }
    }
    for (auto &owner : dex_items) {
        auto &index = owner->method_caller_ids;
        for (uint32_t method = 0; method < index.size(); ++method)
            if (!owner->method_cross_info[method]) DEXKIT_CALLER_CHECK(index.Cursor(method) == index.RowEnd(method));
    }
#endif
    for (auto &owner : dex_items) {
        owner->method_caller_ids.ReleaseBuild();
        decltype(owner->pending_aggregate_method_work_items)().swap(owner->pending_aggregate_method_work_items);
    }
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    BenchmarkDiagnostics::DumpCallerBuild(*this, "released");
#endif
    // The caller returns to FinishBuildCrossRefAggregates for publication.
}

#undef DEXKIT_CALLER_CHECK
} // namespace dexkit
#endif

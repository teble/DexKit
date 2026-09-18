#include "benchmark_diagnostics.h"

#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include "dexkit.h"
#include <map>
#include <set>
#include <latch>
#include <thread>

namespace dexkit {
namespace {
struct Counts {
    uint64_t index_bytes = 0, payload_bytes = 0, entries = 0, buffers = 0, ready = 0;
};

template<class T> void Flat(Counts &count, const std::vector<T> &values) {
    count.index_bytes += values.capacity() * sizeof(T);
    count.entries += values.size();
    count.buffers += values.capacity() != 0;
}

template<class T> void Rows(Counts &count, const std::vector<std::vector<T>> &rows) {
    Flat(count, rows);
    for (const auto &row : rows) {
        count.payload_bytes += row.capacity() * sizeof(T);
        count.buffers += row.capacity() != 0;
        count.ready += !row.empty();
    }
}

template<class Slot> void Slots(Counts &count, const std::unique_ptr<Slot[]> &slots, size_t size) {
    if (!slots) return;
    count.index_bytes += size * sizeof(Slot);
    count.entries += size;
    ++count.buffers;
    for (size_t i = 0; i < size; ++i) {
        if (slots[i].state.load(std::memory_order_acquire) == 2 && slots[i].data) {
            const auto &data = *slots[i].data;
            ++count.ready;
            ++count.buffers;
            count.buffers += data.capacity() != 0;
            count.payload_bytes += sizeof(data) + data.capacity() * sizeof(typename std::decay_t<decltype(data)>::value_type);
        }
    }
}

void Descriptors(Counts &count, const std::vector<std::optional<std::string>> &values) {
    Flat(count, values);
    for (const auto &value : values) {
        if (!value) continue;
        ++count.ready;
        const auto object = reinterpret_cast<uintptr_t>(&*value);
        const auto data = reinterpret_cast<uintptr_t>(value->data());
        if (data < object || data >= object + sizeof(*value)) {
            count.payload_bytes += value->capacity() + 1;
            ++count.buffers;
        }
    }
}

// These snapshots run only in diagnostic builds, after query work has drained.
// The bundled phmap helper uses its actual allocation layout, including control
// bytes and padding. It excludes allocator overhead and borrowed string bytes.
template<class Map>
void NameIndex(const Map &values, const char *phase, int64_t dex, const char *kind) {
    using Access = phmap::priv::hashtable_debug_internal::HashtableDebugAccess<Map>;
    std::fprintf(stderr, "BENCH_NAME_INDEX {\"phase\":\"%s\",\"dex\":%lld,\"kind\":\"%s\","
        "\"size\":%zu,\"capacity\":%zu,\"allocation_bytes\":%zu,\"object_bytes\":%zu}\n",
        phase, (long long)dex, kind, values.size(), values.capacity(),
        Access::AllocatedByteSize(values), sizeof(Map));
}

void DescriptorOccupancy(const std::vector<std::optional<std::string>> &values,
                         const char *phase, uint32_t dex, const char *kind) {
    for (size_t page_size : {64U, 256U, 1024U}) {
        std::vector<uint32_t> pages((values.size() + page_size - 1) / page_size);
        size_t ready = 0, touched = 0, single = 0, largest = 0;
        for (size_t id = 0; id < values.size(); ++id) {
            if (values[id]) { ++pages[id / page_size]; ++ready; }
        }
        for (auto entries : pages) {
            touched += entries != 0;
            single += entries == 1;
            largest = std::max(largest, size_t(entries));
        }
        std::fprintf(stderr, "BENCH_DESCRIPTOR_OCCUPANCY {\"phase\":\"%s\",\"dex\":%u,\"kind\":\"%s\","
            "\"slots\":%zu,\"ready\":%zu,\"page_size\":%zu,\"total_pages\":%zu,\"touched_pages\":%zu,"
            "\"single_entry_pages\":%zu,\"largest_page_entries\":%zu,\"string_object_bytes_per_page\":%zu}\n",
            phase, dex, kind, values.size(), ready, page_size, pages.size(), touched,
            single, largest, page_size * sizeof(std::string));
    }
}

template<class CrossRefs, class Ids>
void CrossRefOccupancy(const CrossRefs &values, const Ids &ids, const std::vector<bool> &defined,
                       const char *phase, uint32_t dex, const char *kind) {
    size_t mapped_defined = 0, mapped_undefined = 0, unmapped_defined = 0, unmapped_undefined = 0;
    for (size_t id = 0; id < values.size(); ++id) {
        if (defined[ids[id].class_idx]) {
            if (values[id]) ++mapped_defined; else ++unmapped_defined;
        } else {
            if (values[id]) ++mapped_undefined; else ++unmapped_undefined;
        }
    }
    std::fprintf(stderr, "BENCH_CROSS_REF {\"phase\":\"%s\",\"dex\":%u,\"kind\":\"%s\","
        "\"slots\":%zu,\"capacity_bytes\":%zu,\"element_bytes\":%zu,\"mapped_defined\":%zu,"
        "\"mapped_undefined\":%zu,\"unmapped_defined\":%zu,\"unmapped_undefined\":%zu}\n",
        phase, dex, kind, values.size(), values.capacity() * sizeof(typename CrossRefs::value_type),
        sizeof(typename CrossRefs::value_type), mapped_defined, mapped_undefined,
        unmapped_defined, unmapped_undefined);
}

template<class Id>
void MemberRows(const std::vector<std::vector<Id>> &values, const char *phase,
                uint32_t dex, const char *kind) {
    size_t entries = 0, capacity = 0, nonempty = 0, noncontiguous = 0;
    for (const auto &row : values) {
        entries += row.size(); capacity += row.capacity(); nonempty += !row.empty();
        for (size_t i = 1; i < row.size(); ++i) {
            if (uint64_t(row[i - 1]) + 1 != row[i]) { ++noncontiguous; break; }
        }
    }
    std::fprintf(stderr, "BENCH_MEMBER_ROWS {\"phase\":\"%s\",\"dex\":%u,\"kind\":\"%s\","
        "\"rows\":%zu,\"nonempty_rows\":%zu,\"noncontiguous_rows\":%zu,\"entries\":%zu,"
        "\"directory_bytes\":%zu,\"payload_capacity_bytes\":%zu}\n",
        phase, dex, kind, values.size(), nonempty, noncontiguous, entries,
        values.capacity() * sizeof(std::vector<Id>), capacity * sizeof(Id));
}
#if DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS
template<bool Promote>
void Descriptors(Counts &count, const VectorDescriptorCache<Promote> &values, const char *phase, uint32_t dex) {
    const auto stats = values.GetStatistics();
    count.index_bytes += stats.fixed_bytes;
    std::fprintf(stderr, "BENCH_VECTOR_DESCRIPTOR_CACHE {\"phase\":\"%s\",\"dex\":%u,"
        "\"promotion_enabled\":%s,\"fixed_bytes\":%zu,\"instrumentation_bytes\":%zu}\n",
        phase, dex, Promote ? "true" : "false", stats.fixed_bytes, stats.instrumentation_bytes);
    for (size_t kind = 0; kind < 2; ++kind) {
        const auto &d = stats.domains[kind];
        count.entries += d.limit; count.ready += d.records;
        count.index_bytes += d.bucket_bytes + d.payload_owner_bytes + d.directory_bytes + d.ready_bytes;
        count.payload_bytes += d.block_bytes + d.dense_object_bytes + d.sparse_char_bytes + d.dense_char_bytes;
        count.buffers += d.bucket_buffers + d.payload_owners + d.block_live + d.directory_live
                + d.char_buffers + (d.dense ? 2 : 0);
        std::fprintf(stderr, "BENCH_VECTOR_DESCRIPTOR_DOMAIN {\"phase\":\"%s\",\"dex\":%u,\"kind\":\"%s\","
            "\"limit\":%zu,\"dense\":%s,\"pending\":%s,\"records\":%zu,\"sparse_records\":%zu,"
            "\"calls\":%llu,\"hits\":%llu,\"dense_hits\":%llu,\"sparse_builds\":%llu,\"dense_builds\":%llu,"
            "\"bucket_bytes\":%zu,\"payload_owner_bytes\":%zu,\"block_bytes\":%zu,\"directory_bytes\":%zu,"
            "\"sparse_char_bytes\":%zu,\"dense_char_bytes\":%zu,\"sso_records\":%zu,"
            "\"dense_object_bytes\":%zu,\"ready_bytes\":%zu,\"conversion_base_bytes\":%zu,"
            "\"sparse_structural_bytes\":%zu,\"conversions\":%zu,\"discarded_records\":%zu,"
            "\"discarded_chars\":%zu,\"structural_at_conversion\":%zu,\"conversion_ns\":%llu}\n",
            phase, dex, kind == 0 ? "method" : "field", d.limit, d.dense ? "true" : "false",
            d.pending ? "true" : "false", d.records, d.sparse_records,
            (unsigned long long)d.calls, (unsigned long long)d.hits, (unsigned long long)d.dense_hits,
            (unsigned long long)d.sparse_builds, (unsigned long long)d.dense_builds,
            d.bucket_bytes, d.payload_owner_bytes, d.block_bytes, d.directory_bytes,
            d.sparse_char_bytes, d.dense_char_bytes, d.sso_records, d.dense_object_bytes, d.ready_bytes,
            d.conversion_base_bytes, d.sparse_structural_bytes, d.conversions, d.discarded_records,
            d.discarded_chars, d.structural_at_conversion, (unsigned long long)d.conversion_ns);
    }
}
#endif
#if DEXKIT_EXPERIMENT_SPARSE_DESCRIPTORS || DEXKIT_EXPERIMENT_HYBRID_DESCRIPTORS
template<bool Promote>
void Descriptors(Counts &count, const HybridDescriptorCache<Promote> &values, const char *phase, uint32_t dex) {
    const auto stats = values.GetStatistics();
    count.index_bytes += stats.fixed_bytes;
    count.entries += stats.limits[0] + stats.limits[1];
    std::fprintf(stderr, "BENCH_HYBRID_DESCRIPTOR_CACHE {\"phase\":\"%s\",\"dex\":%u,"
        "\"promotion_enabled\":%s,\"fixed_bytes\":%zu,\"instrumentation_bytes\":%zu}\n", phase, dex,
        Promote ? "true" : "false", stats.fixed_bytes, stats.instrumentation_bytes);
    for (size_t shard = 0; shard < stats.tables.size(); ++shard) for (size_t kind = 0; kind < 2; ++kind) {
        const auto &table = stats.tables[shard][kind];
        const auto &blocks = table.allocations.blocks, &directory = table.allocations.directory;
        count.index_bytes += table.bucket_bytes + table.dense_bytes + table.payload_owner_bytes + directory.bytes;
        count.payload_bytes += blocks.bytes + table.char_bytes;
        count.ready += table.records;
        count.buffers += (table.capacity != 0) + 2 * (table.dense_bytes != 0) + (table.payload_owner_bytes != 0)
                + blocks.live + directory.live + table.char_buffers;
        std::fprintf(stderr, "BENCH_HYBRID_DESCRIPTOR_TABLE {\"phase\":\"%s\",\"dex\":%u,"
            "\"shard\":%zu,\"kind\":\"%s\",\"slots\":%zu,\"records\":%zu,\"capacity\":%zu,"
            "\"bucket_bytes\":%zu,\"dense_bytes\":%zu,\"payload_owner_bytes\":%zu,"
            "\"payload_instrumentation_bytes\":%zu,\"block_bytes\":%zu,\"block_live\":%zu,"
            "\"block_allocations\":%zu,\"block_peak_bytes\":%zu,\"directory_bytes\":%zu,"
            "\"directory_live\":%zu,\"directory_allocations\":%zu,\"directory_peak_bytes\":%zu,"
            "\"char_bytes\":%zu,\"char_buffers\":%zu,\"sso_records\":%zu,"
            "\"calls\":%llu,\"hits\":%llu,\"dense_hits\":%llu,\"growths\":%zu,"
            "\"largest_bucket_overlap\":%zu,\"promotions\":%zu,\"promotion_records\":%zu,"
            "\"promotion_capacity\":%zu,\"promotion_hash_bytes\":%zu,\"promotion_payload_bytes\":%zu,"
            "\"promotion_overlap_bytes\":%zu,\"promotion_ns\":%llu}\n", phase, dex, shard,
            kind == 0 ? "method" : "field", table.slots, table.records, table.capacity,
            table.bucket_bytes, table.dense_bytes, table.payload_owner_bytes, table.payload_instrumentation_bytes,
            blocks.bytes, blocks.live, blocks.allocations, blocks.peak_bytes,
            directory.bytes, directory.live, directory.allocations, directory.peak_bytes,
            table.char_bytes, table.char_buffers, table.sso_records,
            (unsigned long long)table.calls, (unsigned long long)table.hits, (unsigned long long)table.dense_hits,
            table.growths, table.largest_bucket_overlap, table.promotions, table.promotion_records,
            table.promotion_capacity, table.promotion_hash_bytes, table.promotion_payload_bytes,
            table.promotion_overlap_bytes, (unsigned long long)table.promotion_ns);
    }
}
#endif
#if DEXKIT_EXPERIMENT_NODE_DESCRIPTORS
void Descriptors(Counts &count, const NodeDescriptorCache &values, const char *phase, uint32_t dex) {
    const auto stats = values.GetStatistics();
    count.index_bytes += stats.fixed_bytes;
    count.entries += stats.limits[0] + stats.limits[1];
    std::fprintf(stderr, "BENCH_NODE_DESCRIPTOR_CACHE {\"phase\":\"%s\",\"dex\":%u,"
        "\"fixed_bytes\":%zu,\"instrumentation_bytes\":%zu}\n", phase, dex,
        stats.fixed_bytes, stats.instrumentation_bytes);
    for (size_t shard = 0; shard < stats.tables.size(); ++shard) {
        for (size_t kind = 0; kind < 2; ++kind) {
            const auto &table = stats.tables[shard][kind];
            count.index_bytes += table.bucket_bytes;
            count.payload_bytes += table.node_bytes + table.char_bytes;
            count.ready += table.records;
            count.buffers += (table.capacity != 0) + table.records + table.char_buffers;
            std::fprintf(stderr, "BENCH_NODE_DESCRIPTOR_TABLE {\"phase\":\"%s\",\"dex\":%u,"
                "\"shard\":%zu,\"kind\":\"%s\",\"records\":%zu,\"capacity\":%zu,\"bucket_bytes\":%zu,"
                "\"node_bytes\":%zu,\"char_bytes\":%zu,\"sso_records\":%zu,\"calls\":%llu,\"hits\":%llu,"
                "\"growths\":%llu,\"largest_bucket_overlap\":%zu}\n", phase, dex, shard,
                kind == 0 ? "method" : "field", table.records, table.capacity, table.bucket_bytes,
                table.node_bytes, table.char_bytes, table.sso_records,
                (unsigned long long)table.calls, (unsigned long long)table.hits,
                (unsigned long long)table.growths, table.largest_bucket_overlap);
        }
    }
}
#endif
#if DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS
void Descriptors(Counts &count, const PointerDescriptorCache &values) {
    const auto stats = values.GetStatistics();
    count.index_bytes += stats.index_bytes;
    count.payload_bytes += stats.payload_bytes;
    count.entries += values.size();
    count.ready += stats.records;
    count.buffers += stats.buffers;
}
#endif
#if DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS
void Descriptors(Counts &count, const PagedDescriptorCache &values, const char *phase,
                 uint32_t dex_id, const char *kind) {
    const auto stats = values.GetStatistics();
    count.index_bytes += stats.index_bytes;
    count.payload_bytes += stats.record_capacity_bytes;
    count.entries += values.size();
    count.ready += stats.records;
    count.buffers += stats.buffers;
    std::fprintf(stderr,
        "BENCH_DESCRIPTOR_STORAGE {\"phase\":\"%s\",\"dex\":%u,\"kind\":\"%s\",\"entries\":%zu,\"records\":%zu,\"pages\":%zu,\"blocks\":%zu,"
        "\"capacity_bytes\":%zu,\"used_bytes\":%zu,\"character_bytes\":%zu,\"record_bytes\":%zu,"
        "\"cold_lock_count\":%llu,\"cold_lock_wait_ns\":%llu}\n",
        phase, dex_id, kind, values.size(), stats.records, stats.pages, stats.blocks, stats.record_capacity_bytes,
        stats.record_used_bytes, stats.character_bytes, stats.record_bytes,
        (unsigned long long) stats.cold_lock_count, (unsigned long long) stats.cold_lock_wait_ns);
}
#endif
}

void BenchmarkDiagnostics::DumpCallerBuild(const DexKit &bridge, const char *phase) {
    uint64_t directory = 0, payload = 0, counters = 0, pending = 0, edges = 0;
    for (const auto &owner : bridge.dex_items) {
#if DEXKIT_EXPERIMENT_COMPACT_CALLERS
        const auto &index = owner->method_caller_ids;
        directory += index.offsets_.capacity() * sizeof(CacheOffset);
        payload += index.edges_ * sizeof(CompactCallerIndex::Entry);
        counters += index.build_.capacity() * sizeof(CacheOffset);
        edges += index.edges_;
#else
        directory += owner->method_caller_ids.capacity() * sizeof(decltype(owner->method_caller_ids)::value_type);
        for (const auto &row : owner->method_caller_ids) {
            payload += row.capacity() * sizeof(MethodReference);
            edges += row.size();
        }
#endif
        pending += owner->pending_aggregate_method_work_items.capacity() * sizeof(DexItem::PendingAggregateMethodWorkItem);
    }
    std::fprintf(stderr,
        "BENCH_CALLER_BUILD {\"phase\":\"%s\",\"directory_bytes\":%llu,\"payload_capacity_bytes\":%llu,"
        "\"count_cursor_capacity_bytes\":%llu,\"pending_capacity_bytes\":%llu,\"final_edges\":%llu}\n",
        phase, (unsigned long long)directory, (unsigned long long)payload, (unsigned long long)counters,
        (unsigned long long)pending, (unsigned long long)edges);
}

void BenchmarkDiagnostics::Dump(const DexKit &bridge, const char *phase) {
    const auto begin = std::chrono::steady_clock::now();
#if DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS
    std::fprintf(stderr, "BENCH_VECTOR_DESCRIPTOR_MAINTENANCE {\"phase\":\"%s\","
        "\"runs\":%llu,\"entrant_wait_ns\":%llu,\"maintenance_ns\":%llu,\"pending\":%s}\n",
        phase, (unsigned long long)bridge.descriptor_maintenance_runs_,
        (unsigned long long)bridge.descriptor_maintenance_wait_ns_,
        (unsigned long long)bridge.descriptor_maintenance_ns_,
        bridge.descriptor_maintenance_pending_.load() ? "true" : "false");
#endif
    std::map<std::string, Counts> counts;
    std::set<const MemMap *> images;
    std::array<uint64_t, 3> method_builds{}, field_builds{}, descriptor_bytes{};
    std::array<uint64_t, 3> method_calls{}, field_calls{};
    std::array<uint64_t, 4> field_reverse_reads{};
    std::array<uint64_t, 2> warmup_calls{};
    uint64_t method_comparisons = 0, field_comparisons = 0;
    uint64_t mapped_bytes = 0, method_ids = 0, field_ids = 0;
    for (const auto &owner : bridge.dex_items) {
        const auto &item = *owner;
        for (size_t i = 0; i < field_reverse_reads.size(); ++i)
            field_reverse_reads[i] += item.benchmark_field_reverse_reads[i].load(std::memory_order_relaxed);
        for (size_t i = 0; i < warmup_calls.size(); ++i)
            warmup_calls[i] += item.benchmark_warmup_calls[i].load(std::memory_order_relaxed);
        const auto methods = item.reader.MethodIds().size();
        method_ids += methods;
        field_ids += item.reader.FieldIds().size();
        if (std::string_view(phase) == "pre_close") {
            NameIndex(item.type_ids_map, phase, item.dex_id, "type_ids");
#if !DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS && !DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS && !DEXKIT_EXPERIMENT_NODE_DESCRIPTORS && !DEXKIT_EXPERIMENT_SPARSE_DESCRIPTORS && !DEXKIT_EXPERIMENT_HYBRID_DESCRIPTORS && !DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS && !DEXKIT_EXPERIMENT_UNCACHED_DESCRIPTORS
            DescriptorOccupancy(item.method_descriptors, phase, item.dex_id, "method");
            DescriptorOccupancy(item.field_descriptors, phase, item.dex_id, "field");
#endif
            CrossRefOccupancy(item.method_cross_info, item.reader.MethodIds(), item.type_def_flag,
                              phase, item.dex_id, "method");
            CrossRefOccupancy(item.field_cross_info, item.reader.FieldIds(), item.type_def_flag,
                              phase, item.dex_id, "field");
            MemberRows(item.class_method_ids, phase, item.dex_id, "method");
            MemberRows(item.class_field_ids, phase, item.dex_id, "field");
        }
        if (images.insert(item._image.get()).second) mapped_bytes += item._image->len();
        Slots(counts["lazy_opcodes"], item.lazy_method_opcode_slots, methods);
        Slots(counts["lazy_strings"], item.lazy_method_using_string_slots, methods);
        Slots(counts["lazy_numbers"], item.lazy_using_numbers_slots, methods);
#if DEXKIT_EXPERIMENT_UNCACHED_DESCRIPTORS
        // This census covers persistent bridge storage. Owning result strings
        // are temporary output allocations and remain included in OS peaks.
        counts["descriptors"];
#elif DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS
        Descriptors(counts["descriptors"], item.vector_descriptors, phase, item.dex_id);
#elif DEXKIT_EXPERIMENT_SPARSE_DESCRIPTORS || DEXKIT_EXPERIMENT_HYBRID_DESCRIPTORS
        Descriptors(counts["descriptors"], item.hybrid_descriptors, phase, item.dex_id);
#elif DEXKIT_EXPERIMENT_NODE_DESCRIPTORS
        Descriptors(counts["descriptors"], item.node_descriptors, phase, item.dex_id);
#elif DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS
        Descriptors(counts["descriptors"], item.method_descriptors, phase, item.dex_id, "method");
        Descriptors(counts["descriptors"], item.field_descriptors, phase, item.dex_id, "field");
#else
        Descriptors(counts["descriptors"], item.method_descriptors);
        Descriptors(counts["descriptors"], item.field_descriptors);
#endif
        for (size_t reason = 0; reason < method_builds.size(); ++reason) {
            method_calls[reason] += item.descriptor_diagnostics.method_calls[reason].load(std::memory_order_relaxed);
            field_calls[reason] += item.descriptor_diagnostics.field_calls[reason].load(std::memory_order_relaxed);
            method_builds[reason] += item.descriptor_diagnostics.method_builds[reason].load(std::memory_order_relaxed);
            field_builds[reason] += item.descriptor_diagnostics.field_builds[reason].load(std::memory_order_relaxed);
            descriptor_bytes[reason] += item.descriptor_diagnostics.materialized_bytes[reason].load(std::memory_order_relaxed);
        }
        method_comparisons += item.descriptor_diagnostics.method_comparisons.load(std::memory_order_relaxed);
        field_comparisons += item.descriptor_diagnostics.field_comparisons.load(std::memory_order_relaxed);
#if (DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS || DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS) && !DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS && !DEXKIT_EXPERIMENT_NODE_DESCRIPTORS && !DEXKIT_EXPERIMENT_SPARSE_DESCRIPTORS && !DEXKIT_EXPERIMENT_HYBRID_DESCRIPTORS && !DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS && !DEXKIT_EXPERIMENT_UNCACHED_DESCRIPTORS
        auto &publication = counts["descriptor_publication"];
#if DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS
        publication.index_bytes += sizeof(item.descriptor_mutexes);
        std::fprintf(stderr, "BENCH_POINTER_DESCRIPTORS {\"phase\":\"%s\",\"dex\":%u,\"sso_records\":%zu}\n",
                phase, item.dex_id, item.method_descriptors.GetStatistics().sso_records
                                    + item.field_descriptors.GetStatistics().sso_records);
#else
        publication.index_bytes += (item.reader.MethodIds().size() + item.reader.FieldIds().size())
                * sizeof(std::atomic<uint8_t>) + sizeof(item.descriptor_mutexes);
        publication.entries += item.reader.MethodIds().size() + item.reader.FieldIds().size();
        publication.buffers += 2;
#endif
#endif
#if DEXKIT_EXPERIMENT_COMPACT_STRINGS
        const auto &index = item.method_using_string_ids;
        auto &strings = counts["using_strings"];
        strings.index_bytes += index.offsets_.capacity() * sizeof(CacheOffset)
                             + index.lengths_.capacity() * sizeof(uint32_t);
        strings.entries += index.offsets_.size();
        strings.payload_bytes += index.ids_.capacity() * sizeof(uint32_t);
        strings.buffers += (index.offsets_.capacity() != 0) + (index.lengths_.capacity() != 0)
                         + (index.ids_.capacity() != 0);
        for (auto length : index.lengths_) strings.ready += length != 0;
        if (index.growth_count_) std::fprintf(stderr, "BENCH_GROWTH {\"phase\":\"%s\",\"dex\":%u,\"growths\":%zu,\"moved_id_bytes\":%zu,\"peak_overlap_bytes\":%zu,\"ids\":%zu,\"capacity\":%zu}\n",
                phase, item.dex_id, index.growth_count_, index.moved_bytes_, index.overlap_bytes_, index.ids_.size(), index.ids_.capacity());
#else
        Rows(counts["using_strings"], item.method_using_string_ids);
#endif
#if DEXKIT_EXPERIMENT_COMPACT_INVOKES
        const auto &invoke_index = item.method_invoking_ids;
        auto &invokes = counts["invokes"];
        invokes.index_bytes += invoke_index.offsets_.capacity() * sizeof(CacheOffset)
                             + invoke_index.lengths_.capacity() * sizeof(uint32_t);
        invokes.entries += invoke_index.offsets_.size();
        invokes.payload_bytes += invoke_index.ids_.capacity() * sizeof(InvokeOperandId);
        invokes.buffers += (invoke_index.offsets_.capacity() != 0) + (invoke_index.lengths_.capacity() != 0)
                         + (invoke_index.ids_.capacity() != 0);
        for (auto length : invoke_index.lengths_) invokes.ready += length != 0;
        if (invoke_index.growth_count_) std::fprintf(stderr, "BENCH_INVOKE_GROWTH {\"phase\":\"%s\",\"dex\":%u,\"growths\":%zu,\"moved_id_bytes\":%zu,\"peak_overlap_bytes\":%zu,\"ids\":%zu,\"capacity\":%zu}\n",
                phase, item.dex_id, invoke_index.growth_count_, invoke_index.moved_bytes_, invoke_index.overlap_bytes_, invoke_index.ids_.size(), invoke_index.ids_.capacity());
#else
        Rows(counts["invokes"], item.method_invoking_ids);
#endif
#if DEXKIT_EXPERIMENT_COMPACT_CALLERS
        const auto &caller_index = item.method_caller_ids;
        auto &callers = counts["callers"];
        callers.index_bytes += caller_index.offsets_.capacity() * sizeof(CacheOffset);
        callers.payload_bytes += caller_index.edges_ * sizeof(CompactCallerIndex::Entry);
        callers.entries += caller_index.size();
        callers.buffers += (caller_index.offsets_.capacity() != 0) + (caller_index.edges_ != 0);
        for (size_t method = 0; method < caller_index.size(); ++method) callers.ready += !caller_index[method].empty();
        Flat(counts["caller_build"], caller_index.build_);
#else
        Rows(counts["callers"], item.method_caller_ids);
#endif
        if (!item.method_caller_ids.empty()) {
            uint64_t hash = 14695981039346656037ULL, edges = 0, largest = 0;
            auto append = [&hash](uint64_t value, size_t bytes) {
                for (size_t i = 0; i < bytes; ++i) {
                    hash = (hash ^ (value & 255)) * 1099511628211ULL;
                    value >>= 8;
                }
            };
            for (size_t method = 0; method < item.method_caller_ids.size(); ++method) {
                const auto &row = item.method_caller_ids[method];
                edges += row.size();
                largest = std::max<uint64_t>(largest, row.size());
                append(row.size(), 8);
                for (auto [dex, caller] : row) { append(dex, 2); append(caller, 4); }
            }
            std::fprintf(stderr, "BENCH_CALLER_ROWS {\"phase\":\"%s\",\"dex\":%u,\"rows\":%zu,"
                "\"edges\":%llu,\"largest_row\":%llu,\"ordered_hash\":\"%016llx\"}\n",
                phase, item.dex_id, item.method_caller_ids.size(), (unsigned long long)edges,
                (unsigned long long)largest, (unsigned long long)hash);
        }
#if DEXKIT_EXPERIMENT_COMPACT_FIELDS
        const auto &field_index = item.method_using_field_ids;
        auto &fields = counts["using_fields"];
        fields.index_bytes += field_index.offsets_.capacity() * sizeof(CacheOffset)
                            + field_index.lengths_.capacity() * sizeof(uint32_t);
        fields.entries += field_index.offsets_.size();
        fields.payload_bytes += field_index.uses_.capacity() * sizeof(CompactFieldIndex::Use);
        fields.buffers += (field_index.offsets_.capacity() != 0) + (field_index.lengths_.capacity() != 0)
                        + (field_index.uses_.capacity() != 0);
        for (auto length : field_index.lengths_) fields.ready += length != 0;
        if (field_index.growth_count_) std::fprintf(stderr, "BENCH_FIELD_GROWTH {\"phase\":\"%s\",\"dex\":%u,\"growths\":%zu,\"moved_use_bytes\":%zu,\"peak_overlap_bytes\":%zu,\"uses\":%zu,\"capacity\":%zu}\n",
                phase, item.dex_id, field_index.growth_count_, field_index.moved_bytes_, field_index.overlap_bytes_, field_index.uses_.size(), field_index.uses_.capacity());
#else
        Rows(counts["using_fields"], item.method_using_field_ids);
#endif
        Rows(counts["field_readers"], item.field_get_method_ids);
        Rows(counts["field_writers"], item.field_put_method_ids);
        Rows(counts["full_numbers"], item.method_using_numbers);
        Rows(counts["class_members"], item.class_method_ids);
        Rows(counts["class_members"], item.class_field_ids);
#if !DEXKIT_EXPERIMENT_RAW_INTERFACES
        Rows(counts["class_members"], item.class_interface_ids);
#endif
        Rows(counts["pending_cross_refs"], item.pending_cross_ref_method_ids);
        Rows(counts["pending_cross_refs"], item.pending_cross_ref_field_ids);
        Flat(counts["aggregate_work"], item.pending_aggregate_method_work_items);
        Flat(counts["aggregate_work"], item.pending_aggregate_field_work_items);
        Flat(counts["base_arrays"], item.strings);
        Flat(counts["base_arrays"], item.type_names);
        Flat(counts["base_arrays"], item.type_name_array_count);
        Flat(counts["base_arrays"], item.type_def_idx);
#if !DEXKIT_EXPERIMENT_RAW_SOURCE_FILES
        Flat(counts["base_arrays"], item.class_source_files);
#endif
        Flat(counts["base_arrays"], item.class_access_flags);
        Flat(counts["base_arrays"], item.method_access_flags);
        Flat(counts["base_arrays"], item.field_access_flags);
        Flat(counts["base_arrays"], item.method_codes);
        Flat(counts["base_arrays"], item.proto_type_list);
        Flat(counts["base_arrays"], item.method_cross_info);
        Flat(counts["base_arrays"], item.field_cross_info);
        Flat(counts["annotations"], item.class_annotations);
        Flat(counts["annotations"], item.method_annotations);
        Flat(counts["annotations"], item.field_annotations);
        Rows(counts["annotations"], item.method_parameter_annotations);
        Flat(counts["full_opcodes"], item.method_opcode_seq);
        for (const auto &row : item.method_opcode_seq) if (row) {
            counts["full_opcodes"].payload_bytes += row->capacity();
            counts["full_opcodes"].buffers += row->capacity() != 0;
        }
    }
    if (std::string_view(phase) == "pre_close")
        NameIndex(bridge.class_declare_dex_map, phase, -1, "class_declarations");
    std::fprintf(stderr, "BENCH_CENSUS {\"phase\":\"%s\",\"dexes\":%zu,\"method_ids\":%llu,"
        "\"field_ids\":%llu,\"mapped_bytes\":%llu,\"slot_sizes\":[%zu,%zu,%zu]}\n",
        phase, bridge.dex_items.size(), (unsigned long long) method_ids, (unsigned long long) field_ids,
        (unsigned long long) mapped_bytes, sizeof(DexItem::LazyMethodOpCodesSlot),
        sizeof(DexItem::LazyMethodUsingStringsSlot), sizeof(DexItem::LazyUsingNumbersSlot));
    for (const auto &[name, count] : counts) {
        std::fprintf(stderr, "BENCH_CENSUS {\"phase\":\"%s\",\"category\":\"%s\","
            "\"index_bytes\":%llu,\"payload_capacity_bytes\":%llu,\"entries\":%llu,"
            "\"nonempty_or_ready\":%llu,\"live_buffers\":%llu}\n", phase, name.c_str(),
            (unsigned long long) count.index_bytes, (unsigned long long) count.payload_bytes,
            (unsigned long long) count.entries, (unsigned long long) count.ready,
            (unsigned long long) count.buffers);
    }
    for (size_t reason = 0; reason < method_builds.size(); ++reason) {
        const char *names[] = {"output", "cross_reference", "lookup"};
        std::fprintf(stderr,
            "BENCH_DESCRIPTOR_ACCESS {\"phase\":\"%s\",\"reason\":\"%s\",\"method_calls\":%llu,\"field_calls\":%llu,"
            "\"method_hits\":%llu,\"field_hits\":%llu}\n", phase, names[reason],
            (unsigned long long)method_calls[reason], (unsigned long long)field_calls[reason],
            (unsigned long long)(method_calls[reason] - method_builds[reason]),
            (unsigned long long)(field_calls[reason] - field_builds[reason]));
        std::fprintf(stderr,
            "BENCH_DESCRIPTOR {\"reason\":\"%s\",\"method_builds\":%llu,\"field_builds\":%llu,\"materialized_bytes\":%llu}\n",
            names[reason], (unsigned long long) method_builds[reason],
            (unsigned long long) field_builds[reason], (unsigned long long) descriptor_bytes[reason]);
    }
    std::fprintf(stderr, "BENCH_DESCRIPTOR {\"method_comparisons\":%llu,\"field_comparisons\":%llu}\n",
        (unsigned long long) method_comparisons, (unsigned long long) field_comparisons);
    std::fprintf(stderr, "BENCH_FIELD_REVERSE {\"phase\":\"%s\",\"get_matcher\":%llu,\"put_matcher\":%llu,"
        "\"get_metadata\":%llu,\"put_metadata\":%llu}\n", phase,
        (unsigned long long) field_reverse_reads[0], (unsigned long long) field_reverse_reads[1],
        (unsigned long long) field_reverse_reads[2], (unsigned long long) field_reverse_reads[3]);
    std::fprintf(stderr, "BENCH_WARMUP {\"phase\":\"%s\",\"local_init_calls\":%llu,\"resolve_calls\":%llu,\"aggregate_calls\":%llu}\n",
        phase, (unsigned long long) warmup_calls[0], (unsigned long long) warmup_calls[1],
        (unsigned long long) bridge.benchmark_aggregate_calls.load(std::memory_order_relaxed));
    std::fprintf(stderr, "BENCH_CENSUS {\"phase\":\"%s\",\"diagnostic_ns\":%lld}\n", phase,
        (long long) std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count());
}

void BenchmarkDiagnostics::CheckMetadata(std::string_view apk) {
    const auto require = [](bool value) {
        if (!value) { std::fprintf(stderr, "Metadata equivalence check failed\n"); std::abort(); }
    };
    const auto same_numbers = [](const auto &left, const auto &right) {
        if (left.size() != right.size()) return false;
        const auto bits = [](const EncodeNumber &number) -> int64_t {
            switch (number.type) {
                case BYTE: return number.value.L8;
                case SHORT: return number.value.L16;
                case INT: case FLOAT: return number.value.L32.int_value;
                case LONG: case DOUBLE: return number.value.L64.long_value;
                default: std::abort();
            }
        };
        for (size_t i = 0; i < left.size(); ++i)
            if (left[i].type != right[i].type || bits(left[i]) != bits(right[i])) return false;
        return true;
    };
    DexKit full(apk), lazy(apk), cold(apk);
    require(full.GetDexNum() > 0 && full.GetDexNum() == lazy.GetDexNum());
    full.InitFullCache();
    uint64_t methods = 0, empty_methods = 0, duplicate_strings = 0;
    for (size_t d = 0; d < full.dex_items.size(); ++d) {
        auto &reference = *full.dex_items[d];
        auto &item = *lazy.dex_items[d];
        const auto count = reference.reader.MethodIds().size();
        methods += count;
        for (uint32_t m = 0; m < count; ++m) {
            const auto strings = item.GetUsingStrings(m);
            require(strings == reference.GetUsingStrings(m));
            require(item.GetMethodOpCodes(m) == reference.GetMethodOpCodes(m));
            require(same_numbers(item.GetUsingNumbers(m), reference.GetUsingNumbers(m)));
            empty_methods += item.method_codes[m] == nullptr;
            duplicate_strings += std::set<std::string_view>(strings.begin(), strings.end()).size() < strings.size();
        }
#if DEXKIT_EXPERIMENT_LAZY_DIRECTORIES
        require(!reference.lazy_method_opcode_slots && !reference.lazy_method_using_string_slots
                && !reference.lazy_using_numbers_slots);
#endif
        // Concurrent first access to different and repeated slots of one DEX.
        std::latch start(1);
        std::vector<std::thread> readers;
        for (uint32_t worker = 0; worker < 8; ++worker) readers.emplace_back([&, worker] {
            start.wait();
            for (uint32_t m = worker % 4; m < count; m += 4) {
                auto &target = *cold.dex_items[d];
                require(target.GetUsingStrings(m) == reference.GetUsingStrings(m));
                require(target.GetMethodOpCodes(m) == reference.GetMethodOpCodes(m));
                require(same_numbers(target.GetUsingNumbers(m), reference.GetUsingNumbers(m)));
            }
        });
        start.count_down();
        for (auto &reader : readers) reader.join();
    }
    lazy.InitFullCache();
    for (size_t d = 0; d < lazy.dex_items.size(); ++d) {
        auto &item = *lazy.dex_items[d];
        for (uint32_t m = 0; m < item.reader.MethodIds().size(); ++m) {
            // Previously published lazy payloads remain alive after full warm-up.
            require(*item.lazy_method_opcode_slots[m].data == item.GetMethodOpCodes(m));
            require(same_numbers(*item.lazy_using_numbers_slots[m].data, item.GetUsingNumbers(m)));
            const auto &ids = *item.lazy_method_using_string_slots[m].data;
            std::vector<std::string_view> before;
            for (auto id : ids) before.push_back(item.strings[id]);
            require(before == item.GetUsingStrings(m));
        }
    }
    std::fprintf(stderr, "CHECK_METADATA {\"methods\":%llu,\"empty_methods\":%llu,\"duplicate_string_methods\":%llu,\"workers\":8,\"passed\":true}\n",
            (unsigned long long) methods, (unsigned long long) empty_methods, (unsigned long long) duplicate_strings);
}
}
#endif

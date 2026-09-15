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
#if DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS
void Descriptors(Counts &count, const PagedDescriptorCache &values) {
    const auto stats = values.GetStatistics();
    count.index_bytes += stats.index_bytes;
    count.payload_bytes += stats.record_capacity_bytes;
    count.entries += values.size();
    count.ready += stats.records;
    count.buffers += stats.buffers;
    std::fprintf(stderr,
        "BENCH_DESCRIPTOR_STORAGE {\"entries\":%zu,\"records\":%zu,\"pages\":%zu,\"blocks\":%zu,"
        "\"capacity_bytes\":%zu,\"used_bytes\":%zu,\"character_bytes\":%zu,\"record_bytes\":%zu,"
        "\"cold_lock_count\":%llu,\"cold_lock_wait_ns\":%llu}\n",
        values.size(), stats.records, stats.pages, stats.blocks, stats.record_capacity_bytes,
        stats.record_used_bytes, stats.character_bytes, stats.record_bytes,
        (unsigned long long) stats.cold_lock_count, (unsigned long long) stats.cold_lock_wait_ns);
}
#endif
}

void BenchmarkDiagnostics::Dump(const DexKit &bridge, const char *phase) {
    const auto begin = std::chrono::steady_clock::now();
    std::map<std::string, Counts> counts;
    std::set<const MemMap *> images;
    std::array<uint64_t, 3> method_builds{}, field_builds{}, descriptor_bytes{};
    uint64_t method_comparisons = 0, field_comparisons = 0;
    uint64_t mapped_bytes = 0, method_ids = 0, field_ids = 0;
    for (const auto &owner : bridge.dex_items) {
        const auto &item = *owner;
        const auto methods = item.reader.MethodIds().size();
        method_ids += methods;
        field_ids += item.reader.FieldIds().size();
        if (images.insert(item._image.get()).second) mapped_bytes += item._image->len();
        Slots(counts["lazy_opcodes"], item.lazy_method_opcode_slots, methods);
        Slots(counts["lazy_strings"], item.lazy_method_using_string_slots, methods);
        Slots(counts["lazy_numbers"], item.lazy_using_numbers_slots, methods);
        Descriptors(counts["descriptors"], item.method_descriptors);
        Descriptors(counts["descriptors"], item.field_descriptors);
        for (size_t reason = 0; reason < method_builds.size(); ++reason) {
            method_builds[reason] += item.descriptor_diagnostics.method_builds[reason].load(std::memory_order_relaxed);
            field_builds[reason] += item.descriptor_diagnostics.field_builds[reason].load(std::memory_order_relaxed);
            descriptor_bytes[reason] += item.descriptor_diagnostics.materialized_bytes[reason].load(std::memory_order_relaxed);
        }
        method_comparisons += item.descriptor_diagnostics.method_comparisons.load(std::memory_order_relaxed);
        field_comparisons += item.descriptor_diagnostics.field_comparisons.load(std::memory_order_relaxed);
#if DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS && !DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS
        auto &publication = counts["descriptor_publication"];
        publication.index_bytes += (item.reader.MethodIds().size() + item.reader.FieldIds().size())
                * sizeof(std::atomic<uint8_t>) + sizeof(item.descriptor_mutexes);
        publication.buffers += 2;
#endif
#if DEXKIT_EXPERIMENT_COMPACT_STRINGS
        const auto &index = item.method_using_string_ids;
        auto &strings = counts["using_strings"];
        strings.index_bytes += index.offsets_.capacity() * sizeof(size_t)
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
        Rows(counts["invokes"], item.method_invoking_ids);
        Rows(counts["callers"], item.method_caller_ids);
        Rows(counts["using_fields"], item.method_using_field_ids);
        Rows(counts["field_readers"], item.field_get_method_ids);
        Rows(counts["field_writers"], item.field_put_method_ids);
        Rows(counts["full_numbers"], item.method_using_numbers);
        Rows(counts["class_members"], item.class_method_ids);
        Rows(counts["class_members"], item.class_field_ids);
        Rows(counts["class_members"], item.class_interface_ids);
        Rows(counts["pending_cross_refs"], item.pending_cross_ref_method_ids);
        Rows(counts["pending_cross_refs"], item.pending_cross_ref_field_ids);
        Flat(counts["aggregate_work"], item.pending_aggregate_method_work_items);
        Flat(counts["aggregate_work"], item.pending_aggregate_field_work_items);
        Flat(counts["base_arrays"], item.strings);
        Flat(counts["base_arrays"], item.type_names);
        Flat(counts["base_arrays"], item.type_name_array_count);
        Flat(counts["base_arrays"], item.type_def_idx);
        Flat(counts["base_arrays"], item.class_source_files);
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
            "BENCH_DESCRIPTOR {\"reason\":\"%s\",\"method_builds\":%llu,\"field_builds\":%llu,\"materialized_bytes\":%llu}\n",
            names[reason], (unsigned long long) method_builds[reason],
            (unsigned long long) field_builds[reason], (unsigned long long) descriptor_bytes[reason]);
    }
    std::fprintf(stderr, "BENCH_DESCRIPTOR {\"method_comparisons\":%llu,\"field_comparisons\":%llu}\n",
        (unsigned long long) method_comparisons, (unsigned long long) field_comparisons);
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

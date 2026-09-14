#include "benchmark_diagnostics.h"

#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include "dexkit.h"
#include <map>
#include <set>

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
}

void BenchmarkDiagnostics::Dump(const DexKit &bridge, const char *phase) {
    const auto begin = std::chrono::steady_clock::now();
    std::map<std::string, Counts> counts;
    std::set<const MemMap *> images;
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
    std::fprintf(stderr, "BENCH_CENSUS {\"phase\":\"%s\",\"diagnostic_ns\":%lld}\n", phase,
        (long long) std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count());
}
}
#endif

#include "benchmark_diagnostics.h"

#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include "dexkit.h"
#include "member_descriptor_view.h"
#include <array>
#include <latch>
#include <map>
#include <set>
#include <thread>
#include <type_traits>

namespace dexkit {

void BenchmarkDiagnostics::CheckDenseDescriptors(std::string_view apk) {
    const auto require = [](bool condition, const char *message) {
        if (!condition) {
            std::fprintf(stderr, "Dense descriptor check failed: %s\n", message);
            std::abort();
        }
    };
    DexKit bridge(apk, 1);
    require(bridge.dex_items.size() == 1, "one DEX");
    auto &item = *bridge.dex_items[0];
    constexpr uint32_t count = 60000;
    require(item.reader.MethodIds().size() == count && item.reader.FieldIds().size() == count, "fixture size");
    const auto method = item.GetMethodDescriptor(0), field = item.GetFieldDescriptor(0);
    const auto *method_address = method.data(), *field_address = field.data();
    const auto check = [&](uint32_t index) {
        char expected[32];
        std::snprintf(expected, sizeof(expected), "LA;->m%05u()V", index);
        require(item.GetMethodDescriptor(index) == expected, "method content");
        std::snprintf(expected, sizeof(expected), "LA;->f%05u:I", index);
        require(item.GetFieldDescriptor(index) == expected, "field content");
    };
#if !DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS && !DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS && !DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS
    for (uint32_t i = 0; i < count; ++i) check(i);
#endif
    std::latch start(1);
    std::vector<std::thread> readers;
    std::array<std::string_view, 8> same_methods, same_fields;
    for (size_t worker = 0; worker < same_methods.size(); ++worker) readers.emplace_back([&, worker] {
        start.wait();
        same_methods[worker] = item.GetMethodDescriptor(1);
        same_fields[worker] = item.GetFieldDescriptor(1);
        for (uint32_t i = 0; i < count; ++i) check((i + worker * 997) % count);
    });
    start.count_down();
    for (auto &reader : readers) reader.join();
    for (size_t i = 0; i < same_methods.size(); ++i) {
        require(same_methods[i] == same_methods[0] && same_methods[i].data() == same_methods[0].data(), "method publication");
        require(same_fields[i] == same_fields[0] && same_fields[i].data() == same_fields[0].data(), "field publication");
    }
    require(method == "LA;->m00000()V" && method.data() == method_address, "retained method");
    require(field == "LA;->f00000:I" && field.data() == field_address, "retained field");
    uint64_t built = 0;
    for (size_t reason = 0; reason < 3; ++reason)
        built += item.descriptor_diagnostics.method_builds[reason].load()
               + item.descriptor_diagnostics.field_builds[reason].load();
    require(built == 2 * count, "exactly one construction per slot");
    Dump(bridge, "dense-descriptors-complete");
    std::fprintf(stderr, "CHECK_DENSE_DESCRIPTORS {\"records\":120000,\"threads\":8,\"passed\":true}\n");
}

// Run this in a separate all-flags-off artifact and freeze stdout. Comparing
// its complete bytes with experimental artifacts is an independent oracle for
// descriptors and metadata, including local IDs and ordered interface lists.
void BenchmarkDiagnostics::DumpSymbols(std::string_view apk) {
    DexKit bridge(apk, 1);
    std::puts("DEXKIT_SYMBOL_ORACLE_V1");
    const auto write = [](char kind, size_t dex, uint32_t id, const auto &bean) {
        flatbuffers::FlatBufferBuilder builder;
        if constexpr (std::is_same_v<std::decay_t<decltype(bean)>, ClassBean>)
            builder.Finish(bean.CreateClassMeta(builder));
        else if constexpr (std::is_same_v<std::decay_t<decltype(bean)>, MethodBean>)
            builder.Finish(bean.CreateMethodMeta(builder));
        else
            builder.Finish(bean.CreateFieldMeta(builder));
        std::printf("%c %zu %u %zu\n", kind, dex, id, builder.GetSize());
        std::fwrite(builder.GetBufferPointer(), 1, builder.GetSize(), stdout);
        std::putchar('\n');
    };
    for (size_t d = 0; d < bridge.dex_items.size(); ++d) {
        auto &item = *bridge.dex_items[d];
        for (uint32_t c = 0; c < item.reader.TypeIds().size(); ++c) write('C', d, c, item.GetClassBean(c));
        for (uint32_t m = 0; m < item.reader.MethodIds().size(); ++m) write('M', d, m, item.GetMethodBean(m));
        for (uint32_t f = 0; f < item.reader.FieldIds().size(); ++f) write('F', d, f, item.GetFieldBean(f));
    }
    if (std::ferror(stdout)) std::abort();
}

void BenchmarkDiagnostics::CheckSymbols(std::string_view apk) {
    const auto require = [](bool condition, const char *message) {
        if (!condition) {
            std::fprintf(stderr, "Symbol check failed: %s\n", message);
            std::abort();
        }
    };
    // The generator deliberately places a duplicate Api definition last.
    DexKit reference(apk, 1), target(apk, 1), cold(apk, 1);
    require(target.GetDexNum() == 3, "expected the three-DEX symbol fixture");
    struct Symbol {
        size_t dex;
        uint32_t id;
        std::string descriptor;
    };
    std::vector<Symbol> methods, fields;
    std::map<std::string_view, std::vector<size_t>> method_names, field_names;
    std::set<std::string> defined_methods, defined_fields;
    uint64_t pairs = 0, different_index_matches = 0;
    auto retained_method = target.dex_items[0]->GetMethodDescriptor(0);
    auto retained_field = target.dex_items[0]->GetFieldDescriptor(0);
    const std::string retained_method_copy(retained_method), retained_field_copy(retained_field);
    const auto retained_method_address = retained_method.data();
    const auto retained_field_address = retained_field.data();

    for (size_t d = 0; d < reference.dex_items.size(); ++d) {
        auto &oracle = *reference.dex_items[d];
        auto &item = *target.dex_items[d];
        for (uint32_t m = 0; m < oracle.reader.MethodIds().size(); ++m) {
            const auto &raw = oracle.reader.MethodIds()[m];
            std::string descriptor(oracle.GetMethodDescriptor(m));
            require(item.GetMethodDescriptor(m) == descriptor, "method descriptor bytes");
            const auto parsed = internal::ParseMethodDescriptorView(descriptor);
            require(parsed.has_value(), "parse a real method descriptor");
#if DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS
            require(item.MatchesDescriptor(m, *parsed), "raw method lookup predicate");
#endif
            const auto looked_up = item.GetMethodBean(raw.class_idx, descriptor);
            const auto &members = item.class_method_ids[raw.class_idx];
            const bool defined = std::find(members.begin(), members.end(), m) != members.end();
            require(looked_up.has_value() == defined, "method lookup domain");
            if (defined) {
                require(looked_up->id == m && looked_up->dex_id == d, "method lookup identity");
                defined_methods.emplace(descriptor);
            }
            method_names[oracle.strings[raw.name_idx]].push_back(methods.size());
            methods.push_back({d, m, std::move(descriptor)});
        }
        for (uint32_t f = 0; f < oracle.reader.FieldIds().size(); ++f) {
            const auto &raw = oracle.reader.FieldIds()[f];
            std::string descriptor(oracle.GetFieldDescriptor(f));
            require(item.GetFieldDescriptor(f) == descriptor, "field descriptor bytes");
            const auto parsed = internal::ParseFieldDescriptorView(descriptor);
            require(parsed.has_value(), "parse a real field descriptor");
#if DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS
            require(item.MatchesDescriptor(f, *parsed), "raw field lookup predicate");
#endif
            const auto looked_up = item.GetFieldBean(raw.class_idx, descriptor);
            const auto &members = item.class_field_ids[raw.class_idx];
            const bool defined = std::find(members.begin(), members.end(), f) != members.end();
            require(looked_up.has_value() == defined, "field lookup domain");
            if (defined) {
                require(looked_up->id == f && looked_up->dex_id == d, "field lookup identity");
                defined_fields.emplace(descriptor);
            }
            field_names[oracle.strings[raw.name_idx]].push_back(fields.size());
            fields.push_back({d, f, std::move(descriptor)});
        }
    }
    require(methods.size() < 10000, "this is a bounded metadata fixture");

    const auto method_pair = [&](const Symbol &a, const Symbol &b) {
        const bool expected = a.descriptor == b.descriptor;
        auto &left = *target.dex_items[a.dex];
        auto &right = *target.dex_items[b.dex];
#if DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS
        const bool actual = left.HasSameMethodIdentity(a.id, right, b.id);
#else
        const bool actual = left.GetMethodDescriptor(a.id) == right.GetMethodDescriptor(b.id);
#endif
        require(actual == expected, "method identity vs full text oracle");
        different_index_matches += expected && a.dex != b.dex && a.id != b.id;
        ++pairs;
    };
    const auto field_pair = [&](const Symbol &a, const Symbol &b) {
        const bool expected = a.descriptor == b.descriptor;
        auto &left = *target.dex_items[a.dex];
        auto &right = *target.dex_items[b.dex];
#if DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS
        const bool actual = left.HasSameFieldIdentity(a.id, right, b.id);
#else
        const bool actual = left.GetFieldDescriptor(a.id) == right.GetFieldDescriptor(b.id);
#endif
        require(actual == expected, "field identity vs full text oracle");
        different_index_matches += expected && a.dex != b.dex && a.id != b.id;
        ++pairs;
    };
    for (const auto &[name, indices] : method_names)
        for (auto a : indices) for (auto b : indices) method_pair(methods[a], methods[b]);
    for (const auto &[name, indices] : field_names)
        for (auto a : indices) for (auto b : indices) field_pair(fields[a], fields[b]);
    for (size_t i = 1; i < methods.size(); ++i) method_pair(methods[i - 1], methods[i]);
    for (size_t i = 1; i < fields.size(); ++i) field_pair(fields[i - 1], fields[i]);
    require(different_index_matches > 0, "same symbol with different local IDs was exercised");

    for (auto descriptor : {"Lfixture/Api;->a()I", "Lfixture/Api;->a()J", "Lfixture/Api;->a([I)I",
            "Lfixture/Api;->a([[I)I", "Lfixture/Api;->a(I[Ljava/lang/String;)V",
            "Lfixture/Api;->a([Ljava/lang/String;I)V", "Lfixture/Api;->\xce\xbb(Lfixture/\xce\xa9;)V"}) {
        auto result = target.GetMethodData(descriptor);
        require(result != nullptr, "public method descriptor lookup");
        auto meta = flatbuffers::GetRoot<schema::MethodMeta>(result->GetBufferPointer());
        require(meta->dex_descriptor()->string_view() == descriptor && meta->dex_id() == 2,
                "duplicate definition representative");
    }
    for (auto descriptor : {"", "Lfixture/Api;", "Lfixture/Api;->a", "Lfixture/Api;->a()",
            "Lfixture/Api;->a()II", "Lfixture/Api;->a([V)I", "Lfixture/Api;->a(I)V",
            "Lfixture/Api;->a([[I)J", "Lfixture/Api;->missing()V", "Lfixture/Empty;->a()I"}) {
        require(target.GetMethodData(descriptor) == nullptr, "missing/malformed method lookup");
    }
    for (auto descriptor : {"Lfixture/Api;->a:I", "Lfixture/Api;->a:J", "Lfixture/Api;->c:[I"}) {
        auto result = target.GetFieldData(descriptor);
        require(result != nullptr, "public field descriptor lookup");
        auto meta = flatbuffers::GetRoot<schema::FieldMeta>(result->GetBufferPointer());
        require(meta->dex_descriptor()->string_view() == descriptor && meta->dex_id() == 2,
                "field duplicate definition representative");
    }
    for (auto descriptor : {"", "Lfixture/Api;", "Lfixture/Api;->a", "Lfixture/Api;->a:",
            "Lfixture/Api;->a:V", "Lfixture/Api;->a:[V", "Lfixture/Api;->a:II", "Lfixture/Api;->missing:I"}) {
        require(target.GetFieldData(descriptor) == nullptr, "missing/malformed field lookup");
    }

    target.InitFullCache();
    auto &client = *target.dex_items[1];
    uint64_t blocked_later_references = 0;
    for (uint32_t m = 0; m < client.reader.MethodIds().size(); ++m) {
        const auto &raw = client.reader.MethodIds()[m];
        if (client.type_names[raw.class_idx] != "Lfixture/Api;") continue;
        const auto name = client.strings[raw.name_idx];
        require(client.method_cross_info[m].has_value() == (name == "a"), "legacy method cross-reference progression");
        if (name == "a") require(client.method_cross_info[m].value().first == 2, "method reference target DEX");
        blocked_later_references += name == "c" || name == "\xce\xbb";
    }
    for (uint32_t f = 0; f < client.reader.FieldIds().size(); ++f) {
        const auto &raw = client.reader.FieldIds()[f];
        if (client.type_names[raw.class_idx] != "Lfixture/Api;") continue;
        const auto name = client.strings[raw.name_idx];
        require(client.field_cross_info[f].has_value() == (name == "a"), "legacy field cross-reference progression");
        blocked_later_references += name == "c" || name == "\xce\xbb";
    }
    require(blocked_later_references == 4, "unresolved reference before valid later references");

    flatbuffers::FlatBufferBuilder method_query, field_query;
    method_query.Finish(schema::CreateFindMethod(method_query));
    field_query.Finish(schema::CreateFindField(field_query));
    auto method_results = target.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(method_query.GetBufferPointer()));
    auto field_results = target.FindField(flatbuffers::GetRoot<schema::FindField>(field_query.GetBufferPointer()));
    std::set<std::string> returned_methods, returned_fields;
    for (auto meta : *flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(method_results->GetBufferPointer())->methods()) {
        require(returned_methods.emplace(meta->dex_descriptor()->str()).second, "method text deduplication");
        if (meta->dex_descriptor()->string_view() == "Lfixture/Api;->a()I")
            require(meta->dex_id() == 0, "first collected duplicate method stays the representative");
    }
    for (auto meta : *flatbuffers::GetRoot<schema::FieldMetaArrayHolder>(field_results->GetBufferPointer())->fields()) {
        require(returned_fields.emplace(meta->dex_descriptor()->str()).second, "field text deduplication");
    }
    require(returned_methods == defined_methods && returned_fields == defined_fields, "full declared member result sets");
    require(retained_method.data() == retained_method_address && retained_method == retained_method_copy,
            "retained method view survives growth and full warm-up");
    require(retained_field.data() == retained_field_address && retained_field == retained_field_copy,
            "retained field view survives growth and full warm-up");

#if !DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS && !DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS && !DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS
    // The legacy cache does not publish concurrent cold writes. Its control
    // tests warm reads; experimental publication is stressed below from cold.
    for (const auto &symbol : methods) cold.dex_items[symbol.dex]->GetMethodDescriptor(symbol.id);
    for (const auto &symbol : fields) cold.dex_items[symbol.dex]->GetFieldDescriptor(symbol.id);
#endif
    std::latch start(1);
    std::array<std::string_view, 8> same_slot;
    std::vector<std::thread> readers;
    for (size_t worker = 0; worker < same_slot.size(); ++worker) readers.emplace_back([&, worker] {
        start.wait();
        same_slot[worker] = cold.dex_items[0]->GetMethodDescriptor(1);
        for (size_t i = worker; i < methods.size(); i += same_slot.size()) {
            const auto &symbol = methods[i];
            require(cold.dex_items[symbol.dex]->GetMethodDescriptor(symbol.id) == symbol.descriptor,
                    "concurrent method descriptor content");
        }
        for (size_t i = worker; i < fields.size(); i += same_slot.size()) {
            const auto &symbol = fields[i];
            require(cold.dex_items[symbol.dex]->GetFieldDescriptor(symbol.id) == symbol.descriptor,
                    "concurrent field descriptor content");
        }
    });
    start.count_down();
    for (auto &reader : readers) reader.join();
    for (auto view : same_slot) require(view == same_slot[0] && view.data() == same_slot[0].data(), "same-slot stable publication");
    std::fprintf(stderr,
        "CHECK_SYMBOLS {\"methods\":%zu,\"fields\":%zu,\"pairs\":%llu,\"different_index_matches\":%llu,\"passed\":true}\n",
        methods.size(), fields.size(), (unsigned long long) pairs, (unsigned long long) different_index_matches);
}
} // namespace dexkit
#endif

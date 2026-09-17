#include "dexkit.h"
#include "candidate_pipeline.h"
#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include "benchmark_diagnostics.h"
#endif
#include <array>
#include <cstdio>
#include <latch>
#include <thread>
#include <type_traits>

namespace dexkit {
struct UncachedDescriptorBenchmark {
    static_assert(std::is_same_v<MemberDescriptor, std::string>);
    static_assert(std::is_same_v<decltype(MethodBean{}.dex_descriptor), std::string>);
    static_assert(std::is_same_v<decltype(FieldBean{}.dex_descriptor), std::string>);

    static void Require(bool ok, const char *message) {
        if (!ok) { std::fprintf(stderr, "Uncached descriptor check failed: %s\n", message); std::abort(); }
    }
    template<class Bean>
    static std::vector<uint8_t> Serialize(const Bean &bean) {
        flatbuffers::FlatBufferBuilder buffer;
        if constexpr (std::is_same_v<Bean, MethodBean>) buffer.Finish(bean.CreateMethodMeta(buffer));
        else buffer.Finish(bean.CreateFieldMeta(buffer));
        return {buffer.GetBufferPointer(), buffer.GetBufferPointer() + buffer.GetSize()};
    }
    static void Check(std::string_view apk, bool long_strings) {
        std::vector<MethodBean> methods;
        std::vector<FieldBean> fields;
        std::vector<std::vector<uint8_t>> method_bytes, field_bytes;
        std::unique_ptr<flatbuffers::FlatBufferBuilder> retained_public;
        std::vector<uint8_t> public_bytes;
        AnnotationEncodeValueBean nested_method;
        AnnotationEncodeValueBean nested_field;
        UsingFieldBean using_field;
        std::array<MethodBean, 8> worker_methods;
        std::array<FieldBean, 8> worker_fields;
        {
            DexKit bridge(apk, 1);
            auto *item = bridge.GetDexItem(0);
            Require(item && !item->reader.MethodIds().empty() && !item->reader.FieldIds().empty(), "fixture members");
            uint32_t method_id = 0, field_id = 0;
            if (long_strings) {
                bool found = false;
                for (uint32_t i = 0; i < item->reader.MethodIds().size(); ++i) {
                    if (item->GetMethodBean(i).dex_descriptor.size() > 512) { method_id = i; found = true; break; }
                }
                Require(found, "heap descriptor fixture");
                found = false;
                for (uint32_t i = 0; i < item->reader.FieldIds().size(); ++i) {
                    if (item->GetFieldBean(i).dex_descriptor.size() > sizeof(std::string)) { field_id = i; found = true; break; }
                }
                Require(found, "heap field descriptor fixture");
            } else {
                Require(item->GetMethodBean(0).dex_descriptor.size() < 16, "SSO method fixture");
                Require(item->GetFieldBean(0).dex_descriptor.size() < 16, "SSO field fixture");
            }
            // Copy, move, assign and force vector relocation while the source
            // value is independently destroyed or overwritten.
            for (size_t i = 0; i < 129; ++i) {
                auto method = item->GetMethodBean(method_id);
                auto field = item->GetFieldBean(field_id);
                auto method_copy = method;
                auto field_copy = field;
                method_bytes.push_back(Serialize(method)); field_bytes.push_back(Serialize(field));
                methods.push_back(std::move(method)); fields.push_back(std::move(field));
                method_copy.dex_descriptor.assign(8192, 'x'); field_copy.dex_descriptor.assign(8192, 'y');
                Require(Serialize(methods.back()) == method_bytes.back(), "method copy independence");
                Require(Serialize(fields.back()) == field_bytes.back(), "field copy independence");
            }
            nested_method.type = schema::AnnotationEncodeValueType::MethodValue;
            nested_method.value = std::make_unique<MethodBean>(item->GetMethodBean(method_id));
            nested_field.type = schema::AnnotationEncodeValueType::EnumValue;
            nested_field.value = std::make_unique<FieldBean>(item->GetFieldBean(field_id));
            using_field.field = item->GetFieldBean(field_id); using_field.is_getting = true;

            std::latch start(1);
            std::vector<std::thread> readers;
            for (size_t worker = 0; worker < worker_methods.size(); ++worker) readers.emplace_back([&, worker] {
                start.wait();
                worker_methods[worker] = item->GetMethodBean(method_id);
                worker_fields[worker] = item->GetFieldBean(field_id);
                for (size_t i = 0; i < 64; ++i) {
                    Require(Serialize(item->GetMethodBean(method_id)) == method_bytes[0], "concurrent method");
                    Require(Serialize(item->GetFieldBean(field_id)) == field_bytes[0], "concurrent field");
                }
            });
            start.count_down();
            for (auto &reader : readers) reader.join();

            for (uint32_t workers : {1u, 4u}) {
                bridge.SetThreadNum(workers);
                QueryContext context(QueryKind::FindMethod);
                auto executor = bridge.CreateQueryExecutor(context);
                auto ordinary = SubmitQueryTask(*executor, [&] { return item->GetMethodBean(method_id); });
                auto candidate = internal::SubmitCandidateTask(*executor, [&] { return item->GetFieldBean(field_id); });
                executor->OnSubmissionComplete();
                methods.push_back(ordinary.get()); method_bytes.push_back(method_bytes[0]);
                fields.push_back(candidate.get()); field_bytes.push_back(field_bytes[0]);
            }
            retained_public = bridge.GetMethodByIds({method_id});
            public_bytes.assign(retained_public->GetBufferPointer(), retained_public->GetBufferPointer() + retained_public->GetSize());
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            for (size_t reason = 0; reason < 3; ++reason) {
                const auto &stats = item->descriptor_diagnostics;
                Require(stats.method_calls[reason].load() == stats.method_builds[reason].load(), "no method cache hits");
                Require(stats.field_calls[reason].load() == stats.field_builds[reason].load(), "no field cache hits");
            }
            BenchmarkDiagnostics::Dump(bridge, "uncached-owning-before-close");
#endif
        }
        // Both the DEX mapping and every worker are gone before these reads.
        for (size_t i = 0; i < methods.size(); ++i)
            Require(Serialize(methods[i]) == method_bytes[i], "method ownership after close and relocation");
        for (size_t i = 0; i < fields.size(); ++i)
            Require(Serialize(fields[i]) == field_bytes[i], "field ownership after close and relocation");
        for (size_t i = 0; i < worker_methods.size(); ++i) {
            Require(Serialize(worker_methods[i]) == method_bytes[0], "worker method survives close");
            Require(Serialize(worker_fields[i]) == field_bytes[0], "worker field survives close");
        }
        Require(Serialize(*std::get<std::unique_ptr<MethodBean>>(nested_method.value)) == method_bytes[0], "nested method ownership");
        Require(Serialize(*std::get<std::unique_ptr<FieldBean>>(nested_field.value)) == field_bytes[0], "nested field ownership");
        Require(Serialize(using_field.field) == field_bytes[0], "using-field ownership");
        flatbuffers::FlatBufferBuilder nested;
        nested.Finish(nested_method.CreateAnnotationEncodeValueMeta(nested));
        nested.Clear(); nested.Finish(nested_field.CreateAnnotationEncodeValueMeta(nested));
        nested.Clear(); nested.Finish(using_field.CreateUsingFieldMeta(nested));
        Require(std::equal(public_bytes.begin(), public_bytes.end(), retained_public->GetBufferPointer()), "public result owns bytes");
        std::printf("CHECK_UNCACHED_OWNERSHIP {\"long_strings\":%s,\"passed\":true}\n", long_strings ? "true" : "false");
    }
};
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    dexkit::UncachedDescriptorBenchmark::Check(argv[1], false);
    dexkit::UncachedDescriptorBenchmark::Check(argv[2], true);
}

// Keep the measured nested builder and consumption code identical.
#define main OriginalStringWorkloadMain
#include "string_workload.cpp"
#undef main

#ifndef STRING_SHAPE
#error STRING_SHAPE must select nested (0), flattened AND (1), or root-only (2)
#endif

namespace {
std::unique_ptr<Builder> ShapeQuery(bool absent) {
    if constexpr (STRING_SHAPE == 0) return NestedBroadQuery(absent);
    auto b = std::make_unique<Builder>();
    std::vector<Atom> atoms{{absent ? "AbsentEveryPool" : "Needle", schema::StringMatchType::Contains}};
    if constexpr (STRING_SHAPE == 1) {
        // All atoms use Contains, ignoreCase=false. The duplicate Needle is
        // harmless; preserving it also makes the negative query equivalent.
        atoms.push_back({"LongPrefix/", schema::StringMatchType::Contains});
        atoms.push_back({"Needle", schema::StringMatchType::Contains});
    }
    auto strings = Strings(*b, atoms);
    schema::MethodMatcherBuilder root(*b); root.add_using_strings(strings);
    b->Finish(schema::CreateFindMethod(*b, 0, 0, false, 0, 0, false, root.Finish()));
    return b;
}

Statistics MeasureShape(const char *apk, size_t repeats) {
    Statistics s;
    auto begin = Clock::now(); auto bridge = std::make_unique<DexKit>(apk, 1); s.create_ns = Ns(begin);
    begin = Clock::now(); bridge->SetThreadNum(4);
    auto positive = ShapeQuery(false), negative = ShapeQuery(true); s.setup_ns = Ns(begin);
    for (size_t repeat = 0; repeat < repeats; ++repeat) {
        begin = Clock::now();
        for (auto query : {positive.get(), negative.get()}) {
            const auto leg = Clock::now();
            Consume(s, bridge->FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer())), false);
            (query == positive.get() ? s.positive_ns : s.negative_ns) += Ns(leg);
        }
        (repeat ? s.repeated_ns : s.first_ns) += Ns(begin);
    }
    begin = Clock::now(); bridge.reset(); s.close_ns = Ns(begin);
    return s;
}
}

int main(int argc, char **argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--dump") {
        DexKit bridge(argv[2], 1); bridge.SetThreadNum(4);
        for (bool absent : {false, true}) {
            auto query = ShapeQuery(absent);
            auto result = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
            const auto *methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods();
            std::fprintf(stderr, "SHAPE_RESULTS {\"absent\":%s,\"values\":[", absent ? "true" : "false");
            bool first = true;
            for (const auto *method : *methods) {
                const auto name = method->dex_descriptor()->string_view();
                std::fprintf(stderr, "%s[%u,%u,\"%.*s\"]", first ? "" : ",", method->dex_id(),
                        uint32_t(method->id()), int(name.size()), name.data());
                first = false;
            }
            std::fprintf(stderr, "]}\n");
            const uint32_t size = result->GetSize();
            Require(std::fwrite(&size, sizeof(size), 1, stdout) == 1);
            Require(std::fwrite(result->GetBufferPointer(), 1, size, stdout) == size);
        }
        return 0;
    }
    if (argc != 4) return 2;
    Require(std::string_view(argv[2]) == "string-nested-broad");
    char *end = nullptr; const auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && !*end && repeats > 0 && repeats <= 100000);
    auto begin = Clock::now(); auto s = MeasureShape(argv[1], repeats); const auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"shape\":%d,\"repeats\":%llu,\"create_ns\":%lld,\"setup_ns\":%lld,"
        "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
        "\"positive_ns\":%lld,\"negative_ns\":%lld,\"checksum\":%llu,\"returned\":%llu}\n", argv[2], STRING_SHAPE, repeats,
        (long long)s.create_ns, (long long)s.setup_ns, (long long)s.first_ns, (long long)s.repeated_ns,
        (long long)s.close_ns, (long long)lifecycle, (long long)s.positive_ns, (long long)s.negative_ns,
        (unsigned long long)s.checksum, (unsigned long long)s.returned);
}

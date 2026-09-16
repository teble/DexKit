#define main OriginalCandidateStringWorkloadMain
#include "string_workload.cpp"
#undef main
#include "string_admission_queries.h"

namespace {
std::unique_ptr<Builder> CandidateQuery(std::string_view mode, bool absent) {
    if (mode == "candidate-method-equal-one") return string_admission_fixture::Query(7, absent);
    if (mode == "candidate-method-prefix-one") return string_admission_fixture::Query(8, absent);
    if (mode == "candidate-class-one") return string_admission_fixture::ClassQuery(4, absent);
    if (mode == "candidate-method-nested") return NestedBroadQuery(absent);
    auto data = std::make_unique<Builder>();
    const bool empty = mode == "candidate-method-empty";
    auto words = Strings(*data, empty ? std::vector<Atom>{{absent ? "AbsentEveryPool2" : "AbsentEveryPool"}}
            : std::vector<Atom>{{absent ? "AbsentEveryPool" : "Needle", schema::StringMatchType::Contains},
                               {"LongPrefix/", schema::StringMatchType::Contains}});
    if (mode.starts_with("candidate-class-")) {
        schema::ClassMatcherBuilder matcher(*data); matcher.add_using_strings(words);
        data->Finish(schema::CreateFindClass(*data, 0, 0, false, 0, false, matcher.Finish()));
    } else {
        schema::MethodMatcherBuilder matcher(*data); matcher.add_using_strings(words);
        data->Finish(schema::CreateFindMethod(*data, 0, 0, false, 0, 0, false, matcher.Finish()));
    }
    return data;
}

Statistics MeasureCandidates(const char *apk, std::string_view mode, unsigned workers, bool warm, size_t repeats) {
    Statistics s;
    auto begin = Clock::now(); auto bridge = std::make_unique<DexKit>(apk, 1); s.create_ns = Ns(begin);
    begin = Clock::now(); bridge->SetThreadNum(workers);
    const bool classes = mode.starts_with("candidate-class-");
    auto positive = CandidateQuery(mode, false), negative = CandidateQuery(mode, true);
    if (warm) {
        auto query = string_admission_fixture::WarmupQuery();
        auto result = bridge->FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
        Require(flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods()->size() == 0);
    }
    s.setup_ns = Ns(begin);
    for (size_t repeat = 0; repeat < repeats; ++repeat) {
        begin = Clock::now();
        for (auto *query : {positive.get(), negative.get()}) {
            auto leg = Clock::now();
            Consume(s, classes ? bridge->FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()))
                               : bridge->FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer())), classes);
            (query == positive.get() ? s.positive_ns : s.negative_ns) += Ns(leg);
        }
        (repeat ? s.repeated_ns : s.first_ns) += Ns(begin);
    }
    begin = Clock::now(); bridge.reset(); s.close_ns = Ns(begin);
    return s;
}
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    std::string_view mode(argv[2]);
    const bool warm = mode.ends_with("-warm");
    if (warm) mode.remove_suffix(5);
    Require(mode.ends_with("-w1") || mode.ends_with("-w4"));
    const unsigned workers = mode.ends_with("-w1") ? 1 : 4;
    mode.remove_suffix(3);
    Require(mode == "candidate-method-multiple" || mode == "candidate-class-multiple"
            || mode == "candidate-method-equal-one" || mode == "candidate-method-prefix-one"
            || mode == "candidate-class-one" || mode == "candidate-method-empty"
            || mode == "candidate-method-nested");
    char *end = nullptr; const auto repeats = std::strtoull(argv[3], &end, 10);
    Require(end && !*end && repeats > 0 && repeats <= 100000);
    auto begin = Clock::now(); const auto s = MeasureCandidates(argv[1], mode, workers, warm, repeats);
    const auto lifecycle = Ns(begin);
    std::printf("WORKLOAD {\"mode\":\"%s\",\"repeats\":%llu,\"threads\":%u,\"create_ns\":%lld,\"setup_ns\":%lld,"
            "\"first_ns\":%lld,\"repeated_ns\":%lld,\"close_ns\":%lld,\"lifecycle_ns\":%lld,"
            "\"positive_ns\":%lld,\"negative_ns\":%lld,\"checksum\":%llu,\"returned\":%llu}\n", argv[2], repeats, workers,
            (long long)s.create_ns, (long long)s.setup_ns, (long long)s.first_ns, (long long)s.repeated_ns,
            (long long)s.close_ns, (long long)lifecycle, (long long)s.positive_ns, (long long)s.negative_ns,
            (unsigned long long)s.checksum, (unsigned long long)s.returned);
    return 0;
}

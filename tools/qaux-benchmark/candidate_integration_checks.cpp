#include "string_queries.h"
#include "benchmark_diagnostics.h"
#include "dex_item.h"
#if DEXKIT_EXPERIMENT_CANDIDATE_PIPELINE
#include "candidate_pipeline.h"
#endif

namespace {
using namespace string_fixture;
void Require(bool value) { if (!value) std::abort(); }
std::string Bytes(const Builder &value) {
    return {reinterpret_cast<const char *>(value.GetBufferPointer()), value.GetSize()};
}

struct QueryData {
    Builder data;
    flatbuffers::Offset<schema::MethodMatcher> pure_method;
    flatbuffers::Offset<schema::ClassMatcher> pure_class;
    QueryData(bool classes, bool nested) {
        const auto words = Strings(data, {{"Needle", schema::StringMatchType::Contains}});
        if (classes) {
            schema::ClassMatcherBuilder leaf(data); leaf.add_using_strings(words);
            pure_class = leaf.Finish();
            const auto children = data.CreateVector(std::vector{pure_class});
            const auto interfaces = schema::CreateInterfacesMatcher(data, children);
            schema::ClassMatcherBuilder root(data);
            root.add_using_strings(words);
            if (nested) root.add_interfaces(interfaces);
            data.Finish(schema::CreateFindClass(data, 0, 0, false, 0, false, root.Finish()));
        } else {
            schema::MethodMatcherBuilder leaf(data); leaf.add_using_strings(words);
            pure_method = leaf.Finish();
            const auto children = data.CreateVector(std::vector{pure_method});
            const auto invoking = schema::CreateMethodsMatcher(data, children);
            schema::MethodMatcherBuilder root(data);
            root.add_using_strings(words);
            if (nested) root.add_invoking_methods(invoking);
            data.Finish(schema::CreateFindMethod(data, 0, 0, false, 0, 0, false, root.Finish()));
        }
    }
};

std::string Serialize(std::vector<dexkit::MethodBean> beans) {
    Builder data;
    std::set<std::string_view> descriptors;
    std::vector<flatbuffers::Offset<schema::MethodMeta>> values;
    for (const auto &bean : beans) {
        if (!descriptors.insert(bean.dex_descriptor).second) continue;
        auto value = bean.CreateMethodMeta(data); data.Finish(value); values.push_back(value);
    }
    data.Finish(schema::CreateMethodMetaArrayHolder(data, data.CreateVector(values)));
    return Bytes(data);
}

std::string Serialize(std::vector<dexkit::ClassBean> beans) {
    Builder data;
    std::vector<flatbuffers::Offset<schema::ClassMeta>> values;
    for (const auto &bean : beans) {
        auto value = bean.CreateClassMeta(data); data.Finish(value); values.push_back(value);
    }
    data.Finish(schema::CreateClassMetaArrayHolder(data, data.CreateVector(values)));
    return Bytes(data);
}
}

void dexkit::BenchmarkDiagnostics::CheckCandidatePipeline(std::string_view apk) {
    size_t injected = 0;
    std::vector<std::string> expected;
    for (unsigned workers : {1u, 4u}) {
        DexKit bridge(apk, 1); bridge.SetThreadNum(workers); bridge.InitFullCache();
        Require(bridge.dex_items.size() == 1);
        auto &dex = *bridge.dex_items.front();
        Require(dex.reader.MethodIds().size() == 4500 && dex.reader.ClassDefs().size() == 1500);
        Require(dex.reader.ClassDefs()[0].class_idx == 1499 && dex.reader.ClassDefs()[1499].class_idx == 0);
        Require(dex.GetMethodBean(3000).dex_descriptor == "Lcandidate/C1000;->m0()V");
        size_t case_index = 0;
        for (bool classes : {false, true}) for (bool nested : {false, true}) {
            QueryData data(classes, nested);
            const auto *method = classes ? nullptr : flatbuffers::GetRoot<schema::FindMethod>(data.data.GetBufferPointer());
            const auto *type = classes ? flatbuffers::GetRoot<schema::FindClass>(data.data.GetBufferPointer()) : nullptr;
            auto public_result = classes ? bridge.FindClass(type) : bridge.FindMethod(method);
            const auto count = classes ? flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(public_result->GetBufferPointer())->classes()->size()
                    : flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(public_result->GetBufferPointer())->methods()->size();
            Require(count == (nested ? 1u : classes ? 1499u : 4497u));
            if (workers == 1) expected.push_back(Bytes(*public_result));
            else Require(Bytes(*public_result) == expected[case_index]);

#if DEXKIT_EXPERIMENT_CANDIDATE_PIPELINE
            // A known necessary root is injected for the nested test only.
            // The production selector still rejects its extra relation.
            for (bool split : {false, true}) for (bool reject : {false, true}) {
                QueryContext context(classes ? QueryKind::FindClass : QueryKind::FindMethod);
                const auto analysis = classes ? Analyze(type->matcher(), 1) : Analyze(method->matcher(), 1);
                auto admission = bridge.EnterQueryExecution(analysis.need_flags);
                const std::set<uint32_t> no_ids;
                trie::PackageTrie packages;
                auto source = classes ? dex.SelectCandidates(flatbuffers::GetTemporaryPointer(data.data, data.pure_class),
                        context, 500, split) : dex.SelectCandidates(flatbuffers::GetTemporaryPointer(data.data, data.pure_method),
                        context, 1000, split);
                Require(source.strings.route == inverted_string::QueryPlan::Route::Keywords);
                if (nested) Require((classes ? dex.PlanRootStringCandidates(type->matcher())
                        : dex.PlanRootStringCandidates(method->matcher())).route == inverted_string::QueryPlan::Route::Legacy);
                const std::vector<internal::CandidateSource> sources{source};
                internal::CandidateExecutionOptions options;
                options.working_bytes = reject ? 1 : 1024 * 1024;
                auto prepare = [](const internal::CandidateSource &selected, internal::CandidateBudget::Lease reservation) {
                    return selected.item->PrepareCandidates(selected, std::move(reservation));
                };
                std::atomic<size_t> matched_slices = 0;
                auto check_slice = [&](internal::CandidateSlice slice) {
                    ++matched_slices;
                    Require(slice.begin % source.slice_width == 0);
                    Require(slice.end == (split ? std::min(source.domain.count, slice.begin + source.slice_width)
                            : source.domain.count));
                };
                std::string actual;
                if (classes) {
                    auto legacy = [&](const auto &selected, IQueryExecutor &executor, bool fallback) {
                        Require(fallback);
                        const inverted_string::QueryPlan plan;
                        return selected.item->FindClass(type, no_ids, packages, executor, 500, context, &plan);
                    };
                    auto match = [&](const auto &selected, const internal::PreparedCandidates &prepared, internal::CandidateSlice slice) {
                        check_slice(slice);
                        Require(prepared.RootTruth() && prepared.RootTruth()->Has(1000) && !prepared.RootTruth()->Has(1001));
                        return selected.item->FindClass(type, no_ids, packages, slice.begin, slice.end, context, {}, &prepared);
                    };
                    actual = Serialize(internal::RunCandidatePipeline<ClassBean>(sources, bridge.CreateQueryExecutor(context),
                            context, legacy, prepare, match, options));
                } else {
                    auto legacy = [&](const auto &selected, IQueryExecutor &executor, bool fallback) {
                        Require(fallback);
                        const inverted_string::QueryPlan plan;
                        return selected.item->FindMethod(method, no_ids, no_ids, packages, executor, 1000, context, &plan);
                    };
                    auto match = [&](const auto &selected, const internal::PreparedCandidates &prepared, internal::CandidateSlice slice) {
                        check_slice(slice);
                        Require(prepared.RootTruth() && prepared.RootTruth()->Has(3000) && !prepared.RootTruth()->Has(3003));
                        return selected.item->FindMethod(method, no_ids, no_ids, packages, slice.begin, slice.end, context, {}, &prepared);
                    };
                    actual = Serialize(internal::RunCandidatePipeline<MethodBean>(sources, bridge.CreateQueryExecutor(context),
                            context, legacy, prepare, match, options));
                    // A proof from another query must not be used even if its
                    // DEX, local ID, and matcher-vector addresses coincide.
                    QueryContext other(QueryKind::FindMethod);
                    auto binding = other.BindToCurrentThread();
                    inverted_string::Bits wrong(dex.reader.MethodIds().size());
                    inverted_string::MatchScope wrong_scope(&dex, method->matcher()->using_strings(), false, &wrong,
                            context.GetQueryId());
                    Require(dex.IsMethodUsingStringsMatched(3000, method->matcher()));
                }
                Require(actual == expected[case_index]);
                Require(matched_slices == (reject ? 0u : !split ? 1u : classes ? 3u : 5u));
                ++injected;
            }
#endif
            ++case_index;
        }
    }
    for (const auto &bytes : expected) {
        const auto size = static_cast<uint32_t>(bytes.size());
        Require(std::fwrite(&size, sizeof(size), 1, stdout) == 1);
        Require(std::fwrite(bytes.data(), 1, bytes.size(), stdout) == bytes.size());
    }
    std::fprintf(stderr, "CHECK_CANDIDATE_INTEGRATION {\"public_cases\":8,\"injected\":%zu,\"cross_slice\":true,\"class_order\":true,\"passed\":true}\n", injected);
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    dexkit::BenchmarkDiagnostics::CheckCandidatePipeline(argv[1]);
}

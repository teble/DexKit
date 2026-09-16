#include "candidate_pipeline.h"

#include <cstdlib>
#include <iostream>

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
#error This check must compile without C++ exceptions.
#endif

using namespace dexkit;
using namespace dexkit::internal;

class InlineExecutor final : public IQueryExecutor {
public:
    void Submit(std::function<void()> task) override { task(); }
    void OnSubmissionComplete() override {}
    bool ShouldSkipTask() const override { return false; }
    std::function<bool()> GetShouldSkipTaskFn() const override { return {}; }
};

int main() {
    QueryContext context(QueryKind::FindMethod);
    CandidateSource source;
    source.domain = {nullptr, context.GetQueryId(), CandidateEntity::Method, 2001};
    source.strings.route = inverted_string::QueryPlan::Route::Keywords;
    source.work_bytes = 4096;
    source.split = true;
    source.slice_width = 1000;
    auto legacy = [](const auto &, auto &, bool) {
        std::abort();
        return std::vector<std::future<std::vector<uint32_t>>>{};
    };
    auto prepare = [&](const auto &selected, CandidateBudget::Lease lease) {
        if (QueryContext::Current() != &context) std::abort();
        auto value = std::make_shared<PreparedCandidates>();
        value->domain = selected.domain;
        value->reservation = std::move(lease);
        value->kind = CandidateView::Kind::Bitmap;
        value->bits = inverted_string::Bits(selected.domain.count);
        value->bits.Set(1); value->bits.Set(2000);
        value->MakeSlices(selected.slice_width);
        return std::shared_ptr<const PreparedCandidates>(value);
    };
    auto match = [&](const auto &, const PreparedCandidates &value, CandidateSlice slice) {
        if (QueryContext::Current() != &context) std::abort();
        std::vector<uint32_t> out;
        value.View().Each(slice, [&](auto id) { out.push_back(id); });
        return out;
    };
    const auto result = RunCandidatePipeline<uint32_t>({source}, std::make_unique<InlineExecutor>(),
            context, legacy, prepare, match);
    if (result != std::vector<uint32_t>{1, 2000}) std::abort();
    std::cout << "CANDIDATE_NOEXCEPTIONS_OK preparation slices results cleanup\n";
}

#include "dex_item.h"

#if DEXKIT_EXPERIMENT_CANDIDATE_PIPELINE
#include <limits>

namespace dexkit {

internal::CandidateSource DexItem::SelectCandidates(const schema::MethodMatcher *matcher, QueryContext &context,
        uint32_t width, bool split) {
    const auto *strings = matcher ? matcher->using_strings() : nullptr;
    return MakeCandidateSource(PlanRootStringCandidates(matcher), strings, strings ? strings->size() : 0,
            false, context, width, split);
}

internal::CandidateSource DexItem::SelectCandidates(const schema::ClassMatcher *matcher, QueryContext &context,
        uint32_t width, bool split) {
    const auto *strings = matcher ? matcher->using_strings() : nullptr;
    return MakeCandidateSource(PlanRootStringCandidates(matcher), strings, strings ? strings->size() : 0,
            true, context, width, split);
}

internal::CandidateSource DexItem::MakeCandidateSource(inverted_string::QueryPlan plan, const void *matchers,
        size_t keywords, bool classes, QueryContext &context, uint32_t width, bool split) {
    using namespace internal;
    CandidateSource source;
    source.item = this;
    source.domain = {this, context.GetQueryId(), classes ? CandidateEntity::Class : CandidateEntity::Method,
            static_cast<uint32_t>(classes ? reader.ClassDefs().size() : reader.MethodIds().size()), dex_id};
    source.strings = plan;
    source.root_matchers = matchers;
    source.slice_width = width;
    source.split = split;
    if (!plan.Admitted() || plan.route == inverted_string::QueryPlan::Route::Empty) return source;

    const auto entities = classes ? type_names.size() : reader.MethodIds().size();
    const auto bitmap = plan.route == inverted_string::QueryPlan::Route::Range
            ? std::optional<size_t>(inverted_string::WordCount(entities) * sizeof(uint64_t))
            : inverted_string::BitmapPlanBytes(entities, type_names.size(), keywords, 1);
    size_t bytes = sizeof(PreparedCandidates);
    auto add = [&](size_t count, size_t element) {
        if (count > (std::numeric_limits<size_t>::max() - bytes) / element) return false;
        bytes += count * element;
        return true;
    };
    // Include reverse-index construction's temporary counts/seen arrays when
    // the frozen decision observed a cold index. Persistent index arrays and
    // existing matcher/trie caches are bridge/query facilities, not candidates.
    if (!bitmap || !add(*bitmap, 1)
            || (!plan.index_ready && !add(strings.size(), sizeof(uint32_t) * 2))
            || (classes && !add(reader.ClassDefs().size(), sizeof(uint32_t)))
            || (split && !add((uint64_t(source.domain.count) + width - 1) / width,
                    sizeof(CandidateSlice)))) {
        source.work_bytes = std::numeric_limits<size_t>::max();
    } else {
        source.work_bytes = bytes;
    }
    return source;
}

std::shared_ptr<const internal::PreparedCandidates> DexItem::PrepareCandidates(
        const internal::CandidateSource &source, internal::CandidateBudget::Lease reservation) {
    using namespace internal;
    DEXKIT_CHECK(source.item == this);
    DEXKIT_CHECK(QueryContext::Current() && QueryContext::Current()->GetQueryId() == source.domain.query_id);
    auto prepared = std::make_shared<PreparedCandidates>();
    prepared->domain = source.domain;
    prepared->reservation = std::move(reservation);
    const bool classes = source.domain.entity == CandidateEntity::Class;
    const auto *matchers = static_cast<const StringMatcherVector *>(source.root_matchers);
    if (!BuildRootStringCandidates(matchers, classes, source.strings, prepared->bits)) {
        // A failed producer is a range fallback, never proof of an empty set.
        prepared->bits = {};
        prepared->reservation.Reset();
        return prepared;
    }
    prepared->root_matchers = matchers;
    if (classes) {
        prepared->kind = CandidateView::Kind::Ids;
        size_t count = 0;
        for (auto word : prepared->bits.words) count += std::popcount(word);
        prepared->ids.reserve(std::min(count, reader.ClassDefs().size()));
        prepared->bits.Each([&](uint32_t type) {
            if (type_def_flag[type]) prepared->ids.push_back(type_def_idx[type]);
        });
        std::sort(prepared->ids.begin(), prepared->ids.end());
    } else {
        prepared->kind = CandidateView::Kind::Bitmap;
    }
    if (source.split && !(source.strings.index_ready && source.strings.route == inverted_string::QueryPlan::Route::Range
            && source.strings.postings <= 1)) {
        prepared->MakeSlices(source.slice_width);
    }
    DEXKIT_CHECK(prepared->RetainedBytes() <= prepared->reservation.Bytes());
    prepared->reservation.Shrink(prepared->RetainedBytes());
    return prepared;
}

} // namespace dexkit
#endif

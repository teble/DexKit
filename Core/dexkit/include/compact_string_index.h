#pragma once

#include "index_types.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <vector>

namespace dexkit {

// Experimental immutable-after-publication index. Method traversal need not be
// in ID order. IDs are appended directly while decoding, including duplicates.
template<class Id>
class CompactIdIndex {
public:
    using value_type = Id;
    void resize(size_t methods) {
        offsets_.resize(methods);
        lengths_.resize(methods);
    }

    bool empty() const { return offsets_.empty(); }

    std::vector<Id> *BeginMethod(uint32_t method) {
        offsets_[method] = CheckedIndexCast<CacheOffset>(ids_.size());
        return &ids_;
    }

    void EndMethod(uint32_t method) {
        // Validate both the row length and the total storage domain.
        // Both const-string and invoke instructions use multiple code units.
        const auto begin = offsets_[method];
        if (begin > ids_.size() || ids_.size() - begin > std::numeric_limits<uint32_t>::max()) std::abort();
        CheckedIndexCast<CacheOffset>(ids_.size());
        lengths_[method] = static_cast<uint32_t>(ids_.size() - begin);
    }

    std::span<const Id> operator[](size_t method) const {
        const auto begin = offsets_[method];
        const auto length = lengths_[method];
        if (begin > ids_.size() || length > ids_.size() - begin) std::abort();
        return std::span<const Id>(ids_).subspan(begin, length);
    }

#if DEXKIT_BENCHMARK_DIAGNOSTICS
    void ObserveAppend() {
        if (ids_.capacity() != observed_capacity_) {
            ++growth_count_;
            moved_bytes_ += observed_capacity_ * sizeof(Id);
            overlap_bytes_ = std::max(overlap_bytes_, (observed_capacity_ + ids_.capacity()) * sizeof(Id));
            observed_capacity_ = ids_.capacity();
        }
    }
#endif

private:
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    friend struct BenchmarkDiagnostics;
    size_t growth_count_ = 0, moved_bytes_ = 0, overlap_bytes_ = 0, observed_capacity_ = 0;
#endif
    std::vector<CacheOffset> offsets_;
    std::vector<uint32_t> lengths_;
    std::vector<Id> ids_;
};

// Invocation rows share the same checked append/freeze representation.
using CompactStringIndex = CompactIdIndex<uint32_t>;
using CompactInvocationIndex = CompactIdIndex<InvokeOperandId>;

} // namespace dexkit

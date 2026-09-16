#pragma once

#include "field_use.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace dexkit {

// Experimental forward rows, frozen by the existing cache publication barrier.
// Append directly in method traversal order; preserve every field use.
class CompactFieldIndex {
public:
    using Use = FieldUse;

    void resize(size_t methods) {
        offsets_.resize(methods);
        lengths_.resize(methods);
    }

    bool empty() const { return offsets_.empty(); }

    std::vector<Use> *BeginMethod(uint32_t method) {
        offsets_[method] = uses_.size();
        return &uses_;
    }

    void EndMethod(uint32_t method) {
        const auto begin = offsets_[method];
        if (begin > uses_.size() || uses_.size() - begin > std::numeric_limits<uint32_t>::max()) std::abort();
        lengths_[method] = static_cast<uint32_t>(uses_.size() - begin);
    }

    std::span<const Use> operator[](size_t method) const {
        const auto begin = offsets_[method];
        const auto length = lengths_[method];
        if (begin > uses_.size() || length > uses_.size() - begin) std::abort();
        if (length == 0) return {};
        return std::span<const Use>(uses_).subspan(begin, length);
    }

#if DEXKIT_BENCHMARK_DIAGNOSTICS
    void ObserveAppend() {
        if (uses_.capacity() != observed_capacity_) {
            ++growth_count_;
            moved_bytes_ += observed_capacity_ * sizeof(Use);
            overlap_bytes_ = std::max(overlap_bytes_, (observed_capacity_ + uses_.capacity()) * sizeof(Use));
            observed_capacity_ = uses_.capacity();
        }
    }
#endif

private:
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    friend struct BenchmarkDiagnostics;
    size_t growth_count_ = 0, moved_bytes_ = 0, overlap_bytes_ = 0, observed_capacity_ = 0;
#endif
    std::vector<size_t> offsets_;
    std::vector<uint32_t> lengths_;
    std::vector<Use> uses_;
};

} // namespace dexkit

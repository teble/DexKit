#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace dexkit {

// Experimental immutable-after-publication index. Method traversal need not be
// in ID order. IDs are appended directly while decoding, including duplicates.
class CompactStringIndex {
public:
    void resize(size_t methods) {
        offsets_.resize(methods);
        lengths_.resize(methods);
    }

    std::vector<uint32_t> *BeginMethod(uint32_t method) {
        offsets_[method] = ids_.size();
        return &ids_;
    }

    void EndMethod(uint32_t method) {
        // One method has at most UINT32_MAX code units; each const-string uses
        // at least two. Only the per-method length is narrowed, never the offset.
        lengths_[method] = static_cast<uint32_t>(ids_.size() - offsets_[method]);
    }

    std::span<const uint32_t> operator[](size_t method) const {
        return std::span<const uint32_t>(ids_).subspan(offsets_[method], lengths_[method]);
    }

private:
#if DEXKIT_BENCHMARK_DIAGNOSTICS
    friend struct BenchmarkDiagnostics;
#endif
    std::vector<size_t> offsets_;
    std::vector<uint32_t> lengths_;
    std::vector<uint32_t> ids_;
};

} // namespace dexkit

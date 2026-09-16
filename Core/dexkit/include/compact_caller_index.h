#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

namespace dexkit {

// Built under the existing bridge warmup barrier and immutable after publication.
// The build array holds local counts first, then destination write cursors.
class CompactCallerIndex {
public:
    struct Entry {
        uint16_t first;
        uint32_t second;
        bool operator==(const Entry &) const = default;
    };
    static_assert(std::is_trivial_v<Entry>);
    static_assert(sizeof(Entry) == 8);

    void BeginCounts(size_t methods) {
        if (!build_.empty() || !offsets_.empty()
                || methods >= std::numeric_limits<size_t>::max() / sizeof(size_t)) std::abort();
        build_.resize(methods);
    }

    void Count(uint32_t method) {
        if (method >= build_.size()) std::abort();
        build_[method] = CheckedAdd(build_[method], 1);
    }

    size_t LocalCount(uint32_t method) const { return build_[method]; }

    void BeginLayout() {
        offsets_.resize(build_.size() + 1);
        std::copy(build_.begin(), build_.end(), offsets_.begin() + 1);
    }

    void EmptyRow(uint32_t method) { offsets_[method + 1] = 0; }
    void AddRowCount(uint32_t method, size_t count) {
        offsets_[method + 1] = CheckedAdd(offsets_[method + 1], count);
    }

    void Allocate() {
        for (size_t i = 1; i < offsets_.size(); ++i)
            offsets_[i] = CheckedAdd(offsets_[i - 1], offsets_[i]);
        edges_ = offsets_.back();
        if (edges_ > std::numeric_limits<size_t>::max() / sizeof(Entry)) std::abort();
        // Entry is trivial: default initialization leaves the exact payload
        // untouched. Every element is filled before the publication barrier.
        if (edges_) values_.reset(new Entry[edges_]);
    }

    size_t RowBegin(uint32_t method) const { return offsets_[method]; }
    size_t RowEnd(uint32_t method) const { return offsets_[method + 1]; }
    size_t &Cursor(uint32_t method) { return build_[method]; }

    void Write(size_t position, uint16_t dex, uint32_t method) {
        if (position >= edges_) std::abort();
        values_[position] = Entry{dex, method};
    }

    void ReleaseBuild() { std::vector<size_t>().swap(build_); }
    size_t BuildCapacityBytes() const { return build_.capacity() * sizeof(size_t); }
    bool empty() const { return offsets_.size() <= 1; }
    size_t size() const { return offsets_.empty() ? 0 : offsets_.size() - 1; }

    std::span<const Entry> operator[](size_t method) const {
        if (method >= size()) std::abort();
        const auto begin = offsets_[method], end = offsets_[method + 1];
        if (begin > end || end > edges_) std::abort();
        if (begin == end) return {};
        return {values_.get() + begin, end - begin};
    }

    static size_t CheckedAdd(size_t left, size_t right) {
        if (right > std::numeric_limits<size_t>::max() - left) std::abort();
        return left + right;
    }

private:
    friend struct BenchmarkDiagnostics;
    std::vector<size_t> build_;
    std::vector<size_t> offsets_;
    std::unique_ptr<Entry[]> values_;
    size_t edges_ = 0;
};

} // namespace dexkit

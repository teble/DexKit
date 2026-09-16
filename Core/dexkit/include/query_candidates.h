#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "inverted_string_index.h"

namespace dexkit { class DexItem; }

namespace dexkit::internal {

// This bounds explicitly accounted candidate arrays, not persistent indexes,
// matcher/trie caches, temporary associative containers or trie-hit buffers,
// result beans, allocator overhead, or total process memory.
class CandidateBudget {
    struct State {
        const size_t limit;
        std::atomic<size_t> used = 0;
        std::atomic<size_t> peak = 0;
        explicit State(size_t value) : limit(value) {}
    };

public:
    class Lease {
        friend class CandidateBudget;
        std::shared_ptr<State> owner_;
        size_t bytes_ = 0;
        Lease(std::shared_ptr<State> owner, size_t bytes) : owner_(std::move(owner)), bytes_(bytes) {}
    public:
        Lease() = default;
        Lease(const Lease &) = delete;
        Lease &operator=(const Lease &) = delete;
        Lease(Lease &&other) noexcept : owner_(std::move(other.owner_)), bytes_(std::exchange(other.bytes_, 0)) {}
        Lease &operator=(Lease &&other) noexcept {
            if (this != &other) {
                Reset();
                owner_ = std::move(other.owner_);
                bytes_ = std::exchange(other.bytes_, 0);
            }
            return *this;
        }
        ~Lease() { Reset(); }
        void Reset() noexcept {
            if (owner_) owner_->used.fetch_sub(bytes_, std::memory_order_acq_rel);
            owner_.reset();
            bytes_ = 0;
        }
        void Shrink(size_t bytes) noexcept {
            if (bytes < bytes_) {
                owner_->used.fetch_sub(bytes_ - bytes, std::memory_order_acq_rel);
                bytes_ = bytes;
            }
        }
        [[nodiscard]] size_t Bytes() const { return bytes_; }
    };

    explicit CandidateBudget(size_t bytes) : state_(std::make_shared<State>(bytes)) {}
    [[nodiscard]] std::optional<Lease> TryReserve(size_t bytes) const {
        if (bytes > state_->limit) return std::nullopt;
        auto used = state_->used.load(std::memory_order_relaxed);
        while (used <= state_->limit - bytes) {
            if (state_->used.compare_exchange_weak(used, used + bytes, std::memory_order_acq_rel)) {
                auto peak = state_->peak.load(std::memory_order_relaxed);
                while (peak < used + bytes && !state_->peak.compare_exchange_weak(peak, used + bytes,
                        std::memory_order_relaxed)) {}
                return Lease(state_, bytes);
            }
        }
        return std::nullopt;
    }
    [[nodiscard]] size_t Limit() const { return state_->limit; }
    [[nodiscard]] size_t Used() const { return state_->used.load(std::memory_order_acquire); }
    [[nodiscard]] size_t Peak() const { return state_->peak.load(std::memory_order_relaxed); }
private:
    std::shared_ptr<State> state_;
};

enum class CandidateEntity : uint8_t { Method, Class };

struct CandidateDomain {
    const void *dex = nullptr;
    uint64_t query_id = 0;
    CandidateEntity entity = CandidateEntity::Method;
    // MethodIds count for methods; ClassDefs count for classes.
    uint32_t count = 0;
    uint32_t dex_id = 0;
};

struct CandidateSource {
    DexItem *item = nullptr;
    CandidateDomain domain;
    inverted_string::QueryPlan strings;
    const void *root_matchers = nullptr;
    size_t work_bytes = 0;
    uint32_t slice_width = 1;
    bool split = false;
};

struct CandidateSlice {
    uint32_t begin = 0, end = 0;
};

struct CandidateView {
    enum class Kind : uint8_t { FullRange, Empty, Ids, Bitmap };
    Kind kind = Kind::FullRange;
    // IDs are enumeration coordinates: Method IDs or ClassDef ordinals.
    std::span<const uint32_t> ids;
    // A bitmap view is used only for Method-ID enumeration. Class Type-ID
    // truth is converted once to ordered ClassDef ordinals during preparation.
    const inverted_string::Bits *bitmap = nullptr;

    template<class Visit> void Each(CandidateSlice slice, Visit &&visit) const {
        switch (kind) {
            case Kind::FullRange:
                for (auto id = slice.begin; id < slice.end; ++id) visit(id);
                break;
            case Kind::Empty:
                break;
            case Kind::Ids:
                for (auto it = std::lower_bound(ids.begin(), ids.end(), slice.begin);
                        it != ids.end() && *it < slice.end; ++it) visit(*it);
                break;
            case Kind::Bitmap:
                bitmap->EachRange(slice.begin, slice.end, std::forward<Visit>(visit));
                break;
        }
    }
};

// Published through shared_ptr<const PreparedCandidates>. The bitmap can
// provide a complete root proof while the candidate view uses narrower IDs.
// Taking a slice never changes the bitmap or its proof coverage.
struct PreparedCandidates {
    // Declared first so the reservation outlives the storage it accounts for.
    CandidateBudget::Lease reservation;
    CandidateDomain domain;
    CandidateView::Kind kind = CandidateView::Kind::FullRange;
    const void *root_matchers = nullptr;
    inverted_string::Bits bits;
    std::vector<uint32_t> ids;
    std::vector<CandidateSlice> slices;

    [[nodiscard]] CandidateView View() const { return {kind, ids, &bits}; }
    [[nodiscard]] const inverted_string::Bits *RootTruth() const {
        return root_matchers ? &bits : nullptr;
    }
    [[nodiscard]] size_t RetainedBytes() const {
        return sizeof(*this) + bits.Bytes() + ids.capacity() * sizeof(uint32_t)
                + slices.capacity() * sizeof(CandidateSlice);
    }

    void MakeSlices(uint32_t width) {
        if (kind == CandidateView::Kind::Empty || domain.count == 0) return;
        slices.reserve((uint64_t(domain.count) + width - 1) / width);
        if (kind == CandidateView::Kind::FullRange) {
            for (uint32_t begin = 0; begin < domain.count;) {
                const auto end = static_cast<uint32_t>(std::min<uint64_t>(domain.count, uint64_t(begin) + width));
                slices.push_back({begin, end});
                begin = end;
            }
            return;
        }
        uint32_t last = UINT32_MAX;
        View().Each({0, domain.count}, [&](uint32_t id) {
            const auto ordinal = id / width;
            if (ordinal == last) return;
            last = ordinal;
            const auto begin = ordinal * width;
            slices.push_back({begin, static_cast<uint32_t>(std::min<uint64_t>(domain.count,
                    uint64_t(begin) + width))});
        });
    }
};

// Requested bytes for the controlled arrays. Raw keyword count bounds both
// bitmap-plane headers and last_string; canonicalization can only reduce it.
// The single output group also owns a (key, bitmap) entry during preparation.
inline std::optional<size_t> CandidateArrayBytes(size_t bitmap_bytes, size_t keywords,
        size_t cold_strings, size_t class_defs, size_t slices) {
    size_t bytes = sizeof(PreparedCandidates);
    auto add = [&](size_t count, size_t element) {
        if (count > (std::numeric_limits<size_t>::max() - bytes) / element) return false;
        bytes += count * element;
        return true;
    };
    if (!add(bitmap_bytes, 1)
            || !add(keywords, sizeof(inverted_string::Bits) + sizeof(uint32_t))
            || (keywords && !add(1, sizeof(std::pair<std::string_view, inverted_string::Bits>)))
            || !add(cold_strings, sizeof(uint32_t) * 2)
            || !add(class_defs, sizeof(uint32_t))
            || !add(slices, sizeof(CandidateSlice))) return std::nullopt;
    return bytes;
}

} // namespace dexkit::internal

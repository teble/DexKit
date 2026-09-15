#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

namespace dexkit {

// Dense publication slots own immutable standard strings. DexItem serializes
// cold writes with its shared stripes; destruction requires query quiescence.
class PointerDescriptorCache {
    bool initialized_ = false;
    size_t size_ = 0;
    std::unique_ptr<std::atomic<const std::string *>[]> slots_;

public:
    PointerDescriptorCache() = default;
    PointerDescriptorCache(const PointerDescriptorCache &) = delete;
    PointerDescriptorCache &operator=(const PointerDescriptorCache &) = delete;
    ~PointerDescriptorCache() {
        for (size_t i = 0; i < size_; ++i) delete slots_[i].load(std::memory_order_relaxed);
    }

    void Initialize(size_t size) {
        if (initialized_) std::abort();
        auto slots = std::make_unique<std::atomic<const std::string *>[]>(size);
        slots_ = std::move(slots);
        size_ = size;
        initialized_ = true;
    }
    size_t size() const { return size_; }

    // Like the existing dense cache, callers supply a validated member index.
    const std::string *TryGet(size_t index) const {
        return slots_[index].load(std::memory_order_acquire);
    }

    // The caller holds the member's stripe and has rechecked this slot.
    const std::string &Publish(size_t index, std::unique_ptr<std::string> value) {
        if (index >= size_ || !value || slots_[index].load(std::memory_order_relaxed)) std::abort();
        const auto *published = value.release();
        slots_[index].store(published, std::memory_order_release);
        return *published;
    }

#if DEXKIT_BENCHMARK_DIAGNOSTICS
    struct Statistics {
        size_t index_bytes = 0, payload_bytes = 0, records = 0, buffers = 0, sso_records = 0;
    };
    // Diagnostic snapshots run after active queries have drained.
    Statistics GetStatistics() const {
        Statistics stats;
        stats.index_bytes = size_ * sizeof(std::atomic<const std::string *>);
        stats.buffers = size_ != 0;
        for (size_t i = 0; i < size_; ++i) {
            auto *value = TryGet(i);
            if (!value) continue;
            ++stats.records;
            ++stats.buffers;
            stats.payload_bytes += sizeof(std::string);
            const auto object = reinterpret_cast<uintptr_t>(value);
            const auto data = reinterpret_cast<uintptr_t>(value->data());
            if (data < object || data >= object + sizeof(*value)) {
                stats.payload_bytes += value->capacity() + 1;
                ++stats.buffers;
            } else {
                ++stats.sso_records;
            }
        }
        return stats;
    }
#endif
};

} // namespace dexkit

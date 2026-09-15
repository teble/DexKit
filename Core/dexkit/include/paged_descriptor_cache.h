#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string_view>
#include <vector>
#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include <chrono>
#endif

namespace dexkit {

// Integer-indexed cache whose published views stay valid until destruction.
// Pages and byte blocks are allocated only on cold misses and never move.
class PagedDescriptorCache {
    struct Record {
        size_t length;
        char *data() { return reinterpret_cast<char *>(this + 1); }
        const char *data() const { return reinterpret_cast<const char *>(this + 1); }
        std::string_view view() const { return {data(), length}; }
    };
    static constexpr size_t kPageSize = 256;
    static constexpr size_t kStripeCount = 8;
    static constexpr size_t kInitialBlockSize = 4096;
    static constexpr size_t kMaximumBlockSize = 65536;
    struct Page {
        std::array<std::atomic<Record *>, kPageSize> slots{};
    };
    struct Block {
        std::unique_ptr<std::byte[]> data;
        size_t capacity;
        size_t used = 0;
        // Deliberately do not value-initialize (touch) the entire byte block.
        explicit Block(size_t bytes) : data(new std::byte[bytes]), capacity(bytes) {}
    };
    struct Stripe {
        mutable std::mutex mutex;
        std::vector<std::unique_ptr<Page>> pages;
        std::vector<Block> blocks;
        size_t next_block_size = kInitialBlockSize;
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        uint64_t cold_lock_count = 0;
        uint64_t cold_lock_wait_ns = 0;
#endif
    };

    bool initialized_ = false;
    size_t size_ = 0, page_count_ = 0;
    std::unique_ptr<std::atomic<Page *>[]> directory_;
    std::array<Stripe, kStripeCount> stripes_;

    static size_t CheckedAdd(size_t value, size_t extra) {
        if (extra > std::numeric_limits<size_t>::max() - value) std::abort();
        return value + extra;
    }
    static size_t AlignRecord(size_t value) {
        constexpr size_t alignment = alignof(Record);
        return CheckedAdd(value, alignment - 1) & ~(alignment - 1);
    }
    static Block &GetBlock(Stripe &stripe, size_t needed) {
        if (!stripe.blocks.empty()) {
            auto &block = stripe.blocks.back();
            const auto offset = AlignRecord(block.used);
            if (offset <= block.capacity && needed <= block.capacity - offset) return block;
        }
        stripe.blocks.emplace_back(std::max(stripe.next_block_size, needed));
        stripe.next_block_size = std::min(kMaximumBlockSize, stripe.next_block_size * 2);
        return stripe.blocks.back();
    }

public:
    PagedDescriptorCache() = default;
    PagedDescriptorCache(const PagedDescriptorCache &) = delete;
    PagedDescriptorCache &operator=(const PagedDescriptorCache &) = delete;

    void Initialize(size_t size) {
        if (initialized_) std::abort();
        initialized_ = true;
        size_ = size;
        page_count_ = size / kPageSize + (size % kPageSize != 0);
        if (page_count_) directory_ = std::make_unique<std::atomic<Page *>[]>(page_count_);
    }

    size_t size() const { return size_; }

    // visit must be repeatable and must not reenter this cache: it runs twice
    // under the stripe lock, first to size the record and then to write it.
    template<class VisitParts, class OnBuilt>
    std::string_view GetOrCreate(size_t index, VisitParts &&visit, OnBuilt &&on_built) {
        if (index >= size_) std::abort();
        const auto page_index = index / kPageSize;
        const auto slot = index % kPageSize;
        auto *page = directory_[page_index].load(std::memory_order_acquire);
        if (page) {
            if (auto *record = page->slots[slot].load(std::memory_order_acquire)) return record->view();
        }
        auto &stripe = stripes_[page_index % kStripeCount];
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        const auto wait_begin = std::chrono::steady_clock::now();
#endif
        std::lock_guard lock(stripe.mutex);
#if DEXKIT_BENCHMARK_DIAGNOSTICS
        ++stripe.cold_lock_count;
        stripe.cold_lock_wait_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - wait_begin).count();
#endif
        page = directory_[page_index].load(std::memory_order_relaxed);
        if (!page) {
            auto owned = std::make_unique<Page>();
            page = owned.get();
            stripe.pages.push_back(std::move(owned));
            directory_[page_index].store(page, std::memory_order_release);
        }
        if (auto *record = page->slots[slot].load(std::memory_order_relaxed)) return record->view();

        size_t length = 0;
        visit([&](std::string_view part) { length = CheckedAdd(length, part.size()); });
        const auto needed = CheckedAdd(sizeof(Record), CheckedAdd(length, 1));
        auto &block = GetBlock(stripe, needed);
        const auto offset = AlignRecord(block.used);
        auto *record = ::new (static_cast<void *>(block.data.get() + offset)) Record{length};
        char *out = record->data();
        size_t remaining = length;
        visit([&](std::string_view part) {
            if (part.size() > remaining) std::abort();
            if (!part.empty()) std::memcpy(out, part.data(), part.size());
            out += part.size();
            remaining -= part.size();
        });
        if (remaining != 0) std::abort();
        *out = '\0';
        block.used = offset + needed;
        page->slots[slot].store(record, std::memory_order_release);
        on_built(length);
        return record->view();
    }

    struct Statistics {
        size_t index_bytes = 0, record_capacity_bytes = 0, record_used_bytes = 0;
        size_t character_bytes = 0, record_bytes = 0, records = 0, pages = 0, blocks = 0, buffers = 0;
        uint64_t cold_lock_count = 0, cold_lock_wait_ns = 0;
    };

    // This scan is for diagnostics/checks only; no record counters are updated
    // on the normal path. Stripe locks also make an in-flight snapshot safe.
    Statistics GetStatistics() const {
        Statistics result;
        result.index_bytes = sizeof(*this) + page_count_ * sizeof(std::atomic<Page *>);
        result.buffers = page_count_ != 0;
        for (const auto &stripe : stripes_) {
            std::lock_guard lock(stripe.mutex);
            result.index_bytes += stripe.pages.capacity() * sizeof(std::unique_ptr<Page>)
                    + stripe.blocks.capacity() * sizeof(Block) + stripe.pages.size() * sizeof(Page);
            result.pages += stripe.pages.size();
            result.blocks += stripe.blocks.size();
            result.buffers += stripe.pages.size() + stripe.blocks.size()
                    + (stripe.pages.capacity() != 0) + (stripe.blocks.capacity() != 0);
            for (const auto &block : stripe.blocks) {
                result.record_capacity_bytes += block.capacity;
                result.record_used_bytes += block.used;
            }
            for (const auto &page : stripe.pages) for (const auto &slot : page->slots) {
                if (const auto *record = slot.load(std::memory_order_relaxed)) {
                    ++result.records;
                    result.character_bytes += record->length + 1;
                    result.record_bytes += sizeof(Record) + record->length + 1;
                }
            }
#if DEXKIT_BENCHMARK_DIAGNOSTICS
            result.cold_lock_count += stripe.cold_lock_count;
            result.cold_lock_wait_ns += stripe.cold_lock_wait_ns;
#endif
        }
        return result;
    }
};

} // namespace dexkit

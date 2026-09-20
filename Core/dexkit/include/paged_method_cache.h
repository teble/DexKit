// DexKit - An high-performance runtime parsing library for dex
// implemented in C++.
// Copyright (C) 2022-2023 LuckyPray
// https://github.com/LuckyPray/DexKit
//
// This program is free software: you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see
// <https://www.gnu.org/licenses/>.
// <https://github.com/LuckyPray/DexKit/blob/master/LICENSE>.

#pragma once

#include "common.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace dexkit {

inline constexpr size_t kLazyMethodCacheStripeCount = 64;

enum class LazyMethodCacheState : uint8_t {
    Empty = 0,
    Building = 1,
    Ready = 2,
};

// Directory pages and result blocks never move. The caller publishes a slot
// through state, and ready rows remain alive until the cache is destroyed.
template<class T, size_t PageSize = 128>
class PagedMethodCache {
    static_assert(PageSize > 0);
    static constexpr size_t kRowsPerBlock = 32;
public:
    struct Slot {
        std::atomic<LazyMethodCacheState> state{LazyMethodCacheState::Empty};
        const std::vector<T> *data = nullptr;
    };

    Slot &GetSlot(uint32_t method_idx, size_t method_count) {
        DEXKIT_CHECK(method_idx < method_count);
        std::call_once(directory_once_, [this, method_count] {
            const auto pages = method_count / PageSize + (method_count % PageSize != 0);
            auto directory = std::make_unique<Directory>();
            directory->pages = std::make_unique<PageEntry[]>(pages);
            directory_ = std::move(directory);
        });
        auto &entry = directory_->pages[method_idx / PageSize];
        std::call_once(entry.once, [&entry] {
            entry.page = std::make_unique<Page>();
        });
        return entry.page->slots[method_idx % PageSize];
    }

    const std::vector<T> *Store(size_t stripe, std::vector<T> &&data) {
        DEXKIT_CHECK(directory_ != nullptr && stripe < kLazyMethodCacheStripeCount);
        if (data.empty()) return &directory_->empty;
        // The caller holds this stripe's wait mutex through Store and publication.
        auto &storage = directory_->storage[stripe];
        if (!storage.blocks || storage.blocks->size == kRowsPerBlock) {
            auto block = std::make_unique<ResultBlock>();
            block->next = std::move(storage.blocks);
            storage.blocks = std::move(block);
        }
        auto &row = storage.blocks->rows[storage.blocks->size++];
        row = std::move(data);
        return &row;
    }

private:
    struct Page {
        std::array<Slot, PageSize> slots;
    };

    struct PageEntry {
        std::once_flag once;
        std::unique_ptr<Page> page;
    };

    struct ResultBlock {
        std::array<std::vector<T>, kRowsPerBlock> rows;
        size_t size = 0;
        std::unique_ptr<ResultBlock> next;
    };

    struct StorageShard {
        std::unique_ptr<ResultBlock> blocks;

        ~StorageShard() {
            // Avoid recursive destruction for large populated caches.
            while (blocks) {
                auto block = std::move(blocks);
                blocks = std::move(block->next);
            }
        }
    };

    struct Directory {
        std::unique_ptr<PageEntry[]> pages;
        // Result blocks share the caller's existing publication locks.
        std::array<StorageShard, kLazyMethodCacheStripeCount> storage;
        const std::vector<T> empty;
    };

    std::once_flag directory_once_;
    std::unique_ptr<Directory> directory_;
};

} // namespace dexkit

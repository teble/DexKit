// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "zip_archive.h"

#include <algorithm>
#include "ThreadPool.h"

namespace dexkit {

std::vector<std::unique_ptr<MemMap>> ZipArchive::GetUncompressData(
        const std::vector<const Entry *> &entries, size_t thread_num,
        size_t alignment, bool copy_stored) const {
    if (entries.empty() || alignment == 0) return {};
    std::vector<std::unique_ptr<MemMap>> images;
    images.reserve(entries.size());
    std::vector<std::future<std::unique_ptr<MemMap>>> futures;
    futures.reserve(entries.size());
    ThreadPool pool(std::min(std::max(size_t{1}, thread_num), entries.size()));
    for (const auto *entry : entries) {
        futures.push_back(pool.enqueue([this, entry, alignment, copy_stored]() {
            if (!entry) return std::unique_ptr<MemMap>{};
            // Stored entries are copied exactly once when detachment is needed,
            // including unaligned ZIP slices. The new allocation is aligned.
            auto image = GetUncompressData(*entry, copy_stored ? 1 : alignment);
            if (!image.ok()) return std::unique_ptr<MemMap>{};
            auto owned = copy_stored && entry->method == COMP_STORE
                         ? std::make_unique<MemMap>(image.data(), image.len())
                         : std::make_unique<MemMap>(std::move(image));
            if (!owned->ok()) return std::unique_ptr<MemMap>{};
            return owned;
        }));
    }
    bool failed = false;
    for (auto &future : futures) {
        auto image = future.get();
        failed |= !image;
        images.push_back(std::move(image));
    }
    if (failed) return {};
    return images;
}

} // namespace dexkit

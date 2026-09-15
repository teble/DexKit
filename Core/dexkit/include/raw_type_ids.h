#pragma once

#include <cstddef>
#include <cstdint>
#include "slicer/dex_format.h"

namespace dexkit {
class RawTypeIds {
    const dex::TypeItem *items_ = nullptr;
    size_t size_ = 0;

public:
    RawTypeIds() = default;
    explicit RawTypeIds(const dex::TypeList *list)
        : items_(list ? list->list : nullptr), size_(list ? list->size : 0) {}
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    uint32_t operator[](size_t index) const { return items_[index].type_idx; }
};
} // namespace dexkit

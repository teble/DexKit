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
#include "index_types.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <vector>

namespace dexkit {

// Immutable after publication. Build every row in method-ID order, including
// empty methods. Adjacent boundaries delimit each row; decoding keeps duplicates.
template<class Id>
class CompactIdIndex {
public:
    using value_type = Id;
    void resize(size_t methods) {
        if (methods == std::numeric_limits<size_t>::max()) std::abort();
        offsets_.resize(methods + 1);
    }

    bool empty() const { return offsets_.size() <= 1; }

    std::vector<Id> *BeginMethod(uint32_t method) {
        DEXKIT_CHECK(!empty() && method < offsets_.size() - 1);
        offsets_[method] = static_cast<CacheOffset>(ids_.size());
        return &ids_;
    }

    void EndMethod(uint32_t method) {
        DEXKIT_CHECK(!empty() && method < offsets_.size() - 1);
        const auto end = static_cast<CacheOffset>(ids_.size());
        DEXKIT_CHECK(offsets_[method] <= end);
        offsets_[size_t(method) + 1] = end;
    }

    // No offsets may be consumed before this check. The vector retains the
    // full size even if a provisional 32-bit boundary wrapped while building.
    void FinishBuild() const {
        (void) CheckedIndexCast<CacheOffset>(ids_.size());
    }

    size_t ValueCount() const { return ids_.size(); }

    std::span<const Id> operator[](size_t method) const {
        DEXKIT_CHECK(!empty() && method < offsets_.size() - 1);
        const auto begin = offsets_[method], end = offsets_[method + 1];
        DEXKIT_CHECK(begin <= end && end <= ids_.size());
        return std::span<const Id>(ids_).subspan(begin, end - begin);
    }

private:
    std::vector<CacheOffset> offsets_;
    std::vector<Id> ids_;
};

// Invocation rows share the same checked append/freeze representation.
using CompactStringIndex = CompactIdIndex<uint32_t>;
using CompactInvocationIndex = CompactIdIndex<InvokeOperandId>;

} // namespace dexkit

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>

namespace dexkit {

#if DEXKIT_EXPERIMENT_NARROW_TYPES
using CacheOffset = uint32_t;
using LocalMethodId = uint16_t;
using ClassDefIndex = uint16_t;
#else
using CacheOffset = size_t;
using LocalMethodId = uint32_t;
using ClassDefIndex = uint32_t;
#endif

using MethodReference = std::pair<uint16_t, LocalMethodId>;
static_assert(sizeof(MethodReference) == (sizeof(LocalMethodId) == 2 ? 4 : 8));

// Only storage is narrowed. Arithmetic and public IDs retain their full width.
template<class To, class From>
constexpr To CheckedIndexCast(From value) {
    static_assert(std::is_integral_v<To> && std::is_integral_v<From>);
    if (!std::in_range<To>(value)) std::abort();
    return static_cast<To>(value);
}

inline CacheOffset CheckedOffsetAdd(CacheOffset left, size_t right) {
    if (right > std::numeric_limits<CacheOffset>::max() - left) std::abort();
    return static_cast<CacheOffset>(left + right);
}

} // namespace dexkit

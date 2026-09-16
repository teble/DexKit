#pragma once

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>

namespace dexkit {

// Current field instructions (0x52..0x6d) carry a 16-bit field ID. Keep the
// validation here so adding a wider producer cannot silently truncate IDs.
class PackedFieldUse {
    uint32_t encoded_ = 0;
public:
    PackedFieldUse() = default;
    static constexpr bool CanEncode(uint32_t field) {
        return field <= std::numeric_limits<uint16_t>::max();
    }
    PackedFieldUse(uint32_t field, bool get) {
        if (!CanEncode(field)) std::abort();
        encoded_ = (field << 1) | uint32_t(get);
    }
    std::pair<uint32_t, bool> Decode() const {
        return {encoded_ >> 1, bool(encoded_ & 1)};
    }
    bool operator==(const PackedFieldUse &) const = default;
};
static_assert(sizeof(PackedFieldUse) == sizeof(uint32_t));
static_assert(alignof(PackedFieldUse) == alignof(uint32_t));
static_assert(std::is_trivially_copyable_v<PackedFieldUse>);

inline std::pair<uint32_t, bool> DecodeFieldUse(PackedFieldUse use) { return use.Decode(); }
inline std::pair<uint32_t, bool> DecodeFieldUse(std::pair<uint32_t, bool> use) { return use; }

#if DEXKIT_EXPERIMENT_PACKED_FIELD_USES
using FieldUse = PackedFieldUse;
#else
using FieldUse = std::pair<uint32_t, bool>;
#endif

} // namespace dexkit

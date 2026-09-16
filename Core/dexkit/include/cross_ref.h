#pragma once

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <type_traits>
#include <utility>

namespace dexkit {

// Preserve the complete existing u16/u32 target domain, including (0, 0).
// Identity is written under the existing warmup barrier and then immutable.
class PackedCrossRef {
public:
    using Target = std::pair<uint16_t, uint32_t>;

    constexpr PackedCrossRef() = default;
    constexpr PackedCrossRef(Target target)
        : encoded_(((uint64_t(target.first) << 32) | target.second) + 1) {}

    constexpr bool has_value() const { return encoded_ != 0; }
    constexpr explicit operator bool() const { return has_value(); }
    constexpr Target value() const {
        if (!has_value()) std::abort();
        const uint64_t target = encoded_ - 1;
        return {static_cast<uint16_t>(target >> 32), static_cast<uint32_t>(target)};
    }
    bool operator==(const PackedCrossRef &) const = default;

private:
    uint64_t encoded_ = 0;
};

static_assert(sizeof(PackedCrossRef) == 8);
static_assert(std::is_trivially_copyable_v<PackedCrossRef>);

#if DEXKIT_EXPERIMENT_PACKED_CROSS_INFO
using CrossRefInfo = PackedCrossRef;
#else
using CrossRefInfo = std::optional<PackedCrossRef::Target>;
#endif

} // namespace dexkit

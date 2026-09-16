#include "cross_ref.h"
#include <array>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
using dexkit::PackedCrossRef;
using Target = PackedCrossRef::Target;
void Require(bool condition) { if (!condition) std::abort(); }
}

int main() {
    PackedCrossRef empty;
    Require(!empty && !empty.has_value());
    const std::array<Target, 8> boundaries{{
        {0, 0}, {0, 1}, {1, 0}, {0, UINT32_MAX},
        {UINT16_MAX, 0}, {UINT16_MAX, 1}, {UINT16_MAX, UINT32_MAX}, {1, UINT32_MAX}}};
    std::vector<PackedCrossRef> packed(2);
    std::vector<std::optional<Target>> reference(2);
    for (auto target : boundaries) {
        PackedCrossRef entry(target);
        Require(entry && entry.has_value() && entry.value() == target);
        Require(entry != empty);
        packed.emplace_back(target); reference.emplace_back(target);
    }
    uint32_t random = 0x92bc4571U;
    for (uint32_t i = 0; i < 65536; ++i) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        const Target target{static_cast<uint16_t>(i), random};
        packed.emplace_back(target); reference.emplace_back(target);
        if ((i % 257) == 0) { packed.emplace_back(); reference.emplace_back(); }
    }
    for (size_t i = 0; i < reference.size(); ++i) {
        Require(packed[i].has_value() == reference[i].has_value());
        if (reference[i]) Require(packed[i].value() == reference[i].value());
    }
    const auto copy = packed;
    Require(copy == packed);
    packed.back() = PackedCrossRef{};
    Require(!packed.back());
    std::puts("packed cross-reference null, full target domain and relocation checks passed");
}

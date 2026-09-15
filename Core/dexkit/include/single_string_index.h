#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "string_query_diagnostics.h"

namespace dexkit::single_string {

inline bool IsNonemptyAscii(std::string_view value) {
    if (value.empty()) return false;
    for (unsigned char ch : value) if (ch == 0 || ch > 0x7f) return false;
    return true;
}

// DEX ordering is by unsigned UTF-16 code units. MUTF-8 NUL is C0 80;
// surrogate units remain separate. Only bounded reads are permitted here.
inline bool ReadMutf8Unit(std::string_view value, size_t &offset, uint16_t &unit) {
    if (offset >= value.size()) return false;
    const auto first = static_cast<uint8_t>(value[offset++]);
    if (first > 0 && first < 0x80) {
        unit = first;
    } else if (first >= 0xc0 && first <= 0xdf) {
        if (value.size() - offset < 1) return false;
        const auto second = static_cast<uint8_t>(value[offset++]);
        if ((second & 0xc0) != 0x80) return false;
        unit = static_cast<uint16_t>(((first & 0x1f) << 6) | (second & 0x3f));
        if (unit < 0x80 && !(first == 0xc0 && second == 0x80)) return false;
    } else if (first >= 0xe0 && first <= 0xef) {
        if (value.size() - offset < 2) return false;
        const auto second = static_cast<uint8_t>(value[offset++]);
        const auto third = static_cast<uint8_t>(value[offset++]);
        if ((second & 0xc0) != 0x80 || (third & 0xc0) != 0x80) return false;
        unit = static_cast<uint16_t>(((first & 0xf) << 12) | ((second & 0x3f) << 6) | (third & 0x3f));
        if (unit < 0x800) return false;
    } else {
        return false;
    }
    DEXKIT_STRING_COUNT(decoded_units, 1);
    return true;
}

inline std::optional<int> ComparePrefix(std::string_view value, std::string_view ascii) {
    DEXKIT_STRING_COUNT(comparisons, 1);
    size_t offset = 0;
    for (unsigned char expected : ascii) {
        if (offset == value.size()) return -1;
        uint16_t unit;
        if (!ReadMutf8Unit(value, offset, unit)) return std::nullopt;
        if (unit != expected) return unit < expected ? -1 : 1;
    }
    return 0;
}

struct IdRange {
    size_t begin = 0;
    size_t end = 0;
    bool valid = false;
};

// The input pool follows the DEX UTF-16 ordering contract. A malformed unit
// encountered during a probe abandons the plan; callers use their old path.
template<typename Strings>
IdRange FindIds(const Strings &strings, std::string_view ascii, bool prefix) {
    if (!IsNonemptyAscii(ascii)) return {};
    DEXKIT_STRING_COUNT(ranges, 1);
    auto bound = [&](bool upper) -> std::optional<size_t> {
        size_t left = 0, right = strings.size();
        while (left < right) {
            const size_t middle = left + (right - left) / 2;
            const auto comparison = ComparePrefix(strings[middle], ascii);
            if (!comparison) return std::nullopt;
            if (*comparison < 0 || (upper && *comparison == 0)) left = middle + 1;
            else right = middle;
        }
        return left;
    };
    const auto begin = bound(false);
    if (!begin) return {};
    if (!prefix) {
        if (*begin == strings.size() || strings[*begin] != ascii) return {0, 0, true};
        return {*begin, *begin + 1, true};
    }
    const auto end = bound(true);
    if (!end) return {};
    return {*begin, *end, true};
}
}

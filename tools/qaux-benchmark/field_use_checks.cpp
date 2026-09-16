#include "field_use.h"
#include <cstdio>
#include <vector>

namespace {
void Require(bool condition) { if (!condition) std::abort(); }
} // namespace

int main(int argc, char **argv) {
    using dexkit::PackedFieldUse;
    if (argc == 2) {
        char *end = nullptr;
        const auto id = std::strtoull(argv[1], &end, 10);
        Require(end && *end == '\0' && id > 65535 && id <= UINT32_MAX);
        // The harness requires SIGABRT: a future wider producer must not wrap.
        const auto value = PackedFieldUse(uint32_t(id), true);
        return int(value.Decode().first);
    }
    Require(argc == 1);
    Require(dexkit::DecodeFieldUse(PackedFieldUse()) == std::pair<uint32_t, bool>(0, false));
    Require(!PackedFieldUse::CanEncode(65536) && !PackedFieldUse::CanEncode(UINT32_MAX));
    std::vector<PackedFieldUse> values;
    std::vector<std::pair<uint32_t, bool>> expected;
    for (uint32_t id = 0; id <= 65535; ++id) for (bool get : {false, true}) {
        Require(PackedFieldUse::CanEncode(id));
        Require(PackedFieldUse(id, get).Decode() == std::pair<uint32_t, bool>(id, get));
        values.emplace_back(id, get); expected.emplace_back(id, get);
        if (id % 127 == 0) {
            values.emplace_back(id, get); expected.emplace_back(id, get);
        }
    }
    const auto copied = values;
    const auto moved = std::move(values);
    Require(copied == moved && copied.size() == expected.size());
    for (size_t i = 0; i < copied.size(); ++i) {
        Require(dexkit::DecodeFieldUse(copied[i]) == expected[i]);
        Require(dexkit::DecodeFieldUse(expected[i]) == expected[i]);
    }
    std::printf("CHECK_FIELD_USE {\"passed\":true,\"field_ids\":65536,\"ordered_uses\":%zu}\n", copied.size());
}

// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "metadata.h"

namespace dexkit::smali {

struct Decoded {
    dex::Instruction instruction{};
    uint32_t width = 0;
    uint16_t payload = 0;
};

struct Payload {
    uint32_t offset;
    uint32_t owner = UINT32_MAX;
    uint16_t kind;
};

struct Handler { uint32_t type, target; };
struct TryRange {
    uint32_t start, end;
    std::vector<Handler> handlers;
};

class Code {
public:
    explicit Code(CheckedDex& dex) : dex_(dex), state_(dex.state()) {}
    bool Build(uint32_t offset);
    // The only adapter that calls slicer's unchecked decoder. Emission reuses
    // this adapter too; fixed-width data is copied to an aligned local array.
    bool DecodeAt(uint32_t pc, Decoded& decoded);
    bool EmitAt(uint32_t pc, const Decoded& decoded);
    bool EmitCatches();
    bool Label(uint32_t pc);
    bool HasLabel(uint32_t pc) const { return pc < marks_.size() && (marks_[pc] & kLabel); }
    bool Boundary(uint32_t pc, bool allow_end = false) const;
    const dex::Code& header() const { return header_; }
    size_t byte_offset(uint32_t pc) const { return instructions_ + size_t(pc) * 2; }

private:
    static constexpr uint8_t kInstruction = 1, kPayload = 2, kLabel = 4;
    bool Target(uint32_t pc, int32_t delta, uint32_t& target, bool executable = true);
    bool ValidateRegisters(const dex::Instruction& instruction);
    bool ValidateReference(const dex::Instruction& instruction);
    bool PayloadLinks();
    bool TryBlocks();
    bool EmitPayload(uint32_t pc, uint16_t kind);
    bool Reg(uint32_t reg);
    bool Constant(const dex::Instruction& instruction);

    CheckedDex& dex_;
    State& state_;
    size_t instructions_ = 0;
    std::vector<uint8_t> marks_;
    std::vector<Payload> payloads_;
    std::vector<TryRange> tries_;
    dex::Code header_{};
};

} // namespace dexkit::smali

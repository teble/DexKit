// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "code.h"

namespace dexkit::smali {

// Interprets a debug stream independently of CodeIr's synthetic initial events
// and offset merging. Constructing this object does not read any debug bytes.
class DebugInfo {
public:
    DebugInfo(CheckedDex& dex, const Code& code) : dex_(dex), state_(dex.state()), code_(code) {}
    bool Read(const std::vector<uint16_t>& parameters, bool is_static);
    bool EmitAt(uint32_t pc);
    const std::vector<uint32_t>& parameter_names() const { return parameter_names_; }
private:
    struct Event {
        uint32_t pc = 0, reg = 0, name = dex::kNoIndex, type = dex::kNoIndex, signature = dex::kNoIndex;
        int32_t line = 0;
        uint8_t opcode = 0;
    };
    bool Index(size_t& cursor, uint32_t& value, uint32_t count);
    bool Add(Event event, size_t cursor);
    CheckedDex& dex_;
    State& state_;
    const Code& code_;
    std::vector<uint32_t> parameter_names_;
    std::vector<Event> events_;
    size_t next_ = 0;
};

} // namespace dexkit::smali

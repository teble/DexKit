// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace dexkit {

enum class SmaliError : uint8_t {
    Ok = 0, InvalidIdentity = 1, NotDefined = 2, Unsupported = 3,
    MalformedInput = 4, DebugNotRepresentable = 5, LimitExceeded = 6,
    InternalError = 7, BridgeClosed = 8
};

enum class SmaliPhase : uint8_t { Resolve = 0, Read = 1, Decode = 2, Emit = 3 };
enum class SmaliDebugMode : uint8_t { None = 0, Strict = 1 };
enum class SmaliMemberKind : uint8_t { None = 0, Class = 1, Field = 2, Method = 3 };

struct SmaliStatus {
    SmaliError error = SmaliError::Ok;
    SmaliPhase phase = SmaliPhase::Resolve;
    // Byte offsets are relative to the physical DEX container; code offsets are
    // in 16-bit units relative to the current method. MAX means unavailable.
    uint64_t dex_offset = UINT64_MAX;
    uint32_t code_offset = UINT32_MAX;
    uint32_t detail = 0;
    uint32_t dex_id = UINT32_MAX;
    SmaliMemberKind member_kind = SmaliMemberKind::None;
    uint32_t member_id = UINT32_MAX;
    [[nodiscard]] bool ok() const { return error == SmaliError::Ok; }
};

struct SmaliOptions {
    SmaliDebugMode debug = SmaliDebugMode::None;
    size_t max_output_bytes = 16 * 1024 * 1024;
    // Cumulative bytes visited, including repeated reference resolution.
    size_t max_input_bytes = 64 * 1024 * 1024;
    size_t max_code_units = 1024 * 1024;
    size_t max_items = 1024 * 1024;
    uint32_t max_annotation_depth = 64;
};

} // namespace dexkit

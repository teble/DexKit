// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "checked_dex.h"

namespace dexkit::smali {

// Internal entry points also permit direct tests before the existing loader.
// The caller owns immutable source memory through the complete synchronous call.
// Failure leaves output unchanged. Class indices here are class_def indices;
// public ClassData/DexKit APIs resolve their local type IDs before calling here.
SmaliStatus WriteMethod(std::span<const uint8_t> image, size_t header_offset,
                        uint32_t class_def_index, uint32_t method_id,
                        const SmaliOptions& options, std::string& output);
SmaliStatus WriteClass(std::span<const uint8_t> image, size_t header_offset,
                       uint32_t class_def_index, const SmaliOptions& options,
                       std::string& output);

} // namespace dexkit::smali

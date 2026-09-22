// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "checked_dex.h"
#include "slicer/dex_bytecode.h"

namespace dexkit::smali {

class Metadata {
public:
    explicit Metadata(CheckedDex& dex) : dex_(dex), state_(dex.state()) {}
    bool Index(dex::InstructionIndexType kind, uint32_t index);
    bool Value(size_t& cursor, uint32_t depth = 0, uint8_t* value_type = nullptr);
    bool AnnotationSet(uint32_t offset);
    bool Annotation(size_t& cursor, uint32_t depth, bool nested);
    bool Handle(uint32_t index);
    bool CallSite(uint32_t index);

private:
    bool IndexedValue(size_t& cursor, uint8_t expected, uint32_t& index);
    bool GetHandle(uint32_t index, dex::MethodHandle& handle);
    bool Floating(uint64_t bits, bool wide);
    CheckedDex& dex_;
    State& state_;
};

bool Access(State& state, uint32_t flags, SmaliMemberKind kind);

} // namespace dexkit::smali

// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "checked_dex.h"

namespace dexkit::smali {

struct Member {
    uint32_t index = 0;
    uint32_t access = 0;
    uint32_t code_offset = 0;
};

struct AnnotationOffsets {
    uint32_t annotations = 0;
    uint32_t parameters = 0;
};

struct ClassMembers {
    std::vector<Member> static_fields, instance_fields, direct_methods, virtual_methods;
};

// Per-class borrowed directory view. Validate fixed records once, then binary
// search without reparsing every annotation directory for every member.
class AnnotationDirectory {
public:
    bool Init(CheckedDex& dex, const dex::ClassDef& owner, const ClassMembers& members);
    bool Find(CheckedDex& dex, SmaliMemberKind kind, uint32_t member_id, AnnotationOffsets& offsets) const;
private:
    uint32_t class_annotations_ = 0;
    size_t offsets_[3]{};
    uint32_t counts_[3]{};
};

// Selectors only inspect definition/directory records. They never follow code,
// debug or recursive annotation offsets for an unrelated member.
bool SelectMethod(CheckedDex& dex, const dex::ClassDef& owner, uint32_t method_id, Member& method);
bool SelectClass(CheckedDex& dex, const dex::ClassDef& owner, ClassMembers& members);
bool SelectAnnotations(CheckedDex& dex, const dex::ClassDef& owner,
                       SmaliMemberKind kind, uint32_t member_id, AnnotationOffsets& offsets);
bool CheckMetadataProfile(CheckedDex& dex);

} // namespace dexkit::smali

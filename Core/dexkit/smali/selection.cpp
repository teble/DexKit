// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "selection.h"

#include <algorithm>

namespace dexkit::smali {
namespace {

bool Members(CheckedDex& dex, const dex::ClassDef& owner,
             uint32_t target, Member* method, ClassMembers* members) {
    auto& state = dex.state();
    state.phase = SmaliPhase::Resolve;
    if (!owner.class_data_off) return !method || state.Fail(SmaliError::NotDefined);
    size_t cursor = owner.class_data_off;
    uint32_t counts[4];
    for (auto& count : counts) if (!dex.Uleb(cursor, count)) return false;
    for (unsigned group = 0; group < 4; ++group) {
        const bool methods = group >= 2;
        const auto& header = dex.header();
        uint32_t maximum = methods ? header.method_ids_size : header.field_ids_size;
        if (counts[group] > maximum) return state.Fail(SmaliError::MalformedInput, cursor);
        std::vector<Member>* list = nullptr;
        if (members) {
            switch (group) {
                case 0: list = &members->static_fields; break;
                case 1: list = &members->instance_fields; break;
                case 2: list = &members->direct_methods; break;
                case 3: list = &members->virtual_methods; break;
            }
            if (!state.Items(counts[group])) return false;
            list->clear();
            if (counts[group] > list->max_size()) return state.Fail(SmaliError::LimitExceeded);
            list->reserve(counts[group]);
        }
        uint32_t index = 0;
        for (uint32_t i = 0; i < counts[group]; ++i) {
            if (!members && !state.Items()) return false;
            uint32_t delta;
            Member current;
            if (!dex.Uleb(cursor, delta) || !dex.Uleb(cursor, current.access) ||
                (methods && !dex.Uleb(cursor, current.code_offset))) return false;
            if (index >= maximum || delta >= maximum - index || (i && !delta))
                return state.Fail(SmaliError::MalformedInput, cursor);
            current.index = index += delta;
            if (members || (methods && index == target)) {
                // The local id must really belong to this definition. No
                // cross-DEX canonical method/field cache participates here.
                uint16_t parent;
                size_t offset = methods ? size_t(header.method_ids_off) + index * sizeof(dex::MethodId)
                                        : size_t(header.field_ids_off) + index * sizeof(dex::FieldId);
                if (!dex.Object(offset, parent)) return false;
                if (parent != owner.class_idx) return state.Fail(SmaliError::MalformedInput, offset);
                if (methods) {
                    bool direct = current.access & (dex::kAccStatic | dex::kAccPrivate | dex::kAccConstructor);
                    if (direct != (group == 2)) return state.Fail(SmaliError::MalformedInput, cursor);
                    bool no_body = current.access & (dex::kAccAbstract | dex::kAccNative);
                    if (no_body != (current.code_offset == 0))
                        return state.Fail(SmaliError::MalformedInput, cursor);
                } else if (bool(current.access & dex::kAccStatic) != (group == 0)) {
                    return state.Fail(SmaliError::MalformedInput, cursor);
                }
                if (method) { *method = current; return true; }
                list->push_back(current);
            }
        }
    }
    return !method || state.Fail(SmaliError::NotDefined);
}
} // namespace

bool SelectMethod(CheckedDex& dex, const dex::ClassDef& owner, uint32_t method_id, Member& method) {
    if (method_id >= dex.header().method_ids_size)
        return dex.state().Fail(SmaliError::InvalidIdentity);
    return Members(dex, owner, method_id, &method, nullptr);
}

bool SelectClass(CheckedDex& dex, const dex::ClassDef& owner, ClassMembers& members) {
    ClassMembers selected;
    if (!Members(dex, owner, UINT32_MAX, nullptr, &selected)) return false;
    auto disjoint = [](const auto& a, const auto& b) {
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            if (a[i].index == b[j].index) return false;
            if (a[i].index < b[j].index) ++i; else ++j;
        }
        return true;
    };
    if (!disjoint(selected.static_fields, selected.instance_fields) ||
        !disjoint(selected.direct_methods, selected.virtual_methods))
        return dex.state().Fail(SmaliError::MalformedInput, owner.class_data_off);
    members = std::move(selected);
    return true;
}

bool SelectAnnotations(CheckedDex& dex, const dex::ClassDef& owner,
                       SmaliMemberKind kind, uint32_t member_id, AnnotationOffsets& offsets) {
    offsets = {};
    if (!owner.annotations_off) return true;
    auto& state = dex.state();
    state.phase = SmaliPhase::Read;
    dex::AnnotationsDirectoryItem directory;
    if (!dex.Data(owner.annotations_off, sizeof(directory), 4) ||
        !dex.Object(owner.annotations_off, directory)) return false;
    size_t cursor = size_t(owner.annotations_off) + sizeof(directory);
    const uint32_t counts[]{directory.fields_size, directory.methods_size, directory.parameters_size};
    for (unsigned group = 0; group < 3; ++group) {
        constexpr size_t width = sizeof(dex::MethodAnnotationsItem);
        if (counts[group] > (dex.size() - cursor) / width)
            return state.Fail(SmaliError::MalformedInput, cursor);
        size_t bytes = size_t(counts[group]) * width;
        if (!dex.Data(cursor, bytes, 4)) return false;
        bool relevant = (kind == SmaliMemberKind::Field && group == 0) ||
                        (kind == SmaliMemberKind::Method && group != 0);
        if (relevant) {
            uint32_t previous = 0;
            bool matched = false;
            for (uint32_t i = 0; i < counts[group]; ++i) {
                dex::MethodAnnotationsItem item;
                if (!state.Items() || !dex.Object(cursor + size_t(i) * width, item)) return false;
                if ((i && item.method_idx <= previous) || item.method_idx >=
                    (group == 0 ? dex.header().field_ids_size : dex.header().method_ids_size))
                    return state.Fail(SmaliError::MalformedInput, cursor + size_t(i) * width);
                previous = item.method_idx;
                if (item.method_idx == member_id) {
                    if (matched || !item.annotations_off) return state.Fail(SmaliError::MalformedInput, cursor);
                    matched = true;
                    (group == 2 ? offsets.parameters : offsets.annotations) = item.annotations_off;
                }
            }
        }
        cursor += bytes;
    }
    if (kind == SmaliMemberKind::Class) offsets.annotations = directory.class_annotations_off;
    return true;
}

bool CheckMetadataProfile(CheckedDex& dex) {
    uint32_t offset, count;
    // hiddenapi_class_data_item cannot yet be represented by this emitter.
    // Reject the input category explicitly instead of silently losing flags.
    return dex.MapSection(0xf000, offset, count, 1) &&
           (!count || dex.state().Fail(SmaliError::Unsupported, offset, 0xf000));
}

bool AnnotationDirectory::Init(CheckedDex& dex, const dex::ClassDef& owner) {
    *this = {};
    if (!owner.annotations_off) return true;
    auto& state = dex.state();
    dex::AnnotationsDirectoryItem directory;
    if (!dex.Data(owner.annotations_off, sizeof(directory), 4) ||
        !dex.Object(owner.annotations_off, directory)) return false;
    class_annotations_ = directory.class_annotations_off;
    counts_[0] = directory.fields_size;
    counts_[1] = directory.methods_size;
    counts_[2] = directory.parameters_size;
    size_t cursor = size_t(owner.annotations_off) + sizeof(directory);
    for (unsigned group = 0; group < 3; ++group) {
        constexpr size_t width = sizeof(dex::MethodAnnotationsItem);
        if (counts_[group] > (dex.size() - cursor) / width)
            return state.Fail(SmaliError::MalformedInput, cursor);
        size_t bytes = size_t(counts_[group]) * width;
        if (!dex.Data(cursor, bytes, 4) || !state.Items(counts_[group])) return false;
        offsets_[group] = cursor;
        uint32_t previous = 0;
        for (uint32_t i = 0; i < counts_[group]; ++i) {
            dex::MethodAnnotationsItem item;
            if (!dex.Object(cursor + size_t(i) * width, item)) return false;
            if ((i && item.method_idx <= previous) || !item.annotations_off ||
                item.method_idx >= (group == 0 ? dex.header().field_ids_size : dex.header().method_ids_size))
                return state.Fail(SmaliError::MalformedInput, cursor + size_t(i) * width);
            previous = item.method_idx;
        }
        cursor += bytes;
    }
    return true;
}

bool AnnotationDirectory::Find(CheckedDex& dex, SmaliMemberKind kind, uint32_t member_id,
                               AnnotationOffsets& offsets) const {
    offsets = {};
    if (kind == SmaliMemberKind::Class) { offsets.annotations = class_annotations_; return true; }
    for (unsigned group = kind == SmaliMemberKind::Field ? 0 : 1;
         group < (kind == SmaliMemberKind::Field ? 1U : 3U); ++group) {
        uint32_t low = 0, high = counts_[group];
        while (low < high) {
            uint32_t middle = low + (high - low) / 2;
            dex::MethodAnnotationsItem item;
            if (!dex.state().Items() ||
                !dex.Object(offsets_[group] + size_t(middle) * sizeof(item), item)) return false;
            if (item.method_idx == member_id) {
                (group == 2 ? offsets.parameters : offsets.annotations) = item.annotations_off;
                break;
            }
            if (item.method_idx < member_id) low = middle + 1; else high = middle;
        }
    }
    return true;
}

} // namespace dexkit::smali

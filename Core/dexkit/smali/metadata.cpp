// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "metadata.h"

namespace dexkit::smali {
namespace {
constexpr uint8_t kMethodType = 0x15;
constexpr uint8_t kMethodHandle = 0x16;
constexpr uint16_t kCallSiteSection = 0x0007;
}

bool Access(State& state, uint32_t flags, SmaliMemberKind kind) {
    struct Flag { uint32_t bit; const char* name; uint8_t kinds; };
    constexpr uint8_t c = 1, f = 2, m = 4;
    static constexpr Flag names[]{
        {0x1, "public ", c|f|m}, {0x2, "private ", f|m}, {0x4, "protected ", f|m},
        {0x8, "static ", f|m}, {0x10, "final ", c|f|m}, {0x20, "synchronized ", m},
        {0x40, "volatile ", f}, {0x40, "bridge ", m},
        {0x80, "transient ", f}, {0x80, "varargs ", m}, {0x100, "native ", m},
        {0x200, "interface ", c}, {0x400, "abstract ", c|m}, {0x800, "strictfp ", m},
        {0x1000, "synthetic ", c|f|m}, {0x2000, "annotation ", c}, {0x4000, "enum ", c|f},
        {0x10000, "constructor ", m}, {0x20000, "declared-synchronized ", m}
    };
    uint8_t applicable = kind == SmaliMemberKind::Class ? c : kind == SmaliMemberKind::Field ? f : m;
    for (auto flag : names) {
        if ((flag.kinds & applicable) && (flags & flag.bit)) {
            if (!state.Append(flag.name)) return false;
            flags &= ~flag.bit;
        }
    }
    return !flags || state.Fail(SmaliError::Unsupported, UINT64_MAX, flags);
}

bool Metadata::Index(dex::InstructionIndexType kind, uint32_t index) {
    std::string value;
    switch (kind) {
        case dex::kIndexStringRef: return dex_.Quoted(index);
        case dex::kIndexTypeRef: if (!dex_.Type(index, value)) return false; break;
        case dex::kIndexFieldRef: if (!dex_.Field(index, value)) return false; break;
        case dex::kIndexMethodRef:
        case dex::kIndexMethodAndProtoRef: if (!dex_.Method(index, value)) return false; break;
        case dex::kIndexProtoRef: if (!dex_.Proto(index, value)) return false; break;
        case dex::kIndexMethodHandleRef: return Handle(index);
        case dex::kIndexCallSiteRef: return CallSite(index);
        default: return state_.Fail(SmaliError::Unsupported, UINT64_MAX, kind);
    }
    return state_.Append(value);
}

bool Metadata::GetHandle(uint32_t index, dex::MethodHandle& handle) {
    if (dex_.version() < 38) return state_.Fail(SmaliError::MalformedInput);
    uint32_t offset, count;
    if (!dex_.MapSection(dex::kMethodHandleItem, offset, count, sizeof(handle)) ||
        !dex_.Entry(offset, count, index, handle)) return false;
    if (handle.unused || handle.unused2 || handle.method_handle_type > 8)
        return state_.Fail(SmaliError::MalformedInput, offset + size_t(index) * sizeof(handle));
    return true;
}

bool Metadata::Handle(uint32_t index) {
    dex::MethodHandle handle;
    if (!GetHandle(index, handle)) return false;
    static constexpr const char* names[]{"static-put", "static-get", "instance-put", "instance-get",
        "invoke-static", "invoke-instance", "invoke-constructor", "invoke-direct", "invoke-interface"};
    return state_.Append(names[handle.method_handle_type]) && state_.Append("@") &&
           Index(handle.method_handle_type <= 3 ? dex::kIndexFieldRef : dex::kIndexMethodRef,
                 handle.field_or_method_id);
}

bool Metadata::IndexedValue(size_t& cursor, uint8_t expected, uint32_t& index) {
    uint8_t header;
    if (!dex_.Data(cursor, 1) || !dex_.Object(cursor++, header)) return false;
    unsigned width = (header >> 5) + 1;
    if ((header & 31) != expected || width > 4)
        return state_.Fail(SmaliError::MalformedInput, cursor - 1, header);
    index = 0;
    if (!dex_.Data(cursor, width) || !dex_.Read(cursor, &index, width)) return false;
    cursor += width;
    return true;
}

bool Metadata::CallSite(uint32_t index) {
    if (dex_.version() < 38) return state_.Fail(SmaliError::MalformedInput);
    uint32_t offset, count, array_offset;
    if (!dex_.MapSection(kCallSiteSection, offset, count, 4) ||
        !dex_.Entry(offset, count, index, array_offset)) return false;
    size_t cursor = array_offset;
    uint32_t values, handle_index, name, proto;
    if (!dex_.Uleb(cursor, values)) return false;
    if (values < 3) return state_.Fail(SmaliError::MalformedInput, cursor);
    if (!state_.Items(values) || !IndexedValue(cursor, kMethodHandle, handle_index) ||
        !IndexedValue(cursor, dex::kEncodedString, name) || !IndexedValue(cursor, kMethodType, proto)) return false;
    dex::MethodHandle handle;
    if (!GetHandle(handle_index, handle)) return false;
    if (handle.method_handle_type != dex::METHOD_HANDLE_TYPE_INVOKE_STATIC)
        return state_.Fail(SmaliError::MalformedInput, array_offset, handle.method_handle_type);
    if (!state_.Append("call_site_") || !state_.Number(index) || !state_.Append("(") ||
        !dex_.Quoted(name) || !state_.Append(", ") || !Index(dex::kIndexProtoRef, proto)) return false;
    for (uint32_t i = 3; i < values; ++i) {
        if (!state_.Append(", ") || !Value(cursor)) return false;
    }
    return state_.Append(")@") && Index(dex::kIndexMethodRef, handle.field_or_method_id);
}

bool Metadata::Floating(uint64_t bits, bool wide) {
    const unsigned fraction_bits = wide ? 52 : 23;
    const unsigned exponent_max = wide ? 2047 : 255;
    const uint64_t fraction_mask = (uint64_t(1) << fraction_bits) - 1;
    uint64_t fraction = bits & fraction_mask;
    unsigned exponent = unsigned((bits >> fraction_bits) & exponent_max);
    bool negative = (bits >> (wide ? 63 : 31)) != 0;
    if (exponent == exponent_max && fraction) {
        // smali's NaN literal canonicalizes the payload/sign. Refuse other bit
        // patterns rather than silently replacing an encoded value.
        if (negative || fraction != (uint64_t(1) << (fraction_bits - 1)))
            return state_.Fail(SmaliError::Unsupported);
        return state_.Append(wide ? "NaN" : "NaNf");
    }
    if (negative && !state_.Append("-")) return false;
    if (exponent == exponent_max) return state_.Append(wide ? "Infinity" : "Infinityf");
    if (!state_.Append(exponent ? "0x1." : "0x0.")) return false;
    if (!wide) fraction <<= 1; // Six hex digits represent 24 fraction bits.
    unsigned digits = wide ? 13 : 6;
    static constexpr char hex[] = "0123456789abcdef";
    char buffer[13];
    for (unsigned i = 0; i < digits; ++i) buffer[digits - 1 - i] = hex[(fraction >> (i * 4)) & 15];
    return state_.Append({buffer, digits}) && state_.Append("p") &&
           state_.Number((exponent ? int(exponent) : 1) - (wide ? 1023 : 127)) &&
           state_.Append(wide ? "" : "f");
}

bool Metadata::Value(size_t& cursor, uint32_t depth, uint8_t* value_type) {
    if (depth > state_.options.max_annotation_depth) return state_.Fail(SmaliError::LimitExceeded, cursor);
    if (!state_.Items()) return false;
    uint8_t header;
    if (!dex_.Data(cursor, 1) || !dex_.Object(cursor++, header)) return false;
    uint8_t type = header & 31;
    unsigned arg = header >> 5, width = arg + 1;
    if (value_type) *value_type = type;
    unsigned maximum = 0;
    switch (type) {
        case dex::kEncodedByte: maximum = 1; break;
        case dex::kEncodedShort: case dex::kEncodedChar: maximum = 2; break;
        case dex::kEncodedInt: case dex::kEncodedFloat: case dex::kEncodedString:
        case dex::kEncodedType: case dex::kEncodedField: case dex::kEncodedMethod:
        case dex::kEncodedEnum: maximum = 4; break;
        case dex::kEncodedLong: case dex::kEncodedDouble: maximum = 8; break;
        case kMethodType: case kMethodHandle:
            if (dex_.version() < 38) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
            maximum = 4; break;
        case dex::kEncodedBoolean:
            if (arg > 1) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
            return state_.Append(arg ? "true" : "false");
        case dex::kEncodedNull:
            if (arg) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
            return state_.Append("null");
        case dex::kEncodedArray: {
            if (arg) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
            uint32_t count;
            if (!dex_.Uleb(cursor, count) || !state_.Append("{")) return false;
            if (count > state_.options.max_items - state_.items)
                return state_.Fail(SmaliError::LimitExceeded, cursor);
            for (uint32_t i = 0; i < count; ++i)
                if ((i && !state_.Append(", ")) || !Value(cursor, depth + 1)) return false;
            return state_.Append("}");
        }
        case dex::kEncodedAnnotation:
            if (arg) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
            return state_.Append(".subannotation ") && Annotation(cursor, depth + 1, true);
        default: return state_.Fail(SmaliError::Unsupported, cursor - 1, type);
    }
    if (width > maximum) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
    uint64_t bits = 0;
    if (!dex_.Data(cursor, width) || !dex_.Read(cursor, &bits, width)) return false;
    cursor += width;
    switch (type) {
        case dex::kEncodedByte: case dex::kEncodedShort: case dex::kEncodedInt: case dex::kEncodedLong: {
            if (width < 8 && (bits & (uint64_t(1) << (width * 8 - 1)))) bits |= UINT64_MAX << (width * 8);
            if (!state_.Number(static_cast<int64_t>(bits))) return false;
            return state_.Append(type == dex::kEncodedByte ? "t" : type == dex::kEncodedShort ? "s" :
                                 type == dex::kEncodedLong ? "L" : "");
        }
        case dex::kEncodedChar: {
            static constexpr char hex[] = "0123456789abcdef";
            char literal[]{'\'', '\\', 'u', hex[(bits >> 12) & 15], hex[(bits >> 8) & 15],
                           hex[(bits >> 4) & 15], hex[bits & 15], '\''};
            return state_.Append({literal, sizeof(literal)});
        }
        case dex::kEncodedFloat: return Floating(bits << ((4 - width) * 8), false);
        case dex::kEncodedDouble: return Floating(bits << ((8 - width) * 8), true);
        case dex::kEncodedString: return Index(dex::kIndexStringRef, bits);
        case dex::kEncodedType: {
            std::string type_name;
            return dex_.Type(bits, type_name, true) && state_.Append(type_name);
        }
        case dex::kEncodedField: return Index(dex::kIndexFieldRef, bits);
        case dex::kEncodedMethod: return Index(dex::kIndexMethodRef, bits);
        case dex::kEncodedEnum: return state_.Append(".enum ") && Index(dex::kIndexFieldRef, bits);
        case kMethodType: return Index(dex::kIndexProtoRef, bits);
        case kMethodHandle: return Handle(bits);
        default: return state_.Fail(SmaliError::InternalError);
    }
}

bool Metadata::Annotation(size_t& cursor, uint32_t depth, bool nested) {
    if (depth > state_.options.max_annotation_depth) return state_.Fail(SmaliError::LimitExceeded, cursor);
    uint32_t type, count;
    std::string type_name;
    if (!dex_.Uleb(cursor, type) || !dex_.Type(type, type_name) || !dex_.Uleb(cursor, count)) return false;
    if (type_name.front() != 'L') return state_.Fail(SmaliError::MalformedInput, cursor);
    if (!state_.Items(count) || !state_.Append(type_name) || !state_.Append("\n")) return false;
    uint32_t previous = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t index;
        std::string name;
        if (!dex_.Uleb(cursor, index)) return false;
        if (i && index <= previous) return state_.Fail(SmaliError::MalformedInput, cursor);
        previous = index;
        if (!dex_.Name(index, name) || !state_.Append(name) || !state_.Append(" = ") ||
            !Value(cursor, depth) || !state_.Append("\n")) return false;
    }
    return state_.Append(nested ? ".end subannotation" : ".end annotation\n");
}

bool Metadata::AnnotationSet(uint32_t offset) {
    if (!offset) return true;
    uint32_t count;
    if (!dex_.Data(offset, 4, 4) || !dex_.Object(offset, count)) return false;
    size_t cursor = size_t(offset) + 4;
    if (count > (dex_.size() - cursor) / 4) return state_.Fail(SmaliError::MalformedInput, cursor);
    if (!dex_.Data(cursor, size_t(count) * 4) || !state_.Items(count)) return false;
    uint32_t previous_type = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t annotation_offset;
        if (!dex_.Object(cursor + size_t(i) * 4, annotation_offset)) return false;
        uint8_t visibility;
        if (!dex_.Data(annotation_offset, 1) || !dex_.Object(annotation_offset, visibility)) return false;
        if (visibility > dex::kVisibilitySystem) return state_.Fail(SmaliError::MalformedInput, annotation_offset);
        size_t item = size_t(annotation_offset) + 1, check = item;
        uint32_t type;
        if (!dex_.Uleb(check, type)) return false;
        if (i && type <= previous_type) return state_.Fail(SmaliError::MalformedInput, item);
        previous_type = type;
        static constexpr const char* names[]{"build ", "runtime ", "system "};
        if (!state_.Append(".annotation ") || !state_.Append(names[visibility]) ||
            !Annotation(item, 0, false)) return false;
    }
    return true;
}

} // namespace dexkit::smali

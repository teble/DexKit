// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "checked_dex.h"

#include <charconv>
#include <bit>
#include <limits>

namespace dexkit::smali {

bool State::Fail(SmaliError error, uint64_t offset, uint32_t detail) {
    if (status.ok()) {
        const auto dex_id = status.dex_id;
        status = {error, phase, offset, code_offset, detail};
        status.dex_id = dex_id;
        status.member_kind = member_kind;
        status.member_id = member_id;
    }
    return false;
}

bool State::Input(size_t count, size_t offset) {
    if (!status.ok()) return false;
    if (count > options.max_input_bytes - input_bytes)
        return Fail(SmaliError::LimitExceeded, offset);
    input_bytes += count;
    return true;
}

bool State::Items(size_t count) {
    if (!status.ok()) return false;
    if (count > options.max_items - items) return Fail(SmaliError::LimitExceeded);
    items += count;
    return true;
}

bool State::Append(std::string_view text) {
    if (!status.ok()) return false;
    if (text.size() > options.max_output_bytes - output.size() || text.size() > output.max_size() - output.size())
        return Fail(SmaliError::LimitExceeded);
    output.append(text);
    return true;
}

bool State::TemporaryAppend(std::string& temporary, std::string_view text) {
    if (!status.ok()) return false;
    size_t remaining = options.max_output_bytes - output.size();
    if (temporary.size() > remaining || text.size() > remaining - temporary.size() ||
        text.size() > temporary.max_size() - temporary.size())
        return Fail(SmaliError::LimitExceeded);
    temporary.append(text);
    return true;
}

bool State::Number(int64_t value) {
    char buffer[24];
    auto end = std::to_chars(buffer, buffer + sizeof(buffer), value).ptr;
    return Append({buffer, size_t(end - buffer)});
}

bool State::Hex(uint64_t value, bool wide) {
    char buffer[16];
    auto end = std::to_chars(buffer, buffer + sizeof(buffer), value, 16).ptr;
    return Append("0x") && Append({buffer, size_t(end - buffer)}) && (!wide || Append("L"));
}

bool CheckedDex::Range(size_t offset, size_t bytes, size_t alignment) {
    if (!state_.status.ok()) return false;
    if (!alignment) return state_.Fail(SmaliError::InternalError, offset);
    if (offset % alignment || offset > image_.size() || bytes > image_.size() - offset)
        return state_.Fail(SmaliError::MalformedInput, offset);
    return true;
}

bool CheckedDex::Data(size_t offset, size_t bytes, size_t alignment) {
    if (offset < data_begin_ || offset > data_end_ || bytes > data_end_ - offset)
        return state_.Fail(SmaliError::MalformedInput, offset);
    return Range(offset, bytes, alignment);
}

bool CheckedDex::Read(size_t offset, void* output, size_t bytes) {
    if (!Range(offset, bytes) || !state_.Input(bytes, offset)) return false;
    std::memcpy(output, image_.data() + offset, bytes);
    return true;
}

bool CheckedDex::Table(size_t offset, uint32_t count, size_t width) {
    if (!width) return state_.Fail(SmaliError::InternalError, offset);
    if (!Range(offset, 0, 4)) return false;
    if (count > (image_.size() - offset) / width || (count && offset < header_offset_ + header_.header_size))
        return state_.Fail(SmaliError::MalformedInput, offset);
    return true;
}

bool CheckedDex::Init() {
    if constexpr (std::endian::native != std::endian::little)
        return state_.Fail(SmaliError::Unsupported);
    if (!Range(header_offset_, dex::Header::kV40Size, 4) ||
        !Read(header_offset_, &header_, dex::Header::kV40Size)) return false;
    const auto* magic = header_.magic;
    if (std::memcmp(magic, "dex\n", 4) || magic[7] || magic[4] != '0' ||
        magic[5] < '0' || magic[5] > '9' || magic[6] < '0' || magic[6] > '9')
        return state_.Fail(SmaliError::Unsupported, header_offset_);
    version_ = (magic[5] - '0') * 10 + magic[6] - '0';
    if (version_ != 35 && (version_ < 37 || version_ > 41))
        return state_.Fail(SmaliError::Unsupported, header_offset_, version_);
    if (header_.endian_tag != dex::kEndianConstant)
        return state_.Fail(SmaliError::Unsupported, header_offset_ + 40);
    const size_t expected_header = version_ == 41 ? dex::Header::kV41Size : dex::Header::kV40Size;
    if (header_.header_size != expected_header || header_.file_size < expected_header ||
        !Range(header_offset_, header_.file_size))
        return state_.Fail(SmaliError::MalformedInput, header_offset_);
    if (version_ == 41) {
        if (!Read(header_offset_ + dex::Header::kV40Size,
                  reinterpret_cast<uint8_t*>(&header_) + dex::Header::kV40Size, 8)) return false;
        if (header_.ContainerOff() != header_offset_ || header_.ContainerSize() > image_.size() ||
            header_offset_ > header_.ContainerSize() ||
            header_.file_size > header_.ContainerSize() - header_offset_)
            return state_.Fail(SmaliError::MalformedInput, header_offset_);
        image_ = image_.first(header_.ContainerSize());
        data_begin_ = header_offset_ + expected_header;
        data_end_ = image_.size();
    } else {
        if (header_offset_ != 0) return state_.Fail(SmaliError::MalformedInput, header_offset_);
        image_ = image_.first(header_.file_size);
        if (header_.data_off < expected_header || !Range(header_.data_off, header_.data_size, 4))
            return state_.Fail(SmaliError::MalformedInput, header_.data_off);
        data_begin_ = header_.data_off;
        data_end_ = data_begin_ + header_.data_size;
    }
    if (header_.link_size) return state_.Fail(SmaliError::Unsupported, header_.link_off);
    return Table(header_.string_ids_off, header_.string_ids_size, sizeof(dex::StringId)) &&
           Table(header_.type_ids_off, header_.type_ids_size, sizeof(dex::TypeId)) &&
           Table(header_.proto_ids_off, header_.proto_ids_size, sizeof(dex::ProtoId)) &&
           Table(header_.field_ids_off, header_.field_ids_size, sizeof(dex::FieldId)) &&
           Table(header_.method_ids_off, header_.method_ids_size, sizeof(dex::MethodId)) &&
           Table(header_.class_defs_off, header_.class_defs_size, sizeof(dex::ClassDef));
}

bool CheckedDex::Uleb(size_t& cursor, uint32_t& value) {
    value = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t byte;
        if (!Data(cursor, 1) || !Object(cursor, byte)) return false;
        ++cursor;
        if (i == 4 && (byte & 0xf0)) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
        value |= uint32_t(byte & 0x7f) << (i * 7);
        if (!(byte & 0x80)) return true;
    }
    return state_.Fail(SmaliError::MalformedInput, cursor);
}

bool CheckedDex::Sleb(size_t& cursor, int32_t& value) {
    uint32_t bits = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t byte;
        if (!Data(cursor, 1) || !Object(cursor, byte)) return false;
        ++cursor;
        if (i == 4 && ((byte & 0xf8) != 0 && (byte & 0xf8) != 0x78))
            return state_.Fail(SmaliError::MalformedInput, cursor - 1);
        bits |= uint32_t(byte & 0x7f) << (i * 7);
        if (!(byte & 0x80)) {
            if (i < 4 && (byte & 0x40)) bits |= UINT32_MAX << (7 * (i + 1));
            value = static_cast<int32_t>(bits);
            return true;
        }
    }
    return state_.Fail(SmaliError::MalformedInput, cursor);
}

bool CheckedDex::String(uint32_t index, std::u16string& value) {
    dex::StringId entry;
    if (!Entry(header_.string_ids_off, header_.string_ids_size, index, entry)) return false;
    size_t cursor = entry.string_data_off;
    uint32_t length;
    if (!Uleb(cursor, length)) return false;
    if (length > state_.options.max_input_bytes - state_.input_bytes ||
        length > state_.options.max_output_bytes - state_.output.size() || length > value.max_size())
        return state_.Fail(SmaliError::LimitExceeded, cursor);
    if (!Data(cursor, length)) return false;
    value.clear();
    value.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
        uint8_t first;
        if (!Data(cursor, 1) || !Object(cursor++, first)) return false;
        if (first == 0) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
        uint16_t unit = first;
        if (first >= 0x80) {
            unsigned remaining;
            if ((first & 0xe0) == 0xc0) { remaining = 1; unit &= 0x1f; }
            else if ((first & 0xf0) == 0xe0) { remaining = 2; unit &= 0x0f; }
            else return state_.Fail(SmaliError::MalformedInput, cursor - 1);
            for (unsigned j = 0; j < remaining; ++j) {
                uint8_t next;
                if (!Data(cursor, 1) || !Object(cursor++, next)) return false;
                if ((next & 0xc0) != 0x80) return state_.Fail(SmaliError::MalformedInput, cursor - 1);
                unit = (unit << 6) | (next & 0x3f);
            }
            if ((remaining == 1 && unit < 0x80 && unit != 0) || (remaining == 2 && unit < 0x800))
                return state_.Fail(SmaliError::MalformedInput, cursor - 1);
        }
        value.push_back(static_cast<char16_t>(unit));
    }
    uint8_t terminator;
    if (!Data(cursor, 1) || !Object(cursor, terminator)) return false;
    return terminator == 0 || state_.Fail(SmaliError::MalformedInput, cursor);
}

bool Quote(State& state, const std::u16string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    if (!state.Append("\"")) return false;
    for (uint16_t unit : value) {
        if (unit == '"' || unit == '\\') {
            char escaped[]{'\\', char(unit)};
            if (!state.Append({escaped, 2})) return false;
        } else if (unit >= 0x20 && unit <= 0x7e) {
            char byte = char(unit);
            if (!state.Append({&byte, 1})) return false;
        } else {
            char escaped[]{'\\', 'u', hex[unit >> 12], hex[(unit >> 8) & 15],
                           hex[(unit >> 4) & 15], hex[unit & 15]};
            if (!state.Append({escaped, 6})) return false;
        }
    }
    return state.Append("\"");
}

bool CheckedDex::Quoted(uint32_t index) {
    std::u16string value;
    return String(index, value) && Quote(state_, value);
}

bool CheckedDex::Identifier(const std::u16string& value, std::string& output, bool descriptor) {
    output.clear();
    if (value.empty()) return state_.Fail(SmaliError::Unsupported);
    for (size_t i = 0; i < value.size(); ++i) {
        uint32_t cp = value[i];
        // Conservative unquoted grammar. Extended names may be valid DEX, but
        // are rejected instead of emitting text with a different parse.
        if (cp < 0x80) {
            bool simple = (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
                          (cp >= '0' && cp <= '9') || cp == '$' || cp == '_' || cp == '-';
            if (!simple && !(descriptor && (cp == '/' || cp == '[' || cp == ';')))
                return state_.Fail(SmaliError::Unsupported, UINT64_MAX, cp);
            char byte = char(cp);
            if (!state_.TemporaryAppend(output, {&byte, 1})) return false;
        } else {
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (++i >= value.size() || value[i] < 0xdc00 || value[i] > 0xdfff)
                    return state_.Fail(SmaliError::Unsupported);
                cp = 0x10000 + ((cp - 0xd800) << 10) + value[i] - 0xdc00;
            } else if (cp >= 0xdc00 && cp <= 0xdfff) return state_.Fail(SmaliError::Unsupported);
            if (cp < 0xa1 || (cp >= 0x2000 && cp <= 0x200f) ||
                (cp >= 0x2028 && cp <= 0x202f) || cp == 0x3000 ||
                (cp >= 0xfff0 && cp <= 0xffff))
                return state_.Fail(SmaliError::Unsupported, UINT64_MAX, cp);
            char bytes[4];
            size_t length = 0;
            if (cp < 0x800) bytes[length++] = char(0xc0 | (cp >> 6));
            else if (cp < 0x10000) {
                bytes[length++] = char(0xe0 | (cp >> 12));
                bytes[length++] = char(0x80 | ((cp >> 6) & 63));
            } else {
                bytes[length++] = char(0xf0 | (cp >> 18));
                bytes[length++] = char(0x80 | ((cp >> 12) & 63));
                bytes[length++] = char(0x80 | ((cp >> 6) & 63));
            }
            bytes[length++] = char(0x80 | (cp & 63));
            if (!state_.TemporaryAppend(output, {bytes, length})) return false;
        }
    }
    return true;
}

bool CheckedDex::Name(uint32_t index, std::string& name, bool method) {
    std::u16string value;
    if (!String(index, value)) return false;
    if (method && (value == u"<init>" || value == u"<clinit>")) {
        name.clear();
        return state_.TemporaryAppend(name, value == u"<init>" ? "<init>" : "<clinit>");
    }
    return Identifier(value, name, false);
}

bool CheckedDex::Type(uint32_t index, std::string& descriptor, bool allow_void) {
    dex::TypeId entry;
    std::u16string value;
    if (!Entry(header_.type_ids_off, header_.type_ids_size, index, entry) ||
        !String(entry.descriptor_idx, value) || !Identifier(value, descriptor, true)) return false;
    size_t base = descriptor.find_first_not_of('[');
    if (base == std::string::npos || base > 255) return state_.Fail(SmaliError::MalformedInput);
    char kind = descriptor[base];
    if (kind == 'L') {
        if (descriptor.size() < base + 3 || descriptor.back() != ';')
            return state_.Fail(SmaliError::MalformedInput);
        bool component = false;
        for (size_t i = base + 1; i + 1 < descriptor.size(); ++i) {
            char c = descriptor[i];
            if (c == '[' || c == ';' || (c == '/' && !component))
                return state_.Fail(SmaliError::MalformedInput);
            component = c != '/';
        }
        if (!component) return state_.Fail(SmaliError::MalformedInput);
        return true;
    }
    if (descriptor.size() != base + 1 ||
        (std::string_view("ZBSCIJFD").find(kind) == std::string_view::npos &&
         !(allow_void && base == 0 && kind == 'V')))
        return state_.Fail(SmaliError::MalformedInput);
    return true;
}

bool CheckedDex::TypeList(uint32_t offset, std::vector<uint16_t>& types) {
    types.clear();
    if (offset == 0) return true;
    uint32_t count;
    if (!Data(offset, 4, 4) || !Object(offset, count)) return false;
    if (count > (data_end_ - offset - 4) / 2)
        return state_.Fail(SmaliError::MalformedInput, offset);
    if (!state_.Items(count) || !state_.Input(size_t(count) * 2, offset + 4)) return false;
    if (count > types.max_size()) return state_.Fail(SmaliError::LimitExceeded, offset);
    types.resize(count);
    if (count) std::memcpy(types.data(), image_.data() + offset + 4, size_t(count) * 2);
    for (auto type : types) if (type >= header_.type_ids_size)
        return state_.Fail(SmaliError::MalformedInput, offset, type);
    return true;
}

bool CheckedDex::Proto(uint32_t index, std::string& signature, std::vector<uint16_t>* parameters) {
    dex::ProtoId entry;
    std::vector<uint16_t> local;
    auto& params = parameters ? *parameters : local;
    if (!Entry(header_.proto_ids_off, header_.proto_ids_size, index, entry) ||
        !TypeList(entry.parameters_off, params)) return false;
    signature.clear();
    if (!state_.TemporaryAppend(signature, "(")) return false;
    std::string type;
    for (auto parameter : params) {
        if (!Type(parameter, type)) return false;
        if (!state_.TemporaryAppend(signature, type)) return false;
    }
    if (!Type(entry.return_type_idx, type, true)) return false;
    return state_.TemporaryAppend(signature, ")") && state_.TemporaryAppend(signature, type);
}

bool CheckedDex::Field(uint32_t index, std::string& descriptor, bool qualified) {
    dex::FieldId entry;
    std::string owner, name, type;
    if (!Entry(header_.field_ids_off, header_.field_ids_size, index, entry) ||
        !Type(entry.class_idx, owner) || !Name(entry.name_idx, name) || !Type(entry.type_idx, type)) return false;
    if (owner.front() != 'L') return state_.Fail(SmaliError::MalformedInput);
    descriptor.clear();
    return (!qualified || (state_.TemporaryAppend(descriptor, owner) && state_.TemporaryAppend(descriptor, "->"))) &&
           state_.TemporaryAppend(descriptor, name) && state_.TemporaryAppend(descriptor, ":") &&
           state_.TemporaryAppend(descriptor, type);
}

bool CheckedDex::Method(uint32_t index, std::string& descriptor, bool qualified) {
    dex::MethodId entry;
    std::string owner, name, proto;
    if (!Entry(header_.method_ids_off, header_.method_ids_size, index, entry) ||
        !Type(entry.class_idx, owner) || !Name(entry.name_idx, name, true) || !Proto(entry.proto_idx, proto)) return false;
    if (owner.front() != 'L' && owner.front() != '[') return state_.Fail(SmaliError::MalformedInput);
    descriptor.clear();
    return (!qualified || (state_.TemporaryAppend(descriptor, owner) && state_.TemporaryAppend(descriptor, "->"))) &&
           state_.TemporaryAppend(descriptor, name) && state_.TemporaryAppend(descriptor, proto);
}

bool CheckedDex::ClassDef(uint32_t index, dex::ClassDef& definition) {
    return Entry(header_.class_defs_off, header_.class_defs_size, index, definition);
}

bool CheckedDex::MapSection(uint16_t type, uint32_t& offset, uint32_t& count, size_t width) {
    offset = count = 0;
    if (!width) return state_.Fail(SmaliError::InternalError);
    uint32_t entries;
    if (!Data(header_.map_off, 4, 4) || !Object(header_.map_off, entries)) return false;
    size_t cursor = size_t(header_.map_off) + 4;
    if (entries > (image_.size() - cursor) / sizeof(dex::MapItem))
        return state_.Fail(SmaliError::MalformedInput, header_.map_off);
    if (!Data(cursor, size_t(entries) * sizeof(dex::MapItem), 4)) return false;
    if (!state_.Items(entries)) return false;
    bool found = false;
    for (uint32_t i = 0; i < entries; ++i, cursor += sizeof(dex::MapItem)) {
        dex::MapItem entry;
        if (!Object(cursor, entry)) return false;
        if (entry.type == type) {
            if (found || entry.unused || !Table(entry.offset, entry.size, width))
                return state_.Fail(SmaliError::MalformedInput, cursor);
            found = true;
            offset = entry.offset;
            count = entry.size;
        }
    }
    return true;
}

} // namespace dexkit::smali

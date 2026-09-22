// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstring>
#include <span>
#include <string_view>
#include <vector>
#include <unordered_map>

#include "smali.h"
#include "slicer/dex_format.h"

namespace dexkit::smali {

// All state is owned by one request. Source bytes are borrowed under the
// bridge read lock and query guard; this class never changes the shared Reader.
struct State {
    const SmaliOptions options;
    SmaliStatus status;
    SmaliPhase phase = SmaliPhase::Resolve;
    uint32_t code_offset = UINT32_MAX;
    SmaliMemberKind member_kind = SmaliMemberKind::None;
    uint32_t member_id = UINT32_MAX;
    size_t input_bytes = 0;
    size_t items = 0;
    std::string output;
    // smali has no identity syntax for equal-content method handles. Keep this
    // at request scope so conflicts across instructions/members are detected.
    std::unordered_map<uint32_t, uint32_t> handle_ids;

    bool Fail(SmaliError error, uint64_t offset = UINT64_MAX, uint32_t detail = 0);
    bool Input(size_t count, size_t offset);
    bool Items(size_t count = 1);
    bool Append(std::string_view text);
    bool TemporaryAppend(std::string& temporary, std::string_view text);
    bool Number(int64_t value);
    bool Hex(uint64_t value, bool wide = false);
};

class CheckedDex {
public:
    CheckedDex(std::span<const uint8_t> image, size_t header_offset, State& state)
        : image_(image), header_offset_(header_offset), state_(state) {}

    bool Init();
    bool Range(size_t offset, size_t bytes, size_t alignment = 1);
    bool Data(size_t offset, size_t bytes, size_t alignment = 1);
    bool Read(size_t offset, void* output, size_t bytes);

    template<class T> bool Object(size_t offset, T& output) {
        return Read(offset, &output, sizeof(T));
    }
    template<class T> bool Entry(size_t offset, uint32_t count, uint32_t index, T& output) {
        if (!Range(offset, 0)) return false;
        if (count > (image_.size() - offset) / sizeof(T))
            return state_.Fail(SmaliError::MalformedInput, offset);
        if (index >= count) return state_.Fail(SmaliError::MalformedInput, offset, index);
        // Self-contained protection also covers newly discovered map tables.
        return Object(offset + size_t(index) * sizeof(T), output);
    }

    bool Uleb(size_t& cursor, uint32_t& value);
    bool Sleb(size_t& cursor, int32_t& value);
    bool String(uint32_t index, std::u16string& value);
    bool Type(uint32_t index, std::string& descriptor, bool allow_void = false);
    bool Name(uint32_t index, std::string& name, bool method = false);
    bool Quoted(uint32_t index);
    bool TypeList(uint32_t offset, std::vector<uint16_t>& types);
    bool Proto(uint32_t index, std::string& signature, std::vector<uint16_t>* parameters = nullptr);
    bool Field(uint32_t index, std::string& descriptor, bool qualified = true);
    bool Method(uint32_t index, std::string& descriptor, bool qualified = true);
    bool ClassDef(uint32_t index, dex::ClassDef& definition);
    bool MapSection(uint16_t type, uint32_t& offset, uint32_t& count, size_t width);

    const dex::Header& header() const { return header_; }
    uint32_t version() const { return version_; }
    size_t size() const { return image_.size(); }
    State& state() { return state_; }

private:
    bool Table(size_t offset, uint32_t count, size_t width);
    bool Identifier(const std::u16string& value, std::string& output, bool descriptor);
    std::span<const uint8_t> image_;
    size_t header_offset_;
    size_t data_begin_ = 0;
    size_t data_end_ = 0;
    dex::Header header_{};
    uint32_t version_ = 0;
    State& state_;
};

// Body/metadata emitters share this encoding helper. Literals are ASCII escaped;
// identifiers are separately validated and encoded as ordinary UTF-8.
bool Quote(State& state, const std::u16string& value);

} // namespace dexkit::smali

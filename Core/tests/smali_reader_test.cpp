// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "../dexkit/smali/checked_dex.h"
#include "../dexkit/smali/selection.h"
#include "slicer/dex_bytecode.h"

#include <cstdlib>
#include <iostream>

namespace {
using namespace dexkit;
using namespace dexkit::smali;
int checks = 0;
void Check(bool condition) {
    ++checks;
    if (!condition) { std::cerr << "Failed smali check " << checks << '\n'; std::abort(); }
}

void OpcodeNames() {
    static constexpr const char* expected[]{
#define NAME(o, c, pname, f, i, a, e, v) pname,
#include "slicer/dex_instruction_list.h"
        DEX_INSTRUCTION_LIST(NAME)
#undef DEX_INSTRUCTION_LIST
#undef NAME
    };
    static_assert(std::size(expected) == dex::kNumPackedOpcodes);
    for (size_t i = 0; i < std::size(expected); ++i)
        Check(std::strcmp(expected[i], dex::GetOpcodeName(static_cast<dex::Opcode>(i))) == 0);
}

struct Fixture {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(512);
    dex::Header header{};
    size_t header_offset = 0;
    size_t string_offset = 128;

    explicit Fixture(bool container = false) {
        std::memcpy(header.magic, container ? "dex\n041" : "dex\n035", 8);
        header.header_size = container ? 120 : 112;
        header.file_size = container ? 160 : bytes.size();
        header.endian_tag = dex::kEndianConstant;
        header_offset = container ? 128 : 0;
        header.SetContainer(header_offset, bytes.size());
        header.string_ids_size = 1;
        header.string_ids_off = container ? 248 : 112;
        header.type_ids_size = 1;
        header.type_ids_off = header.string_ids_off + 4;
        header.data_off = 128;
        header.data_size = bytes.size() - header.data_off;
        string_offset = container ? 400 : 128; // Past this logical DEX's end.
        Sync();
        uint32_t str_off = string_offset;
        Put(header.string_ids_off, str_off);
        String({1, 'I', 0});
    }
    template<class T> void Put(size_t offset, T value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); }
    void Sync() { std::memcpy(bytes.data() + header_offset, &header, header.header_size); }
    void String(std::initializer_list<uint8_t> value) {
        std::copy(value.begin(), value.end(), bytes.begin() + string_offset);
    }
};

void StringsAndContainer() {
    for (bool container : {false, true}) {
        Fixture fixture(container);
        SmaliOptions options;
        State state{options};
        CheckedDex dex(fixture.bytes, fixture.header_offset, state);
        Check(dex.Init());
        std::string type;
        Check(dex.Type(0, type) && type == "I");
        // Embedded NUL and an unpaired UTF-16 surrogate survive string escaping.
        fixture.String({3, 'x', 0xc0, 0x80, 0xed, 0xa0, 0x80, 0});
        Check(dex.Quoted(0));
        Check(state.output == "\"x\\u0000\\ud800\"");
    }
    Fixture fixture;
    fixture.String({2, 0xed, 0xa0, 0xbd, 0xed, 0xb8, 0x80, 0});
    SmaliOptions options;
    State state{options};
    CheckedDex dex(fixture.bytes, 0, state);
    Check(dex.Init());
    std::string name;
    Check(!dex.Name(0, name) && state.status.error == SmaliError::Unsupported);
}

void MalformedStrings() {
    for (auto raw : {std::vector<uint8_t>{1, 0xc1, 0x81, 0}, // Overlong ASCII.
                     {1, 0xe0, 0x81, 0x81, 0},             // Overlong 3-byte.
                     {1, 0xf0, 0x90, 0x80, 0x80, 0},       // DEX uses MUTF-8.
                     {1, 'I', 'X', 0},                     // Length mismatch.
                     {2, 'I', 0},                          // Premature NUL.
                     {1, 0xc2, 'X', 0},                    // Bad continuation.
                     {0xff, 0xff, 0xff, 0xff, 0x1f}}) {    // ULEB overflow.
        Fixture fixture;
        std::copy(raw.begin(), raw.end(), fixture.bytes.begin() + 128);
        SmaliOptions options;
        State state{options};
        CheckedDex dex(fixture.bytes, 0, state);
        Check(dex.Init());
        std::u16string value;
        Check(!dex.String(0, value));
        Check(state.status.error == SmaliError::MalformedInput);
    }
}

void BoundsAndLeb() {
    Fixture fixture;
    SmaliOptions options;
    State state{options};
    CheckedDex dex(fixture.bytes, 0, state);
    Check(dex.Init());
    Check(dex.Range(512, 0));
    Check(!dex.Range(SIZE_MAX, 4));
    Check(state.status.error == SmaliError::MalformedInput);
    auto first = state.status;
    state.Fail(SmaliError::Unsupported);
    Check(state.status.error == first.error && state.status.dex_offset == first.dex_offset);

    for (bool signed_value : {false, true}) {
        for (auto raw : {std::vector<uint8_t>{0xff, 0xff, 0xff, 0xff, 0x80},
                         {0x80, 0x80, 0x80, 0x80, 0x10}}) {
            State current{options};
            CheckedDex reader(fixture.bytes, 0, current);
            Check(reader.Init());
            std::copy(raw.begin(), raw.end(), fixture.bytes.begin() + 128);
            size_t cursor = 128;
            uint32_t u; int32_t s;
            Check(!(signed_value ? reader.Sleb(cursor, s) : reader.Uleb(cursor, u)));
        }
    }
    fixture.header.string_ids_size = UINT32_MAX;
    fixture.Sync();
    State invalid{options};
    CheckedDex large(fixture.bytes, 0, invalid);
    Check(!large.Init());
    Check(invalid.status.error == SmaliError::MalformedInput);
}

void LimitsAndNames() {
    Fixture fixture;
    SmaliOptions options;
    options.max_output_bytes = 3;
    State state{options};
    CheckedDex dex(fixture.bytes, 0, state);
    Check(dex.Init());
    Check(state.Append("abc"));
    Check(!state.Append("d"));
    Check(state.status.error == SmaliError::LimitExceeded && state.output == "abc");
    options.max_input_bytes = 111;
    State limited{options};
    CheckedDex limited_reader(fixture.bytes, 0, limited);
    Check(!limited_reader.Init() && limited.status.error == SmaliError::LimitExceeded);
    options.max_input_bytes = 1024;
    options.max_output_bytes = 1024;
    for (const std::string value : {"Lx//y;", "Lx/;", "[V", "L;", "II"}) {
        fixture.bytes[128] = value.size();
        std::copy(value.begin(), value.end(), fixture.bytes.begin() + 129);
        fixture.bytes[129 + value.size()] = 0;
        State current{options};
        CheckedDex reader(fixture.bytes, 0, current);
        std::string type;
        Check(reader.Init());
        Check(!reader.Type(0, type));
        Check(current.status.error == SmaliError::MalformedInput);
    }
}

void SelectionIsolation() {
    Fixture fixture;
    fixture.header.method_ids_off = 120;
    fixture.header.method_ids_size = 2;
    fixture.header.data_off = 192;
    fixture.header.data_size = 320;
    fixture.Sync();
    fixture.Put(120, dex::MethodId{0, 0, 0});
    fixture.Put(128, dex::MethodId{0, 0, 0});
    // Two direct methods: the target has a code pointer that is not followed
    // by selection. The second method has an invalid body pointer at 0xffff.
    const uint8_t records[]{0, 0, 2, 0, 0, 9, 0x80, 2, 1, 9, 0xff, 0xff, 3};
    std::copy(std::begin(records), std::end(records), fixture.bytes.begin() + 192);
    dex::ClassDef owner{};
    owner.class_data_off = 192;
    owner.annotations_off = 224;
    fixture.Put(224, dex::AnnotationsDirectoryItem{UINT32_MAX, 1, 2, 1});
    fixture.Put(240, dex::FieldAnnotationsItem{0, UINT32_MAX});
    fixture.Put(248, dex::MethodAnnotationsItem{0, 400});
    fixture.Put(256, dex::MethodAnnotationsItem{1, UINT32_MAX});
    fixture.Put(264, dex::ParameterAnnotationsItem{0, 420});
    SmaliOptions options;
    State state{options};
    CheckedDex dex(fixture.bytes, 0, state);
    Check(dex.Init());
    Member target;
    Check(SelectMethod(dex, owner, 0, target));
    Check(target.index == 0 && target.access == 9 && target.code_offset == 256);
    AnnotationOffsets annotations;
    Check(SelectAnnotations(dex, owner, SmaliMemberKind::Method, 0, annotations));
    Check(annotations.annotations == 400 && annotations.parameters == 420);
    ClassMembers members;
    Check(SelectClass(dex, owner, members));
    Check(members.direct_methods.size() == 2 && members.direct_methods[1].code_offset == 65535);
    dex::ClassDef empty{};
    Check(SelectClass(dex, empty, members));
    Check(members.static_fields.empty() && members.instance_fields.empty() &&
          members.direct_methods.empty() && members.virtual_methods.empty());
    Check(SelectClass(dex, owner, members));
    // Corrupt a record after the target. A target request may stop as soon as
    // it is located; class selection has to read and reject the later record.
    fixture.bytes[200] = 0; // Second method's index delta duplicates the first.
    State method_state{options};
    CheckedDex method_dex(fixture.bytes, 0, method_state);
    Check(method_dex.Init() && SelectMethod(method_dex, owner, 0, target));
    State class_state{options};
    CheckedDex class_dex(fixture.bytes, 0, class_state);
    Check(class_dex.Init() && !SelectClass(class_dex, owner, members));
    Check(class_state.status.error == SmaliError::MalformedInput);
    Check(members.direct_methods.size() == 2); // Failed selection is transactional.

    fixture.bytes[194] = 0;
    fixture.bytes[195] = 2; // Put static/private/constructor in the virtual group.
    for (uint8_t access : {uint8_t(9), uint8_t(2)}) {
        fixture.bytes[197] = access;
        State bad_group{options};
        CheckedDex bad_dex(fixture.bytes, 0, bad_group);
        Check(bad_dex.Init() && !SelectMethod(bad_dex, owner, 0, target));
        Check(bad_group.status.error == SmaliError::MalformedInput);
    }
    fixture.bytes[194] = 2; fixture.bytes[195] = 0;
    fixture.bytes[197] = 1; // Ordinary public instance method in direct group.
    State bad_direct{options};
    CheckedDex direct_dex(fixture.bytes, 0, bad_direct);
    Check(direct_dex.Init() && !SelectMethod(direct_dex, owner, 0, target));
}

void ReviewRegressions() {
    Fixture fixture;
    fixture.header.data_size = 16;
    fixture.header.map_off = 140;
    fixture.Sync();
    fixture.Put<uint32_t>(140, 1);
    fixture.Put(144, dex::MapItem{0, 0, 1, 0});
    SmaliOptions options;
    State map_state{options};
    CheckedDex map_dex(fixture.bytes, 0, map_state);
    Check(map_dex.Init());
    Check(!CheckMetadataProfile(map_dex));
    Check(map_state.status.error == SmaliError::MalformedInput && map_state.status.dex_offset == 144);

    Fixture normal;
    State misuse{options};
    CheckedDex entry_dex(normal.bytes, 0, misuse);
    uint32_t entry;
    Check(entry_dex.Init());
    Check(!entry_dex.Entry(112, 0x40000001, 0x40000000, entry));
    State zero_alignment{options};
    CheckedDex aligned(normal.bytes, 0, zero_alignment);
    Check(!aligned.Range(0, 0, 0) && zero_alignment.status.error == SmaliError::InternalError);
    State zero_width{options};
    CheckedDex mapped(normal.bytes, 0, zero_width);
    uint32_t offset, count;
    Check(!mapped.MapSection(0, offset, count, 0) && zero_width.status.error == SmaliError::InternalError);

    State diagnostic{options};
    diagnostic.status.dex_id = 7;
    diagnostic.member_kind = SmaliMemberKind::Method;
    diagnostic.member_id = 11;
    diagnostic.code_offset = 3;
    diagnostic.phase = SmaliPhase::Read;
    diagnostic.Fail(SmaliError::MalformedInput, 128, 9);
    diagnostic.Fail(SmaliError::Unsupported);
    Check(diagnostic.status.dex_id == 7 && diagnostic.status.member_id == 11 &&
          diagnostic.status.member_kind == SmaliMemberKind::Method && diagnostic.status.code_offset == 3 &&
          diagnostic.status.dex_offset == 128 && diagnostic.status.detail == 9 &&
          diagnostic.status.phase == SmaliPhase::Read && diagnostic.status.error == SmaliError::MalformedInput);

    options.max_output_bytes = 3;
    State unicode{options};
    normal.String({2, 0xe4, 0xb8, 0xad, 0xe6, 0x96, 0x87, 0});
    CheckedDex unicode_dex(normal.bytes, 0, unicode);
    std::string name;
    Check(unicode_dex.Init() && !unicode_dex.Name(0, name));
    Check(unicode.status.error == SmaliError::LimitExceeded && name.size() <= 3);
    State snapshot{options};
    options.max_output_bytes = 0;
    Check(snapshot.Append("abc"));

    normal.header.string_ids_size = 2;
    normal.header.type_ids_off = 120;
    normal.header.type_ids_size = 2;
    normal.header.proto_ids_off = 128;
    normal.header.proto_ids_size = 1;
    normal.header.data_off = 192;
    normal.header.data_size = 320;
    normal.Sync();
    normal.Put<uint32_t>(112, 200); normal.Put<uint32_t>(116, 204);
    normal.Put<uint32_t>(120, 0); normal.Put<uint32_t>(124, 1);
    normal.Put(128, dex::ProtoId{0, 1, 208});
    normal.bytes[200] = 1; normal.bytes[201] = 'I';
    normal.bytes[204] = 1; normal.bytes[205] = 'V';
    normal.Put<uint32_t>(208, 2);
    options.max_output_bytes = 4;
    State prototype{options};
    CheckedDex proto_dex(normal.bytes, 0, prototype);
    std::string signature;
    Check(proto_dex.Init() && !proto_dex.Proto(0, signature));
    Check(prototype.status.error == SmaliError::LimitExceeded && signature.size() <= 4);
}
} // namespace

int main() {
    OpcodeNames();
    StringsAndContainer();
    MalformedStrings();
    BoundsAndLeb();
    LimitsAndNames();
    SelectionIsolation();
    ReviewRegressions();
    std::cout << checks << " smali reader checks passed\n";
}

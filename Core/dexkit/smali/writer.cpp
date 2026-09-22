// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "writer.h"

#include "code.h"
#include "debug.h"
#include "selection.h"

namespace dexkit::smali {
namespace {

class Writer {
public:
    explicit Writer(CheckedDex& dex) : dex_(dex), state_(dex.state()), metadata_(dex) {}
    bool Method(const dex::ClassDef& owner, const Member& method);
    bool Class(const dex::ClassDef& owner);

private:
    bool Field(const dex::ClassDef& owner, const Member& field, size_t* initial);
    bool Parameters(const std::vector<uint16_t>& types, const std::vector<uint32_t>& names,
                    uint32_t annotation_offset, uint32_t access);
    bool Annotations(const dex::ClassDef& owner, SmaliMemberKind kind, uint32_t id, AnnotationOffsets& result) {
        return whole_class_ ? directory_.Find(dex_, kind, id, result) : SelectAnnotations(dex_, owner, kind, id, result);
    }
    CheckedDex& dex_;
    State& state_;
    Metadata metadata_;
    AnnotationDirectory directory_;
    bool whole_class_ = false;
};

bool Writer::Parameters(const std::vector<uint16_t>& types, const std::vector<uint32_t>& names,
                        uint32_t annotation_offset, uint32_t access) {
    uint32_t count = 0;
    size_t entries = 0;
    if (annotation_offset) {
        if (!dex_.Data(annotation_offset, 4, 4) || !dex_.Object(annotation_offset, count)) return false;
        entries = size_t(annotation_offset) + 4;
        if (count > types.size()) return state_.Fail(SmaliError::MalformedInput, annotation_offset);
        if (!dex_.Data(entries, size_t(count) * 4) || !state_.Items(count)) return false;
    }
    uint32_t reg = access & dex::kAccStatic ? 0 : 1;
    for (size_t i = 0; i < types.size(); ++i) {
        uint32_t annotations = 0;
        if (i < count && !dex_.Object(entries + i * 4, annotations)) return false;
        uint32_t name = i < names.size() ? names[i] : dex::kNoIndex;
        if (annotations || name != dex::kNoIndex) {
            if (!state_.Append(".param p") || !state_.Number(reg)) return false;
            if (name != dex::kNoIndex && (!state_.Append(", ") || !dex_.Quoted(name))) return false;
            if (!state_.Append("\n")) return false;
            if (annotations && (!metadata_.AnnotationSet(annotations) || !state_.Append(".end param\n"))) return false;
        }
        std::string type;
        if (!dex_.Type(types[i], type)) return false;
        reg += type == "J" || type == "D" ? 2 : 1;
    }
    return true;
}

bool Writer::Method(const dex::ClassDef& owner, const Member& method) {
    state_.member_kind = SmaliMemberKind::Method;
    state_.member_id = method.index;
    state_.code_offset = UINT32_MAX;
    state_.phase = SmaliPhase::Read;
    const auto& header = dex_.header();
    dex::MethodId definition;
    std::string name, prototype;
    std::vector<uint16_t> parameters;
    if (!dex_.Entry(header.method_ids_off, header.method_ids_size, method.index, definition) ||
        !dex_.Name(definition.name_idx, name, true) || !dex_.Proto(definition.proto_idx, prototype, &parameters)) return false;
    if (definition.class_idx != owner.class_idx) return state_.Fail(SmaliError::MalformedInput);
    uint32_t input_registers = method.access & dex::kAccStatic ? 0 : 1;
    for (auto parameter : parameters) {
        std::string type;
        if (!dex_.Type(parameter, type)) return false;
        input_registers += type == "J" || type == "D" ? 2 : 1;
    }
    AnnotationOffsets annotations;
    if (!Annotations(owner, SmaliMemberKind::Method, method.index, annotations)) return false;
    Code body(dex_);
    DebugInfo debug(dex_, body);
    if (method.code_offset) {
        if (!body.Build(method.code_offset)) return false;
        if (body.header().ins_size != input_registers)
            return state_.Fail(SmaliError::MalformedInput, method.code_offset);
        if (state_.options.debug == SmaliDebugMode::Strict && !debug.Read(parameters.size())) return false;
    }
    state_.phase = SmaliPhase::Emit;
    state_.code_offset = UINT32_MAX;
    if (!state_.Append(".method ") || !Access(state_, method.access, SmaliMemberKind::Method) ||
        !state_.Append(name) || !state_.Append(prototype) || !state_.Append("\n")) return false;
    if (method.code_offset && (!state_.Append(".registers ") || !state_.Number(body.header().registers_size) ||
                               !state_.Append("\n"))) return false;
    if (!metadata_.AnnotationSet(annotations.annotations) ||
        !Parameters(parameters, debug.parameter_names(), annotations.parameters, method.access)) return false;
    if (method.code_offset) {
        for (uint32_t pc = 0; pc < body.header().insns_size;) {
            Decoded decoded;
            if (!body.DecodeAt(pc, decoded) || !debug.EmitAt(pc) || !body.EmitAt(pc, decoded)) return false;
            pc += decoded.width;
        }
        if (!debug.EmitAt(body.header().insns_size) || !body.EmitCatches()) return false;
    }
    return state_.Append(".end method\n");
}

bool Writer::Field(const dex::ClassDef& owner, const Member& field, size_t* initial) {
    state_.member_kind = SmaliMemberKind::Field;
    state_.member_id = field.index;
    state_.code_offset = UINT32_MAX;
    state_.phase = SmaliPhase::Read;
    std::string descriptor;
    if (!dex_.Field(field.index, descriptor, false)) return false;
    AnnotationOffsets annotations;
    if (!Annotations(owner, SmaliMemberKind::Field, field.index, annotations)) return false;
    state_.phase = SmaliPhase::Emit;
    if (!state_.Append(".field ") || !Access(state_, field.access, SmaliMemberKind::Field) ||
        !state_.Append(descriptor)) return false;
    auto field_type = std::string_view(descriptor).substr(descriptor.find(':') + 1);
    if (initial && (!state_.Append(" = ") || !metadata_.StaticValue(field_type, *initial))) return false;
    if (!state_.Append("\n")) return false;
    if (annotations.annotations && (!metadata_.AnnotationSet(annotations.annotations) ||
                                     !state_.Append(".end field\n"))) return false;
    return true;
}

bool Writer::Class(const dex::ClassDef& owner) {
    state_.member_kind = SmaliMemberKind::Class;
    state_.member_id = owner.class_idx;
    state_.phase = SmaliPhase::Read;
    std::string descriptor;
    ClassMembers members;
    std::vector<uint16_t> interfaces;
    AnnotationOffsets annotations;
    if (!SelectClass(dex_, owner, members) || !directory_.Init(dex_, owner, members)) return false;
    whole_class_ = true;
    if (!dex_.Type(owner.class_idx, descriptor) ||
        !dex_.TypeList(owner.interfaces_off, interfaces) ||
        !Annotations(owner, SmaliMemberKind::Class, owner.class_idx, annotations)) return false;
    if (descriptor.front() != 'L') return state_.Fail(SmaliError::MalformedInput);
    state_.phase = SmaliPhase::Emit;
    if (!state_.Append(".class ") || !Access(state_, owner.access_flags, SmaliMemberKind::Class) ||
        !state_.Append(descriptor) || !state_.Append("\n")) return false;
    if (owner.superclass_idx != dex::kNoIndex) {
        if (!dex_.Type(owner.superclass_idx, descriptor)) return false;
        if (descriptor.front() != 'L') return state_.Fail(SmaliError::MalformedInput);
        if (!state_.Append(".super ") || !state_.Append(descriptor) || !state_.Append("\n")) return false;
    } else if (descriptor != "Ljava/lang/Object;") return state_.Fail(SmaliError::MalformedInput);
    if (owner.source_file_idx != dex::kNoIndex && (!state_.Append(".source ") ||
        !dex_.Quoted(owner.source_file_idx) || !state_.Append("\n"))) return false;
    for (auto interface : interfaces) {
        if (!dex_.Type(interface, descriptor)) return false;
        if (descriptor.front() != 'L') return state_.Fail(SmaliError::MalformedInput);
        if (!state_.Append(".implements ") || !state_.Append(descriptor) || !state_.Append("\n")) return false;
    }
    if (!metadata_.AnnotationSet(annotations.annotations)) return false;
    size_t initial = owner.static_values_off;
    uint32_t initial_count = 0;
    if (initial && !dex_.Uleb(initial, initial_count)) return false;
    if (initial_count > members.static_fields.size()) return state_.Fail(SmaliError::MalformedInput, initial);
    for (size_t i = 0; i < members.static_fields.size(); ++i)
        if (!Field(owner, members.static_fields[i], i < initial_count ? &initial : nullptr)) return false;
    for (auto field : members.instance_fields) if (!Field(owner, field, nullptr)) return false;
    for (auto method : members.direct_methods) if (!Method(owner, method)) return false;
    for (auto method : members.virtual_methods) if (!Method(owner, method)) return false;
    return true;
}

SmaliStatus Write(std::span<const uint8_t> image, size_t header_offset, uint32_t class_index,
                  uint32_t method_id, bool is_method, const SmaliOptions& options, std::string& output) {
    State state{options};
    state.member_kind = is_method ? SmaliMemberKind::Method : SmaliMemberKind::Class;
    state.member_id = is_method ? method_id : UINT32_MAX;
    if (options.debug != SmaliDebugMode::None && options.debug != SmaliDebugMode::Strict) {
        state.Fail(SmaliError::Unsupported);
        return state.status;
    }
    if (options.max_annotation_depth > 256) {
        state.Fail(SmaliError::LimitExceeded);
        return state.status;
    }
    CheckedDex dex(image, header_offset, state);
    if (!dex.Init()) return state.status;
    if (class_index >= dex.header().class_defs_size) {
        state.Fail(SmaliError::InvalidIdentity);
        return state.status;
    }
    dex::ClassDef owner;
    if (!dex.ClassDef(class_index, owner) || !CheckMetadataProfile(dex)) return state.status;
    Writer writer(dex);
    bool success;
    if (is_method) {
        Member method;
        success = SelectMethod(dex, owner, method_id, method) && writer.Method(owner, method);
    } else success = writer.Class(owner);
    if (success && state.status.ok()) output.swap(state.output);
    else if (state.status.ok()) state.Fail(SmaliError::InternalError);
    return state.status;
}
} // namespace

SmaliStatus WriteMethod(std::span<const uint8_t> image, size_t header_offset,
                        uint32_t class_def_index, uint32_t method_id,
                        const SmaliOptions& options, std::string& output) {
    return Write(image, header_offset, class_def_index, method_id, true, options, output);
}

SmaliStatus WriteClass(std::span<const uint8_t> image, size_t header_offset,
                       uint32_t class_def_index, const SmaliOptions& options, std::string& output) {
    return Write(image, header_offset, class_def_index, 0, false, options, output);
}

} // namespace dexkit::smali

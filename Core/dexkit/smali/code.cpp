// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "code.h"

#include <algorithm>
#include <array>

namespace dexkit::smali {

bool Code::DecodeAt(uint32_t pc, Decoded& decoded) {
    state_.phase = SmaliPhase::Decode;
    state_.code_offset = pc;
    decoded = {};
    if (pc >= header_.insns_size) return state_.Fail(SmaliError::MalformedInput, byte_offset(pc));
    uint16_t first;
    if (!dex_.Object(byte_offset(pc), first)) return false;
    if (!(first & 255) && first) {
        if (pc & 1) return state_.Fail(SmaliError::MalformedInput, byte_offset(pc));
        uint16_t size;
        if (header_.insns_size - pc < 2 || !dex_.Object(byte_offset(pc) + 2, size))
            return state_.Fail(SmaliError::MalformedInput, byte_offset(pc));
        uint64_t width;
        if (first == dex::kPackedSwitchSignature) width = 4 + uint64_t(size) * 2;
        else if (first == dex::kSparseSwitchSignature) width = 2 + uint64_t(size) * 4;
        else if (first == dex::kArrayDataSignature) {
            if (header_.insns_size - pc < 4) return state_.Fail(SmaliError::MalformedInput, byte_offset(pc));
            uint32_t count;
            if (!dex_.Object(byte_offset(pc) + 4, count)) return false;
            if (size != 1 && size != 2 && size != 4 && size != 8)
                return state_.Fail(SmaliError::MalformedInput, byte_offset(pc), size);
            width = 4 + (uint64_t(size) * count + 1) / 2;
        } else return state_.Fail(SmaliError::Unsupported, byte_offset(pc), first);
        if (width > header_.insns_size - pc) return state_.Fail(SmaliError::MalformedInput, byte_offset(pc));
        decoded.payload = first;
        decoded.width = static_cast<uint32_t>(width);
        return true;
    }
    auto opcode = static_cast<dex::Opcode>(first & 255);
    auto verify = dex::GetVerifyFlagsFromOpcode(opcode);
    if (verify & (dex::kVerifyError | dex::kVerifyRuntimeOnly))
        return state_.Fail(SmaliError::Unsupported, byte_offset(pc), opcode);
    if ((opcode >= dex::OP_INVOKE_POLYMORPHIC && dex_.version() < 38) ||
        (opcode >= dex::OP_CONST_METHOD_HANDLE && dex_.version() < 39))
        return state_.Fail(SmaliError::MalformedInput, byte_offset(pc), opcode);
    auto format = dex::GetFormatFromOpcode(opcode);
    const size_t width = dex::GetWidthFromFormat(format);
    if (!width || width > 5 || width > header_.insns_size - pc)
        return state_.Fail(SmaliError::MalformedInput, byte_offset(pc), opcode);
    if ((format == dex::k35c || format == dex::k45cc) && (first >> 12) > 5)
        return state_.Fail(SmaliError::MalformedInput, byte_offset(pc), opcode);
    std::array<uint16_t, 5> units{};
    if (!dex_.Read(byte_offset(pc), units.data(), width * 2)) return false;
    decoded.instruction = dex::DecodeInstruction(units.data());
    decoded.width = width;
    return ValidateRegisters(decoded.instruction) && ValidateReference(decoded.instruction);
}

bool Code::ValidateRegisters(const dex::Instruction& inst) {
    auto flags = dex::GetVerifyFlagsFromOpcode(inst.opcode);
    auto reg = [&](uint32_t value, bool wide) {
        return (value < header_.registers_size && (!wide || value + 1 < header_.registers_size)) ||
               state_.Fail(SmaliError::MalformedInput, byte_offset(state_.code_offset), value);
    };
    if ((flags & (dex::kVerifyRegA | dex::kVerifyRegAWide)) && !reg(inst.vA, flags & dex::kVerifyRegAWide)) return false;
    if ((flags & (dex::kVerifyRegB | dex::kVerifyRegBWide)) && !reg(inst.vB, flags & dex::kVerifyRegBWide)) return false;
    if ((flags & (dex::kVerifyRegC | dex::kVerifyRegCWide)) && !reg(inst.vC, flags & dex::kVerifyRegCWide)) return false;
    auto format = dex::GetFormatFromOpcode(inst.opcode);
    if (flags & (dex::kVerifyVarArgNonZero | dex::kVerifyVarArgRangeNonZero))
        if (!inst.vA) return state_.Fail(SmaliError::MalformedInput, byte_offset(state_.code_offset));
    if (format == dex::k35c || format == dex::k45cc) {
        for (uint32_t i = 0; i < inst.vA; ++i) {
            uint32_t r = format == dex::k45cc ? (i ? inst.arg[i - 1] : inst.vC) : inst.arg[i];
            if (!reg(r, false)) return false;
        }
    } else if (format == dex::k3rc || format == dex::k4rcc) {
        if (inst.vA && (inst.vC >= header_.registers_size || inst.vA > header_.registers_size - inst.vC))
            return state_.Fail(SmaliError::MalformedInput, byte_offset(state_.code_offset));
    }
    return true;
}

bool Code::ValidateReference(const dex::Instruction& inst) {
    auto type = dex::GetIndexTypeFromOpcode(inst.opcode);
    auto format = dex::GetFormatFromOpcode(inst.opcode);
    auto index = format == dex::k22c ? inst.vC : inst.vB;
    uint32_t count = 0, offset = 0;
    switch (type) {
        case dex::kIndexNone: return true;
        case dex::kIndexStringRef: count = dex_.header().string_ids_size; break;
        case dex::kIndexTypeRef: count = dex_.header().type_ids_size; break;
        case dex::kIndexFieldRef: count = dex_.header().field_ids_size; break;
        case dex::kIndexMethodRef: count = dex_.header().method_ids_size; break;
        case dex::kIndexProtoRef: count = dex_.header().proto_ids_size; break;
        case dex::kIndexMethodAndProtoRef:
            if (inst.arg[4] >= dex_.header().proto_ids_size)
                return state_.Fail(SmaliError::MalformedInput, byte_offset(state_.code_offset), inst.arg[4]);
            count = dex_.header().method_ids_size;
            break;
        case dex::kIndexCallSiteRef: if (!dex_.MapSection(7, offset, count, 4)) return false; break;
        case dex::kIndexMethodHandleRef:
            if (!dex_.MapSection(dex::kMethodHandleItem, offset, count, sizeof(dex::MethodHandle))) return false;
            break;
        default: return state_.Fail(SmaliError::Unsupported, byte_offset(state_.code_offset), type);
    }
    return index < count || state_.Fail(SmaliError::MalformedInput, byte_offset(state_.code_offset), index);
}

bool Code::Boundary(uint32_t pc, bool allow_end) const {
    return (allow_end && pc == header_.insns_size) || (pc < marks_.size() && (marks_[pc] & kInstruction));
}

bool Code::Target(uint32_t pc, int32_t delta, uint32_t& target, bool executable) {
    state_.code_offset = pc;
    int64_t absolute = int64_t(pc) + delta;
    if (absolute < 0 || absolute >= header_.insns_size)
        return state_.Fail(SmaliError::MalformedInput, byte_offset(pc));
    target = static_cast<uint32_t>(absolute);
    if (!(marks_[target] & (executable ? kInstruction : kPayload)))
        return state_.Fail(SmaliError::MalformedInput, byte_offset(pc), target);
    marks_[target] |= kLabel;
    return true;
}

bool Code::Build(uint32_t offset) {
    payloads_.clear();
    tries_.clear();
    state_.phase = SmaliPhase::Decode;
    state_.code_offset = UINT32_MAX;
    if (!dex_.Data(offset, sizeof(header_), 4) || !dex_.Object(offset, header_)) return false;
    if (!header_.insns_size || header_.registers_size < header_.ins_size)
        return state_.Fail(SmaliError::MalformedInput, offset);
    if (header_.insns_size > state_.options.max_code_units ||
        header_.insns_size >= marks_.max_size() || !state_.Items(header_.insns_size))
        return state_.Fail(SmaliError::LimitExceeded, offset);
    instructions_ = size_t(offset) + sizeof(header_);
    if (header_.insns_size > (dex_.size() - instructions_) / 2 ||
        !dex_.Data(instructions_, size_t(header_.insns_size) * 2))
        return state_.Fail(SmaliError::MalformedInput, offset);
    marks_.assign(size_t(header_.insns_size) + 1, 0);
    for (uint32_t pc = 0; pc < header_.insns_size;) {
        Decoded decoded;
        if (!DecodeAt(pc, decoded)) return false;
        marks_[pc] = decoded.payload ? kPayload : kInstruction;
        if (decoded.payload) payloads_.push_back({pc, UINT32_MAX, decoded.payload});
        pc += decoded.width;
    }
    // Entry and explicit control-flow targets must name real instructions.
    // Do not reject unreachable alignment nops immediately before a payload;
    // proving reachability/type flow belongs to the runtime verifier.
    if (!Boundary(0)) return state_.Fail(SmaliError::MalformedInput, instructions_);
    for (uint32_t pc = 0; pc < header_.insns_size;) {
        Decoded decoded;
        if (!DecodeAt(pc, decoded)) return false;
        if (!decoded.payload) {
            auto format = dex::GetFormatFromOpcode(decoded.instruction.opcode);
            int32_t delta;
            uint32_t target;
            bool branch = true;
            switch (format) {
                case dex::k10t: case dex::k20t: case dex::k30t: delta = int32_t(decoded.instruction.vA); break;
                case dex::k21t: delta = int32_t(decoded.instruction.vB); break;
                case dex::k22t: delta = int32_t(decoded.instruction.vC); break;
                default: branch = false; break;
            }
            if (branch && !Target(pc, delta, target)) return false;
        }
        pc += decoded.width;
    }
    return PayloadLinks() && TryBlocks();
}

bool Code::PayloadLinks() {
    for (uint32_t pc = 0; pc < header_.insns_size;) {
        Decoded decoded;
        if (!DecodeAt(pc, decoded)) return false;
        if (!decoded.payload && dex::GetFormatFromOpcode(decoded.instruction.opcode) == dex::k31t) {
            uint32_t target;
            if (!Target(pc, int32_t(decoded.instruction.vB), target, false)) return false;
            auto it = std::lower_bound(payloads_.begin(), payloads_.end(), target,
                                      [](const Payload& value, uint32_t pc) { return value.offset < pc; });
            if (it == payloads_.end() || it->offset != target) return state_.Fail(SmaliError::InternalError);
            uint16_t expected = decoded.instruction.opcode == dex::OP_PACKED_SWITCH ? dex::kPackedSwitchSignature :
                                decoded.instruction.opcode == dex::OP_SPARSE_SWITCH ? dex::kSparseSwitchSignature : dex::kArrayDataSignature;
            if (it->kind != expected) return state_.Fail(SmaliError::MalformedInput, byte_offset(pc));
            if (it->owner != UINT32_MAX && expected != dex::kArrayDataSignature)
                return state_.Fail(SmaliError::Unsupported, byte_offset(pc), expected);
            it->owner = pc;
        }
        pc += decoded.width;
    }
    for (auto payload : payloads_) {
        state_.code_offset = payload.offset;
        if (payload.kind == dex::kArrayDataSignature) continue;
        if (payload.owner == UINT32_MAX) return state_.Fail(SmaliError::Unsupported, byte_offset(payload.offset), payload.kind);
        uint16_t count;
        if (!dex_.Object(byte_offset(payload.offset) + 2, count)) return false;
        size_t keys = byte_offset(payload.offset) + 4;
        size_t targets = payload.kind == dex::kPackedSwitchSignature ? keys + 4 : keys + size_t(count) * 4;
        int32_t previous = 0, key = 0;
        if (payload.kind == dex::kPackedSwitchSignature) {
            if (!dex_.Object(keys, key)) return false;
            if (count && int64_t(key) + count - 1 > INT32_MAX)
                return state_.Fail(SmaliError::MalformedInput, keys);
        }
        for (uint32_t i = 0; i < count; ++i) {
            state_.code_offset = payload.offset;
            if (payload.kind == dex::kSparseSwitchSignature) {
                if (!dex_.Object(keys + size_t(i) * 4, key)) return false;
                if (i && key <= previous) return state_.Fail(SmaliError::MalformedInput, keys + size_t(i) * 4);
                previous = key;
            }
            int32_t delta; uint32_t target;
            if (!dex_.Object(targets + size_t(i) * 4, delta) || !Target(payload.owner, delta, target)) return false;
        }
    }
    return true;
}

bool Code::TryBlocks() {
    state_.code_offset = UINT32_MAX;
    if (!header_.tries_size) return true;
    size_t cursor = byte_offset(header_.insns_size);
    if (header_.insns_size & 1) {
        uint16_t padding;
        if (!dex_.Data(cursor, 2) || !dex_.Object(cursor, padding)) return false;
        if (padding) return state_.Fail(SmaliError::MalformedInput, cursor);
        cursor += 2;
    }
    size_t tries = cursor;
    if (!dex_.Data(tries, size_t(header_.tries_size) * sizeof(dex::TryBlock), 4)) return false;
    cursor += size_t(header_.tries_size) * sizeof(dex::TryBlock);
    size_t handlers_base = cursor;
    uint32_t count;
    if (!dex_.Uleb(cursor, count) || !state_.Items(count)) return false;
    // Every handler list needs a signed count and at least one handler entry.
    if (count > (dex_.size() - cursor) / 2 || !dex_.Data(cursor, size_t(count) * 2))
        return state_.Fail(SmaliError::MalformedInput, cursor);
    struct CatchList { uint32_t offset; std::vector<Handler> handlers; };
    std::vector<CatchList> lists;
    if (count > lists.max_size()) return state_.Fail(SmaliError::LimitExceeded, cursor);
    lists.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (cursor - handlers_base > UINT32_MAX) return state_.Fail(SmaliError::MalformedInput, cursor);
        CatchList list{static_cast<uint32_t>(cursor - handlers_base), {}};
        int32_t signed_count;
        if (!dex_.Sleb(cursor, signed_count)) return false;
        uint32_t typed = signed_count < 0 ? uint32_t(-int64_t(signed_count)) : uint32_t(signed_count);
        if (!state_.Items(size_t(typed) + (signed_count <= 0))) return false;
        for (uint32_t j = 0; j < typed; ++j) {
            Handler handler;
            if (!dex_.Uleb(cursor, handler.type) || !dex_.Uleb(cursor, handler.target)) return false;
            if (handler.type >= dex_.header().type_ids_size || !Boundary(handler.target))
                return state_.Fail(SmaliError::MalformedInput, cursor);
            marks_[handler.target] |= kLabel;
            list.handlers.push_back(handler);
        }
        if (signed_count <= 0) {
            Handler handler{dex::kNoIndex, 0};
            if (!dex_.Uleb(cursor, handler.target)) return false;
            if (!Boundary(handler.target)) return state_.Fail(SmaliError::MalformedInput, cursor);
            marks_[handler.target] |= kLabel;
            list.handlers.push_back(handler);
        }
        lists.push_back(std::move(list));
    }
    uint32_t previous_end = 0;
    if (!state_.Items(header_.tries_size)) return false;
    for (uint32_t i = 0; i < header_.tries_size; ++i) {
        dex::TryBlock block;
        if (!dex_.Object(tries + size_t(i) * sizeof(block), block)) return false;
        if (!block.insn_count || !Boundary(block.start_addr) || block.start_addr < previous_end ||
            block.insn_count > header_.insns_size - block.start_addr)
            return state_.Fail(SmaliError::MalformedInput, tries + size_t(i) * sizeof(block));
        uint32_t end = block.start_addr + block.insn_count;
        if (end != header_.insns_size && !(marks_[end] & (kInstruction | kPayload)))
            return state_.Fail(SmaliError::MalformedInput, tries + size_t(i) * sizeof(block));
        auto list = std::lower_bound(lists.begin(), lists.end(), block.handler_off,
                                    [](const CatchList& value, uint32_t offset) { return value.offset < offset; });
        if (list == lists.end() || list->offset != block.handler_off)
            return state_.Fail(SmaliError::MalformedInput, tries + size_t(i) * sizeof(block));
        if (!state_.Items(list->handlers.size())) return false;
        tries_.push_back({block.start_addr, end, list->handlers});
        marks_[block.start_addr] |= kLabel;
        marks_[end] |= kLabel;
        previous_end = end;
    }
    return true;
}

bool Code::Reg(uint32_t reg) { return state_.Append("v") && state_.Number(reg); }
bool Code::Label(uint32_t pc) { return state_.Append(":pc_") && state_.Number(pc); }

bool Code::Constant(const dex::Instruction& inst) {
    bool wide = inst.opcode == dex::OP_CONST_WIDE || inst.opcode == dex::OP_CONST_WIDE_16 ||
                inst.opcode == dex::OP_CONST_WIDE_32 || inst.opcode == dex::OP_CONST_WIDE_HIGH16;
    uint64_t value = wide ? uint64_t(int64_t(int32_t(inst.vB))) : inst.vB;
    if (inst.opcode == dex::OP_CONST_WIDE) value = inst.vB_wide;
    else if (inst.opcode == dex::OP_CONST_HIGH16) value = inst.vB << 16;
    else if (inst.opcode == dex::OP_CONST_WIDE_HIGH16) value = uint64_t(inst.vB) << 48;
    return wide ? state_.Hex(value, true) : state_.Number(int32_t(value));
}

bool Code::EmitAt(uint32_t pc, const Decoded& decoded) {
    state_.phase = SmaliPhase::Emit;
    state_.code_offset = pc;
    if (HasLabel(pc) && (!Label(pc) || !state_.Append("\n"))) return false;
    if (decoded.payload) return EmitPayload(pc, decoded.payload);
    auto& inst = decoded.instruction;
    auto format = dex::GetFormatFromOpcode(inst.opcode);
    Metadata metadata(dex_);
    auto comma = [&] { return state_.Append(", "); };
    auto index = [&](uint32_t value) { return metadata.Index(dex::GetIndexTypeFromOpcode(inst.opcode), value); };
    auto label = [&](uint32_t delta) { return Label(uint32_t(int64_t(pc) + int32_t(delta))); };
    if (!state_.Append(dex::GetOpcodeName(inst.opcode))) return false;
    if (format != dex::k10x && !state_.Append(" ")) return false;
    bool ok = false;
    switch (format) {
        case dex::k10x: ok = true; break;
        case dex::k11x: ok = Reg(inst.vA); break;
        case dex::k12x: case dex::k22x: case dex::k32x: ok = Reg(inst.vA) && comma() && Reg(inst.vB); break;
        case dex::k11n: case dex::k21s: case dex::k21h: case dex::k31i: case dex::k51l:
            ok = Reg(inst.vA) && comma() && Constant(inst); break;
        case dex::k10t: case dex::k20t: case dex::k30t: ok = label(inst.vA); break;
        case dex::k21t: case dex::k31t: ok = Reg(inst.vA) && comma() && label(inst.vB); break;
        case dex::k22t: ok = Reg(inst.vA) && comma() && Reg(inst.vB) && comma() && label(inst.vC); break;
        case dex::k21c: case dex::k31c: ok = Reg(inst.vA) && comma() && index(inst.vB); break;
        case dex::k22c: ok = Reg(inst.vA) && comma() && Reg(inst.vB) && comma() && index(inst.vC); break;
        case dex::k23x: ok = Reg(inst.vA) && comma() && Reg(inst.vB) && comma() && Reg(inst.vC); break;
        case dex::k22s: case dex::k22b:
            ok = Reg(inst.vA) && comma() && Reg(inst.vB) && comma() && state_.Number(int32_t(inst.vC)); break;
        case dex::k35c: case dex::k45cc:
            if (!state_.Append("{")) return false;
            for (uint32_t i = 0; i < inst.vA; ++i) {
                uint32_t reg = format == dex::k45cc ? (i ? inst.arg[i - 1] : inst.vC) : inst.arg[i];
                if ((i && !comma()) || !Reg(reg)) return false;
            }
            ok = state_.Append("}, ") && index(inst.vB);
            if (ok && format == dex::k45cc) ok = comma() && metadata.Index(dex::kIndexProtoRef, inst.arg[4]);
            break;
        case dex::k3rc: case dex::k4rcc:
            if (!state_.Append("{")) return false;
            if (inst.vA && (!Reg(inst.vC) || !state_.Append(" .. ") || !Reg(inst.vC + inst.vA - 1))) return false;
            ok = state_.Append("}, ") && index(inst.vB);
            if (ok && format == dex::k4rcc) ok = comma() && metadata.Index(dex::kIndexProtoRef, inst.arg[4]);
            break;
        default: return state_.Fail(SmaliError::Unsupported, byte_offset(pc), format);
    }
    return ok && state_.Append("\n");
}

bool Code::EmitPayload(uint32_t pc, uint16_t kind) {
    uint16_t size;
    if (!dex_.Object(byte_offset(pc) + 2, size)) return false;
    if (kind == dex::kArrayDataSignature) {
        uint32_t count;
        if (!dex_.Object(byte_offset(pc) + 4, count) || !state_.Append(".array-data ") ||
            !state_.Number(size) || !state_.Append("\n")) return false;
        for (uint32_t i = 0; i < count; ++i) {
            uint64_t bits = 0;
            if (!dex_.Read(byte_offset(pc) + 8 + size_t(i) * size, &bits, size) ||
                !state_.Hex(bits, size == 8) || !state_.Append("\n")) return false;
        }
        return state_.Append(".end array-data\n");
    }
    auto payload = std::lower_bound(payloads_.begin(), payloads_.end(), pc,
                                   [](const Payload& value, uint32_t pc) { return value.offset < pc; });
    if (payload == payloads_.end() || payload->offset != pc) return state_.Fail(SmaliError::InternalError);
    size_t keys = byte_offset(pc) + 4;
    size_t targets = kind == dex::kPackedSwitchSignature ? keys + 4 : keys + size_t(size) * 4;
    if (kind == dex::kPackedSwitchSignature) {
        int32_t key;
        if (!dex_.Object(keys, key) || !state_.Append(".packed-switch ") || !state_.Number(key) || !state_.Append("\n")) return false;
    } else if (!state_.Append(".sparse-switch\n")) return false;
    for (uint32_t i = 0; i < size; ++i) {
        if (kind == dex::kSparseSwitchSignature) {
            int32_t key;
            if (!dex_.Object(keys + size_t(i) * 4, key) || !state_.Number(key) || !state_.Append(" -> ")) return false;
        }
        int32_t delta;
        if (!dex_.Object(targets + size_t(i) * 4, delta) ||
            !Label(uint32_t(int64_t(payload->owner) + delta)) || !state_.Append("\n")) return false;
    }
    return state_.Append(kind == dex::kPackedSwitchSignature ? ".end packed-switch\n" : ".end sparse-switch\n");
}

bool Code::EmitCatches() {
    if (HasLabel(header_.insns_size) && (!Label(header_.insns_size) || !state_.Append("\n"))) return false;
    for (const auto& block : tries_) {
        state_.code_offset = block.start;
        for (auto handler : block.handlers) {
            if (handler.type == dex::kNoIndex) {
                if (!state_.Append(".catchall ")) return false;
            } else {
                std::string type;
                if (!dex_.Type(handler.type, type)) return false;
                if (type.front() != 'L') return state_.Fail(SmaliError::MalformedInput);
                if (!state_.Append(".catch ") || !state_.Append(type) || !state_.Append(" ")) return false;
            }
            if (!state_.Append("{") || !Label(block.start) || !state_.Append(" .. ") || !Label(block.end) ||
                !state_.Append("} ") || !Label(handler.target) || !state_.Append("\n")) return false;
        }
    }
    return true;
}

} // namespace dexkit::smali

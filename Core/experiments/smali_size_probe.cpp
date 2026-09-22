// Opt-in linkage experiment, never part of a normal library build.
// Compare identical bounded instruction subsets before choosing a body writer.
#include <algorithm>
#include <charconv>
#include <cstring>
#include <string>
#include <vector>
#include "slicer/dex_bytecode.h"
#if DEXKIT_SMALI_PROBE_IR
#include "slicer/code_ir.h"
#endif

namespace {
void Number(std::string& out, int64_t value) {
    char buffer[24];
    auto end = std::to_chars(buffer, buffer + sizeof(buffer), value).ptr;
    out.append(buffer, end);
}
void Register(std::string& out, uint32_t reg) { out += 'v'; Number(out, reg); }
void Label(std::string& out, uint32_t offset) { out += ":pc_"; Number(out, offset); }

// Both backends get exactly the same preflight, inputs and output contract.
bool Check(const uint16_t* code, size_t size, uint16_t regs,
           std::vector<uint32_t>& starts, std::vector<uint32_t>& labels) {
    if (!code || size == 0 || size > 65536) return false;
    for (size_t pc = 0; pc < size;) {
        auto op = static_cast<dex::Opcode>(code[pc] & 255);
        switch (op) {
            case dex::OP_NOP: if (code[pc] != 0) return false; break;
            case dex::OP_RETURN_VOID:
            case dex::OP_MOVE:
            case dex::OP_CONST_4:
            case dex::OP_CONST_16:
            case dex::OP_GOTO:
            case dex::OP_IF_EQZ: break;
            default: return false;
        }
        size_t width = dex::GetWidthFromFormat(dex::GetFormatFromOpcode(op));
        if (width > size - pc) return false;
        auto inst = dex::DecodeInstruction(code + pc);
        if (op == dex::OP_MOVE && (inst.vA >= regs || inst.vB >= regs)) return false;
        if ((op == dex::OP_CONST_4 || op == dex::OP_CONST_16 || op == dex::OP_IF_EQZ)
            && inst.vA >= regs) return false;
        if (op == dex::OP_GOTO || op == dex::OP_IF_EQZ) {
            int64_t target = int64_t(pc) + int64_t(static_cast<int32_t>(
                op == dex::OP_GOTO ? inst.vA : inst.vB));
            if (target < 0 || target >= static_cast<int64_t>(size)) return false;
            labels.push_back(static_cast<uint32_t>(target));
        }
        starts.push_back(static_cast<uint32_t>(pc));
        pc += width;
    }
    std::sort(labels.begin(), labels.end());
    labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
    for (auto pc : labels) if (!std::binary_search(starts.begin(), starts.end(), pc)) return false;
    return true;
}

#if DEXKIT_SMALI_PROBE_IR
struct BodyVisitor : lir::Visitor {
    std::string& out;
    explicit BodyVisitor(std::string& out) : out(out) {}
    bool Visit(lir::Bytecode* code) override {
        out += dex::GetOpcodeName(code->opcode);
        for (size_t i = 0; i < code->operands.size(); ++i) {
            out += i ? ", " : " ";
            if (!code->operands[i]->Accept(this)) return false;
        }
        out += '\n';
        return true;
    }
    bool Visit(lir::VReg* reg) override { Register(out, reg->reg); return true; }
    bool Visit(lir::Const32* value) override { Number(out, value->u.s4_value); return true; }
    bool Visit(lir::CodeLocation* value) override { Label(out, value->label->offset); return true; }
    bool Visit(lir::Label* label) override { Label(out, label->offset); out += '\n'; return true; }
};
#endif

bool Write(const uint16_t* code, size_t size, uint16_t regs, std::string& out) {
    std::vector<uint32_t> starts, labels;
    if (!Check(code, size, regs, starts, labels)) return false;
    out = ".method public static probe()V\n.registers ";
    Number(out, regs);
    out += '\n';
#if DEXKIT_SMALI_PROBE_IR
    auto dex_ir = std::make_shared<ir::DexFile>();
    auto method = dex_ir->Alloc<ir::EncodedMethod>();
    method->code = dex_ir->Alloc<ir::Code>();
    method->code->registers = regs;
    method->code->instructions = {code, size};
    lir::CodeIr body(method, dex_ir);
    BodyVisitor visitor(out);
    for (auto node : body.instructions) if (!node->Accept(&visitor)) return false;
#else
    for (auto pc : starts) {
        if (std::binary_search(labels.begin(), labels.end(), pc)) { Label(out, pc); out += '\n'; }
        auto inst = dex::DecodeInstruction(code + pc);
        out += dex::GetOpcodeName(inst.opcode);
        switch (inst.opcode) {
            case dex::OP_MOVE:
                out += ' '; Register(out, inst.vA); out += ", "; Register(out, inst.vB); break;
            case dex::OP_CONST_4: case dex::OP_CONST_16:
                out += ' '; Register(out, inst.vA); out += ", "; Number(out, int32_t(inst.vB)); break;
            case dex::OP_GOTO:
                out += ' '; Label(out, pc + int32_t(inst.vA)); break;
            case dex::OP_IF_EQZ:
                out += ' '; Register(out, inst.vA); out += ", "; Label(out, pc + int32_t(inst.vB)); break;
            default: break;
        }
        out += '\n';
    }
#endif
    out += ".end method\n";
    return true;
}
} // namespace

// Exported so LTO/section GC cannot discard the experiment. The caller supplies
// a live aligned buffer and output capacity; no pointers escape the call.
extern "C" __attribute__((visibility("default"))) size_t DexKitSmaliSizeProbe(
        const uint16_t* code, size_t size, uint16_t regs, char* output, size_t capacity) {
    std::string text;
    if (!Write(code, size, regs, text)) return 0;
    if (output && capacity >= text.size()) std::memcpy(output, text.data(), text.size());
    return text.size();
}

namespace {
void At(std::string& out, uint32_t pc) { Label(out, pc); out += '\n'; }
void CatchAll(std::string& out, uint32_t start, uint32_t end, uint32_t handler) {
    out += ".catchall {"; Label(out, start); out += " .. "; Label(out, end);
    out += "} "; Label(out, handler); out += '\n';
}
void Array(std::string& out, const uint16_t* data) {
    // S1 deliberately uses only this fixed width-one payload, not a second
    // general payload validator. Both paths receive the same constant seed.
    out += ".array-data 1\n";
    const auto* bytes = reinterpret_cast<const uint8_t*>(data + 4);
    for (unsigned i = 0; i < 3; ++i) { Number(out, int8_t(bytes[i])); out += "t\n"; }
    out += ".end array-data\n";
}
#if DEXKIT_SMALI_PROBE_IR
struct ControlVisitor final : BodyVisitor {
    uint32_t try_start = 0, try_end = 0, handler = 0;
    bool has_catch = false;
    using BodyVisitor::BodyVisitor;
    bool Visit(lir::Bytecode* code) override { At(out, code->offset); return BodyVisitor::Visit(code); }
    bool Visit(lir::Label*) override { return true; }
    bool Visit(lir::PackedSwitchPayload* payload) override {
        At(out, payload->offset); out += ".packed-switch "; Number(out, payload->first_key); out += '\n';
        for (auto target : payload->targets) { Label(out, target->offset); out += '\n'; }
        out += ".end packed-switch\n";
        return true;
    }
    bool Visit(lir::ArrayData* payload) override {
        At(out, payload->offset); Array(out, payload->data.ptr<uint16_t>()); return true;
    }
    bool Visit(lir::TryBlockBegin*) override { return true; }
    bool Visit(lir::TryBlockEnd* block) override {
        if (!block->handlers.empty() || !block->catch_all) return false;
        has_catch = true; try_start = block->try_begin->offset;
        try_end = block->offset; handler = block->catch_all->offset;
        return true;
    }
};
#endif

bool Control(unsigned fixture, std::string& out) {
    // S1 has an intentionally closed input set: packed switch, array data and
    // a catch-all. It measures lifting/emission, not Reader or malformed input.
    static constexpr uint16_t packed[]{0x002b, 4, 0, 0x000e, 0x0100, 1, 0, 0, 3, 0};
    static constexpr uint16_t array[]{0x0026, 4, 0, 0x000e, 0x0300, 1, 3, 0, 0x00ff, 0x007f};
    static constexpr uint16_t caught[]{0, 0x000e, 0x000d, 0x000e};
    if (fixture > 2) return false;
    const uint16_t* code = fixture == 0 ? packed : fixture == 1 ? array : caught;
    size_t size = fixture == 2 ? 4 : 10;
    out = ".method public static probe()V\n.registers 1\n";
#if DEXKIT_SMALI_PROBE_IR
    auto dex_ir = std::make_shared<ir::DexFile>();
    auto method = dex_ir->Alloc<ir::EncodedMethod>();
    method->code = dex_ir->Alloc<ir::Code>();
    method->code->registers = 1;
    method->code->instructions = {code, size};
    const dex::TryBlock block{0, 1, 1};
    const uint8_t handlers[]{1, 0, 2};
    if (fixture == 2) {
        method->code->try_blocks = {&block, 1};
        method->code->catch_handlers = {handlers, sizeof(handlers)};
    }
    lir::CodeIr body(method, dex_ir);
    ControlVisitor visitor(out);
    for (auto node : body.instructions) if (!node->Accept(&visitor)) return false;
    if (visitor.has_catch) CatchAll(out, visitor.try_start, visitor.try_end, visitor.handler);
#else
    for (uint32_t pc = 0; pc < size;) {
        At(out, pc);
        if (code[pc] == dex::kPackedSwitchSignature) {
            out += ".packed-switch "; Number(out, int32_t(code[pc + 2])); out += '\n';
            Label(out, code[pc + 4]); out += "\n.end packed-switch\n";
        } else if (code[pc] == dex::kArrayDataSignature) {
            Array(out, code + pc);
        } else {
            auto decoded = dex::DecodeInstruction(code + pc);
            out += dex::GetOpcodeName(decoded.opcode);
            if (decoded.opcode == dex::OP_PACKED_SWITCH || decoded.opcode == dex::OP_FILL_ARRAY_DATA) {
                out += ' '; Register(out, decoded.vA); out += ", "; Label(out, pc + int32_t(decoded.vB));
            } else if (decoded.opcode == dex::OP_MOVE_EXCEPTION) {
                out += ' '; Register(out, decoded.vA);
            }
            out += '\n';
        }
        pc += dex::GetWidthFromBytecode(code + pc);
    }
    if (fixture == 2) CatchAll(out, 0, 1, 2);
#endif
    out += ".end method\n";
    return true;
}
} // namespace

extern "C" __attribute__((visibility("default"))) size_t DexKitSmaliControlProbe(
        unsigned fixture, char* output, size_t capacity) {
    std::string text;
    if (!Control(fixture, text)) return 0;
    if (output && capacity >= text.size()) std::memcpy(output, text.data(), text.size());
    return text.size();
}
